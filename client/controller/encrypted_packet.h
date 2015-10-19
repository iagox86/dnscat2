/* encrypted_packet.h
 * By Ron Bowes
 * Created October, 2015
 *
 * See LICENSE.md
 *
 * A class for creating and parsing encrypted dnscat2 packets.
 */

#ifndef __ENCRYPTED_PACKET_H__
#define __ENCRYPTED_PACKET_H__

#include <stdlib.h>

#ifdef WIN32
#include "libs/pstdint.h"
#else
#include <stdint.h>
#endif

#include "controller/packet.h"

typedef struct
{
  uint16_t  nonce;
  packet_t *packet;
} encrypted_packet_t;

/* Create an encrypted packet. */
encrypted_packet_t *encrypted_packet_create(uint16_t nonce, packet_t *packet);

/* Parse a packet from a byte stream. */
encrypted_packet_t *encrypted_packet_parse(uint8_t *data, size_t length, options_t options, uint8_t *their_mac_key, uint8_t *their_write_key);

/* Get a handle to the normal packet. */
packet_t *encrypted_packet_get_packet(encrypted_packet_t *encrypted_packet);

/* Free the packet data structures. */
void encrypted_packet_destroy(encrypted_packet_t *encrypted_packet);

/* Get a user-readable display of the packet (don't forget to safe_free() the memory!) */
char *encrypted_packet_to_s(encrypted_packet_t *packet, options_t options);

/* Print the packet (debugging, mostly) */
void encrypted_packet_print(encrypted_packet_t *packet, options_t options);

/* Needs to be freed with safe_free() */
uint8_t *encrypted_packet_to_bytes(encrypted_packet_t *packet, size_t *length, options_t options);

#endif
