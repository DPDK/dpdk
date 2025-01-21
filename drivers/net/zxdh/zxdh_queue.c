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

static int32_t
zxdh_release_channel(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = hw->queue_num;
	uint32_t var  = 0;
	uint32_t addr = 0;
	uint32_t widx = 0;
	uint32_t bidx = 0;
	uint16_t pch  = 0;
	uint16_t lch  = 0;
	int32_t ret = 0;

	ret = zxdh_timedlock(hw, 1000);
	if (ret) {
		PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout");
		return -1;
	}

	for (lch = 0; lch < nr_vq; lch++) {
		if (hw->channel_context[lch].valid == 0) {
			PMD_DRV_LOG(DEBUG, "Logic channel %d does not need to release", lch);
			continue;
		}

		pch  = hw->channel_context[lch].ph_chno;
		widx = pch / 32;
		bidx = pch % 32;

		addr = ZXDH_QUERES_SHARE_BASE + (widx * sizeof(uint32_t));
		var  = zxdh_read_bar_reg(dev, ZXDH_BAR0_INDEX, addr);
		var &= ~(1 << bidx);
		zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);

		hw->channel_context[lch].valid = 0;
		hw->channel_context[lch].ph_chno = 0;
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
	uint16_t nr_vq = hw->queue_num;
	struct zxdh_virtqueue *vq = NULL;
	int32_t queue_type = 0;
	uint16_t i = 0;

	if (hw->vqs == NULL)
		return 0;

	if (zxdh_release_channel(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to clear coi table");
		return -1;
	}

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (vq == NULL)
			continue;

		ZXDH_VTPCI_OPS(hw)->del_queue(hw, vq);
		queue_type = zxdh_get_queue_type(i);
		if (queue_type == ZXDH_VTNET_RQ) {
			rte_free(vq->sw_ring);
			rte_memzone_free(vq->rxq.mz);
		} else if (queue_type == ZXDH_VTNET_TQ) {
			rte_memzone_free(vq->txq.mz);
			rte_memzone_free(vq->txq.zxdh_net_hdr_mz);
		}

		rte_free(vq);
		hw->vqs[i] = NULL;
		PMD_DRV_LOG(DEBUG, "Release to queue %d success!", i);
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
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtnet_rx *rxvq = dev->data->rx_queues[queue_id];
	struct zxdh_virtqueue *vq = rxvq->vq;

	zxdh_queue_enable_intr(vq);
	zxdh_mb(hw->weak_barriers);
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
