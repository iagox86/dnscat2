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
  message_post_shutdown();

  return SELECT_CLOSE_REMOVE;
}

/* This is called after the drivers are created, to kick things off. */
static void handle_start(driver_exec_t *driver)
{
  message_options_t options[2];

  /* Declare some WIN32 variables needed for starting the sub-process. */
#ifdef WIN32
  STARTUPINFOA         startupInfo;
  PROCESS_INFORMATION  processInformation;
  SECURITY_ATTRIBUTES  sa;
#endif

  /* Set up the session options and create the session. */
  options[0].name = "name";
  if(driver->name)
    options[0].value.s = driver->name;
  else
    options[0].value.s = driver->process;

  options[1].name    = NULL;

  message_post_create_session(options);

#ifdef WIN32
  /* Create a security attributes structure. This is required to inherit handles. */
  ZeroMemory(&sa, sizeof(SECURITY_ATTRIBUTES));
  sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
  sa.lpSecurityDescriptor = NULL;
  sa.bInheritHandle       = TRUE;

  /* Create the anonymous pipes. */
  if(!CreatePipe(&driver->exec_stdin[PIPE_READ], &driver->exec_stdin[PIPE_WRITE], &sa, 0))
    DIE("exec: Couldn't create pipe for stdin");
  if(!CreatePipe(&driver->exec_stdout[PIPE_READ], &driver->exec_stdout[PIPE_WRITE], &sa, 0))
    DIE("exec: Couldn't create pipe for stdout");

  fprintf(stderr, "Attempting to load the program: %s\n", driver->process);

  /* Initialize the STARTUPINFO structure. */
  ZeroMemory(&startupInfo, sizeof(STARTUPINFO));
  startupInfo.cb         = sizeof(STARTUPINFO);
  startupInfo.dwFlags    = STARTF_USESTDHANDLES;
  startupInfo.hStdInput  = driver->exec_stdin[PIPE_READ];
  startupInfo.hStdOutput = driver->exec_stdout[PIPE_WRITE];
  startupInfo.hStdError = driver->exec_stdout[PIPE_WRITE];

  /* Initialize the PROCESS_INFORMATION structure. */
  ZeroMemory(&processInformation, sizeof(PROCESS_INFORMATION));

  /* Create the actual process with an overly-complicated CreateProcess function. */
  if(!CreateProcessA(NULL, driver->process, 0, &sa, TRUE, CREATE_NO_WINDOW, 0, NULL, &startupInfo, &processInformation))
  {
    fprintf(stderr, "Failed to create the process");
    exit(1);
  }

  /* Save the process id and the handle. */
  driver->pid = processInformation.dwProcessId;
  driver->exec_handle = processInformation.hProcess;

  /* Close the duplicate pipes we created -- this lets us detect the proicess termination. */
  CloseHandle(driver->exec_stdin[PIPE_READ]);
  CloseHandle(driver->exec_stdout[PIPE_WRITE]);
  CloseHandle(driver->exec_stdout[PIPE_WRITE]);

  fprintf(stderr, "Successfully created the process!\n\n");

  /* Create a socket_id value - this is a totally arbitrary value that's only used so we can find this entry later. */
  driver->socket_id = --driver->socket_id;

  /* On Windows, add the sub-process's stdout as a pipe. */
  select_group_add_pipe(driver->group, driver->socket_id, driver->exec_stdout[PIPE_READ], driver);
  select_set_recv(driver->group, driver->socket_id, exec_callback);
  select_set_closed(driver->group, driver->socket_id, exec_closed_callback);
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
  select_set_recv(driver->group,         driver->pipe_stdout[PIPE_READ], exec_callback);
  select_set_closed(driver->group,       driver->pipe_stdout[PIPE_READ], exec_closed_callback);
#endif
}

void handle_session_created(driver_exec_t *driver, uint16_t session_id)
{
  driver->session_id = session_id;
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

static void handle_config_int(driver_exec_t *driver, char *name, int value)
{
  LOG_WARNING("Unknown configuration option: %s", name);
}

static void handle_config_string(driver_exec_t *driver, char *name, char *value)
{
  if(!strcmp(name, "name"))
  {
    driver->name = value;
  }
  else
  {
    LOG_WARNING("Unknown configuration option: %s", name);
  }
}

static void handle_message(message_t *message, void *d)
{
  driver_exec_t *driver = (driver_exec_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver);
      break;

    case MESSAGE_SESSION_CREATED:
      handle_session_created(driver, message->message.session_created.session_id);
      break;

    case MESSAGE_DATA_IN:
      handle_data_in(driver, message->message.data_in.data, message->message.data_in.length);
      break;

    case MESSAGE_CONFIG:
      if(message->message.config.type == CONFIG_INT)
        handle_config_int(driver, message->message.config.name, message->message.config.value.int_value);
      else if(message->message.config.type == CONFIG_STRING)
        handle_config_string(driver, message->message.config.name, message->message.config.value.string_value);
      else
      {
        LOG_FATAL("Unknown config type: %d", message->message.config.type);
        abort();
      }
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
  message_subscribe(MESSAGE_SESSION_CREATED, handle_message, driver_exec);
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver_exec);
  message_subscribe(MESSAGE_CONFIG,          handle_message, driver_exec);

  return driver_exec;
}

void driver_exec_destroy(driver_exec_t *driver)
{
  safe_free(driver);
}
