#ifndef __UI_LISTEN__
#define __UI_LISTEN__

#include "types.h"

typedef struct
{
  data_callback_t   *data_callback;
  simple_callback_t *closed_callback;
  void              *callback_param;
} ui_listen_t;

ui_listen_t *ui_listen_create(select_group_t *group, data_callback_t *data_callback, simple_callback_t *closed_callback, void *callback_param);
void ui_listen_destroy(ui_listen_t *ui_listen, select_group_t *group);
void ui_listen_feed(ui_listen_t *ui_listen, uint8_t *data, size_t length);

#endif
