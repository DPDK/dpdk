/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_tx.h"

#define T(name, f6, f5, f4, f3, f2, f1, f0, sz, flags)                         \
	uint16_t __rte_noinline __rte_hot cn9k_nix_xmit_pkts_vec_mseg_##name(  \
		void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)      \
	{                                                                      \
		uint64_t cmd[sz];                                              \
									       \
		/* For TSO inner checksum is a must */                         \
		if (((flags) & NIX_TX_OFFLOAD_TSO_F) &&                        \
		    !((flags) & NIX_TX_OFFLOAD_L3_L4_CSUM_F))                  \
			return 0;                                              \
		return cn9k_nix_xmit_pkts_vector(tx_queue, tx_pkts, pkts, cmd, \
						 (flags) |                     \
							 NIX_TX_MULTI_SEG_F);  \
	}

NIX_TX_FASTPATH_MODES
#undef T
