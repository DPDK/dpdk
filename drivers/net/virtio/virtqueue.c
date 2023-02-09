/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */
#include <stdint.h>
#include <unistd.h>

#include <rte_eal_paging.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_memzone.h>

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
			cookie = vq->rxq.sw_ring[idx];
			if (cookie != NULL) {
				vq->rxq.sw_ring[idx] = NULL;
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
			rte_pktmbuf_free(vq->rxq.sw_ring[desc_idx]);
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

static void
virtqueue_txq_indirect_header_init_packed(struct virtqueue *vq, uint32_t idx)
{
	struct virtio_tx_region *txr;
	struct vring_packed_desc *desc;
	rte_iova_t hdr_mem;

	txr = vq->txq.hdr_mz->addr;
	hdr_mem = vq->txq.hdr_mem;
	desc = txr[idx].tx_packed_indir;

	vring_desc_init_indirect_packed(desc, RTE_DIM(txr[idx].tx_packed_indir));
	desc->addr = hdr_mem + idx * sizeof(*txr) + offsetof(struct virtio_tx_region, tx_hdr);
	desc->len = vq->hw->vtnet_hdr_size;
}

static void
virtqueue_txq_indirect_header_init_split(struct virtqueue *vq, uint32_t idx)
{
	struct virtio_tx_region *txr;
	struct vring_desc *desc;
	rte_iova_t hdr_mem;

	txr = vq->txq.hdr_mz->addr;
	hdr_mem = vq->txq.hdr_mem;
	desc = txr[idx].tx_indir;

	vring_desc_init_split(desc, RTE_DIM(txr[idx].tx_indir));
	desc->addr = hdr_mem + idx * sizeof(*txr) + offsetof(struct virtio_tx_region, tx_hdr);
	desc->len = vq->hw->vtnet_hdr_size;
	desc->flags = VRING_DESC_F_NEXT;
}

void
virtqueue_txq_indirect_headers_init(struct virtqueue *vq)
{
	uint32_t i;

	if (!virtio_with_feature(vq->hw, VIRTIO_RING_F_INDIRECT_DESC))
		return;

	for (i = 0; i < vq->vq_nentries; i++)
		if (virtio_with_packed_queue(vq->hw))
			virtqueue_txq_indirect_header_init_packed(vq, i);
		else
			virtqueue_txq_indirect_header_init_split(vq, i);
}

int
virtqueue_rxvq_reset_packed(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	struct vq_desc_extra *dxp;
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

	memset(vq->mz->addr, 0, vq->mz->len);

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
	uint16_t desc_idx;

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;

	vq->vq_packed.used_wrap_counter = 1;
	vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
	vq->vq_packed.event_flags_shadow = 0;

	memset(vq->mz->addr, 0, vq->mz->len);
	memset(vq->txq.hdr_mz->addr, 0, vq->txq.hdr_mz->len);

	for (desc_idx = 0; desc_idx < vq->vq_nentries; desc_idx++) {
		dxp = &vq->vq_descx[desc_idx];
		if (dxp->cookie != NULL) {
			rte_pktmbuf_free(dxp->cookie);
			dxp->cookie = NULL;
		}
	}

	virtqueue_txq_indirect_headers_init(vq);
	vring_desc_init_packed(vq, size);
	virtqueue_disable_intr(vq);

	return 0;
}


static void
virtio_init_vring(struct virtqueue *vq)
{
	int size = vq->vq_nentries;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	PMD_INIT_FUNC_TRACE();

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);
	if (virtio_with_packed_queue(vq->hw)) {
		vring_init_packed(&vq->vq_packed.ring, ring_mem,
				  VIRTIO_VRING_ALIGN, size);
		vring_desc_init_packed(vq, size);
	} else {
		struct vring *vr = &vq->vq_split.ring;

		vring_init_split(vr, ring_mem, VIRTIO_VRING_ALIGN, size);
		vring_desc_init_split(vr->desc, size);
	}
	/*
	 * Disable device(host) interrupting guest
	 */
	virtqueue_disable_intr(vq);
}

static int
virtio_alloc_queue_headers(struct virtqueue *vq, int numa_node, const char *name)
{
	char hdr_name[VIRTQUEUE_MAX_NAME_SZ];
	const struct rte_memzone **hdr_mz;
	rte_iova_t *hdr_mem;
	ssize_t size;
	int queue_type;

	queue_type = virtio_get_queue_type(vq->hw, vq->vq_queue_index);
	switch (queue_type) {
	case VTNET_TQ:
		/*
		 * For each xmit packet, allocate a virtio_net_hdr
		 * and indirect ring elements
		 */
		size = vq->vq_nentries * sizeof(struct virtio_tx_region);
		hdr_mz = &vq->txq.hdr_mz;
		hdr_mem = &vq->txq.hdr_mem;
		break;
	case VTNET_CQ:
		/* Allocate a page for control vq command, data and status */
		size = rte_mem_page_size();
		hdr_mz = &vq->cq.hdr_mz;
		hdr_mem = &vq->cq.hdr_mem;
		break;
	case VTNET_RQ:
		/* fallthrough */
	default:
		return 0;
	}

	snprintf(hdr_name, sizeof(hdr_name), "%s_hdr", name);
	*hdr_mz = rte_memzone_reserve_aligned(hdr_name, size, numa_node,
			RTE_MEMZONE_IOVA_CONTIG, RTE_CACHE_LINE_SIZE);
	if (*hdr_mz == NULL) {
		if (rte_errno == EEXIST)
			*hdr_mz = rte_memzone_lookup(hdr_name);
		if (*hdr_mz == NULL)
			return -ENOMEM;
	}

	memset((*hdr_mz)->addr, 0, size);

	if (vq->hw->use_va)
		*hdr_mem = (uintptr_t)(*hdr_mz)->addr;
	else
		*hdr_mem = (uintptr_t)(*hdr_mz)->iova;

	return 0;
}

static void
virtio_free_queue_headers(struct virtqueue *vq)
{
	const struct rte_memzone **hdr_mz;
	rte_iova_t *hdr_mem;
	int queue_type;

	queue_type = virtio_get_queue_type(vq->hw, vq->vq_queue_index);
	switch (queue_type) {
	case VTNET_TQ:
		hdr_mz = &vq->txq.hdr_mz;
		hdr_mem = &vq->txq.hdr_mem;
		break;
	case VTNET_CQ:
		hdr_mz = &vq->cq.hdr_mz;
		hdr_mem = &vq->cq.hdr_mem;
		break;
	case VTNET_RQ:
		/* fallthrough */
	default:
		return;
	}

	rte_memzone_free(*hdr_mz);
	*hdr_mz = NULL;
	*hdr_mem = 0;
}

static int
virtio_rxq_sw_ring_alloc(struct virtqueue *vq, int numa_node)
{
	void *sw_ring;
	struct rte_mbuf *mbuf;
	size_t size;

	/* SW ring is only used with vectorized datapath */
	if (!vq->hw->use_vec_rx)
		return 0;

	size = (RTE_PMD_VIRTIO_RX_MAX_BURST + vq->vq_nentries) * sizeof(vq->rxq.sw_ring[0]);

	sw_ring = rte_zmalloc_socket("sw_ring", size, RTE_CACHE_LINE_SIZE, numa_node);
	if (!sw_ring) {
		PMD_INIT_LOG(ERR, "can not allocate RX soft ring");
		return -ENOMEM;
	}

	mbuf = rte_zmalloc_socket("sw_ring", sizeof(*mbuf), RTE_CACHE_LINE_SIZE, numa_node);
	if (!mbuf) {
		PMD_INIT_LOG(ERR, "can not allocate fake mbuf");
		rte_free(sw_ring);
		return -ENOMEM;
	}

	vq->rxq.sw_ring = sw_ring;
	vq->rxq.fake_mbuf = mbuf;

	return 0;
}

static void
virtio_rxq_sw_ring_free(struct virtqueue *vq)
{
	rte_free(vq->rxq.fake_mbuf);
	vq->rxq.fake_mbuf = NULL;
	rte_free(vq->rxq.sw_ring);
	vq->rxq.sw_ring = NULL;
}

struct virtqueue *
virtqueue_alloc(struct virtio_hw *hw, uint16_t index, uint16_t num, int type,
		int node, const char *name)
{
	struct virtqueue *vq;
	const struct rte_memzone *mz;
	unsigned int size;

	size = sizeof(*vq) + num * sizeof(struct vq_desc_extra);
	size = RTE_ALIGN_CEIL(size, RTE_CACHE_LINE_SIZE);

	vq = rte_zmalloc_socket(name, size, RTE_CACHE_LINE_SIZE, node);
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "can not allocate vq");
		return NULL;
	}

	vq->hw = hw;
	vq->vq_queue_index = index;
	vq->vq_nentries = num;
	if (virtio_with_packed_queue(hw)) {
		vq->vq_packed.used_wrap_counter = 1;
		vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
		vq->vq_packed.event_flags_shadow = 0;
		if (type == VTNET_RQ)
			vq->vq_packed.cached_flags |= VRING_DESC_F_WRITE;
	}

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(hw, num, VIRTIO_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_VRING_ALIGN);
	PMD_INIT_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d", size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(name, vq->vq_ring_size, node,
			RTE_MEMZONE_IOVA_CONTIG, VIRTIO_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(name);
		if (mz == NULL)
			goto free_vq;
	}

	memset(mz->addr, 0, mz->len);
	vq->mz = mz;
	vq->vq_ring_virt_mem = mz->addr;

	if (hw->use_va) {
		vq->vq_ring_mem = (uintptr_t)mz->addr;
		vq->mbuf_addr_offset = offsetof(struct rte_mbuf, buf_addr);
	} else {
		vq->vq_ring_mem = mz->iova;
		vq->mbuf_addr_offset = offsetof(struct rte_mbuf, buf_iova);
	}

	PMD_INIT_LOG(DEBUG, "vq->vq_ring_mem: 0x%" PRIx64, vq->vq_ring_mem);
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_virt_mem: %p", vq->vq_ring_virt_mem);

	virtio_init_vring(vq);

	if (virtio_alloc_queue_headers(vq, node, name)) {
		PMD_INIT_LOG(ERR, "Failed to alloc queue headers");
		goto free_mz;
	}

	switch (type) {
	case VTNET_RQ:
		if (virtio_rxq_sw_ring_alloc(vq, node))
			goto free_hdr_mz;
		break;
	case VTNET_TQ:
		virtqueue_txq_indirect_headers_init(vq);
		break;
	}

	return vq;

free_hdr_mz:
	virtio_free_queue_headers(vq);
free_mz:
	rte_memzone_free(mz);
free_vq:
	rte_free(vq);

	return NULL;
}

void
virtqueue_free(struct virtqueue *vq)
{
	int type;

	type = virtio_get_queue_type(vq->hw, vq->vq_queue_index);
	switch (type) {
	case VTNET_RQ:
		virtio_rxq_sw_ring_free(vq);
		break;
	case VTNET_TQ:
	case VTNET_CQ:
		virtio_free_queue_headers(vq);
		break;
	}

	rte_memzone_free(vq->mz);
	rte_free(vq);
}
