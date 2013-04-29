#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "driver.h"
#include "driver_dns.h"
#include "driver_tcp.h"

#include "memory.h"
#include "packet.h"
#include "session.h"
#include "time.h"
#include "types.h"

/* Default options */
#define DEFAULT_DNS_SERVER "localhost"
#define DEFAULT_DNS_PORT   53
#define DEFAULT_DOMAIN     "skullseclabs.org"

typedef struct
{
  select_group_t *group;
  session_t      *session;
  NBBOOL          stdin_is_closed;

  char           *dns_server;
  uint16_t        dns_port;
  char           *domain;
} options_t;

/* Make this a module-level variable so we can clean it up
 * in our atexit() handler */
static options_t *options;

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = (options_t*) param;

  session_send(options->session, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  options_t *options = (options_t*) param;

  fprintf(stderr, "[[dnscat]] :: STDIN is closed, sending remaining data\n");

  session_close(options->session);

  return SELECT_REMOVE;
}

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  options_t *options = (options_t*) param;

  session_do_actions(options->session);

  return SELECT_OK;
}

void cleanup()
{
  fprintf(stderr, "[[dnscat]] :: Terminating\n");

  if(options)
  {
    session_destroy(options->session);
    select_group_destroy(options->group);
    safe_free(options);
    options = NULL;
  }

  print_memory();
}

void catch_signal(int sig)
{
  cleanup();
}

int main(int argc, char *argv[])
{
  char        c;
  int         option_index;
  const char *option_name;
  options = (options_t*)safe_malloc(sizeof(options_t));

  struct option long_options[] =
  {
    {"domain", required_argument, 0, 0}, /* Domain name */
    {"d",      required_argument, 0, 0},
    {"host",   required_argument, 0, 0}, /* DNS server */
    {"port",   required_argument, 0, 0}, /* DNS port */
    {0,        0,                 0, 0}  /* End */
  };

  srand(time(NULL));

  /* Default the options to 0 */
  memset(options, 0, sizeof(options));

  /* Create the select_group */
  options->group = select_group_create();

  /* Default options */
  options->dns_server = DEFAULT_DNS_SERVER;
  options->dns_port   = DEFAULT_DNS_PORT;
  options->domain     = DEFAULT_DOMAIN;

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
          options->dns_server = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          options->dns_port = atoi(optarg);
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
        fprintf(stderr, "Unknown option\n");
        exit(1);
        /* TODO: Usage */
        break;
    }
  }

  /* Tell the user what's going on */
  fprintf(stderr, "Options selected:\n");
  fprintf(stderr, " DNS Server: %s\n", options->dns_server);
  fprintf(stderr, " DNS Port:   %d\n", options->dns_port);
  fprintf(stderr, " Domain:     %s\n", options->domain);

  /* Set up the session */
  options->session = session_create(driver_get_dns(options->domain, options->dns_server, options->dns_port, options->group));

  /* Create the STDIN socket */
#ifdef WIN32
  /* On Windows, the stdin_handle is quite complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(group, -1, stdin_handle, (void*)options);
  select_set_recv(group, -1, stdin_callback);
  select_set_closed(group, -1, stdin_closed_callback);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(options->group, stdin_handle, SOCKET_TYPE_STREAM, (void*)options);
  select_set_recv(options->group, stdin_handle, stdin_callback);
  select_set_closed(options->group, stdin_handle, stdin_closed_callback);

  /* Add the timeout function */
  select_set_timeout(options->group, timeout, (void*)options);
#endif

  atexit(cleanup);
  signal(SIGTERM, catch_signal);

  while(TRUE)
    select_group_do_select(options->group, 1000);

  return 0;
}
