/* session.h
 * By Ron Bowes
 * March, 2013
 *
 * See LICENSE.md
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include "drivers/driver.h"
#include "libs/buffer.h"
#include "libs/memory.h"
#include "libs/types.h"

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} session_state_t;

typedef struct
{
  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_shutdown;
  char           *name;

  char           *download;
  uint32_t        download_first_chunk;
  uint32_t        download_current_chunk;

  NBBOOL          is_command;

  time_t          last_transmit;

  uint16_t       options; /* TODO: Make options work right again. */

  driver_t       *driver;

  buffer_t       *outgoing_buffer;
} session_t;

session_t *session_create_console(select_group_t *group, char *name);
void debug_set_isn(uint16_t value);

NBBOOL session_is_shutdown(session_t *session);

void session_data_incoming(session_t *session, uint8_t *data, size_t length);
uint8_t *session_get_outgoing(session_t *session, size_t *length, size_t max_length);

#endif
