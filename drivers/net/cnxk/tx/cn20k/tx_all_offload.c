/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "cn20k_tx.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_noinline __rte_hot
cn20k_nix_xmit_pkts_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn20k_nix_xmit_pkts_mseg(tx_queue, NULL, tx_pkts, pkts, cmd,
				NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
				NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_MBUF_NOFF_F |
				NIX_TX_OFFLOAD_TSO_F | NIX_TX_OFFLOAD_TSTAMP_F |
				NIX_TX_OFFLOAD_SECURITY_F | NIX_TX_MULTI_SEG_F);
}

uint16_t __rte_noinline __rte_hot
cn20k_nix_xmit_pkts_vec_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn20k_nix_xmit_pkts_vector(tx_queue, NULL, tx_pkts, pkts, cmd,
				NIX_TX_OFFLOAD_L3_L4_CSUM_F | NIX_TX_OFFLOAD_OL3_OL4_CSUM_F |
				NIX_TX_OFFLOAD_VLAN_QINQ_F | NIX_TX_OFFLOAD_MBUF_NOFF_F |
				NIX_TX_OFFLOAD_TSO_F | NIX_TX_OFFLOAD_TSTAMP_F |
				NIX_TX_OFFLOAD_SECURITY_F | NIX_TX_MULTI_SEG_F);
}

#endif
