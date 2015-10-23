/* encrypted_packet.c
 * By Ron Bowes
 * Created October, 2015
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <string.h>

#include "libs/buffer.h"
#include "libs/crypto/salsa20.h"
#include "libs/crypto/sha3.h"
#include "libs/memory.h"
#include "libs/types.h"

#include "encrypted_packet.h"

#define HEADER_LENGTH 5
#define SIGNATURE_LENGTH 6

NBBOOL check_signature(buffer_t *buffer, uint8_t *mac_key)
{
  sha3_ctx  ctx;

  uint8_t   header[HEADER_LENGTH];
  uint8_t   their_signature[SIGNATURE_LENGTH];
  uint8_t   good_signature[32];
  uint8_t  *body;
  size_t    body_length;

  uint8_t  *signed_data         = NULL;
  size_t    signed_length       = -1;

  /* Read the 5-byte header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Read their 6-byte (48-bit) signature off the front of the packet. */
  buffer_read_next_bytes(buffer, their_signature, SIGNATURE_LENGTH);

  /* Read the body. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Re-build the buffer without the signature. */
  buffer_clear(buffer);
  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_bytes(buffer, header, HEADER_LENGTH);
  buffer_add_bytes(buffer, body, body_length);

  /* Get it out as a string. */
  signed_data = buffer_create_string(buffer, &signed_length);

  /* Calculate H(mac_key || data) */
  sha3_256_init(&ctx);
  sha3_update(&ctx, mac_key, 32);
  sha3_update(&ctx, signed_data, signed_length);
  sha3_final(&ctx, good_signature);

  /* Free the data we allocated. */
  safe_free(signed_data);
  safe_free(body);

  /* Validate the signature */
  return (NBBOOL)!memcmp(their_signature, good_signature, SIGNATURE_LENGTH);
}

void decrypt_buffer(buffer_t *buffer, uint8_t *write_key, uint16_t *nonce)
{
  uint8_t   header[HEADER_LENGTH] = {0};
  uint8_t   nonce_str[8]          = {0};
  uint8_t  *body                  = NULL;
  size_t    body_length           = -1;

  /* Read the header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Read the nonce, padded with zeroes. */
  memset(nonce_str, '\0', 8);
  buffer_read_next_bytes(buffer, nonce_str+6, 2);

  /* Read the nonce into a return variable. */
  if(nonce)
    *nonce = buffer_read_int16_at(buffer, HEADER_LENGTH);

  /* Read the body, which is what's encrypted. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Decrypt the body! */
  s20_crypt(write_key, S20_KEYLEN_256, nonce_str, 0, body, body_length);

  /* Re-build the packet without the nonce. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header, 5);
  buffer_add_bytes(buffer, body, body_length);
}

void sign_buffer(buffer_t *buffer, uint8_t *mac_key)
{
  sha3_ctx  ctx;
  uint8_t   signature[32];

  uint8_t   header[HEADER_LENGTH]   = {0};
  uint8_t  *body                    = NULL;
  size_t    body_length             = -1;

  /* Read the header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Read the body. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Generate the signature.  */
  sha3_256_init(&ctx);
  sha3_update(&ctx, mac_key,   32);
  sha3_update(&ctx, header,    HEADER_LENGTH);
  sha3_update(&ctx, body,      body_length);
  sha3_final(&ctx, signature);

  /* Add the truncated signature to the packet. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header,    HEADER_LENGTH);
  buffer_add_bytes(buffer, signature, 32);
  buffer_add_bytes(buffer, body,      body_length);
}

void encrypt_buffer(buffer_t *buffer, uint8_t *write_key, uint16_t nonce)
{
  uint8_t   header[HEADER_LENGTH] = {0};
  uint8_t   nonce_str[8]          = {0};
  uint8_t  *body                  = NULL;
  size_t    body_length           = -1;

  /* Read the header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Set up the nonce. */
  memset(nonce_str, '\0', 8);
  nonce_str[6] = (nonce >> 8) & 0x00FF;
  nonce_str[7] = (nonce >> 0) & 0x00FF;

  /* Read the body, which is what we will encrypt. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Encrypt the body! */
  s20_crypt(write_key, S20_KEYLEN_256, nonce_str, 0, body, body_length);

  /* Re-build the packet with the nonce. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header, 5);
  buffer_add_int16(buffer, nonce);
  buffer_add_bytes(buffer, body, body_length);
}
