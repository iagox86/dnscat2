/* driver_tcp.h
 * By Ron Bowes
 * March, 2013
 *
 * A TCP implementation of the dnscat protocol. This is primarily for testing,
 * because dnscat-over-TCP is kinda retarded.
 *
 * This uses a small wrapper - it prepends a 2-byte size field to every packet
 * sent and received, and buffers data until it gets a full packet. This is
 * required because TCP is stream-based, whereas dnscat is datagram-based.
 */

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

  /* These are for buffering data until we get a full packet */
  buffer_t *buffer;
} tcp_driver_t;

/* Create a driver */
tcp_driver_t *tcp_driver_create(char *host, uint16_t port, select_group_t *group);

/* The four callback functions required for a driver to function. */
void driver_tcp_send(void *driver, uint8_t *data, size_t length);
uint8_t *driver_tcp_recv(void *driver, size_t *length, size_t max_length);
void driver_tcp_close(void *driver);
void driver_tcp_cleanup(void *driver);

#endif
