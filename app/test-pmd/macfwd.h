/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _MACFWD_H_
#define _MACFWD_H_

static inline void
do_macfwd(struct rte_mbuf *pkts_burst[], uint16_t nb_rx,
	  struct fwd_stream *fs)
{
	struct rte_ether_hdr *eth_hdr;
	uint64_t ol_flags = 0;
	uint64_t tx_offloads;
	struct rte_mbuf  *mb;
	struct rte_port *txp = &ports[fs->tx_port];
	uint16_t i;

	tx_offloads = txp->dev_conf.txmode.offloads;
	if (tx_offloads	& RTE_ETH_TX_OFFLOAD_VLAN_INSERT)
		ol_flags = RTE_MBUF_F_TX_VLAN;
	if (tx_offloads & RTE_ETH_TX_OFFLOAD_QINQ_INSERT)
		ol_flags |= RTE_MBUF_F_TX_QINQ;
	if (tx_offloads & RTE_ETH_TX_OFFLOAD_MACSEC_INSERT)
		ol_flags |= RTE_MBUF_F_TX_MACSEC;
	for (i = 0; i < nb_rx; i++) {
		if (likely(i < nb_rx - 1))
			rte_prefetch0(rte_pktmbuf_mtod(pkts_burst[i + 1],
						       void *));
		mb = pkts_burst[i];
		eth_hdr = rte_pktmbuf_mtod(mb, struct rte_ether_hdr *);
		rte_ether_addr_copy(&peer_eth_addrs[fs->peer_addr],
				    &eth_hdr->dst_addr);
		rte_ether_addr_copy(&ports[fs->tx_port].eth_addr,
				    &eth_hdr->src_addr);
		mb->ol_flags &= RTE_MBUF_F_INDIRECT | RTE_MBUF_F_EXTERNAL;
		mb->ol_flags |= ol_flags;
		mb->l2_len = sizeof(struct rte_ether_hdr);
		mb->l3_len = sizeof(struct rte_ipv4_hdr);
		mb->vlan_tci = txp->tx_vlan_id;
		mb->vlan_tci_outer = txp->tx_vlan_id_outer;
	}
}

#endif /* _MACFWD_H_ */
