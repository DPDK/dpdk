/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _GSO_COMMON_H_
#define _GSO_COMMON_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#define IS_FRAGMENTED(frag_off) (((frag_off) & RTE_IPV4_HDR_OFFSET_MASK) != 0 \
		|| ((frag_off) & RTE_IPV4_HDR_MF_FLAG) == RTE_IPV4_HDR_MF_FLAG)

#define TCP_HDR_PSH_MASK ((uint8_t)0x08)
#define TCP_HDR_FIN_MASK ((uint8_t)0x01)

#define IS_IPV4_TCP(flag) (((flag) & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4)) == \
		(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4))

#define IS_IPV4_VXLAN_TCP4(flag) (((flag) & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4 | \
				RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_TUNNEL_MASK)) == \
		(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_OUTER_IPV4 | \
		 RTE_MBUF_F_TX_TUNNEL_VXLAN))

#define IS_IPV4_VXLAN_UDP4(flag) (((flag) & (RTE_MBUF_F_TX_UDP_SEG | RTE_MBUF_F_TX_IPV4 | \
				RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_TUNNEL_MASK)) == \
		(RTE_MBUF_F_TX_UDP_SEG | RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_OUTER_IPV4 | \
		 RTE_MBUF_F_TX_TUNNEL_VXLAN))

#define IS_IPV4_GRE_TCP4(flag) (((flag) & (RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4 | \
				RTE_MBUF_F_TX_OUTER_IPV4 | RTE_MBUF_F_TX_TUNNEL_MASK)) == \
		(RTE_MBUF_F_TX_TCP_SEG | RTE_MBUF_F_TX_IPV4 | RTE_MBUF_F_TX_OUTER_IPV4 | \
		 RTE_MBUF_F_TX_TUNNEL_GRE))

#define IS_IPV4_UDP(flag) (((flag) & (RTE_MBUF_F_TX_UDP_SEG | RTE_MBUF_F_TX_IPV4)) == \
		(RTE_MBUF_F_TX_UDP_SEG | RTE_MBUF_F_TX_IPV4))

/**
 * Internal function which updates the UDP header of a packet, following
 * segmentation. This is required to update the header's datagram length field.
 *
 * @param pkt
 *  The packet containing the UDP header.
 * @param udp_offset
 *  The offset of the UDP header from the start of the packet.
 */
static inline void
update_udp_header(struct rte_mbuf *pkt, uint16_t udp_offset)
{
	struct rte_udp_hdr *udp_hdr;

	udp_hdr = (struct rte_udp_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			udp_offset);
	udp_hdr->dgram_len = rte_cpu_to_be_16(pkt->pkt_len - udp_offset);
}

/**
 * Internal function which updates the TCP header of a packet, following
 * segmentation. This is required to update the header's 'sent' sequence
 * number, and also to clear 'PSH' and 'FIN' flags for non-tail segments.
 *
 * @param pkt
 *  The packet containing the TCP header.
 * @param l4_offset
 *  The offset of the TCP header from the start of the packet.
 * @param sent_seq
 *  The sent sequence number.
 * @param non-tail
 *  Indicates whether or not this is a tail segment.
 */
static inline void
update_tcp_header(struct rte_mbuf *pkt, uint16_t l4_offset, uint32_t sent_seq,
		uint8_t non_tail)
{
	struct rte_tcp_hdr *tcp_hdr;

	tcp_hdr = (struct rte_tcp_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			l4_offset);
	tcp_hdr->sent_seq = rte_cpu_to_be_32(sent_seq);
	if (likely(non_tail))
		tcp_hdr->tcp_flags &= (~(TCP_HDR_PSH_MASK |
					TCP_HDR_FIN_MASK));
}

/**
 * Internal function which updates the IPv4 header of a packet, following
 * segmentation. This is required to update the header's 'total_length' field,
 * to reflect the reduced length of the now-segmented packet. Furthermore, the
 * header's 'packet_id' field must be updated to reflect the new ID of the
 * now-segmented packet.
 *
 * @param pkt
 *  The packet containing the IPv4 header.
 * @param l3_offset
 *  The offset of the IPv4 header from the start of the packet.
 * @param id
 *  The new ID of the packet.
 */
static inline void
update_ipv4_header(struct rte_mbuf *pkt, uint16_t l3_offset, uint16_t id)
{
	struct rte_ipv4_hdr *ipv4_hdr;

	ipv4_hdr = (struct rte_ipv4_hdr *)(rte_pktmbuf_mtod(pkt, char *) +
			l3_offset);
	ipv4_hdr->total_length = rte_cpu_to_be_16(pkt->pkt_len - l3_offset);
	ipv4_hdr->packet_id = rte_cpu_to_be_16(id);
}

/**
 * Internal function which divides the input packet into small segments.
 * Each of the newly-created segments is organized as a two-segment MBUF,
 * where the first segment is a standard mbuf, which stores a copy of
 * packet header, and the second is an indirect mbuf which points to a
 * section of data in the input packet.
 *
 * @param pkt
 *  Packet to segment.
 * @param pkt_hdr_offset
 *  Packet header offset, measured in bytes.
 * @param pyld_unit_size
 *  The max payload length of a GSO segment.
 * @param direct_pool
 *  MBUF pool used for allocating direct buffers for output segments.
 * @param indirect_pool
 *  MBUF pool used for allocating indirect buffers for output segments.
 * @param pkts_out
 *  Pointer array used to keep the mbuf addresses of output segments. If
 *  the memory space in pkts_out is insufficient, gso_do_segment() fails
 *  and returns -EINVAL.
 * @param nb_pkts_out
 *  The max number of items that pkts_out can keep.
 *
 * @return
 *  - The number of segments created in the event of success.
 *  - Return -ENOMEM if run out of memory in MBUF pools.
 *  - Return -EINVAL for invalid parameters.
 */
int gso_do_segment(struct rte_mbuf *pkt,
		uint16_t pkt_hdr_offset,
		uint16_t pyld_unit_size,
		struct rte_mempool *direct_pool,
		struct rte_mempool *indirect_pool,
		struct rte_mbuf **pkts_out,
		uint16_t nb_pkts_out);
#endif
