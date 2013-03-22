#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdint.h>

#include "packet.h"
#include "select_group.h"

typedef void(send_t)(void *driver, uint8_t *data, size_t length);
typedef uint8_t*(recv_t)(void *driver, size_t *length, size_t max_length);
typedef void(close_t)(void *driver);
typedef void(cleanup_t)(void *driver);

typedef struct
{
  void      *driver;

  send_t    *driver_send;
  recv_t    *driver_recv;
  close_t   *driver_close;
  close_t   *driver_cleanup;

  size_t max_packet_size;

} driver_t;

driver_t *driver_get_tcp(char *host, uint16_t port, select_group_t *group);
void driver_destroy(driver_t *driver);

void     driver_send(driver_t *driver, uint8_t *data, size_t length);
void     driver_send_packet(driver_t *driver, packet_t *packet);
uint8_t  *driver_recv(driver_t *driver, size_t *length, size_t max_length);
packet_t *driver_recv_packet(driver_t *driver);

#endif
