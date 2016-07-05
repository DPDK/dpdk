/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
	start_dp = &vq->vq_ring.desc[desc_idx];

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

#define VIRTIO_TX_FREE_THRESH 32
#define VIRTIO_TX_MAX_FREE_BUF_SZ 32
#define VIRTIO_TX_FREE_NR 32
/* TODO: vq->tx_free_cnt could mean num of free slots so we could avoid shift */
static inline void
virtio_xmit_cleanup(struct virtqueue *vq)
{
	uint16_t i, desc_idx;
	uint32_t nb_free = 0;
	struct rte_mbuf *m, *free[VIRTIO_TX_MAX_FREE_BUF_SZ];

	desc_idx = (uint16_t)(vq->vq_used_cons_idx &
		   ((vq->vq_nentries >> 1) - 1));
	m = (struct rte_mbuf *)vq->vq_descx[desc_idx++].cookie;
	m = __rte_pktmbuf_prefree_seg(m);
	if (likely(m != NULL)) {
		free[0] = m;
		nb_free = 1;
		for (i = 1; i < VIRTIO_TX_FREE_NR; i++) {
			m = (struct rte_mbuf *)vq->vq_descx[desc_idx++].cookie;
			m = __rte_pktmbuf_prefree_seg(m);
			if (likely(m != NULL)) {
				if (likely(m->pool == free[0]->pool))
					free[nb_free++] = m;
				else {
					rte_mempool_put_bulk(free[0]->pool,
						(void **)free,
						RTE_MIN(RTE_DIM(free),
							nb_free));
					free[0] = m;
					nb_free = 1;
				}
			}
		}
		rte_mempool_put_bulk(free[0]->pool, (void **)free,
			RTE_MIN(RTE_DIM(free), nb_free));
	} else {
		for (i = 1; i < VIRTIO_TX_FREE_NR; i++) {
			m = (struct rte_mbuf *)vq->vq_descx[desc_idx++].cookie;
			m = __rte_pktmbuf_prefree_seg(m);
			if (m != NULL)
				rte_mempool_put(m->pool, m);
		}
	}

	vq->vq_used_cons_idx += VIRTIO_TX_FREE_NR;
	vq->vq_free_cnt += (VIRTIO_TX_FREE_NR << 1);
}

#endif /* _VIRTIO_RXTX_SIMPLE_H_ */
