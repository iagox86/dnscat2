#include "log.h"
#include "session.h"

#include "sessions.h"


#define MAX_SESSIONS 16

static session_t *sessions[MAX_SESSIONS];
static size_t session_count = 0;

void sessions_add(session_t *session)
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

session_t *sessions_remove(uint16_t session_id)
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

  return session;
}

void sessions_do_actions()
{
  size_t i;

  for(i = 0; i < session_count; i++)
    session_do_actions(sessions[i]);
}
