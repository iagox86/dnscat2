/* driver_command.h
 * By Ron Bowes
 * Created May, 2014
 *
 * See LICENSE.md
 */

#ifndef __DRIVER_command_H__
#define __DRIVER_command_H__

#include "command_packet.h"
#include "libs/ll.h"
#include "libs/select_group.h"
#include "libs/types.h"

typedef struct
{
  char           *name;
  uint16_t        session_id;
  buffer_t       *stream;
  select_group_t *group;
  buffer_t       *outgoing_data;
  NBBOOL          is_shutdown;
  ll_t           *tunnels;
} driver_command_t;

driver_command_t *driver_command_create(select_group_t *group);
void driver_command_destroy(driver_command_t *driver);
void driver_command_data_received(driver_command_t *driver, uint8_t *data, size_t length);
uint8_t *driver_command_get_outgoing(driver_command_t *driver, size_t *length, size_t max_length);
void driver_command_close(driver_command_t *driver);

#endif
