#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "log.h"
#include "memory.h"

#ifdef WIN32
#include "pstdint.h"
#else
#include <stdint.h>
#endif

#include "command_packet.h"

/* Parse a packet from a byte stream. */
command_packet_t *command_packet_parse(uint8_t *data, size_t length, NBBOOL is_request)
{
  command_packet_t *p = safe_malloc(sizeof(command_packet_t));
  buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  p->request_id = buffer_read_next_int16(buffer);
  p->command_id = buffer_read_next_int16(buffer);
  if(!is_request)
    p->r.response.status = buffer_read_next_int16(buffer);

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

static command_packet_t *command_packet_create_response(uint16_t request_id, command_packet_type_t command_id, command_packet_status_t status)
{
  command_packet_t *p = command_packet_create_request(request_id, command_id);

  p->r.response.status = status;
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
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_PING, 0);

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
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_SHELL, 0);

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
  command_packet_t *packet = command_packet_create_response(request_id, COMMAND_EXEC, 0);

  packet->r.response.body.exec.session_id = session_id;

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
        printf("COMMAND_PING [request] :: request_id: 0x%04x :: data: %s", packet->request_id, packet->r.request.body.ping.data);
      else
        printf("COMMAND_PING [response] :: request_id: 0x%04x :: data: %s", packet->request_id, packet->r.response.body.ping.data);
      break;

    case COMMAND_SHELL:
      if(packet->is_request)
        printf("COMMAND_SHELL [request] :: request_id: 0x%04x :: name: %s", packet->request_id, packet->r.request.body.shell.name);
      else
        printf("COMMAND_SHELL [response] :: request_id: 0x%04x :: session_id: 0x%04x", packet->request_id, packet->r.response.body.shell.session_id);
      break;

    case COMMAND_EXEC:
      if(packet->is_request)
        printf("COMMAND_EXEC [request] :: request_id: 0x%04x :: name: %s :: command: %s", packet->request_id, packet->r.request.body.exec.name, packet->r.request.body.exec.command);
      else
        printf("COMMAND_EXEC [response] :: request_id: 0x%04x :: session_id: 0x%04x", packet->request_id, packet->r.response.body.exec.session_id);
      break;
  }
}

/* Needs to be freed with safe_free() */
uint8_t *command_packet_to_bytes(command_packet_t *packet, size_t *length, NBBOOL is_request)
{
  buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

  buffer_add_int16(buffer, packet->request_id);
  buffer_add_int16(buffer, packet->command_id);

  if(!is_request)
    buffer_add_int16(buffer, packet->r.response.status);

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

    default:
      LOG_FATAL("Unknown command_id: 0x%04x", packet->command_id);
      exit(1);
  }

  return buffer_create_string_and_destroy(buffer, length);
}
