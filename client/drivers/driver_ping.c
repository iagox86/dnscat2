/* driver_ping.c
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
#include "libs/select_group.h"
#include "libs/types.h"

#include "driver_ping.h"

#define PING_LENGTH 16

void driver_ping_data_received(driver_ping_t *driver, uint8_t *data, size_t length)
{
  if(!strcmp((char*)data, (char*)driver->data))
  {
    printf("Ping response received! This seems like a valid dnscat2 server.\n");

    exit(0);
  }
  else
  {
    printf("Ping response received, but it didn't contain the right data!\n");
    printf("Expected: %s\n", driver->data);
    printf("Received: %s\n", data);
    printf("\n");
    printf("The only reason this can happen is if something is messing with\n");
    printf("your DNS traffic.\n");
  }
}

uint8_t *driver_ping_get_outgoing(driver_ping_t *driver, size_t *length, size_t max_length)
{
  static NBBOOL already_sent = FALSE;
  uint8_t *result = NULL;

  /* Only return this once. */
  if(already_sent) {
    *length = 0;
    return safe_malloc(0);
  }
  already_sent = TRUE;

  if(PING_LENGTH > max_length)
  {
    LOG_FATAL("Sorry, the ping packet is too long to respect the protocol's length restrictions :(");
    exit(1);
  }

  result = safe_malloc(PING_LENGTH);
  memcpy(result, driver->data, PING_LENGTH);
  *length = PING_LENGTH;

  return result;
}

driver_ping_t *driver_ping_create(select_group_t *group)
{
  size_t i;
  driver_ping_t *driver = (driver_ping_t*) safe_malloc(sizeof(driver_ping_t));
  driver->is_shutdown = FALSE;

  /* Create the data */
  driver->data = safe_malloc(PING_LENGTH + 1);
  memset(driver->data, 0, PING_LENGTH);

  for(i = 0; i < PING_LENGTH; i++)
    driver->data[i] = (rand() % 26) + 'a';

  return driver;
}

void driver_ping_destroy(driver_ping_t *driver)
{
  if(!driver->is_shutdown)
    driver_ping_close(driver);

  if(driver->data)
    safe_free(driver->data);
  safe_free(driver);
}

void driver_ping_close(driver_ping_t *driver)
{
  driver->is_shutdown = TRUE;
}
