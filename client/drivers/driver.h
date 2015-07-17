/* driver.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * This is basically a hack to make a polymorphic class in C. It just lets
 * other stuff call functions, and passes it to the appropriate implementation.
 */

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include "driver_console.h"
#include "driver_exec.h"
#include "driver_ping.h"
#include "drivers/command/driver_command.h"

typedef enum
{
  DRIVER_TYPE_CONSOLE,
  DRIVER_TYPE_EXEC,
  DRIVER_TYPE_COMMAND,
  DRIVER_TYPE_PING,
} driver_type_t;

typedef struct
{
  driver_type_t type;

  union
  {
    driver_console_t *console;
    driver_exec_t    *exec;
    driver_command_t *command;
    driver_ping_t    *ping;
  } real_driver;
} driver_t;

driver_t *driver_create(driver_type_t type, void *real_driver);
void      driver_destroy(driver_t *driver);
void      driver_close(driver_t *driver);
void      driver_data_received(driver_t *driver, uint8_t *data, size_t length);
uint8_t  *driver_get_outgoing(driver_t *driver, size_t *length, size_t max_length);

#endif
