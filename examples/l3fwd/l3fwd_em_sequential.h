/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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

#ifndef __L3FWD_EM_SEQUENTIAL_H__
#define __L3FWD_EM_SEQUENTIAL_H__

/**
 * @file
 * This is an optional implementation of packet classification in Exact-Match
 * path using sequential packet classification method.
 * While hash lookup multi seems to provide better performance, it's disabled
 * by default and can be enabled with NO_HASH_LOOKUP_MULTI global define in
 * compilation time.
 */

#if defined RTE_ARCH_X86
#include "l3fwd_sse.h"
#elif defined RTE_MACHINE_CPUFLAG_NEON
#include "l3fwd_neon.h"
#endif

static __rte_always_inline uint16_t
em_get_dst_port(const struct lcore_conf *qconf, struct rte_mbuf *pkt,
		uint8_t portid)
{
	uint8_t next_hop;
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	uint32_t tcp_or_udp;
	uint32_t l3_ptypes;

	tcp_or_udp = pkt->packet_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);
	l3_ptypes = pkt->packet_type & RTE_PTYPE_L3_MASK;

	if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV4)) {

		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *,
				sizeof(struct ether_hdr));

		next_hop = em_get_ipv4_dst_port(ipv4_hdr, portid,
				qconf->ipv4_lookup_struct);

		if (next_hop >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << next_hop) == 0)
			next_hop = portid;

		return next_hop;

	} else if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV6)) {

		/* Handle IPv6 headers.*/
		ipv6_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv6_hdr *,
				sizeof(struct ether_hdr));

		next_hop = em_get_ipv6_dst_port(ipv6_hdr, portid,
				qconf->ipv6_lookup_struct);

		if (next_hop >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << next_hop) == 0)
			next_hop = portid;

		return next_hop;

	}

	return portid;
}

/*
 * Buffer optimized handling of packets, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_send_packets(int nb_rx, struct rte_mbuf **pkts_burst,
			uint8_t portid, struct lcore_conf *qconf)
{
	int32_t i, j;
	uint16_t dst_port[MAX_PKT_BURST];

	if (nb_rx > 0) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[0],
					       struct ether_hdr *) + 1);
	}

	for (i = 1, j = 0; j < nb_rx; i++, j++) {
		if (i < nb_rx) {
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i],
						       struct ether_hdr *) + 1);
		}
		dst_port[j] = em_get_dst_port(qconf, pkts_burst[j], portid);
	}

	send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);
}
#endif /* __L3FWD_EM_SEQUENTIAL_H__ */
