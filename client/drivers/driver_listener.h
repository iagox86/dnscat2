/* driver_listener.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * TODO!
 */

#ifndef __DRIVER_LISTENER_H__
#define __DRIVER_LISTENER_H__

#include "select_group.h"
#include "session.h"

typedef struct
{
  int             s;
  select_group_t *group;
  char           *host;
  char           *name;
  uint16_t        port;
} driver_listener_t;

driver_listener_t *driver_listener_create(select_group_t *group, char *host, int port, char *name);
void               driver_listener_destroy();

#endif
