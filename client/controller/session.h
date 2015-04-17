/* session.h
 * By Ron Bowes
 * March, 2013
 *
 * See LICENSE.md
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include "../libs/memory.h"
#include "../libs/types.h"
#include "../libs/buffer.h"

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
  NBBOOL          is_closed;
  char           *name;

  char           *download;
  uint32_t        download_first_chunk;
  uint32_t        download_current_chunk;

  NBBOOL          is_command;

  buffer_t       *outgoing_data;

  time_t          last_transmit;

  options_t       options;
} session_t;

session_t *session_create(char *name, char *download, uint32_t first_chunk, NBBOOL is_command);
void debug_set_isn(uint16_t value);
void session_enable_packet_trace();

#endif
