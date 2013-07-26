/* session.h
 * By Ron Bowes
 * March, 2013
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>

#include "buffer.h"
#include "packet.h"

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} session_state_t;

extern NBBOOL trace_packets;

typedef struct
{
  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_closed;
  char           *name;

  size_t          max_packet_size;

  buffer_t       *incoming_data;
  buffer_t       *outgoing_data;

  session_data_callback_t *outgoing_data_callback;
  session_data_callback_t *incoming_data_callback;
  void                    *callback_param;
} session_t;

session_t *session_create();
void       session_destroy(session_t *session);

void       session_set_callbacks(session_t *session, session_data_callback_t *outgoing_data_callback, session_data_callback_t *incoming_data_callback, void *callback_param);
void       session_set_max_size(session_t *session, size_t size);

void       session_set_name(session_t *session, char *name);

void       session_recv(session_t *session, packet_t *packet);
void       session_send(session_t *session, uint8_t *data, size_t length);
void       session_close(session_t *session);
size_t     session_get_bytes_queued(session_t *session);

void       session_do_actions(session_t *session);

void       session_go(session_t *session);

#endif
