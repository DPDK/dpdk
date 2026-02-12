/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022-2023 Google LLC
 * Copyright (c) 2022-2023 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"
#include "rte_malloc.h"

static inline void
gve_free_compl_tags_init(struct gve_tx_queue *txq)
{
	uint16_t sw_mask = txq->sw_size - 1;
	int i;

	for (i = 0; i < sw_mask; i++)
		txq->pkt_ring_dqo[i].next_avail_pkt = (i + 1) & sw_mask;

	txq->pkt_ring_dqo[sw_mask].next_avail_pkt = -1;
	txq->free_compl_tags_head = 0;
	txq->num_free_compl_tags = txq->sw_size;
}

/* Removes first item from the buffer free list, and return its array index. */
static inline bool
gve_free_compl_tags_pop(struct gve_tx_queue *txq, uint16_t *compl_tag)
{
	int16_t head = txq->free_compl_tags_head;
	if (likely(head != -1)) {
		struct gve_tx_pkt *head_pkt = &txq->pkt_ring_dqo[head];

		txq->free_compl_tags_head = head_pkt->next_avail_pkt;
		txq->num_free_compl_tags--;
		*compl_tag = head;
		return true;
	}

	PMD_DRV_DP_LOG(ERR, "Completion tags list is empty!");
	return false;
}

/* Adds gve_tx_pkt in pkt_ring to free list. Assumes that
 * buf_id < txq->sw_size.
 */
static inline void
gve_free_compl_tags_push(struct gve_tx_queue *txq, uint16_t compl_tag)
{
	txq->pkt_ring_dqo[compl_tag].next_avail_pkt = txq->free_compl_tags_head;
	txq->free_compl_tags_head = compl_tag;
	txq->num_free_compl_tags++;
}

static inline void
gve_tx_clean_dqo(struct gve_tx_queue *txq)
{
	struct gve_tx_compl_desc *compl_ring;
	struct gve_tx_compl_desc *compl_desc;
	struct gve_tx_queue *aim_txq;
	struct gve_tx_pkt *pkt;
	uint16_t new_tx_head;
	uint16_t compl_tag;
	uint16_t next;

	next = txq->complq_tail;
	compl_ring = txq->compl_ring;
	compl_desc = &compl_ring[next];

	if (compl_desc->generation != txq->cur_gen_bit)
		return;

	rte_io_rmb();

	aim_txq = txq->txqs[compl_desc->id];

	switch (compl_desc->type) {
	case GVE_COMPL_TYPE_DQO_DESC:
		new_tx_head = rte_le_to_cpu_16(compl_desc->tx_head);
		aim_txq->nb_free +=
			(new_tx_head - aim_txq->last_desc_cleaned)
				& (aim_txq->nb_tx_desc - 1);
		aim_txq->last_desc_cleaned = new_tx_head;
		break;
	case GVE_COMPL_TYPE_DQO_REINJECTION:
		PMD_DRV_DP_LOG(DEBUG, "GVE_COMPL_TYPE_DQO_REINJECTION !!!");
		/* FALLTHROUGH */
	case GVE_COMPL_TYPE_DQO_PKT:
		/* The first segment has buf_id == completion_tag. */
		compl_tag = rte_le_to_cpu_16(compl_desc->completion_tag);
		if (unlikely(compl_tag >= txq->sw_size)) {
			PMD_DRV_DP_LOG(ERR, "Invalid completion tag %d",
				       compl_tag);
			break;
		}

		/* Free packet.*/
		pkt = &aim_txq->pkt_ring_dqo[compl_tag];
		if (unlikely(!pkt->mbuf)) {
			PMD_DRV_DP_LOG(ERR, "No outstanding packet for completion tag %d",
				       compl_tag);
			break;
		}
		rte_pktmbuf_free(pkt->mbuf);
		pkt->mbuf = NULL;
		gve_free_compl_tags_push(txq, compl_tag);
		break;
	case GVE_COMPL_TYPE_DQO_MISS:
		rte_delay_us_sleep(1);
		PMD_DRV_DP_LOG(DEBUG, "GVE_COMPL_TYPE_DQO_MISS ignored !!!");
		break;
	default:
		PMD_DRV_DP_LOG(ERR, "unknown completion type.");
		return;
	}

	next++;
	if (next == txq->nb_complq_desc) {
		next = 0;
		txq->cur_gen_bit ^= 1;
	}
	txq->complq_tail = next;
}

static inline void
gve_tx_clean_descs_dqo(struct gve_tx_queue *txq, uint16_t nb_descs) {
	while (--nb_descs)
		gve_tx_clean_dqo(txq);
}

/* GVE expects at most 10 data descriptors per mtu-sized segment. Beyond this,
 * the hardware will assume the driver is malicious and stop transmitting
 * packets altogether. Validate that a packet can be sent to avoid sending
 * posting descriptors for an invalid packet.
 */
static inline bool
gve_tx_validate_descs(struct rte_mbuf *tx_pkt, uint16_t nb_descs, bool is_tso)
{
	if (!is_tso)
		return nb_descs <= GVE_TX_MAX_DATA_DESCS;

	int tso_segsz = tx_pkt->tso_segsz;
	int num_descs, seg_offset, mbuf_len;
	int headlen = tx_pkt->l2_len + tx_pkt->l3_len + tx_pkt->l4_len;

	/* Headers will be split into their own buffer. */
	num_descs = 1;
	seg_offset = 0;
	mbuf_len = tx_pkt->data_len - headlen;

	while (tx_pkt) {
		if (!mbuf_len)
			goto next_mbuf;

		int seg_remain = tso_segsz - seg_offset;
		if (num_descs == GVE_TX_MAX_DATA_DESCS && seg_remain)
			return false;

		if (seg_remain < mbuf_len) {
			seg_offset = mbuf_len % tso_segsz;
			/* The MSS is bound from above by 9728B, so a
			 * single TSO segment in the middle of an mbuf
			 * will be part of at most two descriptors, and
			 * is not at risk of defying this limitation.
			 * Thus, such segments are ignored.
			 */
			int mbuf_remain = tx_pkt->data_len % GVE_TX_MAX_BUF_SIZE_DQO;

			/* For each TSO segment, HW will prepend
			 * headers. The remaining bytes of this mbuf
			 * will be the start of the payload of the next
			 * TSO segment. In addition, if the final
			 * segment in this mbuf is divided between two
			 * descriptors, both must be counted.
			 */
			num_descs = 1 + !!(seg_offset) +
				(mbuf_remain < seg_offset && mbuf_remain);
		} else {
			seg_offset += mbuf_len;
			num_descs++;
		}

next_mbuf:
		tx_pkt = tx_pkt->next;
		if (tx_pkt)
			mbuf_len = tx_pkt->data_len;
	}


	return true;
}

static uint16_t
gve_tx_pkt_nb_data_descs(struct rte_mbuf *tx_pkt)
{
	int nb_descs = 0;

	while (tx_pkt) {
		nb_descs += (GVE_TX_MAX_BUF_SIZE_DQO - 1 + tx_pkt->data_len) /
			GVE_TX_MAX_BUF_SIZE_DQO;
		tx_pkt = tx_pkt->next;
	}
	return nb_descs;
}

static inline bool
gve_can_tx(struct gve_tx_queue *txq, uint16_t nb_desc, uint16_t nb_pkts)
{
	return txq->nb_free >= nb_desc && txq->num_free_compl_tags >= nb_pkts;
}

static inline void
gve_tx_fill_seg_desc_dqo(volatile union gve_tx_desc_dqo *desc, struct rte_mbuf *tx_pkt)
{
	uint32_t hlen = tx_pkt->l2_len + tx_pkt->l3_len + tx_pkt->l4_len;

	desc->tso_ctx = (struct gve_tx_tso_context_desc_dqo) {};
	desc->tso_ctx.cmd_dtype.dtype = GVE_TX_TSO_CTX_DESC_DTYPE_DQO;
	desc->tso_ctx.cmd_dtype.tso = 1;
	desc->tso_ctx.mss = (uint16_t)tx_pkt->tso_segsz;
	desc->tso_ctx.tso_total_len = tx_pkt->pkt_len - hlen;
	desc->tso_ctx.header_len = (uint8_t)hlen;
}

static void
gve_fill_tx_pkt_desc(struct gve_tx_queue *txq, uint16_t *tx_id,
		     struct rte_mbuf *tx_pkt, uint16_t compl_tag,
		     bool checksum_offload_enable)
{
	volatile union gve_tx_desc_dqo *desc;
	uint16_t mask = txq->nb_tx_desc - 1;
	int mbuf_offset = 0;

	while (mbuf_offset < tx_pkt->data_len) {
		uint64_t buf_addr = rte_mbuf_data_iova(tx_pkt) + mbuf_offset;

		desc = &txq->tx_ring[*tx_id];
		desc->pkt = (struct gve_tx_pkt_desc_dqo) {};
		desc->pkt.buf_addr = rte_cpu_to_le_64(buf_addr);
		desc->pkt.compl_tag = rte_cpu_to_le_16(compl_tag);
		desc->pkt.dtype = GVE_TX_PKT_DESC_DTYPE_DQO;
		desc->pkt.buf_size = RTE_MIN(tx_pkt->data_len - mbuf_offset,
					     GVE_TX_MAX_BUF_SIZE_DQO);
		desc->pkt.end_of_packet = 0;
		desc->pkt.checksum_offload_enable = checksum_offload_enable;

		mbuf_offset += desc->pkt.buf_size;
		*tx_id = (*tx_id + 1) & mask;
	}
}

uint16_t
gve_tx_burst_dqo(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct gve_tx_queue *txq = tx_queue;
	volatile union gve_tx_desc_dqo *txr;
	volatile union gve_tx_desc_dqo *txd;
	uint16_t mask = txq->nb_tx_desc - 1;
	struct gve_tx_pkt *pkts;
	struct rte_mbuf *tx_pkt;
	uint16_t compl_tag;
	const char *reason;
	uint16_t nb_tx = 0;
	uint64_t bytes = 0;
	uint64_t ol_flags;
	uint16_t nb_descs;
	bool csum, tso;
	uint16_t tx_id;

	pkts = txq->pkt_ring_dqo;
	txr = txq->tx_ring;

	tx_id = txq->tx_tail;

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = tx_pkts[nb_tx];

		if (!gve_can_tx(txq, txq->free_thresh, nb_pkts - nb_tx))
			gve_tx_clean_descs_dqo(txq, DQO_TX_MULTIPLIER *
					       txq->rs_thresh);


		if (rte_mbuf_check(tx_pkt, true, &reason)) {
			PMD_DRV_DP_LOG(DEBUG, "Invalid mbuf: %s", reason);
			break;
		}

		ol_flags = tx_pkt->ol_flags;
		tso = !!(ol_flags & RTE_MBUF_F_TX_TCP_SEG);
		csum = !!(ol_flags & GVE_TX_CKSUM_OFFLOAD_MASK_DQO);

		nb_descs = gve_tx_pkt_nb_data_descs(tx_pkt);
		nb_descs += tso;

		/* Clean if there aren't enough descriptors to send the packet. */
		if (unlikely(!gve_can_tx(txq, nb_descs, 1))) {
			int nb_to_clean = RTE_MAX(DQO_TX_MULTIPLIER * txq->rs_thresh,
						  nb_descs);

			gve_tx_clean_descs_dqo(txq, nb_to_clean);
			if (!gve_can_tx(txq, nb_descs, 1))
				break;
		}

		/* Drop packet if it doesn't adhere to hardware limits. */
		if (!gve_tx_validate_descs(tx_pkt, nb_descs, tso)) {
			txq->stats.too_many_descs++;
			break;
		}

		if (tso) {
			txd = &txr[tx_id];
			gve_tx_fill_seg_desc_dqo(txd, tx_pkt);
			tx_id = (tx_id + 1) & mask;
		}

		if (!gve_free_compl_tags_pop(txq, &compl_tag))
			break;

		pkts[compl_tag].mbuf = tx_pkt;
		while (tx_pkt) {
			/* Skip writing descriptors if mbuf has no data. */
			if (!tx_pkt->data_len) {
				tx_pkt = tx_pkt->next;
				continue;
			}
			gve_fill_tx_pkt_desc(txq, &tx_id, tx_pkt, compl_tag,
					     csum);
			bytes += tx_pkt->data_len;
			tx_pkt = tx_pkt->next;
		}
		/* fill the last written descriptor with End of Packet (EOP) bit */
		txd = &txr[(tx_id - 1) & mask];
		txd->pkt.end_of_packet = 1;

		txq->nb_free -= nb_descs;
		txq->nb_used += nb_descs;
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

	for (i = 0; i < txq->sw_size; i++)
		rte_pktmbuf_free(txq->pkt_ring_dqo[i].mbuf);
}

void
gve_tx_queue_release_dqo(struct rte_eth_dev *dev, uint16_t qid)
{
	struct gve_tx_queue *q = dev->data->tx_queues[qid];

	if (q == NULL)
		return;

	gve_release_txq_mbufs_dqo(q);
	rte_free(q->pkt_ring_dqo);
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
gve_reset_tx_ring_state_dqo(struct gve_tx_queue *txq)
{
	struct gve_tx_pkt *pkt_ring_dqo;
	uint32_t size, i;

	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Pointer to txq is NULL");
		return;
	}

	size = txq->nb_tx_desc * sizeof(union gve_tx_desc_dqo);
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_ring)[i] = 0;

	size = txq->nb_complq_desc * sizeof(struct gve_tx_compl_desc);
	for (i = 0; i < size; i++)
		((volatile char *)txq->compl_ring)[i] = 0;

	pkt_ring_dqo = txq->pkt_ring_dqo;
	for (i = 0; i < txq->sw_size; i++)
		pkt_ring_dqo[i].mbuf = NULL;

	gve_free_compl_tags_init(txq);

	txq->tx_tail = 0;
	txq->nb_used = 0;

	txq->last_desc_cleaned = 0;
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
	txq->nb_complq_desc = nb_desc * DQO_TX_MULTIPLIER;
	txq->free_thresh = free_thresh;
	txq->rs_thresh = rs_thresh;
	txq->queue_id = queue_id;
	txq->port_id = dev->data->port_id;
	txq->ntfy_id = queue_id;
	txq->hw = hw;
	txq->ntfy_addr = &hw->db_bar2[rte_be_to_cpu_32(hw->irq_dbs[txq->ntfy_id].id)];

	/* Allocate software ring */
	sw_size = nb_desc;
	txq->pkt_ring_dqo = rte_zmalloc_socket("gve tx sw ring",
					  sw_size * sizeof(struct gve_tx_pkt),
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (txq->pkt_ring_dqo == NULL) {
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
		goto free_txq_pkt_ring;
	}
	txq->tx_ring = (union gve_tx_desc_dqo *)mz->addr;
	txq->tx_ring_phys_addr = mz->iova;
	txq->mz = mz;

	/* Allocate TX completion ring descriptors. */
	mz = rte_eth_dma_zone_reserve(dev, "tx_compl_ring", queue_id,
				       txq->nb_complq_desc * sizeof(struct gve_tx_compl_desc),
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

	gve_reset_tx_ring_state_dqo(txq);

	dev->data->tx_queues[queue_id] = txq;

	return 0;

free_txq_cq_mz:
	rte_memzone_free(txq->compl_ring_mz);
free_txq_mz:
	rte_memzone_free(txq->mz);
free_txq_pkt_ring:
	rte_free(txq->pkt_ring_dqo);
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

	rte_write32(rte_cpu_to_le_32(GVE_NO_INT_MODE_DQO |
				     GVE_ITR_NO_UPDATE_DQO),
		    txq->ntfy_addr);

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

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
	gve_reset_tx_ring_state_dqo(txq);

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
