/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Inspur Corporation
 */

#include "gso_common.h"
#include "gso_tunnel_udp4.h"

#define IPV4_HDR_MF_BIT (1U << 13)

static void
update_tunnel_ipv4_udp_headers(struct rte_mbuf *pkt, struct rte_mbuf **segs,
			       uint16_t nb_segs)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	uint16_t outer_id, inner_id, tail_idx, i, length;
	uint16_t outer_ipv4_offset, inner_ipv4_offset;
	uint16_t outer_udp_offset;
	uint16_t frag_offset = 0, is_mf;

	outer_ipv4_offset = pkt->outer_l2_len;
	outer_udp_offset = outer_ipv4_offset + pkt->outer_l3_len;
	inner_ipv4_offset = outer_udp_offset + pkt->l2_len;

	/* Outer IPv4 header. */
	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			outer_ipv4_offset);
	outer_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);

	/* Inner IPv4 header. */
	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			inner_ipv4_offset);
	inner_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);

	tail_idx = nb_segs - 1;

	for (i = 0; i < nb_segs; i++) {
		update_ipv4_header(segs[i], outer_ipv4_offset, outer_id);
		update_udp_header(segs[i], outer_udp_offset);
		update_ipv4_header(segs[i], inner_ipv4_offset, inner_id);
		/* For the case inner packet is UDP, we must keep UDP
		 * datagram boundary, it must be handled as IP fragment.
		 *
		 * Set IP fragment offset for inner IP header.
		 */
		ipv4_hdr = (struct rte_ipv4_hdr *)
			(rte_pktmbuf_mtod(segs[i], char *) +
				inner_ipv4_offset);
		is_mf = i < tail_idx ? IPV4_HDR_MF_BIT : 0;
		ipv4_hdr->fragment_offset =
			rte_cpu_to_be_16(frag_offset | is_mf);
		length = segs[i]->pkt_len - inner_ipv4_offset - pkt->l3_len;
		frag_offset += (length >> 3);
		outer_id++;
	}
}

int
gso_tunnel_udp4_segment(struct rte_mbuf *pkt,
		uint16_t gso_size,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_ipv4_hdr *inner_ipv4_hdr;
	uint16_t pyld_unit_size, hdr_offset, frag_off;
	int ret;

	hdr_offset = pkt->outer_l2_len + pkt->outer_l3_len + pkt->l2_len;
	inner_ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			hdr_offset);
	/*
	 * Don't process the packet whose MF bit or offset in the inner
	 * IPv4 header are non-zero.
	 */
	frag_off = rte_be_to_cpu_16(inner_ipv4_hdr->fragment_offset);
	if (unlikely(IS_FRAGMENTED(frag_off)))
		return 0;

	hdr_offset += pkt->l3_len;
	/* Don't process the packet without data */
	if ((hdr_offset + pkt->l4_len) >= pkt->pkt_len)
		return 0;

	/* pyld_unit_size must be a multiple of 8 because frag_off
	 * uses 8 bytes as unit.
	 */
	pyld_unit_size = (gso_size - hdr_offset) & ~7U;

	/* Segment the payload */
	ret = gso_do_segment(pkt, hdr_offset, pyld_unit_size, direct_pool,
			indirect_pool, pkts_out, nb_pkts_out);
	if (ret > 1)
		update_tunnel_ipv4_udp_headers(pkt, pkts_out, ret);

	return ret;
}
