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

#include "log.h"
#include "memory.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

#include "driver_ping.h"

#define PING_LENGTH 16

static void handle_ping(driver_ping_t *driver, char *data)
{
  if(!strcmp(data, driver->data))
  {
    printf("Ping response received! This seems like a valid dnscat2 server.\n");
    message_post_shutdown();
  }
  else
  {
    printf("Ping response received, but it didn't contain the right data!\n");
    printf("Expected: %s\n", driver->data);
    printf("Received: %s\n", data);
  }
}

static void handle_heartbeat(driver_ping_t *driver)
{
  static int count = 0;

  count++;
  if(count > 10)
  {
    printf("There doesn't seem to be a dnscat2 server there. :(\n");
    message_post_shutdown();
  }

  printf("Sending a ping...\n");
  message_post_ping_request(driver->data);
}

static void handle_message(message_t *message, void *d)
{
  driver_ping_t *driver = (driver_ping_t*) d;

  switch(message->type)
  {
    case MESSAGE_PING_RESPONSE:
      handle_ping(driver, message->message.ping_response.data);
      break;

    case MESSAGE_HEARTBEAT:
      handle_heartbeat(driver);
      break;

    default:
      LOG_FATAL("driver_ping received an invalid message: %d", message->type);
      abort();
  }
}

driver_ping_t *driver_ping_create(select_group_t *group)
{
  size_t i;
  driver_ping_t *driver = (driver_ping_t*) safe_malloc(sizeof(driver_ping_t));

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_PING_RESPONSE, handle_message, driver);
  message_subscribe(MESSAGE_HEARTBEAT,     handle_message, driver);

  /* Create the data */
  driver->data = safe_malloc(PING_LENGTH + 1);

  for(i = 0; i < PING_LENGTH; i++)
    driver->data[i] = (rand() % 26) + 'a';

  /* Note: The actual ping will be sent on a heartbeat */

  return driver;
}

void driver_ping_destroy(driver_ping_t *driver)
{
  if(driver->data)
    safe_free(driver->data);
  safe_free(driver);
}
