/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Arm Limited.
 * Copyright (c) 2025 Intel Corporation
 */

#include <inttypes.h>
#include <rte_common.h>
#include <rte_malloc.h>

#include "ixgbe_rxtx.h"
#include "ixgbe_rxtx_vec_common.h"

#include "../common/recycle_mbufs.h"

void __rte_cold
ixgbe_tx_free_swring_vec(struct ci_tx_queue *txq)
{
	if (txq == NULL)
		return;

	if (txq->sw_ring != NULL) {
		rte_free(txq->sw_ring_vec - 1);
		txq->sw_ring_vec = NULL;
	}
}

void __rte_cold
ixgbe_reset_tx_queue_vec(struct ci_tx_queue *txq)
{
	static const union ixgbe_adv_tx_desc zeroed_desc = { { 0 } };
	struct ci_tx_entry_vec *txe = txq->sw_ring_vec;
	uint16_t i;

	/* Zero out HW ring memory */
	for (i = 0; i < txq->nb_tx_desc; i++)
		txq->ixgbe_tx_ring[i] = zeroed_desc;

	/* Initialize SW ring entries */
	for (i = 0; i < txq->nb_tx_desc; i++) {
		volatile union ixgbe_adv_tx_desc *txd = &txq->ixgbe_tx_ring[i];

		txd->wb.status = IXGBE_TXD_STAT_DD;
		txe[i].mbuf = NULL;
	}

	txq->tx_next_dd = (uint16_t)(txq->tx_rs_thresh - 1);
	txq->tx_next_rs = (uint16_t)(txq->tx_rs_thresh - 1);

	txq->tx_tail = 0;
	txq->nb_tx_used = 0;
	/*
	 * Always allow 1 descriptor to be un-allocated to avoid
	 * a H/W race condition
	 */
	txq->last_desc_cleaned = (uint16_t)(txq->nb_tx_desc - 1);
	txq->nb_tx_free = (uint16_t)(txq->nb_tx_desc - 1);
	txq->ctx_curr = 0;
	memset(txq->ctx_cache, 0, IXGBE_CTX_NUM * sizeof(struct ixgbe_advctx_info));

	/* for PF, we do not need to initialize the context descriptor */
	if (!txq->is_vf)
		txq->vf_ctx_initialized = 1;
}

void __rte_cold
ixgbe_rx_queue_release_mbufs_vec(struct ci_rx_queue *rxq)
{
	unsigned int i;

	if (rxq->sw_ring == NULL || rxq->rxrearm_nb >= rxq->nb_rx_desc)
		return;

	/* free all mbufs that are valid in the ring */
	if (rxq->rxrearm_nb == 0) {
		for (i = 0; i < rxq->nb_rx_desc; i++) {
			if (rxq->sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	} else {
		for (i = rxq->rx_tail;
		     i != rxq->rxrearm_start;
		     i = (i + 1) % rxq->nb_rx_desc) {
			if (rxq->sw_ring[i].mbuf != NULL)
				rte_pktmbuf_free_seg(rxq->sw_ring[i].mbuf);
		}
	}

	rxq->rxrearm_nb = rxq->nb_rx_desc;

	/* set all entries to NULL */
	memset(rxq->sw_ring, 0, sizeof(rxq->sw_ring[0]) * rxq->nb_rx_desc);
}

int __rte_cold
ixgbe_rxq_vec_setup(struct ci_rx_queue *rxq)
{
	rxq->mbuf_initializer = ci_rxq_mbuf_initializer(rxq->port_id);
	return 0;
}

static const struct ixgbe_txq_ops vec_txq_ops = {
	.free_swring = ixgbe_tx_free_swring_vec,
	.reset = ixgbe_reset_tx_queue_vec,
};

int __rte_cold
ixgbe_txq_vec_setup(struct ci_tx_queue *txq)
{
	if (txq->sw_ring_vec == NULL)
		return -1;

	/* leave the first one for overflow */
	txq->sw_ring_vec = txq->sw_ring_vec + 1;
	txq->ops = &vec_txq_ops;
	txq->vector_tx = 1;

	return 0;
}

int __rte_cold
ixgbe_rx_vec_dev_conf_condition_check(struct rte_eth_dev *dev)
{
#ifndef RTE_LIBRTE_IEEE1588
	struct rte_eth_fdir_conf *fconf = IXGBE_DEV_FDIR_CONF(dev);

	/* no fdir support */
	if (fconf->mode != RTE_FDIR_MODE_NONE)
		return -1;

	for (uint16_t i = 0; i < dev->data->nb_rx_queues; i++) {
		struct ci_rx_queue *rxq = dev->data->rx_queues[i];
		if (!rxq)
			continue;
		if (!ci_rxq_vec_capable(rxq->nb_rx_desc, rxq->rx_free_thresh))
			return -1;
	}
	return 0;
#else
	RTE_SET_USED(dev);
	return -1;
#endif
}

uint16_t
ixgbe_xmit_pkts_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	uint16_t nb_tx = 0;
	struct ci_tx_queue *txq = (struct ci_tx_queue *)tx_queue;

	/* we might check first packet's mempool */
	if (unlikely(nb_pkts == 0))
		return nb_pkts;

	/* check if we need to initialize default context descriptor */
	if (unlikely(!txq->vf_ctx_initialized) &&
			ixgbe_write_default_ctx_desc(txq, tx_pkts[0]->pool, true))
		return 0;

	while (nb_pkts) {
		uint16_t ret, num;

		num = (uint16_t)RTE_MIN(nb_pkts, txq->tx_rs_thresh);
		ret = ixgbe_xmit_fixed_burst_vec(tx_queue, &tx_pkts[nb_tx],
							num);
		nb_tx += ret;
		nb_pkts -= ret;
		if (ret < num)
			break;
	}

	return nb_tx;
}

void
ixgbe_recycle_rx_descriptors_refill_vec(void *rx_queue, uint16_t nb_mbufs)
{
	ci_rx_recycle_mbufs(rx_queue, nb_mbufs);
}

uint16_t
ixgbe_recycle_tx_mbufs_reuse_vec(void *tx_queue,
		struct rte_eth_recycle_rxq_info *recycle_rxq_info)
{
	return ci_tx_recycle_mbufs(tx_queue, ixgbe_tx_desc_done, recycle_rxq_info);
}
