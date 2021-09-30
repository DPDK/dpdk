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
#include "virtio.h"
#include "virtio_rxtx_packed.h"
#include "virtqueue.h"

static inline int
virtqueue_enqueue_batch_packed_vec(struct virtnet_tx *txvq,
				   struct rte_mbuf **tx_pkts)
{
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	uint16_t head_size = vq->hw->vtnet_hdr_size;
	uint16_t idx = vq->vq_avail_idx;
	struct virtio_net_hdr *hdr;
	struct vq_desc_extra *dxp;
	struct vring_packed_desc *p_desc;
	uint16_t i;

	if (idx & PACKED_BATCH_MASK)
		return -1;

	if (unlikely((idx + PACKED_BATCH_SIZE) > vq->vq_nentries))
		return -1;

	/* Map four refcnt and nb_segs from mbufs to one NEON register. */
	uint8x16_t ref_seg_msk = {
		2, 3, 4, 5,
		10, 11, 12, 13,
		18, 19, 20, 21,
		26, 27, 28, 29
	};

	/* Map four data_off from mbufs to one NEON register. */
	uint8x8_t data_msk = {
		0, 1,
		8, 9,
		16, 17,
		24, 25
	};

	uint16x8_t net_hdr_msk = {
		0xFFFF, 0xFFFF,
		0, 0, 0, 0
	};

	uint16x4_t pkts[PACKED_BATCH_SIZE];
	uint8x16x2_t mbuf;
	/* Load four mbufs rearm data. */
	RTE_BUILD_BUG_ON(REFCNT_BITS_OFFSET >= 64);
	pkts[0] = vld1_u16((uint16_t *)&tx_pkts[0]->rearm_data);
	pkts[1] = vld1_u16((uint16_t *)&tx_pkts[1]->rearm_data);
	pkts[2] = vld1_u16((uint16_t *)&tx_pkts[2]->rearm_data);
	pkts[3] = vld1_u16((uint16_t *)&tx_pkts[3]->rearm_data);

	mbuf.val[0] = vreinterpretq_u8_u16(vcombine_u16(pkts[0], pkts[1]));
	mbuf.val[1] = vreinterpretq_u8_u16(vcombine_u16(pkts[2], pkts[3]));

	/* refcnt = 1 and nb_segs = 1 */
	uint32x4_t def_ref_seg = vdupq_n_u32(0x10001);
	/* Check refcnt and nb_segs. */
	uint32x4_t ref_seg = vreinterpretq_u32_u8(vqtbl2q_u8(mbuf, ref_seg_msk));
	uint64x2_t cmp1 = vreinterpretq_u64_u32(~vceqq_u32(ref_seg, def_ref_seg));
	if (unlikely(vgetq_lane_u64(cmp1, 0) || vgetq_lane_u64(cmp1, 1)))
		return -1;

	/* Check headroom is enough. */
	uint16x4_t head_rooms = vdup_n_u16(head_size);
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) !=
			 offsetof(struct rte_mbuf, rearm_data));
	uint16x4_t data_offset = vreinterpret_u16_u8(vqtbl2_u8(mbuf, data_msk));
	uint64x1_t cmp2 = vreinterpret_u64_u16(vclt_u16(data_offset, head_rooms));
	if (unlikely(vget_lane_u64(cmp2, 0)))
		return -1;

	virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		dxp = &vq->vq_descx[idx + i];
		dxp->ndescs = 1;
		dxp->cookie = tx_pkts[i];
	}

	virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		tx_pkts[i]->data_off -= head_size;
		tx_pkts[i]->data_len += head_size;
	}

	uint64x2x2_t desc[PACKED_BATCH_SIZE / 2];
	uint64x2_t base_addr0 = {
		VIRTIO_MBUF_ADDR(tx_pkts[0], vq) + tx_pkts[0]->data_off,
		VIRTIO_MBUF_ADDR(tx_pkts[1], vq) + tx_pkts[1]->data_off
	};
	uint64x2_t base_addr1 = {
		VIRTIO_MBUF_ADDR(tx_pkts[2], vq) + tx_pkts[2]->data_off,
		VIRTIO_MBUF_ADDR(tx_pkts[3], vq) + tx_pkts[3]->data_off
	};

	desc[0].val[0] = base_addr0;
	desc[1].val[0] = base_addr1;

	uint64_t flags = (uint64_t)vq->vq_packed.cached_flags << FLAGS_LEN_BITS_OFFSET;
	uint64x2_t tx_desc0 = {
		flags | (uint64_t)idx << ID_BITS_OFFSET | tx_pkts[0]->data_len,
		flags | (uint64_t)(idx + 1) << ID_BITS_OFFSET | tx_pkts[1]->data_len
	};

	uint64x2_t tx_desc1 = {
		flags | (uint64_t)(idx + 2) << ID_BITS_OFFSET | tx_pkts[2]->data_len,
		flags | (uint64_t)(idx + 3) << ID_BITS_OFFSET | tx_pkts[3]->data_len
	};

	desc[0].val[1] = tx_desc0;
	desc[1].val[1] = tx_desc1;

	if (!vq->hw->has_tx_offload) {
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = rte_pktmbuf_mtod_offset(tx_pkts[i],
					struct virtio_net_hdr *, -head_size);
			/* Clear net hdr. */
			uint16x8_t v_hdr = vld1q_u16((void *)hdr);
			vst1q_u16((void *)hdr, vandq_u16(v_hdr, net_hdr_msk));
		}
	} else {
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = rte_pktmbuf_mtod_offset(tx_pkts[i],
					struct virtio_net_hdr *, -head_size);
			virtqueue_xmit_offload(hdr, tx_pkts[i]);
		}
	}

	/* Enqueue packet buffers. */
	p_desc = &vq->vq_packed.ring.desc[idx];
	vst2q_u64((uint64_t *)p_desc, desc[0]);
	vst2q_u64((uint64_t *)(p_desc + 2), desc[1]);

	virtio_update_batch_stats(&txvq->stats, tx_pkts[0]->pkt_len,
			tx_pkts[1]->pkt_len, tx_pkts[2]->pkt_len,
			tx_pkts[3]->pkt_len);

	vq->vq_avail_idx += PACKED_BATCH_SIZE;
	vq->vq_free_cnt -= PACKED_BATCH_SIZE;

	if (vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^=
			VRING_PACKED_DESC_F_AVAIL_USED;
	}

	return 0;
}

static inline int
virtqueue_dequeue_batch_packed_vec(struct virtnet_rx *rxvq,
				   struct rte_mbuf **rx_pkts)
{
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
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

	uint64x2_t desc_stats = vreinterpretq_u64_u32(~vceqq_u32(v_flag, v_used_flag));

	/* Check all descs are used. */
	if (unlikely(vgetq_lane_u64(desc_stats, 0) || vgetq_lane_u64(desc_stats, 1)))
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
