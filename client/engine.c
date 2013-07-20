/* Created 2013-07-15 */

#include <stdlib.h>
#include <time.h>

#include "driver.h"
#include "log.h"
#include "message.h"
#include "select_group.h"

#include "engine.h"

void engine_go(driver_t *driver1, driver_t *driver2, select_group_t *group)
{
  /* Kick things off. */
  message_t *message = message_create_start();
  message_post(message);
  message_destroy(message);

  /* Loop forever. */
  while(TRUE)
    select_group_do_select(group, 1000); /* TODO: Configurable timeout */
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

  srand(time(NULL));

  engine_go(driver1, driver2, group);

  return 0;
}
