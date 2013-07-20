/* Created 2013-07-15 */

#include <stdlib.h>
#include <time.h>

#include "log.h"
#include "select_group.h"
#include "driver.h"

#include "engine.h"

void engine_go(driver_t *driver1, driver_t *driver2, select_group_t *group)
{
  /* Just do the selects */
  while(TRUE)
  {
    select_group_do_select(group, 1000); /* TODO: Configurable timeout */
  }
}

int main(int argc, const char *argv[])
{
/*  char           *domain   = "skullseclabs.org";
  char           *dns_host = "localhost";
  int             dns_port = 53; */
  select_group_t *group    = select_group_create();

  driver_t *driver1;
  driver_t *driver2;

  /* Set the default log level */
  log_set_min_console_level(LOG_LEVEL_INFO);

  driver1 = driver_create_console(group);
  driver2 = driver_create_dns(group);

  driver_console_init(driver1->driver.console, driver2->driver.dns->my_message_handler);
  driver_dns_init    (driver2->driver.dns,     driver1->driver.console->my_message_handler);

  srand(time(NULL));


  engine_go(driver1, driver2, group);

  return 0;
}
