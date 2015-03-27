/* driver_console.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

#include "driver_console.h"

/* There can only be one driver_console, so store these as global variables. */
static SELECT_RESPONSE_t console_stdin_recv(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *d)
{
  driver_console_t *driver_console = (driver_console_t*) d;

  message_post_data_out(driver_console->session_id, data, length);

  return SELECT_OK;
}

static SELECT_RESPONSE_t console_stdin_closed(void *group, int socket, void *d)
{
  /* When the stdin pipe is closed, the stdin driver signals the end. */
  message_post_shutdown();

  return SELECT_CLOSE_REMOVE;
}

static void handle_data_in(driver_console_t *driver, uint8_t *data, size_t length)
{
  size_t i;

  for(i = 0; i < length; i++)
    fputc(data[i], stdout);
}

static void handle_message(message_t *message, void *d)
{
  driver_console_t *driver = (driver_console_t*) d;

  switch(message->type)
  {
    case MESSAGE_DATA_IN:
      if(message->message.data_in.session_id == driver->session_id)
        handle_data_in(driver, message->message.data_in.data, message->message.data_in.length);
      break;

    default:
      LOG_FATAL("driver_console received an invalid message: %d", message->type);
      abort();
  }
}

driver_console_t *driver_console_create(select_group_t *group, char *name, char *download, int first_chunk)
{
  driver_console_t *driver = (driver_console_t*) safe_malloc(sizeof(driver_console_t));

  message_options_t options[4];

#ifdef WIN32
  /* On Windows, the stdin_handle is quite complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(group, -1, stdin_handle, driver);
  select_set_recv(group,       -1, console_stdin_recv);
  select_set_closed(group,     -1, console_stdin_closed);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(group, stdin_handle, SOCKET_TYPE_STREAM, driver);
  select_set_recv(group,         stdin_handle, console_stdin_recv);
  select_set_closed(group,       stdin_handle, console_stdin_closed);
#endif

  driver->name        = name ? name : "[unnamed console]";
  driver->download    = download;
  driver->first_chunk = first_chunk;

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_DATA_IN,         handle_message, driver);

  options[0].name    = "name";
  options[0].value.s = driver->name;

  if(driver->download)
  {
    options[1].name    = "download";
    options[1].value.s = driver->download;

    options[2].name    = "first_chunk";
    options[2].value.i = driver->first_chunk;
  }
  else
  {
    options[1].name = NULL;
  }

  options[3].name    = NULL;

  driver->session_id = message_post_create_session(options);

  return driver;
}

void driver_console_destroy(driver_console_t *driver)
{
  if(driver->name)
    safe_free(driver->name);
  if(driver->download)
    safe_free(driver->download);
  safe_free(driver);
}
