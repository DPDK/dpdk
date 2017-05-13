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

#ifndef __L3FWD_EM_HLM_SSE_H__
#define __L3FWD_EM_HLM_SSE_H__

#include "l3fwd_sse.h"

static __rte_always_inline void
em_get_dst_port_ipv4x8(struct lcore_conf *qconf, struct rte_mbuf *m[8],
		uint8_t portid, uint16_t dst_port[8])
{
	int32_t ret[8];
	union ipv4_5tuple_host key[8];
	__m128i data[8];

	data[0] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[0], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[1] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[1], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[2] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[2], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[3] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[3], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[4] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[4], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[5] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[5], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[6] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[6], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));
	data[7] = _mm_loadu_si128(rte_pktmbuf_mtod_offset(m[7], __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv4_hdr, time_to_live)));

	key[0].xmm = _mm_and_si128(data[0], mask0.x);
	key[1].xmm = _mm_and_si128(data[1], mask0.x);
	key[2].xmm = _mm_and_si128(data[2], mask0.x);
	key[3].xmm = _mm_and_si128(data[3], mask0.x);
	key[4].xmm = _mm_and_si128(data[4], mask0.x);
	key[5].xmm = _mm_and_si128(data[5], mask0.x);
	key[6].xmm = _mm_and_si128(data[6], mask0.x);
	key[7].xmm = _mm_and_si128(data[7], mask0.x);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
				&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_bulk(qconf->ipv4_lookup_struct, &key_array[0], 8, ret);

	dst_port[0] = (uint8_t) ((ret[0] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[0]]);
	dst_port[1] = (uint8_t) ((ret[1] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[1]]);
	dst_port[2] = (uint8_t) ((ret[2] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[2]]);
	dst_port[3] = (uint8_t) ((ret[3] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[3]]);
	dst_port[4] = (uint8_t) ((ret[4] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[4]]);
	dst_port[5] = (uint8_t) ((ret[5] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[5]]);
	dst_port[6] = (uint8_t) ((ret[6] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[6]]);
	dst_port[7] = (uint8_t) ((ret[7] < 0) ?
			portid : ipv4_l3fwd_out_if[ret[7]]);

	if (dst_port[0] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[0]) == 0)
		dst_port[0] = portid;

	if (dst_port[1] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[1]) == 0)
		dst_port[1] = portid;

	if (dst_port[2] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[2]) == 0)
		dst_port[2] = portid;

	if (dst_port[3] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[3]) == 0)
		dst_port[3] = portid;

	if (dst_port[4] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[4]) == 0)
		dst_port[4] = portid;

	if (dst_port[5] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[5]) == 0)
		dst_port[5] = portid;

	if (dst_port[6] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[6]) == 0)
		dst_port[6] = portid;

	if (dst_port[7] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[7]) == 0)
		dst_port[7] = portid;

}

static inline void
get_ipv6_5tuple(struct rte_mbuf *m0, __m128i mask0,
		__m128i mask1, union ipv6_5tuple_host *key)
{
	__m128i tmpdata0 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len)));

	__m128i tmpdata1 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len) +
				sizeof(__m128i)));

	__m128i tmpdata2 = _mm_loadu_si128(
			rte_pktmbuf_mtod_offset(m0, __m128i *,
				sizeof(struct ether_hdr) +
				offsetof(struct ipv6_hdr, payload_len) +
				sizeof(__m128i) + sizeof(__m128i)));

	key->xmm[0] = _mm_and_si128(tmpdata0, mask0);
	key->xmm[1] = tmpdata1;
	key->xmm[2] = _mm_and_si128(tmpdata2, mask1);
}

static __rte_always_inline void
em_get_dst_port_ipv6x8(struct lcore_conf *qconf, struct rte_mbuf *m[8],
		uint8_t portid, uint16_t dst_port[8])
{
	int32_t ret[8];
	union ipv6_5tuple_host key[8];

	get_ipv6_5tuple(m[0], mask1.x, mask2.x, &key[0]);
	get_ipv6_5tuple(m[1], mask1.x, mask2.x, &key[1]);
	get_ipv6_5tuple(m[2], mask1.x, mask2.x, &key[2]);
	get_ipv6_5tuple(m[3], mask1.x, mask2.x, &key[3]);
	get_ipv6_5tuple(m[4], mask1.x, mask2.x, &key[4]);
	get_ipv6_5tuple(m[5], mask1.x, mask2.x, &key[5]);
	get_ipv6_5tuple(m[6], mask1.x, mask2.x, &key[6]);
	get_ipv6_5tuple(m[7], mask1.x, mask2.x, &key[7]);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
			&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_bulk(qconf->ipv6_lookup_struct, &key_array[0], 8, ret);

	dst_port[0] = (uint8_t) ((ret[0] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[0]]);
	dst_port[1] = (uint8_t) ((ret[1] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[1]]);
	dst_port[2] = (uint8_t) ((ret[2] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[2]]);
	dst_port[3] = (uint8_t) ((ret[3] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[3]]);
	dst_port[4] = (uint8_t) ((ret[4] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[4]]);
	dst_port[5] = (uint8_t) ((ret[5] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[5]]);
	dst_port[6] = (uint8_t) ((ret[6] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[6]]);
	dst_port[7] = (uint8_t) ((ret[7] < 0) ?
			portid : ipv6_l3fwd_out_if[ret[7]]);

	if (dst_port[0] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[0]) == 0)
		dst_port[0] = portid;

	if (dst_port[1] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[1]) == 0)
		dst_port[1] = portid;

	if (dst_port[2] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[2]) == 0)
		dst_port[2] = portid;

	if (dst_port[3] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[3]) == 0)
		dst_port[3] = portid;

	if (dst_port[4] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[4]) == 0)
		dst_port[4] = portid;

	if (dst_port[5] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[5]) == 0)
		dst_port[5] = portid;

	if (dst_port[6] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[6]) == 0)
		dst_port[6] = portid;

	if (dst_port[7] >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port[7]) == 0)
		dst_port[7] = portid;

}

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
	int32_t j;
	uint16_t dst_port[MAX_PKT_BURST];

	/*
	 * Send nb_rx - nb_rx%8 packets
	 * in groups of 8.
	 */
	int32_t n = RTE_ALIGN_FLOOR(nb_rx, 8);

	for (j = 0; j < n; j += 8) {

		uint32_t pkt_type =
			pkts_burst[j]->packet_type &
			pkts_burst[j+1]->packet_type &
			pkts_burst[j+2]->packet_type &
			pkts_burst[j+3]->packet_type &
			pkts_burst[j+4]->packet_type &
			pkts_burst[j+5]->packet_type &
			pkts_burst[j+6]->packet_type &
			pkts_burst[j+7]->packet_type;

		uint32_t l3_type = pkt_type & RTE_PTYPE_L3_MASK;
		uint32_t tcp_or_udp = pkt_type &
			(RTE_PTYPE_L4_TCP | RTE_PTYPE_L4_UDP);

		if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV4)) {

			em_get_dst_port_ipv4x8(qconf, &pkts_burst[j], portid, &dst_port[j]);

		} else if (tcp_or_udp && (l3_type == RTE_PTYPE_L3_IPV6)) {

			em_get_dst_port_ipv6x8(qconf, &pkts_burst[j], portid, &dst_port[j]);

		} else {
			dst_port[j]   = em_get_dst_port(qconf, pkts_burst[j], portid);
			dst_port[j+1] = em_get_dst_port(qconf, pkts_burst[j+1], portid);
			dst_port[j+2] = em_get_dst_port(qconf, pkts_burst[j+2], portid);
			dst_port[j+3] = em_get_dst_port(qconf, pkts_burst[j+3], portid);
			dst_port[j+4] = em_get_dst_port(qconf, pkts_burst[j+4], portid);
			dst_port[j+5] = em_get_dst_port(qconf, pkts_burst[j+5], portid);
			dst_port[j+6] = em_get_dst_port(qconf, pkts_burst[j+6], portid);
			dst_port[j+7] = em_get_dst_port(qconf, pkts_burst[j+7], portid);
		}
	}

	for (; j < nb_rx; j++)
		dst_port[j] = em_get_dst_port(qconf, pkts_burst[j], portid);

	send_packets_multi(qconf, pkts_burst, dst_port, nb_rx);

}
#endif /* __L3FWD_EM_SSE_HLM_H__ */
