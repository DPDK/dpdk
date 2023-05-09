/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2023 Google LLC
 * Copyright (c) 2022-2023 Intel Corporation
 */


#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static inline void
gve_rx_refill_dqo(struct gve_rx_queue *rxq)
{
	volatile struct gve_rx_desc_dqo *rx_buf_ring;
	volatile struct gve_rx_desc_dqo *rx_buf_desc;
	struct rte_mbuf *nmb[rxq->free_thresh];
	uint16_t nb_refill = rxq->free_thresh;
	uint16_t nb_desc = rxq->nb_rx_desc;
	uint16_t next_avail = rxq->bufq_tail;
	struct rte_eth_dev *dev;
	uint64_t dma_addr;
	uint16_t delta;
	int i;

	if (rxq->nb_rx_hold < rxq->free_thresh)
		return;

	rx_buf_ring = rxq->rx_ring;
	delta = nb_desc - next_avail;
	if (unlikely(delta < nb_refill)) {
		if (likely(rte_pktmbuf_alloc_bulk(rxq->mpool, nmb, delta) == 0)) {
			for (i = 0; i < delta; i++) {
				rx_buf_desc = &rx_buf_ring[next_avail + i];
				rxq->sw_ring[next_avail + i] = nmb[i];
				dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb[i]));
				rx_buf_desc->header_buf_addr = 0;
				rx_buf_desc->buf_addr = dma_addr;
			}
			nb_refill -= delta;
			next_avail = 0;
			rxq->nb_rx_hold -= delta;
		} else {
			rxq->stats.no_mbufs_bulk++;
			rxq->stats.no_mbufs += nb_desc - next_avail;
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed += nb_desc - next_avail;
			PMD_DRV_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%u",
				    rxq->port_id, rxq->queue_id);
			return;
		}
	}

	if (nb_desc - next_avail >= nb_refill) {
		if (likely(rte_pktmbuf_alloc_bulk(rxq->mpool, nmb, nb_refill) == 0)) {
			for (i = 0; i < nb_refill; i++) {
				rx_buf_desc = &rx_buf_ring[next_avail + i];
				rxq->sw_ring[next_avail + i] = nmb[i];
				dma_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb[i]));
				rx_buf_desc->header_buf_addr = 0;
				rx_buf_desc->buf_addr = dma_addr;
			}
			next_avail += nb_refill;
			rxq->nb_rx_hold -= nb_refill;
		} else {
			rxq->stats.no_mbufs_bulk++;
			rxq->stats.no_mbufs += nb_desc - next_avail;
			dev = &rte_eth_devices[rxq->port_id];
			dev->data->rx_mbuf_alloc_failed += nb_desc - next_avail;
			PMD_DRV_LOG(DEBUG, "RX mbuf alloc failed port_id=%u queue_id=%u",
				    rxq->port_id, rxq->queue_id);
		}
	}

	rte_write32(next_avail, rxq->qrx_tail);

	rxq->bufq_tail = next_avail;
}

uint16_t
gve_rx_burst_dqo(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile struct gve_rx_compl_desc_dqo *rx_compl_ring;
	volatile struct gve_rx_compl_desc_dqo *rx_desc;
	struct gve_rx_queue *rxq;
	struct rte_mbuf *rxm;
	uint16_t rx_id_bufq;
	uint16_t pkt_len;
	uint16_t rx_id;
	uint16_t nb_rx;
	uint64_t bytes;

	bytes = 0;
	nb_rx = 0;
	rxq = rx_queue;
	rx_id = rxq->rx_tail;
	rx_id_bufq = rxq->next_avail;
	rx_compl_ring = rxq->compl_ring;

	while (nb_rx < nb_pkts) {
		rx_desc = &rx_compl_ring[rx_id];

		/* check status */
		if (rx_desc->generation != rxq->cur_gen_bit)
			break;

		if (unlikely(rx_desc->rx_error)) {
			rxq->stats.errors++;
			continue;
		}

		pkt_len = rx_desc->packet_len;

		rx_id++;
		if (rx_id == rxq->nb_rx_desc) {
			rx_id = 0;
			rxq->cur_gen_bit ^= 1;
		}

		rxm = rxq->sw_ring[rx_id_bufq];
		rx_id_bufq++;
		if (rx_id_bufq == rxq->nb_rx_desc)
			rx_id_bufq = 0;
		rxq->nb_rx_hold++;

		rxm->pkt_len = pkt_len;
		rxm->data_len = pkt_len;
		rxm->port = rxq->port_id;
		rxm->ol_flags = 0;

		rxm->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
		rxm->hash.rss = rte_be_to_cpu_32(rx_desc->hash);

		rx_pkts[nb_rx++] = rxm;
		bytes += pkt_len;
	}

	if (nb_rx > 0) {
		rxq->rx_tail = rx_id;
		if (rx_id_bufq != rxq->next_avail)
			rxq->next_avail = rx_id_bufq;

		gve_rx_refill_dqo(rxq);

		rxq->stats.packets += nb_rx;
		rxq->stats.bytes += bytes;
	}

	return nb_rx;
}

static inline void
gve_release_rxq_mbufs_dqo(struct gve_rx_queue *rxq)
{
	uint16_t i;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->sw_ring[i]) {
			rte_pktmbuf_free_seg(rxq->sw_ring[i]);
			rxq->sw_ring[i] = NULL;
		}
	}

	rxq->nb_avail = rxq->nb_rx_desc;
}

void
gve_rx_queue_release_dqo(struct rte_eth_dev *dev, uint16_t qid)
{
	struct gve_rx_queue *q = dev->data->rx_queues[qid];

	if (q == NULL)
		return;

	gve_release_rxq_mbufs_dqo(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->compl_ring_mz);
	rte_memzone_free(q->mz);
	rte_memzone_free(q->qres_mz);
	q->qres = NULL;
	rte_free(q);
}

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

	/* Free memory if needed */
	if (dev->data->rx_queues[queue_id]) {
		gve_rx_queue_release_dqo(dev, queue_id);
		dev->data->rx_queues[queue_id] = NULL;
	}

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

static int
gve_rxq_mbufs_alloc_dqo(struct gve_rx_queue *rxq)
{
	struct rte_mbuf *nmb;
	uint16_t i;
	int diag;

	diag = rte_pktmbuf_alloc_bulk(rxq->mpool, &rxq->sw_ring[0], rxq->nb_rx_desc);
	if (diag < 0) {
		rxq->stats.no_mbufs_bulk++;
		for (i = 0; i < rxq->nb_rx_desc - 1; i++) {
			nmb = rte_pktmbuf_alloc(rxq->mpool);
			if (!nmb)
				break;
			rxq->sw_ring[i] = nmb;
		}
		if (i < rxq->nb_rx_desc - 1) {
			rxq->stats.no_mbufs += rxq->nb_rx_desc - 1 - i;
			return -ENOMEM;
		}
	}

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (i == rxq->nb_rx_desc - 1)
			break;
		nmb = rxq->sw_ring[i];
		rxq->rx_ring[i].buf_addr = rte_cpu_to_le_64(rte_mbuf_data_iova_default(nmb));
		rxq->rx_ring[i].buf_id = rte_cpu_to_le_16(i);
	}

	rxq->nb_rx_hold = 0;
	rxq->bufq_tail = rxq->nb_rx_desc - 1;

	rte_write32(rxq->bufq_tail, rxq->qrx_tail);

	return 0;
}

int
gve_rx_queue_start_dqo(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct gve_priv *hw = dev->data->dev_private;
	struct gve_rx_queue *rxq;
	int ret;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

	rxq->qrx_tail = &hw->db_bar2[rte_be_to_cpu_32(rxq->qres->db_index)];

	rte_write32(rte_cpu_to_be_32(GVE_IRQ_MASK), rxq->ntfy_addr);

	ret = gve_rxq_mbufs_alloc_dqo(rxq);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to alloc Rx queue mbuf");
		return ret;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
gve_rx_queue_stop_dqo(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct gve_rx_queue *rxq;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];
	gve_release_rxq_mbufs_dqo(rxq);
	gve_reset_rxq_dqo(rxq);

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
gve_stop_rx_queues_dqo(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	uint16_t i;
	int err;

	err = gve_adminq_destroy_rx_queues(hw, dev->data->nb_rx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy rxqs");

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		if (gve_rx_queue_stop_dqo(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Rx queue %d", i);
}

void
gve_set_rx_function_dqo(struct rte_eth_dev *dev)
{
	dev->rx_pkt_burst = gve_rx_burst_dqo;
}
