/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_lcore.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_log.h>
#include <rte_pci.h>
#include <rte_mbuf.h>
#include <rte_bus_pci.h>
#include <rte_memzone.h>
#include <rte_memcpy.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "ntb_hw_intel.h"
#include "rte_pmd_ntb.h"
#include "ntb.h"

static const struct rte_pci_id pci_id_ntb_map[] = {
	{ RTE_PCI_DEVICE(NTB_INTEL_VENDOR_ID, NTB_INTEL_DEV_ID_B2B_SKX) },
	{ RTE_PCI_DEVICE(NTB_INTEL_VENDOR_ID, NTB_INTEL_DEV_ID_B2B_ICX) },
	{ .vendor_id = 0, /* sentinel */ },
};

/* Align with enum ntb_xstats_idx */
static struct rte_rawdev_xstats_name ntb_xstats_names[] = {
	{"Tx-packets"},
	{"Tx-bytes"},
	{"Tx-errors"},
	{"Rx-packets"},
	{"Rx-bytes"},
	{"Rx-missed"},
};
#define NTB_XSTATS_NUM RTE_DIM(ntb_xstats_names)

static inline void
ntb_link_cleanup(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	int status, i;

	if (hw->ntb_ops->spad_write == NULL ||
	    hw->ntb_ops->mw_set_trans == NULL) {
		NTB_LOG(ERR, "Not supported to clean up link.");
		return;
	}

	/* Clean spad registers. */
	for (i = 0; i < hw->spad_cnt; i++) {
		status = (*hw->ntb_ops->spad_write)(dev, i, 0, 0);
		if (status)
			NTB_LOG(ERR, "Failed to clean local spad.");
	}

	/* Clear mw so that peer cannot access local memory.*/
	for (i = 0; i < hw->used_mw_num; i++) {
		status = (*hw->ntb_ops->mw_set_trans)(dev, i, 0, 0);
		if (status)
			NTB_LOG(ERR, "Failed to clean mw.");
	}
}

static inline int
ntb_handshake_work(const struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t val;
	int ret, i;

	if (hw->ntb_ops->spad_write == NULL ||
	    hw->ntb_ops->mw_set_trans == NULL) {
		NTB_LOG(ERR, "Scratchpad/MW setting is not supported.");
		return -ENOTSUP;
	}

	/* Tell peer the mw info of local side. */
	ret = (*hw->ntb_ops->spad_write)(dev, SPAD_NUM_MWS, 1, hw->mw_cnt);
	if (ret < 0)
		return ret;
	for (i = 0; i < hw->mw_cnt; i++) {
		NTB_LOG(INFO, "Local %u mw size: 0x%"PRIx64"", i,
				hw->mw_size[i]);
		val = hw->mw_size[i] >> 32;
		ret = (*hw->ntb_ops->spad_write)(dev, SPAD_MW0_SZ_H + 2 * i,
						 1, val);
		if (ret < 0)
			return ret;
		val = hw->mw_size[i];
		ret = (*hw->ntb_ops->spad_write)(dev, SPAD_MW0_SZ_L + 2 * i,
						 1, val);
		if (ret < 0)
			return ret;
	}

	/* Tell peer about the queue info and map memory to the peer. */
	ret = (*hw->ntb_ops->spad_write)(dev, SPAD_Q_SZ, 1, hw->queue_size);
	if (ret < 0)
		return ret;
	ret = (*hw->ntb_ops->spad_write)(dev, SPAD_NUM_QPS, 1,
					 hw->queue_pairs);
	if (ret < 0)
		return ret;
	ret = (*hw->ntb_ops->spad_write)(dev, SPAD_USED_MWS, 1,
					 hw->used_mw_num);
	if (ret < 0)
		return ret;
	for (i = 0; i < hw->used_mw_num; i++) {
		val = (uint64_t)(size_t)(hw->mz[i]->addr) >> 32;
		ret = (*hw->ntb_ops->spad_write)(dev, SPAD_MW0_BA_H + 2 * i,
						 1, val);
		if (ret < 0)
			return ret;
		val = (uint64_t)(size_t)(hw->mz[i]->addr);
		ret = (*hw->ntb_ops->spad_write)(dev, SPAD_MW0_BA_L + 2 * i,
						 1, val);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < hw->used_mw_num; i++) {
		ret = (*hw->ntb_ops->mw_set_trans)(dev, i, hw->mz[i]->iova,
						   hw->mz[i]->len);
		if (ret < 0)
			return ret;
	}

	/* Ring doorbell 0 to tell peer the device is ready. */
	ret = (*hw->ntb_ops->peer_db_set)(dev, 0);
	if (ret < 0)
		return ret;

	return 0;
}

static void
ntb_dev_intr_handler(void *param)
{
	struct rte_rawdev *dev = (struct rte_rawdev *)param;
	struct ntb_hw *hw = dev->dev_private;
	uint32_t val_h, val_l;
	uint64_t peer_mw_size;
	uint64_t db_bits = 0;
	uint8_t peer_mw_cnt;
	int i = 0;

	if (hw->ntb_ops->db_read == NULL ||
	    hw->ntb_ops->db_clear == NULL ||
	    hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Doorbell is not supported.");
		return;
	}

	db_bits = (*hw->ntb_ops->db_read)(dev);
	if (!db_bits)
		NTB_LOG(ERR, "No doorbells");

	/* Doorbell 0 is for peer device ready. */
	if (db_bits & 1) {
		NTB_LOG(INFO, "DB0: Peer device is up.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, 1);

		/**
		 * Peer dev is already up. All mw settings are already done.
		 * Skip them.
		 */
		if (hw->peer_dev_up)
			return;

		if (hw->ntb_ops->spad_read == NULL) {
			NTB_LOG(ERR, "Scratchpad read is not supported.");
			return;
		}

		/* Check if mw setting on the peer is the same as local. */
		peer_mw_cnt = (*hw->ntb_ops->spad_read)(dev, SPAD_NUM_MWS, 0);
		if (peer_mw_cnt != hw->mw_cnt) {
			NTB_LOG(ERR, "Both mw cnt must be the same.");
			return;
		}

		for (i = 0; i < hw->mw_cnt; i++) {
			val_h = (*hw->ntb_ops->spad_read)
				(dev, SPAD_MW0_SZ_H + 2 * i, 0);
			val_l = (*hw->ntb_ops->spad_read)
				(dev, SPAD_MW0_SZ_L + 2 * i, 0);
			peer_mw_size = ((uint64_t)val_h << 32) | val_l;
			NTB_LOG(DEBUG, "Peer %u mw size: 0x%"PRIx64"", i,
					peer_mw_size);
			if (peer_mw_size != hw->mw_size[i]) {
				NTB_LOG(ERR, "Mw config must be the same.");
				return;
			}
		}

		hw->peer_dev_up = 1;

		/**
		 * Handshake with peer. Spad_write & mw_set_trans only works
		 * when both devices are up. So write spad again when db is
		 * received. And set db again for the later device who may miss
		 * the 1st db.
		 */
		if (ntb_handshake_work(dev) < 0) {
			NTB_LOG(ERR, "Handshake work failed.");
			return;
		}

		/* To get the link info. */
		if (hw->ntb_ops->get_link_status == NULL) {
			NTB_LOG(ERR, "Not supported to get link status.");
			return;
		}
		(*hw->ntb_ops->get_link_status)(dev);
		NTB_LOG(INFO, "Link is up. Link speed: %u. Link width: %u",
			hw->link_speed, hw->link_width);
		return;
	}

	if (db_bits & (1 << 1)) {
		NTB_LOG(INFO, "DB1: Peer device is down.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, 2);

		/* Peer device will be down, So clean local side too. */
		ntb_link_cleanup(dev);

		hw->peer_dev_up = 0;
		/* Response peer's dev_stop request. */
		(*hw->ntb_ops->peer_db_set)(dev, 2);
		return;
	}

	if (db_bits & (1 << 2)) {
		NTB_LOG(INFO, "DB2: Peer device agrees dev to be down.");
		/* Clear received doorbell. */
		(*hw->ntb_ops->db_clear)(dev, (1 << 2));
		hw->peer_dev_up = 0;
		return;
	}

	/* Clear other received doorbells. */
	(*hw->ntb_ops->db_clear)(dev, db_bits);
}

static int
ntb_queue_conf_get(struct rte_rawdev *dev,
		   uint16_t queue_id,
		   rte_rawdev_obj_t queue_conf,
		   size_t conf_size)
{
	struct ntb_queue_conf *q_conf = queue_conf;
	struct ntb_hw *hw = dev->dev_private;

	if (conf_size != sizeof(*q_conf))
		return -EINVAL;

	q_conf->tx_free_thresh = hw->tx_queues[queue_id]->tx_free_thresh;
	q_conf->nb_desc = hw->rx_queues[queue_id]->nb_rx_desc;
	q_conf->rx_mp = hw->rx_queues[queue_id]->mpool;

	return 0;
}

static void
ntb_rxq_release_mbufs(struct ntb_rx_queue *q)
{
	int i;

	if (!q || !q->sw_ring) {
		NTB_LOG(ERR, "Pointer to rxq or sw_ring is NULL");
		return;
	}

	for (i = 0; i < q->nb_rx_desc; i++) {
		if (q->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(q->sw_ring[i].mbuf);
			q->sw_ring[i].mbuf = NULL;
		}
	}
}

static void
ntb_rxq_release(struct ntb_rx_queue *rxq)
{
	if (!rxq) {
		NTB_LOG(ERR, "Pointer to rxq is NULL");
		return;
	}

	ntb_rxq_release_mbufs(rxq);

	rte_free(rxq->sw_ring);
	rte_free(rxq);
}

static int
ntb_rxq_setup(struct rte_rawdev *dev,
	      uint16_t qp_id,
	      rte_rawdev_obj_t queue_conf,
	      size_t conf_size)
{
	struct ntb_queue_conf *rxq_conf = queue_conf;
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_rx_queue *rxq;

	if (conf_size != sizeof(*rxq_conf))
		return -EINVAL;

	/* Allocate the rx queue data structure */
	rxq = rte_zmalloc_socket("ntb rx queue",
				 sizeof(struct ntb_rx_queue),
				 RTE_CACHE_LINE_SIZE,
				 dev->socket_id);
	if (!rxq) {
		NTB_LOG(ERR, "Failed to allocate memory for "
			    "rx queue data structure.");
		return -ENOMEM;
	}

	if (rxq_conf->rx_mp == NULL) {
		NTB_LOG(ERR, "Invalid null mempool pointer.");
		return -EINVAL;
	}
	rxq->nb_rx_desc = rxq_conf->nb_desc;
	rxq->mpool = rxq_conf->rx_mp;
	rxq->port_id = dev->dev_id;
	rxq->queue_id = qp_id;
	rxq->hw = hw;

	/* Allocate the software ring. */
	rxq->sw_ring =
		rte_zmalloc_socket("ntb rx sw ring",
				   sizeof(struct ntb_rx_entry) *
				   rxq->nb_rx_desc,
				   RTE_CACHE_LINE_SIZE,
				   dev->socket_id);
	if (!rxq->sw_ring) {
		ntb_rxq_release(rxq);
		rxq = NULL;
		NTB_LOG(ERR, "Failed to allocate memory for SW ring");
		return -ENOMEM;
	}

	hw->rx_queues[qp_id] = rxq;

	return 0;
}

static void
ntb_txq_release_mbufs(struct ntb_tx_queue *q)
{
	int i;

	if (!q || !q->sw_ring) {
		NTB_LOG(ERR, "Pointer to txq or sw_ring is NULL");
		return;
	}

	for (i = 0; i < q->nb_tx_desc; i++) {
		if (q->sw_ring[i].mbuf) {
			rte_pktmbuf_free_seg(q->sw_ring[i].mbuf);
			q->sw_ring[i].mbuf = NULL;
		}
	}
}

static void
ntb_txq_release(struct ntb_tx_queue *txq)
{
	if (!txq) {
		NTB_LOG(ERR, "Pointer to txq is NULL");
		return;
	}

	ntb_txq_release_mbufs(txq);

	rte_free(txq->sw_ring);
	rte_free(txq);
}

static int
ntb_txq_setup(struct rte_rawdev *dev,
	      uint16_t qp_id,
	      rte_rawdev_obj_t queue_conf,
	      size_t conf_size)
{
	struct ntb_queue_conf *txq_conf = queue_conf;
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_tx_queue *txq;
	uint16_t i, prev;

	if (conf_size != sizeof(*txq_conf))
		return -EINVAL;

	/* Allocate the TX queue data structure. */
	txq = rte_zmalloc_socket("ntb tx queue",
				  sizeof(struct ntb_tx_queue),
				  RTE_CACHE_LINE_SIZE,
				  dev->socket_id);
	if (!txq) {
		NTB_LOG(ERR, "Failed to allocate memory for "
			    "tx queue structure");
		return -ENOMEM;
	}

	txq->nb_tx_desc = txq_conf->nb_desc;
	txq->port_id = dev->dev_id;
	txq->queue_id = qp_id;
	txq->hw = hw;

	/* Allocate software ring */
	txq->sw_ring =
		rte_zmalloc_socket("ntb tx sw ring",
				   sizeof(struct ntb_tx_entry) *
				   txq->nb_tx_desc,
				   RTE_CACHE_LINE_SIZE,
				   dev->socket_id);
	if (!txq->sw_ring) {
		ntb_txq_release(txq);
		txq = NULL;
		NTB_LOG(ERR, "Failed to allocate memory for SW TX ring");
		return -ENOMEM;
	}

	prev = txq->nb_tx_desc - 1;
	for (i = 0; i < txq->nb_tx_desc; i++) {
		txq->sw_ring[i].mbuf = NULL;
		txq->sw_ring[i].last_id = i;
		txq->sw_ring[prev].next_id = i;
		prev = i;
	}

	txq->tx_free_thresh = txq_conf->tx_free_thresh ?
			      txq_conf->tx_free_thresh :
			      NTB_DFLT_TX_FREE_THRESH;
	if (txq->tx_free_thresh >= txq->nb_tx_desc - 3) {
		NTB_LOG(ERR, "tx_free_thresh must be less than nb_desc - 3. "
			"(tx_free_thresh=%u qp_id=%u)", txq->tx_free_thresh,
			qp_id);
		return -EINVAL;
	}

	hw->tx_queues[qp_id] = txq;

	return 0;
}


static int
ntb_queue_setup(struct rte_rawdev *dev,
		uint16_t queue_id,
		rte_rawdev_obj_t queue_conf,
		size_t conf_size)
{
	struct ntb_hw *hw = dev->dev_private;
	int ret;

	if (queue_id >= hw->queue_pairs)
		return -EINVAL;

	ret = ntb_txq_setup(dev, queue_id, queue_conf, conf_size);
	if (ret < 0)
		return ret;

	ret = ntb_rxq_setup(dev, queue_id, queue_conf, conf_size);

	return ret;
}

static int
ntb_queue_release(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct ntb_hw *hw = dev->dev_private;

	if (queue_id >= hw->queue_pairs)
		return -EINVAL;

	ntb_txq_release(hw->tx_queues[queue_id]);
	hw->tx_queues[queue_id] = NULL;
	ntb_rxq_release(hw->rx_queues[queue_id]);
	hw->rx_queues[queue_id] = NULL;

	return 0;
}

static uint16_t
ntb_queue_count(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	return hw->queue_pairs;
}

static int
ntb_queue_init(struct rte_rawdev *dev, uint16_t qp_id)
{
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_rx_queue *rxq = hw->rx_queues[qp_id];
	struct ntb_tx_queue *txq = hw->tx_queues[qp_id];
	volatile struct ntb_header *local_hdr;
	struct ntb_header *remote_hdr;
	uint16_t q_size = hw->queue_size;
	uint32_t hdr_offset;
	void *bar_addr;
	uint16_t i;

	if (hw->ntb_ops->get_peer_mw_addr == NULL) {
		NTB_LOG(ERR, "Getting peer mw addr is not supported.");
		return -EINVAL;
	}

	/* Put queue info into the start of shared memory. */
	hdr_offset = hw->hdr_size_per_queue * qp_id;
	local_hdr = (volatile struct ntb_header *)
		    ((size_t)hw->mz[0]->addr + hdr_offset);
	bar_addr = (*hw->ntb_ops->get_peer_mw_addr)(dev, 0);
	if (bar_addr == NULL)
		return -EINVAL;
	remote_hdr = (struct ntb_header *)
		     ((size_t)bar_addr + hdr_offset);

	/* rxq init. */
	rxq->rx_desc_ring = (struct ntb_desc *)
			    (&remote_hdr->desc_ring);
	rxq->rx_used_ring = (volatile struct ntb_used *)
			    (&local_hdr->desc_ring[q_size]);
	rxq->avail_cnt = &remote_hdr->avail_cnt;
	rxq->used_cnt = &local_hdr->used_cnt;

	for (i = 0; i < rxq->nb_rx_desc - 1; i++) {
		struct rte_mbuf *mbuf = rte_mbuf_raw_alloc(rxq->mpool);
		if (unlikely(!mbuf)) {
			NTB_LOG(ERR, "Failed to allocate mbuf for RX");
			return -ENOMEM;
		}
		mbuf->port = dev->dev_id;

		rxq->sw_ring[i].mbuf = mbuf;

		rxq->rx_desc_ring[i].addr = rte_pktmbuf_mtod(mbuf, size_t);
		rxq->rx_desc_ring[i].len = mbuf->buf_len - RTE_PKTMBUF_HEADROOM;
	}
	rte_wmb();
	*rxq->avail_cnt = rxq->nb_rx_desc - 1;
	rxq->last_avail = rxq->nb_rx_desc - 1;
	rxq->last_used = 0;

	/* txq init */
	txq->tx_desc_ring = (volatile struct ntb_desc *)
			    (&local_hdr->desc_ring);
	txq->tx_used_ring = (struct ntb_used *)
			    (&remote_hdr->desc_ring[q_size]);
	txq->avail_cnt = &local_hdr->avail_cnt;
	txq->used_cnt = &remote_hdr->used_cnt;

	rte_wmb();
	*txq->used_cnt = 0;
	txq->last_used = 0;
	txq->last_avail = 0;
	txq->nb_tx_free = txq->nb_tx_desc - 1;

	/* Set per queue stats. */
	for (i = 0; i < NTB_XSTATS_NUM; i++) {
		hw->ntb_xstats[i + NTB_XSTATS_NUM * (qp_id + 1)] = 0;
		hw->ntb_xstats_off[i + NTB_XSTATS_NUM * (qp_id + 1)] = 0;
	}

	return 0;
}

static inline void
ntb_enqueue_cleanup(struct ntb_tx_queue *txq)
{
	struct ntb_tx_entry *sw_ring = txq->sw_ring;
	uint16_t tx_free = txq->last_avail;
	uint16_t nb_to_clean, i;

	/* avail_cnt + 1 represents where to rx next in the peer. */
	nb_to_clean = (*txq->avail_cnt - txq->last_avail + 1 +
			txq->nb_tx_desc) & (txq->nb_tx_desc - 1);
	nb_to_clean = RTE_MIN(nb_to_clean, txq->tx_free_thresh);
	for (i = 0; i < nb_to_clean; i++) {
		if (sw_ring[tx_free].mbuf)
			rte_pktmbuf_free_seg(sw_ring[tx_free].mbuf);
		tx_free = (tx_free + 1) & (txq->nb_tx_desc - 1);
	}

	txq->nb_tx_free += nb_to_clean;
	txq->last_avail = tx_free;
}

static int
ntb_enqueue_bufs(struct rte_rawdev *dev,
		 struct rte_rawdev_buf **buffers,
		 unsigned int count,
		 rte_rawdev_obj_t context)
{
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_tx_queue *txq = hw->tx_queues[(size_t)context];
	struct ntb_tx_entry *sw_ring = txq->sw_ring;
	struct rte_mbuf *txm;
	struct ntb_used tx_used[NTB_MAX_DESC_SIZE];
	volatile struct ntb_desc *tx_item;
	uint16_t tx_last, nb_segs, off, last_used, avail_cnt;
	uint16_t nb_mbufs = 0;
	uint16_t nb_tx = 0;
	uint64_t bytes = 0;
	void *buf_addr;
	int i;

	if (unlikely(hw->ntb_ops->ioremap == NULL)) {
		NTB_LOG(ERR, "Ioremap not supported.");
		return nb_tx;
	}

	if (unlikely(dev->started == 0 || hw->peer_dev_up == 0)) {
		NTB_LOG(DEBUG, "Link is not up.");
		return nb_tx;
	}

	if (txq->nb_tx_free < txq->tx_free_thresh)
		ntb_enqueue_cleanup(txq);

	off = NTB_XSTATS_NUM * ((size_t)context + 1);
	last_used = txq->last_used;
	avail_cnt = *txq->avail_cnt;/* Where to alloc next. */
	for (nb_tx = 0; nb_tx < count; nb_tx++) {
		txm = (struct rte_mbuf *)(buffers[nb_tx]->buf_addr);
		if (txm == NULL || txq->nb_tx_free < txm->nb_segs)
			break;

		tx_last = (txq->last_used + txm->nb_segs - 1) &
			  (txq->nb_tx_desc - 1);
		nb_segs = txm->nb_segs;
		for (i = 0; i < nb_segs; i++) {
			/* Not enough ring space for tx. */
			if (txq->last_used == avail_cnt)
				goto end_of_tx;
			sw_ring[txq->last_used].mbuf = txm;
			tx_item = txq->tx_desc_ring + txq->last_used;

			if (!tx_item->len) {
				(hw->ntb_xstats[NTB_TX_ERRS_ID + off])++;
				goto end_of_tx;
			}
			if (txm->data_len > tx_item->len) {
				NTB_LOG(ERR, "Data length exceeds buf length."
					" Only %u data would be transmitted.",
					tx_item->len);
				txm->data_len = tx_item->len;
			}

			/* translate remote virtual addr to bar virtual addr */
			buf_addr = (*hw->ntb_ops->ioremap)(dev, tx_item->addr);
			if (buf_addr == NULL) {
				(hw->ntb_xstats[NTB_TX_ERRS_ID + off])++;
				NTB_LOG(ERR, "Null remap addr.");
				goto end_of_tx;
			}
			rte_memcpy(buf_addr, rte_pktmbuf_mtod(txm, void *),
				   txm->data_len);

			tx_used[nb_mbufs].len = txm->data_len;
			tx_used[nb_mbufs++].flags = (txq->last_used ==
						    tx_last) ?
						    NTB_FLAG_EOP : 0;

			/* update stats */
			bytes += txm->data_len;

			txm = txm->next;

			sw_ring[txq->last_used].next_id = (txq->last_used + 1) &
						  (txq->nb_tx_desc - 1);
			sw_ring[txq->last_used].last_id = tx_last;
			txq->last_used = (txq->last_used + 1) &
					 (txq->nb_tx_desc - 1);
		}
		txq->nb_tx_free -= nb_segs;
	}

end_of_tx:
	if (nb_tx) {
		uint16_t nb1, nb2;
		if (nb_mbufs > txq->nb_tx_desc - last_used) {
			nb1 = txq->nb_tx_desc - last_used;
			nb2 = nb_mbufs - txq->nb_tx_desc + last_used;
		} else {
			nb1 = nb_mbufs;
			nb2 = 0;
		}
		rte_memcpy(txq->tx_used_ring + last_used, tx_used,
			   sizeof(struct ntb_used) * nb1);
		rte_memcpy(txq->tx_used_ring, tx_used + nb1,
			   sizeof(struct ntb_used) * nb2);
		rte_wmb();
		*txq->used_cnt = txq->last_used;

		/* update queue stats */
		hw->ntb_xstats[NTB_TX_BYTES_ID + off] += bytes;
		hw->ntb_xstats[NTB_TX_PKTS_ID + off] += nb_tx;
	}

	return nb_tx;
}

static int
ntb_dequeue_bufs(struct rte_rawdev *dev,
		 struct rte_rawdev_buf **buffers,
		 unsigned int count,
		 rte_rawdev_obj_t context)
{
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_rx_queue *rxq = hw->rx_queues[(size_t)context];
	struct ntb_rx_entry *sw_ring = rxq->sw_ring;
	struct ntb_desc rx_desc[NTB_MAX_DESC_SIZE];
	struct rte_mbuf *first, *rxm_t;
	struct rte_mbuf *prev = NULL;
	volatile struct ntb_used *rx_item;
	uint16_t nb_mbufs = 0;
	uint16_t nb_rx = 0;
	uint64_t bytes = 0;
	uint16_t off, last_avail, used_cnt, used_nb;
	int i;

	if (unlikely(dev->started == 0 || hw->peer_dev_up == 0)) {
		NTB_LOG(DEBUG, "Link is not up");
		return nb_rx;
	}

	used_cnt = *rxq->used_cnt;

	if (rxq->last_used == used_cnt)
		return nb_rx;

	last_avail = rxq->last_avail;
	used_nb = (used_cnt - rxq->last_used) & (rxq->nb_rx_desc - 1);
	count = RTE_MIN(count, used_nb);
	for (nb_rx = 0; nb_rx < count; nb_rx++) {
		i = 0;
		while (true) {
			rx_item = rxq->rx_used_ring + rxq->last_used;
			rxm_t = sw_ring[rxq->last_used].mbuf;
			rxm_t->data_len = rx_item->len;
			rxm_t->data_off = RTE_PKTMBUF_HEADROOM;
			rxm_t->port = rxq->port_id;

			if (!i) {
				rxm_t->nb_segs = 1;
				first = rxm_t;
				first->pkt_len = 0;
				buffers[nb_rx]->buf_addr = rxm_t;
			} else {
				prev->next = rxm_t;
				first->nb_segs++;
			}

			prev = rxm_t;
			first->pkt_len += prev->data_len;
			rxq->last_used = (rxq->last_used + 1) &
					 (rxq->nb_rx_desc - 1);

			/* alloc new mbuf */
			rxm_t = rte_mbuf_raw_alloc(rxq->mpool);
			if (unlikely(rxm_t == NULL)) {
				NTB_LOG(ERR, "recv alloc mbuf failed.");
				goto end_of_rx;
			}
			rxm_t->port = rxq->port_id;
			sw_ring[rxq->last_avail].mbuf = rxm_t;
			i++;

			/* fill new desc */
			rx_desc[nb_mbufs].addr =
					rte_pktmbuf_mtod(rxm_t, size_t);
			rx_desc[nb_mbufs++].len = rxm_t->buf_len -
						  RTE_PKTMBUF_HEADROOM;
			rxq->last_avail = (rxq->last_avail + 1) &
					  (rxq->nb_rx_desc - 1);

			if (rx_item->flags & NTB_FLAG_EOP)
				break;
		}
		/* update stats */
		bytes += first->pkt_len;
	}

end_of_rx:
	if (nb_rx) {
		uint16_t nb1, nb2;
		if (nb_mbufs > rxq->nb_rx_desc - last_avail) {
			nb1 = rxq->nb_rx_desc - last_avail;
			nb2 = nb_mbufs - rxq->nb_rx_desc + last_avail;
		} else {
			nb1 = nb_mbufs;
			nb2 = 0;
		}
		rte_memcpy(rxq->rx_desc_ring + last_avail, rx_desc,
			   sizeof(struct ntb_desc) * nb1);
		rte_memcpy(rxq->rx_desc_ring, rx_desc + nb1,
			   sizeof(struct ntb_desc) * nb2);
		rte_wmb();
		*rxq->avail_cnt = rxq->last_avail;

		/* update queue stats */
		off = NTB_XSTATS_NUM * ((size_t)context + 1);
		hw->ntb_xstats[NTB_RX_BYTES_ID + off] += bytes;
		hw->ntb_xstats[NTB_RX_PKTS_ID + off] += nb_rx;
		hw->ntb_xstats[NTB_RX_MISS_ID + off] += (count - nb_rx);
	}

	return nb_rx;
}

static int
ntb_dev_info_get(struct rte_rawdev *dev, rte_rawdev_obj_t dev_info,
		size_t dev_info_size)
{
	struct ntb_hw *hw = dev->dev_private;
	struct ntb_dev_info *info = dev_info;

	if (dev_info_size != sizeof(*info)) {
		NTB_LOG(ERR, "Invalid size parameter to %s", __func__);
		return -EINVAL;
	}

	info->mw_cnt = hw->mw_cnt;
	info->mw_size = hw->mw_size;

	/**
	 * Intel hardware requires that mapped memory base address should be
	 * aligned with EMBARSZ and needs continuous memzone.
	 */
	info->mw_size_align = (uint8_t)(hw->pci_dev->id.vendor_id ==
					NTB_INTEL_VENDOR_ID);

	if (!hw->queue_size || !hw->queue_pairs) {
		NTB_LOG(ERR, "No queue size and queue num assigned.");
		return -EAGAIN;
	}

	hw->hdr_size_per_queue = RTE_ALIGN(sizeof(struct ntb_header) +
				hw->queue_size * sizeof(struct ntb_desc) +
				hw->queue_size * sizeof(struct ntb_used),
				RTE_CACHE_LINE_SIZE);
	info->ntb_hdr_size = hw->hdr_size_per_queue * hw->queue_pairs;

	return 0;
}

static int
ntb_dev_configure(const struct rte_rawdev *dev, rte_rawdev_obj_t config,
		size_t config_size)
{
	struct ntb_dev_config *conf = config;
	struct ntb_hw *hw = dev->dev_private;
	uint32_t xstats_num;
	int ret;

	if (conf == NULL || config_size != sizeof(*conf))
		return -EINVAL;

	hw->queue_pairs	= conf->num_queues;
	hw->queue_size = conf->queue_size;
	hw->used_mw_num = conf->mz_num;
	hw->mz = conf->mz_list;
	hw->rx_queues = rte_zmalloc("ntb_rx_queues",
			sizeof(struct ntb_rx_queue *) * hw->queue_pairs, 0);
	hw->tx_queues = rte_zmalloc("ntb_tx_queues",
			sizeof(struct ntb_tx_queue *) * hw->queue_pairs, 0);
	/* First total stats, then per queue stats. */
	xstats_num = (hw->queue_pairs + 1) * NTB_XSTATS_NUM;
	hw->ntb_xstats = rte_zmalloc("ntb_xstats", xstats_num *
				     sizeof(uint64_t), 0);
	hw->ntb_xstats_off = rte_zmalloc("ntb_xstats_off", xstats_num *
					 sizeof(uint64_t), 0);

	/* Start handshake with the peer. */
	ret = ntb_handshake_work(dev);
	if (ret < 0) {
		rte_free(hw->rx_queues);
		rte_free(hw->tx_queues);
		hw->rx_queues = NULL;
		hw->tx_queues = NULL;
		return ret;
	}

	return 0;
}

static int
ntb_dev_start(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t peer_base_l, peer_val;
	uint64_t peer_base_h;
	uint32_t i;
	int ret;

	if (!hw->link_status || !hw->peer_dev_up)
		return -EINVAL;

	/* Set total stats. */
	for (i = 0; i < NTB_XSTATS_NUM; i++) {
		hw->ntb_xstats[i] = 0;
		hw->ntb_xstats_off[i] = 0;
	}

	for (i = 0; i < hw->queue_pairs; i++) {
		ret = ntb_queue_init(dev, i);
		if (ret) {
			NTB_LOG(ERR, "Failed to init queue.");
			goto err_q_init;
		}
	}

	hw->peer_mw_base = rte_zmalloc("ntb_peer_mw_base", hw->mw_cnt *
					sizeof(uint64_t), 0);
	if (hw->peer_mw_base == NULL) {
		NTB_LOG(ERR, "Cannot allocate memory for peer mw base.");
		ret = -ENOMEM;
		goto err_q_init;
	}

	if (hw->ntb_ops->spad_read == NULL) {
		ret = -ENOTSUP;
		goto err_up;
	}

	peer_val = (*hw->ntb_ops->spad_read)(dev, SPAD_Q_SZ, 0);
	if (peer_val != hw->queue_size) {
		NTB_LOG(ERR, "Inconsistent queue size! (local: %u peer: %u)",
			hw->queue_size, peer_val);
		ret = -EINVAL;
		goto err_up;
	}

	peer_val = (*hw->ntb_ops->spad_read)(dev, SPAD_NUM_QPS, 0);
	if (peer_val != hw->queue_pairs) {
		NTB_LOG(ERR, "Inconsistent number of queues! (local: %u peer:"
			" %u)", hw->queue_pairs, peer_val);
		ret = -EINVAL;
		goto err_up;
	}

	hw->peer_used_mws = (*hw->ntb_ops->spad_read)(dev, SPAD_USED_MWS, 0);

	for (i = 0; i < hw->peer_used_mws; i++) {
		peer_base_h = (*hw->ntb_ops->spad_read)(dev,
				SPAD_MW0_BA_H + 2 * i, 0);
		peer_base_l = (*hw->ntb_ops->spad_read)(dev,
				SPAD_MW0_BA_L + 2 * i, 0);
		hw->peer_mw_base[i] = (peer_base_h << 32) + peer_base_l;
	}

	dev->started = 1;

	return 0;

err_up:
	rte_free(hw->peer_mw_base);
err_q_init:
	for (i = 0; i < hw->queue_pairs; i++) {
		ntb_rxq_release_mbufs(hw->rx_queues[i]);
		ntb_txq_release_mbufs(hw->tx_queues[i]);
	}

	return ret;
}

static void
ntb_dev_stop(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t time_out;
	int status, i;

	if (!hw->peer_dev_up)
		goto clean;

	ntb_link_cleanup(dev);

	/* Notify the peer that device will be down. */
	if (hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Peer doorbell setting is not supported.");
		return;
	}
	status = (*hw->ntb_ops->peer_db_set)(dev, 1);
	if (status) {
		NTB_LOG(ERR, "Failed to tell peer device is down.");
		return;
	}

	/*
	 * Set time out as 1s in case that the peer is stopped accidently
	 * without any notification.
	 */
	time_out = 1000000;

	/* Wait for cleanup work down before db mask clear. */
	while (hw->peer_dev_up && time_out) {
		time_out -= 10;
		rte_delay_us(10);
	}

clean:
	/* Clear doorbells mask. */
	if (hw->ntb_ops->db_set_mask == NULL) {
		NTB_LOG(ERR, "Doorbell mask setting is not supported.");
		return;
	}
	status = (*hw->ntb_ops->db_set_mask)(dev,
				(((uint64_t)1 << hw->db_cnt) - 1));
	if (status)
		NTB_LOG(ERR, "Failed to clear doorbells.");

	for (i = 0; i < hw->queue_pairs; i++) {
		ntb_rxq_release_mbufs(hw->rx_queues[i]);
		ntb_txq_release_mbufs(hw->tx_queues[i]);
	}

	dev->started = 0;
}

static int
ntb_dev_close(struct rte_rawdev *dev)
{
	struct ntb_hw *hw = dev->dev_private;
	struct rte_intr_handle *intr_handle;
	int i;

	if (dev->started)
		ntb_dev_stop(dev);

	/* free queues */
	for (i = 0; i < hw->queue_pairs; i++)
		ntb_queue_release(dev, i);
	hw->queue_pairs = 0;

	intr_handle = &hw->pci_dev->intr_handle;
	/* Clean datapath event and vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
	/* Disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* Unregister callback func to eal lib */
	rte_intr_callback_unregister(intr_handle,
				     ntb_dev_intr_handler, dev);

	return 0;
}

static int
ntb_dev_reset(struct rte_rawdev *rawdev __rte_unused)
{
	return 0;
}

static int
ntb_attr_set(struct rte_rawdev *dev, const char *attr_name,
	     uint64_t attr_value)
{
	struct ntb_hw *hw;
	int index;

	if (dev == NULL || attr_name == NULL) {
		NTB_LOG(ERR, "Invalid arguments for setting attributes");
		return -EINVAL;
	}

	hw = dev->dev_private;

	if (!strncmp(attr_name, NTB_SPAD_USER, NTB_SPAD_USER_LEN)) {
		if (hw->ntb_ops->spad_write == NULL)
			return -ENOTSUP;
		index = atoi(&attr_name[NTB_SPAD_USER_LEN]);
		if (index < 0 || index >= NTB_SPAD_USER_MAX_NUM) {
			NTB_LOG(ERR, "Invalid attribute (%s)", attr_name);
			return -EINVAL;
		}
		(*hw->ntb_ops->spad_write)(dev, hw->spad_user_list[index],
					   1, attr_value);
		NTB_LOG(DEBUG, "Set attribute (%s) Value (%" PRIu64 ")",
			attr_name, attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_QUEUE_SZ_NAME, NTB_ATTR_NAME_LEN)) {
		hw->queue_size = attr_value;
		NTB_LOG(DEBUG, "Set attribute (%s) Value (%" PRIu64 ")",
			attr_name, attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_QUEUE_NUM_NAME, NTB_ATTR_NAME_LEN)) {
		hw->queue_pairs = attr_value;
		NTB_LOG(DEBUG, "Set attribute (%s) Value (%" PRIu64 ")",
			attr_name, attr_value);
		return 0;
	}

	/* Attribute not found. */
	NTB_LOG(ERR, "Attribute not found.");
	return -EINVAL;
}

static int
ntb_attr_get(struct rte_rawdev *dev, const char *attr_name,
	     uint64_t *attr_value)
{
	struct ntb_hw *hw;
	int index;

	if (dev == NULL || attr_name == NULL || attr_value == NULL) {
		NTB_LOG(ERR, "Invalid arguments for getting attributes");
		return -EINVAL;
	}

	hw = dev->dev_private;

	if (!strncmp(attr_name, NTB_TOPO_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->topo;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_LINK_STATUS_NAME, NTB_ATTR_NAME_LEN)) {
		/* hw->link_status only indicates hw link status. */
		*attr_value = hw->link_status && hw->peer_dev_up;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPEED_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->link_speed;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_WIDTH_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->link_width;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_MW_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->mw_cnt;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_DB_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->db_cnt;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPAD_CNT_NAME, NTB_ATTR_NAME_LEN)) {
		*attr_value = hw->spad_cnt;
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	if (!strncmp(attr_name, NTB_SPAD_USER, NTB_SPAD_USER_LEN)) {
		if (hw->ntb_ops->spad_read == NULL)
			return -ENOTSUP;
		index = atoi(&attr_name[NTB_SPAD_USER_LEN]);
		if (index < 0 || index >= NTB_SPAD_USER_MAX_NUM) {
			NTB_LOG(ERR, "Attribute (%s) out of range", attr_name);
			return -EINVAL;
		}
		*attr_value = (*hw->ntb_ops->spad_read)(dev,
				hw->spad_user_list[index], 0);
		NTB_LOG(DEBUG, "Attribute (%s) Value (%" PRIu64 ")",
			attr_name, *attr_value);
		return 0;
	}

	/* Attribute not found. */
	NTB_LOG(ERR, "Attribute not found.");
	return -EINVAL;
}

static inline uint64_t
ntb_stats_update(uint64_t offset, uint64_t stat)
{
	if (stat >= offset)
		return (stat - offset);
	else
		return (uint64_t)(((uint64_t)-1) - offset + stat + 1);
}

static int
ntb_xstats_get(const struct rte_rawdev *dev,
	       const unsigned int ids[],
	       uint64_t values[],
	       unsigned int n)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t i, j, off, xstats_num;

	/* Calculate total stats of all queues. */
	for (i = 0; i < NTB_XSTATS_NUM; i++) {
		hw->ntb_xstats[i] = 0;
		for (j = 0; j < hw->queue_pairs; j++) {
			off = NTB_XSTATS_NUM * (j + 1) + i;
			hw->ntb_xstats[i] +=
			ntb_stats_update(hw->ntb_xstats_off[off],
					 hw->ntb_xstats[off]);
		}
	}

	xstats_num = NTB_XSTATS_NUM * (hw->queue_pairs + 1);
	for (i = 0; i < n && ids[i] < xstats_num; i++) {
		if (ids[i] < NTB_XSTATS_NUM)
			values[i] = hw->ntb_xstats[ids[i]];
		else
			values[i] =
			ntb_stats_update(hw->ntb_xstats_off[ids[i]],
					 hw->ntb_xstats[ids[i]]);
	}

	return i;
}

static int
ntb_xstats_get_names(const struct rte_rawdev *dev,
		     struct rte_rawdev_xstats_name *xstats_names,
		     unsigned int size)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t xstats_num, i, j, off;

	xstats_num = NTB_XSTATS_NUM * (hw->queue_pairs + 1);
	if (xstats_names == NULL || size < xstats_num)
		return xstats_num;

	/* Total stats names */
	memcpy(xstats_names, ntb_xstats_names, sizeof(ntb_xstats_names));

	/* Queue stats names */
	for (i = 0; i < hw->queue_pairs; i++) {
		for (j = 0; j < NTB_XSTATS_NUM; j++) {
			off = j + (i + 1) * NTB_XSTATS_NUM;
			snprintf(xstats_names[off].name,
				sizeof(xstats_names[0].name),
				"%s_q%u", ntb_xstats_names[j].name, i);
		}
	}

	return xstats_num;
}

static uint64_t
ntb_xstats_get_by_name(const struct rte_rawdev *dev,
		       const char *name, unsigned int *id)
{
	struct rte_rawdev_xstats_name *xstats_names;
	struct ntb_hw *hw = dev->dev_private;
	uint32_t xstats_num, i, j, off;

	if (name == NULL)
		return -EINVAL;

	xstats_num = NTB_XSTATS_NUM * (hw->queue_pairs + 1);
	xstats_names = rte_zmalloc("ntb_stats_name",
				   sizeof(struct rte_rawdev_xstats_name) *
				   xstats_num, 0);
	ntb_xstats_get_names(dev, xstats_names, xstats_num);

	/* Calculate total stats of all queues. */
	for (i = 0; i < NTB_XSTATS_NUM; i++) {
		for (j = 0; j < hw->queue_pairs; j++) {
			off = NTB_XSTATS_NUM * (j + 1) + i;
			hw->ntb_xstats[i] +=
			ntb_stats_update(hw->ntb_xstats_off[off],
					 hw->ntb_xstats[off]);
		}
	}

	for (i = 0; i < xstats_num; i++) {
		if (!strncmp(name, xstats_names[i].name,
		    RTE_RAW_DEV_XSTATS_NAME_SIZE)) {
			*id = i;
			rte_free(xstats_names);
			if (i < NTB_XSTATS_NUM)
				return hw->ntb_xstats[i];
			else
				return ntb_stats_update(hw->ntb_xstats_off[i],
							hw->ntb_xstats[i]);
		}
	}

	NTB_LOG(ERR, "Cannot find the xstats name.");

	return -EINVAL;
}

static int
ntb_xstats_reset(struct rte_rawdev *dev,
		 const uint32_t ids[],
		 uint32_t nb_ids)
{
	struct ntb_hw *hw = dev->dev_private;
	uint32_t i, j, off, xstats_num;

	xstats_num = NTB_XSTATS_NUM * (hw->queue_pairs + 1);
	for (i = 0; i < nb_ids && ids[i] < xstats_num; i++) {
		if (ids[i] < NTB_XSTATS_NUM) {
			for (j = 0; j < hw->queue_pairs; j++) {
				off = NTB_XSTATS_NUM * (j + 1) + ids[i];
				hw->ntb_xstats_off[off] = hw->ntb_xstats[off];
			}
		} else {
			hw->ntb_xstats_off[ids[i]] = hw->ntb_xstats[ids[i]];
		}
	}

	return i;
}

static const struct rte_rawdev_ops ntb_ops = {
	.dev_info_get         = ntb_dev_info_get,
	.dev_configure        = ntb_dev_configure,
	.dev_start            = ntb_dev_start,
	.dev_stop             = ntb_dev_stop,
	.dev_close            = ntb_dev_close,
	.dev_reset            = ntb_dev_reset,

	.queue_def_conf       = ntb_queue_conf_get,
	.queue_setup          = ntb_queue_setup,
	.queue_release        = ntb_queue_release,
	.queue_count          = ntb_queue_count,

	.enqueue_bufs         = ntb_enqueue_bufs,
	.dequeue_bufs         = ntb_dequeue_bufs,

	.attr_get             = ntb_attr_get,
	.attr_set             = ntb_attr_set,

	.xstats_get           = ntb_xstats_get,
	.xstats_get_names     = ntb_xstats_get_names,
	.xstats_get_by_name   = ntb_xstats_get_by_name,
	.xstats_reset         = ntb_xstats_reset,
};

static int
ntb_init_hw(struct rte_rawdev *dev, struct rte_pci_device *pci_dev)
{
	struct ntb_hw *hw = dev->dev_private;
	struct rte_intr_handle *intr_handle;
	int ret, i;

	hw->pci_dev = pci_dev;
	hw->peer_dev_up = 0;
	hw->link_status = NTB_LINK_DOWN;
	hw->link_speed = NTB_SPEED_NONE;
	hw->link_width = NTB_WIDTH_NONE;

	switch (pci_dev->id.device_id) {
	case NTB_INTEL_DEV_ID_B2B_SKX:
	case NTB_INTEL_DEV_ID_B2B_ICX:
		hw->ntb_ops = &intel_ntb_ops;
		break;
	default:
		NTB_LOG(ERR, "Not supported device.");
		return -EINVAL;
	}

	if (hw->ntb_ops->ntb_dev_init == NULL)
		return -ENOTSUP;
	ret = (*hw->ntb_ops->ntb_dev_init)(dev);
	if (ret) {
		NTB_LOG(ERR, "Unable to init ntb dev.");
		return ret;
	}

	if (hw->ntb_ops->set_link == NULL)
		return -ENOTSUP;
	ret = (*hw->ntb_ops->set_link)(dev, 1);
	if (ret)
		return ret;

	/* Init doorbell. */
	hw->db_valid_mask = RTE_LEN2MASK(hw->db_cnt, uint64_t);

	intr_handle = &pci_dev->intr_handle;
	/* Register callback func to eal lib */
	rte_intr_callback_register(intr_handle,
				   ntb_dev_intr_handler, dev);

	ret = rte_intr_efd_enable(intr_handle, hw->db_cnt);
	if (ret)
		return ret;

	/* To clarify, the interrupt for each doorbell is already mapped
	 * by default for intel gen3. They are mapped to msix vec 1-32,
	 * and hardware intr is mapped to 0. Map all to 0 for uio.
	 */
	if (!rte_intr_cap_multiple(intr_handle)) {
		for (i = 0; i < hw->db_cnt; i++) {
			if (hw->ntb_ops->vector_bind == NULL)
				return -ENOTSUP;
			ret = (*hw->ntb_ops->vector_bind)(dev, i, 0);
			if (ret)
				return ret;
		}
	}

	if (hw->ntb_ops->db_set_mask == NULL ||
	    hw->ntb_ops->peer_db_set == NULL) {
		NTB_LOG(ERR, "Doorbell is not supported.");
		return -ENOTSUP;
	}
	hw->db_mask = 0;
	ret = (*hw->ntb_ops->db_set_mask)(dev, hw->db_mask);
	if (ret) {
		NTB_LOG(ERR, "Unable to enable intr for all dbs.");
		return ret;
	}

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);

	return ret;
}

static int
ntb_create(struct rte_pci_device *pci_dev, int socket_id)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev = NULL;
	int ret;

	if (pci_dev == NULL) {
		NTB_LOG(ERR, "Invalid pci_dev.");
		return -EINVAL;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "NTB:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	NTB_LOG(INFO, "Init %s on NUMA node %d", name, socket_id);

	/* Allocate device structure. */
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(struct ntb_hw),
					 socket_id);
	if (rawdev == NULL) {
		NTB_LOG(ERR, "Unable to allocate rawdev.");
		return -EINVAL;
	}

	rawdev->dev_ops = &ntb_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	ret = ntb_init_hw(rawdev, pci_dev);
	if (ret < 0) {
		NTB_LOG(ERR, "Unable to init ntb hw.");
		goto fail;
	}

	return ret;

fail:
	if (rawdev != NULL)
		rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
ntb_destroy(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	int ret;

	if (pci_dev == NULL) {
		NTB_LOG(ERR, "Invalid pci_dev.");
		ret = -EINVAL;
		return ret;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "NTB:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	NTB_LOG(INFO, "Closing %s on NUMA node %d", name, rte_socket_id());

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		NTB_LOG(ERR, "Invalid device name (%s)", name);
		ret = -EINVAL;
		return ret;
	}

	ret = rte_rawdev_pmd_release(rawdev);
	if (ret)
		NTB_LOG(ERR, "Failed to destroy ntb rawdev.");

	return ret;
}

static int
ntb_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	return ntb_create(pci_dev, rte_socket_id());
}

static int
ntb_remove(struct rte_pci_device *pci_dev)
{
	return ntb_destroy(pci_dev);
}


static struct rte_pci_driver rte_ntb_pmd = {
	.id_table = pci_id_ntb_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_WC_ACTIVATE,
	.probe = ntb_probe,
	.remove = ntb_remove,
};

RTE_PMD_REGISTER_PCI(raw_ntb, rte_ntb_pmd);
RTE_PMD_REGISTER_PCI_TABLE(raw_ntb, pci_id_ntb_map);
RTE_PMD_REGISTER_KMOD_DEP(raw_ntb, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_LOG_REGISTER(ntb_logtype, pmd.raw.ntb, INFO);
