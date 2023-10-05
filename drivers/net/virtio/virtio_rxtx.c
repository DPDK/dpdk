/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <rte_cycles.h>
#include <rte_memory.h>
#include <rte_branch_prediction.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_prefetch.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_byteorder.h>
#include <rte_net.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>

#include "virtio_logs.h"
#include "virtio_ethdev.h"
#include "virtio.h"
#include "virtqueue.h"
#include "virtio_rxtx.h"
#include "virtio_rxtx_simple.h"
#include "virtio_ring.h"

#ifdef RTE_LIBRTE_VIRTIO_DEBUG_DUMP
#define VIRTIO_DUMP_PACKET(m, len) rte_pktmbuf_dump(stdout, m, len)
#else
#define  VIRTIO_DUMP_PACKET(m, len) do { } while (0)
#endif

void
vq_ring_free_inorder(struct virtqueue *vq, uint16_t desc_idx, uint16_t num)
{
	vq->vq_free_cnt += num;
	vq->vq_desc_tail_idx = desc_idx & (vq->vq_nentries - 1);
}

void
vq_ring_free_chain(struct virtqueue *vq, uint16_t desc_idx)
{
	struct vring_desc *dp, *dp_tail;
	struct vq_desc_extra *dxp;
	uint16_t desc_idx_last = desc_idx;

	dp  = &vq->vq_split.ring.desc[desc_idx];
	dxp = &vq->vq_descx[desc_idx];
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt + dxp->ndescs);
	if ((dp->flags & VRING_DESC_F_INDIRECT) == 0) {
		while (dp->flags & VRING_DESC_F_NEXT) {
			desc_idx_last = dp->next;
			dp = &vq->vq_split.ring.desc[dp->next];
		}
	}
	dxp->ndescs = 0;

	/*
	 * We must append the existing free chain, if any, to the end of
	 * newly freed chain. If the virtqueue was completely used, then
	 * head would be VQ_RING_DESC_CHAIN_END (ASSERTed above).
	 */
	if (vq->vq_desc_tail_idx == VQ_RING_DESC_CHAIN_END) {
		vq->vq_desc_head_idx = desc_idx;
	} else {
		dp_tail = &vq->vq_split.ring.desc[vq->vq_desc_tail_idx];
		dp_tail->next = desc_idx;
	}

	vq->vq_desc_tail_idx = desc_idx_last;
	dp->next = VQ_RING_DESC_CHAIN_END;
}

void
virtio_update_packet_stats(struct virtnet_stats *stats, struct rte_mbuf *mbuf)
{
	uint32_t s = mbuf->pkt_len;
	struct rte_ether_addr *ea;

	stats->bytes += s;

	if (s == 64) {
		stats->size_bins[1]++;
	} else if (s > 64 && s < 1024) {
		uint32_t bin;

		/* count zeros, and offset into correct bin */
		bin = (sizeof(s) * 8) - rte_clz32(s) - 5;
		stats->size_bins[bin]++;
	} else {
		if (s < 64)
			stats->size_bins[0]++;
		else if (s < 1519)
			stats->size_bins[6]++;
		else
			stats->size_bins[7]++;
	}

	ea = rte_pktmbuf_mtod(mbuf, struct rte_ether_addr *);
	if (rte_is_multicast_ether_addr(ea)) {
		if (rte_is_broadcast_ether_addr(ea))
			stats->broadcast++;
		else
			stats->multicast++;
	}
}

static inline void
virtio_rx_stats_updated(struct virtnet_rx *rxvq, struct rte_mbuf *m)
{
	VIRTIO_DUMP_PACKET(m, m->data_len);

	virtio_update_packet_stats(&rxvq->stats, m);
}

static uint16_t
virtqueue_dequeue_burst_rx_packed(struct virtqueue *vq,
				  struct rte_mbuf **rx_pkts,
				  uint32_t *len,
				  uint16_t num)
{
	struct rte_mbuf *cookie;
	uint16_t used_idx;
	uint16_t id;
	struct vring_packed_desc *desc;
	uint16_t i;

	desc = vq->vq_packed.ring.desc;

	for (i = 0; i < num; i++) {
		used_idx = vq->vq_used_cons_idx;
		/* desc_is_used has a load-acquire or rte_io_rmb inside
		 * and wait for used desc in virtqueue.
		 */
		if (!desc_is_used(&desc[used_idx], vq))
			return i;
		len[i] = desc[used_idx].len;
		id = desc[used_idx].id;
		cookie = (struct rte_mbuf *)vq->vq_descx[id].cookie;
		if (unlikely(cookie == NULL)) {
			PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u",
				vq->vq_used_cons_idx);
			break;
		}
		rte_prefetch0(cookie);
		rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));
		rx_pkts[i] = cookie;

		vq->vq_free_cnt++;
		vq->vq_used_cons_idx++;
		if (vq->vq_used_cons_idx >= vq->vq_nentries) {
			vq->vq_used_cons_idx -= vq->vq_nentries;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
	}

	return i;
}

static uint16_t
virtqueue_dequeue_burst_rx(struct virtqueue *vq, struct rte_mbuf **rx_pkts,
			   uint32_t *len, uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_mbuf *cookie;
	uint16_t used_idx, desc_idx;
	uint16_t i;

	/*  Caller does the check */
	for (i = 0; i < num ; i++) {
		used_idx = (uint16_t)(vq->vq_used_cons_idx & (vq->vq_nentries - 1));
		uep = &vq->vq_split.ring.used->ring[used_idx];
		desc_idx = (uint16_t) uep->id;
		len[i] = uep->len;
		cookie = (struct rte_mbuf *)vq->vq_descx[desc_idx].cookie;

		if (unlikely(cookie == NULL)) {
			PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u",
				vq->vq_used_cons_idx);
			break;
		}

		rte_prefetch0(cookie);
		rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));
		rx_pkts[i]  = cookie;
		vq->vq_used_cons_idx++;
		vq_ring_free_chain(vq, desc_idx);
		vq->vq_descx[desc_idx].cookie = NULL;
	}

	return i;
}

static uint16_t
virtqueue_dequeue_rx_inorder(struct virtqueue *vq,
			struct rte_mbuf **rx_pkts,
			uint32_t *len,
			uint16_t num)
{
	struct vring_used_elem *uep;
	struct rte_mbuf *cookie;
	uint16_t used_idx = 0;
	uint16_t i;

	if (unlikely(num == 0))
		return 0;

	for (i = 0; i < num; i++) {
		used_idx = vq->vq_used_cons_idx & (vq->vq_nentries - 1);
		/* Desc idx same as used idx */
		uep = &vq->vq_split.ring.used->ring[used_idx];
		len[i] = uep->len;
		cookie = (struct rte_mbuf *)vq->vq_descx[used_idx].cookie;

		if (unlikely(cookie == NULL)) {
			PMD_DRV_LOG(ERR, "vring descriptor with no mbuf cookie at %u",
				vq->vq_used_cons_idx);
			break;
		}

		rte_prefetch0(cookie);
		rte_packet_prefetch(rte_pktmbuf_mtod(cookie, void *));
		rx_pkts[i]  = cookie;
		vq->vq_used_cons_idx++;
		vq->vq_descx[used_idx].cookie = NULL;
	}

	vq_ring_free_inorder(vq, used_idx, i);
	return i;
}

static inline int
virtqueue_enqueue_refill_inorder(struct virtqueue *vq,
			struct rte_mbuf **cookies,
			uint16_t num)
{
	struct vq_desc_extra *dxp;
	struct virtio_hw *hw = vq->hw;
	struct vring_desc *start_dp;
	uint16_t head_idx, idx, i = 0;

	if (unlikely(vq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(vq->vq_free_cnt < num))
		return -EMSGSIZE;

	head_idx = vq->vq_desc_head_idx & (vq->vq_nentries - 1);
	start_dp = vq->vq_split.ring.desc;

	while (i < num) {
		idx = head_idx & (vq->vq_nentries - 1);
		dxp = &vq->vq_descx[idx];
		dxp->cookie = (void *)cookies[i];
		dxp->ndescs = 1;

		start_dp[idx].addr = VIRTIO_MBUF_ADDR(cookies[i], vq) +
			RTE_PKTMBUF_HEADROOM - hw->vtnet_hdr_size;
		start_dp[idx].len = cookies[i]->buf_len -
			RTE_PKTMBUF_HEADROOM + hw->vtnet_hdr_size;
		start_dp[idx].flags =  VRING_DESC_F_WRITE;

		vq_update_avail_ring(vq, idx);
		head_idx++;
		i++;
	}

	vq->vq_desc_head_idx += num;
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
	return 0;
}

static inline int
virtqueue_enqueue_recv_refill(struct virtqueue *vq, struct rte_mbuf **cookie,
				uint16_t num)
{
	struct vq_desc_extra *dxp;
	struct virtio_hw *hw = vq->hw;
	struct vring_desc *start_dp = vq->vq_split.ring.desc;
	uint16_t idx, i;

	if (unlikely(vq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(vq->vq_free_cnt < num))
		return -EMSGSIZE;

	if (unlikely(vq->vq_desc_head_idx >= vq->vq_nentries))
		return -EFAULT;

	for (i = 0; i < num; i++) {
		idx = vq->vq_desc_head_idx;
		dxp = &vq->vq_descx[idx];
		dxp->cookie = (void *)cookie[i];
		dxp->ndescs = 1;

		start_dp[idx].addr = VIRTIO_MBUF_ADDR(cookie[i], vq) +
			RTE_PKTMBUF_HEADROOM - hw->vtnet_hdr_size;
		start_dp[idx].len = cookie[i]->buf_len - RTE_PKTMBUF_HEADROOM +
			hw->vtnet_hdr_size;
		start_dp[idx].flags = VRING_DESC_F_WRITE;
		vq->vq_desc_head_idx = start_dp[idx].next;
		vq_update_avail_ring(vq, idx);
		if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END) {
			vq->vq_desc_tail_idx = vq->vq_desc_head_idx;
			break;
		}
	}

	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);

	return 0;
}

static inline void
virtqueue_refill_single_packed(struct virtqueue *vq,
			       struct vring_packed_desc *dp,
			       struct rte_mbuf *cookie)
{
	uint16_t flags = vq->vq_packed.cached_flags;
	struct virtio_hw *hw = vq->hw;

	dp->addr = VIRTIO_MBUF_ADDR(cookie, vq) + RTE_PKTMBUF_HEADROOM - hw->vtnet_hdr_size;
	dp->len = cookie->buf_len - RTE_PKTMBUF_HEADROOM + hw->vtnet_hdr_size;

	virtqueue_store_flags_packed(dp, flags, hw->weak_barriers);

	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^=
			VRING_PACKED_DESC_F_AVAIL_USED;
		flags = vq->vq_packed.cached_flags;
	}
}

static inline int
virtqueue_enqueue_recv_refill_packed_init(struct virtqueue *vq,
				     struct rte_mbuf **cookie, uint16_t num)
{
	struct vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	struct vq_desc_extra *dxp;
	uint16_t idx;
	int i;

	if (unlikely(vq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(vq->vq_free_cnt < num))
		return -EMSGSIZE;

	for (i = 0; i < num; i++) {
		idx = vq->vq_avail_idx;
		dxp = &vq->vq_descx[idx];
		dxp->cookie = (void *)cookie[i];
		dxp->ndescs = 1;

		virtqueue_refill_single_packed(vq, &start_dp[idx], cookie[i]);
	}
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
	return 0;
}

static inline int
virtqueue_enqueue_recv_refill_packed(struct virtqueue *vq,
				     struct rte_mbuf **cookie, uint16_t num)
{
	struct vring_packed_desc *start_dp = vq->vq_packed.ring.desc;
	struct vq_desc_extra *dxp;
	uint16_t idx, did;
	int i;

	if (unlikely(vq->vq_free_cnt == 0))
		return -ENOSPC;
	if (unlikely(vq->vq_free_cnt < num))
		return -EMSGSIZE;

	for (i = 0; i < num; i++) {
		idx = vq->vq_avail_idx;
		did = start_dp[idx].id;
		dxp = &vq->vq_descx[did];
		dxp->cookie = (void *)cookie[i];
		dxp->ndescs = 1;

		virtqueue_refill_single_packed(vq, &start_dp[idx], cookie[i]);
	}
	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
	return 0;
}

/* When doing TSO, the IP length is not included in the pseudo header
 * checksum of the packet given to the PMD, but for virtio it is
 * expected.
 */
static void
virtio_tso_fix_cksum(struct rte_mbuf *m)
{
	/* common case: header is not fragmented */
	if (likely(rte_pktmbuf_data_len(m) >= m->l2_len + m->l3_len +
			m->l4_len)) {
		struct rte_ipv4_hdr *iph;
		struct rte_tcp_hdr *th;
		uint16_t prev_cksum, new_cksum;
		uint32_t ip_paylen;
		uint32_t tmp;

		iph = rte_pktmbuf_mtod_offset(m,
					struct rte_ipv4_hdr *, m->l2_len);
		th = RTE_PTR_ADD(iph, m->l3_len);

		/*
		 * Calculate IPv4 header checksum with current total length value
		 * (whatever it is) to have correct checksum after update on edits
		 * done by TSO.
		 */
		if ((iph->version_ihl >> 4) == 4) {
			iph->hdr_checksum = 0;
			iph->hdr_checksum = rte_ipv4_cksum(iph);
		}

		/*
		 * Do not use IPv4 total length and IPv6 payload length fields to get
		 * TSO payload length since it could not fit into 16 bits.
		 */
		ip_paylen = rte_cpu_to_be_32(rte_pktmbuf_pkt_len(m) - m->l2_len -
					m->l3_len);

		/* calculate the new phdr checksum not including ip_paylen */
		prev_cksum = th->cksum;
		tmp = prev_cksum;
		tmp += (ip_paylen & 0xffff) + (ip_paylen >> 16);
		tmp = (tmp & 0xffff) + (tmp >> 16);
		new_cksum = tmp;

		/* replace it in the packet */
		th->cksum = new_cksum;
	}
}




static inline void
virtqueue_enqueue_xmit_inorder(struct virtnet_tx *txvq,
			struct rte_mbuf **cookies,
			uint16_t num)
{
	struct vq_desc_extra *dxp;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct vring_desc *start_dp;
	struct virtio_net_hdr *hdr;
	uint16_t idx;
	int16_t head_size = vq->hw->vtnet_hdr_size;
	uint16_t i = 0;

	idx = vq->vq_desc_head_idx;
	start_dp = vq->vq_split.ring.desc;

	while (i < num) {
		idx = idx & (vq->vq_nentries - 1);
		dxp = &vq->vq_descx[vq->vq_avail_idx & (vq->vq_nentries - 1)];
		dxp->cookie = (void *)cookies[i];
		dxp->ndescs = 1;
		virtio_update_packet_stats(&txvq->stats, cookies[i]);

		hdr = rte_pktmbuf_mtod_offset(cookies[i],
				struct virtio_net_hdr *, -head_size);

		/* if offload disabled, hdr is not zeroed yet, do it now */
		if (!vq->hw->has_tx_offload)
			virtqueue_clear_net_hdr(hdr);
		else
			virtqueue_xmit_offload(hdr, cookies[i]);

		start_dp[idx].addr = VIRTIO_MBUF_DATA_DMA_ADDR(cookies[i], vq) - head_size;
		start_dp[idx].len = cookies[i]->data_len + head_size;
		start_dp[idx].flags = 0;


		vq_update_avail_ring(vq, idx);

		idx++;
		i++;
	};

	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - num);
	vq->vq_desc_head_idx = idx & (vq->vq_nentries - 1);
}

static inline void
virtqueue_enqueue_xmit_packed_fast(struct virtnet_tx *txvq,
				   struct rte_mbuf *cookie,
				   int in_order)
{
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct vring_packed_desc *dp;
	struct vq_desc_extra *dxp;
	uint16_t idx, id, flags;
	int16_t head_size = vq->hw->vtnet_hdr_size;
	struct virtio_net_hdr *hdr;

	id = in_order ? vq->vq_avail_idx : vq->vq_desc_head_idx;
	idx = vq->vq_avail_idx;
	dp = &vq->vq_packed.ring.desc[idx];

	dxp = &vq->vq_descx[id];
	dxp->ndescs = 1;
	dxp->cookie = cookie;

	flags = vq->vq_packed.cached_flags;

	/* prepend cannot fail, checked by caller */
	hdr = rte_pktmbuf_mtod_offset(cookie, struct virtio_net_hdr *,
				      -head_size);

	/* if offload disabled, hdr is not zeroed yet, do it now */
	if (!vq->hw->has_tx_offload)
		virtqueue_clear_net_hdr(hdr);
	else
		virtqueue_xmit_offload(hdr, cookie);

	dp->addr = VIRTIO_MBUF_DATA_DMA_ADDR(cookie, vq) - head_size;
	dp->len = cookie->data_len + head_size;
	dp->id = id;

	if (++vq->vq_avail_idx >= vq->vq_nentries) {
		vq->vq_avail_idx -= vq->vq_nentries;
		vq->vq_packed.cached_flags ^= VRING_PACKED_DESC_F_AVAIL_USED;
	}

	vq->vq_free_cnt--;

	if (!in_order) {
		vq->vq_desc_head_idx = dxp->next;
		if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
			vq->vq_desc_tail_idx = VQ_RING_DESC_CHAIN_END;
	}

	virtqueue_store_flags_packed(dp, flags, vq->hw->weak_barriers);
}

static inline void
virtqueue_enqueue_xmit(struct virtnet_tx *txvq, struct rte_mbuf *cookie,
			uint16_t needed, int use_indirect, int can_push,
			int in_order)
{
	struct virtio_tx_region *txr = txvq->hdr_mz->addr;
	struct vq_desc_extra *dxp;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct vring_desc *start_dp;
	uint16_t seg_num = cookie->nb_segs;
	uint16_t head_idx, idx;
	int16_t head_size = vq->hw->vtnet_hdr_size;
	bool prepend_header = false;
	struct virtio_net_hdr *hdr;

	head_idx = vq->vq_desc_head_idx;
	idx = head_idx;
	if (in_order)
		dxp = &vq->vq_descx[vq->vq_avail_idx & (vq->vq_nentries - 1)];
	else
		dxp = &vq->vq_descx[idx];
	dxp->cookie = (void *)cookie;
	dxp->ndescs = needed;

	start_dp = vq->vq_split.ring.desc;

	if (can_push) {
		/* prepend cannot fail, checked by caller */
		hdr = rte_pktmbuf_mtod_offset(cookie, struct virtio_net_hdr *,
					      -head_size);
		prepend_header = true;

		/* if offload disabled, it is not zeroed below, do it now */
		if (!vq->hw->has_tx_offload)
			virtqueue_clear_net_hdr(hdr);
	} else if (use_indirect) {
		/* setup tx ring slot to point to indirect
		 * descriptor list stored in reserved region.
		 *
		 * the first slot in indirect ring is already preset
		 * to point to the header in reserved region
		 */
		start_dp[idx].addr = txvq->hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_indir, txr);
		start_dp[idx].len = (seg_num + 1) * sizeof(struct vring_desc);
		start_dp[idx].flags = VRING_DESC_F_INDIRECT;
		hdr = (struct virtio_net_hdr *)&txr[idx].tx_hdr;

		/* loop below will fill in rest of the indirect elements */
		start_dp = txr[idx].tx_indir;
		idx = 1;
	} else {
		/* setup first tx ring slot to point to header
		 * stored in reserved region.
		 */
		start_dp[idx].addr = txvq->hdr_mem + RTE_PTR_DIFF(&txr[idx].tx_hdr, txr);
		start_dp[idx].len = vq->hw->vtnet_hdr_size;
		start_dp[idx].flags = VRING_DESC_F_NEXT;
		hdr = (struct virtio_net_hdr *)&txr[idx].tx_hdr;

		idx = start_dp[idx].next;
	}

	if (vq->hw->has_tx_offload)
		virtqueue_xmit_offload(hdr, cookie);

	do {
		start_dp[idx].addr = VIRTIO_MBUF_DATA_DMA_ADDR(cookie, vq);
		start_dp[idx].len = cookie->data_len;
		if (prepend_header) {
			start_dp[idx].addr -= head_size;
			start_dp[idx].len += head_size;
			prepend_header = false;
		}
		start_dp[idx].flags = cookie->next ? VRING_DESC_F_NEXT : 0;
		idx = start_dp[idx].next;
	} while ((cookie = cookie->next) != NULL);

	if (use_indirect)
		idx = vq->vq_split.ring.desc[head_idx].next;

	vq->vq_free_cnt = (uint16_t)(vq->vq_free_cnt - needed);

	vq->vq_desc_head_idx = idx;
	vq_update_avail_ring(vq, head_idx);

	if (!in_order) {
		if (vq->vq_desc_head_idx == VQ_RING_DESC_CHAIN_END)
			vq->vq_desc_tail_idx = idx;
	}
}

void
virtio_dev_cq_start(struct rte_eth_dev *dev)
{
	struct virtio_hw *hw = dev->data->dev_private;

	if (hw->cvq) {
		rte_spinlock_init(&hw->cvq->lock);
		VIRTQUEUE_DUMP(virtnet_cq_to_vq(hw->cvq));
	}
}

int
virtio_dev_rx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id __rte_unused,
			const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mp)
{
	uint16_t vq_idx = 2 * queue_idx + VTNET_SQ_RQ_QUEUE_IDX;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = hw->vqs[vq_idx];
	struct virtnet_rx *rxvq;
	uint16_t rx_free_thresh;
	uint16_t buf_size;
	const char *error;

	PMD_INIT_FUNC_TRACE();

	if (rx_conf->rx_deferred_start) {
		PMD_INIT_LOG(ERR, "Rx deferred start is not supported");
		return -EINVAL;
	}

	buf_size = virtio_rx_mem_pool_buf_size(mp);
	if (!virtio_rx_check_scatter(hw->max_rx_pkt_len, buf_size,
				     hw->rx_ol_scatter, &error)) {
		PMD_INIT_LOG(ERR, "RxQ %u Rx scatter check failed: %s",
			     queue_idx, error);
		return -EINVAL;
	}

	rx_free_thresh = rx_conf->rx_free_thresh;
	if (rx_free_thresh == 0)
		rx_free_thresh =
			RTE_MIN(vq->vq_nentries / 4, DEFAULT_RX_FREE_THRESH);

	if (rx_free_thresh & 0x3) {
		PMD_INIT_LOG(ERR, "rx_free_thresh must be multiples of four."
			" (rx_free_thresh=%u port=%u queue=%u)",
			rx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	if (rx_free_thresh >= vq->vq_nentries) {
		PMD_INIT_LOG(ERR, "rx_free_thresh must be less than the "
			"number of RX entries (%u)."
			" (rx_free_thresh=%u port=%u queue=%u)",
			vq->vq_nentries,
			rx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}
	vq->vq_free_thresh = rx_free_thresh;

	/*
	 * For split ring vectorized path descriptors number must be
	 * equal to the ring size.
	 */
	if (nb_desc > vq->vq_nentries ||
	    (!virtio_with_packed_queue(hw) && hw->use_vec_rx)) {
		nb_desc = vq->vq_nentries;
	}
	vq->vq_free_cnt = RTE_MIN(vq->vq_free_cnt, nb_desc);

	rxvq = &vq->rxq;
	rxvq->mpool = mp;
	dev->data->rx_queues[queue_idx] = rxvq;

	return 0;
}

int
virtio_dev_rx_queue_setup_finish(struct rte_eth_dev *dev, uint16_t queue_idx)
{
	uint16_t vq_idx = 2 * queue_idx + VTNET_SQ_RQ_QUEUE_IDX;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = hw->vqs[vq_idx];
	struct virtnet_rx *rxvq = &vq->rxq;
	struct rte_mbuf *m;
	uint16_t desc_idx;
	int error, nbufs, i;
	bool in_order = virtio_with_feature(hw, VIRTIO_F_IN_ORDER);

	PMD_INIT_FUNC_TRACE();

	/* Allocate blank mbufs for the each rx descriptor */
	nbufs = 0;

	if (hw->use_vec_rx && !virtio_with_packed_queue(hw)) {
		for (desc_idx = 0; desc_idx < vq->vq_nentries;
		     desc_idx++) {
			vq->vq_split.ring.avail->ring[desc_idx] = desc_idx;
			vq->vq_split.ring.desc[desc_idx].flags =
				VRING_DESC_F_WRITE;
		}

		virtio_rxq_vec_setup(rxvq);
	}

	if (hw->use_vec_rx) {
		memset(rxvq->fake_mbuf, 0, sizeof(*rxvq->fake_mbuf));
		for (desc_idx = 0; desc_idx < RTE_PMD_VIRTIO_RX_MAX_BURST; desc_idx++)
			vq->rxq.sw_ring[vq->vq_nentries + desc_idx] = rxvq->fake_mbuf;
	}

	if (hw->use_vec_rx && !virtio_with_packed_queue(hw)) {
		while (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
			virtio_rxq_rearm_vec(rxvq);
			nbufs += RTE_VIRTIO_VPMD_RX_REARM_THRESH;
		}
	} else if (!virtio_with_packed_queue(vq->hw) && in_order) {
		if ((!virtqueue_full(vq))) {
			uint16_t free_cnt = vq->vq_free_cnt;
			struct rte_mbuf *pkts[free_cnt];

			if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, pkts,
				free_cnt)) {
				error = virtqueue_enqueue_refill_inorder(vq,
						pkts,
						free_cnt);
				if (unlikely(error)) {
					for (i = 0; i < free_cnt; i++)
						rte_pktmbuf_free(pkts[i]);
				} else {
					nbufs += free_cnt;
				}
			}

			vq_update_avail_idx(vq);
		}
	} else {
		while (!virtqueue_full(vq)) {
			m = rte_mbuf_raw_alloc(rxvq->mpool);
			if (m == NULL)
				break;

			/* Enqueue allocated buffers */
			if (virtio_with_packed_queue(vq->hw))
				error = virtqueue_enqueue_recv_refill_packed_init(vq,
						&m, 1);
			else
				error = virtqueue_enqueue_recv_refill(vq,
						&m, 1);
			if (error) {
				rte_pktmbuf_free(m);
				break;
			}
			nbufs++;
		}

		if (!virtio_with_packed_queue(vq->hw))
			vq_update_avail_idx(vq);
	}

	PMD_INIT_LOG(DEBUG, "Allocated %d bufs (port=%u queue=%u)", nbufs,
		     dev->data->port_id, queue_idx);

	VIRTQUEUE_DUMP(vq);

	return 0;
}

/*
 * struct rte_eth_dev *dev: Used to update dev
 * uint16_t nb_desc: Defaults to values read from config space
 * unsigned int socket_id: Used to allocate memzone
 * const struct rte_eth_txconf *tx_conf: Used to setup tx engine
 * uint16_t queue_idx: Just used as an index in dev txq list
 */
int
virtio_dev_tx_queue_setup(struct rte_eth_dev *dev,
			uint16_t queue_idx,
			uint16_t nb_desc,
			unsigned int socket_id __rte_unused,
			const struct rte_eth_txconf *tx_conf)
{
	uint16_t vq_idx = 2 * queue_idx + VTNET_SQ_TQ_QUEUE_IDX;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = hw->vqs[vq_idx];
	struct virtnet_tx *txvq;
	uint16_t tx_free_thresh;

	PMD_INIT_FUNC_TRACE();

	if (tx_conf->tx_deferred_start) {
		PMD_INIT_LOG(ERR, "Tx deferred start is not supported");
		return -EINVAL;
	}

	if (nb_desc == 0 || nb_desc > vq->vq_nentries)
		nb_desc = vq->vq_nentries;
	vq->vq_free_cnt = RTE_MIN(vq->vq_free_cnt, nb_desc);

	txvq = &vq->txq;

	tx_free_thresh = tx_conf->tx_free_thresh;
	if (tx_free_thresh == 0)
		tx_free_thresh =
			RTE_MIN(vq->vq_nentries / 4, DEFAULT_TX_FREE_THRESH);

	if (tx_free_thresh >= (vq->vq_nentries - 3)) {
		PMD_DRV_LOG(ERR, "tx_free_thresh must be less than the "
			"number of TX entries minus 3 (%u)."
			" (tx_free_thresh=%u port=%u queue=%u)",
			vq->vq_nentries - 3,
			tx_free_thresh, dev->data->port_id, queue_idx);
		return -EINVAL;
	}

	vq->vq_free_thresh = tx_free_thresh;

	dev->data->tx_queues[queue_idx] = txvq;
	return 0;
}

int
virtio_dev_tx_queue_setup_finish(struct rte_eth_dev *dev,
				uint16_t queue_idx)
{
	uint16_t vq_idx = 2 * queue_idx + VTNET_SQ_TQ_QUEUE_IDX;
	struct virtio_hw *hw = dev->data->dev_private;
	struct virtqueue *vq = hw->vqs[vq_idx];

	PMD_INIT_FUNC_TRACE();

	if (!virtio_with_packed_queue(hw)) {
		if (virtio_with_feature(hw, VIRTIO_F_IN_ORDER))
			vq->vq_split.ring.desc[vq->vq_nentries - 1].next = 0;
	}

	VIRTQUEUE_DUMP(vq);

	return 0;
}

static inline void
virtio_discard_rxbuf(struct virtqueue *vq, struct rte_mbuf *m)
{
	int error;
	/*
	 * Requeue the discarded mbuf. This should always be
	 * successful since it was just dequeued.
	 */
	if (virtio_with_packed_queue(vq->hw))
		error = virtqueue_enqueue_recv_refill_packed(vq, &m, 1);
	else
		error = virtqueue_enqueue_recv_refill(vq, &m, 1);

	if (unlikely(error)) {
		PMD_DRV_LOG(ERR, "cannot requeue discarded mbuf");
		rte_pktmbuf_free(m);
	}
}

static inline void
virtio_discard_rxbuf_inorder(struct virtqueue *vq, struct rte_mbuf *m)
{
	int error;

	error = virtqueue_enqueue_refill_inorder(vq, &m, 1);
	if (unlikely(error)) {
		PMD_DRV_LOG(ERR, "cannot requeue discarded mbuf");
		rte_pktmbuf_free(m);
	}
}

/* Optionally fill offload information in structure */
static inline int
virtio_rx_offload(struct rte_mbuf *m, struct virtio_net_hdr *hdr)
{
	struct rte_net_hdr_lens hdr_lens;
	uint32_t hdrlen, ptype;
	int l4_supported = 0;

	/* nothing to do */
	if (hdr->flags == 0 && hdr->gso_type == VIRTIO_NET_HDR_GSO_NONE)
		return 0;

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
				return -EINVAL;
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

	/* GSO request, save required information in mbuf */
	if (hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
		/* Check unsupported modes */
		if ((hdr->gso_type & VIRTIO_NET_HDR_GSO_ECN) ||
		    (hdr->gso_size == 0)) {
			return -EINVAL;
		}

		/* Update mss lengths in mbuf */
		m->tso_segsz = hdr->gso_size;
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
			case VIRTIO_NET_HDR_GSO_TCPV4:
			case VIRTIO_NET_HDR_GSO_TCPV6:
				m->ol_flags |= RTE_MBUF_F_RX_LRO |
					RTE_MBUF_F_RX_L4_CKSUM_NONE;
				break;
			default:
				return -EINVAL;
		}
	}

	return 0;
}

#define DESC_PER_CACHELINE (RTE_CACHE_LINE_SIZE / sizeof(struct vring_desc))
uint16_t
virtio_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts, uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	struct rte_mbuf *rxm;
	uint16_t nb_used, num, nb_rx;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t i, nb_enqueued;
	uint32_t hdr_size;
	struct virtio_net_hdr *hdr;

	nb_rx = 0;
	if (unlikely(hw->started == 0))
		return nb_rx;

	nb_used = virtqueue_nused(vq);

	num = likely(nb_used <= nb_pkts) ? nb_used : nb_pkts;
	if (unlikely(num > VIRTIO_MBUF_BURST_SZ))
		num = VIRTIO_MBUF_BURST_SZ;
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((vq->vq_used_cons_idx + num) % DESC_PER_CACHELINE);

	num = virtqueue_dequeue_burst_rx(vq, rcv_pkts, len, num);
	PMD_RX_LOG(DEBUG, "used:%d dequeue:%d", nb_used, num);

	nb_enqueued = 0;
	hdr_size = hw->vtnet_hdr_size;

	for (i = 0; i < num ; i++) {
		rxm = rcv_pkts[i];

		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		if (unlikely(len[i] < hdr_size + RTE_ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		rxm->port = hw->port_id;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;

		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		hdr = (struct virtio_net_hdr *)((char *)rxm->buf_addr +
			RTE_PKTMBUF_HEADROOM - hdr_size);

		if (hw->vlan_strip)
			rte_vlan_strip(rxm);

		if (hw->has_rx_offload && virtio_rx_offload(rxm, hdr) < 0) {
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		virtio_rx_stats_updated(rxvq, rxm);

		rx_pkts[nb_rx++] = rxm;
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	if (likely(!virtqueue_full(vq))) {
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (likely(rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts,
						free_cnt) == 0)) {
			error = virtqueue_enqueue_recv_refill(vq, new_pkts,
					free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev = &rte_eth_devices[hw->port_id];
			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}

	if (likely(nb_enqueued)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}

uint16_t
virtio_recv_pkts_packed(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	struct rte_mbuf *rxm;
	uint16_t num, nb_rx;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t i, nb_enqueued;
	uint32_t hdr_size;
	struct virtio_net_hdr *hdr;

	nb_rx = 0;
	if (unlikely(hw->started == 0))
		return nb_rx;

	num = RTE_MIN(VIRTIO_MBUF_BURST_SZ, nb_pkts);
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((vq->vq_used_cons_idx + num) % DESC_PER_CACHELINE);

	num = virtqueue_dequeue_burst_rx_packed(vq, rcv_pkts, len, num);
	PMD_RX_LOG(DEBUG, "dequeue:%d", num);

	nb_enqueued = 0;
	hdr_size = hw->vtnet_hdr_size;

	for (i = 0; i < num; i++) {
		rxm = rcv_pkts[i];

		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		if (unlikely(len[i] < hdr_size + RTE_ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		rxm->port = hw->port_id;
		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;

		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		hdr = (struct virtio_net_hdr *)((char *)rxm->buf_addr +
			RTE_PKTMBUF_HEADROOM - hdr_size);

		if (hw->vlan_strip)
			rte_vlan_strip(rxm);

		if (hw->has_rx_offload && virtio_rx_offload(rxm, hdr) < 0) {
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		virtio_rx_stats_updated(rxvq, rxm);

		rx_pkts[nb_rx++] = rxm;
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	if (likely(!virtqueue_full(vq))) {
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (likely(rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts,
						free_cnt) == 0)) {
			error = virtqueue_enqueue_recv_refill_packed(vq,
					new_pkts, free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev = &rte_eth_devices[hw->port_id];
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


uint16_t
virtio_recv_pkts_inorder(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	struct rte_mbuf *rxm;
	struct rte_mbuf *prev = NULL;
	uint16_t nb_used, num, nb_rx;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t nb_enqueued;
	uint32_t seg_num;
	uint32_t seg_res;
	uint32_t hdr_size;
	int32_t i;

	nb_rx = 0;
	if (unlikely(hw->started == 0))
		return nb_rx;

	nb_used = virtqueue_nused(vq);
	nb_used = RTE_MIN(nb_used, nb_pkts);
	nb_used = RTE_MIN(nb_used, VIRTIO_MBUF_BURST_SZ);

	PMD_RX_LOG(DEBUG, "used:%d", nb_used);

	nb_enqueued = 0;
	seg_num = 1;
	seg_res = 0;
	hdr_size = hw->vtnet_hdr_size;

	num = virtqueue_dequeue_rx_inorder(vq, rcv_pkts, len, nb_used);

	for (i = 0; i < num; i++) {
		struct virtio_net_hdr_mrg_rxbuf *header;

		PMD_RX_LOG(DEBUG, "dequeue:%d", num);
		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		rxm = rcv_pkts[i];

		if (unlikely(len[i] < hdr_size + RTE_ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf_inorder(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		header = (struct virtio_net_hdr_mrg_rxbuf *)
			 ((char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM
			 - hdr_size);

		if (virtio_with_feature(hw, VIRTIO_NET_F_MRG_RXBUF)) {
			seg_num = header->num_buffers;
			if (seg_num == 0)
				seg_num = 1;
		} else {
			seg_num = 1;
		}

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->nb_segs = seg_num;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		rxm->port = hw->port_id;

		rx_pkts[nb_rx] = rxm;
		prev = rxm;

		if (vq->hw->has_rx_offload &&
				virtio_rx_offload(rxm, &header->hdr) < 0) {
			virtio_discard_rxbuf_inorder(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		if (hw->vlan_strip)
			rte_vlan_strip(rx_pkts[nb_rx]);

		seg_res = seg_num - 1;

		/* Merge remaining segments */
		while (seg_res != 0 && i < (num - 1)) {
			i++;

			rxm = rcv_pkts[i];
			rxm->data_off = RTE_PKTMBUF_HEADROOM - hdr_size;
			rxm->pkt_len = (uint32_t)(len[i]);
			rxm->data_len = (uint16_t)(len[i]);

			rx_pkts[nb_rx]->pkt_len += (uint32_t)(len[i]);

			prev->next = rxm;
			prev = rxm;
			seg_res -= 1;
		}

		if (!seg_res) {
			virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}

	/* Last packet still need merge segments */
	while (seg_res != 0) {
		uint16_t rcv_cnt = RTE_MIN((uint16_t)seg_res,
					VIRTIO_MBUF_BURST_SZ);

		if (likely(virtqueue_nused(vq) >= rcv_cnt)) {
			num = virtqueue_dequeue_rx_inorder(vq, rcv_pkts, len,
							   rcv_cnt);
			uint16_t extra_idx = 0;

			rcv_cnt = num;
			while (extra_idx < rcv_cnt) {
				rxm = rcv_pkts[extra_idx];
				rxm->data_off =
					RTE_PKTMBUF_HEADROOM - hdr_size;
				rxm->pkt_len = (uint32_t)(len[extra_idx]);
				rxm->data_len = (uint16_t)(len[extra_idx]);
				prev->next = rxm;
				prev = rxm;
				rx_pkts[nb_rx]->pkt_len += len[extra_idx];
				extra_idx += 1;
			};
			seg_res -= rcv_cnt;

			if (!seg_res) {
				virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
				nb_rx++;
			}
		} else {
			PMD_RX_LOG(ERR,
					"No enough segments for packet.");
			rte_pktmbuf_free(rx_pkts[nb_rx]);
			rxvq->stats.errors++;
			break;
		}
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */

	if (likely(!virtqueue_full(vq))) {
		/* free_cnt may include mrg descs */
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt)) {
			error = virtqueue_enqueue_refill_inorder(vq, new_pkts,
					free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev = &rte_eth_devices[hw->port_id];
			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}

	if (likely(nb_enqueued)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}

uint16_t
virtio_recv_mergeable_pkts(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	struct rte_mbuf *rxm;
	struct rte_mbuf *prev = NULL;
	uint16_t nb_used, num, nb_rx = 0;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	int error;
	uint32_t nb_enqueued = 0;
	uint32_t seg_num = 0;
	uint32_t seg_res = 0;
	uint32_t hdr_size = hw->vtnet_hdr_size;
	int32_t i;

	if (unlikely(hw->started == 0))
		return nb_rx;

	nb_used = virtqueue_nused(vq);

	PMD_RX_LOG(DEBUG, "used:%d", nb_used);

	num = likely(nb_used <= nb_pkts) ? nb_used : nb_pkts;
	if (unlikely(num > VIRTIO_MBUF_BURST_SZ))
		num = VIRTIO_MBUF_BURST_SZ;
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((vq->vq_used_cons_idx + num) %
				DESC_PER_CACHELINE);


	num = virtqueue_dequeue_burst_rx(vq, rcv_pkts, len, num);

	for (i = 0; i < num; i++) {
		struct virtio_net_hdr_mrg_rxbuf *header;

		PMD_RX_LOG(DEBUG, "dequeue:%d", num);
		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		rxm = rcv_pkts[i];

		if (unlikely(len[i] < hdr_size + RTE_ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		header = (struct virtio_net_hdr_mrg_rxbuf *)
			 ((char *)rxm->buf_addr + RTE_PKTMBUF_HEADROOM
			 - hdr_size);
		seg_num = header->num_buffers;
		if (seg_num == 0)
			seg_num = 1;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->nb_segs = seg_num;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		rxm->port = hw->port_id;

		rx_pkts[nb_rx] = rxm;
		prev = rxm;

		if (hw->has_rx_offload &&
				virtio_rx_offload(rxm, &header->hdr) < 0) {
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		if (hw->vlan_strip)
			rte_vlan_strip(rx_pkts[nb_rx]);

		seg_res = seg_num - 1;

		/* Merge remaining segments */
		while (seg_res != 0 && i < (num - 1)) {
			i++;

			rxm = rcv_pkts[i];
			rxm->data_off = RTE_PKTMBUF_HEADROOM - hdr_size;
			rxm->pkt_len = (uint32_t)(len[i]);
			rxm->data_len = (uint16_t)(len[i]);

			rx_pkts[nb_rx]->pkt_len += (uint32_t)(len[i]);

			prev->next = rxm;
			prev = rxm;
			seg_res -= 1;
		}

		if (!seg_res) {
			virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}

	/* Last packet still need merge segments */
	while (seg_res != 0) {
		uint16_t rcv_cnt = RTE_MIN((uint16_t)seg_res,
					VIRTIO_MBUF_BURST_SZ);

		if (likely(virtqueue_nused(vq) >= rcv_cnt)) {
			num = virtqueue_dequeue_burst_rx(vq, rcv_pkts, len,
							   rcv_cnt);
			uint16_t extra_idx = 0;

			rcv_cnt = num;
			while (extra_idx < rcv_cnt) {
				rxm = rcv_pkts[extra_idx];
				rxm->data_off =
					RTE_PKTMBUF_HEADROOM - hdr_size;
				rxm->pkt_len = (uint32_t)(len[extra_idx]);
				rxm->data_len = (uint16_t)(len[extra_idx]);
				prev->next = rxm;
				prev = rxm;
				rx_pkts[nb_rx]->pkt_len += len[extra_idx];
				extra_idx += 1;
			};
			seg_res -= rcv_cnt;

			if (!seg_res) {
				virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
				nb_rx++;
			}
		} else {
			PMD_RX_LOG(ERR,
					"No enough segments for packet.");
			rte_pktmbuf_free(rx_pkts[nb_rx]);
			rxvq->stats.errors++;
			break;
		}
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	if (likely(!virtqueue_full(vq))) {
		/* free_cnt may include mrg descs */
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt)) {
			error = virtqueue_enqueue_recv_refill(vq, new_pkts,
					free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev = &rte_eth_devices[hw->port_id];
			dev->data->rx_mbuf_alloc_failed += free_cnt;
		}
	}

	if (likely(nb_enqueued)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_RX_LOG(DEBUG, "Notified");
		}
	}

	return nb_rx;
}

uint16_t
virtio_recv_mergeable_pkts_packed(void *rx_queue,
			struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_rx *rxvq = rx_queue;
	struct virtqueue *vq = virtnet_rxq_to_vq(rxvq);
	struct virtio_hw *hw = vq->hw;
	struct rte_mbuf *rxm;
	struct rte_mbuf *prev = NULL;
	uint16_t num, nb_rx = 0;
	uint32_t len[VIRTIO_MBUF_BURST_SZ];
	struct rte_mbuf *rcv_pkts[VIRTIO_MBUF_BURST_SZ];
	uint32_t nb_enqueued = 0;
	uint32_t seg_num = 0;
	uint32_t seg_res = 0;
	uint32_t hdr_size = hw->vtnet_hdr_size;
	int32_t i;
	int error;

	if (unlikely(hw->started == 0))
		return nb_rx;


	num = nb_pkts;
	if (unlikely(num > VIRTIO_MBUF_BURST_SZ))
		num = VIRTIO_MBUF_BURST_SZ;
	if (likely(num > DESC_PER_CACHELINE))
		num = num - ((vq->vq_used_cons_idx + num) % DESC_PER_CACHELINE);

	num = virtqueue_dequeue_burst_rx_packed(vq, rcv_pkts, len, num);

	for (i = 0; i < num; i++) {
		struct virtio_net_hdr_mrg_rxbuf *header;

		PMD_RX_LOG(DEBUG, "dequeue:%d", num);
		PMD_RX_LOG(DEBUG, "packet len:%d", len[i]);

		rxm = rcv_pkts[i];

		if (unlikely(len[i] < hdr_size + RTE_ETHER_HDR_LEN)) {
			PMD_RX_LOG(ERR, "Packet drop");
			nb_enqueued++;
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		header = (struct virtio_net_hdr_mrg_rxbuf *)((char *)
			  rxm->buf_addr + RTE_PKTMBUF_HEADROOM - hdr_size);
		seg_num = header->num_buffers;

		if (seg_num == 0)
			seg_num = 1;

		rxm->data_off = RTE_PKTMBUF_HEADROOM;
		rxm->nb_segs = seg_num;
		rxm->ol_flags = 0;
		rxm->vlan_tci = 0;
		rxm->pkt_len = (uint32_t)(len[i] - hdr_size);
		rxm->data_len = (uint16_t)(len[i] - hdr_size);

		rxm->port = hw->port_id;
		rx_pkts[nb_rx] = rxm;
		prev = rxm;

		if (hw->has_rx_offload &&
				virtio_rx_offload(rxm, &header->hdr) < 0) {
			virtio_discard_rxbuf(vq, rxm);
			rxvq->stats.errors++;
			continue;
		}

		if (hw->vlan_strip)
			rte_vlan_strip(rx_pkts[nb_rx]);

		seg_res = seg_num - 1;

		/* Merge remaining segments */
		while (seg_res != 0 && i < (num - 1)) {
			i++;

			rxm = rcv_pkts[i];
			rxm->data_off = RTE_PKTMBUF_HEADROOM - hdr_size;
			rxm->pkt_len = (uint32_t)(len[i]);
			rxm->data_len = (uint16_t)(len[i]);

			rx_pkts[nb_rx]->pkt_len += (uint32_t)(len[i]);

			prev->next = rxm;
			prev = rxm;
			seg_res -= 1;
		}

		if (!seg_res) {
			virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}

	/* Last packet still need merge segments */
	while (seg_res != 0) {
		uint16_t rcv_cnt = RTE_MIN((uint16_t)seg_res,
					VIRTIO_MBUF_BURST_SZ);
		uint16_t extra_idx = 0;

		rcv_cnt = virtqueue_dequeue_burst_rx_packed(vq, rcv_pkts,
				len, rcv_cnt);
		if (unlikely(rcv_cnt == 0)) {
			PMD_RX_LOG(ERR, "No enough segments for packet.");
			rte_pktmbuf_free(rx_pkts[nb_rx]);
			rxvq->stats.errors++;
			break;
		}

		while (extra_idx < rcv_cnt) {
			rxm = rcv_pkts[extra_idx];

			rxm->data_off = RTE_PKTMBUF_HEADROOM - hdr_size;
			rxm->pkt_len = (uint32_t)(len[extra_idx]);
			rxm->data_len = (uint16_t)(len[extra_idx]);

			prev->next = rxm;
			prev = rxm;
			rx_pkts[nb_rx]->pkt_len += len[extra_idx];
			extra_idx += 1;
		}
		seg_res -= rcv_cnt;
		if (!seg_res) {
			virtio_rx_stats_updated(rxvq, rx_pkts[nb_rx]);
			nb_rx++;
		}
	}

	rxvq->stats.packets += nb_rx;

	/* Allocate new mbuf for the used descriptor */
	if (likely(!virtqueue_full(vq))) {
		/* free_cnt may include mrg descs */
		uint16_t free_cnt = vq->vq_free_cnt;
		struct rte_mbuf *new_pkts[free_cnt];

		if (!rte_pktmbuf_alloc_bulk(rxvq->mpool, new_pkts, free_cnt)) {
			error = virtqueue_enqueue_recv_refill_packed(vq,
					new_pkts, free_cnt);
			if (unlikely(error)) {
				for (i = 0; i < free_cnt; i++)
					rte_pktmbuf_free(new_pkts[i]);
			}
			nb_enqueued += free_cnt;
		} else {
			struct rte_eth_dev *dev = &rte_eth_devices[hw->port_id];
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

uint16_t
virtio_xmit_pkts_prepare(void *tx_queue __rte_unused, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	uint16_t nb_tx;
	int error;

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *m = tx_pkts[nb_tx];

#ifdef RTE_LIBRTE_ETHDEV_DEBUG
		error = rte_validate_tx_offload(m);
		if (unlikely(error)) {
			rte_errno = -error;
			break;
		}
#endif

		/* Do VLAN tag insertion */
		if (unlikely(m->ol_flags & RTE_MBUF_F_TX_VLAN)) {
			error = rte_vlan_insert(&m);
			/* rte_vlan_insert() may change pointer
			 * even in the case of failure
			 */
			tx_pkts[nb_tx] = m;

			if (unlikely(error)) {
				rte_errno = -error;
				break;
			}
		}

		error = rte_net_intel_cksum_prepare(m);
		if (unlikely(error)) {
			rte_errno = -error;
			break;
		}

		if (m->ol_flags & RTE_MBUF_F_TX_TCP_SEG)
			virtio_tso_fix_cksum(m);
	}

	return nb_tx;
}

uint16_t
virtio_xmit_pkts_packed(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t nb_tx = 0;
	bool in_order = virtio_with_feature(hw, VIRTIO_F_IN_ORDER);

	if (unlikely(hw->started == 0 && tx_pkts != hw->inject_pkts))
		return nb_tx;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);

	if (nb_pkts > vq->vq_free_cnt)
		virtio_xmit_cleanup_packed(vq, nb_pkts - vq->vq_free_cnt,
					   in_order);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int can_push = 0, use_indirect = 0, slots, need;

		/* optimize ring usage */
		if ((virtio_with_feature(hw, VIRTIO_F_ANY_LAYOUT) ||
		      virtio_with_feature(hw, VIRTIO_F_VERSION_1)) &&
		    rte_mbuf_refcnt_read(txm) == 1 &&
		    RTE_MBUF_DIRECT(txm) &&
		    txm->nb_segs == 1 &&
		    rte_pktmbuf_headroom(txm) >= hdr_size &&
		    rte_is_aligned(rte_pktmbuf_mtod(txm, char *),
			   __alignof__(struct virtio_net_hdr_mrg_rxbuf)))
			can_push = 1;
		else if (virtio_with_feature(hw, VIRTIO_RING_F_INDIRECT_DESC) &&
			 txm->nb_segs < VIRTIO_MAX_TX_INDIRECT)
			use_indirect = 1;
		/* How many main ring entries are needed to this Tx?
		 * indirect   => 1
		 * any_layout => number of segments
		 * default    => number of segments + 1
		 */
		slots = use_indirect ? 1 : (txm->nb_segs + !can_push);
		need = slots - vq->vq_free_cnt;

		/* Positive value indicates it need free vring descriptors */
		if (unlikely(need > 0)) {
			virtio_xmit_cleanup_packed(vq, need, in_order);
			need = slots - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
					   "No free tx descriptors to transmit");
				break;
			}
		}

		/* Enqueue Packet buffers */
		if (can_push)
			virtqueue_enqueue_xmit_packed_fast(txvq, txm, in_order);
		else
			virtqueue_enqueue_xmit_packed(txvq, txm, slots,
						      use_indirect, 0,
						      in_order);

		virtio_update_packet_stats(&txvq->stats, txm);
	}

	txvq->stats.packets += nb_tx;

	if (likely(nb_tx)) {
		if (unlikely(virtqueue_kick_prepare_packed(vq))) {
			virtqueue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}

	return nb_tx;
}

uint16_t
virtio_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t nb_used, nb_tx = 0;

	if (unlikely(hw->started == 0 && tx_pkts != hw->inject_pkts))
		return nb_tx;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);

	nb_used = virtqueue_nused(vq);

	if (likely(nb_used > vq->vq_nentries - vq->vq_free_thresh))
		virtio_xmit_cleanup(vq, nb_used);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int can_push = 0, use_indirect = 0, slots, need;

		/* optimize ring usage */
		if ((virtio_with_feature(hw, VIRTIO_F_ANY_LAYOUT) ||
		      virtio_with_feature(hw, VIRTIO_F_VERSION_1)) &&
		    rte_mbuf_refcnt_read(txm) == 1 &&
		    RTE_MBUF_DIRECT(txm) &&
		    txm->nb_segs == 1 &&
		    rte_pktmbuf_headroom(txm) >= hdr_size &&
		    rte_is_aligned(rte_pktmbuf_mtod(txm, char *),
				   __alignof__(struct virtio_net_hdr_mrg_rxbuf)))
			can_push = 1;
		else if (virtio_with_feature(hw, VIRTIO_RING_F_INDIRECT_DESC) &&
			 txm->nb_segs < VIRTIO_MAX_TX_INDIRECT)
			use_indirect = 1;

		/* How many main ring entries are needed to this Tx?
		 * any_layout => number of segments
		 * indirect   => 1
		 * default    => number of segments + 1
		 */
		slots = use_indirect ? 1 : (txm->nb_segs + !can_push);
		need = slots - vq->vq_free_cnt;

		/* Positive value indicates it need free vring descriptors */
		if (unlikely(need > 0)) {
			nb_used = virtqueue_nused(vq);

			need = RTE_MIN(need, (int)nb_used);

			virtio_xmit_cleanup(vq, need);
			need = slots - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
					   "No free tx descriptors to transmit");
				break;
			}
		}

		/* Enqueue Packet buffers */
		virtqueue_enqueue_xmit(txvq, txm, slots, use_indirect,
			can_push, 0);

		virtio_update_packet_stats(&txvq->stats, txm);
	}

	txvq->stats.packets += nb_tx;

	if (likely(nb_tx)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}

	return nb_tx;
}

static __rte_always_inline int
virtio_xmit_try_cleanup_inorder(struct virtqueue *vq, uint16_t need)
{
	uint16_t nb_used, nb_clean, nb_descs;

	nb_descs = vq->vq_free_cnt + need;
	nb_used = virtqueue_nused(vq);
	nb_clean = RTE_MIN(need, (int)nb_used);

	virtio_xmit_cleanup_inorder(vq, nb_clean);

	return nb_descs - vq->vq_free_cnt;
}

uint16_t
virtio_xmit_pkts_inorder(void *tx_queue,
			struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts)
{
	struct virtnet_tx *txvq = tx_queue;
	struct virtqueue *vq = virtnet_txq_to_vq(txvq);
	struct virtio_hw *hw = vq->hw;
	uint16_t hdr_size = hw->vtnet_hdr_size;
	uint16_t nb_used, nb_tx = 0, nb_inorder_pkts = 0;
	struct rte_mbuf *inorder_pkts[nb_pkts];
	int need;

	if (unlikely(hw->started == 0 && tx_pkts != hw->inject_pkts))
		return nb_tx;

	if (unlikely(nb_pkts < 1))
		return nb_pkts;

	VIRTQUEUE_DUMP(vq);
	PMD_TX_LOG(DEBUG, "%d packets to xmit", nb_pkts);
	nb_used = virtqueue_nused(vq);

	if (likely(nb_used > vq->vq_nentries - vq->vq_free_thresh))
		virtio_xmit_cleanup_inorder(vq, nb_used);

	for (nb_tx = 0; nb_tx < nb_pkts; nb_tx++) {
		struct rte_mbuf *txm = tx_pkts[nb_tx];
		int slots;

		/* optimize ring usage */
		if ((virtio_with_feature(hw, VIRTIO_F_ANY_LAYOUT) ||
		     virtio_with_feature(hw, VIRTIO_F_VERSION_1)) &&
		     rte_mbuf_refcnt_read(txm) == 1 &&
		     RTE_MBUF_DIRECT(txm) &&
		     txm->nb_segs == 1 &&
		     rte_pktmbuf_headroom(txm) >= hdr_size &&
		     rte_is_aligned(rte_pktmbuf_mtod(txm, char *),
				__alignof__(struct virtio_net_hdr_mrg_rxbuf))) {
			inorder_pkts[nb_inorder_pkts] = txm;
			nb_inorder_pkts++;

			continue;
		}

		if (nb_inorder_pkts) {
			need = nb_inorder_pkts - vq->vq_free_cnt;
			if (unlikely(need > 0)) {
				need = virtio_xmit_try_cleanup_inorder(vq,
								       need);
				if (unlikely(need > 0)) {
					PMD_TX_LOG(ERR,
						"No free tx descriptors to "
						"transmit");
					break;
				}
			}
			virtqueue_enqueue_xmit_inorder(txvq, inorder_pkts,
							nb_inorder_pkts);
			nb_inorder_pkts = 0;
		}

		slots = txm->nb_segs + 1;
		need = slots - vq->vq_free_cnt;
		if (unlikely(need > 0)) {
			need = virtio_xmit_try_cleanup_inorder(vq, slots);

			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
					"No free tx descriptors to transmit");
				break;
			}
		}
		/* Enqueue Packet buffers */
		virtqueue_enqueue_xmit(txvq, txm, slots, 0, 0, 1);

		virtio_update_packet_stats(&txvq->stats, txm);
	}

	/* Transmit all inorder packets */
	if (nb_inorder_pkts) {
		need = nb_inorder_pkts - vq->vq_free_cnt;
		if (unlikely(need > 0)) {
			need = virtio_xmit_try_cleanup_inorder(vq,
								  need);
			if (unlikely(need > 0)) {
				PMD_TX_LOG(ERR,
					"No free tx descriptors to transmit");
				nb_inorder_pkts = vq->vq_free_cnt;
				nb_tx -= need;
			}
		}

		virtqueue_enqueue_xmit_inorder(txvq, inorder_pkts,
						nb_inorder_pkts);
	}

	txvq->stats.packets += nb_tx;

	if (likely(nb_tx)) {
		vq_update_avail_idx(vq);

		if (unlikely(virtqueue_kick_prepare(vq))) {
			virtqueue_notify(vq);
			PMD_TX_LOG(DEBUG, "Notified backend after xmit");
		}
	}

	VIRTQUEUE_DUMP(vq);

	return nb_tx;
}

__rte_weak uint16_t
virtio_recv_pkts_packed_vec(void *rx_queue __rte_unused,
			    struct rte_mbuf **rx_pkts __rte_unused,
			    uint16_t nb_pkts __rte_unused)
{
	return 0;
}

__rte_weak uint16_t
virtio_xmit_pkts_packed_vec(void *tx_queue __rte_unused,
			    struct rte_mbuf **tx_pkts __rte_unused,
			    uint16_t nb_pkts __rte_unused)
{
	return 0;
}
