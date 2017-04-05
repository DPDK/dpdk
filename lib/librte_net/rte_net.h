/*-
 *   BSD LICENSE
 *
 *   Copyright 2016 6WIND S.A.
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

#ifndef _RTE_NET_PTYPE_H_
#define _RTE_NET_PTYPE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

/**
 * Structure containing header lengths associated to a packet, filled
 * by rte_net_get_ptype().
 */
struct rte_net_hdr_lens {
	uint8_t l2_len;
	uint8_t l3_len;
	uint8_t l4_len;
	uint8_t tunnel_len;
	uint8_t inner_l2_len;
	uint8_t inner_l3_len;
	uint8_t inner_l4_len;
};

/**
 * Parse an Ethernet packet to get its packet type.
 *
 * This function parses the network headers in mbuf data and return its
 * packet type.
 *
 * If it is provided by the user, it also fills a rte_net_hdr_lens
 * structure that contains the lengths of the parsed network
 * headers. Each length field is valid only if the associated packet
 * type is set. For instance, hdr_lens->l2_len is valid only if
 * (retval & RTE_PTYPE_L2_MASK) != RTE_PTYPE_UNKNOWN.
 *
 * Supported packet types are:
 *   L2: Ether, Vlan, QinQ
 *   L3: IPv4, IPv6
 *   L4: TCP, UDP, SCTP
 *   Tunnels: IPv4, IPv6, Gre, Nvgre
 *
 * @param m
 *   The packet mbuf to be parsed.
 * @param hdr_lens
 *   A pointer to a structure where the header lengths will be returned,
 *   or NULL.
 * @param layers
 *   List of layers to parse. The function will stop at the first
 *   empty layer. Examples:
 *   - To parse all known layers, use RTE_PTYPE_ALL_MASK.
 *   - To parse only L2 and L3, use RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK
 * @return
 *   The packet type of the packet.
 */
uint32_t rte_net_get_ptype(const struct rte_mbuf *m,
	struct rte_net_hdr_lens *hdr_lens, uint32_t layers);

/**
 * Prepare pseudo header checksum
 *
 * This function prepares pseudo header checksum for TSO and non-TSO tcp/udp in
 * provided mbufs packet data and based on the requested offload flags.
 *
 * - for non-TSO tcp/udp packets full pseudo-header checksum is counted and set
 *   in packet data,
 * - for TSO the IP payload length is not included in pseudo header.
 *
 * This function expects that used headers are in the first data segment of
 * mbuf, are not fragmented and can be safely modified.
 *
 * @param m
 *   The packet mbuf to be fixed.
 * @param ol_flags
 *   TX offloads flags to use with this packet.
 * @return
 *   0 if checksum is initialized properly
 */
static inline int
rte_net_intel_cksum_flags_prepare(struct rte_mbuf *m, uint64_t ol_flags)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	struct tcp_hdr *tcp_hdr;
	struct udp_hdr *udp_hdr;
	uint64_t inner_l3_offset = m->l2_len;

	if ((ol_flags & PKT_TX_OUTER_IP_CKSUM) ||
		(ol_flags & PKT_TX_OUTER_IPV6))
		inner_l3_offset += m->outer_l2_len + m->outer_l3_len;

	if ((ol_flags & PKT_TX_UDP_CKSUM) == PKT_TX_UDP_CKSUM) {
		if (ol_flags & PKT_TX_IPV4) {
			ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
					inner_l3_offset);

			if (ol_flags & PKT_TX_IP_CKSUM)
				ipv4_hdr->hdr_checksum = 0;

			udp_hdr = (struct udp_hdr *)((char *)ipv4_hdr +
					m->l3_len);
			udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
					ol_flags);
		} else {
			ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *,
					inner_l3_offset);
			/* non-TSO udp */
			udp_hdr = rte_pktmbuf_mtod_offset(m, struct udp_hdr *,
					inner_l3_offset + m->l3_len);
			udp_hdr->dgram_cksum = rte_ipv6_phdr_cksum(ipv6_hdr,
					ol_flags);
		}
	} else if ((ol_flags & PKT_TX_TCP_CKSUM) ||
			(ol_flags & PKT_TX_TCP_SEG)) {
		if (ol_flags & PKT_TX_IPV4) {
			ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct ipv4_hdr *,
					inner_l3_offset);

			if (ol_flags & PKT_TX_IP_CKSUM)
				ipv4_hdr->hdr_checksum = 0;

			/* non-TSO tcp or TSO */
			tcp_hdr = (struct tcp_hdr *)((char *)ipv4_hdr +
					m->l3_len);
			tcp_hdr->cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
					ol_flags);
		} else {
			ipv6_hdr = rte_pktmbuf_mtod_offset(m, struct ipv6_hdr *,
					inner_l3_offset);
			/* non-TSO tcp or TSO */
			tcp_hdr = rte_pktmbuf_mtod_offset(m, struct tcp_hdr *,
					inner_l3_offset + m->l3_len);
			tcp_hdr->cksum = rte_ipv6_phdr_cksum(ipv6_hdr,
					ol_flags);
		}
	}

	return 0;
}

/**
 * Prepare pseudo header checksum
 *
 * This function prepares pseudo header checksum for TSO and non-TSO tcp/udp in
 * provided mbufs packet data.
 *
 * - for non-TSO tcp/udp packets full pseudo-header checksum is counted and set
 *   in packet data,
 * - for TSO the IP payload length is not included in pseudo header.
 *
 * This function expects that used headers are in the first data segment of
 * mbuf, are not fragmented and can be safely modified.
 *
 * @param m
 *   The packet mbuf to be fixed.
 * @return
 *   0 if checksum is initialized properly
 */
static inline int
rte_net_intel_cksum_prepare(struct rte_mbuf *m)
{
	return rte_net_intel_cksum_flags_prepare(m, m->ol_flags);
}

#ifdef __cplusplus
}
#endif


#endif /* _RTE_NET_PTYPE_H_ */
