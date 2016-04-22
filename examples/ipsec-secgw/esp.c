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

#define IP_ESP_HDR_SZ (sizeof(struct ip) + sizeof(struct esp_hdr))

static inline void
random_iv_u64(uint64_t *buf, uint16_t n)
{
	unsigned left = n & 0x7;
	unsigned i;

	RTE_ASSERT((n & 0x3) == 0);

	for (i = 0; i < (n >> 3); i++)
		buf[i] = rte_rand();

	if (left)
		*((uint32_t *)&buf[i]) = (uint32_t)lrand48();
}

/* IPv4 Tunnel */
int
esp4_tunnel_inbound_pre_crypto(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	int32_t payload_len;
	struct rte_crypto_sym_op *sym_cop;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	payload_len = rte_pktmbuf_pkt_len(m) - IP_ESP_HDR_SZ - sa->iv_len -
		sa->digest_len;

	if ((payload_len & (sa->block_size - 1)) || (payload_len <= 0)) {
		RTE_LOG(DEBUG, IPSEC_ESP, "payload %d not multiple of %u\n",
				payload_len, sa->block_size);
		return -EINVAL;
	}

	sym_cop = (struct rte_crypto_sym_op *)(cop + 1);

	sym_cop->m_src = m;
	sym_cop->cipher.data.offset = IP_ESP_HDR_SZ + sa->iv_len;
	sym_cop->cipher.data.length = payload_len;

	sym_cop->cipher.iv.data = rte_pktmbuf_mtod_offset(m, void*,
			IP_ESP_HDR_SZ);
	sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			IP_ESP_HDR_SZ);
	sym_cop->cipher.iv.length = sa->iv_len;

	sym_cop->auth.data.offset = sizeof(struct ip);
	if (sa->auth_algo == RTE_CRYPTO_AUTH_AES_GCM)
		sym_cop->auth.data.length = sizeof(struct esp_hdr);
	else
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
esp4_tunnel_inbound_post_crypto(struct rte_mbuf *m, struct ipsec_sa *sa,
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
		if (padding[i] != i) {
			RTE_LOG(ERR, IPSEC_ESP, "invalid pad_len field\n");
			return -EINVAL;
		}
	}

	if (rte_pktmbuf_trim(m, *pad_len + 2 + sa->digest_len)) {
		RTE_LOG(ERR, IPSEC_ESP,
				"failed to remove pad_len + digest\n");
		return -EINVAL;
	}

	return ip4ip_inbound(m, sizeof(struct esp_hdr) + sa->iv_len);
}

int
esp4_tunnel_outbound_pre_crypto(struct rte_mbuf *m, struct ipsec_sa *sa,
		struct rte_crypto_op *cop)
{
	uint16_t pad_payload_len, pad_len;
	struct ip *ip;
	struct esp_hdr *esp;
	int i;
	char *padding;
	struct rte_crypto_sym_op *sym_cop;

	RTE_ASSERT(m != NULL);
	RTE_ASSERT(sa != NULL);
	RTE_ASSERT(cop != NULL);

	/* Payload length */
	pad_payload_len = RTE_ALIGN_CEIL(rte_pktmbuf_pkt_len(m) + 2,
			sa->block_size);
	pad_len = pad_payload_len - rte_pktmbuf_pkt_len(m);

	rte_prefetch0(rte_pktmbuf_mtod_offset(m, void *,
				rte_pktmbuf_pkt_len(m)));

	/* Check maximum packet size */
	if (unlikely(IP_ESP_HDR_SZ + sa->iv_len + pad_payload_len +
				sa->digest_len > IP_MAXPACKET)) {
		RTE_LOG(DEBUG, IPSEC_ESP, "ipsec packet is too big\n");
		return -EINVAL;
	}

	padding = rte_pktmbuf_append(m, pad_len + sa->digest_len);

	RTE_ASSERT(padding != NULL);

	ip = ip4ip_outbound(m, sizeof(struct esp_hdr) + sa->iv_len,
			sa->src, sa->dst);

	esp = (struct esp_hdr *)(ip + 1);
	esp->spi = sa->spi;
	esp->seq = htonl(sa->seq++);

	RTE_LOG(DEBUG, IPSEC_ESP, "pktlen %u\n", rte_pktmbuf_pkt_len(m));

	/* Fill pad_len using default sequential scheme */
	for (i = 0; i < pad_len - 2; i++)
		padding[i] = i + 1;

	padding[pad_len - 2] = pad_len - 2;
	padding[pad_len - 1] = IPPROTO_IPIP;

	sym_cop = (struct rte_crypto_sym_op *)(cop + 1);

	sym_cop->m_src = m;
	sym_cop->cipher.data.offset = IP_ESP_HDR_SZ + sa->iv_len;
	sym_cop->cipher.data.length = pad_payload_len;

	sym_cop->cipher.iv.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
			IP_ESP_HDR_SZ);
	sym_cop->cipher.iv.phys_addr = rte_pktmbuf_mtophys_offset(m,
			IP_ESP_HDR_SZ);
	sym_cop->cipher.iv.length = sa->iv_len;

	sym_cop->auth.data.offset = sizeof(struct ip);
	sym_cop->auth.data.length = sizeof(struct esp_hdr) + sa->iv_len +
		pad_payload_len;

	sym_cop->auth.digest.data = rte_pktmbuf_mtod_offset(m, uint8_t *,
			IP_ESP_HDR_SZ + sa->iv_len + pad_payload_len);
	sym_cop->auth.digest.phys_addr = rte_pktmbuf_mtophys_offset(m,
			IP_ESP_HDR_SZ + sa->iv_len + pad_payload_len);
	sym_cop->auth.digest.length = sa->digest_len;

	if (sa->cipher_algo == RTE_CRYPTO_CIPHER_AES_CBC)
		random_iv_u64((uint64_t *)sym_cop->cipher.iv.data,
				sym_cop->cipher.iv.length);

	return 0;
}

int
esp4_tunnel_outbound_post_crypto(struct rte_mbuf *m __rte_unused,
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
