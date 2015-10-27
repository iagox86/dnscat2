/**
 * encryptor.c
 * Created by Ron Bowes
 * October, 2015
 *
 * See LICENSE.md
 */

#include <stdio.h>
#include <string.h>

#include "libs/buffer.h"
#include "libs/crypto/sha3.h"
#include "libs/memory.h"
#include "libs/types.h"

#include "encryptor.h"
#include "encryptor_sas_dict.h"

#define SAS_AUTHSTRING ("authstring")

void encryptor_print_sas(uint8_t *shared_secret, uint8_t *my_pubkey, uint8_t *their_pubkey)
{
  sha3_ctx ctx;
  uint8_t  hash[32];
  size_t   i;

  sha3_256_init(&ctx);
  sha3_update(&ctx, (uint8_t*)SAS_AUTHSTRING, strlen(SAS_AUTHSTRING));
  sha3_update(&ctx, shared_secret,            32);
  sha3_update(&ctx, my_pubkey,                64);
  sha3_update(&ctx, their_pubkey,             64);
  sha3_final(&ctx, hash);

  for(i = 0; i < 6; i++)
    printf("%s ", sas_dict[hash[i]]);
  printf("\n");
}
