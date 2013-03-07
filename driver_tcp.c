#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include "memory.h"
#include "tcp.h"

#include "driver_tcp.h"

void driver_tcp_init(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  d->s = -1;
}

int driver_tcp_connect(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("DRIVER::TCP : connect()\n");

  assert(d->s == -1);
  assert(d->host);

  d->s = tcp_connect(d->host, d->port);

  /* Make the socket non-blocking. */
  tcp_set_nonblocking(d->s);

  return d->s != -1;
}

int driver_tcp_bind(void *driver)
{
  printf("driver_tcp_bind\n");
  exit(0);

  return TRUE;
}

int driver_tcp_listen(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("driver_tcp_listen\n");
  exit(0);

  assert(d->s == -1);
  assert(d->host);
  assert(d->port);

  d->s = tcp_listen(d->host, d->port);
}

int driver_tcp_send(void *driver, uint8_t *data, size_t length)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("DRIVER::TCP : send(%d bytes)\n", length);

  assert(d->s != -1);
  assert(data);
  assert(length > 0);

  return tcp_send(d->s, data, length);
}

int driver_tcp_recv(void *driver, uint8_t *buf, size_t buf_length)
{
  ssize_t len;
  tcp_driver_t *d = (tcp_driver_t*) driver;

  assert(d->s != -1);
  assert(buf);
  assert(buf_length > 0);

  len = tcp_recv(d->s, buf, buf_length);
  if(len == -1)
    if(errno == EAGAIN || errno == EWOULDBLOCK)
      len = 0;

  return len;
}

void driver_tcp_close(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("driver_tcp_close\n");
  exit(0);

  assert(d->s != -1); /* We can't close a closed socket */

  tcp_close(d->s);
  d->s = -1;
}

void driver_tcp_cleanup(void *driver)
{
  tcp_driver_t *d = (tcp_driver_t*) driver;

  printf("driver_tcp_cleanup\n");
  exit(0);

  assert(d->s == -1); /* Make sure we aren't cleaning up an active socket */
}

driver_t *tcp_get_driver(char *host, uint16_t port)
{
  tcp_driver_t *tcp_driver = safe_malloc(sizeof(tcp_driver_t));
  driver_t *driver = safe_malloc(sizeof(driver_t));

  tcp_driver->s    = -1;
  tcp_driver->host = safe_strdup(host);
  tcp_driver->port = port;

  driver->driver_init    = driver_tcp_init;
  driver->driver_connect = driver_tcp_connect;
  driver->driver_bind    = NULL;
  driver->driver_listen  = NULL;
  driver->driver_send    = driver_tcp_send;
  driver->driver_recv    = driver_tcp_recv;
  driver->driver_close   = NULL;
  driver->driver_cleanup = driver_tcp_cleanup;

  driver->driver = (void*) tcp_driver;
  driver->default_window_size = 1;
  driver->max_window_size = 1;
  driver->max_packet_size = 65535;

  return driver;
}
