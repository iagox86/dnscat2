/* driver_dns.h
 * By Ron Bowes
 *
 * See LICENSE.md
 *
 * This is a "tunnel driver" for DNS. What that means is, it converts data
 * on the wire - in this case, DNS packets - into a stream of bytes that
 * form the actual dnscat protocol.
 *
 * By abstracting out the 'tunnel protocol' from the dnscat protocol, it
 * makes it trivial to use the same program to tunnel over multiple different
 * protocols - DNS, ICMP, etc.
 */

#ifndef __DRIVER_DNS_H__
#define __DRIVER_DNS_H__

#include "libs/dns.h"
#include "libs/select_group.h"

/* Types of DNS queries we support */
#ifndef WIN32
#define DNS_TYPES "TXT, CNAME, MX, A, AAAA"
#else
#define DNS_TYPES "TXT, CNAME, MX, A"
#endif

/* The default types. */
#define DEFAULT_TYPES "TXT,CNAME,MX"

/* The maximum number of types that can be selected amongst. */
#define DNS_MAX_TYPES 32

typedef struct
{
  int              s;

  select_group_t  *group;
  char            *domain;
  char            *dns_server;
  int              dns_port;

  NBBOOL           is_closed;

  dns_type_t       types[DNS_MAX_TYPES];
  size_t           type_count;

} driver_dns_t;

driver_dns_t *driver_dns_create(select_group_t *group, char *domain, char *host, uint16_t port, char *types, char *server);
void          driver_dns_destroy();
void          driver_dns_go(driver_dns_t *driver);

#endif
