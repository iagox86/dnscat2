/* session.h
 * By Ron Bowes
 * March, 2013
 *
 * See LICENSE.md
 *
 * A session keeps track of an active dnscat session. That includes
 * bookkeeping data like sequence/acknowledgement numbers and buffering
 * data that hasn't been sent yet.
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include "drivers/driver.h"
#include "libs/buffer.h"
#include "libs/memory.h"
#include "libs/types.h"

#ifndef NO_ENCRYPTION
#include "libs/crypto/encryptor.h"
#endif

typedef enum
{
#ifndef NO_ENCRYPTION
  SESSION_STATE_BEFORE_INIT,
  SESSION_STATE_BEFORE_AUTH,
#endif
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED,

  SESSION_STATE_COUNT
} session_state_t;

char *session_state_to_string(session_state_t state);

typedef struct
{
  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_shutdown;
  char           *name;

  uint64_t        last_transmit;

  int             missed_transmissions;

  uint16_t       options;
  NBBOOL         is_command;

  NBBOOL         is_ping;

  driver_t       *driver;

  buffer_t       *outgoing_buffer;

#ifndef NO_ENCRYPTION
  encryptor_t *encryptor;

  /* Used for renegotiation. */
  encryptor_t *new_encryptor;
#endif
} session_t;

/* TODO: re-order and comment these. */
session_t *session_create_console(select_group_t *group, char *name);
session_t *session_create_exec(select_group_t *group, char *name, char *process);
session_t *session_create_command(select_group_t *group, char *name);
session_t *session_create_ping(select_group_t *group, char *name);

void debug_set_isn(uint16_t value);

NBBOOL session_is_shutdown(session_t *session);

NBBOOL session_data_incoming(session_t *session, uint8_t *data, size_t length);
uint8_t *session_get_outgoing(session_t *session, size_t *packet_length, size_t max_length);

void session_enable_packet_trace();
void session_set_delay(int delay_ms);
void session_set_transmit_immediately(NBBOOL transmit_immediately);
#ifndef NO_ENCRYPTION
void session_set_preshared_secret(char *new_preshared_secret);
void session_set_encryption(NBBOOL new_encryption);
#endif
void session_kill(session_t *session);
void session_destroy(session_t *session);

#endif
