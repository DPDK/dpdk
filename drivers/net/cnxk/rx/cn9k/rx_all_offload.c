/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_rx.h"

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_noinline __rte_hot
cn9k_nix_recv_pkts_all_offload(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)
{
	return cn9k_nix_recv_pkts(rx_queue, rx_pkts, pkts,
				  NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F |
					  NIX_RX_OFFLOAD_CHECKSUM_F | NIX_RX_OFFLOAD_MARK_UPDATE_F |
					   NIX_RX_OFFLOAD_VLAN_STRIP_F |
					  NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);
}

uint16_t __rte_noinline __rte_hot
cn9k_nix_recv_pkts_vec_all_offload(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)
{
	return cn9k_nix_recv_pkts_vector(rx_queue, rx_pkts, pkts,
		NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
			NIX_RX_OFFLOAD_MARK_UPDATE_F |
			NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F |
			NIX_RX_MULTI_SEG_F);
}

uint16_t __rte_noinline __rte_hot
cn9k_nix_recv_pkts_all_offload_tst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)
{
	return cn9k_nix_recv_pkts(rx_queue, rx_pkts, pkts,
				  NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F |
					  NIX_RX_OFFLOAD_CHECKSUM_F | NIX_RX_OFFLOAD_MARK_UPDATE_F |
					  NIX_RX_OFFLOAD_TSTAMP_F | NIX_RX_OFFLOAD_VLAN_STRIP_F |
					  NIX_RX_OFFLOAD_SECURITY_F | NIX_RX_MULTI_SEG_F);
}

uint16_t __rte_noinline __rte_hot
cn9k_nix_recv_pkts_vec_all_offload_tst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)
{
	return cn9k_nix_recv_pkts_vector(rx_queue, rx_pkts, pkts,
		NIX_RX_OFFLOAD_RSS_F | NIX_RX_OFFLOAD_PTYPE_F | NIX_RX_OFFLOAD_CHECKSUM_F |
			NIX_RX_OFFLOAD_MARK_UPDATE_F | NIX_RX_OFFLOAD_TSTAMP_F |
			NIX_RX_OFFLOAD_VLAN_STRIP_F | NIX_RX_OFFLOAD_SECURITY_F |
			NIX_RX_MULTI_SEG_F);
}

#endif
