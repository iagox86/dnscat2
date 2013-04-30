#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "driver.h"

#include "memory.h"
#include "packet.h"
#include "session.h"
#include "time.h"
#include "types.h"

typedef struct
{
  select_group_t *group;
  session_t      *session;
  driver_t       *driver;
  NBBOOL          stdin_is_closed;

  int             poll_rate;
} options_t;

/* Make this a module-level variable so we can clean it up
 * in our atexit() handler */
static options_t *options;

/* Default values */
#define DEFAULT_POLL_RATE    1000

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
  char c;
  int  option_index;

  struct option long_options[] =
  {
    {"host", required_argument, 0, 0},
    {0,        0,                 0, 0}  /* End */
  };

  options = (options_t*)safe_malloc(sizeof(options_t));

  srand(time(NULL));

  /* Default the options to 0 */
  memset(options, 0, sizeof(options));

  /* Set default values. */
  options->poll_rate       = DEFAULT_POLL_RATE;

  /* Parse the command line options. */
  opterr = 0;
  while((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != EOF)
  {
    switch(c)
    {
      case '?':
      default:
        /* Do nothing; we expect some unknown arguments. */
        break;
    }
  }

  /* Reset the option index to begin processing from the start. */
  optind = 1;

  /* TODO: What if I let the driver create the session? */
  options->group   = select_group_create();
  options->driver  = driver_create(argc, argv, options->group);
  options->session = session_create(options->driver);

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
    select_group_do_select(options->group, options->poll_rate);

  return 0;
}
