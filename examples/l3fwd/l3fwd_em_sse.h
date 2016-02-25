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

#ifndef __L3FWD_EM_SSE_H__
#define __L3FWD_EM_SSE_H__

#define MASK_ALL_PKTS   0xff
#define EXCLUDE_1ST_PKT 0xfe
#define EXCLUDE_2ND_PKT 0xfd
#define EXCLUDE_3RD_PKT 0xfb
#define EXCLUDE_4TH_PKT 0xf7
#define EXCLUDE_5TH_PKT 0xef
#define EXCLUDE_6TH_PKT 0xdf
#define EXCLUDE_7TH_PKT 0xbf
#define EXCLUDE_8TH_PKT 0x7f

static inline void
simple_ipv4_fwd_8pkts(struct rte_mbuf *m[8], uint8_t portid,
			struct lcore_conf *qconf)
{
	struct ether_hdr *eth_hdr[8];
	struct ipv4_hdr *ipv4_hdr[8];
	uint8_t dst_port[8];
	int32_t ret[8];
	union ipv4_5tuple_host key[8];
	__m128i data[8];

	eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *);
	eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *);
	eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *);
	eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *);
	eth_hdr[4] = rte_pktmbuf_mtod(m[4], struct ether_hdr *);
	eth_hdr[5] = rte_pktmbuf_mtod(m[5], struct ether_hdr *);
	eth_hdr[6] = rte_pktmbuf_mtod(m[6], struct ether_hdr *);
	eth_hdr[7] = rte_pktmbuf_mtod(m[7], struct ether_hdr *);

	/* Handle IPv4 headers.*/
	ipv4_hdr[0] = rte_pktmbuf_mtod_offset(m[0], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[1] = rte_pktmbuf_mtod_offset(m[1], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[2] = rte_pktmbuf_mtod_offset(m[2], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[3] = rte_pktmbuf_mtod_offset(m[3], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[4] = rte_pktmbuf_mtod_offset(m[4], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[5] = rte_pktmbuf_mtod_offset(m[5], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[6] = rte_pktmbuf_mtod_offset(m[6], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));
	ipv4_hdr[7] = rte_pktmbuf_mtod_offset(m[7], struct ipv4_hdr *,
					      sizeof(struct ether_hdr));

#ifdef DO_RFC_1812_CHECKS
	/* Check to make sure the packet is valid (RFC1812) */
	uint8_t valid_mask = MASK_ALL_PKTS;

	if (is_valid_ipv4_pkt(ipv4_hdr[0], m[0]->pkt_len) < 0) {
		rte_pktmbuf_free(m[0]);
		valid_mask &= EXCLUDE_1ST_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[1], m[1]->pkt_len) < 0) {
		rte_pktmbuf_free(m[1]);
		valid_mask &= EXCLUDE_2ND_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[2], m[2]->pkt_len) < 0) {
		rte_pktmbuf_free(m[2]);
		valid_mask &= EXCLUDE_3RD_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[3], m[3]->pkt_len) < 0) {
		rte_pktmbuf_free(m[3]);
		valid_mask &= EXCLUDE_4TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[4], m[4]->pkt_len) < 0) {
		rte_pktmbuf_free(m[4]);
		valid_mask &= EXCLUDE_5TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[5], m[5]->pkt_len) < 0) {
		rte_pktmbuf_free(m[5]);
		valid_mask &= EXCLUDE_6TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[6], m[6]->pkt_len) < 0) {
		rte_pktmbuf_free(m[6]);
		valid_mask &= EXCLUDE_7TH_PKT;
	}
	if (is_valid_ipv4_pkt(ipv4_hdr[7], m[7]->pkt_len) < 0) {
		rte_pktmbuf_free(m[7]);
		valid_mask &= EXCLUDE_8TH_PKT;
	}
	if (unlikely(valid_mask != MASK_ALL_PKTS)) {
		if (valid_mask == 0) {
			return;
		} else {
			uint8_t i = 0;

			for (i = 0; i < 8; i++) {
				if ((0x1 << i) & valid_mask) {
					l3fwd_em_simple_forward(m[i],
							portid, qconf);
				}
			}
			return;
		}
	}
#endif /* End of #ifdef DO_RFC_1812_CHECKS */

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

	key[0].xmm = _mm_and_si128(data[0], mask0);
	key[1].xmm = _mm_and_si128(data[1], mask0);
	key[2].xmm = _mm_and_si128(data[2], mask0);
	key[3].xmm = _mm_and_si128(data[3], mask0);
	key[4].xmm = _mm_and_si128(data[4], mask0);
	key[5].xmm = _mm_and_si128(data[5], mask0);
	key[6].xmm = _mm_and_si128(data[6], mask0);
	key[7].xmm = _mm_and_si128(data[7], mask0);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
				&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_multi(qconf->ipv4_lookup_struct, &key_array[0], 8, ret);
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

#ifdef DO_RFC_1812_CHECKS
	/* Update time to live and header checksum */
	--(ipv4_hdr[0]->time_to_live);
	--(ipv4_hdr[1]->time_to_live);
	--(ipv4_hdr[2]->time_to_live);
	--(ipv4_hdr[3]->time_to_live);
	++(ipv4_hdr[0]->hdr_checksum);
	++(ipv4_hdr[1]->hdr_checksum);
	++(ipv4_hdr[2]->hdr_checksum);
	++(ipv4_hdr[3]->hdr_checksum);
	--(ipv4_hdr[4]->time_to_live);
	--(ipv4_hdr[5]->time_to_live);
	--(ipv4_hdr[6]->time_to_live);
	--(ipv4_hdr[7]->time_to_live);
	++(ipv4_hdr[4]->hdr_checksum);
	++(ipv4_hdr[5]->hdr_checksum);
	++(ipv4_hdr[6]->hdr_checksum);
	++(ipv4_hdr[7]->hdr_checksum);
#endif

	/* dst addr */
	*(uint64_t *)&eth_hdr[0]->d_addr = dest_eth_addr[dst_port[0]];
	*(uint64_t *)&eth_hdr[1]->d_addr = dest_eth_addr[dst_port[1]];
	*(uint64_t *)&eth_hdr[2]->d_addr = dest_eth_addr[dst_port[2]];
	*(uint64_t *)&eth_hdr[3]->d_addr = dest_eth_addr[dst_port[3]];
	*(uint64_t *)&eth_hdr[4]->d_addr = dest_eth_addr[dst_port[4]];
	*(uint64_t *)&eth_hdr[5]->d_addr = dest_eth_addr[dst_port[5]];
	*(uint64_t *)&eth_hdr[6]->d_addr = dest_eth_addr[dst_port[6]];
	*(uint64_t *)&eth_hdr[7]->d_addr = dest_eth_addr[dst_port[7]];

	/* src addr */
	ether_addr_copy(&ports_eth_addr[dst_port[0]], &eth_hdr[0]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[1]], &eth_hdr[1]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[2]], &eth_hdr[2]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[3]], &eth_hdr[3]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[4]], &eth_hdr[4]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[5]], &eth_hdr[5]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[6]], &eth_hdr[6]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[7]], &eth_hdr[7]->s_addr);

	send_single_packet(qconf, m[0], (uint8_t)dst_port[0]);
	send_single_packet(qconf, m[1], (uint8_t)dst_port[1]);
	send_single_packet(qconf, m[2], (uint8_t)dst_port[2]);
	send_single_packet(qconf, m[3], (uint8_t)dst_port[3]);
	send_single_packet(qconf, m[4], (uint8_t)dst_port[4]);
	send_single_packet(qconf, m[5], (uint8_t)dst_port[5]);
	send_single_packet(qconf, m[6], (uint8_t)dst_port[6]);
	send_single_packet(qconf, m[7], (uint8_t)dst_port[7]);
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

static inline void
simple_ipv6_fwd_8pkts(struct rte_mbuf *m[8], uint8_t portid,
			struct lcore_conf *qconf)
{
	struct ether_hdr *eth_hdr[8];
	__attribute__((unused)) struct ipv6_hdr *ipv6_hdr[8];
	uint8_t dst_port[8];
	int32_t ret[8];
	union ipv6_5tuple_host key[8];

	eth_hdr[0] = rte_pktmbuf_mtod(m[0], struct ether_hdr *);
	eth_hdr[1] = rte_pktmbuf_mtod(m[1], struct ether_hdr *);
	eth_hdr[2] = rte_pktmbuf_mtod(m[2], struct ether_hdr *);
	eth_hdr[3] = rte_pktmbuf_mtod(m[3], struct ether_hdr *);
	eth_hdr[4] = rte_pktmbuf_mtod(m[4], struct ether_hdr *);
	eth_hdr[5] = rte_pktmbuf_mtod(m[5], struct ether_hdr *);
	eth_hdr[6] = rte_pktmbuf_mtod(m[6], struct ether_hdr *);
	eth_hdr[7] = rte_pktmbuf_mtod(m[7], struct ether_hdr *);

	/* Handle IPv6 headers.*/
	ipv6_hdr[0] = rte_pktmbuf_mtod_offset(m[0], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[1] = rte_pktmbuf_mtod_offset(m[1], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[2] = rte_pktmbuf_mtod_offset(m[2], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[3] = rte_pktmbuf_mtod_offset(m[3], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[4] = rte_pktmbuf_mtod_offset(m[4], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[5] = rte_pktmbuf_mtod_offset(m[5], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[6] = rte_pktmbuf_mtod_offset(m[6], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));
	ipv6_hdr[7] = rte_pktmbuf_mtod_offset(m[7], struct ipv6_hdr *,
					      sizeof(struct ether_hdr));

	get_ipv6_5tuple(m[0], mask1, mask2, &key[0]);
	get_ipv6_5tuple(m[1], mask1, mask2, &key[1]);
	get_ipv6_5tuple(m[2], mask1, mask2, &key[2]);
	get_ipv6_5tuple(m[3], mask1, mask2, &key[3]);
	get_ipv6_5tuple(m[4], mask1, mask2, &key[4]);
	get_ipv6_5tuple(m[5], mask1, mask2, &key[5]);
	get_ipv6_5tuple(m[6], mask1, mask2, &key[6]);
	get_ipv6_5tuple(m[7], mask1, mask2, &key[7]);

	const void *key_array[8] = {&key[0], &key[1], &key[2], &key[3],
				&key[4], &key[5], &key[6], &key[7]};

	rte_hash_lookup_multi(qconf->ipv6_lookup_struct, &key_array[0], 8, ret);
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

	/* dst addr */
	*(uint64_t *)&eth_hdr[0]->d_addr = dest_eth_addr[dst_port[0]];
	*(uint64_t *)&eth_hdr[1]->d_addr = dest_eth_addr[dst_port[1]];
	*(uint64_t *)&eth_hdr[2]->d_addr = dest_eth_addr[dst_port[2]];
	*(uint64_t *)&eth_hdr[3]->d_addr = dest_eth_addr[dst_port[3]];
	*(uint64_t *)&eth_hdr[4]->d_addr = dest_eth_addr[dst_port[4]];
	*(uint64_t *)&eth_hdr[5]->d_addr = dest_eth_addr[dst_port[5]];
	*(uint64_t *)&eth_hdr[6]->d_addr = dest_eth_addr[dst_port[6]];
	*(uint64_t *)&eth_hdr[7]->d_addr = dest_eth_addr[dst_port[7]];

	/* src addr */
	ether_addr_copy(&ports_eth_addr[dst_port[0]], &eth_hdr[0]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[1]], &eth_hdr[1]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[2]], &eth_hdr[2]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[3]], &eth_hdr[3]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[4]], &eth_hdr[4]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[5]], &eth_hdr[5]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[6]], &eth_hdr[6]->s_addr);
	ether_addr_copy(&ports_eth_addr[dst_port[7]], &eth_hdr[7]->s_addr);

	send_single_packet(qconf, m[0], (uint8_t)dst_port[0]);
	send_single_packet(qconf, m[1], (uint8_t)dst_port[1]);
	send_single_packet(qconf, m[2], (uint8_t)dst_port[2]);
	send_single_packet(qconf, m[3], (uint8_t)dst_port[3]);
	send_single_packet(qconf, m[4], (uint8_t)dst_port[4]);
	send_single_packet(qconf, m[5], (uint8_t)dst_port[5]);
	send_single_packet(qconf, m[6], (uint8_t)dst_port[6]);
	send_single_packet(qconf, m[7], (uint8_t)dst_port[7]);
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

		if (pkt_type & RTE_PTYPE_L3_IPV4) {
			simple_ipv4_fwd_8pkts(
				&pkts_burst[j], portid, qconf);
		} else if (pkt_type & RTE_PTYPE_L3_IPV6) {
			simple_ipv6_fwd_8pkts(&pkts_burst[j],
						portid, qconf);
		} else {
			l3fwd_em_simple_forward(pkts_burst[j], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+1], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+2], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+3], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+4], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+5], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+6], portid, qconf);
			l3fwd_em_simple_forward(pkts_burst[j+7], portid, qconf);
		}
	}
	for (; j < nb_rx ; j++)
		l3fwd_em_simple_forward(pkts_burst[j], portid, qconf);
}

#endif /* __L3FWD_EM_SSE_H__ */
