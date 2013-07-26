#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "errno.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

#include "driver_exec.h"

#define PIPE_READ  0
#define PIPE_WRITE 1

static SELECT_RESPONSE_t exec_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_exec_t *driver_exec = (driver_exec_t*) param;

  message_post_data_out(driver_exec->session_id, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t exec_closed_callback(void *group, int socket, void *d)
{
  message_post_destroy();

  return SELECT_CLOSE_REMOVE;
}

/* This is called after the drivers are created, to kick things off. */
static void handle_start(driver_exec_t *driver)
{
  message_post_create_session(&driver->session_id);

  /* Create the exec socket */
#ifdef WIN32
  /* TODO */
#else
  LOG_INFO("Attempting to start process '%s'...", driver->process);

  /* Create communication channels. */
  if(pipe(driver->pipe_stdin) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  if(pipe(driver->pipe_stdout) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  driver->pid = fork();

  if(driver->pid == -1)
  {
    LOG_FATAL("exec: couldn't create process (%d)", errno);
    exit(1);
  }

  /* If we're in the child process... */
  if(driver->pid == 0)
  {
    /* Copy the pipes. */
    if(dup2(driver->pipe_stdin[PIPE_READ], STDIN_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDIN handle");

    if(dup2(driver->pipe_stdout[PIPE_WRITE], STDOUT_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDOUT handle");

    if(dup2(driver->pipe_stdout[PIPE_WRITE], STDERR_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDERR handle");

    /* Execute the new process. */
    execlp("/bin/sh", "sh", "-c", driver->process, (char*) NULL);

    /* If execlp returns, bad stuff happened. */
    LOG_FATAL("exec: execlp failed (%d)", errno);
    exit(1);
  }

  LOG_WARNING("Started: %s (pid: %d)", driver->process, driver->pid);
  close(driver->pipe_stdin[PIPE_READ]);
  close(driver->pipe_stdout[PIPE_WRITE]);

  /* Add the sub-process's stdout as a socket. */
  select_group_add_socket(driver->group, driver->pipe_stdout[PIPE_READ], SOCKET_TYPE_STREAM, driver);
  select_set_recv(driver->group,   driver->pipe_stdout[PIPE_READ], exec_callback);
  select_set_closed(driver->group, driver->pipe_stdout[PIPE_READ], exec_closed_callback);
#endif

}

static void handle_data_in(driver_exec_t *driver, uint8_t *data, size_t length)
{
  write(driver->pipe_stdin[PIPE_WRITE], data, length);
}

static void handle_message(message_t *message, void *d)
{
  driver_exec_t *driver = (driver_exec_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver);
      break;

    case MESSAGE_DATA_IN:
      handle_data_in(driver, message->message.data.data, message->message.data.length);
      break;

    default:
      LOG_FATAL("driver_exec received an invalid message!");
      exit(1);
  }
}

driver_exec_t *driver_exec_create(select_group_t *group, char *process)
{
  driver_exec_t *driver_exec = (driver_exec_t*) safe_malloc(sizeof(driver_exec_t));

  driver_exec->process = process;
  driver_exec->group   = group;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,           handle_message, driver_exec);
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver_exec);

  return driver_exec;
}

void driver_exec_destroy(driver_exec_t *driver)
{
  safe_free(driver);
}
