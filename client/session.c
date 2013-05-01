#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "memory.h"
#include "packet.h"
#include "session.h"
#include "time.h"
#include "types.h"

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  session_t *session = (session_t*) param;

  session_send(session, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  session_t *session = (session_t*) param;

  fprintf(stderr, "[[dnscat]] :: STDIN is closed, sending remaining data\n");

  session_close(session);

  return SELECT_REMOVE;
}

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  session_t *session = (session_t*) param;

  session_do_actions(session);

  return SELECT_OK;
}

session_t *session_create(driver_send_t *driver_send, void *driver_send_param)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));
  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;
  session->group         = select_group_create();
  session->max_packet_size = 20; /* TODO */

  session->driver_send   = driver_send;
  session->driver_send_param = driver_send_param;

  session->incoming_data = buffer_create(BO_BIG_ENDIAN);
  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  /* Create the STDIN socket */
#ifdef WIN32
  /* On Windows, the stdin_handle is quite complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(session->group, -1, stdin_handle, (void*)session);
  select_set_recv(session->group, -1, stdin_callback);
  select_set_closed(session->group, -1, stdin_closed_callback);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(session->group, stdin_handle, SOCKET_TYPE_STREAM, (void*)session);
  select_set_recv(session->group, stdin_handle, stdin_callback);
  select_set_closed(session->group, stdin_handle, stdin_closed_callback);

  /* Add the timeout function */
  select_set_timeout(session->group, timeout, (void*)session);
#endif

  return session;
}

void session_destroy(session_t *session)
{
  select_group_destroy(session->group);
  buffer_destroy(session->incoming_data);
  buffer_destroy(session->outgoing_data);
  safe_free(session);
}

static void session_send_packet(session_t *session, packet_t *packet)
{
  size_t   length;
  uint8_t *data = packet_to_bytes(packet, &length);

  session->driver_send(data, length, session->driver_send_param);

  safe_free(data);
}

static void send_final_fin(session_t *session)
{
  packet_t *packet;

  /* Alert the user */
  fprintf(stderr, "[[dnscat]] :: Buffers are clear, sending a FIN\n");

  /* Send the FIN */
  packet = packet_create_fin(session->id);
  session_send_packet(session, packet);
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
      session_send_packet(session, packet);
      packet_destroy(packet);
      break;

    case SESSION_STATE_ESTABLISHED:
      /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
      data = buffer_read_remaining_bytes(session->outgoing_data, &length, session->max_packet_size - packet_get_msg_size(), FALSE); /* TODO: Magic number */
      fprintf(stderr, "[[dnscat]] :: Sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...\n", session->my_seq, session->their_seq, length);

      /* Create a packet with that data */
      packet = packet_create_msg(session->id, session->my_seq, session->their_seq, data, length);

      /* Send the packet */
      session_send_packet(session, packet);

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

void session_recv(session_t *session, uint8_t *data, size_t length)
{
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

void session_register_socket(session_t *session, int s, SOCKET_TYPE_t type, select_recv *recv_callback, select_closed *closed_callback, void *param)
{
    select_group_add_socket(session->group, s, type, param);
    select_set_recv(session->group, s, recv_callback);
    if(closed_callback)
      select_set_closed(session->group, s, closed_callback);
}

void session_unregister_socket(session_t *session, int s)
{
  select_group_remove_socket(session->group, s);
}

void session_go(session_t *session)
{
  while(TRUE)
    select_group_do_select(session->group, 2000);
}

