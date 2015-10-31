/* packet.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"

#include "packet.h"

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
      if(packet->body.syn.options & OPT_NAME)
        packet->body.syn.name = buffer_alloc_next_ntstring(buffer);
      break;

    case PACKET_TYPE_MSG:
      packet->body.msg.seq     = buffer_read_next_int16(buffer);
      packet->body.msg.ack     = buffer_read_next_int16(buffer);
      packet->body.msg.data    = buffer_read_remaining_bytes(buffer, &packet->body.msg.data_length, -1, FALSE);
      break;

    case PACKET_TYPE_FIN:
      packet->body.fin.reason = buffer_alloc_next_ntstring(buffer);
      break;

    case PACKET_TYPE_PING:
      packet->body.ping.data = buffer_alloc_next_ntstring(buffer);
      break;

#ifndef NO_ENCRYPTION
    case PACKET_TYPE_ENC:
      packet->body.enc.subtype = buffer_read_next_int16(buffer);
      packet->body.enc.flags   = buffer_read_next_int16(buffer);

      switch(packet->body.enc.subtype)
      {
        case PACKET_ENC_SUBTYPE_INIT:
          buffer_read_next_bytes(buffer, packet->body.enc.public_key, 64);
          break;
        case PACKET_ENC_SUBTYPE_AUTH:
          buffer_read_next_bytes(buffer, packet->body.enc.authenticator, 32);
          break;
      }
      break;
#endif

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

packet_t *packet_create_msg(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type          = PACKET_TYPE_MSG;
  packet->packet_id            = rand() % 0xFFFF;
  packet->session_id           = session_id;
  packet->body.msg.seq         = seq;
  packet->body.msg.ack         = ack;
  packet->body.msg.data        = safe_memcpy(data, data_length);
  packet->body.msg.data_length = data_length;

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

#ifndef NO_ENCRYPTION
packet_t *packet_create_enc(uint16_t session_id, uint16_t flags)
{
  packet_t *packet = (packet_t*) safe_malloc(sizeof(packet_t));

  packet->packet_type = PACKET_TYPE_ENC;
  packet->packet_id   = rand() % 0xFFFF;
  packet->session_id  = session_id;
  packet->body.enc.subtype     = -1;

  return packet;
}

void packet_enc_set_init(packet_t *packet, uint8_t *public_key)
{
  if(packet->packet_type != PACKET_TYPE_ENC)
  {
    LOG_FATAL("Attempted to set encryption options for a non-ENC message\n");
    exit(1);
  }

  packet->body.enc.subtype = PACKET_ENC_SUBTYPE_INIT;
  memcpy(packet->body.enc.public_key, public_key, 64);
}

void packet_enc_set_auth(packet_t *packet, uint8_t *authenticator)
{
  if(packet->packet_type != PACKET_TYPE_ENC)
  {
    LOG_FATAL("Attempted to set encryption options for a non-ENC message\n");
    exit(1);
  }

  packet->body.enc.subtype = PACKET_ENC_SUBTYPE_AUTH;
  memcpy(packet->body.enc.authenticator, authenticator, 32);
}
#endif

size_t packet_get_msg_size(options_t options)
{
  static size_t size = 0;

  /* If the size isn't known yet, calculate it. */
  if(size == 0)
  {
    packet_t *p;

    p = packet_create_msg(0, 0, 0, (uint8_t *)"", 0);
    safe_free(packet_to_bytes(p, &size, options));
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

/* TODO: This is a little hacky - converting it to a bytestream and back to
 * clone - but it's by far the easiest way! */
packet_t *packet_clone(packet_t *packet, options_t options)
{
  uint8_t  *packet_bytes  = NULL;
  size_t    packet_length = -1;
  packet_t *result;

  packet_bytes = packet_to_bytes(packet, &packet_length, options);
  result = packet_parse(packet_bytes, packet_length, options);
  safe_free(packet_bytes);
  return result;
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
        buffer_add_ntstring(buffer, packet->body.syn.name);

      break;

    case PACKET_TYPE_MSG:
      buffer_add_int16(buffer, packet->body.msg.seq);
      buffer_add_int16(buffer, packet->body.msg.ack);
      buffer_add_bytes(buffer, packet->body.msg.data, packet->body.msg.data_length);
      break;

    case PACKET_TYPE_FIN:
      buffer_add_ntstring(buffer, packet->body.fin.reason);
      break;

    case PACKET_TYPE_PING:
      buffer_add_ntstring(buffer, packet->body.ping.data);
      break;

#ifndef NO_ENCRYPTION
    case PACKET_TYPE_ENC:
      buffer_add_int16(buffer, packet->body.enc.subtype);
      buffer_add_int16(buffer, packet->body.enc.flags);

      if(packet->body.enc.subtype == PACKET_ENC_SUBTYPE_INIT)
      {
        buffer_add_bytes(buffer, packet->body.enc.public_key, 64);
      }
      else if(packet->body.enc.subtype == PACKET_ENC_SUBTYPE_AUTH)
      {
        buffer_add_bytes(buffer, packet->body.enc.authenticator, 32);
      }
      else if(packet->body.enc.subtype == -1)
      {
        LOG_FATAL("Error: One of the packet_enc_set_*() functions have to be called!");
        exit(1);
      }
      else
      {
        LOG_FATAL("Error: Unknown encryption subtype: 0x%04x", packet->body.enc.subtype);
        exit(1);
      }
      break;
#endif

    default:
      LOG_FATAL("Error: Unknown message type: %u\n", packet->packet_type);
      exit(1);
  }

  return buffer_create_string_and_destroy(buffer, length);
}

void packet_print(packet_t *packet, options_t options)
{
  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    printf("Type = SYN :: [0x%04x] session = 0x%04x, seq = 0x%04x, options = 0x%04x", packet->packet_id, packet->session_id, packet->body.syn.seq, packet->body.syn.options);
  }
  else if(packet->packet_type == PACKET_TYPE_MSG)
  {
    printf("Type = MSG :: [0x%04x] session = 0x%04x, seq = 0x%04x, ack = 0x%04x, data = 0x%x bytes", packet->packet_id, packet->session_id, packet->body.msg.seq, packet->body.msg.ack, (unsigned int)packet->body.msg.data_length);
  }
  else if(packet->packet_type == PACKET_TYPE_FIN)
  {
    printf("Type = FIN :: [0x%04x] session = 0x%04x :: %s", packet->packet_id, packet->session_id, packet->body.fin.reason);
  }
  else if(packet->packet_type == PACKET_TYPE_PING)
  {
    printf("Type = PING :: [0x%04x] data = %s", packet->packet_id, packet->body.ping.data);
  }
#ifndef NO_ENCRYPTION
  else if(packet->packet_type == PACKET_TYPE_ENC)
  {
    printf("Type = ENC :: [0x%04x] session = 0x%04x", packet->packet_id, packet->session_id);
  }
#endif
  else
  {
    printf("Unknown packet type!");
  }
}

void packet_destroy(packet_t *packet)
{
  if(packet->packet_type == PACKET_TYPE_SYN)
  {
    if(packet->body.syn.name)
      safe_free(packet->body.syn.name);
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

#ifndef NO_ENCRYPTION
  if(packet->packet_type == PACKET_TYPE_ENC)
  {
    /* Nothing was allocated. */
  }
#endif

  safe_free(packet);
}

char *packet_type_to_string(packet_type_t type)
{
  switch(type)
  {
    case PACKET_TYPE_SYN:
      return "SYN";
    case PACKET_TYPE_MSG:
      return "MSG";
    case PACKET_TYPE_FIN:
      return "FIN";
    case PACKET_TYPE_PING:
      return "PING";
#ifndef NO_ENCRYPTION
    case PACKET_TYPE_ENC:
      return "ENC";
#endif
    case PACKET_TYPE_COUNT_NOT_PING:
      return "Unknown!";
  }

  return "Unknown";
}
