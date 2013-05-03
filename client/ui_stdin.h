#ifndef __UI_STDIN__
#define __UI_STDIN__

#include "types.h"

typedef struct
{
  data_callback_t   *data_callback;
  simple_callback_t *closed_callback;
  void              *callback_param;
} ui_stdin_t;

ui_stdin_t *ui_stdin_create(select_group_t *group, data_callback_t *data_callback, simple_callback_t *closed_callback, void *callback_param);
void ui_stdin_destroy(ui_stdin_t *ui_stdin, select_group_t *group);
void ui_stdin_feed(ui_stdin_t *ui_stdin, uint8_t *data, size_t length);

#endif
