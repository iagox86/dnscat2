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
#include "libs/my_getopt.h"
#else
#include <getopt.h>
#include <sys/socket.h>
#endif

#include "controller/controller.h"
#include "controller/session.h"
#include "libs/buffer.h"
#include "libs/ll.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/udp.h"
#include "tunnel_drivers/driver_dns.h"
#include "tunnel_drivers/tunnel_driver.h"

/* Default options */
#define NAME    "dnscat2"
#define VERSION "v0.05"

/* Default options */
#define DEFAULT_DNS_HOST NULL
#define DEFAULT_DNS_PORT 53

/* Define these outside the function so they can be freed by the atexec() */
select_group_t *group         = NULL;
driver_dns_t   *tunnel_driver = NULL;
char           *system_dns    = NULL;

typedef struct
{
  char *process;
} exec_options_t;

typedef struct
{
  driver_type_t type;
  union
  {
    exec_options_t exec;
  } options;
} make_driver_t;

static make_driver_t *make_console()
{
  make_driver_t *make_driver = (make_driver_t*) safe_malloc(sizeof(make_driver_t));
  make_driver->type = DRIVER_TYPE_CONSOLE;

  return make_driver;
}

static make_driver_t *make_command()
{
  make_driver_t *make_driver = (make_driver_t*) safe_malloc(sizeof(make_driver_t));
  make_driver->type = DRIVER_TYPE_COMMAND;

  return make_driver;
}

static make_driver_t *make_ping()
{
  make_driver_t *make_driver = (make_driver_t*) safe_malloc(sizeof(make_driver_t));
  make_driver->type = DRIVER_TYPE_PING;

  return make_driver;
}

static make_driver_t *make_exec(char *process)
{
  make_driver_t *make_driver = (make_driver_t*) safe_malloc(sizeof(make_driver_t));
  make_driver->type = DRIVER_TYPE_EXEC;
  make_driver->options.exec.process = process;

  return make_driver;
}

static int create_drivers(ll_t *drivers)
{
  int num_created = 0;
  make_driver_t *this_driver;

  while((this_driver = ll_remove_first(drivers)))
  {
    num_created++;
    switch(this_driver->type)
    {
      case DRIVER_TYPE_CONSOLE:
        printf("Creating a console session!\n");
        controller_add_session(session_create_console(group, "console"));
        break;

      case DRIVER_TYPE_EXEC:
        printf("Creating a exec('%s') session!\n", this_driver->options.exec.process);
        controller_add_session(session_create_exec(group, this_driver->options.exec.process, this_driver->options.exec.process));
        break;

      case DRIVER_TYPE_COMMAND:
        printf("Creating a command session!\n");
        controller_add_session(session_create_command(group, "command"));
        break;

      case DRIVER_TYPE_PING:
        printf("Creating a ping session!\n");
        controller_add_session(session_create_ping(group, "ping"));
        break;
    }
    safe_free(this_driver);
  }

  /* Default to creating a command session. */
  if(num_created == 0)
  {
    num_created++;
    controller_add_session(session_create_command(group, "command"));
  }

  return num_created;
}

typedef struct
{
  char     *host;
  uint16_t  port;
  char     *server;
  char     *domain;
} dns_options_t;

typedef struct
{
  tunnel_driver_type_t type;
  union
  {
    dns_options_t dns_options;
  };
} make_tunnel_driver_t;

static void cleanup(void)
{
  LOG_WARNING("Terminating");

  controller_destroy();

  if(tunnel_driver)
    driver_dns_destroy(tunnel_driver);

  if(group)
    select_group_destroy(group);

  if(system_dns)
    safe_free(system_dns);

  print_memory();
}

void usage(char *name, char *message)
{
  fprintf(stderr,
"Usage: %s [args] [domain]\n"
"\n"
"General options:\n"
" --help -h               This page.\n"
" --version               Get the version.\n"
" --delay <ms>            Set the maximum delay between packets (default: 1000).\n"
"                         The minimum is technically 50 for technical reasons,\n"
"                         but transmitting too quickly might make performance\n"
"                         worse.\n"
" --steady                If set, always wait for the delay before sending.\n"
"                         the next message (by default, when a response is\n"
"                         received, the next message is immediately transmitted.\n"
" --max-retransmits <n>   Only re-transmit a message <n> times before giving up\n"
"                         and assuming the server is dead (default: 20).\n"
" --retransmit-forever    Set if you want the client to re-transmit forever\n"
"                         until a server turns up. This can be helpful, but also\n"
"                         makes the server potentially run forever.\n"
#ifndef NO_ENCRYPTION
" --secret                Set the shared secret; set the same one on the server\n"
"                         and the client to prevent man-in-the-middle attacks!\n"
" --no-encryption         Turn off encryption/authentication.\n"
#endif
"\n"
"Input options:\n"
" --console               Send/receive output to the console.\n"
" --exec -e <process>     Execute the given process and link it to the stream.\n"
/*" --listen -l <port>      Listen on the given port and link each connection to\n"
"                         a new stream\n"*/
" --command               Start an interactive 'command' session (default).\n"
" --ping                  Simply check if there's a dnscat2 server listening.\n"
"\n"
"Debug options:\n"
" -d                      Display more debug info (can be used multiple times).\n"
" -q                      Display less debug info (can be used multiple times).\n"
" --packet-trace          Display incoming/outgoing dnscat2 packets\n"
"\n"
"Driver options:\n"
" --dns <options>         Enable DNS mode with the given domain.\n"
"   domain=<domain>       The domain to make requests for.\n"
"   host=<hostname>       The host to listen on (default: 0.0.0.0).\n"
"   port=<port>           The port to listen on (default: 53).\n"
"   type=<type>           The type of DNS requests to use, can use\n"
"                         multiple comma-separated (options: TXT, MX,\n"
"                         CNAME, A, AAAA) (default: "DEFAULT_TYPES").\n"
"   server=<server>       The upstream server for making DNS requests\n"
"                         (default: autodetected = %s).\n"
#if 0
" --tcp <options>         Enable TCP mode.\n"
"   port=<port>           The port to listen on (default: 1234).\n"
"   host=<hostname>       The host to listen on (default: 0.0.0.0).\n"
#endif
"\n"
"Examples:\n"
" ./dnscat --dns domain=skullseclabs.org\n"
" ./dnscat --dns domain=skullseclabs.org,server=8.8.8.8,port=53\n"
" ./dnscat --dns domain=skullseclabs.org,port=5353\n"
" ./dnscat --dns domain=skullseclabs.org,port=53,type=A,CNAME\n"
#if 0
" --tcp port=1234\n"
" --tcp port=1234,host=127.0.0.1\n"
#endif
"\n"
"By default, a --dns driver on port 53 is enabled if a hostname is\n"
"passed on the commandline:\n"
"\n"
" ./dnscat skullseclabs.org\n"
"\n"
"ERROR: %s\n"
"\n"
, name, system_dns, message
);
  exit(0);
}

driver_dns_t *create_dns_driver_internal(select_group_t *group, char *domain, char *host, uint16_t port, char *type, char *server)
{
  if(!server && !domain)
  {
    printf("\n");
    printf("** WARNING!\n");
    printf("*\n");
    printf("* It looks like you're running dnscat2 with the system DNS server,\n");
    printf("* and no domain name!");
    printf("*\n");
    printf("* That's cool, I'm not going to stop you, but the odds are really,\n");
    printf("* really high that this won't work. You either need to provide a\n");
    printf("* domain to use DNS resolution (requires an authoritative server):\n");
    printf("*\n");
    printf("*     dnscat mydomain.com\n");
    printf("*\n");
    printf("* Or you have to provide a server to connect directly to:\n");
    printf("*\n");
    printf("*     dnscat --dns=server=1.2.3.4,port=53\n");
    printf("*\n");
    printf("* I'm going to let this keep running, but once again, this likely\n");
    printf("* isn't what you want!\n");
    printf("*\n");
    printf("** WARNING!\n");
    printf("\n");
  }

  if(!server)
    server = system_dns;

  if(!server)
  {
    LOG_FATAL("Couldn't determine the system DNS server! Please manually set");
    LOG_FATAL("the dns server with --dns server=8.8.8.8");
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

  return driver_dns_create(group, domain, host, port, type, server);
}

driver_dns_t *create_dns_driver(select_group_t *group, char *options)
{
  char     *domain = NULL;
  char     *host = "0.0.0.0";
  uint16_t  port = 53;
  char     *type = DEFAULT_TYPES;
  char     *server = system_dns;

  char *token = NULL;

  for(token = strtok(options, ":,"); token && *token; token = strtok(NULL, ":,"))
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
        LOG_FATAL("Unknown --dns option: %s\n", name);
        exit(1);
      }
    }
    else
    {
      LOG_FATAL("ERROR parsing --dns: it has to be colon-separated name=value pairs!\n");
      exit(1);
    }
  }

  return create_dns_driver_internal(group, domain, host, port, type, server);
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
#if 0
    {"name",    required_argument, 0, 0}, /* Name */
    {"n",       required_argument, 0, 0},
    {"download",required_argument, 0, 0}, /* Download */
    {"n",       required_argument, 0, 0},
    {"chunk",   required_argument, 0, 0}, /* Download chunk */
    {"isn",     required_argument, 0, 0}, /* Initial sequence number */
#endif

    {"delay",              required_argument, 0, 0}, /* Retransmit delay */
    {"steady",             no_argument,       0, 0}, /* Don't transmit immediately after getting a response. */
    {"max-retransmits",    required_argument, 0, 0}, /* Set the max retransmissions */
    {"retransmit-forever", no_argument,       0, 0}, /* Retransmit forever if needed */
#ifndef NO_ENCRYPTION
    {"secret",             required_argument, 0, 0}, /* Pre-shared secret */
    {"no-encryption",      no_argument,       0, 0}, /* Disable encryption */
#endif

    /* i/o options. */
    {"console", no_argument,       0, 0}, /* Enable console */
    {"exec",    required_argument, 0, 0}, /* Enable execute */
    {"e",       required_argument, 0, 0},
    {"command", no_argument,       0, 0}, /* Enable command (default) */
    {"ping",    no_argument,       0, 0}, /* Ping */

    /* Tunnel drivers */
    {"dns",     required_argument, 0, 0}, /* Enable DNS */
#if 0
    {"tcp",     optional_argument, 0, 0}, /* Enable TCP */
#endif

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

  NBBOOL            tunnel_driver_created = FALSE;
  ll_t             *drivers_to_create     = ll_create(NULL);
  uint32_t          drivers_created       = 0;

  log_level_t       min_log_level = LOG_LEVEL_WARNING;

  group = select_group_create();
  system_dns = dns_get_system();

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
          printf(NAME" "VERSION" (client)\n");
          exit(0);
        }
        else if(!strcmp(option_name, "isn"))
        {
          uint16_t isn = (uint16_t) (atoi(optarg) & 0xFFFF);
          debug_set_isn(isn);
        }
        else if(!strcmp(option_name, "delay"))
        {
          int delay = (int) atoi(optarg);
          session_set_delay(delay);
          LOG_INFO("Setting delay between packets to %dms", delay);
        }
        else if(!strcmp(option_name, "steady"))
        {
          session_set_transmit_immediately(FALSE);
        }
        else if(!strcmp(option_name, "max-retransmits"))
        {
          controller_set_max_retransmits(atoi(optarg));
        }
        else if(!strcmp(option_name, "retransmit-forever"))
        {
          controller_set_max_retransmits(-1);
        }
#ifndef NO_ENCRYPTION
        else if(!strcmp(option_name, "secret"))
        {
          session_set_preshared_secret(optarg);
        }
        else if(!strcmp(option_name, "no-encryption"))
        {
          session_set_encryption(FALSE);
        }
#endif

        /* i/o drivers */
        else if(!strcmp(option_name, "console"))
        {
          ll_add(drivers_to_create, ll_32(drivers_created++), make_console());

/*          session = session_create_console(group, "console");
          controller_add_session(session); */
        }
        else if(!strcmp(option_name, "exec") || !strcmp(option_name, "e"))
        {
          ll_add(drivers_to_create, ll_32(drivers_created++), make_exec(optarg));

/*          session = session_create_exec(group, optarg, optarg);
          controller_add_session(session); */
        }
        else if(!strcmp(option_name, "command"))
        {
          ll_add(drivers_to_create, ll_32(drivers_created++), make_command());

/*          session = session_create_command(group, "command");
          controller_add_session(session); */
        }
        else if(!strcmp(option_name, "ping"))
        {
          ll_add(drivers_to_create, ll_32(drivers_created++), make_ping());

/*          session = session_create_ping(group, "ping");
          controller_add_session(session); */
        }

        /* Tunnel driver options */
        else if(!strcmp(option_name, "dns"))
        {
          tunnel_driver_created = TRUE;
          tunnel_driver = create_dns_driver(group, optarg);
        }
        else if(!strcmp(option_name, "tcp"))
        {
          tunnel_driver_created = TRUE;
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
          min_log_level++;
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

  create_drivers(drivers_to_create);
  ll_destroy(drivers_to_create);

  if(tunnel_driver_created && argv[optind])
  {
    printf("It looks like you used --dns and also passed a domain on the commandline.\n");
    printf("That's not allowed! Either use '--dns domain=xxx' or don't use a --dns\n");
    printf("argument!\n");
    exit(1);
  }

  /* If no output was set, use the domain, and use the last option as the
   * domain. */
  if(!tunnel_driver_created)
  {
    /* Make sure they gave a domain. */
    if(optind >= argc)
    {
      printf("Starting DNS driver without a domain! This will only work if you\n");
      printf("are directly connecting to the dnscat2 server.\n");
      printf("\n");
      printf("You'll need to use --dns server=<server> if you aren't.\n");
      tunnel_driver = create_dns_driver_internal(group, NULL, "0.0.0.0", 53, DEFAULT_TYPES, NULL);
    }
    else
    {
      tunnel_driver = create_dns_driver_internal(group, argv[optind], "0.0.0.0", 53, DEFAULT_TYPES, NULL);
    }
  }

  /* Be sure we clean up at exit. */
  atexit(cleanup);

  /* Start the driver! */
  driver_dns_go(tunnel_driver);

  return 0;
}
