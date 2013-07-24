#ifndef __SESSIONS_H__
#define __SESSIONS_H__

#include "session.h"

session_t *sessions_get_by_id(uint16_t session_id);
void sessions_do_actions();
size_t sessions_total_bytes_queued();
void sessions_close();
void sessions_destroy();

void sessions_init();

#endif
