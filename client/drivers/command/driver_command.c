/* driver_command.c
 * By Ron Bowes
 * Created May, 2014
 *
 * See LICENSE.md
 */

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
#include "controller/session.h"
#include "controller/controller.h"
#include "drivers/driver_exec.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/types.h"

#include "driver_command.h"

void driver_command_data_received(driver_command_t *driver, uint8_t *data, size_t length)
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
      LOG_WARNING("Got a ping request! Responding!");
      out = command_packet_create_ping_response(in->request_id, in->r.request.body.ping.data);
    }
    else if(in->command_id == COMMAND_SHELL && in->is_request == TRUE)
    {
#ifdef WIN32
      session_t *session = session_create_exec(driver->group, "cmd.exe", "cmd.exe");
#else
      session_t *session = session_create_exec(driver->group, "sh", "sh");
#endif
      controller_add_session(session);

      out = command_packet_create_shell_response(in->request_id, session->id);
    }
    else if(in->command_id == COMMAND_EXEC && in->is_request == TRUE)
    {
      session_t *session = session_create_exec(driver->group, in->r.request.body.exec.name, in->r.request.body.exec.command);
      controller_add_session(session);

      out = command_packet_create_exec_response(in->request_id, session->id);
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
        FILE *f = NULL;

#ifdef WIN32
        fopen_s(&f, in->r.request.body.download.filename, "rb");
#else
        f = fopen(in->r.request.body.download.filename, "rb");
#endif
        if(!f)
        {
          out = command_packet_create_error_response(in->request_id, -1, "Error opening file for reading");
        }
        else
        {
          data = safe_malloc(s.st_size);

          if(fread(data, 1, s.st_size, f) == s.st_size)
            out = command_packet_create_download_response(in->request_id, data, s.st_size);
          else
            out = command_packet_create_error_response(in->request_id, -1, "There was an error reading the file");

          fclose(f);
          safe_free(data);
        }
      }
    }
    else if(in->command_id == COMMAND_UPLOAD && in->is_request == TRUE)
    {
#ifdef WIN32
      FILE *f;
      fopen_s(&f, in->r.request.body.upload.filename, "wb");
#else
      FILE *f = fopen(in->r.request.body.upload.filename, "wb");
#endif
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
    else if(in->command_id == COMMAND_SHUTDOWN && in->is_request == TRUE)
    {
      controller_kill_all_sessions();

      out = command_packet_create_shutdown_response(in->request_id);
    }
    else
    {
      LOG_ERROR("Got a command packet that we don't know how to handle!\n");

      out = command_packet_create_error_response(in->request_id, 0xFFFF, "Not implemented yet!");
    }

    if(out)
    {
      uint8_t *data;
      uint32_t length;

      printf("Response: ");
      command_packet_print(out);

      data = command_packet_to_bytes(out, &length);
      buffer_add_bytes(driver->outgoing_data, data, length);
      safe_free(data);
    }

    command_packet_destroy(in);
  }
}

uint8_t *driver_command_get_outgoing(driver_command_t *driver, size_t *length, size_t max_length)
{
  /* If the driver has been killed and we have no bytes left, return NULL to close the session. */
  if(driver->is_shutdown && buffer_get_remaining_bytes(driver->outgoing_data) == 0)
    return NULL;

  return buffer_read_remaining_bytes(driver->outgoing_data, length, max_length, TRUE);
}

driver_command_t *driver_command_create(select_group_t *group)
{
  driver_command_t *driver = (driver_command_t*) safe_malloc(sizeof(driver_command_t));

  driver->stream = command_packet_stream_create(TRUE);
  driver->group = group;
  driver->is_shutdown = FALSE;
  driver->outgoing_data = buffer_create(BO_LITTLE_ENDIAN);

  return driver;
}

void driver_command_destroy(driver_command_t *driver)
{
  if(!driver->is_shutdown)
    driver_command_close(driver);

  if(driver->name)
    safe_free(driver->name);

  if(driver->stream)
    command_packet_stream_destroy(driver->stream);
  safe_free(driver);
}

void driver_command_close(driver_command_t *driver)
{
  driver->is_shutdown = TRUE;
}

