/* driver_exec.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * Implements an i/o driver that executes a process, and tunnels the
 * stdout/stdin/stderr through the socket to the server. As far as the
 * server is aware, it's indistinguishable from a console (since it's
 * simply text going in and out).
 */

#ifndef __DRIVER_EXEC_H__
#define __DRIVER_EXEC_H__

#include <sys/types.h>

#include "libs/buffer.h"
#include "libs/select_group.h"

typedef struct
{
  char           *process;
  select_group_t *group;
  buffer_t       *outgoing_data;
  NBBOOL          is_shutdown;

#ifdef WIN32
  HANDLE exec_stdin[2];  /* The stdin handle. */
  HANDLE exec_stdout[2]; /* The stdout handle. */
  DWORD  pid;            /* Process id. */
  HANDLE exec_handle;    /* Handle to the executing process. */
  int    socket_id;      /* An arbitrary number that identifies the socket. */
#else
  int pipe_stdin[2];
  int pipe_stdout[2];
  pid_t pid;
#endif
} driver_exec_t;

driver_exec_t *driver_exec_create(select_group_t *group, char *process);
void           driver_exec_destroy(driver_exec_t *driver);
void           driver_exec_data_received(driver_exec_t *driver, uint8_t *data, size_t length);
uint8_t       *driver_exec_get_outgoing(driver_exec_t *driver, size_t *length, size_t max_length);
void           driver_exec_close(driver_exec_t *driver);

#endif
