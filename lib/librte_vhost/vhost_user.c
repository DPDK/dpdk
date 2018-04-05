/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

/* Security model
 * --------------
 * The vhost-user protocol connection is an external interface, so it must be
 * robust against invalid inputs.
 *
 * This is important because the vhost-user master is only one step removed
 * from the guest.  Malicious guests that have escaped will then launch further
 * attacks from the vhost-user master.
 *
 * Even in deployments where guests are trusted, a bug in the vhost-user master
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
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_log.h>

#include "iotlb.h"
#include "vhost.h"
#include "vhost_user.h"

#define VIRTIO_MIN_MTU 68
#define VIRTIO_MAX_MTU 65535

static const char *vhost_message_str[VHOST_USER_MAX] = {
	[VHOST_USER_NONE] = "VHOST_USER_NONE",
	[VHOST_USER_GET_FEATURES] = "VHOST_USER_GET_FEATURES",
	[VHOST_USER_SET_FEATURES] = "VHOST_USER_SET_FEATURES",
	[VHOST_USER_SET_OWNER] = "VHOST_USER_SET_OWNER",
	[VHOST_USER_RESET_OWNER] = "VHOST_USER_RESET_OWNER",
	[VHOST_USER_SET_MEM_TABLE] = "VHOST_USER_SET_MEM_TABLE",
	[VHOST_USER_SET_LOG_BASE] = "VHOST_USER_SET_LOG_BASE",
	[VHOST_USER_SET_LOG_FD] = "VHOST_USER_SET_LOG_FD",
	[VHOST_USER_SET_VRING_NUM] = "VHOST_USER_SET_VRING_NUM",
	[VHOST_USER_SET_VRING_ADDR] = "VHOST_USER_SET_VRING_ADDR",
	[VHOST_USER_SET_VRING_BASE] = "VHOST_USER_SET_VRING_BASE",
	[VHOST_USER_GET_VRING_BASE] = "VHOST_USER_GET_VRING_BASE",
	[VHOST_USER_SET_VRING_KICK] = "VHOST_USER_SET_VRING_KICK",
	[VHOST_USER_SET_VRING_CALL] = "VHOST_USER_SET_VRING_CALL",
	[VHOST_USER_SET_VRING_ERR]  = "VHOST_USER_SET_VRING_ERR",
	[VHOST_USER_GET_PROTOCOL_FEATURES]  = "VHOST_USER_GET_PROTOCOL_FEATURES",
	[VHOST_USER_SET_PROTOCOL_FEATURES]  = "VHOST_USER_SET_PROTOCOL_FEATURES",
	[VHOST_USER_GET_QUEUE_NUM]  = "VHOST_USER_GET_QUEUE_NUM",
	[VHOST_USER_SET_VRING_ENABLE]  = "VHOST_USER_SET_VRING_ENABLE",
	[VHOST_USER_SEND_RARP]  = "VHOST_USER_SEND_RARP",
	[VHOST_USER_NET_SET_MTU]  = "VHOST_USER_NET_SET_MTU",
	[VHOST_USER_SET_SLAVE_REQ_FD]  = "VHOST_USER_SET_SLAVE_REQ_FD",
	[VHOST_USER_IOTLB_MSG]  = "VHOST_USER_IOTLB_MSG",
};

static uint64_t
get_blk_size(int fd)
{
	struct stat stat;
	int ret;

	ret = fstat(fd, &stat);
	return ret == -1 ? (uint64_t)-1 : (uint64_t)stat.st_blksize;
}

static void
free_mem_region(struct virtio_net *dev)
{
	uint32_t i;
	struct rte_vhost_mem_region *reg;

	if (!dev || !dev->mem)
		return;

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
	if (dev->mem) {
		free_mem_region(dev);
		rte_free(dev->mem);
		dev->mem = NULL;
	}

	free(dev->guest_pages);
	dev->guest_pages = NULL;

	if (dev->log_addr) {
		munmap((void *)(uintptr_t)dev->log_addr, dev->log_size);
		dev->log_addr = 0;
	}

	if (dev->slave_req_fd >= 0) {
		close(dev->slave_req_fd);
		dev->slave_req_fd = -1;
	}
}

/*
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
static int
vhost_user_set_owner(void)
{
	return 0;
}

static int
vhost_user_reset_owner(struct virtio_net *dev)
{
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		did = dev->vdpa_dev_id;
		vdpa_dev = rte_vdpa_get_device(did);
		if (vdpa_dev && vdpa_dev->ops->dev_close)
			vdpa_dev->ops->dev_close(dev->vid);
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		dev->notify_ops->destroy_device(dev->vid);
	}

	cleanup_device(dev, 0);
	reset_device(dev);
	return 0;
}

/*
 * The features that we support are requested.
 */
static uint64_t
vhost_user_get_features(struct virtio_net *dev)
{
	uint64_t features = 0;

	rte_vhost_driver_get_features(dev->ifname, &features);
	return features;
}

/*
 * The queue number that we support are requested.
 */
static uint32_t
vhost_user_get_queue_num(struct virtio_net *dev)
{
	uint32_t queue_num = 0;

	rte_vhost_driver_get_queue_num(dev->ifname, &queue_num);
	return queue_num;
}

/*
 * We receive the negotiated features supported by us and the virtio device.
 */
static int
vhost_user_set_features(struct virtio_net *dev, uint64_t features)
{
	uint64_t vhost_features = 0;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	rte_vhost_driver_get_features(dev->ifname, &vhost_features);
	if (features & ~vhost_features) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) received invalid negotiated features.\n",
			dev->vid);
		return -1;
	}

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		if (dev->features == features)
			return 0;

		/*
		 * Error out if master tries to change features while device is
		 * in running state. The exception being VHOST_F_LOG_ALL, which
		 * is enabled when the live-migration starts.
		 */
		if ((dev->features ^ features) & ~(1ULL << VHOST_F_LOG_ALL)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"(%d) features changed while device is running.\n",
				dev->vid);
			return -1;
		}

		if (dev->notify_ops->features_changed)
			dev->notify_ops->features_changed(dev->vid, features);
	}

	did = dev->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (vdpa_dev && vdpa_dev->ops->set_features)
		vdpa_dev->ops->set_features(dev->vid);

	dev->features = features;
	if (dev->features &
		((1 << VIRTIO_NET_F_MRG_RXBUF) | (1ULL << VIRTIO_F_VERSION_1))) {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr);
	}
	VHOST_LOG_DEBUG(VHOST_CONFIG,
		"(%d) mergeable RX buffers %s, virtio 1 %s\n",
		dev->vid,
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
			free_vq(vq);
		}
	}

	return 0;
}

/*
 * The virtio device sends us the size of the descriptor ring.
 */
static int
vhost_user_set_vring_num(struct virtio_net *dev,
			 VhostUserMsg *msg)
{
	struct vhost_virtqueue *vq = dev->virtqueue[msg->payload.state.index];

	vq->size = msg->payload.state.num;

	/* VIRTIO 1.0, 2.4 Virtqueues says:
	 *
	 *   Queue Size value is always a power of 2. The maximum Queue Size
	 *   value is 32768.
	 */
	if ((vq->size & (vq->size - 1)) || vq->size > 32768) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"invalid virtqueue size %u\n", vq->size);
		return -1;
	}

	if (dev->dequeue_zero_copy) {
		vq->nr_zmbuf = 0;
		vq->last_zmbuf_idx = 0;
		vq->zmbuf_size = vq->size;
		vq->zmbufs = rte_zmalloc(NULL, vq->zmbuf_size *
					 sizeof(struct zcopy_mbuf), 0);
		if (vq->zmbufs == NULL) {
			RTE_LOG(WARNING, VHOST_CONFIG,
				"failed to allocate mem for zero copy; "
				"zero copy is force disabled\n");
			dev->dequeue_zero_copy = 0;
		}
		TAILQ_INIT(&vq->zmbuf_list);
	}

	vq->shadow_used_ring = rte_malloc(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE);
	if (!vq->shadow_used_ring) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to allocate memory for shadow used ring.\n");
		return -1;
	}

	vq->batch_copy_elems = rte_malloc(NULL,
				vq->size * sizeof(struct batch_copy_elem),
				RTE_CACHE_LINE_SIZE);
	if (!vq->batch_copy_elems) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to allocate memory for batching copy.\n");
		return -1;
	}

	return 0;
}

/*
 * Reallocate virtio_dev and vhost_virtqueue data structure to make them on the
 * same numa node as the memory of vring descriptor.
 */
#ifdef RTE_LIBRTE_VHOST_NUMA
static struct virtio_net*
numa_realloc(struct virtio_net *dev, int index)
{
	int oldnode, newnode;
	struct virtio_net *old_dev;
	struct vhost_virtqueue *old_vq, *vq;
	struct zcopy_mbuf *new_zmbuf;
	struct vring_used_elem *new_shadow_used_ring;
	struct batch_copy_elem *new_batch_copy_elems;
	int ret;

	old_dev = dev;
	vq = old_vq = dev->virtqueue[index];

	ret = get_mempolicy(&newnode, NULL, 0, old_vq->desc,
			    MPOL_F_NODE | MPOL_F_ADDR);

	/* check if we need to reallocate vq */
	ret |= get_mempolicy(&oldnode, NULL, 0, old_vq,
			     MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Unable to get vq numa information.\n");
		return dev;
	}
	if (oldnode != newnode) {
		RTE_LOG(INFO, VHOST_CONFIG,
			"reallocate vq from %d to %d node\n", oldnode, newnode);
		vq = rte_malloc_socket(NULL, sizeof(*vq), 0, newnode);
		if (!vq)
			return dev;

		memcpy(vq, old_vq, sizeof(*vq));
		TAILQ_INIT(&vq->zmbuf_list);

		new_zmbuf = rte_malloc_socket(NULL, vq->zmbuf_size *
			sizeof(struct zcopy_mbuf), 0, newnode);
		if (new_zmbuf) {
			rte_free(vq->zmbufs);
			vq->zmbufs = new_zmbuf;
		}

		new_shadow_used_ring = rte_malloc_socket(NULL,
			vq->size * sizeof(struct vring_used_elem),
			RTE_CACHE_LINE_SIZE,
			newnode);
		if (new_shadow_used_ring) {
			rte_free(vq->shadow_used_ring);
			vq->shadow_used_ring = new_shadow_used_ring;
		}

		new_batch_copy_elems = rte_malloc_socket(NULL,
			vq->size * sizeof(struct batch_copy_elem),
			RTE_CACHE_LINE_SIZE,
			newnode);
		if (new_batch_copy_elems) {
			rte_free(vq->batch_copy_elems);
			vq->batch_copy_elems = new_batch_copy_elems;
		}

		rte_free(old_vq);
	}

	/* check if we need to reallocate dev */
	ret = get_mempolicy(&oldnode, NULL, 0, old_dev,
			    MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Unable to get dev numa information.\n");
		goto out;
	}
	if (oldnode != newnode) {
		RTE_LOG(INFO, VHOST_CONFIG,
			"reallocate dev from %d to %d node\n",
			oldnode, newnode);
		dev = rte_malloc_socket(NULL, sizeof(*dev), 0, newnode);
		if (!dev) {
			dev = old_dev;
			goto out;
		}

		memcpy(dev, old_dev, sizeof(*dev));
		rte_free(old_dev);
	}

out:
	dev->virtqueue[index] = vq;
	vhost_devices[dev->vid] = dev;

	if (old_vq != vq)
		vhost_user_iotlb_init(dev, index);

	return dev;
}
#else
static struct virtio_net*
numa_realloc(struct virtio_net *dev, int index __rte_unused)
{
	return dev;
}
#endif

/* Converts QEMU virtual address to Vhost virtual address. */
static uint64_t
qva_to_vva(struct virtio_net *dev, uint64_t qva)
{
	struct rte_vhost_mem_region *reg;
	uint32_t i;

	/* Find the region where the address lives. */
	for (i = 0; i < dev->mem->nregions; i++) {
		reg = &dev->mem->regions[i];

		if (qva >= reg->guest_user_addr &&
		    qva <  reg->guest_user_addr + reg->size) {
			return qva - reg->guest_user_addr +
			       reg->host_user_addr;
		}
	}

	return 0;
}


/*
 * Converts ring address to Vhost virtual address.
 * If IOMMU is enabled, the ring address is a guest IO virtual address,
 * else it is a QEMU virtual address.
 */
static uint64_t
ring_addr_to_vva(struct virtio_net *dev, struct vhost_virtqueue *vq,
		uint64_t ra, uint64_t size)
{
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)) {
		uint64_t vva;

		vva = vhost_user_iotlb_cache_find(vq, ra,
					&size, VHOST_ACCESS_RW);
		if (!vva)
			vhost_user_iotlb_miss(dev, ra, VHOST_ACCESS_RW);

		return vva;
	}

	return qva_to_vva(dev, ra);
}

static struct virtio_net *
translate_ring_addresses(struct virtio_net *dev, int vq_index)
{
	struct vhost_virtqueue *vq = dev->virtqueue[vq_index];
	struct vhost_vring_addr *addr = &vq->ring_addrs;

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	if (vq->desc && vq->avail && vq->used)
		return dev;

	vq->desc = (struct vring_desc *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->desc_user_addr, sizeof(struct vring_desc));
	if (vq->desc == 0) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to find desc ring address.\n",
			dev->vid);
		return dev;
	}

	dev = numa_realloc(dev, vq_index);
	vq = dev->virtqueue[vq_index];
	addr = &vq->ring_addrs;

	vq->avail = (struct vring_avail *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->avail_user_addr, sizeof(struct vring_avail));
	if (vq->avail == 0) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to find avail ring address.\n",
			dev->vid);
		return dev;
	}

	vq->used = (struct vring_used *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->used_user_addr, sizeof(struct vring_used));
	if (vq->used == 0) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to find used ring address.\n",
			dev->vid);
		return dev;
	}

	if (vq->last_used_idx != vq->used->idx) {
		RTE_LOG(WARNING, VHOST_CONFIG,
			"last_used_idx (%u) and vq->used->idx (%u) mismatches; "
			"some packets maybe resent for Tx and dropped for Rx\n",
			vq->last_used_idx, vq->used->idx);
		vq->last_used_idx  = vq->used->idx;
		vq->last_avail_idx = vq->used->idx;
	}

	vq->log_guest_addr = addr->log_guest_addr;

	VHOST_LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address desc: %p\n",
			dev->vid, vq->desc);
	VHOST_LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address avail: %p\n",
			dev->vid, vq->avail);
	VHOST_LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address used: %p\n",
			dev->vid, vq->used);
	VHOST_LOG_DEBUG(VHOST_CONFIG, "(%d) log_guest_addr: %" PRIx64 "\n",
			dev->vid, vq->log_guest_addr);

	return dev;
}

/*
 * The virtio device sends us the desc, used and avail ring addresses.
 * This function then converts these to our address space.
 */
static int
vhost_user_set_vring_addr(struct virtio_net **pdev, VhostUserMsg *msg)
{
	struct vhost_virtqueue *vq;
	struct vhost_vring_addr *addr = &msg->payload.addr;
	struct virtio_net *dev = *pdev;

	if (dev->mem == NULL)
		return -1;

	/* addr->index refers to the queue index. The txq 1, rxq is 0. */
	vq = dev->virtqueue[msg->payload.addr.index];

	/*
	 * Rings addresses should not be interpreted as long as the ring is not
	 * started and enabled
	 */
	memcpy(&vq->ring_addrs, addr, sizeof(*addr));

	vring_invalidate(dev, vq);

	if (vq->enabled && (dev->features &
				(1ULL << VHOST_USER_F_PROTOCOL_FEATURES))) {
		dev = translate_ring_addresses(dev, msg->payload.addr.index);
		if (!dev)
			return -1;

		*pdev = dev;
	}

	return 0;
}

/*
 * The virtio device sends us the available ring last used index.
 */
static int
vhost_user_set_vring_base(struct virtio_net *dev,
			  VhostUserMsg *msg)
{
	dev->virtqueue[msg->payload.state.index]->last_used_idx  =
			msg->payload.state.num;
	dev->virtqueue[msg->payload.state.index]->last_avail_idx =
			msg->payload.state.num;

	return 0;
}

static int
add_one_guest_page(struct virtio_net *dev, uint64_t guest_phys_addr,
		   uint64_t host_phys_addr, uint64_t size)
{
	struct guest_page *page, *last_page;

	if (dev->nr_guest_pages == dev->max_guest_pages) {
		dev->max_guest_pages *= 2;
		dev->guest_pages = realloc(dev->guest_pages,
					dev->max_guest_pages * sizeof(*page));
		if (!dev->guest_pages) {
			RTE_LOG(ERR, VHOST_CONFIG, "cannot realloc guest_pages\n");
			return -1;
		}
	}

	if (dev->nr_guest_pages > 0) {
		last_page = &dev->guest_pages[dev->nr_guest_pages - 1];
		/* merge if the two pages are continuous */
		if (host_phys_addr == last_page->host_phys_addr +
				      last_page->size) {
			last_page->size += size;
			return 0;
		}
	}

	page = &dev->guest_pages[dev->nr_guest_pages++];
	page->guest_phys_addr = guest_phys_addr;
	page->host_phys_addr  = host_phys_addr;
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
	uint64_t host_phys_addr;
	uint64_t size;

	host_phys_addr = rte_mem_virt2iova((void *)(uintptr_t)host_user_addr);
	size = page_size - (guest_phys_addr & (page_size - 1));
	size = RTE_MIN(size, reg_size);

	if (add_one_guest_page(dev, guest_phys_addr, host_phys_addr, size) < 0)
		return -1;

	host_user_addr  += size;
	guest_phys_addr += size;
	reg_size -= size;

	while (reg_size > 0) {
		size = RTE_MIN(reg_size, page_size);
		host_phys_addr = rte_mem_virt2iova((void *)(uintptr_t)
						  host_user_addr);
		if (add_one_guest_page(dev, guest_phys_addr, host_phys_addr,
				size) < 0)
			return -1;

		host_user_addr  += size;
		guest_phys_addr += size;
		reg_size -= size;
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

		RTE_LOG(INFO, VHOST_CONFIG,
			"guest physical page region %u\n"
			"\t guest_phys_addr: %" PRIx64 "\n"
			"\t host_phys_addr : %" PRIx64 "\n"
			"\t size           : %" PRIx64 "\n",
			i,
			page->guest_phys_addr,
			page->host_phys_addr,
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

static int
vhost_user_set_mem_table(struct virtio_net *dev, struct VhostUserMsg *pmsg)
{
	struct VhostUserMemory memory = pmsg->payload.memory;
	struct rte_vhost_mem_region *reg;
	void *mmap_addr;
	uint64_t mmap_size;
	uint64_t mmap_offset;
	uint64_t alignment;
	uint32_t i;
	int populate;
	int fd;

	if (memory.nregions > VHOST_MEMORY_MAX_NREGIONS) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"too many memory regions (%u)\n", memory.nregions);
		return -1;
	}

	if (dev->mem && !vhost_memory_changed(&memory, dev->mem)) {
		RTE_LOG(INFO, VHOST_CONFIG,
			"(%d) memory regions not changed\n", dev->vid);

		for (i = 0; i < memory.nregions; i++)
			close(pmsg->fds[i]);

		return 0;
	}

	if (dev->mem) {
		free_mem_region(dev);
		rte_free(dev->mem);
		dev->mem = NULL;
	}

	dev->nr_guest_pages = 0;
	if (!dev->guest_pages) {
		dev->max_guest_pages = 8;
		dev->guest_pages = malloc(dev->max_guest_pages *
						sizeof(struct guest_page));
		if (dev->guest_pages == NULL) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"(%d) failed to allocate memory "
				"for dev->guest_pages\n",
				dev->vid);
			return -1;
		}
	}

	dev->mem = rte_zmalloc("vhost-mem-table", sizeof(struct rte_vhost_memory) +
		sizeof(struct rte_vhost_mem_region) * memory.nregions, 0);
	if (dev->mem == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to allocate memory for dev->mem\n",
			dev->vid);
		return -1;
	}
	dev->mem->nregions = memory.nregions;

	for (i = 0; i < memory.nregions; i++) {
		fd  = pmsg->fds[i];
		reg = &dev->mem->regions[i];

		reg->guest_phys_addr = memory.regions[i].guest_phys_addr;
		reg->guest_user_addr = memory.regions[i].userspace_addr;
		reg->size            = memory.regions[i].memory_size;
		reg->fd              = fd;

		mmap_offset = memory.regions[i].mmap_offset;

		/* Check for memory_size + mmap_offset overflow */
		if (mmap_offset >= -reg->size) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"mmap_offset (%#"PRIx64") and memory_size "
				"(%#"PRIx64") overflow\n",
				mmap_offset, reg->size);
			goto err_mmap;
		}

		mmap_size = reg->size + mmap_offset;

		/* mmap() without flag of MAP_ANONYMOUS, should be called
		 * with length argument aligned with hugepagesz at older
		 * longterm version Linux, like 2.6.32 and 3.2.72, or
		 * mmap() will fail with EINVAL.
		 *
		 * to avoid failure, make sure in caller to keep length
		 * aligned.
		 */
		alignment = get_blk_size(fd);
		if (alignment == (uint64_t)-1) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"couldn't get hugepage size through fstat\n");
			goto err_mmap;
		}
		mmap_size = RTE_ALIGN_CEIL(mmap_size, alignment);

		populate = (dev->dequeue_zero_copy) ? MAP_POPULATE : 0;
		mmap_addr = mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
				 MAP_SHARED | populate, fd, 0);

		if (mmap_addr == MAP_FAILED) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"mmap region %u failed.\n", i);
			goto err_mmap;
		}

		reg->mmap_addr = mmap_addr;
		reg->mmap_size = mmap_size;
		reg->host_user_addr = (uint64_t)(uintptr_t)mmap_addr +
				      mmap_offset;

		if (dev->dequeue_zero_copy)
			if (add_guest_pages(dev, reg, alignment) < 0) {
				RTE_LOG(ERR, VHOST_CONFIG,
					"adding guest pages to region %u failed.\n",
					i);
				goto err_mmap;
			}

		RTE_LOG(INFO, VHOST_CONFIG,
			"guest memory region %u, size: 0x%" PRIx64 "\n"
			"\t guest physical addr: 0x%" PRIx64 "\n"
			"\t guest virtual  addr: 0x%" PRIx64 "\n"
			"\t host  virtual  addr: 0x%" PRIx64 "\n"
			"\t mmap addr : 0x%" PRIx64 "\n"
			"\t mmap size : 0x%" PRIx64 "\n"
			"\t mmap align: 0x%" PRIx64 "\n"
			"\t mmap off  : 0x%" PRIx64 "\n",
			i, reg->size,
			reg->guest_phys_addr,
			reg->guest_user_addr,
			reg->host_user_addr,
			(uint64_t)(uintptr_t)mmap_addr,
			mmap_size,
			alignment,
			mmap_offset);
	}

	dump_guest_pages(dev);

	return 0;

err_mmap:
	free_mem_region(dev);
	rte_free(dev->mem);
	dev->mem = NULL;
	return -1;
}

static int
vq_is_ready(struct vhost_virtqueue *vq)
{
	return vq && vq->desc && vq->avail && vq->used &&
	       vq->kickfd != VIRTIO_UNINITIALIZED_EVENTFD &&
	       vq->callfd != VIRTIO_UNINITIALIZED_EVENTFD;
}

static int
virtio_is_ready(struct virtio_net *dev)
{
	struct vhost_virtqueue *vq;
	uint32_t i;

	if (dev->nr_vring == 0)
		return 0;

	for (i = 0; i < dev->nr_vring; i++) {
		vq = dev->virtqueue[i];

		if (!vq_is_ready(vq))
			return 0;
	}

	RTE_LOG(INFO, VHOST_CONFIG,
		"virtio is now ready for processing.\n");
	return 1;
}

static void
vhost_user_set_vring_call(struct virtio_net *dev, struct VhostUserMsg *pmsg)
{
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;

	file.index = pmsg->payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (pmsg->payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = pmsg->fds[0];
	RTE_LOG(INFO, VHOST_CONFIG,
		"vring call idx:%d file:%d\n", file.index, file.fd);

	vq = dev->virtqueue[file.index];
	if (vq->callfd >= 0)
		close(vq->callfd);

	vq->callfd = file.fd;
}

static void
vhost_user_set_vring_kick(struct virtio_net **pdev, struct VhostUserMsg *pmsg)
{
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;
	struct virtio_net *dev = *pdev;

	file.index = pmsg->payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (pmsg->payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = pmsg->fds[0];
	RTE_LOG(INFO, VHOST_CONFIG,
		"vring kick idx:%d file:%d\n", file.index, file.fd);

	/* Interpret ring addresses only when ring is started. */
	dev = translate_ring_addresses(dev, file.index);
	if (!dev)
		return;

	*pdev = dev;

	vq = dev->virtqueue[file.index];

	/*
	 * When VHOST_USER_F_PROTOCOL_FEATURES is not negotiated,
	 * the ring starts already enabled. Otherwise, it is enabled via
	 * the SET_VRING_ENABLE message.
	 */
	if (!(dev->features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)))
		vq->enabled = 1;

	if (vq->kickfd >= 0)
		close(vq->kickfd);
	vq->kickfd = file.fd;
}

static void
free_zmbufs(struct vhost_virtqueue *vq)
{
	struct zcopy_mbuf *zmbuf, *next;

	for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
	     zmbuf != NULL; zmbuf = next) {
		next = TAILQ_NEXT(zmbuf, next);

		rte_pktmbuf_free(zmbuf->mbuf);
		TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
	}

	rte_free(vq->zmbufs);
}

/*
 * when virtio is stopped, qemu will send us the GET_VRING_BASE message.
 */
static int
vhost_user_get_vring_base(struct virtio_net *dev,
			  VhostUserMsg *msg)
{
	struct vhost_virtqueue *vq = dev->virtqueue[msg->payload.state.index];
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	/* We have to stop the queue (virtio) if it is running. */
	if (dev->flags & VIRTIO_DEV_RUNNING) {
		did = dev->vdpa_dev_id;
		vdpa_dev = rte_vdpa_get_device(did);
		if (vdpa_dev && vdpa_dev->ops->dev_close)
			vdpa_dev->ops->dev_close(dev->vid);
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		dev->notify_ops->destroy_device(dev->vid);
	}

	dev->flags &= ~VIRTIO_DEV_READY;
	dev->flags &= ~VIRTIO_DEV_VDPA_CONFIGURED;

	/* Here we are safe to get the last avail index */
	msg->payload.state.num = vq->last_avail_idx;

	RTE_LOG(INFO, VHOST_CONFIG,
		"vring base idx:%d file:%d\n", msg->payload.state.index,
		msg->payload.state.num);
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

	if (dev->dequeue_zero_copy)
		free_zmbufs(vq);
	rte_free(vq->shadow_used_ring);
	vq->shadow_used_ring = NULL;

	rte_free(vq->batch_copy_elems);
	vq->batch_copy_elems = NULL;

	return 0;
}

/*
 * when virtio queues are ready to work, qemu will send us to
 * enable the virtio queue pair.
 */
static int
vhost_user_set_vring_enable(struct virtio_net *dev,
			    VhostUserMsg *msg)
{
	int enable = (int)msg->payload.state.num;
	int index = (int)msg->payload.state.index;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	RTE_LOG(INFO, VHOST_CONFIG,
		"set queue enable: %d to qp idx: %d\n",
		enable, index);

	did = dev->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (vdpa_dev && vdpa_dev->ops->set_vring_state)
		vdpa_dev->ops->set_vring_state(dev->vid, index, enable);

	if (dev->notify_ops->vring_state_changed)
		dev->notify_ops->vring_state_changed(dev->vid,
				index, enable);

	dev->virtqueue[index]->enabled = enable;

	return 0;
}

static void
vhost_user_get_protocol_features(struct virtio_net *dev,
				 struct VhostUserMsg *msg)
{
	uint64_t features, protocol_features;

	rte_vhost_driver_get_features(dev->ifname, &features);
	rte_vhost_driver_get_protocol_features(dev->ifname, &protocol_features);

	/*
	 * REPLY_ACK protocol feature is only mandatory for now
	 * for IOMMU feature. If IOMMU is explicitly disabled by the
	 * application, disable also REPLY_ACK feature for older buggy
	 * Qemu versions (from v2.7.0 to v2.9.0).
	 */
	if (!(features & (1ULL << VIRTIO_F_IOMMU_PLATFORM)))
		protocol_features &= ~(1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK);

	msg->payload.u64 = protocol_features;
	msg->size = sizeof(msg->payload.u64);
}

static void
vhost_user_set_protocol_features(struct virtio_net *dev,
				 uint64_t protocol_features)
{
	if (protocol_features & ~VHOST_USER_PROTOCOL_FEATURES)
		return;

	dev->protocol_features = protocol_features;
}

static int
vhost_user_set_log_base(struct virtio_net *dev, struct VhostUserMsg *msg)
{
	int fd = msg->fds[0];
	uint64_t size, off;
	void *addr;

	if (fd < 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "invalid log fd: %d\n", fd);
		return -1;
	}

	if (msg->size != sizeof(VhostUserLog)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"invalid log base msg size: %"PRId32" != %d\n",
			msg->size, (int)sizeof(VhostUserLog));
		return -1;
	}

	size = msg->payload.log.mmap_size;
	off  = msg->payload.log.mmap_offset;

	/* Don't allow mmap_offset to point outside the mmap region */
	if (off > size) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"log offset %#"PRIx64" exceeds log size %#"PRIx64"\n",
			off, size);
		return -1;
	}

	RTE_LOG(INFO, VHOST_CONFIG,
		"log mmap size: %"PRId64", offset: %"PRId64"\n",
		size, off);

	/*
	 * mmap from 0 to workaround a hugepage mmap bug: mmap will
	 * fail when offset is not page size aligned.
	 */
	addr = mmap(0, size + off, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) {
		RTE_LOG(ERR, VHOST_CONFIG, "mmap log base failed!\n");
		return -1;
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

	return 0;
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
vhost_user_send_rarp(struct virtio_net *dev, struct VhostUserMsg *msg)
{
	uint8_t *mac = (uint8_t *)&msg->payload.u64;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	RTE_LOG(DEBUG, VHOST_CONFIG,
		":: mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	memcpy(dev->mac.addr_bytes, mac, 6);

	/*
	 * Set the flag to inject a RARP broadcast packet at
	 * rte_vhost_dequeue_burst().
	 *
	 * rte_smp_wmb() is for making sure the mac is copied
	 * before the flag is set.
	 */
	rte_smp_wmb();
	rte_atomic16_set(&dev->broadcast_rarp, 1);
	did = dev->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (vdpa_dev && vdpa_dev->ops->migration_done)
		vdpa_dev->ops->migration_done(dev->vid);

	return 0;
}

static int
vhost_user_net_set_mtu(struct virtio_net *dev, struct VhostUserMsg *msg)
{
	if (msg->payload.u64 < VIRTIO_MIN_MTU ||
			msg->payload.u64 > VIRTIO_MAX_MTU) {
		RTE_LOG(ERR, VHOST_CONFIG, "Invalid MTU size (%"PRIu64")\n",
				msg->payload.u64);

		return -1;
	}

	dev->mtu = msg->payload.u64;

	return 0;
}

static int
vhost_user_set_req_fd(struct virtio_net *dev, struct VhostUserMsg *msg)
{
	int fd = msg->fds[0];

	if (fd < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Invalid file descriptor for slave channel (%d)\n",
				fd);
		return -1;
	}

	dev->slave_req_fd = fd;

	return 0;
}

static int
is_vring_iotlb_update(struct vhost_virtqueue *vq, struct vhost_iotlb_msg *imsg)
{
	struct vhost_vring_addr *ra;
	uint64_t start, end;

	start = imsg->iova;
	end = start + imsg->size;

	ra = &vq->ring_addrs;
	if (ra->desc_user_addr >= start && ra->desc_user_addr < end)
		return 1;
	if (ra->avail_user_addr >= start && ra->avail_user_addr < end)
		return 1;
	if (ra->used_user_addr >= start && ra->used_user_addr < end)
		return 1;

	return 0;
}

static int
is_vring_iotlb_invalidate(struct vhost_virtqueue *vq,
				struct vhost_iotlb_msg *imsg)
{
	uint64_t istart, iend, vstart, vend;

	istart = imsg->iova;
	iend = istart + imsg->size - 1;

	vstart = (uintptr_t)vq->desc;
	vend = vstart + sizeof(struct vring_desc) * vq->size - 1;
	if (vstart <= iend && istart <= vend)
		return 1;

	vstart = (uintptr_t)vq->avail;
	vend = vstart + sizeof(struct vring_avail);
	vend += sizeof(uint16_t) * vq->size - 1;
	if (vstart <= iend && istart <= vend)
		return 1;

	vstart = (uintptr_t)vq->used;
	vend = vstart + sizeof(struct vring_used);
	vend += sizeof(struct vring_used_elem) * vq->size - 1;
	if (vstart <= iend && istart <= vend)
		return 1;

	return 0;
}

static int
vhost_user_iotlb_msg(struct virtio_net **pdev, struct VhostUserMsg *msg)
{
	struct virtio_net *dev = *pdev;
	struct vhost_iotlb_msg *imsg = &msg->payload.iotlb;
	uint16_t i;
	uint64_t vva;

	switch (imsg->type) {
	case VHOST_IOTLB_UPDATE:
		vva = qva_to_vva(dev, imsg->uaddr);
		if (!vva)
			return -1;

		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];

			vhost_user_iotlb_cache_insert(vq, imsg->iova, vva,
					imsg->size, imsg->perm);

			if (is_vring_iotlb_update(vq, imsg))
				*pdev = dev = translate_ring_addresses(dev, i);
		}
		break;
	case VHOST_IOTLB_INVALIDATE:
		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];

			vhost_user_iotlb_cache_remove(vq, imsg->iova,
					imsg->size);

			if (is_vring_iotlb_invalidate(vq, imsg))
				vring_invalidate(dev, vq);
		}
		break;
	default:
		RTE_LOG(ERR, VHOST_CONFIG, "Invalid IOTLB message type (%d)\n",
				imsg->type);
		return -1;
	}

	return 0;
}

/* return bytes# of read on success or negative val on failure. */
static int
read_vhost_message(int sockfd, struct VhostUserMsg *msg)
{
	int ret;

	ret = read_fd_message(sockfd, (char *)msg, VHOST_USER_HDR_SIZE,
		msg->fds, VHOST_MEMORY_MAX_NREGIONS);
	if (ret <= 0)
		return ret;

	if (msg && msg->size) {
		if (msg->size > sizeof(msg->payload)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"invalid msg size: %d\n", msg->size);
			return -1;
		}
		ret = read(sockfd, &msg->payload, msg->size);
		if (ret <= 0)
			return ret;
		if (ret != (int)msg->size) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"read control message failed\n");
			return -1;
		}
	}

	return ret;
}

static int
send_vhost_message(int sockfd, struct VhostUserMsg *msg, int *fds, int fd_num)
{
	if (!msg)
		return 0;

	return send_fd_message(sockfd, (char *)msg,
		VHOST_USER_HDR_SIZE + msg->size, fds, fd_num);
}

static int
send_vhost_reply(int sockfd, struct VhostUserMsg *msg)
{
	if (!msg)
		return 0;

	msg->flags &= ~VHOST_USER_VERSION_MASK;
	msg->flags &= ~VHOST_USER_NEED_REPLY;
	msg->flags |= VHOST_USER_VERSION;
	msg->flags |= VHOST_USER_REPLY_MASK;

	return send_vhost_message(sockfd, msg, NULL, 0);
}

/*
 * Allocate a queue pair if it hasn't been allocated yet
 */
static int
vhost_user_check_and_alloc_queue_pair(struct virtio_net *dev, VhostUserMsg *msg)
{
	uint16_t vring_idx;

	switch (msg->request.master) {
	case VHOST_USER_SET_VRING_KICK:
	case VHOST_USER_SET_VRING_CALL:
	case VHOST_USER_SET_VRING_ERR:
		vring_idx = msg->payload.u64 & VHOST_USER_VRING_IDX_MASK;
		break;
	case VHOST_USER_SET_VRING_NUM:
	case VHOST_USER_SET_VRING_BASE:
	case VHOST_USER_SET_VRING_ENABLE:
		vring_idx = msg->payload.state.index;
		break;
	case VHOST_USER_SET_VRING_ADDR:
		vring_idx = msg->payload.addr.index;
		break;
	default:
		return 0;
	}

	if (vring_idx >= VHOST_MAX_VRING) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"invalid vring index: %u\n", vring_idx);
		return -1;
	}

	if (dev->virtqueue[vring_idx])
		return 0;

	return alloc_vring_queue(dev, vring_idx);
}

static void
vhost_user_lock_all_queue_pairs(struct virtio_net *dev)
{
	unsigned int i = 0;
	unsigned int vq_num = 0;

	while (vq_num < dev->nr_vring) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq) {
			rte_spinlock_lock(&vq->access_lock);
			vq_num++;
		}
		i++;
	}
}

static void
vhost_user_unlock_all_queue_pairs(struct virtio_net *dev)
{
	unsigned int i = 0;
	unsigned int vq_num = 0;

	while (vq_num < dev->nr_vring) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq) {
			rte_spinlock_unlock(&vq->access_lock);
			vq_num++;
		}
		i++;
	}
}

int
vhost_user_msg_handler(int vid, int fd)
{
	struct virtio_net *dev;
	struct VhostUserMsg msg;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;
	int ret;
	int unlock_required = 0;
	uint32_t skip_master = 0;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	if (!dev->notify_ops) {
		dev->notify_ops = vhost_driver_callback_get(dev->ifname);
		if (!dev->notify_ops) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"failed to get callback ops for driver %s\n",
				dev->ifname);
			return -1;
		}
	}

	ret = read_vhost_message(fd, &msg);
	if (ret <= 0 || msg.request.master >= VHOST_USER_MAX) {
		if (ret < 0)
			RTE_LOG(ERR, VHOST_CONFIG,
				"vhost read message failed\n");
		else if (ret == 0)
			RTE_LOG(INFO, VHOST_CONFIG,
				"vhost peer closed\n");
		else
			RTE_LOG(ERR, VHOST_CONFIG,
				"vhost read incorrect message\n");

		return -1;
	}

	ret = 0;
	if (msg.request.master != VHOST_USER_IOTLB_MSG)
		RTE_LOG(INFO, VHOST_CONFIG, "read message %s\n",
			vhost_message_str[msg.request.master]);
	else
		RTE_LOG(DEBUG, VHOST_CONFIG, "read message %s\n",
			vhost_message_str[msg.request.master]);

	ret = vhost_user_check_and_alloc_queue_pair(dev, &msg);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to alloc queue\n");
		return -1;
	}

	/*
	 * Note: we don't lock all queues on VHOST_USER_GET_VRING_BASE
	 * and VHOST_USER_RESET_OWNER, since it is sent when virtio stops
	 * and device is destroyed. destroy_device waits for queues to be
	 * inactive, so it is safe. Otherwise taking the access_lock
	 * would cause a dead lock.
	 */
	switch (msg.request.master) {
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
	case VHOST_USER_SET_SLAVE_REQ_FD:
		vhost_user_lock_all_queue_pairs(dev);
		unlock_required = 1;
		break;
	default:
		break;

	}

	if (dev->extern_ops.pre_msg_handle) {
		uint32_t need_reply;

		ret = (*dev->extern_ops.pre_msg_handle)(dev->vid,
				(void *)&msg, &need_reply, &skip_master);
		if (ret < 0)
			goto skip_to_reply;

		if (need_reply)
			send_vhost_reply(fd, &msg);

		if (skip_master)
			goto skip_to_post_handle;
	}

	switch (msg.request.master) {
	case VHOST_USER_GET_FEATURES:
		msg.payload.u64 = vhost_user_get_features(dev);
		msg.size = sizeof(msg.payload.u64);
		send_vhost_reply(fd, &msg);
		break;
	case VHOST_USER_SET_FEATURES:
		ret = vhost_user_set_features(dev, msg.payload.u64);
		if (ret)
			return -1;
		break;

	case VHOST_USER_GET_PROTOCOL_FEATURES:
		vhost_user_get_protocol_features(dev, &msg);
		send_vhost_reply(fd, &msg);
		break;
	case VHOST_USER_SET_PROTOCOL_FEATURES:
		vhost_user_set_protocol_features(dev, msg.payload.u64);
		break;

	case VHOST_USER_SET_OWNER:
		vhost_user_set_owner();
		break;
	case VHOST_USER_RESET_OWNER:
		vhost_user_reset_owner(dev);
		break;

	case VHOST_USER_SET_MEM_TABLE:
		ret = vhost_user_set_mem_table(dev, &msg);
		break;

	case VHOST_USER_SET_LOG_BASE:
		vhost_user_set_log_base(dev, &msg);

		/* it needs a reply */
		msg.size = sizeof(msg.payload.u64);
		send_vhost_reply(fd, &msg);
		break;
	case VHOST_USER_SET_LOG_FD:
		close(msg.fds[0]);
		RTE_LOG(INFO, VHOST_CONFIG, "not implemented.\n");
		break;

	case VHOST_USER_SET_VRING_NUM:
		vhost_user_set_vring_num(dev, &msg);
		break;
	case VHOST_USER_SET_VRING_ADDR:
		vhost_user_set_vring_addr(&dev, &msg);
		break;
	case VHOST_USER_SET_VRING_BASE:
		vhost_user_set_vring_base(dev, &msg);
		break;

	case VHOST_USER_GET_VRING_BASE:
		vhost_user_get_vring_base(dev, &msg);
		msg.size = sizeof(msg.payload.state);
		send_vhost_reply(fd, &msg);
		break;

	case VHOST_USER_SET_VRING_KICK:
		vhost_user_set_vring_kick(&dev, &msg);
		break;
	case VHOST_USER_SET_VRING_CALL:
		vhost_user_set_vring_call(dev, &msg);
		break;

	case VHOST_USER_SET_VRING_ERR:
		if (!(msg.payload.u64 & VHOST_USER_VRING_NOFD_MASK))
			close(msg.fds[0]);
		RTE_LOG(INFO, VHOST_CONFIG, "not implemented\n");
		break;

	case VHOST_USER_GET_QUEUE_NUM:
		msg.payload.u64 = (uint64_t)vhost_user_get_queue_num(dev);
		msg.size = sizeof(msg.payload.u64);
		send_vhost_reply(fd, &msg);
		break;

	case VHOST_USER_SET_VRING_ENABLE:
		vhost_user_set_vring_enable(dev, &msg);
		break;
	case VHOST_USER_SEND_RARP:
		vhost_user_send_rarp(dev, &msg);
		break;

	case VHOST_USER_NET_SET_MTU:
		ret = vhost_user_net_set_mtu(dev, &msg);
		break;

	case VHOST_USER_SET_SLAVE_REQ_FD:
		ret = vhost_user_set_req_fd(dev, &msg);
		break;

	case VHOST_USER_IOTLB_MSG:
		ret = vhost_user_iotlb_msg(&dev, &msg);
		break;

	default:
		ret = -1;
		break;
	}

skip_to_post_handle:
	if (dev->extern_ops.post_msg_handle) {
		uint32_t need_reply;

		ret = (*dev->extern_ops.post_msg_handle)(
				dev->vid, (void *)&msg, &need_reply);
		if (ret < 0)
			goto skip_to_reply;

		if (need_reply)
			send_vhost_reply(fd, &msg);
	}

skip_to_reply:
	if (unlock_required)
		vhost_user_unlock_all_queue_pairs(dev);

	if (msg.flags & VHOST_USER_NEED_REPLY) {
		msg.payload.u64 = !!ret;
		msg.size = sizeof(msg.payload.u64);
		send_vhost_reply(fd, &msg);
	}

	if (!(dev->flags & VIRTIO_DEV_RUNNING) && virtio_is_ready(dev)) {
		dev->flags |= VIRTIO_DEV_READY;

		if (!(dev->flags & VIRTIO_DEV_RUNNING)) {
			if (dev->dequeue_zero_copy) {
				RTE_LOG(INFO, VHOST_CONFIG,
						"dequeue zero copy is enabled\n");
			}

			if (dev->notify_ops->new_device(dev->vid) == 0)
				dev->flags |= VIRTIO_DEV_RUNNING;
		}
	}

	did = dev->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (vdpa_dev && virtio_is_ready(dev) &&
			!(dev->flags & VIRTIO_DEV_VDPA_CONFIGURED) &&
			msg.request.master == VHOST_USER_SET_VRING_ENABLE) {
		if (vdpa_dev->ops->dev_conf)
			vdpa_dev->ops->dev_conf(dev->vid);
		dev->flags |= VIRTIO_DEV_VDPA_CONFIGURED;
	}

	return 0;
}

int
vhost_user_iotlb_miss(struct virtio_net *dev, uint64_t iova, uint8_t perm)
{
	int ret;
	struct VhostUserMsg msg = {
		.request.slave = VHOST_USER_SLAVE_IOTLB_MSG,
		.flags = VHOST_USER_VERSION,
		.size = sizeof(msg.payload.iotlb),
		.payload.iotlb = {
			.iova = iova,
			.perm = perm,
			.type = VHOST_IOTLB_MISS,
		},
	};

	ret = send_vhost_message(dev->slave_req_fd, &msg, NULL, 0);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to send IOTLB miss message (%d)\n",
				ret);
		return ret;
	}

	return 0;
}
