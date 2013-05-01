/* options_tcp.c
 * Created March/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>

#include "buffer.h"
#include "memory.h"
#include "select_group.h"
#include "session.h"
#include "tcp.h"

typedef struct
{
  session_t *session;

  int       s;
  char     *host;
  uint16_t  port;

  /* This is for buffering data until we get a full packet */
  buffer_t *buffer;
} options_t;

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 2000

options_t *options = NULL;


static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  options_t *options = (options_t*)param;

  /* Cleanup - if the buffer is empty, reset it */
  if(buffer_get_remaining_bytes(options->buffer) == 0)
    buffer_clear(options->buffer);

  buffer_add_bytes(options->buffer, data, length);

  /* If we have at least a length value */
  if(buffer_get_remaining_bytes(options->buffer) >= 2)
  {
    /* Read the length. */
    uint16_t expected_length = buffer_peek_next_int16(options->buffer);

    /* Check if we have the full length. */
    if(buffer_get_remaining_bytes(options->buffer) - 2 >= expected_length)
    {
      uint8_t *data;
      size_t   returned_length;

      /* Consume the value we already know */
      buffer_read_next_int16(options->buffer);

      /* Read the rest of the buffer. */
      data = buffer_read_remaining_bytes(options->buffer, &returned_length, expected_length, TRUE);

      /* Sanity check. */
      assert(expected_length == returned_length);

      /* Do the callback. */
      session_recv(options->session, data, returned_length);

      /* Free it. */
      safe_free(data);

      /* Clear the buffer if it's empty. */
      if(buffer_get_remaining_bytes(options->buffer) == 0)
        buffer_clear(options->buffer);
    }
  }

  return SELECT_OK;
}

void tcpcat_close(options_t *options)
{
  printf("[[TCP]] :: close()\n");

  assert(options->s && options->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  session_unregister_socket(options->session, options->s);
  tcp_close(options->s);
  options->s = -1;
}

static SELECT_RESPONSE_t closed_callback(void *group, int s, void *param)
{
  options_t *options = (options_t*)param;

  printf("[[TCP]] :: Connection closed\n");

  tcpcat_close(options);

  return SELECT_OK;
}

void tcpcat_send(uint8_t *data, size_t length, void *d)
{
  options_t     *options = (options_t*) d;
  buffer_t     *buffer;
  uint8_t      *encoded_data;
  size_t        encoded_length;

  if(options->s == -1)
  {
    /* Attempt a TCP connection */
    printf("[[TCP]] :: connecting to %s:%d\n", options->host, options->port);
    options->s = tcp_connect(options->host, options->port);

    /* If it fails, just return (it will try again next send) */
    if(options->s == -1)
    {
      printf("[[TCP]] :: connection failed!\n");
      return;
    }

    /* If it succeeds, add it to the select_group */
    session_register_socket(options->session, options->s, SOCKET_TYPE_STREAM, recv_callback, closed_callback, options);
  }

  assert(options->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int16(buffer, length);
  buffer_add_bytes(buffer, data, length);
  encoded_data = buffer_create_string_and_destroy(buffer, &encoded_length);

  if(tcp_send(options->s, encoded_data, encoded_length) == -1)
  {
    printf("[[TCP]] send error, closing socket!\n");
    tcpcat_close(options);
  }
}

void cleanup()
{
  fprintf(stderr, "[[tcpcat]] :: Terminating\n");

  if(options)
  {
    session_destroy(options->session);

    /* Ensure the options is closed */
    if(options->s != -1)
      tcpcat_close(options);
    buffer_destroy(options->buffer);
    safe_free(options);
    options = NULL;
  }

  print_memory();
}

int main(int argc, char *argv[])
{
  /* Define the options specific to the DNS protocol. */
  struct option long_options[] =
  {
    {"host", required_argument, 0, 0},
    {"port", required_argument, 0, 0},
    {0,      0,                 0, 0}  /* End */
  };
  char        c;
  int         option_index;
  const char *option_name;

  options        = safe_malloc(sizeof(options_t));

  srand(time(NULL));

  /* Set up some default options. */
  options->s               = -1;

  options->host            = DEFAULT_HOST;
  options->port            = DEFAULT_PORT;
  options->buffer          = buffer_create(BO_BIG_ENDIAN);

  options->session         = session_create(tcpcat_send, options);

  /* Parse the command line options. */
  opterr = 0;
  while((c = getopt_long_only(argc, argv, "", long_options, &option_index)) != EOF)
  {
    switch(c)
    {
      case 0:
        option_name = long_options[option_index].name;

        printf("name: %s\n", option_name);

        if(!strcmp(option_name, "host"))
        {
          options->host = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          options->port = atoi(optarg);
        }
        else
        {
          fprintf(stderr, "Unknown option: %s\n", option_name);
          exit(1);
          /* TODO: Usage */
        }
        break;

      case '?':
      default:
        /* Do nothing; we expect some unknown arguments. */
        break;
    }
  }

  /* Tell the user what's going on */
  fprintf(stderr, "Options selected:\n");
  fprintf(stderr, " Host: %s\n", options->host);
  fprintf(stderr, " Port: %d\n", options->port);

  atexit(cleanup);

  session_go(options->session);

  return 0;
}

