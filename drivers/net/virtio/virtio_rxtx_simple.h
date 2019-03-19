/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2016 Intel Corporation
 */

#ifndef _VIRTIO_RXTX_SIMPLE_H_
#define _VIRTIO_RXTX_SIMPLE_H_

#include <stdint.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"

#define RTE_VIRTIO_VPMD_RX_BURST 32
#define RTE_VIRTIO_VPMD_RX_REARM_THRESH RTE_VIRTIO_VPMD_RX_BURST

static inline void
virtio_rxq_rearm_vec(struct virtnet_rx *rxvq)
{
	int i;
	uint16_t desc_idx;
	struct rte_mbuf **sw_ring;
	struct vring_desc *start_dp;
	int ret;
	struct virtqueue *vq = rxvq->vq;

	desc_idx = vq->vq_avail_idx & (vq->vq_nentries - 1);
	sw_ring = &vq->sw_ring[desc_idx];
	start_dp = &vq->vq_split.ring.desc[desc_idx];

	ret = rte_mempool_get_bulk(rxvq->mpool, (void **)sw_ring,
		RTE_VIRTIO_VPMD_RX_REARM_THRESH);
	if (unlikely(ret)) {
		rte_eth_devices[rxvq->port_id].data->rx_mbuf_alloc_failed +=
			RTE_VIRTIO_VPMD_RX_REARM_THRESH;
		return;
	}

	for (i = 0; i < RTE_VIRTIO_VPMD_RX_REARM_THRESH; i++) {
		uintptr_t p;

		p = (uintptr_t)&sw_ring[i]->rearm_data;
		*(uint64_t *)p = rxvq->mbuf_initializer;

		start_dp[i].addr =
			VIRTIO_MBUF_ADDR(sw_ring[i], vq) +
			RTE_PKTMBUF_HEADROOM - vq->hw->vtnet_hdr_size;
		start_dp[i].len = sw_ring[i]->buf_len -
			RTE_PKTMBUF_HEADROOM + vq->hw->vtnet_hdr_size;
	}

	vq->vq_avail_idx += RTE_VIRTIO_VPMD_RX_REARM_THRESH;
	vq->vq_free_cnt -= RTE_VIRTIO_VPMD_RX_REARM_THRESH;
	vq_update_avail_idx(vq);
}

#endif /* _VIRTIO_RXTX_SIMPLE_H_ */
