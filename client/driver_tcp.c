/* driver_tcp.c
 * Created March/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

#include "buffer.h"
#include "memory.h"
#include "select_group.h"
#include "tcp.h"

#include "driver_tcp.h"

tcp_driver_t *tcp_driver_create(char *host, uint16_t port, select_group_t *group)
{
  tcp_driver_t *tcp_driver = (tcp_driver_t*)safe_malloc(sizeof(tcp_driver_t));
  tcp_driver->s     = -1;
  tcp_driver->host  = safe_strdup(host);
  tcp_driver->port  = port;
  tcp_driver->group = group;
  tcp_driver->buffer = buffer_create(BO_BIG_ENDIAN);

  return tcp_driver;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  tcp_driver_t *driver = (tcp_driver_t*)param;

  buffer_add_bytes(driver->buffer, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t closed_callback(void *group, int s, void *param)
{
  tcp_driver_t *driver = (tcp_driver_t*)param;

  printf("[[TCP]] :: Connection closed\n");

  driver_tcp_close(driver);

  return SELECT_OK;
}

void driver_tcp_send(void *driver, uint8_t *data, size_t length)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;
  buffer_t     *buffer;
  uint8_t      *encoded_data;
  size_t        encoded_length;

  if(d->s == -1)
  {
    /* Attempt a TCP connection */
    printf("[[TCP]] :: connecting to %s:%d\n", d->host, d->port);
    d->s = tcp_connect(d->host, d->port);

    /* If it fails, just return (it will try again next send) */
    if(d->s == -1)
    {
      printf("[[TCP]] :: connection failed!\n");
      return;
    }

    /* If it succeeds, add it to the select_group */
    select_group_add_socket(d->group, d->s, SOCKET_TYPE_STREAM, d);
    select_set_recv(d->group, d->s, recv_callback);
    select_set_closed(d->group, d->s, closed_callback);
  }

  printf("[[TCP]] :: send(%zu bytes)\n", length);

  assert(d->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int16(buffer, length);
  buffer_add_bytes(buffer, data, length);
  encoded_data = buffer_create_string_and_destroy(buffer, &encoded_length);

  if(tcp_send(d->s, encoded_data, encoded_length) == -1)
  {
    printf("[[TCP]] send error, closing socket!\n");
    driver_tcp_close(driver);
  }
}

uint8_t *driver_tcp_recv(void *driver, size_t *length, size_t max_length)
{
  uint8_t *ret;
  size_t expected_length;
  size_t returned_length;
  tcp_driver_t *d = (tcp_driver_t*) driver;

  if(buffer_get_remaining_bytes(d->buffer) >= 2)
  {
    expected_length = buffer_peek_next_int16(d->buffer);
    printf("Waiting for %zx bytes...\n", expected_length);
    printf("We have %zx bytes...\n", buffer_get_remaining_bytes(d->buffer) - 2);

    if(buffer_get_remaining_bytes(d->buffer) - 2 >= expected_length)
    {
      /* Consume the value we already know */
      buffer_read_next_int16(d->buffer);

      /* Read the rest of the buffer. */
      ret = buffer_read_remaining_bytes(d->buffer, &returned_length, expected_length);

      assert(expected_length == returned_length); /* Make sure the right number of bytes are returned by the buffer */

      *length = returned_length;

      buffer_print(d->buffer);
      return ret;
    }
  }

  /* By default, return NULL */
  *length = 0;
  return NULL;
}

void driver_tcp_close(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("[[TCP]] :: close()\n");

  assert(d->s && d->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  select_group_remove_and_close_socket(d->group, d->s);
  d->s = -1;
}

void driver_tcp_cleanup(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("[[TCP]] :: cleanup()\n");

  /* Ensure the driver is closed */
  if(d->s != -1)
    driver_tcp_close(driver);

  buffer_destroy(d->buffer);

  safe_free(d->host);
  d->host = NULL;
  safe_free(d);
}
