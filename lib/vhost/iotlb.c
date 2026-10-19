/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 Red Hat, Inc.
 */

#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_tailq.h>

#include "iotlb.h"
#include "vhost.h"

struct vhost_iotlb {
	rte_rwlock_t			pending_lock;
	struct vhost_iotlb_entry	*pool;
	TAILQ_HEAD(, vhost_iotlb_entry)	list;
	TAILQ_HEAD(, vhost_iotlb_entry)	pending_list;
	int				cache_nr;
	rte_spinlock_t			free_lock;
	SLIST_HEAD(, vhost_iotlb_entry)	free_list;
};

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
vhost_user_iotlb_pool_get(struct virtio_net *dev, int asid)
{
	struct vhost_iotlb_entry *node;

	rte_spinlock_lock(&dev->iotlb[asid]->free_lock);
	node = SLIST_FIRST(&dev->iotlb[asid]->free_list);
	if (node != NULL)
		SLIST_REMOVE_HEAD(&dev->iotlb[asid]->free_list, next_free);
	rte_spinlock_unlock(&dev->iotlb[asid]->free_lock);
	return node;
}

static void
vhost_user_iotlb_pool_put(struct virtio_net *dev, int asid, struct vhost_iotlb_entry *node)
{
	rte_spinlock_lock(&dev->iotlb[asid]->free_lock);
	SLIST_INSERT_HEAD(&dev->iotlb[asid]->free_list, node, next_free);
	rte_spinlock_unlock(&dev->iotlb[asid]->free_lock);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev, int asid);

static void
vhost_user_iotlb_pending_remove_all(struct virtio_net *dev, int asid)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&dev->iotlb[asid]->pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb[asid]->pending_list, next, temp_node) {
		TAILQ_REMOVE(&dev->iotlb[asid]->pending_list, node, next);
		vhost_user_iotlb_pool_put(dev, asid, node);
	}

	rte_rwlock_write_unlock(&dev->iotlb[asid]->pending_lock);
}

bool
vhost_user_iotlb_pending_miss(struct virtio_net *dev, int asid, uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	bool found = false;

	rte_rwlock_read_lock(&dev->iotlb[asid]->pending_lock);

	TAILQ_FOREACH(node, &dev->iotlb[asid]->pending_list, next) {
		if ((node->iova == iova) && (node->perm == perm)) {
			found = true;
			break;
		}
	}

	rte_rwlock_read_unlock(&dev->iotlb[asid]->pending_lock);

	return found;
}

void
vhost_user_iotlb_pending_insert(struct virtio_net *dev, int asid, uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;

	node = vhost_user_iotlb_pool_get(dev, asid);
	if (node == NULL) {
		VHOST_CONFIG_LOG(dev->ifname, DEBUG,
			"IOTLB pool empty, clear entries for pending insertion");
		if (!TAILQ_EMPTY(&dev->iotlb[asid]->pending_list))
			vhost_user_iotlb_pending_remove_all(dev, asid);
		else
			vhost_user_iotlb_cache_random_evict(dev, asid);
		node = vhost_user_iotlb_pool_get(dev, asid);
		if (node == NULL) {
			VHOST_CONFIG_LOG(dev->ifname, ERR,
				"IOTLB pool still empty, pending insertion failure");
			return;
		}
	}

	node->iova = iova;
	node->perm = perm;

	rte_rwlock_write_lock(&dev->iotlb[asid]->pending_lock);

	TAILQ_INSERT_TAIL(&dev->iotlb[asid]->pending_list, node, next);

	rte_rwlock_write_unlock(&dev->iotlb[asid]->pending_lock);
}

void
vhost_user_iotlb_pending_remove(struct virtio_net *dev, int asid,
				uint64_t iova, uint64_t size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&dev->iotlb[asid]->pending_lock);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb[asid]->pending_list, next,
				temp_node) {
		if (node->iova < iova)
			continue;
		if (node->iova >= iova + size)
			continue;
		if ((node->perm & perm) != node->perm)
			continue;
		TAILQ_REMOVE(&dev->iotlb[asid]->pending_list, node, next);
		vhost_user_iotlb_pool_put(dev, asid, node);
	}

	rte_rwlock_write_unlock(&dev->iotlb[asid]->pending_lock);
}

static void
vhost_user_iotlb_cache_remove_all(struct virtio_net *dev, int asid)
{
	struct vhost_iotlb_entry *node, *temp_node;

	vhost_user_iotlb_wr_lock_all(dev);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb[asid]->list, next, temp_node) {
		vhost_user_iotlb_clear_dump(dev, node, NULL, NULL);

		TAILQ_REMOVE(&dev->iotlb[asid]->list, node, next);
		vhost_user_iotlb_remove_notify(dev, node);
		vhost_user_iotlb_pool_put(dev, asid, node);
	}

	dev->iotlb[asid]->cache_nr = 0;

	vhost_user_iotlb_wr_unlock_all(dev);
}

static void
vhost_user_iotlb_cache_random_evict(struct virtio_net *dev, int asid)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;
	int entry_idx;

	vhost_user_iotlb_wr_lock_all(dev);

	entry_idx = rte_rand() % dev->iotlb[asid]->cache_nr;

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb[asid]->list, next, temp_node) {
		if (!entry_idx) {
			struct vhost_iotlb_entry *next_node = RTE_TAILQ_NEXT(node, next);

			vhost_user_iotlb_clear_dump(dev, node, prev_node, next_node);

			TAILQ_REMOVE(&dev->iotlb[asid]->list, node, next);
			vhost_user_iotlb_remove_notify(dev, node);
			vhost_user_iotlb_pool_put(dev, asid, node);
			dev->iotlb[asid]->cache_nr--;
			break;
		}
		prev_node = node;
		entry_idx--;
	}

	vhost_user_iotlb_wr_unlock_all(dev);
}

void
vhost_user_iotlb_cache_insert(struct virtio_net *dev, int asid, uint64_t iova, uint64_t uaddr,
				uint64_t uoffset, uint64_t size, uint64_t page_size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *new_node;

	new_node = vhost_user_iotlb_pool_get(dev, asid);
	if (new_node == NULL) {
		VHOST_CONFIG_LOG(dev->ifname, DEBUG,
			"IOTLB pool empty, clear entries for cache insertion");
		if (!TAILQ_EMPTY(&dev->iotlb[asid]->list))
			vhost_user_iotlb_cache_random_evict(dev, asid);
		else
			vhost_user_iotlb_pending_remove_all(dev, asid);
		new_node = vhost_user_iotlb_pool_get(dev, asid);
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

	TAILQ_FOREACH(node, &dev->iotlb[asid]->list, next) {
		/*
		 * Entries must be invalidated before being updated.
		 * So if iova already in list, assume identical.
		 */
		if (node->iova == new_node->iova) {
			vhost_user_iotlb_pool_put(dev, asid, new_node);
			goto unlock;
		} else if (node->iova > new_node->iova) {
			vhost_user_iotlb_set_dump(dev, new_node);

			TAILQ_INSERT_BEFORE(node, new_node, next);
			dev->iotlb[asid]->cache_nr++;
			goto unlock;
		}
	}

	vhost_user_iotlb_set_dump(dev, new_node);

	TAILQ_INSERT_TAIL(&dev->iotlb[asid]->list, new_node, next);
	dev->iotlb[asid]->cache_nr++;

unlock:
	vhost_user_iotlb_pending_remove(dev, asid, iova, size, perm);

	vhost_user_iotlb_wr_unlock_all(dev);
}

void
vhost_user_iotlb_cache_remove(struct virtio_net *dev, int asid, uint64_t iova, uint64_t size)
{
	struct vhost_iotlb_entry *node, *temp_node, *prev_node = NULL;

	if (unlikely(!size))
		return;

	vhost_user_iotlb_wr_lock_all(dev);

	RTE_TAILQ_FOREACH_SAFE(node, &dev->iotlb[asid]->list, next, temp_node) {
		/* Sorted list */
		if (unlikely(iova + size < node->iova))
			break;

		if (iova < node->iova + node->size) {
			struct vhost_iotlb_entry *next_node = RTE_TAILQ_NEXT(node, next);

			vhost_user_iotlb_clear_dump(dev, node, prev_node, next_node);

			TAILQ_REMOVE(&dev->iotlb[asid]->list, node, next);
			vhost_user_iotlb_remove_notify(dev, node);
			vhost_user_iotlb_pool_put(dev, asid, node);
			dev->iotlb[asid]->cache_nr--;
		} else {
			prev_node = node;
		}
	}

	vhost_user_iotlb_wr_unlock_all(dev);
}

uint64_t
vhost_user_iotlb_cache_find(struct virtio_net *dev, int asid,
			    uint64_t iova, uint64_t *size, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	uint64_t offset, vva = 0, mapped = 0;

	if (unlikely(!*size))
		goto out;

	TAILQ_FOREACH(node, &dev->iotlb[asid]->list, next) {
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
vhost_user_iotlb_flush_all(struct virtio_net *dev, int asid)
{
	vhost_user_iotlb_cache_remove_all(dev, asid);
	vhost_user_iotlb_pending_remove_all(dev, asid);
}

static int
vhost_user_iotlb_init_one(struct virtio_net *dev, int asid)
{
	unsigned int i;
	int socket = 0;

	if (dev->iotlb[asid] != NULL) {
		if (dev->iotlb[asid]->pool != NULL) {
			/*
			 * The cache has already been initialized,
			 * just drop all cached and pending entries.
			 */
			vhost_user_iotlb_flush_all(dev, asid);
			rte_free(dev->iotlb[asid]->pool);
		}
		rte_free(dev->iotlb[asid]);
	}

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&socket, NULL, 0, dev, MPOL_F_NODE | MPOL_F_ADDR) != 0)
		socket = 0;
#endif

	dev->iotlb[asid] = rte_malloc_socket("iotlb", sizeof(struct vhost_iotlb), 0, socket);
	if (!dev->iotlb[asid]) {
		VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to allocate IOTLB");
		return -1;
	}

	rte_spinlock_init(&dev->iotlb[asid]->free_lock);
	rte_rwlock_init(&dev->iotlb[asid]->pending_lock);

	SLIST_INIT(&dev->iotlb[asid]->free_list);
	TAILQ_INIT(&dev->iotlb[asid]->list);
	TAILQ_INIT(&dev->iotlb[asid]->pending_list);

	if (dev->flags & VIRTIO_DEV_SUPPORT_IOMMU) {
		dev->iotlb[asid]->pool = rte_calloc_socket("iotlb_pool", IOTLB_CACHE_SIZE,
			sizeof(struct vhost_iotlb_entry), 0, socket);
		if (!dev->iotlb[asid]->pool) {
			VHOST_CONFIG_LOG(dev->ifname, ERR, "Failed to create IOTLB cache pool");
			goto free_iotlb;
		}
		for (i = 0; i < IOTLB_CACHE_SIZE; i++)
			vhost_user_iotlb_pool_put(dev, asid, &dev->iotlb[asid]->pool[i]);
	}

	dev->iotlb[asid]->cache_nr = 0;

	return 0;

free_iotlb:
	rte_free(dev->iotlb[asid]);
	dev->iotlb[asid] = NULL;
	return -1;
}

int
vhost_user_iotlb_init(struct virtio_net *dev)
{
	int i;

	for (i = 0; i < IOTLB_MAX_ASID; i++)
		if (vhost_user_iotlb_init_one(dev, i) < 0)
			goto fail;

	return 0;
fail:
	while (i--) {
		rte_free(dev->iotlb[i]->pool);
		dev->iotlb[i]->pool = NULL;
		rte_free(dev->iotlb[i]);
		dev->iotlb[i] = NULL;
	}

	return -1;
}

void
vhost_user_iotlb_destroy(struct virtio_net *dev)
{
	int i;

	for (i = 0; i < IOTLB_MAX_ASID; i++) {
		if (dev->iotlb[i]) {
			rte_free(dev->iotlb[i]->pool);
			dev->iotlb[i]->pool = NULL;

			rte_free(dev->iotlb[i]);
			dev->iotlb[i] = NULL;
		}
	}
}
