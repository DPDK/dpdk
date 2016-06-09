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
#include <rte_memcpy.h>
#include <rte_crypto.h>
#include <rte_cryptodev.h>
#include <rte_random.h>

#include "ipsec.h"
#include "esp.h"
#include "ipip.h"

static inline void
random_iv_u64(uint64_t *buf, uint16_t n)
{
	uint32_t left = n & 0x7;
	uint32_t i;

	RTE_ASSERT((n & 0x3) == 0);

	for (i = 0; i < (n >> 3); i++)
		buf[i] = rte_rand();

	if (left)
		*((uint32_t *)&buf[i]) = (uint32_t)lrand48();
}

int
esp_inbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	int32_t payload_len, ip_hdr_len;
	struct rte_crypto_sym_op *sym_cop;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	ip_hdr_len = 0;
	switch (sa->flags) {
	case IP4_TUNNEL:
		ip_hdr_len = sizeof(struct ip);
		break;
	case IP6_TUNNEL:
		ip_hdr_len = sizeof(struct ip6_hdr);
		break;
	}

	payload_len = rte_pktmbuf_pkt_len(m) - ip_hdr_len -
		sizeof(struct esp_hdr) - sa->iv_len - sa->digest_len;

	if ((payload_len & (sa->block_size - 1)) || (payload_len <= 0)) {
		RTE_LOG(DEBUG, IPSEC_ESP, "payload %d not multiple of %u\n",
				payload_len, sa->block_size);
		return -EINVAL;
	}

	sym_cop = (struct rte_crypto_sym_op *)(cop + 1);

	sym_cop->m_src = m;
	sym_cop->cipher.data.offset =  ip_hdr_len + sizeof(struct esp_hdr) +
		sa->iv_len;
	sym_cop->cipher.data.length = payload_len;

	sym_cop->cipher.iv.data = rte_pktmbuf_mtod_offset(m, void*,
			 ip_hdr_len + sizeof(struct esp_hdr));
	sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			 ip_hdr_len + sizeof(struct esp_hdr));
	sym_cop->cipher.iv.length = sa->iv_len;

	sym_cop->auth.data.offset = ip_hdr_len;
	sym_cop->auth.data.length = sizeof(struct esp_hdr) +
		sa->iv_len + payload_len;

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
	uint8_t *nexthdr, *pad_len;
	uint8_t *padding;
	uint16_t i;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	if (cop->status != RTE_CRYPTO_OP_STATUS_SUCCESS) {
		RTE_LOG(ERR, IPSEC_ESP, "Failed crypto op\n");
		return -1;
	}

	nexthdr = rte_pktmbuf_mtod_offset(m, uint8_t*,
			rte_pktmbuf_pkt_len(m) - sa->digest_len - 1);
	pad_len = nexthdr - 1;

	padding = pad_len - *pad_len;
	for (i = 0; i < *pad_len; i++) {
		if (padding[i] != i + 1) {
			RTE_LOG(ERR, IPSEC_ESP, "invalid pad_len field\n");
			return -EINVAL;
		}
	}

	if (rte_pktmbuf_trim(m, *pad_len + 2 + sa->digest_len)) {
		RTE_LOG(ERR, IPSEC_ESP,
				"failed to remove pad_len + digest\n");
		return -EINVAL;
	}

	ipip_inbound(m, sizeof(struct esp_hdr) + sa->iv_len);

	return 0;
}

int
esp_outbound(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	uint16_t pad_payload_len, pad_len, ip_hdr_len;
	struct ip *ip4;
	struct ip6_hdr *ip6;
	struct esp_hdr *esp;
	int32_t i;
	char *padding;
	struct rte_crypto_sym_op *sym_cop;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	/* Payload length */
	pad_payload_len = RTE_ALIGN_CEIL(rte_pktmbuf_pkt_len(m) + 2,
			sa->block_size);
	pad_len = pad_payload_len - rte_pktmbuf_pkt_len(m);

	ip_hdr_len = 0;
	switch (sa->flags) {
	case IP4_TUNNEL:
		ip_hdr_len = sizeof(struct ip);
		break;
	case IP6_TUNNEL:
		ip_hdr_len = sizeof(struct ip6_hdr);
		break;
	}

	/* Check maximum packet size */
	if (unlikely(ip_hdr_len + sizeof(struct esp_hdr) + sa->iv_len +
			pad_payload_len + sa->digest_len > IP_MAXPACKET)) {
		RTE_LOG(ERR, IPSEC_ESP, "ipsec packet is too big\n");
		return -EINVAL;
	}

	padding = rte_pktmbuf_append(m, pad_len + sa->digest_len);
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
	default:
		RTE_LOG(ERR, IPSEC_ESP, "Unsupported SA flags: 0x%x\n",
				sa->flags);
		return -EINVAL;
	}

	sa->seq++;
	esp->spi = rte_cpu_to_be_32(sa->spi);
	esp->seq = rte_cpu_to_be_32(sa->seq);

	if (sa->cipher_algo == RTE_CRYPTO_CIPHER_AES_CBC)
		random_iv_u64((uint64_t *)(esp + 1), sa->iv_len);

	/* Fill pad_len using default sequential scheme */
	for (i = 0; i < pad_len - 2; i++)
		padding[i] = i + 1;
	padding[pad_len - 2] = pad_len - 2;

	if (RTE_ETH_IS_IPV4_HDR(m->packet_type))
		padding[pad_len - 1] = IPPROTO_IPIP;
	else
		padding[pad_len - 1] = IPPROTO_IPV6;

	sym_cop = (struct rte_crypto_sym_op *)(cop + 1);

	sym_cop->m_src = m;
	sym_cop->cipher.data.offset = ip_hdr_len + sizeof(struct esp_hdr) +
			sa->iv_len;
	sym_cop->cipher.data.length = pad_payload_len;

	sym_cop->cipher.iv.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
			 ip_hdr_len + sizeof(struct esp_hdr));
	sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			 ip_hdr_len + sizeof(struct esp_hdr));
	sym_cop->cipher.iv.length = sa->iv_len;

	sym_cop->auth.data.offset = ip_hdr_len;
	sym_cop->auth.data.length = sizeof(struct esp_hdr) + sa->iv_len +
		pad_payload_len;

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
