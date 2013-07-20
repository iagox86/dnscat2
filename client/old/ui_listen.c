/* ui_listen.c
 * Created May 1, 2013
 * By Ron Bowes
 *
 * See LICENSE.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "select_group.h"
#include "types.h"

#include "ui_listen.h"

static SELECT_RESPONSE_t listen_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  ui_listen_t *ui_listen = (ui_listen_t*) param;

  ui_listen->data_callback(data, length, ui_listen->callback_param);

  return SELECT_OK;
}

static SELECT_RESPONSE_t listen_closed_callback(void *group, int socket, void *param)
{
  ui_listen_t *ui_listen = (ui_listen_t*) param;

  LOG_WARNING("listen is closed, sending any remaining buffered data");

  ui_listen->closed_callback(ui_listen->callback_param);

  return SELECT_REMOVE;
}

ui_listen_t *ui_listen_create(select_group_t *group, data_callback_t *data_callback, simple_callback_t *closed_callback, void *callback_param)
{

  ui_listen_t *ui_listen = (ui_listen_t*) safe_malloc(sizeof(ui_listen_t));
  ui_listen->data_callback   = data_callback;
  ui_listen->closed_callback = closed_callback;
  ui_listen->callback_param  = callback_param;

  return ui_listen;
}

void ui_listen_destroy(ui_listen_t *ui_listen, select_group_t *group)
{
  safe_free(ui_listen);
}

void ui_listen_feed(ui_listen_t *ui_listen, uint8_t *data, size_t length)
{
  size_t i;

  for(i = 0; i < length; i++)
    fputc(data[i], stdout);
  fflush(stdout);
}
