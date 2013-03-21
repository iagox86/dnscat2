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
  tcp_driver->incoming_data = buffer_create(BO_BIG_ENDIAN);

  return tcp_driver;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  tcp_driver_t *driver = (tcp_driver_t*)param;

  printf("[[TCP]] :: Received %zu bytes", length);

  buffer_add_bytes(driver->incoming_data, data, length);
  buffer_print(driver->incoming_data);

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

  assert(d->s != -1);
  assert(data);
  assert(length > 0);

  /* TODO: Send length */
  if(tcp_send(d->s, data, length) == -1)
  {
    printf("[[TCP]] send error, closing socket!\n");
    driver_tcp_close(driver);
  }
}

/* TODO: This should only receive from the buffer, and only if it's a full packet */
size_t driver_tcp_recv(void *driver, uint8_t *buf, size_t buf_length)
{
  ssize_t len;
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("[[TCP] :: recv(up to %zu bytes)\n", buf_length);

  assert(d->s != -1);
  assert(buf);
  assert(buf_length > 0);

  /* TODO: Receive length */
  len = tcp_recv(d->s, buf, buf_length);
  if(len == -1)
  {
    printf("[[TCP]] :: Error: couldn't receive data, closing connection\n");
    driver_tcp_close(driver);

    return -1;
  }

  return len;
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

  buffer_destroy(d->incoming_data);

  safe_free(d->host);
  d->host = NULL;
  safe_free(d);
}
