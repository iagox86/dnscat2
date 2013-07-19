/* Created 2013-07-15 */
/* Drivers are stateless! */

#ifndef __DRIVER_H__
#define __DRIVER_H__

#include "session.h"
#include "types.h"

#include "driver_console.h"
#include "driver_dns.h"

typedef enum
{
  DRIVER_TYPE_CONSOLE,
  DRIVER_TYPE_DNS
} driver_type_t;

typedef struct
{
  driver_type_t type;
  union
  {
    driver_console_t *console;
    driver_dns_t     *dns;
  } driver;
} driver_t;

driver_t *driver_create_console(select_group_t *group);
driver_t *driver_create_dns(select_group_t *group);

#endif
