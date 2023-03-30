/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

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

uint16_t
gve_rx_burst(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	volatile struct gve_rx_desc *rxr, *rxd;
	struct gve_rx_queue *rxq = rx_queue;
	uint16_t rx_id = rxq->rx_tail;
	struct rte_mbuf *rxe;
	uint16_t nb_rx, len;
	uint64_t bytes = 0;
	uint64_t addr;
	uint16_t i;

	rxr = rxq->rx_desc_ring;
	nb_rx = 0;

	for (i = 0; i < nb_pkts; i++) {
		rxd = &rxr[rx_id];
		if (GVE_SEQNO(rxd->flags_seq) != rxq->expected_seqno)
			break;

		if (rxd->flags_seq & GVE_RXF_ERR) {
			rxq->stats.errors++;
			continue;
		}

		len = rte_be_to_cpu_16(rxd->len) - GVE_RX_PAD;
		rxe = rxq->sw_ring[rx_id];
		if (rxq->is_gqi_qpl) {
			addr = (uint64_t)(rxq->qpl->mz->addr) + rx_id * PAGE_SIZE + GVE_RX_PAD;
			rte_memcpy((void *)((size_t)rxe->buf_addr + rxe->data_off),
				   (void *)(size_t)addr, len);
		}
		rxe->pkt_len = len;
		rxe->data_len = len;
		rxe->port = rxq->port_id;
		rxe->ol_flags = 0;

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

		rxq->expected_seqno = gve_next_seqno(rxq->expected_seqno);

		rx_id++;
		if (rx_id == rxq->nb_rx_desc)
			rx_id = 0;

		rx_pkts[nb_rx] = rxe;
		bytes += len;
		nb_rx++;
	}

	rxq->nb_avail += nb_rx;
	rxq->rx_tail = rx_id;

	if (rxq->nb_avail > rxq->free_thresh)
		gve_rx_refill(rxq);

	if (nb_rx) {
		rxq->stats.packets += nb_rx;
		rxq->stats.bytes += bytes;
	}

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

void
gve_stop_rx_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	struct gve_rx_queue *rxq;
	uint16_t i;
	int err;

	err = gve_adminq_destroy_rx_queues(hw, dev->data->nb_rx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy rxqs");

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		gve_release_rxq_mbufs(rxq);
		gve_reset_rxq(rxq);
	}
}
