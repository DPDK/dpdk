/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_vect.h>

#include "otx2_ethdev.h"

#define NIX_XMIT_FC_OR_RETURN(txq, pkts) do {				\
	/* Cached value is low, Update the fc_cache_pkts */		\
	if (unlikely((txq)->fc_cache_pkts < (pkts))) {			\
		/* Multiply with sqe_per_sqb to express in pkts */	\
		(txq)->fc_cache_pkts =					\
			((txq)->nb_sqb_bufs_adj - *(txq)->fc_mem) <<    \
				(txq)->sqes_per_sqb_log2;		\
		/* Check it again for the room */			\
		if (unlikely((txq)->fc_cache_pkts < (pkts)))		\
			return 0;					\
	}								\
} while (0)


static __rte_always_inline uint16_t
nix_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	      uint16_t pkts, uint64_t *cmd, const uint16_t flags)
{
	struct otx2_eth_txq *txq = tx_queue; uint16_t i;
	const rte_iova_t io_addr = txq->io_addr;
	void *lmt_addr = txq->lmt_addr;

	NIX_XMIT_FC_OR_RETURN(txq, pkts);

	otx2_lmt_mov(cmd, &txq->cmd[0], otx2_nix_tx_ext_subs(flags));

	/* Lets commit any changes in the packet */
	rte_cio_wmb();

	for (i = 0; i < pkts; i++) {
		otx2_nix_xmit_prepare(tx_pkts[i], cmd, flags);
		/* Passing no of segdw as 4: HDR + EXT + SG + SMEM */
		otx2_nix_xmit_prepare_tstamp(cmd, &txq->cmd[0],
					     tx_pkts[i]->ol_flags, 4, flags);
		otx2_nix_xmit_one(cmd, lmt_addr, io_addr, flags);
	}

	/* Reduce the cached count */
	txq->fc_cache_pkts -= pkts;

	return pkts;
}

#define T(name, f4, f3, f2, f1, f0, sz, flags)				\
static uint16_t __rte_noinline	__hot					\
otx2_nix_xmit_pkts_ ## name(void *tx_queue,				\
			struct rte_mbuf **tx_pkts, uint16_t pkts)	\
{									\
	uint64_t cmd[sz];						\
									\
	return nix_xmit_pkts(tx_queue, tx_pkts, pkts, cmd, flags);	\
}

NIX_TX_FASTPATH_MODES
#undef T

static inline void
pick_tx_func(struct rte_eth_dev *eth_dev,
	     const eth_tx_burst_t tx_burst[2][2][2][2][2])
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	/* [TSTMP] [NOFF] [VLAN] [OL3_OL4_CSUM] [IL3_IL4_CSUM] */
	eth_dev->tx_pkt_burst = tx_burst
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_TSTAMP_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_MBUF_NOFF_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_VLAN_QINQ_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_OL3_OL4_CSUM_F)]
		[!!(dev->tx_offload_flags & NIX_TX_OFFLOAD_L3_L4_CSUM_F)];
}

void
otx2_eth_set_tx_function(struct rte_eth_dev *eth_dev)
{
	const eth_tx_burst_t nix_eth_tx_burst[2][2][2][2][2] = {
#define T(name, f4, f3, f2, f1, f0, sz, flags)				\
	[f4][f3][f2][f1][f0] =  otx2_nix_xmit_pkts_ ## name,

NIX_TX_FASTPATH_MODES
#undef T
	};

	pick_tx_func(eth_dev, nix_eth_tx_burst);

	rte_mb();
}
