/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#ifndef __INCLUDE_IP4_LOOKUP_SSE_H__
#define __INCLUDE_IP4_LOOKUP_SSE_H__

/* X86 SSE */
static uint16_t
ip4_lookup_node_process_vec(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	struct rte_mbuf *mbuf0, *mbuf1, *mbuf2, *mbuf3, **pkts;
	struct rte_lpm *lpm = IP4_LOOKUP_NODE_LPM(node->ctx);
	const int dyn = IP4_LOOKUP_NODE_PRIV1_OFF(node->ctx);
	rte_edge_t next0, next1, next2, next3, next_index;
	struct rte_ipv4_hdr *ipv4_hdr;
	uint32_t ip0, ip1, ip2, ip3;
	void **to_next, **from;
	uint16_t last_spec = 0;
	uint16_t n_left_from;
	uint16_t held = 0;
	uint32_t drop_nh;
	rte_xmm_t dst;
	__m128i dip; /* SSE register */
	int rc, i;

	/* Speculative next */
	next_index = RTE_NODE_IP4_LOOKUP_NEXT_REWRITE;
	/* Drop node */
	drop_nh = ((uint32_t)RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP) << 16;

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	if (n_left_from >= 4) {
		for (i = 0; i < 4; i++)
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[i], void *,
						sizeof(struct rte_ether_hdr)));
	}

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);
	while (n_left_from >= 4) {
		/* Prefetch next-next mbufs */
		if (likely(n_left_from > 11)) {
			rte_prefetch0(pkts[8]);
			rte_prefetch0(pkts[9]);
			rte_prefetch0(pkts[10]);
			rte_prefetch0(pkts[11]);
		}

		/* Prefetch next mbuf data */
		if (likely(n_left_from > 7)) {
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[4], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[5], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[6], void *,
						sizeof(struct rte_ether_hdr)));
			rte_prefetch0(rte_pktmbuf_mtod_offset(pkts[7], void *,
						sizeof(struct rte_ether_hdr)));
		}

		mbuf0 = pkts[0];
		mbuf1 = pkts[1];
		mbuf2 = pkts[2];
		mbuf3 = pkts[3];

		pkts += 4;
		n_left_from -= 4;

		/* Extract DIP of mbuf0 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf0, struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
		ip0 = ipv4_hdr->dst_addr;
		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf0, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf0, dyn)->ttl = ipv4_hdr->time_to_live;

		/* Extract DIP of mbuf1 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf1, struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
		ip1 = ipv4_hdr->dst_addr;
		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf1, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf1, dyn)->ttl = ipv4_hdr->time_to_live;

		/* Extract DIP of mbuf2 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf2, struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
		ip2 = ipv4_hdr->dst_addr;
		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf2, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf2, dyn)->ttl = ipv4_hdr->time_to_live;

		/* Extract DIP of mbuf3 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf3, struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
		ip3 = ipv4_hdr->dst_addr;

		/* Prepare for lookup x4 */
		dip = _mm_set_epi32(ip3, ip2, ip1, ip0);

		/* Byte swap 4 IPV4 addresses. */
		const __m128i bswap_mask = _mm_set_epi8(
			12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
		dip = _mm_shuffle_epi8(dip, bswap_mask);

		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf3, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf3, dyn)->ttl = ipv4_hdr->time_to_live;

		/* Perform LPM lookup to get NH and next node */
		rte_lpm_lookupx4(lpm, dip, dst.u32, drop_nh);

		/* Extract next node id and NH */
		node_mbuf_priv1(mbuf0, dyn)->nh = dst.u32[0] & 0xFFFF;
		next0 = (dst.u32[0] >> 16);

		node_mbuf_priv1(mbuf1, dyn)->nh = dst.u32[1] & 0xFFFF;
		next1 = (dst.u32[1] >> 16);

		node_mbuf_priv1(mbuf2, dyn)->nh = dst.u32[2] & 0xFFFF;
		next2 = (dst.u32[2] >> 16);

		node_mbuf_priv1(mbuf3, dyn)->nh = dst.u32[3] & 0xFFFF;
		next3 = (dst.u32[3] >> 16);

		/* Enqueue four to next node */
		rte_edge_t fix_spec =
			(next_index ^ next0) | (next_index ^ next1) |
			(next_index ^ next2) | (next_index ^ next3);

		if (unlikely(fix_spec)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			/* Next0 */
			if (next_index == next0) {
				to_next[0] = from[0];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next0,
						    from[0]);
			}

			/* Next1 */
			if (next_index == next1) {
				to_next[0] = from[1];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next1,
						    from[1]);
			}

			/* Next2 */
			if (next_index == next2) {
				to_next[0] = from[2];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next2,
						    from[2]);
			}

			/* Next3 */
			if (next_index == next3) {
				to_next[0] = from[3];
				to_next++;
				held++;
			} else {
				rte_node_enqueue_x1(graph, node, next3,
						    from[3]);
			}

			from += 4;

		} else {
			last_spec += 4;
		}
	}

	while (n_left_from > 0) {
		uint32_t next_hop;

		mbuf0 = pkts[0];

		pkts += 1;
		n_left_from -= 1;

		/* Extract DIP of mbuf0 */
		ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf0, struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
		/* Extract cksum, ttl as ipv4 hdr is in cache */
		node_mbuf_priv1(mbuf0, dyn)->cksum = ipv4_hdr->hdr_checksum;
		node_mbuf_priv1(mbuf0, dyn)->ttl = ipv4_hdr->time_to_live;

		rc = rte_lpm_lookup(lpm, rte_be_to_cpu_32(ipv4_hdr->dst_addr),
				    &next_hop);
		next_hop = (rc == 0) ? next_hop : drop_nh;

		node_mbuf_priv1(mbuf0, dyn)->nh = next_hop & 0xFFFF;
		next0 = (next_hop >> 16);

		if (unlikely(next_index ^ next0)) {
			/* Copy things successfully speculated till now */
			rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			rte_node_enqueue_x1(graph, node, next0, from[0]);
			from += 1;
		} else {
			last_spec += 1;
		}
	}

	/* !!! Home run !!! */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}

	held += last_spec;
	/* Copy things successfully speculated till now */
	rte_memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);

	return nb_objs;
}

#endif /* __INCLUDE_IP4_LOOKUP_SSE_H__ */
