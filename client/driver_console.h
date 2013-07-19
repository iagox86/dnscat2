#ifndef __DRIVER_CONSOLE_H__
#define __DRIVER_CONSOLE_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  select_group_t     *group; /* TODO: Do I need to keep this? */
  message_handler_t  *my_message_handler;
  message_handler_t  *their_message_handler;
  uint16_t            session_id;
} driver_console_t;

driver_console_t  *driver_console_create(select_group_t *group);
void driver_console_init(driver_console_t *driver, message_handler_t *their_message_handler);

#endif
