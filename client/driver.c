#include "driver_console.h"
#include "driver_dns.h"
#include "memory.h"
#include "select_group.h"

#include "driver.h"

driver_t *driver_create()
{
  return (driver_t*) safe_malloc(sizeof(driver_t));
}

driver_t *driver_create_console(select_group_t *group)
{
  driver_t *driver = driver_create();
  driver->type = DRIVER_TYPE_CONSOLE;
  driver->driver.console = driver_console_create(group);

  return driver;
}

driver_t *driver_create_dns(select_group_t *group)
{
  driver_t *driver = driver_create();
  driver->type = DRIVER_TYPE_DNS;
  driver->driver.dns = driver_dns_create(group);

  return driver;
}

