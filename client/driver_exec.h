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
#else
  int pipe_stdin[2];
  int pipe_stdout[2];
  pid_t pid;
#endif
} driver_exec_t;

driver_exec_t *driver_exec_create(select_group_t *group, char *process);
void           driver_exec_destroy();

#endif
