/* tunnel_driver.h
 * Created April/2015
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * I don't think this is actually being used yet.
 */

#include "driver_dns.h"
#include "types.h"

typedef enum
{
  TUNNEL_DRIVER_DNS,
  TUNNEL_DRIVER_TCP
} tunnel_driver_type;

typedef struct
{
  tunnel_driver_type type;
  union
  {
    driver_dns_t dns;
    /* driver_tcp_t tcp; */
  } driver;
} t_tunnel_driver;

void tunnel_driver_send(uint8_t *data, size_t size)
{
}
