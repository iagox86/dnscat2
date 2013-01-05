#ifndef __DRIVER_TCP_H__
#define __DRIVER_TCP_H__

typedef struct
{
  int test;
} tcp_driver_t;

driver_t *tcp_get_driver(char *host, uint16_t port);

#endif
