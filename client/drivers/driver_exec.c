/* driver_exec.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#endif

#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/types.h"

#include "driver_exec.h"

#define PIPE_READ  0
#define PIPE_WRITE 1

#ifndef WIN32
/* This is necessary for some systems... see:
 * https://github.com/iagox86/dnscat2/issues/61
 */
int kill(pid_t pid, int sig);
#endif

static SELECT_RESPONSE_t exec_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_exec_t *driver = (driver_exec_t*) param;

  buffer_add_bytes(driver->outgoing_data, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t exec_closed_callback(void *group, int socket, void *d)
{
  /* When the stdin pipe is closed, the stdin driver signals the end. */
  driver_exec_t *driver = (driver_exec_t*) d;

  /* Record that we've been shut down - we'll continue reading to the end of the buffer, still. */
  driver->is_shutdown = TRUE;

  return SELECT_CLOSE_REMOVE;
}

void driver_exec_data_received(driver_exec_t *driver, uint8_t *data, size_t length)
{
#ifdef WIN32
  DWORD written;
  WriteFile(driver->exec_stdin[PIPE_WRITE], data, (DWORD)length, &written, NULL);
#else
  if(write(driver->pipe_stdin[PIPE_WRITE], data, length) != length)
    LOG_ERROR("There was a problem writing data. :(");
#endif
}

uint8_t *driver_exec_get_outgoing(driver_exec_t *driver, size_t *length, size_t max_length)
{
  /* If the driver has been killed and we have no bytes left, return NULL to close the session. */
  if(driver->is_shutdown && buffer_get_remaining_bytes(driver->outgoing_data) == 0)
    return NULL;

  return buffer_read_remaining_bytes(driver->outgoing_data, length, max_length, TRUE);
}

driver_exec_t *driver_exec_create(select_group_t *group, char *process)
{
  driver_exec_t *driver = (driver_exec_t*) safe_malloc(sizeof(driver_exec_t));

  /* Declare some WIN32 variables needed for starting the sub-process. */
#ifdef WIN32
  STARTUPINFOA         startupInfo;
  PROCESS_INFORMATION  processInformation;
  SECURITY_ATTRIBUTES  sa;
#endif

  driver->process       = process;
  driver->group         = group;
  driver->outgoing_data = buffer_create(BO_BIG_ENDIAN);

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
  LOG_WARNING("Starting: /bin/sh -c '%s'", driver->process);

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

  return driver;
}

void driver_exec_destroy(driver_exec_t *driver)
{
  if(!driver->is_shutdown)
    driver_exec_close(driver);
  safe_free(driver);
}

void driver_exec_close(driver_exec_t *driver)
{
  LOG_WARNING("exec driver shut down; killing process %d", driver->pid);
#ifdef WIN32
  TerminateProcess(driver->exec_handle, SIGINT);
#else
  kill(driver->pid, SIGINT);
#endif
  driver->is_shutdown = TRUE;
}
