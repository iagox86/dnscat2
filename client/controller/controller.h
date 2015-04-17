/**
 * controller.h
 * Created by Ron Bowes
 * On April, 2015
 *
 * See LICENSE.md
 */

typedef enum
{
  TUNNEL_DRIVER_DNS,
} tunnel_driver_t;

void controller_data_received(uint8_t *data, size_t length);
void controller_set_send_sink(tunnel_driver_t type, void *driver);
