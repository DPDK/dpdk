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

#include <tmmintrin.h>

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

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"

#define RTE_VIRTIO_VPMD_RX_BURST 32
#define RTE_VIRTIO_DESC_PER_LOOP 8
#define RTE_VIRTIO_VPMD_RX_REARM_THRESH RTE_VIRTIO_VPMD_RX_BURST

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

/* virtio vPMD receive routine, only accept(nb_pkts >= RTE_VIRTIO_DESC_PER_LOOP)
 *
 * This routine is for non-mergeable RX, one desc for each guest buffer.
 * This routine is based on the RX ring layout optimization. Each entry in the
 * avail ring points to the desc with the same index in the desc ring and this
 * will never be changed in the driver.
 *
 * - nb_pkts < RTE_VIRTIO_DESC_PER_LOOP, just return no packet
 */
uint16_t
virtio_recv_pkts_vec(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = rxvq->vq;
	uint16_t nb_used;
	uint16_t desc_idx;
	struct vring_used_elem *rused;
	struct rte_mbuf **sw_ring;
	struct rte_mbuf **sw_ring_end;
	uint16_t nb_pkts_received;
	__m128i shuf_msk1, shuf_msk2, len_adjust;

	shuf_msk1 = _mm_set_epi8(
		0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF,		/* vlan tci */
		5, 4,			/* dat len */
		0xFF, 0xFF, 5, 4,	/* pkt len */
		0xFF, 0xFF, 0xFF, 0xFF	/* packet type */

	);

	shuf_msk2 = _mm_set_epi8(
		0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF,		/* vlan tci */
		13, 12,			/* dat len */
		0xFF, 0xFF, 13, 12,	/* pkt len */
		0xFF, 0xFF, 0xFF, 0xFF	/* packet type */
	);

	/* Subtract the header length.
	*  In which case do we need the header length in used->len ?
	*/
	len_adjust = _mm_set_epi16(
		0, 0,
		0,
		(uint16_t)-vq->hw->vtnet_hdr_size,
		0, (uint16_t)-vq->hw->vtnet_hdr_size,
		0, 0);

	if (unlikely(nb_pkts < RTE_VIRTIO_DESC_PER_LOOP))
		return 0;

	nb_used = VIRTQUEUE_NUSED(vq);

	rte_compiler_barrier();

	if (unlikely(nb_used == 0))
		return 0;

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_VIRTIO_DESC_PER_LOOP);
	nb_used = RTE_MIN(nb_used, nb_pkts);

	desc_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
	rused = &vq->vq_ring.used->ring[desc_idx];
	sw_ring  = &vq->sw_ring[desc_idx];
	sw_ring_end = &vq->sw_ring[vq->vq_nentries];

	_mm_prefetch((const void *)rused, _MM_HINT_T0);

	if (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
		virtio_rxq_rearm_vec(rxvq);
		if (unlikely(virtqueue_kick_prepare(vq)))
			virtqueue_notify(vq);
	}

	for (nb_pkts_received = 0;
		nb_pkts_received < nb_used;) {
		__m128i desc[RTE_VIRTIO_DESC_PER_LOOP / 2];
		__m128i mbp[RTE_VIRTIO_DESC_PER_LOOP / 2];
		__m128i pkt_mb[RTE_VIRTIO_DESC_PER_LOOP];

		mbp[0] = _mm_loadu_si128((__m128i *)(sw_ring + 0));
		desc[0] = _mm_loadu_si128((__m128i *)(rused + 0));
		_mm_storeu_si128((__m128i *)&rx_pkts[0], mbp[0]);

		mbp[1] = _mm_loadu_si128((__m128i *)(sw_ring + 2));
		desc[1] = _mm_loadu_si128((__m128i *)(rused + 2));
		_mm_storeu_si128((__m128i *)&rx_pkts[2], mbp[1]);

		mbp[2] = _mm_loadu_si128((__m128i *)(sw_ring + 4));
		desc[2] = _mm_loadu_si128((__m128i *)(rused + 4));
		_mm_storeu_si128((__m128i *)&rx_pkts[4], mbp[2]);

		mbp[3] = _mm_loadu_si128((__m128i *)(sw_ring + 6));
		desc[3] = _mm_loadu_si128((__m128i *)(rused + 6));
		_mm_storeu_si128((__m128i *)&rx_pkts[6], mbp[3]);

		pkt_mb[1] = _mm_shuffle_epi8(desc[0], shuf_msk2);
		pkt_mb[0] = _mm_shuffle_epi8(desc[0], shuf_msk1);
		pkt_mb[1] = _mm_add_epi16(pkt_mb[1], len_adjust);
		pkt_mb[0] = _mm_add_epi16(pkt_mb[0], len_adjust);
		_mm_storeu_si128((void *)&rx_pkts[1]->rx_descriptor_fields1,
			pkt_mb[1]);
		_mm_storeu_si128((void *)&rx_pkts[0]->rx_descriptor_fields1,
			pkt_mb[0]);

		pkt_mb[3] = _mm_shuffle_epi8(desc[1], shuf_msk2);
		pkt_mb[2] = _mm_shuffle_epi8(desc[1], shuf_msk1);
		pkt_mb[3] = _mm_add_epi16(pkt_mb[3], len_adjust);
		pkt_mb[2] = _mm_add_epi16(pkt_mb[2], len_adjust);
		_mm_storeu_si128((void *)&rx_pkts[3]->rx_descriptor_fields1,
			pkt_mb[3]);
		_mm_storeu_si128((void *)&rx_pkts[2]->rx_descriptor_fields1,
			pkt_mb[2]);

		pkt_mb[5] = _mm_shuffle_epi8(desc[2], shuf_msk2);
		pkt_mb[4] = _mm_shuffle_epi8(desc[2], shuf_msk1);
		pkt_mb[5] = _mm_add_epi16(pkt_mb[5], len_adjust);
		pkt_mb[4] = _mm_add_epi16(pkt_mb[4], len_adjust);
		_mm_storeu_si128((void *)&rx_pkts[5]->rx_descriptor_fields1,
			pkt_mb[5]);
		_mm_storeu_si128((void *)&rx_pkts[4]->rx_descriptor_fields1,
			pkt_mb[4]);

		pkt_mb[7] = _mm_shuffle_epi8(desc[3], shuf_msk2);
		pkt_mb[6] = _mm_shuffle_epi8(desc[3], shuf_msk1);
		pkt_mb[7] = _mm_add_epi16(pkt_mb[7], len_adjust);
		pkt_mb[6] = _mm_add_epi16(pkt_mb[6], len_adjust);
		_mm_storeu_si128((void *)&rx_pkts[7]->rx_descriptor_fields1,
			pkt_mb[7]);
		_mm_storeu_si128((void *)&rx_pkts[6]->rx_descriptor_fields1,
			pkt_mb[6]);

		if (unlikely(nb_used <= RTE_VIRTIO_DESC_PER_LOOP)) {
			if (sw_ring + nb_used <= sw_ring_end)
				nb_pkts_received += nb_used;
			else
				nb_pkts_received += sw_ring_end - sw_ring;
			break;
		} else {
			if (unlikely(sw_ring + RTE_VIRTIO_DESC_PER_LOOP >=
				sw_ring_end)) {
				nb_pkts_received += sw_ring_end - sw_ring;
				break;
			} else {
				nb_pkts_received += RTE_VIRTIO_DESC_PER_LOOP;

				rx_pkts += RTE_VIRTIO_DESC_PER_LOOP;
				sw_ring += RTE_VIRTIO_DESC_PER_LOOP;
				rused   += RTE_VIRTIO_DESC_PER_LOOP;
				nb_used -= RTE_VIRTIO_DESC_PER_LOOP;
			}
		}
	}

	vq->vq_used_cons_idx += nb_pkts_received;
	vq->vq_free_cnt += nb_pkts_received;
	rxvq->stats.packets += nb_pkts_received;
	return nb_pkts_received;
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

uint16_t
virtio_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = txvq->vq;
	uint16_t nb_used;
	uint16_t desc_idx;
	struct vring_desc *start_dp;
	uint16_t nb_tail, nb_commit;
	int i;
	uint16_t desc_idx_max = (vq->vq_nentries >> 1) - 1;

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
