/**
 * encryptor.c
 * Created by Ron Bowes
 * October, 2015
 *
 * See LICENSE.md
 */

#ifndef NO_ENCRYPTION

#include <stdio.h>
#include <string.h>

#include "libs/buffer.h"
#include "libs/crypto/sha3.h"
#include "libs/crypto/salsa20.h"
#include "libs/crypto/micro-ecc/uECC.h"
#include "libs/memory.h"
#include "libs/types.h"

#include "encryptor.h"
#include "encryptor_sas_dict.h"

#define SAS_AUTHSTRING ("authstring")

#define HEADER_LENGTH 5
#define SIGNATURE_LENGTH 6

static void make_key(encryptor_t *encryptor, char *key_name, uint8_t *result)
{
  sha3_ctx ctx;

  sha3_256_init(&ctx);
  sha3_update(&ctx, encryptor->shared_secret, 32);
  sha3_update(&ctx, (uint8_t*)key_name, strlen(key_name));
  sha3_final(&ctx, result);
}

static void make_authenticator(encryptor_t *encryptor, char *authstring, uint8_t *buffer)
{
  sha3_ctx ctx;

  sha3_256_init(&ctx);
  sha3_update(&ctx, (uint8_t*)authstring, strlen(authstring));
  sha3_update(&ctx, encryptor->shared_secret,    32);
  sha3_update(&ctx, encryptor->my_public_key,    64);
  sha3_update(&ctx, encryptor->their_public_key, 64);
  sha3_update(&ctx, (uint8_t*)encryptor->preshared_secret, strlen(encryptor->preshared_secret));
  sha3_final(&ctx, buffer);
}

encryptor_t *encryptor_create(char *preshared_secret)
{
  encryptor_t *encryptor = safe_malloc(sizeof(encryptor_t));

  if(!uECC_make_key(encryptor->my_public_key, encryptor->my_private_key, uECC_secp256r1()))
    return NULL;

  encryptor->preshared_secret = preshared_secret;

  return encryptor;
}

NBBOOL encryptor_set_their_public_key(encryptor_t *encryptor, uint8_t *their_public_key)
{
  if(!uECC_shared_secret(their_public_key, encryptor->my_private_key, encryptor->shared_secret, uECC_secp256r1()))
    return FALSE;

  /* Store their key (we need it to generate the SAS). */
  memcpy(encryptor->their_public_key, their_public_key, 64);

  make_key(encryptor, "client_write_key", encryptor->my_write_key);
  make_key(encryptor, "client_mac_key",   encryptor->my_mac_key);
  make_key(encryptor, "server_write_key", encryptor->their_write_key);
  make_key(encryptor, "server_mac_key",   encryptor->their_mac_key);

  if(encryptor->preshared_secret)
  {
    make_authenticator(encryptor, "client", encryptor->my_authenticator);
    make_authenticator(encryptor, "server", encryptor->their_authenticator);
  }

  return TRUE;
}

uint16_t encryptor_get_nonce(encryptor_t *encryptor)
{
  return encryptor->nonce++;
}

NBBOOL encryptor_should_we_renegotiate(encryptor_t *encryptor)
{
  return encryptor->nonce > 0xFFF0;
}

void encryptor_print(encryptor_t *encryptor)
{
  print_hex("my_private_key",      encryptor->my_private_key,   32);
  print_hex("my_public_key",       encryptor->my_public_key,    64);
  print_hex("their_public_key",    encryptor->their_public_key, 64);
  printf("\n");
  print_hex("shared_secret",       encryptor->shared_secret, 32);
  if(encryptor->preshared_secret)
  {
    print_hex("my_authenticator",    encryptor->my_authenticator, 32);
    print_hex("their_authenticator", encryptor->their_authenticator, 32);
  }
  printf("\n");
  print_hex("my_write_key",        encryptor->my_write_key,     32);
  print_hex("my_mac_key",          encryptor->my_mac_key,       32);
  print_hex("their_write_key",     encryptor->their_write_key,  32);
  print_hex("their_mac_key",       encryptor->their_mac_key,    32);
}

void encryptor_print_sas(encryptor_t *encryptor)
{
  sha3_ctx ctx;
  uint8_t  hash[32];
  size_t   i;

  sha3_256_init(&ctx);
  sha3_update(&ctx, (uint8_t*)SAS_AUTHSTRING, strlen(SAS_AUTHSTRING));
  sha3_update(&ctx, encryptor->shared_secret,    32);
  sha3_update(&ctx, encryptor->my_public_key,    64);
  sha3_update(&ctx, encryptor->their_public_key, 64);
  sha3_final(&ctx, hash);

  for(i = 0; i < 6; i++)
    printf("%s ", sas_dict[hash[i]]);
  printf("\n");
}

NBBOOL encryptor_check_signature(encryptor_t *encryptor, buffer_t *buffer)
{
  sha3_ctx  ctx;

  uint8_t   header[HEADER_LENGTH];
  uint8_t   their_signature[SIGNATURE_LENGTH];
  uint8_t   good_signature[32];
  uint8_t  *body;
  size_t    body_length;

  uint8_t  *signed_data         = NULL;
  size_t    signed_length       = -1;

  /* Start from the beginning. */
  buffer_reset(buffer);

  /* Read the 5-byte header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Read their 6-byte (48-bit) signature off the front of the packet. */
  buffer_read_next_bytes(buffer, their_signature, SIGNATURE_LENGTH);

  /* Read the body. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Re-build the buffer without the signature. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header, HEADER_LENGTH);
  buffer_add_bytes(buffer, body, body_length);

  /* Get it out as a string. */
  signed_data = buffer_create_string(buffer, &signed_length);

  /* Calculate H(mac_key || data) */
  sha3_256_init(&ctx);
  sha3_update(&ctx, encryptor->their_mac_key, 32);
  sha3_update(&ctx, signed_data, signed_length);
  sha3_final(&ctx, good_signature);

  /* Free the data we allocated. */
  safe_free(signed_data);
  safe_free(body);

  /* Validate the signature */
  return (NBBOOL)!memcmp(their_signature, good_signature, SIGNATURE_LENGTH);
}

void encryptor_decrypt_buffer(encryptor_t *encryptor, buffer_t *buffer, uint16_t *nonce)
{
  uint8_t   header[HEADER_LENGTH] = {0};
  uint8_t   nonce_str[8]          = {0};
  uint8_t  *body                  = NULL;
  size_t    body_length           = -1;

  /* Start from the beginning. */
  buffer_reset(buffer);

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
  s20_crypt(encryptor->their_write_key, S20_KEYLEN_256, nonce_str, 0, body, body_length);

  /* Re-build the packet without the nonce. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header, 5);
  buffer_add_bytes(buffer, body, body_length);

  /* Free up memory. */
  safe_free(body);
}

void encryptor_sign_buffer(encryptor_t *encryptor, buffer_t *buffer)
{
  sha3_ctx  ctx;
  uint8_t   signature[32];

  uint8_t   header[HEADER_LENGTH]   = {0};
  uint8_t  *body                    = NULL;
  size_t    body_length             = -1;

  /* Start from the beginning. */
  buffer_reset(buffer);

  /* Read the header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Read the body. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Generate the signature.  */
  sha3_256_init(&ctx);
  sha3_update(&ctx, encryptor->my_mac_key, 32);
  sha3_update(&ctx, header,                HEADER_LENGTH);
  sha3_update(&ctx, body,                  body_length);
  sha3_final(&ctx, signature);

  /* Add the truncated signature to the packet. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header,    HEADER_LENGTH);
  buffer_add_bytes(buffer, signature, SIGNATURE_LENGTH);
  buffer_add_bytes(buffer, body,      body_length);

  /* Free memory. */
  safe_free(body);
}

void encryptor_encrypt_buffer(encryptor_t *encryptor, buffer_t *buffer)
{
  uint8_t   header[HEADER_LENGTH] = {0};
  uint16_t  nonce                 = encryptor_get_nonce(encryptor);
  uint8_t   nonce_str[8]          = {0};
  uint8_t  *body                  = NULL;
  size_t    body_length           = -1;

  /* Start from the beginning. */
  buffer_reset(buffer);

  /* Read the header. */
  buffer_read_next_bytes(buffer, header, HEADER_LENGTH);

  /* Set up the nonce. */
  memset(nonce_str, '\0', 8);
  nonce_str[6] = (nonce >> 8) & 0x00FF;
  nonce_str[7] = (nonce >> 0) & 0x00FF;

  /* Read the body, which is what we will encrypt. */
  body = buffer_read_remaining_bytes(buffer, &body_length, -1, FALSE);

  /* Encrypt the body! */
  s20_crypt(encryptor->my_write_key, S20_KEYLEN_256, nonce_str, 0, body, body_length);

  /* Re-build the packet with the nonce. */
  buffer_clear(buffer);
  buffer_add_bytes(buffer, header, 5);
  buffer_add_int16(buffer, nonce);
  buffer_add_bytes(buffer, body, body_length);

  /* Free memory. */
  safe_free(body);
}

void encryptor_destroy(encryptor_t *encryptor)
{
  /* safe_free() does this anyway, but let's do it explicitly since it's crypto. */
  memset(encryptor, 0, sizeof(encryptor_t));
  safe_free(encryptor);
}

#endif
