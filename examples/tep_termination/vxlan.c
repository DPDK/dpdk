/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 * Redistributions of source code must retain the above copyright
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
#include <stdint.h>
#include <rte_mbuf.h>
#include <rte_hash_crc.h>
#include <rte_byteorder.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "main.h"
#include "vxlan.h"

/**
 * Parse an ethernet header to fill the ethertype, outer_l2_len, outer_l3_len and
 * ipproto. This function is able to recognize IPv4/IPv6 with one optional vlan
 * header.
 */
static void
parse_ethernet(struct ether_hdr *eth_hdr, union tunnel_offload_info *info,
		uint8_t *l4_proto)
{
	struct ipv4_hdr *ipv4_hdr;
	struct ipv6_hdr *ipv6_hdr;
	uint16_t ethertype;

	info->outer_l2_len = sizeof(struct ether_hdr);
	ethertype = rte_be_to_cpu_16(eth_hdr->ether_type);

	if (ethertype == ETHER_TYPE_VLAN) {
		struct vlan_hdr *vlan_hdr = (struct vlan_hdr *)(eth_hdr + 1);
		info->outer_l2_len  += sizeof(struct vlan_hdr);
		ethertype = rte_be_to_cpu_16(vlan_hdr->eth_proto);
	}

	switch (ethertype) {
	case ETHER_TYPE_IPv4:
		ipv4_hdr = (struct ipv4_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv4_hdr);
		*l4_proto = ipv4_hdr->next_proto_id;
		break;
	case ETHER_TYPE_IPv6:
		ipv6_hdr = (struct ipv6_hdr *)
			((char *)eth_hdr + info->outer_l2_len);
		info->outer_l3_len = sizeof(struct ipv6_hdr);
		*l4_proto = ipv6_hdr->proto;
		break;
	default:
		info->outer_l3_len = 0;
		*l4_proto = 0;
		break;
	}
}

int
decapsulation(struct rte_mbuf *pkt)
{
	uint8_t l4_proto = 0;
	uint16_t outer_header_len;
	struct udp_hdr *udp_hdr;
	union tunnel_offload_info info = { .data = 0 };
	struct ether_hdr *phdr = rte_pktmbuf_mtod(pkt, struct ether_hdr *);

	parse_ethernet(phdr, &info, &l4_proto);
	if (l4_proto != IPPROTO_UDP)
		return -1;

	udp_hdr = (struct udp_hdr *)((char *)phdr +
		info.outer_l2_len + info.outer_l3_len);

	/** check udp destination port, 4789 is the default vxlan port
	 * (rfc7348) or that the rx offload flag is set (i40e only
	 * currently)*/
	if (udp_hdr->dst_port != rte_cpu_to_be_16(DEFAULT_VXLAN_PORT) &&
			(pkt->ol_flags & (PKT_RX_TUNNEL_IPV4_HDR |
				PKT_RX_TUNNEL_IPV6_HDR)) == 0)
		return -1;
	outer_header_len = info.outer_l2_len + info.outer_l3_len
		+ sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr);

	rte_pktmbuf_adj(pkt, outer_header_len);

	return 0;
}

void
encapsulation(struct rte_mbuf *m, uint8_t queue_id)
{
	uint vport_id;
	uint64_t ol_flags = 0;
	uint32_t old_len = m->pkt_len, hash;
	struct ether_hdr *phdr = rte_pktmbuf_mtod(m, struct ether_hdr *);

	/*Allocate space for new ethernet, IPv4, UDP and VXLAN headers*/
	struct ether_hdr *pneth = (struct ether_hdr *) rte_pktmbuf_prepend(m,
		sizeof(struct ether_hdr) + sizeof(struct ipv4_hdr)
		+ sizeof(struct udp_hdr) + sizeof(struct vxlan_hdr));

	struct ipv4_hdr *ip = (struct ipv4_hdr *) &pneth[1];
	struct udp_hdr *udp = (struct udp_hdr *) &ip[1];
	struct vxlan_hdr *vxlan = (struct vxlan_hdr *) &udp[1];

	/* convert TX queue ID to vport ID */
	vport_id = queue_id - 1;

	/* replace original Ethernet header with ours */
	pneth = rte_memcpy(pneth, &app_l2_hdr[vport_id],
		sizeof(struct ether_hdr));

	/* copy in IP header */
	ip = rte_memcpy(ip, &app_ip_hdr[vport_id],
		sizeof(struct ipv4_hdr));
	ip->total_length = rte_cpu_to_be_16(m->data_len
				- sizeof(struct ether_hdr));

	/* outer IP checksum */
	ol_flags |= PKT_TX_OUTER_IP_CKSUM;
	ip->hdr_checksum = 0;

	m->outer_l2_len = sizeof(struct ether_hdr);
	m->outer_l3_len = sizeof(struct ipv4_hdr);

	m->ol_flags |= ol_flags;

	/*VXLAN HEADER*/
	vxlan->vx_flags = rte_cpu_to_be_32(VXLAN_HF_VNI);
	vxlan->vx_vni = rte_cpu_to_be_32(vxdev.out_key << 8);

	/*UDP HEADER*/
	udp->dgram_cksum = 0;
	udp->dgram_len = rte_cpu_to_be_16(old_len
				+ sizeof(struct udp_hdr)
				+ sizeof(struct vxlan_hdr));

	udp->dst_port = rte_cpu_to_be_16(vxdev.dst_port);
	hash = rte_hash_crc(phdr, 2 * ETHER_ADDR_LEN, phdr->ether_type);
	udp->src_port = rte_cpu_to_be_16((((uint64_t) hash * PORT_RANGE) >> 32)
					+ PORT_MIN);

	return;
}
