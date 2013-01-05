#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

/* Controller will:
 * - Manage the list of sessions
 * - Grab data as needed and feed it to the driver
 * - Driver knows how much data it can accept
 * - This has to know the difference between a client and a server
 */

#include "buffer.h"
#include "driver.h"
#include "types.h"

typedef void(output_pipe_t)(uint8_t *data, uint16_t length);
typedef int(input_pipe_t)(uint8_t *data, uint16_t max_length);

typedef struct
{
  uint16_t id;
  uint16_t window_size;
  uint16_t current_seq;
  uint16_t current_ack;
  uint16_t max_packet_size;

  driver_t  *driver;

  output_pipe_t *output;
  input_pipe_t  *input;

  buffer_t *queued_data;
} controller_t;

controller_t *controller_new(driver_t *driver);

#endif
