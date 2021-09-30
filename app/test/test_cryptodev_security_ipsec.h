/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _TEST_CRYPTODEV_SECURITY_IPSEC_H_
#define _TEST_CRYPTODEV_SECURITY_IPSEC_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#define IPSEC_TEST_PACKETS_MAX 32

struct ipsec_test_data {
	struct {
		uint8_t data[32];
	} key;

	struct {
		uint8_t data[1024];
		unsigned int len;
	} input_text;

	struct {
		uint8_t data[1024];
		unsigned int len;
	} output_text;

	struct {
		uint8_t data[4];
		unsigned int len;
	} salt;

	struct {
		uint8_t data[16];
	} iv;

	struct rte_security_ipsec_xform ipsec_xform;

	bool aead;

	union {
		struct {
			struct rte_crypto_sym_xform cipher;
			struct rte_crypto_sym_xform auth;
		} chain;
		struct rte_crypto_sym_xform aead;
	} xform;
};

struct ipsec_test_flags {
	bool display_alg;
	bool sa_expiry_pkts_soft;
	bool sa_expiry_pkts_hard;
	bool icv_corrupt;
	bool iv_gen;
	uint32_t tunnel_hdr_verify;
	bool udp_encap;
	bool udp_ports_verify;
	bool ip_csum;
	bool l4_csum;
};

struct crypto_param {
	enum rte_crypto_sym_xform_type type;
	union {
		enum rte_crypto_cipher_algorithm cipher;
		enum rte_crypto_auth_algorithm auth;
		enum rte_crypto_aead_algorithm aead;
	} alg;
	uint16_t key_length;
};

static const struct crypto_param aead_list[] = {
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead =  RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 16,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead = RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 24,
	},
	{
		.type = RTE_CRYPTO_SYM_XFORM_AEAD,
		.alg.aead = RTE_CRYPTO_AEAD_AES_GCM,
		.key_length = 32
	},
};

int test_ipsec_sec_caps_verify(struct rte_security_ipsec_xform *ipsec_xform,
			       const struct rte_security_capability *sec_cap,
			       bool silent);

int test_ipsec_crypto_caps_aead_verify(
		const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead);

void test_ipsec_td_in_from_out(const struct ipsec_test_data *td_out,
			       struct ipsec_test_data *td_in);

void test_ipsec_td_prepare(const struct crypto_param *param1,
			   const struct crypto_param *param2,
			   const struct ipsec_test_flags *flags,
			   struct ipsec_test_data *td_array,
			   int nb_td);

void test_ipsec_td_update(struct ipsec_test_data td_inb[],
			  const struct ipsec_test_data td_outb[],
			  int nb_td,
			  const struct ipsec_test_flags *flags);

void test_ipsec_display_alg(const struct crypto_param *param1,
			    const struct crypto_param *param2);

int test_ipsec_post_process(struct rte_mbuf *m,
			    const struct ipsec_test_data *td,
			    struct ipsec_test_data *res_d, bool silent,
			    const struct ipsec_test_flags *flags);

int test_ipsec_status_check(struct rte_crypto_op *op,
			    const struct ipsec_test_flags *flags,
			    enum rte_security_ipsec_sa_direction dir,
			    int pkt_num);

#endif
