#include <stdint.h>
#include <stdlib.h>

#include "packet.h"

packet_t *packet_parse(uint8_t *data, size_t length)
{
  return NULL;
}

packet_t *create_syn(uint16_t session_id, uint16_t seq, uint16_t options)
{
  return NULL;
}

packet_t *create_msg(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length)
{
  return NULL;
}

packet_t *create_fin(uint16_t session_id)
{
  return NULL;
}

uint8_t *packet_to_bytes(packet_t *packet, size_t *length)
{
  return NULL;
}
