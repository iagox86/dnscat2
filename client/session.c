#include <stdio.h>
#include <stdlib.h>

#include "buffer.h"
#include "memory.h"
#include "session.h"

session_t *session_create()
{
  session_t *session = (session_t *) safe_malloc(sizeof(session_t));

  session->id = rand() & 0xFFFF;
  session->my_seq = rand() & 0xFFFF;
  session->incoming_data = buffer_create(BO_BIG_ENDIAN);
  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  return session;
}

void session_queue_outgoing(session_t *session, uint8_t *data, size_t length)
{
  buffer_add_bytes(session->outgoing_data, data, length);
  printf("There are currently %zd bytes queued (outgoing)\n", buffer_get_remaining_bytes(session->outgoing_data));
}

size_t session_read_outgoing(session_t *session, uint8_t *buffer, size_t max_length)
{
  size_t amount_to_read = (max_length < buffer_get_remaining_bytes(session->outgoing_data)) ? max_length : buffer_get_remaining_bytes(session->outgoing_data);
  buffer_read_next_bytes(session->outgoing_data, buffer, amount_to_read);
  return amount_to_read;
}

void session_queue_incoming(session_t *session, uint8_t *data, size_t length)
{
  buffer_add_bytes(session->outgoing_data, data, length);
  printf("There are currently %zd bytes queued (incoming)\n", buffer_get_remaining_bytes(session->outgoing_data));
}

size_t session_read_incoming(session_t *session, uint8_t *buffer, size_t max_length)
{
  size_t amount_to_read = (max_length < buffer_get_remaining_bytes(session->incoming_data)) ? max_length : buffer_get_remaining_bytes(session->incoming_data);
  buffer_read_next_bytes(session->incoming_data, buffer, amount_to_read);
  return amount_to_read;
}

void session_destroy(session_t *session)
{
  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}
