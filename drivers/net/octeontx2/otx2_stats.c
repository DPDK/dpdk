/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include "otx2_ethdev.h"

int
otx2_nix_dev_stats_get(struct rte_eth_dev *eth_dev,
		       struct rte_eth_stats *stats)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	uint64_t reg, val;
	uint32_t qidx, i;
	int64_t *addr;

	stats->opackets = otx2_read64(dev->base +
			NIX_LF_TX_STATX(NIX_STAT_LF_TX_TX_UCAST));
	stats->opackets += otx2_read64(dev->base +
			NIX_LF_TX_STATX(NIX_STAT_LF_TX_TX_MCAST));
	stats->opackets += otx2_read64(dev->base +
			NIX_LF_TX_STATX(NIX_STAT_LF_TX_TX_BCAST));
	stats->oerrors = otx2_read64(dev->base +
			NIX_LF_TX_STATX(NIX_STAT_LF_TX_TX_DROP));
	stats->obytes = otx2_read64(dev->base +
			NIX_LF_TX_STATX(NIX_STAT_LF_TX_TX_OCTS));

	stats->ipackets = otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_UCAST));
	stats->ipackets += otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_MCAST));
	stats->ipackets += otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_BCAST));
	stats->imissed = otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_DROP));
	stats->ibytes = otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_OCTS));
	stats->ierrors = otx2_read64(dev->base +
			NIX_LF_RX_STATX(NIX_STAT_LF_RX_RX_ERR));

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
		if (dev->txmap[i] & (1U << 31)) {
			qidx = dev->txmap[i] & 0xFFFF;
			reg = (((uint64_t)qidx) << 32);

			addr = (int64_t *)(dev->base + NIX_LF_SQ_OP_PKTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_opackets[i] = val;

			addr = (int64_t *)(dev->base + NIX_LF_SQ_OP_OCTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_obytes[i] = val;

			addr = (int64_t *)(dev->base + NIX_LF_SQ_OP_DROP_PKTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_errors[i] = val;
		}
	}

	for (i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++) {
		if (dev->rxmap[i] & (1U << 31)) {
			qidx = dev->rxmap[i] & 0xFFFF;
			reg = (((uint64_t)qidx) << 32);

			addr = (int64_t *)(dev->base + NIX_LF_RQ_OP_PKTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_ipackets[i] = val;

			addr = (int64_t *)(dev->base + NIX_LF_RQ_OP_OCTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_ibytes[i] = val;

			addr = (int64_t *)(dev->base + NIX_LF_RQ_OP_DROP_PKTS);
			val = otx2_atomic64_add_nosync(reg, addr);
			if (val & OP_ERR)
				val = 0;
			stats->q_errors[i] += val;
		}
	}

	return 0;
}

void
otx2_nix_dev_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);
	struct otx2_mbox *mbox = dev->mbox;

	otx2_mbox_alloc_msg_nix_stats_rst(mbox);
	otx2_mbox_process(mbox);
}

int
otx2_nix_queue_stats_mapping(struct rte_eth_dev *eth_dev, uint16_t queue_id,
			     uint8_t stat_idx, uint8_t is_rx)
{
	struct otx2_eth_dev *dev = otx2_eth_pmd_priv(eth_dev);

	if (is_rx)
		dev->rxmap[stat_idx] = ((1U << 31) | queue_id);
	else
		dev->txmap[stat_idx] = ((1U << 31) | queue_id);

	return 0;
}
