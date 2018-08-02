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
