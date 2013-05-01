#ifndef __UI_STDIN__
#define __UI_STDIN__

#include "session.h"

typedef struct
{
  session_t *session;
} ui_stdin_t;

ui_stdin_t *ui_stdin_initialize(select_group_t *group, session_t *session);
void ui_stdin_destroy(ui_stdin_t *ui_stdin);

#endif
