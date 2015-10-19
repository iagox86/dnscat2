/* encrypted_packet.c
 * By Ron Bowes
 * Created October, 2015
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "libs/buffer.h"
#include "libs/crypto/salsa20.h"
#include "libs/crypto/sha3.h"
#include "libs/memory.h"
#include "libs/types.h"

#include "encrypted_packet.h"

/* Create an encrypted packet. */
encrypted_packet_t *encrypted_packet_create(uint16_t nonce, packet_t *packet)
{
  encrypted_packet_t *encrypted_packet = (encrypted_packet_t*) safe_malloc(sizeof(encrypted_packet_t));

  encrypted_packet->nonce = nonce;
  encrypted_packet->packet = packet;

  return encrypted_packet;
}

/* Parse a packet from a byte stream. */
encrypted_packet_t *encrypted_packet_parse(uint8_t *data, size_t length, options_t options, uint8_t *their_mac_key, uint8_t *their_write_key)
{
  encrypted_packet_t  *encrypted_packet    = NULL;
  buffer_t            *buffer              = NULL;
  uint8_t             *signed_data         = NULL;
  size_t               signed_length       = -1;
  uint8_t             *encrypted_data      = NULL;
  size_t               encrypted_length    = -1;
  uint8_t             *decrypted_data      = NULL;
  buffer_t            *nonce_buffer        = NULL;
  uint8_t             *nonce_str           = NULL;
  size_t               nonce_length        = -1;
  uint8_t              their_signature[6];
  uint8_t              good_signature[32];
  sha3_ctx             ctx;

  /* Put the data into a buffer. */
  buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  /* Generate our own signature (without consuming bytes). */
  signed_data = buffer_read_remaining_bytes(buffer, &signed_length, -1, FALSE);
  sha3_256_init(&ctx);
  sha3_update(&ctx, their_mac_key, 32);
  sha3_update(&ctx, signed_data, signed_length);
  sha3_final(&ctx, good_signature);
  safe_free(signed_data);

  /* Validate the signature */
  if(memcmp(their_signature, good_signature, 6))
  {
    printf("Signature didn't match!");
    exit(0); /* TODO: I should return NULL and handle this elsewhere */
  }

  /* Now we can read the nonce since we know it's validated. */
  encrypted_packet        = (encrypted_packet_t*) safe_malloc(sizeof(encrypted_packet_t));
  encrypted_packet->nonce = buffer_read_next_int16(buffer);

  /* We need to make the nonce a 64-bit string. */
  nonce_buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int32(nonce_buffer, 0);
  buffer_add_int16(nonce_buffer, 0);
  buffer_add_int16(nonce_buffer, encrypted_packet->nonce);
  nonce_str = buffer_create_string_and_destroy(nonce_buffer, &nonce_length);

  /* Read the encrypted data and decrypt it. */
  encrypted_data = (uint8_t*) buffer_read_remaining_bytes(buffer, &encrypted_length, -1, TRUE);
  decrypted_data = (uint8_t*) safe_malloc(encrypted_length);
  s20_crypt(their_write_key, S20_KEYLEN_256, nonce_str, 0, decrypted_data, encrypted_length);
  safe_free(encrypted_data);

  /* Create a packet and save it. */
  encrypted_packet->packet = packet_parse(decrypted_data, encrypted_length, options);

  return encrypted_packet;
}

/* Get a handle to the normal packet. */
packet_t *encrypted_packet_get_packet(encrypted_packet_t *encrypted_packet)
{
  return NULL;
}

/* Free the packet data structures. */
void encrypted_packet_destroy(encrypted_packet_t *encrypted_packet)
{
}

/* Get a user-readable display of the packet (don't forget to safe_free() the memory!) */
char *encrypted_packet_to_s(encrypted_packet_t *packet, options_t options)
{
  return NULL;
}

/* Print the packet (debugging, mostly) */
void encrypted_packet_print(encrypted_packet_t *packet, options_t options)
{
}

/* Needs to be freed with safe_free() */
uint8_t *encrypted_packet_to_bytes(encrypted_packet_t *packet, size_t *length, options_t options)
{
  return NULL;
}
