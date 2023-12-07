/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef _TEST_CRYPTODEV_SECURITY_TLS_RECORD_H_
#define _TEST_CRYPTODEV_SECURITY_TLS_RECORD_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "test_security_proto.h"

#define TLS_RECORD_MAX_LEN 16384u

struct tls_record_test_data {
	struct {
		uint8_t data[32];
	} key;

	struct {
		uint8_t data[64];
	} auth_key;

	struct {
		uint8_t data[TLS_RECORD_MAX_LEN];
		unsigned int len;
	} input_text;

	struct {
		uint8_t data[TLS_RECORD_MAX_LEN];
		unsigned int len;
	} output_text;

	struct {
		uint8_t data[12];
		unsigned int len;
	} imp_nonce;

	struct {
		uint8_t data[16];
	} iv;

	union {
		struct {
			struct rte_crypto_sym_xform cipher;
			struct rte_crypto_sym_xform auth;
		} chain;
		struct rte_crypto_sym_xform aead;
	} xform;

	struct rte_security_tls_record_xform tls_record_xform;
	uint8_t app_type;
	bool aead;
};

struct tls_record_test_flags {
	bool display_alg;
};

extern struct tls_record_test_data tls_test_data_aes_128_gcm_v1;
extern struct tls_record_test_data tls_test_data_aes_128_gcm_v2;
extern struct tls_record_test_data tls_test_data_aes_256_gcm;
extern struct tls_record_test_data dtls_test_data_aes_128_gcm;
extern struct tls_record_test_data dtls_test_data_aes_256_gcm;
extern struct tls_record_test_data tls_test_data_aes_128_cbc_sha1_hmac;
extern struct tls_record_test_data tls_test_data_aes_128_cbc_sha256_hmac;
extern struct tls_record_test_data tls_test_data_aes_256_cbc_sha1_hmac;
extern struct tls_record_test_data tls_test_data_aes_256_cbc_sha256_hmac;
extern struct tls_record_test_data tls_test_data_3des_cbc_sha1_hmac;
extern struct tls_record_test_data tls_test_data_null_cipher_sha1_hmac;
extern struct tls_record_test_data tls_test_data_chacha20_poly1305;
extern struct tls_record_test_data dtls_test_data_chacha20_poly1305;
extern struct tls_record_test_data dtls_test_data_aes_128_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_aes_128_cbc_sha256_hmac;
extern struct tls_record_test_data dtls_test_data_aes_256_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_aes_256_cbc_sha256_hmac;
extern struct tls_record_test_data dtls_test_data_3des_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_null_cipher_sha1_hmac;

int test_tls_record_status_check(struct rte_crypto_op *op);

int test_tls_record_sec_caps_verify(struct rte_security_tls_record_xform *tls_record_xform,
				    const struct rte_security_capability *sec_cap, bool silent);

void test_tls_record_td_read_from_write(const struct tls_record_test_data *td_out,
					struct tls_record_test_data *td_in);

void test_tls_record_td_prepare(const struct crypto_param *param1,
				const struct crypto_param *param2,
				const struct tls_record_test_flags *flags,
				struct tls_record_test_data *td_array, int nb_td);

void test_tls_record_td_update(struct tls_record_test_data td_inb[],
			       const struct tls_record_test_data td_outb[], int nb_td,
			       const struct tls_record_test_flags *flags);

int test_tls_record_post_process(const struct rte_mbuf *m, const struct tls_record_test_data *td,
				 struct tls_record_test_data *res_d, bool silent);

#endif
