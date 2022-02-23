/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef RTE_EXEC_ENV_WINDOWS

#include <rte_common.h>
#include <rte_cryptodev.h>
#include <rte_esp.h>
#include <rte_ip.h>
#include <rte_security.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "test.h"
#include "test_cryptodev_security_ipsec.h"

#define IV_LEN_MAX 16

struct crypto_param_comb alg_list[RTE_DIM(aead_list) +
				  (RTE_DIM(cipher_list) *
				   RTE_DIM(auth_list))];

static bool
is_valid_ipv4_pkt(const struct rte_ipv4_hdr *pkt)
{
	/* The IP version number must be 4 */
	if (((pkt->version_ihl) >> 4) != 4)
		return false;
	/*
	 * The IP header length field must be large enough to hold the
	 * minimum length legal IP datagram (20 bytes = 5 words).
	 */
	if ((pkt->version_ihl & 0xf) < 5)
		return false;

	/*
	 * The IP total length field must be large enough to hold the IP
	 * datagram header, whose length is specified in the IP header length
	 * field.
	 */
	if (rte_cpu_to_be_16(pkt->total_length) < sizeof(struct rte_ipv4_hdr))
		return false;

	return true;
}

static bool
is_valid_ipv6_pkt(const struct rte_ipv6_hdr *pkt)
{
	/* The IP version number must be 6 */
	if ((rte_be_to_cpu_32((pkt->vtc_flow)) >> 28) != 6)
		return false;

	return true;
}

void
test_ipsec_alg_list_populate(void)
{
	unsigned long i, j, index = 0;

	for (i = 0; i < RTE_DIM(aead_list); i++) {
		alg_list[index].param1 = &aead_list[i];
		alg_list[index].param2 = NULL;
		index++;
	}

	for (i = 0; i < RTE_DIM(cipher_list); i++) {
		for (j = 0; j < RTE_DIM(auth_list); j++) {
			alg_list[index].param1 = &cipher_list[i];
			alg_list[index].param2 = &auth_list[j];
			index++;
		}
	}
}

int
test_ipsec_sec_caps_verify(struct rte_security_ipsec_xform *ipsec_xform,
			   const struct rte_security_capability *sec_cap,
			   bool silent)
{
	/* Verify security capabilities */

	if (ipsec_xform->options.esn == 1 && sec_cap->ipsec.options.esn == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "ESN is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.udp_encap == 1 &&
	    sec_cap->ipsec.options.udp_encap == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "UDP encapsulation is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.udp_ports_verify == 1 &&
	    sec_cap->ipsec.options.udp_ports_verify == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "UDP encapsulation ports "
				"verification is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_dscp == 1 &&
	    sec_cap->ipsec.options.copy_dscp == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Copy DSCP is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_flabel == 1 &&
	    sec_cap->ipsec.options.copy_flabel == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Copy Flow Label is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.copy_df == 1 &&
	    sec_cap->ipsec.options.copy_df == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Copy DP bit is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.dec_ttl == 1 &&
	    sec_cap->ipsec.options.dec_ttl == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Decrement TTL is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.ecn == 1 && sec_cap->ipsec.options.ecn == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "ECN is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.stats == 1 &&
	    sec_cap->ipsec.options.stats == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1, "Stats is not supported\n");
		return -ENOTSUP;
	}

	if ((ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) &&
	    (ipsec_xform->options.iv_gen_disable == 1) &&
	    (sec_cap->ipsec.options.iv_gen_disable != 1)) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Application provided IV is not supported\n");
		return -ENOTSUP;
	}

	if ((ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
	    (ipsec_xform->options.tunnel_hdr_verify >
	    sec_cap->ipsec.options.tunnel_hdr_verify)) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Tunnel header verify is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.ip_csum_enable == 1 &&
	    sec_cap->ipsec.options.ip_csum_enable == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Inner IP checksum is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->options.l4_csum_enable == 1 &&
	    sec_cap->ipsec.options.l4_csum_enable == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Inner L4 checksum is not supported\n");
		return -ENOTSUP;
	}

	if (ipsec_xform->replay_win_sz > sec_cap->ipsec.replay_win_sz_max) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Replay window size is not supported\n");
		return -ENOTSUP;
	}

	return 0;
}

int
test_ipsec_crypto_caps_aead_verify(
		const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *aead)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *crypto_cap;
	int j = 0;

	while ((crypto_cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (crypto_cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				crypto_cap->sym.xform_type == aead->type &&
				crypto_cap->sym.aead.algo == aead->aead.algo) {
			sym_cap = &crypto_cap->sym;
			if (rte_cryptodev_sym_capability_check_aead(sym_cap,
					aead->aead.key.length,
					aead->aead.digest_length,
					aead->aead.aad_length,
					aead->aead.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}

int
test_ipsec_crypto_caps_cipher_verify(
		const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *cipher)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *cap;
	int j = 0;

	while ((cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				cap->sym.xform_type == cipher->type &&
				cap->sym.cipher.algo == cipher->cipher.algo) {
			sym_cap = &cap->sym;
			if (rte_cryptodev_sym_capability_check_cipher(sym_cap,
					cipher->cipher.key.length,
					cipher->cipher.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}

int
test_ipsec_crypto_caps_auth_verify(
		const struct rte_security_capability *sec_cap,
		struct rte_crypto_sym_xform *auth)
{
	const struct rte_cryptodev_symmetric_capability *sym_cap;
	const struct rte_cryptodev_capabilities *cap;
	int j = 0;

	while ((cap = &sec_cap->crypto_capabilities[j++])->op !=
			RTE_CRYPTO_OP_TYPE_UNDEFINED) {
		if (cap->op == RTE_CRYPTO_OP_TYPE_SYMMETRIC &&
				cap->sym.xform_type == auth->type &&
				cap->sym.auth.algo == auth->auth.algo) {
			sym_cap = &cap->sym;
			if (rte_cryptodev_sym_capability_check_auth(sym_cap,
					auth->auth.key.length,
					auth->auth.digest_length,
					auth->auth.iv.length) == 0)
				return 0;
		}
	}

	return -ENOTSUP;
}

void
test_ipsec_td_in_from_out(const struct ipsec_test_data *td_out,
			  struct ipsec_test_data *td_in)
{
	memcpy(td_in, td_out, sizeof(*td_in));

	/* Populate output text of td_in with input text of td_out */
	memcpy(td_in->output_text.data, td_out->input_text.data,
	       td_out->input_text.len);
	td_in->output_text.len = td_out->input_text.len;

	/* Populate input text of td_in with output text of td_out */
	memcpy(td_in->input_text.data, td_out->output_text.data,
	       td_out->output_text.len);
	td_in->input_text.len = td_out->output_text.len;

	td_in->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;

	if (td_in->aead) {
		td_in->xform.aead.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
	} else {
		td_in->xform.chain.auth.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
		td_in->xform.chain.cipher.cipher.op =
				RTE_CRYPTO_CIPHER_OP_DECRYPT;
	}
}

static bool
is_ipv4(void *ip)
{
	struct rte_ipv4_hdr *ipv4 = ip;
	uint8_t ip_ver;

	ip_ver = (ipv4->version_ihl & 0xf0) >> RTE_IPV4_IHL_MULTIPLIER;
	if (ip_ver == IPVERSION)
		return true;
	else
		return false;
}

static void
test_ipsec_csum_init(void *ip, bool l3, bool l4)
{
	struct rte_ipv4_hdr *ipv4;
	struct rte_tcp_hdr *tcp;
	struct rte_udp_hdr *udp;
	uint8_t next_proto;
	uint8_t size;

	if (is_ipv4(ip)) {
		ipv4 = ip;
		size = sizeof(struct rte_ipv4_hdr);
		next_proto = ipv4->next_proto_id;

		if (l3)
			ipv4->hdr_checksum = 0;
	} else {
		size = sizeof(struct rte_ipv6_hdr);
		next_proto = ((struct rte_ipv6_hdr *)ip)->proto;
	}

	if (l4) {
		switch (next_proto) {
		case IPPROTO_TCP:
			tcp = (struct rte_tcp_hdr *)RTE_PTR_ADD(ip, size);
			tcp->cksum = 0;
			break;
		case IPPROTO_UDP:
			udp = (struct rte_udp_hdr *)RTE_PTR_ADD(ip, size);
			udp->dgram_cksum = 0;
			break;
		default:
			return;
		}
	}
}

void
test_ipsec_td_prepare(const struct crypto_param *param1,
		      const struct crypto_param *param2,
		      const struct ipsec_test_flags *flags,
		      struct ipsec_test_data *td_array,
		      int nb_td)

{
	struct ipsec_test_data *td;
	int i;

	memset(td_array, 0, nb_td * sizeof(*td));

	for (i = 0; i < nb_td; i++) {
		td = &td_array[i];

		/* Prepare fields based on param */

		if (param1->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
			/* Copy template for packet & key fields */
			if (flags->ipv6)
				memcpy(td, &pkt_aes_256_gcm_v6, sizeof(*td));
			else
				memcpy(td, &pkt_aes_256_gcm, sizeof(*td));

			td->aead = true;
			td->xform.aead.aead.algo = param1->alg.aead;
			td->xform.aead.aead.key.length = param1->key_length;
		} else {
			/* Copy template for packet & key fields */
			if (flags->ipv6)
				memcpy(td, &pkt_aes_128_cbc_hmac_sha256_v6,
					sizeof(*td));
			else
				memcpy(td, &pkt_aes_128_cbc_hmac_sha256,
					sizeof(*td));

			td->aead = false;
			td->xform.chain.cipher.cipher.algo = param1->alg.cipher;
			td->xform.chain.cipher.cipher.key.length =
					param1->key_length;
			td->xform.chain.cipher.cipher.iv.length =
					param1->iv_length;
			td->xform.chain.auth.auth.algo = param2->alg.auth;
			td->xform.chain.auth.auth.key.length =
					param2->key_length;
			td->xform.chain.auth.auth.digest_length =
					param2->digest_length;

		}

		if (flags->iv_gen)
			td->ipsec_xform.options.iv_gen_disable = 0;

		if (flags->sa_expiry_pkts_soft)
			td->ipsec_xform.life.packets_soft_limit =
					IPSEC_TEST_PACKETS_MAX - 1;

		if (flags->ip_csum) {
			td->ipsec_xform.options.ip_csum_enable = 1;
			test_ipsec_csum_init(&td->input_text.data, true, false);
		}

		if (flags->l4_csum) {
			td->ipsec_xform.options.l4_csum_enable = 1;
			test_ipsec_csum_init(&td->input_text.data, false, true);
		}

		if (flags->transport) {
			td->ipsec_xform.mode =
					RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT;
		} else {
			td->ipsec_xform.mode =
					RTE_SECURITY_IPSEC_SA_MODE_TUNNEL;

			if (flags->tunnel_ipv6)
				td->ipsec_xform.tunnel.type =
						RTE_SECURITY_IPSEC_TUNNEL_IPV6;
			else
				td->ipsec_xform.tunnel.type =
						RTE_SECURITY_IPSEC_TUNNEL_IPV4;
		}

		if (flags->stats_success)
			td->ipsec_xform.options.stats = 1;

		if (flags->fragment) {
			struct rte_ipv4_hdr *ip;
			ip = (struct rte_ipv4_hdr *)&td->input_text.data;
			ip->fragment_offset = 4;
			ip->hdr_checksum = rte_ipv4_cksum(ip);
		}

		if (flags->df == TEST_IPSEC_COPY_DF_INNER_0 ||
		    flags->df == TEST_IPSEC_COPY_DF_INNER_1)
			td->ipsec_xform.options.copy_df = 1;

		if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_0 ||
		    flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1)
			td->ipsec_xform.options.copy_dscp = 1;

		if (flags->dec_ttl_or_hop_limit)
			td->ipsec_xform.options.dec_ttl = 1;
	}
}

void
test_ipsec_td_update(struct ipsec_test_data td_inb[],
		     const struct ipsec_test_data td_outb[],
		     int nb_td,
		     const struct ipsec_test_flags *flags)
{
	int i;

	for (i = 0; i < nb_td; i++) {
		memcpy(td_inb[i].output_text.data, td_outb[i].input_text.data,
		       td_outb[i].input_text.len);
		td_inb[i].output_text.len = td_outb->input_text.len;

		if (flags->icv_corrupt) {
			int icv_pos = td_inb[i].input_text.len - 4;
			td_inb[i].input_text.data[icv_pos] += 1;
		}

		if (flags->sa_expiry_pkts_hard)
			td_inb[i].ipsec_xform.life.packets_hard_limit =
					IPSEC_TEST_PACKETS_MAX - 1;

		if (flags->udp_encap)
			td_inb[i].ipsec_xform.options.udp_encap = 1;

		if (flags->udp_ports_verify)
			td_inb[i].ipsec_xform.options.udp_ports_verify = 1;

		td_inb[i].ipsec_xform.options.tunnel_hdr_verify =
			flags->tunnel_hdr_verify;

		if (flags->ip_csum)
			td_inb[i].ipsec_xform.options.ip_csum_enable = 1;

		if (flags->l4_csum)
			td_inb[i].ipsec_xform.options.l4_csum_enable = 1;

		/* Clear outbound specific flags */
		td_inb[i].ipsec_xform.options.iv_gen_disable = 0;
	}
}

void
test_ipsec_display_alg(const struct crypto_param *param1,
		       const struct crypto_param *param2)
{
	if (param1->type == RTE_CRYPTO_SYM_XFORM_AEAD) {
		printf("\t%s [%d]",
		       rte_crypto_aead_algorithm_strings[param1->alg.aead],
		       param1->key_length * 8);
	} else {
		printf("\t%s",
		       rte_crypto_cipher_algorithm_strings[param1->alg.cipher]);
		if (param1->alg.cipher != RTE_CRYPTO_CIPHER_NULL)
			printf(" [%d]", param1->key_length * 8);
		printf(" %s",
		       rte_crypto_auth_algorithm_strings[param2->alg.auth]);
		if (param2->alg.auth != RTE_CRYPTO_AUTH_NULL)
			printf(" [%dB ICV]", param2->digest_length);
	}
	printf("\n");
}

static int
test_ipsec_tunnel_hdr_len_get(const struct ipsec_test_data *td)
{
	int len = 0;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		if (td->ipsec_xform.mode == RTE_SECURITY_IPSEC_SA_MODE_TUNNEL) {
			if (td->ipsec_xform.tunnel.type ==
					RTE_SECURITY_IPSEC_TUNNEL_IPV4)
				len += sizeof(struct rte_ipv4_hdr);
			else
				len += sizeof(struct rte_ipv6_hdr);
		}
	}

	return len;
}

static int
test_ipsec_iv_verify_push(struct rte_mbuf *m, const struct ipsec_test_data *td)
{
	static uint8_t iv_queue[IV_LEN_MAX * IPSEC_TEST_PACKETS_MAX];
	uint8_t *iv_tmp, *output_text = rte_pktmbuf_mtod(m, uint8_t *);
	int i, iv_pos, iv_len;
	static int index;

	if (td->aead)
		iv_len = td->xform.aead.aead.iv.length - td->salt.len;
	else
		iv_len = td->xform.chain.cipher.cipher.iv.length;

	iv_pos = test_ipsec_tunnel_hdr_len_get(td) + sizeof(struct rte_esp_hdr);
	output_text += iv_pos;

	TEST_ASSERT(iv_len <= IV_LEN_MAX, "IV length greater than supported");

	/* Compare against previous values */
	for (i = 0; i < index; i++) {
		iv_tmp = &iv_queue[i * IV_LEN_MAX];

		if (memcmp(output_text, iv_tmp, iv_len) == 0) {
			printf("IV repeated");
			return TEST_FAILED;
		}
	}

	/* Save IV for future comparisons */

	iv_tmp = &iv_queue[index * IV_LEN_MAX];
	memcpy(iv_tmp, output_text, iv_len);
	index++;

	if (index == IPSEC_TEST_PACKETS_MAX)
		index = 0;

	return TEST_SUCCESS;
}

static int
test_ipsec_l3_csum_verify(struct rte_mbuf *m)
{
	uint16_t actual_cksum, expected_cksum;
	struct rte_ipv4_hdr *ip;

	ip = rte_pktmbuf_mtod(m, struct rte_ipv4_hdr *);

	if (!is_ipv4((void *)ip))
		return TEST_SKIPPED;

	actual_cksum = ip->hdr_checksum;

	ip->hdr_checksum = 0;

	expected_cksum = rte_ipv4_cksum(ip);

	if (actual_cksum != expected_cksum)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_ipsec_l4_csum_verify(struct rte_mbuf *m)
{
	uint16_t actual_cksum = 0, expected_cksum = 0;
	struct rte_ipv4_hdr *ipv4;
	struct rte_ipv6_hdr *ipv6;
	struct rte_tcp_hdr *tcp;
	struct rte_udp_hdr *udp;
	void *ip, *l4;

	ip = rte_pktmbuf_mtod(m, void *);

	if (is_ipv4(ip)) {
		ipv4 = ip;
		l4 = RTE_PTR_ADD(ipv4, sizeof(struct rte_ipv4_hdr));

		switch (ipv4->next_proto_id) {
		case IPPROTO_TCP:
			tcp = (struct rte_tcp_hdr *)l4;
			actual_cksum = tcp->cksum;
			tcp->cksum = 0;
			expected_cksum = rte_ipv4_udptcp_cksum(ipv4, l4);
			break;
		case IPPROTO_UDP:
			udp = (struct rte_udp_hdr *)l4;
			actual_cksum = udp->dgram_cksum;
			udp->dgram_cksum = 0;
			expected_cksum = rte_ipv4_udptcp_cksum(ipv4, l4);
			break;
		default:
			break;
		}
	} else {
		ipv6 = ip;
		l4 = RTE_PTR_ADD(ipv6, sizeof(struct rte_ipv6_hdr));

		switch (ipv6->proto) {
		case IPPROTO_TCP:
			tcp = (struct rte_tcp_hdr *)l4;
			actual_cksum = tcp->cksum;
			tcp->cksum = 0;
			expected_cksum = rte_ipv6_udptcp_cksum(ipv6, l4);
			break;
		case IPPROTO_UDP:
			udp = (struct rte_udp_hdr *)l4;
			actual_cksum = udp->dgram_cksum;
			udp->dgram_cksum = 0;
			expected_cksum = rte_ipv6_udptcp_cksum(ipv6, l4);
			break;
		default:
			break;
		}
	}

	if (actual_cksum != expected_cksum)
		return TEST_FAILED;

	return TEST_SUCCESS;
}

static int
test_ipsec_ttl_or_hop_decrement_verify(void *received, void *expected)
{
	struct rte_ipv4_hdr *iph4_ex, *iph4_re;
	struct rte_ipv6_hdr *iph6_ex, *iph6_re;

	if (is_ipv4(received) && is_ipv4(expected)) {
		iph4_ex = expected;
		iph4_re = received;
		iph4_ex->time_to_live -= 1;
		if (iph4_re->time_to_live != iph4_ex->time_to_live)
			return TEST_FAILED;
	} else if (!is_ipv4(received) && !is_ipv4(expected)) {
		iph6_ex = expected;
		iph6_re = received;
		iph6_ex->hop_limits -= 1;
		if (iph6_re->hop_limits != iph6_ex->hop_limits)
			return TEST_FAILED;
	} else {
		printf("IP header version miss match\n");
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_ipsec_td_verify(struct rte_mbuf *m, const struct ipsec_test_data *td,
		     bool silent, const struct ipsec_test_flags *flags)
{
	uint8_t *output_text = rte_pktmbuf_mtod(m, uint8_t *);
	uint32_t skip, len = rte_pktmbuf_pkt_len(m);
	uint8_t td_output_text[4096];
	int ret;

	/* For tests with status as error for test success, skip verification */
	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
	    (flags->icv_corrupt ||
	     flags->sa_expiry_pkts_hard ||
	     flags->tunnel_hdr_verify ||
	     td->ar_packet))
		return TEST_SUCCESS;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS &&
	   flags->udp_encap) {
		const struct rte_ipv4_hdr *iph4;
		const struct rte_ipv6_hdr *iph6;

		if (td->ipsec_xform.tunnel.type ==
				RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
			iph4 = (const struct rte_ipv4_hdr *)output_text;
			if (iph4->next_proto_id != IPPROTO_UDP) {
				printf("UDP header is not found\n");
				return TEST_FAILED;
			}
		} else {
			iph6 = (const struct rte_ipv6_hdr *)output_text;
			if (iph6->proto != IPPROTO_UDP) {
				printf("UDP header is not found\n");
				return TEST_FAILED;
			}
		}

		len -= sizeof(struct rte_udp_hdr);
		output_text += sizeof(struct rte_udp_hdr);
	}

	if (len != td->output_text.len) {
		printf("Output length (%d) not matching with expected (%d)\n",
			len, td->output_text.len);
		return TEST_FAILED;
	}

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) &&
				flags->fragment) {
		const struct rte_ipv4_hdr *iph4;
		iph4 = (const struct rte_ipv4_hdr *)output_text;
		if (iph4->fragment_offset) {
			printf("Output packet is fragmented");
			return TEST_FAILED;
		}
	}

	skip = test_ipsec_tunnel_hdr_len_get(td);

	len -= skip;
	output_text += skip;

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
				flags->ip_csum) {
		if (m->ol_flags & RTE_MBUF_F_RX_IP_CKSUM_GOOD)
			ret = test_ipsec_l3_csum_verify(m);
		else
			ret = TEST_FAILED;

		if (ret == TEST_FAILED)
			printf("Inner IP checksum test failed\n");

		return ret;
	}

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
				flags->l4_csum) {
		if (m->ol_flags & RTE_MBUF_F_RX_L4_CKSUM_GOOD)
			ret = test_ipsec_l4_csum_verify(m);
		else
			ret = TEST_FAILED;

		if (ret == TEST_FAILED)
			printf("Inner L4 checksum test failed\n");

		return ret;
	}

	memcpy(td_output_text, td->output_text.data + skip, len);

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
				flags->dec_ttl_or_hop_limit) {
		if (test_ipsec_ttl_or_hop_decrement_verify(output_text, td_output_text)) {
			printf("Inner TTL/hop limit decrement test failed\n");
			return TEST_FAILED;
		}
	}

	if (test_ipsec_pkt_update(td_output_text, flags)) {
		printf("Could not update expected vector");
		return TEST_FAILED;
	}

	if (memcmp(output_text, td_output_text, len)) {
		if (silent)
			return TEST_FAILED;

		printf("TestCase %s line %d: %s\n", __func__, __LINE__,
			"output text not as expected\n");

		rte_hexdump(stdout, "expected", td_output_text, len);
		rte_hexdump(stdout, "actual", output_text, len);
		return TEST_FAILED;
	}

	return TEST_SUCCESS;
}

static int
test_ipsec_res_d_prepare(struct rte_mbuf *m, const struct ipsec_test_data *td,
		   struct ipsec_test_data *res_d)
{
	uint8_t *output_text = rte_pktmbuf_mtod(m, uint8_t *);
	uint32_t len = rte_pktmbuf_pkt_len(m);

	memcpy(res_d, td, sizeof(*res_d));
	memcpy(res_d->input_text.data, output_text, len);
	res_d->input_text.len = len;

	res_d->ipsec_xform.direction = RTE_SECURITY_IPSEC_SA_DIR_INGRESS;
	if (res_d->aead) {
		res_d->xform.aead.aead.op = RTE_CRYPTO_AEAD_OP_DECRYPT;
	} else {
		res_d->xform.chain.cipher.cipher.op =
				RTE_CRYPTO_CIPHER_OP_DECRYPT;
		res_d->xform.chain.auth.auth.op = RTE_CRYPTO_AUTH_OP_VERIFY;
	}

	return TEST_SUCCESS;
}

static int
test_ipsec_iph4_hdr_validate(const struct rte_ipv4_hdr *iph4,
			     const struct ipsec_test_flags *flags)
{
	uint8_t tos, dscp;
	uint16_t f_off;

	if (!is_valid_ipv4_pkt(iph4)) {
		printf("Tunnel outer header is not IPv4\n");
		return -1;
	}

	f_off = rte_be_to_cpu_16(iph4->fragment_offset);
	if (flags->df == TEST_IPSEC_COPY_DF_INNER_1 ||
	    flags->df == TEST_IPSEC_SET_DF_1_INNER_0) {
		if (!(f_off & RTE_IPV4_HDR_DF_FLAG)) {
			printf("DF bit is not set\n");
			return -1;
		}
	} else {
		if (f_off & RTE_IPV4_HDR_DF_FLAG) {
			printf("DF bit is set\n");
			return -1;
		}
	}

	tos = iph4->type_of_service;
	dscp = (tos & RTE_IPV4_HDR_DSCP_MASK) >> 2;

	if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1 ||
	    flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0) {
		if (dscp != TEST_IPSEC_DSCP_VAL) {
			printf("DSCP value is not matching [exp: %x, actual: %x]\n",
			       TEST_IPSEC_DSCP_VAL, dscp);
			return -1;
		}
	} else {
		if (dscp != 0) {
			printf("DSCP value is set [exp: 0, actual: %x]\n",
			       dscp);
			return -1;
		}
	}

	return 0;
}

static int
test_ipsec_iph6_hdr_validate(const struct rte_ipv6_hdr *iph6,
			     const struct ipsec_test_flags *flags)
{
	uint32_t vtc_flow;
	uint8_t dscp;

	if (!is_valid_ipv6_pkt(iph6)) {
		printf("Tunnel outer header is not IPv6\n");
		return -1;
	}

	vtc_flow = rte_be_to_cpu_32(iph6->vtc_flow);
	dscp = (vtc_flow & RTE_IPV6_HDR_DSCP_MASK) >>
	       (RTE_IPV6_HDR_TC_SHIFT + 2);

	if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1 ||
	    flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0) {
		if (dscp != TEST_IPSEC_DSCP_VAL) {
			printf("DSCP value is not matching [exp: %x, actual: %x]\n",
			       TEST_IPSEC_DSCP_VAL, dscp);
			return -1;
		}
	} else {
		if (dscp != 0) {
			printf("DSCP value is set [exp: 0, actual: %x]\n",
			       dscp);
			return -1;
		}
	}

	return 0;
}

int
test_ipsec_post_process(struct rte_mbuf *m, const struct ipsec_test_data *td,
			struct ipsec_test_data *res_d, bool silent,
			const struct ipsec_test_flags *flags)
{
	uint8_t *output_text = rte_pktmbuf_mtod(m, uint8_t *);
	int ret;

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		const struct rte_ipv4_hdr *iph4;
		const struct rte_ipv6_hdr *iph6;

		if (flags->iv_gen) {
			ret = test_ipsec_iv_verify_push(m, td);
			if (ret != TEST_SUCCESS)
				return ret;
		}

		iph4 = (const struct rte_ipv4_hdr *)output_text;

		if (td->ipsec_xform.mode ==
				RTE_SECURITY_IPSEC_SA_MODE_TRANSPORT) {
			if (flags->ipv6) {
				iph6 = (const struct rte_ipv6_hdr *)output_text;
				if (is_valid_ipv6_pkt(iph6) == false) {
					printf("Transport packet is not IPv6\n");
					return TEST_FAILED;
				}
			} else {
				if (is_valid_ipv4_pkt(iph4) == false) {
					printf("Transport packet is not IPv4\n");
					return TEST_FAILED;
				}
			}
		} else {
			if (td->ipsec_xform.tunnel.type ==
					RTE_SECURITY_IPSEC_TUNNEL_IPV4) {
				if (test_ipsec_iph4_hdr_validate(iph4, flags))
					return TEST_FAILED;
			} else {
				iph6 = (const struct rte_ipv6_hdr *)output_text;
				if (test_ipsec_iph6_hdr_validate(iph6, flags))
					return TEST_FAILED;
			}
		}
	}

	/*
	 * In case of known vector tests & all inbound tests, res_d provided
	 * would be NULL and output data need to be validated against expected.
	 * For inbound, output_text would be plain packet and for outbound
	 * output_text would IPsec packet. Validate by comparing against
	 * known vectors.
	 *
	 * In case of combined mode tests, the output_text from outbound
	 * operation (ie, IPsec packet) would need to be inbound processed to
	 * obtain the plain text. Copy output_text to result data, 'res_d', so
	 * that inbound processing can be done.
	 */

	if (res_d == NULL)
		return test_ipsec_td_verify(m, td, silent, flags);
	else
		return test_ipsec_res_d_prepare(m, td, res_d);
}

int
test_ipsec_status_check(const struct ipsec_test_data *td,
			struct rte_crypto_op *op,
			const struct ipsec_test_flags *flags,
			enum rte_security_ipsec_sa_direction dir,
			int pkt_num)
{
	int ret = TEST_SUCCESS;

	if ((dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
	    td->ar_packet) {
		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			printf("Anti replay test case failed\n");
			return TEST_FAILED;
		} else {
			return TEST_SUCCESS;
		}
	}

	if (dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
	    flags->sa_expiry_pkts_hard &&
	    pkt_num == IPSEC_TEST_PACKETS_MAX) {
		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			printf("SA hard expiry (pkts) test failed\n");
			return TEST_FAILED;
		} else {
			return TEST_SUCCESS;
		}
	}

	if ((dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
	    flags->tunnel_hdr_verify) {
		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			printf("Tunnel header verify test case failed\n");
			return TEST_FAILED;
		} else {
			return TEST_SUCCESS;
		}
	}

	if (dir == RTE_SECURITY_IPSEC_SA_DIR_INGRESS && flags->icv_corrupt) {
		if (op->status != RTE_CRYPTO_OP_STATUS_ERROR) {
			printf("ICV corruption test case failed\n");
			ret = TEST_FAILED;
		}
	} else {
		if (op->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
			printf("Security op processing failed [pkt_num: %d]\n",
			       pkt_num);
			ret = TEST_FAILED;
		}
	}

	if (flags->sa_expiry_pkts_soft && pkt_num == IPSEC_TEST_PACKETS_MAX) {
		if (!(op->aux_flags &
		      RTE_CRYPTO_OP_AUX_FLAGS_IPSEC_SOFT_EXPIRY)) {
			printf("SA soft expiry (pkts) test failed\n");
			ret = TEST_FAILED;
		}
	}

	return ret;
}

int
test_ipsec_stats_verify(struct rte_security_ctx *ctx,
			struct rte_security_session *sess,
			const struct ipsec_test_flags *flags,
			enum rte_security_ipsec_sa_direction dir)
{
	struct rte_security_stats stats = {0};
	int ret = TEST_SUCCESS;

	if (flags->stats_success) {
		if (rte_security_session_stats_get(ctx, sess, &stats) < 0)
			return TEST_FAILED;

		if (dir == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
			if (stats.ipsec.opackets != 1 ||
			    stats.ipsec.oerrors != 0)
				ret = TEST_FAILED;
		} else {
			if (stats.ipsec.ipackets != 1 ||
			    stats.ipsec.ierrors != 0)
				ret = TEST_FAILED;
		}
	}

	return ret;
}

int
test_ipsec_pkt_update(uint8_t *pkt, const struct ipsec_test_flags *flags)
{
	struct rte_ipv4_hdr *iph4;
	struct rte_ipv6_hdr *iph6;
	bool cksum_dirty = false;

	iph4 = (struct rte_ipv4_hdr *)pkt;

	if (flags->df == TEST_IPSEC_COPY_DF_INNER_1 ||
	    flags->df == TEST_IPSEC_SET_DF_0_INNER_1 ||
	    flags->df == TEST_IPSEC_COPY_DF_INNER_0 ||
	    flags->df == TEST_IPSEC_SET_DF_1_INNER_0) {
		uint16_t frag_off;

		if (!is_ipv4(iph4)) {
			printf("Invalid packet type\n");
			return -1;
		}

		frag_off = rte_be_to_cpu_16(iph4->fragment_offset);

		if (flags->df == TEST_IPSEC_COPY_DF_INNER_1 ||
		    flags->df == TEST_IPSEC_SET_DF_0_INNER_1)
			frag_off |= RTE_IPV4_HDR_DF_FLAG;
		else
			frag_off &= ~RTE_IPV4_HDR_DF_FLAG;

		iph4->fragment_offset = rte_cpu_to_be_16(frag_off);
		cksum_dirty = true;
	}

	if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1 ||
	    flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1 ||
	    flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_0 ||
	    flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0) {

		if (is_ipv4(iph4)) {
			uint8_t tos;

			tos = iph4->type_of_service;
			if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1 ||
			    flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				tos |= (RTE_IPV4_HDR_DSCP_MASK &
					(TEST_IPSEC_DSCP_VAL << 2));
			else
				tos &= ~RTE_IPV4_HDR_DSCP_MASK;

			iph4->type_of_service = tos;
			cksum_dirty = true;
		} else {
			uint32_t vtc_flow;

			iph6 = (struct rte_ipv6_hdr *)pkt;

			vtc_flow = rte_be_to_cpu_32(iph6->vtc_flow);
			if (flags->dscp == TEST_IPSEC_COPY_DSCP_INNER_1 ||
			    flags->dscp == TEST_IPSEC_SET_DSCP_0_INNER_1)
				vtc_flow |= (RTE_IPV6_HDR_DSCP_MASK &
					     (TEST_IPSEC_DSCP_VAL << (RTE_IPV6_HDR_TC_SHIFT + 2)));
			else
				vtc_flow &= ~RTE_IPV6_HDR_DSCP_MASK;

			iph6->vtc_flow = rte_cpu_to_be_32(vtc_flow);
		}
	}

	if (cksum_dirty && is_ipv4(iph4)) {
		iph4->hdr_checksum = 0;
		iph4->hdr_checksum = rte_ipv4_cksum(iph4);
	}

	return 0;
}

#endif /* !RTE_EXEC_ENV_WINDOWS */
