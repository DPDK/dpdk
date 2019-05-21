/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include "gso_common.h"
#include "gso_udp4.h"

#define IPV4_HDR_MF_BIT (1U << 13)

static inline void
update_ipv4_udp_headers(struct rte_mbuf *pkt, struct rte_mbuf **segs,
		uint16_t nb_segs)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	uint16_t frag_offset = 0, is_mf;
	uint16_t l2_hdrlen = pkt->l2_len, l3_hdrlen = pkt->l3_len;
	uint16_t tail_idx = nb_segs - 1, length, i;

	/*
	 * Update IP header fields for output segments. Specifically,
	 * keep the same IP id, update fragment offset and total
	 * length.
	 */
	for (i = 0; i < nb_segs; i++) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(segs[i],
			struct rte_ipv4_hdr *, l2_hdrlen);
		length = segs[i]->pkt_len - l2_hdrlen;
		ipv4_hdr->total_length = rte_cpu_to_be_16(length);

		is_mf = i < tail_idx ? IPV4_HDR_MF_BIT : 0;
		ipv4_hdr->fragment_offset =
			rte_cpu_to_be_16(frag_offset | is_mf);
		frag_offset += ((length - l3_hdrlen) >> 3);
	}
}

int
gso_udp4_segment(struct rte_mbuf *pkt,
		uint16_t gso_size,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	uint16_t pyld_unit_size, hdr_offset;
	uint16_t frag_off;
	int ret;

	/* Don't process the fragmented packet */
	ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr *,
			pkt->l2_len);
	frag_off = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
	if (unlikely(IS_FRAGMENTED(frag_off))) {
		pkts_out[0] = pkt;
		return 1;
	}

	/*
	 * UDP fragmentation is the same as IP fragmentation.
	 * Except the first one, other output packets just have l2
	 * and l3 headers.
	 */
	hdr_offset = pkt->l2_len + pkt->l3_len;

	/* Don't process the packet without data. */
	if (unlikely(hdr_offset + pkt->l4_len >= pkt->pkt_len)) {
		pkts_out[0] = pkt;
		return 1;
	}

	pyld_unit_size = gso_size - hdr_offset;

	/* Segment the payload */
	ret = gso_do_segment(pkt, hdr_offset, pyld_unit_size, direct_pool,
			indirect_pool, pkts_out, nb_pkts_out);
	if (ret > 1)
		update_ipv4_udp_headers(pkt, pkts_out, ret);

	return ret;
}
