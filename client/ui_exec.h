#ifndef __UI_exec__
#define __UI_exec__

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

#include "session.h"

typedef struct
{
  session_t *session;
  char      *process;

#ifdef WIN32
#else
  int        stdin[2];
  int        stdout[2];
  pid_t      pid;
#endif

} ui_exec_t;

ui_exec_t *ui_exec_initialize(select_group_t *group, session_t *session, char *process);
void ui_exec_destroy(ui_exec_t *ui_exec);

void ui_exec_feed(uint8_t *data, size_t length, ui_exec_t *ui_exec);

#endif
