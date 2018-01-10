/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <arpa/inet.h>

#include <rte_arp.h>

#define RARP_PKT_SIZE	64
int
rte_net_make_rarp_packet(struct rte_mbuf *mbuf, const struct ether_addr *mac)
{
	struct ether_hdr *eth_hdr;
	struct arp_hdr *rarp;

	if (mbuf->buf_len < RARP_PKT_SIZE)
		return -1;

	/* Ethernet header. */
	eth_hdr = rte_pktmbuf_mtod(mbuf, struct ether_hdr *);
	memset(eth_hdr->d_addr.addr_bytes, 0xff, ETHER_ADDR_LEN);
	ether_addr_copy(mac, &eth_hdr->s_addr);
	eth_hdr->ether_type = htons(ETHER_TYPE_RARP);

	/* RARP header. */
	rarp = (struct arp_hdr *)(eth_hdr + 1);
	rarp->arp_hrd = htons(ARP_HRD_ETHER);
	rarp->arp_pro = htons(ETHER_TYPE_IPv4);
	rarp->arp_hln = ETHER_ADDR_LEN;
	rarp->arp_pln = 4;
	rarp->arp_op  = htons(ARP_OP_REVREQUEST);

	ether_addr_copy(mac, &rarp->arp_data.arp_sha);
	ether_addr_copy(mac, &rarp->arp_data.arp_tha);
	memset(&rarp->arp_data.arp_sip, 0x00, 4);
	memset(&rarp->arp_data.arp_tip, 0x00, 4);

	mbuf->data_len = RARP_PKT_SIZE;
	mbuf->pkt_len = RARP_PKT_SIZE;

	return 0;
}
