/**
 * controller.c
 * Created by Ron Bowes
 * On April, 2015
 *
 * See LICENSE.md
 */

#include "controller.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include "pstdint.h"
#else
#include <stdint.h>
#endif

#include "driver_dns.h"
#include "packet.h"
#include "session.h"

tunnel_driver_t  tunnel_driver_type = -1;
void            *tunnel_driver      = NULL;

typedef struct _session_entry_t
{
  session_t *session;
  struct _session_entry_t *next;
} session_entry_t;

static session_entry_t *first_session;

static session_t *controller_create_session(uint16_t session_id)
{
  session_entry_t *entry = NULL;

  session_t *session = session_create();

  /* Add it to the linked list. */
  entry = safe_malloc(sizeof(session_entry_t));
  entry->session = session;
  entry->next = first_session;
  first_session = entry;
}

static session_t *sessions_get_by_id(uint16_t session_id)
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    if(entry->session->id == session_id)
      return entry->session;
  return NULL;
}

void controller_data_received(uint8_t *data, size_t length)
{
  /* Parse the packet to get the session id */
  packet_t *packet = packet_parse(data, length, 0);
  session_t *session;

  /* Check if it's a ping packet, since those don't need a session. */
  if(packet->packet_type == PACKET_TYPE_PING)
  {
    session = NULL;

    /* TODO: Return a PING response */
    return;
  }

  /* If it's not a ping packet, find the session and handle accordingly. */
  session = sessions_get_by_id(packet->session_id);
  packet_destroy(packet);

  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_packet_in): %d", packet->session_id);
    return;
  }

  /* Now that we know the session, parse it properly */
  packet = packet_parse(data, length, session->options);

  /* Display if appropriate. */
  if(packet_trace)
  {
    printf("INCOMING: ");
    packet_print(packet, session->options);
  }
}

void controller_set_tunnel_driver(tunnel_driver_t type, void *driver)
{
  tunnel_driver_type = type;
  tunnel_driver      = driver;
}

void controller_send(packet_t *packet)
{
  switch(tunnel_driver_type)
  {
    case TUNNEL_DRIVER_DNS:
      driver_dns_send((driver_dns_t*) tunnel_driver, packet->data, packet->length);
      break;

    default:
      printf("ERROR: tunnel_driver isn't set right!\n");
      exit(1);
  }
}

static void do_send_stuff(session_t *session)
{
  packet_t *packet;
  uint8_t  *data;
  size_t    length;

  /* Don't transmit too quickly without receiving anything. */
  if(!can_i_transmit_yet(session))
  {
    LOG_INFO("Retransmission timer hasn't expired, not re-sending...");
    return;
  }

  switch(session->state)
  {
    case SESSION_STATE_NEW:
      LOG_INFO("In SESSION_STATE_NEW, sending a SYN packet (SEQ = 0x%04x)...", session->my_seq);
      packet = packet_create_syn(session->id, session->my_seq, 0);
      if(session->name)
        packet_syn_set_name(packet, session->name);
      if(session->download)
        packet_syn_set_download(packet, session->download);
      if(session->download_first_chunk)
        packet_syn_set_chunked_download(packet);
      if(session->is_command)
        packet_syn_set_is_command(packet);

      update_counter(session);
      do_send_packet(session, packet);

      packet_destroy(packet);
      break;

    case SESSION_STATE_ESTABLISHED:
      if(session->download_first_chunk)
      {
        /* We don't allow outgoing data in chunked mode */
        packet = packet_create_msg_chunked(session->id, session->download_current_chunk);
      }
      else
      {
        /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
        data = buffer_read_remaining_bytes(session->outgoing_data, &length, max_packet_length - packet_get_msg_size(session->options), FALSE);
        LOG_INFO("In SESSION_STATE_ESTABLISHED, sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...", session->my_seq, session->their_seq, length);

        packet = packet_create_msg_normal(session->id, session->my_seq, session->their_seq, data, length);

        safe_free(data);
      }

      /* Send the packet */
      update_counter(session);
      do_send_packet(session, packet);

      /* Free everything */
      packet_destroy(packet);

      break;

    default:
      LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
      exit(1);
  }
}

static void handle_packet_in(uint8_t *data, size_t length)
{
  NBBOOL poll_right_away = FALSE;

  /* Parse the packet to get the session id */
  packet_t *packet = packet_parse(data, length, 0);
  session_t *session;

  /* Check if it's a ping packet, since those don't need a session. */
  if(packet->packet_type == PACKET_TYPE_PING)
  {
    /* Display if appropriate. */
    if(packet_trace)
    {
      printf("INCOMING: ");
      packet_print(packet, 0);
    }
    /* Let listeners know that a ping happened */
    message_post_ping_response(packet->body.ping.data);

    packet_destroy(packet);
    return;
  }

  /* If it's not a ping packet, find the session and handle accordingly. */
  session = sessions_get_by_id(packet->session_id);
  packet_destroy(packet);

  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_packet_in): %d", packet->session_id);
    return;
  }

  /* Now that we know the session, parse it properly */
  packet = packet_parse(data, length, session->options);

  /* Display if appropriate. */
  if(packet_trace)
  {
    printf("INCOMING: ");
    packet_print(packet, session->options);
  }

  switch(session->state)
  {
    case SESSION_STATE_NEW:
      if(packet->packet_type == PACKET_TYPE_SYN)
      {
        LOG_INFO("In SESSION_STATE_NEW, received SYN (ISN = 0x%04x)", packet->body.syn.seq);
        session->their_seq = packet->body.syn.seq;
        session->options   = packet->body.syn.options;
        session->state = SESSION_STATE_ESTABLISHED;
      }
      else if(packet->packet_type == PACKET_TYPE_MSG)
      {
        LOG_WARNING("In SESSION_STATE_NEW, received unexpected MSG (ignoring)");
      }
      else if(packet->packet_type == PACKET_TYPE_FIN)
      {
        LOG_FATAL("In SESSION_STATE_NEW, received FIN: %s", packet->body.fin.reason);

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

        if(session->download_first_chunk)
        {
          if(packet->body.msg.options.chunked.chunk == session->download_current_chunk)
          {
            /* Let listeners know that data has arrived. */
            message_post_data_in(session->id, packet->body.msg.data, packet->body.msg.data_length);

            /* Go to the next chunk. */
            session->download_current_chunk++;

            /* Don't wait to poll again. */
            reset_counter(session);
            poll_right_away = TRUE;
          }
          else
          {
            LOG_WARNING("Bad chunk received (%d instead of %d)", packet->body.msg.options.chunked.chunk, session->download_current_chunk);
            packet_destroy(packet);
            return;
          }
        }
        else
        {
          /* Validate the SEQ */
          if(packet->body.msg.options.normal.seq == session->their_seq)
          {
            /* Verify the ACK is sane */
            uint16_t bytes_acked = packet->body.msg.options.normal.ack - session->my_seq;

            if(bytes_acked <= buffer_get_remaining_bytes(session->outgoing_data))
            {
              /* Reset the retransmit counter since we got some valid data. */
              reset_counter(session);

              /* Increment their sequence number */
              session->their_seq = (session->their_seq + packet->body.msg.data_length) & 0xFFFF;

              /* Remove the acknowledged data from the buffer */
              buffer_consume(session->outgoing_data, bytes_acked);

              /* Increment my sequence number */
              if(bytes_acked != 0)
              {
                session->my_seq = (session->my_seq + bytes_acked) & 0xFFFF;
                poll_right_away = TRUE;
              }

              /* Print the data, if we received any, and then immediately receive more. */
              if(packet->body.msg.data_length > 0)
              {
                message_post_data_in(session->id, packet->body.msg.data, packet->body.msg.data_length);
                poll_right_away = TRUE;
              }
            }
            else
            {
              LOG_WARNING("Bad ACK received (%d bytes acked; %d bytes in the buffer)", bytes_acked, buffer_get_remaining_bytes(session->outgoing_data));
              packet_destroy(packet);
              return;
            }
          }
          else
          {
            LOG_WARNING("Bad SEQ received (Expected %d, received %d)", session->their_seq, packet->body.msg.options.normal.seq);
            packet_destroy(packet);
            return;
          }
        }
      }
      else if(packet->packet_type == PACKET_TYPE_FIN)
      {
        LOG_FATAL("In SESSION_STATE_ESTABLISHED, received FIN: %s", packet->body.fin.reason);
        message_post_close_session(session->id);
      }
      else
      {
        LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
        message_post_close_session(session->id);
      }

      break;
    default:
      LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
      packet_destroy(packet);
      message_post_close_session(session->id);
      exit(1);
  }

  /* If there is still outgoing data to be sent, and new data has been ACKed
   * (ie, this isn't a retransmission), send it. */
  if(poll_right_away)
    do_send_stuff(session);

  packet_destroy(packet);
}

