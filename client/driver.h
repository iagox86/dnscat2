/* driver.h
 * By Ron Bowes
 * Created March, 2013
 *
 * (See LICENSE.txt)
 *
 * This is a general "driver" class for dnscat drivers. A driver is a specific
 * protocol implementation - be it TCP, UDP, DNS, etc - that knows how to wrap
 * and send packets in their own protocol.
 *
 * The 'driver' module (this) is a general way that dnscat can interface with
 * these protocols without having to know any details of the protocol.
 *
 * To add a driver, you'll need to add a function to driver.c and driver.h in
 * the same style as driver_get_tcp().
 */

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include <stdint.h>

#include "buffer.h"
#include "packet.h"
#include "select_group.h"
#include "session.h"

/* An instance of the 'driver' module. */
typedef struct
{
  /* This will be, for example, a tcp_driver_t, dns_driver_t, etc. */
  void      *driver;

  /* We have to define the max packet size at this level so the other classes
   * can truncate packets appropriately. */
  size_t max_packet_size;
  session_t *session;

#if defined DNSCAT_DNS
  int                s;
  char              *domain;
  char              *dns_host;
  uint16_t           dns_port;
  int                poll_rate;

#elif defined DNSCAT_TCP
  int       s;
  char     *host;
  uint16_t  port;

  /* This is for buffering data until we get a full packet */
  buffer_t *buffer;
#else
  /* This is what you'll get when you compile everything except the drivers. */
#endif
} driver_t;

/* Create an instance of driver_t for TCP connections. */
void driver_create(int argc, char *argv[]);

/* Destroy an instance of driver_t - this can be any protocol. */
void driver_destroy(driver_t *driver);

void driver_send(uint8_t *data, size_t length, void *d);

void driver_close(driver_t *driver);

#endif
