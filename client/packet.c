#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "packet.h"

/* Header for snprintf(), since cygwin doesn't expose it on c89
 * programs. */
int snprintf(char *STR, size_t SIZE, const char *FORMAT, ...);

packet_t *packet_parse(uint8_t *data, size_t length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));
  buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  /* Validate the size */
  if(buffer_get_length(buffer) > MAX_PACKET_SIZE)
  {
    LOG_FATAL("Packet is too long: %zu bytes\n", buffer_get_length(buffer));
    exit(1);
  }

  packet->packet_type = buffer_read_next_int8(buffer);
  packet->session_id   = buffer_read_next_int16(buffer);

  switch(packet->packet_type)
  {
    case PACKET_TYPE_SYN:
      packet->body.syn.seq     = buffer_read_next_int16(buffer);
      packet->body.syn.options = buffer_read_next_int16(buffer);
      break;

    case PACKET_TYPE_MSG:
      packet->body.msg.seq     = buffer_read_next_int16(buffer);
      packet->body.msg.ack     = buffer_read_next_int16(buffer);
      packet->body.msg.data    = buffer_read_remaining_bytes(buffer, &packet->body.msg.data_length, -1, FALSE);
      break;

    case PACKET_TYPE_FIN:
      /* Do nothing */
      break;

    default:
      LOG_FATAL("Error: unknown message type (0x%02x)\n", packet->packet_type);
      exit(0);
  }

  buffer_destroy(buffer);

  return packet;
}

packet_t *packet_create_syn(uint16_t session_id, uint16_t seq, uint16_t options)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));
  packet->packet_type     = PACKET_TYPE_SYN;
  packet->session_id       = session_id;
  packet->body.syn.seq     = seq;
  packet->body.syn.options = options;

  return packet;
}

packet_t *packet_create_msg(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type         = PACKET_TYPE_MSG;
  packet->session_id           = session_id;
  packet->body.msg.seq         = seq;
  packet->body.msg.ack         = ack;
  packet->body.msg.data        = safe_memcpy(data, data_length);
  packet->body.msg.data_length = data_length;

  return packet;
}

packet_t *packet_create_fin(uint16_t session_id)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type     = PACKET_TYPE_FIN;
  packet->session_id       = session_id;

  return packet;
}

void packet_syn_set_name(packet_t *packet, char *name)
{
  if(packet->packet_type != PACKET_TYPE_SYN)
  {
    LOG_FATAL("Attempted to set the 'name' field of a non-SYN message\n");
    exit(1);
  }

  /* Free the name if it's already set */
  if(packet->body.syn.name)
    safe_free(packet->body.syn.name);

  packet->body.syn.options |= OPT_NAME;
  packet->body.syn.name = safe_strdup(name);
}

void packet_syn_set_download(packet_t *packet, char *filename)
{
  if(packet->packet_type != PACKET_TYPE_SYN)
  {
    LOG_FATAL("Attempted to set the 'download' field of a non-SYN message\n");
    exit(1);
  }

  /* Free the name if it's already set */
  if(packet->body.syn.filename)
    safe_free(packet->body.syn.filename);

  packet->body.syn.options |= OPT_DOWNLOAD;
  packet->body.syn.filename = safe_strdup(filename);
}

size_t packet_get_syn_size()
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_syn(0, 0, 0);
    uint8_t *data = packet_to_bytes(p, &size);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

size_t packet_get_msg_size()
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_msg(0, 0, 0, (uint8_t *)"", 0);
    uint8_t *data = packet_to_bytes(p, &size);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

size_t packet_get_fin_size()
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_fin(0);
    uint8_t *data = packet_to_bytes(p, &size);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

uint8_t *packet_to_bytes(packet_t *packet, size_t *length)
{
  buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

  buffer_add_int8(buffer, packet->packet_type);
  buffer_add_int16(buffer, packet->session_id);

  switch(packet->packet_type)
  {
    case PACKET_TYPE_SYN:
      buffer_add_int16(buffer, packet->body.syn.seq);
      buffer_add_int16(buffer, packet->body.syn.options);

      if(packet->body.syn.options & OPT_NAME)
      {
        buffer_add_ntstring(buffer, packet->body.syn.name);
      }
      if(packet->body.syn.options & OPT_DOWNLOAD)
      {
        buffer_add_ntstring(buffer, packet->body.syn.filename);
      }

      break;

    case PACKET_TYPE_MSG:
      buffer_add_int16(buffer, packet->body.msg.seq);
      buffer_add_int16(buffer, packet->body.msg.ack);
      buffer_add_bytes(buffer, packet->body.msg.data, packet->body.msg.data_length);
      break;

    case PACKET_TYPE_FIN:
      /* Do nothing */
      break;

    default:
      LOG_FATAL("Error: Unknown message type: %u\n", packet->packet_type);
      exit(1);
  }

  return buffer_create_string_and_destroy(buffer, length);
}

char *packet_to_s(packet_t *packet)
{
  /* This is ugly, but I don't have a good automatic "printf" allocator. */
  char *ret = safe_malloc(1024);

  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    snprintf(ret, 1024, "Type = SYN :: session = 0x%04x, seq = 0x%04x, options = 0x%04x", packet->session_id, packet->body.syn.seq, packet->body.syn.options);
  }
  else if(packet->packet_type == PACKET_TYPE_MSG)
  {
    snprintf(ret, 1024, "Type = MSG :: session = 0x%04x, seq = 0x%04x, ack = 0x%04x", packet->session_id, packet->body.msg.seq, packet->body.msg.ack);
  }
  else if(packet->packet_type == PACKET_TYPE_FIN)
  {
    snprintf(ret, 1024, "Type = FIN :: session = 0x%04x", packet->session_id);
  }
  else
  {
    snprintf(ret, 1024, "Unknown packet type!");
  }

  return ret;
}

void packet_print(packet_t *packet)
{
  char *str = packet_to_s(packet);
  printf("%s\n", str);
  safe_free(str);
}

void packet_destroy(packet_t *packet)
{
  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    if(packet->body.syn.name)
      safe_free(packet->body.syn.name);
    if(packet->body.syn.filename)
      safe_free(packet->body.syn.filename);
  }

  if(packet->packet_type == PACKET_TYPE_MSG)
  {
    safe_free(packet->body.msg.data);
  }

  safe_free(packet);
}

