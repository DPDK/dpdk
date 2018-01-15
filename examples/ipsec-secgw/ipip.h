/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Intel Corporation
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
		if (inip4->ip_sum >= rte_cpu_to_be_16(0xffff - 0x100))
			inip4->ip_sum += rte_cpu_to_be_16(0x100) + 1;
		else
			inip4->ip_sum += rte_cpu_to_be_16(0x100);
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
		outip6->ip6_plen = htons(rte_pktmbuf_data_len(m) -
					 sizeof(struct ip6_hdr));

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
	m->packet_type &= ~RTE_PTYPE_L4_MASK;
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
	if (ip4->ip_tos & IPTOS_ECN_MASK) {
		unsigned long sum;
		uint8_t old;

		old = ip4->ip_tos;
		ip4->ip_tos |= IPTOS_ECN_CE;
		sum = old + (~(*(uint8_t *)&ip4->ip_tos) & 0xff);
		sum += rte_be_to_cpu_16(ip4->ip_sum);
		sum = (sum & 0xffff) + (sum >> 16);
		ip4->ip_sum = rte_cpu_to_be_16(sum + (sum >> 16));
	}
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
		if (inip4->ip_sum >= rte_cpu_to_be_16(0xffff - 0x100))
			inip4->ip_sum += rte_cpu_to_be_16(0x100) + 1;
		else
			inip4->ip_sum += rte_cpu_to_be_16(0x100);
		m->packet_type &= ~RTE_PTYPE_L4_MASK;
		if (inip4->ip_p == IPPROTO_UDP)
			m->packet_type |= RTE_PTYPE_L4_UDP;
		else if (inip4->ip_p == IPPROTO_TCP)
			m->packet_type |= RTE_PTYPE_L4_TCP;
	} else {
		inip6 = (struct ip6_hdr *)inip4;
		if (set_ecn)
			ip6_ecn_setup(inip6);
		/* XXX This should be done by the forwarding engine instead */
		inip6->ip6_hops -= 1;
	}
}

#endif /* __IPIP_H__ */
