/* driver_dns.h
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#ifndef __DRIVER_DNS_H__
#define __DRIVER_DNS_H__

#include "libs/dns.h"
#include "libs/select_group.h"

typedef struct
{
  int        s;

  char      *domain;
  char      *dns_server;
  int        dns_port;

  NBBOOL     is_closed;
  dns_type_t type;

} driver_dns_t;

driver_dns_t *driver_dns_create(select_group_t *group, char *domain, char *host, uint16_t port, dns_type_t type, char *server);
void          driver_dns_destroy();

#endif
