/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2017 Red Hat, Inc.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_tailq.h>

#include "iotlb.h"
#include "vhost.h"

struct vhost_iotlb_entry {
	TAILQ_ENTRY(vhost_iotlb_entry) next;

	uint64_t iova;
	uint64_t uaddr;
	uint64_t size;
	uint8_t perm;
};

#define IOTLB_CACHE_SIZE 2048

static void
vhost_user_iotlb_cache_random_evict(struct vhost_virtqueue *vq);

static void
vhost_user_iotlb_pending_remove_all(struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&vq->iotlb_pending_lock);

	TAILQ_FOREACH_SAFE(node, &vq->iotlb_pending_list, next, temp_node) {
		TAILQ_REMOVE(&vq->iotlb_pending_list, node, next);
		rte_mempool_put(vq->iotlb_pool, node);
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
vhost_user_iotlb_pending_insert(struct vhost_virtqueue *vq,
				uint64_t iova, uint8_t perm)
{
	struct vhost_iotlb_entry *node;
	int ret;

	ret = rte_mempool_get(vq->iotlb_pool, (void **)&node);
	if (ret) {
		RTE_LOG(DEBUG, VHOST_CONFIG, "IOTLB pool empty, clear entries\n");
		if (!TAILQ_EMPTY(&vq->iotlb_pending_list))
			vhost_user_iotlb_pending_remove_all(vq);
		else
			vhost_user_iotlb_cache_random_evict(vq);
		ret = rte_mempool_get(vq->iotlb_pool, (void **)&node);
		if (ret) {
			RTE_LOG(ERR, VHOST_CONFIG, "IOTLB pool still empty, failure\n");
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

	TAILQ_FOREACH_SAFE(node, &vq->iotlb_pending_list, next, temp_node) {
		if (node->iova < iova)
			continue;
		if (node->iova >= iova + size)
			continue;
		if ((node->perm & perm) != node->perm)
			continue;
		TAILQ_REMOVE(&vq->iotlb_pending_list, node, next);
		rte_mempool_put(vq->iotlb_pool, node);
	}

	rte_rwlock_write_unlock(&vq->iotlb_pending_lock);
}

static void
vhost_user_iotlb_cache_remove_all(struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		TAILQ_REMOVE(&vq->iotlb_list, node, next);
		rte_mempool_put(vq->iotlb_pool, node);
	}

	vq->iotlb_cache_nr = 0;

	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

static void
vhost_user_iotlb_cache_random_evict(struct vhost_virtqueue *vq)
{
	struct vhost_iotlb_entry *node, *temp_node;
	int entry_idx;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	entry_idx = rte_rand() % vq->iotlb_cache_nr;

	TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		if (!entry_idx) {
			TAILQ_REMOVE(&vq->iotlb_list, node, next);
			rte_mempool_put(vq->iotlb_pool, node);
			vq->iotlb_cache_nr--;
			break;
		}
		entry_idx--;
	}

	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

void
vhost_user_iotlb_cache_insert(struct vhost_virtqueue *vq, uint64_t iova,
				uint64_t uaddr, uint64_t size, uint8_t perm)
{
	struct vhost_iotlb_entry *node, *new_node;
	int ret;

	ret = rte_mempool_get(vq->iotlb_pool, (void **)&new_node);
	if (ret) {
		RTE_LOG(DEBUG, VHOST_CONFIG, "IOTLB pool empty, clear entries\n");
		if (!TAILQ_EMPTY(&vq->iotlb_list))
			vhost_user_iotlb_cache_random_evict(vq);
		else
			vhost_user_iotlb_pending_remove_all(vq);
		ret = rte_mempool_get(vq->iotlb_pool, (void **)&new_node);
		if (ret) {
			RTE_LOG(ERR, VHOST_CONFIG, "IOTLB pool still empty, failure\n");
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
			rte_mempool_put(vq->iotlb_pool, new_node);
			goto unlock;
		} else if (node->iova > new_node->iova) {
			TAILQ_INSERT_BEFORE(node, new_node, next);
			vq->iotlb_cache_nr++;
			goto unlock;
		}
	}

	TAILQ_INSERT_TAIL(&vq->iotlb_list, new_node, next);
	vq->iotlb_cache_nr++;

unlock:
	vhost_user_iotlb_pending_remove(vq, iova, size, perm);

	rte_rwlock_write_unlock(&vq->iotlb_lock);

}

void
vhost_user_iotlb_cache_remove(struct vhost_virtqueue *vq,
					uint64_t iova, uint64_t size)
{
	struct vhost_iotlb_entry *node, *temp_node;

	if (unlikely(!size))
		return;

	rte_rwlock_write_lock(&vq->iotlb_lock);

	TAILQ_FOREACH_SAFE(node, &vq->iotlb_list, next, temp_node) {
		/* Sorted list */
		if (unlikely(iova + size < node->iova))
			break;

		if (iova < node->iova + node->size) {
			TAILQ_REMOVE(&vq->iotlb_list, node, next);
			rte_mempool_put(vq->iotlb_pool, node);
			vq->iotlb_cache_nr--;
		}
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
vhost_user_iotlb_flush_all(struct vhost_virtqueue *vq)
{
	vhost_user_iotlb_cache_remove_all(vq);
	vhost_user_iotlb_pending_remove_all(vq);
}

int
vhost_user_iotlb_init(struct virtio_net *dev, int vq_index)
{
	char pool_name[RTE_MEMPOOL_NAMESIZE];
	struct vhost_virtqueue *vq = dev->virtqueue[vq_index];
	int socket = 0;

	if (vq->iotlb_pool) {
		/*
		 * The cache has already been initialized,
		 * just drop all cached and pending entries.
		 */
		vhost_user_iotlb_flush_all(vq);
	}

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&socket, NULL, 0, vq, MPOL_F_NODE | MPOL_F_ADDR) != 0)
		socket = 0;
#endif

	rte_rwlock_init(&vq->iotlb_lock);
	rte_rwlock_init(&vq->iotlb_pending_lock);

	TAILQ_INIT(&vq->iotlb_list);
	TAILQ_INIT(&vq->iotlb_pending_list);

	snprintf(pool_name, sizeof(pool_name), "iotlb_cache_%d_%d",
			dev->vid, vq_index);

	/* If already created, free it and recreate */
	vq->iotlb_pool = rte_mempool_lookup(pool_name);
	if (vq->iotlb_pool)
		rte_mempool_free(vq->iotlb_pool);

	vq->iotlb_pool = rte_mempool_create(pool_name,
			IOTLB_CACHE_SIZE, sizeof(struct vhost_iotlb_entry), 0,
			0, 0, NULL, NULL, NULL, socket,
			MEMPOOL_F_NO_CACHE_ALIGN |
			MEMPOOL_F_SP_PUT |
			MEMPOOL_F_SC_GET);
	if (!vq->iotlb_pool) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to create IOTLB cache pool (%s)\n",
				pool_name);
		return -1;
	}

	vq->iotlb_cache_nr = 0;

	return 0;
}
