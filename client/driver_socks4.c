#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "tcp.h"
#include "types.h"

#include "driver_socks4.h"

typedef struct _socks4_client_t
{
  int              in_socket;
  uint16_t         in_port;
  char            *in_host;

  uint16_t         out_port;
  char            *out_host;

  NBBOOL           socks_initialized;

  uint16_t         session_id;
  driver_socks4_t *driver;

  struct _socks4_client_t *next;
} client_entry_t;

static client_entry_t *first_client = NULL;

static SELECT_RESPONSE_t client_recv(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *c)
{
  client_entry_t *client = (client_entry_t*) c;

  if(client->socks_initialized)
  {
    /* If socks is initialized, just forward the data. */
    message_post_data_out(client->session_id, data, length);
  }
  else
  {
    /* If socks isn't initialized, handle the forward request. */
    buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);
    uint8_t version;
    uint8_t command_code;
    uint16_t port;
    uint32_t ip;
    char user_id[1024];
    char hostname[1024];

    buffer_t *buffer_out;
    uint8_t  *data_out;
    size_t    data_out_length;

    if(buffer_get_remaining_bytes(buffer) < 1)
    {
      LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'version'");
      return SELECT_CLOSE_REMOVE;
    }

    version = buffer_read_next_int8(buffer);

    if(version != 4)
    {
      LOG_ERROR("Invalid SOCKS4 request: We only support SOCKS4, but version %d was requested", version);
      return SELECT_CLOSE_REMOVE;
    }
    if(buffer_get_remaining_bytes(buffer) < 1)
    {
      LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'command_code'");
      return SELECT_CLOSE_REMOVE;
    }

    command_code = buffer_read_next_int8(buffer);

    if(command_code != 1)
    {
      LOG_ERROR("Invalid SOCKS4 request: We only support streaming, but port binding was requested");
      return SELECT_CLOSE_REMOVE;
    }
    if(buffer_get_remaining_bytes(buffer) < 2)
    {
      LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'port'");
      return SELECT_CLOSE_REMOVE;
    }

    port = buffer_read_next_int16(buffer);

    if(buffer_get_remaining_bytes(buffer) < 4)
    {
      LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'ip'");
      return SELECT_CLOSE_REMOVE;
    }

    ip = buffer_read_next_int32(buffer);

    if(!buffer_can_read_ntstring(buffer))
    {
      LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'user id'");
      return SELECT_CLOSE_REMOVE;
    }

    buffer_read_next_ntstring(buffer, user_id, 1024);

    /* Check if there's a hostname attached (ie, socks4a). */
    if(((ip & 0xFFFFFF00) == 0) && ((ip & 0x000000FF) != 0))
    {
      if(!buffer_can_read_ntstring(buffer))
      {
        LOG_ERROR("Invalid SOCKS4 request: not enough bytes to read 'hostname'");
        return SELECT_CLOSE_REMOVE;
      }

      buffer_read_next_ntstring(buffer, hostname, 1024);
    }
    else
    {
      sprintf(hostname, "%d.%d.%d.%d",
          (ip >> 24) & 0x000000FF,
          (ip >> 16) & 0x000000FF,
          (ip >>  8) & 0x000000FF,
          (ip >>  0) & 0x000000FF);

    }

    /* Create the session before responding. */
    client->session_id = message_post_create_session_with_tunnel(hostname, port);

    /* Mark it as initialized. */
    client->socks_initialized = TRUE;

    /* Respond. */
    buffer_out = buffer_create(BO_BIG_ENDIAN);
    buffer_add_int8(buffer_out, 0);
    buffer_add_int8(buffer_out, 0x5a); /* Granted. */
    buffer_add_int16(buffer_out, 0x0000); /* Ignored. */
    buffer_add_int32(buffer_out, 0x00000000); /* Ignored. */

    /* Convert the buffer to bytes. */
    data_out = buffer_create_string_and_destroy(buffer_out, &data_out_length);
    tcp_send(socket, data_out, data_out_length);
    safe_free(data_out);
  }

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
  LOG_FATAL("socks4 socket went away!");
  exit(1);

  return SELECT_CLOSE_REMOVE;
}

static SELECT_RESPONSE_t listener_accept(void *group, int s, void *d)
{
  driver_socks4_t *driver = (driver_socks4_t*) d;
  client_entry_t *client = safe_malloc(sizeof(client_entry_t));

  client->in_socket = tcp_accept(s, &client->in_host, &client->in_port);
  client->socks_initialized = FALSE;
  client->driver     = driver;
  client->next       = first_client;
  first_client       = client;

  LOG_WARNING("Received a connection from %s:%d (created session %d)", client->in_host, client->in_port, client->session_id);

  select_group_add_socket(group, client->in_socket, SOCKET_TYPE_STREAM, client);
  select_set_recv(group, client->in_socket, client_recv);
  select_set_closed(group, client->in_socket, client_closed);

  return SELECT_OK;
}

/* This is called after the drivers are created, to kick things off. */
static void handle_start(driver_socks4_t *driver)
{
  driver->s = tcp_listen(driver->host, driver->port);
  if(!driver->s)
  {
    LOG_FATAL("Failed to listen on %s:%d", driver->host, driver->port);
    exit(1);
  }

  /* On Linux, the stdin_handle is easy. */
  select_group_add_socket(driver->group, driver->s, SOCKET_TYPE_LISTEN, driver);
  select_set_listen(driver->group, driver->s, listener_accept);
  select_set_closed(driver->group, driver->s, listener_closed);
}

static void handle_session_closed(driver_socks4_t *driver, uint16_t session_id)
{
  client_entry_t *client;

  for(client = first_client; client; client = client->next)
  {
    if(client->session_id == session_id)
    {
      tcp_close(client->in_socket);
      return;
    }
  }

  LOG_WARNING("Couldn't find socks4 to close: session %d", session_id);
}

/* Note: This won't be used until after the SOCKS connection is established, because that's when the dnscat
 * connection actually starts. */
static void handle_data_in(driver_socks4_t *driver, uint16_t session_id, uint8_t *data, size_t length)
{
  client_entry_t *client;

  for(client = first_client; client; client = client->next)
  {
    if(client->session_id == session_id)
    {
      tcp_send(client->in_socket, data, length);
      return;
    }
  }

  LOG_WARNING("Couldn't find socks4 to send data to: %d bytes to session %d", length, session_id);
}

static void handle_shutdown()
{
  /* TODO: Clean up. */
}

static void handle_message(message_t *message, void *d)
{
  driver_socks4_t *driver = (driver_socks4_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver);
      break;

    case MESSAGE_SESSION_CLOSED:
      handle_session_closed(driver, message->message.session_closed.session_id);
      break;

    case MESSAGE_DATA_IN:
      handle_data_in(driver, message->message.data_in.session_id, message->message.data_in.data, message->message.data_in.length);
      break;

    case MESSAGE_SHUTDOWN:
      handle_shutdown();
      break;

    default:
      LOG_FATAL("driver_socks4 received an invalid message!");
      abort();
  }
}

driver_socks4_t *driver_socks4_create(select_group_t *group, char *host, uint16_t port)
{
  driver_socks4_t *driver = (driver_socks4_t*) safe_malloc(sizeof(driver_socks4_t));

  driver->group = group;
  driver->port = port;
  driver->host = host;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,           handle_message, driver);
  message_subscribe(MESSAGE_SESSION_CLOSED,  handle_message, driver);
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver);
  message_subscribe(MESSAGE_SHUTDOWN,        handle_message, driver);

  return driver;
}

void driver_socks4_destroy(driver_socks4_t *driver)
{
  safe_free(driver);
}
