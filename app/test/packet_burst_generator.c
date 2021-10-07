/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_byteorder.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "packet_burst_generator.h"

#define UDP_SRC_PORT 1024
#define UDP_DST_PORT 1024


#define IP_DEFTTL  64   /* from RFC 1340. */

static void
copy_buf_to_pkt_segs(void *buf, unsigned len, struct rte_mbuf *pkt,
		unsigned offset)
{
	struct rte_mbuf *seg;
	void *seg_buf;
	unsigned copy_len;

	seg = pkt;
	while (offset >= seg->data_len) {
		offset -= seg->data_len;
		seg = seg->next;
	}
	copy_len = seg->data_len - offset;
	seg_buf = rte_pktmbuf_mtod_offset(seg, char *, offset);
	while (len > copy_len) {
		rte_memcpy(seg_buf, buf, (size_t) copy_len);
		len -= copy_len;
		buf = ((char *) buf + copy_len);
		seg = seg->next;
		seg_buf = rte_pktmbuf_mtod(seg, void *);
	}
	rte_memcpy(seg_buf, buf, (size_t) len);
}

static inline void
copy_buf_to_pkt(void *buf, unsigned len, struct rte_mbuf *pkt, unsigned offset)
{
	if (offset + len <= pkt->data_len) {
		rte_memcpy(rte_pktmbuf_mtod_offset(pkt, char *, offset), buf,
			   (size_t) len);
		return;
	}
	copy_buf_to_pkt_segs(buf, len, pkt, offset);
}

void
initialize_eth_header(struct rte_ether_hdr *eth_hdr,
		struct rte_ether_addr *src_mac,
		struct rte_ether_addr *dst_mac, uint16_t ether_type,
		uint8_t vlan_enabled, uint16_t van_id)
{
	rte_ether_addr_copy(dst_mac, &eth_hdr->dst_addr);
	rte_ether_addr_copy(src_mac, &eth_hdr->src_addr);

	if (vlan_enabled) {
		struct rte_vlan_hdr *vhdr = (struct rte_vlan_hdr *)(
			(uint8_t *)eth_hdr + sizeof(struct rte_ether_hdr));

		eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);

		vhdr->eth_proto =  rte_cpu_to_be_16(ether_type);
		vhdr->vlan_tci = van_id;
	} else {
		eth_hdr->ether_type = rte_cpu_to_be_16(ether_type);
	}
}

void
initialize_arp_header(struct rte_arp_hdr *arp_hdr,
		struct rte_ether_addr *src_mac,
		struct rte_ether_addr *dst_mac,
		uint32_t src_ip, uint32_t dst_ip,
		uint32_t opcode)
{
	arp_hdr->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
	arp_hdr->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	arp_hdr->arp_hlen = RTE_ETHER_ADDR_LEN;
	arp_hdr->arp_plen = sizeof(uint32_t);
	arp_hdr->arp_opcode = rte_cpu_to_be_16(opcode);
	rte_ether_addr_copy(src_mac, &arp_hdr->arp_data.arp_sha);
	arp_hdr->arp_data.arp_sip = src_ip;
	rte_ether_addr_copy(dst_mac, &arp_hdr->arp_data.arp_tha);
	arp_hdr->arp_data.arp_tip = dst_ip;
}

uint16_t
initialize_udp_header(struct rte_udp_hdr *udp_hdr, uint16_t src_port,
		uint16_t dst_port, uint16_t pkt_data_len)
{
	uint16_t pkt_len;

	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_udp_hdr));

	udp_hdr->src_port = rte_cpu_to_be_16(src_port);
	udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
	udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
	udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

	return pkt_len;
}

uint16_t
initialize_tcp_header(struct rte_tcp_hdr *tcp_hdr, uint16_t src_port,
		uint16_t dst_port, uint16_t pkt_data_len)
{
	uint16_t pkt_len;

	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_tcp_hdr));

	memset(tcp_hdr, 0, sizeof(struct rte_tcp_hdr));
	tcp_hdr->src_port = rte_cpu_to_be_16(src_port);
	tcp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
	tcp_hdr->data_off = (sizeof(struct rte_tcp_hdr) << 2) & 0xF0;

	return pkt_len;
}

uint16_t
initialize_sctp_header(struct rte_sctp_hdr *sctp_hdr, uint16_t src_port,
		uint16_t dst_port, uint16_t pkt_data_len)
{
	uint16_t pkt_len;

	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_udp_hdr));

	sctp_hdr->src_port = rte_cpu_to_be_16(src_port);
	sctp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
	sctp_hdr->tag = 0;
	sctp_hdr->cksum = 0; /* No SCTP checksum. */

	return pkt_len;
}

uint16_t
initialize_ipv6_header(struct rte_ipv6_hdr *ip_hdr, uint8_t *src_addr,
		uint8_t *dst_addr, uint16_t pkt_data_len)
{
	ip_hdr->vtc_flow = rte_cpu_to_be_32(0x60000000); /* Set version to 6. */
	ip_hdr->payload_len = rte_cpu_to_be_16(pkt_data_len);
	ip_hdr->proto = IPPROTO_UDP;
	ip_hdr->hop_limits = IP_DEFTTL;

	rte_memcpy(ip_hdr->src_addr, src_addr, sizeof(ip_hdr->src_addr));
	rte_memcpy(ip_hdr->dst_addr, dst_addr, sizeof(ip_hdr->dst_addr));

	return (uint16_t) (pkt_data_len + sizeof(struct rte_ipv6_hdr));
}

uint16_t
initialize_ipv4_header(struct rte_ipv4_hdr *ip_hdr, uint32_t src_addr,
		uint32_t dst_addr, uint16_t pkt_data_len)
{
	uint16_t pkt_len;
	unaligned_uint16_t *ptr16;
	uint32_t ip_cksum;

	/*
	 * Initialize IP header.
	 */
	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_ipv4_hdr));

	ip_hdr->version_ihl   = RTE_IPV4_VHL_DEF;
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
	ptr16 = (unaligned_uint16_t *)ip_hdr;
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

uint16_t
initialize_ipv4_header_proto(struct rte_ipv4_hdr *ip_hdr, uint32_t src_addr,
		uint32_t dst_addr, uint16_t pkt_data_len, uint8_t proto)
{
	uint16_t pkt_len;
	unaligned_uint16_t *ptr16;
	uint32_t ip_cksum;

	/*
	 * Initialize IP header.
	 */
	pkt_len = (uint16_t) (pkt_data_len + sizeof(struct rte_ipv4_hdr));

	ip_hdr->version_ihl   = RTE_IPV4_VHL_DEF;
	ip_hdr->type_of_service   = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live   = IP_DEFTTL;
	ip_hdr->next_proto_id = proto;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length   = rte_cpu_to_be_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(src_addr);
	ip_hdr->dst_addr = rte_cpu_to_be_32(dst_addr);

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t *)ip_hdr;
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


int
generate_packet_burst(struct rte_mempool *mp, struct rte_mbuf **pkts_burst,
		struct rte_ether_hdr *eth_hdr, uint8_t vlan_enabled,
		void *ip_hdr, uint8_t ipv4, struct rte_udp_hdr *udp_hdr,
		int nb_pkt_per_burst, uint8_t pkt_len, uint8_t nb_pkt_segs)
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

		pkt->data_len = pkt_len;
		pkt_seg = pkt;
		for (i = 1; i < nb_pkt_segs; i++) {
			pkt_seg->next = rte_pktmbuf_alloc(mp);
			if (pkt_seg->next == NULL) {
				pkt->nb_segs = i;
				rte_pktmbuf_free(pkt);
				goto nomore_mbuf;
			}
			pkt_seg = pkt_seg->next;
			pkt_seg->data_len = pkt_len;
		}
		pkt_seg->next = NULL; /* Last segment of packet. */

		/*
		 * Copy headers in first packet segment(s).
		 */
		if (vlan_enabled)
			eth_hdr_size = sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_vlan_hdr);
		else
			eth_hdr_size = sizeof(struct rte_ether_hdr);

		copy_buf_to_pkt(eth_hdr, eth_hdr_size, pkt, 0);

		if (ipv4) {
			copy_buf_to_pkt(ip_hdr, sizeof(struct rte_ipv4_hdr),
				pkt, eth_hdr_size);
			copy_buf_to_pkt(udp_hdr, sizeof(*udp_hdr), pkt,
				eth_hdr_size + sizeof(struct rte_ipv4_hdr));
		} else {
			copy_buf_to_pkt(ip_hdr, sizeof(struct rte_ipv6_hdr),
				pkt, eth_hdr_size);
			copy_buf_to_pkt(udp_hdr, sizeof(*udp_hdr), pkt,
				eth_hdr_size + sizeof(struct rte_ipv6_hdr));
		}

		/*
		 * Complete first mbuf of packet and append it to the
		 * burst of packets to be transmitted.
		 */
		pkt->nb_segs = nb_pkt_segs;
		pkt->pkt_len = pkt_len;
		pkt->l2_len = eth_hdr_size;

		if (ipv4) {
			pkt->vlan_tci  = RTE_ETHER_TYPE_IPV4;
			pkt->l3_len = sizeof(struct rte_ipv4_hdr);
		} else {
			pkt->vlan_tci  = RTE_ETHER_TYPE_IPV6;
			pkt->l3_len = sizeof(struct rte_ipv6_hdr);
		}

		pkts_burst[nb_pkt] = pkt;
	}

	return nb_pkt;
}

int
generate_packet_burst_proto(struct rte_mempool *mp,
		struct rte_mbuf **pkts_burst, struct rte_ether_hdr *eth_hdr,
		uint8_t vlan_enabled, void *ip_hdr,
		uint8_t ipv4, uint8_t proto, void *proto_hdr,
		int nb_pkt_per_burst, uint8_t pkt_len, uint8_t nb_pkt_segs)
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

		pkt->data_len = pkt_len;
		pkt_seg = pkt;
		for (i = 1; i < nb_pkt_segs; i++) {
			pkt_seg->next = rte_pktmbuf_alloc(mp);
			if (pkt_seg->next == NULL) {
				pkt->nb_segs = i;
				rte_pktmbuf_free(pkt);
				goto nomore_mbuf;
			}
			pkt_seg = pkt_seg->next;
			pkt_seg->data_len = pkt_len;
		}
		pkt_seg->next = NULL; /* Last segment of packet. */

		/*
		 * Copy headers in first packet segment(s).
		 */
		if (vlan_enabled)
			eth_hdr_size = sizeof(struct rte_ether_hdr) +
				sizeof(struct rte_vlan_hdr);
		else
			eth_hdr_size = sizeof(struct rte_ether_hdr);

		copy_buf_to_pkt(eth_hdr, eth_hdr_size, pkt, 0);

		if (ipv4) {
			copy_buf_to_pkt(ip_hdr, sizeof(struct rte_ipv4_hdr),
					pkt, eth_hdr_size);
			switch (proto) {
			case IPPROTO_UDP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_udp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv4_hdr));
				break;
			case IPPROTO_TCP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_tcp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv4_hdr));
				break;
			case IPPROTO_SCTP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_sctp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv4_hdr));
				break;
			default:
				break;
			}
		} else {
			copy_buf_to_pkt(ip_hdr, sizeof(struct rte_ipv6_hdr),
					pkt, eth_hdr_size);
			switch (proto) {
			case IPPROTO_UDP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_udp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv6_hdr));
				break;
			case IPPROTO_TCP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_tcp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv6_hdr));
				break;
			case IPPROTO_SCTP:
				copy_buf_to_pkt(proto_hdr,
					sizeof(struct rte_sctp_hdr), pkt,
					eth_hdr_size +
						sizeof(struct rte_ipv6_hdr));
				break;
			default:
				break;
			}
		}

		/*
		 * Complete first mbuf of packet and append it to the
		 * burst of packets to be transmitted.
		 */
		pkt->nb_segs = nb_pkt_segs;
		pkt->pkt_len = pkt_len;
		pkt->l2_len = eth_hdr_size;

		if (ipv4) {
			pkt->vlan_tci  = RTE_ETHER_TYPE_IPV4;
			pkt->l3_len = sizeof(struct rte_ipv4_hdr);
		} else {
			pkt->vlan_tci  = RTE_ETHER_TYPE_IPV6;
			pkt->l3_len = sizeof(struct rte_ipv6_hdr);
		}

		pkts_burst[nb_pkt] = pkt;
	}

	return nb_pkt;
}
