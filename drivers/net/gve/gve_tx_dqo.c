/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2023 Google LLC
 * Copyright (c) 2022-2023 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static int
check_tx_thresh_dqo(uint16_t nb_desc, uint16_t tx_rs_thresh,
		    uint16_t tx_free_thresh)
{
	if (tx_rs_thresh >= (nb_desc - 2)) {
		PMD_DRV_LOG(ERR, "tx_rs_thresh (%u) must be less than the "
			    "number of TX descriptors (%u) minus 2",
			    tx_rs_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_free_thresh >= (nb_desc - 3)) {
		PMD_DRV_LOG(ERR, "tx_free_thresh (%u) must be less than the "
			    "number of TX descriptors (%u) minus 3.",
			    tx_free_thresh, nb_desc);
		return -EINVAL;
	}
	if (tx_rs_thresh > tx_free_thresh) {
		PMD_DRV_LOG(ERR, "tx_rs_thresh (%u) must be less than or "
			    "equal to tx_free_thresh (%u).",
			    tx_rs_thresh, tx_free_thresh);
		return -EINVAL;
	}
	if ((nb_desc % tx_rs_thresh) != 0) {
		PMD_DRV_LOG(ERR, "tx_rs_thresh (%u) must be a divisor of the "
			    "number of TX descriptors (%u).",
			    tx_rs_thresh, nb_desc);
		return -EINVAL;
	}

	return 0;
}

static void
gve_reset_txq_dqo(struct gve_tx_queue *txq)
{
	struct rte_mbuf **sw_ring;
	uint32_t size, i;

	if (txq == NULL) {
		PMD_DRV_LOG(DEBUG, "Pointer to txq is NULL");
		return;
	}

	size = txq->nb_tx_desc * sizeof(union gve_tx_desc_dqo);
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	size = txq->sw_size * sizeof(struct gve_tx_compl_desc);
	for (i = 0; i < size; i++)
		((volatile char *)txq->compl_ring)[i] = 0;

	sw_ring = txq->sw_ring;
	for (i = 0; i < txq->sw_size; i++)
		sw_ring[i] = NULL;

	txq->tx_tail = 0;
	txq->nb_used = 0;

	txq->last_desc_cleaned = 0;
	txq->sw_tail = 0;
	txq->nb_free = txq->nb_tx_desc - 1;

	txq->complq_tail = 0;
	txq->cur_gen_bit = 1;
}

int
gve_tx_queue_setup_dqo(struct rte_eth_dev *dev, uint16_t queue_id,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_txconf *conf)
{
	struct gve_priv *hw = dev->data->dev_private;
	const struct rte_memzone *mz;
	struct gve_tx_queue *txq;
	uint16_t free_thresh;
	uint16_t rs_thresh;
	uint16_t sw_size;
	int err = 0;

	if (nb_desc != hw->tx_desc_cnt) {
		PMD_DRV_LOG(WARNING, "gve doesn't support nb_desc config, use hw nb_desc %u.",
			    hw->tx_desc_cnt);
	}
	nb_desc = hw->tx_desc_cnt;

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("gve txq",
				 sizeof(struct gve_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for tx queue structure");
		return -ENOMEM;
	}

	/* need to check free_thresh here */
	free_thresh = conf->tx_free_thresh ?
			conf->tx_free_thresh : GVE_DEFAULT_TX_FREE_THRESH;
	rs_thresh = conf->tx_rs_thresh ?
			conf->tx_rs_thresh : GVE_DEFAULT_TX_RS_THRESH;
	if (check_tx_thresh_dqo(nb_desc, rs_thresh, free_thresh))
		return -EINVAL;

	txq->nb_tx_desc = nb_desc;
	txq->free_thresh = free_thresh;
	txq->rs_thresh = rs_thresh;
	txq->queue_id = queue_id;
	txq->port_id = dev->data->port_id;
	txq->ntfy_id = queue_id;
	txq->hw = hw;
	txq->ntfy_addr = &hw->db_bar2[rte_be_to_cpu_32(hw->irq_dbs[txq->ntfy_id].id)];

	/* Allocate software ring */
	sw_size = nb_desc * DQO_TX_MULTIPLIER;
	txq->sw_ring = rte_zmalloc_socket("gve tx sw ring",
					  sw_size * sizeof(struct rte_mbuf *),
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->sw_ring == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW TX ring");
		err = -ENOMEM;
		goto free_txq;
	}
	txq->sw_size = sw_size;

	/* Allocate TX hardware ring descriptors. */
	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_id,
				      nb_desc * sizeof(union gve_tx_desc_dqo),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX");
		err = -ENOMEM;
		goto free_txq_sw_ring;
	}
	txq->tx_ring = (union gve_tx_desc_dqo *)mz->addr;
	txq->tx_ring_phys_addr = mz->iova;
	txq->mz = mz;

	/* Allocate TX completion ring descriptors. */
	mz = rte_eth_dma_zone_reserve(dev, "tx_compl_ring", queue_id,
				      sw_size * sizeof(struct gve_tx_compl_desc),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX completion queue");
		err = -ENOMEM;
		goto free_txq_mz;
	}
	txq->compl_ring = (struct gve_tx_compl_desc *)mz->addr;
	txq->compl_ring_phys_addr = mz->iova;
	txq->compl_ring_mz = mz;
	txq->txqs = dev->data->tx_queues;

	mz = rte_eth_dma_zone_reserve(dev, "txq_res", queue_id,
				      sizeof(struct gve_queue_resources),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX resource");
		err = -ENOMEM;
		goto free_txq_cq_mz;
	}
	txq->qres = (struct gve_queue_resources *)mz->addr;
	txq->qres_mz = mz;

	gve_reset_txq_dqo(txq);

	dev->data->tx_queues[queue_id] = txq;

	return 0;

free_txq_cq_mz:
	rte_memzone_free(txq->compl_ring_mz);
free_txq_mz:
	rte_memzone_free(txq->mz);
free_txq_sw_ring:
	rte_free(txq->sw_ring);
free_txq:
	rte_free(txq);
	return err;
}
