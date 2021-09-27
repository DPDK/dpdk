/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
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
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#endif

static __rte_always_inline uint16_t
em_get_dst_port(const struct lcore_conf *qconf, struct rte_mbuf *pkt,
		uint16_t portid)
{
	uint8_t next_hop;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	uint32_t tcp_or_udp;
	uint32_t l3_ptypes;

	tcp_or_udp = pkt->packet_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);
	l3_ptypes = pkt->packet_type & RTE_PTYPE_L3_MASK;

	if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV4)) {

		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
				sizeof(struct rte_ether_hdr));

		next_hop = em_get_ipv4_dst_port(ipv4_hdr, portid,
				qconf->ipv4_lookup_struct);

		if (next_hop >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << next_hop) == 0)
			next_hop = portid;

		return next_hop;

	} else if (tcp_or_udp && (l3_ptypes == RTE_PTYPE_L3_IPV6)) {

		/* Handle IPv6 headers.*/
		ipv6_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv6_hdr *,
				sizeof(struct rte_ether_hdr));

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
			uint16_t portid, struct lcore_conf *qconf)
{
	int32_t i, j;
	uint16_t dst_port[MAX_PKT_BURST];

	if (nb_rx > 0) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[0],
					       struct rte_ether_hdr *) + 1);
	}

	for (i = 1, j = 0; j < nb_rx; i++, j++) {
		if (i < nb_rx) {
			rte_prefetch0(rte_pktmbuf_mtod(
					pkts_burst[i],
					struct rte_ether_hdr *) + 1);
		}
		dst_port[j] = em_get_dst_port(qconf, pkts_burst[j], portid);
	}

	send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);
}

/*
 * Buffer optimized handling of events, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_process_events(int nb_rx, struct rte_event **events,
		     struct lcore_conf *qconf)
{
	int32_t i, j;

	rte_prefetch0(rte_pktmbuf_mtod(events[0]->mbuf,
		      struct rte_ether_hdr *) + 1);

	for (i = 1, j = 0; j < nb_rx; i++, j++) {
		struct rte_mbuf *mbuf = events[j]->mbuf;

		if (i < nb_rx) {
			rte_prefetch0(rte_pktmbuf_mtod(
					events[i]->mbuf,
					struct rte_ether_hdr *) + 1);
		}
		mbuf->port = em_get_dst_port(qconf, mbuf, mbuf->port);
		process_packet(mbuf, &mbuf->port);
	}
}

static inline void
l3fwd_em_process_event_vector(struct rte_event_vector *vec,
			      struct lcore_conf *qconf)
{
	struct rte_mbuf **mbufs = vec->mbufs;
	int32_t i, j;

	rte_prefetch0(rte_pktmbuf_mtod(mbufs[0], struct rte_ether_hdr *) + 1);

	if (vec->attr_valid)
		vec->port = em_get_dst_port(qconf, mbufs[0], mbufs[0]->port);

	for (i = 0, j = 1; i < vec->nb_elem; i++, j++) {
		if (j < vec->nb_elem)
			rte_prefetch0(rte_pktmbuf_mtod(mbufs[j],
						       struct rte_ether_hdr *) +
				      1);
		mbufs[i]->port =
			em_get_dst_port(qconf, mbufs[i], mbufs[i]->port);
		process_packet(mbufs[i], &mbufs[i]->port);
		event_vector_attr_validate(vec, mbufs[i]);
	}
}

#endif /* __L3FWD_EM_SEQUENTIAL_H__ */
