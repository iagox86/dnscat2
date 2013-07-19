#ifndef __ENGINE_H__
#define __ENGINE_H__

#include "driver.h"
#include "select_group.h"

void engine_go(driver_t *driver1, driver_t *driver2, select_group_t *group);

#endif
