/* log.c
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "assert.h"
#include "memory.h"

#include "log.h"

static log_level_t log_console_min = LOG_LEVEL_WARNING;
static log_level_t log_file_min = LOG_LEVEL_INFO;
static FILE *log_file = NULL;

static char *log_levels[] = { "INFO", "WARNING", "ERROR", "FATAL" };

void log_to_file(char *filename, log_level_t min_level)
{
  assert(min_level >= LOG_LEVEL_INFO || min_level <= LOG_LEVEL_FATAL);

#ifdef WIN32
  fopen_s(&log_file, filename, "w");
#else
  log_file = fopen(filename, "w");
#endif
  if(log_file)
    log_file_min = min_level;
  else
    LOG_WARNING("Couldn't open logfile: %s", filename);
}

void log_set_min_console_level(log_level_t min_level)
{
  assert(min_level >= LOG_LEVEL_INFO || min_level <= LOG_LEVEL_FATAL);

  log_console_min = min_level;
}

/* Most of this code is from the manpage for vsprintf() */
static void log_internal(log_level_t level, char *format, va_list args)
{
  assert(level >= LOG_LEVEL_INFO || level <= LOG_LEVEL_FATAL);

  if(level >= log_console_min)
  {
    fprintf(stderr, "[[ %s ]] :: ", log_levels[level]);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
  }

  if(log_file && level >= log_file_min)
  {
    vfprintf(log_file, format, args);
  }
}

void log_info(char *format, ...)
{
  va_list args;

  va_start(args, format);
  log_internal(LOG_LEVEL_INFO, format, args);
  va_end(args);
}

void log_warning(char *format, ...)
{
  va_list args;

  va_start(args, format);
  log_internal(LOG_LEVEL_WARNING, format, args);
  va_end(args);
}

void log_error(char *format, ...)
{
  va_list args;

  va_start(args, format);
  log_internal(LOG_LEVEL_ERROR, format, args);
  va_end(args);
}

void log_fatal(char *format, ...)
{
  va_list args;

  va_start(args, format);
  log_internal(LOG_LEVEL_FATAL, format, args);
  va_end(args);
}

static void handle_message(message_t *message, void *d)
{
  /*char *tmp;*/

  switch(message->type)
  {
    case MESSAGE_CONFIG:
      if(message->message.config.type == CONFIG_INT)
        LOG_WARNING("Setting config: %s => %d", message->message.config.name, message->message.config.value.int_value);
      else if(message->message.config.type == CONFIG_STRING)
        LOG_WARNING("Setting config: %s => %s", message->message.config.name, message->message.config.value.string_value);
      else
        LOG_WARNING("Setting config: %s => [unknown type]", message->message.config.name);
      break;

    case MESSAGE_SHUTDOWN:
      LOG_WARNING("Dnscat shutting down");
      break;

    case MESSAGE_CREATE_SESSION:
      LOG_WARNING("Session creation request");
      break;

    case MESSAGE_SESSION_CREATED:
      LOG_WARNING("Session successfully created: %d", message->message.session_created.session_id);
      break;

    case MESSAGE_CLOSE_SESSION:
      LOG_WARNING("Session closure request: %d", message->message.close_session.session_id);
      break;

    case MESSAGE_SESSION_CLOSED:
      LOG_WARNING("Session closed: %d", message->message.session_closed.session_id);
      break;

    case MESSAGE_DATA_OUT:
      LOG_INFO("Data queued: %d bytes to session %d", message->message.data_out.length, message->message.data_out.session_id);
      break;

/*    case MESSAGE_PACKET_OUT:
      tmp = packet_to_s(message->message.packet_out.packet);
      LOG_INFO("[OUT]: %s", tmp);
      safe_free(tmp);
      break;

    case MESSAGE_PACKET_IN:
      tmp = packet_to_s(message->message.packet_in.packet);
      LOG_INFO("[IN]: %s", tmp);
      safe_free(tmp);
      break;*/

    case MESSAGE_DATA_IN:
      LOG_INFO("Data being returned to the client; %d bytes to session %d", message->message.data_in.length, message->message.data_in.session_id);
      break;

    case MESSAGE_HEARTBEAT:
      /* Logging this is just annoying. */
      /* LOG_WARNING("Heartbeat..."); */
      break;

    default:
      LOG_FATAL("Unknown message type: %d", message->type);
      exit(1);
  }
}

void log_init()
{
  message_subscribe(MESSAGE_CONFIG,           handle_message, NULL);
  message_subscribe(MESSAGE_SHUTDOWN,         handle_message, NULL);
  message_subscribe(MESSAGE_CREATE_SESSION,   handle_message, NULL);
  message_subscribe(MESSAGE_SESSION_CREATED,  handle_message, NULL);
  message_subscribe(MESSAGE_CLOSE_SESSION,    handle_message, NULL);
  message_subscribe(MESSAGE_SESSION_CLOSED,   handle_message, NULL);
  message_subscribe(MESSAGE_DATA_OUT,         handle_message, NULL);
/*  message_subscribe(MESSAGE_PACKET_OUT,       handle_message, NULL);
  message_subscribe(MESSAGE_PACKET_IN,        handle_message, NULL); */
  message_subscribe(MESSAGE_DATA_IN,          handle_message, NULL);
  message_subscribe(MESSAGE_HEARTBEAT,        handle_message, NULL);
}
