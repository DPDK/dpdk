/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

/* Security model
 * --------------
 * The vhost-user protocol connection is an external interface, so it must be
 * robust against invalid inputs.
 *
 * This is important because the vhost-user frontend is only one step removed
 * from the guest.  Malicious guests that have escaped will then launch further
 * attacks from the vhost-user frontend.
 *
 * Even in deployments where guests are trusted, a bug in the vhost-user frontend
 * can still cause invalid messages to be sent.  Such messages must not
 * compromise the stability of the DPDK application by causing crashes, memory
 * corruption, or other problematic behavior.
 *
 * Do not assume received VhostUserMsg fields contain sensible values!
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif
#ifdef RTE_LIBRTE_VHOST_POSTCOPY
#include <linux/userfaultfd.h>
#endif
#ifdef F_ADD_SEALS /* if file sealing is supported, so is memfd */
#include <linux/memfd.h>
#define MEMFD_SUPPORTED
#endif

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_vfio.h>
#include <rte_errno.h>

#include "iotlb.h"
#include "vhost.h"
#include "vhost_user.h"

#define VIRTIO_MIN_MTU 68
#define VIRTIO_MAX_MTU 65535

#define INFLIGHT_ALIGNMENT	64
#define INFLIGHT_VERSION	0x1

typedef struct vhost_message_handler {
	const char *description;
	int (*callback)(struct virtio_net **pdev, struct vhu_msg_context *ctx,
		int main_fd);
	bool accepts_fd;
} vhost_message_handler_t;
static vhost_message_handler_t vhost_message_handlers[];

static int send_vhost_reply(struct virtio_net *dev, int sockfd, struct vhu_msg_context *ctx);
static int read_vhost_message(struct virtio_net *dev, int sockfd, struct vhu_msg_context *ctx);

static void
close_msg_fds(struct vhu_msg_context *ctx)
{
	int i;

	for (i = 0; i < ctx->fd_num; i++) {
		int fd = ctx->fds[i];

		if (fd == -1)
			continue;

		ctx->fds[i] = -1;
		close(fd);
	}
}

/*
 * Ensure the expected number of FDs is received,
 * close all FDs and return an error if this is not the case.
 */
static int
validate_msg_fds(struct virtio_net *dev, struct vhu_msg_context *ctx, int expected_fds)
{
	if (ctx->fd_num == expected_fds)
		return 0;

	VHOST_LOG_CONFIG(dev->ifname, ERR,
		"expect %d FDs for request %s, received %d\n",
		expected_fds, vhost_message_handlers[ctx->msg.request.frontend].description,
		ctx->fd_num);

	close_msg_fds(ctx);

	return -1;
}

static uint64_t
get_blk_size(int fd)
{
	struct stat stat;
	int ret;

	ret = fstat(fd, &stat);
	return ret == -1 ? (uint64_t)-1 : (uint64_t)stat.st_blksize;
}

static void
async_dma_map(struct virtio_net *dev, bool do_map)
{
	int ret = 0;
	uint32_t i;
	struct guest_page *page;

	if (do_map) {
		for (i = 0; i < dev->nr_guest_pages; i++) {
			page = &dev->guest_pages[i];
			ret = rte_vfio_container_dma_map(RTE_VFIO_DEFAULT_CONTAINER_FD,
							 page->host_user_addr,
							 page->host_iova,
							 page->size);
			if (ret) {
				/*
				 * DMA device may bind with kernel driver, in this case,
				 * we don't need to program IOMMU manually. However, if no
				 * device is bound with vfio/uio in DPDK, and vfio kernel
				 * module is loaded, the API will still be called and return
				 * with ENODEV.
				 *
				 * DPDK vfio only returns ENODEV in very similar situations
				 * (vfio either unsupported, or supported but no devices found).
				 * Either way, no mappings could be performed. We treat it as
				 * normal case in async path. This is a workaround.
				 */
				if (rte_errno == ENODEV)
					return;

				/* DMA mapping errors won't stop VHOST_USER_SET_MEM_TABLE. */
				VHOST_LOG_CONFIG(dev->ifname, ERR, "DMA engine map failed\n");
			}
		}

	} else {
		for (i = 0; i < dev->nr_guest_pages; i++) {
			page = &dev->guest_pages[i];
			ret = rte_vfio_container_dma_unmap(RTE_VFIO_DEFAULT_CONTAINER_FD,
							   page->host_user_addr,
							   page->host_iova,
							   page->size);
			if (ret) {
				/* like DMA map, ignore the kernel driver case when unmap. */
				if (rte_errno == EINVAL)
					return;

				VHOST_LOG_CONFIG(dev->ifname, ERR, "DMA engine unmap failed\n");
			}
		}
	}
}

static void
free_mem_region(struct virtio_net *dev)
{
	uint32_t i;
	struct rte_vhost_mem_region *reg;

	if (!dev || !dev->mem)
		return;

	if (dev->async_copy && rte_vfio_is_enabled("vfio"))
		async_dma_map(dev, false);

	for (i = 0; i < dev->mem->nregions; i++) {
		reg = &dev->mem->regions[i];
		if (reg->host_user_addr) {
			munmap(reg->mmap_addr, reg->mmap_size);
			close(reg->fd);
		}
	}
}

void
vhost_backend_cleanup(struct virtio_net *dev)
{
	struct rte_vdpa_device *vdpa_dev;

	vdpa_dev = dev->vdpa_dev;
	if (vdpa_dev && vdpa_dev->ops->dev_cleanup != NULL)
		vdpa_dev->ops->dev_cleanup(dev->vid);

	if (dev->mem) {
		free_mem_region(dev);
		rte_free(dev->mem);
		dev->mem = NULL;
	}

	rte_free(dev->guest_pages);
	dev->guest_pages = NULL;

	if (dev->log_addr) {
		munmap((void *)(uintptr_t)dev->log_addr, dev->log_size);
		dev->log_addr = 0;
	}

	if (dev->inflight_info) {
		if (dev->inflight_info->addr) {
			munmap(dev->inflight_info->addr,
			       dev->inflight_info->size);
			dev->inflight_info->addr = NULL;
		}

		if (dev->inflight_info->fd >= 0) {
			close(dev->inflight_info->fd);
			dev->inflight_info->fd = -1;
		}

		rte_free(dev->inflight_info);
		dev->inflight_info = NULL;
	}

	if (dev->backend_req_fd >= 0) {
		close(dev->backend_req_fd);
		dev->backend_req_fd = -1;
	}

	if (dev->postcopy_ufd >= 0) {
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
	}

	dev->postcopy_listening = 0;

	vhost_user_iotlb_destroy(dev);
}

static void
vhost_user_notify_queue_state(struct virtio_net *dev, struct vhost_virtqueue *vq,
	int enable)
{
	struct rte_vdpa_device *vdpa_dev = dev->vdpa_dev;

	/* Configure guest notifications on enable */
	if (enable && vq->notif_enable != VIRTIO_UNINITIALIZED_NOTIF)
		vhost_enable_guest_notification(dev, vq, vq->notif_enable);

	if (vdpa_dev && vdpa_dev->ops->set_vring_state)
		vdpa_dev->ops->set_vring_state(dev->vid, vq->index, enable);

	if (dev->notify_ops->vring_state_changed)
		dev->notify_ops->vring_state_changed(dev->vid, vq->index, enable);
}

/*
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
static int
vhost_user_set_owner(struct virtio_net **pdev __rte_unused,
			struct vhu_msg_context *ctx __rte_unused,
			int main_fd __rte_unused)
{
	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_reset_owner(struct virtio_net **pdev,
			struct vhu_msg_context *ctx __rte_unused,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	vhost_destroy_device_notify(dev);

	cleanup_device(dev, 0);
	reset_device(dev);
	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * The features that we support are requested.
 */
static int
vhost_user_get_features(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t features = 0;

	rte_vhost_driver_get_features(dev->ifname, &features);

	ctx->msg.payload.u64 = features;
	ctx->msg.size = sizeof(ctx->msg.payload.u64);
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * The queue number that we support are requested.
 */
static int
vhost_user_get_queue_num(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint32_t queue_num = 0;

	rte_vhost_driver_get_queue_num(dev->ifname, &queue_num);

	ctx->msg.payload.u64 = (uint64_t)queue_num;
	ctx->msg.size = sizeof(ctx->msg.payload.u64);
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * We receive the negotiated features supported by us and the virtio device.
 */
static int
vhost_user_set_features(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t features = ctx->msg.payload.u64;
	uint64_t vhost_features = 0;
	struct rte_vdpa_device *vdpa_dev;

	rte_vhost_driver_get_features(dev->ifname, &vhost_features);
	if (features & ~vhost_features) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "received invalid negotiated features.\n");
		dev->flags |= VIRTIO_DEV_FEATURES_FAILED;
		dev->status &= ~VIRTIO_DEVICE_STATUS_FEATURES_OK;

		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		if (dev->features == features)
			return RTE_VHOST_MSG_RESULT_OK;

		/*
		 * Error out if frontend tries to change features while device is
		 * in running state. The exception being VHOST_F_LOG_ALL, which
		 * is enabled when the live-migration starts.
		 */
		if ((dev->features ^ features) & ~(1ULL << VHOST_F_LOG_ALL)) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"features changed while device is running.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		if (dev->notify_ops->features_changed)
			dev->notify_ops->features_changed(dev->vid, features);
	}

	dev->features = features;
	if (dev->features &
		((1ULL << VIRTIO_NET_F_MRG_RXBUF) |
		 (1ULL << VIRTIO_F_VERSION_1) |
		 (1ULL << VIRTIO_F_RING_PACKED))) {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr);
	}
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"negotiated Virtio features: 0x%" PRIx64 "\n",
		dev->features);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG,
		"mergeable RX buffers %s, virtio 1 %s\n",
		(dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF)) ? "on" : "off",
		(dev->features & (1ULL << VIRTIO_F_VERSION_1)) ? "on" : "off");

	if ((dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET) &&
	    !(dev->features & (1ULL << VIRTIO_NET_F_MQ))) {
		/*
		 * Remove all but first queue pair if MQ hasn't been
		 * negotiated. This is safe because the device is not
		 * running at this stage.
		 */
		while (dev->nr_vring > 2) {
			struct vhost_virtqueue *vq;

			vq = dev->virtqueue[--dev->nr_vring];
			if (!vq)
				continue;

			dev->virtqueue[dev->nr_vring] = NULL;
			cleanup_vq(vq, 1);
			cleanup_vq_inflight(dev, vq);
			/* vhost_user_lock_all_queue_pairs locked all qps */
			vq_assert_lock(dev, vq);
			rte_rwlock_write_unlock(&vq->access_lock);
			free_vq(dev, vq);
		}
	}

	vdpa_dev = dev->vdpa_dev;
	if (vdpa_dev)
		vdpa_dev->ops->set_features(dev->vid);

	dev->flags &= ~VIRTIO_DEV_FEATURES_FAILED;
	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * The virtio device sends us the size of the descriptor ring.
 */
static int
vhost_user_set_vring_num(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq = dev->virtqueue[ctx->msg.payload.state.index];

	if (ctx->msg.payload.state.num > 32768) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid virtqueue size %u\n",
			ctx->msg.payload.state.num);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	vq->size = ctx->msg.payload.state.num;

	/* VIRTIO 1.0, 2.4 Virtqueues says:
	 *
	 *   Queue Size value is always a power of 2. The maximum Queue Size
	 *   value is 32768.
	 *
	 * VIRTIO 1.1 2.7 Virtqueues says:
	 *
	 *   Packed virtqueues support up to 2^15 entries each.
	 */
	if (!vq_is_packed(dev)) {
		if (vq->size & (vq->size - 1)) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"invalid virtqueue size %u\n",
				vq->size);
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	if (vq_is_packed(dev)) {
		rte_free(vq->shadow_used_packed);
		vq->shadow_used_packed = rte_malloc_socket(NULL,
				vq->size *
				sizeof(struct vring_used_elem_packed),
				RTE_CACHE_LINE_SIZE, vq->numa_node);
		if (!vq->shadow_used_packed) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for shadow used ring.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}

	} else {
		rte_free(vq->shadow_used_split);

		vq->shadow_used_split = rte_malloc_socket(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE, vq->numa_node);

		if (!vq->shadow_used_split) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for vq internal data.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	rte_free(vq->batch_copy_elems);
	vq->batch_copy_elems = rte_malloc_socket(NULL,
				vq->size * sizeof(struct batch_copy_elem),
				RTE_CACHE_LINE_SIZE, vq->numa_node);
	if (!vq->batch_copy_elems) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to allocate memory for batching copy.\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * Reallocate virtio_dev, vhost_virtqueue and related data structures to
 * make them on the same numa node as the memory of vring descriptor.
 */
#ifdef RTE_LIBRTE_VHOST_NUMA
static void
numa_realloc(struct virtio_net **pdev, struct vhost_virtqueue **pvq)
{
	int node, dev_node;
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	struct batch_copy_elem *bce;
	struct guest_page *gp;
	struct rte_vhost_memory *mem;
	size_t mem_size;
	int ret;

	dev = *pdev;
	vq = *pvq;

	/*
	 * If VQ is ready, it is too late to reallocate, it certainly already
	 * happened anyway on VHOST_USER_SET_VRING_ADRR.
	 */
	if (vq->ready)
		return;

	ret = get_mempolicy(&node, NULL, 0, vq->desc, MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"unable to get virtqueue %d numa information.\n",
			vq->index);
		return;
	}

	if (node == vq->numa_node)
		goto out_dev_realloc;

	vq = rte_realloc_socket(*pvq, sizeof(**pvq), 0, node);
	if (!vq) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to realloc virtqueue %d on node %d\n",
			(*pvq)->index, node);
		return;
	}
	*pvq = vq;

	if (vq != dev->virtqueue[vq->index]) {
		VHOST_LOG_CONFIG(dev->ifname, INFO, "reallocated virtqueue on node %d\n", node);
		dev->virtqueue[vq->index] = vq;
	}

	if (vq_is_packed(dev)) {
		struct vring_used_elem_packed *sup;

		sup = rte_realloc_socket(vq->shadow_used_packed, vq->size * sizeof(*sup),
				RTE_CACHE_LINE_SIZE, node);
		if (!sup) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to realloc shadow packed on node %d\n",
				node);
			return;
		}
		vq->shadow_used_packed = sup;
	} else {
		struct vring_used_elem *sus;

		sus = rte_realloc_socket(vq->shadow_used_split, vq->size * sizeof(*sus),
				RTE_CACHE_LINE_SIZE, node);
		if (!sus) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to realloc shadow split on node %d\n",
				node);
			return;
		}
		vq->shadow_used_split = sus;
	}

	bce = rte_realloc_socket(vq->batch_copy_elems, vq->size * sizeof(*bce),
			RTE_CACHE_LINE_SIZE, node);
	if (!bce) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to realloc batch copy elem on node %d\n",
			node);
		return;
	}
	vq->batch_copy_elems = bce;

	if (vq->log_cache) {
		struct log_cache_entry *lc;

		lc = rte_realloc_socket(vq->log_cache, sizeof(*lc) * VHOST_LOG_CACHE_NR, 0, node);
		if (!lc) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to realloc log cache on node %d\n",
				node);
			return;
		}
		vq->log_cache = lc;
	}

	if (vq->resubmit_inflight) {
		struct rte_vhost_resubmit_info *ri;

		ri = rte_realloc_socket(vq->resubmit_inflight, sizeof(*ri), 0, node);
		if (!ri) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to realloc resubmit inflight on node %d\n",
				node);
			return;
		}
		vq->resubmit_inflight = ri;

		if (ri->resubmit_list) {
			struct rte_vhost_resubmit_desc *rd;

			rd = rte_realloc_socket(ri->resubmit_list, sizeof(*rd) * ri->resubmit_num,
					0, node);
			if (!rd) {
				VHOST_LOG_CONFIG(dev->ifname, ERR,
					"failed to realloc resubmit list on node %d\n",
					node);
				return;
			}
			ri->resubmit_list = rd;
		}
	}

	vq->numa_node = node;

out_dev_realloc:

	if (dev->flags & VIRTIO_DEV_RUNNING)
		return;

	ret = get_mempolicy(&dev_node, NULL, 0, dev, MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "unable to get numa information.\n");
		return;
	}

	if (dev_node == node)
		return;

	dev = rte_realloc_socket(*pdev, sizeof(**pdev), 0, node);
	if (!dev) {
		VHOST_LOG_CONFIG((*pdev)->ifname, ERR, "failed to realloc dev on node %d\n", node);
		return;
	}
	*pdev = dev;

	VHOST_LOG_CONFIG(dev->ifname, INFO, "reallocated device on node %d\n", node);
	vhost_devices[dev->vid] = dev;

	mem_size = sizeof(struct rte_vhost_memory) +
		sizeof(struct rte_vhost_mem_region) * dev->mem->nregions;
	mem = rte_realloc_socket(dev->mem, mem_size, 0, node);
	if (!mem) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to realloc mem table on node %d\n",
			node);
		return;
	}
	dev->mem = mem;

	gp = rte_realloc_socket(dev->guest_pages, dev->max_guest_pages * sizeof(*gp),
			RTE_CACHE_LINE_SIZE, node);
	if (!gp) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to realloc guest pages on node %d\n",
			node);
		return;
	}
	dev->guest_pages = gp;

	vhost_user_iotlb_init(dev);
}
#else
static void
numa_realloc(struct virtio_net **pdev, struct vhost_virtqueue **pvq)
{
	RTE_SET_USED(pdev);
	RTE_SET_USED(pvq);
}
#endif

/* Converts QEMU virtual address to Vhost virtual address. */
static uint64_t
qva_to_vva(struct virtio_net *dev, uint64_t qva, uint64_t *len)
{
	struct rte_vhost_mem_region *r;
	uint32_t i;

	if (unlikely(!dev || !dev->mem))
		goto out_error;

	/* Find the region where the address lives. */
	for (i = 0; i < dev->mem->nregions; i++) {
		r = &dev->mem->regions[i];

		if (qva >= r->guest_user_addr &&
		    qva <  r->guest_user_addr + r->size) {

			if (unlikely(*len > r->guest_user_addr + r->size - qva))
				*len = r->guest_user_addr + r->size - qva;

			return qva - r->guest_user_addr +
			       r->host_user_addr;
		}
	}
out_error:
	*len = 0;

	return 0;
}


/*
 * Converts ring address to Vhost virtual address.
 * If IOMMU is enabled, the ring address is a guest IO virtual address,
 * else it is a QEMU virtual address.
 */
static uint64_t
ring_addr_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t ra, uint64_t *size)
{
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)) {
		uint64_t vva;

		vhost_user_iotlb_rd_lock(vq);
		vva = vhost_iova_to_vva(dev, vq, ra,
					size, VHOST_ACCESS_RW);
		vhost_user_iotlb_rd_unlock(vq);

		return vva;
	}

	return qva_to_vva(dev, ra, size);
}

static uint64_t
log_addr_to_gpa(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	uint64_t log_gpa;

	vhost_user_iotlb_rd_lock(vq);
	log_gpa = translate_log_addr(dev, vq, vq->ring_addrs.log_guest_addr);
	vhost_user_iotlb_rd_unlock(vq);

	return log_gpa;
}

static uint64_t
hua_to_alignment(struct rte_vhost_memory *mem, void *ptr)
{
	struct rte_vhost_mem_region *r;
	uint32_t i;
	uintptr_t hua = (uintptr_t)ptr;

	for (i = 0; i < mem->nregions; i++) {
		r = &mem->regions[i];
		if (hua >= r->host_user_addr &&
			hua < r->host_user_addr + r->size) {
			return get_blk_size(r->fd);
		}
	}

	/* If region isn't found, don't align at all */
	return 1;
}

void
mem_set_dump(void *ptr, size_t size, bool enable, uint64_t pagesz)
{
#ifdef MADV_DONTDUMP
	void *start = RTE_PTR_ALIGN_FLOOR(ptr, pagesz);
	uintptr_t end = RTE_ALIGN_CEIL((uintptr_t)ptr + size, pagesz);
	size_t len = end - (uintptr_t)start;

	if (madvise(start, len, enable ? MADV_DODUMP : MADV_DONTDUMP) == -1) {
		rte_log(RTE_LOG_INFO, vhost_config_log_level,
			"VHOST_CONFIG: could not set coredump preference (%s).\n", strerror(errno));
	}
#endif
}

static void
translate_ring_addresses(struct virtio_net **pdev, struct vhost_virtqueue **pvq)
{
	struct vhost_virtqueue *vq;
	struct virtio_net *dev;
	uint64_t len, expected_len;

	dev = *pdev;
	vq = *pvq;

	if (vq->ring_addrs.flags & (1 << VHOST_VRING_F_LOG)) {
		vq->log_guest_addr =
			log_addr_to_gpa(dev, vq);
		if (vq->log_guest_addr == 0) {
			VHOST_LOG_CONFIG(dev->ifname, DEBUG, "failed to map log_guest_addr.\n");
			return;
		}
	}

	if (vq_is_packed(dev)) {
		len = sizeof(struct vring_packed_desc) * vq->size;
		vq->desc_packed = (struct vring_packed_desc *)(uintptr_t)
			ring_addr_to_vva(dev, vq, vq->ring_addrs.desc_user_addr, &len);
		if (vq->desc_packed == NULL ||
				len != sizeof(struct vring_packed_desc) *
				vq->size) {
			VHOST_LOG_CONFIG(dev->ifname, DEBUG, "failed to map desc_packed ring.\n");
			return;
		}

		mem_set_dump(vq->desc_packed, len, true,
			hua_to_alignment(dev->mem, vq->desc_packed));
		numa_realloc(&dev, &vq);
		*pdev = dev;
		*pvq = vq;

		len = sizeof(struct vring_packed_desc_event);
		vq->driver_event = (struct vring_packed_desc_event *)
					(uintptr_t)ring_addr_to_vva(dev,
					vq, vq->ring_addrs.avail_user_addr, &len);
		if (vq->driver_event == NULL ||
				len != sizeof(struct vring_packed_desc_event)) {
			VHOST_LOG_CONFIG(dev->ifname, DEBUG,
				"failed to find driver area address.\n");
			return;
		}

		mem_set_dump(vq->driver_event, len, true,
			hua_to_alignment(dev->mem, vq->driver_event));
		len = sizeof(struct vring_packed_desc_event);
		vq->device_event = (struct vring_packed_desc_event *)
					(uintptr_t)ring_addr_to_vva(dev,
					vq, vq->ring_addrs.used_user_addr, &len);
		if (vq->device_event == NULL ||
				len != sizeof(struct vring_packed_desc_event)) {
			VHOST_LOG_CONFIG(dev->ifname, DEBUG,
				"failed to find device area address.\n");
			return;
		}

		mem_set_dump(vq->device_event, len, true,
			hua_to_alignment(dev->mem, vq->device_event));
		vq->access_ok = true;
		return;
	}

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	if (vq->desc && vq->avail && vq->used)
		return;

	len = sizeof(struct vring_desc) * vq->size;
	vq->desc = (struct vring_desc *)(uintptr_t)ring_addr_to_vva(dev,
			vq, vq->ring_addrs.desc_user_addr, &len);
	if (vq->desc == 0 || len != sizeof(struct vring_desc) * vq->size) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG, "failed to map desc ring.\n");
		return;
	}

	mem_set_dump(vq->desc, len, true, hua_to_alignment(dev->mem, vq->desc));
	numa_realloc(&dev, &vq);
	*pdev = dev;
	*pvq = vq;

	len = sizeof(struct vring_avail) + sizeof(uint16_t) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		len += sizeof(uint16_t);
	expected_len = len;
	vq->avail = (struct vring_avail *)(uintptr_t)ring_addr_to_vva(dev,
			vq, vq->ring_addrs.avail_user_addr, &len);
	if (vq->avail == 0 || len != expected_len) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG, "failed to map avail ring.\n");
		return;
	}

	mem_set_dump(vq->avail, len, true, hua_to_alignment(dev->mem, vq->avail));
	len = sizeof(struct vring_used) +
		sizeof(struct vring_used_elem) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		len += sizeof(uint16_t);
	expected_len = len;
	vq->used = (struct vring_used *)(uintptr_t)ring_addr_to_vva(dev,
			vq, vq->ring_addrs.used_user_addr, &len);
	if (vq->used == 0 || len != expected_len) {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG, "failed to map used ring.\n");
		return;
	}

	mem_set_dump(vq->used, len, true, hua_to_alignment(dev->mem, vq->used));

	if (vq->last_used_idx != vq->used->idx) {
		VHOST_LOG_CONFIG(dev->ifname, WARNING,
			"last_used_idx (%u) and vq->used->idx (%u) mismatches;\n",
			vq->last_used_idx, vq->used->idx);
		vq->last_used_idx  = vq->used->idx;
		vq->last_avail_idx = vq->used->idx;
		VHOST_LOG_CONFIG(dev->ifname, WARNING,
			"some packets maybe resent for Tx and dropped for Rx\n");
	}

	vq->access_ok = true;

	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "mapped address desc: %p\n", vq->desc);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "mapped address avail: %p\n", vq->avail);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "mapped address used: %p\n", vq->used);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "log_guest_addr: %" PRIx64 "\n", vq->log_guest_addr);
}

/*
 * The virtio device sends us the desc, used and avail ring addresses.
 * This function then converts these to our address space.
 */
static int
vhost_user_set_vring_addr(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq;
	struct vhost_vring_addr *addr = &ctx->msg.payload.addr;
	bool access_ok;

	if (dev->mem == NULL)
		return RTE_VHOST_MSG_RESULT_ERR;

	/* addr->index refers to the queue index. The txq 1, rxq is 0. */
	vq = dev->virtqueue[ctx->msg.payload.addr.index];

	access_ok = vq->access_ok;

	/*
	 * Rings addresses should not be interpreted as long as the ring is not
	 * started and enabled
	 */
	memcpy(&vq->ring_addrs, addr, sizeof(*addr));

	vring_invalidate(dev, vq);

	if ((vq->enabled && (dev->features &
				(1ULL << VHOST_USER_F_PROTOCOL_FEATURES))) ||
			access_ok) {
		translate_ring_addresses(&dev, &vq);
		*pdev = dev;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * The virtio device sends us the available ring last used index.
 */
static int
vhost_user_set_vring_base(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq = dev->virtqueue[ctx->msg.payload.state.index];
	uint64_t val = ctx->msg.payload.state.num;

	if (vq_is_packed(dev)) {
		/*
		 * Bit[0:14]: avail index
		 * Bit[15]: avail wrap counter
		 */
		vq->last_avail_idx = val & 0x7fff;
		vq->avail_wrap_counter = !!(val & (0x1 << 15));
		/*
		 * Set used index to same value as available one, as
		 * their values should be the same since ring processing
		 * was stopped at get time.
		 */
		vq->last_used_idx = vq->last_avail_idx;
		vq->used_wrap_counter = vq->avail_wrap_counter;
	} else {
		vq->last_used_idx = ctx->msg.payload.state.num;
		vq->last_avail_idx = ctx->msg.payload.state.num;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"vring base idx:%u last_used_idx:%u last_avail_idx:%u.\n",
		ctx->msg.payload.state.index, vq->last_used_idx, vq->last_avail_idx);

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
add_one_guest_page(struct virtio_net *dev, uint64_t guest_phys_addr,
		   uint64_t host_iova, uint64_t host_user_addr, uint64_t size)
{
	struct guest_page *page, *last_page;
	struct guest_page *old_pages;

	if (dev->nr_guest_pages == dev->max_guest_pages) {
		dev->max_guest_pages *= 2;
		old_pages = dev->guest_pages;
		dev->guest_pages = rte_realloc(dev->guest_pages,
					dev->max_guest_pages * sizeof(*page),
					RTE_CACHE_LINE_SIZE);
		if (dev->guest_pages == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "cannot realloc guest_pages\n");
			rte_free(old_pages);
			return -1;
		}
	}

	if (dev->nr_guest_pages > 0) {
		last_page = &dev->guest_pages[dev->nr_guest_pages - 1];
		/* merge if the two pages are continuous */
		if (host_iova == last_page->host_iova + last_page->size &&
		    guest_phys_addr == last_page->guest_phys_addr + last_page->size &&
		    host_user_addr == last_page->host_user_addr + last_page->size) {
			last_page->size += size;
			return 0;
		}
	}

	page = &dev->guest_pages[dev->nr_guest_pages++];
	page->guest_phys_addr = guest_phys_addr;
	page->host_iova  = host_iova;
	page->host_user_addr = host_user_addr;
	page->size = size;

	return 0;
}

static int
add_guest_pages(struct virtio_net *dev, struct rte_vhost_mem_region *reg,
		uint64_t page_size)
{
	uint64_t reg_size = reg->size;
	uint64_t host_user_addr  = reg->host_user_addr;
	uint64_t guest_phys_addr = reg->guest_phys_addr;
	uint64_t host_iova;
	uint64_t size;

	host_iova = rte_mem_virt2iova((void *)(uintptr_t)host_user_addr);
	size = page_size - (guest_phys_addr & (page_size - 1));
	size = RTE_MIN(size, reg_size);

	if (add_one_guest_page(dev, guest_phys_addr, host_iova,
			       host_user_addr, size) < 0)
		return -1;

	host_user_addr  += size;
	guest_phys_addr += size;
	reg_size -= size;

	while (reg_size > 0) {
		size = RTE_MIN(reg_size, page_size);
		host_iova = rte_mem_virt2iova((void *)(uintptr_t)
						  host_user_addr);
		if (add_one_guest_page(dev, guest_phys_addr, host_iova,
				       host_user_addr, size) < 0)
			return -1;

		host_user_addr  += size;
		guest_phys_addr += size;
		reg_size -= size;
	}

	/* sort guest page array if over binary search threshold */
	if (dev->nr_guest_pages >= VHOST_BINARY_SEARCH_THRESH) {
		qsort((void *)dev->guest_pages, dev->nr_guest_pages,
			sizeof(struct guest_page), guest_page_addrcmp);
	}

	return 0;
}

#ifdef RTE_LIBRTE_VHOST_DEBUG
/* TODO: enable it only in debug mode? */
static void
dump_guest_pages(struct virtio_net *dev)
{
	uint32_t i;
	struct guest_page *page;

	for (i = 0; i < dev->nr_guest_pages; i++) {
		page = &dev->guest_pages[i];

		VHOST_LOG_CONFIG(dev->ifname, INFO, "guest physical page region %u\n", i);
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\tguest_phys_addr: %" PRIx64 "\n",
			page->guest_phys_addr);
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\thost_iova : %" PRIx64 "\n",
			page->host_iova);
		VHOST_LOG_CONFIG(dev->ifname, INFO, "\tsize           : %" PRIx64 "\n",
			page->size);
	}
}
#else
#define dump_guest_pages(dev)
#endif

static bool
vhost_memory_changed(struct VhostUserMemory *new,
		     struct rte_vhost_memory *old)
{
	uint32_t i;

	if (new->nregions != old->nregions)
		return true;

	for (i = 0; i < new->nregions; ++i) {
		VhostUserMemoryRegion *new_r = &new->regions[i];
		struct rte_vhost_mem_region *old_r = &old->regions[i];

		if (new_r->guest_phys_addr != old_r->guest_phys_addr)
			return true;
		if (new_r->memory_size != old_r->size)
			return true;
		if (new_r->userspace_addr != old_r->guest_user_addr)
			return true;
	}

	return false;
}

#ifdef RTE_LIBRTE_VHOST_POSTCOPY
static int
vhost_user_postcopy_region_register(struct virtio_net *dev,
		struct rte_vhost_mem_region *reg)
{
	struct uffdio_register reg_struct;

	/*
	 * Let's register all the mmapped area to ensure
	 * alignment on page boundary.
	 */
	reg_struct.range.start = (uint64_t)(uintptr_t)reg->mmap_addr;
	reg_struct.range.len = reg->mmap_size;
	reg_struct.mode = UFFDIO_REGISTER_MODE_MISSING;

	if (ioctl(dev->postcopy_ufd, UFFDIO_REGISTER,
				&reg_struct)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to register ufd for region "
			"%" PRIx64 " - %" PRIx64 " (ufd = %d) %s\n",
			(uint64_t)reg_struct.range.start,
			(uint64_t)reg_struct.range.start +
			(uint64_t)reg_struct.range.len - 1,
			dev->postcopy_ufd,
			strerror(errno));
		return -1;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t userfaultfd registered for range : %" PRIx64 " - %" PRIx64 "\n",
		(uint64_t)reg_struct.range.start,
		(uint64_t)reg_struct.range.start +
		(uint64_t)reg_struct.range.len - 1);

	return 0;
}
#else
static int
vhost_user_postcopy_region_register(struct virtio_net *dev __rte_unused,
		struct rte_vhost_mem_region *reg __rte_unused)
{
	return -1;
}
#endif

static int
vhost_user_postcopy_register(struct virtio_net *dev, int main_fd,
		struct vhu_msg_context *ctx)
{
	struct VhostUserMemory *memory;
	struct rte_vhost_mem_region *reg;
	struct vhu_msg_context ack_ctx;
	uint32_t i;

	if (!dev->postcopy_listening)
		return 0;

	/*
	 * We haven't a better way right now than sharing
	 * DPDK's virtual address with Qemu, so that Qemu can
	 * retrieve the region offset when handling userfaults.
	 */
	memory = &ctx->msg.payload.memory;
	for (i = 0; i < memory->nregions; i++) {
		reg = &dev->mem->regions[i];
		memory->regions[i].userspace_addr = reg->host_user_addr;
	}

	/* Send the addresses back to qemu */
	ctx->fd_num = 0;
	send_vhost_reply(dev, main_fd, ctx);

	/* Wait for qemu to acknowledge it got the addresses
	 * we've got to wait before we're allowed to generate faults.
	 */
	if (read_vhost_message(dev, main_fd, &ack_ctx) <= 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to read qemu ack on postcopy set-mem-table\n");
		return -1;
	}

	if (validate_msg_fds(dev, &ack_ctx, 0) != 0)
		return -1;

	if (ack_ctx.msg.request.frontend != VHOST_USER_SET_MEM_TABLE) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"bad qemu ack on postcopy set-mem-table (%d)\n",
			ack_ctx.msg.request.frontend);
		return -1;
	}

	/* Now userfault register and we can use the memory */
	for (i = 0; i < memory->nregions; i++) {
		reg = &dev->mem->regions[i];
		if (vhost_user_postcopy_region_register(dev, reg) < 0)
			return -1;
	}

	return 0;
}

static int
vhost_user_mmap_region(struct virtio_net *dev,
		struct rte_vhost_mem_region *region,
		uint64_t mmap_offset)
{
	void *mmap_addr;
	uint64_t mmap_size;
	uint64_t alignment;
	int populate;

	/* Check for memory_size + mmap_offset overflow */
	if (mmap_offset >= -region->size) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"mmap_offset (%#"PRIx64") and memory_size (%#"PRIx64") overflow\n",
			mmap_offset, region->size);
		return -1;
	}

	mmap_size = region->size + mmap_offset;

	/* mmap() without flag of MAP_ANONYMOUS, should be called with length
	 * argument aligned with hugepagesz at older longterm version Linux,
	 * like 2.6.32 and 3.2.72, or mmap() will fail with EINVAL.
	 *
	 * To avoid failure, make sure in caller to keep length aligned.
	 */
	alignment = get_blk_size(region->fd);
	if (alignment == (uint64_t)-1) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "couldn't get hugepage size through fstat\n");
		return -1;
	}
	mmap_size = RTE_ALIGN_CEIL(mmap_size, alignment);
	if (mmap_size == 0) {
		/*
		 * It could happen if initial mmap_size + alignment overflows
		 * the sizeof uint64, which could happen if either mmap_size or
		 * alignment value is wrong.
		 *
		 * mmap() kernel implementation would return an error, but
		 * better catch it before and provide useful info in the logs.
		 */
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"mmap size (0x%" PRIx64 ") or alignment (0x%" PRIx64 ") is invalid\n",
			region->size + mmap_offset, alignment);
		return -1;
	}

	populate = dev->async_copy ? MAP_POPULATE : 0;
	mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
			MAP_SHARED | populate, region->fd, 0);

	if (mmap_addr == MAP_FAILED) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "mmap failed (%s).\n", strerror(errno));
		return -1;
	}

	region->mmap_addr = mmap_addr;
	region->mmap_size = mmap_size;
	region->host_user_addr = (uint64_t)(uintptr_t)mmap_addr + mmap_offset;
	mem_set_dump(mmap_addr, mmap_size, false, alignment);

	if (dev->async_copy) {
		if (add_guest_pages(dev, region, alignment) < 0) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"adding guest pages to region failed.\n");
			return -1;
		}
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"guest memory region size: 0x%" PRIx64 "\n",
		region->size);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t guest physical addr: 0x%" PRIx64 "\n",
		region->guest_phys_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t guest virtual  addr: 0x%" PRIx64 "\n",
		region->guest_user_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t host  virtual  addr: 0x%" PRIx64 "\n",
		region->host_user_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t mmap addr : 0x%" PRIx64 "\n",
		(uint64_t)(uintptr_t)mmap_addr);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t mmap size : 0x%" PRIx64 "\n",
		mmap_size);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t mmap align: 0x%" PRIx64 "\n",
		alignment);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t mmap off  : 0x%" PRIx64 "\n",
		mmap_offset);

	return 0;
}

static int
vhost_user_set_mem_table(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd)
{
	struct virtio_net *dev = *pdev;
	struct VhostUserMemory *memory = &ctx->msg.payload.memory;
	struct rte_vhost_mem_region *reg;
	int numa_node = SOCKET_ID_ANY;
	uint64_t mmap_offset;
	uint32_t i;
	bool async_notify = false;

	if (validate_msg_fds(dev, ctx, memory->nregions) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (memory->nregions > VHOST_MEMORY_MAX_NREGIONS) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"too many memory regions (%u)\n",
			memory->nregions);
		goto close_msg_fds;
	}

	if (dev->mem && !vhost_memory_changed(memory, dev->mem)) {
		VHOST_LOG_CONFIG(dev->ifname, INFO, "memory regions not changed\n");

		close_msg_fds(ctx);

		return RTE_VHOST_MSG_RESULT_OK;
	}

	if (dev->mem) {
		if (dev->flags & VIRTIO_DEV_VDPA_CONFIGURED) {
			struct rte_vdpa_device *vdpa_dev = dev->vdpa_dev;

			if (vdpa_dev && vdpa_dev->ops->dev_close)
				vdpa_dev->ops->dev_close(dev->vid);
			dev->flags &= ~VIRTIO_DEV_VDPA_CONFIGURED;
		}

		/* notify the vhost application to stop DMA transfers */
		if (dev->async_copy && dev->notify_ops->vring_state_changed) {
			for (i = 0; i < dev->nr_vring; i++) {
				dev->notify_ops->vring_state_changed(dev->vid,
						i, 0);
			}
			async_notify = true;
		}

		/* Flush IOTLB cache as previous HVAs are now invalid */
		if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
			vhost_user_iotlb_flush_all(dev);

		free_mem_region(dev);
		rte_free(dev->mem);
		dev->mem = NULL;
	}

	/*
	 * If VQ 0 has already been allocated, try to allocate on the same
	 * NUMA node. It can be reallocated later in numa_realloc().
	 */
	if (dev->nr_vring > 0)
		numa_node = dev->virtqueue[0]->numa_node;

	dev->nr_guest_pages = 0;
	if (dev->guest_pages == NULL) {
		dev->max_guest_pages = 8;
		dev->guest_pages = rte_zmalloc_socket(NULL,
					dev->max_guest_pages *
					sizeof(struct guest_page),
					RTE_CACHE_LINE_SIZE,
					numa_node);
		if (dev->guest_pages == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for dev->guest_pages\n");
			goto close_msg_fds;
		}
	}

	dev->mem = rte_zmalloc_socket("vhost-mem-table", sizeof(struct rte_vhost_memory) +
		sizeof(struct rte_vhost_mem_region) * memory->nregions, 0, numa_node);
	if (dev->mem == NULL) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to allocate memory for dev->mem\n");
		goto free_guest_pages;
	}

	for (i = 0; i < memory->nregions; i++) {
		reg = &dev->mem->regions[i];

		reg->guest_phys_addr = memory->regions[i].guest_phys_addr;
		reg->guest_user_addr = memory->regions[i].userspace_addr;
		reg->size            = memory->regions[i].memory_size;
		reg->fd              = ctx->fds[i];

		/*
		 * Assign invalid file descriptor value to avoid double
		 * closing on error path.
		 */
		ctx->fds[i] = -1;

		mmap_offset = memory->regions[i].mmap_offset;

		if (vhost_user_mmap_region(dev, reg, mmap_offset) < 0) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to mmap region %u\n", i);
			goto free_mem_table;
		}

		dev->mem->nregions++;
	}

	if (dev->async_copy && rte_vfio_is_enabled("vfio"))
		async_dma_map(dev, true);

	if (vhost_user_postcopy_register(dev, main_fd, ctx) < 0)
		goto free_mem_table;

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (!vq)
			continue;

		if (vq->desc || vq->avail || vq->used) {
			/*
			 * If the memory table got updated, the ring addresses
			 * need to be translated again as virtual addresses have
			 * changed.
			 */
			vring_invalidate(dev, vq);

			translate_ring_addresses(&dev, &vq);
			*pdev = dev;
		}
	}

	dump_guest_pages(dev);

	if (async_notify) {
		for (i = 0; i < dev->nr_vring; i++)
			dev->notify_ops->vring_state_changed(dev->vid, i, 1);
	}

	return RTE_VHOST_MSG_RESULT_OK;

free_mem_table:
	free_mem_region(dev);
	rte_free(dev->mem);
	dev->mem = NULL;

free_guest_pages:
	rte_free(dev->guest_pages);
	dev->guest_pages = NULL;
close_msg_fds:
	close_msg_fds(ctx);
	return RTE_VHOST_MSG_RESULT_ERR;
}

static bool
vq_is_ready(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	bool rings_ok;

	if (!vq)
		return false;

	if (vq_is_packed(dev))
		rings_ok = vq->desc_packed && vq->driver_event &&
			vq->device_event;
	else
		rings_ok = vq->desc && vq->avail && vq->used;

	return rings_ok &&
	       vq->kickfd != VIRTIO_UNINITIALIZED_EVENTFD &&
	       vq->callfd != VIRTIO_UNINITIALIZED_EVENTFD &&
	       vq->enabled;
}

#define VIRTIO_BUILTIN_NUM_VQS_TO_BE_READY 2u
#define VIRTIO_BLK_NUM_VQS_TO_BE_READY 1u

static int
virtio_is_ready(struct virtio_net *dev)
{
	struct rte_vdpa_device *vdpa_dev;
	struct vhost_virtqueue *vq;
	uint32_t vdpa_type;
	uint32_t i, nr_vring = dev->nr_vring;

	if (dev->flags & VIRTIO_DEV_READY)
		return 1;

	if (!dev->nr_vring)
		return 0;

	vdpa_dev = dev->vdpa_dev;
	if (vdpa_dev)
		vdpa_type = vdpa_dev->type;
	else
		vdpa_type = -1;

	if (vdpa_type == RTE_VHOST_VDPA_DEVICE_TYPE_BLK) {
		nr_vring = VIRTIO_BLK_NUM_VQS_TO_BE_READY;
	} else {
		if (dev->flags & VIRTIO_DEV_BUILTIN_VIRTIO_NET)
			nr_vring = VIRTIO_BUILTIN_NUM_VQS_TO_BE_READY;
	}

	if (dev->nr_vring < nr_vring)
		return 0;

	for (i = 0; i < nr_vring; i++) {
		vq = dev->virtqueue[i];

		if (!vq_is_ready(dev, vq))
			return 0;
	}

	/* If supported, ensure the frontend is really done with config */
	if (dev->protocol_features & (1ULL << VHOST_USER_PROTOCOL_F_STATUS))
		if (!(dev->status & VIRTIO_DEVICE_STATUS_DRIVER_OK))
			return 0;

	dev->flags |= VIRTIO_DEV_READY;

	if (!(dev->flags & VIRTIO_DEV_RUNNING))
		VHOST_LOG_CONFIG(dev->ifname, INFO, "virtio is now ready for processing.\n");
	return 1;
}

static void *
inflight_mem_alloc(struct virtio_net *dev, const char *name, size_t size, int *fd)
{
	void *ptr;
	int mfd = -1;
	uint64_t alignment;
	char fname[20] = "/tmp/memfd-XXXXXX";

	*fd = -1;
#ifdef MEMFD_SUPPORTED
	mfd = memfd_create(name, MFD_CLOEXEC);
#else
	RTE_SET_USED(name);
#endif
	if (mfd == -1) {
		mfd = mkstemp(fname);
		if (mfd == -1) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to get inflight buffer fd\n");
			return NULL;
		}

		unlink(fname);
	}

	if (ftruncate(mfd, size) == -1) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to alloc inflight buffer\n");
		close(mfd);
		return NULL;
	}

	ptr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
	if (ptr == MAP_FAILED) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to mmap inflight buffer\n");
		close(mfd);
		return NULL;
	}

	alignment = get_blk_size(mfd);
	mem_set_dump(ptr, size, false, alignment);
	*fd = mfd;
	return ptr;
}

static uint32_t
get_pervq_shm_size_split(uint16_t queue_size)
{
	return RTE_ALIGN_MUL_CEIL(sizeof(struct rte_vhost_inflight_desc_split) *
				  queue_size + sizeof(uint64_t) +
				  sizeof(uint16_t) * 4, INFLIGHT_ALIGNMENT);
}

static uint32_t
get_pervq_shm_size_packed(uint16_t queue_size)
{
	return RTE_ALIGN_MUL_CEIL(sizeof(struct rte_vhost_inflight_desc_packed)
				  * queue_size + sizeof(uint64_t) +
				  sizeof(uint16_t) * 6 + sizeof(uint8_t) * 9,
				  INFLIGHT_ALIGNMENT);
}

static int
vhost_user_get_inflight_fd(struct virtio_net **pdev,
			   struct vhu_msg_context *ctx,
			   int main_fd __rte_unused)
{
	struct rte_vhost_inflight_info_packed *inflight_packed;
	uint64_t pervq_inflight_size, mmap_size;
	uint16_t num_queues, queue_size;
	struct virtio_net *dev = *pdev;
	int fd, i, j;
	int numa_node = SOCKET_ID_ANY;
	void *addr;

	if (ctx->msg.size != sizeof(ctx->msg.payload.inflight)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid get_inflight_fd message size is %d\n",
			ctx->msg.size);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	/*
	 * If VQ 0 has already been allocated, try to allocate on the same
	 * NUMA node. It can be reallocated later in numa_realloc().
	 */
	if (dev->nr_vring > 0)
		numa_node = dev->virtqueue[0]->numa_node;

	if (dev->inflight_info == NULL) {
		dev->inflight_info = rte_zmalloc_socket("inflight_info",
				sizeof(struct inflight_mem_info), 0, numa_node);
		if (!dev->inflight_info) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to alloc dev inflight area\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
		dev->inflight_info->fd = -1;
	}

	num_queues = ctx->msg.payload.inflight.num_queues;
	queue_size = ctx->msg.payload.inflight.queue_size;

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"get_inflight_fd num_queues: %u\n",
		ctx->msg.payload.inflight.num_queues);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"get_inflight_fd queue_size: %u\n",
		ctx->msg.payload.inflight.queue_size);

	if (vq_is_packed(dev))
		pervq_inflight_size = get_pervq_shm_size_packed(queue_size);
	else
		pervq_inflight_size = get_pervq_shm_size_split(queue_size);

	mmap_size = num_queues * pervq_inflight_size;
	addr = inflight_mem_alloc(dev, "vhost-inflight", mmap_size, &fd);
	if (!addr) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to alloc vhost inflight area\n");
			ctx->msg.payload.inflight.mmap_size = 0;
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	memset(addr, 0, mmap_size);

	if (dev->inflight_info->addr) {
		munmap(dev->inflight_info->addr, dev->inflight_info->size);
		dev->inflight_info->addr = NULL;
	}

	if (dev->inflight_info->fd >= 0) {
		close(dev->inflight_info->fd);
		dev->inflight_info->fd = -1;
	}

	dev->inflight_info->addr = addr;
	dev->inflight_info->size = ctx->msg.payload.inflight.mmap_size = mmap_size;
	dev->inflight_info->fd = ctx->fds[0] = fd;
	ctx->msg.payload.inflight.mmap_offset = 0;
	ctx->fd_num = 1;

	if (vq_is_packed(dev)) {
		for (i = 0; i < num_queues; i++) {
			inflight_packed =
				(struct rte_vhost_inflight_info_packed *)addr;
			inflight_packed->used_wrap_counter = 1;
			inflight_packed->old_used_wrap_counter = 1;
			for (j = 0; j < queue_size; j++)
				inflight_packed->desc[j].next = j + 1;
			addr = (void *)((char *)addr + pervq_inflight_size);
		}
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"send inflight mmap_size: %"PRIu64"\n",
		ctx->msg.payload.inflight.mmap_size);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"send inflight mmap_offset: %"PRIu64"\n",
		ctx->msg.payload.inflight.mmap_offset);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"send inflight fd: %d\n", ctx->fds[0]);

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_set_inflight_fd(struct virtio_net **pdev,
			   struct vhu_msg_context *ctx,
			   int main_fd __rte_unused)
{
	uint64_t mmap_size, mmap_offset;
	uint16_t num_queues, queue_size;
	struct virtio_net *dev = *pdev;
	uint32_t pervq_inflight_size;
	struct vhost_virtqueue *vq;
	void *addr;
	int fd, i;
	int numa_node = SOCKET_ID_ANY;

	if (validate_msg_fds(dev, ctx, 1) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	fd = ctx->fds[0];
	if (ctx->msg.size != sizeof(ctx->msg.payload.inflight) || fd < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid set_inflight_fd message size is %d,fd is %d\n",
			ctx->msg.size, fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	mmap_size = ctx->msg.payload.inflight.mmap_size;
	mmap_offset = ctx->msg.payload.inflight.mmap_offset;
	num_queues = ctx->msg.payload.inflight.num_queues;
	queue_size = ctx->msg.payload.inflight.queue_size;

	if (vq_is_packed(dev))
		pervq_inflight_size = get_pervq_shm_size_packed(queue_size);
	else
		pervq_inflight_size = get_pervq_shm_size_split(queue_size);

	VHOST_LOG_CONFIG(dev->ifname, INFO, "set_inflight_fd mmap_size: %"PRIu64"\n", mmap_size);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set_inflight_fd mmap_offset: %"PRIu64"\n",
		mmap_offset);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set_inflight_fd num_queues: %u\n",
		num_queues);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set_inflight_fd queue_size: %u\n",
		queue_size);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set_inflight_fd fd: %d\n",
		fd);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set_inflight_fd pervq_inflight_size: %d\n",
		pervq_inflight_size);

	/*
	 * If VQ 0 has already been allocated, try to allocate on the same
	 * NUMA node. It can be reallocated later in numa_realloc().
	 */
	if (dev->nr_vring > 0)
		numa_node = dev->virtqueue[0]->numa_node;

	if (!dev->inflight_info) {
		dev->inflight_info = rte_zmalloc_socket("inflight_info",
				sizeof(struct inflight_mem_info), 0, numa_node);
		if (dev->inflight_info == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to alloc dev inflight area\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
		dev->inflight_info->fd = -1;
	}

	if (dev->inflight_info->addr) {
		munmap(dev->inflight_info->addr, dev->inflight_info->size);
		dev->inflight_info->addr = NULL;
	}

	addr = mmap(0, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED,
		    fd, mmap_offset);
	if (addr == MAP_FAILED) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to mmap share memory.\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->inflight_info->fd >= 0) {
		close(dev->inflight_info->fd);
		dev->inflight_info->fd = -1;
	}

	mem_set_dump(addr, mmap_size, false, get_blk_size(fd));
	dev->inflight_info->fd = fd;
	dev->inflight_info->addr = addr;
	dev->inflight_info->size = mmap_size;

	for (i = 0; i < num_queues; i++) {
		vq = dev->virtqueue[i];
		if (!vq)
			continue;

		if (vq_is_packed(dev)) {
			vq->inflight_packed = addr;
			vq->inflight_packed->desc_num = queue_size;
		} else {
			vq->inflight_split = addr;
			vq->inflight_split->desc_num = queue_size;
		}
		addr = (void *)((char *)addr + pervq_inflight_size);
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_vring_call(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;
	int expected_fds;

	expected_fds = (ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK) ? 0 : 1;
	if (validate_msg_fds(dev, ctx, expected_fds) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	file.index = ctx->msg.payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = ctx->fds[0];
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"vring call idx:%d file:%d\n",
		file.index, file.fd);

	vq = dev->virtqueue[file.index];

	if (vq->ready) {
		vq->ready = false;
		vhost_user_notify_queue_state(dev, vq, 0);
	}

	if (vq->callfd >= 0)
		close(vq->callfd);

	vq->callfd = file.fd;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int vhost_user_set_vring_err(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	int expected_fds;

	expected_fds = (ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK) ? 0 : 1;
	if (validate_msg_fds(dev, ctx, expected_fds) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (!(ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK))
		close(ctx->fds[0]);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "not implemented\n");

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
resubmit_desc_compare(const void *a, const void *b)
{
	const struct rte_vhost_resubmit_desc *desc0 = a;
	const struct rte_vhost_resubmit_desc *desc1 = b;

	if (desc1->counter > desc0->counter)
		return 1;

	return -1;
}

static int
vhost_check_queue_inflights_split(struct virtio_net *dev,
				  struct vhost_virtqueue *vq)
{
	uint16_t i;
	uint16_t resubmit_num = 0, last_io, num;
	struct vring_used *used = vq->used;
	struct rte_vhost_resubmit_info *resubmit;
	struct rte_vhost_inflight_info_split *inflight_split;

	if (!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD)))
		return RTE_VHOST_MSG_RESULT_OK;

	/* The frontend may still not support the inflight feature
	 * although we negotiate the protocol feature.
	 */
	if ((!vq->inflight_split))
		return RTE_VHOST_MSG_RESULT_OK;

	if (!vq->inflight_split->version) {
		vq->inflight_split->version = INFLIGHT_VERSION;
		return RTE_VHOST_MSG_RESULT_OK;
	}

	if (vq->resubmit_inflight)
		return RTE_VHOST_MSG_RESULT_OK;

	inflight_split = vq->inflight_split;
	vq->global_counter = 0;
	last_io = inflight_split->last_inflight_io;

	if (inflight_split->used_idx != used->idx) {
		inflight_split->desc[last_io].inflight = 0;
		rte_atomic_thread_fence(__ATOMIC_SEQ_CST);
		inflight_split->used_idx = used->idx;
	}

	for (i = 0; i < inflight_split->desc_num; i++) {
		if (inflight_split->desc[i].inflight == 1)
			resubmit_num++;
	}

	vq->last_avail_idx += resubmit_num;

	if (resubmit_num) {
		resubmit = rte_zmalloc_socket("resubmit", sizeof(struct rte_vhost_resubmit_info),
				0, vq->numa_node);
		if (!resubmit) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for resubmit info.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		resubmit->resubmit_list = rte_zmalloc_socket("resubmit_list",
				resubmit_num * sizeof(struct rte_vhost_resubmit_desc),
				0, vq->numa_node);
		if (!resubmit->resubmit_list) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
					"failed to allocate memory for inflight desc.\n");
			rte_free(resubmit);
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		num = 0;
		for (i = 0; i < vq->inflight_split->desc_num; i++) {
			if (vq->inflight_split->desc[i].inflight == 1) {
				resubmit->resubmit_list[num].index = i;
				resubmit->resubmit_list[num].counter =
					inflight_split->desc[i].counter;
				num++;
			}
		}
		resubmit->resubmit_num = num;

		if (resubmit->resubmit_num > 1)
			qsort(resubmit->resubmit_list, resubmit->resubmit_num,
			      sizeof(struct rte_vhost_resubmit_desc),
			      resubmit_desc_compare);

		vq->global_counter = resubmit->resubmit_list[0].counter + 1;
		vq->resubmit_inflight = resubmit;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_check_queue_inflights_packed(struct virtio_net *dev,
				   struct vhost_virtqueue *vq)
{
	uint16_t i;
	uint16_t resubmit_num = 0, old_used_idx, num;
	struct rte_vhost_resubmit_info *resubmit;
	struct rte_vhost_inflight_info_packed *inflight_packed;

	if (!(dev->protocol_features &
	    (1ULL << VHOST_USER_PROTOCOL_F_INFLIGHT_SHMFD)))
		return RTE_VHOST_MSG_RESULT_OK;

	/* The frontend may still not support the inflight feature
	 * although we negotiate the protocol feature.
	 */
	if ((!vq->inflight_packed))
		return RTE_VHOST_MSG_RESULT_OK;

	if (!vq->inflight_packed->version) {
		vq->inflight_packed->version = INFLIGHT_VERSION;
		return RTE_VHOST_MSG_RESULT_OK;
	}

	if (vq->resubmit_inflight)
		return RTE_VHOST_MSG_RESULT_OK;

	inflight_packed = vq->inflight_packed;
	vq->global_counter = 0;
	old_used_idx = inflight_packed->old_used_idx;

	if (inflight_packed->used_idx != old_used_idx) {
		if (inflight_packed->desc[old_used_idx].inflight == 0) {
			inflight_packed->old_used_idx =
				inflight_packed->used_idx;
			inflight_packed->old_used_wrap_counter =
				inflight_packed->used_wrap_counter;
			inflight_packed->old_free_head =
				inflight_packed->free_head;
		} else {
			inflight_packed->used_idx =
				inflight_packed->old_used_idx;
			inflight_packed->used_wrap_counter =
				inflight_packed->old_used_wrap_counter;
			inflight_packed->free_head =
				inflight_packed->old_free_head;
		}
	}

	for (i = 0; i < inflight_packed->desc_num; i++) {
		if (inflight_packed->desc[i].inflight == 1)
			resubmit_num++;
	}

	if (resubmit_num) {
		resubmit = rte_zmalloc_socket("resubmit", sizeof(struct rte_vhost_resubmit_info),
				0, vq->numa_node);
		if (resubmit == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for resubmit info.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		resubmit->resubmit_list = rte_zmalloc_socket("resubmit_list",
				resubmit_num * sizeof(struct rte_vhost_resubmit_desc),
				0, vq->numa_node);
		if (resubmit->resubmit_list == NULL) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate memory for resubmit desc.\n");
			rte_free(resubmit);
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		num = 0;
		for (i = 0; i < inflight_packed->desc_num; i++) {
			if (vq->inflight_packed->desc[i].inflight == 1) {
				resubmit->resubmit_list[num].index = i;
				resubmit->resubmit_list[num].counter =
					inflight_packed->desc[i].counter;
				num++;
			}
		}
		resubmit->resubmit_num = num;

		if (resubmit->resubmit_num > 1)
			qsort(resubmit->resubmit_list, resubmit->resubmit_num,
			      sizeof(struct rte_vhost_resubmit_desc),
			      resubmit_desc_compare);

		vq->global_counter = resubmit->resubmit_list[0].counter + 1;
		vq->resubmit_inflight = resubmit;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_vring_kick(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;
	int expected_fds;

	expected_fds = (ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK) ? 0 : 1;
	if (validate_msg_fds(dev, ctx, expected_fds) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	file.index = ctx->msg.payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (ctx->msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = ctx->fds[0];
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"vring kick idx:%d file:%d\n",
		file.index, file.fd);

	/* Interpret ring addresses only when ring is started. */
	vq = dev->virtqueue[file.index];
	translate_ring_addresses(&dev, &vq);
	*pdev = dev;

	/*
	 * When VHOST_USER_F_PROTOCOL_FEATURES is not negotiated,
	 * the ring starts already enabled. Otherwise, it is enabled via
	 * the SET_VRING_ENABLE message.
	 */
	if (!(dev->features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES))) {
		vq->enabled = true;
	}

	if (vq->ready) {
		vq->ready = false;
		vhost_user_notify_queue_state(dev, vq, 0);
	}

	if (vq->kickfd >= 0)
		close(vq->kickfd);
	vq->kickfd = file.fd;

	if (vq_is_packed(dev)) {
		if (vhost_check_queue_inflights_packed(dev, vq)) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to inflights for vq: %d\n",
				file.index);
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	} else {
		if (vhost_check_queue_inflights_split(dev, vq)) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to inflights for vq: %d\n",
				file.index);
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * when virtio is stopped, qemu will send us the GET_VRING_BASE message.
 */
static int
vhost_user_get_vring_base(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq = dev->virtqueue[ctx->msg.payload.state.index];
	uint64_t val;

	/* We have to stop the queue (virtio) if it is running. */
	vhost_destroy_device_notify(dev);

	dev->flags &= ~VIRTIO_DEV_READY;
	dev->flags &= ~VIRTIO_DEV_VDPA_CONFIGURED;

	/* Here we are safe to get the indexes */
	if (vq_is_packed(dev)) {
		/*
		 * Bit[0:14]: avail index
		 * Bit[15]: avail wrap counter
		 */
		val = vq->last_avail_idx & 0x7fff;
		val |= vq->avail_wrap_counter << 15;
		ctx->msg.payload.state.num = val;
	} else {
		ctx->msg.payload.state.num = vq->last_avail_idx;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"vring base idx:%d file:%d\n",
		ctx->msg.payload.state.index, ctx->msg.payload.state.num);
	/*
	 * Based on current qemu vhost-user implementation, this message is
	 * sent and only sent in vhost_vring_stop.
	 * TODO: cleanup the vring, it isn't usable since here.
	 */
	if (vq->kickfd >= 0)
		close(vq->kickfd);

	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;

	if (vq->callfd >= 0)
		close(vq->callfd);

	vq->callfd = VIRTIO_UNINITIALIZED_EVENTFD;

	vq->signalled_used_valid = false;

	if (vq_is_packed(dev)) {
		rte_free(vq->shadow_used_packed);
		vq->shadow_used_packed = NULL;
	} else {
		rte_free(vq->shadow_used_split);
		vq->shadow_used_split = NULL;
	}

	rte_free(vq->batch_copy_elems);
	vq->batch_copy_elems = NULL;

	rte_free(vq->log_cache);
	vq->log_cache = NULL;

	ctx->msg.size = sizeof(ctx->msg.payload.state);
	ctx->fd_num = 0;

	vhost_user_iotlb_flush_all(dev);

	vring_invalidate(dev, vq);

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * when virtio queues are ready to work, qemu will send us to
 * enable the virtio queue pair.
 */
static int
vhost_user_set_vring_enable(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq;
	bool enable = !!ctx->msg.payload.state.num;
	int index = (int)ctx->msg.payload.state.index;

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"set queue enable: %d to qp idx: %d\n",
		enable, index);

	vq = dev->virtqueue[index];
	if (!(dev->flags & VIRTIO_DEV_VDPA_CONFIGURED)) {
		/* vhost_user_lock_all_queue_pairs locked all qps */
		vq_assert_lock(dev, vq);
		if (enable && vq->async && vq->async->pkts_inflight_n) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to enable vring. Inflight packets must be completed first\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	vq->enabled = enable;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_get_protocol_features(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t features, protocol_features;

	rte_vhost_driver_get_features(dev->ifname, &features);
	rte_vhost_driver_get_protocol_features(dev->ifname, &protocol_features);

	ctx->msg.payload.u64 = protocol_features;
	ctx->msg.size = sizeof(ctx->msg.payload.u64);
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_set_protocol_features(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t protocol_features = ctx->msg.payload.u64;
	uint64_t backend_protocol_features = 0;

	rte_vhost_driver_get_protocol_features(dev->ifname,
			&backend_protocol_features);
	if (protocol_features & ~backend_protocol_features) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "received invalid protocol features.\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->protocol_features = protocol_features;
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"negotiated Vhost-user protocol features: 0x%" PRIx64 "\n",
		dev->protocol_features);

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_log_base(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	int fd = ctx->fds[0];
	uint64_t size, off;
	uint64_t alignment;
	void *addr;
	uint32_t i;

	if (validate_msg_fds(dev, ctx, 1) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (fd < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "invalid log fd: %d\n", fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (ctx->msg.size != sizeof(VhostUserLog)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid log base msg size: %"PRId32" != %d\n",
			ctx->msg.size, (int)sizeof(VhostUserLog));
		goto close_msg_fds;
	}

	size = ctx->msg.payload.log.mmap_size;
	off  = ctx->msg.payload.log.mmap_offset;

	/* Check for mmap size and offset overflow. */
	if (off >= -size) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"log offset %#"PRIx64" and log size %#"PRIx64" overflow\n",
			off, size);
		goto close_msg_fds;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"log mmap size: %"PRId64", offset: %"PRId64"\n",
		size, off);

	/*
	 * mmap from 0 to workaround a hugepage mmap bug: mmap will
	 * fail when offset is not page size aligned.
	 */
	addr = mmap(0, size + off, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	alignment = get_blk_size(fd);
	close(fd);
	if (addr == MAP_FAILED) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "mmap log base failed!\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	/*
	 * Free previously mapped log memory on occasionally
	 * multiple VHOST_USER_SET_LOG_BASE.
	 */
	if (dev->log_addr) {
		munmap((void *)(uintptr_t)dev->log_addr, dev->log_size);
	}
	dev->log_addr = (uint64_t)(uintptr_t)addr;
	dev->log_base = dev->log_addr + off;
	dev->log_size = size;
	mem_set_dump(addr, size + off, false, alignment);

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		rte_free(vq->log_cache);
		vq->log_cache = NULL;
		vq->log_cache_nb_elem = 0;
		vq->log_cache = rte_malloc_socket("vq log cache",
				sizeof(struct log_cache_entry) * VHOST_LOG_CACHE_NR,
				0, vq->numa_node);
		/*
		 * If log cache alloc fail, don't fail migration, but no
		 * caching will be done, which will impact performance
		 */
		if (!vq->log_cache)
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to allocate VQ logging cache\n");
	}

	/*
	 * The spec is not clear about it (yet), but QEMU doesn't expect
	 * any payload in the reply.
	 */
	ctx->msg.size = 0;
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;

close_msg_fds:
	close_msg_fds(ctx);
	return RTE_VHOST_MSG_RESULT_ERR;
}

static int vhost_user_set_log_fd(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	if (validate_msg_fds(dev, ctx, 1) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	close(ctx->fds[0]);
	VHOST_LOG_CONFIG(dev->ifname, DEBUG, "not implemented.\n");

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * An rarp packet is constructed and broadcasted to notify switches about
 * the new location of the migrated VM, so that packets from outside will
 * not be lost after migration.
 *
 * However, we don't actually "send" a rarp packet here, instead, we set
 * a flag 'broadcast_rarp' to let rte_vhost_dequeue_burst() inject it.
 */
static int
vhost_user_send_rarp(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint8_t *mac = (uint8_t *)&ctx->msg.payload.u64;
	struct rte_vdpa_device *vdpa_dev;

	VHOST_LOG_CONFIG(dev->ifname, DEBUG,
		"MAC: " RTE_ETHER_ADDR_PRT_FMT "\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	memcpy(dev->mac.addr_bytes, mac, 6);

	/*
	 * Set the flag to inject a RARP broadcast packet at
	 * rte_vhost_dequeue_burst().
	 *
	 * __ATOMIC_RELEASE ordering is for making sure the mac is
	 * copied before the flag is set.
	 */
	__atomic_store_n(&dev->broadcast_rarp, 1, __ATOMIC_RELEASE);
	vdpa_dev = dev->vdpa_dev;
	if (vdpa_dev && vdpa_dev->ops->migration_done)
		vdpa_dev->ops->migration_done(dev->vid);

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_net_set_mtu(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	if (ctx->msg.payload.u64 < VIRTIO_MIN_MTU ||
			ctx->msg.payload.u64 > VIRTIO_MAX_MTU) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid MTU size (%"PRIu64")\n",
			ctx->msg.payload.u64);

		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->mtu = ctx->msg.payload.u64;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_req_fd(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	int fd = ctx->fds[0];

	if (validate_msg_fds(dev, ctx, 1) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (fd < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid file descriptor for backend channel (%d)\n", fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->backend_req_fd >= 0)
		close(dev->backend_req_fd);

	dev->backend_req_fd = fd;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
is_vring_iotlb_split(struct vhost_virtqueue *vq, struct vhost_iotlb_msg *imsg)
{
	struct vhost_vring_addr *ra;
	uint64_t start, end, len;

	start = imsg->iova;
	end = start + imsg->size;

	ra = &vq->ring_addrs;
	len = sizeof(struct vring_desc) * vq->size;
	if (ra->desc_user_addr < end && (ra->desc_user_addr + len) > start)
		return 1;

	len = sizeof(struct vring_avail) + sizeof(uint16_t) * vq->size;
	if (ra->avail_user_addr < end && (ra->avail_user_addr + len) > start)
		return 1;

	len = sizeof(struct vring_used) +
	       sizeof(struct vring_used_elem) * vq->size;
	if (ra->used_user_addr < end && (ra->used_user_addr + len) > start)
		return 1;

	if (ra->flags & (1 << VHOST_VRING_F_LOG)) {
		len = sizeof(uint64_t);
		if (ra->log_guest_addr < end &&
		    (ra->log_guest_addr + len) > start)
			return 1;
	}

	return 0;
}

static int
is_vring_iotlb_packed(struct vhost_virtqueue *vq, struct vhost_iotlb_msg *imsg)
{
	struct vhost_vring_addr *ra;
	uint64_t start, end, len;

	start = imsg->iova;
	end = start + imsg->size;

	ra = &vq->ring_addrs;
	len = sizeof(struct vring_packed_desc) * vq->size;
	if (ra->desc_user_addr < end && (ra->desc_user_addr + len) > start)
		return 1;

	len = sizeof(struct vring_packed_desc_event);
	if (ra->avail_user_addr < end && (ra->avail_user_addr + len) > start)
		return 1;

	len = sizeof(struct vring_packed_desc_event);
	if (ra->used_user_addr < end && (ra->used_user_addr + len) > start)
		return 1;

	if (ra->flags & (1 << VHOST_VRING_F_LOG)) {
		len = sizeof(uint64_t);
		if (ra->log_guest_addr < end &&
		    (ra->log_guest_addr + len) > start)
			return 1;
	}

	return 0;
}

static int is_vring_iotlb(struct virtio_net *dev,
			  struct vhost_virtqueue *vq,
			  struct vhost_iotlb_msg *imsg)
{
	if (vq_is_packed(dev))
		return is_vring_iotlb_packed(vq, imsg);
	else
		return is_vring_iotlb_split(vq, imsg);
}

static int
vhost_user_get_config(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct rte_vdpa_device *vdpa_dev = dev->vdpa_dev;
	int ret = 0;

	if (validate_msg_fds(dev, ctx, 0) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (!vdpa_dev) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "is not vDPA device!\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (vdpa_dev->ops->get_config) {
		ret = vdpa_dev->ops->get_config(dev->vid,
					   ctx->msg.payload.cfg.region,
					   ctx->msg.payload.cfg.size);
		if (ret != 0) {
			ctx->msg.size = 0;
			VHOST_LOG_CONFIG(dev->ifname, ERR, "get_config() return error!\n");
		}
	} else {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "get_config() not supported!\n");
	}

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_set_config(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct rte_vdpa_device *vdpa_dev = dev->vdpa_dev;
	int ret = 0;

	if (validate_msg_fds(dev, ctx, 0) != 0)
		return RTE_VHOST_MSG_RESULT_ERR;

	if (ctx->msg.payload.cfg.size > VHOST_USER_MAX_CONFIG_SIZE) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"vhost_user_config size: %"PRIu32", should not be larger than %d\n",
			ctx->msg.payload.cfg.size, VHOST_USER_MAX_CONFIG_SIZE);
		goto out;
	}

	if (!vdpa_dev) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "is not vDPA device!\n");
		goto out;
	}

	if (vdpa_dev->ops->set_config) {
		ret = vdpa_dev->ops->set_config(dev->vid,
			ctx->msg.payload.cfg.region,
			ctx->msg.payload.cfg.offset,
			ctx->msg.payload.cfg.size,
			ctx->msg.payload.cfg.flags);
		if (ret)
			VHOST_LOG_CONFIG(dev->ifname, ERR, "set_config() return error!\n");
	} else {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "set_config() not supported!\n");
	}

	return RTE_VHOST_MSG_RESULT_OK;

out:
	return RTE_VHOST_MSG_RESULT_ERR;
}

static int
vhost_user_iotlb_msg(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_iotlb_msg *imsg = &ctx->msg.payload.iotlb;
	uint16_t i;
	uint64_t vva, len, pg_sz;

	switch (imsg->type) {
	case VHOST_IOTLB_UPDATE:
		len = imsg->size;
		vva = qva_to_vva(dev, imsg->uaddr, &len);
		if (!vva)
			return RTE_VHOST_MSG_RESULT_ERR;

		pg_sz = hua_to_alignment(dev->mem, (void *)(uintptr_t)vva);

		vhost_user_iotlb_cache_insert(dev, imsg->iova, vva, 0, len, pg_sz, imsg->perm);

		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];

			if (!vq)
				continue;

			if (is_vring_iotlb(dev, vq, imsg)) {
				rte_rwlock_write_lock(&vq->access_lock);
				translate_ring_addresses(&dev, &vq);
				*pdev = dev;
				rte_rwlock_write_unlock(&vq->access_lock);
			}
		}
		break;
	case VHOST_IOTLB_INVALIDATE:
		vhost_user_iotlb_cache_remove(dev, imsg->iova, imsg->size);

		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];

			if (!vq)
				continue;

			if (is_vring_iotlb(dev, vq, imsg)) {
				rte_rwlock_write_lock(&vq->access_lock);
				vring_invalidate(dev, vq);
				rte_rwlock_write_unlock(&vq->access_lock);
			}
		}
		break;
	default:
		VHOST_LOG_CONFIG(dev->ifname, ERR, "invalid IOTLB message type (%d)\n",
			imsg->type);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_postcopy_advise(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
#ifdef RTE_LIBRTE_VHOST_POSTCOPY
	struct uffdio_api api_struct;

	dev->postcopy_ufd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);

	if (dev->postcopy_ufd == -1) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"userfaultfd not available: %s\n",
			strerror(errno));
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	api_struct.api = UFFD_API;
	api_struct.features = 0;
	if (ioctl(dev->postcopy_ufd, UFFDIO_API, &api_struct)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"UFFDIO_API ioctl failure: %s\n",
			strerror(errno));
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	ctx->fds[0] = dev->postcopy_ufd;
	ctx->fd_num = 1;

	return RTE_VHOST_MSG_RESULT_REPLY;
#else
	dev->postcopy_ufd = -1;
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_ERR;
#endif
}

static int
vhost_user_set_postcopy_listen(struct virtio_net **pdev,
			struct vhu_msg_context *ctx __rte_unused,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	if (dev->mem && dev->mem->nregions) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"regions already registered at postcopy-listen\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	dev->postcopy_listening = 1;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_postcopy_end(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	dev->postcopy_listening = 0;
	if (dev->postcopy_ufd >= 0) {
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
	}

	ctx->msg.payload.u64 = 0;
	ctx->msg.size = sizeof(ctx->msg.payload.u64);
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_get_status(struct virtio_net **pdev,
		      struct vhu_msg_context *ctx,
		      int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	ctx->msg.payload.u64 = dev->status;
	ctx->msg.size = sizeof(ctx->msg.payload.u64);
	ctx->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_set_status(struct virtio_net **pdev,
			struct vhu_msg_context *ctx,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	/* As per Virtio specification, the device status is 8bits long */
	if (ctx->msg.payload.u64 > UINT8_MAX) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"invalid VHOST_USER_SET_STATUS payload 0x%" PRIx64 "\n",
			ctx->msg.payload.u64);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->status = ctx->msg.payload.u64;

	if ((dev->status & VIRTIO_DEVICE_STATUS_FEATURES_OK) &&
	    (dev->flags & VIRTIO_DEV_FEATURES_FAILED)) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"FEATURES_OK bit is set but feature negotiation failed\n");
		/*
		 * Clear the bit to let the driver know about the feature
		 * negotiation failure
		 */
		dev->status &= ~VIRTIO_DEVICE_STATUS_FEATURES_OK;
	}

	VHOST_LOG_CONFIG(dev->ifname, INFO, "new device status(0x%08x):\n", dev->status);
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-RESET: %u\n",
		(dev->status == VIRTIO_DEVICE_STATUS_RESET));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-ACKNOWLEDGE: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_ACK));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-DRIVER: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_DRIVER));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-FEATURES_OK: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_FEATURES_OK));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-DRIVER_OK: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_DRIVER_OK));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-DEVICE_NEED_RESET: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_DEV_NEED_RESET));
	VHOST_LOG_CONFIG(dev->ifname, INFO,
		"\t-FAILED: %u\n",
		!!(dev->status & VIRTIO_DEVICE_STATUS_FAILED));

	return RTE_VHOST_MSG_RESULT_OK;
}

#define VHOST_MESSAGE_HANDLERS \
VHOST_MESSAGE_HANDLER(VHOST_USER_NONE, NULL, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_FEATURES, vhost_user_get_features, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_FEATURES, vhost_user_set_features, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_OWNER, vhost_user_set_owner, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_RESET_OWNER, vhost_user_reset_owner, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_MEM_TABLE, vhost_user_set_mem_table, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_LOG_BASE, vhost_user_set_log_base, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_LOG_FD, vhost_user_set_log_fd, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_NUM, vhost_user_set_vring_num, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_ADDR, vhost_user_set_vring_addr, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_BASE, vhost_user_set_vring_base, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_VRING_BASE, vhost_user_get_vring_base, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_KICK, vhost_user_set_vring_kick, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_CALL, vhost_user_set_vring_call, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_ERR, vhost_user_set_vring_err, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_PROTOCOL_FEATURES, vhost_user_get_protocol_features, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_PROTOCOL_FEATURES, vhost_user_set_protocol_features, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_QUEUE_NUM, vhost_user_get_queue_num, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_VRING_ENABLE, vhost_user_set_vring_enable, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SEND_RARP, vhost_user_send_rarp, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_NET_SET_MTU, vhost_user_net_set_mtu, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_BACKEND_REQ_FD, vhost_user_set_req_fd, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_IOTLB_MSG, vhost_user_iotlb_msg, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_CONFIG, vhost_user_get_config, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_CONFIG, vhost_user_set_config, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_POSTCOPY_ADVISE, vhost_user_set_postcopy_advise, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_POSTCOPY_LISTEN, vhost_user_set_postcopy_listen, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_POSTCOPY_END, vhost_user_postcopy_end, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_INFLIGHT_FD, vhost_user_get_inflight_fd, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_INFLIGHT_FD, vhost_user_set_inflight_fd, true) \
VHOST_MESSAGE_HANDLER(VHOST_USER_SET_STATUS, vhost_user_set_status, false) \
VHOST_MESSAGE_HANDLER(VHOST_USER_GET_STATUS, vhost_user_get_status, false)

#define VHOST_MESSAGE_HANDLER(id, handler, accepts_fd) \
	[id] = { #id, handler, accepts_fd },
static vhost_message_handler_t vhost_message_handlers[] = {
	VHOST_MESSAGE_HANDLERS
};
#undef VHOST_MESSAGE_HANDLER

/* return bytes# of read on success or negative val on failure. */
static int
read_vhost_message(struct virtio_net *dev, int sockfd, struct  vhu_msg_context *ctx)
{
	int ret;

	ret = read_fd_message(dev->ifname, sockfd, (char *)&ctx->msg, VHOST_USER_HDR_SIZE,
		ctx->fds, VHOST_MEMORY_MAX_NREGIONS, &ctx->fd_num);
	if (ret <= 0)
		goto out;

	if (ret != VHOST_USER_HDR_SIZE) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "Unexpected header size read\n");
		ret = -1;
		goto out;
	}

	if (ctx->msg.size) {
		if (ctx->msg.size > sizeof(ctx->msg.payload)) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "invalid msg size: %d\n",
				ctx->msg.size);
			ret = -1;
			goto out;
		}
		ret = read(sockfd, &ctx->msg.payload, ctx->msg.size);
		if (ret <= 0)
			goto out;
		if (ret != (int)ctx->msg.size) {
			VHOST_LOG_CONFIG(dev->ifname, ERR, "read control message failed\n");
			ret = -1;
			goto out;
		}
	}

out:
	if (ret <= 0)
		close_msg_fds(ctx);

	return ret;
}

static int
send_vhost_message(struct virtio_net *dev, int sockfd, struct vhu_msg_context *ctx)
{
	if (!ctx)
		return 0;

	return send_fd_message(dev->ifname, sockfd, (char *)&ctx->msg,
		VHOST_USER_HDR_SIZE + ctx->msg.size, ctx->fds, ctx->fd_num);
}

static int
send_vhost_reply(struct virtio_net *dev, int sockfd, struct vhu_msg_context *ctx)
{
	if (!ctx)
		return 0;

	ctx->msg.flags &= ~VHOST_USER_VERSION_MASK;
	ctx->msg.flags &= ~VHOST_USER_NEED_REPLY;
	ctx->msg.flags |= VHOST_USER_VERSION;
	ctx->msg.flags |= VHOST_USER_REPLY_MASK;

	return send_vhost_message(dev, sockfd, ctx);
}

static int
send_vhost_backend_message(struct virtio_net *dev, struct vhu_msg_context *ctx)
{
	return send_vhost_message(dev, dev->backend_req_fd, ctx);
}

static int
send_vhost_backend_message_process_reply(struct virtio_net *dev, struct vhu_msg_context *ctx)
{
	struct vhu_msg_context msg_reply;
	int ret;

	rte_spinlock_lock(&dev->backend_req_lock);
	ret = send_vhost_backend_message(dev, ctx);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to send config change (%d)\n", ret);
		goto out;
	}

	ret = read_vhost_message(dev, dev->backend_req_fd, &msg_reply);
	if (ret <= 0) {
		if (ret < 0)
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"vhost read backend message reply failed\n");
		else
			VHOST_LOG_CONFIG(dev->ifname, INFO, "vhost peer closed\n");
		ret = -1;
		goto out;
	}

	if (msg_reply.msg.request.backend != ctx->msg.request.backend) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"received unexpected msg type (%u), expected %u\n",
			msg_reply.msg.request.backend, ctx->msg.request.backend);
		ret = -1;
		goto out;
	}

	ret = msg_reply.msg.payload.u64 ? -1 : 0;
out:
	rte_spinlock_unlock(&dev->backend_req_lock);
	return ret;
}

/*
 * Allocate a queue pair if it hasn't been allocated yet
 */
static int
vhost_user_check_and_alloc_queue_pair(struct virtio_net *dev,
			struct vhu_msg_context *ctx)
{
	uint32_t vring_idx;

	switch (ctx->msg.request.frontend) {
	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_VRING_ERR:
		vring_idx = ctx->msg.payload.u64 & VHOST_USER_VRING_IDX_MASK;
		break;
	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_GET_VRING_BASE:
	case VHOST_USER_SET_VRING_ENABLE:
		vring_idx = ctx->msg.payload.state.index;
		break;
	case VHOST_USER_SET_VRING_ADDR:
		vring_idx = ctx->msg.payload.addr.index;
		break;
	case VHOST_USER_SET_INFLIGHT_FD:
		vring_idx = ctx->msg.payload.inflight.num_queues - 1;
		break;
	default:
		return 0;
	}

	if (vring_idx >= VHOST_MAX_VRING) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "invalid vring index: %u\n", vring_idx);
		return -1;
	}

	if (dev->virtqueue[vring_idx])
		return 0;

	return alloc_vring_queue(dev, vring_idx);
}

static void
vhost_user_lock_all_queue_pairs(struct virtio_net *dev)
	__rte_no_thread_safety_analysis
{
	unsigned int i = 0;
	unsigned int vq_num = 0;

	while (vq_num < dev->nr_vring) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq) {
			rte_rwlock_write_lock(&vq->access_lock);
			vq_num++;
		}
		i++;
	}
}

static void
vhost_user_unlock_all_queue_pairs(struct virtio_net *dev)
	__rte_no_thread_safety_analysis
{
	unsigned int i = 0;
	unsigned int vq_num = 0;

	while (vq_num < dev->nr_vring) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq) {
			rte_rwlock_write_unlock(&vq->access_lock);
			vq_num++;
		}
		i++;
	}
}

int
vhost_user_msg_handler(int vid, int fd)
{
	struct virtio_net *dev;
	struct vhu_msg_context ctx;
	vhost_message_handler_t *msg_handler;
	struct rte_vdpa_device *vdpa_dev;
	int msg_result = RTE_VHOST_MSG_RESULT_OK;
	int ret;
	int unlock_required = 0;
	bool handled;
	uint32_t request;
	uint32_t i;
	uint16_t blk_call_fd;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	if (!dev->notify_ops) {
		dev->notify_ops = vhost_driver_callback_get(dev->ifname);
		if (!dev->notify_ops) {
			VHOST_LOG_CONFIG(dev->ifname, ERR,
				"failed to get callback ops for driver\n");
			return -1;
		}
	}

	ctx.msg.request.frontend = VHOST_USER_NONE;
	ret = read_vhost_message(dev, fd, &ctx);
	if (ret == 0) {
		VHOST_LOG_CONFIG(dev->ifname, INFO, "vhost peer closed\n");
		return -1;
	}

	request = ctx.msg.request.frontend;
	if (request > VHOST_USER_NONE && request < RTE_DIM(vhost_message_handlers))
		msg_handler = &vhost_message_handlers[request];
	else
		msg_handler = NULL;

	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "vhost read message %s%s%sfailed\n",
				msg_handler != NULL ? "for " : "",
				msg_handler != NULL ? msg_handler->description : "",
				msg_handler != NULL ? " " : "");
		return -1;
	}

	if (msg_handler != NULL && msg_handler->description != NULL) {
		if (request != VHOST_USER_IOTLB_MSG)
			VHOST_LOG_CONFIG(dev->ifname, INFO,
				"read message %s\n",
				msg_handler->description);
		else
			VHOST_LOG_CONFIG(dev->ifname, DEBUG,
				"read message %s\n",
				msg_handler->description);
	} else {
		VHOST_LOG_CONFIG(dev->ifname, DEBUG, "external request %d\n", request);
	}

	ret = vhost_user_check_and_alloc_queue_pair(dev, &ctx);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to alloc queue\n");
		return -1;
	}

	/*
	 * Note: we don't lock all queues on VHOST_USER_GET_VRING_BASE
	 * and VHOST_USER_RESET_OWNER, since it is sent when virtio stops
	 * and device is destroyed. destroy_device waits for queues to be
	 * inactive, so it is safe. Otherwise taking the access_lock
	 * would cause a dead lock.
	 */
	switch (request) {
	case VHOST_USER_SET_FEATURES:
	case VHOST_USER_SET_PROTOCOL_FEATURES:
	case VHOST_USER_SET_OWNER:
	case VHOST_USER_SET_MEM_TABLE:
	case VHOST_USER_SET_LOG_BASE:
	case VHOST_USER_SET_LOG_FD:
	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_ADDR:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_VRING_ERR:
	case VHOST_USER_SET_VRING_ENABLE:
	case VHOST_USER_SEND_RARP:
	case VHOST_USER_NET_SET_MTU:
	case VHOST_USER_SET_BACKEND_REQ_FD:
		if (!(dev->flags & VIRTIO_DEV_VDPA_CONFIGURED)) {
			vhost_user_lock_all_queue_pairs(dev);
			unlock_required = 1;
		}
		break;
	default:
		break;

	}

	handled = false;
	if (dev->extern_ops.pre_msg_handle) {
		RTE_BUILD_BUG_ON(offsetof(struct vhu_msg_context, msg) != 0);
		msg_result = (*dev->extern_ops.pre_msg_handle)(dev->vid, &ctx);
		switch (msg_result) {
		case RTE_VHOST_MSG_RESULT_REPLY:
			send_vhost_reply(dev, fd, &ctx);
			/* Fall-through */
		case RTE_VHOST_MSG_RESULT_ERR:
		case RTE_VHOST_MSG_RESULT_OK:
			handled = true;
			goto skip_to_post_handle;
		case RTE_VHOST_MSG_RESULT_NOT_HANDLED:
		default:
			break;
		}
	}

	if (msg_handler == NULL || msg_handler->callback == NULL)
		goto skip_to_post_handle;

	if (!msg_handler->accepts_fd && validate_msg_fds(dev, &ctx, 0) != 0) {
		msg_result = RTE_VHOST_MSG_RESULT_ERR;
	} else {
		msg_result = msg_handler->callback(&dev, &ctx, fd);
	}

	switch (msg_result) {
	case RTE_VHOST_MSG_RESULT_ERR:
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"processing %s failed.\n",
			msg_handler->description);
		handled = true;
		break;
	case RTE_VHOST_MSG_RESULT_OK:
		VHOST_LOG_CONFIG(dev->ifname, DEBUG,
			"processing %s succeeded.\n",
			msg_handler->description);
		handled = true;
		break;
	case RTE_VHOST_MSG_RESULT_REPLY:
		VHOST_LOG_CONFIG(dev->ifname, DEBUG,
			"processing %s succeeded and needs reply.\n",
			msg_handler->description);
		send_vhost_reply(dev, fd, &ctx);
		handled = true;
		break;
	default:
		break;
	}

skip_to_post_handle:
	if (msg_result != RTE_VHOST_MSG_RESULT_ERR &&
			dev->extern_ops.post_msg_handle) {
		RTE_BUILD_BUG_ON(offsetof(struct vhu_msg_context, msg) != 0);
		msg_result = (*dev->extern_ops.post_msg_handle)(dev->vid, &ctx);
		switch (msg_result) {
		case RTE_VHOST_MSG_RESULT_REPLY:
			send_vhost_reply(dev, fd, &ctx);
			/* Fall-through */
		case RTE_VHOST_MSG_RESULT_ERR:
		case RTE_VHOST_MSG_RESULT_OK:
			handled = true;
		case RTE_VHOST_MSG_RESULT_NOT_HANDLED:
		default:
			break;
		}
	}

	/* If message was not handled at this stage, treat it as an error */
	if (!handled) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"vhost message (req: %d) was not handled.\n",
			request);
		close_msg_fds(&ctx);
		msg_result = RTE_VHOST_MSG_RESULT_ERR;
	}

	/*
	 * If the request required a reply that was already sent,
	 * this optional reply-ack won't be sent as the
	 * VHOST_USER_NEED_REPLY was cleared in send_vhost_reply().
	 */
	if (ctx.msg.flags & VHOST_USER_NEED_REPLY) {
		ctx.msg.payload.u64 = msg_result == RTE_VHOST_MSG_RESULT_ERR;
		ctx.msg.size = sizeof(ctx.msg.payload.u64);
		ctx.fd_num = 0;
		send_vhost_reply(dev, fd, &ctx);
	} else if (msg_result == RTE_VHOST_MSG_RESULT_ERR) {
		VHOST_LOG_CONFIG(dev->ifname, ERR, "vhost message handling failed.\n");
		ret = -1;
		goto unlock;
	}

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];
		bool cur_ready = vq_is_ready(dev, vq);

		if (cur_ready != (vq && vq->ready)) {
			vq->ready = cur_ready;
			vhost_user_notify_queue_state(dev, vq, cur_ready);
		}
	}

unlock:
	if (unlock_required)
		vhost_user_unlock_all_queue_pairs(dev);

	if (ret != 0 || !virtio_is_ready(dev))
		goto out;

	/*
	 * Virtio is now ready. If not done already, it is time
	 * to notify the application it can process the rings and
	 * configure the vDPA device if present.
	 */

	if (!(dev->flags & VIRTIO_DEV_RUNNING)) {
		if (dev->notify_ops->new_device(dev->vid) == 0)
			dev->flags |= VIRTIO_DEV_RUNNING;
	}

	vdpa_dev = dev->vdpa_dev;
	if (!vdpa_dev)
		goto out;

	if (vdpa_dev->type == RTE_VHOST_VDPA_DEVICE_TYPE_BLK) {
		if (request == VHOST_USER_SET_VRING_CALL) {
			blk_call_fd = ctx.msg.payload.u64 & VHOST_USER_VRING_IDX_MASK;
			if (blk_call_fd != dev->nr_vring - 1)
				goto out;
		} else {
			goto out;
		}
	}

	if (!(dev->flags & VIRTIO_DEV_VDPA_CONFIGURED)) {
		if (vdpa_dev->ops->dev_conf(dev->vid))
			VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to configure vDPA device\n");
		else
			dev->flags |= VIRTIO_DEV_VDPA_CONFIGURED;
	}

out:
	return ret;
}

static int
vhost_user_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm)
{
	int ret;
	struct vhu_msg_context ctx = {
		.msg = {
			.request.backend = VHOST_USER_BACKEND_IOTLB_MSG,
			.flags = VHOST_USER_VERSION,
			.size = sizeof(ctx.msg.payload.iotlb),
			.payload.iotlb = {
				.iova = iova,
				.perm = perm,
				.type = VHOST_IOTLB_MISS,
			},
		},
	};

	ret = send_vhost_message(dev, dev->backend_req_fd, &ctx);
	if (ret < 0) {
		VHOST_LOG_CONFIG(dev->ifname, ERR,
			"failed to send IOTLB miss message (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

int
rte_vhost_backend_config_change(int vid, bool need_reply)
{
	struct vhu_msg_context ctx = {
		.msg = {
			.request.backend = VHOST_USER_BACKEND_CONFIG_CHANGE_MSG,
			.flags = VHOST_USER_VERSION,
			.size = 0,
		}
	};
	struct virtio_net *dev;
	int ret;

	dev = get_device(vid);
	if (!dev)
		return -ENODEV;

	if (!need_reply) {
		ret = send_vhost_backend_message(dev, &ctx);
	} else {
		ctx.msg.flags |= VHOST_USER_NEED_REPLY;
		ret = send_vhost_backend_message_process_reply(dev, &ctx);
	}

	if (ret < 0)
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to send config change (%d)\n", ret);
	return ret;
}

static int vhost_user_backend_set_vring_host_notifier(struct virtio_net *dev,
						    int index, int fd,
						    uint64_t offset,
						    uint64_t size)
{
	int ret;
	struct vhu_msg_context ctx = {
		.msg = {
			.request.backend = VHOST_USER_BACKEND_VRING_HOST_NOTIFIER_MSG,
			.flags = VHOST_USER_VERSION | VHOST_USER_NEED_REPLY,
			.size = sizeof(ctx.msg.payload.area),
			.payload.area = {
				.u64 = index & VHOST_USER_VRING_IDX_MASK,
				.size = size,
				.offset = offset,
			},
		},
	};

	if (fd < 0)
		ctx.msg.payload.area.u64 |= VHOST_USER_VRING_NOFD_MASK;
	else {
		ctx.fds[0] = fd;
		ctx.fd_num = 1;
	}

	ret = send_vhost_backend_message_process_reply(dev, &ctx);
	if (ret < 0)
		VHOST_LOG_CONFIG(dev->ifname, ERR, "failed to set host notifier (%d)\n", ret);

	return ret;
}

int rte_vhost_host_notifier_ctrl(int vid, uint16_t qid, bool enable)
{
	struct virtio_net *dev;
	struct rte_vdpa_device *vdpa_dev;
	int vfio_device_fd, ret = 0;
	uint64_t offset, size;
	unsigned int i, q_start, q_last;

	dev = get_device(vid);
	if (!dev)
		return -ENODEV;

	vdpa_dev = dev->vdpa_dev;
	if (vdpa_dev == NULL)
		return -ENODEV;

	if (!(dev->features & (1ULL << VIRTIO_F_VERSION_1)) ||
	    !(dev->features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER)))
		return -ENOTSUP;

	if (qid == RTE_VHOST_QUEUE_ALL) {
		q_start = 0;
		q_last = dev->nr_vring - 1;
	} else {
		if (qid >= dev->nr_vring)
			return -EINVAL;
		q_start = qid;
		q_last = qid;
	}

	if (vdpa_dev->ops->get_vfio_device_fd == NULL)
		return -ENOTSUP;
	if (vdpa_dev->ops->get_notify_area == NULL)
		return -ENOTSUP;

	vfio_device_fd = vdpa_dev->ops->get_vfio_device_fd(vid);
	if (vfio_device_fd < 0)
		return -ENOTSUP;

	if (enable) {
		for (i = q_start; i <= q_last; i++) {
			if (vdpa_dev->ops->get_notify_area(vid, i, &offset,
					&size) < 0) {
				ret = -ENOTSUP;
				goto disable;
			}

			if (vhost_user_backend_set_vring_host_notifier(dev, i,
					vfio_device_fd, offset, size) < 0) {
				ret = -EFAULT;
				goto disable;
			}
		}
	} else {
disable:
		for (i = q_start; i <= q_last; i++) {
			vhost_user_backend_set_vring_host_notifier(dev, i, -1,
					0, 0);
		}
	}

	return ret;
}

static int
vhost_user_inject_irq(struct virtio_net *dev __rte_unused, struct vhost_virtqueue *vq)
{
	if (vq->callfd < 0)
		return -1;

	return eventfd_write(vq->callfd, (eventfd_t)1);
}

static struct vhost_backend_ops vhost_user_backend_ops = {
	.iotlb_miss = vhost_user_iotlb_miss,
	.inject_irq = vhost_user_inject_irq,
};

int
vhost_user_new_device(void)
{
	return vhost_new_device(&vhost_user_backend_ops);
}
