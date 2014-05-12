#ifndef __DRIVER_command_H__
#define __DRIVER_command_H__

#include "message.h"
#include "select_group.h"
#include "session.h"

typedef struct
{
  uint16_t   session_id;
  char      *name;
  char      *download;
  uint32_t   first_chunk;
} driver_command_t;

driver_command_t  *driver_command_create(select_group_t *group);
void               driver_command_destroy();

#endif
