/* driver_console.h
 * By Ron Bowes
 *
 * See LICENSE.md
 */

#ifndef __DRIVER_CONSOLE_H__
#define __DRIVER_CONSOLE_H__

#include "../controller/session.h"
#include "../libs/select_group.h"

typedef struct
{
#if 0
  char      *name;
  char      *download;
  uint32_t   first_chunk;
#endif
  session_t *session;
  select_group_t *group;
} driver_console_t;

/*driver_console_t  *driver_console_create(select_group_t *group, char *name, char *download, int first_chunk);*/
driver_console_t *driver_console_create(select_group_t *group, session_t *session);
void               driver_console_destroy();

#endif
