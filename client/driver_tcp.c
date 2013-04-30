/* driver_tcp.c
 * Created March/2013
 * By Ron Bowes
 */

#ifdef DNSCAT_TCP

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

#include "buffer.h"
#include "memory.h"
#include "select_group.h"
#include "tcp.h"

#include "driver.h"

driver_t *driver_tcp_create(char *host, uint16_t port, select_group_t *group)
{
  /* Set the tcp-specific options for the driver */
  driver_t *driver        = (driver_t *)safe_malloc(sizeof(driver_t));
  driver->max_packet_size = 1024;

  driver->s     = -1;
  driver->host  = safe_strdup(host);
  driver->port  = port;
  driver->group = group;
  driver->buffer = buffer_create(BO_BIG_ENDIAN);

  return driver;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_t *driver = (driver_t*)param;

  /* Cleanup - if the buffer is empty, reset it */
  if(buffer_get_remaining_bytes(driver->buffer) == 0)
    buffer_clear(driver->buffer);

  buffer_add_bytes(driver->buffer, data, length);

  /* If we have at least a length value */
  if(buffer_get_remaining_bytes(driver->buffer) >= 2)
  {
    /* Read the length. */
    uint16_t expected_length = buffer_peek_next_int16(driver->buffer);

    /* Check if we have the full length. */
    if(buffer_get_remaining_bytes(driver->buffer) - 2 >= expected_length)
    {
      uint8_t *data;
      size_t   returned_length;

      /* Consume the value we already know */
      buffer_read_next_int16(driver->buffer);

      /* Read the rest of the buffer. */
      data = buffer_read_remaining_bytes(driver->buffer, &returned_length, expected_length, TRUE);

      /* Sanity check. */
      assert(expected_length == returned_length);

      /* Do the callback. */
      driver->callback(data, returned_length, driver->callback_param);

      /* Free it. */
      safe_free(data);

      /* Clear the buffer if it's empty. */
      if(buffer_get_remaining_bytes(driver->buffer) == 0)
        buffer_clear(driver->buffer);
    }
  }

  return SELECT_OK;
}


static SELECT_RESPONSE_t closed_callback(void *group, int s, void *param)
{
  driver_t *driver = (driver_t*)param;

  printf("[[TCP]] :: Connection closed\n");

  driver_close(driver);

  return SELECT_OK;
}

void driver_send_packet(driver_t *driver, packet_t *packet)
{
  size_t   length;
  uint8_t *data = packet_to_bytes(packet, &length);

  driver_send(driver, data, length);

  safe_free(data);
}

void driver_send(driver_t *driver, uint8_t *data, size_t length)
{
  buffer_t     *buffer;
  uint8_t      *encoded_data;
  size_t        encoded_length;

  if(driver->s == -1)
  {
    /* Attempt a TCP connection */
    printf("[[TCP]] :: connecting to %s:%d\n", driver->host, driver->port);
    driver->s = tcp_connect(driver->host, driver->port);

    /* If it fails, just return (it will try again next send) */
    if(driver->s == -1)
    {
      printf("[[TCP]] :: connection failed!\n");
      return;
    }

    /* If it succeeds, add it to the select_group */
    select_group_add_socket(driver->group, driver->s, SOCKET_TYPE_STREAM, driver);
    select_set_recv(driver->group, driver->s, recv_callback);
    select_set_closed(driver->group, driver->s, closed_callback);
  }

  assert(driver->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int16(buffer, length);
  buffer_add_bytes(buffer, data, length);
  encoded_data = buffer_create_string_and_destroy(buffer, &encoded_length);

  if(tcp_send(driver->s, encoded_data, encoded_length) == -1)
  {
    printf("[[TCP]] send error, closing socket!\n");
    driver_close(driver);
  }
}

void driver_close(driver_t *driver)
{
  printf("[[TCP]] :: close()\n");

  assert(driver->s && driver->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  select_group_remove_and_close_socket(driver->group, driver->s);
  driver->s = -1;
}

void driver_destroy(driver_t *driver)
{
  printf("[[TCP]] :: cleanup()\n");

  /* Ensure the driver is closed */
  if(driver->s != -1)
    driver_close(driver);

  buffer_destroy(driver->buffer);

  safe_free(driver->host);
  driver->host = NULL;
  safe_free(driver);
}

void driver_register_callback(driver_t *driver, driver_callback_t *callback, void *callback_param)
{
  driver->callback       = callback;
  driver->callback_param = callback_param;
}

#endif
