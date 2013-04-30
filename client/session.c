#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "driver.h"
#include "memory.h"
#include "packet.h"
#include "session.h"
#include "time.h"
#include "types.h"

/* Prototype so we can register it as a callback. */
void session_recv_callback(uint8_t *data, size_t length, void *session);

session_t *session_create(driver_t *driver)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));
  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;
  session->driver        = driver;

  session->incoming_data = buffer_create(BO_BIG_ENDIAN);
  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  /* Register the callback. */
  driver_register_callback(driver, session_recv_callback, session);

  return session;
}

void session_destroy(session_t *session)
{
  driver_destroy(session->driver);
  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}

static void send_final_fin(session_t *session)
{
  packet_t *packet;

  /* Alert the user */
  fprintf(stderr, "[[dnscat]] :: Buffers are clear, sending a FIN\n");

  /* Send the FIN */
  packet = packet_create_fin(session->id);
  driver_send_packet(session->driver, packet);
  packet_destroy(packet);
}

void session_close(session_t *session)
{
  session->is_closed = TRUE;
}

void session_force_close(session_t *session)
{
  send_final_fin(session);
}

NBBOOL session_is_data_queued(session_t *session)
{
  return buffer_get_remaining_bytes(session->outgoing_data) != 0;
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

      fprintf(stderr, "[[dnscat]] :: Sending a SYN packet (SEQ = 0x%04x)...\n", session->my_seq);
      packet = packet_create_syn(session->id, session->my_seq, 0);
      driver_send_packet(session->driver, packet);
      packet_destroy(packet);
      break;

    case SESSION_STATE_ESTABLISHED:
      /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
      data = buffer_read_remaining_bytes(session->outgoing_data, &length, session->driver->max_packet_size - packet_get_msg_size(), FALSE); /* TODO: Magic number */
      fprintf(stderr, "[[dnscat]] :: Sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...\n", session->my_seq, session->their_seq, length);

      /* Create a packet with that data */
      packet = packet_create_msg(session->id, session->my_seq, session->their_seq, data, length);

      /* Send the packet */
      driver_send_packet(session->driver, packet);

      /* Free everything */
      packet_destroy(packet);
      safe_free(data);
      break;

    default:
      fprintf(stderr, "[[ERROR]] :: Wound up in an unknown state: 0x%x\n", session->state);
      exit(1);
  }
}

void session_send(session_t *session, uint8_t *data, size_t length)
{
  /* Add the data to the outgoing buffer. */
  buffer_add_bytes(session->outgoing_data, data, length);

  /* Trigger a send. */
  do_send_stuff(session);
}

void session_recv_callback(uint8_t *data, size_t length, void *s)
{
  session_t *session = (session_t*) s;
  packet_t *packet = packet_parse(data, length);

  if(packet)
  {
    switch(session->state)
    {
      case SESSION_STATE_NEW:
        if(packet->message_type == MESSAGE_TYPE_SYN)
        {
          fprintf(stderr, "[[dnscat]] SYN received from server (SEQ = 0x%04x)\n", packet->body.syn.seq);
          session->their_seq = packet->body.syn.seq;
          session->state = SESSION_STATE_ESTABLISHED;
        }
        else if(packet->message_type == MESSAGE_TYPE_MSG)
        {
          fprintf(stderr, "[[WARNING]] :: Unexpected MSG received (ignoring)\n");
        }
        else if(packet->message_type == MESSAGE_TYPE_FIN)
        {
          fprintf(stderr, "[[dnscat]] :: Connection closed\n");

          exit(0);
        }
        else
        {
          fprintf(stderr, "[[ERROR]] :: Unknown packet type: 0x%02x\n", packet->message_type);
          exit(1);
        }

        break;
      case SESSION_STATE_ESTABLISHED:
        if(packet->message_type == MESSAGE_TYPE_SYN)
        {
          fprintf(stderr, "[[WARNING]] :: Unexpected SYN received (ignoring)\n");
        }
        else if(packet->message_type == MESSAGE_TYPE_MSG)
        {
          fprintf(stderr, "[[dnscat]] :: Received a MSG from the server\n");

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
                for(i = 0; i < packet->body.msg.data_length; i++)
                  printf("%c", packet->body.msg.data[i]);
                /*fprintf(stderr, "[[data]] :: %s [0x%zx bytes]\n", packet->body.msg.data, packet->body.msg.data_length);*/

              }
              /* TODO: Do something better with this. */
            }
            else
            {
              fprintf(stderr, "[[WARNING]] :: Bad ACK received\n");
            }
          }
          else
          {
            fprintf(stderr, "[[WARNING]] :: Bad SEQ received\n");
          }
        }
        else if(packet->message_type == MESSAGE_TYPE_FIN)
        {
          fprintf(stderr, "[[dnscat]] :: Connection closed by server\n");
          packet_destroy(packet);

          exit(0);
        }
        else
        {
          fprintf(stderr, "[[ERROR]] :: Unknown packet type: 0x%02x\n", packet->message_type);
          packet_destroy(packet);

          send_final_fin(session);
          exit(0);
        }

        break;
      default:
        fprintf(stderr, "[[ERROR]] :: Wound up in an unknown state: 0x%x\n", session->state);
        packet_destroy(packet);
        send_final_fin(session);
        exit(0);
    }

    packet_destroy(packet);
  }
  else
  {
    fprintf(stderr, "[[ERROR]] :: Couldn't parse an incoming packet!");
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
