/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numa.h>
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
pthread_mutex_t vhost_dev_lock = PTHREAD_MUTEX_INITIALIZER;

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
			VHOST_LOG_CONFIG(ERR,
				"IOTLB miss req failed for IOVA 0x%" PRIx64 "\n",
				iova);
			vhost_user_iotlb_pending_remove(vq, iova, 1, perm);
		}

		vhost_user_iotlb_rd_lock(vq);
	}

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
	 * __sync_ built-ins are deprecated, but __atomic_ ones
	 * are sub-optimized in older GCC versions.
	 */
	__sync_fetch_and_or_1(addr, (1U << nr));
#else
	__atomic_fetch_or(addr, (1U << nr), __ATOMIC_RELAXED);
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
	rte_smp_wmb();

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
		VHOST_LOG_DATA(ERR,
			"Failed to write log for IOVA 0x%" PRIx64 ". No IOTLB entry found\n",
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

	rte_smp_wmb();

	log_base = (unsigned long *)(uintptr_t)dev->log_base;

	for (i = 0; i < vq->log_cache_nb_elem; i++) {
		struct log_cache_entry *elem = vq->log_cache + i;

#if defined(RTE_TOOLCHAIN_GCC) && (GCC_VERSION < 70100)
		/*
		 * '__sync' builtins are deprecated, but '__atomic' ones
		 * are sub-optimized in older GCC versions.
		 */
		__sync_fetch_and_or(log_base + elem->offset, elem->val);
#else
		__atomic_fetch_or(log_base + elem->offset, elem->val,
				__ATOMIC_RELAXED);
#endif
	}

	rte_smp_wmb();

	vq->log_cache_nb_elem = 0;
}

static __rte_always_inline void
vhost_log_cache_page(struct virtio_net *dev, struct vhost_virtqueue *vq,
			uint64_t page)
{
	uint32_t bit_nr = page % (sizeof(unsigned long) << 3);
	uint32_t offset = page / (sizeof(unsigned long) << 3);
	int i;

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
		rte_smp_wmb();
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
		VHOST_LOG_DATA(ERR,
			"Failed to write log for IOVA 0x%" PRIx64 ". No IOTLB entry found\n",
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

	idesc = rte_malloc(__func__, desc_len, 0);
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
			free(vq->resubmit_inflight->resubmit_list);
			vq->resubmit_inflight->resubmit_list = NULL;
		}
		free(vq->resubmit_inflight);
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
{
	if (vq->async_pkts_pending)
		rte_free(vq->async_pkts_pending);
	if (vq->async_pkts_info)
		rte_free(vq->async_pkts_info);
	if (vq->it_pool)
		rte_free(vq->it_pool);
	if (vq->vec_pool)
		rte_free(vq->vec_pool);

	vq->async_pkts_pending = NULL;
	vq->async_pkts_info = NULL;
	vq->it_pool = NULL;
	vq->vec_pool = NULL;
}

void
free_vq(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	if (vq_is_packed(dev))
		rte_free(vq->shadow_used_packed);
	else {
		rte_free(vq->shadow_used_split);
		vhost_free_async_mem(vq);
	}
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
		free_vq(dev, dev->virtqueue[i]);

	rte_free(dev);
}

static __rte_always_inline int
log_translate(struct virtio_net *dev, struct vhost_virtqueue *vq)
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
 *
 * Caller should have iotlb_lock read-locked
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
			VHOST_LOG_CONFIG(ERR,
				"VQ: Failed to find GPA for log_addr: 0x%"
				PRIx64 " hva: 0x%" PRIx64 "\n",
				log_addr, hva);
			return 0;
		}
		return gpa;

	} else
		return log_addr;
}

/* Caller should have iotlb_lock read-locked */
static int
vring_translate_split(struct virtio_net *dev, struct vhost_virtqueue *vq)
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

/* Caller should have iotlb_lock read-locked */
static int
vring_translate_packed(struct virtio_net *dev, struct vhost_virtqueue *vq)
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
	vq->log_guest_addr = 0;

	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		vhost_user_iotlb_wr_unlock(vq);
}

static void
init_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;

	if (vring_idx >= VHOST_MAX_VRING) {
		VHOST_LOG_CONFIG(ERR,
				"Failed not init vring, out of bound (%d)\n",
				vring_idx);
		return;
	}

	vq = dev->virtqueue[vring_idx];
	if (!vq) {
		VHOST_LOG_CONFIG(ERR, "Virtqueue not allocated (%d)\n",
				vring_idx);
		return;
	}

	memset(vq, 0, sizeof(struct vhost_virtqueue));

	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->callfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->notif_enable = VIRTIO_UNINITIALIZED_NOTIF;

	vhost_user_iotlb_init(dev, vring_idx);
	/* Backends are set to -1 indicating an inactive device. */
	vq->backend = -1;
}

static void
reset_vring_queue(struct virtio_net *dev, uint32_t vring_idx)
{
	struct vhost_virtqueue *vq;
	int callfd;

	if (vring_idx >= VHOST_MAX_VRING) {
		VHOST_LOG_CONFIG(ERR,
				"Failed not init vring, out of bound (%d)\n",
				vring_idx);
		return;
	}

	vq = dev->virtqueue[vring_idx];
	if (!vq) {
		VHOST_LOG_CONFIG(ERR, "Virtqueue not allocated (%d)\n",
				vring_idx);
		return;
	}

	callfd = vq->callfd;
	init_vring_queue(dev, vring_idx);
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
			VHOST_LOG_CONFIG(ERR,
				"Failed to allocate memory for vring:%u.\n", i);
			return -1;
		}

		dev->virtqueue[i] = vq;
		init_vring_queue(dev, i);
		rte_spinlock_init(&vq->access_lock);
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

	pthread_mutex_lock(&vhost_dev_lock);
	for (i = 0; i < MAX_VHOST_DEVICE; i++) {
		if (vhost_devices[i] == NULL)
			break;
	}

	if (i == MAX_VHOST_DEVICE) {
		VHOST_LOG_CONFIG(ERR,
			"Failed to find a free slot for new device.\n");
		pthread_mutex_unlock(&vhost_dev_lock);
		return -1;
	}

	dev = rte_zmalloc(NULL, sizeof(struct virtio_net), 0);
	if (dev == NULL) {
		VHOST_LOG_CONFIG(ERR,
			"Failed to allocate memory for new dev.\n");
		pthread_mutex_unlock(&vhost_dev_lock);
		return -1;
	}

	vhost_devices[i] = dev;
	pthread_mutex_unlock(&vhost_dev_lock);

	dev->vid = i;
	dev->flags = VIRTIO_DEV_BUILTIN_VIRTIO_NET;
	dev->slave_req_fd = -1;
	dev->postcopy_ufd = -1;
	rte_spinlock_init(&dev->slave_req_lock);

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
vhost_setup_virtio_net(int vid, bool enable, bool compliant_ol_flags)
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
		VHOST_LOG_CONFIG(ERR,
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

	rte_smp_mb();

	vq->inflight_split->desc[idx].inflight = 0;

	rte_smp_mb();

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

	rte_smp_mb();

	inflight_info->desc[head].inflight = 0;

	rte_smp_mb();

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

	dev = get_device(vid);
	if (!dev)
		return -1;

	if (vring_idx >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[vring_idx];
	if (!vq)
		return -1;

	if (vq_is_packed(dev))
		vhost_vring_call_packed(dev, vq);
	else
		vhost_vring_call_split(dev, vq);

	return 0;
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

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(!vq->enabled || vq->avail == NULL))
		goto out;

	ret = *(volatile uint16_t *)&vq->avail->idx - vq->last_used_idx;

out:
	rte_spinlock_unlock(&vq->access_lock);
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

	rte_smp_wmb();

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

	rte_spinlock_lock(&vq->access_lock);

	vq->notif_enable = enable;
	ret = vhost_enable_guest_notification(dev, vq, enable);

	rte_spinlock_unlock(&vq->access_lock);

	return ret;
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
		VHOST_LOG_DATA(ERR, "(%d) %s: invalid virtqueue idx %d.\n",
			dev->vid, __func__, qid);
		return 0;
	}

	vq = dev->virtqueue[qid];
	if (vq == NULL)
		return 0;

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(vq->enabled == 0 || vq->avail == NULL))
		goto out;

	ret = *((volatile uint16_t *)&vq->avail->idx) - vq->last_avail_idx;

out:
	rte_spinlock_unlock(&vq->access_lock);
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

int rte_vhost_get_log_base(int vid, uint64_t *log_base,
		uint64_t *log_size)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || log_base == NULL || log_size == NULL)
		return -1;

	*log_base = dev->log_base;
	*log_size = dev->log_size;

	return 0;
}

int rte_vhost_get_vring_base(int vid, uint16_t queue_id,
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

int rte_vhost_set_vring_base(int vid, uint16_t queue_id,
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

int rte_vhost_extern_callback_register(int vid,
		struct rte_vhost_user_extern_ops const * const ops, void *ctx)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL || ops == NULL)
		return -1;

	dev->extern_ops = *ops;
	dev->extern_data = ctx;
	return 0;
}

int rte_vhost_async_channel_register(int vid, uint16_t queue_id,
					uint32_t features,
					struct rte_vhost_async_channel_ops *ops)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = get_device(vid);
	struct rte_vhost_async_features f;
	int node;

	if (dev == NULL || ops == NULL)
		return -1;

	f.intval = features;

	if (queue_id >= VHOST_MAX_VRING)
		return -1;

	vq = dev->virtqueue[queue_id];

	if (unlikely(vq == NULL || !dev->async_copy))
		return -1;

	/* packed queue is not supported */
	if (unlikely(vq_is_packed(dev) || !f.async_inorder)) {
		VHOST_LOG_CONFIG(ERR,
			"async copy is not supported on packed queue or non-inorder mode "
			"(vid %d, qid: %d)\n", vid, queue_id);
		return -1;
	}

	if (unlikely(ops->check_completed_copies == NULL ||
		ops->transfer_data == NULL))
		return -1;

	rte_spinlock_lock(&vq->access_lock);

	if (unlikely(vq->async_registered)) {
		VHOST_LOG_CONFIG(ERR,
			"async register failed: channel already registered "
			"(vid %d, qid: %d)\n", vid, queue_id);
		goto reg_out;
	}

#ifdef RTE_LIBRTE_VHOST_NUMA
	if (get_mempolicy(&node, NULL, 0, vq, MPOL_F_NODE | MPOL_F_ADDR)) {
		VHOST_LOG_CONFIG(ERR,
			"unable to get numa information in async register. "
			"allocating async buffer memory on the caller thread node\n");
		node = SOCKET_ID_ANY;
	}
#else
	node = SOCKET_ID_ANY;
#endif

	vq->async_pkts_pending = rte_malloc_socket(NULL,
			vq->size * sizeof(uintptr_t),
			RTE_CACHE_LINE_SIZE, node);
	vq->async_pkts_info = rte_malloc_socket(NULL,
			vq->size * sizeof(struct async_inflight_info),
			RTE_CACHE_LINE_SIZE, node);
	vq->it_pool = rte_malloc_socket(NULL,
			VHOST_MAX_ASYNC_IT * sizeof(struct rte_vhost_iov_iter),
			RTE_CACHE_LINE_SIZE, node);
	vq->vec_pool = rte_malloc_socket(NULL,
			VHOST_MAX_ASYNC_VEC * sizeof(struct iovec),
			RTE_CACHE_LINE_SIZE, node);
	if (!vq->async_pkts_pending || !vq->async_pkts_info ||
		!vq->it_pool || !vq->vec_pool) {
		vhost_free_async_mem(vq);
		VHOST_LOG_CONFIG(ERR,
				"async register failed: cannot allocate memory for vq data "
				"(vid %d, qid: %d)\n", vid, queue_id);
		goto reg_out;
	}

	vq->async_ops.check_completed_copies = ops->check_completed_copies;
	vq->async_ops.transfer_data = ops->transfer_data;

	vq->async_inorder = f.async_inorder;
	vq->async_threshold = f.async_threshold;

	vq->async_registered = true;

reg_out:
	rte_spinlock_unlock(&vq->access_lock);

	return 0;
}

int rte_vhost_async_channel_unregister(int vid, uint16_t queue_id)
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

	ret = 0;

	if (!vq->async_registered)
		return ret;

	if (!rte_spinlock_trylock(&vq->access_lock)) {
		VHOST_LOG_CONFIG(ERR, "Failed to unregister async channel. "
			"virt queue busy.\n");
		return -1;
	}

	if (vq->async_pkts_inflight_n) {
		VHOST_LOG_CONFIG(ERR, "Failed to unregister async channel. "
			"async inflight packets must be completed before unregistration.\n");
		ret = -1;
		goto out;
	}

	vhost_free_async_mem(vq);

	vq->async_ops.transfer_data = NULL;
	vq->async_ops.check_completed_copies = NULL;
	vq->async_registered = false;

out:
	rte_spinlock_unlock(&vq->access_lock);

	return ret;
}

RTE_LOG_REGISTER(vhost_config_log_level, lib.vhost.config, INFO);
RTE_LOG_REGISTER(vhost_data_log_level, lib.vhost.data, WARNING);
