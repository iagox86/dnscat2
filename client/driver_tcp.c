/* driver_tcp.c
 * Created March/2013
 * By Ron Bowes
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "buffer.h"
#include "memory.h"
#include "select_group.h"
#include "tcp.h"

#define DNSCAT_TCP

#include "driver.h"

#define DEFAULT_HOST "localhost"
#define DEFAULT_PORT 2000

driver_t *driver_create(int argc, char *argv[], select_group_t *group)
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

  /* Set the dns-specific options for the driver */
  driver_t *driver        = safe_malloc(sizeof(driver_t));

  /* Set up some default options. */
  driver->max_packet_size = 1394;
  driver->s               = -1;
  driver->group           = group;

  driver->host            = DEFAULT_HOST;
  driver->port            = DEFAULT_PORT;

  driver->callback        = NULL; /* TODO: I can probably pass this as an arg now */
  driver->callback_param  = NULL;

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
          driver->host = optarg;
        }
        else if(!strcmp(option_name, "port"))
        {
          driver->port = atoi(optarg);
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
  fprintf(stderr, " Host: %s\n", driver->host);
  fprintf(stderr, " Port: %d\n", driver->port);

  exit(0);

  return driver;
}

static SELECT_RESPONSE_t recv_callback(void *group, int s, uint8_t *data, size_t length, char *addr, uint16_t port, void *param)
{
  driver_t *driver = (driver_t*)param;

  /* Cleanup - if the buffer is empty, reset it */
  if(buffer_get_remaining_bytes(driver->buffer) == 0)
    buffer_clear(driver->buffer);

  buffer_add_bytes(driver->buffer, data, length);

  /* If we have at least a length value */
  if(buffer_get_remaining_bytes(driver->buffer) >= 2)
  {
    /* Read the length. */
    uint16_t expected_length = buffer_peek_next_int16(driver->buffer);

    /* Check if we have the full length. */
    if(buffer_get_remaining_bytes(driver->buffer) - 2 >= expected_length)
    {
      uint8_t *data;
      size_t   returned_length;

      /* Consume the value we already know */
      buffer_read_next_int16(driver->buffer);

      /* Read the rest of the buffer. */
      data = buffer_read_remaining_bytes(driver->buffer, &returned_length, expected_length, TRUE);

      /* Sanity check. */
      assert(expected_length == returned_length);

      /* Do the callback. */
      driver->callback(data, returned_length, driver->callback_param);

      /* Free it. */
      safe_free(data);

      /* Clear the buffer if it's empty. */
      if(buffer_get_remaining_bytes(driver->buffer) == 0)
        buffer_clear(driver->buffer);
    }
  }

  return SELECT_OK;
}


static SELECT_RESPONSE_t closed_callback(void *group, int s, void *param)
{
  driver_t *driver = (driver_t*)param;

  printf("[[TCP]] :: Connection closed\n");

  driver_close(driver);

  return SELECT_OK;
}

void driver_send_packet(driver_t *driver, packet_t *packet)
{
  size_t   length;
  uint8_t *data = packet_to_bytes(packet, &length);

  driver_send(driver, data, length);

  safe_free(data);
}

void driver_send(driver_t *driver, uint8_t *data, size_t length)
{
  buffer_t     *buffer;
  uint8_t      *encoded_data;
  size_t        encoded_length;

  if(driver->s == -1)
  {
    /* Attempt a TCP connection */
    printf("[[TCP]] :: connecting to %s:%d\n", driver->host, driver->port);
    driver->s = tcp_connect(driver->host, driver->port);

    /* If it fails, just return (it will try again next send) */
    if(driver->s == -1)
    {
      printf("[[TCP]] :: connection failed!\n");
      return;
    }

    /* If it succeeds, add it to the select_group */
    select_group_add_socket(driver->group, driver->s, SOCKET_TYPE_STREAM, driver);
    select_set_recv(driver->group, driver->s, recv_callback);
    select_set_closed(driver->group, driver->s, closed_callback);
  }

  assert(driver->s != -1); /* Make sure we have a valid socket. */
  assert(data); /* Make sure they aren't trying to send NULL. */
  assert(length > 0); /* Make sure they aren't trying to send 0 bytes. */

  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int16(buffer, length);
  buffer_add_bytes(buffer, data, length);
  encoded_data = buffer_create_string_and_destroy(buffer, &encoded_length);

  if(tcp_send(driver->s, encoded_data, encoded_length) == -1)
  {
    printf("[[TCP]] send error, closing socket!\n");
    driver_close(driver);
  }
}

void driver_close(driver_t *driver)
{
  printf("[[TCP]] :: close()\n");

  assert(driver->s && driver->s != -1); /* We can't close a closed socket */

  /* Remove from the select_group */
  select_group_remove_and_close_socket(driver->group, driver->s);
  driver->s = -1;
}

void driver_destroy(driver_t *driver)
{
  printf("[[TCP]] :: cleanup()\n");

  /* Ensure the driver is closed */
  if(driver->s != -1)
    driver_close(driver);

  buffer_destroy(driver->buffer);

  safe_free(driver->host);
  driver->host = NULL;
  safe_free(driver);
}

void driver_register_callback(driver_t *driver, driver_callback_t *callback, void *callback_param)
{
  driver->callback       = callback;
  driver->callback_param = callback_param;
}
