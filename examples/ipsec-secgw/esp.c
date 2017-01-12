/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
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
	sym_cop->cipher.data.offset =  ip_hdr_len + sizeof(struct esp_hdr) +
		sa->iv_len;
	sym_cop->cipher.data.length = payload_len;

	struct cnt_blk *icb;
	uint8_t *aad;
	uint8_t *iv = RTE_PTR_ADD(ip4, ip_hdr_len + sizeof(struct esp_hdr));

	switch (sa->cipher_algo) {
	case RTE_CRYPTO_CIPHER_NULL:
	case RTE_CRYPTO_CIPHER_AES_CBC:
		sym_cop->cipher.iv.data = iv;
		sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
				 ip_hdr_len + sizeof(struct esp_hdr));
		sym_cop->cipher.iv.length = sa->iv_len;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
	case RTE_CRYPTO_CIPHER_AES_GCM:
		icb = get_cnt_blk(m);
		icb->salt = sa->salt;
		memcpy(&icb->iv, iv, 8);
		icb->cnt = rte_cpu_to_be_32(1);
		sym_cop->cipher.iv.data = (uint8_t *)icb;
		sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			 (uint8_t *)icb - rte_pktmbuf_mtod(m, uint8_t *));
		sym_cop->cipher.iv.length = 16;
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
	case RTE_CRYPTO_AUTH_AES_GCM:
		aad = get_aad(m);
		memcpy(aad, iv - sizeof(struct esp_hdr), 8);
		sym_cop->auth.aad.data = aad;
		sym_cop->auth.aad.phys_addr = rte_pktmbuf_mtophys_offset(m,
				aad - rte_pktmbuf_mtod(m, uint8_t *));
		sym_cop->auth.aad.length = 8;
		break;
	default:
		RTE_LOG(ERR, IPSEC_ESP, "unsupported auth algorithm %u\n",
				sa->auth_algo);
		return -EINVAL;
	}

	sym_cop->auth.digest.data = rte_pktmbuf_mtod_offset(m, void*,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
	sym_cop->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(m,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
	sym_cop->auth.digest.length = sa->digest_len;

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

	if (cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, IPSEC_ESP, "failed crypto op\n");
		return -1;
	}

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
			ip6->ip6_plen = htons(rte_pktmbuf_data_len(m));
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
	uint8_t *padding, *new_ip, nlp;
	struct rte_crypto_sym_op *sym_cop;
	int32_t i;
	uint16_t pad_payload_len, pad_len, ip_hdr_len;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

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

	padding = (uint8_t *)rte_pktmbuf_append(m, pad_len + sa->digest_len);
	if (unlikely(padding == NULL)) {
		RTE_LOG(ERR, IPSEC_ESP, "not enough mbuf trailing space\n");
		return -ENOSPC;
	}
	rte_prefetch0(padding);

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
		if (likely(ip4->ip_v == IPVERSION)) {
			ip4 = (struct ip *)new_ip;
			ip4->ip_p = IPPROTO_ESP;
			ip4->ip_len = htons(rte_pktmbuf_data_len(m));
		} else {
			ip6 = (struct ip6_hdr *)new_ip;
			ip6->ip6_nxt = IPPROTO_ESP;
			ip6->ip6_plen = htons(rte_pktmbuf_data_len(m));
		}
	}

	sa->seq++;
	esp->spi = rte_cpu_to_be_32(sa->spi);
	esp->seq = rte_cpu_to_be_32((uint32_t)sa->seq);

	uint64_t *iv = (uint64_t *)(esp + 1);

	sym_cop = get_sym_cop(cop);
	sym_cop->m_src = m;
	switch (sa->cipher_algo) {
	case RTE_CRYPTO_CIPHER_NULL:
	case RTE_CRYPTO_CIPHER_AES_CBC:
		memset(iv, 0, sa->iv_len);
		sym_cop->cipher.data.offset = ip_hdr_len +
			sizeof(struct esp_hdr);
		sym_cop->cipher.data.length = pad_payload_len + sa->iv_len;
		break;
	case RTE_CRYPTO_CIPHER_AES_CTR:
	case RTE_CRYPTO_CIPHER_AES_GCM:
		*iv = sa->seq;
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
	icb->iv = sa->seq;
	icb->cnt = rte_cpu_to_be_32(1);
	sym_cop->cipher.iv.data = (uint8_t *)icb;
	sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			 (uint8_t *)icb - rte_pktmbuf_mtod(m, uint8_t *));
	sym_cop->cipher.iv.length = 16;

	uint8_t *aad;

	switch (sa->auth_algo) {
	case RTE_CRYPTO_AUTH_NULL:
	case RTE_CRYPTO_AUTH_SHA1_HMAC:
	case RTE_CRYPTO_AUTH_SHA256_HMAC:
		sym_cop->auth.data.offset = ip_hdr_len;
		sym_cop->auth.data.length = sizeof(struct esp_hdr) +
			sa->iv_len + pad_payload_len;
		break;
	case RTE_CRYPTO_AUTH_AES_GCM:
		aad = get_aad(m);
		memcpy(aad, esp, 8);
		sym_cop->auth.aad.data = aad;
		sym_cop->auth.aad.phys_addr = rte_pktmbuf_mtophys_offset(m,
				aad - rte_pktmbuf_mtod(m, uint8_t *));
		sym_cop->auth.aad.length = 8;
		break;
	default:
		RTE_LOG(ERR, IPSEC_ESP, "unsupported auth algorithm %u\n",
				sa->auth_algo);
		return -EINVAL;
	}

	sym_cop->auth.digest.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
	sym_cop->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(m,
			rte_pktmbuf_pkt_len(m) - sa->digest_len);
	sym_cop->auth.digest.length = sa->digest_len;

	return 0;
}

int
esp_outbound_post(struct rte_mbuf *m __rte_unused,
		struct ipsec_sa *sa __rte_unused,
		struct rte_crypto_op *cop)
{
	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	if (cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, IPSEC_ESP, "Failed crypto op\n");
		return -1;
	}

	return 0;
}
