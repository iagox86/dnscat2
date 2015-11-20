/* driver_listener.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/tcp.h"
#include "libs/types.h"

#include "driver_listener.h"

typedef struct _listener_client_t
{
  int                s;
  char              *address;
  uint16_t           port;
  uint16_t           id;
  driver_listener_t *driver;

  struct _listener_client_t *next;
} client_entry_t;

static uint32_t tunnel_id = 0;
static client_entry_t *first_client = NULL;

static SELECT_RESPONSE_t client_recv(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *c)
{
  client_entry_t *client = (client_entry_t*) c;

  return SELECT_OK;
}

static SELECT_RESPONSE_t client_closed(void *group, int socket, void *c)
{
  client_entry_t *client = (client_entry_t*) c;

  message_post_close_session(client->session_id);

  /* TODO: Unlink it from the entry list. */

  return SELECT_CLOSE_REMOVE;
}

static SELECT_RESPONSE_t listener_closed(void *group, int socket, void *c)
{
  LOG_FATAL("Listener socket went away!");
  exit(1);

  return SELECT_CLOSE_REMOVE;
}

static SELECT_RESPONSE_t listener_accept(void *group, int s, void *d)
{
  driver_listener_t *driver = (driver_listener_t*) d;
  client_entry_t    *client = safe_malloc(sizeof(client_entry_t));

  client->s = tcp_accept(s, &client->address, &client->port);

  client->driver     = driver;
  client->next       = first_client;
  first_client       = client;

  LOG_WARNING("Received a connection from %s:%d (created session %d)", client->address, client->port, client->session_id);

  select_group_add_socket(group, client->s, SOCKET_TYPE_STREAM, client);
  select_set_recv(group, client->s, client_recv);
  select_set_closed(group, client->s, client_closed);

  return SELECT_OK;
}

uint8_t *driver_listener_get_outgoing(driver_console_t *driver, size_t *length, size_t max_length)
{
  return NULL;
}

driver_listener_t *driver_listener_create(select_group_t *group, char *host, int port, char *name)
{
  driver_listener_t *driver = (driver_listener_t*) safe_malloc(sizeof(driver_listener_t));

  driver->group = group;
  driver->host  = host;
  driver->port  = port;
  driver->s     = tcp_listen(driver->host, driver->port);

  if(!driver->s)
  {
    LOG_FATAL("Failed to listen on %s:%d", driver->host, driver->port);
    exit(1);
  }

  /* On Linux, the stdin_handle is easy. */
  select_group_add_socket(driver->group, driver->s, SOCKET_TYPE_LISTEN, driver);
  select_set_listen(driver->group, driver->s, listener_accept);
  select_set_closed(driver->group, driver->s, listener_closed);

  return driver;
}

void driver_listener_destroy(driver_listener_t *driver)
{
  safe_free(driver);
}
