#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *s)
{
  printf("Received %zd bytes from stdin\n", length);
  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *s)
{
  printf("stdin closed\n");
  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_timeout(void *group, void *param)
{
  printf("timeout\n");
  return SELECT_OK;
}

int main(int argc, const char *argv[])
{
  select_group_t *group = select_group_create();

  /* TODO: Test this on Windows */
#ifdef WIN32
  /* On Windows, the stdin_handle is quire complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(group, -1, stdin_handle, NULL);
  select_set_recv(group, -1, stdin_callback);
  select_set_closed(group, -1, stdin_closed_callback);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(group, stdin_handle, SOCKET_TYPE_STREAM, NULL);
  select_set_recv(group, stdin_handle, stdin_callback);
  select_set_closed(group, stdin_handle, stdin_closed_callback);

  /* Add the timeout function */
  select_set_timeout(group, stdin_timeout, NULL);
#endif


  while(TRUE)
  {
    select_group_do_select(group, 1000);
  }

  return 0;
}
