#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "memory.h"
#include "packet.h"
#include "session.h"
#include "types.h"

typedef struct
{
  select_group_t *group;
  session_t *session;
} options_t;

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = (options_t*) param;

  session_queue_outgoing(options->session, data, length);

  printf("Received %zd bytes from stdin\n", length);
  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  /*options_t *options = (options_t*) param;*/
  /* TODO: send a FIN */
  printf("stdin closed\n");
  exit(0);
}

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  /*options_t *options = (options_t*) param;*/
  /* TODO: send queued data, depending on the session state */
  printf("timeout\n");
  return SELECT_OK;
}

int main(int argc, const char *argv[])
{
  options_t *options = (options_t*)safe_malloc(sizeof(options_t));

  options->group = select_group_create();
  options->session = session_create();

  /* TODO: Test this on Windows */
#ifdef WIN32
  /* On Windows, the stdin_handle is quire complicated, and involves a sub-thread. */
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

  /* TODO: Determine which interface we're using, create it, and somehow put it
   * in the select_group (perhaps it can return its own select-able socket?) */

  while(TRUE)
  {
    select_group_do_select(options->group, 1000);
  }

  return 0;
}
