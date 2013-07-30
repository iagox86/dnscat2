#ifndef __DRIVER_SOCKS4_H__
#define __DRIVER_SOCKS4_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  int             s;
  select_group_t *group;

  char           *host;
  uint16_t        port;
} driver_socks4_t;

driver_socks4_t *driver_socks4_create(select_group_t *group, char *host, uint16_t port);
void             driver_socks4_destroy();

#endif
