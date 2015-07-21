/* driver.c
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

#include "libs/log.h"
#include "libs/memory.h"
#include "driver_console.h"

#include "driver.h"

driver_t *driver_create(driver_type_t type, void *real_driver)
{
  driver_t *driver = (driver_t *)safe_malloc(sizeof(driver_t));
  driver->type = type;

  switch(type)
  {
    case DRIVER_TYPE_CONSOLE:
      driver->real_driver.console = (driver_console_t*) real_driver;
      break;

    case DRIVER_TYPE_EXEC:
      driver->real_driver.exec = (driver_exec_t*) real_driver;
      break;

    case DRIVER_TYPE_COMMAND:
      driver->real_driver.command = (driver_command_t*) real_driver;
      break;

    case DRIVER_TYPE_PING:
      driver->real_driver.ping = (driver_ping_t*) real_driver;
      break;

    default:
      LOG_FATAL("UNKNOWN DRIVER TYPE! (%d in driver_create)\n", type);
      exit(1);
      break;
  }

  return driver;
}

void driver_destroy(driver_t *driver)
{
  switch(driver->type)
  {
    case DRIVER_TYPE_CONSOLE:
      driver_console_destroy(driver->real_driver.console);
      break;

    case DRIVER_TYPE_EXEC:
      driver_exec_destroy(driver->real_driver.exec);
      break;

    case DRIVER_TYPE_COMMAND:
      driver_command_destroy(driver->real_driver.command);
      break;

    case DRIVER_TYPE_PING:
      driver_ping_destroy(driver->real_driver.ping);
      break;

    default:
      LOG_FATAL("UNKNOWN DRIVER TYPE! (%d in driver_destroy)\n", driver->type);
      exit(1);
      break;
  }

  safe_free(driver);
}

void driver_close(driver_t *driver)
{
  switch(driver->type)
  {
    case DRIVER_TYPE_CONSOLE:
      driver_console_close(driver->real_driver.console);
      break;

    case DRIVER_TYPE_EXEC:
      driver_exec_close(driver->real_driver.exec);
      break;

    case DRIVER_TYPE_COMMAND:
      driver_command_close(driver->real_driver.command);
      break;

    case DRIVER_TYPE_PING:
      driver_ping_close(driver->real_driver.ping);
      break;

    default:
      LOG_FATAL("UNKNOWN DRIVER TYPE! (%d in driver_close)\n", driver->type);
      exit(1);
      break;
  }
}

void driver_data_received(driver_t *driver, uint8_t *data, size_t length)
{
  switch(driver->type)
  {
    case DRIVER_TYPE_CONSOLE:
      driver_console_data_received(driver->real_driver.console, data, length);
      break;

    case DRIVER_TYPE_EXEC:
      driver_exec_data_received(driver->real_driver.exec, data, length);
      break;

    case DRIVER_TYPE_COMMAND:
      driver_command_data_received(driver->real_driver.command, data, length);
      break;

    case DRIVER_TYPE_PING:
      driver_ping_data_received(driver->real_driver.ping, data, length);
      break;

    default:
      LOG_FATAL("UNKNOWN DRIVER TYPE! (%d in driver_data_received)\n", driver->type);
      exit(1);
      break;
  }
}

uint8_t *driver_get_outgoing(driver_t *driver, size_t *length, size_t max_length)
{
  switch(driver->type)
  {
    case DRIVER_TYPE_CONSOLE:
      return driver_console_get_outgoing(driver->real_driver.console, length, max_length);
      break;

    case DRIVER_TYPE_EXEC:
      return driver_exec_get_outgoing(driver->real_driver.exec, length, max_length);
      break;

    case DRIVER_TYPE_COMMAND:
      return driver_command_get_outgoing(driver->real_driver.command, length, max_length);
      break;

    case DRIVER_TYPE_PING:
      return driver_ping_get_outgoing(driver->real_driver.ping, length, max_length);
      break;

    default:
      LOG_FATAL("UNKNOWN DRIVER TYPE! (%d in driver_get_outgoing)\n", driver->type);
      exit(1);
      break;
  }
}

