/* packet.h
 * By Ron Bowes
 * Created March, 2013
 *
 * See LICENSE.md
 *
 * A class for creating and parsing dnscat packets. This is part of the
 * "dnscat protocol", it's assumed that the packets are already marshalled
 * and unmarshalled by a tunnelling protocol.
 */

#ifndef __PACKET_H__
#define __PACKET_H__

#include <stdlib.h>

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#define MAX_PACKET_SIZE 1024

typedef enum
{
  PACKET_TYPE_SYN = 0x00,
  PACKET_TYPE_MSG = 0x01,
  PACKET_TYPE_FIN = 0x02,
  PACKET_TYPE_PING = 0xFF,
} packet_type_t;

typedef struct
{
  uint16_t seq;
  uint16_t options;
  char    *name;
  char    *filename;
} syn_packet_t;

typedef enum
{
  OPT_NAME             = 0x0001,
  /* OPT_TUNNEL = 2,   // Deprecated */
  /* OPT_DATAGRAM = 4, // Deprecated */
  OPT_DOWNLOAD         = 0x0008,
  OPT_CHUNKED_DOWNLOAD = 0x0010,
  OPT_COMMAND          = 0x0020,
} options_t;

typedef struct
{
  uint16_t seq;
  uint16_t ack;
} normal_msg_t;

typedef struct
{
  uint32_t chunk;
} chunked_msg_t;

typedef struct
{
  union {
    struct { uint16_t seq; uint16_t ack; } normal;
    struct { uint32_t chunk; }             chunked;
  } options;
  uint8_t *data;
  size_t   data_length;
} msg_packet_t;

typedef struct
{
  char *reason;
} fin_packet_t;

typedef struct
{
  char *data;
} ping_packet_t;

typedef struct
{
  uint16_t packet_id;
  packet_type_t packet_type;
  uint16_t session_id;

  union
  {
    syn_packet_t  syn;
    msg_packet_t  msg;
    fin_packet_t  fin;
    ping_packet_t ping;
  } body;
} packet_t;

/* Parse a packet from a byte stream. */
packet_t *packet_parse(uint8_t *data, size_t length, options_t options);

/* Just get the session_id. */
uint16_t packet_peek_session_id(uint8_t *data, size_t length);

/* Create a packet with the given characteristics. */
packet_t *packet_create_syn(uint16_t session_id, uint16_t seq, options_t options);
packet_t *packet_create_msg_normal(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length);
packet_t *packet_create_msg_chunked(uint16_t session_id, uint32_t chunk);
packet_t *packet_create_fin(uint16_t session_id, char *reason);
packet_t *packet_create_ping(uint16_t session_id, char *data);

/* Set the OPT_NAME field and add a name value. */
void packet_syn_set_name(packet_t *packet, char *name);

/* Set the OPT_DOWNLOAD field and add a filename value */
void packet_syn_set_download(packet_t *packet, char *filename);

/* Set the OPT_CHUNKED_DOWNLOAD field */
void packet_syn_set_chunked_download(packet_t *packet);

/* Set the OPT_COMMAND flag */
void packet_syn_set_is_command(packet_t *packet);

/* Get minimum packet sizes so we can avoid magic numbers. */
size_t packet_get_syn_size();
size_t packet_get_msg_size(options_t options);
size_t packet_get_fin_size(options_t options);
size_t packet_get_ping_size();

/* Free the packet data structures. */
void packet_destroy(packet_t *packet);

/* Get a user-readable display of the packet (don't forget to safe_free() the memory!) */
char *packet_to_s(packet_t *packet, options_t options);

/* Print the packet (debugging, mostly) */
void packet_print(packet_t *packet, options_t options);

/* Needs to be freed with safe_free() */
uint8_t *packet_to_bytes(packet_t *packet, size_t *length, options_t options);

#endif
