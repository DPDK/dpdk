/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_vhost.h>
#include <rte_rwlock.h>

#include "iotlb.h"
#include "vhost.h"
#include "vhost_user.h"

struct virtio_net *vhost_devices[MAX_VHOST_DEVICE];

/* Called with iotlb_lock read-locked */
uint64_t
__vhost_iova_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
		    uint64_t iova, uint64_t *size, uint8_t perm)
{
	uint64_t vva, tmp_size;

	if (unlikely(!*size))
		return 0;

	tmp_size = *size;

	vva = vhost_user_iotlb_cache_find(vq, iova, &tmp_size, perm);
	if (tmp_size == *size)
		return vva;

	iova += tmp_size;

	if (!vhost_user_iotlb_pending_miss(vq, iova, perm)) {
		/*
		 * iotlb_lock is read-locked for a full burst,
		 * but it only protects the iotlb cache.
		 * In case of IOTLB miss, we might block on the socket,
		 * which could cause a deadlock with QEMU if an IOTLB update
		 * is being handled. We can safely unlock here to avoid it.
		 */
		vhost_user_iotlb_rd_unlock(vq);

		vhost_user_iotlb_pending_insert(vq, iova, perm);
		if (vhost_user_iotlb_miss(dev, iova, perm)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"IOTLB miss req failed for IOVA 0x%" PRIx64 "\n",
				iova);
			vhost_user_iotlb_pending_remove(vq, iova, 1, perm);
		}

		vhost_user_iotlb_rd_lock(vq);
	}

	return 0;
}

void
cleanup_vq(struct vhost_virtqueue *vq, int destroy)
{
	if ((vq->callfd >= 0) && (destroy != 0))
		close(vq->callfd);
	if (vq->kickfd >= 0)
		close(vq->kickfd);
}

/*
 * Unmap any memory, close any file descriptors and
 * free any memory owned by a device.
 */
void
cleanup_device(struct virtio_net *dev, int destroy)
{
	uint32_t i;

	vhost_backend_cleanup(dev);

	for (i = 0; i < dev->nr_vring; i++)
		cleanup_vq(dev->virtqueue[i], destroy);
}

void
free_vq(struct vhost_virtqueue *vq)
{
	rte_free(vq->shadow_used_ring);
	rte_free(vq->batch_copy_elems);
	rte_mempool_free(vq->iotlb_pool);
	rte_free(vq);
}

/*
 * Release virtqueues and device memory.
 */
static void
free_device(struct virtio_net *dev)
{
	uint32_t i;

	for (i = 0; i < dev->nr_vring; i++)
		free_vq(dev->virtqueue[i]);

	rte_free(dev);
}

int
vring_translate(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint64_t req_size, size;

	if (!(dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)))
		goto out;

	req_size = sizeof(struct vring_desc) * vq->size;
	size = req_size;
	vq->desc = (struct vring_desc *)(uintptr_t)vhost_iova_to_vva(dev, vq,
						vq->ring_addrs.desc_user_addr,
						&size, VHOST_ACCESS_RW);
	if (!vq->desc || size != req_size)
		return -1;

	req_size = sizeof(struct vring_avail);
	req_size += sizeof(uint16_t) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		req_size += sizeof(uint16_t);
	size = req_size;
	vq->avail = (struct vring_avail *)(uintptr_t)vhost_iova_to_vva(dev, vq,
						vq->ring_addrs.avail_user_addr,
						&size, VHOST_ACCESS_RW);
	if (!vq->avail || size != req_size)
		return -1;

	req_size = sizeof(struct vring_used);
	req_size += sizeof(struct vring_used_elem) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		req_size += sizeof(uint16_t);
	size = req_size;
	vq->used = (struct vring_used *)(uintptr_t)vhost_iova_to_vva(dev, vq,
						vq->ring_addrs.used_user_addr,
						&size, VHOST_ACCESS_RW);
	if (!vq->used || size != req_size)
		return -1;

out:
	vq->access_ok = 1;

	return 0;
}

void
vring_invalidate(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_wr_lock(vq);

	vq->access_ok = 0;
	vq->desc = NULL;
	vq->avail = NULL;
	vq->used = NULL;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_wr_unlock(vq);
}

static void
init_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;

	if (vring_idx >= VHOST_MAX_VRING) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Failed not init vring, out of bound (%d)\n",
				vring_idx);
		return;
	}

	vq = dev->virtqueue[vring_idx];

	memset(vq, 0, sizeof(struct vhost_virtqueue));

	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->callfd = VIRTIO_UNINITIALIZED_EVENTFD;

	vhost_user_iotlb_init(dev, vring_idx);
	/* Backends are set to -1 indicating an inactive device. */
	vq->backend = -1;

	TAILQ_INIT(&vq->zmbuf_list);
}

static void
reset_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;
	int callfd;

	if (vring_idx >= VHOST_MAX_VRING) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Failed not init vring, out of bound (%d)\n",
				vring_idx);
		return;
	}

	vq = dev->virtqueue[vring_idx];
	callfd = vq->callfd;
	init_vring_queue(dev, vring_idx);
	vq->callfd = callfd;
}

int
alloc_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;

	vq = rte_malloc(NULL, sizeof(struct vhost_virtqueue), 0);
	if (vq == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to allocate memory for vring:%u.\n", vring_idx);
		return -1;
	}

	dev->virtqueue[vring_idx] = vq;
	init_vring_queue(dev, vring_idx);
	rte_spinlock_init(&vq->access_lock);

	dev->nr_vring += 1;

	return 0;
}

/*
 * Reset some variables in device structure, while keeping few
 * others untouched, such as vid, ifname, nr_vring: they
 * should be same unless the device is removed.
 */
void
reset_device(struct virtio_net *dev)
{
	uint32_t i;

	dev->features = 0;
	dev->protocol_features = 0;
	dev->flags &= VIRTIO_DEV_BUILTIN_VIRTIO_NET;

	for (i = 0; i < dev->nr_vring; i++)
		reset_vring_queue(dev, i);
}

/*
 * Invoked when there is a new vhost-user connection established (when
 * there is a new virtio device being attached).
 */
int
vhost_new_device(void)
{
	struct virtio_net *dev;
	int i;

	dev = rte_zmalloc(NULL, sizeof(struct virtio_net), 0);
	if (dev == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to allocate memory for new dev.\n");
		return -1;
	}

	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		if (vhost_devices[i] == NULL)
			break;
	}
	if (i == MAX_VHOST_DEVICE) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to find a free slot for new device.\n");
		rte_free(dev);
		return -1;
	}

	vhost_devices[i] = dev;
	dev->vid = i;
	dev->flags = VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	dev->slave_req_fd = -1;
	dev->vdpa_dev_id = -1;

	return i;
}

/*
 * Invoked when there is the vhost-user connection is broken (when
 * the virtio device is being detached).
 */
void
vhost_destroy_device(int vid)
{
	struct virtio_net *dev = get_device(vid);
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	if (dev == NULL)
		return;

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		did = dev->vdpa_dev_id;
		vdpa_dev = rte_vdpa_get_device(did);
		if (vdpa_dev && vdpa_dev->ops->dev_close)
			vdpa_dev->ops->dev_close(dev->vid);
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		dev->notify_ops->destroy_device(vid);
	}

	cleanup_device(dev, 1);
	free_device(dev);

	vhost_devices[vid] = NULL;
}

void
vhost_attach_vdpa_device(int vid, int did)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	if (rte_vdpa_get_device(did) == NULL)
		return;

	dev->vdpa_dev_id = did;
}

void
vhost_detach_vdpa_device(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->vdpa_dev_id = -1;
}

void
vhost_set_ifname(int vid, const char *if_name, unsigned int if_len)
{
	struct virtio_net *dev;
	unsigned int len;

	dev = get_device(vid);
	if (dev == NULL)
		return;

	len = if_len > sizeof(dev->ifname) ?
		sizeof(dev->ifname) : if_len;

	strncpy(dev->ifname, if_name, len);
	dev->ifname[sizeof(dev->ifname) - 1] = '\0';
}

void
vhost_enable_dequeue_zero_copy(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->dequeue_zero_copy = 1;
}

void
vhost_set_builtin_virtio_net(int vid, bool enable)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	if (enable)
		dev->flags |= VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	else
		dev->flags &= ~VIRTIO_DEV_BUILTIN_VIRTIO_NET;
}

int
rte_vhost_get_mtu(int vid, uint16_t *mtu)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -ENODEV;

	if (!(dev->flags & VIRTIO_DEV_READY))
		return -EAGAIN;

	if (!(dev->features & (1ULL << VIRTIO_NET_F_MTU)))
		return -ENOTSUP;

	*mtu = dev->mtu;

	return 0;
}

int
rte_vhost_get_numa_node(int vid)
{
#ifdef RTE_LIBRTE_VHOST_NUMA
	struct virtio_net *dev = get_device(vid);
	int numa_node;
	int ret;

	if (dev == NULL)
		return -1;

	ret = get_mempolicy(&numa_node, NULL, 0, dev,
			    MPOL_F_NODE | MPOL_F_ADDR);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to query numa node: %s\n",
			vid, rte_strerror(errno));
		return -1;
	}

	return numa_node;
#else
	RTE_SET_USED(vid);
	return -1;
#endif
}

uint32_t
rte_vhost_get_queue_num(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return 0;

	return dev->nr_vring / 2;
}

uint16_t
rte_vhost_get_vring_num(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return 0;

	return dev->nr_vring;
}

int
rte_vhost_get_ifname(int vid, char *buf, size_t len)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return -1;

	len = RTE_MIN(len, sizeof(dev->ifname));

	strncpy(buf, dev->ifname, len);
	buf[len - 1] = '\0';

	return 0;
}

int
rte_vhost_get_negotiated_features(int vid, uint64_t *features)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (!dev)
		return -1;

	*features = dev->features;
	return 0;
}

int
rte_vhost_get_mem_table(int vid, struct rte_vhost_memory **mem)
{
	struct virtio_net *dev;
	struct rte_vhost_memory *m;
	size_t size;

	dev = get_device(vid);
	if (!dev)
		return -1;

	size = dev->mem->nregions * sizeof(struct rte_vhost_mem_region);
	m = malloc(sizeof(struct rte_vhost_memory) + size);
	if (!m)
		return -1;

	m->nregions = dev->mem->nregions;
	memcpy(m->regions, dev->mem->regions, size);
	*mem = m;

	return 0;
}

int
rte_vhost_get_vhost_vring(int vid, uint16_t vring_idx,
			  struct rte_vhost_vring *vring)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (!dev)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	vring->desc  = vq->desc;
	vring->avail = vq->avail;
	vring->used  = vq->used;
	vring->log_guest_addr  = vq->log_guest_addr;

	vring->callfd  = vq->callfd;
	vring->kickfd  = vq->kickfd;
	vring->size    = vq->size;

	return 0;
}

int
rte_vhost_vring_call(int vid, uint16_t vring_idx)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (!dev)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	vhost_vring_call(dev, vq);
	return 0;
}

uint16_t
rte_vhost_avail_entries(int vid, uint16_t queue_id)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (!dev)
		return 0;

	vq = dev->virtqueue[queue_id];
	if (!vq->enabled)
		return 0;

	return *(volatile uint16_t *)&vq->avail->idx - vq->last_used_idx;
}

int
rte_vhost_enable_guest_notification(int vid, uint16_t queue_id, int enable)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -1;

	if (enable)
		dev->virtqueue[queue_id]->used->flags &=
			~VRING_USED_F_NO_NOTIFY;
	else
		dev->virtqueue[queue_id]->used->flags |= VRING_USED_F_NO_NOTIFY;
	return 0;
}

void
rte_vhost_log_write(int vid, uint64_t addr, uint64_t len)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	vhost_log_write(dev, addr, len);
}

void
rte_vhost_log_used_vring(int vid, uint16_t vring_idx,
			 uint64_t offset, uint64_t len)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (dev == NULL)
		return;

	if (vring_idx >= VHOST_MAX_VRING)
		return;
	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return;

	vhost_log_used_vring(dev, vq, offset, len);
}

uint32_t
rte_vhost_rx_queue_count(int vid, uint16_t qid)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (dev == NULL)
		return 0;

	if (unlikely(qid >= dev->nr_vring || (qid & 1) == 0)) {
		RTE_LOG(ERR, VHOST_DATA, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, qid);
		return 0;
	}

	vq = dev->virtqueue[qid];
	if (vq == NULL)
		return 0;

	if (unlikely(vq->enabled == 0 || vq->avail == NULL))
		return 0;

	return *((volatile uint16_t *)&vq->avail->idx) - vq->last_avail_idx;
}

int rte_vhost_get_vdpa_device_id(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return -1;

	return dev->vdpa_dev_id;
}

int rte_vhost_get_log_base(int vid, uint64_t *log_base,
		uint64_t *log_size)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -1;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return -1;
	}

	*log_base = dev->log_base;
	*log_size = dev->log_size;

	return 0;
}

int rte_vhost_get_vring_base(int vid, uint16_t queue_id,
		uint16_t *last_avail_idx, uint16_t *last_used_idx)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -1;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return -1;
	}

	*last_avail_idx = dev->virtqueue[queue_id]->last_avail_idx;
	*last_used_idx = dev->virtqueue[queue_id]->last_used_idx;

	return 0;
}

int rte_vhost_set_vring_base(int vid, uint16_t queue_id,
		uint16_t last_avail_idx, uint16_t last_used_idx)
{
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -1;

	if (unlikely(!(dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET))) {
		RTE_LOG(ERR, VHOST_DATA,
			"(%d) %s: built-in vhost net backend is disabled.\n",
			dev->vid, __func__);
		return -1;
	}

	dev->virtqueue[queue_id]->last_avail_idx = last_avail_idx;
	dev->virtqueue[queue_id]->last_used_idx = last_used_idx;

	return 0;
}
