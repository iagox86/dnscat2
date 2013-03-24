/* driver.h
 * Created March/2013
 * By Ron Bowes
 */

#include <assert.h>

#include "memory.h"
#include "packet.h"

#include "driver_dns.h"
#include "driver_tcp.h"

#include "driver.h"

driver_t *driver_get_tcp(char *host, uint16_t port, select_group_t *group)
{
  /* Set the tcp-specific options for the driver */
  driver_t *driver        = safe_malloc(sizeof(driver_t));
  driver->driver_send     = driver_tcp_send;
  driver->driver_recv     = driver_tcp_recv;
  driver->driver_close    = driver_tcp_close;
  driver->driver_cleanup  = driver_tcp_cleanup;
  driver->max_packet_size = 1024;
  driver->driver          = tcp_driver_create(host, port, group);

  /* Set the tcp_driver_t options */

  return driver;
}

driver_t *driver_get_dns(char *host, uint16_t port, select_group_t *group)
{
  /* Set the tcp-specific options for the driver */
  driver_t *driver        = safe_malloc(sizeof(driver_t));
  driver->driver_send     = driver_dns_send;
  driver->driver_recv     = driver_dns_recv;
  driver->driver_close    = driver_dns_close;
  driver->driver_cleanup  = driver_dns_cleanup;
  driver->max_packet_size = 32;
  driver->driver          = dns_driver_create(host, port, group);


  return driver;
}

void driver_destroy(driver_t *driver)
{
  driver->driver_cleanup(driver->driver);
  safe_free(driver);
}

void driver_send(driver_t *driver, uint8_t *data, size_t length)
{
  assert(length <= driver->max_packet_size); /* Make sure we aren't sending a packet that's too big */
  driver->driver_send(driver->driver, data, length);
}

void driver_send_packet(driver_t *driver, packet_t *packet)
{
  uint8_t *bytes;
  size_t   length;

  bytes = packet_to_bytes(packet, &length);
  driver_send(driver, bytes, length);
  safe_free(bytes);
}

uint8_t *driver_recv(driver_t *driver, size_t *length, size_t max_length)
{
  return driver->driver_recv(driver->driver, length, max_length);
}

packet_t *driver_recv_packet(driver_t *driver)
{
  uint8_t  *bytes  = NULL;
  size_t    length = 0;
  packet_t *packet = NULL;

  bytes = driver_recv(driver, &length, -1);
  if(bytes)
  {
    packet = packet_parse(bytes, length);
    safe_free(bytes);
  }

  return packet;
}

