/* driver.h
 * Created March/2013
 * By Ron Bowes
 */

#include <assert.h>

#include "driver_tcp.h"
#include "memory.h"

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

size_t driver_recv(driver_t *driver, uint8_t *buf, size_t buf_length)
{
  return driver->driver_recv(driver->driver, buf, buf_length);
}
