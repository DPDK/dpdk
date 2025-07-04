/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn10k_ethdev.h"
#include "cn10k_rx.h"

#if defined(RTE_ARCH_ARM64) && !defined(CNXK_DIS_TMPLT_FUNC)
static void
pick_rx_func(struct rte_eth_dev *eth_dev,
	     const eth_rx_burst_t rx_burst[NIX_RX_OFFLOAD_MAX])
{
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	/* [VLAN] [TSP] [MARK] [CKSUM] [PTYPE] [RSS] */
	eth_dev->rx_pkt_burst =
		rx_burst[dev->rx_offload_flags & (NIX_RX_OFFLOAD_MAX - 1)];

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].rx_pkt_burst =
			eth_dev->rx_pkt_burst;

	rte_atomic_thread_fence(rte_memory_order_release);
}
#endif

static uint16_t __rte_noinline __rte_hot __rte_unused
cn10k_nix_flush_rx(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)
{
	const uint16_t flags = NIX_RX_MULTI_SEG_F | NIX_RX_REAS_F | NIX_RX_OFFLOAD_SECURITY_F;
	return cn10k_nix_flush_recv_pkts(rx_queue, rx_pkts, pkts, flags);
}

#if defined(RTE_ARCH_ARM64)
static void
cn10k_eth_set_rx_tmplt_func(struct rte_eth_dev *eth_dev)
{
#if !defined(CNXK_DIS_TMPLT_FUNC)
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	const eth_rx_burst_t nix_eth_rx_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_mseg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_mseg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_reas[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_reas_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_burst_mseg_reas[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_reas_mseg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_vec_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst_mseg[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_vec_mseg_##name,

		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst_reas[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_reas_vec_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	const eth_rx_burst_t nix_eth_rx_vec_burst_mseg_reas[NIX_RX_OFFLOAD_MAX] = {
#define R(name, flags)[flags] = cn10k_nix_recv_pkts_reas_vec_mseg_##name,
		NIX_RX_FASTPATH_MODES
#undef R
	};

	/* Copy multi seg version with security for tear down sequence */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev->rx_pkt_burst_no_offload = cn10k_nix_flush_rx;

	if (dev->scalar_ena) {
		if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER) {
			if (dev->rx_offload_flags & NIX_RX_REAS_F)
				return pick_rx_func(eth_dev,
						nix_eth_rx_burst_mseg_reas);
			else
				return pick_rx_func(eth_dev,
						nix_eth_rx_burst_mseg);
		}
		if (dev->rx_offload_flags & NIX_RX_REAS_F)
			return pick_rx_func(eth_dev, nix_eth_rx_burst_reas);
		else
			return pick_rx_func(eth_dev, nix_eth_rx_burst);
	}

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SCATTER) {
		if (dev->rx_offload_flags & NIX_RX_REAS_F)
			return pick_rx_func(eth_dev,
					nix_eth_rx_vec_burst_mseg_reas);
		else
			return pick_rx_func(eth_dev, nix_eth_rx_vec_burst_mseg);
	}

	if (dev->rx_offload_flags & NIX_RX_REAS_F)
		return pick_rx_func(eth_dev, nix_eth_rx_vec_burst_reas);
	else
		return pick_rx_func(eth_dev, nix_eth_rx_vec_burst);
#else
	RTE_SET_USED(eth_dev);
#endif
}

static void
cn10k_eth_set_rx_blk_func(struct rte_eth_dev *eth_dev)
{
#if defined(CNXK_DIS_TMPLT_FUNC)
	struct cnxk_eth_dev *dev = cnxk_eth_pmd_priv(eth_dev);

	/* Copy multi seg version with security for tear down sequence */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		dev->rx_pkt_burst_no_offload = cn10k_nix_flush_rx;

	if (dev->scalar_ena) {
		eth_dev->rx_pkt_burst = cn10k_nix_recv_pkts_all_offload;
		if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			eth_dev->rx_pkt_burst = cn10k_nix_recv_pkts_all_offload_tst;
	} else {
		eth_dev->rx_pkt_burst = cn10k_nix_recv_pkts_vec_all_offload;
		if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
			eth_dev->rx_pkt_burst = cn10k_nix_recv_pkts_vec_all_offload_tst;
	}

	if (eth_dev->data->dev_started)
		rte_eth_fp_ops[eth_dev->data->port_id].rx_pkt_burst = eth_dev->rx_pkt_burst;
#else
	RTE_SET_USED(eth_dev);
#endif
}
#endif

void
cn10k_eth_set_rx_function(struct rte_eth_dev *eth_dev)
{
#if defined(RTE_ARCH_ARM64)
	cn10k_eth_set_rx_blk_func(eth_dev);
	cn10k_eth_set_rx_tmplt_func(eth_dev);

	rte_atomic_thread_fence(rte_memory_order_release);
#else
	RTE_SET_USED(eth_dev);
#endif
}
