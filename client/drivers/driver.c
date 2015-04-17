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

#include "libs/memory.h"
#include "driver_console.h"

#include "driver.h"

driver_t *driver_create(driver_type_t type, void *real_driver)
{
  driver_t *driver = (driver_t *)safe_malloc(sizeof(driver_t));
  switch(type)
  {
    case DRIVER_TYPE_CONSOLE:
      driver->real_driver.console = (driver_console_t*) real_driver;
      break;

    default:
      printf("UNKNOWN DRIVER TYPE!\n");
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
      return driver_console_destroy(driver->real_driver.console);
      break;

    default:
      printf("UNKNOWN DRIVER TYPE!\n");
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
      return driver_console_close(driver->real_driver.console);
      break;

    default:
      printf("UNKNOWN DRIVER TYPE!\n");
      exit(1);
      break;
  }

  safe_free(driver);
}

void driver_data_received(driver_t *driver, uint8_t *data, size_t length)
{
  switch(driver->type)
  {
    case DRIVER_TYPE_CONSOLE:
      return driver_console_data_received(driver->real_driver.console, data, length);
      break;

    default:
      printf("UNKNOWN DRIVER TYPE!\n");
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

    default:
      printf("UNKNOWN DRIVER TYPE!\n");
      exit(1);
      break;
  }
}

