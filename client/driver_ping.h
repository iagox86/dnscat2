/* driver_ping.h
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#ifndef __DRIVER_PING_H__
#define __DRIVER_PING_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  char *data;
} driver_ping_t;

driver_ping_t  *driver_ping_create(select_group_t *group);
void            driver_ping_destroy();

#endif
