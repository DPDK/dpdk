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
#define UDP_CUSTOM_SPORT 4650
#define UDP_CUSTOM_DPORT 4660

#ifndef IPVERSION
#define IPVERSION 4
#endif

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

	if (ipsec_xform->direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) {
		if (ipsec_xform->replay_win_sz > sec_cap->ipsec.replay_win_sz_max) {
			if (!silent)
				RTE_LOG(INFO, USER1,
					"Replay window size is not supported\n");
			return -ENOTSUP;
		}
	}

	if (ipsec_xform->options.ingress_oop == 1 &&
	    sec_cap->ipsec.options.ingress_oop == 0) {
		if (!silent)
			RTE_LOG(INFO, USER1,
				"Inline Ingress OOP processing is not supported\n");
		return -ENOTSUP;
	}

	return 0;
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

			if (param1->alg.aead == RTE_CRYPTO_AEAD_AES_CCM)
				td->salt.len = 3;

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

			if (param1->type == RTE_CRYPTO_SYM_XFORM_AUTH) {
				td->xform.chain.auth.auth.algo =
						param1->alg.auth;
				td->xform.chain.auth.auth.key.length =
						param1->key_length;
				td->xform.chain.auth.auth.digest_length =
						param1->digest_length;
				td->auth_only = true;

				if (td->xform.chain.auth.auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
					td->xform.chain.auth.auth.iv.length =
						param1->iv_length;
					td->aes_gmac = true;
				}
			} else {
				td->xform.chain.cipher.cipher.algo =
						param1->alg.cipher;
				td->xform.chain.cipher.cipher.key.length =
						param1->key_length;
				td->xform.chain.cipher.cipher.iv.length =
						param1->iv_length;
				td->xform.chain.auth.auth.algo =
						param2->alg.auth;
				td->xform.chain.auth.auth.key.length =
						param2->key_length;
				td->xform.chain.auth.auth.digest_length =
						param2->digest_length;

				if (td->xform.chain.auth.auth.algo == RTE_CRYPTO_AUTH_AES_GMAC) {
					td->xform.chain.auth.auth.iv.length =
						param2->iv_length;
					td->aes_gmac = true;
				}
			}
		}

		/* Adjust the data to requested length */
		if (flags->plaintext_len && flags->ipv6) {
			struct rte_ipv6_hdr *ip6 = (struct rte_ipv6_hdr *)td->input_text.data;
			struct rte_tcp_hdr *tcp;
			int64_t payload_len;
			uint8_t *data;
			int64_t i;

			payload_len = RTE_MIN(flags->plaintext_len, IPSEC_TEXT_MAX_LEN);
			payload_len -= sizeof(struct rte_ipv6_hdr);
			payload_len -= sizeof(struct rte_tcp_hdr);
			if (payload_len <= 16)
				payload_len = 16;

			/* IPv6 */
			ip6->proto = IPPROTO_TCP;
			ip6->payload_len = sizeof(*tcp) + payload_len;
			ip6->payload_len = rte_cpu_to_be_16(ip6->payload_len);

			/* TCP */
			tcp = (struct rte_tcp_hdr *)(ip6 + 1);
			data = (uint8_t *)(tcp + 1);
			for (i = 0; i < payload_len; i++)
				data[i] = i;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv6_udptcp_cksum(ip6, tcp);
			td->input_text.len = payload_len + sizeof(struct rte_ipv6_hdr) +
				sizeof(struct rte_tcp_hdr);
		} else if (flags->plaintext_len) {
			struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)td->input_text.data;
			struct rte_tcp_hdr *tcp;
			int64_t payload_len;
			uint8_t *data;
			int64_t i;

			payload_len = RTE_MIN(flags->plaintext_len, IPSEC_TEXT_MAX_LEN);
			payload_len -= sizeof(struct rte_ipv4_hdr);
			payload_len -= sizeof(struct rte_tcp_hdr);
			if (payload_len <= 8)
				payload_len = 8;

			/* IPv4 */
			ip->next_proto_id = IPPROTO_TCP;
			ip->total_length = sizeof(*ip) + sizeof(*tcp) + payload_len;
			ip->total_length = rte_cpu_to_be_16(ip->total_length);
			ip->hdr_checksum = 0;
			ip->hdr_checksum = rte_ipv4_cksum(ip);

			/* TCP */
			tcp = (struct rte_tcp_hdr *)(ip + 1);
			data = (uint8_t *)(tcp + 1);
			for (i = 0; i < payload_len; i++)
				data[i] = i;
			tcp->cksum = 0;
			tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);
			td->input_text.len = payload_len + sizeof(struct rte_ipv4_hdr) +
				sizeof(struct rte_tcp_hdr);
		}

		if (flags->ah) {
			td->ipsec_xform.proto =
					RTE_SECURITY_IPSEC_SA_PROTO_AH;
		}

		if (flags->iv_gen)
			td->ipsec_xform.options.iv_gen_disable = 0;

		if (flags->sa_expiry_pkts_soft)
			td->ipsec_xform.life.packets_soft_limit = TEST_SEC_PKTS_MAX - 1;

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

		if (flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_0 ||
		    flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_1)
			td->ipsec_xform.options.copy_flabel = 1;

		if (flags->dec_ttl_or_hop_limit)
			td->ipsec_xform.options.dec_ttl = 1;

		if (flags->udp_encap && flags->udp_encap_custom_ports) {
			td->ipsec_xform.udp.sport = UDP_CUSTOM_SPORT;
			td->ipsec_xform.udp.dport = UDP_CUSTOM_DPORT;
		}
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
			td_inb[i].ipsec_xform.life.packets_hard_limit = TEST_SEC_PKTS_MAX - 1;

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
test_ipsec_iv_verify_push(const uint8_t *output_text, const struct ipsec_test_data *td)
{
	static uint8_t iv_queue[IV_LEN_MAX * TEST_SEC_PKTS_MAX];
	int i, iv_pos, iv_len;
	static int index;
	uint8_t *iv_tmp;

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

	if (index == TEST_SEC_PKTS_MAX)
		index = 0;

	return TEST_SUCCESS;
}

static int
test_ipsec_l3_csum_verify(uint8_t *output_text)
{
	uint16_t actual_cksum, expected_cksum;
	struct rte_ipv4_hdr *ip;

	ip = (struct rte_ipv4_hdr *)output_text;

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
test_ipsec_l4_csum_verify(uint8_t *output_text)
{
	uint16_t actual_cksum = 0, expected_cksum = 0;
	struct rte_ipv4_hdr *ipv4;
	struct rte_ipv6_hdr *ipv6;
	struct rte_tcp_hdr *tcp;
	struct rte_udp_hdr *udp;
	void *ip, *l4;

	ip = output_text;

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
test_ipsec_td_verify(uint8_t *output_text, uint32_t len, uint32_t ol_flags,
		const struct ipsec_test_data *td, bool silent, const struct ipsec_test_flags *flags)
{
	uint8_t td_output_text[IPSEC_TEXT_MAX_LEN];
	uint32_t skip;
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

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
				flags->ip_csum) {
		if (ol_flags & RTE_MBUF_F_RX_IP_CKSUM_GOOD)
			ret = test_ipsec_l3_csum_verify(output_text);
		else
			ret = TEST_FAILED;

		if (ret == TEST_FAILED)
			printf("Inner IP checksum test failed\n");

		return ret;
	}

	if ((td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_INGRESS) &&
				flags->l4_csum) {
		if (ol_flags & RTE_MBUF_F_RX_L4_CKSUM_GOOD)
			ret = test_ipsec_l4_csum_verify(output_text);
		else
			ret = TEST_FAILED;

		if (ret == TEST_FAILED)
			printf("Inner L4 checksum test failed\n");

		return ret;
	}

	skip = test_ipsec_tunnel_hdr_len_get(td);

	len -= skip;
	output_text += skip;

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
test_ipsec_res_d_prepare(const uint8_t *output_text, uint32_t len,
		const struct ipsec_test_data *td, struct ipsec_test_data *res_d)
{
	memcpy(res_d, td, sizeof(*res_d));

	memcpy(&res_d->input_text.data, output_text, len);
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

	if (flags->ah && iph4->next_proto_id != IPPROTO_AH) {
		printf("Tunnel outer header proto is not AH\n");
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
	uint32_t flabel;
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

	flabel = vtc_flow & RTE_IPV6_HDR_FL_MASK;

	if (flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_1 ||
	    flags->flabel == TEST_IPSEC_SET_FLABEL_1_INNER_0) {
		if (flabel != TEST_IPSEC_FLABEL_VAL) {
			printf("FLABEL value is not matching [exp: %x, actual: %x]\n",
			       TEST_IPSEC_FLABEL_VAL, flabel);
			return -1;
		}
	} else {
		if (flabel != 0) {
			printf("FLABEL value is set [exp: 0, actual: %x]\n",
			       flabel);
			return -1;
		}
	}

	return 0;
}

int
test_ipsec_post_process(const struct rte_mbuf *m, const struct ipsec_test_data *td,
			struct ipsec_test_data *res_d, bool silent,
			const struct ipsec_test_flags *flags)
{
	uint32_t len = rte_pktmbuf_pkt_len(m), data_len;
	uint8_t output_text[IPSEC_TEXT_MAX_LEN];
	const struct rte_mbuf *seg;
	const uint8_t *output;
	int ret;

	memset(output_text, 0, IPSEC_TEXT_MAX_LEN);
	/* Actual data in packet might be less in error cases,
	 * hence take minimum of pkt_len and sum of data_len.
	 * This is done to run through negative test cases.
	 */
	data_len = 0;
	seg = m;
	while (seg) {
		data_len += seg->data_len;
		seg = seg->next;
	}
	len = RTE_MIN(len, data_len);
	TEST_ASSERT(len <= IPSEC_TEXT_MAX_LEN, "Invalid packet length: %u", len);
	/* Copy mbuf payload to continuous buffer */
	output = rte_pktmbuf_read(m, 0, len, output_text);
	if (output != output_text)
		/* Single segment mbuf, copy manually */
		memcpy(output_text, output, len);

	if (td->ipsec_xform.direction == RTE_SECURITY_IPSEC_SA_DIR_EGRESS) {
		const struct rte_ipv4_hdr *iph4;
		const struct rte_ipv6_hdr *iph6;

		if (flags->iv_gen) {
			ret = test_ipsec_iv_verify_push(output_text, td);
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

				if (flags->ah && iph4->next_proto_id != IPPROTO_AH) {
					printf("Transport IPv4 header proto is not AH\n");
					return -1;
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

			if (flags->udp_encap_custom_ports) {
				const struct rte_udp_hdr *udph;

				udph = (const struct rte_udp_hdr *)(output_text +
					sizeof(struct rte_ipv4_hdr));
				if ((rte_be_to_cpu_16(udph->src_port) != UDP_CUSTOM_SPORT) ||
				    (rte_be_to_cpu_16(udph->dst_port) != UDP_CUSTOM_DPORT)) {
					printf("UDP custom ports not matching.\n");
					return TEST_FAILED;
				}
			}
		} else {
			iph6 = (const struct rte_ipv6_hdr *)output_text;

			if (iph6->proto != IPPROTO_UDP) {
				printf("UDP header is not found\n");
				return TEST_FAILED;
			}

			if (flags->udp_encap_custom_ports) {
				const struct rte_udp_hdr *udph;

				udph = (const struct rte_udp_hdr *)(output_text +
					sizeof(struct rte_ipv6_hdr));
				if ((rte_be_to_cpu_16(udph->src_port) != UDP_CUSTOM_SPORT) ||
				    (rte_be_to_cpu_16(udph->dst_port) != UDP_CUSTOM_DPORT)) {
					printf("UDP custom ports not matching.\n");
					return TEST_FAILED;
				}
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
		return test_ipsec_td_verify(output_text, len, m->ol_flags, td, silent, flags);
	else
		return test_ipsec_res_d_prepare(output_text, len, td, res_d);
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
	    pkt_num == TEST_SEC_PKTS_MAX) {
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

	if (flags->sa_expiry_pkts_soft && pkt_num == TEST_SEC_PKTS_MAX) {
		if (!(op->aux_flags &
		      RTE_CRYPTO_OP_AUX_FLAGS_IPSEC_SOFT_EXPIRY)) {
			printf("SA soft expiry (pkts) test failed\n");
			ret = TEST_FAILED;
		}
	}

	return ret;
}

int
test_ipsec_stats_verify(void *ctx,
			void *sess,
			const struct ipsec_test_flags *flags,
			enum rte_security_ipsec_sa_direction dir)
{
	struct rte_security_stats stats = {0};
	int retries = 0, ret = TEST_SUCCESS;

	if (flags->stats_success) {
stats_get:
		ret = TEST_SUCCESS;

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

		if (ret == TEST_FAILED && retries < TEST_STATS_RETRIES) {
			retries++;
			rte_delay_ms(1);
			goto stats_get;
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
	    flags->dscp == TEST_IPSEC_SET_DSCP_1_INNER_0 ||
	    flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_1 ||
	    flags->flabel == TEST_IPSEC_SET_FLABEL_0_INNER_1 ||
	    flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_0 ||
	    flags->flabel == TEST_IPSEC_SET_FLABEL_1_INNER_0) {

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

			if (flags->flabel == TEST_IPSEC_COPY_FLABEL_INNER_1 ||
			    flags->flabel == TEST_IPSEC_SET_FLABEL_0_INNER_1)
				vtc_flow |= (RTE_IPV6_HDR_FL_MASK &
					     (TEST_IPSEC_FLABEL_VAL << RTE_IPV6_HDR_FL_SHIFT));
			else
				vtc_flow &= ~RTE_IPV6_HDR_FL_MASK;

			iph6->vtc_flow = rte_cpu_to_be_32(vtc_flow);
		}
	}

	if (cksum_dirty && is_ipv4(iph4)) {
		iph4->hdr_checksum = 0;
		iph4->hdr_checksum = rte_ipv4_cksum(iph4);
	}

	return 0;
}
