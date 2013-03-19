#ifndef __DRIVER_TCP_H__
#define __DRIVER_TCP_H__

#include "driver.h"

typedef struct
{
  int       s;
  char     *host;
  uint16_t  port;
} tcp_driver_t;

driver_t *tcp_get_driver(char *host, uint16_t port);

#endif
