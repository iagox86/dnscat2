#include "log.h"
#include "message.h"
#include "session.h"

#include "sessions.h"


#define MAX_SESSIONS 16

static session_t *sessions[MAX_SESSIONS];
static size_t session_count = 0;

static void sessions_add(session_t *session)
{
  if(session_count >= MAX_SESSIONS)
  {
    LOG_FATAL("Too many sessions! We need to make this list auto-grow...");
    exit(1);
  }
  sessions[session_count++] = session;
}

session_t *sessions_get_by_id(uint16_t session_id)
{
  size_t i;

  /* TODO: Handle closed sessions, etc. */
  for(i = 0; i < session_count; i++)
  {
    if(sessions[i]->id == session_id)
      return sessions[i];
  }

  return NULL;
}

static void sessions_remove(uint16_t session_id)
{
  size_t i;
  session_t *session = NULL;

  /* TODO: Handle closed sessions, etc. */
  for(i = 0; i < session_count && !session; i++)
  {
    if(sessions[i]->id == session_id)
      session = sessions[i];
  }

  if(session)
  {
    for( ; i < session_count - 1; i++)
      sessions[i] = sessions[i + 1];

    sessions[i + 1] = 0;
    session_count--;
  }
}

void sessions_do_actions()
{
  size_t i;

  for(i = 0; i < session_count; i++)
    session_do_actions(sessions[i]);
}

size_t sessions_total_bytes_queued()
{
  size_t i;
  size_t bytes_queued = 0;

  for(i = 0; i < session_count; i++)
    bytes_queued += session_get_bytes_queued(sessions[i]);

  return bytes_queued;
}

void sessions_close()
{
  size_t i;
  for(i = 0; i < session_count; i++)
    session_close(sessions[i]);
}

/* TODO: I need a better way to close/destroy sessions still... */
void sessions_destroy()
{
  size_t i;

  for(i = 0; i < session_count; i++)
    session_destroy(sessions[i]);

  session_count = 0;
}

static void handle_create(uint16_t *session_id_out)
{
  session_t *session = session_create();
  sessions_add(session);
  *session_id_out = session->id;

  message_post_session_created(&session->outgoing_data_callback, &session->incoming_data_callback, &session->callback_param, &session->max_packet_size);

  if(session->outgoing_data_callback == NULL || session->incoming_data_callback == NULL)
  {
    LOG_FATAL("Nothing handled the SESSION_CREATED message properly!");
    exit(1);
  }
}

static void handle_destroy(uint16_t session_id)
{
  session_t *session = sessions_get_by_id(session_id);
  session_destroy(session);
  sessions_remove(session_id);
}

static void handle_message(message_t *message, void *d)
{
  switch(message->type)
  {
    case MESSAGE_CREATE_SESSION:
      handle_create(&message->message.create_session.out.session_id);
      break;

    case MESSAGE_DESTROY_SESSION:
      handle_destroy(message->message.destroy_session.session_id);
      break;

    default:
      LOG_FATAL("Unknown message type: %d\n", message->type);
      exit(1);
  }
}

void sessions_init()
{
  message_subscribe(MESSAGE_CREATE_SESSION,  handle_message, NULL);
  message_subscribe(MESSAGE_DESTROY_SESSION, handle_message, NULL);
}
