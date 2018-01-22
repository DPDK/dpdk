/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <arpa/inet.h>

#include <rte_arp.h>

#define RARP_PKT_SIZE	64
struct rte_mbuf * __rte_experimental
rte_net_make_rarp_packet(struct rte_mempool *mpool,
		const struct ether_addr *mac)
{
	struct ether_hdr *eth_hdr;
	struct arp_hdr *rarp;
	struct rte_mbuf *mbuf;

	if (mpool == NULL)
		return NULL;

	mbuf = rte_pktmbuf_alloc(mpool);
	if (mbuf == NULL)
		return NULL;

	eth_hdr = (struct ether_hdr *)rte_pktmbuf_append(mbuf, RARP_PKT_SIZE);
	if (eth_hdr == NULL) {
		rte_pktmbuf_free(mbuf);
		return NULL;
	}

	/* Ethernet header. */
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

	return mbuf;
}
