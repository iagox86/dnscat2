#ifndef __DRIVER_CONSOLE_H__
#define __DRIVER_CONSOLE_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  select_group_t     *group; /* TODO: Do I need to keep this? */
  uint16_t            session_id;
} driver_console_t;

driver_console_t  *driver_console_create(select_group_t *group);

#endif
