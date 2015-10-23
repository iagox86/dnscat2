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

#include "libs/buffer.h"
#include "libs/types.h"

/* Validates that the packet, stored in buffer, has a valid signature.
 * It also removes the signature from the buffer. */
NBBOOL check_signature(buffer_t *buffer, uint8_t *mac_key);

/* Decrypt the packet, stored in buffer. Also removes the nonce and
 * returns it in the nonce parameter, if it's not NULL. */
void decrypt_buffer(buffer_t *buffer, uint8_t *write_key, uint16_t *nonce);

/* Adds a signature to the packet stored in buffer. */
void sign_buffer(buffer_t *buffer, uint8_t *mac_key);

/* Encrypts the packet stored in buffer, and adds the nonce to it. */
void encrypt_buffer(buffer_t *buffer, uint8_t *write_key, uint16_t nonce);

#endif
