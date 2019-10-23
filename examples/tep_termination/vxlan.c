/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "main.h"
#include "vxlan.h"

static uint16_t
get_psd_sum(void *l3_hdr, uint16_t ethertype, uint64_t ol_flags)
{
	if (ethertype == RTE_ETHER_TYPE_IPV4)
		return rte_ipv4_phdr_cksum(l3_hdr, ol_flags);
	else /* assume ethertype == RTE_ETHER_TYPE_IPV6 */
		return rte_ipv6_phdr_cksum(l3_hdr, ol_flags);
}

/**
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
static void
parse_ethernet(struct rte_ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto)
{
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	uint16_t ethertype;

	info->outer_l2_len = sizeof(struct rte_ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == RTE_ETHER_TYPE_VLAN) {
		struct rte_vlan_hdr *vlan_hdr =
			(struct rte_vlan_hdr *)(eth_hdr + 1);
		info->outer_l2_len  += sizeof(struct rte_vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
	case RTE_ETHER_TYPE_IPV4:
		ipv4_hdr = (struct rte_ipv4_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct rte_ipv4_hdr);
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case RTE_ETHER_TYPE_IPV6:
		ipv6_hdr = (struct rte_ipv6_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct rte_ipv6_hdr);
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		info->outer_l3_len = 0;
		*l4_proto = 0;
		break;
	}
}

/**
 * Calculate the checksum of a packet in hardware
 */
static uint64_t
process_inner_cksums(struct rte_ether_hdr *eth_hdr,
		union tunnel_offload_info *info)
{
	void *l3_hdr = NULL;
	uint8_t l4_proto;
	uint16_t ethertype;
	struct rte_ipv4_hdr *ipv4_hdr;
	struct rte_ipv6_hdr *ipv6_hdr;
	struct rte_udp_hdr *udp_hdr;
	struct rte_tcp_hdr *tcp_hdr;
	struct rte_sctp_hdr *sctp_hdr;
	uint64_t ol_flags = 0;

	info->l2_len = sizeof(struct rte_ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == RTE_ETHER_TYPE_VLAN) {
		struct rte_vlan_hdr *vlan_hdr =
			(struct rte_vlan_hdr *)(eth_hdr + 1);
		info->l2_len  += sizeof(struct rte_vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	l3_hdr = (char *)eth_hdr + info->l2_len;

	if (ethertype == RTE_ETHER_TYPE_IPV4) {
		ipv4_hdr = (struct rte_ipv4_hdr *)l3_hdr;
		ipv4_hdr->hdr_checksum = 0;
		ol_flags |= PKT_TX_IPV4;
		ol_flags |= PKT_TX_IP_CKSUM;
		info->l3_len = sizeof(struct rte_ipv4_hdr);
		l4_proto = ipv4_hdr->next_proto_id;
	} else if (ethertype == RTE_ETHER_TYPE_IPV6) {
		ipv6_hdr = (struct rte_ipv6_hdr *)l3_hdr;
		info->l3_len = sizeof(struct rte_ipv6_hdr);
		l4_proto = ipv6_hdr->proto;
		ol_flags |= PKT_TX_IPV6;
	} else
		return 0; /* packet type not supported, nothing to do */

	if (l4_proto == IPPROTO_UDP) {
		udp_hdr = (struct rte_udp_hdr *)((char *)l3_hdr + info->l3_len);
		ol_flags |= PKT_TX_UDP_CKSUM;
		udp_hdr->dgram_cksum = get_psd_sum(l3_hdr,
				ethertype, ol_flags);
	} else if (l4_proto == IPPROTO_TCP) {
		tcp_hdr = (struct rte_tcp_hdr *)((char *)l3_hdr + info->l3_len);
		/* Put PKT_TX_TCP_SEG bit setting before get_psd_sum(), because
		 * it depends on PKT_TX_TCP_SEG to calculate pseudo-header
		 * checksum.
		 */
		if (tso_segsz != 0) {
			ol_flags |= PKT_TX_TCP_SEG;
			info->tso_segsz = tso_segsz;
			info->l4_len = (tcp_hdr->data_off & 0xf0) >> 2;
		}
		ol_flags |= PKT_TX_TCP_CKSUM;
		tcp_hdr->cksum = get_psd_sum(l3_hdr, ethertype, ol_flags);

	} else if (l4_proto == IPPROTO_SCTP) {
		sctp_hdr = (struct rte_sctp_hdr *)
			((char *)l3_hdr + info->l3_len);
		sctp_hdr->cksum = 0;
		ol_flags |= PKT_TX_SCTP_CKSUM;
	}

	return ol_flags;
}

int
decapsulation(struct rte_mbuf *pkt)
{
	uint8_t l4_proto = 0;
	uint16_t outer_header_len;
	struct rte_udp_hdr *udp_hdr;
	union tunnel_offload_info info = { .data = 0 };
	struct rte_ether_hdr *phdr =
		rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);

	parse_ethernet(phdr, &info, &l4_proto);

	if (l4_proto != IPPROTO_UDP)
		return -1;

	udp_hdr = (struct rte_udp_hdr *)((char *)phdr +
		info.outer_l2_len + info.outer_l3_len);

	/** check udp destination port, 4789 is the default vxlan port
	 * (rfc7348) or that the rx offload flag is set (i40e only
	 * currently)*/
	if (udp_hdr->dst_port != rte_cpu_to_be_16(RTE_VXLAN_DEFAULT_PORT) &&
		(pkt->packet_type & RTE_PTYPE_TUNNEL_MASK) == 0)
		return -1;
	outer_header_len = info.outer_l2_len + info.outer_l3_len
		+ sizeof(struct rte_udp_hdr) + sizeof(struct rte_vxlan_hdr);

	rte_pktmbuf_adj(pkt, outer_header_len);

	return 0;
}

void
encapsulation(struct rte_mbuf *m, uint8_t queue_id)
{
	uint vport_id;
	uint64_t ol_flags = 0;
	uint32_t old_len = m->pkt_len, hash;
	union tunnel_offload_info tx_offload = { .data = 0 };
	struct rte_ether_hdr *phdr =
		rte_pktmbuf_mtod(m, struct rte_ether_hdr *);

	/*Allocate space for new ethernet, IPv4, UDP and VXLAN headers*/
	struct rte_ether_hdr *pneth =
		(struct rte_ether_hdr *) rte_pktmbuf_prepend(m,
		sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)
		+ sizeof(struct rte_udp_hdr) + sizeof(struct rte_vxlan_hdr));

	struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *) &pneth[1];
	struct rte_udp_hdr *udp = (struct rte_udp_hdr *) &ip[1];
	struct rte_vxlan_hdr *vxlan = (struct rte_vxlan_hdr *) &udp[1];

	/* convert TX queue ID to vport ID */
	vport_id = queue_id - 1;

	/* replace original Ethernet header with ours */
	pneth = rte_memcpy(pneth, &app_l2_hdr[vport_id],
		sizeof(struct rte_ether_hdr));

	/* copy in IP header */
	ip = rte_memcpy(ip, &app_ip_hdr[vport_id],
		sizeof(struct rte_ipv4_hdr));
	ip->total_length = rte_cpu_to_be_16(m->pkt_len
				- sizeof(struct rte_ether_hdr));

	/* outer IP checksum */
	ol_flags |= PKT_TX_OUTER_IP_CKSUM;
	ip->hdr_checksum = 0;

	/* inner IP checksum offload */
	if (tx_checksum) {
		ol_flags |= process_inner_cksums(phdr, &tx_offload);
		m->l2_len = tx_offload.l2_len;
		m->l3_len = tx_offload.l3_len;
		m->l4_len = tx_offload.l4_len;
		m->l2_len += RTE_ETHER_VXLAN_HLEN;
	}

	m->outer_l2_len = sizeof(struct rte_ether_hdr);
	m->outer_l3_len = sizeof(struct rte_ipv4_hdr);

	ol_flags |= PKT_TX_TUNNEL_VXLAN;

	m->ol_flags |= ol_flags;
	m->tso_segsz = tx_offload.tso_segsz;

	/*VXLAN HEADER*/
	vxlan->vx_flags = rte_cpu_to_be_32(VXLAN_HF_VNI);
	vxlan->vx_vni = rte_cpu_to_be_32(vxdev.out_key << 8);

	/*UDP HEADER*/
	udp->dgram_cksum = 0;
	udp->dgram_len = rte_cpu_to_be_16(old_len
				+ sizeof(struct rte_udp_hdr)
				+ sizeof(struct rte_vxlan_hdr));

	udp->dst_port = rte_cpu_to_be_16(vxdev.dst_port);
	hash = rte_hash_crc(phdr, 2 * RTE_ETHER_ADDR_LEN, phdr->ether_type);
	udp->src_port = rte_cpu_to_be_16((((uint64_t) hash * PORT_RANGE) >> 32)
					+ PORT_MIN);

	return;
}
