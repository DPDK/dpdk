/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Intel Corporation. All rights reserved.
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

#include "gso_common.h"
#include "gso_tcp4.h"

static void
update_ipv4_tcp_headers(struct rte_mbuf *pkt, uint8_t ipid_delta,
		struct rte_mbuf **segs, uint16_t nb_segs)
{
	struct ipv4_hdr *ipv4_hdr;
	struct tcp_hdr *tcp_hdr;
	uint32_t sent_seq;
	uint16_t id, tail_idx, i;
	uint16_t l3_offset = pkt->l2_len;
	uint16_t l4_offset = l3_offset + pkt->l3_len;

	ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char*) +
			l3_offset);
	tcp_hdr = (struct tcp_hdr *)((char *)ipv4_hdr + pkt->l3_len);
	id = rte_be_to_cpu_16(ipv4_hdr->packet_id);
	sent_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
	tail_idx = nb_segs - 1;

	for (i = 0; i < nb_segs; i++) {
		update_ipv4_header(segs[i], l3_offset, id);
		update_tcp_header(segs[i], l4_offset, sent_seq, i < tail_idx);
		id += ipid_delta;
		sent_seq += (segs[i]->pkt_len - segs[i]->data_len);
	}
}

int
gso_tcp4_segment(struct rte_mbuf *pkt,
		uint16_t gso_size,
		uint8_t ipid_delta,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out)
{
	struct ipv4_hdr *ipv4_hdr;
	uint16_t pyld_unit_size, hdr_offset;
	uint16_t frag_off;
	int ret;

	/* Don't process the fragmented packet */
	ipv4_hdr = (struct ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			pkt->l2_len);
	frag_off = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
	if (unlikely(IS_FRAGMENTED(frag_off))) {
		pkts_out[0] = pkt;
		return 1;
	}

	/* Don't process the packet without data */
	hdr_offset = pkt->l2_len + pkt->l3_len + pkt->l4_len;
	if (unlikely(hdr_offset >= pkt->pkt_len)) {
		pkts_out[0] = pkt;
		return 1;
	}

	pyld_unit_size = gso_size - hdr_offset;

	/* Segment the payload */
	ret = gso_do_segment(pkt, hdr_offset, pyld_unit_size, direct_pool,
			indirect_pool, pkts_out, nb_pkts_out);
	if (ret > 1)
		update_ipv4_tcp_headers(pkt, ipid_delta, pkts_out, ret);

	return ret;
}
