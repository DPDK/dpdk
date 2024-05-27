/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include "cn9k_ethdev.h"
#include "cn9k_tx.h"

#if defined(CNXK_DIS_TMPLT_FUNC)

uint16_t __rte_noinline __rte_hot
cn9k_nix_xmit_pkts_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	struct cn9k_eth_txq *txq = (struct cn9k_eth_txq *)tx_queue;
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn9k_nix_xmit_pkts_mseg(tx_queue, tx_pkts, pkts, cmd, txq->tx_offload_flags);
}

uint16_t __rte_noinline __rte_hot
cn9k_nix_xmit_pkts_vec_all_offload(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t pkts)
{
	struct cn9k_eth_txq *txq = (struct cn9k_eth_txq *)tx_queue;
	uint64_t cmd[8 + CNXK_NIX_TX_MSEG_SG_DWORDS - 2];

	return cn9k_nix_xmit_pkts_vector(tx_queue, tx_pkts, pkts, cmd, txq->tx_offload_flags);
}

#endif
