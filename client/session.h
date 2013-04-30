/* session.h
 * By Ron Bowes
 * March, 2013
 *
 * Track the session - sequence numbers, state, data buffers, etc.
 *
 * Data is queued for sending via session_send(). Received data is simply
 * displayed, currently, but in future versions it will be returned to the
 * main program somehow. TODO: Update this
 * 
 * Note that this has to manage the session properly - via SYN/MSG/FIN - and
 * this also splits data into the proper-length chunks for the protocol. As
 * such, the send/receives aren't immediate, but are buffered.
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>

#include "buffer.h"

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} session_state_t;

typedef struct
{
  driver_t *driver;

  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_closed;

  buffer_t       *incoming_data;
  buffer_t       *outgoing_data;
} session_t;

session_t *session_create(driver_t *driver);
void       session_destroy(session_t *session);

void       session_send(session_t *session, uint8_t *data, size_t length);
void       session_close(session_t *session);
void       session_force_close(session_t *session);

void       session_do_actions(session_t *session);

#endif
