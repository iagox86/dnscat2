/* log.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * This is a pretty boring module that is used for logging data to the
 * console.
 */

#ifndef __LOG_H__
#define __LOG_H__

typedef enum
{
  LOG_LEVEL_INFO    = 0,
  LOG_LEVEL_WARNING = 1,
  LOG_LEVEL_ERROR   = 2,
  LOG_LEVEL_FATAL   = 3
} log_level_t;

#define LOG_INFO    log_info
#define LOG_WARNING log_warning
#define LOG_ERROR   log_error
#define LOG_FATAL   log_fatal

void log_init();

void log_to_file(char *filename, log_level_t min_level);
void log_set_min_console_level(log_level_t level);
log_level_t log_get_min_console_level();

void log_info(char *format, ...);
void log_warning(char *format, ...);
void log_error(char *format, ...);
void log_fatal(char *format, ...);

#endif
