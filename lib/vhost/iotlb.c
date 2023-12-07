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
	uint64_t uoffset;
	uint64_t size;
	uint8_t page_shift;
	uint8_t perm;
};

#define IOTLB_CACHE_SIZE 2048

static void
vhost_user_iotlb_remove_notify(struct virtio_net *dev, struct vhost_iotlb_entry *entry)
{
	if (dev->backend_ops->iotlb_remove_notify == NULL)
		return;

	dev->backend_ops->iotlb_remove_notify(entry->uaddr, entry->uoffset, entry->size);
}

static bool
vhost_user_iotlb_share_page(struct vhost_iotlb_entry *a, struct vhost_iotlb_entry *b)
{
	uint64_t a_start, a_end, b_start;

	if (a == NULL || b == NULL)
		return false;

	a_start = a->uaddr + a->uoffset;
	b_start = b->uaddr + b->uoffset;

	/* Assumes entry a lower than entry b */
	RTE_ASSERT(a_start < b_start);
	a_end = RTE_ALIGN_CEIL(a_start + a->size, RTE_BIT64(a->page_shift));
	b_start = RTE_ALIGN_FLOOR(b_start, RTE_BIT64(b->page_shift));

	return a_end > b_start;
}

static void
vhost_user_iotlb_set_dump(struct virtio_net *dev, struct vhost_iotlb_entry *node)
{
	uint64_t start;

	start = node->uaddr + node->uoffset;
	mem_set_dump(dev, (void *)(uintptr_t)start, node->size, true, RTE_BIT64(node->page_shift));
}

static void
vhost_user_iotlb_clear_dump(struct virtio_net *dev, struct vhost_iotlb_entry *node,
		struct vhost_iotlb_entry *prev, struct vhost_iotlb_entry *next)
{
	uint64_t start, end;

	start = node->uaddr + node->uoffset;
	end = start + node->size;

	/* Skip first page if shared with previous entry. */
	if (vhost_user_iotlb_share_page(prev, node))
		start = RTE_ALIGN_CEIL(start, RTE_BIT64(node->page_shift));

	/* Skip last page if shared with next entry. */
	if (vhost_user_iotlb_share_page(node, next))
		end = RTE_ALIGN_FLOOR(end, RTE_BIT64(node->page_shift));

	if (end > start)
		mem_set_dump(dev, (void *)(uintptr_t)start, end - start, false,
			RTE_BIT64(node->page_shift));
}

static struct vhost_iotlb_entry *
vhost_user_iotlb_pool_get(struct virtio_net *dev)
{
	struct vhost_iotlb_entry *node;

	rte_spinlock_lock(&dev->iotlb_free_lock);
	node = SLIST_FIRST(&dev->iotlb_free_list);
	if (node != NULL)
		SLIST_REMOVE_HEAD(&dev->iotlb_free_list, next_free);
	rte_spinlock_unlock(&dev->iotlb_free_lock);
	return node;
}

static void
vhost_user_iotlb_pool_put(struct virtio_net *dev, struct vhost_iotlb_entry *node)
{
	rte_spinlock_lock(&dev->iotlb_free_lock);
	SLIST_INSERT_HEAD(&dev->iotlb_free_list, node, next_free);
	rte_spinlock_unlock(&dev->iotlb_free_lock);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev);

static void
vhost_user_iotlb_pending_remove_all(struct virtio_net *dev)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&dev->iotlb_pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb_pending_list, next, temp_node) {
		TAILQ_REMOVE(&dev->iotlb_pending_list, node, next);
		vhost_user_iotlb_pool_put(dev, node);
	}

	rte_rwlock_write_unlock(&dev->iotlb_pending_lock);
}

bool
vhost_user_iotlb_pending_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	bool found = false;

	rte_rwlock_read_lock(&dev->iotlb_pending_lock);

	TAILQ_FOREACH(node, &dev->iotlb_pending_list, next) {
		if ((node->iova == iova) && (node->perm == perm)) {
			found = true;
			break;
		}
	}

	rte_rwlock_read_unlock(&dev->iotlb_pending_lock);

	return found;
}

void
vhost_user_iotlb_pending_insert(struct virtio_net *dev, uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;

	node = vhost_user_iotlb_pool_get(dev);
	if (node == NULL) {
		VHOST_CONFIG_LOG(dev->ifname, DEBUG,
			"IOTLB pool empty, clear entries for pending insertion");
		if (!TAILQ_EMPTY(&dev->iotlb_pending_list))
			vhost_user_iotlb_pending_remove_all(dev);
		else
			vhost_user_iotlb_cache_random_evict(dev);
		node = vhost_user_iotlb_pool_get(dev);
		if (node == NULL) {
			VHOST_CONFIG_LOG(dev->ifname, ERR,
				"IOTLB pool still empty, pending insertion failure");
			return;
		}
	}

	node->iova = iova;
	node->perm = perm;

	rte_rwlock_write_lock(&dev->iotlb_pending_lock);

	TAILQ_INSERT_TAIL(&dev->iotlb_pending_list, node, next);

	rte_rwlock_write_unlock(&dev->iotlb_pending_lock);
}

void
vhost_user_iotlb_pending_remove(struct virtio_net *dev, uint64_t iova, uint64_t size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&dev->iotlb_pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb_pending_list, next,
				temp_node) {
		if (node->iova < iova)
			continue;
		if (node->iova >= iova + size)
			continue;
		if ((node->perm & perm) != node->perm)
			continue;
		TAILQ_REMOVE(&dev->iotlb_pending_list, node, next);
		vhost_user_iotlb_pool_put(dev, node);
	}

	rte_rwlock_write_unlock(&dev->iotlb_pending_lock);
}

static void
vhost_user_iotlb_cache_remove_all(struct virtio_net *dev)
{
	struct vhost_iotlb_entry *node, *temp_node;

	vhost_user_iotlb_wr_lock_all(dev);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb_list, next, temp_node) {
		vhost_user_iotlb_clear_dump(dev, node, NULL, NULL);

		TAILQ_REMOVE(&dev->iotlb_list, node, next);
		vhost_user_iotlb_remove_notify(dev, node);
		vhost_user_iotlb_pool_put(dev, node);
	}

	dev->iotlb_cache_nr = 0;

	vhost_user_iotlb_wr_unlock_all(dev);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;
	int entry_idx;

	vhost_user_iotlb_wr_lock_all(dev);

	entry_idx = rte_rand() % dev->iotlb_cache_nr;

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb_list, next, temp_node) {
		if (!entry_idx) {
			struct vhost_iotlb_entry *next_node = RTE_TAILQ_NEXT(node, next);

			vhost_user_iotlb_clear_dump(dev, node, prev_node, next_node);

			TAILQ_REMOVE(&dev->iotlb_list, node, next);
			vhost_user_iotlb_remove_notify(dev, node);
			vhost_user_iotlb_pool_put(dev, node);
			dev->iotlb_cache_nr--;
			break;
		}
		prev_node = node;
		entry_idx--;
	}

	vhost_user_iotlb_wr_unlock_all(dev);
}

void
vhost_user_iotlb_cache_insert(struct virtio_net *dev, uint64_t iova, uint64_t uaddr,
				uint64_t uoffset, uint64_t size, uint64_t page_size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *new_node;

	new_node = vhost_user_iotlb_pool_get(dev);
	if (new_node == NULL) {
		VHOST_CONFIG_LOG(dev->ifname, DEBUG,
			"IOTLB pool empty, clear entries for cache insertion");
		if (!TAILQ_EMPTY(&dev->iotlb_list))
			vhost_user_iotlb_cache_random_evict(dev);
		else
			vhost_user_iotlb_pending_remove_all(dev);
		new_node = vhost_user_iotlb_pool_get(dev);
		if (new_node == NULL) {
			VHOST_CONFIG_LOG(dev->ifname, ERR,
				"IOTLB pool still empty, cache insertion failed");
			return;
		}
	}

	new_node->iova = iova;
	new_node->uaddr = uaddr;
	new_node->uoffset = uoffset;
	new_node->size = size;
	new_node->page_shift = rte_ctz64(page_size);
	new_node->perm = perm;

	vhost_user_iotlb_wr_lock_all(dev);

	TAILQ_FOREACH(node, &dev->iotlb_list, next) {
		/*
		 * Entries must be invalidated before being updated.
		 * So if iova already in list, assume identical.
		 */
		if (node->iova == new_node->iova) {
			vhost_user_iotlb_pool_put(dev, new_node);
			goto unlock;
		} else if (node->iova > new_node->iova) {
			vhost_user_iotlb_set_dump(dev, new_node);

			TAILQ_INSERT_BEFORE(node, new_node, next);
			dev->iotlb_cache_nr++;
			goto unlock;
		}
	}

	vhost_user_iotlb_set_dump(dev, new_node);

	TAILQ_INSERT_TAIL(&dev->iotlb_list, new_node, next);
	dev->iotlb_cache_nr++;

unlock:
	vhost_user_iotlb_pending_remove(dev, iova, size, perm);

	vhost_user_iotlb_wr_unlock_all(dev);
}

void
vhost_user_iotlb_cache_remove(struct virtio_net *dev, uint64_t iova, uint64_t size)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;

	if (unlikely(!size))
		return;

	vhost_user_iotlb_wr_lock_all(dev);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb_list, next, temp_node) {
		/* Sorted list */
		if (unlikely(iova + size < node->iova))
			break;

		if (iova < node->iova + node->size) {
			struct vhost_iotlb_entry *next_node = RTE_TAILQ_NEXT(node, next);

			vhost_user_iotlb_clear_dump(dev, node, prev_node, next_node);

			TAILQ_REMOVE(&dev->iotlb_list, node, next);
			vhost_user_iotlb_remove_notify(dev, node);
			vhost_user_iotlb_pool_put(dev, node);
			dev->iotlb_cache_nr--;
		} else {
			prev_node = node;
		}
	}

	vhost_user_iotlb_wr_unlock_all(dev);
}

uint64_t
vhost_user_iotlb_cache_find(struct virtio_net *dev, uint64_t iova, uint64_t *size, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	uint64_t offset, vva = 0, mapped = 0;

	if (unlikely(!*size))
		goto out;

	TAILQ_FOREACH(node, &dev->iotlb_list, next) {
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
			vva = node->uaddr + node->uoffset + offset;

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
vhost_user_iotlb_flush_all(struct virtio_net *dev)
{
	vhost_user_iotlb_cache_remove_all(dev);
	vhost_user_iotlb_pending_remove_all(dev);
}

int
vhost_user_iotlb_init(struct virtio_net *dev)
{
	unsigned int i;
	int socket = 0;

	if (dev->iotlb_pool) {
		/*
		 * The cache has already been initialized,
		 * just drop all cached and pending entries.
		 */
		vhost_user_iotlb_flush_all(dev);
		rte_free(dev->iotlb_pool);
	}

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&socket, NULL, 0, dev, MPOL_F_NODE | MPOL_F_ADDR) != 0)
		socket = 0;
#endif

	rte_spinlock_init(&dev->iotlb_free_lock);
	rte_rwlock_init(&dev->iotlb_pending_lock);

	SLIST_INIT(&dev->iotlb_free_list);
	TAILQ_INIT(&dev->iotlb_list);
	TAILQ_INIT(&dev->iotlb_pending_list);

	if (dev->flags & VIRTIO_DEV_SUPPORT_IOMMU) {
		dev->iotlb_pool = rte_calloc_socket("iotlb", IOTLB_CACHE_SIZE,
			sizeof(struct vhost_iotlb_entry), 0, socket);
		if (!dev->iotlb_pool) {
			VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to create IOTLB cache pool");
			return -1;
		}
		for (i = 0; i < IOTLB_CACHE_SIZE; i++)
			vhost_user_iotlb_pool_put(dev, &dev->iotlb_pool[i]);
	}

	dev->iotlb_cache_nr = 0;

	return 0;
}

void
vhost_user_iotlb_destroy(struct virtio_net *dev)
{
	rte_free(dev->iotlb_pool);
}
