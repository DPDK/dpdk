/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Arm Corporation
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <rte_net.h>
#include <rte_vect.h>

#include "virtio_ethdev.h"
#include "virtio_pci.h"
#include "virtio_rxtx_packed.h"
#include "virtqueue.h"

static inline int
virtqueue_dequeue_batch_packed_vec(struct virtnet_rx *rxvq,
				   struct rte_mbuf **rx_pkts)
{
	struct virtqueue *vq = rxvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint16_t head_size = hw->vtnet_hdr_size;
	uint16_t id = vq->vq_used_cons_idx;
	struct vring_packed_desc *p_desc;
	uint16_t i;

	if (id & PACKED_BATCH_MASK)
		return -1;

	if (unlikely((id + PACKED_BATCH_SIZE) > vq->vq_nentries))
		return -1;

	/* Map packed descriptor to mbuf fields. */
	uint8x16_t shuf_msk1 = {
		0xFF, 0xFF, 0xFF, 0xFF, /* pkt_type set as unknown */
		0, 1,			/* octet 1~0, low 16 bits pkt_len */
		0xFF, 0xFF,		/* skip high 16 bits of pkt_len, zero out */
		0, 1,			/* octet 1~0, 16 bits data_len */
		0xFF, 0xFF,		/* vlan tci set as unknown */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	uint8x16_t shuf_msk2 = {
		0xFF, 0xFF, 0xFF, 0xFF, /* pkt_type set as unknown */
		8, 9,			/* octet 9~8, low 16 bits pkt_len */
		0xFF, 0xFF,		/* skip high 16 bits of pkt_len, zero out */
		8, 9,			/* octet 9~8, 16 bits data_len */
		0xFF, 0xFF,		/* vlan tci set as unknown */
		0xFF, 0xFF, 0xFF, 0xFF
	};

	/* Subtract the header length. */
	uint16x8_t len_adjust = {
		0, 0,		/* ignore pkt_type field */
		head_size,	/* sub head_size on pkt_len */
		0,		/* ignore high 16 bits of pkt_len */
		head_size,	/* sub head_size on data_len */
		0, 0, 0		/* ignore non-length fields */
	};

	uint64x2_t desc[PACKED_BATCH_SIZE / 2];
	uint64x2x2_t mbp[PACKED_BATCH_SIZE / 2];
	uint64x2_t pkt_mb[PACKED_BATCH_SIZE];

	p_desc = &vq->vq_packed.ring.desc[id];
	/* Load high 64 bits of packed descriptor 0,1. */
	desc[0] = vld2q_u64((uint64_t *)(p_desc)).val[1];
	/* Load high 64 bits of packed descriptor 2,3. */
	desc[1] = vld2q_u64((uint64_t *)(p_desc + 2)).val[1];

	/* Only care avail/used bits. */
	uint32x4_t v_mask = vdupq_n_u32(PACKED_FLAGS_MASK);
	/* Extract high 32 bits of packed descriptor (id, flags). */
	uint32x4_t v_desc = vuzp2q_u32(vreinterpretq_u32_u64(desc[0]),
				vreinterpretq_u32_u64(desc[1]));
	uint32x4_t v_flag = vandq_u32(v_desc, v_mask);

	uint32x4_t v_used_flag = vdupq_n_u32(0);
	if (vq->vq_packed.used_wrap_counter)
		v_used_flag = vdupq_n_u32(PACKED_FLAGS_MASK);

	poly128_t desc_stats = vreinterpretq_p128_u32(~vceqq_u32(v_flag, v_used_flag));

	/* Check all descs are used. */
	if (desc_stats)
		return -1;

	/* Load 2 mbuf pointers per time. */
	mbp[0] = vld2q_u64((uint64_t *)&vq->vq_descx[id]);
	vst1q_u64((uint64_t *)&rx_pkts[0], mbp[0].val[0]);

	mbp[1] = vld2q_u64((uint64_t *)&vq->vq_descx[id + 2]);
	vst1q_u64((uint64_t *)&rx_pkts[2], mbp[1].val[0]);

	/**
	 *  Update data length and packet length for descriptor.
	 *  structure of pkt_mb:
	 *  --------------------------------------------------------------------
	 *  |32 bits pkt_type|32 bits pkt_len|16 bits data_len|16 bits vlan_tci|
	 *  --------------------------------------------------------------------
	 */
	pkt_mb[0] = vreinterpretq_u64_u8(vqtbl1q_u8(
			vreinterpretq_u8_u64(desc[0]), shuf_msk1));
	pkt_mb[1] = vreinterpretq_u64_u8(vqtbl1q_u8(
			vreinterpretq_u8_u64(desc[0]), shuf_msk2));
	pkt_mb[2] = vreinterpretq_u64_u8(vqtbl1q_u8(
			vreinterpretq_u8_u64(desc[1]), shuf_msk1));
	pkt_mb[3] = vreinterpretq_u64_u8(vqtbl1q_u8(
			vreinterpretq_u8_u64(desc[1]), shuf_msk2));

	pkt_mb[0] = vreinterpretq_u64_u16(vsubq_u16(
			vreinterpretq_u16_u64(pkt_mb[0]), len_adjust));
	pkt_mb[1] = vreinterpretq_u64_u16(vsubq_u16(
			vreinterpretq_u16_u64(pkt_mb[1]), len_adjust));
	pkt_mb[2] = vreinterpretq_u64_u16(vsubq_u16(
			vreinterpretq_u16_u64(pkt_mb[2]), len_adjust));
	pkt_mb[3] = vreinterpretq_u64_u16(vsubq_u16(
			vreinterpretq_u16_u64(pkt_mb[3]), len_adjust));

	vst1q_u64((void *)&rx_pkts[0]->rx_descriptor_fields1, pkt_mb[0]);
	vst1q_u64((void *)&rx_pkts[1]->rx_descriptor_fields1, pkt_mb[1]);
	vst1q_u64((void *)&rx_pkts[2]->rx_descriptor_fields1, pkt_mb[2]);
	vst1q_u64((void *)&rx_pkts[3]->rx_descriptor_fields1, pkt_mb[3]);

	if (hw->has_rx_offload) {
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			char *addr = (char *)rx_pkts[i]->buf_addr +
				RTE_PKTMBUF_HEADROOM - head_size;
			virtio_vec_rx_offload(rx_pkts[i],
					(struct virtio_net_hdr *)addr);
		}
	}

	virtio_update_batch_stats(&rxvq->stats, rx_pkts[0]->pkt_len,
			rx_pkts[1]->pkt_len, rx_pkts[2]->pkt_len,
			rx_pkts[3]->pkt_len);

	vq->vq_free_cnt += PACKED_BATCH_SIZE;

	vq->vq_used_cons_idx += PACKED_BATCH_SIZE;
	if (vq->vq_used_cons_idx >= vq->vq_nentries) {
		vq->vq_used_cons_idx -= vq->vq_nentries;
		vq->vq_packed.used_wrap_counter ^= 1;
	}

	return 0;
}
