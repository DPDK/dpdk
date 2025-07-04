/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _TEST_CRYPTODEV_SECURITY_IPSEC_H_
#define _TEST_CRYPTODEV_SECURITY_IPSEC_H_

#include <rte_cryptodev.h>
#include <rte_security.h>

#include "test_security_proto.h"

#define IPSEC_TEXT_MAX_LEN 16384u

struct ipsec_test_data {
	struct {
		uint8_t data[32];
	} key;
	struct {
		uint8_t data[64];
	} auth_key;

	struct {
		uint8_t data[IPSEC_TEXT_MAX_LEN];
		unsigned int len;
	} input_text;

	struct {
		uint8_t data[IPSEC_TEXT_MAX_LEN];
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

	bool aes_gmac;

	bool auth_only;

	/* Antireplay packet */
	bool ar_packet;

	union {
		struct {
			struct rte_crypto_sym_xform cipher;
			struct rte_crypto_sym_xform auth;
		} chain;
		struct rte_crypto_sym_xform aead;
	} xform;
};

enum df_flags {
	TEST_IPSEC_COPY_DF_INNER_0 = 1,
	TEST_IPSEC_COPY_DF_INNER_1,
	TEST_IPSEC_SET_DF_0_INNER_1,
	TEST_IPSEC_SET_DF_1_INNER_0,
};

#define TEST_IPSEC_DSCP_VAL 0x12

enum dscp_flags {
	TEST_IPSEC_COPY_DSCP_INNER_0 = 1,
	TEST_IPSEC_COPY_DSCP_INNER_1,
	TEST_IPSEC_SET_DSCP_0_INNER_1,
	TEST_IPSEC_SET_DSCP_1_INNER_0,
};

#define TEST_IPSEC_FLABEL_VAL 0x1234

enum flabel_flags {
	TEST_IPSEC_COPY_FLABEL_INNER_0 = 1,
	TEST_IPSEC_COPY_FLABEL_INNER_1,
	TEST_IPSEC_SET_FLABEL_0_INNER_1,
	TEST_IPSEC_SET_FLABEL_1_INNER_0,
};

struct ipsec_test_flags {
	bool display_alg;
	bool sa_expiry_pkts_soft;
	bool sa_expiry_pkts_hard;
	bool sa_expiry_bytes_soft;
	bool sa_expiry_bytes_hard;
	bool icv_corrupt;
	bool iv_gen;
	uint32_t tunnel_hdr_verify;
	bool udp_encap;
	bool udp_ports_verify;
	bool udp_encap_custom_ports;
	bool ip_csum;
	bool l4_csum;
	bool ipv6;
	bool tunnel_ipv6;
	bool transport;
	bool fragment;
	bool stats_success;
	bool antireplay;
	bool use_ext_mbuf;
	enum df_flags df;
	enum dscp_flags dscp;
	enum flabel_flags flabel;
	bool dec_ttl_or_hop_limit;
	bool ah;
	uint32_t plaintext_len;
	int nb_segs_in_mbuf;
	bool inb_oop;
	bool rx_inject;
};

extern struct ipsec_test_data pkt_aes_256_gcm;
extern struct ipsec_test_data pkt_aes_256_gcm_v6;
extern struct ipsec_test_data pkt_aes_128_cbc_hmac_sha256;
extern struct ipsec_test_data pkt_aes_128_cbc_hmac_sha256_v6;

int test_ipsec_sec_caps_verify(struct rte_security_ipsec_xform *ipsec_xform,
			       const struct rte_security_capability *sec_cap,
			       bool silent);

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

int test_ipsec_post_process(const struct rte_mbuf *m,
			    const struct ipsec_test_data *td,
			    struct ipsec_test_data *res_d, bool silent,
			    const struct ipsec_test_flags *flags);

int test_ipsec_status_check(const struct ipsec_test_data *td,
			    struct rte_crypto_op *op,
			    const struct ipsec_test_flags *flags,
			    enum rte_security_ipsec_sa_direction dir,
			    int pkt_num);

int test_ipsec_stats_verify(void *ctx,
			    void *sess,
			    const struct ipsec_test_flags *flags,
			    enum rte_security_ipsec_sa_direction dir);

int test_ipsec_pkt_update(uint8_t *pkt, const struct ipsec_test_flags *flags);

#endif
