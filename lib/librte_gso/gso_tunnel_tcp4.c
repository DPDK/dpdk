/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include "gso_common.h"
#include "gso_tunnel_tcp4.h"

static void
update_tunnel_ipv4_tcp_headers(struct rte_mbuf *pkt, uint8_t ipid_delta,
		struct rte_mbuf **segs, uint16_t nb_segs)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	uint32_t sent_seq;
	uint16_t outer_id, inner_id, tail_idx, i;
	uint16_t outer_ipv4_offset, inner_ipv4_offset;
	uint16_t udp_gre_offset, tcp_offset;
	uint8_t update_udp_hdr;

	outer_ipv4_offset = pkt->outer_l2_len;
	udp_gre_offset = outer_ipv4_offset + pkt->outer_l3_len;
	inner_ipv4_offset = udp_gre_offset + pkt->l2_len;
	tcp_offset = inner_ipv4_offset + pkt->l3_len;

	/* Outer IPv4 header. */
	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			outer_ipv4_offset);
	outer_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);

	/* Inner IPv4 header. */
	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			inner_ipv4_offset);
	inner_id = rte_be_to_cpu_16(ipv4_hdr->packet_id);

	tcp_hdr = (struct rte_tcp_hdr *)((char *)ipv4_hdr + pkt->l3_len);
	sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
	tail_idx = nb_segs - 1;

	/* Only update UDP header for VxLAN packets. */
	update_udp_hdr = (pkt->ol_flags & PKT_TX_TUNNEL_VXLAN) ? 1 : 0;

	for (i = 0; i < nb_segs; i++) {
		update_ipv4_header(segs[i], outer_ipv4_offset, outer_id);
		if (update_udp_hdr)
			update_udp_header(segs[i], udp_gre_offset);
		update_ipv4_header(segs[i], inner_ipv4_offset, inner_id);
		update_tcp_header(segs[i], tcp_offset, sent_seq, i < tail_idx);
		outer_id++;
		inner_id += ipid_delta;
		sent_seq += (segs[i]->pkt_len - segs[i]->data_len);
	}
}

int
gso_tunnel_tcp4_segment(struct rte_mbuf *pkt,
		uint16_t gso_size,
		uint8_t ipid_delta,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_ipv4_hdr *inner_ipv4_hdr;
	uint16_t pyld_unit_size, hdr_offset, frag_off;
	int ret = 1;

	hdr_offset = pkt->outer_l2_len + pkt->outer_l3_len + pkt->l2_len;
	inner_ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			hdr_offset);
	/*
	 * Don't process the packet whose MF bit or offset in the inner
	 * IPv4 header are non-zero.
	 */
	frag_off = rte_be_to_cpu_16(inner_ipv4_hdr->fragment_offset);
	if (unlikely(IS_FRAGMENTED(frag_off))) {
		pkts_out[0] = pkt;
		return 1;
	}

	hdr_offset += pkt->l3_len + pkt->l4_len;
	/* Don't process the packet without data */
	if (hdr_offset >= pkt->pkt_len) {
		pkts_out[0] = pkt;
		return 1;
	}
	pyld_unit_size = gso_size - hdr_offset;

	/* Segment the payload */
	ret = gso_do_segment(pkt, hdr_offset, pyld_unit_size, direct_pool,
			indirect_pool, pkts_out, nb_pkts_out);
	if (ret <= 1)
		return ret;

	update_tunnel_ipv4_tcp_headers(pkt, ipid_delta, pkts_out, ret);

	return ret;
}
