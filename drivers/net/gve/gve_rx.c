/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

#define GVE_PKT_CONT_BIT_IS_SET(x) (GVE_RXF_PKT_CONT & (x))

static inline void
gve_rx_refill(struct gve_rx_queue *rxq)
{
	uint16_t mask = rxq->nb_rx_desc - 1;
	uint16_t idx = rxq->next_avail & mask;
	uint32_t next_avail = rxq->next_avail;
	uint16_t nb_alloc, i;
	struct rte_mbuf *nmb;
	int diag;

	/* wrap around */
	nb_alloc = rxq->nb_rx_desc - idx;
	if (nb_alloc <= rxq->nb_avail) {
		diag = rte_pktmbuf_alloc_bulk(rxq->mpool, &rxq->sw_ring[idx], nb_alloc);
		if (diag < 0) {
			rxq->stats.no_mbufs_bulk++;
			for (i = 0; i < nb_alloc; i++) {
				nmb = rte_pktmbuf_alloc(rxq->mpool);
				if (!nmb)
					break;
				rxq->sw_ring[idx + i] = nmb;
			}
			if (i != nb_alloc) {
				rxq->stats.no_mbufs += nb_alloc - i;
				nb_alloc = i;
			}
		}
		rxq->nb_avail -= nb_alloc;
		next_avail += nb_alloc;

		/* queue page list mode doesn't need real refill. */
		if (rxq->is_gqi_qpl) {
			idx += nb_alloc;
		} else {
			for (i = 0; i < nb_alloc; i++) {
				nmb = rxq->sw_ring[idx];
				rxq->rx_data_ring[idx].addr =
					rte_cpu_to_be_64(rte_mbuf_data_iova(nmb));
				idx++;
			}
		}
		if (idx == rxq->nb_rx_desc)
			idx = 0;
	}

	if (rxq->nb_avail > 0) {
		nb_alloc = rxq->nb_avail;
		if (rxq->nb_rx_desc < idx + rxq->nb_avail)
			nb_alloc = rxq->nb_rx_desc - idx;
		diag = rte_pktmbuf_alloc_bulk(rxq->mpool, &rxq->sw_ring[idx], nb_alloc);
		if (diag < 0) {
			rxq->stats.no_mbufs_bulk++;
			for (i = 0; i < nb_alloc; i++) {
				nmb = rte_pktmbuf_alloc(rxq->mpool);
				if (!nmb)
					break;
				rxq->sw_ring[idx + i] = nmb;
			}
			if (i != nb_alloc) {
				rxq->stats.no_mbufs += nb_alloc - i;
				nb_alloc = i;
			}
		}
		rxq->nb_avail -= nb_alloc;
		next_avail += nb_alloc;

		if (!rxq->is_gqi_qpl) {
			for (i = 0; i < nb_alloc; i++) {
				nmb = rxq->sw_ring[idx];
				rxq->rx_data_ring[idx].addr =
					rte_cpu_to_be_64(rte_mbuf_data_iova(nmb));
				idx++;
			}
		}
	}

	if (next_avail != rxq->next_avail) {
		rte_write32(rte_cpu_to_be_32(next_avail), rxq->qrx_tail);
		rxq->next_avail = next_avail;
	}
}

/*
 * This method processes a single rte_mbuf and handles packet segmentation
 * In QPL mode it copies data from the mbuf to the gve_rx_queue.
 */
static void
gve_rx_mbuf(struct gve_rx_queue *rxq, struct rte_mbuf *rxe, uint16_t len,
	    uint16_t rx_id)
{
	uint16_t padding = 0;
	uint64_t addr;

	rxe->data_len = len;
	if (!rxq->ctx.mbuf_head) {
		rxq->ctx.mbuf_head = rxe;
		rxq->ctx.mbuf_tail = rxe;
		rxe->nb_segs = 1;
		rxe->pkt_len = len;
		rxe->data_len = len;
		rxe->port = rxq->port_id;
		rxe->ol_flags = 0;
		padding = GVE_RX_PAD;
	} else {
		rxq->ctx.mbuf_head->pkt_len += len;
		rxq->ctx.mbuf_head->nb_segs += 1;
		rxq->ctx.mbuf_tail->next = rxe;
		rxq->ctx.mbuf_tail = rxe;
	}
	if (rxq->is_gqi_qpl) {
		addr = (uint64_t)(rxq->qpl->mz->addr) + rx_id * PAGE_SIZE + padding;
		rte_memcpy((void *)((size_t)rxe->buf_addr + rxe->data_off),
				    (void *)(size_t)addr, len);
	}
}

/*
 * This method processes a single packet fragment associated with the
 * passed packet descriptor.
 * This methods returns whether the fragment is the last fragment
 * of a packet.
 */
static bool
gve_rx(struct gve_rx_queue *rxq, volatile struct gve_rx_desc *rxd, uint16_t rx_id)
{
	bool is_last_frag = !GVE_PKT_CONT_BIT_IS_SET(rxd->flags_seq);
	uint16_t frag_size = rte_be_to_cpu_16(rxd->len);
	struct gve_rx_ctx *ctx = &rxq->ctx;
	bool is_first_frag = ctx->total_frags == 0;
	struct rte_mbuf *rxe;

	if (ctx->drop_pkt)
		goto finish_frag;

	if (rxd->flags_seq & GVE_RXF_ERR) {
		ctx->drop_pkt = true;
		rxq->stats.errors++;
		goto finish_frag;
	}

	if (is_first_frag)
		frag_size -= GVE_RX_PAD;

	rxe = rxq->sw_ring[rx_id];
	gve_rx_mbuf(rxq, rxe, frag_size, rx_id);
	rxq->stats.bytes += frag_size;

	if (is_first_frag) {
		if (rxd->flags_seq & GVE_RXF_TCP)
			rxe->packet_type |= RTE_PTYPE_L4_TCP;
		if (rxd->flags_seq & GVE_RXF_UDP)
			rxe->packet_type |= RTE_PTYPE_L4_UDP;
		if (rxd->flags_seq & GVE_RXF_IPV4)
			rxe->packet_type |= RTE_PTYPE_L3_IPV4;
		if (rxd->flags_seq & GVE_RXF_IPV6)
			rxe->packet_type |= RTE_PTYPE_L3_IPV6;

		if (gve_needs_rss(rxd->flags_seq)) {
			rxe->ol_flags |= RTE_MBUF_F_RX_RSS_HASH;
			rxe->hash.rss = rte_be_to_cpu_32(rxd->rss_hash);
		}
	}

finish_frag:
	ctx->total_frags++;
	return is_last_frag;
}

static void
gve_rx_ctx_clear(struct gve_rx_ctx *ctx)
{
	ctx->mbuf_head = NULL;
	ctx->mbuf_tail = NULL;
	ctx->drop_pkt = false;
	ctx->total_frags = 0;
}

uint16_t
gve_rx_burst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile struct gve_rx_desc *rxr, *rxd;
	struct gve_rx_queue *rxq = rx_queue;
	struct gve_rx_ctx *ctx = &rxq->ctx;
	uint16_t rx_id = rxq->rx_tail;
	uint16_t nb_rx;

	rxr = rxq->rx_desc_ring;
	nb_rx = 0;

	while (nb_rx < nb_pkts) {
		rxd = &rxr[rx_id];
		if (GVE_SEQNO(rxd->flags_seq) != rxq->expected_seqno)
			break;

		if (gve_rx(rxq, rxd, rx_id)) {
			if (!ctx->drop_pkt)
				rx_pkts[nb_rx++] = ctx->mbuf_head;
			rxq->nb_avail += ctx->total_frags;
			gve_rx_ctx_clear(ctx);
		}

		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		rxq->expected_seqno = gve_next_seqno(rxq->expected_seqno);
	}

	rxq->rx_tail = rx_id;

	if (rxq->nb_avail > rxq->free_thresh)
		gve_rx_refill(rxq);

	if (nb_rx)
		rxq->stats.packets += nb_rx;

	return nb_rx;
}

static inline void
gve_reset_rxq(struct gve_rx_queue *rxq)
{
	struct rte_mbuf **sw_ring;
	uint32_t size, i;

	if (rxq == NULL) {
		PMD_DRV_LOG(ERR, "pointer to rxq is NULL");
		return;
	}

	size = rxq->nb_rx_desc * sizeof(struct gve_rx_desc);
	for (i = 0; i < size; i++)
		((volatile char *)rxq->rx_desc_ring)[i] = 0;

	size = rxq->nb_rx_desc * sizeof(union gve_rx_data_slot);
	for (i = 0; i < size; i++)
		((volatile char *)rxq->rx_data_ring)[i] = 0;

	sw_ring = rxq->sw_ring;
	for (i = 0; i < rxq->nb_rx_desc; i++)
		sw_ring[i] = NULL;

	rxq->rx_tail = 0;
	rxq->next_avail = 0;
	rxq->nb_avail = rxq->nb_rx_desc;
	rxq->expected_seqno = 1;
}

static inline void
gve_release_rxq_mbufs(struct gve_rx_queue *rxq)
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
gve_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct gve_rx_queue *q = dev->data->rx_queues[qid];

	if (!q)
		return;

	if (q->is_gqi_qpl) {
		gve_adminq_unregister_page_list(q->hw, q->qpl->id);
		q->qpl = NULL;
	}

	gve_release_rxq_mbufs(q);
	rte_free(q->sw_ring);
	rte_memzone_free(q->data_mz);
	rte_memzone_free(q->mz);
	rte_memzone_free(q->qres_mz);
	q->qres = NULL;
	rte_free(q);
}

int
gve_rx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_id,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *conf, struct rte_mempool *pool)
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

	/* Free memory if needed. */
	if (dev->data->rx_queues[queue_id]) {
		gve_rx_queue_release(dev, queue_id);
		dev->data->rx_queues[queue_id] = NULL;
	}

	/* Allocate the RX queue data structure. */
	rxq = rte_zmalloc_socket("gve rxq",
				 sizeof(struct gve_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 socket_id);
	if (!rxq) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for rx queue structure");
		err = -ENOMEM;
		goto err_rxq;
	}

	free_thresh = conf->rx_free_thresh ? conf->rx_free_thresh : GVE_DEFAULT_RX_FREE_THRESH;
	if (free_thresh >= nb_desc) {
		PMD_DRV_LOG(ERR, "rx_free_thresh (%u) must be less than nb_desc (%u) minus 3.",
			    free_thresh, rxq->nb_rx_desc);
		err = -EINVAL;
		goto err_rxq;
	}

	rxq->nb_rx_desc = nb_desc;
	rxq->free_thresh = free_thresh;
	rxq->queue_id = queue_id;
	rxq->port_id = dev->data->port_id;
	rxq->ntfy_id = hw->num_ntfy_blks / 2 + queue_id;
	rxq->is_gqi_qpl = hw->queue_format == GVE_GQI_QPL_FORMAT;
	rxq->mpool = pool;
	rxq->hw = hw;
	rxq->ntfy_addr = &hw->db_bar2[rte_be_to_cpu_32(hw->irq_dbs[rxq->ntfy_id].id)];

	rxq->rx_buf_len = rte_pktmbuf_data_room_size(rxq->mpool) - RTE_PKTMBUF_HEADROOM;

	/* Allocate software ring */
	rxq->sw_ring = rte_zmalloc_socket("gve rx sw ring", sizeof(struct rte_mbuf *) * nb_desc,
					  RTE_CACHE_LINE_SIZE, socket_id);
	if (!rxq->sw_ring) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for SW RX ring");
		err = -ENOMEM;
		goto err_rxq;
	}

	mz = rte_eth_dma_zone_reserve(dev, "rx_ring", queue_id,
				      nb_desc * sizeof(struct gve_rx_desc),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX");
		err = -ENOMEM;
		goto err_sw_ring;
	}
	rxq->rx_desc_ring = (struct gve_rx_desc *)mz->addr;
	rxq->rx_ring_phys_addr = mz->iova;
	rxq->mz = mz;

	mz = rte_eth_dma_zone_reserve(dev, "gve rx data ring", queue_id,
				      sizeof(union gve_rx_data_slot) * nb_desc,
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate memory for RX data ring");
		err = -ENOMEM;
		goto err_rx_ring;
	}
	rxq->rx_data_ring = (union gve_rx_data_slot *)mz->addr;
	rxq->data_mz = mz;
	if (rxq->is_gqi_qpl) {
		rxq->qpl = &hw->qpl[rxq->ntfy_id];
		err = gve_adminq_register_page_list(hw, rxq->qpl);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to register qpl %u", queue_id);
			goto err_data_ring;
		}
	}

	mz = rte_eth_dma_zone_reserve(dev, "rxq_res", queue_id,
				      sizeof(struct gve_queue_resources),
				      PAGE_SIZE, socket_id);
	if (mz == NULL) {
		PMD_DRV_LOG(ERR, "Failed to reserve DMA memory for RX resource");
		err = -ENOMEM;
		goto err_data_ring;
	}
	rxq->qres = (struct gve_queue_resources *)mz->addr;
	rxq->qres_mz = mz;

	gve_reset_rxq(rxq);

	dev->data->rx_queues[queue_id] = rxq;

	return 0;

err_data_ring:
	rte_memzone_free(rxq->data_mz);
err_rx_ring:
	rte_memzone_free(rxq->mz);
err_sw_ring:
	rte_free(rxq->sw_ring);
err_rxq:
	rte_free(rxq);
	return err;
}

static int
gve_rxq_mbufs_alloc(struct gve_rx_queue *rxq)
{
	struct rte_mbuf *nmb;
	uint16_t i;
	int diag;

	diag = rte_pktmbuf_alloc_bulk(rxq->mpool, &rxq->sw_ring[0], rxq->nb_rx_desc);
	if (diag < 0) {
		for (i = 0; i < rxq->nb_rx_desc - 1; i++) {
			nmb = rte_pktmbuf_alloc(rxq->mpool);
			if (!nmb)
				break;
			rxq->sw_ring[i] = nmb;
		}
		if (i < rxq->nb_rx_desc - 1)
			return -ENOMEM;
	}
	rxq->nb_avail = 0;
	rxq->next_avail = rxq->nb_rx_desc - 1;

	for (i = 0; i < rxq->nb_rx_desc; i++) {
		if (rxq->is_gqi_qpl) {
			rxq->rx_data_ring[i].addr = rte_cpu_to_be_64(i * PAGE_SIZE);
		} else {
			if (i == rxq->nb_rx_desc - 1)
				break;
			nmb = rxq->sw_ring[i];
			rxq->rx_data_ring[i].addr = rte_cpu_to_be_64(rte_mbuf_data_iova(nmb));
		}
	}

	rte_write32(rte_cpu_to_be_32(rxq->next_avail), rxq->qrx_tail);

	return 0;
}

int
gve_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct gve_priv *hw = dev->data->dev_private;
	struct gve_rx_queue *rxq;
	int ret;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];

	rxq->qrx_tail = &hw->db_bar2[rte_be_to_cpu_32(rxq->qres->db_index)];

	rte_write32(rte_cpu_to_be_32(GVE_IRQ_MASK), rxq->ntfy_addr);

	ret = gve_rxq_mbufs_alloc(rxq);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to alloc Rx queue mbuf");
		return ret;
	}

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

int
gve_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id)
{
	struct gve_rx_queue *rxq;

	if (rx_queue_id >= dev->data->nb_rx_queues)
		return -EINVAL;

	rxq = dev->data->rx_queues[rx_queue_id];
	gve_release_rxq_mbufs(rxq);
	gve_reset_rxq(rxq);

	dev->data->rx_queue_state[rx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
gve_stop_rx_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	uint16_t i;
	int err;

	if (!gve_is_gqi(hw))
		return gve_stop_rx_queues_dqo(dev);

	err = gve_adminq_destroy_rx_queues(hw, dev->data->nb_rx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy rxqs");

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		if (gve_rx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Rx queue %d", i);
}

void
gve_set_rx_function(struct rte_eth_dev *dev)
{
	dev->rx_pkt_burst = gve_rx_burst;
}
