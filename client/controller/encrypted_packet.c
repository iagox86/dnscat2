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
encrypted_packet_t *encrypted_packet_create(uint16_t nonce, packet_t *packet, options_t options)
{
  encrypted_packet_t *encrypted_packet = (encrypted_packet_t*) safe_malloc(sizeof(encrypted_packet_t));

  encrypted_packet->nonce = nonce;
  encrypted_packet->packet = packet_clone(packet, options);

  return encrypted_packet;
}

/* When this function returns, the buffer's offset will be past the signature
 * but before the nonce. */
static NBBOOL check_signature(buffer_t *buffer, uint8_t *mac_key)
{
  sha3_ctx  ctx;

  uint8_t   header[5];
  uint8_t   their_signature[6];
  uint8_t   nonce[8];
  uint8_t   good_signature[32];
  uint8_t  *body;
  size_t    body_length;

  uint8_t  *signed_data         = NULL;
  size_t    signed_length       = -1;

  /* Read the 5-byte header. */
  buffer_read_bytes(buffer, header, 5);

  /* Read their 6-byte (48-bit) signature off the front of the packet. */
  buffer_read_next_bytes(buffer, their_signature, 6);

  /* Read the nonce, padded with NUL bytes. */
  memset(nonce, '\0', 8);
  buffer_read_next_bytes(buffer, nonce+6, 2);

  /* Read the body. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Re-build the buffer without the signature. */
  buffer_clear(buffer);
  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_bytes(buffer, header, 5);
  buffer_add_bytes(buffer, nonce, 8);
  buffer_add_bytes(buffer, body, body_length);

  /* Get it out as a string. */
  signed_data = buffer_read_remaining_bytes(buffer, &signed_length, -1, FALSE);

  /* Calculate H(mac_key || data) */
  sha3_256_init(&ctx);
  sha3_update(&ctx, mac_key, 32);
  sha3_update(&ctx, signed_data, signed_length);
  sha3_final(&ctx, good_signature);

  /* Free the data we allocated. */
  safe_free(signed_data);
  safe_free(body);

  /* Validate the signature */
  return (NBBOOL)!memcmp(their_signature, good_signature, 6);
}

static void sign_buffer(buffer_t *buffer, uint8_t *mac_key)
{
  sha3_ctx  ctx;
  uint8_t   header[5];
  uint8_t  *body;
  uint8_t  *signed_data = NULL;
  size_t    signed_length;
  uint8_t   signature[32];

  /* Read in all the data so we can generate a signature. */
  signed_data = buffer_read_remaining_bytes(buffer, &signed_length, -1, FALSE);

  /* Generate the signature. */
  sha3_256_init(&ctx);
  sha3_update(&ctx, mac_key, 32);
  sha3_update(&ctx, signed_data, signed_length);
  sha3_final(&ctx, signature);

  /* Add the truncated signature to the packet. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, signed_data, 5);
  buffer_add_bytes(buffer, signature, 6);
  buffer_add_bytes(buffer, signed_data+5, signed_length-5);
}

void decrypt_buffer(buffer_t *buffer, uint8_t *write_key)
{
  size_t    length;
  uint8_t  *data         = NULL;
  uint16_t  nonce;
  uint8_t   nonce_str[8] = { 0 };

  /* Read and consume the remainder of the message. */
  nonce = buffer_read_next_int16(buffer);
  data = buffer_read_remaining_bytes(buffer, &length, -1, TRUE);

  /* Set up the 16-bit nonce into nonce_str */
  memset(nonce_str, 0, 8);
  nonce_str[6] = (nonce >> 8) & 0x00FF;
  nonce_str[7] = (nonce >> 0) & 0x00FF;

  /* Decrypt it! */
  s20_crypt(write_key, S20_KEYLEN_256, nonce_str, 0, data, length);

  /* Put it back in the buffer */
  buffer_clear(buffer);
  buffer_add_int16(buffer, nonce);
  buffer_add_bytes(buffer, data, length);
}

void encrypt_buffer(buffer_t *buffer, uint8_t *write_key)
{
  decrypt_buffer(buffer, write_key);
}

/* Parse a packet from a byte stream. */
encrypted_packet_t *encrypted_packet_parse(uint8_t *data, size_t length, options_t options, uint8_t *their_mac_key, uint8_t *their_write_key)
{
  encrypted_packet_t  *encrypted_packet    = NULL;
  buffer_t            *buffer              = NULL;
  uint8_t             *decrypted_data      = NULL;
  size_t               decrypted_length    = -1;

  /* Put the data into a buffer. */
  buffer = buffer_create_with_data(BO_BIG_ENDIAN, data, length);

  /* Check the signature. */
  if(!check_signature(buffer, their_mac_key))
  {
    printf("The peer signed a message wrong!\n");
    exit(0); /* TODO: I should return FALSE and handle this elsewhere */
  }

  /* Decrypt the data. */
  decrypt_buffer(buffer, their_write_key);

  /* Create a packet and save it. */
  encrypted_packet         = (encrypted_packet_t*) safe_malloc(sizeof(encrypted_packet_t));
  encrypted_packet->nonce  = buffer_read_next_int16(buffer);

  /* Read the decrypted packet. */
  decrypted_data = buffer_read_remaining_bytes(buffer, &decrypted_length, -1, TRUE);
  encrypted_packet->packet = packet_parse(decrypted_data, decrypted_length, options);

  /* Free memory */
  safe_free(decrypted_data);

  return encrypted_packet;
}

/* Get a handle to the normal packet. */
packet_t *encrypted_packet_get_packet(encrypted_packet_t *encrypted_packet)
{
  return encrypted_packet->packet;
}

/* Free the packet data structures. */
void encrypted_packet_destroy(encrypted_packet_t *encrypted_packet)
{
  packet_destroy(encrypted_packet->packet);
}

/* Get a user-readable display of the packet (don't forget to safe_free() the memory!) */
#if 0
char *encrypted_packet_to_s(encrypted_packet_t *packet, options_t options)
{
  return NULL;
}
#endif

/* Print the packet (debugging, mostly) */
void encrypted_packet_print(encrypted_packet_t *encrypted_packet, options_t options)
{
  printf("[ENCRYPTED] nonce=0x%04x ", encrypted_packet->nonce);
  packet_print(encrypted_packet->packet, options);
}

/* Needs to be freed with safe_free() */
uint8_t *encrypted_packet_to_bytes(encrypted_packet_t *encrypted_packet, uint8_t *write_key, uint8_t *mac_key, size_t *length, options_t options)
{
  /* Stick the packet into a buffer. */
  uint8_t  *packet_bytes;
  size_t    packet_length;
  buffer_t *buffer;

  /* Serialize the packet. */
  packet_bytes = packet_to_bytes(encrypted_packet->packet, &packet_length, options);

  /* Add the nonce and the packet to the buffer. */
  buffer = buffer_create(BO_BIG_ENDIAN);
  buffer_add_int16(buffer, encrypted_packet->nonce);
  buffer_add_bytes(buffer, packet_bytes, packet_length);

  /* Free memory. */
  safe_free(packet_bytes);

  /* Encrypt it. */
  encrypt_buffer(buffer, write_key);

  /* Sign it. */
  sign_buffer(buffer, mac_key);

  return buffer_create_string_and_destroy(buffer, length);
}







