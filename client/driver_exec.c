#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#endif

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
  driver_exec_t *driver = (driver_exec_t*) d;

  message_post_close_session(driver->session_id);

  return SELECT_CLOSE_REMOVE;
}

static void handle_data_in(driver_exec_t *driver, uint8_t *data, size_t length)
{
#ifdef WIN32
  DWORD written;
  WriteFile(driver->exec_stdin[PIPE_WRITE], data, (DWORD)length, &written, NULL);
#else
  write(driver->pipe_stdin[PIPE_WRITE], data, length);
#endif
}

static void handle_message(message_t *message, void *d)
{
  driver_exec_t *driver = (driver_exec_t*) d;

  switch(message->type)
  {
    case MESSAGE_DATA_IN:
      if(message->message.data_in.session_id == driver->session_id)
        handle_data_in(driver, message->message.data_in.data, message->message.data_in.length);
      break;

    default:
      LOG_FATAL("driver_exec received an invalid message!");
      exit(1);
  }
}

driver_exec_t *driver_exec_create(select_group_t *group, char *process, char *name)
{
  driver_exec_t *driver_exec = (driver_exec_t*) safe_malloc(sizeof(driver_exec_t));
  message_options_t options[2];

  /* Declare some WIN32 variables needed for starting the sub-process. */
#ifdef WIN32
  STARTUPINFOA         startupInfo;
  PROCESS_INFORMATION  processInformation;
  SECURITY_ATTRIBUTES  sa;
#endif

  driver_exec->process = process;
  driver_exec->group   = group;
  driver_exec->name    = name ? name : process;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver_exec);

  /* Set up the session options and create the session. */
  options[0].name    = "name";
  options[0].value.s = driver_exec->name;

  options[1].name    = NULL;

  driver_exec->session_id = message_post_create_session(options);
#ifdef WIN32
  /* Create a security attributes structure. This is required to inherit handles. */
  ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
  sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle       = TRUE;

  /* Create the anonymous pipes. */
  if(!CreatePipe(&driver_exec->exec_stdin[PIPE_READ], &driver_exec->exec_stdin[PIPE_WRITE], &sa, 0))
    DIE("exec: Couldn't create pipe for stdin");
  if(!CreatePipe(&driver_exec->exec_stdout[PIPE_READ], &driver_exec->exec_stdout[PIPE_WRITE], &sa, 0))
    DIE("exec: Couldn't create pipe for stdout");

  fprintf(stderr, "Attempting to load the program: %s\n", driver_exec->process);

  /* Initialize the STARTUPINFO structure. */
  ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
  startupInfo.cb         = sizeof(STARTUPINFO);
  startupInfo.dwFlags    = STARTF_USESTDHANDLES;
  startupInfo.hStdInput  = driver_exec->exec_stdin[PIPE_READ];
  startupInfo.hStdOutput = driver_exec->exec_stdout[PIPE_WRITE];
  startupInfo.hStdError = driver_exec->exec_stdout[PIPE_WRITE];

  /* Initialize the PROCESS_INFORMATION structure. */
  ZeroMemory(&processInformation, sizeof(PROCESS_INFORMATION));

  /* Create the actual process with an overly-complicated CreateProcess function. */
  if(!CreateProcessA(NULL, driver_exec->process, 0, &sa, TRUE, CREATE_NO_WINDOW, 0, NULL, &startupInfo, &processInformation))
  {
    fprintf(stderr, "Failed to create the process");
    exit(1);
  }

  /* Save the process id and the handle. */
  driver_exec->pid = processInformation.dwProcessId;
  driver_exec->exec_handle = processInformation.hProcess;

  /* Close the duplicate pipes we created -- this lets us detect the proicess termination. */
  CloseHandle(driver_exec->exec_stdin[PIPE_READ]);
  CloseHandle(driver_exec->exec_stdout[PIPE_WRITE]);
  CloseHandle(driver_exec->exec_stdout[PIPE_WRITE]);

  fprintf(stderr, "Successfully created the process!\n\n");

  /* Create a socket_id value - this is a totally arbitrary value that's only used so we can find this entry later. */
  driver_exec->socket_id = --driver_exec->socket_id;

  /* On Windows, add the sub-process's stdout as a pipe. */
  select_group_add_pipe(driver_exec->group, driver_exec->socket_id, driver_exec->exec_stdout[PIPE_READ], driver_exec);
  select_set_recv(driver_exec->group, driver_exec->socket_id, exec_callback);
  select_set_closed(driver_exec->group, driver_exec->socket_id, exec_closed_callback);
#else
  LOG_INFO("Attempting to start process '%s'...", driver_exec->process);

  /* Create communication channels. */
  if(pipe(driver_exec->pipe_stdin) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  if(pipe(driver_exec->pipe_stdout) == -1)
  {
    LOG_FATAL("exec: couldn't create pipe (%d)", errno);
    exit(1);
  }

  driver_exec->pid = fork();

  if(driver_exec->pid == -1)
  {
    LOG_FATAL("exec: couldn't create process (%d)", errno);
    exit(1);
  }

  /* If we're in the child process... */
  if(driver_exec->pid == 0)
  {
    /* Copy the pipes. */
    if(dup2(driver_exec->pipe_stdin[PIPE_READ], STDIN_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDIN handle");

    if(dup2(driver_exec->pipe_stdout[PIPE_WRITE], STDOUT_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDOUT handle");

    if(dup2(driver_exec->pipe_stdout[PIPE_WRITE], STDERR_FILENO) == -1)
      nbdie("exec: couldn't duplicate STDERR handle");

    /* Execute the new process. */
    execlp("/bin/sh", "sh", "-c", driver_exec->process, (char*) NULL);

    /* If execlp returns, bad stuff happened. */
    LOG_FATAL("exec: execlp failed (%d)", errno);
    exit(1);
  }

  LOG_WARNING("Started: %s (pid: %d)", driver_exec->process, driver_exec->pid);
  close(driver_exec->pipe_stdin[PIPE_READ]);
  close(driver_exec->pipe_stdout[PIPE_WRITE]);

  /* Add the sub-process's stdout as a socket. */
  select_group_add_socket(driver_exec->group, driver_exec->pipe_stdout[PIPE_READ], SOCKET_TYPE_STREAM, driver_exec);
  select_set_recv(driver_exec->group,         driver_exec->pipe_stdout[PIPE_READ], exec_callback);
  select_set_closed(driver_exec->group,       driver_exec->pipe_stdout[PIPE_READ], exec_closed_callback);
#endif
  return driver_exec;
}

void driver_exec_destroy(driver_exec_t *driver)
{
  safe_free(driver);
}
