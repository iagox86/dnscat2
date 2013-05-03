#ifndef __UI_exec__
#define __UI_exec__

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#endif

typedef struct
{
  data_callback_t   *data_callback;
  simple_callback_t *closed_callback;
  void              *callback_param;

  char              *process;

#ifdef WIN32
#else
  int        pipe_stdin[2];
  int        pipe_stdout[2];
  pid_t      pid;
#endif

} ui_exec_t;

ui_exec_t *ui_exec_create(select_group_t *group, char *process, data_callback_t *data_callback, simple_callback_t *closed_callback, void *callback_param);
void ui_exec_destroy(ui_exec_t *ui_exec, select_group_t *group);

void ui_exec_feed(ui_exec_t *ui_exec, uint8_t *data, size_t length);

#endif
