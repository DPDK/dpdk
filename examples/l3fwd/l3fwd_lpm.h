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

#ifndef __L3FWD_LPM_H__
#define __L3FWD_LPM_H__

static __rte_always_inline void
l3fwd_lpm_simple_forward(struct rte_mbuf *m, uint8_t portid,
		struct lcore_conf *qconf)
{
	struct ether_hdr *eth_hdr;
	struct ipv4_hdr *ipv4_hdr;
	uint16_t dst_port;

	eth_hdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	if (RTE_ETH_IS_IPV4_HDR(m->packet_type)) {
		/* Handle IPv4 headers.*/
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
						   sizeof(struct ether_hdr));

#ifdef DO_RFC_1812_CHECKS
		/* Check to make sure the packet is valid (RFC1812) */
		if (is_valid_ipv4_pkt(ipv4_hdr, m->pkt_len) < 0) {
			rte_pktmbuf_free(m);
			return;
		}
#endif
		 dst_port = lpm_get_ipv4_dst_port(ipv4_hdr, portid,
						qconf->ipv4_lookup_struct);

		if (dst_port >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port) == 0)
			dst_port = portid;

#ifdef DO_RFC_1812_CHECKS
		/* Update time to live and header checksum */
		--(ipv4_hdr->time_to_live);
		++(ipv4_hdr->hdr_checksum);
#endif
		/* dst addr */
		*(uint64_t *)&eth_hdr->d_addr = dest_eth_addr[dst_port];

		/* src addr */
		ether_addr_copy(&ports_eth_addr[dst_port], &eth_hdr->s_addr);

		send_single_packet(qconf, m, dst_port);
	} else if (RTE_ETH_IS_IPV6_HDR(m->packet_type)) {
		/* Handle IPv6 headers.*/
		struct ipv6_hdr *ipv6_hdr;

		ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *,
						   sizeof(struct ether_hdr));

		dst_port = lpm_get_ipv6_dst_port(ipv6_hdr, portid,
					qconf->ipv6_lookup_struct);

		if (dst_port >= RTE_MAX_ETHPORTS ||
			(enabled_port_mask & 1 << dst_port) == 0)
			dst_port = portid;

		/* dst addr */
		*(uint64_t *)&eth_hdr->d_addr = dest_eth_addr[dst_port];

		/* src addr */
		ether_addr_copy(&ports_eth_addr[dst_port], &eth_hdr->s_addr);

		send_single_packet(qconf, m, dst_port);
	} else {
		/* Free the mbuf that contains non-IPV4/IPV6 packet */
		rte_pktmbuf_free(m);
	}
}

static inline void
l3fwd_lpm_no_opt_send_packets(int nb_rx, struct rte_mbuf **pkts_burst,
				uint8_t portid, struct lcore_conf *qconf)
{
	int32_t j;

	/* Prefetch first packets */
	for (j = 0; j < PREFETCH_OFFSET && j < nb_rx; j++)
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[j], void *));

	/* Prefetch and forward already prefetched packets. */
	for (j = 0; j < (nb_rx - PREFETCH_OFFSET); j++) {
		rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[
				j + PREFETCH_OFFSET], void *));
		l3fwd_lpm_simple_forward(pkts_burst[j], portid, qconf);
	}

	/* Forward remaining prefetched packets */
	for (; j < nb_rx; j++)
		l3fwd_lpm_simple_forward(pkts_burst[j], portid, qconf);
}

#endif /* __L3FWD_LPM_H__ */
