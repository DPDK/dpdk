/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_ethdev.h"
#include "cn10k_tx.h"

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			       \
	uint16_t __rte_noinline __rte_hot cn10k_nix_xmit_pkts_##name(	       \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)      \
	{                                                                      \
		uint64_t cmd[sz];                                              \
									       \
		/* For TSO inner checksum is a must */                         \
		if (((flags) & NIX_TX_OFFLOAD_TSO_F) &&			       \
		    !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))		       \
			return 0;                                              \
		return cn10k_nix_xmit_pkts(tx_queue, tx_pkts, pkts, cmd,       \
					   0, flags);			       \
	}

NIX_TX_FASTPATH_MODES
#undef T

static inline void
pick_tx_func(struct rte_eth_dev *eth_dev,
	     const eth_tx_burst_t tx_burst[2][2][2][2][2][2][2])
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	/* [SEC] [TSP] [TSO] [NOFF] [VLAN] [OL3_OL4_CSUM] [IL3_IL4_CSUM] */
	eth_dev->tx_pkt_burst = tx_burst
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_SECURITY_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSTAMP_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSO_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_MBUF_NOFF_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_VLAN_QINQ_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F)];

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].tx_pkt_burst =
		    eth_dev->tx_pkt_burst;
}

void
cn10k_eth_set_tx_function(struct rte_eth_dev *eth_dev)
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	const eth_tx_burst_t nix_eth_tx_burst[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
	[f6][f5][f4][f3][f2][f1][f0] = cn10k_nix_xmit_pkts_##name,

		NIX_TX_FASTPATH_MODES
#undef T
	};

	const eth_tx_burst_t nix_eth_tx_burst_mseg[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)			       \
	[f6][f5][f4][f3][f2][f1][f0] = cn10k_nix_xmit_pkts_mseg_##name,

		NIX_TX_FASTPATH_MODES
#undef T
	};

	const eth_tx_burst_t nix_eth_tx_vec_burst[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
	[f6][f5][f4][f3][f2][f1][f0] = cn10k_nix_xmit_pkts_vec_##name,

		NIX_TX_FASTPATH_MODES
#undef T
	};

	const eth_tx_burst_t nix_eth_tx_vec_burst_mseg[2][2][2][2][2][2][2] = {
#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
	[f6][f5][f4][f3][f2][f1][f0] = cn10k_nix_xmit_pkts_vec_mseg_##name,

		NIX_TX_FASTPATH_MODES
#undef T
	};

	if (dev->scalar_ena) {
		pick_tx_func(eth_dev, nix_eth_tx_burst);
		if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
			pick_tx_func(eth_dev, nix_eth_tx_burst_mseg);
	} else {
		pick_tx_func(eth_dev, nix_eth_tx_vec_burst);
		if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS)
			pick_tx_func(eth_dev, nix_eth_tx_vec_burst_mseg);
	}

	rte_mb();
}
