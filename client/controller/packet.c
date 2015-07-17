/* packet.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"

#include "packet.h"

/* Header for snprintf(), since cygwin doesn't expose it on c89
 * programs. */
#ifndef __APPLE__
int snprintf(char *STR, size_t SIZE, const char *FORMAT, ...);
#endif

packet_t *packet_parse(uint8_t *data, size_t length, options_t options)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));
  buffer_t *buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  /* Validate the size */
  if(buffer_get_length(buffer) > MAX_PACKET_SIZE)
  {
    LOG_FATAL("Packet is too long: %zu bytes\n", buffer_get_length(buffer));
    exit(1);
  }

  packet->packet_id    = buffer_read_next_int16(buffer);
  packet->packet_type  = (packet_type_t) buffer_read_next_int8(buffer);
  packet->session_id   = buffer_read_next_int16(buffer);

  switch(packet->packet_type)
  {
    case PACKET_TYPE_SYN:
      packet->body.syn.seq     = buffer_read_next_int16(buffer);
      packet->body.syn.options = buffer_read_next_int16(buffer);
      break;

    case PACKET_TYPE_MSG:
      if(options & OPT_CHUNKED_DOWNLOAD)
      {
        packet->body.msg.options.chunked.chunk = buffer_read_next_int32(buffer);
      }
      else
      {
        packet->body.msg.options.normal.seq     = buffer_read_next_int16(buffer);
        packet->body.msg.options.normal.ack     = buffer_read_next_int16(buffer);
      }
      packet->body.msg.data    = buffer_read_remaining_bytes(buffer, &packet->body.msg.data_length, -1, FALSE);
      break;

    case PACKET_TYPE_FIN:
      packet->body.fin.reason = buffer_alloc_next_ntstring(buffer);

      break;

    case PACKET_TYPE_PING:
      packet->body.ping.data = buffer_alloc_next_ntstring(buffer);

      break;

    default:
      LOG_FATAL("Error: unknown message type (0x%02x)\n", packet->packet_type);
      exit(0);
  }

  buffer_destroy(buffer);

  return packet;
}

uint16_t packet_peek_session_id(uint8_t *data, size_t length)
{
  buffer_t *buffer = NULL;
  uint16_t session_id = -1;

  /* Create a buffer of the first 5 bytes. */
  if(length < 5)
  {
    LOG_FATAL("Packet is too short!\n");
    return -1;
  }

  /* Create a buffer with the first 5 bytes of data. */
  buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, 5);

  /* Discard packet_id. */
  buffer_consume(buffer, 2);

  /* Discard packet_type. */
  buffer_consume(buffer, 1);

  /* Finally, get the session_id. */
  session_id = buffer_read_next_int16(buffer);

  /* Kill the buffer. */
  buffer_destroy(buffer);

  /* Done! */
  return session_id;
}

packet_t *packet_create_syn(uint16_t session_id, uint16_t seq, options_t options)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type      = PACKET_TYPE_SYN;
  packet->packet_id        = rand() % 0xFFFF;
  packet->session_id       = session_id;
  packet->body.syn.seq     = seq;
  packet->body.syn.options = options;

  return packet;
}

packet_t *packet_create_msg_normal(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type                 = PACKET_TYPE_MSG;
  packet->packet_id                   = rand() % 0xFFFF;
  packet->session_id                  = session_id;
  packet->body.msg.options.normal.seq = seq;
  packet->body.msg.options.normal.ack = ack;
  packet->body.msg.data               = safe_memcpy(data, data_length);
  packet->body.msg.data_length        = data_length;

  return packet;
}

packet_t *packet_create_msg_chunked(uint16_t session_id, uint32_t chunk)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type                    = PACKET_TYPE_MSG;
  packet->packet_id                      = rand() % 0xFFFF;
  packet->session_id                     = session_id;
  packet->body.msg.options.chunked.chunk = chunk;
  packet->body.msg.data                  = safe_memcpy("", 0);
  packet->body.msg.data_length           = 0;

  return packet;
}

packet_t *packet_create_fin(uint16_t session_id, char *reason)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type     = PACKET_TYPE_FIN;
  packet->packet_id       = rand() % 0xFFFF;
  packet->session_id      = session_id;
  packet->body.fin.reason = safe_strdup(reason);

  return packet;
}

packet_t *packet_create_ping(uint16_t session_id, char *data)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type     = PACKET_TYPE_PING;
  packet->packet_id       = rand() % 0xFFFF;
  packet->session_id      = session_id;
  packet->body.ping.data  = safe_strdup(data);

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

void packet_syn_set_chunked_download(packet_t *packet)
{
  if(packet->packet_type != PACKET_TYPE_SYN)
  {
    LOG_FATAL("Attempted to set the 'download chunk' field of a non-SYN message\n");
    exit(1);
  }

  /* Just set the field, we don't need anything else. */
  packet->body.syn.options |= OPT_CHUNKED_DOWNLOAD;
}

void packet_syn_set_is_command(packet_t *packet)
{
  if(packet->packet_type != PACKET_TYPE_SYN)
  {
    LOG_FATAL("Attempted to set the 'is_command' field of a non-SYN message\n");
    exit(1);
  }

  /* Just set the field, we don't need anything else. */
  packet->body.syn.options |= OPT_COMMAND;
}

size_t packet_get_syn_size()
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_syn(0, 0, (options_t)0);
    uint8_t *data = packet_to_bytes(p, &size, (options_t)0);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

size_t packet_get_msg_size(options_t options)
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p;

    if(options & OPT_CHUNKED_DOWNLOAD)
      p = packet_create_msg_chunked(0, 0);
    else
      p = packet_create_msg_normal(0, 0, 0, (uint8_t *)"", 0);
    safe_free(packet_to_bytes(p, &size, options));
    packet_destroy(p);
  }

  return size;
}

size_t packet_get_fin_size(options_t options)
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_fin(0, "");
    uint8_t *data = packet_to_bytes(p, &size, options);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

size_t packet_get_ping_size()
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p = packet_create_ping(0, "");
    uint8_t *data = packet_to_bytes(p, &size, (options_t)0);
    safe_free(data);
    packet_destroy(p);
  }

  return size;
}

uint8_t *packet_to_bytes(packet_t *packet, size_t *length, options_t options)
{
  buffer_t *buffer = buffer_create(BO_BIG_ENDIAN);

  buffer_add_int16(buffer, packet->packet_id);
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
      if(options & OPT_CHUNKED_DOWNLOAD)
      {
        buffer_add_int32(buffer, packet->body.msg.options.chunked.chunk);
      }
      else
      {
        buffer_add_int16(buffer, packet->body.msg.options.normal.seq);
        buffer_add_int16(buffer, packet->body.msg.options.normal.ack);
      }
      buffer_add_bytes(buffer, packet->body.msg.data, packet->body.msg.data_length);
      break;

    case PACKET_TYPE_FIN:
      buffer_add_ntstring(buffer, packet->body.fin.reason);

      break;

    case PACKET_TYPE_PING:
      buffer_add_ntstring(buffer, packet->body.ping.data);

      break;

    default:
      LOG_FATAL("Error: Unknown message type: %u\n", packet->packet_type);
      exit(1);
  }

  return buffer_create_string_and_destroy(buffer, length);
}

char *packet_to_s(packet_t *packet, options_t options)
{
  /* This is ugly, but I don't have a good automatic "printf" allocator. */
  char *ret = safe_malloc(1024);

#ifdef WIN32
  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    _snprintf_s(ret, 1024, 1024, "Type = SYN :: [0x%04x] session = 0x%04x, seq = 0x%04x, options = 0x%04x", packet->packet_id, packet->session_id, packet->body.syn.seq, packet->body.syn.options);
  }
  else if(packet->packet_type == PACKET_TYPE_MSG)
  {
    if(options & OPT_CHUNKED_DOWNLOAD)
      _snprintf_s(ret, 1024, 1024, "Type = MSG :: [0x%04x] session = 0x%04x, chunk = 0x%04x", packet->packet_id, packet->session_id, packet->body.msg.options.chunked.chunk, packet->body.msg.data_length);
    else
      _snprintf_s(ret, 1024, 1024, "Type = MSG :: [0x%04x] session = 0x%04x, seq = 0x%04x, ack = 0x%04x", packet->packet_id, packet->session_id, packet->body.msg.options.normal.seq, packet->body.msg.options.normal.ack, packet->body.msg.data_length);
  }
  else if(packet->packet_type == PACKET_TYPE_FIN)
  {
    _snprintf_s(ret, 1024, 1024, "Type = FIN :: [0x%04x] session = 0x%04x :: %s", packet->packet_id, packet->session_id, packet->body.fin.reason);
  }
  else if(packet->packet_type == PACKET_TYPE_PING)
  {
    _snprintf_s(ret, 1024, 1024, "Type = PING :: [0x%04x] data = %s", packet->packet_id, packet->body.ping.data);
  }
  else
  {
    _snprintf_s(ret, 1024, 1024, "Unknown packet type!");
  }
#else
  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    snprintf(ret, 1024, "Type = SYN :: [0x%04x] session = 0x%04x, seq = 0x%04x, options = 0x%04x", packet->packet_id, packet->session_id, packet->body.syn.seq, packet->body.syn.options);
  }
  else if(packet->packet_type == PACKET_TYPE_MSG)
  {
    if(options & OPT_CHUNKED_DOWNLOAD)
      snprintf(ret, 1024, "Type = MSG :: [0x%04x] session = 0x%04x, chunk = 0x%04x, data = 0x%x bytes", packet->packet_id, packet->session_id, packet->body.msg.options.chunked.chunk, (unsigned int)packet->body.msg.data_length);
    else
      snprintf(ret, 1024, "Type = MSG :: [0x%04x] session = 0x%04x, seq = 0x%04x, ack = 0x%04x, data = 0x%x bytes", packet->packet_id, packet->session_id, packet->body.msg.options.normal.seq, packet->body.msg.options.normal.ack, (unsigned int)packet->body.msg.data_length);
  }
  else if(packet->packet_type == PACKET_TYPE_FIN)
  {
    snprintf(ret, 1024, "Type = FIN :: [0x%04x] session = 0x%04x :: %s", packet->packet_id, packet->session_id, packet->body.fin.reason);
  }
  else if(packet->packet_type == PACKET_TYPE_PING)
  {
    snprintf(ret, 1024, "Type = PING :: [0x%04x] data = %s", packet->packet_id, packet->body.ping.data);
  }
  else
  {
    snprintf(ret, 1024, "Unknown packet type!");
  }
#endif

  return ret;
}

void packet_print(packet_t *packet, options_t options)
{
  char *str = packet_to_s(packet, options);
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
    if(packet->body.msg.data)
      safe_free(packet->body.msg.data);
  }

  if(packet->packet_type == PACKET_TYPE_FIN)
  {
    if(packet->body.fin.reason)
      safe_free(packet->body.fin.reason);
  }

  if(packet->packet_type == PACKET_TYPE_PING)
  {
    if(packet->body.ping.data)
      safe_free(packet->body.ping.data);
  }

  safe_free(packet);
}

