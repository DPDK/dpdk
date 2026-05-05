/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2025 Institute of Software Chinese Academy of Sciences (ISCAS).
 */

#ifndef __INCLUDE_IP4_LOOKUP_RVV_H__
#define __INCLUDE_IP4_LOOKUP_RVV_H__

static __rte_always_inline vuint32m8_t
bswap32_vec(vuint32m8_t v, size_t vl)
{
	vuint32m8_t low16 = __riscv_vor_vv_u32m8(
		__riscv_vsll_vx_u32m8(__riscv_vand_vx_u32m8(v, 0xFF, vl), 24, vl),
		__riscv_vsll_vx_u32m8(__riscv_vand_vx_u32m8(v, 0xFF00, vl), 8, vl),
		vl);

	vuint32m8_t high16 = __riscv_vor_vv_u32m8(
		__riscv_vsrl_vx_u32m8(__riscv_vand_vx_u32m8(v, 0xFF0000, vl), 8, vl),
		__riscv_vsrl_vx_u32m8(v, 24, vl),
		vl);

	return __riscv_vor_vv_u32m8(low16, high16, vl);
}

static __rte_always_inline void
ip4_lookup_rvv_lpm_lookup(const struct rte_lpm *lpm, const uint32_t *ips,
			uint32_t *hop, size_t vl, uint32_t defv)
{
	/* Load IP addresses (network byte order) */
	vuint32m8_t v_ip = bswap32_vec(__riscv_vle32_v_u32m8(ips, vl), vl);

	vuint32m8_t v_tbl24_byte_offset = __riscv_vsll_vx_u32m8(
			__riscv_vsrl_vx_u32m8(v_ip, 8, vl), 2, vl);

	vuint32m8_t vtbl_entry = __riscv_vluxei32_v_u32m8(
		(const uint32_t *)lpm->tbl24, v_tbl24_byte_offset, vl);

	vbool4_t mask = __riscv_vmseq_vx_u32m8_b4(
		__riscv_vand_vx_u32m8(vtbl_entry, RTE_LPM_VALID_EXT_ENTRY_BITMASK, vl),
		RTE_LPM_VALID_EXT_ENTRY_BITMASK, vl);

	vuint32m8_t vtbl8_index = __riscv_vsll_vx_u32m8(
		__riscv_vadd_vv_u32m8(
			__riscv_vsll_vx_u32m8(
				__riscv_vand_vx_u32m8(vtbl_entry, 0x00FFFFFF, vl), 8, vl),
			__riscv_vand_vx_u32m8(v_ip, 0x000000FF, vl), vl),
		2, vl);

	vtbl_entry = __riscv_vluxei32_v_u32m8_mu(
		mask, vtbl_entry, (const uint32_t *)(lpm->tbl8), vtbl8_index, vl);

	vuint32m8_t vnext_hop = __riscv_vand_vx_u32m8(vtbl_entry, 0x00FFFFFF, vl);
	mask = __riscv_vmseq_vx_u32m8_b4(
		__riscv_vand_vx_u32m8(vtbl_entry, RTE_LPM_LOOKUP_SUCCESS, vl), 0, vl);

	vnext_hop = __riscv_vmerge_vxm_u32m8(vnext_hop, defv, mask, vl);

	__riscv_vse32_v_u32m8(hop, vnext_hop, vl);
}

/* Can be increased further for VLEN > 256 */
#define RVV_MAX_BURST 64U

static uint16_t
ip4_lookup_node_process_vec(struct rte_graph *graph, struct rte_node *node,
			void **objs, uint16_t nb_objs)
{
	struct rte_mbuf **pkts;
	struct rte_lpm *lpm = IP4_LOOKUP_NODE_LPM(node->ctx);
	const int dyn = IP4_LOOKUP_NODE_PRIV1_OFF(node->ctx);
	rte_edge_t next_index;
	void **to_next, **from;
	uint16_t last_spec = 0;
	uint16_t n_left_from;
	uint16_t held = 0;
	uint32_t drop_nh;

	/* Temporary arrays for batch processing */
	uint32_t ips[RVV_MAX_BURST];
	uint32_t res[RVV_MAX_BURST];
	rte_edge_t next_hops[RVV_MAX_BURST];

	/* Speculative next */
	next_index = RTE_NODE_IP4_LOOKUP_NEXT_REWRITE;
	/* Drop node */
	drop_nh = ((uint32_t)RTE_NODE_IP4_LOOKUP_NEXT_PKT_DROP) << 16;

	pkts = (struct rte_mbuf **)objs;
	from = objs;
	n_left_from = nb_objs;

	/* Get stream for the speculated next node */
	to_next = rte_node_next_stream_get(graph, node, next_index, nb_objs);

	while (n_left_from > 0) {
		rte_edge_t fix_spec = 0;

		size_t vl = __riscv_vsetvl_e32m8(RTE_MIN(n_left_from, RVV_MAX_BURST));

		/* Extract IP addresses and metadata from current batch */
		for (size_t i = 0; i < vl; i++) {
			struct rte_ipv4_hdr *ipv4_hdr =
				rte_pktmbuf_mtod_offset(pkts[i], struct rte_ipv4_hdr *,
						sizeof(struct rte_ether_hdr));
			ips[i] = ipv4_hdr->dst_addr;
			node_mbuf_priv1(pkts[i], dyn)->cksum = ipv4_hdr->hdr_checksum;
			node_mbuf_priv1(pkts[i], dyn)->ttl = ipv4_hdr->time_to_live;
		}

		/* Perform LPM lookup */
		ip4_lookup_rvv_lpm_lookup(lpm, ips, res, vl, drop_nh);

		for (size_t i = 0; i < vl; i++) {
			/* Update statistics */
			if ((res[i] >> 16) == (drop_nh >> 16))
				NODE_INCREMENT_XSTAT_ID(node, 0, 1, 1);

			/* Extract next hop and next node */
			node_mbuf_priv1(pkts[i], dyn)->nh = res[i] & 0xFFFF;
			next_hops[i] = res[i] >> 16;

			/* Check speculation */
			fix_spec |= (next_index ^ next_hops[i]);
		}

		if (unlikely(fix_spec)) {
			/* Copy successfully speculated packets before this batch */
			memcpy(to_next, from, last_spec * sizeof(from[0]));
			from += last_spec;
			to_next += last_spec;
			held += last_spec;
			last_spec = 0;

			/* Process each packet in current batch individually */
			for (size_t i = 0; i < vl; i++) {
				if (next_index == next_hops[i]) {
					*to_next++ = from[i];
					held++;
				} else {
					rte_node_enqueue_x1(graph, node, next_hops[i], from[i]);
				}
			}

			from += vl;
		} else {
			last_spec += vl;
		}

		pkts += vl;
		n_left_from -= vl;
	}

	/* Handle successfully speculated packets */
	if (likely(last_spec == nb_objs)) {
		rte_node_next_stream_move(graph, node, next_index);
		return nb_objs;
	}

	held += last_spec;
	memcpy(to_next, from, last_spec * sizeof(from[0]));
	rte_node_next_stream_put(graph, node, next_index, held);

	return nb_objs;
}
#endif
