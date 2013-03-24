#ifndef __DRIVER_DNS_H__
#define __DRIVER_DNS_H__

#include "buffer.h"
#include "driver.h"
#include "select_group.h"

typedef struct
{
  int       s;
  char     *dns_host;
  uint16_t  dns_port;

  select_group_t *group;
  buffer_t *incoming_data;
} dns_driver_t;

dns_driver_t *dns_driver_create(char *dns_host, uint16_t dns_port, select_group_t *group);
void driver_dns_send(void *driver, uint8_t *data, size_t length);
uint8_t *driver_dns_recv(void *driver, size_t *length, size_t max_length);
void driver_dns_close(void *driver);
void driver_dns_cleanup(void *driver);

#endif
