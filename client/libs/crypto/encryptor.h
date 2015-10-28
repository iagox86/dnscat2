/**
 * encryptor.h
 * Created by Ron Bowes
 * October, 2015
 *
 * See LICENSE.md
 */
#ifndef __ENCRYPTOR_H__
#define __ENCRYPTOR_H__

typedef struct
{
  char *preshared_secret;

  uint8_t my_private_key[32];
  uint8_t my_public_key[64];
  uint8_t their_public_key[64];

  uint8_t shared_secret[32];
  uint8_t my_authenticator[32];
  uint8_t their_authenticator[32];

  uint8_t my_write_key[32];
  uint8_t my_mac_key[32];
  uint8_t their_write_key[32];
  uint8_t their_mac_key[32];

  uint16_t nonce;
} encryptor_t;

/* Create a new encryptor and generate a new private key. */
encryptor_t *encryptor_create(char *preshared_secret);

/* Set their pubkey, and also calculate all the various derived values. */
NBBOOL encryptor_set_their_public_key(encryptor_t *encryptor, uint8_t *their_public_key);

/* Get the next nonce. */
uint16_t encryptor_get_nonce(encryptor_t *encryptor);

/* Check if we should re-negotiate. */
NBBOOL encryptor_should_we_renegotiate(encryptor_t *encryptor);

/* Print all the internal encryptor variables. */
void encryptor_print(encryptor_t *encryptor);

/* Print the short authentication string. */
void encryptor_print_sas(encryptor_t *encryptor);

/* Validates that the packet, stored in buffer, has a valid signature.
 * It also removes the signature from the buffer. */
NBBOOL encryptor_check_signature(encryptor_t *encryptor, buffer_t *buffer);

/* Decrypt the packet, stored in buffer. Also removes the nonce and
 * returns it in the nonce parameter, if it's not NULL. */
void encryptor_decrypt_buffer(encryptor_t *encryptor, buffer_t *buffer, uint16_t *nonce);

/* Adds a signature to the packet stored in buffer. */
void encryptor_sign_buffer(encryptor_t *encryptor, buffer_t *buffer);

/* Encrypts the packet stored in buffer, and adds the nonce to it. */
void encryptor_encrypt_buffer(encryptor_t *encryptor, buffer_t *buffer);

/* Destroy the encryptor and free/wipe any memory used. */
void encryptor_destroy(encryptor_t *encryptor);


#endif
