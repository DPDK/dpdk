/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_net.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtio_rxtx_packed.h"
#include "virtqueue.h"

#ifdef CC_AVX512_SUPPORT
#include "virtio_rxtx_packed_avx.h"
#elif defined(RTE_ARCH_ARM)
#include "virtio_rxtx_packed_neon.h"
#endif

uint16_t
virtio_xmit_pkts_packed_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t nb_tx = 0;
	uint16_t remained;

	if (unlikely(hw->started == 0 && tx_pkts != hw->inject_pkts))
		return nb_tx;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);

	if (vq->vq_free_cnt <= vq->vq_nentries - vq->vq_free_thresh)
		virtio_xmit_cleanup_inorder_packed(vq, vq->vq_free_thresh);

	remained = RTE_MIN(nb_pkts, vq->vq_free_cnt);

	while (remained) {
		if (remained >= PACKED_BATCH_SIZE) {
			if (!virtqueue_enqueue_batch_packed_vec(txvq,
						&tx_pkts[nb_tx])) {
				nb_tx += PACKED_BATCH_SIZE;
				remained -= PACKED_BATCH_SIZE;
				continue;
			}
		}
		if (!virtqueue_enqueue_single_packed_vec(txvq,
					tx_pkts[nb_tx])) {
			nb_tx++;
			remained--;
			continue;
		}
		break;
	};

	txvq->stats.packets += nb_tx;

	if (likely(nb_tx)) {
		if (unlikely(virtqueue_kick_prepare_packed(vq))) {
			virtqueue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}

	return nb_tx;
}

uint16_t
virtio_recv_pkts_packed_vec(void *rx_queue,
			    struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t num, nb_rx = 0;
	uint32_t nb_enqueued = 0;
	uint16_t free_cnt = vq->vq_free_thresh;

	if (unlikely(hw->started == 0))
		return nb_rx;

	num = RTE_MIN(VIRTIO_MBUF_BURST_SZ, nb_pkts);
	if (likely(num > PACKED_BATCH_SIZE))
		num = num - ((vq->vq_used_cons_idx + num) % PACKED_BATCH_SIZE);

	while (num) {
		if (num >= PACKED_BATCH_SIZE) {
			if (!virtqueue_dequeue_batch_packed_vec(rxvq,
						&rx_pkts[nb_rx])) {
				nb_rx += PACKED_BATCH_SIZE;
				num -= PACKED_BATCH_SIZE;
				continue;
			}
		}
		if (!virtqueue_dequeue_single_packed_vec(rxvq,
					&rx_pkts[nb_rx])) {
			nb_rx++;
			num--;
			continue;
		}
		break;
	};

	PMD_RX_LOG(DEBUG, "dequeue:%d", num);

	rxvq->stats.packets += nb_rx;

	if (likely(vq->vq_free_cnt >= free_cnt)) {
		struct rte_mbuf *new_pkts[free_cnt];
		if (likely(rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts,
						free_cnt) == 0)) {
			virtio_recv_refill_packed_vec(rxvq, new_pkts,
					free_cnt);
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev =
				&rte_eth_devices[rxvq->port_id];
			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}

	if (likely(nb_enqueued)) {
		if (unlikely(virtqueue_kick_prepare_packed(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}
