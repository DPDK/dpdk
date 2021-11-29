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
	uint16_t i, cmp;

	if (vq->vq_avail_idx & PACKED_BATCH_MASK)
		return -1;

	if (unlikely((idx + PACKED_BATCH_SIZE) > vq->vq_nentries))
		return -1;

	/* Load four mbufs rearm data */
	RTE_BUILD_BUG_ON(REFCNT_BITS_OFFSET >= 64);
	RTE_BUILD_BUG_ON(SEG_NUM_BITS_OFFSET >= 64);
	__m256i mbufs = _mm256_set_epi64x(*tx_pkts[3]->rearm_data,
					  *tx_pkts[2]->rearm_data,
					  *tx_pkts[1]->rearm_data,
					  *tx_pkts[0]->rearm_data);

	/* refcnt=1 and nb_segs=1 */
	__m256i mbuf_ref = _mm256_set1_epi64x(DEFAULT_REARM_DATA);
	__m256i head_rooms = _mm256_set1_epi16(head_size);

	/* Check refcnt and nb_segs */
	const __mmask16 mask = 0x6 | 0x6 << 4 | 0x6 << 8 | 0x6 << 12;
	cmp = _mm256_mask_cmpneq_epu16_mask(mask, mbufs, mbuf_ref);
	if (unlikely(cmp))
		return -1;

	/* Check headroom is enough */
	const __mmask16 data_mask = 0x1 | 0x1 << 4 | 0x1 << 8 | 0x1 << 12;
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_off) !=
		offsetof(struct rte_mbuf, rearm_data));
	cmp = _mm256_mask_cmplt_epu16_mask(data_mask, mbufs, head_rooms);
	if (unlikely(cmp))
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

	__m512i descs_base = _mm512_set_epi64(tx_pkts[3]->data_len,
			VIRTIO_MBUF_ADDR(tx_pkts[3], vq),
			tx_pkts[2]->data_len,
			VIRTIO_MBUF_ADDR(tx_pkts[2], vq),
			tx_pkts[1]->data_len,
			VIRTIO_MBUF_ADDR(tx_pkts[1], vq),
			tx_pkts[0]->data_len,
			VIRTIO_MBUF_ADDR(tx_pkts[0], vq));

	/* id offset and data offset */
	__m512i data_offsets = _mm512_set_epi64((uint64_t)3 << ID_BITS_OFFSET,
						tx_pkts[3]->data_off,
						(uint64_t)2 << ID_BITS_OFFSET,
						tx_pkts[2]->data_off,
						(uint64_t)1 << ID_BITS_OFFSET,
						tx_pkts[1]->data_off,
						0, tx_pkts[0]->data_off);

	__m512i new_descs = _mm512_add_epi64(descs_base, data_offsets);

	uint64_t flags_temp = (uint64_t)idx << ID_BITS_OFFSET |
		(uint64_t)vq->vq_packed.cached_flags << FLAGS_BITS_OFFSET;

	/* flags offset and guest virtual address offset */
	__m128i flag_offset = _mm_set_epi64x(flags_temp, 0);
	__m512i v_offset = _mm512_broadcast_i32x4(flag_offset);
	__m512i v_desc = _mm512_add_epi64(new_descs, v_offset);

	if (!vq->hw->has_tx_offload) {
		__m128i all_mask = _mm_set1_epi16(0xFFFF);
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = rte_pktmbuf_mtod_offset(tx_pkts[i],
					struct virtio_net_hdr *, -head_size);
			__m128i v_hdr = _mm_loadu_si128((void *)hdr);
			if (unlikely(_mm_mask_test_epi16_mask(NET_HDR_MASK,
							v_hdr, all_mask))) {
				__m128i all_zero = _mm_setzero_si128();
				_mm_mask_storeu_epi16((void *)hdr,
						NET_HDR_MASK, all_zero);
			}
		}
	} else {
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			hdr = rte_pktmbuf_mtod_offset(tx_pkts[i],
					struct virtio_net_hdr *, -head_size);
			virtqueue_xmit_offload(hdr, tx_pkts[i]);
		}
	}

	/* Enqueue Packet buffers */
	_mm512_storeu_si512((void *)&vq->vq_packed.ring.desc[idx], v_desc);

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

static inline uint16_t
virtqueue_dequeue_batch_packed_vec(struct virtnet_rx *rxvq,
				   struct rte_mbuf **rx_pkts)
{
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint64_t addrs[PACKED_BATCH_SIZE];
	uint16_t id = vq->vq_used_cons_idx;
	uint8_t desc_stats;
	uint16_t i;
	void *desc_addr;

	if (id & PACKED_BATCH_MASK)
		return -1;

	if (unlikely((id + PACKED_BATCH_SIZE) > vq->vq_nentries))
		return -1;

	/* only care avail/used bits */
#if defined(RTE_ARCH_I686)
	__m512i v_mask = _mm512_set4_epi64(PACKED_FLAGS_MASK, 0x0,
					   PACKED_FLAGS_MASK, 0x0);
#else
	__m512i v_mask = _mm512_maskz_set1_epi64(0xaa, PACKED_FLAGS_MASK);
#endif
	desc_addr = &vq->vq_packed.ring.desc[id];

	__m512i v_desc = _mm512_loadu_si512(desc_addr);
	__m512i v_flag = _mm512_and_epi64(v_desc, v_mask);

	__m512i v_used_flag = _mm512_setzero_si512();
	if (vq->vq_packed.used_wrap_counter)
#if defined(RTE_ARCH_I686)
		v_used_flag = _mm512_set4_epi64(PACKED_FLAGS_MASK, 0x0,
						PACKED_FLAGS_MASK, 0x0);
#else
		v_used_flag = _mm512_maskz_set1_epi64(0xaa, PACKED_FLAGS_MASK);
#endif

	/* Check all descs are used */
	desc_stats = _mm512_cmpneq_epu64_mask(v_flag, v_used_flag);
	if (desc_stats)
		return -1;

	virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
		rx_pkts[i] = (struct rte_mbuf *)vq->vq_descx[id + i].cookie;
		rte_packet_prefetch(rte_pktmbuf_mtod(rx_pkts[i], void *));

		addrs[i] = (uintptr_t)rx_pkts[i]->rx_descriptor_fields1;
	}

	/*
	 * load len from desc, store into mbuf pkt_len and data_len
	 * len limited by l6bit buf_len, pkt_len[16:31] can be ignored
	 */
	const __mmask16 mask = 0x6 | 0x6 << 4 | 0x6 << 8 | 0x6 << 12;
	__m512i values = _mm512_maskz_shuffle_epi32(mask, v_desc, 0xAA);

	/* reduce hdr_len from pkt_len and data_len */
	__m512i mbuf_len_offset = _mm512_maskz_set1_epi32(mask,
			(uint32_t)-hdr_size);

	__m512i v_value = _mm512_add_epi32(values, mbuf_len_offset);

	/* assert offset of data_len */
	RTE_BUILD_BUG_ON(offsetof(struct rte_mbuf, data_len) !=
		offsetof(struct rte_mbuf, rx_descriptor_fields1) + 8);

	__m512i v_index = _mm512_set_epi64(addrs[3] + 8, addrs[3],
					   addrs[2] + 8, addrs[2],
					   addrs[1] + 8, addrs[1],
					   addrs[0] + 8, addrs[0]);
	/* batch store into mbufs */
	_mm512_i64scatter_epi64(0, v_index, v_value, 1);

	if (hw->has_rx_offload) {
		virtio_for_each_try_unroll(i, 0, PACKED_BATCH_SIZE) {
			char *addr = (char *)rx_pkts[i]->buf_addr +
				RTE_PKTMBUF_HEADROOM - hdr_size;
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
