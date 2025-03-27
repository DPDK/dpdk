/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <stdint.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "zxdh_queue.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_common.h"
#include "zxdh_msg.h"

#define ZXDH_MBUF_MIN_SIZE       sizeof(struct zxdh_net_hdr_dl)
#define ZXDH_MBUF_SIZE_4K             4096
#define ZXDH_RX_FREE_THRESH           32
#define ZXDH_TX_FREE_THRESH           32

struct rte_mbuf *
zxdh_queue_detach_unused(struct zxdh_virtqueue *vq)
{
	struct rte_mbuf *cookie = NULL;
	int32_t          idx    = 0;

	if (vq == NULL)
		return NULL;

	for (idx = 0; idx < vq->vq_nentries; idx++) {
		cookie = vq->vq_descx[idx].cookie;
		if (cookie != NULL) {
			vq->vq_descx[idx].cookie = NULL;
			return cookie;
		}
	}
	return NULL;
}

static void
zxdh_clear_channel(struct rte_eth_dev *dev, uint16_t lch)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t pch;
	uint32_t var, addr, widx, bidx;

	if (hw->channel_context[lch].valid == 0)
		return;
	/* get coi table offset and index */
	pch  = hw->channel_context[lch].ph_chno;
	widx = pch / 32;
	bidx = pch % 32;
	addr = ZXDH_QUERES_SHARE_BASE + (widx * sizeof(uint32_t));
	var  = zxdh_read_bar_reg(dev, ZXDH_BAR0_INDEX, addr);
	var &= ~(1 << bidx);
	zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);
	hw->channel_context[lch].valid = 0;
	hw->channel_context[lch].ph_chno = 0;
	PMD_DRV_LOG(DEBUG, " phyque %d release end ", pch);
}

static int32_t
zxdh_release_channel(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	u_int16_t rxq_num = hw->rx_qnum;
	u_int16_t txq_num = hw->tx_qnum;
	uint16_t lch, i;
	int32_t ret = 0;

	if (hw->queue_set_flag == 1) {
		for (i = 0; i < rxq_num; i++) {
			lch = i * 2;
			PMD_DRV_LOG(DEBUG, "free success!");
			if (hw->channel_context[lch].valid == 0)
				continue;
			PMD_DRV_LOG(DEBUG, "phyque %d  no need to release backend do it",
						hw->channel_context[lch].ph_chno);
			hw->channel_context[lch].valid = 0;
			hw->channel_context[lch].ph_chno = 0;
		}
		for (i = 0; i < txq_num; i++) {
			lch = i * 2 + 1;
			PMD_DRV_LOG(DEBUG, "free success!");
			if (hw->channel_context[lch].valid == 0)
				continue;
			PMD_DRV_LOG(DEBUG, "phyque %d  no need to release backend do it",
						hw->channel_context[lch].ph_chno);
			hw->channel_context[lch].valid = 0;
			hw->channel_context[lch].ph_chno = 0;
		}
		hw->queue_set_flag = 0;
		return 0;
	}
	ret = zxdh_timedlock(hw, 1000);
	if (ret) {
		PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout");
		return -1;
	}

	for (i = 0 ; i < rxq_num ; i++) {
		lch = i * 2;
		zxdh_clear_channel(dev, lch);
	}
	for (i = 0; i < txq_num ; i++) {
		lch = i * 2 + 1;
		zxdh_clear_channel(dev, lch);
	}
	zxdh_release_lock(hw);

	return 0;
}

int32_t
zxdh_get_queue_type(uint16_t vtpci_queue_idx)
{
	if (vtpci_queue_idx % 2 == 0)
		return ZXDH_VTNET_RQ;
	else
		return ZXDH_VTNET_TQ;
}

int32_t
zxdh_free_queues(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtqueue *vq = NULL;
	u_int16_t rxq_num = hw->rx_qnum;
	u_int16_t txq_num = hw->tx_qnum;
	uint16_t i = 0;

	if (hw->vqs == NULL)
		return 0;

	for (i = 0; i < rxq_num; i++) {
		vq = hw->vqs[i * 2];
		if (vq == NULL)
			continue;

		ZXDH_VTPCI_OPS(hw)->del_queue(hw, vq);
		rte_memzone_free(vq->rxq.mz);
		rte_free(vq);
		hw->vqs[i * 2] = NULL;
		PMD_MSG_LOG(DEBUG, "Release to queue %d success!", i * 2);
	}
	for (i = 0; i < txq_num; i++) {
		vq = hw->vqs[i * 2 + 1];
		if (vq == NULL)
			continue;

		ZXDH_VTPCI_OPS(hw)->del_queue(hw, vq);
		rte_memzone_free(vq->txq.mz);
		rte_memzone_free(vq->txq.zxdh_net_hdr_mz);
		rte_free(vq);
		hw->vqs[i * 2 + 1] = NULL;
		PMD_DRV_LOG(DEBUG, "Release to queue %d success!", i * 2 + 1);
	}

	if (zxdh_release_channel(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to clear coi table");
		return -1;
	}

	rte_free(hw->vqs);
	hw->vqs = NULL;

	return 0;
}

static int
zxdh_check_mempool(struct rte_mempool *mp, uint16_t offset, uint16_t min_length)
{
	uint16_t data_room_size;

	if (mp == NULL)
		return -EINVAL;
	data_room_size = rte_pktmbuf_data_room_size(mp);
	if (data_room_size < offset + min_length) {
		PMD_RX_LOG(ERR,
				   "%s mbuf_data_room_size %u < %u (%u + %u)",
				   mp->name, data_room_size,
				   offset + min_length, offset, min_length);
		return -EINVAL;
	}
	return 0;
}

int32_t
zxdh_dev_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			uint32_t socket_id __rte_unused,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t vtpci_logic_qidx = 2 * queue_idx + ZXDH_RQ_QUEUE_IDX;
	struct zxdh_virtqueue *vq = hw->vqs[vtpci_logic_qidx];
	int32_t ret = 0;

	if (rx_conf->rx_deferred_start) {
		PMD_RX_LOG(ERR, "Rx deferred start is not supported");
		return -EINVAL;
	}
	uint16_t rx_free_thresh = rx_conf->rx_free_thresh;

	if (rx_free_thresh == 0)
		rx_free_thresh = RTE_MIN(vq->vq_nentries / 4, ZXDH_RX_FREE_THRESH);

	/* rx_free_thresh must be multiples of four. */
	if (rx_free_thresh & 0x3) {
		PMD_RX_LOG(ERR, "(rx_free_thresh=%u port=%u queue=%u)",
			rx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}
	/* rx_free_thresh must be less than the number of RX entries */
	if (rx_free_thresh >= vq->vq_nentries) {
		PMD_RX_LOG(ERR, "RX entries (%u). (rx_free_thresh=%u port=%u queue=%u)",
			vq->vq_nentries, rx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}
	vq->vq_free_thresh = rx_free_thresh;
	nb_desc = ZXDH_QUEUE_DEPTH;

	vq->vq_free_cnt = RTE_MIN(vq->vq_free_cnt, nb_desc);
	struct zxdh_virtnet_rx *rxvq = &vq->rxq;

	rxvq->queue_id = vtpci_logic_qidx;

	int mbuf_min_size  = ZXDH_MBUF_MIN_SIZE;

	if (rx_conf->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		mbuf_min_size = ZXDH_MBUF_SIZE_4K;

	ret = zxdh_check_mempool(mp, RTE_PKTMBUF_HEADROOM, mbuf_min_size);
	if (ret != 0) {
		PMD_RX_LOG(ERR,
			"rxq setup but mpool size too small(<%d) failed", mbuf_min_size);
		return -EINVAL;
	}
	rxvq->mpool = mp;
	if (queue_idx < dev->data->nb_rx_queues)
		dev->data->rx_queues[queue_idx] = rxvq;

	return 0;
}

int32_t
zxdh_dev_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			uint32_t socket_id __rte_unused,
			const struct rte_eth_txconf *tx_conf)
{
	uint16_t vtpci_logic_qidx = 2 * queue_idx + ZXDH_TQ_QUEUE_IDX;
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtqueue *vq = hw->vqs[vtpci_logic_qidx];
	struct zxdh_virtnet_tx *txvq = NULL;
	uint16_t tx_free_thresh = 0;

	if (tx_conf->tx_deferred_start) {
		PMD_TX_LOG(ERR, "Tx deferred start is not supported");
		return -EINVAL;
	}

	nb_desc = ZXDH_QUEUE_DEPTH;

	vq->vq_free_cnt = RTE_MIN(vq->vq_free_cnt, nb_desc);

	txvq = &vq->txq;
	txvq->queue_id = vtpci_logic_qidx;

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh = RTE_MIN(vq->vq_nentries / 4, ZXDH_TX_FREE_THRESH);

	/* tx_free_thresh must be less than the number of TX entries minus 3 */
	if (tx_free_thresh >= (vq->vq_nentries - 3)) {
		PMD_TX_LOG(ERR, "TX entries - 3 (%u). (tx_free_thresh=%u port=%u queue=%u)",
				vq->vq_nentries - 3, tx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	vq->vq_free_thresh = tx_free_thresh;

	if (queue_idx < dev->data->nb_tx_queues)
		dev->data->tx_queues[queue_idx] = txvq;

	return 0;
}

int32_t
zxdh_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct zxdh_virtqueue *vq = rxvq->vq;

	zxdh_queue_enable_intr(vq);
	return 0;
}

int32_t
zxdh_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct zxdh_virtqueue  *vq	= rxvq->vq;

	zxdh_queue_disable_intr(vq);
	return 0;
}

int32_t zxdh_enqueue_recv_refill_packed(struct zxdh_virtqueue *vq,
			struct rte_mbuf **cookie, uint16_t num)
{
	struct zxdh_vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	struct zxdh_vq_desc_extra *dxp;
	uint16_t flags = vq->vq_packed.cached_flags;
	int32_t i;
	uint16_t idx;

	for (i = 0; i < num; i++) {
		idx = vq->vq_avail_idx;
		dxp = &vq->vq_descx[idx];
		dxp->cookie = (void *)cookie[i];
		dxp->ndescs = 1;
		/* rx pkt fill in data_off */
		start_dp[idx].addr = rte_mbuf_iova_get(cookie[i]) + RTE_PKTMBUF_HEADROOM;
		start_dp[idx].len = cookie[i]->buf_len - RTE_PKTMBUF_HEADROOM;

		zxdh_queue_store_flags_packed(&start_dp[idx], flags);
		if (++vq->vq_avail_idx >= vq->vq_nentries) {
			vq->vq_avail_idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^= ZXDH_VRING_PACKED_DESC_F_AVAIL_USED;
			flags = vq->vq_packed.cached_flags;
		}
	}
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
	return 0;
}

int32_t zxdh_dev_rx_queue_setup_finish(struct rte_eth_dev *dev, uint16_t logic_qidx)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtqueue *vq = hw->vqs[logic_qidx];
	struct zxdh_virtnet_rx *rxvq = &vq->rxq;
	uint16_t desc_idx;
	int32_t error = 0;

	/* Allocate blank mbufs for the each rx descriptor */
	memset(&rxvq->fake_mbuf, 0, sizeof(rxvq->fake_mbuf));
	for (desc_idx = 0; desc_idx < ZXDH_MBUF_BURST_SZ; desc_idx++)
		vq->sw_ring[vq->vq_nentries + desc_idx] = &rxvq->fake_mbuf;

	while (!zxdh_queue_full(vq)) {
		struct rte_mbuf *new_pkts[ZXDH_MBUF_BURST_SZ];
		uint16_t free_cnt = RTE_MIN(ZXDH_MBUF_BURST_SZ, vq->vq_free_cnt);

		if (likely(rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt) == 0)) {
			error = zxdh_enqueue_recv_refill_packed(vq, new_pkts, free_cnt);
			if (unlikely(error)) {
				int32_t i;
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
		} else {
			PMD_DRV_LOG(ERR, "port %d rxq %d allocated bufs from %s failed",
				hw->port_id, logic_qidx, rxvq->mpool->name);
			break;
		}
	}
	return 0;
}

void zxdh_queue_rxvq_flush(struct zxdh_virtqueue *vq)
{
	struct zxdh_vq_desc_extra *dxp = NULL;
	uint16_t i = 0;
	struct zxdh_vring_packed_desc *descs = vq->vq_packed.ring.desc;
	int32_t cnt = 0;

	i = vq->vq_used_cons_idx;
	while (zxdh_desc_used(&descs[i], vq) && cnt++ < vq->vq_nentries) {
		dxp = &vq->vq_descx[descs[i].id];
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
		vq->vq_free_cnt++;
		vq->vq_used_cons_idx++;
		if (vq->vq_used_cons_idx >= vq->vq_nentries) {
			vq->vq_used_cons_idx -= vq->vq_nentries;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
		i = vq->vq_used_cons_idx;
	}
}
