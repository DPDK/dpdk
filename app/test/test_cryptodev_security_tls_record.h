/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef _TEST_CRYPTODEV_SECURITY_TLS_RECORD_H_
#define _TEST_CRYPTODEV_SECURITY_TLS_RECORD_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "test_security_proto.h"

/* TLS 1.2 Ciphertext length can be up to (2^14 + 2048 + 5 (TLS Header)) Bytes */
#define TLS_1_2_RECORD_CIPHERTEXT_MAX_LEN  (18437u)
static_assert(TLS_1_2_RECORD_CIPHERTEXT_MAX_LEN <= TEST_SEC_CIPHERTEXT_MAX_LEN,
	      "TEST_SEC_CIPHERTEXT_MAX_LEN should be at least RECORD MAX LEN!");

/* TLS 1.2 Plaintext length can be up to (2^14 + 1024) Bytes */
#define TLS_1_2_RECORD_PLAINTEXT_MAX_LEN   (17408u)
static_assert(TLS_1_2_RECORD_PLAINTEXT_MAX_LEN <= TEST_SEC_CLEARTEXT_MAX_LEN,
	      "TEST_SEC_CLEARTEXT_MAX_LEN should be at least RECORD MAX LEN!");

/* DTLS 1.2 Ciphertext length is similar to TLS 1.2 */
#define DTLS_1_2_RECORD_CIPHERTEXT_MAX_LEN (18437u)
static_assert(DTLS_1_2_RECORD_CIPHERTEXT_MAX_LEN <= TEST_SEC_CIPHERTEXT_MAX_LEN,
	      "TEST_SEC_CIPHERTEXT_MAX_LEN should be at least RECORD MAX LEN!");

/* DTLS 1.2 Plaintext length is similar to TLS 1.2 */
#define DTLS_1_2_RECORD_PLAINTEXT_MAX_LEN  (17408u)
static_assert(DTLS_1_2_RECORD_PLAINTEXT_MAX_LEN <= TEST_SEC_CLEARTEXT_MAX_LEN,
	      "TEST_SEC_CLEARTEXT_MAX_LEN should be at least RECORD MAX LEN!");

/* TLS 1.3 Ciphertext length can be up to (2^14 + 256 + 5 (TLS Header)) Bytes */
#define TLS_1_3_RECORD_CIPHERTEXT_MAX_LEN  (16645u)
static_assert(TLS_1_3_RECORD_CIPHERTEXT_MAX_LEN <= TEST_SEC_CIPHERTEXT_MAX_LEN,
	      "TEST_SEC_CIPHERTEXT_MAX_LEN should be at least RECORD MAX LEN!");

/* TLS 1.3 Plaintext length can be up to 2^14 Bytes */
#define TLS_1_3_RECORD_PLAINTEXT_MAX_LEN   (16384u)
static_assert(TLS_1_3_RECORD_PLAINTEXT_MAX_LEN <= TEST_SEC_CLEARTEXT_MAX_LEN,
	      "TEST_SEC_CLEARTEXT_MAX_LEN should be at least RECORD MAX LEN!");

#define TLS_RECORD_PLAINTEXT_MIN_LEN       (1u)
#define TLS_RECORD_PAD_CORRUPT_OFFSET      20

enum tls_record_test_content_type {
	TLS_RECORD_TEST_CONTENT_TYPE_APP,
	/* For verifying zero packet length */
	TLS_RECORD_TEST_CONTENT_TYPE_HANDSHAKE,
	/* For verifying handling of custom content types */
	TLS_RECORD_TEST_CONTENT_TYPE_CUSTOM,
};

struct tls_record_test_data {
	struct {
		uint8_t data[32];
	} key;

	struct {
		uint8_t data[64];
	} auth_key;

	struct {
		uint8_t data[TEST_SEC_CIPHERTEXT_MAX_LEN];
		unsigned int len;
	} input_text;

	struct {
		uint8_t data[TEST_SEC_CIPHERTEXT_MAX_LEN];
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
	bool ar_packet;
};

struct tls_record_test_flags {
	bool display_alg;
	bool data_walkthrough;
	bool pkt_corruption;
	bool zero_len;
	bool padding_corruption;
	bool out_of_place;
	bool skip_sess_destroy;
	uint8_t nb_segs_in_mbuf;
	uint8_t opt_padding;
	enum rte_security_tls_version tls_version;
	enum tls_record_test_content_type content_type;
	int ar_win_size;
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
extern struct tls_record_test_data tls_test_data_aes_256_cbc_sha384_hmac;
extern struct tls_record_test_data tls_test_data_3des_cbc_sha1_hmac;
extern struct tls_record_test_data tls_test_data_null_cipher_sha1_hmac;
extern struct tls_record_test_data tls_test_data_chacha20_poly1305;
extern struct tls_record_test_data dtls_test_data_chacha20_poly1305;
extern struct tls_record_test_data dtls_test_data_aes_128_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_aes_128_cbc_sha256_hmac;
extern struct tls_record_test_data dtls_test_data_aes_256_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_aes_256_cbc_sha256_hmac;
extern struct tls_record_test_data dtls_test_data_aes_256_cbc_sha384_hmac;
extern struct tls_record_test_data dtls_test_data_3des_cbc_sha1_hmac;
extern struct tls_record_test_data dtls_test_data_null_cipher_sha1_hmac;
extern struct tls_record_test_data tls13_test_data_aes_128_gcm;
extern struct tls_record_test_data tls13_test_data_aes_256_gcm;
extern struct tls_record_test_data tls13_test_data_chacha20_poly1305;

int test_tls_record_status_check(struct rte_crypto_op *op,
				 const struct tls_record_test_data *td);

int test_tls_record_sec_caps_verify(struct rte_security_tls_record_xform *tls_record_xform,
				    const struct rte_security_capability *sec_cap, bool silent);

void test_tls_record_td_read_from_write(const struct tls_record_test_data *td_out,
					struct tls_record_test_data *td_in);

int test_tls_record_td_prepare(const struct crypto_param *param1,
			       const struct crypto_param *param2,
			       const struct tls_record_test_flags *flags,
			       struct tls_record_test_data *td_array, int nb_td,
			       unsigned int data_len);

void test_tls_record_td_update(struct tls_record_test_data td_inb[],
			       const struct tls_record_test_data td_outb[], int nb_td,
			       const struct tls_record_test_flags *flags);

int test_tls_record_post_process(const struct rte_mbuf *m, const struct tls_record_test_data *td,
				 struct tls_record_test_data *res_d, bool silent,
				 const struct tls_record_test_flags *flags);
#endif
