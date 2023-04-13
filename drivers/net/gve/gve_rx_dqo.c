/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2023 Google LLC
 * Copyright (c) 2022-2023 Intel Corporation
 */


#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static void
gve_reset_rxq_dqo(struct gve_rx_queue *rxq)
{
	struct rte_mbuf **sw_ring;
	uint32_t size, i;

	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "pointer to rxq is NULL");
		return;
	}

	size = rxq->nb_rx_desc * sizeof(struct gve_rx_desc_dqo);
	for (i = 0; i < size; i++)
		((volatile char *)rxq->rx_ring)[i] = 0;

	size = rxq->nb_rx_desc * sizeof(struct gve_rx_compl_desc_dqo);
	for (i = 0; i < size; i++)
		((volatile char *)rxq->compl_ring)[i] = 0;

	sw_ring = rxq->sw_ring;
	for (i = 0; i < rxq->nb_rx_desc; i++)
		sw_ring[i] = NULL;

	rxq->bufq_tail = 0;
	rxq->next_avail = 0;
	rxq->nb_rx_hold = rxq->nb_rx_desc - 1;

	rxq->rx_tail = 0;
	rxq->cur_gen_bit = 1;
}

int
gve_rx_queue_setup_dqo(struct rte_eth_dev *dev, uint16_t queue_id,
		       uint16_t nb_desc, unsigned int socket_id,
		       const struct rte_eth_rxconf *conf,
		       struct rte_mempool *pool)
{
	struct gve_priv *hw = dev->data->dev_private;
	const struct rte_memzone *mz;
	struct gve_rx_queue *rxq;
	uint16_t free_thresh;
	int err = 0;

	if (nb_desc != hw->rx_desc_cnt) {
		PMD_DRV_LOG(WARNING, "gve doesn't support nb_desc config, use hw nb_desc %u.",
			    hw->rx_desc_cnt);
	}
	nb_desc = hw->rx_desc_cnt;

	/* Allocate the RX queue data structure. */
	rxq = rte_zmalloc_socket("gve rxq",
				 sizeof(struct gve_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for rx queue structure");
		return -ENOMEM;
	}

	/* check free_thresh here */
	free_thresh = conf->rx_free_thresh ?
			conf->rx_free_thresh : GVE_DEFAULT_RX_FREE_THRESH;
	if (free_thresh >= nb_desc) {
		PMD_DRV_LOG(ERR, "rx_free_thresh (%u) must be less than nb_desc (%u).",
			    free_thresh, rxq->nb_rx_desc);
		err = -EINVAL;
		goto free_rxq;
	}

	rxq->nb_rx_desc = nb_desc;
	rxq->free_thresh = free_thresh;
	rxq->queue_id = queue_id;
	rxq->port_id = dev->data->port_id;
	rxq->ntfy_id = hw->num_ntfy_blks / 2 + queue_id;

	rxq->mpool = pool;
	rxq->hw = hw;
	rxq->ntfy_addr = &hw->db_bar2[rte_be_to_cpu_32(hw->irq_dbs[rxq->ntfy_id].id)];

	rxq->rx_buf_len =
		rte_pktmbuf_data_room_size(rxq->mpool) - RTE_PKTMBUF_HEADROOM;

	/* Allocate software ring */
	rxq->sw_ring = rte_zmalloc_socket("gve rx sw ring",
					  nb_desc * sizeof(struct rte_mbuf *),
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (rxq->sw_ring == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW RX ring");
		err = -ENOMEM;
		goto free_rxq;
	}

	/* Allocate RX buffer queue */
	mz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_id,
				      nb_desc * sizeof(struct gve_rx_desc_dqo),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX buffer queue");
		err = -ENOMEM;
		goto free_rxq_sw_ring;
	}
	rxq->rx_ring = (struct gve_rx_desc_dqo *)mz->addr;
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->mz = mz;

	/* Allocate RX completion queue */
	mz = rte_eth_dma_zone_reserve(dev, "compl_ring", queue_id,
				      nb_desc * sizeof(struct gve_rx_compl_desc_dqo),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX completion queue");
		err = -ENOMEM;
		goto free_rxq_mz;
	}
	/* Zero all the descriptors in the ring */
	memset(mz->addr, 0, nb_desc * sizeof(struct gve_rx_compl_desc_dqo));
	rxq->compl_ring = (struct gve_rx_compl_desc_dqo *)mz->addr;
	rxq->compl_ring_phys_addr = mz->iova;
	rxq->compl_ring_mz = mz;

	mz = rte_eth_dma_zone_reserve(dev, "rxq_res", queue_id,
				      sizeof(struct gve_queue_resources),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX resource");
		err = -ENOMEM;
		goto free_rxq_cq_mz;
	}
	rxq->qres = (struct gve_queue_resources *)mz->addr;
	rxq->qres_mz = mz;

	gve_reset_rxq_dqo(rxq);

	dev->data->rx_queues[queue_id] = rxq;

	return 0;

free_rxq_cq_mz:
	rte_memzone_free(rxq->compl_ring_mz);
free_rxq_mz:
	rte_memzone_free(rxq->mz);
free_rxq_sw_ring:
	rte_free(rxq->sw_ring);
free_rxq:
	rte_free(rxq);
	return err;
}
