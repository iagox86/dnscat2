/* ui_stdin.c
 * Created May 1, 2013
 * By Ron Bowes
 *
 * See LICENSE.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "select_group.h"

#include "session.h"

typedef struct
{
  session_t *session;
} ui_stdin_t;

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  ui_stdin_t *ui_stdin = (ui_stdin_t*) param;

  session_send(ui_stdin->session, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  ui_stdin_t *ui_stdin = (ui_stdin_t*) param;

  LOG_WARNING("STDIN is closed, sending any remaining buffered data");

  session_close(ui_stdin->session);

  return SELECT_REMOVE;
}

ui_stdin_t *ui_stdin_initialize(select_group_t *group, session_t *session)
{
  ui_stdin_t *ui_stdin = (ui_stdin_t*) safe_malloc(sizeof(ui_stdin_t));

  ui_stdin->session = session;

  /* Create the STDIN socket */
#ifdef WIN32
  /* On Windows, the stdin_handle is quite complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(ui_stdin->group, -1, stdin_handle, (void*)ui_stdin);
  select_set_recv(ui_stdin->group, -1, stdin_callback);
  select_set_closed(ui_stdin->group, -1, stdin_closed_callback);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(group, stdin_handle, SOCKET_TYPE_STREAM, (void*)ui_stdin);
  select_set_recv(group, stdin_handle, stdin_callback);
  select_set_closed(group, stdin_handle, stdin_closed_callback);
#endif

  return ui_stdin;
}

void ui_stdin_destroy(ui_stdin_t *ui_stdin)
{
  safe_free(ui_stdin);
}
