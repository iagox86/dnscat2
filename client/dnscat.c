/* dnscat.c
 * Created March/2013
 * By Ron Bowes
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

#include "buffer.h"
#include "dns.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "udp.h"

#include "driver_console.h"
#include "driver_command.h"
#include "driver_dns.h"
#include "driver_exec.h"
#include "driver_listener.h"
#include "driver_ping.h"

/* Default options */
#define NAME    "dnscat2"
#define VERSION "0.01"

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

/* Input drivers. */
driver_console_t  *driver_console  = NULL;
driver_command_t  *driver_command  = NULL;
driver_exec_t     *driver_exec     = NULL;
driver_listener_t *driver_listener = NULL;
driver_ping_t     *driver_ping     = NULL;

/* Output drivers. */
driver_dns_t     *driver_dns     = NULL;

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

  message_post_shutdown();
  message_cleanup();

  if(group)
    select_group_destroy(group);

  if(driver_console)
    driver_console_destroy(driver_console);
  if(driver_command)
    driver_command_destroy(driver_command);
  if(driver_dns)
    driver_dns_destroy(driver_dns);
  if(driver_exec)
    driver_exec_destroy(driver_exec);
  if(driver_listener)
    driver_listener_destroy(driver_listener);
  if(driver_ping)
    driver_ping_destroy(driver_ping);

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
"DNS-specific options:\n"
" --dns <domain>          Enable DNS mode with the given domain\n"
" --host <host>           The DNS server [default: %s]\n"
" --port <port>           The DNS port [default: 53]\n"
" --type <port>           The type of DNS record to use (" DNS_TYPES ")\n"
"\n"

"Debug options:\n"
" -d                      Display more debug info (can be used multiple times)\n"
" -q                      Display less debug info (can be used multiple times)\n"
" --packet-trace          Display incoming/outgoing dnscat2 packets\n"
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

    /* DNS-specific options */
#if 0
    {"dns",        required_argument, 0, 0}, /* Enable DNS (default) */
#endif
    {"dnshost",    required_argument, 0, 0}, /* DNS server */
    {"host",       required_argument, 0, 0}, /* (alias) */
    {"dnsport",    required_argument, 0, 0}, /* DNS port */
    {"port",       required_argument, 0, 0}, /* (alias) */
    {"type",       required_argument, 0, 0},

    /* Debug options */
    {"d",            no_argument, 0, 0}, /* More debug */
    {"q",            no_argument, 0, 0}, /* Less debug */
    {"packet-trace", no_argument, 0, 0}, /* Trace packets */

    /* Sentry */
    {0,              0,                 0, 0}  /* End */
  };

  /* Define DNS options so we can set them later. */
  struct {
    char     *host;
    uint16_t  port;
  } dns_options = { DEFAULT_DNS_HOST, DEFAULT_DNS_PORT };

  char              c;
  int               option_index;
  const char       *option_name;

  NBBOOL            output_set = FALSE;

  char             *name     = NULL;
  char             *download = NULL;
  uint32_t          chunk    = -1;

  dns_type_t        dns_type = _DNS_TYPE_TEXT; /* TODO: Is this the best default? */

  log_level_t       min_log_level = LOG_LEVEL_WARNING;

  drivers_t input_type = TYPE_NOT_SET;

  char *exec_process = NULL;

  int listen_port = 0;


  /* Initialize the modules that need initialization. */
  log_init();
  sessions_init();

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

        /* DNS-specific options */
#if 0
        else if(!strcmp(option_name, "dns"))
        {
          output_set = TRUE;
          driver_dns = driver_dns_create(group, optarg);
        }
#endif
        else if(!strcmp(option_name, "dnshost") || !strcmp(option_name, "host"))
        {
          dns_options.host = optarg;
        }
        else if(!strcmp(option_name, "dnsport") || !strcmp(option_name, "port"))
        {
          dns_options.port = atoi(optarg);
        }
        else if(!strcmp(option_name, "type"))
        {
          if(!strcmp(optarg, "TXT") || !strcmp(optarg, "txt") || !strcmp(optarg, "TEXT") || !strcmp(optarg, "text"))
            dns_type = _DNS_TYPE_TEXT;
          else if(!strcmp(optarg, "CNAME") || !strcmp(optarg, "cname"))
            dns_type = _DNS_TYPE_CNAME;
          else if(!strcmp(optarg, "MX") || !strcmp(optarg, "mx"))
            dns_type = _DNS_TYPE_MX;
          else if(!strcmp(optarg, "A") || !strcmp(optarg, "a"))
            dns_type = _DNS_TYPE_A;
#ifndef WIN32
          else if(!strcmp(optarg, "AAAA") || !strcmp(optarg, "aaaa"))
            dns_type = _DNS_TYPE_AAAA;
#endif
          else
            usage(argv[0], "Unknown DNS type! Valid types are: " DNS_TYPES);

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

  switch(input_type)
  {
    case TYPE_CONSOLE:
      LOG_WARNING("INPUT: Console");
      driver_console_create(group, name, download, chunk);
      break;

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
  }

  /* If no output was set, use the domain, and use the last option as the
   * domain. */
  if(!output_set)
  {
    /* Make sure they gave a domain. */
    if(optind >= argc)
    {
      LOG_WARNING("Starting DNS driver without a domain! You'll probably need to use --host to specify a direct connection to your server.");
      driver_dns = driver_dns_create(group, NULL, dns_type);
    }
    else
    {
      driver_dns = driver_dns_create(group, argv[optind], dns_type);
    }
  }

  if(driver_dns)
  {
    if(dns_options.host == DEFAULT_DNS_HOST)
      driver_dns->dns_host = dns_get_system();
    else
      driver_dns->dns_host = safe_strdup(dns_options.host);

    if(!driver_dns->dns_host)
    {
      LOG_FATAL("Couldn't determine the system DNS server! Please use --host to set one.");
      LOG_FATAL("You can also create a proper /etc/resolv.conf file to fix this");
      exit(1);
    }

    driver_dns->dns_port = dns_options.port;
    if(driver_dns->domain)
      LOG_WARNING("OUTPUT: DNS tunnel to %s via %s:%d", driver_dns->domain, driver_dns->dns_host, driver_dns->dns_port);
    else
      LOG_WARNING("OUTPUT: DNS tunnel to %s:%d (no domain set! This probably needs to be the exact server where the dnscat2 server is running!)", driver_dns->dns_host, driver_dns->dns_port);
  }
  else
  {
    LOG_FATAL("OUTPUT: Ended up with an unknown output driver!");
    exit(1);
  }

  /* Be sure we clean up at exit. */
  atexit(cleanup);

  /* Add the timeout function */
  select_set_timeout(group, timeout, NULL);
  while(TRUE)
    select_group_do_select(group, 1000);

  return 0;
}
