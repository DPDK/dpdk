/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include <stdint.h>

#include <rte_mbuf.h>

#include "virtqueue.h"
#include "virtio_logs.h"
#include "virtio.h"
#include "virtio_rxtx_simple.h"

/*
 * Two types of mbuf to be cleaned:
 * 1) mbuf that has been consumed by backend but not used by virtio.
 * 2) mbuf that hasn't been consumed by backend.
 */
struct rte_mbuf *
virtqueue_detach_unused(struct virtqueue *vq)
{
	struct rte_mbuf *cookie;
	struct virtio_hw *hw;
	uint16_t start, end;
	int type, idx;

	if (vq == NULL)
		return NULL;

	hw = vq->hw;
	type = virtio_get_queue_type(hw, vq->vq_queue_index);
	start = vq->vq_avail_idx & (vq->vq_nentries - 1);
	end = (vq->vq_avail_idx + vq->vq_free_cnt) & (vq->vq_nentries - 1);

	for (idx = 0; idx < vq->vq_nentries; idx++) {
		if (hw->use_vec_rx && !virtio_with_packed_queue(hw) &&
		    type == VTNET_RQ) {
			if (start <= end && idx >= start && idx < end)
				continue;
			if (start > end && (idx >= start || idx < end))
				continue;
			cookie = vq->sw_ring[idx];
			if (cookie != NULL) {
				vq->sw_ring[idx] = NULL;
				return cookie;
			}
		} else {
			cookie = vq->vq_descx[idx].cookie;
			if (cookie != NULL) {
				vq->vq_descx[idx].cookie = NULL;
				return cookie;
			}
		}
	}

	return NULL;
}

/* Flush used descs */
static void
virtqueue_rxvq_flush_packed(struct virtqueue *vq)
{
	struct vq_desc_extra *dxp;
	uint16_t i;

	struct vring_packed_desc *descs = vq->vq_packed.ring.desc;
	int cnt = 0;

	i = vq->vq_used_cons_idx;
	while (desc_is_used(&descs[i], vq) && cnt++ < vq->vq_nentries) {
		dxp = &vq->vq_descx[descs[i].id];
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
		vq->vq_free_cnt++;
		vq->vq_used_cons_idx++;
		if (vq->vq_used_cons_idx >= vq->vq_nentries) {
			vq->vq_used_cons_idx -= vq->vq_nentries;
			vq->vq_packed.used_wrap_counter ^= 1;
		}
		i = vq->vq_used_cons_idx;
	}
}

/* Flush the elements in the used ring. */
static void
virtqueue_rxvq_flush_split(struct virtqueue *vq)
{
	struct virtnet_rx *rxq = &vq->rxq;
	struct virtio_hw *hw = vq->hw;
	struct vring_used_elem *uep;
	struct vq_desc_extra *dxp;
	uint16_t used_idx, desc_idx;
	uint16_t nb_used, i;

	nb_used = virtqueue_nused(vq);

	for (i = 0; i < nb_used; i++) {
		used_idx = vq->vq_used_cons_idx & (vq->vq_nentries - 1);
		uep = &vq->vq_split.ring.used->ring[used_idx];
		if (hw->use_vec_rx) {
			desc_idx = used_idx;
			rte_pktmbuf_free(vq->sw_ring[desc_idx]);
			vq->vq_free_cnt++;
		} else if (hw->use_inorder_rx) {
			desc_idx = (uint16_t)uep->id;
			dxp = &vq->vq_descx[desc_idx];
			if (dxp->cookie != NULL) {
				rte_pktmbuf_free(dxp->cookie);
				dxp->cookie = NULL;
			}
			vq_ring_free_inorder(vq, desc_idx, 1);
		} else {
			desc_idx = (uint16_t)uep->id;
			dxp = &vq->vq_descx[desc_idx];
			if (dxp->cookie != NULL) {
				rte_pktmbuf_free(dxp->cookie);
				dxp->cookie = NULL;
			}
			vq_ring_free_chain(vq, desc_idx);
		}
		vq->vq_used_cons_idx++;
	}

	if (hw->use_vec_rx) {
		while (vq->vq_free_cnt >= RTE_VIRTIO_VPMD_RX_REARM_THRESH) {
			virtio_rxq_rearm_vec(rxq);
			if (virtqueue_kick_prepare(vq))
				virtqueue_notify(vq);
		}
	}
}

/* Flush the elements in the used ring. */
void
virtqueue_rxvq_flush(struct virtqueue *vq)
{
	struct virtio_hw *hw = vq->hw;

	if (virtio_with_packed_queue(hw))
		virtqueue_rxvq_flush_packed(vq);
	else
		virtqueue_rxvq_flush_split(vq);
}

int
virtqueue_rxvq_reset_packed(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	struct vq_desc_extra *dxp;
	struct virtnet_rx *rxvq;
	uint16_t desc_idx;

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;

	vq->vq_packed.used_wrap_counter = 1;
	vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
	vq->vq_packed.event_flags_shadow = 0;
	vq->vq_packed.cached_flags |= VRING_DESC_F_WRITE;

	rxvq = &vq->rxq;
	memset(rxvq->mz->addr, 0, rxvq->mz->len);

	for (desc_idx = 0; desc_idx < vq->vq_nentries; desc_idx++) {
		dxp = &vq->vq_descx[desc_idx];
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
	}

	vring_desc_init_packed(vq, size);

	virtqueue_disable_intr(vq);
	return 0;
}

int
virtqueue_txvq_reset_packed(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	struct vq_desc_extra *dxp;
	struct virtnet_tx *txvq;
	uint16_t desc_idx;
	struct virtio_tx_region *txr;
	struct vring_packed_desc *start_dp;

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;

	vq->vq_packed.used_wrap_counter = 1;
	vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
	vq->vq_packed.event_flags_shadow = 0;

	txvq = &vq->txq;
	txr = txvq->virtio_net_hdr_mz->addr;
	memset(txvq->mz->addr, 0, txvq->mz->len);
	memset(txvq->virtio_net_hdr_mz->addr, 0,
		txvq->virtio_net_hdr_mz->len);

	for (desc_idx = 0; desc_idx < vq->vq_nentries; desc_idx++) {
		dxp = &vq->vq_descx[desc_idx];
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}

		if (virtio_with_feature(vq->hw, VIRTIO_RING_F_INDIRECT_DESC)) {
			/* first indirect descriptor is always the tx header */
			start_dp = txr[desc_idx].tx_packed_indir;
			vring_desc_init_indirect_packed(start_dp,
							RTE_DIM(txr[desc_idx].tx_packed_indir));
			start_dp->addr = txvq->virtio_net_hdr_mem
					 + desc_idx * sizeof(*txr)
					 + offsetof(struct virtio_tx_region, tx_hdr);
			start_dp->len = vq->hw->vtnet_hdr_size;
		}
	}

	vring_desc_init_packed(vq, size);

	virtqueue_disable_intr(vq);
	return 0;
}
