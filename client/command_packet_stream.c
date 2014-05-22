#include <stdio.h>
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
  uint16_t length;

  if(buffer_get_remaining_bytes(stream->buffer) < 2)
    return FALSE;

  length = buffer_peek_next_int16(stream->buffer);

  if(length > MAX_COMMAND_PACKET_SIZE)
  {
    LOG_FATAL("Command size is too long! (length = %d, max = %d)", length, MAX_COMMAND_PACKET_SIZE);
    exit(1);
  }

  /* I realize some people hate the "if(x) return TRUE else return FALSE"
   * paradigm, but I like it. */
  if(buffer_get_remaining_bytes(stream->buffer) >= (length + 2))
  {
    printf("We got them!\n");
    return TRUE;
  }

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

#if 0
/* Testing code */
#include <stdio.h>
int main(int argc, const char *argv[])
{
  command_packet_stream_t *request  = command_packet_stream_create(TRUE);
  command_packet_stream_t *response = command_packet_stream_create(FALSE);

  char *str;

  command_packet_t *c1 = command_packet_create_ping_request(0x1111, "this is ping request");
  command_packet_t *c2 = command_packet_create_shell_request(0x2222, "this is shell name");
  command_packet_t *c3 = command_packet_create_exec_request(0x3333, "this is exec name", "this is exec file");

  command_packet_t *c4 = command_packet_create_ping_response(0x4444, "this is ping response");
  command_packet_t *c5 = command_packet_create_shell_response(0x5555, 0x1234);
  command_packet_t *c6 = command_packet_create_exec_response(0x6666, 0x4321);

  command_packet_t *c7 = command_packet_create_error_request(0x7777, 0xaaaa, "this is error reason (request)");
  command_packet_t *c8 = command_packet_create_error_response(0x8888, 0xbbbb, "this is error reason (response)");

  uint8_t *bytes;
  size_t   length;

  bytes = command_packet_to_bytes(c1, &length);
  command_packet_stream_feed(request, bytes, length);

  bytes = command_packet_to_bytes(c2, &length);
  command_packet_stream_feed(request, bytes, length);

  bytes = command_packet_to_bytes(c3, &length);
  command_packet_stream_feed(request, bytes, length);

  bytes = command_packet_to_bytes(c4, &length);
  command_packet_stream_feed(response, bytes, length);

  bytes = command_packet_to_bytes(c5, &length);
  command_packet_stream_feed(response, bytes, length);

  bytes = command_packet_to_bytes(c6, &length);
  command_packet_stream_feed(response, bytes, length);

  bytes = command_packet_to_bytes(c7, &length);
  command_packet_stream_feed(request, bytes, length);

  bytes = command_packet_to_bytes(c8, &length);
  command_packet_stream_feed(response, bytes, length);

  while(command_packet_stream_ready(request))
  {
    command_packet_t *packet = command_packet_stream_read(request);
    command_packet_print(packet);
    command_packet_destroy(packet);
  }

  while(command_packet_stream_ready(response))
  {
    command_packet_t *packet = command_packet_stream_read(response);
    command_packet_print(packet);
    command_packet_destroy(packet);
  }

  return 0;
}
#endif
