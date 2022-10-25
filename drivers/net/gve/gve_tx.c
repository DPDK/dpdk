/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static inline void
gve_reset_txq(struct gve_tx_queue *txq)
{
	struct rte_mbuf **sw_ring = txq->sw_ring;
	uint32_t size, i;

	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Pointer to txq is NULL");
		return;
	}

	size = txq->nb_tx_desc * sizeof(union gve_tx_desc);
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_desc_ring)[i] = 0;

	for (i = 0; i < txq->nb_tx_desc; i++) {
		sw_ring[i] = NULL;
		if (txq->is_gqi_qpl) {
			txq->iov_ring[i].iov_base = 0;
			txq->iov_ring[i].iov_len = 0;
		}
	}

	txq->tx_tail = 0;
	txq->nb_free = txq->nb_tx_desc - 1;
	txq->next_to_clean = 0;

	if (txq->is_gqi_qpl) {
		txq->fifo_size = PAGE_SIZE * txq->hw->tx_pages_per_qpl;
		txq->fifo_avail = txq->fifo_size;
		txq->fifo_head = 0;
		txq->fifo_base = (uint64_t)(txq->qpl->mz->addr);

		txq->sw_tail = 0;
		txq->sw_nb_free = txq->nb_tx_desc - 1;
		txq->sw_ntc = 0;
	}
}

static inline void
gve_release_txq_mbufs(struct gve_tx_queue *txq)
{
	uint16_t i;

	for (i = 0; i < txq->nb_tx_desc; i++) {
		if (txq->sw_ring[i]) {
			rte_pktmbuf_free_seg(txq->sw_ring[i]);
			txq->sw_ring[i] = NULL;
		}
	}
}

void
gve_tx_queue_release(void *txq)
{
	struct gve_tx_queue *q = txq;

	if (!q)
		return;

	if (q->is_gqi_qpl) {
		gve_adminq_unregister_page_list(q->hw, q->qpl->id);
		rte_free(q->iov_ring);
		q->qpl = NULL;
	}

	gve_release_txq_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_memzone_free(q->qres_mz);
	q->qres = NULL;
	rte_free(q);
}

int
gve_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id, uint16_t nb_desc,
		   unsigned int socket_id, const struct rte_eth_txconf *conf)
{
	struct gve_priv *hw = dev->data->dev_private;
	const struct rte_memzone *mz;
	struct gve_tx_queue *txq;
	uint16_t free_thresh;
	int err = 0;

	if (nb_desc != hw->tx_desc_cnt) {
		PMD_DRV_LOG(WARNING, "gve doesn't support nb_desc config, use hw nb_desc %u.",
			    hw->tx_desc_cnt);
	}
	nb_desc = hw->tx_desc_cnt;

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_id]) {
		gve_tx_queue_release(dev->data->tx_queues[queue_id]);
		dev->data->tx_queues[queue_id] = NULL;
	}

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("gve txq", sizeof(struct gve_tx_queue),
				 RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for tx queue structure");
		err = -ENOMEM;
		goto err_txq;
	}

	free_thresh = conf->tx_free_thresh ? conf->tx_free_thresh : GVE_DEFAULT_TX_FREE_THRESH;
	if (free_thresh >= nb_desc - 3) {
		PMD_DRV_LOG(ERR, "tx_free_thresh (%u) must be less than nb_desc (%u) minus 3.",
			    free_thresh, txq->nb_tx_desc);
		err = -EINVAL;
		goto err_txq;
	}

	txq->nb_tx_desc = nb_desc;
	txq->free_thresh = free_thresh;
	txq->queue_id = queue_id;
	txq->port_id = dev->data->port_id;
	txq->ntfy_id = queue_id;
	txq->is_gqi_qpl = hw->queue_format == GVE_GQI_QPL_FORMAT;
	txq->hw = hw;
	txq->ntfy_addr = &hw->db_bar2[rte_be_to_cpu_32(hw->irq_dbs[txq->ntfy_id].id)];

	/* Allocate software ring */
	txq->sw_ring = rte_zmalloc_socket("gve tx sw ring",
					  sizeof(struct rte_mbuf *) * nb_desc,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (!txq->sw_ring) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW TX ring");
		err = -ENOMEM;
		goto err_txq;
	}

	mz = rte_eth_dma_zone_reserve(dev, "tx_ring", queue_id,
				      nb_desc * sizeof(union gve_tx_desc),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX");
		err = -ENOMEM;
		goto err_sw_ring;
	}
	txq->tx_desc_ring = (union gve_tx_desc *)mz->addr;
	txq->tx_ring_phys_addr = mz->iova;
	txq->mz = mz;

	if (txq->is_gqi_qpl) {
		txq->iov_ring = rte_zmalloc_socket("gve tx iov ring",
						   sizeof(struct gve_tx_iovec) * nb_desc,
						   RTE_CACHE_LINE_SIZE, socket_id);
		if (!txq->iov_ring) {
			PMD_DRV_LOG(ERR, "Failed to allocate memory for SW TX ring");
			err = -ENOMEM;
			goto err_tx_ring;
		}
		txq->qpl = &hw->qpl[queue_id];
		err = gve_adminq_register_page_list(hw, txq->qpl);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to register qpl %u", queue_id);
			goto err_iov_ring;
		}
	}

	mz = rte_eth_dma_zone_reserve(dev, "txq_res", queue_id, sizeof(struct gve_queue_resources),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for TX resource");
		err = -ENOMEM;
		goto err_iov_ring;
	}
	txq->qres = (struct gve_queue_resources *)mz->addr;
	txq->qres_mz = mz;

	gve_reset_txq(txq);

	dev->data->tx_queues[queue_id] = txq;

	return 0;

err_iov_ring:
	if (txq->is_gqi_qpl)
		rte_free(txq->iov_ring);
err_tx_ring:
	rte_memzone_free(txq->mz);
err_sw_ring:
	rte_free(txq->sw_ring);
err_txq:
	rte_free(txq);
	return err;
}

void
gve_stop_tx_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	struct gve_tx_queue *txq;
	uint16_t i;
	int err;

	err = gve_adminq_destroy_tx_queues(hw, dev->data->nb_tx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy txqs");

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		gve_release_txq_mbufs(txq);
		gve_reset_txq(txq);
	}
}
