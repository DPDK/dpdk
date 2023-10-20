/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numa.h>
#include <numaif.h>
#endif

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_vhost.h>

#include "iotlb.h"
#include "vhost.h"
#include "vhost_user.h"

struct virtio_net *vhost_devices[RTE_MAX_VHOST_DEVICE];
pthread_mutex_t vhost_dev_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t vhost_dma_lock = PTHREAD_MUTEX_INITIALIZER;

struct vhost_vq_stats_name_off {
	char name[RTE_VHOST_STATS_NAME_SIZE];
	unsigned int offset;
};

static const struct vhost_vq_stats_name_off vhost_vq_stat_strings[] = {
	{"good_packets",           offsetof(struct vhost_virtqueue, stats.packets)},
	{"good_bytes",             offsetof(struct vhost_virtqueue, stats.bytes)},
	{"multicast_packets",      offsetof(struct vhost_virtqueue, stats.multicast)},
	{"broadcast_packets",      offsetof(struct vhost_virtqueue, stats.broadcast)},
	{"undersize_packets",      offsetof(struct vhost_virtqueue, stats.size_bins[0])},
	{"size_64_packets",        offsetof(struct vhost_virtqueue, stats.size_bins[1])},
	{"size_65_127_packets",    offsetof(struct vhost_virtqueue, stats.size_bins[2])},
	{"size_128_255_packets",   offsetof(struct vhost_virtqueue, stats.size_bins[3])},
	{"size_256_511_packets",   offsetof(struct vhost_virtqueue, stats.size_bins[4])},
	{"size_512_1023_packets",  offsetof(struct vhost_virtqueue, stats.size_bins[5])},
	{"size_1024_1518_packets", offsetof(struct vhost_virtqueue, stats.size_bins[6])},
	{"size_1519_max_packets",  offsetof(struct vhost_virtqueue, stats.size_bins[7])},
	{"guest_notifications",    offsetof(struct vhost_virtqueue, stats.guest_notifications)},
	{"guest_notifications_offloaded", offsetof(struct vhost_virtqueue,
		stats.guest_notifications_offloaded)},
	{"guest_notifications_error", offsetof(struct vhost_virtqueue,
		stats.guest_notifications_error)},
	{"guest_notifications_suppressed", offsetof(struct vhost_virtqueue,
		stats.guest_notifications_suppressed)},
	{"iotlb_hits",             offsetof(struct vhost_virtqueue, stats.iotlb_hits)},
	{"iotlb_misses",           offsetof(struct vhost_virtqueue, stats.iotlb_misses)},
	{"inflight_submitted",     offsetof(struct vhost_virtqueue, stats.inflight_submitted)},
	{"inflight_completed",     offsetof(struct vhost_virtqueue, stats.inflight_completed)},
};

#define VHOST_NB_VQ_STATS RTE_DIM(vhost_vq_stat_strings)

static int
vhost_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm)
{
	return dev->backend_ops->iotlb_miss(dev, iova, perm);
}

uint64_t
__vhost_iova_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
		    uint64_t iova, uint64_t *size, uint8_t perm)
{
	uint64_t vva, tmp_size;

	if (unlikely(!*size))
		return 0;

	tmp_size = *size;

	vva = vhost_user_iotlb_cache_find(dev, iova, &tmp_size, perm);
	if (tmp_size == *size) {
		if (dev->flags & VIRTIO_DEV_STATS_ENABLED)
			vq->stats.iotlb_hits++;
		return vva;
	}

	if (dev->flags & VIRTIO_DEV_STATS_ENABLED)
		vq->stats.iotlb_misses++;

	iova += tmp_size;

	if (!vhost_user_iotlb_pending_miss(dev, iova, perm)) {
		/*
		 * iotlb_lock is read-locked for a full burst,
		 * but it only protects the iotlb cache.
		 * In case of IOTLB miss, we might block on the socket,
		 * which could cause a deadlock with QEMU if an IOTLB update
		 * is being handled. We can safely unlock here to avoid it.
		 */
		vhost_user_iotlb_rd_unlock(vq);

		vhost_user_iotlb_pending_insert(dev, iova, perm);
		if (vhost_iotlb_miss(dev, iova, perm)) {
			VHOST_LOG_DATA(dev->ifname, ERR,
				"IOTLB miss req failed for IOVA 0x%" PRIx64 "\n",
				iova);
			vhost_user_iotlb_pending_remove(dev, iova, 1, perm);
		}

		vhost_user_iotlb_rd_lock(vq);
	}

	tmp_size = *size;
	/* Retry in case of VDUSE, as it is synchronous */
	vva = vhost_user_iotlb_cache_find(dev, iova, &tmp_size, perm);
	if (tmp_size == *size)
		return vva;

	return 0;
}

#define VHOST_LOG_PAGE	4096

/*
 * Atomically set a bit in memory.
 */
static __rte_always_inline void
vhost_set_bit(unsigned int nr, volatile uint8_t *addr)
{
#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION < 70100)
	/*
	 * __sync_ built-ins are deprecated, but rte_atomic_ ones
	 * are sub-optimized in older GCC versions.
	 */
	__sync_fetch_and_or_1(addr, (1U << nr));
#else
	rte_atomic_fetch_or_explicit((volatile uint8_t __rte_atomic *)addr, (1U << nr),
		rte_memory_order_relaxed);
#endif
}

static __rte_always_inline void
vhost_log_page(uint8_t *log_base, uint64_t page)
{
	vhost_set_bit(page % 8, &log_base[page / 8]);
}

void
__vhost_log_write(struct virtio_net *dev, uint64_t addr, uint64_t len)
{
	uint64_t page;

	if (unlikely(!dev->log_base || !len))
		return;

	if (unlikely(dev->log_size <= ((addr + len - 1) / VHOST_LOG_PAGE / 8)))
		return;

	/* To make sure guest memory updates are committed before logging */
	rte_atomic_thread_fence(rte_memory_order_release);

	page = addr / VHOST_LOG_PAGE;
	while (page * VHOST_LOG_PAGE < addr + len) {
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);
		page += 1;
	}
}

void
__vhost_log_write_iova(struct virtio_net *dev, struct vhost_virtqueue *vq,
			     uint64_t iova, uint64_t len)
{
	uint64_t hva, gpa, map_len;
	map_len = len;

	hva = __vhost_iova_to_vva(dev, vq, iova, &map_len, VHOST_ACCESS_RW);
	if (map_len != len) {
		VHOST_LOG_DATA(dev->ifname, ERR,
			"failed to write log for IOVA 0x%" PRIx64 ". No IOTLB entry found\n",
			iova);
		return;
	}

	gpa = hva_to_gpa(dev, hva, len);
	if (gpa)
		__vhost_log_write(dev, gpa, len);
}

void
__vhost_log_cache_sync(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	unsigned long *log_base;
	int i;

	if (unlikely(!dev->log_base))
		return;

	/* No cache, nothing to sync */
	if (unlikely(!vq->log_cache))
		return;

	rte_atomic_thread_fence(rte_memory_order_release);

	log_base = (unsigned long *)(uintptr_t)dev->log_base;

	for (i = 0; i < vq->log_cache_nb_elem; i++) {
		struct log_cache_entry *elem = vq->log_cache + i;

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION < 70100)
		/*
		 * '__sync' builtins are deprecated, but 'rte_atomic' ones
		 * are sub-optimized in older GCC versions.
		 */
		__sync_fetch_and_or(log_base + elem->offset, elem->val);
#else
		rte_atomic_fetch_or_explicit(
			(unsigned long __rte_atomic *)(log_base + elem->offset),
			elem->val, rte_memory_order_relaxed);
#endif
	}

	rte_atomic_thread_fence(rte_memory_order_release);

	vq->log_cache_nb_elem = 0;
}

static __rte_always_inline void
vhost_log_cache_page(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t page)
{
	uint32_t bit_nr = page % (sizeof(unsigned long) << 3);
	uint32_t offset = page / (sizeof(unsigned long) << 3);
	int i;

	if (unlikely(!vq->log_cache)) {
		/* No logging cache allocated, write dirty log map directly */
		rte_atomic_thread_fence(rte_memory_order_release);
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);

		return;
	}

	for (i = 0; i < vq->log_cache_nb_elem; i++) {
		struct log_cache_entry *elem = vq->log_cache + i;

		if (elem->offset == offset) {
			elem->val |= (1UL << bit_nr);
			return;
		}
	}

	if (unlikely(i >= VHOST_LOG_CACHE_NR)) {
		/*
		 * No more room for a new log cache entry,
		 * so write the dirty log map directly.
		 */
		rte_atomic_thread_fence(rte_memory_order_release);
		vhost_log_page((uint8_t *)(uintptr_t)dev->log_base, page);

		return;
	}

	vq->log_cache[i].offset = offset;
	vq->log_cache[i].val = (1UL << bit_nr);
	vq->log_cache_nb_elem++;
}

void
__vhost_log_cache_write(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t addr, uint64_t len)
{
	uint64_t page;

	if (unlikely(!dev->log_base || !len))
		return;

	if (unlikely(dev->log_size <= ((addr + len - 1) / VHOST_LOG_PAGE / 8)))
		return;

	page = addr / VHOST_LOG_PAGE;
	while (page * VHOST_LOG_PAGE < addr + len) {
		vhost_log_cache_page(dev, vq, page);
		page += 1;
	}
}

void
__vhost_log_cache_write_iova(struct virtio_net *dev, struct vhost_virtqueue *vq,
			     uint64_t iova, uint64_t len)
{
	uint64_t hva, gpa, map_len;
	map_len = len;

	hva = __vhost_iova_to_vva(dev, vq, iova, &map_len, VHOST_ACCESS_RW);
	if (map_len != len) {
		VHOST_LOG_DATA(dev->ifname, ERR,
			"failed to write log for IOVA 0x%" PRIx64 ". No IOTLB entry found\n",
			iova);
		return;
	}

	gpa = hva_to_gpa(dev, hva, len);
	if (gpa)
		__vhost_log_cache_write(dev, vq, gpa, len);
}

void *
vhost_alloc_copy_ind_table(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t desc_addr, uint64_t desc_len)
{
	void *idesc;
	uint64_t src, dst;
	uint64_t len, remain = desc_len;

	idesc = rte_malloc_socket(__func__, desc_len, 0, vq->numa_node);
	if (unlikely(!idesc))
		return NULL;

	dst = (uint64_t)(uintptr_t)idesc;

	while (remain) {
		len = remain;
		src = vhost_iova_to_vva(dev, vq, desc_addr, &len,
				VHOST_ACCESS_RO);
		if (unlikely(!src || !len)) {
			rte_free(idesc);
			return NULL;
		}

		rte_memcpy((void *)(uintptr_t)dst, (void *)(uintptr_t)src, len);

		remain -= len;
		dst += len;
		desc_addr += len;
	}

	return idesc;
}

void
cleanup_vq(struct vhost_virtqueue *vq, int destroy)
{
	if ((vq->callfd >= 0) && (destroy != 0))
		close(vq->callfd);
	if (vq->kickfd >= 0)
		close(vq->kickfd);
}

void
cleanup_vq_inflight(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	if (!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD)))
		return;

	if (vq_is_packed(dev)) {
		if (vq->inflight_packed)
			vq->inflight_packed = NULL;
	} else {
		if (vq->inflight_split)
			vq->inflight_split = NULL;
	}

	if (vq->resubmit_inflight) {
		if (vq->resubmit_inflight->resubmit_list) {
			rte_free(vq->resubmit_inflight->resubmit_list);
			vq->resubmit_inflight->resubmit_list = NULL;
		}
		rte_free(vq->resubmit_inflight);
		vq->resubmit_inflight = NULL;
	}
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

	for (i = 0; i < dev->nr_vring; i++) {
		cleanup_vq(dev->virtqueue[i], destroy);
		cleanup_vq_inflight(dev, dev->virtqueue[i]);
	}
}

static void
vhost_free_async_mem(struct vhost_virtqueue *vq)
	__rte_exclusive_locks_required(&vq->access_lock)
{
	if (!vq->async)
		return;

	rte_free(vq->async->pkts_info);
	rte_free(vq->async->pkts_cmpl_flag);

	rte_free(vq->async->buffers_packed);
	vq->async->buffers_packed = NULL;
	rte_free(vq->async->descs_split);
	vq->async->descs_split = NULL;

	rte_free(vq->async);
	vq->async = NULL;
}

void
free_vq(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	if (vq_is_packed(dev))
		rte_free(vq->shadow_used_packed);
	else
		rte_free(vq->shadow_used_split);

	rte_rwlock_write_lock(&vq->access_lock);
	vhost_free_async_mem(vq);
	rte_rwlock_write_unlock(&vq->access_lock);
	rte_free(vq->batch_copy_elems);
	rte_free(vq->log_cache);
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
		free_vq(dev, dev->virtqueue[i]);

	rte_free(dev);
}

static __rte_always_inline int
log_translate(struct virtio_net *dev, struct vhost_virtqueue *vq)
	__rte_shared_locks_required(&vq->iotlb_lock)
{
	if (likely(!(vq->ring_addrs.flags & (1 << VHOST_VRING_F_LOG))))
		return 0;

	vq->log_guest_addr = translate_log_addr(dev, vq,
						vq->ring_addrs.log_guest_addr);
	if (vq->log_guest_addr == 0)
		return -1;

	return 0;
}

/*
 * Converts vring log address to GPA
 * If IOMMU is enabled, the log address is IOVA
 * If IOMMU not enabled, the log address is already GPA
 */
uint64_t
translate_log_addr(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t log_addr)
{
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)) {
		const uint64_t exp_size = sizeof(uint64_t);
		uint64_t hva, gpa;
		uint64_t size = exp_size;

		hva = vhost_iova_to_vva(dev, vq, log_addr,
					&size, VHOST_ACCESS_RW);

		if (size != exp_size)
			return 0;

		gpa = hva_to_gpa(dev, hva, exp_size);
		if (!gpa) {
			VHOST_LOG_DATA(dev->ifname, ERR,
				"failed to find GPA for log_addr: 0x%"
				PRIx64 " hva: 0x%" PRIx64 "\n",
				log_addr, hva);
			return 0;
		}
		return gpa;

	} else
		return log_addr;
}

static int
vring_translate_split(struct virtio_net *dev, struct vhost_virtqueue *vq)
	__rte_shared_locks_required(&vq->iotlb_lock)
{
	uint64_t req_size, size;

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

	return 0;
}

static int
vring_translate_packed(struct virtio_net *dev, struct vhost_virtqueue *vq)
	__rte_shared_locks_required(&vq->iotlb_lock)
{
	uint64_t req_size, size;

	req_size = sizeof(struct vring_packed_desc) * vq->size;
	size = req_size;
	vq->desc_packed = (struct vring_packed_desc *)(uintptr_t)
		vhost_iova_to_vva(dev, vq, vq->ring_addrs.desc_user_addr,
				&size, VHOST_ACCESS_RW);
	if (!vq->desc_packed || size != req_size)
		return -1;

	req_size = sizeof(struct vring_packed_desc_event);
	size = req_size;
	vq->driver_event = (struct vring_packed_desc_event *)(uintptr_t)
		vhost_iova_to_vva(dev, vq, vq->ring_addrs.avail_user_addr,
				&size, VHOST_ACCESS_RW);
	if (!vq->driver_event || size != req_size)
		return -1;

	req_size = sizeof(struct vring_packed_desc_event);
	size = req_size;
	vq->device_event = (struct vring_packed_desc_event *)(uintptr_t)
		vhost_iova_to_vva(dev, vq, vq->ring_addrs.used_user_addr,
				&size, VHOST_ACCESS_RW);
	if (!vq->device_event || size != req_size)
		return -1;

	return 0;
}

int
vring_translate(struct virtio_net *dev, struct vhost_virtqueue *vq)
{

	if (!(dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)))
		return -1;

	if (vq_is_packed(dev)) {
		if (vring_translate_packed(dev, vq) < 0)
			return -1;
	} else {
		if (vring_translate_split(dev, vq) < 0)
			return -1;
	}

	if (log_translate(dev, vq) < 0)
		return -1;

	vq->access_ok = true;

	return 0;
}

void
vring_invalidate(struct virtio_net *dev __rte_unused, struct vhost_virtqueue *vq)
{
	vhost_user_iotlb_wr_lock(vq);

	vq->access_ok = false;
	vq->desc = NULL;
	vq->avail = NULL;
	vq->used = NULL;
	vq->log_guest_addr = 0;

	vhost_user_iotlb_wr_unlock(vq);
}

static void
init_vring_queue(struct virtio_net *dev __rte_unused, struct vhost_virtqueue *vq,
	uint32_t vring_idx)
{
	int numa_node = SOCKET_ID_ANY;

	memset(vq, 0, sizeof(struct vhost_virtqueue));

	vq->index = vring_idx;
	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->callfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->notif_enable = VIRTIO_UNINITIALIZED_NOTIF;

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&numa_node, NULL, 0, vq, MPOL_F_NODE | MPOL_F_ADDR)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to query numa node: %s\n",
			rte_strerror(errno));
		numa_node = SOCKET_ID_ANY;
	}
#endif
	vq->numa_node = numa_node;
}

static void
reset_vring_queue(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	int callfd;

	callfd = vq->callfd;
	init_vring_queue(dev, vq, vq->index);
	vq->callfd = callfd;
}

int
alloc_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;
	uint32_t i;

	/* Also allocate holes, if any, up to requested vring index. */
	for (i = 0; i <= vring_idx; i++) {
		if (dev->virtqueue[i])
			continue;

		vq = rte_zmalloc(NULL, sizeof(struct vhost_virtqueue), 0);
		if (vq == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for vring %u.\n",
				i);
			return -1;
		}

		dev->virtqueue[i] = vq;
		init_vring_queue(dev, vq, i);
		rte_rwlock_init(&vq->access_lock);
		rte_rwlock_init(&vq->iotlb_lock);
		vq->avail_wrap_counter = 1;
		vq->used_wrap_counter = 1;
		vq->signalled_used_valid = false;
	}

	dev->nr_vring = RTE_MAX(dev->nr_vring, vring_idx + 1);

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

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (!vq) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to reset vring, virtqueue not allocated (%d)\n", i);
			continue;
		}
		reset_vring_queue(dev, vq);
	}
}

/*
 * Invoked when there is a new vhost-user connection established (when
 * there is a new virtio device being attached).
 */
int
vhost_new_device(struct vhost_backend_ops *ops)
{
	struct virtio_net *dev;
	int i;

	if (ops == NULL) {
		VHOST_LOG_CONFIG("device", ERR, "missing backend ops.\n");
		return -1;
	}

	if (ops->iotlb_miss == NULL) {
		VHOST_LOG_CONFIG("device", ERR, "missing IOTLB miss backend op.\n");
		return -1;
	}

	if (ops->inject_irq == NULL) {
		VHOST_LOG_CONFIG("device", ERR, "missing IRQ injection backend op.\n");
		return -1;
	}

	pthread_mutex_lock(&vhost_dev_lock);
	for (i = 0; i < RTE_MAX_VHOST_DEVICE; i++) {
		if (vhost_devices[i] == NULL)
			break;
	}

	if (i == RTE_MAX_VHOST_DEVICE) {
		VHOST_LOG_CONFIG("device", ERR, "failed to find a free slot for new device.\n");
		pthread_mutex_unlock(&vhost_dev_lock);
		return -1;
	}

	dev = rte_zmalloc(NULL, sizeof(struct virtio_net), 0);
	if (dev == NULL) {
		VHOST_LOG_CONFIG("device", ERR, "failed to allocate memory for new device.\n");
		pthread_mutex_unlock(&vhost_dev_lock);
		return -1;
	}

	vhost_devices[i] = dev;
	pthread_mutex_unlock(&vhost_dev_lock);

	dev->vid = i;
	dev->flags = VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	dev->backend_req_fd = -1;
	dev->postcopy_ufd = -1;
	rte_spinlock_init(&dev->backend_req_lock);
	dev->backend_ops = ops;

	return i;
}

void
vhost_destroy_device_notify(struct virtio_net *dev)
{
	struct rte_vdpa_device *vdpa_dev;

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		vdpa_dev = dev->vdpa_dev;
		if (vdpa_dev)
			vdpa_dev->ops->dev_close(dev->vid);
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		dev->notify_ops->destroy_device(dev->vid);
	}
}

/*
 * Invoked when there is the vhost-user connection is broken (when
 * the virtio device is being detached).
 */
void
vhost_destroy_device(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	vhost_destroy_device_notify(dev);

	cleanup_device(dev, 1);
	free_device(dev);

	vhost_devices[vid] = NULL;
}

void
vhost_attach_vdpa_device(int vid, struct rte_vdpa_device *vdpa_dev)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->vdpa_dev = vdpa_dev;
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
vhost_setup_virtio_net(int vid, bool enable, bool compliant_ol_flags, bool stats_enabled,
	bool support_iommu)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	if (enable)
		dev->flags |= VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	else
		dev->flags &= ~VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	if (!compliant_ol_flags)
		dev->flags |= VIRTIO_DEV_LEGACY_OL_FLAGS;
	else
		dev->flags &= ~VIRTIO_DEV_LEGACY_OL_FLAGS;
	if (stats_enabled)
		dev->flags |= VIRTIO_DEV_STATS_ENABLED;
	else
		dev->flags &= ~VIRTIO_DEV_STATS_ENABLED;
	if (support_iommu)
		dev->flags |= VIRTIO_DEV_SUPPORT_IOMMU;
	else
		dev->flags &= ~VIRTIO_DEV_SUPPORT_IOMMU;

	if (vhost_user_iotlb_init(dev) < 0)
		VHOST_LOG_CONFIG("device", ERR, "failed to init IOTLB\n");

}

void
vhost_enable_extbuf(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->extbuf = 1;
}

void
vhost_enable_linearbuf(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->linearbuf = 1;
}

int
rte_vhost_get_mtu(int vid, uint16_t *mtu)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || mtu == NULL)
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

	if (dev == NULL || numa_available() != 0)
		return -1;

	ret = get_mempolicy(&numa_node, NULL, 0, dev,
			    MPOL_F_NODE | MPOL_F_ADDR);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to query numa node: %s\n",
			rte_strerror(errno));
		return -1;
	}

	return numa_node;
#else
	RTE_SET_USED(vid);
	return -1;
#endif
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

	if (dev == NULL || buf == NULL)
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
	if (dev == NULL || features == NULL)
		return -1;

	*features = dev->features;
	return 0;
}

int
rte_vhost_get_negotiated_protocol_features(int vid,
					   uint64_t *protocol_features)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL || protocol_features == NULL)
		return -1;

	*protocol_features = dev->protocol_features;
	return 0;
}

int
rte_vhost_get_mem_table(int vid, struct rte_vhost_memory **mem)
{
	struct virtio_net *dev;
	struct rte_vhost_memory *m;
	size_t size;

	dev = get_device(vid);
	if (dev == NULL || mem == NULL)
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
	if (dev == NULL || vring == NULL)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	if (vq_is_packed(dev)) {
		vring->desc_packed = vq->desc_packed;
		vring->driver_event = vq->driver_event;
		vring->device_event = vq->device_event;
	} else {
		vring->desc = vq->desc;
		vring->avail = vq->avail;
		vring->used = vq->used;
	}
	vring->log_guest_addr  = vq->log_guest_addr;

	vring->callfd  = vq->callfd;
	vring->kickfd  = vq->kickfd;
	vring->size    = vq->size;

	return 0;
}

int
rte_vhost_get_vhost_ring_inflight(int vid, uint16_t vring_idx,
				  struct rte_vhost_ring_inflight *vring)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	if (vq_is_packed(dev)) {
		if (unlikely(!vq->inflight_packed))
			return -1;

		vring->inflight_packed = vq->inflight_packed;
	} else {
		if (unlikely(!vq->inflight_split))
			return -1;

		vring->inflight_split = vq->inflight_split;
	}

	vring->resubmit_inflight = vq->resubmit_inflight;

	return 0;
}

int
rte_vhost_set_inflight_desc_split(int vid, uint16_t vring_idx,
				  uint16_t idx)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	if (unlikely(!vq->inflight_split))
		return -1;

	if (unlikely(idx >= vq->size))
		return -1;

	vq->inflight_split->desc[idx].counter = vq->global_counter++;
	vq->inflight_split->desc[idx].inflight = 1;
	return 0;
}

int
rte_vhost_set_inflight_desc_packed(int vid, uint16_t vring_idx,
				   uint16_t head, uint16_t last,
				   uint16_t *inflight_entry)
{
	struct rte_vhost_inflight_info_packed *inflight_info;
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	struct vring_packed_desc *desc;
	uint16_t old_free_head, free_head;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(!vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	inflight_info = vq->inflight_packed;
	if (unlikely(!inflight_info))
		return -1;

	if (unlikely(head >= vq->size))
		return -1;

	desc = vq->desc_packed;
	old_free_head = inflight_info->old_free_head;
	if (unlikely(old_free_head >= vq->size))
		return -1;

	free_head = old_free_head;

	/* init header descriptor */
	inflight_info->desc[old_free_head].num = 0;
	inflight_info->desc[old_free_head].counter = vq->global_counter++;
	inflight_info->desc[old_free_head].inflight = 1;

	/* save desc entry in flight entry */
	while (head != ((last + 1) % vq->size)) {
		inflight_info->desc[old_free_head].num++;
		inflight_info->desc[free_head].addr = desc[head].addr;
		inflight_info->desc[free_head].len = desc[head].len;
		inflight_info->desc[free_head].flags = desc[head].flags;
		inflight_info->desc[free_head].id = desc[head].id;

		inflight_info->desc[old_free_head].last = free_head;
		free_head = inflight_info->desc[free_head].next;
		inflight_info->free_head = free_head;
		head = (head + 1) % vq->size;
	}

	inflight_info->old_free_head = free_head;
	*inflight_entry = old_free_head;

	return 0;
}

int
rte_vhost_clr_inflight_desc_split(int vid, uint16_t vring_idx,
				  uint16_t last_used_idx, uint16_t idx)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	if (unlikely(!vq->inflight_split))
		return -1;

	if (unlikely(idx >= vq->size))
		return -1;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	vq->inflight_split->desc[idx].inflight = 0;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	vq->inflight_split->used_idx = last_used_idx;
	return 0;
}

int
rte_vhost_clr_inflight_desc_packed(int vid, uint16_t vring_idx,
				   uint16_t head)
{
	struct rte_vhost_inflight_info_packed *inflight_info;
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(!vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	inflight_info = vq->inflight_packed;
	if (unlikely(!inflight_info))
		return -1;

	if (unlikely(head >= vq->size))
		return -1;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	inflight_info->desc[head].inflight = 0;

	rte_atomic_thread_fence(rte_memory_order_seq_cst);

	inflight_info->old_free_head = inflight_info->free_head;
	inflight_info->old_used_idx = inflight_info->used_idx;
	inflight_info->old_used_wrap_counter = inflight_info->used_wrap_counter;

	return 0;
}

int
rte_vhost_set_last_inflight_io_split(int vid, uint16_t vring_idx,
				     uint16_t idx)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	if (unlikely(!vq->inflight_split))
		return -1;

	if (unlikely(idx >= vq->size))
		return -1;

	vq->inflight_split->last_inflight_io = idx;
	return 0;
}

int
rte_vhost_set_last_inflight_io_packed(int vid, uint16_t vring_idx,
				      uint16_t head)
{
	struct rte_vhost_inflight_info_packed *inflight_info;
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	uint16_t last;

	dev = get_device(vid);
	if (unlikely(!dev))
		return -1;

	if (unlikely(!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD))))
		return 0;

	if (unlikely(!vq_is_packed(dev)))
		return -1;

	if (unlikely(vring_idx >= VHOST_MAX_VRING))
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (unlikely(!vq))
		return -1;

	inflight_info = vq->inflight_packed;
	if (unlikely(!inflight_info))
		return -1;

	if (unlikely(head >= vq->size))
		return -1;

	last = inflight_info->desc[head].last;
	if (unlikely(last >= vq->size))
		return -1;

	inflight_info->desc[last].next = inflight_info->free_head;
	inflight_info->free_head = head;
	inflight_info->used_idx += inflight_info->desc[head].num;
	if (inflight_info->used_idx >= inflight_info->desc_num) {
		inflight_info->used_idx -= inflight_info->desc_num;
		inflight_info->used_wrap_counter =
			!inflight_info->used_wrap_counter;
	}

	return 0;
}

int
rte_vhost_vring_call(int vid, uint16_t vring_idx)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	int ret = 0;

	dev = get_device(vid);
	if (!dev)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	rte_rwlock_read_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	if (vq_is_packed(dev))
		vhost_vring_call_packed(dev, vq);
	else
		vhost_vring_call_split(dev, vq);

out_unlock:
	rte_rwlock_read_unlock(&vq->access_lock);

	return ret;
}

int
rte_vhost_vring_call_nonblock(int vid, uint16_t vring_idx)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	int ret = 0;

	dev = get_device(vid);
	if (!dev)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	if (rte_rwlock_read_trylock(&vq->access_lock))
		return -EAGAIN;

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	if (vq_is_packed(dev))
		vhost_vring_call_packed(dev, vq);
	else
		vhost_vring_call_split(dev, vq);

out_unlock:
	rte_rwlock_read_unlock(&vq->access_lock);

	return ret;
}

uint16_t
rte_vhost_avail_entries(int vid, uint16_t queue_id)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	uint16_t ret = 0;

	dev = get_device(vid);
	if (!dev)
		return 0;

	if (queue_id >= VHOST_MAX_VRING)
		return 0;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return 0;

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok))
		goto out;

	if (unlikely(!vq->enabled))
		goto out;

	ret = *(volatile uint16_t *)&vq->avail->idx - vq->last_used_idx;

out:
	rte_rwlock_write_unlock(&vq->access_lock);
	return ret;
}

static inline int
vhost_enable_notify_split(struct virtio_net *dev,
		struct vhost_virtqueue *vq, int enable)
{
	if (vq->used == NULL)
		return -1;

	if (!(dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))) {
		if (enable)
			vq->used->flags &= ~VRING_USED_F_NO_NOTIFY;
		else
			vq->used->flags |= VRING_USED_F_NO_NOTIFY;
	} else {
		if (enable)
			vhost_avail_event(vq) = vq->last_avail_idx;
	}
	return 0;
}

static inline int
vhost_enable_notify_packed(struct virtio_net *dev,
		struct vhost_virtqueue *vq, int enable)
{
	uint16_t flags;

	if (vq->device_event == NULL)
		return -1;

	if (!enable) {
		vq->device_event->flags = VRING_EVENT_F_DISABLE;
		return 0;
	}

	flags = VRING_EVENT_F_ENABLE;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX)) {
		flags = VRING_EVENT_F_DESC;
		vq->device_event->off_wrap = vq->last_avail_idx |
			vq->avail_wrap_counter << 15;
	}

	rte_atomic_thread_fence(rte_memory_order_release);

	vq->device_event->flags = flags;
	return 0;
}

int
vhost_enable_guest_notification(struct virtio_net *dev,
		struct vhost_virtqueue *vq, int enable)
{
	/*
	 * If the virtqueue is not ready yet, it will be applied
	 * when it will become ready.
	 */
	if (!vq->ready)
		return 0;

	if (vq_is_packed(dev))
		return vhost_enable_notify_packed(dev, vq, enable);
	else
		return vhost_enable_notify_split(dev, vq, enable);
}

int
rte_vhost_enable_guest_notification(int vid, uint16_t queue_id, int enable)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	int ret;

	if (!dev)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return -1;

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	vq->notif_enable = enable;
	ret = vhost_enable_guest_notification(dev, vq, enable);

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

void
rte_vhost_notify_guest(int vid, uint16_t queue_id)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;

	if (!dev ||  queue_id >= VHOST_MAX_VRING)
		return;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return;

	rte_rwlock_read_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok))
		goto out_unlock;

	rte_atomic_store_explicit(&vq->irq_pending, false, rte_memory_order_release);

	if (dev->backend_ops->inject_irq(dev, vq)) {
		if (dev->flags & VIRTIO_DEV_STATS_ENABLED)
			rte_atomic_fetch_add_explicit(&vq->stats.guest_notifications_error,
					1, rte_memory_order_relaxed);
	} else {
		if (dev->flags & VIRTIO_DEV_STATS_ENABLED)
			rte_atomic_fetch_add_explicit(&vq->stats.guest_notifications,
					1, rte_memory_order_relaxed);
		if (dev->notify_ops->guest_notified)
			dev->notify_ops->guest_notified(dev->vid);
	}

out_unlock:
	rte_rwlock_read_unlock(&vq->access_lock);
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
	uint32_t ret = 0;

	dev = get_device(vid);
	if (dev == NULL)
		return 0;

	if (unlikely(qid >= dev->nr_vring || (qid & 1) == 0)) {
		VHOST_LOG_DATA(dev->ifname, ERR,
			"%s: invalid virtqueue idx %d.\n",
			__func__, qid);
		return 0;
	}

	vq = dev->virtqueue[qid];
	if (vq == NULL)
		return 0;

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok))
		goto out;

	if (unlikely(!vq->enabled))
		goto out;

	ret = *((volatile uint16_t *)&vq->avail->idx) - vq->last_avail_idx;

out:
	rte_rwlock_write_unlock(&vq->access_lock);
	return ret;
}

struct rte_vdpa_device *
rte_vhost_get_vdpa_device(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return NULL;

	return dev->vdpa_dev;
}

int
rte_vhost_get_log_base(int vid, uint64_t *log_base,
		uint64_t *log_size)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || log_base == NULL || log_size == NULL)
		return -1;

	*log_base = dev->log_base;
	*log_size = dev->log_size;

	return 0;
}

int
rte_vhost_get_vring_base(int vid, uint16_t queue_id,
		uint16_t *last_avail_idx, uint16_t *last_used_idx)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || last_avail_idx == NULL || last_used_idx == NULL)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return -1;

	if (vq_is_packed(dev)) {
		*last_avail_idx = (vq->avail_wrap_counter << 15) |
				  vq->last_avail_idx;
		*last_used_idx = (vq->used_wrap_counter << 15) |
				 vq->last_used_idx;
	} else {
		*last_avail_idx = vq->last_avail_idx;
		*last_used_idx = vq->last_used_idx;
	}

	return 0;
}

int
rte_vhost_set_vring_base(int vid, uint16_t queue_id,
		uint16_t last_avail_idx, uint16_t last_used_idx)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);

	if (!dev)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return -1;

	if (vq_is_packed(dev)) {
		vq->last_avail_idx = last_avail_idx & 0x7fff;
		vq->avail_wrap_counter = !!(last_avail_idx & (1 << 15));
		vq->last_used_idx = last_used_idx & 0x7fff;
		vq->used_wrap_counter = !!(last_used_idx & (1 << 15));
	} else {
		vq->last_avail_idx = last_avail_idx;
		vq->last_used_idx = last_used_idx;
	}

	return 0;
}

int
rte_vhost_get_vring_base_from_inflight(int vid,
				       uint16_t queue_id,
				       uint16_t *last_avail_idx,
				       uint16_t *last_used_idx)
{
	struct rte_vhost_inflight_info_packed *inflight_info;
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || last_avail_idx == NULL || last_used_idx == NULL)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];
	if (!vq)
		return -1;

	if (!vq_is_packed(dev))
		return -1;

	inflight_info = vq->inflight_packed;
	if (!inflight_info)
		return -1;

	*last_avail_idx = (inflight_info->old_used_wrap_counter << 15) |
			  inflight_info->old_used_idx;
	*last_used_idx = *last_avail_idx;

	return 0;
}

int
rte_vhost_extern_callback_register(int vid,
		struct rte_vhost_user_extern_ops const * const ops, void *ctx)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || ops == NULL)
		return -1;

	dev->extern_ops = *ops;
	dev->extern_data = ctx;
	return 0;
}

static __rte_always_inline int
async_channel_register(struct virtio_net *dev, struct vhost_virtqueue *vq)
	__rte_exclusive_locks_required(&vq->access_lock)
{
	struct vhost_async *async;
	int node = vq->numa_node;

	if (unlikely(vq->async)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"async register failed: already registered (qid: %d)\n",
			vq->index);
		return -1;
	}

	async = rte_zmalloc_socket(NULL, sizeof(struct vhost_async), 0, node);
	if (!async) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to allocate async metadata (qid: %d)\n",
			vq->index);
		return -1;
	}

	async->pkts_info = rte_malloc_socket(NULL, vq->size * sizeof(struct async_inflight_info),
			RTE_CACHE_LINE_SIZE, node);
	if (!async->pkts_info) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to allocate async_pkts_info (qid: %d)\n",
			vq->index);
		goto out_free_async;
	}

	async->pkts_cmpl_flag = rte_zmalloc_socket(NULL, vq->size * sizeof(bool),
			RTE_CACHE_LINE_SIZE, node);
	if (!async->pkts_cmpl_flag) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to allocate async pkts_cmpl_flag (qid: %d)\n",
			vq->index);
		goto out_free_async;
	}

	if (vq_is_packed(dev)) {
		async->buffers_packed = rte_malloc_socket(NULL,
				vq->size * sizeof(struct vring_used_elem_packed),
				RTE_CACHE_LINE_SIZE, node);
		if (!async->buffers_packed) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate async buffers (qid: %d)\n",
				vq->index);
			goto out_free_inflight;
		}
	} else {
		async->descs_split = rte_malloc_socket(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE, node);
		if (!async->descs_split) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate async descs (qid: %d)\n",
				vq->index);
			goto out_free_inflight;
		}
	}

	vq->async = async;

	return 0;
out_free_inflight:
	rte_free(async->pkts_info);
out_free_async:
	rte_free(async);

	return -1;
}

int
rte_vhost_async_channel_register(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);
	int ret;

	if (dev == NULL)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];

	if (unlikely(vq == NULL || !dev->async_copy || dev->vdpa_dev != NULL))
		return -1;

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	ret = async_channel_register(dev, vq);

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

int
rte_vhost_async_channel_register_thread_unsafe(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];

	if (unlikely(vq == NULL || !dev->async_copy || dev->vdpa_dev != NULL))
		return -1;

	vq_assert_lock(dev, vq);

	return async_channel_register(dev, vq);
}

int
rte_vhost_async_channel_unregister(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);
	int ret = -1;

	if (dev == NULL)
		return ret;

	if (queue_id >= VHOST_MAX_VRING)
		return ret;

	vq = dev->virtqueue[queue_id];

	if (vq == NULL)
		return ret;

	if (rte_rwlock_write_trylock(&vq->access_lock)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to unregister async channel, virtqueue busy.\n");
		return ret;
	}

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	if (!vq->async) {
		ret = 0;
	} else if (vq->async->pkts_inflight_n) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to unregister async channel.\n");
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"inflight packets must be completed before unregistration.\n");
	} else {
		vhost_free_async_mem(vq);
		ret = 0;
	}

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

int
rte_vhost_async_channel_unregister_thread_unsafe(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return -1;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];

	if (vq == NULL)
		return -1;

	vq_assert_lock(dev, vq);

	if (!vq->async)
		return 0;

	if (vq->async->pkts_inflight_n) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to unregister async channel.\n");
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"inflight packets must be completed before unregistration.\n");
		return -1;
	}

	vhost_free_async_mem(vq);

	return 0;
}

int
rte_vhost_async_dma_configure(int16_t dma_id, uint16_t vchan_id)
{
	struct rte_dma_info info;
	void *pkts_cmpl_flag_addr;
	uint16_t max_desc;

	pthread_mutex_lock(&vhost_dma_lock);

	if (!rte_dma_is_valid(dma_id)) {
		VHOST_LOG_CONFIG("dma", ERR, "DMA %d is not found.\n", dma_id);
		goto error;
	}

	if (rte_dma_info_get(dma_id, &info) != 0) {
		VHOST_LOG_CONFIG("dma", ERR, "Fail to get DMA %d information.\n", dma_id);
		goto error;
	}

	if (vchan_id >= info.max_vchans) {
		VHOST_LOG_CONFIG("dma", ERR, "Invalid DMA %d vChannel %u.\n", dma_id, vchan_id);
		goto error;
	}

	if (!dma_copy_track[dma_id].vchans) {
		struct async_dma_vchan_info *vchans;

		vchans = rte_zmalloc(NULL, sizeof(struct async_dma_vchan_info) * info.max_vchans,
				RTE_CACHE_LINE_SIZE);
		if (vchans == NULL) {
			VHOST_LOG_CONFIG("dma", ERR,
				"Failed to allocate vchans for DMA %d vChannel %u.\n",
				dma_id, vchan_id);
			goto error;
		}

		dma_copy_track[dma_id].vchans = vchans;
	}

	if (dma_copy_track[dma_id].vchans[vchan_id].pkts_cmpl_flag_addr) {
		VHOST_LOG_CONFIG("dma", INFO, "DMA %d vChannel %u already registered.\n",
			dma_id, vchan_id);
		pthread_mutex_unlock(&vhost_dma_lock);
		return 0;
	}

	max_desc = info.max_desc;
	if (!rte_is_power_of_2(max_desc))
		max_desc = rte_align32pow2(max_desc);

	pkts_cmpl_flag_addr = rte_zmalloc(NULL, sizeof(bool *) * max_desc, RTE_CACHE_LINE_SIZE);
	if (!pkts_cmpl_flag_addr) {
		VHOST_LOG_CONFIG("dma", ERR,
			"Failed to allocate pkts_cmpl_flag_addr for DMA %d vChannel %u.\n",
			dma_id, vchan_id);

		if (dma_copy_track[dma_id].nr_vchans == 0) {
			rte_free(dma_copy_track[dma_id].vchans);
			dma_copy_track[dma_id].vchans = NULL;
		}
		goto error;
	}

	dma_copy_track[dma_id].vchans[vchan_id].pkts_cmpl_flag_addr = pkts_cmpl_flag_addr;
	dma_copy_track[dma_id].vchans[vchan_id].ring_size = max_desc;
	dma_copy_track[dma_id].vchans[vchan_id].ring_mask = max_desc - 1;
	dma_copy_track[dma_id].nr_vchans++;

	pthread_mutex_unlock(&vhost_dma_lock);
	return 0;

error:
	pthread_mutex_unlock(&vhost_dma_lock);
	return -1;
}

int
rte_vhost_async_get_inflight(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);
	int ret = -1;

	if (dev == NULL)
		return ret;

	if (queue_id >= VHOST_MAX_VRING)
		return ret;

	vq = dev->virtqueue[queue_id];

	if (vq == NULL)
		return ret;

	if (rte_rwlock_write_trylock(&vq->access_lock)) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG,
			"failed to check in-flight packets. virtqueue busy.\n");
		return ret;
	}

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	if (vq->async)
		ret = vq->async->pkts_inflight_n;

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

int
rte_vhost_async_get_inflight_thread_unsafe(int vid, uint16_t queue_id)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);
	int ret = -1;

	if (dev == NULL)
		return ret;

	if (queue_id >= VHOST_MAX_VRING)
		return ret;

	vq = dev->virtqueue[queue_id];

	if (vq == NULL)
		return ret;

	vq_assert_lock(dev, vq);

	if (!vq->async)
		return ret;

	ret = vq->async->pkts_inflight_n;

	return ret;
}

int
rte_vhost_get_monitor_addr(int vid, uint16_t queue_id,
		struct rte_vhost_power_monitor_cond *pmc)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	int ret = 0;

	if (dev == NULL)
		return -1;
	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];
	if (vq == NULL)
		return -1;

	rte_rwlock_read_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	if (vq_is_packed(dev)) {
		struct vring_packed_desc *desc;
		desc = vq->desc_packed;
		pmc->addr = &desc[vq->last_avail_idx].flags;
		if (vq->avail_wrap_counter)
			pmc->val = VRING_DESC_F_AVAIL;
		else
			pmc->val = VRING_DESC_F_USED;
		pmc->mask = VRING_DESC_F_AVAIL | VRING_DESC_F_USED;
		pmc->size = sizeof(desc[vq->last_avail_idx].flags);
		pmc->match = 1;
	} else {
		pmc->addr = &vq->avail->idx;
		pmc->val = vq->last_avail_idx & (vq->size - 1);
		pmc->mask = vq->size - 1;
		pmc->size = sizeof(vq->avail->idx);
		pmc->match = 0;
	}

out_unlock:
	rte_rwlock_read_unlock(&vq->access_lock);

	return ret;
}


int
rte_vhost_vring_stats_get_names(int vid, uint16_t queue_id,
		struct rte_vhost_stat_name *name, unsigned int size)
{
	struct virtio_net *dev = get_device(vid);
	unsigned int i;

	if (dev == NULL)
		return -1;

	if (queue_id >= dev->nr_vring)
		return -1;

	if (!(dev->flags & VIRTIO_DEV_STATS_ENABLED))
		return -1;

	if (name == NULL || size < VHOST_NB_VQ_STATS)
		return VHOST_NB_VQ_STATS;

	for (i = 0; i < VHOST_NB_VQ_STATS; i++)
		snprintf(name[i].name, sizeof(name[i].name), "%s_q%u_%s",
				(queue_id & 1) ? "rx" : "tx",
				queue_id / 2, vhost_vq_stat_strings[i].name);

	return VHOST_NB_VQ_STATS;
}

int
rte_vhost_vring_stats_get(int vid, uint16_t queue_id,
		struct rte_vhost_stat *stats, unsigned int n)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	unsigned int i;
	int ret = VHOST_NB_VQ_STATS;

	if (dev == NULL)
		return -1;

	if (queue_id >= dev->nr_vring)
		return -1;

	if (!(dev->flags & VIRTIO_DEV_STATS_ENABLED))
		return -1;

	if (stats == NULL || n < VHOST_NB_VQ_STATS)
		return VHOST_NB_VQ_STATS;

	vq = dev->virtqueue[queue_id];

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}

	for (i = 0; i < VHOST_NB_VQ_STATS; i++) {
		/*
		 * No need to the read atomic counters as such, due to the
		 * above write access_lock preventing them to be updated.
		 */
		stats[i].value =
			*(uint64_t *)(((char *)vq) + vhost_vq_stat_strings[i].offset);
		stats[i].id = i;
	}

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

int rte_vhost_vring_stats_reset(int vid, uint16_t queue_id)
{
	struct virtio_net *dev = get_device(vid);
	struct vhost_virtqueue *vq;
	int ret = 0;

	if (dev == NULL)
		return -1;

	if (queue_id >= dev->nr_vring)
		return -1;

	if (!(dev->flags & VIRTIO_DEV_STATS_ENABLED))
		return -1;

	vq = dev->virtqueue[queue_id];

	rte_rwlock_write_lock(&vq->access_lock);

	if (unlikely(!vq->access_ok)) {
		ret = -1;
		goto out_unlock;
	}
	/*
	 * No need to the reset atomic counters as such, due to the
	 * above write access_lock preventing them to be updated.
	 */
	memset(&vq->stats, 0, sizeof(vq->stats));

out_unlock:
	rte_rwlock_write_unlock(&vq->access_lock);

	return ret;
}

int
rte_vhost_async_dma_unconfigure(int16_t dma_id, uint16_t vchan_id)
{
	struct rte_dma_info info;
	struct rte_dma_stats stats = { 0 };

	pthread_mutex_lock(&vhost_dma_lock);

	if (!rte_dma_is_valid(dma_id)) {
		VHOST_LOG_CONFIG("dma", ERR, "DMA %d is not found.\n", dma_id);
		goto error;
	}

	if (rte_dma_info_get(dma_id, &info) != 0) {
		VHOST_LOG_CONFIG("dma", ERR, "Fail to get DMA %d information.\n", dma_id);
		goto error;
	}

	if (vchan_id >= info.max_vchans || !dma_copy_track[dma_id].vchans ||
		!dma_copy_track[dma_id].vchans[vchan_id].pkts_cmpl_flag_addr) {
		VHOST_LOG_CONFIG("dma", ERR, "Invalid channel %d:%u.\n", dma_id, vchan_id);
		goto error;
	}

	if (rte_dma_stats_get(dma_id, vchan_id, &stats) != 0) {
		VHOST_LOG_CONFIG("dma", ERR,
				 "Failed to get stats for DMA %d vChannel %u.\n", dma_id, vchan_id);
		goto error;
	}

	if (stats.submitted - stats.completed != 0) {
		VHOST_LOG_CONFIG("dma", ERR,
				 "Do not unconfigure when there are inflight packets.\n");
		goto error;
	}

	rte_free(dma_copy_track[dma_id].vchans[vchan_id].pkts_cmpl_flag_addr);
	dma_copy_track[dma_id].vchans[vchan_id].pkts_cmpl_flag_addr = NULL;
	dma_copy_track[dma_id].nr_vchans--;

	if (dma_copy_track[dma_id].nr_vchans == 0) {
		rte_free(dma_copy_track[dma_id].vchans);
		dma_copy_track[dma_id].vchans = NULL;
	}

	pthread_mutex_unlock(&vhost_dma_lock);
	return 0;

error:
	pthread_mutex_unlock(&vhost_dma_lock);
	return -1;
}

RTE_LOG_REGISTER_SUFFIX(vhost_config_log_level, config, INFO);
RTE_LOG_REGISTER_SUFFIX(vhost_data_log_level, data, WARNING);
