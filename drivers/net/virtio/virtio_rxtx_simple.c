/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_byteorder.h>

#include "virtio_rxtx_simple.h"

#ifndef __INTEL_COMPILER
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif

int __attribute__((cold))
virtqueue_enqueue_recv_refill_simple(struct virtqueue *vq,
	struct rte_mbuf *cookie)
{
	struct vq_desc_extra *dxp;
	struct vring_desc *start_dp;
	uint16_t desc_idx;

	desc_idx = vq->vq_avail_idx & (vq->vq_nentries - 1);
	dxp = &vq->vq_descx[desc_idx];
	dxp->cookie = (void *)cookie;
	vq->sw_ring[desc_idx] = cookie;

	start_dp = vq->vq_ring.desc;
	start_dp[desc_idx].addr =
		VIRTIO_MBUF_ADDR(cookie, vq) +
		RTE_PKTMBUF_HEADROOM - vq->hw->vtnet_hdr_size;
	start_dp[desc_idx].len = cookie->buf_len -
		RTE_PKTMBUF_HEADROOM + vq->hw->vtnet_hdr_size;

	vq->vq_free_cnt--;
	vq->vq_avail_idx++;

	return 0;
}

uint16_t
virtio_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = txvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint16_t nb_used;
	uint16_t desc_idx;
	struct vring_desc *start_dp;
	uint16_t nb_tail, nb_commit;
	int i;
	uint16_t desc_idx_max = (vq->vq_nentries >> 1) - 1;
	uint16_t nb_tx = 0;

	if (unlikely(hw->started == 0))
		return nb_tx;

	nb_used = VIRTQUEUE_NUSED(vq);
	rte_compiler_barrier();

	if (nb_used >= VIRTIO_TX_FREE_THRESH)
		virtio_xmit_cleanup(vq);

	nb_commit = nb_pkts = RTE_MIN((vq->vq_free_cnt >> 1), nb_pkts);
	desc_idx = (uint16_t)(vq->vq_avail_idx & desc_idx_max);
	start_dp = vq->vq_ring.desc;
	nb_tail = (uint16_t) (desc_idx_max + 1 - desc_idx);

	if (nb_commit >= nb_tail) {
		for (i = 0; i < nb_tail; i++)
			vq->vq_descx[desc_idx + i].cookie = tx_pkts[i];
		for (i = 0; i < nb_tail; i++) {
			start_dp[desc_idx].addr =
				VIRTIO_MBUF_DATA_DMA_ADDR(*tx_pkts, vq);
			start_dp[desc_idx].len = (*tx_pkts)->pkt_len;
			tx_pkts++;
			desc_idx++;
		}
		nb_commit -= nb_tail;
		desc_idx = 0;
	}
	for (i = 0; i < nb_commit; i++)
		vq->vq_descx[desc_idx + i].cookie = tx_pkts[i];
	for (i = 0; i < nb_commit; i++) {
		start_dp[desc_idx].addr =
			VIRTIO_MBUF_DATA_DMA_ADDR(*tx_pkts, vq);
		start_dp[desc_idx].len = (*tx_pkts)->pkt_len;
		tx_pkts++;
		desc_idx++;
	}

	rte_compiler_barrier();

	vq->vq_free_cnt -= (uint16_t)(nb_pkts << 1);
	vq->vq_avail_idx += nb_pkts;
	vq->vq_ring.avail->idx = vq->vq_avail_idx;
	txvq->stats.packets += nb_pkts;

	if (likely(nb_pkts)) {
		if (unlikely(virtqueue_kick_prepare(vq)))
			virtqueue_notify(vq);
	}

	return nb_pkts;
}

int __attribute__((cold))
virtio_rxq_vec_setup(struct virtnet_rx *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;

	return 0;
}

/* Stub for linkage when arch specific implementation is not available */
uint16_t __attribute__((weak))
virtio_recv_pkts_vec(void *rx_queue __rte_unused,
		     struct rte_mbuf **rx_pkts __rte_unused,
		     uint16_t nb_pkts __rte_unused)
{
	rte_panic("Wrong weak function linked by linker\n");
	return 0;
}
