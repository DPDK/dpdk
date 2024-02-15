/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2023 Google LLC
 * Copyright (c) 2022-2023 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static inline void
gve_tx_clean_dqo(struct gve_tx_queue *txq)
{
	struct gve_tx_compl_desc *compl_ring;
	struct gve_tx_compl_desc *compl_desc;
	struct gve_tx_queue *aim_txq;
	uint16_t nb_desc_clean;
	struct rte_mbuf *txe, *txe_next;
	uint16_t compl_tag;
	uint16_t next;

	next = txq->complq_tail;
	compl_ring = txq->compl_ring;
	compl_desc = &compl_ring[next];

	if (compl_desc->generation != txq->cur_gen_bit)
		return;

	compl_tag = rte_le_to_cpu_16(compl_desc->completion_tag);

	aim_txq = txq->txqs[compl_desc->id];

	switch (compl_desc->type) {
	case GVE_COMPL_TYPE_DQO_DESC:
		/* need to clean Descs from last_cleaned to compl_tag */
		if (aim_txq->last_desc_cleaned > compl_tag)
			nb_desc_clean = aim_txq->nb_tx_desc - aim_txq->last_desc_cleaned +
					compl_tag;
		else
			nb_desc_clean = compl_tag - aim_txq->last_desc_cleaned;
		aim_txq->nb_free += nb_desc_clean;
		aim_txq->last_desc_cleaned = compl_tag;
		break;
	case GVE_COMPL_TYPE_DQO_REINJECTION:
		PMD_DRV_LOG(DEBUG, "GVE_COMPL_TYPE_DQO_REINJECTION !!!");
		/* FALLTHROUGH */
	case GVE_COMPL_TYPE_DQO_PKT:
		/* free all segments. */
		txe = aim_txq->sw_ring[compl_tag];
		while (txe != NULL) {
			txe_next = txe->next;
			rte_pktmbuf_free_seg(txe);
			if (aim_txq->sw_ring[compl_tag] == txe)
				aim_txq->sw_ring[compl_tag] = NULL;
			txe = txe_next;
			compl_tag = (compl_tag + 1) & (aim_txq->sw_size - 1);
		}
		break;
	case GVE_COMPL_TYPE_DQO_MISS:
		rte_delay_us_sleep(1);
		PMD_DRV_LOG(DEBUG, "GVE_COMPL_TYPE_DQO_MISS ignored !!!");
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown completion type.");
		return;
	}

	next++;
	if (next == txq->nb_tx_desc * DQO_TX_MULTIPLIER) {
		next = 0;
		txq->cur_gen_bit ^= 1;
	}

	txq->complq_tail = next;
}

uint16_t
gve_tx_burst_dqo(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct gve_tx_queue *txq = tx_queue;
	volatile union gve_tx_desc_dqo *txr;
	volatile union gve_tx_desc_dqo *txd;
	struct rte_mbuf **sw_ring;
	struct rte_mbuf *tx_pkt;
	uint16_t mask, sw_mask;
	uint16_t nb_to_clean;
	uint16_t nb_tx = 0;
	uint64_t ol_flags;
	uint16_t nb_used;
	uint16_t tx_id;
	uint16_t sw_id;
	uint64_t bytes;
	uint16_t first_sw_id;

	sw_ring = txq->sw_ring;
	txr = txq->tx_ring;

	bytes = 0;
	mask = txq->nb_tx_desc - 1;
	sw_mask = txq->sw_size - 1;
	tx_id = txq->tx_tail;
	sw_id = txq->sw_tail;

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = tx_pkts[nb_tx];

		if (txq->nb_free <= txq->free_thresh) {
			nb_to_clean = DQO_TX_MULTIPLIER * txq->rs_thresh;
			while (nb_to_clean--)
				gve_tx_clean_dqo(txq);
		}

		if (txq->nb_free < tx_pkt->nb_segs)
			break;

		ol_flags = tx_pkt->ol_flags;
		nb_used = tx_pkt->nb_segs;
		first_sw_id = sw_id;
		do {
			if (sw_ring[sw_id] != NULL)
				PMD_DRV_LOG(DEBUG, "Overwriting an entry in sw_ring");

			txd = &txr[tx_id];
			sw_ring[sw_id] = tx_pkt;

			/* fill Tx descriptor */
			txd->pkt.buf_addr = rte_cpu_to_le_64(rte_mbuf_data_iova(tx_pkt));
			txd->pkt.dtype = GVE_TX_PKT_DESC_DTYPE_DQO;
			txd->pkt.compl_tag = rte_cpu_to_le_16(first_sw_id);
			txd->pkt.buf_size = RTE_MIN(tx_pkt->data_len, GVE_TX_MAX_BUF_SIZE_DQO);

			/* size of desc_ring and sw_ring could be different */
			tx_id = (tx_id + 1) & mask;
			sw_id = (sw_id + 1) & sw_mask;

			bytes += tx_pkt->data_len;
			tx_pkt = tx_pkt->next;
		} while (tx_pkt);

		/* fill the last descriptor with End of Packet (EOP) bit */
		txd->pkt.end_of_packet = 1;

		if (ol_flags & GVE_TX_CKSUM_OFFLOAD_MASK)
			txd->pkt.checksum_offload_enable = 1;

		txq->nb_free -= nb_used;
		txq->nb_used += nb_used;
	}

	/* update the tail pointer if any packets were processed */
	if (nb_tx > 0) {
		/* Request a descriptor completion on the last descriptor */
		txq->re_cnt += nb_tx;
		if (txq->re_cnt >= GVE_TX_MIN_RE_INTERVAL) {
			txd = &txr[(tx_id - 1) & mask];
			txd->pkt.report_event = true;
			txq->re_cnt = 0;
		}

		rte_write32(tx_id, txq->qtx_tail);
		txq->tx_tail = tx_id;
		txq->sw_tail = sw_id;

		txq->stats.packets += nb_tx;
		txq->stats.bytes += bytes;
		txq->stats.errors += nb_pkts - nb_tx;
	}

	return nb_tx;
}

static inline void
gve_release_txq_mbufs_dqo(struct gve_tx_queue *txq)
{
	uint16_t i;

	for (i = 0; i < txq->sw_size; i++) {
		if (txq->sw_ring[i]) {
			rte_pktmbuf_free_seg(txq->sw_ring[i]);
			txq->sw_ring[i] = NULL;
		}
	}
}

void
gve_tx_queue_release_dqo(struct rte_eth_dev *dev, uint16_t qid)
{
	struct gve_tx_queue *q = dev->data->tx_queues[qid];

	if (q == NULL)
		return;

	gve_release_txq_mbufs_dqo(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->mz);
	rte_memzone_free(q->compl_ring_mz);
	rte_memzone_free(q->qres_mz);
	q->qres = NULL;
	rte_free(q);
}

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

	/* Free memory if needed. */
	if (dev->data->tx_queues[queue_id]) {
		gve_tx_queue_release_dqo(dev, queue_id);
		dev->data->tx_queues[queue_id] = NULL;
	}

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

int
gve_tx_queue_start_dqo(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct gve_priv *hw = dev->data->dev_private;
	struct gve_tx_queue *txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];

	txq->qtx_tail = &hw->db_bar2[rte_be_to_cpu_32(txq->qres->db_index)];
	txq->qtx_head =
		&hw->cnt_array[rte_be_to_cpu_32(txq->qres->counter_index)];

	rte_write32(rte_cpu_to_be_32(GVE_IRQ_MASK), txq->ntfy_addr);

	dev->data->rx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
gve_tx_queue_stop_dqo(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct gve_tx_queue *txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];
	gve_release_txq_mbufs_dqo(txq);
	gve_reset_txq_dqo(txq);

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
gve_stop_tx_queues_dqo(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	uint16_t i;
	int err;

	err = gve_adminq_destroy_tx_queues(hw, dev->data->nb_tx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy txqs");

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		if (gve_tx_queue_stop_dqo(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Tx queue %d", i);
}

void
gve_set_tx_function_dqo(struct rte_eth_dev *dev)
{
	dev->tx_pkt_burst = gve_tx_burst_dqo;
}
