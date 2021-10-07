/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_arp.h>
#include <rte_byteorder.h>

#define RARP_PKT_SIZE	64
struct rte_mbuf *
rte_net_make_rarp_packet(struct rte_mempool *mpool,
		const struct rte_ether_addr *mac)
{
	struct rte_ether_hdr *eth_hdr;
	struct rte_arp_hdr *rarp;
	struct rte_mbuf *mbuf;

	if (mpool == NULL)
		return NULL;

	mbuf = rte_pktmbuf_alloc(mpool);
	if (mbuf == NULL)
		return NULL;

	eth_hdr = (struct rte_ether_hdr *)
		rte_pktmbuf_append(mbuf, RARP_PKT_SIZE);
	if (eth_hdr == NULL) {
		rte_pktmbuf_free(mbuf);
		return NULL;
	}

	/* Ethernet header. */
	memset(eth_hdr->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);
	rte_ether_addr_copy(mac, &eth_hdr->src_addr);
	eth_hdr->ether_type = RTE_BE16(RTE_ETHER_TYPE_RARP);

	/* RARP header. */
	rarp = (struct rte_arp_hdr *)(eth_hdr + 1);
	rarp->arp_hardware = RTE_BE16(RTE_ARP_HRD_ETHER);
	rarp->arp_protocol = RTE_BE16(RTE_ETHER_TYPE_IPV4);
	rarp->arp_hlen = RTE_ETHER_ADDR_LEN;
	rarp->arp_plen = 4;
	rarp->arp_opcode  = RTE_BE16(RTE_ARP_OP_REVREQUEST);

	rte_ether_addr_copy(mac, &rarp->arp_data.arp_sha);
	rte_ether_addr_copy(mac, &rarp->arp_data.arp_tha);
	memset(&rarp->arp_data.arp_sip, 0x00, 4);
	memset(&rarp->arp_data.arp_tip, 0x00, 4);

	return mbuf;
}
