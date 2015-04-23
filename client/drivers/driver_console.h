/* driver_console.h
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#ifndef __DRIVER_CONSOLE_H__
#define __DRIVER_CONSOLE_H__

#include "libs/buffer.h"
#include "libs/select_group.h"

typedef struct
{
#if 0
  char      *name;
  char      *download;
  uint32_t   first_chunk;
#endif
  select_group_t *group;
  buffer_t       *outgoing_data;
  NBBOOL          is_shutdown;
} driver_console_t;

driver_console_t *driver_console_create(select_group_t *group);
void              driver_console_destroy(driver_console_t *driver);
void              driver_console_data_received(driver_console_t *driver, uint8_t *data, size_t length);
uint8_t          *driver_console_get_outgoing(driver_console_t *driver, size_t *length, size_t max_length);
void              driver_console_close(driver_console_t *driver);

#endif
