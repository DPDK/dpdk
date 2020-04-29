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
	__m512i v_mask = _mm512_maskz_set1_epi64(0xaa, PACKED_FLAGS_MASK);
	desc_addr = &vq->vq_packed.ring.desc[id];

	__m512i v_desc = _mm512_loadu_si512(desc_addr);
	__m512i v_flag = _mm512_and_epi64(v_desc, v_mask);

	__m512i v_used_flag = _mm512_setzero_si512();
	if (vq->vq_packed.used_wrap_counter)
		v_used_flag = _mm512_maskz_set1_epi64(0xaa, PACKED_FLAGS_MASK);

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
