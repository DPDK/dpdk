/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <fcntl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_random.h>

#include "ipsec.h"
#include "esp.h"
#include "ipip.h"

int
esp_inbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	struct ip *ip4;
	struct rte_crypto_sym_op *sym_cop;
	int32_t payload_len, ip_hdr_len;

	RTE_ASSERT(sa != NULL);
	if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO)
		return 0;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(cop != NULL);

	ip4 = rte_pktmbuf_mtod(m, struct ip *);
	if (likely(ip4->ip_v == IPVERSION))
		ip_hdr_len = ip4->ip_hl * 4;
	else if (ip4->ip_v == IP6_VERSION)
		/* XXX No option headers supported */
		ip_hdr_len = sizeof(struct ip6_hdr);
	else {
		RTE_LOG(ERR, IPSEC_ESP, "invalid IP packet type %d\n",
				ip4->ip_v);
		return -EINVAL;
	}

	payload_len = rte_pktmbuf_pkt_len(m) - ip_hdr_len -
		sizeof(struct esp_hdr) - sa->iv_len - sa->digest_len;

	if ((payload_len & (sa->block_size - 1)) || (payload_len <= 0)) {
		RTE_LOG_DP(DEBUG, IPSEC_ESP, "payload %d not multiple of %u\n",
				payload_len, sa->block_size);
		return -EINVAL;
	}

	sym_cop = get_sym_cop(cop);
	sym_cop->m_src = m;

	if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
		sym_cop->aead.data.offset =  ip_hdr_len + sizeof(struct esp_hdr) +
			sa->iv_len;
		sym_cop->aead.data.length = payload_len;

		struct cnt_blk *icb;
		uint8_t *aad;
		uint8_t *iv = RTE_PTR_ADD(ip4, ip_hdr_len + sizeof(struct esp_hdr));

		icb = get_cnt_blk(m);
		icb->salt = sa->salt;
		memcpy(&icb->iv, iv, 8);
		icb->cnt = rte_cpu_to_be_32(1);

		aad = get_aad(m);
		memcpy(aad, iv - sizeof(struct esp_hdr), 8);
		sym_cop->aead.aad.data = aad;
		sym_cop->aead.aad.phys_addr = rte_pktmbuf_iova_offset(m,
				aad - rte_pktmbuf_mtod(m, uint8_t *));

		sym_cop->aead.digest.data = rte_pktmbuf_mtod_offset(m, void*,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
		sym_cop->aead.digest.phys_addr = rte_pktmbuf_iova_offset(m,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
	} else {
		sym_cop->cipher.data.offset =  ip_hdr_len + sizeof(struct esp_hdr) +
			sa->iv_len;
		sym_cop->cipher.data.length = payload_len;

		struct cnt_blk *icb;
		uint8_t *iv = RTE_PTR_ADD(ip4, ip_hdr_len + sizeof(struct esp_hdr));
		uint8_t *iv_ptr = rte_crypto_op_ctod_offset(cop,
					uint8_t *, IV_OFFSET);

		switch (sa->cipher_algo) {
		case RTE_CRYPTO_CIPHER_NULL:
		case RTE_CRYPTO_CIPHER_3DES_CBC:
		case RTE_CRYPTO_CIPHER_AES_CBC:
			/* Copy IV at the end of crypto operation */
			rte_memcpy(iv_ptr, iv, sa->iv_len);
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			icb = get_cnt_blk(m);
			icb->salt = sa->salt;
			memcpy(&icb->iv, iv, 8);
			icb->cnt = rte_cpu_to_be_32(1);
			break;
		default:
			RTE_LOG(ERR, IPSEC_ESP, "unsupported cipher algorithm %u\n",
					sa->cipher_algo);
			return -EINVAL;
		}

		switch (sa->auth_algo) {
		case RTE_CRYPTO_AUTH_NULL:
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			sym_cop->auth.data.offset = ip_hdr_len;
			sym_cop->auth.data.length = sizeof(struct esp_hdr) +
				sa->iv_len + payload_len;
			break;
		default:
			RTE_LOG(ERR, IPSEC_ESP, "unsupported auth algorithm %u\n",
					sa->auth_algo);
			return -EINVAL;
		}

		sym_cop->auth.digest.data = rte_pktmbuf_mtod_offset(m, void*,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
		sym_cop->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
	}

	return 0;
}

int
esp_inbound_post(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	struct ip *ip4, *ip;
	struct ip6_hdr *ip6;
	uint8_t *nexthdr, *pad_len;
	uint8_t *padding;
	uint16_t i;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	if ((sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) ||
			(sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO)) {
		if (m->ol_flags & PKT_RX_SEC_OFFLOAD) {
			if (m->ol_flags & PKT_RX_SEC_OFFLOAD_FAILED)
				cop->status = RTE_CRYPTO_OP_STATUS_ERROR;
			else
				cop->status = RTE_CRYPTO_OP_STATUS_SUCCESS;
		} else
			cop->status = RTE_CRYPTO_OP_STATUS_NOT_PROCESSED;
	}

	if (cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, IPSEC_ESP, "%s() failed crypto op\n", __func__);
		return -1;
	}

	if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO &&
	    sa->ol_flags & RTE_SECURITY_RX_HW_TRAILER_OFFLOAD) {
		nexthdr = &m->inner_esp_next_proto;
	} else {
		nexthdr = rte_pktmbuf_mtod_offset(m, uint8_t*,
				rte_pktmbuf_pkt_len(m) - sa->digest_len - 1);
		pad_len = nexthdr - 1;

		padding = pad_len - *pad_len;
		for (i = 0; i < *pad_len; i++) {
			if (padding[i] != i + 1) {
				RTE_LOG(ERR, IPSEC_ESP, "invalid padding\n");
				return -EINVAL;
			}
		}

		if (rte_pktmbuf_trim(m, *pad_len + 2 + sa->digest_len)) {
			RTE_LOG(ERR, IPSEC_ESP,
					"failed to remove pad_len + digest\n");
			return -EINVAL;
		}
	}

	if (unlikely(sa->flags == TRANSPORT)) {
		ip = rte_pktmbuf_mtod(m, struct ip *);
		ip4 = (struct ip *)rte_pktmbuf_adj(m,
				sizeof(struct esp_hdr) + sa->iv_len);
		if (likely(ip->ip_v == IPVERSION)) {
			memmove(ip4, ip, ip->ip_hl * 4);
			ip4->ip_p = *nexthdr;
			ip4->ip_len = htons(rte_pktmbuf_data_len(m));
		} else {
			ip6 = (struct ip6_hdr *)ip4;
			/* XXX No option headers supported */
			memmove(ip6, ip, sizeof(struct ip6_hdr));
			ip6->ip6_nxt = *nexthdr;
			ip6->ip6_plen = htons(rte_pktmbuf_data_len(m) -
					      sizeof(struct ip6_hdr));
		}
	} else
		ipip_inbound(m, sizeof(struct esp_hdr) + sa->iv_len);

	return 0;
}

int
esp_outbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	struct ip *ip4;
	struct ip6_hdr *ip6;
	struct esp_hdr *esp = NULL;
	uint8_t *padding = NULL, *new_ip, nlp;
	struct rte_crypto_sym_op *sym_cop;
	int32_t i;
	uint16_t pad_payload_len, pad_len, ip_hdr_len;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);

	ip_hdr_len = 0;

	ip4 = rte_pktmbuf_mtod(m, struct ip *);
	if (likely(ip4->ip_v == IPVERSION)) {
		if (unlikely(sa->flags == TRANSPORT)) {
			ip_hdr_len = ip4->ip_hl * 4;
			nlp = ip4->ip_p;
		} else
			nlp = IPPROTO_IPIP;
	} else if (ip4->ip_v == IP6_VERSION) {
		if (unlikely(sa->flags == TRANSPORT)) {
			/* XXX No option headers supported */
			ip_hdr_len = sizeof(struct ip6_hdr);
			ip6 = (struct ip6_hdr *)ip4;
			nlp = ip6->ip6_nxt;
		} else
			nlp = IPPROTO_IPV6;
	} else {
		RTE_LOG(ERR, IPSEC_ESP, "invalid IP packet type %d\n",
				ip4->ip_v);
		return -EINVAL;
	}

	/* Padded payload length */
	pad_payload_len = RTE_ALIGN_CEIL(rte_pktmbuf_pkt_len(m) -
			ip_hdr_len + 2, sa->block_size);
	pad_len = pad_payload_len + ip_hdr_len - rte_pktmbuf_pkt_len(m);

	RTE_ASSERT(sa->flags == IP4_TUNNEL || sa->flags == IP6_TUNNEL ||
			sa->flags == TRANSPORT);

	if (likely(sa->flags == IP4_TUNNEL))
		ip_hdr_len = sizeof(struct ip);
	else if (sa->flags == IP6_TUNNEL)
		ip_hdr_len = sizeof(struct ip6_hdr);
	else if (sa->flags != TRANSPORT) {
		RTE_LOG(ERR, IPSEC_ESP, "Unsupported SA flags: 0x%x\n",
				sa->flags);
		return -EINVAL;
	}

	/* Check maximum packet size */
	if (unlikely(ip_hdr_len + sizeof(struct esp_hdr) + sa->iv_len +
			pad_payload_len + sa->digest_len > IP_MAXPACKET)) {
		RTE_LOG(ERR, IPSEC_ESP, "ipsec packet is too big\n");
		return -EINVAL;
	}

	/* Add trailer padding if it is not constructed by HW */
	if (sa->type != RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO ||
	    (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO &&
	     !(sa->ol_flags & RTE_SECURITY_TX_HW_TRAILER_OFFLOAD))) {
		padding = (uint8_t *)rte_pktmbuf_append(m, pad_len +
							sa->digest_len);
		if (unlikely(padding == NULL)) {
			RTE_LOG(ERR, IPSEC_ESP,
					"not enough mbuf trailing space\n");
			return -ENOSPC;
		}
		rte_prefetch0(padding);
	}

	switch (sa->flags) {
	case IP4_TUNNEL:
		ip4 = ip4ip_outbound(m, sizeof(struct esp_hdr) + sa->iv_len,
				&sa->src, &sa->dst);
		esp = (struct esp_hdr *)(ip4 + 1);
		break;
	case IP6_TUNNEL:
		ip6 = ip6ip_outbound(m, sizeof(struct esp_hdr) + sa->iv_len,
				&sa->src, &sa->dst);
		esp = (struct esp_hdr *)(ip6 + 1);
		break;
	case TRANSPORT:
		new_ip = (uint8_t *)rte_pktmbuf_prepend(m,
				sizeof(struct esp_hdr) + sa->iv_len);
		memmove(new_ip, ip4, ip_hdr_len);
		esp = (struct esp_hdr *)(new_ip + ip_hdr_len);
		ip4 = (struct ip *)new_ip;
		if (likely(ip4->ip_v == IPVERSION)) {
			ip4->ip_p = IPPROTO_ESP;
			ip4->ip_len = htons(rte_pktmbuf_data_len(m));
		} else {
			ip6 = (struct ip6_hdr *)new_ip;
			ip6->ip6_nxt = IPPROTO_ESP;
			ip6->ip6_plen = htons(rte_pktmbuf_data_len(m) -
					      sizeof(struct ip6_hdr));
		}
	}

	sa->seq++;
	esp->spi = rte_cpu_to_be_32(sa->spi);
	esp->seq = rte_cpu_to_be_32((uint32_t)sa->seq);

	/* set iv */
	uint64_t *iv = (uint64_t *)(esp + 1);
	if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
		*iv = rte_cpu_to_be_64(sa->seq);
	} else {
		switch (sa->cipher_algo) {
		case RTE_CRYPTO_CIPHER_NULL:
		case RTE_CRYPTO_CIPHER_3DES_CBC:
		case RTE_CRYPTO_CIPHER_AES_CBC:
			memset(iv, 0, sa->iv_len);
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			*iv = rte_cpu_to_be_64(sa->seq);
			break;
		default:
			RTE_LOG(ERR, IPSEC_ESP,
				"unsupported cipher algorithm %u\n",
				sa->cipher_algo);
			return -EINVAL;
		}
	}

	if (sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO) {
		if (sa->ol_flags & RTE_SECURITY_TX_HW_TRAILER_OFFLOAD) {
			/* Set the inner esp next protocol for HW trailer */
			m->inner_esp_next_proto = nlp;
			m->packet_type |= RTE_PTYPE_TUNNEL_ESP;
		} else {
			padding[pad_len - 2] = pad_len - 2;
			padding[pad_len - 1] = nlp;
		}
		goto done;
	}

	RTE_ASSERT(cop != NULL);
	sym_cop = get_sym_cop(cop);
	sym_cop->m_src = m;

	if (sa->aead_algo == RTE_CRYPTO_AEAD_AES_GCM) {
		uint8_t *aad;

		sym_cop->aead.data.offset = ip_hdr_len +
			sizeof(struct esp_hdr) + sa->iv_len;
		sym_cop->aead.data.length = pad_payload_len;

		/* Fill pad_len using default sequential scheme */
		for (i = 0; i < pad_len - 2; i++)
			padding[i] = i + 1;
		padding[pad_len - 2] = pad_len - 2;
		padding[pad_len - 1] = nlp;

		struct cnt_blk *icb = get_cnt_blk(m);
		icb->salt = sa->salt;
		icb->iv = rte_cpu_to_be_64(sa->seq);
		icb->cnt = rte_cpu_to_be_32(1);

		aad = get_aad(m);
		memcpy(aad, esp, 8);
		sym_cop->aead.aad.data = aad;
		sym_cop->aead.aad.phys_addr = rte_pktmbuf_iova_offset(m,
				aad - rte_pktmbuf_mtod(m, uint8_t *));

		sym_cop->aead.digest.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
		sym_cop->aead.digest.phys_addr = rte_pktmbuf_iova_offset(m,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
	} else {
		switch (sa->cipher_algo) {
		case RTE_CRYPTO_CIPHER_NULL:
		case RTE_CRYPTO_CIPHER_3DES_CBC:
		case RTE_CRYPTO_CIPHER_AES_CBC:
			sym_cop->cipher.data.offset = ip_hdr_len +
				sizeof(struct esp_hdr);
			sym_cop->cipher.data.length = pad_payload_len + sa->iv_len;
			break;
		case RTE_CRYPTO_CIPHER_AES_CTR:
			sym_cop->cipher.data.offset = ip_hdr_len +
				sizeof(struct esp_hdr) + sa->iv_len;
			sym_cop->cipher.data.length = pad_payload_len;
			break;
		default:
			RTE_LOG(ERR, IPSEC_ESP, "unsupported cipher algorithm %u\n",
					sa->cipher_algo);
			return -EINVAL;
		}

		/* Fill pad_len using default sequential scheme */
		for (i = 0; i < pad_len - 2; i++)
			padding[i] = i + 1;
		padding[pad_len - 2] = pad_len - 2;
		padding[pad_len - 1] = nlp;

		struct cnt_blk *icb = get_cnt_blk(m);
		icb->salt = sa->salt;
		icb->iv = rte_cpu_to_be_64(sa->seq);
		icb->cnt = rte_cpu_to_be_32(1);

		switch (sa->auth_algo) {
		case RTE_CRYPTO_AUTH_NULL:
		case RTE_CRYPTO_AUTH_SHA1_HMAC:
		case RTE_CRYPTO_AUTH_SHA256_HMAC:
			sym_cop->auth.data.offset = ip_hdr_len;
			sym_cop->auth.data.length = sizeof(struct esp_hdr) +
				sa->iv_len + pad_payload_len;
			break;
		default:
			RTE_LOG(ERR, IPSEC_ESP, "unsupported auth algorithm %u\n",
					sa->auth_algo);
			return -EINVAL;
		}

		sym_cop->auth.digest.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
		sym_cop->auth.digest.phys_addr = rte_pktmbuf_iova_offset(m,
				rte_pktmbuf_pkt_len(m) - sa->digest_len);
	}

done:
	return 0;
}

int
esp_outbound_post(struct rte_mbuf *m,
		  struct ipsec_sa *sa,
		  struct rte_crypto_op *cop)
{
	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);

	if ((sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_PROTOCOL) ||
			(sa->type == RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO)) {
		m->ol_flags |= PKT_TX_SEC_OFFLOAD;
	} else {
		RTE_ASSERT(cop != NULL);
		if (cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
			RTE_LOG(ERR, IPSEC_ESP, "%s() failed crypto op\n",
				__func__);
			return -1;
		}
	}

	return 0;
}
