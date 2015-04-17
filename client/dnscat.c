/* dnscat.c
 * Created March/2013
 * By Ron Bowes
 *
 * See LICENSE.md
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include "my_getopt.h"
#else
#include <getopt.h>
#include <sys/socket.h>
#endif

#include "tunnel_drivers/dns/driver_dns.h"
#include "controller/controller.h"
#include "controller/session.h"
#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/udp.h"

#include "drivers/driver_console.h"
#if 0
#include "driver_command.h"
#include "driver_dns.h"
#include "driver_exec.h"
#include "driver_listener.h"
#include "driver_ping.h"
#endif

/* Default options */
#define NAME    "dnscat2"
#define VERSION "0.02"

/* Default options */
#define DEFAULT_DNS_HOST NULL
#define DEFAULT_DNS_PORT 53

/* Types of DNS queries we support */
#ifndef WIN32
#define DNS_TYPES "TXT, CNAME, MX, A, AAAA"
#else
#define DNS_TYPES "TXT, CNAME, MX, A"
#endif

/* Define these outside the function so they can be freed by the atexec() */
select_group_t   *group          = NULL;

typedef enum {
  TYPE_NOT_SET,

  TYPE_CONSOLE,
  TYPE_COMMAND,
  TYPE_EXEC,
  TYPE_LISTENER,
  TYPE_PING,

  TYPE_DNS,
} drivers_t;

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  message_post_heartbeat();

  return SELECT_OK;
}

static void cleanup(void)
{
  LOG_WARNING("Terminating");

  controller_shutdown();

  if(group)
    select_group_destroy(group);

  print_memory();
}

void usage(char *name, char *message)
{
  fprintf(stderr,
"Usage: %s [args] [domain]\n"
"\n"

"General options:\n"
" --help -h               This page\n"
" --version               Ge the version\n"
" --name -n <name>        Give this connection a name, which will show up in\n"
"                         the server list\n"
" --download <filename>   Request the given file off the server\n"
" --chunk <n>             start at the given chunk of the --download file\n"
" --ping                  Attempt to ping a dnscat2 server\n"
"\n"
"Input options:\n"
" --console               Send/receive output to the console\n"
" --exec -e <process>     Execute the given process and link it to the stream\n"
" --listen -l <port>      Listen on the given port and link each connection to\n"
"                         a new stream\n"
"\n"
"Debug options:\n"
" -d                      Display more debug info (can be used multiple times)\n"
" -q                      Display less debug info (can be used multiple times)\n"
" --packet-trace          Display incoming/outgoing dnscat2 packets\n"
"\n"
"Tunnel driver options and possible <options>:\n"
" --dns <options>         Enable DNS mode with the given domain\n"
"   domain=<domain>       The domain to make requests for\n"
"   host=<hostname>       The host to listen on (default: 0.0.0.0)\n"
"   port=<port>           The port to listen on (default: 53)\n"
"   type=<type>           The type of DNS requests to use, can use\n"
"                         multiple comma-separated (options: TXT, MX,\n"
"                         CNAME, A, AAAA) (default: TXT)\n"
"   server=<server>       The upstream server for making DNS requests\n"
"                         (default: %s)\n"
" --tcp <options>         Enable TCP mode\n"
"   port=<port>           The port to listen on (default: 1234)\n"
"   host=<hostname>       The host to listen on (default: 0.0.0.0)\n"
"\n"
"Examples:\n"
" --dns=domain=skullseclabs.org\n"
" --dns=domain=skullseclabs.org:port=53\n"
" --dns=domain=skullseclabs.org:port=53:type=A,CNAME\n"
" --tcp=port=1234\n"
" --tcp=port=1234:host=127.0.0.1\n"
"\n"
"By default, a --dns listener on port 53 is enabled if a hostname is\n"
"passed on the commandline\n"
"\n"
"Tunnel driver options are semicolon-separated name=value pairs, with the\n"
"following possibilities (depending on the protocol):\n"
"\n"
"ERROR: %s\n"
"\n"
, name, dns_get_system(), message
);
  exit(0);
}

void too_many_inputs(char *name)
{
  usage(name, "More than one of --exec, --console, --listen, and --ping can't be set!");
}

void create_dns_driver_internal(select_group_t *group, char *domain, char *host, uint16_t port, char *type, char *server)
{
  driver_dns_t *driver = NULL;

  if(!server)
    server = dns_get_system();

  if(!server)
  {
    LOG_FATAL("Couldn't determine the system DNS server! Please manually set");
    LOG_FATAL("the dns server with --dns=server=8.8.8.8");
    LOG_FATAL("");
    LOG_FATAL("You can also fix this by creating a proper /etc/resolv.conf\n");
    exit(1);
  }

  printf("Creating DNS driver:\n");
  printf(" domain = %s\n", domain);
  printf(" host   = %s\n", host);
  printf(" port   = %u\n", port);
  printf(" type   = %s\n", type);
  printf(" server = %s\n", server);

  driver = driver_dns_create(group, domain, host, port, _DNS_TYPE_TEXT, server);

  controller_set_tunnel_driver(TUNNEL_DRIVER_DNS, driver);
}

void create_dns_driver(select_group_t *group, char *options)
{
  char     *domain = NULL;
  char     *host = "0.0.0.0";
  uint16_t  port = 53;
  char     *type = "TXT";
  char     *server = dns_get_system();

  char *token = NULL;

  for(token = strtok(options, ":"); token && *token; token = strtok(NULL, ":"))
  {
    char *name  = token;
    char *value = strchr(token, '=');

    if(value)
    {
      *value = '\0';
      value++;

      if(!strcmp(name, "domain"))
        domain = value;
      else if(!strcmp(name, "host"))
        host = value;
      else if(!strcmp(name, "port"))
        port = atoi(value);
      else if(!strcmp(name, "type"))
        type = value;
      else if(!strcmp(name, "server"))
        server = value;
      else
      {
        printf("Unknown --dns option: %s\n", name);
        exit(1);
      }
    }
    else
    {
      printf("ERROR parsing --dns: it has to be colon-separated name=value pairs!\n");
      exit(1);
    }
  }

  create_dns_driver_internal(group, domain, host, port, type, server);
}

void create_tcp_driver(char *options)
{
  char *host = "0.0.0.0";
  uint16_t port = 1234;

  printf(" host   = %s\n", host);
  printf(" port   = %u\n", port);
}

int main(int argc, char *argv[])
{
  /* Define the options specific to the DNS protocol. */
  struct option long_options[] =
  {
    /* General options */
    {"help",    no_argument,       0, 0}, /* Help */
    {"h",       no_argument,       0, 0},
    {"version", no_argument,       0, 0}, /* Version */
    {"name",    required_argument, 0, 0}, /* Name */
    {"n",       required_argument, 0, 0},
    {"download",required_argument, 0, 0}, /* Download */
    {"n",       required_argument, 0, 0},
    {"chunk",   required_argument, 0, 0}, /* Download chunk */
    {"ping",    no_argument,       0, 0}, /* Ping */
    {"isn",     required_argument, 0, 0}, /* Initial sequence number */

    /* Console options. */
    {"console", no_argument,       0, 0}, /* Enable console (default) */

    /* Execute-specific options. */
    {"exec",    required_argument, 0, 0}, /* Enable execute */
    {"e",       required_argument, 0, 0},

    /* Listener options */
    {"listen",  required_argument, 0, 0}, /* Enable listener */
    {"l",       required_argument, 0, 0},

    /* Tunnel drivers */
    {"dns",        optional_argument, 0, 0}, /* Enable DNS */
    {"tcp",        optional_argument, 0, 0}, /* Enable DNS */

    /* Debug options */
    {"d",            no_argument, 0, 0}, /* More debug */
    {"q",            no_argument, 0, 0}, /* Less debug */
    {"packet-trace", no_argument, 0, 0}, /* Trace packets */

    /* Sentry */
    {0,              0,                 0, 0}  /* End */
  };

  char              c;
  int               option_index;
  const char       *option_name;

  NBBOOL            output_set = FALSE;

  char             *name     = NULL;
  char             *download = NULL;
  uint32_t          chunk    = -1;

  /* TODO: Fix types */
  /*dns_type_t        dns_type = _DNS_TYPE_TEXT; */ /* TODO: Is this the best default? */

  log_level_t       min_log_level = LOG_LEVEL_WARNING;

  drivers_t input_type = TYPE_NOT_SET;

  char *exec_process = NULL;

  int listen_port = 0;

  session_t *session = NULL;


  /* Initialize the modules that need initialization. */
  log_init();

  group = select_group_create();

  /* Seed with the current time; not great, but it'll suit our purposes. */
  srand((unsigned int)time(NULL));

  /* This is required for win32 support. */
  winsock_initialize();

  /* Set the default log level */
  log_set_min_console_level(min_log_level);

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
          usage(argv[0], "--help requested");
        }
        if(!strcmp(option_name, "version"))
        {
          printf(NAME" v"VERSION" (client)\n");
          exit(0);
        }
        else if(!strcmp(option_name, "name") || !strcmp(option_name, "n"))
        {
          name = optarg;
        }
        else if(!strcmp(option_name, "download"))
        {
          download = optarg;
        }
        else if(!strcmp(option_name, "chunk"))
        {
          chunk = atoi(optarg);
        }
        else if(!strcmp(option_name, "ping"))
        {
          if(input_type != TYPE_NOT_SET)
            too_many_inputs(argv[0]);

          input_type = TYPE_PING;

          /* Turn off logging, since this is a simple ping. */
          min_log_level++;
          log_set_min_console_level(min_log_level);
        }
        else if(!strcmp(option_name, "isn"))
        {
          uint16_t isn = (uint16_t) (atoi(optarg) & 0xFFFF);
          debug_set_isn(isn);
        }

        /* Console-specific options. */
        else if(!strcmp(option_name, "console"))
        {
          if(input_type != TYPE_NOT_SET)
            too_many_inputs(argv[0]);

          input_type = TYPE_CONSOLE;
        }

        /* Execute options. */
        else if(!strcmp(option_name, "exec") || !strcmp(option_name, "e"))
        {
          if(input_type != TYPE_NOT_SET)
            too_many_inputs(argv[0]);

          exec_process = optarg;
          input_type = TYPE_EXEC;
        }

        /* Listener options. */
        else if(!strcmp(option_name, "listen") || !strcmp(option_name, "l"))
        {
          if(input_type != TYPE_NOT_SET)
            too_many_inputs(argv[0]);

          listen_port = atoi(optarg);

          input_type = TYPE_LISTENER;
        }

        /* Tunnel driver options */
        else if(!strcmp(option_name, "dns"))
        {
          output_set = TRUE;
          create_dns_driver(group, optarg);
        }
        else if(!strcmp(option_name, "tcp"))
        {
          output_set = TRUE;
          create_tcp_driver(optarg);
        }

        /* Debug options */
        else if(!strcmp(option_name, "d"))
        {
          if(min_log_level > 0)
          {
            min_log_level--;
            log_set_min_console_level(min_log_level);
          }
        }
        else if(!strcmp(option_name, "q"))
        {
          log_set_min_console_level(min_log_level);
        }
        else if(!strcmp(option_name, "packet-trace"))
        {
          session_enable_packet_trace();
        }
        else
        {
          usage(argv[0], "Unknown option");
        }
        break;

      case '?':
      default:
        usage(argv[0], "Unrecognized argument");
        break;
    }
  }

  if(chunk != -1 && !download)
  {
    LOG_FATAL("--chunk can only be used with --download");
    exit(1);
  }

  /* If no input was created, default to command. */
  if(input_type == TYPE_NOT_SET)
    input_type = TYPE_COMMAND;

  /* Create the initial session. */
  session = session_create_console("FIXME: Session Naming :)", NULL, 0, FALSE);
  switch(input_type)
  {
    case TYPE_CONSOLE:
      LOG_WARNING("INPUT: Console");
      driver_console_create(group, session);
      break;
#if 0
    case TYPE_COMMAND:
      LOG_WARNING("INPUT: Command");
      driver_command_create(group, name);
      break;

    case TYPE_EXEC:
      LOG_WARNING("INPUT: Executing %s", exec_process);

      if(exec_process == NULL)
        usage(argv[0], "--exec set without a process!");

      driver_exec_create(group, exec_process, name);
      break;

    case TYPE_LISTENER:
      LOG_WARNING("INPUT: Listening on port %d", driver_listener->port);
      if(listen_port == 0)
        usage(argv[0], "--listen set without a port!");

      driver_listener = driver_listener_create(group, "0.0.0.0", listen_port, name);
      break;

    case TYPE_PING:
      LOG_WARNING("INPUT: ping");
      driver_ping = driver_ping_create(group);
      break;

    case TYPE_NOT_SET:
      usage(argv[0], "You have to pick an input type!");
      break;

    default:
      usage(argv[0], "Unknown type?");
#endif
  }

  /* If no output was set, use the domain, and use the last option as the
   * domain. */
  if(!output_set)
  {
    /* Make sure they gave a domain. */
    if(optind >= argc)
    {
      LOG_WARNING("Starting DNS driver without a domain! This probably won't work;\n");
      LOG_WARNING("You'll probably need to use --dns .");
      create_dns_driver_internal(group, NULL, "0.0.0.0", 53, "TXT", NULL);
    }
    else
    {
      create_dns_driver_internal(group, argv[optind], "0.0.0.0", 53, "TXT", NULL);
    }
  }

  /* Be sure we clean up at exit. */
  atexit(cleanup);

  /* Add the timeout function */
  select_set_timeout(group, timeout, NULL);
  while(TRUE)
    select_group_do_select(group, 1000);

  return 0;
}
