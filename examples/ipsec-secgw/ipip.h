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

#ifndef __IPIP_H__
#define __IPIP_H__

#include <stdint.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <rte_mbuf.h>

#define IPV6_VERSION (6)

static inline  struct ip *
ip4ip_outbound(struct rte_mbuf *m, uint32_t offset, uint32_t src, uint32_t dst)
{
	struct ip *inip, *outip;

	inip = rte_pktmbuf_mtod(m, struct ip*);

	RTE_ASSERT(inip->ip_v == IPVERSION || inip->ip_v == IPV6_VERSION);

	offset += sizeof(struct ip);

	outip = (struct ip *)rte_pktmbuf_prepend(m, offset);

	RTE_ASSERT(outip != NULL);

	/* Per RFC4301 5.1.2.1 */
	outip->ip_v = IPVERSION;
	outip->ip_hl = 5;
	outip->ip_tos = inip->ip_tos;
	outip->ip_len = htons(rte_pktmbuf_data_len(m));

	outip->ip_id = 0;
	outip->ip_off = 0;

	outip->ip_ttl = IPDEFTTL;
	outip->ip_p = IPPROTO_ESP;

	outip->ip_src.s_addr = src;
	outip->ip_dst.s_addr = dst;

	return outip;
}

static inline int
ip4ip_inbound(struct rte_mbuf *m, uint32_t offset)
{
	struct ip *inip;
	struct ip *outip;

	outip = rte_pktmbuf_mtod(m, struct ip*);

	RTE_ASSERT(outip->ip_v == IPVERSION);

	offset += sizeof(struct ip);
	inip = (struct ip *)rte_pktmbuf_adj(m, offset);
	RTE_ASSERT(inip->ip_v == IPVERSION || inip->ip_v == IPV6_VERSION);

	/* Check packet is still bigger than IP header (inner) */
	RTE_ASSERT(rte_pktmbuf_pkt_len(m) > sizeof(struct ip));

	/* RFC4301 5.1.2.1 Note 6 */
	if ((inip->ip_tos & htons(IPTOS_ECN_ECT0 | IPTOS_ECN_ECT1)) &&
			((outip->ip_tos & htons(IPTOS_ECN_CE)) == IPTOS_ECN_CE))
		inip->ip_tos |= htons(IPTOS_ECN_CE);

	return 0;
}

#endif /* __IPIP_H__ */
