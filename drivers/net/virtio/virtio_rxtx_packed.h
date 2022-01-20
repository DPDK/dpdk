/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#ifndef _VIRTIO_RXTX_PACKED_H_
#define _VIRTIO_RXTX_PACKED_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_net.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtio.h"
#include "virtqueue.h"

#define BYTE_SIZE 8

#ifdef CC_AVX512_SUPPORT
/* flag bits offset in packed ring desc higher 64bits */
#define FLAGS_BITS_OFFSET ((offsetof(struct vring_packed_desc, flags) - \
	offsetof(struct vring_packed_desc, len)) * BYTE_SIZE)
#elif defined(RTE_ARCH_ARM)
/* flag bits offset in packed ring desc from ID */
#define FLAGS_BITS_OFFSET ((offsetof(struct vring_packed_desc, flags) - \
	offsetof(struct vring_packed_desc, id)) * BYTE_SIZE)
#define FLAGS_LEN_BITS_OFFSET ((offsetof(struct vring_packed_desc, flags) - \
	offsetof(struct vring_packed_desc, len)) * BYTE_SIZE)
#endif

#define PACKED_FLAGS_MASK ((0ULL | VRING_PACKED_DESC_F_AVAIL_USED) << \
	FLAGS_BITS_OFFSET)

/* reference count offset in mbuf rearm data */
#define REFCNT_BITS_OFFSET ((offsetof(struct rte_mbuf, refcnt) - \
	offsetof(struct rte_mbuf, rearm_data)) * BYTE_SIZE)

#ifdef CC_AVX512_SUPPORT
/* segment number offset in mbuf rearm data */
#define SEG_NUM_BITS_OFFSET ((offsetof(struct rte_mbuf, nb_segs) - \
	offsetof(struct rte_mbuf, rearm_data)) * BYTE_SIZE)
/* default rearm data */
#define DEFAULT_REARM_DATA (1ULL << SEG_NUM_BITS_OFFSET | \
	1ULL << REFCNT_BITS_OFFSET)
#endif

/* id bits offset in packed ring desc higher 64bits */
#define ID_BITS_OFFSET ((offsetof(struct vring_packed_desc, id) - \
	offsetof(struct vring_packed_desc, len)) * BYTE_SIZE)

/* net hdr short size mask */
#define NET_HDR_MASK 0x3F

#ifdef RTE_ARCH_ARM
/* The cache line size on different Arm platforms are different, so
 * put a four batch size here to match with the minimum cache line
 * size and accommodate NEON register size.
 */
#define PACKED_BATCH_SIZE 4
#else
#define PACKED_BATCH_SIZE (RTE_CACHE_LINE_SIZE / \
	sizeof(struct vring_packed_desc))
#endif
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
#define virtio_for_each_try_unroll(iter, val, size) _Pragma("unroll 4") \
	for (iter = val; iter < size; iter++)
#endif

#ifndef virtio_for_each_try_unroll
#define virtio_for_each_try_unroll(iter, val, size) \
	for (iter = val; iter < size; iter++)
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
virtqueue_enqueue_single_packed_vec(struct virtnet_tx *txvq,
				    struct rte_mbuf *txm)
{
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t slots, can_push = 0, use_indirect = 0;
	int16_t need;

	/* optimize ring usage */
	if ((virtio_with_feature(hw, VIRTIO_F_ANY_LAYOUT) ||
	     virtio_with_feature(hw, VIRTIO_F_VERSION_1)) &&
	     rte_mbuf_refcnt_read(txm) == 1 && RTE_MBUF_DIRECT(txm) &&
	     txm->nb_segs == 1 && rte_pktmbuf_headroom(txm) >= hdr_size)
		can_push = 1;
	else if (virtio_with_feature(hw, VIRTIO_RING_F_INDIRECT_DESC) &&
		 txm->nb_segs < VIRTIO_MAX_TX_INDIRECT)
		use_indirect = 1;

	/* How many main ring entries are needed to this Tx?
	 * indirect   => 1
	 * any_layout => number of segments
	 * default    => number of segments + 1
	 */
	can_push = rte_mbuf_refcnt_read(txm) == 1 &&
		   RTE_MBUF_DIRECT(txm) &&
		   txm->nb_segs == 1 &&
		   rte_pktmbuf_headroom(txm) >= hdr_size;

	slots = use_indirect ? 1 : (txm->nb_segs + !can_push);
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
	virtqueue_enqueue_xmit_packed(txvq, txm, slots, use_indirect,
				can_push, 1);

	txvq->stats.bytes += txm->pkt_len;
	return 0;
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
	m->ol_flags |= RTE_MBUF_F_RX_IP_CKSUM_UNKNOWN;

	ptype = rte_net_get_ptype(m, &hdr_lens, RTE_PTYPE_ALL_MASK);
	m->packet_type = ptype;
	if ((ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_TCP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_UDP ||
	    (ptype & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_SCTP)
		l4_supported = 1;

	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
		hdrlen = hdr_lens.l2_len + hdr_lens.l3_len + hdr_lens.l4_len;
		if (hdr->csum_start <= hdrlen && l4_supported) {
			m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_NONE;
		} else {
			/* Unknown proto or tunnel, do sw cksum. We can assume
			 * the cksum field is in the first segment since the
			 * buffers we provided to the host are large enough.
			 * In case of SCTP, this will be wrong since it's a CRC
			 * but there's nothing we can do.
			 */
			uint16_t csum = 0, off;

			if (rte_raw_cksum_mbuf(m, hdr->csum_start,
				rte_pktmbuf_pkt_len(m) - hdr->csum_start,
				&csum) < 0)
				return -1;
			if (likely(csum != 0xffff))
				csum = ~csum;
			off = hdr->csum_offset + hdr->csum_start;
			if (rte_pktmbuf_data_len(m) >= off + 1)
				*rte_pktmbuf_mtod_offset(m, uint16_t *,
					off) = csum;
		}
	} else if (hdr->flags & VIRTIO_NET_HDR_F_DATA_VALID && l4_supported) {
		m->ol_flags |= RTE_MBUF_F_RX_L4_CKSUM_GOOD;
	}

	return 0;
}

static inline uint16_t
virtqueue_dequeue_single_packed_vec(struct virtnet_rx *rxvq,
				    struct rte_mbuf **rx_pkts)
{
	uint16_t used_idx, id;
	uint32_t len;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
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
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
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

#endif /* _VIRTIO_RXTX_PACKED_H_ */
