/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium networks Ltd. 2016
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
 *     * Neither the name of Cavium networks nor the names of its
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

#include <rte_byteorder.h>
#include <rte_branch_prediction.h>
#include <rte_cycles.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_errno.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include <rte_vect.h>

#include "virtio_rxtx_simple.h"

#define RTE_VIRTIO_VPMD_RX_BURST 32
#define RTE_VIRTIO_DESC_PER_LOOP 8
#define RTE_VIRTIO_VPMD_RX_REARM_THRESH RTE_VIRTIO_VPMD_RX_BURST

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
	struct virtio_hw *hw = vq->hw;
	uint16_t nb_used;
	uint16_t desc_idx;
	struct vring_used_elem *rused;
	struct rte_mbuf **sw_ring;
	struct rte_mbuf **sw_ring_end;
	uint16_t nb_pkts_received = 0;

	uint8x16_t shuf_msk1 = {
		0xFF, 0xFF, 0xFF, 0xFF, /* packet type */
		4, 5, 0xFF, 0xFF,       /* pkt len */
		4, 5,                   /* dat len */
		0xFF, 0xFF,             /* vlan tci */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	uint8x16_t shuf_msk2 = {
		0xFF, 0xFF, 0xFF, 0xFF, /* packet type */
		12, 13, 0xFF, 0xFF,     /* pkt len */
		12, 13,                 /* dat len */
		0xFF, 0xFF,             /* vlan tci */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	/* Subtract the header length.
	 *  In which case do we need the header length in used->len ?
	 */
	uint16x8_t len_adjust = {
		0, 0,
		(uint16_t)vq->hw->vtnet_hdr_size, 0,
		(uint16_t)vq->hw->vtnet_hdr_size,
		0,
		0, 0
	};

	if (unlikely(hw->started == 0))
		return nb_pkts_received;

	if (unlikely(nb_pkts < RTE_VIRTIO_DESC_PER_LOOP))
		return 0;

	nb_used = VIRTQUEUE_NUSED(vq);

	rte_rmb();

	if (unlikely(nb_used == 0))
		return 0;

	nb_pkts = RTE_ALIGN_FLOOR(nb_pkts, RTE_VIRTIO_DESC_PER_LOOP);
	nb_used = RTE_MIN(nb_used, nb_pkts);

	desc_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
	rused = &vq->vq_ring.used->ring[desc_idx];
	sw_ring  = &vq->sw_ring[desc_idx];
	sw_ring_end = &vq->sw_ring[vq->vq_nentries];

	rte_prefetch_non_temporal(rused);

	if (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
		virtio_rxq_rearm_vec(rxvq);
		if (unlikely(virtqueue_kick_prepare(vq)))
			virtqueue_notify(vq);
	}

	for (nb_pkts_received = 0;
		nb_pkts_received < nb_used;) {
		uint64x2_t desc[RTE_VIRTIO_DESC_PER_LOOP / 2];
		uint64x2_t mbp[RTE_VIRTIO_DESC_PER_LOOP / 2];
		uint64x2_t pkt_mb[RTE_VIRTIO_DESC_PER_LOOP];

		mbp[0] = vld1q_u64((uint64_t *)(sw_ring + 0));
		desc[0] = vld1q_u64((uint64_t *)(rused + 0));
		vst1q_u64((uint64_t *)&rx_pkts[0], mbp[0]);

		mbp[1] = vld1q_u64((uint64_t *)(sw_ring + 2));
		desc[1] = vld1q_u64((uint64_t *)(rused + 2));
		vst1q_u64((uint64_t *)&rx_pkts[2], mbp[1]);

		mbp[2] = vld1q_u64((uint64_t *)(sw_ring + 4));
		desc[2] = vld1q_u64((uint64_t *)(rused + 4));
		vst1q_u64((uint64_t *)&rx_pkts[4], mbp[2]);

		mbp[3] = vld1q_u64((uint64_t *)(sw_ring + 6));
		desc[3] = vld1q_u64((uint64_t *)(rused + 6));
		vst1q_u64((uint64_t *)&rx_pkts[6], mbp[3]);

		pkt_mb[1] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[0]), shuf_msk2));
		pkt_mb[0] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[0]), shuf_msk1));
		pkt_mb[1] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[1]), len_adjust));
		pkt_mb[0] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[0]), len_adjust));
		vst1q_u64((void *)&rx_pkts[1]->rx_descriptor_fields1,
			pkt_mb[1]);
		vst1q_u64((void *)&rx_pkts[0]->rx_descriptor_fields1,
			pkt_mb[0]);

		pkt_mb[3] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[1]), shuf_msk2));
		pkt_mb[2] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[1]), shuf_msk1));
		pkt_mb[3] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[3]), len_adjust));
		pkt_mb[2] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[2]), len_adjust));
		vst1q_u64((void *)&rx_pkts[3]->rx_descriptor_fields1,
			pkt_mb[3]);
		vst1q_u64((void *)&rx_pkts[2]->rx_descriptor_fields1,
			pkt_mb[2]);

		pkt_mb[5] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[2]), shuf_msk2));
		pkt_mb[4] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[2]), shuf_msk1));
		pkt_mb[5] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[5]), len_adjust));
		pkt_mb[4] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[4]), len_adjust));
		vst1q_u64((void *)&rx_pkts[5]->rx_descriptor_fields1,
			pkt_mb[5]);
		vst1q_u64((void *)&rx_pkts[4]->rx_descriptor_fields1,
			pkt_mb[4]);

		pkt_mb[7] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[3]), shuf_msk2));
		pkt_mb[6] = vreinterpretq_u64_u8(vqtbl1q_u8(
				vreinterpretq_u8_u64(desc[3]), shuf_msk1));
		pkt_mb[7] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[7]), len_adjust));
		pkt_mb[6] = vreinterpretq_u64_u16(vsubq_u16(
				vreinterpretq_u16_u64(pkt_mb[6]), len_adjust));
		vst1q_u64((void *)&rx_pkts[7]->rx_descriptor_fields1,
			pkt_mb[7]);
		vst1q_u64((void *)&rx_pkts[6]->rx_descriptor_fields1,
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
