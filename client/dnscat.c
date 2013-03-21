#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "select_group.h"

#include "driver.h"
#include "driver_tcp.h"
#include "memory.h"
#include "packet.h"
#include "time.h"
#include "types.h"

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} state_t;

typedef struct
{
  select_group_t *group;
  driver_t *driver;

  /* Session information */
  uint16_t session_id;
  state_t  session_state;
  uint16_t their_seq;
  uint16_t my_seq;

  buffer_t *incoming_data;
  buffer_t *outgoing_data;
} options_t;

static SELECT_RESPONSE_t stdin_callback(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = (options_t*) param;

  buffer_add_bytes(options->outgoing_data, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t stdin_closed_callback(void *group, int socket, void *param)
{
  options_t *options = (options_t*) param;
  /* TODO: send a FIN */

  printf("[[dnscat]] stdin closed\n");

  buffer_destroy(options->outgoing_data);
  buffer_destroy(options->incoming_data);
  select_group_destroy(options->group);
  driver_destroy(options->driver);
  safe_free(options);

  print_memory();

  exit(0);
}

static void timeout_new_send(options_t *options)
{
  /* Send a syn */
  packet_t *packet = packet_create_syn(options->session_id, options->my_seq, 0);
  size_t    length;
  uint8_t  *bytes = packet_to_bytes(packet, &length);

  printf("[[dnscat]] :: Sending a SYN packet...\n");
  driver_send(options->driver, bytes, length);
  printf("[[dnscat]] :: My SEQ = 0x%04x\n", packet->body.syn.seq);

  safe_free(bytes);
  packet_destroy(packet);
}

static void timeout_new_recv(options_t *options)
{
  size_t length;
  uint8_t *bytes = driver_recv(options->driver, &length, -1);

  if(bytes)
  {
    packet_t *packet = packet_parse(bytes, length);

    switch(packet->message_type)
    {
      case MESSAGE_TYPE_SYN:
        printf("[[dnscat]] :: Received a response to our SYN!\n");
        options->session_state = SESSION_STATE_ESTABLISHED;
        options->their_seq = packet->body.syn.seq;

        printf("[[dnscat]] :: Their SEQ = 0x%04x\n", packet->body.syn.seq);
        break;

      case MESSAGE_TYPE_MSG:
        printf("[[ERROR]] :: Unexpected MSG packet\n");
        exit(1);

      case MESSAGE_TYPE_FIN:
        printf("[[dnscat]] :: Connection closed\n");
        exit(1);

      default:
        printf("[[ERROR]] :: Unknown message type received: 0x%02x\n", packet->message_type);
        break;
    }

    packet_destroy(packet);
    safe_free(bytes);
  }
}

static void timeout_established_send(options_t *options)
{
  uint8_t  *data;
  size_t    length;
  packet_t *packet;
  uint8_t  *bytes;

  data = buffer_read_remaining_bytes(options->outgoing_data, &length, options->driver->max_packet_size - 6, FALSE); /* TODO: Magic number */
  printf("[[dnscat]] :: Sending 0x%zx bytes of data...\n", length);
  packet = packet_create_msg(options->session_id, options->my_seq, options->their_seq, data, length);

  bytes = packet_to_bytes(packet, &length);
  driver_send(options->driver, bytes, length);
  safe_free(bytes);
  packet_destroy(packet);
  safe_free(data);
}

static void timeout_established_recv(options_t *options)
{
  size_t   length;
  uint8_t *data;

  data = driver_recv(options->driver, &length, -1);
  if(data)
  {
    packet_t *packet = packet_parse(data, length);
    switch(packet->message_type)
    {
      case MESSAGE_TYPE_SYN:
        printf("[[WARNING]] :: Server sent an unexpected SYN; ignoring\n");
        break;
      case MESSAGE_TYPE_MSG:
        printf("[[DNSCAT]] :: Received a MSG from the server\n");

        /* Validate the SEQ */
        if(packet->body.msg.seq == options->their_seq)
        {
          options->their_seq += packet->body.msg.data_length;

          /* TODO: Verify the ACK */
          buffer_consume(options->outgoing_data, packet->body.msg.ack - options->my_seq);
          options->my_seq = packet->body.msg.ack;

          if(packet->body.msg.data_length > 0)
            printf("[[data]] :: %s [0x%zx bytes]\n", packet->body.msg.data, packet->body.msg.data_length);
        }
        else
        {
          printf("[[WARNING]] :: Received a bad SEQ from server; ignoring\n");
        }
        /* TODO */
        break;
      case MESSAGE_TYPE_FIN:
        printf("[[dnscat]] :: Server closed the connection\n");
        exit(0);
        break;
      default:
        printf("[[ERROR]] :: Server sent an unknown message type: 0x%02x\n", packet->message_type);
        exit(1);
        break;
    }
    safe_free(data);
  }
}

static SELECT_RESPONSE_t timeout(void *group, void *param)
{
  options_t *options = (options_t*) param;

  switch(options->session_state)
  {
    case SESSION_STATE_NEW:
      timeout_new_recv(options);
      timeout_new_send(options);
      break;
    case SESSION_STATE_ESTABLISHED:
      timeout_established_recv(options);
      timeout_established_send(options);
      break;
    default:
      printf("[[ERROR]] :: We ended up in an unknown state!\n");
      exit(1);
  }

  return SELECT_OK;
}

int main(int argc, const char *argv[])
{
  options_t *options = (options_t*)safe_malloc(sizeof(options_t));
  srand(time(NULL));

  /* Set up the session */
  options->session_id = rand() % 0xFFFF;
  options->session_state = SESSION_STATE_NEW;
  options->their_seq = 0;
  options->my_seq = rand() % 0xFFFF;
  options->incoming_data = buffer_create(BO_BIG_ENDIAN);
  options->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  /* Set up the STDIN socket */
  options->group = select_group_create();

#ifdef WIN32
  /* On Windows, the stdin_handle is quire complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(group, -1, stdin_handle, (void*)options);
  select_set_recv(group, -1, stdin_callback);
  select_set_closed(group, -1, stdin_closed_callback);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(options->group, stdin_handle, SOCKET_TYPE_STREAM, (void*)options);
  select_set_recv(options->group, stdin_handle, stdin_callback);
  select_set_closed(options->group, stdin_handle, stdin_closed_callback);

  /* Add the timeout function */
  select_set_timeout(options->group, timeout, (void*)options);
#endif

  /* Create the TCP driver */
  options->driver = driver_get_tcp("localhost", 2000, options->group);

  while(TRUE)
  {
    select_group_do_select(options->group, 1000);
  }

  return 0;
}
