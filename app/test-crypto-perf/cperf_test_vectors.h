/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#ifndef _CPERF_TEST_VECTRORS_
#define _CPERF_TEST_VECTRORS_

#include "cperf_options.h"

struct cperf_test_vector {
	struct {
		uint8_t *data;
		uint32_t length;
	} plaintext;

	struct {
		uint8_t *data;
		uint16_t length;
	} cipher_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} auth_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} aead_key;

	struct {
		uint8_t *data;
		uint16_t length;
	} cipher_iv;

	struct {
		uint8_t *data;
		uint16_t length;
	} auth_iv;

	struct {
		uint8_t *data;
		uint16_t length;
	} aead_iv;

	struct {
		uint8_t *data;
		uint32_t length;
	} ciphertext;

	struct {
		uint8_t *data;
		rte_iova_t phys_addr;
		uint16_t length;
	} aad;

	struct {
		uint8_t *data;
		rte_iova_t phys_addr;
		uint16_t length;
	} digest;

	struct {
		uint32_t auth_offset;
		uint32_t auth_length;
		uint32_t cipher_offset;
		uint32_t cipher_length;
		uint32_t aead_offset;
		uint32_t aead_length;
	} data;

	struct {
		uint8_t *mod;
		uint8_t *exp;
		uint32_t mlen;
		uint32_t elen;
	} modex;

	struct {
		uint8_t *sign_r;
		uint8_t *sign_s;
		uint8_t *message;
		uint8_t *cipher;
		uint32_t sign_len;
		uint32_t msg_len;
		uint32_t cipher_len;
	} sm2;
};

struct cperf_modex_test_data {
#define DATA_SIZE  1024
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} base;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} exponent;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} modulus;
	struct {
		uint8_t data[DATA_SIZE];
		uint16_t len;
	} result;
};

#define TEST_DATA_SIZE 4096
struct cperf_rsa_plaintext {
	uint8_t data[TEST_DATA_SIZE];
	unsigned int len;
};

struct cperf_rsa_test_data {
	char name[64];
	enum rte_crypto_rsa_priv_key_type key_type;
	rte_crypto_param n;
	rte_crypto_param e;
	rte_crypto_param d;
	rte_crypto_param p;
	rte_crypto_param q;
	rte_crypto_param dp;
	rte_crypto_param dq;
	rte_crypto_param qinv;
	rte_crypto_param sign;
	rte_crypto_param cipher;
	enum rte_crypto_rsa_padding_type padding;
};

struct cperf_ecdsa_test_data {
	rte_crypto_param pubkey_qx;
	rte_crypto_param pubkey_qy;
	rte_crypto_param pkey;
	rte_crypto_param k;
	rte_crypto_param sign_r;
	rte_crypto_param sign_s;
	rte_crypto_param message;
	int curve;
};

struct cperf_eddsa_test_data {
	rte_crypto_param pubkey;
	rte_crypto_param pkey;
	rte_crypto_param sign;
	rte_crypto_param message;
	int curve;
	int instance;
};

struct cperf_sm2_test_data {
	rte_crypto_param pubkey_qx;
	rte_crypto_param pubkey_qy;
	rte_crypto_param pkey;
	rte_crypto_param k;
	rte_crypto_param sign_r;
	rte_crypto_param sign_s;
	rte_crypto_param id;
	rte_crypto_param cipher;
	rte_crypto_param message;
	rte_crypto_param digest;
	int curve;
};

struct cperf_test_vector*
cperf_test_vector_get_dummy(struct cperf_options *options);

extern uint8_t ciphertext[2048];

extern uint8_t cipher_key[];
extern uint8_t auth_key[];

extern uint8_t iv[];
extern uint8_t aad[];

extern uint8_t digest[2048];

extern struct cperf_modex_test_data modex_perf_data[10];
extern struct cperf_ecdsa_test_data secp192r1_perf_data;
extern struct cperf_ecdsa_test_data secp224r1_perf_data;
extern struct cperf_ecdsa_test_data secp256r1_perf_data;
extern struct cperf_ecdsa_test_data secp384r1_perf_data;
extern struct cperf_ecdsa_test_data secp521r1_perf_data;
extern struct cperf_eddsa_test_data ed25519_perf_data;
extern struct cperf_sm2_test_data sm2_perf_data;
extern struct cperf_rsa_test_data rsa_pub_perf_data[4];
extern struct cperf_rsa_test_data rsa_exp_perf_data[4];
extern struct cperf_rsa_test_data rsa_qt_perf_data[4];
extern struct cperf_rsa_plaintext rsa_plaintext;

#endif
