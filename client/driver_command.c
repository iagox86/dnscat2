#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "command_packet.h"
#include "command_packet_stream.h"
#include "driver_exec.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

#include "driver_command.h"

/* This is called after the drivers are created, to kick things off. */
static void handle_start(driver_command_t *driver)
{
  char *name = (driver->name) ? (driver->name) : "[unnamed command]";
  message_options_t options[3];

  options[0].name    = "name";
  options[0].value.s = name;

  options[1].name    = "is_command";
  options[1].value.i = TRUE;

  options[2].name    = NULL;

  message_post_create_session(options);
}

static void handle_session_created(driver_command_t *driver, uint16_t session_id)
{
  driver->session_id = session_id;
}

static void handle_data_in(driver_command_t *driver, uint8_t *data, size_t length)
{
  command_packet_stream_feed(driver->stream, data, length);

  while(command_packet_stream_ready(driver->stream))
  {
    command_packet_t *in = command_packet_stream_read(driver->stream);
    command_packet_t *out = NULL;

    printf("Got a command:\n");
    command_packet_print(in);

    if(in->command_id == COMMAND_PING && in->is_request == TRUE)
    {
      printf("Got a ping request! Responding!\n");
      out = command_packet_create_ping_response(in->request_id, in->r.request.body.ping.data);
    }
    else if(in->command_id == COMMAND_SHELL && in->is_request == TRUE)
    {
      /* TODO: Choose the appropriate shell for the OS */
      printf("Creating a new driver_exec...\n");
      driver_exec_t *new_driver = driver_exec_create(driver->group, "/bin/sh");
      driver_exec_manual_start(new_driver);
      printf("Driver created?\n");
    }
    else
    {
      printf("Got a command packet that we don't know how to handle!\n");

      out = command_packet_create_error_response(in->request_id, 0xFFFF, "Not implemented yet!");
    }

    if(out)
    {
      uint8_t *data;
      size_t   length;

      data = command_packet_to_bytes(out, &length);

      message_post_data_out(driver->session_id, data, length);
    }
  }
}

static void handle_config_int(driver_command_t *driver, char *name, int value)
{
}

static void handle_config_string(driver_command_t *driver, char *name, char *value)
{
}

static void handle_message(message_t *message, void *d)
{
  driver_command_t *driver = (driver_command_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver);
      break;

    case MESSAGE_SESSION_CREATED:
      handle_session_created(driver, message->message.session_created.session_id);
      break;

    case MESSAGE_DATA_IN:
      handle_data_in(driver, message->message.data_in.data, message->message.data_in.length);
      break;

    case MESSAGE_CONFIG:
      if(message->message.config.type == CONFIG_INT)
        handle_config_int(driver, message->message.config.name, message->message.config.value.int_value);
      else if(message->message.config.type == CONFIG_STRING)
        handle_config_string(driver, message->message.config.name, message->message.config.value.string_value);
      else
      {
        LOG_FATAL("Unknown");
        abort();
      }
      break;

    default:
      LOG_FATAL("driver_command received an invalid message: %d", message->type);
      abort();
  }
}

driver_command_t *driver_command_create(select_group_t *group)
{
  driver_command_t *driver = (driver_command_t*) safe_malloc(sizeof(driver_command_t));

  driver->stream = command_packet_stream_create(TRUE);
  driver->group = group;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,           handle_message, driver);
  message_subscribe(MESSAGE_SESSION_CREATED, handle_message, driver);
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver);
  message_subscribe(MESSAGE_CONFIG,          handle_message, driver);

  return driver;
}

void driver_command_destroy(driver_command_t *driver)
{
  if(driver->name)
    safe_free(driver->name);
  if(driver->stream)
    command_packet_stream_destroy(driver->stream);
  safe_free(driver);
}
