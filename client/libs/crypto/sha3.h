/* sha3.h */
#ifndef __SHA3_H__
#define __SHA3_H__
#include "ustd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define sha3_224_hash_size  28
#define sha3_256_hash_size  32
#define sha3_384_hash_size  48
#define sha3_512_hash_size  64
#define sha3_max_permutation_size 25
#define sha3_max_rate_in_qwords 24

/**
 * SHA3 Algorithm context.
 */
typedef struct sha3_ctx
{
	/* 1600 bits algorithm hashing state */
	uint64_t hash[sha3_max_permutation_size];
	/* 1536-bit buffer for leftovers */
	uint64_t message[sha3_max_rate_in_qwords];
	/* count of bytes in the message[] buffer */
	unsigned rest;
	/* size of a message block processed at once */
	unsigned block_size;
} sha3_ctx;

/* methods for calculating the hash function */

void sha3_224_init(sha3_ctx *ctx);
void sha3_256_init(sha3_ctx *ctx);
void sha3_384_init(sha3_ctx *ctx);
void sha3_512_init(sha3_ctx *ctx);
void sha3_update(sha3_ctx *ctx, const unsigned char* msg, size_t size);
void sha3_final(sha3_ctx *ctx, unsigned char* result);

#ifdef USE_KECCAK
#define keccak_224_init sha3_224_init
#define keccak_256_init sha3_256_init
#define keccak_384_init sha3_384_init
#define keccak_512_init sha3_512_init
#define keccak_update sha3_update
void keccak_final(sha3_ctx *ctx, unsigned char* result);
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* __SHA3_H__ */
