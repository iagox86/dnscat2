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
#include "controller/session.h"
#include "controller/controller.h"
#include "drivers/driver_exec.h"
#include "libs/log.h"
#include "libs/memory.h"
#include "libs/select_group.h"
#include "libs/tcp.h"
#include "libs/types.h"

#include "driver_command.h"

static uint16_t request_id()
{
  static uint16_t request_id = 0;

  return request_id++;
}

/* I moved some functions into other files for better organization;
 * this includes them. */
#include "commands_standard.h"
#include "commands_tunnel.h"

void driver_command_data_received(driver_command_t *driver, uint8_t *data, size_t length)
{
  command_packet_t *in  = NULL;
  command_packet_t *out = NULL;

  buffer_add_bytes(driver->stream, data, length);

  while((in = command_packet_read(driver->stream)))
  {
    /* TUNNEL_DATA commands are too noisy to print. */
    if(in->command_id != TUNNEL_DATA)
    {
      printf("Got a command: ");
      command_packet_print(in);
    }

    switch(in->command_id)
    {
      case COMMAND_PING:
        out = handle_ping(driver, in);
        break;

      case COMMAND_SHELL:
        out = handle_shell(driver, in);
        break;

      case COMMAND_EXEC:
        out = handle_exec(driver, in);
        break;

      case COMMAND_DOWNLOAD:
        out = handle_download(driver, in);
        break;

      case COMMAND_UPLOAD:
        out = handle_upload(driver, in);
        break;

      case COMMAND_SHUTDOWN:
        out = handle_shutdown(driver, in);
        break;

      case TUNNEL_CONNECT:
        out = handle_tunnel_connect(driver, in);
        break;

      case TUNNEL_DATA:
        out = handle_tunnel_data(driver, in);
        break;

      case TUNNEL_CLOSE:
        out = handle_tunnel_close(driver, in);
        break;

      case COMMAND_ERROR:
        out = handle_error(driver, in);
        break;

      default:
        LOG_ERROR("Got a command packet that we don't know how to handle!\n");
        out = command_packet_create_error_response(in->request_id, 0xFFFF, "Not implemented yet!");
    }

    /* Respond if and only if an outgoing packet was created. */
    if(out)
    {
      uint8_t *data;
      size_t   length;

      if(out->command_id != TUNNEL_DATA)
      {
        printf("Response: ");
        command_packet_print(out);
      }

      data = command_packet_to_bytes(out, &length);
      buffer_add_bytes(driver->outgoing_data, data, length);
      safe_free(data);
      command_packet_destroy(out);
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

  driver->stream        = buffer_create(BO_BIG_ENDIAN);
  driver->group         = group;
  driver->is_shutdown   = FALSE;
  driver->outgoing_data = buffer_create(BO_LITTLE_ENDIAN);
  driver->tunnels       = ll_create(NULL);

  return driver;
}

void driver_command_destroy(driver_command_t *driver)
{
  if(!driver->is_shutdown)
    driver_command_close(driver);

  if(driver->name)
    safe_free(driver->name);

  if(driver->stream)
    buffer_destroy(driver->stream);
  safe_free(driver);
}

void driver_command_close(driver_command_t *driver)
{
  driver->is_shutdown = TRUE;
}
