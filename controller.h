#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

/* Controller will:
 * - Manage the list of sessions
 * - Grab data as needed and feed it to the driver
 * - Find out from the driver how much data it can accept at a given
 *   time
 * - This has to know the difference between a client and a server
 */

#include "buffer.h"
#include "driver.h"
#include "memory.h"
#include "types.h"

typedef enum
{
  FLAG_STREAM, /* Packet contains SEQ/ACK */
} flags_t;

typedef struct
{
  uint16_t id;
  uint16_t window_size;
  uint16_t current_seq;
  uint16_t current_ack;
  size_t max_packet_size;
  uint8_t data_waiting;

  driver_t  *driver;

  buffer_t *outgoing_data;
  buffer_t *incoming_data;
} controller_t;

controller_t *controller_create(driver_t *driver);
void controller_connect(controller_t *c);
void controller_send(controller_t *c, uint8_t *data, size_t length);
int controller_do_actions(controller_t *c);
int controller_recv(controller_t *c, uint8_t *buffer, size_t max_length);
void controller_cleanup(controller_t *c);

#endif
