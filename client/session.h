/* session.h
 * By Ron Bowes
 * March, 2013
 */

#ifndef __SESSION_H__
#define __SESSION_H__

#include <stdint.h>

#include "buffer.h"
#include "ui_stdin.h"
#include "ui_exec.h"

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} session_state_t;

typedef enum
{
  UI_NONE,
  UI_STDIN,
  UI_EXEC
} ui_t;

extern NBBOOL trace_packets;

typedef struct
{
  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_closed;

  int             max_packet_size;

  buffer_t       *incoming_data;
  buffer_t       *outgoing_data;

  data_callback_t *outgoing_data_callback;
  void            *outgoing_data_callback_param;

  ui_t            ui_type;
  union
  {
    ui_stdin_t   *ui_stdin;
    ui_exec_t    *ui_exec;
  } ui;

} session_t;

session_t *session_create(select_group_t *group, data_callback_t *outgoing_data_callback, void *outgoing_data_callback_param);
void       session_destroy(session_t *session, select_group_t *group);

void       session_recv(session_t *session, uint8_t *data, size_t length);
void       session_send(session_t *session, uint8_t *data, size_t length);
void       session_close(session_t *session);

void       session_do_actions(session_t *session);

void       session_go(session_t *session);

void       session_set_ui_stdin(session_t *session, select_group_t *group);
void       session_set_ui_exec(session_t *session, char *process, select_group_t *group);

#endif
