/**
 * controller.c
 * Created by Ron Bowes
 * On April, 2015
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "libs/dns.h"
#include "libs/log.h"
#include "tunnel_drivers/driver_dns.h"
#include "packet.h"
#include "session.h"

#include "controller.h"

static int max_retransmits = 20;

typedef struct _session_entry_t
{
  session_t *session;
  struct _session_entry_t *next;
} session_entry_t;
static session_entry_t *first_session;

void controller_add_session(session_t *session)
{
  session_entry_t *entry = NULL;

  /* Add it to the linked list. */
  entry = safe_malloc(sizeof(session_entry_t));
  entry->session = session;
  entry->next = first_session;
  first_session = entry;
}

size_t controller_open_session_count()
{
  size_t count = 0;

  session_entry_t *entry = first_session;

  while(entry)
  {
    if(!session_is_shutdown(entry->session))
      count++;

    entry = entry->next;
  }

  return count;
}

static session_t *sessions_get_by_id(uint16_t session_id)
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    if(entry->session->id == session_id)
      return entry->session;

  return NULL;
}

/* Version beta 0.01 suffered from a bug that I didn't understand: if one
 * session had a ton of data to send, all the other sessions were ignored till
 * it was done (if it was a new session). This function will get the "next"
 * session using a global variable.
 */
static session_entry_t *current_session = NULL;

#if 0
static session_t *sessions_get_next()
{
  /* If there's no session, use the first one. */
  if(!current_session)
    current_session = first_session;

  /* If there's still no session, give up. */
  if(!current_session)
    return NULL;

  /* Get the next session. */
  current_session = current_session->next;

  /* If we're at the end, go to the beginning. Also attempting a record for the
   * number of NULL checks one a single pointer. */
  if(!current_session)
    current_session = first_session;

  /* If this is NULL, something crazy is happening (we already had a
   * first_session at some point in the past, so it means the linked list has
   * had a member removed, which we can't currently do. */
  assert(current_session);

  /* All done! */
  return current_session->session;
}
#endif

/* Get the next session in line that isn't shutdown.
 * TODO: can/should we give higher priority to sessions that have data
 * waiting?
 */
static session_t *sessions_get_next_active()
{
  /* Keep track of where we start. */
  session_entry_t *start = NULL;

  /* If there's no session, use the first one. */
  if(!current_session)
    current_session = first_session;

  /* If there's still no session, give up. */
  if(!current_session)
    return NULL;

  /* Record our starting point. */
  start = current_session;

  /* Get the next session. */
  do
  {
    current_session = current_session->next;

    /* If we're at the end, go to the beginning. Also attempting a record for the
    * number of NULL checks one a single pointer. */
    if(!current_session)
      current_session = first_session;

    if(!session_is_shutdown(current_session->session))
      return current_session->session;
  }
  while(current_session != start);

  /* If we reached the starting point, there are no active sessions. */
  return NULL;
}

NBBOOL controller_data_incoming(uint8_t *data, size_t length)
{
  uint16_t session_id = packet_peek_session_id(data, length);
  session_t *session = sessions_get_by_id(session_id);

  /* If we weren't able to find a session, print an error and return. */
  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (%s): %d", __FUNCTION__, session_id);
    return FALSE;
  }

  /* Pass the data onto the session. */
  return session_data_incoming(session, data, length);
}

uint8_t *controller_get_outgoing(size_t *length, size_t max_length)
{
  /* This needs to somehow be balanced. */
  session_t *session = sessions_get_next_active();

  if(!session)
  {
    /* TODO: Drop to a "probe for sessions" mode instead. */
    LOG_FATAL("There are no active sessions left! Goodbye!");
    exit(0);
    return NULL;
  }

  return session_get_outgoing(session, length, max_length);
}

static void kill_ignored_sessions()
{
  session_entry_t *entry = first_session;

  if(max_retransmits < 0)
    return;

  while(entry)
  {
    if(!entry->session->is_shutdown && entry->session->missed_transmissions > max_retransmits)
    {
      LOG_ERROR("The server hasn't returned a valid response in the last %d attempts.. closing session.", entry->session->missed_transmissions - 1);
      session_kill(entry->session);
    }

    entry = entry->next;
  }
}

void controller_kill_all_sessions()
{
  session_entry_t *entry = first_session;

  while(entry)
  {
    if(!entry->session->is_shutdown)
      session_kill(entry->session);
    entry = entry->next;
  }
}

/* This will be called something like every 50ms. */
void controller_heartbeat()
{
  kill_ignored_sessions();
}

void controller_set_max_retransmits(int retransmits)
{
  max_retransmits = retransmits;
}

void controller_destroy()
{
  session_entry_t *prev_session = NULL;
  session_entry_t *entry = first_session;

  while(entry)
  {
    if(!entry->session->is_shutdown)
      session_kill(entry->session);
    session_destroy(entry->session);

    prev_session = entry;
    entry = entry->next;
    safe_free(prev_session);
  }
}
