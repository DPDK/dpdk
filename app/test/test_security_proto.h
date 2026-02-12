/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef _TEST_SECURITY_PROTO_H_
#define _TEST_SECURITY_PROTO_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "test_cryptodev.h"

#define TEST_SEC_CLEARTEXT_MAX_LEN  17408u
#define TEST_SEC_CIPHERTEXT_MAX_LEN 18437u
#define TEST_SEC_PKTS_MAX 32

static_assert(TEST_SEC_CIPHERTEXT_MAX_LEN <= LARGE_MBUF_DATAPAYLOAD_SIZE,
	      "TEST_SEC_CIPHERTEXT_MAX_LEN should not be greater than LARGE_MBUF_DATAPAYLOAD_SIZE");

struct crypto_param {
	enum rte_crypto_sym_xform_type type;
	union {
		enum rte_crypto_cipher_algorithm cipher;
		enum rte_crypto_auth_algorithm auth;
		enum rte_crypto_aead_algorithm aead;
	} alg;
	uint16_t key_length;
	uint16_t iv_length;
	uint16_t digest_length;
};

static const struct crypto_param aead_list[] = {
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead =  RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 16,
		.digest_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead = RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 24,
		.digest_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead = RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 32,
		.digest_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead = RTE_CRYPTO_AEAD_AES_CCM,
		.key_length = 32
	},
};

static const struct crypto_param cipher_list[] = {
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_NULL,
		.key_length = 0,
		.iv_length = 0,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_DES_CBC,
		.key_length = 8,
		.iv_length = 8,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_3DES_CBC,
		.key_length = 24,
		.iv_length = 8,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_AES_CBC,
		.key_length = 16,
		.iv_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_AES_CBC,
		.key_length = 32,
		.iv_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_AES_CTR,
		.key_length = 16,
		.iv_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_AES_CTR,
		.key_length = 24,
		.iv_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_CIPHER,
		.alg.cipher =  RTE_CRYPTO_CIPHER_AES_CTR,
		.key_length = 32,
		.iv_length = 16,
	},
};

static const struct crypto_param auth_list[] = {
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_NULL,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_MD5_HMAC,
		.key_length = 16,
		.digest_length = 12,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA1_HMAC,
		.key_length = 20,
		.digest_length = 12,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA1_HMAC,
		.key_length = 20,
		.digest_length = 20,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA256_HMAC,
		.key_length = 32,
		.digest_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA256_HMAC,
		.key_length = 32,
		.digest_length = 32,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA384_HMAC,
		.key_length = 48,
		.digest_length = 24,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_SHA512_HMAC,
		.key_length = 64,
		.digest_length = 32,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_AES_XCBC_MAC,
		.key_length = 16,
		.digest_length = 12,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AUTH,
		.alg.auth =  RTE_CRYPTO_AUTH_AES_GMAC,
		.key_length = 16,
		.digest_length = 16,
		.iv_length = 12,
	},
};

struct crypto_param_comb {
	const struct crypto_param *param1;
	const struct crypto_param *param2;
};

extern struct crypto_param_comb sec_alg_list[RTE_DIM(aead_list) +
					     (RTE_DIM(cipher_list) * RTE_DIM(auth_list))];

extern struct crypto_param_comb sec_auth_only_alg_list[2 * (RTE_DIM(auth_list) - 1)];

void test_sec_alg_list_populate(void);

void test_sec_auth_only_alg_list_populate(void);

int test_sec_crypto_caps_aead_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead);

int test_sec_crypto_caps_cipher_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *cipher);

int test_sec_crypto_caps_auth_verify(const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *auth);

void test_sec_alg_display(const struct crypto_param *param1, const struct crypto_param *param2);

void test_sec_proto_pattern_generate(void);

void test_sec_proto_pattern_set(uint8_t *buf, int len);

#endif
