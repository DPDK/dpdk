/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>
#include <linux/virtio_net.h>

#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_vhost.h>

#include "main.h"

/*
 * A very simple vhost-user net driver implementation, without
 * any extra features being enabled, such as TSO and mrg-Rx.
 */

void
vs_vhost_net_setup(struct vhost_dev *dev)
{
	uint16_t i;
	int vid = dev->vid;
	struct vhost_queue *queue;

	RTE_LOG(INFO, VHOST_CONFIG,
		"setting builtin vhost-user net driver\n");

	rte_vhost_get_negotiated_features(vid, &dev->features);
	if (dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF))
		dev->hdr_len = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	else
		dev->hdr_len = sizeof(struct virtio_net_hdr);

	rte_vhost_get_mem_table(vid, &dev->mem);

	dev->nr_vrings = rte_vhost_get_vring_num(vid);
	for (i = 0; i < dev->nr_vrings; i++) {
		queue = &dev->queues[i];

		queue->last_used_idx  = 0;
		queue->last_avail_idx = 0;
		rte_vhost_get_vhost_vring(vid, i, &queue->vr);
	}
}

void
vs_vhost_net_remove(struct vhost_dev *dev)
{
	free(dev->mem);
}

static __rte_always_inline int
enqueue_pkt(struct vhost_dev *dev, struct rte_vhost_vring *vr,
	    struct rte_mbuf *m, uint16_t desc_idx)
{
	uint32_t desc_avail, desc_offset;
	uint64_t desc_chunck_len;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct vring_desc *desc;
	uint64_t desc_addr, desc_gaddr;
	struct virtio_net_hdr virtio_hdr = {0, 0, 0, 0, 0, 0};
	/* A counter to avoid desc dead loop chain */
	uint16_t nr_desc = 1;

	desc = &vr->desc[desc_idx];
	desc_chunck_len = desc->len;
	desc_gaddr = desc->addr;
	desc_addr = rte_vhost_va_from_guest_pa(
			dev->mem, desc_gaddr, &desc_chunck_len);
	/*
	 * Checking of 'desc_addr' placed outside of 'unlikely' macro to avoid
	 * performance issue with some versions of gcc (4.8.4 and 5.3.0) which
	 * otherwise stores offset on the stack instead of in a register.
	 */
	if (unlikely(desc->len < dev->hdr_len) || !desc_addr)
		return -1;

	rte_prefetch0((void *)(uintptr_t)desc_addr);

	/* write virtio-net header */
	if (likely(desc_chunck_len >= dev->hdr_len)) {
		*(struct virtio_net_hdr *)(uintptr_t)desc_addr = virtio_hdr;
		desc_offset = dev->hdr_len;
	} else {
		uint64_t len;
		uint64_t remain = dev->hdr_len;
		uint64_t src = (uint64_t)(uintptr_t)&virtio_hdr, dst;
		uint64_t guest_addr = desc_gaddr;

		while (remain) {
			len = remain;
			dst = rte_vhost_va_from_guest_pa(dev->mem,
					guest_addr, &len);
			if (unlikely(!dst || !len))
				return -1;

			rte_memcpy((void *)(uintptr_t)dst,
					(void *)(uintptr_t)src,
					len);

			remain -= len;
			guest_addr += len;
			src += len;
		}

		desc_chunck_len = desc->len - dev->hdr_len;
		desc_gaddr += dev->hdr_len;
		desc_addr = rte_vhost_va_from_guest_pa(
				dev->mem, desc_gaddr,
				&desc_chunck_len);
		if (unlikely(!desc_addr))
			return -1;

		desc_offset = 0;
	}

	desc_avail  = desc->len - dev->hdr_len;

	mbuf_avail  = rte_pktmbuf_data_len(m);
	mbuf_offset = 0;
	while (mbuf_avail != 0 || m->next != NULL) {
		/* done with current mbuf, fetch next */
		if (mbuf_avail == 0) {
			m = m->next;

			mbuf_offset = 0;
			mbuf_avail  = rte_pktmbuf_data_len(m);
		}

		/* done with current desc buf, fetch next */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0) {
				/* Room in vring buffer is not enough */
				return -1;
			}
			if (unlikely(desc->next >= vr->size ||
				     ++nr_desc > vr->size))
				return -1;

			desc = &vr->desc[desc->next];
			desc_chunck_len = desc->len;
			desc_gaddr = desc->addr;
			desc_addr = rte_vhost_va_from_guest_pa(
					dev->mem, desc_gaddr, &desc_chunck_len);
			if (unlikely(!desc_addr))
				return -1;

			desc_offset = 0;
			desc_avail  = desc->len;
		} else if (unlikely(desc_chunck_len == 0)) {
			desc_chunck_len = desc_avail;
			desc_gaddr += desc_offset;
			desc_addr = rte_vhost_va_from_guest_pa(dev->mem,
					desc_gaddr,
					&desc_chunck_len);
			if (unlikely(!desc_addr))
				return -1;

			desc_offset = 0;
		}

		cpy_len = RTE_MIN(desc_chunck_len, mbuf_avail);
		rte_memcpy((void *)((uintptr_t)(desc_addr + desc_offset)),
			rte_pktmbuf_mtod_offset(m, void *, mbuf_offset),
			cpy_len);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
		desc_chunck_len -= cpy_len;
	}

	return 0;
}

uint16_t
vs_enqueue_pkts(struct vhost_dev *dev, uint16_t queue_id,
		struct rte_mbuf **pkts, uint32_t count)
{
	struct vhost_queue *queue;
	struct rte_vhost_vring *vr;
	uint16_t avail_idx, free_entries, start_idx;
	uint16_t desc_indexes[MAX_PKT_BURST];
	uint16_t used_idx;
	uint32_t i;

	queue = &dev->queues[queue_id];
	vr    = &queue->vr;

	avail_idx = *((volatile uint16_t *)&vr->avail->idx);
	start_idx = queue->last_used_idx;
	free_entries = avail_idx - start_idx;
	count = RTE_MIN(count, free_entries);
	count = RTE_MIN(count, (uint32_t)MAX_PKT_BURST);
	if (count == 0)
		return 0;

	/* Retrieve all of the desc indexes first to avoid caching issues. */
	rte_prefetch0(&vr->avail->ring[start_idx & (vr->size - 1)]);
	for (i = 0; i < count; i++) {
		used_idx = (start_idx + i) & (vr->size - 1);
		desc_indexes[i] = vr->avail->ring[used_idx];
		vr->used->ring[used_idx].id = desc_indexes[i];
		vr->used->ring[used_idx].len = pkts[i]->pkt_len +
					       dev->hdr_len;
	}

	rte_prefetch0(&vr->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		uint16_t desc_idx = desc_indexes[i];
		int err;

		err = enqueue_pkt(dev, vr, pkts[i], desc_idx);
		if (unlikely(err)) {
			used_idx = (start_idx + i) & (vr->size - 1);
			vr->used->ring[used_idx].len = dev->hdr_len;
		}

		if (i + 1 < count)
			rte_prefetch0(&vr->desc[desc_indexes[i+1]]);
	}

	rte_smp_wmb();

	*(volatile uint16_t *)&vr->used->idx += count;
	queue->last_used_idx += count;

	rte_vhost_vring_call(dev->vid, queue_id);

	return count;
}

static __rte_always_inline int
dequeue_pkt(struct vhost_dev *dev, struct rte_vhost_vring *vr,
	    struct rte_mbuf *m, uint16_t desc_idx,
	    struct rte_mempool *mbuf_pool)
{
	struct vring_desc *desc;
	uint64_t desc_addr, desc_gaddr;
	uint32_t desc_avail, desc_offset;
	uint64_t desc_chunck_len;
	uint32_t mbuf_avail, mbuf_offset;
	uint32_t cpy_len;
	struct rte_mbuf *cur = m, *prev = m;
	/* A counter to avoid desc dead loop chain */
	uint32_t nr_desc = 1;

	desc = &vr->desc[desc_idx];
	if (unlikely((desc->len < dev->hdr_len)) ||
			(desc->flags & VRING_DESC_F_INDIRECT))
		return -1;

	desc_chunck_len = desc->len;
	desc_gaddr = desc->addr;
	desc_addr = rte_vhost_va_from_guest_pa(
			dev->mem, desc_gaddr, &desc_chunck_len);
	if (unlikely(!desc_addr))
		return -1;

	/*
	 * We don't support ANY_LAYOUT, neither VERSION_1, meaning
	 * a Tx packet from guest must have 2 desc buffers at least:
	 * the first for storing the header and the others for
	 * storing the data.
	 *
	 * And since we don't support TSO, we could simply skip the
	 * header.
	 */
	desc = &vr->desc[desc->next];
	desc_chunck_len = desc->len;
	desc_gaddr = desc->addr;
	desc_addr = rte_vhost_va_from_guest_pa(
			dev->mem, desc_gaddr, &desc_chunck_len);
	if (unlikely(!desc_addr))
		return -1;
	rte_prefetch0((void *)(uintptr_t)desc_addr);

	desc_offset = 0;
	desc_avail  = desc->len;
	nr_desc    += 1;

	mbuf_offset = 0;
	mbuf_avail  = m->buf_len - RTE_PKTMBUF_HEADROOM;
	while (1) {
		cpy_len = RTE_MIN(desc_chunck_len, mbuf_avail);
		rte_memcpy(rte_pktmbuf_mtod_offset(cur, void *,
						   mbuf_offset),
			(void *)((uintptr_t)(desc_addr + desc_offset)),
			cpy_len);

		mbuf_avail  -= cpy_len;
		mbuf_offset += cpy_len;
		desc_avail  -= cpy_len;
		desc_offset += cpy_len;
		desc_chunck_len -= cpy_len;

		/* This desc reaches to its end, get the next one */
		if (desc_avail == 0) {
			if ((desc->flags & VRING_DESC_F_NEXT) == 0)
				break;

			if (unlikely(desc->next >= vr->size ||
				     ++nr_desc > vr->size))
				return -1;
			desc = &vr->desc[desc->next];

			desc_chunck_len = desc->len;
			desc_gaddr = desc->addr;
			desc_addr = rte_vhost_va_from_guest_pa(
					dev->mem, desc_gaddr, &desc_chunck_len);
			if (unlikely(!desc_addr))
				return -1;
			rte_prefetch0((void *)(uintptr_t)desc_addr);

			desc_offset = 0;
			desc_avail  = desc->len;
		} else if (unlikely(desc_chunck_len == 0)) {
			desc_chunck_len = desc_avail;
			desc_gaddr += desc_offset;
			desc_addr = rte_vhost_va_from_guest_pa(dev->mem,
					desc_gaddr,
					&desc_chunck_len);
			if (unlikely(!desc_addr))
				return -1;

			desc_offset = 0;
		}

		/*
		 * This mbuf reaches to its end, get a new one
		 * to hold more data.
		 */
		if (mbuf_avail == 0) {
			cur = rte_pktmbuf_alloc(mbuf_pool);
			if (unlikely(cur == NULL)) {
				RTE_LOG(ERR, VHOST_DATA, "Failed to "
					"allocate memory for mbuf.\n");
				return -1;
			}

			prev->next = cur;
			prev->data_len = mbuf_offset;
			m->nb_segs += 1;
			m->pkt_len += mbuf_offset;
			prev = cur;

			mbuf_offset = 0;
			mbuf_avail  = cur->buf_len - RTE_PKTMBUF_HEADROOM;
		}
	}

	prev->data_len = mbuf_offset;
	m->pkt_len    += mbuf_offset;

	return 0;
}

uint16_t
vs_dequeue_pkts(struct vhost_dev *dev, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count)
{
	struct vhost_queue *queue;
	struct rte_vhost_vring *vr;
	uint32_t desc_indexes[MAX_PKT_BURST];
	uint32_t used_idx;
	uint32_t i = 0;
	uint16_t free_entries;
	uint16_t avail_idx;

	queue = &dev->queues[queue_id];
	vr    = &queue->vr;

	free_entries = *((volatile uint16_t *)&vr->avail->idx) -
			queue->last_avail_idx;
	if (free_entries == 0)
		return 0;

	/* Prefetch available and used ring */
	avail_idx = queue->last_avail_idx & (vr->size - 1);
	used_idx  = queue->last_used_idx  & (vr->size - 1);
	rte_prefetch0(&vr->avail->ring[avail_idx]);
	rte_prefetch0(&vr->used->ring[used_idx]);

	count = RTE_MIN(count, MAX_PKT_BURST);
	count = RTE_MIN(count, free_entries);

	if (unlikely(count == 0))
		return 0;

	/*
	 * Retrieve all of the head indexes first and pre-update used entries
	 * to avoid caching issues.
	 */
	for (i = 0; i < count; i++) {
		avail_idx = (queue->last_avail_idx + i) & (vr->size - 1);
		used_idx  = (queue->last_used_idx  + i) & (vr->size - 1);
		desc_indexes[i] = vr->avail->ring[avail_idx];

		vr->used->ring[used_idx].id  = desc_indexes[i];
		vr->used->ring[used_idx].len = 0;
	}

	/* Prefetch descriptor index. */
	rte_prefetch0(&vr->desc[desc_indexes[0]]);
	for (i = 0; i < count; i++) {
		int err;

		if (likely(i + 1 < count))
			rte_prefetch0(&vr->desc[desc_indexes[i + 1]]);

		pkts[i] = rte_pktmbuf_alloc(mbuf_pool);
		if (unlikely(pkts[i] == NULL)) {
			RTE_LOG(ERR, VHOST_DATA,
				"Failed to allocate memory for mbuf.\n");
			break;
		}

		err = dequeue_pkt(dev, vr, pkts[i], desc_indexes[i], mbuf_pool);
		if (unlikely(err)) {
			rte_pktmbuf_free(pkts[i]);
			break;
		}

	}

	queue->last_avail_idx += i;
	queue->last_used_idx += i;
	rte_smp_wmb();
	rte_smp_rmb();

	vr->used->idx += i;

	rte_vhost_vring_call(dev->vid, queue_id);

	return i;
}
