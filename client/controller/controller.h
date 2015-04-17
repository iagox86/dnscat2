/**
 * controller.h
 * Created by Ron Bowes
 * On April, 2015
 *
 * See LICENSE.md
 */

#include "../libs/types.h"

void controller_data_incoming(uint8_t *data, size_t length);
uint8_t *controller_get_outgoing(size_t *length, size_t max_length);
