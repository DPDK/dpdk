/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_rx.h"

#define R(name, f6, f5, f4, f3, f2, f1, f0, flags)                             \
	uint16_t __rte_noinline __rte_hot cn9k_nix_recv_pkts_mseg_##name(      \
		void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t pkts)      \
	{                                                                      \
		return cn9k_nix_recv_pkts(rx_queue, rx_pkts, pkts,             \
					  (flags) | NIX_RX_MULTI_SEG_F);       \
	}

NIX_RX_FASTPATH_MODES
#undef R
