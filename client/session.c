#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "message.h"
#include "packet.h"
#include "session.h"

NBBOOL trace_packets;

typedef struct _session_entry_t
{
  session_t *session;
  struct _session_entry_t *next;
} session_entry_t;

static session_entry_t *first_session;

void sessions_add(session_t *session)
{
  session_entry_t *entry = safe_malloc(sizeof(session_entry_t));
  entry->session = session;
  entry->next = first_session;
  first_session = entry;
}

session_t *sessions_get_by_id(uint16_t session_id)
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    if(entry->session->id == session_id)
      return entry->session;
  return NULL;
}

#if 0
static void sessions_remove(uint16_t session_id)
{
  session_entry_t *entry;
  session_entry_t *previous = NULL;

  for(entry = first_session; entry; entry = entry->next)
  {
    if(entry->session->id == session_id)
    {
      if(previous == NULL)
        first_session = entry->next;
      else
        previous->next = entry->next;

      safe_free(entry);

      return;
    }
  }
}
#endif

void sessions_do_actions()
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    session_do_actions(entry->session);
}

size_t sessions_total_bytes_queued()
{
  session_entry_t *entry;
  size_t bytes_queued = 0;

  for(entry = first_session; entry; entry = entry->next)
    bytes_queued += session_get_bytes_queued(entry->session);

  return bytes_queued;
}

void sessions_close()
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    session_close(entry->session);
}

/* TODO: I need a better way to close/destroy sessions still... */
void sessions_destroy()
{
  session_entry_t *entry;
  session_entry_t *next;

  entry = first_session;

  while(entry)
  {
    next = entry->next;
    session_destroy(entry->session);
    safe_free(entry);
    entry = next;
  }
  first_session = NULL;
}

static NBBOOL check_closed(session_t *session, char *name)
{
  if(!session)
  {
    LOG_ERROR("Tried to call %s() on an invalid session!", name);
    return TRUE;
  }

  if(session->is_closed)
  {
    LOG_ERROR("Tried to call %s() on a closed session!", name);
    return TRUE;
  }

  return FALSE;
}

static NBBOOL check_callbacks(session_t *session, char *name)
{
  if(session->incoming_data_callback == NULL || session->outgoing_data_callback == NULL)
  {
    LOG_ERROR("Tried to call %s() before setting callbacks!", name);
    return FALSE;
  }

  return TRUE;
}

static void session_send_packet(session_t *session, packet_t *packet)
{
  size_t   length;
  uint8_t *data = packet_to_bytes(packet, &length);

  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_send_packet"))
    return;

  if(!check_callbacks(session, "session_send_packet"))
    return;

  if(trace_packets)
  {
    printf("SEND: ");
    packet_print(packet);
  }

  session->outgoing_data_callback(session->id, data, length, session->callback_param);

  safe_free(data);
}

static void do_send_stuff(session_t *session)
{
  packet_t *packet;
  uint8_t  *data;
  size_t    length;

  /* Make sure the session isn't closed. */
  if(check_closed(session, "do_send_stuff"))
    return;

  switch(session->state)
  {
    case SESSION_STATE_NEW:
      LOG_INFO("In SESSION_STATE_NEW, sending a SYN packet (SEQ = 0x%04x)...", session->my_seq);
      packet = packet_create_syn(session->id, session->my_seq, 0);
      if(session->name)
        packet_syn_set_name(packet, session->name);

      session_send_packet(session, packet);
      packet_destroy(packet);
      break;

    case SESSION_STATE_ESTABLISHED:
      /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
      data = buffer_read_remaining_bytes(session->outgoing_data, &length, session->max_packet_size - packet_get_msg_size(), FALSE); /* TODO: Magic number */
      LOG_INFO("In SESSION_STATE_ESTABLISHED, sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...", session->my_seq, session->their_seq, length);

      /* Create a packet with that data */
      packet = packet_create_msg(session->id, session->my_seq, session->their_seq, data, length);

      /* Send the packet */
      session_send_packet(session, packet);

      /* Free everything */
      packet_destroy(packet);
      safe_free(data);
      break;

    default:
      LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
      exit(1);
  }
}

void session_send(session_t *session, uint8_t *data, size_t length)
{
  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_send"))
    return;

  LOG_INFO("Queuing %zd bytes of data to send", length);

  /* Add the data to the outgoing buffer. */
  buffer_add_bytes(session->outgoing_data, data, length);

  /* Trigger a send. */
  do_send_stuff(session);
}

session_t *session_create(session_data_callback_t *outgoing_data_callback, session_data_callback_t *incoming_data_callback, void *callback_param, size_t max_size)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));

  LOG_INFO("Creating a new session");

  session->id            = rand() % 0xFFFF;
  session->my_seq        = rand() % 0xFFFF; /* Random isn */

  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;

  session->incoming_data = buffer_create(BO_BIG_ENDIAN);
  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  session->outgoing_data_callback = NULL;
  session->incoming_data_callback = NULL;
  session->callback_param         = NULL;

  message_post_session_created(session->id, &session->outgoing_data_callback, &session->incoming_data_callback, &session->callback_param, &session->max_packet_size);

  return session;
}

void session_set_callbacks(session_t *session, session_data_callback_t *outgoing_data_callback, session_data_callback_t *incoming_data_callback, void *callback_param)
{
  session->outgoing_data_callback       = outgoing_data_callback;
  session->incoming_data_callback       = incoming_data_callback;
  session->callback_param               = callback_param;
}

void session_set_max_size(session_t *session, size_t size)
{
  session->max_packet_size = size; /* TODO: Fix this again */
}

void session_destroy(session_t *session)
{
  if(!session)
  {
    LOG_ERROR("Tried to free session a second time!");
    return;
  }

  message_post_session_destroyed(session->id);

  if(session->name)
    safe_free(session->name);

  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}

void session_set_name(session_t *session, char *name)
{
  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_set_name"))
    return;

  if(session->name)
    safe_free(session->name);
  session->name = safe_strdup(name);
}

void session_close(session_t *session)
{
  packet_t *packet;

  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_close"))
    return;

  /* Alert the user */
  LOG_WARNING("Sending the final FIN to the server before closing");

  /* Send the FIN */
  packet = packet_create_fin(session->id);
  session_send_packet(session, packet);
  packet_destroy(packet);

  /* Mark the session as closed. */
  session->is_closed = TRUE;
}

size_t session_get_bytes_queued(session_t *session)
{
  return buffer_get_remaining_bytes(session->outgoing_data);
}

static void clean_up_buffers(session_t *session)
{
  if(buffer_get_remaining_bytes(session->outgoing_data) == 0)
    buffer_clear(session->outgoing_data);
  if(buffer_get_remaining_bytes(session->incoming_data) == 0)
    buffer_clear(session->incoming_data);
}

void session_recv(session_t *session, packet_t *packet)
{
  NBBOOL new_bytes_acked = FALSE;

  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_recv"))
    return;
  if(!check_callbacks(session, "session_recv"))
    return;

  if(trace_packets)
  {
    printf("RECV: ");
    packet_print(packet);
  }

  if(packet)
  {
    if(packet->session_id != session->id)
    {
      LOG_ERROR("Server responded to an invalid session id! Received 0x%04x, expected 0x%04x (ignoring it)", packet->session_id, session->id);
    }
    else
    {
      switch(session->state)
      {
        case SESSION_STATE_NEW:
          if(packet->packet_type == PACKET_TYPE_SYN)
          {
            LOG_INFO("In SESSION_STATE_NEW, received SYN (ISN = 0x%04x)", packet->body.syn.seq);
            session->their_seq = packet->body.syn.seq;
            session->state = SESSION_STATE_ESTABLISHED;
          }
          else if(packet->packet_type == PACKET_TYPE_MSG)
          {
            LOG_WARNING("In SESSION_STATE_NEW, received unexpected MSG (ignoring)");
          }
          else if(packet->packet_type == PACKET_TYPE_FIN)
          {
            LOG_FATAL("In SESSION_STATE_NEW, received FIN - connection closed");

            exit(0);
          }
          else
          {
            LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
            exit(1);
          }

          break;
        case SESSION_STATE_ESTABLISHED:
          if(packet->packet_type == PACKET_TYPE_SYN)
          {
            LOG_WARNING("In SESSION_STATE_ESTABLISHED, recieved SYN (ignoring)");
          }
          else if(packet->packet_type == PACKET_TYPE_MSG)
          {
            LOG_INFO("In SESSION_STATE_ESTABLISHED, received a MSG");

            /* Validate the SEQ */
            if(packet->body.msg.seq == session->their_seq)
            {
              /* Verify the ACK is sane TODO: I'm not sure that wraparound will work well here. */
              uint16_t bytes_acked = packet->body.msg.ack - session->my_seq;

              if(bytes_acked <= buffer_get_remaining_bytes(session->outgoing_data))
              {
                /* Increment their sequence number */
                session->their_seq = (session->their_seq + packet->body.msg.data_length) & 0xFFFF;

                /* Remove the acknowledged data from the buffer */
                buffer_consume(session->outgoing_data, bytes_acked);

                /* Increment my sequence number */
                if(bytes_acked != 0)
                {
                  session->my_seq = (session->my_seq + bytes_acked) & 0xFFFF;
                  new_bytes_acked = TRUE;
                }

                /* Print the data, if we received any */
                /* TODO */
                if(packet->body.msg.data_length > 0)
                  session->incoming_data_callback(session->id, packet->body.msg.data, packet->body.msg.data_length, session->callback_param);
              }
              else
              {
                LOG_WARNING("Bad ACK received (%d bytes acked; %d bytes in the buffer)", bytes_acked, buffer_get_remaining_bytes(session->outgoing_data));
              }
            }
            else
            {
              LOG_WARNING("Bad SEQ received");
            }
          }
          else if(packet->packet_type == PACKET_TYPE_FIN)
          {
            LOG_FATAL("In SESSION_STATE_ESTABLISHED, received FIN - connection closed");
            packet_destroy(packet);

            exit(0);
          }
          else
          {
            LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
            packet_destroy(packet);
            session_close(session); /* TODO: I bet these break stuff, I should be using the message. */
          }

          break;
        default:
          LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
          packet_destroy(packet);
          session_close(session);
      }
    }

    packet_destroy(packet);
  }
  else
  {
    LOG_FATAL("Couldn't parse an incoming packet!");
    exit(1);
  }

  /* If there is still outgoing data to be sent, and new data has been ACKed
   * (ie, this isn't a retransmission), send it. */
  if(!session->is_closed)
    if(buffer_get_remaining_bytes(session->outgoing_data) > 0 && new_bytes_acked)
      do_send_stuff(session);
}

void session_do_actions(session_t *session)
{
  /* Make sure the session isn't closed. */
  if(check_closed(session, "session_do_actions"))
    return;

  /* Cleanup the incoming/outgoing buffers, if we can */
  clean_up_buffers(session);

  /* Send stuff if we can */
  do_send_stuff(session);
}
