/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/queue.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_string_fns.h>
#include <rte_flow.h>

#include "testpmd.h"

/*
 * Forwarding of packets in MAC mode.
 * Change the source and the destination Ethernet addressed of packets
 * before forwarding them.
 */
static bool
pkt_burst_mac_forward(struct fwd_stream *fs)
{
	struct rte_mbuf  *pkts_burst[MAX_PKT_BURST];
	struct rte_port  *txp;
	struct rte_mbuf  *mb;
	struct rte_ether_hdr *eth_hdr;
	uint16_t nb_rx;
	uint16_t i;
	uint64_t ol_flags = 0;
	uint64_t tx_offloads;

	/*
	 * Receive a burst of packets and forward them.
	 */
	nb_rx = common_fwd_stream_receive(fs, pkts_burst, nb_pkt_per_burst);
	if (unlikely(nb_rx == 0))
		return false;

	txp = &ports[fs->tx_port];
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

	common_fwd_stream_transmit(fs, pkts_burst, nb_rx);

	return true;
}

struct fwd_engine mac_fwd_engine = {
	.fwd_mode_name  = "mac",
	.stream_init    = common_fwd_stream_init,
	.packet_fwd     = pkt_burst_mac_forward,
};
