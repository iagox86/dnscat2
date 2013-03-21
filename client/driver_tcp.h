#ifndef __DRIVER_TCP_H__
#define __DRIVER_TCP_H__

#include "buffer.h"
#include "driver.h"
#include "select_group.h"

typedef struct
{
  int       s;
  char     *host;
  uint16_t  port;

  select_group_t *group;
  buffer_t *incoming_data;
} tcp_driver_t;

tcp_driver_t *tcp_driver_create(char *host, uint16_t port, select_group_t *group);
int driver_tcp_connect(void *driver);
void driver_tcp_send(void *driver, uint8_t *data, size_t length);
size_t driver_tcp_recv(void *driver, uint8_t *buf, size_t buf_length);
void driver_tcp_close(void *driver);
void driver_tcp_cleanup(void *driver);

#endif
