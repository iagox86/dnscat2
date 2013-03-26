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

typedef struct
{
  select_group_t *group;
  session_t      *session;
  NBBOOL          stdin_is_closed;
} options_t;

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = (options_t*) param;

  session_send(options->session, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  options_t *options = (options_t*) param;

  printf("[[dnscat]] :: STDIN is closed, sending remaining data\n");

  session_close(options->session);

  print_memory();
  exit(0);

  return SELECT_REMOVE;
}

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  options_t *options = (options_t*) param;

  session_do_actions(options->session);

  return SELECT_OK;
}

int main(int argc, const char *argv[])
{
  options_t *options = (options_t*)safe_malloc(sizeof(options_t));
  srand(time(NULL));

  /* Create the select_group */
  options->group = select_group_create();

  /* Set up the session */
  /*options->session = session_create(driver_get_tcp("localhost", 2000, options->group));*/
  /*options->session = session_create(driver_get_dns("4.2.2.1", 53, options->group));*/
  options->session = session_create(driver_get_dns("localhost", 53, options->group));

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

  while(TRUE)
    select_group_do_select(options->group, 1000);

  return 0;
}
