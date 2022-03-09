/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_rx.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_##name(	       \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn9k_nix_recv_pkts(rx_queue, rx_pkts, pkts, (flags));   \
	}

NIX_RX_FASTPATH_MODES
#undef R

static inline void
pick_rx_func(struct rte_eth_dev *eth_dev,
	     const eth_rx_burst_t rx_burst[2][2][2][2][2][2][2])
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	/* [TSP] [MARK] [VLAN] [CKSUM] [PTYPE] [RSS] */
	eth_dev->rx_pkt_burst = rx_burst
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_SECURITY_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_VLAN_STRIP_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_TSTAMP_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_MARK_UPDATE_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_CHECKSUM_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_PTYPE_F)]
		[!!(dev->rx_offload_flags & NIX_RX_OFFLOAD_RSS_F)];

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].rx_pkt_burst =
		    eth_dev->rx_pkt_burst;

	rte_atomic_thread_fence(__ATOMIC_RELEASE);
}

void
cn9k_eth_set_rx_function(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	const eth_rx_burst_t nix_eth_rx_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
	[f6][f5][f4][f3][f2][f1][f0] = cn9k_nix_recv_pkts_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_mseg[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
	[f6][f5][f4][f3][f2][f1][f0] = cn9k_nix_recv_pkts_mseg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
	[f6][f5][f4][f3][f2][f1][f0] = cn9k_nix_recv_pkts_vec_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst_mseg[2][2][2][2][2][2][2] = {
#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)			       \
	[f6][f5][f4][f3][f2][f1][f0] = cn9k_nix_recv_pkts_vec_mseg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Copy multi seg version with no offload for tear down sequence */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev->rx_pkt_burst_no_offload =
			nix_eth_rx_burst_mseg[0][0][0][0][0][0][0];

	if (dev->scalar_ena) {
		if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
			return pick_rx_func(eth_dev, nix_eth_rx_burst_mseg);
		return pick_rx_func(eth_dev, nix_eth_rx_burst);
	}

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER)
		return pick_rx_func(eth_dev, nix_eth_rx_vec_burst_mseg);
	return pick_rx_func(eth_dev, nix_eth_rx_vec_burst);
}
