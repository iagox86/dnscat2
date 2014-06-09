#ifndef __DRIVER_EXEC_H__
#define __DRIVER_EXEC_H__

#include <sys/types.h>

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  uint16_t        session_id;
  char           *process;
  select_group_t *group;
  char           *name;

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

driver_exec_t *driver_exec_create(select_group_t *group, char *process, char *name);
void           driver_exec_destroy();

/* This can be used to start the driver without sending MESSAGE_START */
void driver_exec_manual_start(driver_exec_t *driver);

#endif
