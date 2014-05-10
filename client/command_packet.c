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
    p->status = buffer_read_next_int16(buffer);

  switch(p->command_id)
  {
    case COMMAND_PING:
      p->body.ping.data = buffer_alloc_next_ntstring(buffer);
      break;

    case COMMAND_SHELL:
      p->body.shell.name = buffer_alloc_next_ntstring(buffer);
      break;

    case COMMAND_EXEC:
      p->body.exec.name    = buffer_alloc_next_ntstring(buffer);
      p->body.exec.command = buffer_alloc_next_ntstring(buffer);
      break;

    default:
      LOG_FATAL("Unknown command_id: 0x%04x", p->command_id);
      exit(1);
  }

  return p;
}

static command_packet_t *command_packet_create(uint16_t request_id, command_packet_type_t command_id, command_packet_status_t status)
{
  command_packet_t *p = safe_malloc(sizeof(command_packet_t));

  p->request_id = request_id;
  p->command_id = command_id;
  p->status = status;

  return p;
}

/* Create a packet with the given characteristics. */
command_packet_t *command_packet_create_ping(uint16_t request_id, command_packet_type_t command_id, command_packet_status_t status, char *data)
{
  command_packet_t *p = command_packet_create(request_id, command_id, status);

  p->body.ping.data = safe_strdup(data);

  return p;
}
command_packet_t *command_packet_create_shell(uint16_t request_id, command_packet_type_t command_id, command_packet_status_t status, char *name)
{
  command_packet_t *p = command_packet_create(request_id, command_id, status);

  p->body.shell.name = safe_strdup(name);

  return p;
}
command_packet_t *command_packet_create_exec(uint16_t request_id, command_packet_type_t command_id, command_packet_status_t status, char *name, char *command)
{
  command_packet_t *p = command_packet_create(request_id, command_id, status);

  p->body.exec.name    = safe_strdup(name);
  p->body.exec.command = safe_strdup(command);

  return p;
}

/* Free the packet data structures. */
void command_packet_destroy(command_packet_t *packet)
{
  switch(packet->command_id)
  {
    case COMMAND_PING:
      if(packet->body.ping.data)
        safe_free(packet->body.ping.data);
      break;

    case COMMAND_SHELL:
      if(packet->body.shell.name)
        safe_free(packet->body.shell.name);
      break;

    case COMMAND_EXEC:
      if(packet->body.exec.name)
        safe_free(packet->body.exec.name);
      if(packet->body.exec.command)
        safe_free(packet->body.exec.command);
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
      printf("COMMAND_PING :: request_id: 0x%04x :: status: 0x%04x :: data: %s", packet->request_id, packet->status, packet->body.ping.data);
      break;

    case COMMAND_SHELL:
      printf("COMMAND_SHELL :: request_id: 0x%04x :: status: 0x%04x :: name: %s", packet->request_id, packet->status, packet->body.shell.name);
      break;

    case COMMAND_EXEC:
      printf("COMMAND_EXEC :: request_id: 0x%04x :: status: 0x%04x :: name: %s :: command: %s", packet->request_id, packet->status, packet->body.exec.name, packet->body.exec.command);
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
    buffer_add_int16(buffer, packet->status);

  switch(packet->command_id)
  {
    case COMMAND_PING:
      buffer_add_ntstring(buffer, packet->body.ping.data);
      break;

    case COMMAND_SHELL:
      buffer_add_ntstring(buffer, packet->body.shell.name);
      break;

    case COMMAND_EXEC:
      buffer_add_ntstring(buffer, packet->body.exec.name);
      buffer_add_ntstring(buffer, packet->body.exec.command);
      break;

    default:
      LOG_FATAL("Unknown command_id: 0x%04x", packet->command_id);
      exit(1);
  }

  return buffer_create_string_and_destroy(buffer, length);
}
