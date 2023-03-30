/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 Red Hat, Inc.
 */

#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_tailq.h>

#include "iotlb.h"
#include "vhost.h"

struct vhost_iotlb_entry {
	TAILQ_ENTRY(vhost_iotlb_entry) next;
	SLIST_ENTRY(vhost_iotlb_entry) next_free;

	uint64_t iova;
	uint64_t uaddr;
	uint64_t size;
	uint8_t perm;
};

#define IOTLB_CACHE_SIZE 2048

static struct vhost_iotlb_entry *
vhost_user_iotlb_pool_get(struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node;

	rte_spinlock_lock(&vq->iotlb_free_lock);
	node = SLIST_FIRST(&vq->iotlb_free_list);
	if (node != NULL)
		SLIST_REMOVE_HEAD(&vq->iotlb_free_list, next_free);
	rte_spinlock_unlock(&vq->iotlb_free_lock);
	return node;
}

static void
vhost_user_iotlb_pool_put(struct vhost_virtqueue *vq,
	struct vhost_iotlb_entry *node)
{
	rte_spinlock_lock(&vq->iotlb_free_lock);
	SLIST_INSERT_HEAD(&vq->iotlb_free_list, node, next_free);
	rte_spinlock_unlock(&vq->iotlb_free_lock);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev, struct vhost_virtqueue *vq);

static void
vhost_user_iotlb_pending_remove_all(struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&vq->iotlb_pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &vq->iotlb_pending_list, next, temp_node) {
		TAILQ_REMOVE(&vq->iotlb_pending_list, node, next);
		vhost_user_iotlb_pool_put(vq, node);
	}

	rte_rwlock_write_unlock(&vq->iotlb_pending_lock);
}

bool
vhost_user_iotlb_pending_miss(struct vhost_virtqueue *vq, uint64_t iova,
				uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	bool found = false;

	rte_rwlock_read_lock(&vq->iotlb_pending_lock);

	TAILQ_FOREACH(node, &vq->iotlb_pending_list, next) {
		if ((node->iova == iova) && (node->perm == perm)) {
			found = true;
			break;
		}
	}

	rte_rwlock_read_unlock(&vq->iotlb_pending_lock);

	return found;
}

void
vhost_user_iotlb_pending_insert(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;

	node = vhost_user_iotlb_pool_get(vq);
	if (node == NULL) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG,
			"IOTLB pool for vq %"PRIu32" empty, clear entries for pending insertion\n",
			vq->index);
		if (!TAILQ_EMPTY(&vq->iotlb_pending_list))
			vhost_user_iotlb_pending_remove_all(vq);
		else
			vhost_user_iotlb_cache_random_evict(dev, vq);
		node = vhost_user_iotlb_pool_get(vq);
		if (node == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"IOTLB pool vq %"PRIu32" still empty, pending insertion failure\n",
				vq->index);
			return;
		}
	}

	node->iova = iova;
	node->perm = perm;

	rte_rwlock_write_lock(&vq->iotlb_pending_lock);

	TAILQ_INSERT_TAIL(&vq->iotlb_pending_list, node, next);

	rte_rwlock_write_unlock(&vq->iotlb_pending_lock);
}

void
vhost_user_iotlb_pending_remove(struct vhost_virtqueue *vq,
				uint64_t iova, uint64_t size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&vq->iotlb_pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &vq->iotlb_pending_list, next,
				temp_node) {
		if (node->iova < iova)
			continue;
		if (node->iova >= iova + size)
			continue;
		if ((node->perm & perm) != node->perm)
			continue;
		TAILQ_REMOVE(&vq->iotlb_pending_list, node, next);
		vhost_user_iotlb_pool_put(vq, node);
	}

	rte_rwlock_write_unlock(&vq->iotlb_pending_lock);
}

static void
vhost_user_iotlb_cache_remove_all(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		mem_set_dump((void *)(uintptr_t)node->uaddr, node->size, false,
			hua_to_alignment(dev->mem, (void *)(uintptr_t)node->uaddr));
		TAILQ_REMOVE(&vq->iotlb_list, node, next);
		vhost_user_iotlb_pool_put(vq, node);
	}

	vq->iotlb_cache_nr = 0;

	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;
	uint64_t alignment, mask;
	int entry_idx;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	entry_idx = rte_rand() % vq->iotlb_cache_nr;

	RTE_TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		if (!entry_idx) {
			struct vhost_iotlb_entry *next_node;
			alignment = hua_to_alignment(dev->mem, (void *)(uintptr_t)node->uaddr);
			mask = ~(alignment - 1);

			/* Don't disable coredump if the previous node is in the same page */
			if (prev_node == NULL ||
					(node->uaddr & mask) != (prev_node->uaddr & mask)) {
				next_node = RTE_TAILQ_NEXT(node, next);
				/* Don't disable coredump if the next node is in the same page */
				if (next_node == NULL || ((node->uaddr + node->size - 1) & mask) !=
						(next_node->uaddr & mask))
					mem_set_dump((void *)(uintptr_t)node->uaddr, node->size,
							false, alignment);
			}
			TAILQ_REMOVE(&vq->iotlb_list, node, next);
			vhost_user_iotlb_pool_put(vq, node);
			vq->iotlb_cache_nr--;
			break;
		}
		prev_node = node;
		entry_idx--;
	}

	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

void
vhost_user_iotlb_cache_insert(struct virtio_net *dev, struct vhost_virtqueue *vq,
				uint64_t iova, uint64_t uaddr,
				uint64_t size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *new_node;

	new_node = vhost_user_iotlb_pool_get(vq);
	if (new_node == NULL) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG,
			"IOTLB pool vq %"PRIu32" empty, clear entries for cache insertion\n",
			vq->index);
		if (!TAILQ_EMPTY(&vq->iotlb_list))
			vhost_user_iotlb_cache_random_evict(dev, vq);
		else
			vhost_user_iotlb_pending_remove_all(vq);
		new_node = vhost_user_iotlb_pool_get(vq);
		if (new_node == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"IOTLB pool vq %"PRIu32" still empty, cache insertion failed\n",
				vq->index);
			return;
		}
	}

	new_node->iova = iova;
	new_node->uaddr = uaddr;
	new_node->size = size;
	new_node->perm = perm;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	TAILQ_FOREACH(node, &vq->iotlb_list, next) {
		/*
		 * Entries must be invalidated before being updated.
		 * So if iova already in list, assume identical.
		 */
		if (node->iova == new_node->iova) {
			vhost_user_iotlb_pool_put(vq, new_node);
			goto unlock;
		} else if (node->iova > new_node->iova) {
			mem_set_dump((void *)(uintptr_t)new_node->uaddr, new_node->size, true,
				hua_to_alignment(dev->mem, (void *)(uintptr_t)new_node->uaddr));
			TAILQ_INSERT_BEFORE(node, new_node, next);
			vq->iotlb_cache_nr++;
			goto unlock;
		}
	}

	mem_set_dump((void *)(uintptr_t)new_node->uaddr, new_node->size, true,
		hua_to_alignment(dev->mem, (void *)(uintptr_t)new_node->uaddr));
	TAILQ_INSERT_TAIL(&vq->iotlb_list, new_node, next);
	vq->iotlb_cache_nr++;

unlock:
	vhost_user_iotlb_pending_remove(vq, iova, size, perm);

	rte_rwlock_write_unlock(&vq->iotlb_lock);

}

void
vhost_user_iotlb_cache_remove(struct virtio_net *dev, struct vhost_virtqueue *vq,
					uint64_t iova, uint64_t size)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;
	uint64_t alignment, mask;

	if (unlikely(!size))
		return;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		/* Sorted list */
		if (unlikely(iova + size < node->iova))
			break;

		if (iova < node->iova + node->size) {
			struct vhost_iotlb_entry *next_node;
			alignment = hua_to_alignment(dev->mem, (void *)(uintptr_t)node->uaddr);
			mask = ~(alignment-1);

			/* Don't disable coredump if the previous node is in the same page */
			if (prev_node == NULL ||
					(node->uaddr & mask) != (prev_node->uaddr & mask)) {
				next_node = RTE_TAILQ_NEXT(node, next);
				/* Don't disable coredump if the next node is in the same page */
				if (next_node == NULL || ((node->uaddr + node->size - 1) & mask) !=
						(next_node->uaddr & mask))
					mem_set_dump((void *)(uintptr_t)node->uaddr, node->size,
							false, alignment);
			}

			TAILQ_REMOVE(&vq->iotlb_list, node, next);
			vhost_user_iotlb_pool_put(vq, node);
			vq->iotlb_cache_nr--;
		} else
			prev_node = node;
	}

	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

uint64_t
vhost_user_iotlb_cache_find(struct vhost_virtqueue *vq, uint64_t iova,
						uint64_t *size, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	uint64_t offset, vva = 0, mapped = 0;

	if (unlikely(!*size))
		goto out;

	TAILQ_FOREACH(node, &vq->iotlb_list, next) {
		/* List sorted by iova */
		if (unlikely(iova < node->iova))
			break;

		if (iova >= node->iova + node->size)
			continue;

		if (unlikely((perm & node->perm) != perm)) {
			vva = 0;
			break;
		}

		offset = iova - node->iova;
		if (!vva)
			vva = node->uaddr + offset;

		mapped += node->size - offset;
		iova = node->iova + node->size;

		if (mapped >= *size)
			break;
	}

out:
	/* Only part of the requested chunk is mapped */
	if (unlikely(mapped < *size))
		*size = mapped;

	return vva;
}

void
vhost_user_iotlb_flush_all(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	vhost_user_iotlb_cache_remove_all(dev, vq);
	vhost_user_iotlb_pending_remove_all(vq);
}

int
vhost_user_iotlb_init(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	unsigned int i;
	int socket = 0;

	if (vq->iotlb_pool) {
		/*
		 * The cache has already been initialized,
		 * just drop all cached and pending entries.
		 */
		vhost_user_iotlb_flush_all(dev, vq);
		rte_free(vq->iotlb_pool);
	}

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&socket, NULL, 0, vq, MPOL_F_NODE | MPOL_F_ADDR) != 0)
		socket = 0;
#endif

	rte_spinlock_init(&vq->iotlb_free_lock);
	rte_rwlock_init(&vq->iotlb_lock);
	rte_rwlock_init(&vq->iotlb_pending_lock);

	SLIST_INIT(&vq->iotlb_free_list);
	TAILQ_INIT(&vq->iotlb_list);
	TAILQ_INIT(&vq->iotlb_pending_list);

	if (dev->flags & VIRTIO_DEV_SUPPORT_IOMMU) {
		vq->iotlb_pool = rte_calloc_socket("iotlb", IOTLB_CACHE_SIZE,
			sizeof(struct vhost_iotlb_entry), 0, socket);
		if (!vq->iotlb_pool) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"Failed to create IOTLB cache pool for vq %"PRIu32"\n",
				vq->index);
			return -1;
		}
		for (i = 0; i < IOTLB_CACHE_SIZE; i++)
			vhost_user_iotlb_pool_put(vq, &vq->iotlb_pool[i]);
	}

	vq->iotlb_cache_nr = 0;

	return 0;
}

void
vhost_user_iotlb_destroy(struct vhost_virtqueue *vq)
{
	rte_free(vq->iotlb_pool);
}
