/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Intel Corporation
 */

#include "gve_ethdev.h"
#include "base/gve_adminq.h"

static inline void
gve_free_bulk_mbuf(struct rte_mbuf **txep, int num)
{
	struct rte_mbuf *m, *free[GVE_TX_MAX_FREE_SZ];
	int nb_free = 0;
	int i, s;

	if (unlikely(num == 0))
		return;

	/* Find the 1st mbuf which needs to be free */
	for (s = 0; s < num; s++) {
		if (txep[s] != NULL) {
			m = rte_pktmbuf_prefree_seg(txep[s]);
			if (m != NULL)
				break;
		}
	}

	if (s == num)
		return;

	free[0] = m;
	nb_free = 1;
	for (i = s + 1; i < num; i++) {
		if (likely(txep[i] != NULL)) {
			m = rte_pktmbuf_prefree_seg(txep[i]);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool)) {
					free[nb_free++] = m;
				} else {
					rte_mempool_put_bulk(free[0]->pool, (void *)free, nb_free);
					free[0] = m;
					nb_free = 1;
				}
			}
			txep[i] = NULL;
		}
	}
	rte_mempool_put_bulk(free[0]->pool, (void **)free, nb_free);
}

static inline void
gve_tx_clean(struct gve_tx_queue *txq)
{
	uint16_t mask = txq->nb_tx_desc - 1;
	uint32_t start = txq->next_to_clean & mask;
	uint32_t ntc, nb_clean, i;
	struct gve_tx_iovec *iov;

	ntc = rte_be_to_cpu_32(rte_read32(txq->qtx_head));
	ntc = ntc & mask;

	if (ntc == start)
		return;

	/* if wrap around, free twice. */
	if (ntc < start) {
		nb_clean = txq->nb_tx_desc - start;
		if (nb_clean > GVE_TX_MAX_FREE_SZ)
			nb_clean = GVE_TX_MAX_FREE_SZ;
		if (txq->is_gqi_qpl) {
			for (i = start; i < start + nb_clean; i++) {
				iov = &txq->iov_ring[i];
				txq->fifo_avail += iov->iov_len;
				iov->iov_base = 0;
				iov->iov_len = 0;
			}
		} else {
			gve_free_bulk_mbuf(&txq->sw_ring[start], nb_clean);
		}
		txq->nb_free += nb_clean;
		start += nb_clean;
		if (start == txq->nb_tx_desc)
			start = 0;
		txq->next_to_clean += nb_clean;
	}

	if (ntc > start) {
		nb_clean = ntc - start;
		if (nb_clean > GVE_TX_MAX_FREE_SZ)
			nb_clean = GVE_TX_MAX_FREE_SZ;
		if (txq->is_gqi_qpl) {
			for (i = start; i < start + nb_clean; i++) {
				iov = &txq->iov_ring[i];
				txq->fifo_avail += iov->iov_len;
				iov->iov_base = 0;
				iov->iov_len = 0;
			}
		} else {
			gve_free_bulk_mbuf(&txq->sw_ring[start], nb_clean);
		}
		txq->nb_free += nb_clean;
		txq->next_to_clean += nb_clean;
	}
}

static inline void
gve_tx_clean_swr_qpl(struct gve_tx_queue *txq)
{
	uint32_t start = txq->sw_ntc;
	uint32_t ntc, nb_clean;

	ntc = txq->sw_tail;

	if (ntc == start)
		return;

	/* if wrap around, free twice. */
	if (ntc < start) {
		nb_clean = txq->nb_tx_desc - start;
		if (nb_clean > GVE_TX_MAX_FREE_SZ)
			nb_clean = GVE_TX_MAX_FREE_SZ;
		gve_free_bulk_mbuf(&txq->sw_ring[start], nb_clean);

		txq->sw_nb_free += nb_clean;
		start += nb_clean;
		if (start == txq->nb_tx_desc)
			start = 0;
		txq->sw_ntc = start;
	}

	if (ntc > start) {
		nb_clean = ntc - start;
		if (nb_clean > GVE_TX_MAX_FREE_SZ)
			nb_clean = GVE_TX_MAX_FREE_SZ;
		gve_free_bulk_mbuf(&txq->sw_ring[start], nb_clean);
		txq->sw_nb_free += nb_clean;
		start += nb_clean;
		txq->sw_ntc = start;
	}
}

static inline void
gve_tx_fill_pkt_desc(volatile union gve_tx_desc *desc, struct rte_mbuf *mbuf,
		     uint8_t desc_cnt, uint16_t len, uint64_t addr)
{
	uint64_t csum_l4 = mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK;
	uint8_t l4_csum_offset = 0;
	uint8_t l4_hdr_offset = 0;

	if (mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
		csum_l4 |= RTE_MBUF_F_TX_TCP_CKSUM;

	switch (csum_l4) {
	case RTE_MBUF_F_TX_TCP_CKSUM:
		l4_csum_offset = offsetof(struct rte_tcp_hdr, cksum);
		l4_hdr_offset = mbuf->l2_len + mbuf->l3_len;
		break;
	case RTE_MBUF_F_TX_UDP_CKSUM:
		l4_csum_offset = offsetof(struct rte_udp_hdr, dgram_cksum);
		l4_hdr_offset = mbuf->l2_len + mbuf->l3_len;
		break;
	case RTE_MBUF_F_TX_SCTP_CKSUM:
		l4_csum_offset = offsetof(struct rte_sctp_hdr, cksum);
		l4_hdr_offset = mbuf->l2_len + mbuf->l3_len;
		break;
	}

	if (mbuf->ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		desc->pkt.type_flags = GVE_TXD_TSO | GVE_TXF_L4CSUM;
		desc->pkt.l4_csum_offset = l4_csum_offset >> 1;
		desc->pkt.l4_hdr_offset = l4_hdr_offset >> 1;
	} else if (mbuf->ol_flags & RTE_MBUF_F_TX_L4_MASK) {
		desc->pkt.type_flags = GVE_TXD_STD | GVE_TXF_L4CSUM;
		desc->pkt.l4_csum_offset = l4_csum_offset >> 1;
		desc->pkt.l4_hdr_offset = l4_hdr_offset >> 1;
	} else {
		desc->pkt.type_flags = GVE_TXD_STD;
		desc->pkt.l4_csum_offset = 0;
		desc->pkt.l4_hdr_offset = 0;
	}
	desc->pkt.desc_cnt = desc_cnt;
	desc->pkt.len = rte_cpu_to_be_16(mbuf->pkt_len);
	desc->pkt.seg_len = rte_cpu_to_be_16(len);
	desc->pkt.seg_addr = rte_cpu_to_be_64(addr);
}

static inline void
gve_tx_fill_seg_desc(volatile union gve_tx_desc *desc, uint64_t ol_flags,
		      union gve_tx_offload tx_offload,
		      uint16_t len, uint64_t addr)
{
	desc->seg.type_flags = GVE_TXD_SEG;
	if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
		if (ol_flags & RTE_MBUF_F_TX_IPV6)
			desc->seg.type_flags |= GVE_TXSF_IPV6;
		desc->seg.l3_offset = tx_offload.l2_len >> 1;
		desc->seg.mss = rte_cpu_to_be_16(tx_offload.tso_segsz);
	}
	desc->seg.seg_len = rte_cpu_to_be_16(len);
	desc->seg.seg_addr = rte_cpu_to_be_64(addr);
}

static inline bool
is_fifo_avail(struct gve_tx_queue *txq, uint16_t len)
{
	if (txq->fifo_avail < len)
		return false;
	/* Don't split segment. */
	if (txq->fifo_head + len > txq->fifo_size &&
	    txq->fifo_size - txq->fifo_head + len > txq->fifo_avail)
		return false;
	return true;
}
static inline uint64_t
gve_tx_alloc_from_fifo(struct gve_tx_queue *txq, uint16_t tx_id, uint16_t len)
{
	uint32_t head = txq->fifo_head;
	uint32_t size = txq->fifo_size;
	struct gve_tx_iovec *iov;
	uint32_t aligned_head;
	uint32_t iov_len = 0;
	uint64_t fifo_addr;

	iov = &txq->iov_ring[tx_id];

	/* Don't split segment */
	if (head + len > size) {
		iov_len += (size - head);
		head = 0;
	}

	fifo_addr = head;
	iov_len += len;
	iov->iov_base = head;

	/* Re-align to a cacheline for next head */
	head += len;
	aligned_head = RTE_ALIGN(head, RTE_CACHE_LINE_SIZE);
	iov_len += (aligned_head - head);
	iov->iov_len = iov_len;

	if (aligned_head == txq->fifo_size)
		aligned_head = 0;
	txq->fifo_head = aligned_head;
	txq->fifo_avail -= iov_len;

	return fifo_addr;
}

static inline uint16_t
gve_tx_burst_qpl(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	union gve_tx_offload tx_offload = {0};
	volatile union gve_tx_desc *txr, *txd;
	struct gve_tx_queue *txq = tx_queue;
	struct rte_mbuf **sw_ring = txq->sw_ring;
	uint16_t mask = txq->nb_tx_desc - 1;
	uint16_t tx_id = txq->tx_tail & mask;
	uint64_t ol_flags, addr, fifo_addr;
	uint32_t tx_tail = txq->tx_tail;
	struct rte_mbuf *tx_pkt, *first;
	uint16_t sw_id = txq->sw_tail;
	uint16_t nb_used, i;
	uint64_t bytes = 0;
	uint16_t nb_tx = 0;
	uint32_t hlen;

	txr = txq->tx_desc_ring;

	if (txq->nb_free < txq->free_thresh || txq->fifo_avail == 0)
		gve_tx_clean(txq);

	if (txq->sw_nb_free < txq->free_thresh)
		gve_tx_clean_swr_qpl(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;
		ol_flags = tx_pkt->ol_flags;

		if (txq->sw_nb_free < tx_pkt->nb_segs) {
			gve_tx_clean_swr_qpl(txq);
			if (txq->sw_nb_free < tx_pkt->nb_segs)
				goto end_of_tx;
		}

		/* Even for multi-segs, use 1 qpl buf for data */
		nb_used = 1;
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			nb_used++;

		if (txq->nb_free < nb_used)
			goto end_of_tx;

		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;

		first = tx_pkt;
		txd = &txr[tx_id];
		hlen = ol_flags & RTE_MBUF_F_TX_TCP_SEG ?
			(uint32_t)(tx_offload.l2_len + tx_offload.l3_len + tx_offload.l4_len) :
			tx_pkt->pkt_len;

		sw_ring[sw_id] = tx_pkt;
		if (!is_fifo_avail(txq, hlen)) {
			gve_tx_clean(txq);
			if (!is_fifo_avail(txq, hlen))
				goto end_of_tx;
		}
		addr = (uint64_t)(tx_pkt->buf_addr) + tx_pkt->data_off;
		fifo_addr = gve_tx_alloc_from_fifo(txq, tx_id, hlen);

		/* For TSO, check if there's enough fifo space for data first */
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
			if (!is_fifo_avail(txq, tx_pkt->pkt_len - hlen)) {
				gve_tx_clean(txq);
				if (!is_fifo_avail(txq, tx_pkt->pkt_len - hlen))
					goto end_of_tx;
			}
		}
		if (tx_pkt->nb_segs == 1 || ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			rte_memcpy((void *)(size_t)(fifo_addr + txq->fifo_base),
				   (void *)(size_t)addr, hlen);
		else
			rte_pktmbuf_read(tx_pkt, 0, hlen,
					 (void *)(size_t)(fifo_addr + txq->fifo_base));
		gve_tx_fill_pkt_desc(txd, tx_pkt, nb_used, hlen, fifo_addr);

		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
			tx_id = (tx_id + 1) & mask;
			txd = &txr[tx_id];
			addr = (uint64_t)(tx_pkt->buf_addr) + tx_pkt->data_off + hlen;
			fifo_addr = gve_tx_alloc_from_fifo(txq, tx_id, tx_pkt->pkt_len - hlen);
			if (tx_pkt->nb_segs == 1)
				rte_memcpy((void *)(size_t)(fifo_addr + txq->fifo_base),
					   (void *)(size_t)addr,
					   tx_pkt->pkt_len - hlen);
			else
				rte_pktmbuf_read(tx_pkt, hlen, tx_pkt->pkt_len - hlen,
						 (void *)(size_t)(fifo_addr + txq->fifo_base));

			gve_tx_fill_seg_desc(txd, ol_flags, tx_offload,
					     tx_pkt->pkt_len - hlen, fifo_addr);
		}

		/* record mbuf in sw_ring for free */
		for (i = 1; i < first->nb_segs; i++) {
			sw_id = (sw_id + 1) & mask;
			tx_pkt = tx_pkt->next;
			sw_ring[sw_id] = tx_pkt;
		}

		sw_id = (sw_id + 1) & mask;
		tx_id = (tx_id + 1) & mask;

		txq->nb_free -= nb_used;
		txq->sw_nb_free -= first->nb_segs;
		tx_tail += nb_used;

		bytes += first->pkt_len;
	}

end_of_tx:
	if (nb_tx) {
		rte_write32(rte_cpu_to_be_32(tx_tail), txq->qtx_tail);
		txq->tx_tail = tx_tail;
		txq->sw_tail = sw_id;

		txq->stats.packets += nb_tx;
		txq->stats.bytes += bytes;
		txq->stats.errors += nb_pkts - nb_tx;
	}

	return nb_tx;
}

static inline uint16_t
gve_tx_burst_ra(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	union gve_tx_offload tx_offload = {0};
	volatile union gve_tx_desc *txr, *txd;
	struct gve_tx_queue *txq = tx_queue;
	struct rte_mbuf **sw_ring = txq->sw_ring;
	uint16_t mask = txq->nb_tx_desc - 1;
	uint16_t tx_id = txq->tx_tail & mask;
	uint32_t tx_tail = txq->tx_tail;
	struct rte_mbuf *tx_pkt, *first;
	uint16_t nb_used, hlen, i;
	uint64_t ol_flags, addr;
	uint64_t bytes = 0;
	uint16_t nb_tx = 0;

	txr = txq->tx_desc_ring;

	if (txq->nb_free < txq->free_thresh)
		gve_tx_clean(txq);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		tx_pkt = *tx_pkts++;
		ol_flags = tx_pkt->ol_flags;

		nb_used = tx_pkt->nb_segs;
		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			nb_used++;

		if (txq->nb_free < nb_used)
			goto end_of_tx;

		tx_offload.l2_len = tx_pkt->l2_len;
		tx_offload.l3_len = tx_pkt->l3_len;
		tx_offload.l4_len = tx_pkt->l4_len;
		tx_offload.tso_segsz = tx_pkt->tso_segsz;

		first = tx_pkt;
		txd = &txr[tx_id];

		hlen = ol_flags & RTE_MBUF_F_TX_TCP_SEG ?
			(uint32_t)(tx_offload.l2_len + tx_offload.l3_len + tx_offload.l4_len) :
			tx_pkt->pkt_len;
		/*
		 * if tso, the driver needs to fill 2 descs for 1 mbuf
		 * so only put this mbuf into the 1st tx entry in sw ring
		 */
		sw_ring[tx_id] = tx_pkt;
		addr = rte_mbuf_data_iova(tx_pkt);
		gve_tx_fill_pkt_desc(txd, tx_pkt, nb_used, hlen, addr);

		if (ol_flags & RTE_MBUF_F_TX_TCP_SEG) {
			tx_id = (tx_id + 1) & mask;
			txd = &txr[tx_id];
			addr = rte_mbuf_data_iova(tx_pkt) + hlen;
			gve_tx_fill_seg_desc(txd, ol_flags, tx_offload,
					     tx_pkt->data_len - hlen, addr);
		}

		for (i = 1; i < first->nb_segs; i++) {
			tx_id = (tx_id + 1) & mask;
			txd = &txr[tx_id];
			tx_pkt = tx_pkt->next;
			sw_ring[tx_id] = tx_pkt;
			addr = rte_mbuf_data_iova(tx_pkt);
			gve_tx_fill_seg_desc(txd, ol_flags, tx_offload,
					     tx_pkt->data_len, addr);
		}
		tx_id = (tx_id + 1) & mask;

		txq->nb_free -= nb_used;
		tx_tail += nb_used;

		bytes += first->pkt_len;
	}

end_of_tx:
	if (nb_tx) {
		rte_write32(rte_cpu_to_be_32(tx_tail), txq->qtx_tail);
		txq->tx_tail = tx_tail;

		txq->stats.packets += nb_tx;
		txq->stats.bytes += bytes;
		txq->stats.errors += nb_pkts - nb_tx;
	}

	return nb_tx;
}

uint16_t
gve_tx_burst(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct gve_tx_queue *txq = tx_queue;

	if (txq->is_gqi_qpl)
		return gve_tx_burst_qpl(tx_queue, tx_pkts, nb_pkts);

	return gve_tx_burst_ra(tx_queue, tx_pkts, nb_pkts);
}

static inline void
gve_reset_txq(struct gve_tx_queue *txq)
{
	struct rte_mbuf **sw_ring;
	uint32_t size, i;

	if (txq == NULL) {
		PMD_DRV_LOG(ERR, "Pointer to txq is NULL");
		return;
	}

	size = txq->nb_tx_desc * sizeof(union gve_tx_desc);
	for (i = 0; i < size; i++)
		((volatile char *)txq->tx_desc_ring)[i] = 0;

	sw_ring = txq->sw_ring;
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
gve_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct gve_tx_queue *q = dev->data->tx_queues[qid];

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
		gve_tx_queue_release(dev, queue_id);
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

int
gve_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id)
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
gve_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id)
{
	struct gve_tx_queue *txq;

	if (tx_queue_id >= dev->data->nb_tx_queues)
		return -EINVAL;

	txq = dev->data->tx_queues[tx_queue_id];
	gve_release_txq_mbufs(txq);
	gve_reset_txq(txq);

	dev->data->tx_queue_state[tx_queue_id] = RTE_ETH_QUEUE_STATE_STOPPED;

	return 0;
}

void
gve_stop_tx_queues(struct rte_eth_dev *dev)
{
	struct gve_priv *hw = dev->data->dev_private;
	uint16_t i;
	int err;

	if (!gve_is_gqi(hw))
		return gve_stop_tx_queues_dqo(dev);

	err = gve_adminq_destroy_tx_queues(hw, dev->data->nb_tx_queues);
	if (err != 0)
		PMD_DRV_LOG(WARNING, "failed to destroy txqs");

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		if (gve_tx_queue_stop(dev, i) != 0)
			PMD_DRV_LOG(WARNING, "Fail to stop Tx queue %d", i);
}

void
gve_set_tx_function(struct rte_eth_dev *dev)
{
	dev->tx_pkt_burst = gve_tx_burst;
}
