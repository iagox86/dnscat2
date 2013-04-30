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

/* A callback that lets the main process be alerted when a new packet
 * arrives. */
typedef void(driver_callback_t)(uint8_t *data, size_t length, void *param);

/* An instance of the 'driver' module. */
typedef struct
{
  /* This will be, for example, a tcp_driver_t, dns_driver_t, etc. */
  void      *driver;

  /* We have to define the max packet size at this level so the other classes
   * can truncate packets appropriately. */
  size_t max_packet_size;

  driver_callback_t *callback;
  void              *callback_param;

#if defined DNSCAT_DNS
  int                s;
  char              *domain;
  char              *dns_host;
  uint16_t           dns_port;
  select_group_t    *group;
  int                poll_rate;


#elif defined DNSCAT_TCP
  int       s;
  char     *host;
  uint16_t  port;

  select_group_t *group;

  /* This is for buffering data until we get a full packet */
  buffer_t *buffer;
#else
  /* This is what you'll get when you compile everything except the drivers. */
#endif
} driver_t;

/* Create an instance of driver_t for TCP connections. */
driver_t *driver_create(int argc, char *argv[], select_group_t *group);

/* Destroy an instance of driver_t - this can be any protocol. */
void driver_destroy(driver_t *driver);

/* Send or receive data - as a byte array - through the driver. (Note: after
 * calling driver_recv(), use safe_free() to free the memory.) */
void driver_send(driver_t *driver, uint8_t *data, size_t length);

/* Send or receive data - as a packet_t object - through the driver. (Note:
 * after calling driver_recv_packet(), use packet_destroy() to delete it.) */
void driver_send_packet(driver_t *driver, packet_t *packet);

void driver_register_callback(driver_t *driver, driver_callback_t *callback, void *callback_param);
void driver_close(driver_t *driver);

#endif
