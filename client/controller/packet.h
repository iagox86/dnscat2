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
  PACKET_TYPE_SYN    = 0x00,
  PACKET_TYPE_MSG    = 0x01,
  PACKET_TYPE_FIN    = 0x02,
#ifndef NO_ENCRYPTION
  PACKET_TYPE_ENC    = 0x03,
#endif
  PACKET_TYPE_COUNT_NOT_PING,

  PACKET_TYPE_PING   = 0xFF,

} packet_type_t;

char *packet_type_to_string(packet_type_t type);

typedef enum
{
  PACKET_ENC_SUBTYPE_INIT = 0x00,
  PACKET_ENC_SUBTYPE_AUTH = 0x01,
} packet_enc_subtype_t;

typedef struct
{
  uint16_t seq;
  uint16_t options;
  char    *name;
} syn_packet_t;

typedef enum
{
  OPT_NAME             = 0x0001,
  /* OPT_TUNNEL = 2,   // Deprecated */
  /* OPT_DATAGRAM = 4, // Deprecated */
  /* OPT_DOWNLOAD = 8, // Deprecated */
  /* OPT_CHUNKED_DOWNLOAD = 16, // Deprecated */
  OPT_COMMAND          = 0x0020,
} options_t;

typedef struct
{
  uint16_t seq;
  uint16_t ack;
} normal_msg_t;

typedef struct
{
  uint16_t seq;
  uint16_t ack;
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

#ifndef NO_ENCRYPTION
typedef struct
{
  packet_enc_subtype_t subtype;
  uint16_t             flags;

  uint8_t public_key[64];

  uint8_t authenticator[32];
} enc_packet_t;
#endif

typedef struct
{
  uint16_t packet_id;
  packet_type_t packet_type;
  uint16_t session_id;

  union
  {
    syn_packet_t    syn;
    msg_packet_t    msg;
    fin_packet_t    fin;
    ping_packet_t   ping;
#ifndef NO_ENCRYPTION
    enc_packet_t    enc;
#endif
  } body;
} packet_t;

/* Parse a packet from a byte stream. */
packet_t *packet_parse(uint8_t *data, size_t length, options_t options);

/* Just get the session_id. */
uint16_t packet_peek_session_id(uint8_t *data, size_t length);

/* Create a packet with the given characteristics. */
packet_t *packet_create_syn(uint16_t session_id, uint16_t seq, options_t options);
packet_t *packet_create_msg(uint16_t session_id, uint16_t seq, uint16_t ack, uint8_t *data, size_t data_length);
packet_t *packet_create_fin(uint16_t session_id, char *reason);
packet_t *packet_create_ping(uint16_t session_id, char *data);

#ifndef NO_ENCRYPTION
packet_t *packet_create_enc(uint16_t session_id, uint16_t flags);
#endif

/* Set the OPT_NAME field and add a name value. */
void packet_syn_set_name(packet_t *packet, char *name);

/* Set the OPT_COMMAND flag */
void packet_syn_set_is_command(packet_t *packet);

#ifndef NO_ENCRYPTION
/* Set up an encrypted session. */
void packet_enc_set_init(packet_t *packet, uint8_t *public_key);

/* Authenticate with a PSK. */
void packet_enc_set_auth(packet_t *packet, uint8_t *authenticator);
#endif

/* Get minimum packet sizes so we can avoid magic numbers. */
size_t packet_get_msg_size(options_t options);
size_t packet_get_ping_size();

/* Free the packet data structures. */
void packet_destroy(packet_t *packet);

/* Print the packet (debugging, mostly) */
void packet_print(packet_t *packet, options_t options);

/* Needs to be freed with safe_free() */
uint8_t *packet_to_bytes(packet_t *packet, size_t *length, options_t options);

/* Create a new copy of the packet. */
packet_t *packet_clone(packet_t *packet, options_t options);

#endif
