#include <stdlib.h>

#include "buffer.h"
#include "command_packet.h"
#include "log.h"
#include "memory.h"

#ifdef WIN32
#include "pstdint.h"
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

void command_packet_stream_feed(command_packet_stream_t *stream, uint8_t *data, uint16_t length)
{
  buffer_add_bytes(stream->buffer, data, length);
}

NBBOOL command_packet_stream_ready(command_packet_stream_t *stream)
{
  uint16_t length = buffer_read_int16_at(stream->buffer, 0);
  if(length > MAX_COMMAND_PACKET_SIZE)
  {
    LOG_FATAL("Command buffer is too long!");
    exit(1);
  }

  /* I realize some people hate the "if(x) return TRUE else return FALSE"
   * paradigm, but I like it. */
  if(buffer_get_length(stream->buffer) >= (length + 2))
    return TRUE;

  return FALSE;
}

command_packet_t *command_packet_stream_read(command_packet_stream_t *stream)
{
  uint16_t length;
  uint8_t *data;

  command_packet_t *command_packet;

  if(!command_packet_stream_ready(stream))
    return NULL;

  length = buffer_read_next_int16(stream->buffer);
  data = safe_malloc(length);
  buffer_read_next_bytes(stream->buffer, data, length);

  command_packet = command_packet_parse(data, length, stream->is_request);

  safe_free(data);

  return command_packet;
}
