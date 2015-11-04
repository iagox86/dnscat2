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

log_level_t log_get_min_console_level()
{
  return log_console_min;
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
