/* ui_exec.c
 * Created May 2, 2013
 * By Ron Bowes
 *
 * See LICENSE.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "errno.h"
#include "log.h"
#include "memory.h"
#include "select_group.h"
#include "session.h"

#include "ui_exec.h"

#define PIPE_READ  0
#define PIPE_WRITE 1

static SELECT_RESPONSE_t exec_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  ui_exec_t *ui_exec = (ui_exec_t*) param;

  ui_exec->data_callback(data, length, ui_exec->callback_param);

  return SELECT_OK;
}

static SELECT_RESPONSE_t exec_closed_callback(void *group, int socket, void *param)
{
  ui_exec_t *ui_exec = (ui_exec_t*) param;

  LOG_WARNING("exec is closed, sending any remaining buffered data");

  ui_exec->closed_callback(ui_exec->callback_param);

  return SELECT_REMOVE;
}

ui_exec_t *ui_exec_create(select_group_t *group, char *process, data_callback_t *data_callback, simple_callback_t *closed_callback, void *callback_param)
{
  ui_exec_t *ui_exec = (ui_exec_t*) safe_malloc(sizeof(ui_exec_t));

  ui_exec->process = safe_strdup(process);
  ui_exec->data_callback   = data_callback;
  ui_exec->closed_callback = closed_callback;
  ui_exec->callback_param  = callback_param;

  /* Create the exec socket */
#ifdef WIN32
  /* TODO */
#else
  LOG_INFO("Attempting to start process '%s'...", process);

  /* Create communication channels. */
  if(pipe(ui_exec->pipe_stdin) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  if(pipe(ui_exec->pipe_stdout) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  ui_exec->pid = fork();

  if(ui_exec->pid == -1)
  {
    LOG_FATAL("exec: couldn't create process (%d)", errno);
    exit(1);
  }

  /* If we're in the child process... */
  if(ui_exec->pid == 0)
  {
    /* Copy the pipes. */
    if(dup2(ui_exec->pipe_stdin[PIPE_READ], STDIN_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDIN handle");

    if(dup2(ui_exec->pipe_stdout[PIPE_WRITE], STDOUT_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDOUT handle");

    if(dup2(ui_exec->pipe_stdout[PIPE_WRITE], STDERR_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDERR handle");

    /* Execute the new process. */
    execlp("/bin/sh", "sh", "-c", process, (char*) NULL);

    /* If execlp returns, bad stuff happened. */
    LOG_FATAL("exec: execlp failed (%d)", errno);
    return FALSE;
  }

  LOG_WARNING("Started: %s (pid: %d)\n", process, ui_exec->pid);
  close(ui_exec->pipe_stdin[PIPE_READ]);
  close(ui_exec->pipe_stdout[PIPE_WRITE]);

  /* Add the sub-process's stdout as a socket. */
  select_group_add_socket(group, ui_exec->pipe_stdout[PIPE_READ], SOCKET_TYPE_STREAM, ui_exec);
  select_set_recv(group, ui_exec->pipe_stdout[PIPE_READ], exec_callback);
  select_set_closed(group, ui_exec->pipe_stdout[PIPE_READ], exec_closed_callback);

#endif

  return ui_exec;
}

void ui_exec_destroy(ui_exec_t *ui_exec, select_group_t *group)
{
/*  select_group_remove_socket(group, ui_exec->pipe_stdout[PIPE_READ]);*/
  safe_free(ui_exec->process);
  safe_free(ui_exec);
}

void ui_exec_feed(ui_exec_t *ui_exec, uint8_t *data, size_t length)
{
  LOG_INFO("Sending %d bytes to '%s'...", length, ui_exec->process);
  write(ui_exec->pipe_stdin[PIPE_WRITE], data, length);
  fflush(NULL);
}
