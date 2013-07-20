#ifndef __SESSIONS_H__
#define __SESSIONS_H__

#include "session.h"

void sessions_add(session_t *session);
session_t *sessions_get_by_id(uint16_t session_id);
session_t *sessions_remove(uint16_t session_id);
void sessions_do_actions();
void sessions_destroy();

#endif
