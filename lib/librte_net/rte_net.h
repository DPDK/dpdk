/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
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
 * Skip IPv6 header extensions.
 *
 * This function skips all IPv6 extensions, returning size of
 * complete header including options and final protocol value.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * @param proto
 *   Protocol field of IPv6 header.
 * @param m
 *   The packet mbuf to be parsed.
 * @param off
 *   On input, must contain the offset to the first byte following
 *   IPv6 header, on output, contains offset to the first byte
 *   of next layer (after any IPv6 extension header)
 * @param frag
 *   Contains 1 in output if packet is an IPv6 fragment.
 * @return
 *   Protocol that follows IPv6 header.
 *   -1 if an error occurs during mbuf parsing.
 */
__rte_experimental
int
rte_net_skip_ip6_ext(uint16_t proto, const struct rte_mbuf *m, uint32_t *off,
	int *frag);

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
	/* Initialise ipv4_hdr to avoid false positive compiler warnings. */
	struct rte_ipv4_hdr *ipv4_hdr = NULL;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	struct rte_udp_hdr *udp_hdr;
	uint64_t inner_l3_offset = m->l2_len;

	/*
	 * Does packet set any of available offloads?
	 * Mainly it is required to avoid fragmented headers check if
	 * no offloads are requested.
	 */
	if (!(ol_flags & (PKT_TX_IP_CKSUM | PKT_TX_L4_MASK | PKT_TX_TCP_SEG)))
		return 0;

	if (ol_flags & (PKT_TX_OUTER_IPV4 | PKT_TX_OUTER_IPV6))
		inner_l3_offset += m->outer_l2_len + m->outer_l3_len;

	/*
	 * Check if headers are fragmented.
	 * The check could be less strict depending on which offloads are
	 * requested and headers to be used, but let's keep it simple.
	 */
	if (unlikely(rte_pktmbuf_data_len(m) <
		     inner_l3_offset + m->l3_len + m->l4_len))
		return -ENOTSUP;

	if (ol_flags & PKT_TX_IPV4) {
		ipv4_hdr = rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
				inner_l3_offset);

		if (ol_flags & PKT_TX_IP_CKSUM)
			ipv4_hdr->hdr_checksum = 0;
	}

	if ((ol_flags & PKT_TX_L4_MASK) == PKT_TX_UDP_CKSUM) {
		if (ol_flags & PKT_TX_IPV4) {
			udp_hdr = (struct rte_udp_hdr *)((char *)ipv4_hdr +
					m->l3_len);
			udp_hdr->dgram_cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
					ol_flags);
		} else {
			ipv6_hdr = rte_pktmbuf_mtod_offset(m,
				struct rte_ipv6_hdr *, inner_l3_offset);
			/* non-TSO udp */
			udp_hdr = rte_pktmbuf_mtod_offset(m,
					struct rte_udp_hdr *,
					inner_l3_offset + m->l3_len);
			udp_hdr->dgram_cksum = rte_ipv6_phdr_cksum(ipv6_hdr,
					ol_flags);
		}
	} else if ((ol_flags & PKT_TX_L4_MASK) == PKT_TX_TCP_CKSUM ||
			(ol_flags & PKT_TX_TCP_SEG)) {
		if (ol_flags & PKT_TX_IPV4) {
			/* non-TSO tcp or TSO */
			tcp_hdr = (struct rte_tcp_hdr *)((char *)ipv4_hdr +
					m->l3_len);
			tcp_hdr->cksum = rte_ipv4_phdr_cksum(ipv4_hdr,
					ol_flags);
		} else {
			ipv6_hdr = rte_pktmbuf_mtod_offset(m,
				struct rte_ipv6_hdr *, inner_l3_offset);
			/* non-TSO tcp or TSO */
			tcp_hdr = rte_pktmbuf_mtod_offset(m,
					struct rte_tcp_hdr *,
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
