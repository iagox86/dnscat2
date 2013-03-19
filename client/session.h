#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>
#include <stdlib.h>

#include "buffer.h"

typedef enum
{
  STATE_NEW,
  STATE_ESTABLISHED
} session_state_t;

typedef struct
{
  uint16_t id;
  session_state_t state;
  uint16_t their_seq;
  uint16_t my_seq;

  buffer_t *incoming_data;
  buffer_t *outgoing_data;
} session_t;

session_t *session_create();
void session_destroy(session_t *session);

void session_queue_outgoing(uint8_t *data, size_t length);
size_t session_read_incoming(uint8_t *buffer, size_t max_length);

#endif
