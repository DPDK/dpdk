/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

#include <rte_byteorder.h>
#include <rte_mbuf.h>

#include "packet_burst_generator.h"

#define UDP_SRC_PORT 1024
#define UDP_DST_PORT 1024


#define IP_DEFTTL  64   /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static void
copy_buf_to_pkt_segs(void *buf, unsigned len, struct rte_mbuf *pkt,
		unsigned offset)
{
	struct rte_mbuf *seg;
	void *seg_buf;
	unsigned copy_len;

	seg = pkt;
	while (offset >= seg->pkt.data_len) {
		offset -= seg->pkt.data_len;
		seg = seg->pkt.next;
	}
	copy_len = seg->pkt.data_len - offset;
	seg_buf = ((char *) seg->pkt.data + offset);
	while (len > copy_len) {
		rte_memcpy(seg_buf, buf, (size_t) copy_len);
		len -= copy_len;
		buf = ((char *) buf + copy_len);
		seg = seg->pkt.next;
		seg_buf = seg->pkt.data;
	}
	rte_memcpy(seg_buf, buf, (size_t) len);
}

static inline void
copy_buf_to_pkt(void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
	if (offset + len <= pkt->pkt.data_len) {
		rte_memcpy(((char *) pkt->pkt.data + offset), buf, (size_t) len);
		return;
	}
	copy_buf_to_pkt_segs(buf, len, pkt, offset);
}


void
initialize_eth_header(struct ether_hdr *eth_hdr, struct ether_addr *src_mac,
		struct ether_addr *dst_mac, uint8_t vlan_enabled, uint16_t van_id)
{
	ether_addr_copy(dst_mac, &eth_hdr->d_addr);
	ether_addr_copy(src_mac, &eth_hdr->s_addr);

	if (vlan_enabled) {
		struct vlan_hdr *vhdr = (struct vlan_hdr *)((uint8_t *)eth_hdr +
				sizeof(struct ether_hdr));

		eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);

		vhdr->eth_proto =  rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		vhdr->vlan_tci = van_id;
	} else {
		eth_hdr->ether_type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	}

}

uint16_t
initialize_udp_header(struct udp_hdr *udp_hdr, uint16_t src_port,
		uint16_t dst_port, uint16_t pkt_data_len)
{
	uint16_t pkt_len;

	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct udp_hdr));

	udp_hdr->src_port = rte_cpu_to_be_16(src_port);
	udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
	udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
	udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

	return pkt_len;
}


uint16_t
initialize_ipv6_header(struct ipv6_hdr *ip_hdr, uint8_t *src_addr,
		uint8_t *dst_addr, uint16_t pkt_data_len)
{
	ip_hdr->vtc_flow = 0;
	ip_hdr->payload_len = pkt_data_len;
	ip_hdr->proto = IPPROTO_UDP;
	ip_hdr->hop_limits = IP_DEFTTL;

	rte_memcpy(ip_hdr->src_addr, src_addr, sizeof(ip_hdr->src_addr));
	rte_memcpy(ip_hdr->dst_addr, dst_addr, sizeof(ip_hdr->dst_addr));

	return (uint16_t) (pkt_data_len + sizeof(struct ipv6_hdr));
}

uint16_t
initialize_ipv4_header(struct ipv4_hdr *ip_hdr, uint32_t src_addr,
		uint32_t dst_addr, uint16_t pkt_data_len)
{
	uint16_t pkt_len;
	uint16_t *ptr16;
	uint32_t ip_cksum;

	/*
	 * Initialize IP header.
	 */
	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct ipv4_hdr));

	ip_hdr->version_ihl   = IP_VHL_DEF;
	ip_hdr->type_of_service   = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live   = IP_DEFTTL;
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length   = rte_cpu_to_be_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(src_addr);
	ip_hdr->dst_addr = rte_cpu_to_be_32(dst_addr);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (uint16_t *)ip_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0]; ip_cksum += ptr16[1];
	ip_cksum += ptr16[2]; ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6]; ip_cksum += ptr16[7];
	ip_cksum += ptr16[8]; ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) +
		(ip_cksum & 0x0000FFFF);
	ip_cksum %= 65536;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip_hdr->hdr_checksum = (uint16_t) ip_cksum;

	return pkt_len;
}



/*
 * The maximum number of segments per packet is used when creating
 * scattered transmit packets composed of a list of mbufs.
 */
#define RTE_MAX_SEGS_PER_PKT 255 /**< pkt.nb_segs is a 8-bit unsigned char. */

#define TXONLY_DEF_PACKET_LEN 64
#define TXONLY_DEF_PACKET_LEN_128 128

uint16_t tx_pkt_length = TXONLY_DEF_PACKET_LEN;
uint16_t tx_pkt_seg_lengths[RTE_MAX_SEGS_PER_PKT] = {
		TXONLY_DEF_PACKET_LEN_128,
};

uint8_t  tx_pkt_nb_segs = 1;

int
generate_packet_burst(struct rte_mempool *mp, struct rte_mbuf **pkts_burst,
		struct ether_hdr *eth_hdr, uint8_t vlan_enabled, void *ip_hdr,
		uint8_t ipv4, struct udp_hdr *udp_hdr, int nb_pkt_per_burst)
{
	int i, nb_pkt = 0;
	size_t eth_hdr_size;

	struct rte_mbuf *pkt_seg;
	struct rte_mbuf *pkt;

	for (nb_pkt = 0; nb_pkt < nb_pkt_per_burst; nb_pkt++) {
		pkt = rte_pktmbuf_alloc(mp);
		if (pkt == NULL) {
nomore_mbuf:
			if (nb_pkt == 0)
				return -1;
			break;
		}

		pkt->pkt.data_len = tx_pkt_seg_lengths[0];
		pkt_seg = pkt;
		for (i = 1; i < tx_pkt_nb_segs; i++) {
			pkt_seg->pkt.next = rte_pktmbuf_alloc(mp);
			if (pkt_seg->pkt.next == NULL) {
				pkt->pkt.nb_segs = i;
				rte_pktmbuf_free(pkt);
				goto nomore_mbuf;
			}
			pkt_seg = pkt_seg->pkt.next;
			pkt_seg->pkt.data_len = tx_pkt_seg_lengths[i];
		}
		pkt_seg->pkt.next = NULL; /* Last segment of packet. */

		/*
		 * Copy headers in first packet segment(s).
		 */
		if (vlan_enabled)
			eth_hdr_size = sizeof(struct ether_hdr) + sizeof(struct vlan_hdr);
		else
			eth_hdr_size = sizeof(struct ether_hdr);

		copy_buf_to_pkt(eth_hdr, eth_hdr_size, pkt, 0);

		if (ipv4) {
			copy_buf_to_pkt(ip_hdr, sizeof(struct ipv4_hdr), pkt, eth_hdr_size);
			copy_buf_to_pkt(udp_hdr, sizeof(*udp_hdr), pkt, eth_hdr_size +
					sizeof(struct ipv4_hdr));
		} else {
			copy_buf_to_pkt(ip_hdr, sizeof(struct ipv6_hdr), pkt, eth_hdr_size);
			copy_buf_to_pkt(udp_hdr, sizeof(*udp_hdr), pkt, eth_hdr_size +
					sizeof(struct ipv6_hdr));
		}

		/*
		 * Complete first mbuf of packet and append it to the
		 * burst of packets to be transmitted.
		 */
		pkt->pkt.nb_segs = tx_pkt_nb_segs;
		pkt->pkt.pkt_len = tx_pkt_length;
		pkt->pkt.vlan_macip.f.l2_len = eth_hdr_size;

		if (ipv4) {
			pkt->pkt.vlan_macip.f.vlan_tci  = ETHER_TYPE_IPv4;
			pkt->pkt.vlan_macip.f.l3_len = sizeof(struct ipv4_hdr);

			if (vlan_enabled)
				pkt->ol_flags = PKT_RX_IPV4_HDR | PKT_RX_VLAN_PKT;
			else
				pkt->ol_flags = PKT_RX_IPV4_HDR;
		} else {
			pkt->pkt.vlan_macip.f.vlan_tci  = ETHER_TYPE_IPv6;
			pkt->pkt.vlan_macip.f.l3_len = sizeof(struct ipv6_hdr);

			if (vlan_enabled)
				pkt->ol_flags = PKT_RX_IPV6_HDR | PKT_RX_VLAN_PKT;
			else
				pkt->ol_flags = PKT_RX_IPV6_HDR;
		}

		pkts_burst[nb_pkt] = pkt;
	}

	return nb_pkt;
}
