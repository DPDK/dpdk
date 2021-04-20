/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2017 Red Hat, Inc.
 */

#ifndef _VHOST_IOTLB_H_
#define _VHOST_IOTLB_H_

#include <stdbool.h>

#include "vhost.h"

static __rte_always_inline void
vhost_user_iotlb_rd_lock(struct vhost_virtqueue *vq)
{
	rte_rwlock_read_lock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_rd_unlock(struct vhost_virtqueue *vq)
{
	rte_rwlock_read_unlock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_lock(struct vhost_virtqueue *vq)
{
	rte_rwlock_write_lock(&vq->iotlb_lock);
}

static __rte_always_inline void
vhost_user_iotlb_wr_unlock(struct vhost_virtqueue *vq)
{
	rte_rwlock_write_unlock(&vq->iotlb_lock);
}

void vhost_user_iotlb_cache_insert(struct vhost_virtqueue *vq, uint64_t iova,
					uint64_t uaddr, uint64_t size,
					uint8_t perm);
void vhost_user_iotlb_cache_remove(struct vhost_virtqueue *vq,
					uint64_t iova, uint64_t size);
uint64_t vhost_user_iotlb_cache_find(struct vhost_virtqueue *vq, uint64_t iova,
					uint64_t *size, uint8_t perm);
bool vhost_user_iotlb_pending_miss(struct vhost_virtqueue *vq, uint64_t iova,
						uint8_t perm);
void vhost_user_iotlb_pending_insert(struct vhost_virtqueue *vq, uint64_t iova,
						uint8_t perm);
void vhost_user_iotlb_pending_remove(struct vhost_virtqueue *vq, uint64_t iova,
						uint64_t size, uint8_t perm);
void vhost_user_iotlb_flush_all(struct vhost_virtqueue *vq);
int vhost_user_iotlb_init(struct virtio_net *dev, int vq_index);

#endif /* _VHOST_IOTLB_H_ */
