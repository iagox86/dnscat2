/* command_packet.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "command_packet.h"

/* Parse a packet from a byte stream. */
command_packet_t *command_packet_parse(uint8_t *data, uint32_t length, NBBOOL is_request)
{
  command_packet_t *p = safe_malloc(sizeof(command_packet_t));
  buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  p->request_id = buffer_read_next_int16(buffer);
  p->command_id = (command_packet_type_t) buffer_read_next_int16(buffer);
  p->is_request = is_request;

  switch(p->command_id)
  {
    case COMMAND_PING:
      if(is_request)
        p->r.request.body.ping.data = buffer_alloc_next_ntstring(buffer);
      else
        p->r.response.body.ping.data = buffer_alloc_next_ntstring(buffer);
      break;

    case COMMAND_SHELL:
      if(is_request)
        p->r.request.body.shell.name = buffer_alloc_next_ntstring(buffer);
      else
        p->r.response.body.shell.session_id = buffer_read_next_int16(buffer);
      break;

    case COMMAND_EXEC:
      if(is_request)
      {
        p->r.request.body.exec.name    = buffer_alloc_next_ntstring(buffer);
        p->r.request.body.exec.command = buffer_alloc_next_ntstring(buffer);
      }
      else
      {
        p->r.response.body.exec.session_id = buffer_read_next_int16(buffer);
      }
      break;

    case COMMAND_DOWNLOAD:
      if(is_request)
      {
        p->r.request.body.download.filename = buffer_alloc_next_ntstring(buffer);
      }
      else
      {
        p->r.response.body.download.data = (uint8_t*)buffer_read_remaining_bytes(buffer, (size_t*)&p->r.response.body.download.length, -1, TRUE);
      }

      break;

    case COMMAND_UPLOAD:
      if(is_request)
      {
        p->r.request.body.upload.filename = buffer_alloc_next_ntstring(buffer);
        p->r.request.body.upload.data = buffer_read_remaining_bytes(buffer, (size_t*)&p->r.request.body.upload.length, -1, TRUE);
      }
      else
      {
      }
      break;

    case COMMAND_SHUTDOWN:
      break;

    case TUNNEL_CONNECT:
      if(is_request)
      {
        p->r.request.body.tunnel_connect.host = buffer_alloc_next_ntstring(buffer);
        p->r.request.body.tunnel_connect.port = buffer_read_next_int16(buffer);
      }
      else
      {
        p->r.response.body.tunnel_connect.tunnel_id = buffer_read_next_int32(buffer);
      }
      break;

    case TUNNEL_DATA:
      if(is_request)
      {
        p->r.request.body.tunnel_data.tunnel_id = buffer_read_next_int32(buffer);
        p->r.request.body.tunnel_data.data = buffer_read_remaining_bytes(buffer, (size_t*)&p->r.request.body.tunnel_data.length, -1, TRUE);
      }
      else
      {
        /* n/a */
      }
      break;

    case TUNNEL_CLOSE:
      if(is_request)
      {
        p->r.request.body.tunnel_data.tunnel_id = buffer_read_next_int32(buffer);
      }
      else
      {
        /* n/a */
      }
      break;

    case COMMAND_ERROR:
      if(is_request)
      {
        p->r.request.body.error.status = buffer_read_next_int16(buffer);
        p->r.request.body.error.reason = buffer_alloc_next_ntstring(buffer);
      }
      else
      {
        p->r.request.body.error.status = buffer_read_next_int16(buffer);
        p->r.request.body.error.reason = buffer_alloc_next_ntstring(buffer);
      }
      break;

    default:
      LOG_FATAL("Unknown command_id: 0x%04x", p->command_id);
      exit(1);
  }

  return p;
}

static command_packet_t *command_packet_create_request(uint16_t request_id, command_packet_type_t command_id)
{
  command_packet_t *p = safe_malloc(sizeof(command_packet_t));

  p->request_id = request_id;
  p->command_id = command_id;
  p->is_request = TRUE;

  return p;
}

static command_packet_t *command_packet_create_response(uint16_t request_id, command_packet_type_t command_id)
{
  command_packet_t *p = command_packet_create_request(request_id, command_id);

  p->is_request = FALSE;

  return p;
}

command_packet_t *command_packet_create_ping_request(uint16_t request_id, char *data)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_PING);

  packet->r.request.body.ping.data = safe_strdup(data);

  return packet;
}

command_packet_t *command_packet_create_ping_response(uint16_t request_id, char *data)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_PING);

  packet->r.response.body.ping.data = safe_strdup(data);

  return packet;
}

command_packet_t *command_packet_create_shell_request(uint16_t request_id, char *name)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_SHELL);

  packet->r.request.body.shell.name = safe_strdup(name);

  return packet;
}

command_packet_t *command_packet_create_shell_response(uint16_t request_id, uint16_t session_id)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_SHELL);

  packet->r.response.body.shell.session_id = session_id;

  return packet;
}

command_packet_t *command_packet_create_exec_request(uint16_t request_id, char *name, char *command)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_EXEC);

  packet->r.request.body.exec.name    = safe_strdup(name);
  packet->r.request.body.exec.command = safe_strdup(command);

  return packet;
}

command_packet_t *command_packet_create_exec_response(uint16_t request_id, uint16_t session_id)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_EXEC);

  packet->r.response.body.exec.session_id = session_id;

  return packet;
}

command_packet_t *command_packet_create_download_request(uint16_t request_id, char *filename)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_DOWNLOAD);

  packet->r.request.body.download.filename = safe_strdup(filename);

  return packet;
}

command_packet_t *command_packet_create_download_response(uint16_t request_id, uint8_t *data, uint32_t length)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_DOWNLOAD);
  packet->r.response.body.download.data = safe_malloc(length);
  memcpy(packet->r.response.body.download.data, data, length);
  packet->r.response.body.download.length = length;

  return packet;
}

command_packet_t *command_packet_create_upload_request(uint16_t request_id, char *filename, uint8_t *data, uint32_t length)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_UPLOAD);

  packet->r.request.body.upload.filename = safe_strdup(filename);
  packet->r.request.body.upload.data = safe_malloc(length);
  memcpy(packet->r.request.body.upload.data, data, length);
  packet->r.request.body.upload.length = length;

  return packet;
}

command_packet_t *command_packet_create_upload_response(uint16_t request_id)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_UPLOAD);

  return packet;
}

command_packet_t *command_packet_create_shutdown_response(uint16_t request_id)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_SHUTDOWN);

  return packet;
}

command_packet_t *command_packet_create_tunnel_connect_request(uint16_t request_id, char *host, uint16_t port)
{
  command_packet_t *packet = command_packet_create_request(request_id, TUNNEL_CONNECT);

  packet->r.request.body.tunnel_connect.host = safe_strdup(host);
  packet->r.request.body.tunnel_connect.port = port;

  return packet;
}

command_packet_t *command_packet_create_tunnel_connect_response(uint16_t request_id, uint32_t tunnel_id)
{
  command_packet_t *packet = command_packet_create_response(request_id, TUNNEL_CONNECT);

  packet->r.response.body.tunnel_connect.tunnel_id = tunnel_id;

  return packet;
}

command_packet_t *command_packet_create_tunnel_data_request(uint16_t request_id, uint32_t tunnel_id, uint8_t *data, uint32_t length)
{
  command_packet_t *packet = command_packet_create_request(request_id, TUNNEL_DATA);

  packet->r.request.body.tunnel_data.tunnel_id = tunnel_id;
  packet->r.request.body.tunnel_data.data = safe_malloc(length);
  memcpy(packet->r.request.body.tunnel_data.data, data, length);
  packet->r.request.body.tunnel_data.length = length;

  return packet;
}

command_packet_t *command_packet_create_tunnel_close_request(uint16_t request_id, uint32_t tunnel_id)
{
  command_packet_t *packet = command_packet_create_request(request_id, TUNNEL_CLOSE);

  packet->r.request.body.tunnel_close.tunnel_id = tunnel_id;

  return packet;
}

command_packet_t *command_packet_create_error_request(uint16_t request_id, uint16_t status, char *reason)
{
  command_packet_t *packet = command_packet_create_request(request_id, COMMAND_ERROR);

  packet->r.request.body.error.status = status;
  packet->r.request.body.error.reason = safe_strdup(reason);

  return packet;
}

command_packet_t *command_packet_create_error_response(uint16_t request_id, uint16_t status, char *reason)
{
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_ERROR);

  packet->r.response.body.error.status = status;
  packet->r.response.body.error.reason = safe_strdup(reason);

  return packet;
}

/* Free the packet data structures. */
void command_packet_destroy(command_packet_t *packet)
{
  switch(packet->command_id)
  {
    case COMMAND_PING:
      if(packet->is_request)
      {
        if(packet->r.request.body.ping.data)
          safe_free(packet->r.request.body.ping.data);
      }
      else
      {
        if(packet->r.response.body.ping.data)
          safe_free(packet->r.response.body.ping.data);
      }
      break;

    case COMMAND_SHELL:
      if(packet->is_request)
      {
        if(packet->r.request.body.shell.name)
          safe_free(packet->r.request.body.shell.name);
      }
      break;

    case COMMAND_EXEC:
      if(packet->is_request)
      {
        if(packet->r.request.body.exec.name)
          safe_free(packet->r.request.body.exec.name);
        if(packet->r.request.body.exec.command)
          safe_free(packet->r.request.body.exec.command);
      }
      break;

    case COMMAND_DOWNLOAD:
      if(packet->is_request)
      {
        if(packet->r.request.body.download.filename)
          safe_free(packet->r.request.body.download.filename);
      }
      else
      {
        if(packet->r.response.body.download.data)
          safe_free(packet->r.response.body.download.data);
      }
      break;

    case COMMAND_UPLOAD:
      if(packet->is_request)
      {
        if(packet->r.request.body.upload.filename)
          safe_free(packet->r.request.body.upload.filename);
        if(packet->r.request.body.upload.data)
          safe_free(packet->r.request.body.upload.data);
      }
      else
      {
      }
      break;

    case TUNNEL_CONNECT:
      if(packet->is_request)
      {
        if(packet->r.request.body.tunnel_connect.host)
          safe_free(packet->r.request.body.tunnel_connect.host);
      }
      else
      {
      }
      break;

    case TUNNEL_DATA:
      if(packet->is_request)
      {
        if(packet->r.request.body.tunnel_data.data)
          safe_free(packet->r.request.body.tunnel_data.data);
      }
      else
      {
      }
      break;

    case TUNNEL_CLOSE:
      if(packet->is_request)
      {
      }
      else
      {
      }
      break;

    case COMMAND_ERROR:
      if(packet->is_request)
      {
        if(packet->r.request.body.error.reason)
          safe_free(packet->r.request.body.error.reason);
      }
      break;


    default:
      LOG_FATAL("Unknown command_id: 0x%04x", packet->command_id);
      exit(1);
  }

  safe_free(packet);
}

/* Print the packet (debugging, mostly) */
void command_packet_print(command_packet_t *packet)
{
  switch(packet->command_id)
  {
    case COMMAND_PING:
      if(packet->is_request)
        printf("COMMAND_PING [request] :: request_id: 0x%04x :: data: %s\n", packet->request_id, packet->r.request.body.ping.data);
      else
        printf("COMMAND_PING [response] :: request_id: 0x%04x :: data: %s\n", packet->request_id, packet->r.response.body.ping.data);
      break;

    case COMMAND_SHELL:
      if(packet->is_request)
        printf("COMMAND_SHELL [request] :: request_id: 0x%04x :: name: %s\n", packet->request_id, packet->r.request.body.shell.name);
      else
        printf("COMMAND_SHELL [response] :: request_id: 0x%04x :: session_id: 0x%04x\n", packet->request_id, packet->r.response.body.shell.session_id);
      break;

    case COMMAND_EXEC:
      if(packet->is_request)
        printf("COMMAND_EXEC [request] :: request_id: 0x%04x :: name: %s :: command: %s\n", packet->request_id, packet->r.request.body.exec.name, packet->r.request.body.exec.command);
      else
        printf("COMMAND_EXEC [response] :: request_id: 0x%04x :: session_id: 0x%04x\n", packet->request_id, packet->r.response.body.exec.session_id);
      break;

    case COMMAND_DOWNLOAD:
      if(packet->is_request)
        printf("COMMAND_DOWNLOAD [request] :: request_id: 0x%04x :: filename: %s\n", packet->request_id, packet->r.request.body.download.filename);
      else
        printf("COMMAND_DOWNLOAD [response] :: request_id: 0x%04x :: data: 0x%x bytes\n", packet->request_id, (int)packet->r.response.body.download.length);
      break;

    case COMMAND_UPLOAD:
      if(packet->is_request)
        printf("COMMAND_UPLOAD [request] :: request_id: 0x%04x :: filename: %s :: data: 0x%x bytes\n", packet->request_id, packet->r.request.body.upload.filename, (int)packet->r.request.body.upload.length);
      else
        printf("COMMAND_UPLOAD [response] :: request_id: 0x%04x\n", packet->request_id);
      break;

    case COMMAND_SHUTDOWN:
      if(packet->is_request)
        printf("COMMAND_SHUTDOWN [request] :: request_id 0x%04x\n", packet->request_id);
      else
        printf("COMMAND_SHUTDOWN [response] :: request_id 0x%04x\n", packet->request_id);
      break;

    case TUNNEL_CONNECT:
      if(packet->is_request)
        printf("TUNNEL_CONNECT [request] :: request_id 0x%04x :: host %s :: port %d\n", packet->request_id, packet->r.request.body.tunnel_connect.host, packet->r.request.body.tunnel_connect.port);
      else
        printf("TUNNEL_CONNECT [response] :: request_id 0x%04x :: tunnel_id %d\n", packet->request_id, packet->r.response.body.tunnel_connect.tunnel_id);
      break;

    case TUNNEL_DATA:
      if(packet->is_request)
        printf("TUNNEL_DATA [request] :: request_id 0x%04x :: tunnel_id %d :: data %zd bytes\n", packet->request_id, packet->r.request.body.tunnel_data.tunnel_id, packet->r.request.body.tunnel_data.length);
      else
        printf("TUNNEL_DATA [response] :: request_id 0x%04x :: this shouldn't actually exist\n", packet->request_id);
      break;

    case TUNNEL_CLOSE:
      if(packet->is_request)
        printf("TUNNEL_CLOSE [request] :: request_id 0x%04x :: tunnel_id %d\n", packet->request_id, packet->r.request.body.tunnel_close.tunnel_id);
      else
        printf("TUNNEL_CLOSE [response] :: request_id 0x%04x :: this shouldn't actually exist\n", packet->request_id);
      break;

    case COMMAND_ERROR:
      if(packet->is_request)
        printf("COMMAND_ERROR [request] :: request_id: 0x%04x :: status: 0x%04x :: reason: %s\n", packet->request_id, packet->r.request.body.error.status, packet->r.request.body.error.reason);
      else
        printf("COMMAND_ERROR [response] :: request_id: 0x%04x :: status: 0x%04x :: reason: %s\n", packet->request_id, packet->r.response.body.error.status, packet->r.response.body.error.reason);
      break;
  }
}

/* Needs to be freed with safe_free() */
uint8_t *command_packet_to_bytes(command_packet_t *packet, size_t *length)
{
  buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_t *buffer_with_size = buffer_create(BO_BIG_ENDIAN);

  buffer_add_int16(buffer, packet->request_id);
  buffer_add_int16(buffer, packet->command_id);

  switch(packet->command_id)
  {
    case COMMAND_PING:
      if(packet->is_request)
        buffer_add_ntstring(buffer, packet->r.request.body.ping.data);
      else
        buffer_add_ntstring(buffer, packet->r.response.body.ping.data);

      break;

    case COMMAND_SHELL:
      if(packet->is_request)
        buffer_add_ntstring(buffer, packet->r.request.body.shell.name);
      else
        buffer_add_int16(buffer, packet->r.response.body.shell.session_id);
      break;

    case COMMAND_EXEC:
      if(packet->is_request)
      {
        buffer_add_ntstring(buffer, packet->r.request.body.exec.name);
        buffer_add_ntstring(buffer, packet->r.request.body.exec.command);
      }
      else
      {
        buffer_add_int16(buffer, packet->r.response.body.exec.session_id);
      }
      break;

    case COMMAND_DOWNLOAD:
      if(packet->is_request)
      {
        buffer_add_ntstring(buffer, packet->r.request.body.download.filename);
      }
      else
      {
        buffer_add_bytes(buffer, packet->r.response.body.download.data, packet->r.response.body.download.length);
      }
      break;

    case COMMAND_UPLOAD:
      if(packet->is_request)
      {
        buffer_add_ntstring(buffer, packet->r.request.body.upload.filename);
        buffer_add_bytes(buffer, packet->r.request.body.upload.data, packet->r.request.body.upload.length);
      }
      else
      {
      }
      break;

    case COMMAND_SHUTDOWN:
      break;

    case TUNNEL_CONNECT:
      if(packet->is_request)
      {
        buffer_add_ntstring(buffer, packet->r.request.body.tunnel_connect.host);
        buffer_add_int16(buffer, packet->r.request.body.tunnel_connect.port);
      }
      else
      {
        buffer_add_int32(buffer, packet->r.response.body.tunnel_connect.tunnel_id);
      }
      break;

    case TUNNEL_DATA:
      if(packet->is_request)
      {
        buffer_add_int32(buffer, packet->r.request.body.tunnel_data.tunnel_id);
        buffer_add_bytes(buffer, packet->r.request.body.tunnel_data.data, packet->r.request.body.tunnel_data.length);
      }
      else
      {
      }
      break;

    case TUNNEL_CLOSE:
      if(packet->is_request)
      {
        buffer_add_int32(buffer, packet->r.request.body.tunnel_close.tunnel_id);
      }
      else
      {
      }
      break;

    case COMMAND_ERROR:
      if(packet->is_request)
      {
        buffer_add_int16(buffer, packet->r.request.body.error.status);
        buffer_add_ntstring(buffer, packet->r.request.body.error.reason);
      }
      else
      {
        buffer_add_int16(buffer, packet->r.response.body.error.status);
        buffer_add_ntstring(buffer, packet->r.response.body.error.reason);
      }
      break;


    default:
      LOG_FATAL("Unknown command_id: 0x%04x", packet->command_id);
      exit(1);
  }

  buffer_add_int32(buffer_with_size, buffer_get_length(buffer));
  buffer_add_buffer(buffer_with_size, buffer);
  buffer_destroy(buffer);

  return buffer_create_string_and_destroy(buffer_with_size, length);
}
