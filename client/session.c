#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "packet.h"
#include "session.h"

session_t *session_create(driver_send_t *driver_send, void *driver_send_param)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));

  LOG_INFO("Creating a new session");

  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;
  session->max_packet_size = 20; /* TODO */

  session->driver_send   = driver_send;
  session->driver_send_param = driver_send_param;

  session->incoming_data = buffer_create(BO_BIG_ENDIAN);
  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  return session;
}

void session_destroy(session_t *session)
{
  LOG_INFO("Cleaning up the session");

  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}

static void session_send_packet(session_t *session, packet_t *packet)
{
  size_t   length;
  uint8_t *data = packet_to_bytes(packet, &length);

  LOG_INFO("SENDING:");
  packet_print(packet);
  session->driver_send(data, length, session->driver_send_param);

  safe_free(data);
}

static void send_final_fin(session_t *session)
{
  packet_t *packet;

  /* Alert the user */
  LOG_INFO("Sending the final FIN to the server before closing");

  /* Send the FIN */
  packet = packet_create_fin(session->id);
  session_send_packet(session, packet);
  packet_destroy(packet);
}

void session_close(session_t *session)
{
  session->is_closed = TRUE;
}

static void clean_up_buffers(session_t *session)
{
  if(buffer_get_remaining_bytes(session->outgoing_data) == 0)
    buffer_clear(session->outgoing_data);
  if(buffer_get_remaining_bytes(session->incoming_data) == 0)
    buffer_clear(session->incoming_data);
}

static void do_send_stuff(session_t *session)
{
  packet_t *packet;
  uint8_t  *data;
  size_t    length;

  switch(session->state)
  {
    case SESSION_STATE_NEW:
      /* Create a new id / sequence number before sending the packet. That way,
       * lost SYN packets don't mess up future sessions. */
      session->id = rand() % 0xFFFF;
      session->my_seq = rand() % 0xFFFF;

      LOG_INFO("In SESSION_STATE_NEW, sending a SYN packet (SEQ = 0x%04x)...", session->my_seq);
      packet = packet_create_syn(session->id, session->my_seq, 0);
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
  LOG_INFO("Queuing %zd bytes of data to send", length);

  /* Add the data to the outgoing buffer. */
  buffer_add_bytes(session->outgoing_data, data, length);

  /* Trigger a send. */
  do_send_stuff(session);
}

void session_recv(session_t *session, uint8_t *data, size_t length)
{
  packet_t *packet = packet_parse(data, length);

  LOG_INFO("RECEIVED:");
  packet_print(packet);

  if(packet)
  {
    switch(session->state)
    {
      case SESSION_STATE_NEW:
        if(packet->message_type == MESSAGE_TYPE_SYN)
        {
          LOG_INFO("In SESSION_STATE_NEW, received SYN (ISN = 0x%04x)", packet->body.syn.seq);
          session->their_seq = packet->body.syn.seq;
          session->state = SESSION_STATE_ESTABLISHED;
        }
        else if(packet->message_type == MESSAGE_TYPE_MSG)
        {
          LOG_WARNING("In SESSION_STATE_NEW, received unexpected MSG (ignoring)");
        }
        else if(packet->message_type == MESSAGE_TYPE_FIN)
        {
          LOG_FATAL("In SESSION_STATE_NEW, received FIN - connection closed");

          exit(0);
        }
        else
        {
          LOG_FATAL("Unknown packet type: 0x%02x", packet->message_type);
          exit(1);
        }

        break;
      case SESSION_STATE_ESTABLISHED:
        if(packet->message_type == MESSAGE_TYPE_SYN)
        {
          LOG_WARNING("In SESSION_STATE_ESTABLISHED, recieved SYN (ignoring)");
        }
        else if(packet->message_type == MESSAGE_TYPE_MSG)
        {
          LOG_INFO("In SESSION_STATE_ESTABLISHED, received a MSG");

          /* Validate the SEQ */
          if(packet->body.msg.seq == session->their_seq)
          {
            /* Verify the ACK is sane */
            if(packet->body.msg.ack <= session->my_seq + buffer_get_remaining_bytes(session->outgoing_data))
            {
              /* Increment their sequence number */
              session->their_seq += packet->body.msg.data_length;

              /* Remove the acknowledged data from the buffer */
              buffer_consume(session->outgoing_data, packet->body.msg.ack - session->my_seq);

              /* Increment my sequence number */
              session->my_seq = packet->body.msg.ack;

              /* Print the data, if we received any */
              if(packet->body.msg.data_length > 0)
              {
                int i;

                /* Output the actual data. */
                for(i = 0; i < packet->body.msg.data_length; i++)
                  putchar(packet->body.msg.data[i]);
              }
              /* TODO: Do something better with this. */
            }
            else
            {
              LOG_WARNING("Bad ACK received");
            }
          }
          else
          {
            LOG_WARNING("Bad SEQ received");
          }
        }
        else if(packet->message_type == MESSAGE_TYPE_FIN)
        {
          LOG_FATAL("In SESSION_STATE_ESTABLISHED, received FIN - connection closed");
          packet_destroy(packet);

          exit(0);
        }
        else
        {
          LOG_FATAL("Unknown packet type: 0x%02x", packet->message_type);

          packet_destroy(packet);
          send_final_fin(session);
          exit(0);
        }

        break;
      default:
        LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
        packet_destroy(packet);
        send_final_fin(session);
        exit(0);
    }

    packet_destroy(packet);
  }
  else
  {
    LOG_FATAL("Couldn't parse an incoming packet!");
    exit(1);
  }

  /* If there is still outgoing data to be sent, after getting a response, send it. */
  if(buffer_get_remaining_bytes(session->outgoing_data) > 0)
    do_send_stuff(session);
}

void session_do_actions(session_t *session)
{
  /* Cleanup the incoming/outgoing buffers, if we can */
  clean_up_buffers(session);

  /* Send stuff if we can */
  do_send_stuff(session);

  /* If the session is closed and no data is queued, close properly */
  if(session->is_closed && buffer_get_remaining_bytes(session->incoming_data) == 0 && buffer_get_remaining_bytes(session->outgoing_data) == 0)
  {
    send_final_fin(session);
    exit(0);
  }
}
