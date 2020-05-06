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
#include "virtqueue.h"

#define BYTE_SIZE 8
/* flag bits offset in packed ring desc higher 64bits */
#define FLAGS_BITS_OFFSET ((offsetof(struct vring_packed_desc, flags) - \
	offsetof(struct vring_packed_desc, len)) * BYTE_SIZE)

#define PACKED_FLAGS_MASK ((0ULL | VRING_PACKED_DESC_F_AVAIL_USED) << \
	FLAGS_BITS_OFFSET)

/* reference count offset in mbuf rearm data */
#define REFCNT_BITS_OFFSET ((offsetof(struct rte_mbuf, refcnt) - \
	offsetof(struct rte_mbuf, rearm_data)) * BYTE_SIZE)
/* segment number offset in mbuf rearm data */
#define SEG_NUM_BITS_OFFSET ((offsetof(struct rte_mbuf, nb_segs) - \
	offsetof(struct rte_mbuf, rearm_data)) * BYTE_SIZE)

/* default rearm data */
#define DEFAULT_REARM_DATA (1ULL << SEG_NUM_BITS_OFFSET | \
	1ULL << REFCNT_BITS_OFFSET)

/* id bits offset in packed ring desc higher 64bits */
#define ID_BITS_OFFSET ((offsetof(struct vring_packed_desc, id) - \
	offsetof(struct vring_packed_desc, len)) * BYTE_SIZE)

/* net hdr short size mask */
#define NET_HDR_MASK 0x3F

#define PACKED_BATCH_SIZE (RTE_CACHE_LINE_SIZE / \
	sizeof(struct vring_packed_desc))
#define PACKED_BATCH_MASK (PACKED_BATCH_SIZE - 1)

#ifdef VIRTIO_GCC_UNROLL_PRAGMA
#define virtio_for_each_try_unroll(iter, val, size) _Pragma("GCC unroll 4") \
	for (iter = val; iter < size; iter++)
#endif

#ifdef VIRTIO_CLANG_UNROLL_PRAGMA
#define virtio_for_each_try_unroll(iter, val, size) _Pragma("unroll 4") \
	for (iter = val; iter < size; iter++)
#endif

#ifdef VIRTIO_ICC_UNROLL_PRAGMA
#define virtio_for_each_try_unroll(iter, val, size) _Pragma("unroll (4)") \
	for (iter = val; iter < size; iter++)
#endif

#ifndef virtio_for_each_try_unroll
#define virtio_for_each_try_unroll(iter, val, num) \
	for (iter = val; iter < num; iter++)
#endif

static inline void
virtio_update_batch_stats(struct virtnet_stats *stats,
			  uint16_t pkt_len1,
			  uint16_t pkt_len2,
			  uint16_t pkt_len3,
			  uint16_t pkt_len4)
{
	stats->bytes += pkt_len1;
	stats->bytes += pkt_len2;
	stats->bytes += pkt_len3;
	stats->bytes += pkt_len4;
}

static inline int
virtqueue_enqueue_batch_packed_vec(struct virtnet_tx *txvq,
				   struct rte_mbuf **tx_pkts)
{
	struct virtqueue *vq = txvq->vq;
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
			virtqueue_xmit_offload(hdr, tx_pkts[i], true);
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

static inline int
virtqueue_enqueue_single_packed_vec(struct virtnet_tx *txvq,
				    struct rte_mbuf *txm)
{
	struct virtqueue *vq = txvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t slots, can_push;
	int16_t need;

	/* How many main ring entries are needed to this Tx?
	 * any_layout => number of segments
	 * default    => number of segments + 1
	 */
	can_push = rte_mbuf_refcnt_read(txm) == 1 &&
		   RTE_MBUF_DIRECT(txm) &&
		   txm->nb_segs == 1 &&
		   rte_pktmbuf_headroom(txm) >= hdr_size;

	slots = txm->nb_segs + !can_push;
	need = slots - vq->vq_free_cnt;

	/* Positive value indicates it need free vring descriptors */
	if (unlikely(need > 0)) {
		virtio_xmit_cleanup_inorder_packed(vq, need);
		need = slots - vq->vq_free_cnt;
		if (unlikely(need > 0)) {
			PMD_TX_LOG(ERR,
				   "No free tx descriptors to transmit");
			return -1;
		}
	}

	/* Enqueue Packet buffers */
	virtqueue_enqueue_xmit_packed(txvq, txm, slots, can_push, 1);

	txvq->stats.bytes += txm->pkt_len;
	return 0;
}

uint16_t
virtio_xmit_pkts_packed_vec(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = txvq->vq;
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

/* Optionally fill offload information in structure */
static inline int
virtio_vec_rx_offload(struct rte_mbuf *m, struct virtio_net_hdr *hdr)
{
	struct rte_net_hdr_lens hdr_lens;
	uint32_t hdrlen, ptype;
	int l4_supported = 0;

	/* nothing to do */
	if (hdr->flags == 0)
		return 0;

	/* GSO not support in vec path, skip check */
	m->ol_flags |= PKT_RX_IP_CKSUM_UNKNOWN;

	ptype = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);
	m->packet_type = ptype;
	if ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_SCTP)
		l4_supported = 1;

	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		hdrlen = hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len;
		if (hdr->csum_start <= hdrlen && l4_supported) {
			m->ol_flags |= PKT_RX_L4_CKSUM_NONE;
		} else {
			/* Unknown proto or tunnel, do sw cksum. We can assume
			 * the cksum field is in the first segment since the
			 * buffers we provided to the host are large enough.
			 * In case of SCTP, this will be wrong since it's a CRC
			 * but there's nothing we can do.
			 */
			uint16_t csum = 0, off;

			rte_raw_cksum_mbuf(m, hdr->csum_start,
				rte_pktmbuf_pkt_len(m) - hdr->csum_start,
				&csum);
			if (likely(csum != 0xffff))
				csum = ~csum;
			off = hdr->csum_offset + hdr->csum_start;
			if (rte_pktmbuf_data_len(m) >= off + 1)
				*rte_pktmbuf_mtod_offset(m, uint16_t *,
					off) = csum;
		}
	} else if (hdr->flags & VIRTIO_NET_HDR_F_DATA_VALID && l4_supported) {
		m->ol_flags |= PKT_RX_L4_CKSUM_GOOD;
	}

	return 0;
}

static inline uint16_t
virtqueue_dequeue_batch_packed_vec(struct virtnet_rx *rxvq,
				   struct rte_mbuf **rx_pkts)
{
	struct virtqueue *vq = rxvq->vq;
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
	 * len limiated by l6bit buf_len, pkt_len[16:31] can be ignored
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

static uint16_t
virtqueue_dequeue_single_packed_vec(struct virtnet_rx *rxvq,
				    struct rte_mbuf **rx_pkts)
{
	uint16_t used_idx, id;
	uint32_t len;
	struct virtqueue *vq = rxvq->vq;
	struct virtio_hw *hw = vq->hw;
	uint32_t hdr_size = hw->vtnet_hdr_size;
	struct virtio_net_hdr *hdr;
	struct vring_packed_desc *desc;
	struct rte_mbuf *cookie;

	desc = vq->vq_packed.ring.desc;
	used_idx = vq->vq_used_cons_idx;
	if (!desc_is_used(&desc[used_idx], vq))
		return -1;

	len = desc[used_idx].len;
	id = desc[used_idx].id;
	cookie = (struct rte_mbuf *)vq->vq_descx[id].cookie;
	if (unlikely(cookie == NULL)) {
		PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u",
				vq->vq_used_cons_idx);
		return -1;
	}
	rte_prefetch0(cookie);
	rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));

	cookie->data_off = RTE_PKTMBUF_HEADROOM;
	cookie->ol_flags = 0;
	cookie->pkt_len = (uint32_t)(len - hdr_size);
	cookie->data_len = (uint32_t)(len - hdr_size);

	hdr = (struct virtio_net_hdr *)((char *)cookie->buf_addr +
					RTE_PKTMBUF_HEADROOM - hdr_size);
	if (hw->has_rx_offload)
		virtio_vec_rx_offload(cookie, hdr);

	*rx_pkts = cookie;

	rxvq->stats.bytes += cookie->pkt_len;

	vq->vq_free_cnt++;
	vq->vq_used_cons_idx++;
	if (vq->vq_used_cons_idx >= vq->vq_nentries) {
		vq->vq_used_cons_idx -= vq->vq_nentries;
		vq->vq_packed.used_wrap_counter ^= 1;
	}

	return 0;
}

static inline void
virtio_recv_refill_packed_vec(struct virtnet_rx *rxvq,
			      struct rte_mbuf **cookie,
			      uint16_t num)
{
	struct virtqueue *vq = rxvq->vq;
	struct vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	uint16_t flags = vq->vq_packed.cached_flags;
	struct virtio_hw *hw = vq->hw;
	struct vq_desc_extra *dxp;
	uint16_t idx, i;
	uint16_t batch_num, total_num = 0;
	uint16_t head_idx = vq->vq_avail_idx;
	uint16_t head_flag = vq->vq_packed.cached_flags;
	uint64_t addr;

	do {
		idx = vq->vq_avail_idx;

		batch_num = PACKED_BATCH_SIZE;
		if (unlikely((idx + PACKED_BATCH_SIZE) > vq->vq_nentries))
			batch_num = vq->vq_nentries - idx;
		if (unlikely((total_num + batch_num) > num))
			batch_num = num - total_num;

		virtio_for_each_try_unroll(i, 0, batch_num) {
			dxp = &vq->vq_descx[idx + i];
			dxp->cookie = (void *)cookie[total_num + i];

			addr = VIRTIO_MBUF_ADDR(cookie[total_num + i], vq) +
				RTE_PKTMBUF_HEADROOM - hw->vtnet_hdr_size;
			start_dp[idx + i].addr = addr;
			start_dp[idx + i].len = cookie[total_num + i]->buf_len
				- RTE_PKTMBUF_HEADROOM + hw->vtnet_hdr_size;
			if (total_num || i) {
				virtqueue_store_flags_packed(&start_dp[idx + i],
						flags, hw->weak_barriers);
			}
		}

		vq->vq_avail_idx += batch_num;
		if (vq->vq_avail_idx >= vq->vq_nentries) {
			vq->vq_avail_idx -= vq->vq_nentries;
			vq->vq_packed.cached_flags ^=
				VRING_PACKED_DESC_F_AVAIL_USED;
			flags = vq->vq_packed.cached_flags;
		}
		total_num += batch_num;
	} while (total_num < num);

	virtqueue_store_flags_packed(&start_dp[head_idx], head_flag,
				hw->weak_barriers);
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
}

uint16_t
virtio_recv_pkts_packed_vec(void *rx_queue,
			    struct rte_mbuf **rx_pkts,
			    uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = rxvq->vq;
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
		if (!virtqueue_dequeue_batch_packed_vec(rxvq,
					&rx_pkts[nb_rx])) {
			nb_rx += PACKED_BATCH_SIZE;
			num -= PACKED_BATCH_SIZE;
			continue;
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
