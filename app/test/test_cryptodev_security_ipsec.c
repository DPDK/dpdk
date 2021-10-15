/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

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

extern struct ipsec_test_data pkt_aes_256_gcm;

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
		/* Copy template for packet & key fields */
		memcpy(td, &pkt_aes_256_gcm, sizeof(*td));

		/* Override fields based on param */

		if (param1->type == RTE_CRYPTO_SYM_XFORM_AEAD)
			td->aead = true;
		else
			td->aead = false;

		td->xform.aead.aead.algo = param1->alg.aead;
		td->xform.aead.aead.key.length = param1->key_length;

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

	}

	RTE_SET_USED(param2);
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
	if (param1->type == RTE_CRYPTO_SYM_XFORM_AEAD)
		printf("\t%s [%d]\n",
		       rte_crypto_aead_algorithm_strings[param1->alg.aead],
		       param1->key_length);

	RTE_SET_USED(param2);
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
test_ipsec_td_verify(struct rte_mbuf *m, const struct ipsec_test_data *td,
		     bool silent, const struct ipsec_test_flags *flags)
{
	uint8_t *output_text = rte_pktmbuf_mtod(m, uint8_t *);
	uint32_t skip, len = rte_pktmbuf_pkt_len(m);
	int ret;

	/* For tests with status as error for test success, skip verification */
	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS &&
	    (flags->icv_corrupt ||
	     flags->sa_expiry_pkts_hard ||
	     flags->tunnel_hdr_verify))
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


	if (memcmp(output_text, td->output_text.data + skip, len)) {
		if (silent)
			return TEST_FAILED;

		printf("TestCase %s line %d: %s\n", __func__, __LINE__,
			"output text not as expected\n");

		rte_hexdump(stdout, "expected", td->output_text.data + skip,
			    len);
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
		printf("Only AEAD supported\n");
		return TEST_SKIPPED;
	}

	return TEST_SUCCESS;
}

int
test_ipsec_post_process(struct rte_mbuf *m, const struct ipsec_test_data *td,
			struct ipsec_test_data *res_d, bool silent,
			const struct ipsec_test_flags *flags)
{
	int ret;

	if (flags->iv_gen &&
	    td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		ret = test_ipsec_iv_verify_push(m, td);
		if (ret != TEST_SUCCESS)
			return ret;
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
test_ipsec_status_check(struct rte_crypto_op *op,
			const struct ipsec_test_flags *flags,
			enum rte_security_ipsec_sa_direction dir,
			int pkt_num)
{
	int ret = TEST_SUCCESS;

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
