#ifndef __DRIVER_CONSOLE_H__
#define __DRIVER_CONSOLE_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  uint16_t   session_id;
  char      *name;
} driver_console_t;

driver_console_t  *driver_console_create(select_group_t *group);
void               driver_console_destroy();

#endif
