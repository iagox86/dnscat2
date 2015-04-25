/* driver_ping.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * This is a super simple drivers that just sends some set data to the
 * server, then verifies it when it comes back.
 */

#ifndef __DRIVER_PING_H__
#define __DRIVER_PING_H__

#include "libs/select_group.h"

typedef struct
{
  char   *data;
  NBBOOL  is_shutdown;
} driver_ping_t;

driver_ping_t *driver_ping_create(select_group_t *group);
void           driver_ping_destroy(driver_ping_t *driver);
void           driver_ping_data_received(driver_ping_t *driver, uint8_t *data, size_t length);
uint8_t       *driver_ping_get_outgoing(driver_ping_t *driver, size_t *length, size_t max_length);
void           driver_ping_close(driver_ping_t *driver);

#endif
