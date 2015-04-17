/* session.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"

#include "session.h"

/* Set to TRUE after getting the 'shutdown' message. */
static NBBOOL is_shutdown = FALSE;

/* The maximum length of packets. */
static size_t max_packet_length = 10000;

/* Allow the user to override the initial sequence number. */
static uint32_t isn = 0xFFFFFFFF;

/* Enable/disable packet tracing. */
static NBBOOL packet_trace;

#define RETRANSMIT_DELAY 1 /* Seconds */

/* Allow anything to go out. Call this at the start or after receiving legit data. */
static void reset_counter(session_t *session)
{
  session->last_transmit = 0;
}

/* Wait for a delay or incoming data before retransmitting. Call this after transmitting data. */
static void update_counter(session_t *session)
{
  session->last_transmit = time(NULL);
}

/* Decide whether or not we should transmit data yet. */
static NBBOOL can_i_transmit_yet(session_t *session)
{
  if(time(NULL) - session->last_transmit > RETRANSMIT_DELAY)
    return TRUE;
  return FALSE;
}

#if 0
static void do_send_packet(session_t *session, packet_t *packet)
{
  size_t length;
  uint8_t *data = packet_to_bytes(packet, &length, session->options);

  /* Display if appropriate. */
  if(packet_trace)
  {
    printf("OUTGOING: ");
    packet_print(packet, session->options);
  }

  /* TODO: Do something with the data */
  message_post_packet_out(data, length);

  safe_free(data);
}
#endif


void session_recv(session_t *session, packet_t *packet)
{
}

static void session_destroy(session_t *session)
{
  if(session->name)
    safe_free(session->name);
  if(session->download)
    safe_free(session->download);

  safe_free(session);
}

static void remove_completed_sessions()
{
  printf("remove_completed_sessions() not implemented\n");
  exit(0);
#if 0
  session_entry_t *this;
  session_entry_t *previous = NULL;
  session_entry_t *next;

  for(this = first_session; this; this = next)
  {
    session_t *session = this->session;
    next = this->next;

    if(session->is_closed && buffer_get_remaining_bytes(session->outgoing_data) == 0)
    {
      /* Send a final FIN */
      packet_t *packet = packet_create_fin(session->id, "Session closed");
      LOG_WARNING("Session %d is out of data and closed, killing it!", session->id);
      do_send_packet(session, packet);
      packet_destroy(packet);

      /* Let listeners know that the session is closed before we unlink the session. */
      message_post_session_closed(session->id);

      /* Destroy and unlink the session. */
      session_destroy(session);
      if(previous)
        previous->next = this->next;
      else
        first_session = this->next;
      safe_free(this);
    }
    else
    {
      previous = this;
    }
  }

  if(first_session == NULL && is_shutdown)
  {
    LOG_WARNING("Everything's done!");
    exit(0);
  }
#endif
}

static void handle_config_int(char *name, int value)
{
  if(!strcmp(name, "max_packet_length"))
    max_packet_length = value;
}

static void handle_config_string(char *name, char *value)
{
}

static void handle_shutdown()
{
  printf("handle_shutdown() not implemented\n");
#if 0
  session_entry_t *entry;

  LOG_WARNING("Received SHUTDOWN message!");

  is_shutdown = TRUE;

  for(entry = first_session; entry; entry = entry->next)
    message_post_close_session(entry->session->id);
#endif
}

static session_t *session_create(char *name)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));

  session->id            = rand() % 0xFFFF;

  /* Check if it's a 16-bit value (I set it to a bigger value to set a random isn) */
  if(isn == (isn & 0xFFFF))
    session->my_seq        = (uint16_t) isn; /* Use the hardcoded one. */
  else
    session->my_seq        = rand() % 0xFFFF; /* Random isn */

  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;

  session->last_transmit = 0;

  session->name = NULL;
  if(name)
  {
    session->name = safe_strdup(name);
    LOG_INFO("Setting session->name to %s", session->name);
  }

#if 0
  session->download               = NULL;
  session->download_first_chunk   = 0;
  session->download_current_chunk = 0;
  session->is_command             = FALSE
#endif

  return session;
}

session_t *session_create_console(select_group_t *group, char *name)
{
  session_t *session     = session_create(name);

  session->driver = driver_create(DRIVER_TYPE_CONSOLE, driver_console_create(group));

  return session;
}

static void handle_close_session(uint16_t session_id)
{
  /* TODO */
  printf("handle_close_session() not implemented\n");
#if 0
  session_t *session = sessions_get_by_id(session_id);
  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_close_session): %d", session_id);
    return;
  }

  if(session->is_closed)
  {
    LOG_WARNING("Trying to close a closed session: %d", session_id);
  }
  else
  {
    /* Mark the session as closed, it'll be removed in the heartbeat */
    session->is_closed = TRUE;
  }
#endif
}

static void handle_data_out(uint16_t session_id, uint8_t *data, size_t length)
{
  session_t *session = sessions_get_by_id(session_id);
  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_data_out): %d", session_id);
    return;
  }

  /* Add the bytes to the outgoing data buffer. */
  buffer_add_bytes(session->outgoing_data, data, length);

  /* Trigger a send. */
  do_send_stuff(session);
}

static void handle_ping_request(char *ping_data)
{
  packet_t *packet = packet_create_ping(ping_data);
  size_t length;
  uint8_t *data = packet_to_bytes(packet, &length, 0);

  message_post_packet_out(data, length);

  packet_destroy(packet);
  safe_free(data);
}

static void handle_heartbeat()
{
  printf("handle_heartbeat() not implemented\n");
#if 0
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
  {
    /* Cleanup the incoming/outgoing buffers, if we can, to save memory */
    if(buffer_get_remaining_bytes(entry->session->outgoing_data) == 0)
      buffer_clear(entry->session->outgoing_data);

    /* Send stuff if we can */
    do_send_stuff(entry->session);
  }

  /* Remove any completed sessions. */
  remove_completed_sessions();
#endif
}

void debug_set_isn(uint16_t value)
{
  isn = value;

  LOG_WARNING("WARNING: Setting a custom ISN can be dangerous!");
}

void session_enable_packet_trace()
{
  packet_trace = TRUE;
}

NBBOOL session_is_shutdown(session_t *session)
{
  return session->is_shutdown;
}
