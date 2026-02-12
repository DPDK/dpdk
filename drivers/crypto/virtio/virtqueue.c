/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.
 */

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_crypto.h>
#include <rte_malloc.h>
#include <rte_errno.h>

#include "virtio_cryptodev.h"
#include "virtqueue.h"

static inline void
virtqueue_disable_intr_packed(struct virtqueue *vq)
{
	/*
	 * Set RING_EVENT_FLAGS_DISABLE to hint host
	 * not to interrupt when it consumes packets
	 * Note: this is only considered a hint to the host
	 */
	if (vq->vq_packed.event_flags_shadow != RING_EVENT_FLAGS_DISABLE) {
		vq->vq_packed.event_flags_shadow = RING_EVENT_FLAGS_DISABLE;
		vq->vq_packed.ring.driver->desc_event_flags =
			vq->vq_packed.event_flags_shadow;
	}
}

static inline void
virtqueue_disable_intr_split(struct virtqueue *vq)
{
	/*
	 * Set VRING_AVAIL_F_NO_INTERRUPT to hint host
	 * not to interrupt when it consumes packets
	 * Note: this is only considered a hint to the host
	 */
	vq->vq_split.ring.avail->flags |= VRING_AVAIL_F_NO_INTERRUPT;
}

void
virtqueue_disable_intr(struct virtqueue *vq)
{
	if (vtpci_with_packed_queue(vq->hw))
		virtqueue_disable_intr_packed(vq);
	else
		virtqueue_disable_intr_split(vq);
}

void
virtqueue_detatch_unused(struct virtqueue *vq)
{
	struct rte_crypto_op *cop = NULL;

	int idx;

	if (vq != NULL)
		for (idx = 0; idx < vq->vq_nentries; idx++) {
			cop = vq->vq_descx[idx].crypto_op;
			if (cop) {
				if (cop->type == RTE_CRYPTO_OP_TYPE_SYMMETRIC) {
					rte_pktmbuf_free(cop->sym->m_src);
					rte_pktmbuf_free(cop->sym->m_dst);
				}

				rte_crypto_op_free(cop);
				vq->vq_descx[idx].crypto_op = NULL;
			}
		}
}

static void
virtio_init_vring(struct virtqueue *vq)
{
	uint8_t *ring_mem = vq->vq_ring_virt_mem;
	int size = vq->vq_nentries;

	PMD_INIT_FUNC_TRACE();

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct vq_desc_extra) * vq->vq_nentries);
	if (vtpci_with_packed_queue(vq->hw)) {
		vring_init_packed(&vq->vq_packed.ring, ring_mem, vq->vq_ring_mem,
				  VIRTIO_PCI_VRING_ALIGN, size);
		vring_desc_init_packed(vq, size);
	} else {
		struct vring *vr = &vq->vq_split.ring;

		vring_init_split(vr, ring_mem, vq->vq_ring_mem, VIRTIO_PCI_VRING_ALIGN, size);
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
	case VTCRYPTO_DATAQ:
		/*
		 * Op cookie for every ring element. This memory can be optimized
		 * based on descriptor requirements. For example, if a descriptor
		 * is indirect, then the cookie can be shared among all the
		 * descriptors in the chain.
		 */
		size = vq->vq_nentries * sizeof(struct virtio_crypto_op_cookie);
		hdr_mz = &vq->dq.hdr_mz;
		hdr_mem = &vq->dq.hdr_mem;
		break;
	case VTCRYPTO_CTRLQ:
		/* One control operation at a time in control queue */
		size = sizeof(struct virtio_pmd_ctrl);
		hdr_mz = &vq->cq.hdr_mz;
		hdr_mem = &vq->cq.hdr_mem;
		break;
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
	case VTCRYPTO_DATAQ:
		hdr_mz = &vq->dq.hdr_mz;
		hdr_mem = &vq->dq.hdr_mem;
		break;
	case VTCRYPTO_CTRLQ:
		hdr_mz = &vq->cq.hdr_mz;
		hdr_mem = &vq->cq.hdr_mem;
		break;
	default:
		return;
	}

	rte_memzone_free(*hdr_mz);
	*hdr_mz = NULL;
	*hdr_mem = 0;
}

struct virtqueue *
virtcrypto_queue_alloc(struct virtio_crypto_hw *hw, uint16_t index, uint16_t num,
		int node, const char *name)
{
	const struct rte_memzone *mz;
	struct virtqueue *vq;
	unsigned int size;

	size = sizeof(*vq) + num * sizeof(struct vq_desc_extra);
	size = RTE_ALIGN_CEIL(size, RTE_CACHE_LINE_SIZE);

	vq = rte_zmalloc_socket(name, size, RTE_CACHE_LINE_SIZE, node);
	if (vq == NULL) {
		PMD_INIT_LOG(ERR, "can not allocate vq");
		return NULL;
	}

	PMD_INIT_LOG(DEBUG, "vq: %p", vq);
	vq->hw = hw;
	vq->vq_queue_index = index;
	vq->vq_nentries = num;
	if (vtpci_with_packed_queue(hw)) {
		vq->vq_packed.used_wrap_counter = 1;
		vq->vq_packed.cached_flags = VRING_PACKED_DESC_F_AVAIL;
		vq->vq_packed.event_flags_shadow = 0;
	}

	/*
	 * Reserve a memzone for vring elements
	 */
	size = vring_size(hw, num, VIRTIO_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, VIRTIO_PCI_VRING_ALIGN);
	PMD_INIT_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d", size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(name, vq->vq_ring_size, node,
			RTE_MEMZONE_IOVA_CONTIG, VIRTIO_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(name);
		if (mz == NULL)
			goto free_vq;
	}

	memset(mz->addr, 0, mz->len);
	vq->mz = mz;
	vq->vq_ring_virt_mem = mz->addr;

	if (hw->use_va)
		vq->vq_ring_mem = (uintptr_t)mz->addr;
	else
		vq->vq_ring_mem = mz->iova;

	PMD_INIT_LOG(DEBUG, "vq->vq_ring_mem: 0x%" PRIx64, vq->vq_ring_mem);
	PMD_INIT_LOG(DEBUG, "vq->vq_ring_virt_mem: %p", vq->vq_ring_virt_mem);

	virtio_init_vring(vq);

	if (virtio_alloc_queue_headers(vq, node, name)) {
		PMD_INIT_LOG(ERR, "Failed to alloc queue headers");
		goto free_mz;
	}

	return vq;

free_mz:
	rte_memzone_free(mz);
free_vq:
	rte_free(vq);

	return NULL;
}

void
virtcrypto_queue_free(struct virtqueue *vq)
{
	virtio_free_queue_headers(vq);
	rte_memzone_free(vq->mz);
	rte_free(vq);
}
