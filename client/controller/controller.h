/**
 * controller.h
 * Created by Ron Bowes
 * On April, 2015
 *
 * See LICENSE.md
 */

#include "libs/types.h"
#include "session.h"

size_t controller_open_session_count();
void controller_add_session(session_t *session);
void controller_data_incoming(uint8_t *data, size_t length);
uint8_t *controller_get_outgoing(size_t *length, size_t max_length);
