/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 Red Hat, Inc.
 */

#ifndef _VHOST_IOTLB_H_
#define _VHOST_IOTLB_H_

#include <stdbool.h>

#include "vhost.h"

static __rte_always_inline void
vhost_user_iotlb_rd_lock(struct vhost_virtqueue *vq)
	__rte_shared_lock_function(&vq->iotlb_lock)
{
	rte_rwlock_read_lock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_rd_unlock(struct vhost_virtqueue *vq)
	__rte_unlock_function(&vq->iotlb_lock)
{
	rte_rwlock_read_unlock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_lock(struct vhost_virtqueue *vq)
	__rte_exclusive_lock_function(&vq->iotlb_lock)
{
	rte_rwlock_write_lock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_unlock(struct vhost_virtqueue *vq)
	__rte_unlock_function(&vq->iotlb_lock)
{
	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_lock_all(struct virtio_net *dev)
	__rte_no_thread_safety_analysis
{
	uint32_t i;

	for (i = 0; i < dev->nr_vring; i++)
		rte_rwlock_write_lock(&dev->virtqueue[i]->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_unlock_all(struct virtio_net *dev)
	__rte_no_thread_safety_analysis
{
	uint32_t i;

	for (i = 0; i < dev->nr_vring; i++)
		rte_rwlock_write_unlock(&dev->virtqueue[i]->iotlb_lock);
}

void vhost_user_iotlb_cache_insert(struct virtio_net *dev, uint64_t iova, uint64_t uaddr,
		uint64_t uoffset, uint64_t size, uint64_t page_size, uint8_t perm);
void vhost_user_iotlb_cache_remove(struct virtio_net *dev, uint64_t iova, uint64_t size);
uint64_t vhost_user_iotlb_cache_find(struct virtio_net *dev, uint64_t iova,
					uint64_t *size, uint8_t perm);
bool vhost_user_iotlb_pending_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm);
void vhost_user_iotlb_pending_insert(struct virtio_net *dev, uint64_t iova, uint8_t perm);
void vhost_user_iotlb_pending_remove(struct virtio_net *dev, uint64_t iova,
						uint64_t size, uint8_t perm);
void vhost_user_iotlb_flush_all(struct virtio_net *dev);
int vhost_user_iotlb_init(struct virtio_net *dev);
void vhost_user_iotlb_destroy(struct virtio_net *dev);

#endif /* _VHOST_IOTLB_H_ */
