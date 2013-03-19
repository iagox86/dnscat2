#ifndef __PACKET_H__
#define __PACKET_H__

#include <stdint.h>
#include <stdlib.h>

#define MAX_PACKET_SIZE 1024

typedef enum
{
  MESSAGE_TYPE_SYN = 0x00,
  MESSAGE_TYPE_MSG = 0x01,
  MESSAGE_TYPE_FIN = 0x02,
} message_type_t;

typedef struct
{
  uint16_t seq;
  uint16_t options;
} syn_packet_t;

typedef struct
{
  uint16_t seq;
  uint16_t ack;
  uint8_t *data;
  size_t   data_length;
} msg_packet_t;

typedef struct
{
} fin_packet_t;

typedef struct
{
  message_type_t message_type;
  uint16_t session_id;

  union
  {
    syn_packet_t syn;
    msg_packet_t msg;
    fin_packet_t fin;
  } body;
} packet_t;

packet_t *packet_parse(uint8_t *data, size_t length);
packet_t *packet_create_syn(uint16_t session_id, uint16_t seq, uint16_t options);
packet_t *packet_create_msg(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length);
packet_t *packet_create_fin(uint16_t session_id);

void packet_print(packet_t *packet);

/* Needs to be freed with safe_free() */
uint8_t *packet_to_bytes(packet_t *packet, size_t *length);

void packet_destroy(packet_t *packet);

#endif
