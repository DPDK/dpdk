/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn10k_tx.h"

#ifdef _ROC_API_H_
#error "roc_api.h is included"
#endif

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_noinline __rte_hot
cn10k_nix_xmit_pkts_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	struct cn10k_eth_txq *txq = (struct cn10k_eth_txq *)tx_queue;
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn10k_nix_xmit_pkts_mseg(tx_queue, NULL, tx_pkts, pkts, cmd, txq->tx_offload_flags);
}

uint16_t __rte_noinline __rte_hot
cn10k_nix_xmit_pkts_vec_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	struct cn10k_eth_txq *txq = (struct cn10k_eth_txq *)tx_queue;
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn10k_nix_xmit_pkts_vector(tx_queue, NULL, tx_pkts, pkts, cmd,
					  txq->tx_offload_flags);
}

#endif
