/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _TEST_CRYPTODEV_SECURITY_IPSEC_H_
#define _TEST_CRYPTODEV_SECURITY_IPSEC_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

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

int test_ipsec_sec_caps_verify(struct rte_security_ipsec_xform *ipsec_xform,
			       const struct rte_security_capability *sec_cap,
			       bool silent);

int test_ipsec_crypto_caps_aead_verify(
		const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead);

void test_ipsec_td_in_from_out(const struct ipsec_test_data *td_out,
			       struct ipsec_test_data *td_in);

int test_ipsec_post_process(struct rte_mbuf *m,
			    const struct ipsec_test_data *td,
			    struct ipsec_test_data *res_d, bool silent);

int test_ipsec_status_check(struct rte_crypto_op *op,
			    enum rte_security_ipsec_sa_direction dir);

#endif
