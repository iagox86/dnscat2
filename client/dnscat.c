/* driver_dns.c
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
#include "memory.h"
#include "udp.h"

#define DNSCAT_DNS

#include "driver.h"

/* Default options */
#define DEFAULT_DNS_SERVER   "localhost"
#define DEFAULT_DNS_PORT     53
#define DEFAULT_DOMAIN       "skullseclabs.org"

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

  /* Set the dns-specific options for the driver */
  driver_t *driver                 = safe_malloc(sizeof(driver_t));

  /* Set up some default options. */
  driver->max_packet_size = 20; /* TODO: Calculate this better. */
  driver->s               = -1;

  driver->dns_host        = DEFAULT_DNS_SERVER;
  driver->dns_port        = DEFAULT_DNS_PORT;
  driver->domain          = DEFAULT_DOMAIN;

  driver->session         = session_create(driver_send, driver);

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
          driver->domain = optarg;
        }
        else if(!strcmp(option_name, "host"))
        {
          driver->dns_host = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          driver->dns_port = atoi(optarg);
        }
        else
        {
          fprintf(stderr, "Unknown option: %s\n", option_name);
          exit(1);
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
  fprintf(stderr, "Options selected:\n");
  fprintf(stderr, " DNS Server: %s\n", driver->dns_host);
  fprintf(stderr, " DNS Port:   %d\n", driver->dns_port);
  fprintf(stderr, " Domain:     %s\n", driver->domain);

  session_go(driver->session);

  return 0;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_t *driver = param;
  dns_t    *dns    = dns_create_from_packet(data, length);

  /* TODO */
  if(dns->rcode != DNS_RCODE_SUCCESS)
  {
    /* TODO: Handle errors more gracefully */
    switch(dns->rcode)
    {
      case DNS_RCODE_FORMAT_ERROR:
        fprintf(stderr, "DNS ERROR: RCODE_FORMAT_ERROR\n");
        break;
      case DNS_RCODE_SERVER_FAILURE:
        fprintf(stderr, "DNS ERROR: RCODE_SERVER_FAILURE\n");
        break;
      case DNS_RCODE_NAME_ERROR:
        fprintf(stderr, "DNS ERROR: RCODE_NAME_ERROR\n");
        break;
      case DNS_RCODE_NOT_IMPLEMENTED:
        fprintf(stderr, "DNS ERROR: RCODE_NOT_IMPLEMENTED\n");
        break;
      case DNS_RCODE_REFUSED:
        fprintf(stderr, "DNS ERROR: RCODE_REFUSED\n");
        break;
      default:
        fprintf(stderr, "DNS ERROR: Unknown error code (0x%04x)\n", dns->rcode);
        break;
    }
  }
  else if(dns->question_count != 1)
  {
    fprintf(stderr, "DNS returned the wrong number of response fields.\n");
    exit(1);
  }
  else if(dns->answer_count != 1)
  {
    fprintf(stderr, "DNS returned the wrong number of response fields.\n");
    exit(1);
  }
  else if(dns->answers[0].type == DNS_TYPE_TEXT)
  {
    char *answer;
    char buf[3];
    size_t i;

    /* TODO: We don't actually need the .domain suffix if we use TEXT records */
    answer = (char*)dns->answers[0].answer->TEXT.text;

    fprintf(stderr, "Received: %s\n", answer);
    if(!strcmp(answer, driver->domain))
    {
      fprintf(stderr, "WARNING: Received a 'nil' answer; ignoring\n");
    }
    else
    {
      /* Find the domain, which should be at the end of the string */
      char *domain = strstr(answer, driver->domain);
      if(!domain)
      {
        fprintf(stderr, "ERROR: Answer didn't contain the domain\n");
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
            fprintf(stderr, "WARNING: Answer contained an odd number of digits\n");
          }
          else if(!isxdigit((int)answer[i]))
          {
            /* ignore */
            fprintf(stderr, "WARNING: Answer contained an invalid digit: '%c'\n", answer[i]);
          }
          else if(!isxdigit((int)answer[i+1]))
          {
            /* ignore */
            fprintf(stderr, "WARNING: Answer contained an invalid digit: '%c'\n", answer[i+1]);
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

          session_recv(driver->session, data, length);

          safe_free(data);
        }
        buffer_destroy(incoming_data);
      }
    }
  }
  else
  {
    fprintf(stderr, "Unknown DNS type returned\n");
    exit(1);
  }

  dns_destroy(dns);

  return SELECT_OK;
}


void driver_send(uint8_t *data, size_t length, void *d)
{
  driver_t     *driver = (driver_t*)d;
  size_t        i;
  dns_t        *dns;
  buffer_t     *buffer;
  uint8_t      *encoded_bytes;
  size_t        encoded_length;
  uint8_t      *dns_bytes;
  size_t        dns_length;

  if(driver->s == -1)
  {
    driver->s = udp_create_socket(0, "0.0.0.0");

    if(driver->s == -1)
    {
      fprintf(stderr, "[[DNS]] :: couldn't create socket!\n");
      return;
    }

    /* If it succeeds, add it to the select_group */
    session_register_socket(driver->session, driver->s, SOCKET_TYPE_STREAM, recv_callback, NULL, driver);
  }

  assert(driver->s != -1); /* Make sure we have a valid socket. */
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

  fprintf(stderr, "SEND: %s\n", encoded_bytes);
  dns = dns_create(DNS_OPCODE_QUERY, DNS_FLAG_RD, DNS_RCODE_SUCCESS);
  dns_add_question(dns, (char*)encoded_bytes, DNS_TYPE_TEXT, DNS_CLASS_IN);
  dns_bytes = dns_to_packet(dns, &dns_length);

  udp_send(driver->s, driver->dns_host, driver->dns_port, dns_bytes, dns_length);

  safe_free(dns_bytes);
  safe_free(encoded_bytes);
  dns_destroy(dns);
}

void driver_close(driver_t *driver)
{
  fprintf(stderr, "[[UDP]] :: close()\n");

  assert(driver->s && driver->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  session_unregister_socket(driver->session, driver->s);
  udp_close(driver->s);
  driver->s = -1;
}

void driver_destroy(driver_t *driver)
{
  fprintf(stderr, "[[DNS]] :: cleanup()\n");

  /* Ensure the driver is closed */
  if(driver->s != -1)
    driver_close(driver);

  safe_free(driver->domain);
  safe_free(driver->dns_host);
}

#if 0
void cleanup()
{
  fprintf(stderr, "[[dnscat]] :: Terminating\n");

  if(options)
  {
    session_destroy(options->session);
    safe_free(options);
    options = NULL;
  }

  print_memory();
}
#endif
