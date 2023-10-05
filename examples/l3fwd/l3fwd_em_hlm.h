/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2018 Intel Corporation.
 * Copyright(c) 2017-2018 Linaro Limited.
 */

#ifndef __L3FWD_EM_HLM_H__
#define __L3FWD_EM_HLM_H__

#if defined RTE_ARCH_X86
#include "l3fwd_sse.h"
#include "l3fwd_em_hlm_sse.h"
#elif defined __ARM_NEON
#include "l3fwd_neon.h"
#include "l3fwd_em_hlm_neon.h"
#endif

#ifdef RTE_ARCH_ARM64
#define EM_HASH_LOOKUP_COUNT 16
#else
#define EM_HASH_LOOKUP_COUNT 8
#endif


static __rte_always_inline void
em_get_dst_port_ipv4xN(struct lcore_conf *qconf, struct rte_mbuf *m[],
		uint16_t portid, uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv4_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv4_5tuple(m[i], mask0.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				portid : ipv4_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = portid;
	}
}

static __rte_always_inline void
em_get_dst_port_ipv6xN(struct lcore_conf *qconf, struct rte_mbuf *m[],
		uint16_t portid, uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv6_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv6_5tuple(m[i], mask1.x, mask2.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv6_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				portid : ipv6_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = portid;
	}
}

static __rte_always_inline void
em_get_dst_port_ipv4xN_events(struct lcore_conf *qconf, struct rte_mbuf *m[],
			      uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv4_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv4_5tuple(m[i], mask0.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				m[i]->port : ipv4_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = m[i]->port;
	}
}

static __rte_always_inline void
em_get_dst_port_ipv6xN_events(struct lcore_conf *qconf, struct rte_mbuf *m[],
			      uint16_t dst_port[])
{
	int i;
	int32_t ret[EM_HASH_LOOKUP_COUNT];
	union ipv6_5tuple_host key[EM_HASH_LOOKUP_COUNT];
	const void *key_array[EM_HASH_LOOKUP_COUNT];

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		get_ipv6_5tuple(m[i], mask1.x, mask2.x, &key[i]);
		key_array[i] = &key[i];
	}

	rte_hash_lookup_bulk(qconf->ipv6_lookup_struct, &key_array[0],
			     EM_HASH_LOOKUP_COUNT, ret);

	for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
		dst_port[i] = ((ret[i] < 0) ?
				m[i]->port : ipv6_l3fwd_out_if[ret[i]]);

		if (dst_port[i] >= RTE_MAX_ETHPORTS ||
				(enabled_port_mask & 1 << dst_port[i]) == 0)
			dst_port[i] = m[i]->port;
	}
}

static __rte_always_inline uint16_t
em_get_dst_port(const struct lcore_conf *qconf, struct rte_mbuf *pkt,
		uint16_t portid)
{
	uint16_t next_hop;
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

static inline void
l3fwd_em_process_packets(int nb_rx, struct rte_mbuf **pkts_burst,
			 uint16_t *dst_port, uint16_t portid,
			 struct lcore_conf *qconf, const uint8_t do_step3)
{
	int32_t i, j, pos;

	/*
	 * Send nb_rx - nb_rx % EM_HASH_LOOKUP_COUNT packets
	 * in groups of EM_HASH_LOOKUP_COUNT.
	 */
	int32_t n = RTE_ALIGN_FLOOR(nb_rx, EM_HASH_LOOKUP_COUNT);

	for (j = 0; j < EM_HASH_LOOKUP_COUNT && j < nb_rx; j++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j],
					       struct rte_ether_hdr *) + 1);
	}

	for (j = 0; j < n; j += EM_HASH_LOOKUP_COUNT) {

		uint32_t pkt_type = RTE_PTYPE_L3_MASK |
				    RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP;
		uint32_t l3_type, tcp_or_udp;

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkt_type &= pkts_burst[j + i]->packet_type;

		l3_type = pkt_type & RTE_PTYPE_L3_MASK;
		tcp_or_udp = pkt_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);

		for (i = 0, pos = j + EM_HASH_LOOKUP_COUNT;
		     i < EM_HASH_LOOKUP_COUNT && pos < nb_rx; i++, pos++) {
			rte_prefetch0(rte_pktmbuf_mtod(
					pkts_burst[pos],
					struct rte_ether_hdr *) + 1);
		}

		if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV4)) {

			em_get_dst_port_ipv4xN(qconf, &pkts_burst[j], portid,
					       &dst_port[j]);

		} else if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV6)) {

			em_get_dst_port_ipv6xN(qconf, &pkts_burst[j], portid,
					       &dst_port[j]);

		} else {
			for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
				dst_port[j + i] = em_get_dst_port(qconf,
						pkts_burst[j + i], portid);
		}

		for (i = 0; i < EM_HASH_LOOKUP_COUNT && do_step3; i += FWDSTEP)
			processx4_step3(&pkts_burst[j + i], &dst_port[j + i]);
	}

	for (; j < nb_rx; j++) {
		dst_port[j] = em_get_dst_port(qconf, pkts_burst[j], portid);
		if (do_step3)
			process_packet(pkts_burst[j], &pkts_burst[j]->port);
	}
}

/*
 * Buffer optimized handling of packets, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_send_packets(int nb_rx, struct rte_mbuf **pkts_burst, uint16_t portid,
		      struct lcore_conf *qconf)
{
	uint16_t dst_port[MAX_PKT_BURST];

	l3fwd_em_process_packets(nb_rx, pkts_burst, dst_port, portid, qconf, 0);
	send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);
}

#ifdef RTE_LIB_EVENTDEV
/*
 * Buffer optimized handling of events, invoked
 * from main_loop.
 */
static inline void
l3fwd_em_process_events(int nb_rx, struct rte_event **ev,
		     struct lcore_conf *qconf)
{
	int32_t i, j, pos;
	uint16_t dst_port[MAX_PKT_BURST];
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];

	/*
	 * Send nb_rx - nb_rx % EM_HASH_LOOKUP_COUNT packets
	 * in groups of EM_HASH_LOOKUP_COUNT.
	 */
	int32_t n = RTE_ALIGN_FLOOR(nb_rx, EM_HASH_LOOKUP_COUNT);

	for (j = 0; j < nb_rx; j++)
		pkts_burst[j] = ev[j]->mbuf;

	for (j = 0; j < n; j += EM_HASH_LOOKUP_COUNT) {

		uint32_t pkt_type = RTE_PTYPE_L3_MASK |
				    RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP;
		uint32_t l3_type, tcp_or_udp;

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkt_type &= pkts_burst[j + i]->packet_type;

		l3_type = pkt_type & RTE_PTYPE_L3_MASK;
		tcp_or_udp = pkt_type & (RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);

		for (i = 0, pos = j + EM_HASH_LOOKUP_COUNT;
		     i < EM_HASH_LOOKUP_COUNT && pos < nb_rx; i++, pos++) {
			rte_prefetch0(rte_pktmbuf_mtod(
					pkts_burst[pos],
					struct rte_ether_hdr *) + 1);
		}

		if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV4)) {

			em_get_dst_port_ipv4xN_events(qconf, &pkts_burst[j],
					       &dst_port[j]);

		} else if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV6)) {

			em_get_dst_port_ipv6xN_events(qconf, &pkts_burst[j],
					       &dst_port[j]);

		} else {
			for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++) {
				pkts_burst[j + i]->port = em_get_dst_port(qconf,
						pkts_burst[j + i],
						pkts_burst[j + i]->port);
				process_packet(pkts_burst[j + i],
						&pkts_burst[j + i]->port);
			}
			continue;
		}
		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i += FWDSTEP)
			processx4_step3(&pkts_burst[j + i], &dst_port[j + i]);

		for (i = 0; i < EM_HASH_LOOKUP_COUNT; i++)
			pkts_burst[j + i]->port = dst_port[j + i];

	}

	for (; j < nb_rx; j++) {
		pkts_burst[j]->port = em_get_dst_port(qconf, pkts_burst[j],
						      pkts_burst[j]->port);
		process_packet(pkts_burst[j], &pkts_burst[j]->port);
	}
}

static inline void
l3fwd_em_process_event_vector(struct rte_event_vector *vec,
			      struct lcore_conf *qconf, uint16_t *dst_port)
{
	uint16_t i;

	if (vec->attr_valid)
		l3fwd_em_process_packets(vec->nb_elem, vec->mbufs, dst_port,
					 vec->port, qconf, 1);
	else
		for (i = 0; i < vec->nb_elem; i++)
			l3fwd_em_process_packets(1, &vec->mbufs[i],
						 &dst_port[i],
						 vec->mbufs[i]->port, qconf, 1);

	process_event_vector(vec, dst_port);
}
#endif /* RTE_LIB_EVENTDEV */

#endif /* __L3FWD_EM_HLM_H__ */
