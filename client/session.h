#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>

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
  NBBOOL          stdin_closed;

  buffer_t       *incoming_data;
  buffer_t       *outgoing_data;
} session_t;

session_t *session_create(driver_t *driver);
void       session_destroy(session_t *session);
void       session_send(session_t *session, uint8_t *data, size_t length);
void       session_recv(session_t *session, uint8_t *data, size_t length);
NBBOOL     session_is_data_queued(session_t *session);
void       session_do_actions(session_t *session);

#endif
