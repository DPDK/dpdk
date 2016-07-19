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
#include <netinet/ip6.h>

#include <rte_mbuf.h>

static inline void *
ipip_outbound(struct rte_mbuf *m, uint32_t offset, uint32_t is_ipv6,
		struct ip_addr *src,  struct ip_addr *dst)
{
	struct ip *inip4, *outip4;
	struct ip6_hdr *inip6, *outip6;
	uint8_t ds_ecn;

	inip4 = rte_pktmbuf_mtod(m, struct ip *);

	RTE_ASSERT(inip4->ip_v == IPVERSION || inip4->ip_v == IP6_VERSION);

	if (inip4->ip_v == IPVERSION) {
		/* XXX This should be done by the forwarding engine instead */
		inip4->ip_ttl -= 1;
		ds_ecn = inip4->ip_tos;
	} else {
		inip6 = (struct ip6_hdr *)inip4;
		/* XXX This should be done by the forwarding engine instead */
		inip6->ip6_hops -= 1;
		ds_ecn = ntohl(inip6->ip6_flow) >> 20;
	}

	if (is_ipv6) {
		offset += sizeof(struct ip6_hdr);
		outip6 = (struct ip6_hdr *)rte_pktmbuf_prepend(m, offset);

		RTE_ASSERT(outip6 != NULL);

		/* Per RFC4301 5.1.2.1 */
		outip6->ip6_flow = htonl(IP6_VERSION << 28 | ds_ecn << 20);
		outip6->ip6_plen = htons(rte_pktmbuf_data_len(m));

		outip6->ip6_nxt = IPPROTO_ESP;
		outip6->ip6_hops = IPDEFTTL;

		memcpy(&outip6->ip6_src.s6_addr, src, 16);
		memcpy(&outip6->ip6_dst.s6_addr, dst, 16);

		return outip6;
	}

	offset += sizeof(struct ip);
	outip4 = (struct ip *)rte_pktmbuf_prepend(m, offset);

	RTE_ASSERT(outip4 != NULL);

	/* Per RFC4301 5.1.2.1 */
	outip4->ip_v = IPVERSION;
	outip4->ip_hl = 5;
	outip4->ip_tos = ds_ecn;
	outip4->ip_len = htons(rte_pktmbuf_data_len(m));

	outip4->ip_id = 0;
	outip4->ip_off = 0;

	outip4->ip_ttl = IPDEFTTL;
	outip4->ip_p = IPPROTO_ESP;

	outip4->ip_src.s_addr = src->ip.ip4;
	outip4->ip_dst.s_addr = dst->ip.ip4;

	return outip4;
}

static inline struct ip *
ip4ip_outbound(struct rte_mbuf *m, uint32_t offset,
		struct ip_addr *src,  struct ip_addr *dst)
{
	return ipip_outbound(m, offset, 0, src, dst);
}

static inline struct ip6_hdr *
ip6ip_outbound(struct rte_mbuf *m, uint32_t offset,
		struct ip_addr *src,  struct ip_addr *dst)
{
	return ipip_outbound(m, offset, 1, src, dst);
}

static inline void
ip4_ecn_setup(struct ip *ip4)
{
	if (ip4->ip_tos & IPTOS_ECN_MASK)
		ip4->ip_tos |= IPTOS_ECN_CE;
}

static inline void
ip6_ecn_setup(struct ip6_hdr *ip6)
{
	if ((ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK)
		ip6->ip6_flow = htonl(ntohl(ip6->ip6_flow) |
					(IPTOS_ECN_CE << 20));
}

static inline void
ipip_inbound(struct rte_mbuf *m, uint32_t offset)
{
	struct ip *inip4, *outip4;
	struct ip6_hdr *inip6, *outip6;
	uint32_t ip_len, set_ecn;

	outip4 = rte_pktmbuf_mtod(m, struct ip*);

	RTE_ASSERT(outip4->ip_v == IPVERSION || outip4->ip_v == IP6_VERSION);

	if (outip4->ip_v == IPVERSION) {
		ip_len = sizeof(struct ip);
		set_ecn = ((outip4->ip_tos & IPTOS_ECN_CE) == IPTOS_ECN_CE);
	} else {
		outip6 = (struct ip6_hdr *)outip4;
		ip_len = sizeof(struct ip6_hdr);
		set_ecn = ntohl(outip6->ip6_flow) >> 20;
		set_ecn = ((set_ecn & IPTOS_ECN_CE) == IPTOS_ECN_CE);
	}

	inip4 = (struct ip *)rte_pktmbuf_adj(m, offset + ip_len);
	RTE_ASSERT(inip4->ip_v == IPVERSION || inip4->ip_v == IP6_VERSION);

	/* Check packet is still bigger than IP header (inner) */
	RTE_ASSERT(rte_pktmbuf_pkt_len(m) > ip_len);

	/* RFC4301 5.1.2.1 Note 6 */
	if (inip4->ip_v == IPVERSION) {
		if (set_ecn)
			ip4_ecn_setup(inip4);
		/* XXX This should be done by the forwarding engine instead */
		inip4->ip_ttl -= 1;
	} else {
		inip6 = (struct ip6_hdr *)inip4;
		if (set_ecn)
			ip6_ecn_setup(inip6);
		/* XXX This should be done by the forwarding engine instead */
		inip6->ip6_hops -= 1;
	}
}

#endif /* __IPIP_H__ */
