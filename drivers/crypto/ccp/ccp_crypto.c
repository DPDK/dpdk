/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/cmac.h> /*sub key apis*/
#include <openssl/evp.h> /*sub key apis*/

#include <rte_hexdump.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_spinlock.h>
#include <rte_string_fns.h>
#include <cryptodev_pmd.h>

#include "ccp_dev.h"
#include "ccp_crypto.h"
#include "ccp_pci.h"
#include "ccp_pmd_private.h"

#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/hmac.h>

extern int iommu_mode;
void *sha_ctx;
/* SHA initial context values */
uint32_t ccp_sha1_init[SHA_COMMON_DIGEST_SIZE / sizeof(uint32_t)] = {
	SHA1_H4, SHA1_H3,
	SHA1_H2, SHA1_H1,
	SHA1_H0, 0x0U,
	0x0U, 0x0U,
};

uint32_t ccp_sha224_init[SHA256_DIGEST_SIZE / sizeof(uint32_t)] = {
	SHA224_H7, SHA224_H6,
	SHA224_H5, SHA224_H4,
	SHA224_H3, SHA224_H2,
	SHA224_H1, SHA224_H0,
};

uint32_t ccp_sha256_init[SHA256_DIGEST_SIZE / sizeof(uint32_t)] = {
	SHA256_H7, SHA256_H6,
	SHA256_H5, SHA256_H4,
	SHA256_H3, SHA256_H2,
	SHA256_H1, SHA256_H0,
};

uint64_t ccp_sha384_init[SHA512_DIGEST_SIZE / sizeof(uint64_t)] = {
	SHA384_H7, SHA384_H6,
	SHA384_H5, SHA384_H4,
	SHA384_H3, SHA384_H2,
	SHA384_H1, SHA384_H0,
};

uint64_t ccp_sha512_init[SHA512_DIGEST_SIZE / sizeof(uint64_t)] = {
	SHA512_H7, SHA512_H6,
	SHA512_H5, SHA512_H4,
	SHA512_H3, SHA512_H2,
	SHA512_H1, SHA512_H0,
};

#if defined(_MSC_VER)
#define SHA3_CONST(x) x
#else
#define SHA3_CONST(x) x##L
#endif

/** 'Words' here refers to uint64_t */
#define SHA3_KECCAK_SPONGE_WORDS \
	(((1600) / 8) / sizeof(uint64_t))
typedef struct sha3_context_ {
	uint64_t saved;
	/**
	 * The portion of the input message that we
	 * didn't consume yet
	 */
	union {
		uint64_t s[SHA3_KECCAK_SPONGE_WORDS];
		/* Keccak's state */
		uint8_t sb[SHA3_KECCAK_SPONGE_WORDS * 8];
		/**total 200 ctx size**/
	};
	unsigned int byteIndex;
	/**
	 * 0..7--the next byte after the set one
	 * (starts from 0; 0--none are buffered)
	 */
	unsigned int wordIndex;
	/**
	 * 0..24--the next word to integrate input
	 * (starts from 0)
	 */
	unsigned int capacityWords;
	/**
	 * the double size of the hash output in
	 * words (e.g. 16 for Keccak 512)
	 */
} sha3_context;

#ifndef SHA3_ROTL64
#define SHA3_ROTL64(x, y) \
	(((x) << (y)) | ((x) >> ((sizeof(uint64_t)*8) - (y))))
#endif

static const uint64_t keccakf_rndc[24] = {
	SHA3_CONST(0x0000000000000001UL), SHA3_CONST(0x0000000000008082UL),
	SHA3_CONST(0x800000000000808aUL), SHA3_CONST(0x8000000080008000UL),
	SHA3_CONST(0x000000000000808bUL), SHA3_CONST(0x0000000080000001UL),
	SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008009UL),
	SHA3_CONST(0x000000000000008aUL), SHA3_CONST(0x0000000000000088UL),
	SHA3_CONST(0x0000000080008009UL), SHA3_CONST(0x000000008000000aUL),
	SHA3_CONST(0x000000008000808bUL), SHA3_CONST(0x800000000000008bUL),
	SHA3_CONST(0x8000000000008089UL), SHA3_CONST(0x8000000000008003UL),
	SHA3_CONST(0x8000000000008002UL), SHA3_CONST(0x8000000000000080UL),
	SHA3_CONST(0x000000000000800aUL), SHA3_CONST(0x800000008000000aUL),
	SHA3_CONST(0x8000000080008081UL), SHA3_CONST(0x8000000000008080UL),
	SHA3_CONST(0x0000000080000001UL), SHA3_CONST(0x8000000080008008UL)
};

static const unsigned int keccakf_rotc[24] = {
	1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14, 27, 41, 56, 8, 25, 43, 62,
	18, 39, 61, 20, 44
};

static const unsigned int keccakf_piln[24] = {
	10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4, 15, 23, 19, 13, 12, 2, 20,
	14, 22, 9, 6, 1
};

static enum ccp_cmd_order
ccp_get_cmd_id(const struct rte_crypto_sym_xform *xform)
{
	enum ccp_cmd_order res = CCP_CMD_NOT_SUPPORTED;

	if (xform == NULL)
		return res;
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
		if (xform->next == NULL)
			return CCP_CMD_AUTH;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_CIPHER)
			return CCP_CMD_HASH_CIPHER;
	}
	if (xform->type == RTE_CRYPTO_SYM_XFORM_CIPHER) {
		if (xform->next == NULL)
			return CCP_CMD_CIPHER;
		else if (xform->next->type == RTE_CRYPTO_SYM_XFORM_AUTH)
			return CCP_CMD_CIPHER_HASH;
	}
	if (xform->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		return CCP_CMD_COMBINED;
	return res;
}

/* partial hash using openssl */
static int partial_hash_sha1(uint8_t *data_in, uint8_t *data_out)
{
	SHA_CTX ctx;

	if (!SHA1_Init(&ctx))
		return -EFAULT;
	SHA1_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx, SHA_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha224(uint8_t *data_in, uint8_t *data_out)
{
	SHA256_CTX ctx;

	if (!SHA224_Init(&ctx))
		return -EFAULT;
	SHA256_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx,
		   SHA256_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha256(uint8_t *data_in, uint8_t *data_out)
{
	SHA256_CTX ctx;

	if (!SHA256_Init(&ctx))
		return -EFAULT;
	SHA256_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx,
		   SHA256_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha384(uint8_t *data_in, uint8_t *data_out)
{
	SHA512_CTX ctx;

	if (!SHA384_Init(&ctx))
		return -EFAULT;
	SHA512_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx,
		   SHA512_DIGEST_LENGTH);
	return 0;
}

static int partial_hash_sha512(uint8_t *data_in, uint8_t *data_out)
{
	SHA512_CTX ctx;

	if (!SHA512_Init(&ctx))
		return -EFAULT;
	SHA512_Transform(&ctx, data_in);
	rte_memcpy(data_out, &ctx,
		   SHA512_DIGEST_LENGTH);
	return 0;
}

static void
keccakf(uint64_t s[25])
{
	int i, j, round;
	uint64_t t, bc[5];
#define KECCAK_ROUNDS 24

	for (round = 0; round < KECCAK_ROUNDS; round++) {

		/* Theta */
		for (i = 0; i < 5; i++)
			bc[i] = s[i] ^ s[i + 5] ^ s[i + 10] ^ s[i + 15] ^
				s[i + 20];

		for (i = 0; i < 5; i++) {
			t = bc[(i + 4) % 5] ^ SHA3_ROTL64(bc[(i + 1) % 5], 1);
			for (j = 0; j < 25; j += 5)
				s[j + i] ^= t;
		}

		/* Rho Pi */
		t = s[1];
		for (i = 0; i < 24; i++) {
			j = keccakf_piln[i];
			bc[0] = s[j];
			s[j] = SHA3_ROTL64(t, keccakf_rotc[i]);
			t = bc[0];
		}

		/* Chi */
		for (j = 0; j < 25; j += 5) {
			for (i = 0; i < 5; i++)
				bc[i] = s[j + i];
			for (i = 0; i < 5; i++)
				s[j + i] ^= (~bc[(i + 1) % 5]) &
					    bc[(i + 2) % 5];
		}

		/* Iota */
		s[0] ^= keccakf_rndc[round];
	}
}

static void
sha3_Init224(void *priv)
{
	sha3_context *ctx = (sha3_context *) priv;

	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 2 * 224 / (8 * sizeof(uint64_t));
}

static void
sha3_Init256(void *priv)
{
	sha3_context *ctx = (sha3_context *) priv;

	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 2 * 256 / (8 * sizeof(uint64_t));
}

static void
sha3_Init384(void *priv)
{
	sha3_context *ctx = (sha3_context *) priv;

	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 2 * 384 / (8 * sizeof(uint64_t));
}

static void
sha3_Init512(void *priv)
{
	sha3_context *ctx = (sha3_context *) priv;

	memset(ctx, 0, sizeof(*ctx));
	ctx->capacityWords = 2 * 512 / (8 * sizeof(uint64_t));
}


/* This is simply the 'update' with the padding block.
 * The padding block is 0x01 || 0x00* || 0x80. First 0x01 and last 0x80
 * bytes are always present, but they can be the same byte.
 */
static void
sha3_Update(void *priv, void const *bufIn, size_t len)
{
	sha3_context *ctx = (sha3_context *) priv;
	unsigned int old_tail = (8 - ctx->byteIndex) & 7;
	size_t words;
	unsigned int tail;
	size_t i;
	const uint8_t *buf = bufIn;

	if (len < old_tail) {
		while (len--)
			ctx->saved |= (uint64_t) (*(buf++)) <<
				      ((ctx->byteIndex++) * 8);
		return;
	}

	if (old_tail) {
		len -= old_tail;
		while (old_tail--)
			ctx->saved |= (uint64_t) (*(buf++)) <<
				      ((ctx->byteIndex++) * 8);

		ctx->s[ctx->wordIndex] ^= ctx->saved;
		ctx->byteIndex = 0;
		ctx->saved = 0;
		if (++ctx->wordIndex ==
		   (SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords)) {
			keccakf(ctx->s);
			ctx->wordIndex = 0;
		}
	}

	words = len / sizeof(uint64_t);
	tail = len - words * sizeof(uint64_t);

	for (i = 0; i < words; i++, buf += sizeof(uint64_t)) {
		const uint64_t t = (uint64_t) (buf[0]) |
			((uint64_t) (buf[1]) << 8 * 1) |
			((uint64_t) (buf[2]) << 8 * 2) |
			((uint64_t) (buf[3]) << 8 * 3) |
			((uint64_t) (buf[4]) << 8 * 4) |
			((uint64_t) (buf[5]) << 8 * 5) |
			((uint64_t) (buf[6]) << 8 * 6) |
			((uint64_t) (buf[7]) << 8 * 7);
		ctx->s[ctx->wordIndex] ^= t;
		if (++ctx->wordIndex ==
		   (SHA3_KECCAK_SPONGE_WORDS - ctx->capacityWords)) {
			keccakf(ctx->s);
			ctx->wordIndex = 0;
		}
	}

	while (tail--)
		ctx->saved |= (uint64_t) (*(buf++)) << ((ctx->byteIndex++) * 8);
}

int partial_hash_sha3_224(uint8_t *data_in, uint8_t *data_out)
{
	sha3_context *ctx;
	int i;

	ctx = rte_zmalloc("sha3-ctx", sizeof(sha3_context), 0);
	if (!ctx) {
		CCP_LOG_ERR("sha3-ctx creation failed");
		return -ENOMEM;
	}
	sha3_Init224(ctx);
	sha3_Update(ctx, data_in, SHA3_224_BLOCK_SIZE);
	for (i = 0; i < CCP_SHA3_CTX_SIZE; i++, data_out++)
		*data_out = ctx->sb[CCP_SHA3_CTX_SIZE - i - 1];
	rte_free(ctx);

	return 0;
}

int partial_hash_sha3_256(uint8_t *data_in, uint8_t *data_out)
{
	sha3_context *ctx;
	int i;

	ctx = rte_zmalloc("sha3-ctx", sizeof(sha3_context), 0);
	if (!ctx) {
		CCP_LOG_ERR("sha3-ctx creation failed");
		return -ENOMEM;
	}
	sha3_Init256(ctx);
	sha3_Update(ctx, data_in, SHA3_256_BLOCK_SIZE);
	for (i = 0; i < CCP_SHA3_CTX_SIZE; i++, data_out++)
		*data_out = ctx->sb[CCP_SHA3_CTX_SIZE - i - 1];
	rte_free(ctx);

	return 0;
}

int partial_hash_sha3_384(uint8_t *data_in, uint8_t *data_out)
{
	sha3_context *ctx;
	int i;

	ctx = rte_zmalloc("sha3-ctx", sizeof(sha3_context), 0);
	if (!ctx) {
		CCP_LOG_ERR("sha3-ctx creation failed");
		return -ENOMEM;
	}
	sha3_Init384(ctx);
	sha3_Update(ctx, data_in, SHA3_384_BLOCK_SIZE);
	for (i = 0; i < CCP_SHA3_CTX_SIZE; i++, data_out++)
		*data_out = ctx->sb[CCP_SHA3_CTX_SIZE - i - 1];
	rte_free(ctx);

	return 0;
}

int partial_hash_sha3_512(uint8_t *data_in, uint8_t *data_out)
{
	sha3_context *ctx;
	int i;

	ctx = rte_zmalloc("sha3-ctx", sizeof(sha3_context), 0);
	if (!ctx) {
		CCP_LOG_ERR("sha3-ctx creation failed");
		return -ENOMEM;
	}
	sha3_Init512(ctx);
	sha3_Update(ctx, data_in, SHA3_512_BLOCK_SIZE);
	for (i = 0; i < CCP_SHA3_CTX_SIZE; i++, data_out++)
		*data_out = ctx->sb[CCP_SHA3_CTX_SIZE - i - 1];
	rte_free(ctx);

	return 0;
}

static int generate_partial_hash(struct ccp_session *sess)
{

	uint8_t ipad[sess->auth.block_size];
	uint8_t	opad[sess->auth.block_size];
	uint8_t *ipad_t, *opad_t;
	uint32_t *hash_value_be32, hash_temp32[8];
	uint64_t *hash_value_be64, hash_temp64[8];
	int i, count;
	uint8_t *hash_value_sha3;

	opad_t = ipad_t = (uint8_t *)sess->auth.key;

	hash_value_be32 = (uint32_t *)((uint8_t *)sess->auth.pre_compute);
	hash_value_be64 = (uint64_t *)((uint8_t *)sess->auth.pre_compute);

	/* considering key size is always equal to block size of algorithm */
	for (i = 0; i < sess->auth.block_size; i++) {
		ipad[i] = (ipad_t[i] ^ HMAC_IPAD_VALUE);
		opad[i] = (opad_t[i] ^ HMAC_OPAD_VALUE);
	}

	switch (sess->auth.algo) {
	case CCP_AUTH_ALGO_SHA1_HMAC:
		count = SHA1_DIGEST_SIZE >> 2;

		if (partial_hash_sha1(ipad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];

		hash_value_be32 = (uint32_t *)((uint8_t *)sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha1(opad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];
		return 0;
	case CCP_AUTH_ALGO_SHA224_HMAC:
		count = SHA256_DIGEST_SIZE >> 2;

		if (partial_hash_sha224(ipad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];

		hash_value_be32 = (uint32_t *)((uint8_t *)sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha224(opad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];
		return 0;
	case CCP_AUTH_ALGO_SHA3_224_HMAC:
		hash_value_sha3 = sess->auth.pre_compute;
		if (partial_hash_sha3_224(ipad, hash_value_sha3))
			return -1;

		hash_value_sha3 = (uint8_t *)(sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha3_224(opad, hash_value_sha3))
			return -1;
		return 0;
	case CCP_AUTH_ALGO_SHA256_HMAC:
		count = SHA256_DIGEST_SIZE >> 2;

		if (partial_hash_sha256(ipad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];

		hash_value_be32 = (uint32_t *)((uint8_t *)sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha256(opad, (uint8_t *)hash_temp32))
			return -1;
		for (i = 0; i < count; i++, hash_value_be32++)
			*hash_value_be32 = hash_temp32[count - 1 - i];
		return 0;
	case CCP_AUTH_ALGO_SHA3_256_HMAC:
		hash_value_sha3 = sess->auth.pre_compute;
		if (partial_hash_sha3_256(ipad, hash_value_sha3))
			return -1;

		hash_value_sha3 = (uint8_t *)(sess->auth.pre_compute
					      + sess->auth.ctx_len);
		if (partial_hash_sha3_256(opad, hash_value_sha3))
			return -1;
		return 0;
	case CCP_AUTH_ALGO_SHA384_HMAC:
		count = SHA512_DIGEST_SIZE >> 3;

		if (partial_hash_sha384(ipad, (uint8_t *)hash_temp64))
			return -1;
		for (i = 0; i < count; i++, hash_value_be64++)
			*hash_value_be64 = hash_temp64[count - 1 - i];

		hash_value_be64 = (uint64_t *)((uint8_t *)sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha384(opad, (uint8_t *)hash_temp64))
			return -1;
		for (i = 0; i < count; i++, hash_value_be64++)
			*hash_value_be64 = hash_temp64[count - 1 - i];
		return 0;
	case CCP_AUTH_ALGO_SHA3_384_HMAC:
		hash_value_sha3 = sess->auth.pre_compute;
		if (partial_hash_sha3_384(ipad, hash_value_sha3))
			return -1;

		hash_value_sha3 = (uint8_t *)(sess->auth.pre_compute
					      + sess->auth.ctx_len);
		if (partial_hash_sha3_384(opad, hash_value_sha3))
			return -1;
		return 0;
	case CCP_AUTH_ALGO_SHA512_HMAC:
		count = SHA512_DIGEST_SIZE >> 3;

		if (partial_hash_sha512(ipad, (uint8_t *)hash_temp64))
			return -1;
		for (i = 0; i < count; i++, hash_value_be64++)
			*hash_value_be64 = hash_temp64[count - 1 - i];

		hash_value_be64 = (uint64_t *)((uint8_t *)sess->auth.pre_compute
					       + sess->auth.ctx_len);
		if (partial_hash_sha512(opad, (uint8_t *)hash_temp64))
			return -1;
		for (i = 0; i < count; i++, hash_value_be64++)
			*hash_value_be64 = hash_temp64[count - 1 - i];
		return 0;
	case CCP_AUTH_ALGO_SHA3_512_HMAC:
		hash_value_sha3 = sess->auth.pre_compute;
		if (partial_hash_sha3_512(ipad, hash_value_sha3))
			return -1;

		hash_value_sha3 = (uint8_t *)(sess->auth.pre_compute
					      + sess->auth.ctx_len);
		if (partial_hash_sha3_512(opad, hash_value_sha3))
			return -1;
		return 0;
	default:
		CCP_LOG_ERR("Invalid auth algo");
		return -1;
	}
}

/* prepare temporary keys K1 and K2 */
static void prepare_key(unsigned char *k, unsigned char *l, int bl)
{
	int i;
	/* Shift block to left, including carry */
	for (i = 0; i < bl; i++) {
		k[i] = l[i] << 1;
		if (i < bl - 1 && l[i + 1] & 0x80)
			k[i] |= 1;
	}
	/* If MSB set fixup with R */
	if (l[0] & 0x80)
		k[bl - 1] ^= bl == 16 ? 0x87 : 0x1b;
}

/* subkeys K1 and K2 generation for CMAC */
static int
generate_cmac_subkeys(struct ccp_session *sess)
{
	const EVP_CIPHER *algo;
	EVP_CIPHER_CTX *ctx;
	unsigned char *ccp_ctx;
	size_t i;
	int dstlen, totlen;
	unsigned char zero_iv[AES_BLOCK_SIZE] = {0};
	unsigned char dst[2 * AES_BLOCK_SIZE] = {0};
	unsigned char k1[AES_BLOCK_SIZE] = {0};
	unsigned char k2[AES_BLOCK_SIZE] = {0};

	if (sess->auth.ut.aes_type == CCP_AES_TYPE_128)
		algo =  EVP_aes_128_cbc();
	else if (sess->auth.ut.aes_type == CCP_AES_TYPE_192)
		algo =  EVP_aes_192_cbc();
	else if (sess->auth.ut.aes_type == CCP_AES_TYPE_256)
		algo =  EVP_aes_256_cbc();
	else {
		CCP_LOG_ERR("Invalid CMAC type length");
		return -1;
	}

	ctx = EVP_CIPHER_CTX_new();
	if (!ctx) {
		CCP_LOG_ERR("ctx creation failed");
		return -1;
	}
	if (EVP_EncryptInit(ctx, algo, (unsigned char *)sess->auth.key,
			    (unsigned char *)zero_iv) <= 0)
		goto key_generate_err;
	if (EVP_CIPHER_CTX_set_padding(ctx, 0) <= 0)
		goto key_generate_err;
	if (EVP_EncryptUpdate(ctx, dst, &dstlen, zero_iv,
			      AES_BLOCK_SIZE) <= 0)
		goto key_generate_err;
	if (EVP_EncryptFinal_ex(ctx, dst + dstlen, &totlen) <= 0)
		goto key_generate_err;

	memset(sess->auth.pre_compute, 0, CCP_SB_BYTES * 2);

	ccp_ctx = (unsigned char *)(sess->auth.pre_compute + CCP_SB_BYTES - 1);
	prepare_key(k1, dst, AES_BLOCK_SIZE);
	for (i = 0; i < AES_BLOCK_SIZE;  i++, ccp_ctx--)
		*ccp_ctx = k1[i];

	ccp_ctx = (unsigned char *)(sess->auth.pre_compute +
				   (2 * CCP_SB_BYTES) - 1);
	prepare_key(k2, k1, AES_BLOCK_SIZE);
	for (i = 0; i < AES_BLOCK_SIZE;  i++, ccp_ctx--)
		*ccp_ctx = k2[i];

	EVP_CIPHER_CTX_free(ctx);

	return 0;

key_generate_err:
	CCP_LOG_ERR("CMAC Init failed");
		return -1;
}

/* configure session */
static int
ccp_configure_session_cipher(struct ccp_session *sess,
			     const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_cipher_xform *cipher_xform = NULL;
	size_t i, j, x;

	cipher_xform = &xform->cipher;

	/* set cipher direction */
	if (cipher_xform->op ==  RTE_CRYPTO_CIPHER_OP_ENCRYPT)
		sess->cipher.dir = CCP_CIPHER_DIR_ENCRYPT;
	else
		sess->cipher.dir = CCP_CIPHER_DIR_DECRYPT;

	/* set cipher key */
	sess->cipher.key_length = cipher_xform->key.length;
	rte_memcpy(sess->cipher.key, cipher_xform->key.data,
		   cipher_xform->key.length);

	/* set iv parameters */
	sess->iv.offset = cipher_xform->iv.offset;
	sess->iv.length = cipher_xform->iv.length;

	switch (cipher_xform->algo) {
	case RTE_CRYPTO_CIPHER_AES_CTR:
		sess->cipher.algo = CCP_CIPHER_ALGO_AES_CTR;
		sess->cipher.um.aes_mode = CCP_AES_MODE_CTR;
		sess->cipher.engine = CCP_ENGINE_AES;
		break;
	case RTE_CRYPTO_CIPHER_AES_ECB:
		sess->cipher.algo = CCP_CIPHER_ALGO_AES_CBC;
		sess->cipher.um.aes_mode = CCP_AES_MODE_ECB;
		sess->cipher.engine = CCP_ENGINE_AES;
		break;
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sess->cipher.algo = CCP_CIPHER_ALGO_AES_CBC;
		sess->cipher.um.aes_mode = CCP_AES_MODE_CBC;
		sess->cipher.engine = CCP_ENGINE_AES;
		break;
	case RTE_CRYPTO_CIPHER_3DES_CBC:
		sess->cipher.algo = CCP_CIPHER_ALGO_3DES_CBC;
		sess->cipher.um.des_mode = CCP_DES_MODE_CBC;
		sess->cipher.engine = CCP_ENGINE_3DES;
		break;
	default:
		CCP_LOG_ERR("Unsupported cipher algo");
		return -1;
	}


	switch (sess->cipher.engine) {
	case CCP_ENGINE_AES:
		if (sess->cipher.key_length == 16)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_128;
		else if (sess->cipher.key_length == 24)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_192;
		else if (sess->cipher.key_length == 32)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_256;
		else {
			CCP_LOG_ERR("Invalid cipher key length");
			return -1;
		}
		for (i = 0; i < sess->cipher.key_length ; i++)
			sess->cipher.key_ccp[sess->cipher.key_length - i - 1] =
				sess->cipher.key[i];
		break;
	case CCP_ENGINE_3DES:
		if (sess->cipher.key_length == 16)
			sess->cipher.ut.des_type = CCP_DES_TYPE_128;
		else if (sess->cipher.key_length == 24)
			sess->cipher.ut.des_type = CCP_DES_TYPE_192;
		else {
			CCP_LOG_ERR("Invalid cipher key length");
			return -1;
		}
		for (j = 0, x = 0; j < sess->cipher.key_length/8; j++, x += 8)
			for (i = 0; i < 8; i++)
				sess->cipher.key_ccp[(8 + x) - i - 1] =
					sess->cipher.key[i + x];
		break;
	default:
		CCP_LOG_ERR("Invalid CCP Engine");
		return -ENOTSUP;
	}
	if (iommu_mode == 2) {
		sess->cipher.nonce_phys = rte_mem_virt2iova(sess->cipher.nonce);
		sess->cipher.key_phys = rte_mem_virt2iova(sess->cipher.key_ccp);
	} else {
		sess->cipher.nonce_phys = rte_mem_virt2phy(sess->cipher.nonce);
		sess->cipher.key_phys = rte_mem_virt2phy(sess->cipher.key_ccp);
	}
	return 0;
}

static int
ccp_configure_session_auth(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_auth_xform *auth_xform = NULL;
	size_t i;

	auth_xform = &xform->auth;

	sess->auth.digest_length = auth_xform->digest_length;
	if (auth_xform->op ==  RTE_CRYPTO_AUTH_OP_GENERATE)
		sess->auth.op = CCP_AUTH_OP_GENERATE;
	else
		sess->auth.op = CCP_AUTH_OP_VERIFY;
	switch (auth_xform->algo) {
	case RTE_CRYPTO_AUTH_MD5_HMAC:
		if (sess->auth_opt) {
			sess->auth.algo = CCP_AUTH_ALGO_MD5_HMAC;
			sess->auth.offset = ((CCP_SB_BYTES << 1) -
					     MD5_DIGEST_SIZE);
			sess->auth.key_length = auth_xform->key.length;
			sess->auth.block_size = MD5_BLOCK_SIZE;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else
			return -1; /* HMAC MD5 not supported on CCP */
		break;
	case RTE_CRYPTO_AUTH_SHA1:
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.algo = CCP_AUTH_ALGO_SHA1;
		sess->auth.ut.sha_type = CCP_SHA_TYPE_1;
		sess->auth.ctx = (void *)ccp_sha1_init;
		sess->auth.ctx_len = CCP_SB_BYTES;
		sess->auth.offset = CCP_SB_BYTES - SHA1_DIGEST_SIZE;
		rte_memcpy(sha_ctx, sess->auth.ctx, SHA_COMMON_DIGEST_SIZE);
		break;
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
		if (sess->auth_opt) {
			if (auth_xform->key.length > SHA1_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA1_HMAC;
			sess->auth.offset = CCP_SB_BYTES - SHA1_DIGEST_SIZE;
			sess->auth.block_size = SHA1_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else {
			if (auth_xform->key.length > SHA1_BLOCK_SIZE)
				return -1;
			sess->auth.engine = CCP_ENGINE_SHA;
			sess->auth.algo = CCP_AUTH_ALGO_SHA1_HMAC;
			sess->auth.ut.sha_type = CCP_SHA_TYPE_1;
			sess->auth.ctx_len = CCP_SB_BYTES;
			sess->auth.offset = CCP_SB_BYTES - SHA1_DIGEST_SIZE;
			sess->auth.block_size = SHA1_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			memset(sess->auth.pre_compute, 0,
			       sess->auth.ctx_len << 1);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
			if (generate_partial_hash(sess))
				return -1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA224:
		sess->auth.algo = CCP_AUTH_ALGO_SHA224;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA_TYPE_224;
		sess->auth.ctx = (void *)ccp_sha224_init;
		sess->auth.ctx_len = CCP_SB_BYTES;
		sess->auth.offset = CCP_SB_BYTES - SHA224_DIGEST_SIZE;
		rte_memcpy(sha_ctx, sess->auth.ctx, SHA256_DIGEST_SIZE);
		break;
	case RTE_CRYPTO_AUTH_SHA224_HMAC:
		if (sess->auth_opt) {
			if (auth_xform->key.length > SHA224_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA224_HMAC;
			sess->auth.offset = CCP_SB_BYTES - SHA224_DIGEST_SIZE;
			sess->auth.block_size = SHA224_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else {
			if (auth_xform->key.length > SHA224_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA224_HMAC;
			sess->auth.engine = CCP_ENGINE_SHA;
			sess->auth.ut.sha_type = CCP_SHA_TYPE_224;
			sess->auth.ctx_len = CCP_SB_BYTES;
			sess->auth.offset = CCP_SB_BYTES - SHA224_DIGEST_SIZE;
			sess->auth.block_size = SHA224_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			memset(sess->auth.pre_compute, 0,
			       sess->auth.ctx_len << 1);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
			if (generate_partial_hash(sess))
				return -1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA3_224:
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_224;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_224;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA224_DIGEST_SIZE;
		break;
	case RTE_CRYPTO_AUTH_SHA3_224_HMAC:
		if (auth_xform->key.length > SHA3_224_BLOCK_SIZE)
			return -1;
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_224_HMAC;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_224;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA224_DIGEST_SIZE;
		sess->auth.block_size = SHA3_224_BLOCK_SIZE;
		sess->auth.key_length = auth_xform->key.length;
		memset(sess->auth.key, 0, sess->auth.block_size);
		memset(sess->auth.pre_compute, 0, 2 * sess->auth.ctx_len);
		rte_memcpy(sess->auth.key, auth_xform->key.data,
			   auth_xform->key.length);
		if (generate_partial_hash(sess))
			return -1;
		break;
	case RTE_CRYPTO_AUTH_SHA256:
		sess->auth.algo = CCP_AUTH_ALGO_SHA256;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA_TYPE_256;
		sess->auth.ctx = (void *)ccp_sha256_init;
		sess->auth.ctx_len = CCP_SB_BYTES;
		sess->auth.offset = CCP_SB_BYTES - SHA256_DIGEST_SIZE;
		rte_memcpy(sha_ctx, sess->auth.ctx, SHA256_DIGEST_SIZE);
		break;
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		if (sess->auth_opt) {
			if (auth_xform->key.length > SHA256_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA256_HMAC;
			sess->auth.offset = CCP_SB_BYTES - SHA256_DIGEST_SIZE;
			sess->auth.block_size = SHA256_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else {
			if (auth_xform->key.length > SHA256_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA256_HMAC;
			sess->auth.engine = CCP_ENGINE_SHA;
			sess->auth.ut.sha_type = CCP_SHA_TYPE_256;
			sess->auth.ctx_len = CCP_SB_BYTES;
			sess->auth.offset = CCP_SB_BYTES - SHA256_DIGEST_SIZE;
			sess->auth.block_size = SHA256_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			memset(sess->auth.pre_compute, 0,
			       sess->auth.ctx_len << 1);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
			if (generate_partial_hash(sess))
				return -1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA3_256:
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_256;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_256;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA256_DIGEST_SIZE;
		break;
	case RTE_CRYPTO_AUTH_SHA3_256_HMAC:
		if (auth_xform->key.length > SHA3_256_BLOCK_SIZE)
			return -1;
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_256_HMAC;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_256;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA256_DIGEST_SIZE;
		sess->auth.block_size = SHA3_256_BLOCK_SIZE;
		sess->auth.key_length = auth_xform->key.length;
		memset(sess->auth.key, 0, sess->auth.block_size);
		memset(sess->auth.pre_compute, 0, 2 * sess->auth.ctx_len);
		rte_memcpy(sess->auth.key, auth_xform->key.data,
			   auth_xform->key.length);
		if (generate_partial_hash(sess))
			return -1;
		break;
	case RTE_CRYPTO_AUTH_SHA384:
		sess->auth.algo = CCP_AUTH_ALGO_SHA384;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA_TYPE_384;
		sess->auth.ctx = (void *)ccp_sha384_init;
		sess->auth.ctx_len = CCP_SB_BYTES << 1;
		sess->auth.offset = (CCP_SB_BYTES << 1) - SHA384_DIGEST_SIZE;
		rte_memcpy(sha_ctx, sess->auth.ctx, SHA512_DIGEST_SIZE);
		break;
	case RTE_CRYPTO_AUTH_SHA384_HMAC:
		if (sess->auth_opt) {
			if (auth_xform->key.length > SHA384_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA384_HMAC;
			sess->auth.offset = ((CCP_SB_BYTES << 1) -
					     SHA384_DIGEST_SIZE);
			sess->auth.block_size = SHA384_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else {
			if (auth_xform->key.length > SHA384_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA384_HMAC;
			sess->auth.engine = CCP_ENGINE_SHA;
			sess->auth.ut.sha_type = CCP_SHA_TYPE_384;
			sess->auth.ctx_len = CCP_SB_BYTES << 1;
			sess->auth.offset = ((CCP_SB_BYTES << 1) -
					     SHA384_DIGEST_SIZE);
			sess->auth.block_size = SHA384_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			memset(sess->auth.pre_compute, 0,
			       sess->auth.ctx_len << 1);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
			if (generate_partial_hash(sess))
				return -1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA3_384:
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_384;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_384;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA384_DIGEST_SIZE;
		break;
	case RTE_CRYPTO_AUTH_SHA3_384_HMAC:
		if (auth_xform->key.length > SHA3_384_BLOCK_SIZE)
			return -1;
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_384_HMAC;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_384;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA384_DIGEST_SIZE;
		sess->auth.block_size = SHA3_384_BLOCK_SIZE;
		sess->auth.key_length = auth_xform->key.length;
		memset(sess->auth.key, 0, sess->auth.block_size);
		memset(sess->auth.pre_compute, 0, 2 * sess->auth.ctx_len);
		rte_memcpy(sess->auth.key, auth_xform->key.data,
			   auth_xform->key.length);
		if (generate_partial_hash(sess))
			return -1;
		break;
	case RTE_CRYPTO_AUTH_SHA512:
		sess->auth.algo = CCP_AUTH_ALGO_SHA512;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA_TYPE_512;
		sess->auth.ctx = (void *)ccp_sha512_init;
		sess->auth.ctx_len = CCP_SB_BYTES << 1;
		sess->auth.offset = (CCP_SB_BYTES << 1) - SHA512_DIGEST_SIZE;
		rte_memcpy(sha_ctx, sess->auth.ctx, SHA512_DIGEST_SIZE);
		break;
	case RTE_CRYPTO_AUTH_SHA512_HMAC:
		if (sess->auth_opt) {
			if (auth_xform->key.length > SHA512_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA512_HMAC;
			sess->auth.offset = ((CCP_SB_BYTES << 1) -
					     SHA512_DIGEST_SIZE);
			sess->auth.block_size = SHA512_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
		} else {
			if (auth_xform->key.length > SHA512_BLOCK_SIZE)
				return -1;
			sess->auth.algo = CCP_AUTH_ALGO_SHA512_HMAC;
			sess->auth.engine = CCP_ENGINE_SHA;
			sess->auth.ut.sha_type = CCP_SHA_TYPE_512;
			sess->auth.ctx_len = CCP_SB_BYTES << 1;
			sess->auth.offset = ((CCP_SB_BYTES << 1) -
					     SHA512_DIGEST_SIZE);
			sess->auth.block_size = SHA512_BLOCK_SIZE;
			sess->auth.key_length = auth_xform->key.length;
			memset(sess->auth.key, 0, sess->auth.block_size);
			memset(sess->auth.pre_compute, 0,
			       sess->auth.ctx_len << 1);
			rte_memcpy(sess->auth.key, auth_xform->key.data,
				   auth_xform->key.length);
			if (generate_partial_hash(sess))
				return -1;
		}
		break;
	case RTE_CRYPTO_AUTH_SHA3_512:
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_512;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_512;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA512_DIGEST_SIZE;
		break;
	case RTE_CRYPTO_AUTH_SHA3_512_HMAC:
		if (auth_xform->key.length > SHA3_512_BLOCK_SIZE)
			return -1;
		sess->auth.algo = CCP_AUTH_ALGO_SHA3_512_HMAC;
		sess->auth.engine = CCP_ENGINE_SHA;
		sess->auth.ut.sha_type = CCP_SHA3_TYPE_512;
		sess->auth.ctx_len = CCP_SHA3_CTX_SIZE;
		sess->auth.offset = CCP_SHA3_CTX_SIZE - SHA512_DIGEST_SIZE;
		sess->auth.block_size = SHA3_512_BLOCK_SIZE;
		sess->auth.key_length = auth_xform->key.length;
		memset(sess->auth.key, 0, sess->auth.block_size);
		memset(sess->auth.pre_compute, 0, 2 * sess->auth.ctx_len);
		rte_memcpy(sess->auth.key, auth_xform->key.data,
			   auth_xform->key.length);
		if (generate_partial_hash(sess))
			return -1;
		break;
	case RTE_CRYPTO_AUTH_AES_CMAC:
		sess->auth.algo = CCP_AUTH_ALGO_AES_CMAC;
		sess->auth.engine = CCP_ENGINE_AES;
		sess->auth.um.aes_mode = CCP_AES_MODE_CMAC;
		sess->auth.key_length = auth_xform->key.length;
		/* padding and hash result */
		sess->auth.ctx_len = CCP_SB_BYTES << 1;
		sess->auth.offset = AES_BLOCK_SIZE;
		sess->auth.block_size = AES_BLOCK_SIZE;
		if (sess->auth.key_length == 16)
			sess->auth.ut.aes_type = CCP_AES_TYPE_128;
		else if (sess->auth.key_length == 24)
			sess->auth.ut.aes_type = CCP_AES_TYPE_192;
		else if (sess->auth.key_length == 32)
			sess->auth.ut.aes_type = CCP_AES_TYPE_256;
		else {
			CCP_LOG_ERR("Invalid CMAC key length");
			return -1;
		}
		rte_memcpy(sess->auth.key, auth_xform->key.data,
			   sess->auth.key_length);
		for (i = 0; i < sess->auth.key_length; i++)
			sess->auth.key_ccp[sess->auth.key_length - i - 1] =
				sess->auth.key[i];
		if (generate_cmac_subkeys(sess))
			return -1;
		break;
	default:
		CCP_LOG_ERR("Unsupported hash algo");
		return -ENOTSUP;
	}
	return 0;
}

static int
ccp_configure_session_aead(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform)
{
	const struct rte_crypto_aead_xform *aead_xform = NULL;
	size_t i;

	aead_xform = &xform->aead;

	sess->cipher.key_length = aead_xform->key.length;
	rte_memcpy(sess->cipher.key, aead_xform->key.data,
		   aead_xform->key.length);

	if (aead_xform->op == RTE_CRYPTO_AEAD_OP_ENCRYPT) {
		sess->cipher.dir = CCP_CIPHER_DIR_ENCRYPT;
		sess->auth.op = CCP_AUTH_OP_GENERATE;
	} else {
		sess->cipher.dir = CCP_CIPHER_DIR_DECRYPT;
		sess->auth.op = CCP_AUTH_OP_VERIFY;
	}
	sess->aead_algo = aead_xform->algo;
	sess->auth.aad_length = aead_xform->aad_length;
	sess->auth.digest_length = aead_xform->digest_length;

	/* set iv parameters */
	sess->iv.offset = aead_xform->iv.offset;
	sess->iv.length = aead_xform->iv.length;

	switch (aead_xform->algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		sess->cipher.algo = CCP_CIPHER_ALGO_AES_GCM;
		sess->cipher.um.aes_mode = CCP_AES_MODE_GCTR;
		sess->cipher.engine = CCP_ENGINE_AES;
		if (sess->cipher.key_length == 16)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_128;
		else if (sess->cipher.key_length == 24)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_192;
		else if (sess->cipher.key_length == 32)
			sess->cipher.ut.aes_type = CCP_AES_TYPE_256;
		else {
			CCP_LOG_ERR("Invalid aead key length");
			return -1;
		}
		for (i = 0; i < sess->cipher.key_length; i++)
			sess->cipher.key_ccp[sess->cipher.key_length - i - 1] =
				sess->cipher.key[i];
		sess->auth.algo = CCP_AUTH_ALGO_AES_GCM;
		sess->auth.engine = CCP_ENGINE_AES;
		sess->auth.um.aes_mode = CCP_AES_MODE_GHASH;
		sess->auth.ctx_len = CCP_SB_BYTES;
		sess->auth.offset = 0;
		sess->auth.block_size = AES_BLOCK_SIZE;
		sess->cmd_id = CCP_CMD_COMBINED;
		break;
	default:
		CCP_LOG_ERR("Unsupported aead algo");
		return -ENOTSUP;
	}
	if (iommu_mode == 2) {
		sess->cipher.nonce_phys = rte_mem_virt2iova(sess->cipher.nonce);
		sess->cipher.key_phys = rte_mem_virt2iova(sess->cipher.key_ccp);
	} else {
		sess->cipher.nonce_phys = rte_mem_virt2phy(sess->cipher.nonce);
		sess->cipher.key_phys = rte_mem_virt2phy(sess->cipher.key_ccp);
	}
	return 0;
}

int
ccp_set_session_parameters(struct ccp_session *sess,
			   const struct rte_crypto_sym_xform *xform,
			   struct ccp_private *internals)
{
	const struct rte_crypto_sym_xform *cipher_xform = NULL;
	const struct rte_crypto_sym_xform *auth_xform = NULL;
	const struct rte_crypto_sym_xform *aead_xform = NULL;
	int ret = 0;

	sess->auth_opt = internals->auth_opt;
	sess->cmd_id = ccp_get_cmd_id(xform);

	switch (sess->cmd_id) {
	case CCP_CMD_CIPHER:
		cipher_xform = xform;
		break;
	case CCP_CMD_AUTH:
		auth_xform = xform;
		break;
	case CCP_CMD_CIPHER_HASH:
		cipher_xform = xform;
		auth_xform = xform->next;
		break;
	case CCP_CMD_HASH_CIPHER:
		auth_xform = xform;
		cipher_xform = xform->next;
		break;
	case CCP_CMD_COMBINED:
		aead_xform = xform;
		break;
	default:
		CCP_LOG_ERR("Unsupported cmd_id");
		return -1;
	}

	/* Default IV length = 0 */
	sess->iv.length = 0;
	if (cipher_xform) {
		ret = ccp_configure_session_cipher(sess, cipher_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported cipher parameters");
			return ret;
		}
	}
	if (auth_xform) {
		ret = ccp_configure_session_auth(sess, auth_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported auth parameters");
			return ret;
		}
	}
	if (aead_xform) {
		ret = ccp_configure_session_aead(sess, aead_xform);
		if (ret != 0) {
			CCP_LOG_ERR("Invalid/unsupported aead parameters");
			return ret;
		}
	}
	return ret;
}

/* calculate CCP descriptors requirement */
static inline int
ccp_cipher_slot(struct ccp_session *session)
{
	int count = 0;

	switch (session->cipher.algo) {
	case CCP_CIPHER_ALGO_AES_CBC:
		count = 2;
		/**< op + passthrough for iv */
		break;
	case CCP_CIPHER_ALGO_AES_ECB:
		count = 1;
		/**<only op*/
		break;
	case CCP_CIPHER_ALGO_AES_CTR:
		count = 2;
		/**< op + passthrough for iv */
		break;
	case CCP_CIPHER_ALGO_3DES_CBC:
		count = 2;
		/**< op + passthrough for iv */
		break;
	default:
		CCP_LOG_ERR("Unsupported cipher algo %d",
			    session->cipher.algo);
	}
	return count;
}

static inline int
ccp_auth_slot(struct ccp_session *session)
{
	int count = 0;

	switch (session->auth.algo) {
	case CCP_AUTH_ALGO_SHA1:
	case CCP_AUTH_ALGO_SHA224:
	case CCP_AUTH_ALGO_SHA256:
	case CCP_AUTH_ALGO_SHA384:
	case CCP_AUTH_ALGO_SHA512:
		count = 3;
		/**< op + lsb passthrough cpy to/from*/
		break;
	case CCP_AUTH_ALGO_MD5_HMAC:
		break;
	case CCP_AUTH_ALGO_SHA1_HMAC:
	case CCP_AUTH_ALGO_SHA224_HMAC:
	case CCP_AUTH_ALGO_SHA256_HMAC:
		if (session->auth_opt == 0)
			count = 6;
		break;
	case CCP_AUTH_ALGO_SHA384_HMAC:
	case CCP_AUTH_ALGO_SHA512_HMAC:
		/**
		 * 1. Load PHash1 = H(k ^ ipad); to LSB
		 * 2. generate IHash = H(hash on meassage with PHash1
		 * as init values);
		 * 3. Retrieve IHash 2 slots for 384/512
		 * 4. Load Phash2 = H(k ^ opad); to LSB
		 * 5. generate FHash = H(hash on Ihash with Phash2
		 * as init value);
		 * 6. Retrieve HMAC output from LSB to host memory
		 */
		if (session->auth_opt == 0)
			count = 7;
		break;
	case CCP_AUTH_ALGO_SHA3_224:
	case CCP_AUTH_ALGO_SHA3_256:
	case CCP_AUTH_ALGO_SHA3_384:
	case CCP_AUTH_ALGO_SHA3_512:
		count = 1;
		/**< only op ctx and dst in host memory*/
		break;
	case CCP_AUTH_ALGO_SHA3_224_HMAC:
	case CCP_AUTH_ALGO_SHA3_256_HMAC:
		count = 3;
		break;
	case CCP_AUTH_ALGO_SHA3_384_HMAC:
	case CCP_AUTH_ALGO_SHA3_512_HMAC:
		count = 4;
		/**
		 * 1. Op to Perform Ihash
		 * 2. Retrieve result from LSB to host memory
		 * 3. Perform final hash
		 */
		break;
	case CCP_AUTH_ALGO_AES_CMAC:
		count = 4;
		/**
		 * op
		 * extra descriptor in padding case
		 * (k1/k2(255:128) with iv(127:0))
		 * Retrieve result
		 */
		break;
	default:
		CCP_LOG_ERR("Unsupported auth algo %d",
			    session->auth.algo);
	}

	return count;
}

static int
ccp_aead_slot(struct ccp_session *session)
{
	int count = 0;

	switch (session->aead_algo) {
	case RTE_CRYPTO_AEAD_AES_GCM:
		break;
	default:
		CCP_LOG_ERR("Unsupported aead algo %d",
			    session->aead_algo);
	}
	switch (session->auth.algo) {
	case CCP_AUTH_ALGO_AES_GCM:
		count = 5;
		/**
		 * 1. Passthru iv
		 * 2. Hash AAD
		 * 3. GCTR
		 * 4. Reload passthru
		 * 5. Hash Final tag
		 */
		break;
	default:
		CCP_LOG_ERR("Unsupported combined auth ALGO %d",
			    session->auth.algo);
	}
	return count;
}

int
ccp_compute_slot_count(struct ccp_session *session)
{
	int count = 0;

	switch (session->cmd_id) {
	case CCP_CMD_CIPHER:
		count = ccp_cipher_slot(session);
		break;
	case CCP_CMD_AUTH:
		count = ccp_auth_slot(session);
		break;
	case CCP_CMD_CIPHER_HASH:
	case CCP_CMD_HASH_CIPHER:
		count = ccp_cipher_slot(session);
		count += ccp_auth_slot(session);
		break;
	case CCP_CMD_COMBINED:
		count = ccp_aead_slot(session);
		break;
	default:
		CCP_LOG_ERR("Unsupported cmd_id");

	}

	return count;
}

static uint8_t
algo_select(int sessalgo,
	    const EVP_MD **algo)
{
	int res = 0;

	switch (sessalgo) {
	case CCP_AUTH_ALGO_MD5_HMAC:
		*algo = EVP_md5();
		break;
	case CCP_AUTH_ALGO_SHA1_HMAC:
		*algo = EVP_sha1();
		break;
	case CCP_AUTH_ALGO_SHA224_HMAC:
		*algo = EVP_sha224();
		break;
	case CCP_AUTH_ALGO_SHA256_HMAC:
		*algo = EVP_sha256();
		break;
	case CCP_AUTH_ALGO_SHA384_HMAC:
		*algo = EVP_sha384();
		break;
	case CCP_AUTH_ALGO_SHA512_HMAC:
		*algo = EVP_sha512();
		break;
	default:
		res = -EINVAL;
		break;
	}
	return res;
}

static int
process_cpu_auth_hmac(uint8_t *src, uint8_t *dst,
		      __rte_unused uint8_t *iv,
		      EVP_PKEY *pkey,
		      int srclen,
		      EVP_MD_CTX *ctx,
		      const EVP_MD *algo,
		      uint16_t d_len)
{
	size_t dstlen;
	unsigned char temp_dst[64];

	if (EVP_DigestSignInit(ctx, NULL, algo, NULL, pkey) <= 0)
		goto process_auth_err;

	if (EVP_DigestSignUpdate(ctx, (char *)src, srclen) <= 0)
		goto process_auth_err;

	if (EVP_DigestSignFinal(ctx, temp_dst, &dstlen) <= 0)
		goto process_auth_err;

	memcpy(dst, temp_dst, d_len);
	return 0;
process_auth_err:
	CCP_LOG_ERR("Process cpu auth failed");
	return -EINVAL;
}

static int cpu_crypto_auth(struct ccp_qp *qp,
			   struct rte_crypto_op *op,
			   struct ccp_session *sess,
			   EVP_MD_CTX *ctx)
{
	uint8_t *src, *dst;
	int srclen, status;
	struct rte_mbuf *mbuf_src, *mbuf_dst;
	const EVP_MD *algo = NULL;
	EVP_PKEY *pkey;

	algo_select(sess->auth.algo, &algo);
	pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, sess->auth.key,
				    sess->auth.key_length);
	mbuf_src = op->sym->m_src;
	mbuf_dst = op->sym->m_dst ? op->sym->m_dst : op->sym->m_src;
	srclen = op->sym->auth.data.length;
	src = rte_pktmbuf_mtod_offset(mbuf_src, uint8_t *,
				      op->sym->auth.data.offset);

	if (sess->auth.op == CCP_AUTH_OP_VERIFY) {
		dst = qp->temp_digest;
	} else {
		dst = op->sym->auth.digest.data;
		if (dst == NULL) {
			dst = rte_pktmbuf_mtod_offset(mbuf_dst, uint8_t *,
						     op->sym->auth.data.offset +
						     sess->auth.digest_length);
		}
	}
	status = process_cpu_auth_hmac(src, dst, NULL,
				       pkey, srclen,
				       ctx,
				       algo,
				       sess->auth.digest_length);
	if (status) {
		op->status = RTE_CRYPTO_OP_STATUS_ERROR;
		return status;
	}

	if (sess->auth.op == CCP_AUTH_OP_VERIFY) {
		if (memcmp(dst, op->sym->auth.digest.data,
			   sess->auth.digest_length) != 0) {
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;
		} else {
			op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		}
	} else {
		op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	}
	EVP_PKEY_free(pkey);
	return 0;
}

static void
ccp_perform_passthru(struct ccp_passthru *pst,
		     struct ccp_queue *cmd_q)
{
	struct ccp_desc *desc;
	union ccp_function function;

	desc = &cmd_q->qbase_desc[cmd_q->qidx];

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_PASSTHRU;

	CCP_CMD_SOC(desc) = 0;
	CCP_CMD_IOC(desc) = 0;
	CCP_CMD_INIT(desc) = 0;
	CCP_CMD_EOM(desc) = 0;
	CCP_CMD_PROT(desc) = 0;

	function.raw = 0;
	CCP_PT_BYTESWAP(&function) = pst->byte_swap;
	CCP_PT_BITWISE(&function) = pst->bit_mod;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = pst->len;

	if (pst->dir) {
		CCP_CMD_SRC_LO(desc) = (uint32_t)(pst->src_addr);
		CCP_CMD_SRC_HI(desc) = high32_value(pst->src_addr);
		CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

		CCP_CMD_DST_LO(desc) = (uint32_t)(pst->dest_addr);
		CCP_CMD_DST_HI(desc) = 0;
		CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SB;

		if (pst->bit_mod != CCP_PASSTHRU_BITWISE_NOOP)
			CCP_CMD_LSB_ID(desc) = cmd_q->sb_key;
	} else {

		CCP_CMD_SRC_LO(desc) = (uint32_t)(pst->src_addr);
		CCP_CMD_SRC_HI(desc) = 0;
		CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SB;

		CCP_CMD_DST_LO(desc) = (uint32_t)(pst->dest_addr);
		CCP_CMD_DST_HI(desc) = high32_value(pst->dest_addr);
		CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SYSTEM;
	}

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;
}

static int
ccp_perform_hmac(struct rte_crypto_op *op,
		 struct ccp_queue *cmd_q)
{

	struct ccp_session *session;
	union ccp_function function;
	struct ccp_desc *desc;
	uint32_t tail;
	phys_addr_t src_addr, dest_addr, dest_addr_t;
	struct ccp_passthru pst;
	uint64_t auth_msg_bits;
	void *append_ptr;
	uint8_t *addr;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					 ccp_cryptodev_driver_id);
	addr = session->auth.pre_compute;

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->auth.data.offset);
	append_ptr = (void *)rte_pktmbuf_append(op->sym->m_src,
						session->auth.ctx_len);
	if (iommu_mode == 2) {
		dest_addr = (phys_addr_t)rte_mem_virt2iova(append_ptr);
		pst.src_addr = (phys_addr_t)rte_mem_virt2iova((void *)addr);
	} else {
		dest_addr = (phys_addr_t)rte_mem_virt2phy(append_ptr);
		pst.src_addr = (phys_addr_t)rte_mem_virt2phy((void *)addr);
	}
	dest_addr_t = dest_addr;

	/** Load PHash1 to LSB*/
	pst.dest_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
	pst.len = session->auth.ctx_len;
	pst.dir = 1;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	ccp_perform_passthru(&pst, cmd_q);

	/**sha engine command descriptor for IntermediateHash*/

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;

	CCP_CMD_SOC(desc) = 0;
	CCP_CMD_IOC(desc) = 0;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;
	CCP_CMD_PROT(desc) = 0;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = op->sym->auth.data.length;
	auth_msg_bits = (op->sym->auth.data.length +
			 session->auth.block_size)  * 8;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_sha;
	CCP_CMD_SHA_LO(desc) = ((uint32_t)auth_msg_bits);
	CCP_CMD_SHA_HI(desc) = high32_value(auth_msg_bits);

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* Intermediate Hash value retrieve */
	if ((session->auth.ut.sha_type == CCP_SHA_TYPE_384) ||
	    (session->auth.ut.sha_type == CCP_SHA_TYPE_512)) {

		pst.src_addr =
			(phys_addr_t)((cmd_q->sb_sha + 1) * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t;
		pst.len = CCP_SB_BYTES;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);

		pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t + CCP_SB_BYTES;
		pst.len = CCP_SB_BYTES;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);

	} else {
		pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t;
		pst.len = session->auth.ctx_len;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);

	}

	/** Load PHash2 to LSB*/
	addr += session->auth.ctx_len;
	if (iommu_mode == 2)
		pst.src_addr = (phys_addr_t)rte_mem_virt2iova((void *)addr);
	else
		pst.src_addr = (phys_addr_t)rte_mem_virt2phy((void *)addr);
	pst.dest_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
	pst.len = session->auth.ctx_len;
	pst.dir = 1;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	ccp_perform_passthru(&pst, cmd_q);

	/**sha engine command descriptor for FinalHash*/
	dest_addr_t += session->auth.offset;

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;

	CCP_CMD_SOC(desc) = 0;
	CCP_CMD_IOC(desc) = 0;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;
	CCP_CMD_PROT(desc) = 0;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = (session->auth.ctx_len -
			     session->auth.offset);
	auth_msg_bits = (session->auth.block_size +
			 session->auth.ctx_len -
			 session->auth.offset) * 8;

	CCP_CMD_SRC_LO(desc) = (uint32_t)(dest_addr_t);
	CCP_CMD_SRC_HI(desc) = high32_value(dest_addr_t);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_sha;
	CCP_CMD_SHA_LO(desc) = ((uint32_t)auth_msg_bits);
	CCP_CMD_SHA_HI(desc) = high32_value(auth_msg_bits);

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* Retrieve hmac output */
	pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
	pst.dest_addr = dest_addr;
	pst.len = session->auth.ctx_len;
	pst.dir = 0;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	if ((session->auth.ut.sha_type == CCP_SHA_TYPE_384) ||
	    (session->auth.ut.sha_type == CCP_SHA_TYPE_512))
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	else
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
	ccp_perform_passthru(&pst, cmd_q);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;

}

static int
ccp_perform_sha(struct rte_crypto_op *op,
		struct ccp_queue *cmd_q)
{
	struct ccp_session *session;
	union ccp_function function;
	struct ccp_desc *desc;
	uint32_t tail;
	phys_addr_t src_addr, dest_addr;
	struct ccp_passthru pst;
	void *append_ptr;
	uint64_t auth_msg_bits;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->auth.data.offset);
	append_ptr = (void *)rte_pktmbuf_append(op->sym->m_src,
						session->auth.ctx_len);
	if (iommu_mode == 2) {
		dest_addr = (phys_addr_t)rte_mem_virt2iova(append_ptr);
		pst.src_addr = (phys_addr_t)sha_ctx;
	} else {
		dest_addr = (phys_addr_t)rte_mem_virt2phy(append_ptr);
		pst.src_addr = (phys_addr_t)rte_mem_virt2phy((void *)
						     session->auth.ctx);
	}

	/** Passthru sha context*/

	pst.dest_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
	pst.len = session->auth.ctx_len;
	pst.dir = 1;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	ccp_perform_passthru(&pst, cmd_q);

	/**prepare sha command descriptor*/

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;

	CCP_CMD_SOC(desc) = 0;
	CCP_CMD_IOC(desc) = 0;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;
	CCP_CMD_PROT(desc) = 0;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = op->sym->auth.data.length;
	auth_msg_bits = op->sym->auth.data.length * 8;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_sha;
	CCP_CMD_SHA_LO(desc) = ((uint32_t)auth_msg_bits);
	CCP_CMD_SHA_HI(desc) = high32_value(auth_msg_bits);

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* Hash value retrieve */
	pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
	pst.dest_addr = dest_addr;
	pst.len = session->auth.ctx_len;
	pst.dir = 0;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	if ((session->auth.ut.sha_type == CCP_SHA_TYPE_384) ||
	    (session->auth.ut.sha_type == CCP_SHA_TYPE_512))
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	else
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
	ccp_perform_passthru(&pst, cmd_q);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;

}

static int
ccp_perform_sha3_hmac(struct rte_crypto_op *op,
		      struct ccp_queue *cmd_q)
{
	struct ccp_session *session;
	struct ccp_passthru pst;
	union ccp_function function;
	struct ccp_desc *desc;
	uint8_t *append_ptr;
	uint32_t tail;
	phys_addr_t src_addr, dest_addr, ctx_paddr, dest_addr_t;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->auth.data.offset);
	append_ptr = (uint8_t *)rte_pktmbuf_append(op->sym->m_src,
						session->auth.ctx_len);
	if (!append_ptr) {
		CCP_LOG_ERR("CCP MBUF append failed\n");
		return -1;
	}
	if (iommu_mode == 2) {
		dest_addr = (phys_addr_t)rte_mem_virt2iova((void *)append_ptr);
		ctx_paddr = (phys_addr_t)rte_mem_virt2iova(
					session->auth.pre_compute);
	} else {
		dest_addr = (phys_addr_t)rte_mem_virt2phy((void *)append_ptr);
		ctx_paddr = (phys_addr_t)rte_mem_virt2phy(
					session->auth.pre_compute);
	}
	dest_addr_t = dest_addr + (session->auth.ctx_len / 2);
	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	/*desc1 for SHA3-Ihash operation */
	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;
	CCP_CMD_LEN(desc) = op->sym->auth.data.length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = (cmd_q->sb_sha * CCP_SB_BYTES);
	CCP_CMD_DST_HI(desc) = 0;
	CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SB;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)ctx_paddr);
	CCP_CMD_KEY_HI(desc) = high32_value(ctx_paddr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();
	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* Intermediate Hash value retrieve */
	if ((session->auth.ut.sha_type == CCP_SHA3_TYPE_384) ||
	    (session->auth.ut.sha_type == CCP_SHA3_TYPE_512)) {

		pst.src_addr =
			(phys_addr_t)((cmd_q->sb_sha + 1) * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t;
		pst.len = CCP_SB_BYTES;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);

		pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t + CCP_SB_BYTES;
		pst.len = CCP_SB_BYTES;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);

	} else {
		pst.src_addr = (phys_addr_t)(cmd_q->sb_sha * CCP_SB_BYTES);
		pst.dest_addr = dest_addr_t;
		pst.len = CCP_SB_BYTES;
		pst.dir = 0;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);
	}

	/**sha engine command descriptor for FinalHash*/
	ctx_paddr += CCP_SHA3_CTX_SIZE;
	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	if (session->auth.ut.sha_type == CCP_SHA3_TYPE_224) {
		dest_addr_t += (CCP_SB_BYTES - SHA224_DIGEST_SIZE);
		CCP_CMD_LEN(desc) = SHA224_DIGEST_SIZE;
	} else if (session->auth.ut.sha_type == CCP_SHA3_TYPE_256) {
		CCP_CMD_LEN(desc) = SHA256_DIGEST_SIZE;
	} else if (session->auth.ut.sha_type == CCP_SHA3_TYPE_384) {
		dest_addr_t += (2 * CCP_SB_BYTES - SHA384_DIGEST_SIZE);
		CCP_CMD_LEN(desc) = SHA384_DIGEST_SIZE;
	} else {
		CCP_CMD_LEN(desc) = SHA512_DIGEST_SIZE;
	}

	CCP_CMD_SRC_LO(desc) = ((uint32_t)dest_addr_t);
	CCP_CMD_SRC_HI(desc) = high32_value(dest_addr_t);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = (uint32_t)dest_addr;
	CCP_CMD_DST_HI(desc) = high32_value(dest_addr);
	CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)ctx_paddr);
	CCP_CMD_KEY_HI(desc) = high32_value(ctx_paddr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();
	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static int
ccp_perform_sha3(struct rte_crypto_op *op,
		 struct ccp_queue *cmd_q)
{
	struct ccp_session *session;
	union ccp_function function;
	struct ccp_desc *desc;
	uint8_t *ctx_addr = NULL, *append_ptr = NULL;
	uint32_t tail;
	phys_addr_t src_addr, dest_addr, ctx_paddr;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->auth.data.offset);
	append_ptr = (uint8_t *)rte_pktmbuf_append(op->sym->m_src,
						session->auth.ctx_len);
	if (!append_ptr) {
		CCP_LOG_ERR("CCP MBUF append failed\n");
		return -1;
	}
	if (iommu_mode == 2) {
		dest_addr = (phys_addr_t)rte_mem_virt2iova((void *)append_ptr);
		ctx_paddr = (phys_addr_t)rte_mem_virt2iova((void *)ctx_addr);
	} else {
		dest_addr = (phys_addr_t)rte_mem_virt2phy((void *)append_ptr);
		ctx_paddr = (phys_addr_t)rte_mem_virt2phy((void *)ctx_addr);
	}

	ctx_addr = session->auth.sha3_ctx;

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	/* prepare desc for SHA3 operation */
	CCP_CMD_ENGINE(desc) = CCP_ENGINE_SHA;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;

	function.raw = 0;
	CCP_SHA_TYPE(&function) = session->auth.ut.sha_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = op->sym->auth.data.length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = ((uint32_t)dest_addr);
	CCP_CMD_DST_HI(desc) = high32_value(dest_addr);
	CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)ctx_paddr);
	CCP_CMD_KEY_HI(desc) = high32_value(ctx_paddr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static int
ccp_perform_aes_cmac(struct rte_crypto_op *op,
		     struct ccp_queue *cmd_q)
{
	struct ccp_session *session;
	union ccp_function function;
	struct ccp_passthru pst;
	struct ccp_desc *desc;
	uint32_t tail;
	uint8_t *src_tb, *append_ptr, *ctx_addr;
	phys_addr_t src_addr, dest_addr, key_addr;
	int length, non_align_len;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);
	key_addr = rte_mem_virt2phy(session->auth.key_ccp);

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->auth.data.offset);
	append_ptr = (uint8_t *)rte_pktmbuf_append(op->sym->m_src,
						session->auth.ctx_len);
	dest_addr = (phys_addr_t)rte_mem_virt2phy((void *)append_ptr);

	function.raw = 0;
	CCP_AES_ENCRYPT(&function) = CCP_CIPHER_DIR_ENCRYPT;
	CCP_AES_MODE(&function) = session->auth.um.aes_mode;
	CCP_AES_TYPE(&function) = session->auth.ut.aes_type;

	if (op->sym->auth.data.length % session->auth.block_size == 0) {

		ctx_addr = session->auth.pre_compute;
		memset(ctx_addr, 0, AES_BLOCK_SIZE);
		if (iommu_mode == 2)
			pst.src_addr = (phys_addr_t)rte_mem_virt2iova(
							(void *)ctx_addr);
		else
			pst.src_addr = (phys_addr_t)rte_mem_virt2phy(
							(void *)ctx_addr);

		pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
		pst.len = CCP_SB_BYTES;
		pst.dir = 1;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
		ccp_perform_passthru(&pst, cmd_q);

		desc = &cmd_q->qbase_desc[cmd_q->qidx];
		memset(desc, 0, Q_DESC_SIZE);

		/* prepare desc for aes-cmac command */
		CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
		CCP_CMD_EOM(desc) = 1;
		CCP_CMD_FUNCTION(desc) = function.raw;

		CCP_CMD_LEN(desc) = op->sym->auth.data.length;
		CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
		CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
		CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

		CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
		CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
		CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;
		CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

		cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

		rte_wmb();

		tail =
		(uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
			      cmd_q->qcontrol | CMD_Q_RUN);
	} else {
		ctx_addr = session->auth.pre_compute + CCP_SB_BYTES;
		memset(ctx_addr, 0, AES_BLOCK_SIZE);
		if (iommu_mode == 2)
			pst.src_addr = (phys_addr_t)rte_mem_virt2iova(
							(void *)ctx_addr);
		else
			pst.src_addr = (phys_addr_t)rte_mem_virt2phy(
							(void *)ctx_addr);
		pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
		pst.len = CCP_SB_BYTES;
		pst.dir = 1;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
		ccp_perform_passthru(&pst, cmd_q);

		length = (op->sym->auth.data.length / AES_BLOCK_SIZE);
		length *= AES_BLOCK_SIZE;
		non_align_len = op->sym->auth.data.length - length;
		/* prepare desc for aes-cmac command */
		/*Command 1*/
		desc = &cmd_q->qbase_desc[cmd_q->qidx];
		memset(desc, 0, Q_DESC_SIZE);

		CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
		CCP_CMD_INIT(desc) = 1;
		CCP_CMD_FUNCTION(desc) = function.raw;

		CCP_CMD_LEN(desc) = length;
		CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
		CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
		CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

		CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
		CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
		CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;
		CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

		cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

		/*Command 2*/
		append_ptr = append_ptr + CCP_SB_BYTES;
		memset(append_ptr, 0, AES_BLOCK_SIZE);
		src_tb = rte_pktmbuf_mtod_offset(op->sym->m_src,
						 uint8_t *,
						 op->sym->auth.data.offset +
						 length);
		rte_memcpy(append_ptr, src_tb, non_align_len);
		append_ptr[non_align_len] = CMAC_PAD_VALUE;

		desc = &cmd_q->qbase_desc[cmd_q->qidx];
		memset(desc, 0, Q_DESC_SIZE);

		CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
		CCP_CMD_EOM(desc) = 1;
		CCP_CMD_FUNCTION(desc) = function.raw;
		CCP_CMD_LEN(desc) = AES_BLOCK_SIZE;

		CCP_CMD_SRC_LO(desc) = ((uint32_t)(dest_addr + CCP_SB_BYTES));
		CCP_CMD_SRC_HI(desc) = high32_value(dest_addr + CCP_SB_BYTES);
		CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

		CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
		CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
		CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;
		CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

		cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

		rte_wmb();
		tail =
		(uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
		CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
			      cmd_q->qcontrol | CMD_Q_RUN);
	}
	/* Retrieve result */
	pst.dest_addr = dest_addr;
	pst.src_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
	pst.len = CCP_SB_BYTES;
	pst.dir = 0;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
	ccp_perform_passthru(&pst, cmd_q);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static int
ccp_perform_aes(struct rte_crypto_op *op,
		struct ccp_queue *cmd_q,
		struct ccp_batch_info *b_info)
{
	struct ccp_session *session;
	union ccp_function function;
	uint8_t *lsb_buf;
	struct ccp_passthru pst = {0};
	struct ccp_desc *desc;
	phys_addr_t src_addr, dest_addr, key_addr;
	uint8_t *iv;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);
	function.raw = 0;

	iv = rte_crypto_op_ctod_offset(op, uint8_t *, session->iv.offset);
	if (session->cipher.um.aes_mode != CCP_AES_MODE_ECB) {
		if (session->cipher.um.aes_mode == CCP_AES_MODE_CTR) {
			rte_memcpy(session->cipher.nonce + AES_BLOCK_SIZE,
				   iv, session->iv.length);
			pst.src_addr = (phys_addr_t)session->cipher.nonce_phys;
			CCP_AES_SIZE(&function) = 0x1F;
		} else {
			lsb_buf =
			&(b_info->lsb_buf[b_info->lsb_buf_idx*CCP_SB_BYTES]);
			rte_memcpy(lsb_buf +
				   (CCP_SB_BYTES - session->iv.length),
				   iv, session->iv.length);
			pst.src_addr = b_info->lsb_buf_phys +
				(b_info->lsb_buf_idx * CCP_SB_BYTES);
			b_info->lsb_buf_idx++;
		}

		pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
		pst.len = CCP_SB_BYTES;
		pst.dir = 1;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);
	}

	desc = &cmd_q->qbase_desc[cmd_q->qidx];

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->cipher.data.offset);
	if (likely(op->sym->m_dst != NULL))
		dest_addr = rte_pktmbuf_iova_offset(op->sym->m_dst,
						op->sym->cipher.data.offset);
	else
		dest_addr = src_addr;
	key_addr = session->cipher.key_phys;

	/* prepare desc for aes command */
	CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;

	CCP_AES_ENCRYPT(&function) = session->cipher.dir;
	CCP_AES_MODE(&function) = session->cipher.um.aes_mode;
	CCP_AES_TYPE(&function) = session->cipher.ut.aes_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = op->sym->cipher.data.length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = ((uint32_t)dest_addr);
	CCP_CMD_DST_HI(desc) = high32_value(dest_addr);
	CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
	CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	if (session->cipher.um.aes_mode != CCP_AES_MODE_ECB)
		CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;
	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static int
ccp_perform_3des(struct rte_crypto_op *op,
		struct ccp_queue *cmd_q,
		struct ccp_batch_info *b_info)
{
	struct ccp_session *session;
	union ccp_function function;
	unsigned char *lsb_buf;
	struct ccp_passthru pst;
	struct ccp_desc *desc;
	uint32_t tail;
	uint8_t *iv;
	phys_addr_t src_addr, dest_addr, key_addr;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	iv = rte_crypto_op_ctod_offset(op, uint8_t *, session->iv.offset);
	switch (session->cipher.um.des_mode) {
	case CCP_DES_MODE_CBC:
		lsb_buf = &(b_info->lsb_buf[b_info->lsb_buf_idx*CCP_SB_BYTES]);
		b_info->lsb_buf_idx++;

		rte_memcpy(lsb_buf + (CCP_SB_BYTES - session->iv.length),
			   iv, session->iv.length);
		if (iommu_mode == 2)
			pst.src_addr = (phys_addr_t)rte_mem_virt2iova(
							(void *) lsb_buf);
		else
			pst.src_addr = (phys_addr_t)rte_mem_virt2phy(
							(void *) lsb_buf);
		pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
		pst.len = CCP_SB_BYTES;
		pst.dir = 1;
		pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
		pst.byte_swap = CCP_PASSTHRU_BYTESWAP_256BIT;
		ccp_perform_passthru(&pst, cmd_q);
		break;
	case CCP_DES_MODE_CFB:
	case CCP_DES_MODE_ECB:
		CCP_LOG_ERR("Unsupported DES cipher mode");
		return -ENOTSUP;
	}

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->cipher.data.offset);
	if (unlikely(op->sym->m_dst != NULL))
		dest_addr =
			rte_pktmbuf_iova_offset(op->sym->m_dst,
						   op->sym->cipher.data.offset);
	else
		dest_addr = src_addr;

	if (iommu_mode == 2)
		key_addr = rte_mem_virt2iova(session->cipher.key_ccp);
	else
		key_addr = rte_mem_virt2phy(session->cipher.key_ccp);

	desc = &cmd_q->qbase_desc[cmd_q->qidx];

	memset(desc, 0, Q_DESC_SIZE);

	/* prepare desc for des command */
	CCP_CMD_ENGINE(desc) = CCP_ENGINE_3DES;

	CCP_CMD_SOC(desc) = 0;
	CCP_CMD_IOC(desc) = 0;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_EOM(desc) = 1;
	CCP_CMD_PROT(desc) = 0;

	function.raw = 0;
	CCP_DES_ENCRYPT(&function) = session->cipher.dir;
	CCP_DES_MODE(&function) = session->cipher.um.des_mode;
	CCP_DES_TYPE(&function) = session->cipher.ut.des_type;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = op->sym->cipher.data.length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = ((uint32_t)dest_addr);
	CCP_CMD_DST_HI(desc) = high32_value(dest_addr);
	CCP_CMD_DST_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
	CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	if (session->cipher.um.des_mode)
		CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;

	rte_wmb();

	/* Write the new tail address back to the queue register */
	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	/* Turn the queue back on using our cached control register */
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static int
ccp_perform_aes_gcm(struct rte_crypto_op *op, struct ccp_queue *cmd_q)
{
	struct ccp_session *session;
	union ccp_function function;
	uint8_t *iv;
	struct ccp_passthru pst;
	struct ccp_desc *desc;
	uint32_t tail;
	uint64_t *temp;
	phys_addr_t src_addr, dest_addr, key_addr, aad_addr;
	phys_addr_t digest_dest_addr;
	int length, non_align_len;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					 ccp_cryptodev_driver_id);
	iv = rte_crypto_op_ctod_offset(op, uint8_t *, session->iv.offset);
	key_addr = session->cipher.key_phys;

	src_addr = rte_pktmbuf_iova_offset(op->sym->m_src,
					      op->sym->aead.data.offset);
	if (unlikely(op->sym->m_dst != NULL))
		dest_addr = rte_pktmbuf_iova_offset(op->sym->m_dst,
						op->sym->aead.data.offset);
	else
		dest_addr = src_addr;
	rte_pktmbuf_append(op->sym->m_src, session->auth.ctx_len);
	digest_dest_addr = op->sym->aead.digest.phys_addr;
	temp = (uint64_t *)(op->sym->aead.digest.data + AES_BLOCK_SIZE);
	*temp++ = rte_bswap64(session->auth.aad_length << 3);
	*temp = rte_bswap64(op->sym->aead.data.length << 3);

	non_align_len = op->sym->aead.data.length % AES_BLOCK_SIZE;
	length = CCP_ALIGN(op->sym->aead.data.length, AES_BLOCK_SIZE);

	aad_addr = op->sym->aead.aad.phys_addr;

	/* CMD1 IV Passthru */
	rte_memcpy(session->cipher.nonce + AES_BLOCK_SIZE, iv,
		   session->iv.length);
	pst.src_addr = session->cipher.nonce_phys;
	pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
	pst.len = CCP_SB_BYTES;
	pst.dir = 1;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	ccp_perform_passthru(&pst, cmd_q);

	/* CMD2 GHASH-AAD */
	function.raw = 0;
	CCP_AES_ENCRYPT(&function) = CCP_AES_MODE_GHASH_AAD;
	CCP_AES_MODE(&function) = CCP_AES_MODE_GHASH;
	CCP_AES_TYPE(&function) = session->cipher.ut.aes_type;

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
	CCP_CMD_INIT(desc) = 1;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = session->auth.aad_length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)aad_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(aad_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
	CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;
	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* CMD3 : GCTR Plain text */
	function.raw = 0;
	CCP_AES_ENCRYPT(&function) = session->cipher.dir;
	CCP_AES_MODE(&function) = CCP_AES_MODE_GCTR;
	CCP_AES_TYPE(&function) = session->cipher.ut.aes_type;
	if (non_align_len == 0)
		CCP_AES_SIZE(&function) = (AES_BLOCK_SIZE << 3) - 1;
	else
		CCP_AES_SIZE(&function) = (non_align_len << 3) - 1;


	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
	CCP_CMD_EOM(desc) = 1;
	CCP_CMD_FUNCTION(desc) = function.raw;

	CCP_CMD_LEN(desc) = length;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)src_addr);
	CCP_CMD_SRC_HI(desc) = high32_value(src_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = ((uint32_t)dest_addr);
	CCP_CMD_DST_HI(desc) = high32_value(dest_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
	CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;
	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	/* CMD4 : PT to copy IV */
	pst.src_addr = session->cipher.nonce_phys;
	pst.dest_addr = (phys_addr_t)(cmd_q->sb_iv * CCP_SB_BYTES);
	pst.len = AES_BLOCK_SIZE;
	pst.dir = 1;
	pst.bit_mod = CCP_PASSTHRU_BITWISE_NOOP;
	pst.byte_swap = CCP_PASSTHRU_BYTESWAP_NOOP;
	ccp_perform_passthru(&pst, cmd_q);

	/* CMD5 : GHASH-Final */
	function.raw = 0;
	CCP_AES_ENCRYPT(&function) = CCP_AES_MODE_GHASH_FINAL;
	CCP_AES_MODE(&function) = CCP_AES_MODE_GHASH;
	CCP_AES_TYPE(&function) = session->cipher.ut.aes_type;

	desc = &cmd_q->qbase_desc[cmd_q->qidx];
	memset(desc, 0, Q_DESC_SIZE);

	CCP_CMD_ENGINE(desc) = CCP_ENGINE_AES;
	CCP_CMD_FUNCTION(desc) = function.raw;
	/* Last block (AAD_len || PT_len)*/
	CCP_CMD_LEN(desc) = AES_BLOCK_SIZE;

	CCP_CMD_SRC_LO(desc) = ((uint32_t)digest_dest_addr + AES_BLOCK_SIZE);
	CCP_CMD_SRC_HI(desc) = high32_value(digest_dest_addr + AES_BLOCK_SIZE);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_DST_LO(desc) = ((uint32_t)digest_dest_addr);
	CCP_CMD_DST_HI(desc) = high32_value(digest_dest_addr);
	CCP_CMD_SRC_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_KEY_LO(desc) = ((uint32_t)key_addr);
	CCP_CMD_KEY_HI(desc) = high32_value(key_addr);
	CCP_CMD_KEY_MEM(desc) = CCP_MEMTYPE_SYSTEM;

	CCP_CMD_LSB_ID(desc) = cmd_q->sb_iv;

	cmd_q->qidx = (cmd_q->qidx + 1) % COMMANDS_PER_QUEUE;
	rte_wmb();

	tail = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx * Q_DESC_SIZE);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE, tail);
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
		      cmd_q->qcontrol | CMD_Q_RUN);

	op->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	return 0;
}

static inline int
ccp_crypto_cipher(struct rte_crypto_op *op,
		  struct ccp_queue *cmd_q,
		  struct ccp_batch_info *b_info)
{
	int result = 0;
	struct ccp_session *session;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					 ccp_cryptodev_driver_id);

	switch (session->cipher.algo) {
	case CCP_CIPHER_ALGO_AES_CBC:
		result = ccp_perform_aes(op, cmd_q, b_info);
		b_info->desccnt += 2;
		break;
	case CCP_CIPHER_ALGO_AES_CTR:
		result = ccp_perform_aes(op, cmd_q, b_info);
		b_info->desccnt += 2;
		break;
	case CCP_CIPHER_ALGO_AES_ECB:
		result = ccp_perform_aes(op, cmd_q, b_info);
		b_info->desccnt += 1;
		break;
	case CCP_CIPHER_ALGO_3DES_CBC:
		result = ccp_perform_3des(op, cmd_q, b_info);
		b_info->desccnt += 2;
		break;
	default:
		CCP_LOG_ERR("Unsupported cipher algo %d",
			    session->cipher.algo);
		return -ENOTSUP;
	}
	return result;
}

static inline int
ccp_crypto_auth(struct rte_crypto_op *op,
		struct ccp_queue *cmd_q,
		struct ccp_batch_info *b_info)
{

	int result = 0;
	struct ccp_session *session;

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	switch (session->auth.algo) {
	case CCP_AUTH_ALGO_SHA1:
	case CCP_AUTH_ALGO_SHA224:
	case CCP_AUTH_ALGO_SHA256:
	case CCP_AUTH_ALGO_SHA384:
	case CCP_AUTH_ALGO_SHA512:
		result = ccp_perform_sha(op, cmd_q);
		b_info->desccnt += 3;
		break;
	case CCP_AUTH_ALGO_MD5_HMAC:
		if (session->auth_opt == 0)
			result = -1;
		break;
	case CCP_AUTH_ALGO_SHA1_HMAC:
	case CCP_AUTH_ALGO_SHA224_HMAC:
	case CCP_AUTH_ALGO_SHA256_HMAC:
		if (session->auth_opt == 0) {
			result = ccp_perform_hmac(op, cmd_q);
			b_info->desccnt += 6;
		}
		break;
	case CCP_AUTH_ALGO_SHA384_HMAC:
	case CCP_AUTH_ALGO_SHA512_HMAC:
		if (session->auth_opt == 0) {
			result = ccp_perform_hmac(op, cmd_q);
			b_info->desccnt += 7;
		}
		break;
	case CCP_AUTH_ALGO_SHA3_224:
	case CCP_AUTH_ALGO_SHA3_256:
	case CCP_AUTH_ALGO_SHA3_384:
	case CCP_AUTH_ALGO_SHA3_512:
		result = ccp_perform_sha3(op, cmd_q);
		b_info->desccnt += 1;
		break;
	case CCP_AUTH_ALGO_SHA3_224_HMAC:
	case CCP_AUTH_ALGO_SHA3_256_HMAC:
		result = ccp_perform_sha3_hmac(op, cmd_q);
		b_info->desccnt += 3;
		break;
	case CCP_AUTH_ALGO_SHA3_384_HMAC:
	case CCP_AUTH_ALGO_SHA3_512_HMAC:
		result = ccp_perform_sha3_hmac(op, cmd_q);
		b_info->desccnt += 4;
		break;
	case CCP_AUTH_ALGO_AES_CMAC:
		result = ccp_perform_aes_cmac(op, cmd_q);
		b_info->desccnt += 4;
		break;
	default:
		CCP_LOG_ERR("Unsupported auth algo %d",
			    session->auth.algo);
		return -ENOTSUP;
	}

	return result;
}

static inline int
ccp_crypto_aead(struct rte_crypto_op *op,
		struct ccp_queue *cmd_q,
		struct ccp_batch_info *b_info)
{
	int result = 0;
	struct ccp_session *session;

	session = (struct ccp_session *)get_sym_session_private_data(
					op->sym->session,
					ccp_cryptodev_driver_id);

	switch (session->auth.algo) {
	case CCP_AUTH_ALGO_AES_GCM:
		if (session->cipher.algo != CCP_CIPHER_ALGO_AES_GCM) {
			CCP_LOG_ERR("Incorrect chain order");
			return -1;
		}
		result = ccp_perform_aes_gcm(op, cmd_q);
		b_info->desccnt += 5;
		break;
	default:
		CCP_LOG_ERR("Unsupported aead algo %d",
			    session->aead_algo);
		return -ENOTSUP;
	}
	return result;
}

int
process_ops_to_enqueue(struct ccp_qp *qp,
		       struct rte_crypto_op **op,
		       struct ccp_queue *cmd_q,
		       uint16_t nb_ops,
		       uint16_t total_nb_ops,
		       int slots_req,
		       uint16_t b_idx)
{
	int i, result = 0;
	struct ccp_batch_info *b_info;
	struct ccp_session *session;
	EVP_MD_CTX *auth_ctx = NULL;

	if (rte_mempool_get(qp->batch_mp, (void **)&b_info)) {
		CCP_LOG_ERR("batch info allocation failed");
		return 0;
	}

	auth_ctx = EVP_MD_CTX_create();
	if (unlikely(!auth_ctx)) {
		CCP_LOG_ERR("Unable to create auth ctx");
		return 0;
	}
	b_info->auth_ctr = 0;

	/* populate batch info necessary for dequeue */
	b_info->op_idx = 0;
	b_info->b_idx = 0;
	b_info->lsb_buf_idx = 0;
	b_info->desccnt = 0;
	b_info->cmd_q = cmd_q;
	if (iommu_mode == 2)
		b_info->lsb_buf_phys =
			(phys_addr_t)rte_mem_virt2iova((void *)b_info->lsb_buf);
	else
		b_info->lsb_buf_phys =
			(phys_addr_t)rte_mem_virt2phy((void *)b_info->lsb_buf);

	rte_atomic64_sub(&b_info->cmd_q->free_slots, slots_req);

	b_info->head_offset = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx *
					 Q_DESC_SIZE);
	for (i = b_idx; i < (nb_ops+b_idx); i++) {
		session = (struct ccp_session *)get_sym_session_private_data(
						 op[i]->sym->session,
						 ccp_cryptodev_driver_id);
		switch (session->cmd_id) {
		case CCP_CMD_CIPHER:
			result = ccp_crypto_cipher(op[i], cmd_q, b_info);
			break;
		case CCP_CMD_AUTH:
			if (session->auth_opt) {
				b_info->auth_ctr++;
				result = cpu_crypto_auth(qp, op[i],
							 session, auth_ctx);
			} else
				result = ccp_crypto_auth(op[i], cmd_q, b_info);
			break;
		case CCP_CMD_CIPHER_HASH:
			result = ccp_crypto_cipher(op[i], cmd_q, b_info);
			if (result)
				break;
			result = ccp_crypto_auth(op[i], cmd_q, b_info);
			break;
		case CCP_CMD_HASH_CIPHER:
			if (session->auth_opt) {
				result = cpu_crypto_auth(qp, op[i],
							 session, auth_ctx);
				if (op[i]->status !=
				    RTE_CRYPTO_OP_STATUS_SUCCESS)
					CCP_LOG_ERR("RTE_CRYPTO_OP_STATUS_AUTH_FAILED");
			} else
				result = ccp_crypto_auth(op[i], cmd_q, b_info);

			if (result)
				break;
			result = ccp_crypto_cipher(op[i], cmd_q, b_info);
			break;
		case CCP_CMD_COMBINED:
			result = ccp_crypto_aead(op[i], cmd_q, b_info);
			break;
		default:
			CCP_LOG_ERR("Unsupported cmd_id");
			result = -1;
		}
		if (unlikely(result < 0)) {
			rte_atomic64_add(&b_info->cmd_q->free_slots,
					 (slots_req - b_info->desccnt));
			break;
		}
		b_info->op[i] = op[i];
	}

	b_info->opcnt = i;
	b_info->b_idx = b_idx;
	b_info->total_nb_ops = total_nb_ops;
	b_info->tail_offset = (uint32_t)(cmd_q->qbase_phys_addr + cmd_q->qidx *
					 Q_DESC_SIZE);

	rte_wmb();
	/* Write the new tail address back to the queue register */
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_TAIL_LO_BASE,
			      b_info->tail_offset);
	/* Turn the queue back on using our cached control register */
	CCP_WRITE_REG(cmd_q->reg_base, CMD_Q_CONTROL_BASE,
			      cmd_q->qcontrol | CMD_Q_RUN);

	rte_ring_enqueue(qp->processed_pkts, (void *)b_info);

	EVP_MD_CTX_destroy(auth_ctx);
	return i-b_idx;
}

static inline void ccp_auth_dq_prepare(struct rte_crypto_op *op)
{
	struct ccp_session *session;
	uint8_t *digest_data, *addr;
	struct rte_mbuf *m_last;
	int offset, digest_offset;
	uint8_t digest_le[64];

	session = (struct ccp_session *)get_sym_session_private_data(
					 op->sym->session,
					ccp_cryptodev_driver_id);

	if (session->cmd_id == CCP_CMD_COMBINED) {
		digest_data = op->sym->aead.digest.data;
		digest_offset = op->sym->aead.data.offset +
					op->sym->aead.data.length;
	} else {
		digest_data = op->sym->auth.digest.data;
		digest_offset = op->sym->auth.data.offset +
					op->sym->auth.data.length;
	}
	m_last = rte_pktmbuf_lastseg(op->sym->m_src);
	addr = (uint8_t *)((char *)m_last->buf_addr + m_last->data_off +
			   m_last->data_len - session->auth.ctx_len);

	rte_mb();
	offset = session->auth.offset;

	if (session->auth.engine == CCP_ENGINE_SHA)
		if ((session->auth.ut.sha_type != CCP_SHA_TYPE_1) &&
		    (session->auth.ut.sha_type != CCP_SHA_TYPE_224) &&
		    (session->auth.ut.sha_type != CCP_SHA_TYPE_256)) {
			/* All other algorithms require byte
			 * swap done by host
			 */
			unsigned int i;

			offset = session->auth.ctx_len -
				session->auth.offset - 1;
			for (i = 0; i < session->auth.digest_length; i++)
				digest_le[i] = addr[offset - i];
			offset = 0;
			addr = digest_le;
		}

	op->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
	if (session->auth.op == CCP_AUTH_OP_VERIFY) {
		if (memcmp(addr + offset, digest_data,
			   session->auth.digest_length) != 0)
			op->status = RTE_CRYPTO_OP_STATUS_AUTH_FAILED;

	} else {
		if (unlikely(digest_data == 0))
			digest_data = rte_pktmbuf_mtod_offset(
					op->sym->m_dst, uint8_t *,
					digest_offset);
		rte_memcpy(digest_data, addr + offset,
			   session->auth.digest_length);
	}
	/* Trim area used for digest from mbuf. */
	rte_pktmbuf_trim(op->sym->m_src,
			 session->auth.ctx_len);
}

static int
ccp_prepare_ops(struct ccp_qp *qp,
		struct rte_crypto_op **op_d,
		struct ccp_batch_info *b_info,
		uint16_t nb_ops)
{
	int i, min_ops;
	struct ccp_session *session;

	EVP_MD_CTX *auth_ctx = NULL;

	auth_ctx = EVP_MD_CTX_create();
	if (unlikely(!auth_ctx)) {
		CCP_LOG_ERR("Unable to create auth ctx");
		return 0;
	}
	min_ops = RTE_MIN(nb_ops, b_info->opcnt);

	for (i =  b_info->b_idx; i < min_ops; i++) {
		op_d[i] = b_info->op[b_info->b_idx + b_info->op_idx++];
		session = (struct ccp_session *)get_sym_session_private_data(
						 op_d[i]->sym->session,
						ccp_cryptodev_driver_id);
		switch (session->cmd_id) {
		case CCP_CMD_CIPHER:
			op_d[i]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			break;
		case CCP_CMD_AUTH:
			if (session->auth_opt == 0)
				ccp_auth_dq_prepare(op_d[i]);
			break;
		case CCP_CMD_CIPHER_HASH:
			if (session->auth_opt)
				cpu_crypto_auth(qp, op_d[i],
						session, auth_ctx);
			else
				ccp_auth_dq_prepare(op_d[i]);
			break;
		case CCP_CMD_HASH_CIPHER:
			if (session->auth_opt)
				op_d[i]->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
			else
				ccp_auth_dq_prepare(op_d[i]);
			break;
		case CCP_CMD_COMBINED:
			ccp_auth_dq_prepare(op_d[i]);
			break;
		default:
			CCP_LOG_ERR("Unsupported cmd_id");
		}
	}

	EVP_MD_CTX_destroy(auth_ctx);
	b_info->opcnt -= min_ops;
	return min_ops;
}

int
process_ops_to_dequeue(struct ccp_qp *qp,
		       struct rte_crypto_op **op,
		       uint16_t nb_ops,
		       uint16_t *total_nb_ops)
{
	struct ccp_batch_info *b_info;
	uint32_t cur_head_offset;

	if (qp->b_info != NULL) {
		b_info = qp->b_info;
		if (unlikely(b_info->op_idx > 0))
			goto success;
	} else if (rte_ring_dequeue(qp->processed_pkts,
				    (void **)&b_info))
		return 0;

	if (b_info->auth_ctr == b_info->opcnt)
		goto success;
	*total_nb_ops = b_info->total_nb_ops;
	cur_head_offset = CCP_READ_REG(b_info->cmd_q->reg_base,
				       CMD_Q_HEAD_LO_BASE);

	if (b_info->head_offset < b_info->tail_offset) {
		if ((cur_head_offset >= b_info->head_offset) &&
		    (cur_head_offset < b_info->tail_offset)) {
			qp->b_info = b_info;
			return 0;
		}
	} else if (b_info->tail_offset != b_info->head_offset) {
		if ((cur_head_offset >= b_info->head_offset) ||
		    (cur_head_offset < b_info->tail_offset)) {
			qp->b_info = b_info;
			return 0;
		}
	}


success:
	*total_nb_ops = b_info->total_nb_ops;
	nb_ops = ccp_prepare_ops(qp, op, b_info, nb_ops);
	rte_atomic64_add(&b_info->cmd_q->free_slots, b_info->desccnt);
	b_info->desccnt = 0;
	if (b_info->opcnt > 0) {
		qp->b_info = b_info;
	} else {
		rte_mempool_put(qp->batch_mp, (void *)b_info);
		qp->b_info = NULL;
	}

	return nb_ops;
}
