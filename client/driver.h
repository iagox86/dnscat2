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

#include "packet.h"
#include "select_group.h"

/* Define the callback function types that each driver needs to implement. */
typedef void(send_t)(void *driver, uint8_t *data, size_t length);
typedef uint8_t*(recv_t)(void *driver, size_t *length, size_t max_length);
typedef void(close_t)(void *driver);
typedef void(cleanup_t)(void *driver);

/* An instance of the 'driver' module. */
typedef struct
{
  /* This will be, for example, a tcp_driver_t, dns_driver_t, etc. */
  void      *driver;

  /* The callback functions - these must be filled in when the driver is
   * created. */
  send_t    *driver_send;
  recv_t    *driver_recv;
  close_t   *driver_close;
  close_t   *driver_cleanup;

  /* We have to define the max packet size at this level so the other classes
   * can truncate packets appropriately. */
  size_t max_packet_size;

} driver_t;

/* Create an instance of driver_t for TCP connections. */
driver_t *driver_get_tcp(char *host, uint16_t port, select_group_t *group);

/* Destroy an instance of driver_t - this can be any protocol. */
void driver_destroy(driver_t *driver);

/* Send or receive data - as a byte array - through the driver. (Note: after
 * calling driver_recv(), use safe_free() to free the memory.) */
void     driver_send(driver_t *driver, uint8_t *data, size_t length);
uint8_t  *driver_recv(driver_t *driver, size_t *length, size_t max_length);

/* Send or receive data - as a packet_t object - through the driver. (Note:
 * after calling driver_recv_packet(), use packet_destroy() to delete it.) */
void     driver_send_packet(driver_t *driver, packet_t *packet);
packet_t *driver_recv_packet(driver_t *driver);

#endif
