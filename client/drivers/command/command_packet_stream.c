/* command_packet_stream.c
 * By Ron Bowes
 * Created May, 2014
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <stdlib.h>

#include "command_packet.h"
#include "libs/buffer.h"
#include "libs/log.h"
#include "libs/memory.h"

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "command_packet_stream.h"

command_packet_stream_t *command_packet_stream_create(NBBOOL is_request)
{
  command_packet_stream_t *stream = safe_malloc(sizeof(command_packet_stream_t));

  stream->is_request = is_request;
  stream->buffer = buffer_create(BO_BIG_ENDIAN);

  return stream;
}

void command_packet_stream_destroy(command_packet_stream_t *stream)
{
  buffer_destroy(stream->buffer);
  safe_free(stream);
}

void command_packet_stream_feed(command_packet_stream_t *stream, uint8_t *data, uint16_t length)
{
  buffer_add_bytes(stream->buffer, data, length);
}

NBBOOL command_packet_stream_ready(command_packet_stream_t *stream)
{
  uint32_t length;

  if(buffer_get_remaining_bytes(stream->buffer) < 4)
  {
    LOG_INFO("Stream only contains 0x%x bytes", buffer_get_remaining_bytes(stream->buffer));
    return FALSE;
  }

  length = buffer_peek_next_int32(stream->buffer);

  if(length > MAX_COMMAND_PACKET_SIZE)
  {
    LOG_FATAL("Command size is too long! (length = %d, max = %d)", length, MAX_COMMAND_PACKET_SIZE);
    exit(1);
  }

  /* Check for overflow. */
  if(length + 4 < length)
    return FALSE;

  if(buffer_get_remaining_bytes(stream->buffer) >= (size_t)(length + 4))
    return TRUE;

  LOG_INFO("Stream only contains 0x%x bytes, we need 0x%x to continue", buffer_get_remaining_bytes(stream->buffer), length+4);
  return FALSE;
}

command_packet_t *command_packet_stream_read(command_packet_stream_t *stream)
{
  uint32_t length;
  uint8_t *data;

  command_packet_t *command_packet;

  if(!command_packet_stream_ready(stream))
    return NULL;

  length = buffer_read_next_int32(stream->buffer);
  data = safe_malloc(length);
  buffer_read_next_bytes(stream->buffer, data, length);

  command_packet = command_packet_parse(data, length, stream->is_request);

  safe_free(data);

  return command_packet;
}
