#include <assert.h>
#include <unistd.h>

#include "controller.h"
#include <stdio.h>

static uint16_t current_id = 0;

controller_t *controller_create(driver_t *driver)
{
  controller_t *c;

  driver->driver_init(driver->driver);

  assert(driver->driver);
  assert(driver->driver_send);
  assert(driver->driver_recv);
  assert(driver->driver_connect);

  c                  = safe_malloc(sizeof(controller_t));
  c->id              = current_id++;
  c->window_size     = driver->default_window_size;
  c->current_seq     = 0;
  c->current_ack     = 0;
  c->max_packet_size = driver->max_packet_size;
  c->driver          = driver;

  c->outgoing_data   = buffer_create(BO_LITTLE_ENDIAN);
  c->incoming_data = buffer_create(BO_LITTLE_ENDIAN);

  return c;
}

void controller_connect(controller_t *c)
{
  c->driver->driver_connect(c->driver->driver);
}

void controller_send(controller_t *c, uint8_t *data, size_t length)
{
  buffer_add_bytes(c->outgoing_data, data, length);
}

/* Returns -1 on error, otherwise returns number of bytes waiting to be received */
int controller_do_actions(controller_t *c)
{
  if(buffer_get_length(c->outgoing_data) != 0)
  {
    /* TODO: We need a better function for this */
    size_t to_read;

    uint8_t in_buffer[1024]; /* TODO: Better size? Peek function? */
    size_t in_length;

    buffer_print(c->outgoing_data);

    /* Figure out how much data is waiting to go out */
    to_read = buffer_get_remaining_bytes(c->outgoing_data);
    if(to_read > 0)
    {
      uint8_t *buffer;

      /* Limit the amount of data to the max_packet_size */
      to_read = to_read > c->max_packet_size ? c->max_packet_size : to_read;

      /* Allocate a buffer and send it */
      buffer = safe_malloc(to_read);
      buffer_read_next_bytes(c->outgoing_data, buffer, to_read);
      c->driver->driver_send(c->driver->driver, buffer, to_read);
      safe_free(buffer);
    }

    /* Read incoming data if there is any */
    in_length = c->driver->driver_recv(c->driver->driver, in_buffer, 1024);
    if(in_length == -1)
    {
      printf("driver_recv returned an error!\n");

      return -1;
    }

    if(in_length > 0)
    {
      printf("in_length = %zu\n", in_length);
      buffer_add_bytes(c->incoming_data, in_buffer, in_length);
    }

    printf("Data received so far: %zu bytes\n", buffer_get_length(c->incoming_data));
  }
}

int controller_recv(controller_t *c, uint8_t *buffer, size_t max_length)
{
  size_t to_read;

  to_read = buffer_get_length(c->incoming_data) - buffer_get_current_offset(c->incoming_data);
  to_read = to_read > max_length ? max_length : to_read;

  if(to_read == 0)
    return 0;

  buffer_read_next_bytes(c->incoming_data, buffer, to_read);

  return to_read;
}

void controller_cleanup(controller_t *c)
{
  c->driver->driver_close(c->driver->driver);
  c->driver->driver_cleanup(c->driver->driver);
  safe_free(c->driver);

  buffer_destroy(c->incoming_data);
  buffer_destroy(c->outgoing_data);
  safe_free(c);
}
