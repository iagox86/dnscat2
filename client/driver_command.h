#ifndef __DRIVER_command_H__
#define __DRIVER_command_H__

#include "command_packet.h"
#include "command_packet_stream.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

typedef struct
{
  char     *name;
  uint16_t  session_id;
  command_packet_stream_t *stream;
  select_group_t *group;

  NBBOOL started;
} driver_command_t;

driver_command_t  *driver_command_create(select_group_t *group);
void               driver_command_destroy();

#endif
