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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <assert.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif
#ifdef RTE_LIBRTE_VHOST_POSTCOPY
#include <linux/userfaultfd.h>
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
	[VHOST_USER_CRYPTO_CREATE_SESS] = "VHOST_USER_CRYPTO_CREATE_SESS",
	[VHOST_USER_CRYPTO_CLOSE_SESS] = "VHOST_USER_CRYPTO_CLOSE_SESS",
	[VHOST_USER_POSTCOPY_ADVISE]  = "VHOST_USER_POSTCOPY_ADVISE",
	[VHOST_USER_POSTCOPY_LISTEN]  = "VHOST_USER_POSTCOPY_LISTEN",
	[VHOST_USER_POSTCOPY_END]  = "VHOST_USER_POSTCOPY_END",
};

static int send_vhost_reply(int sockfd, struct VhostUserMsg *msg);
static int read_vhost_message(int sockfd, struct VhostUserMsg *msg);

static uint64_t
get_blk_size(int fd)
{
	struct stat stat;
	int ret;

	ret = fstat(fd, &stat);
	return ret == -1 ? (uint64_t)-1 : (uint64_t)stat.st_blksize;
}

/*
 * Reclaim all the outstanding zmbufs for a virtqueue.
 */
static void
drain_zmbuf_list(struct vhost_virtqueue *vq)
{
	struct zcopy_mbuf *zmbuf, *next;

	for (zmbuf = TAILQ_FIRST(&vq->zmbuf_list);
	     zmbuf != NULL; zmbuf = next) {
		next = TAILQ_NEXT(zmbuf, next);

		while (!mbuf_is_consumed(zmbuf->mbuf))
			usleep(1000);

		TAILQ_REMOVE(&vq->zmbuf_list, zmbuf, next);
		restore_mbuf(zmbuf->mbuf);
		rte_pktmbuf_free(zmbuf->mbuf);
		put_zmbuf(zmbuf);
		vq->nr_zmbuf -= 1;
	}
}

static void
free_mem_region(struct virtio_net *dev)
{
	uint32_t i;
	struct rte_vhost_mem_region *reg;
	struct vhost_virtqueue *vq;

	if (!dev || !dev->mem)
		return;

	if (dev->dequeue_zero_copy) {
		for (i = 0; i < dev->nr_vring; i++) {
			vq = dev->virtqueue[i];
			if (vq)
				drain_zmbuf_list(vq);
		}
	}

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

	if (dev->postcopy_ufd >= 0) {
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
	}

	dev->postcopy_listening = 0;
}

/*
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
static int
vhost_user_set_owner(struct virtio_net **pdev __rte_unused,
			struct VhostUserMsg *msg __rte_unused,
			int main_fd __rte_unused)
{
	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_reset_owner(struct virtio_net **pdev,
			struct VhostUserMsg *msg __rte_unused,
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
vhost_user_get_features(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t features = 0;

	rte_vhost_driver_get_features(dev->ifname, &features);

	msg->payload.u64 = features;
	msg->size = sizeof(msg->payload.u64);
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * The queue number that we support are requested.
 */
static int
vhost_user_get_queue_num(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint32_t queue_num = 0;

	rte_vhost_driver_get_queue_num(dev->ifname, &queue_num);

	msg->payload.u64 = (uint64_t)queue_num;
	msg->size = sizeof(msg->payload.u64);
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * We receive the negotiated features supported by us and the virtio device.
 */
static int
vhost_user_set_features(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t features = msg->payload.u64;
	uint64_t vhost_features = 0;
	struct rte_vdpa_device *vdpa_dev;
	int did = -1;

	rte_vhost_driver_get_features(dev->ifname, &vhost_features);
	if (features & ~vhost_features) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) received invalid negotiated features.\n",
			dev->vid);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		if (dev->features == features)
			return RTE_VHOST_MSG_RESULT_OK;

		/*
		 * Error out if master tries to change features while device is
		 * in running state. The exception being VHOST_F_LOG_ALL, which
		 * is enabled when the live-migration starts.
		 */
		if ((dev->features ^ features) & ~(1ULL << VHOST_F_LOG_ALL)) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"(%d) features changed while device is running.\n",
				dev->vid);
			return RTE_VHOST_MSG_RESULT_ERR;
		}

		if (dev->notify_ops->features_changed)
			dev->notify_ops->features_changed(dev->vid, features);
	}

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
			free_vq(dev, vq);
		}
	}

	did = dev->vdpa_dev_id;
	vdpa_dev = rte_vdpa_get_device(did);
	if (vdpa_dev && vdpa_dev->ops->set_features)
		vdpa_dev->ops->set_features(dev->vid);

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * The virtio device sends us the size of the descriptor ring.
 */
static int
vhost_user_set_vring_num(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
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
		return RTE_VHOST_MSG_RESULT_ERR;
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

	if (vq_is_packed(dev)) {
		vq->shadow_used_packed = rte_malloc(NULL,
				vq->size *
				sizeof(struct vring_used_elem_packed),
				RTE_CACHE_LINE_SIZE);
		if (!vq->shadow_used_packed) {
			RTE_LOG(ERR, VHOST_CONFIG,
					"failed to allocate memory for shadow used ring.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}

	} else {
		vq->shadow_used_split = rte_malloc(NULL,
				vq->size * sizeof(struct vring_used_elem),
				RTE_CACHE_LINE_SIZE);
		if (!vq->shadow_used_split) {
			RTE_LOG(ERR, VHOST_CONFIG,
					"failed to allocate memory for shadow used ring.\n");
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	vq->batch_copy_elems = rte_malloc(NULL,
				vq->size * sizeof(struct batch_copy_elem),
				RTE_CACHE_LINE_SIZE);
	if (!vq->batch_copy_elems) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"failed to allocate memory for batching copy.\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	return RTE_VHOST_MSG_RESULT_OK;
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
	struct vring_used_elem *new_shadow_used_split;
	struct vring_used_elem_packed *new_shadow_used_packed;
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

		if (dev->dequeue_zero_copy) {
			new_zmbuf = rte_malloc_socket(NULL, vq->zmbuf_size *
					sizeof(struct zcopy_mbuf), 0, newnode);
			if (new_zmbuf) {
				rte_free(vq->zmbufs);
				vq->zmbufs = new_zmbuf;
			}
		}

		if (vq_is_packed(dev)) {
			new_shadow_used_packed = rte_malloc_socket(NULL,
					vq->size *
					sizeof(struct vring_used_elem_packed),
					RTE_CACHE_LINE_SIZE,
					newnode);
			if (new_shadow_used_packed) {
				rte_free(vq->shadow_used_packed);
				vq->shadow_used_packed = new_shadow_used_packed;
			}
		} else {
			new_shadow_used_split = rte_malloc_socket(NULL,
					vq->size *
					sizeof(struct vring_used_elem),
					RTE_CACHE_LINE_SIZE,
					newnode);
			if (new_shadow_used_split) {
				rte_free(vq->shadow_used_split);
				vq->shadow_used_split = new_shadow_used_split;
			}
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

		vva = vhost_user_iotlb_cache_find(vq, ra,
					size, VHOST_ACCESS_RW);
		if (!vva)
			vhost_user_iotlb_miss(dev, ra, VHOST_ACCESS_RW);

		return vva;
	}

	return qva_to_vva(dev, ra, size);
}

static struct virtio_net *
translate_ring_addresses(struct virtio_net *dev, int vq_index)
{
	struct vhost_virtqueue *vq = dev->virtqueue[vq_index];
	struct vhost_vring_addr *addr = &vq->ring_addrs;
	uint64_t len, expected_len;

	if (vq_is_packed(dev)) {
		len = sizeof(struct vring_packed_desc) * vq->size;
		vq->desc_packed = (struct vring_packed_desc *)(uintptr_t)
			ring_addr_to_vva(dev, vq, addr->desc_user_addr, &len);
		vq->log_guest_addr = 0;
		if (vq->desc_packed == NULL ||
				len != sizeof(struct vring_packed_desc) *
				vq->size) {
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"(%d) failed to map desc_packed ring.\n",
				dev->vid);
			return dev;
		}

		dev = numa_realloc(dev, vq_index);
		vq = dev->virtqueue[vq_index];
		addr = &vq->ring_addrs;

		len = sizeof(struct vring_packed_desc_event);
		vq->driver_event = (struct vring_packed_desc_event *)
					(uintptr_t)ring_addr_to_vva(dev,
					vq, addr->avail_user_addr, &len);
		if (vq->driver_event == NULL ||
				len != sizeof(struct vring_packed_desc_event)) {
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"(%d) failed to find driver area address.\n",
				dev->vid);
			return dev;
		}

		len = sizeof(struct vring_packed_desc_event);
		vq->device_event = (struct vring_packed_desc_event *)
					(uintptr_t)ring_addr_to_vva(dev,
					vq, addr->used_user_addr, &len);
		if (vq->device_event == NULL ||
				len != sizeof(struct vring_packed_desc_event)) {
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"(%d) failed to find device area address.\n",
				dev->vid);
			return dev;
		}

		return dev;
	}

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	if (vq->desc && vq->avail && vq->used)
		return dev;

	len = sizeof(struct vring_desc) * vq->size;
	vq->desc = (struct vring_desc *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->desc_user_addr, &len);
	if (vq->desc == 0 || len != sizeof(struct vring_desc) * vq->size) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to map desc ring.\n",
			dev->vid);
		return dev;
	}

	dev = numa_realloc(dev, vq_index);
	vq = dev->virtqueue[vq_index];
	addr = &vq->ring_addrs;

	len = sizeof(struct vring_avail) + sizeof(uint16_t) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		len += sizeof(uint16_t);
	expected_len = len;
	vq->avail = (struct vring_avail *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->avail_user_addr, &len);
	if (vq->avail == 0 || len != expected_len) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to map avail ring.\n",
			dev->vid);
		return dev;
	}

	len = sizeof(struct vring_used) +
		sizeof(struct vring_used_elem) * vq->size;
	if (dev->features & (1ULL << VIRTIO_RING_F_EVENT_IDX))
		len += sizeof(uint16_t);
	expected_len = len;
	vq->used = (struct vring_used *)(uintptr_t)ring_addr_to_vva(dev,
			vq, addr->used_user_addr, &len);
	if (vq->used == 0 || len != expected_len) {
		RTE_LOG(DEBUG, VHOST_CONFIG,
			"(%d) failed to map used ring.\n",
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
vhost_user_set_vring_addr(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq;
	struct vhost_vring_addr *addr = &msg->payload.addr;

	if (dev->mem == NULL)
		return RTE_VHOST_MSG_RESULT_ERR;

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
			return RTE_VHOST_MSG_RESULT_ERR;

		*pdev = dev;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

/*
 * The virtio device sends us the available ring last used index.
 */
static int
vhost_user_set_vring_base(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq = dev->virtqueue[msg->payload.state.index];
	uint64_t val = msg->payload.state.num;

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
		vq->last_used_idx = msg->payload.state.num;
		vq->last_avail_idx = msg->payload.state.num;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
add_one_guest_page(struct virtio_net *dev, uint64_t guest_phys_addr,
		   uint64_t host_phys_addr, uint64_t size)
{
	struct guest_page *page, *last_page;
	struct guest_page *old_pages;

	if (dev->nr_guest_pages == dev->max_guest_pages) {
		dev->max_guest_pages *= 2;
		old_pages = dev->guest_pages;
		dev->guest_pages = realloc(dev->guest_pages,
					dev->max_guest_pages * sizeof(*page));
		if (!dev->guest_pages) {
			RTE_LOG(ERR, VHOST_CONFIG, "cannot realloc guest_pages\n");
			free(old_pages);
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
vhost_user_set_mem_table(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd)
{
	struct virtio_net *dev = *pdev;
	struct VhostUserMemory *memory = &msg->payload.memory;
	struct rte_vhost_mem_region *reg;
	void *mmap_addr;
	uint64_t mmap_size;
	uint64_t mmap_offset;
	uint64_t alignment;
	uint32_t i;
	int populate;
	int fd;

	if (memory->nregions > VHOST_MEMORY_MAX_NREGIONS) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"too many memory regions (%u)\n", memory->nregions);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (dev->mem && !vhost_memory_changed(memory, dev->mem)) {
		RTE_LOG(INFO, VHOST_CONFIG,
			"(%d) memory regions not changed\n", dev->vid);

		for (i = 0; i < memory->nregions; i++)
			close(msg->fds[i]);

		return RTE_VHOST_MSG_RESULT_OK;
	}

	if (dev->mem) {
		free_mem_region(dev);
		rte_free(dev->mem);
		dev->mem = NULL;
	}

	/* Flush IOTLB cache as previous HVAs are now invalid */
	if (dev->features & (1ULL << VIRTIO_F_IOMMU_PLATFORM))
		for (i = 0; i < dev->nr_vring; i++)
			vhost_user_iotlb_flush_all(dev->virtqueue[i]);

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
			return RTE_VHOST_MSG_RESULT_ERR;
		}
	}

	dev->mem = rte_zmalloc("vhost-mem-table", sizeof(struct rte_vhost_memory) +
		sizeof(struct rte_vhost_mem_region) * memory->nregions, 0);
	if (dev->mem == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to allocate memory for dev->mem\n",
			dev->vid);
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	dev->mem->nregions = memory->nregions;

	for (i = 0; i < memory->nregions; i++) {
		fd  = msg->fds[i];
		reg = &dev->mem->regions[i];

		reg->guest_phys_addr = memory->regions[i].guest_phys_addr;
		reg->guest_user_addr = memory->regions[i].userspace_addr;
		reg->size            = memory->regions[i].memory_size;
		reg->fd              = fd;

		mmap_offset = memory->regions[i].mmap_offset;

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

		if (dev->postcopy_listening) {
			/*
			 * We haven't a better way right now than sharing
			 * DPDK's virtual address with Qemu, so that Qemu can
			 * retrieve the region offset when handling userfaults.
			 */
			memory->regions[i].userspace_addr =
				reg->host_user_addr;
		}
	}
	if (dev->postcopy_listening) {
		/* Send the addresses back to qemu */
		msg->fd_num = 0;
		send_vhost_reply(main_fd, msg);

		/* Wait for qemu to acknolwedge it's got the addresses
		 * we've got to wait before we're allowed to generate faults.
		 */
		VhostUserMsg ack_msg;
		if (read_vhost_message(main_fd, &ack_msg) <= 0) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to read qemu ack on postcopy set-mem-table\n");
			goto err_mmap;
		}
		if (ack_msg.request.master != VHOST_USER_SET_MEM_TABLE) {
			RTE_LOG(ERR, VHOST_CONFIG,
				"Bad qemu ack on postcopy set-mem-table (%d)\n",
				ack_msg.request.master);
			goto err_mmap;
		}

		/* Now userfault register and we can use the memory */
		for (i = 0; i < memory->nregions; i++) {
#ifdef RTE_LIBRTE_VHOST_POSTCOPY
			reg = &dev->mem->regions[i];
			struct uffdio_register reg_struct;

			/*
			 * Let's register all the mmap'ed area to ensure
			 * alignment on page boundary.
			 */
			reg_struct.range.start =
				(uint64_t)(uintptr_t)reg->mmap_addr;
			reg_struct.range.len = reg->mmap_size;
			reg_struct.mode = UFFDIO_REGISTER_MODE_MISSING;

			if (ioctl(dev->postcopy_ufd, UFFDIO_REGISTER,
						&reg_struct)) {
				RTE_LOG(ERR, VHOST_CONFIG,
					"Failed to register ufd for region %d: (ufd = %d) %s\n",
					i, dev->postcopy_ufd,
					strerror(errno));
				goto err_mmap;
			}
			RTE_LOG(INFO, VHOST_CONFIG,
				"\t userfaultfd registered for range : %llx - %llx\n",
				reg_struct.range.start,
				reg_struct.range.start +
				reg_struct.range.len - 1);
#else
			goto err_mmap;
#endif
		}
	}

	for (i = 0; i < dev->nr_vring; i++) {
		struct vhost_virtqueue *vq = dev->virtqueue[i];

		if (vq->desc || vq->avail || vq->used) {
			/*
			 * If the memory table got updated, the ring addresses
			 * need to be translated again as virtual addresses have
			 * changed.
			 */
			vring_invalidate(dev, vq);

			dev = translate_ring_addresses(dev, i);
			if (!dev) {
				dev = *pdev;
				goto err_mmap;
			}

			*pdev = dev;
		}
	}

	dump_guest_pages(dev);

	return RTE_VHOST_MSG_RESULT_OK;

err_mmap:
	free_mem_region(dev);
	rte_free(dev->mem);
	dev->mem = NULL;
	return RTE_VHOST_MSG_RESULT_ERR;
}

static bool
vq_is_ready(struct virtio_net *dev, struct vhost_virtqueue *vq)
{
	bool rings_ok;

	if (!vq)
		return false;

	if (vq_is_packed(dev))
		rings_ok = !!vq->desc_packed;
	else
		rings_ok = vq->desc && vq->avail && vq->used;

	return rings_ok &&
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

		if (!vq_is_ready(dev, vq))
			return 0;
	}

	RTE_LOG(INFO, VHOST_CONFIG,
		"virtio is now ready for processing.\n");
	return 1;
}

static int
vhost_user_set_vring_call(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;

	file.index = msg->payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (msg->payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = msg->fds[0];
	RTE_LOG(INFO, VHOST_CONFIG,
		"vring call idx:%d file:%d\n", file.index, file.fd);

	vq = dev->virtqueue[file.index];
	if (vq->callfd >= 0)
		close(vq->callfd);

	vq->callfd = file.fd;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int vhost_user_set_vring_err(struct virtio_net **pdev __rte_unused,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	if (!(msg->payload.u64 & VHOST_USER_VRING_NOFD_MASK))
		close(msg->fds[0]);
	RTE_LOG(INFO, VHOST_CONFIG, "not implemented\n");

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_vring_kick(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_vring_file file;
	struct vhost_virtqueue *vq;

	file.index = msg->payload.u64 & VHOST_USER_VRING_IDX_MASK;
	if (msg->payload.u64 & VHOST_USER_VRING_NOFD_MASK)
		file.fd = VIRTIO_INVALID_EVENTFD;
	else
		file.fd = msg->fds[0];
	RTE_LOG(INFO, VHOST_CONFIG,
		"vring kick idx:%d file:%d\n", file.index, file.fd);

	/* Interpret ring addresses only when ring is started. */
	dev = translate_ring_addresses(dev, file.index);
	if (!dev)
		return RTE_VHOST_MSG_RESULT_ERR;

	*pdev = dev;

	vq = dev->virtqueue[file.index];

	/*
	 * When VHOST_USER_F_PROTOCOL_FEATURES is not negotiated,
	 * the ring starts already enabled. Otherwise, it is enabled via
	 * the SET_VRING_ENABLE message.
	 */
	if (!(dev->features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES))) {
		vq->enabled = 1;
		if (dev->notify_ops->vring_state_changed)
			dev->notify_ops->vring_state_changed(
				dev->vid, file.index, 1);
	}

	if (vq->kickfd >= 0)
		close(vq->kickfd);
	vq->kickfd = file.fd;

	return RTE_VHOST_MSG_RESULT_OK;
}

static void
free_zmbufs(struct vhost_virtqueue *vq)
{
	drain_zmbuf_list(vq);

	rte_free(vq->zmbufs);
}

/*
 * when virtio is stopped, qemu will send us the GET_VRING_BASE message.
 */
static int
vhost_user_get_vring_base(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_virtqueue *vq = dev->virtqueue[msg->payload.state.index];
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
		msg->payload.state.num = val;
	} else {
		msg->payload.state.num = vq->last_avail_idx;
	}

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

	vq->signalled_used_valid = false;

	if (dev->dequeue_zero_copy)
		free_zmbufs(vq);
	if (vq_is_packed(dev)) {
		rte_free(vq->shadow_used_packed);
		vq->shadow_used_packed = NULL;
	} else {
		rte_free(vq->shadow_used_split);
		vq->shadow_used_split = NULL;
	}

	rte_free(vq->batch_copy_elems);
	vq->batch_copy_elems = NULL;

	msg->size = sizeof(msg->payload.state);
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

/*
 * when virtio queues are ready to work, qemu will send us to
 * enable the virtio queue pair.
 */
static int
vhost_user_set_vring_enable(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
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

	/* On disable, rings have to be stopped being processed. */
	if (!enable && dev->dequeue_zero_copy)
		drain_zmbuf_list(dev->virtqueue[index]);

	dev->virtqueue[index]->enabled = enable;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_get_protocol_features(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
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
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int
vhost_user_set_protocol_features(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	uint64_t protocol_features = msg->payload.u64;
	uint64_t slave_protocol_features = 0;

	rte_vhost_driver_get_protocol_features(dev->ifname,
			&slave_protocol_features);
	if (protocol_features & ~slave_protocol_features) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) received invalid protocol features.\n",
			dev->vid);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->protocol_features = protocol_features;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_log_base(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	int fd = msg->fds[0];
	uint64_t size, off;
	void *addr;

	if (fd < 0) {
		RTE_LOG(ERR, VHOST_CONFIG, "invalid log fd: %d\n", fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	if (msg->size != sizeof(VhostUserLog)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"invalid log base msg size: %"PRId32" != %d\n",
			msg->size, (int)sizeof(VhostUserLog));
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	size = msg->payload.log.mmap_size;
	off  = msg->payload.log.mmap_offset;

	/* Don't allow mmap_offset to point outside the mmap region */
	if (off > size) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"log offset %#"PRIx64" exceeds log size %#"PRIx64"\n",
			off, size);
		return RTE_VHOST_MSG_RESULT_ERR;
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

	/*
	 * The spec is not clear about it (yet), but QEMU doesn't expect
	 * any payload in the reply.
	 */
	msg->size = 0;
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

static int vhost_user_set_log_fd(struct virtio_net **pdev __rte_unused,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	close(msg->fds[0]);
	RTE_LOG(INFO, VHOST_CONFIG, "not implemented.\n");

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
vhost_user_send_rarp(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
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

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_net_set_mtu(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	if (msg->payload.u64 < VIRTIO_MIN_MTU ||
			msg->payload.u64 > VIRTIO_MAX_MTU) {
		RTE_LOG(ERR, VHOST_CONFIG, "Invalid MTU size (%"PRIu64")\n",
				msg->payload.u64);

		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->mtu = msg->payload.u64;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_req_fd(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	int fd = msg->fds[0];

	if (fd < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Invalid file descriptor for slave channel (%d)\n",
				fd);
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	dev->slave_req_fd = fd;

	return RTE_VHOST_MSG_RESULT_OK;
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
vhost_user_iotlb_msg(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
	struct vhost_iotlb_msg *imsg = &msg->payload.iotlb;
	uint16_t i;
	uint64_t vva, len;

	switch (imsg->type) {
	case VHOST_IOTLB_UPDATE:
		len = imsg->size;
		vva = qva_to_vva(dev, imsg->uaddr, &len);
		if (!vva)
			return RTE_VHOST_MSG_RESULT_ERR;

		for (i = 0; i < dev->nr_vring; i++) {
			struct vhost_virtqueue *vq = dev->virtqueue[i];

			vhost_user_iotlb_cache_insert(vq, imsg->iova, vva,
					len, imsg->perm);

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
		return RTE_VHOST_MSG_RESULT_ERR;
	}

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_set_postcopy_advise(struct virtio_net **pdev,
			struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;
#ifdef RTE_LIBRTE_VHOST_POSTCOPY
	struct uffdio_api api_struct;

	dev->postcopy_ufd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);

	if (dev->postcopy_ufd == -1) {
		RTE_LOG(ERR, VHOST_CONFIG, "Userfaultfd not available: %s\n",
			strerror(errno));
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	api_struct.api = UFFD_API;
	api_struct.features = 0;
	if (ioctl(dev->postcopy_ufd, UFFDIO_API, &api_struct)) {
		RTE_LOG(ERR, VHOST_CONFIG, "UFFDIO_API ioctl failure: %s\n",
			strerror(errno));
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	msg->fds[0] = dev->postcopy_ufd;
	msg->fd_num = 1;

	return RTE_VHOST_MSG_RESULT_REPLY;
#else
	dev->postcopy_ufd = -1;
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_ERR;
#endif
}

static int
vhost_user_set_postcopy_listen(struct virtio_net **pdev,
			struct VhostUserMsg *msg __rte_unused,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	if (dev->mem && dev->mem->nregions) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Regions already registered at postcopy-listen\n");
		return RTE_VHOST_MSG_RESULT_ERR;
	}
	dev->postcopy_listening = 1;

	return RTE_VHOST_MSG_RESULT_OK;
}

static int
vhost_user_postcopy_end(struct virtio_net **pdev, struct VhostUserMsg *msg,
			int main_fd __rte_unused)
{
	struct virtio_net *dev = *pdev;

	dev->postcopy_listening = 0;
	if (dev->postcopy_ufd >= 0) {
		close(dev->postcopy_ufd);
		dev->postcopy_ufd = -1;
	}

	msg->payload.u64 = 0;
	msg->size = sizeof(msg->payload.u64);
	msg->fd_num = 0;

	return RTE_VHOST_MSG_RESULT_REPLY;
}

typedef int (*vhost_message_handler_t)(struct virtio_net **pdev,
					struct VhostUserMsg *msg,
					int main_fd);
static vhost_message_handler_t vhost_message_handlers[VHOST_USER_MAX] = {
	[VHOST_USER_NONE] = NULL,
	[VHOST_USER_GET_FEATURES] = vhost_user_get_features,
	[VHOST_USER_SET_FEATURES] = vhost_user_set_features,
	[VHOST_USER_SET_OWNER] = vhost_user_set_owner,
	[VHOST_USER_RESET_OWNER] = vhost_user_reset_owner,
	[VHOST_USER_SET_MEM_TABLE] = vhost_user_set_mem_table,
	[VHOST_USER_SET_LOG_BASE] = vhost_user_set_log_base,
	[VHOST_USER_SET_LOG_FD] = vhost_user_set_log_fd,
	[VHOST_USER_SET_VRING_NUM] = vhost_user_set_vring_num,
	[VHOST_USER_SET_VRING_ADDR] = vhost_user_set_vring_addr,
	[VHOST_USER_SET_VRING_BASE] = vhost_user_set_vring_base,
	[VHOST_USER_GET_VRING_BASE] = vhost_user_get_vring_base,
	[VHOST_USER_SET_VRING_KICK] = vhost_user_set_vring_kick,
	[VHOST_USER_SET_VRING_CALL] = vhost_user_set_vring_call,
	[VHOST_USER_SET_VRING_ERR] = vhost_user_set_vring_err,
	[VHOST_USER_GET_PROTOCOL_FEATURES] = vhost_user_get_protocol_features,
	[VHOST_USER_SET_PROTOCOL_FEATURES] = vhost_user_set_protocol_features,
	[VHOST_USER_GET_QUEUE_NUM] = vhost_user_get_queue_num,
	[VHOST_USER_SET_VRING_ENABLE] = vhost_user_set_vring_enable,
	[VHOST_USER_SEND_RARP] = vhost_user_send_rarp,
	[VHOST_USER_NET_SET_MTU] = vhost_user_net_set_mtu,
	[VHOST_USER_SET_SLAVE_REQ_FD] = vhost_user_set_req_fd,
	[VHOST_USER_IOTLB_MSG] = vhost_user_iotlb_msg,
	[VHOST_USER_POSTCOPY_ADVISE] = vhost_user_set_postcopy_advise,
	[VHOST_USER_POSTCOPY_LISTEN] = vhost_user_set_postcopy_listen,
	[VHOST_USER_POSTCOPY_END] = vhost_user_postcopy_end,
};


/* return bytes# of read on success or negative val on failure. */
static int
read_vhost_message(int sockfd, struct VhostUserMsg *msg)
{
	int ret;

	ret = read_fd_message(sockfd, (char *)msg, VHOST_USER_HDR_SIZE,
		msg->fds, VHOST_MEMORY_MAX_NREGIONS, &msg->fd_num);
	if (ret <= 0)
		return ret;

	if (msg->size) {
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
send_vhost_message(int sockfd, struct VhostUserMsg *msg)
{
	if (!msg)
		return 0;

	return send_fd_message(sockfd, (char *)msg,
		VHOST_USER_HDR_SIZE + msg->size, msg->fds, msg->fd_num);
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

	return send_vhost_message(sockfd, msg);
}

static int
send_vhost_slave_message(struct virtio_net *dev, struct VhostUserMsg *msg)
{
	int ret;

	if (msg->flags & VHOST_USER_NEED_REPLY)
		rte_spinlock_lock(&dev->slave_req_lock);

	ret = send_vhost_message(dev->slave_req_fd, msg);
	if (ret < 0 && (msg->flags & VHOST_USER_NEED_REPLY))
		rte_spinlock_unlock(&dev->slave_req_lock);

	return ret;
}

/*
 * Allocate a queue pair if it hasn't been allocated yet
 */
static int
vhost_user_check_and_alloc_queue_pair(struct virtio_net *dev,
			struct VhostUserMsg *msg)
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
	bool handled;
	int request;

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
	if (ret <= 0) {
		if (ret < 0)
			RTE_LOG(ERR, VHOST_CONFIG,
				"vhost read message failed\n");
		else
			RTE_LOG(INFO, VHOST_CONFIG,
				"vhost peer closed\n");

		return -1;
	}

	ret = 0;
	request = msg.request.master;
	if (request > VHOST_USER_NONE && request < VHOST_USER_MAX &&
			vhost_message_str[request]) {
		if (request != VHOST_USER_IOTLB_MSG)
			RTE_LOG(INFO, VHOST_CONFIG, "read message %s\n",
				vhost_message_str[request]);
		else
			RTE_LOG(DEBUG, VHOST_CONFIG, "read message %s\n",
				vhost_message_str[request]);
	} else {
		RTE_LOG(DEBUG, VHOST_CONFIG, "External request %d\n", request);
	}

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
	case VHOST_USER_SET_SLAVE_REQ_FD:
		vhost_user_lock_all_queue_pairs(dev);
		unlock_required = 1;
		break;
	default:
		break;

	}

	handled = false;
	if (dev->extern_ops.pre_msg_handle) {
		ret = (*dev->extern_ops.pre_msg_handle)(dev->vid,
				(void *)&msg);
		switch (ret) {
		case RTE_VHOST_MSG_RESULT_REPLY:
			send_vhost_reply(fd, &msg);
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

	if (request > VHOST_USER_NONE && request < VHOST_USER_MAX) {
		if (!vhost_message_handlers[request])
			goto skip_to_post_handle;
		ret = vhost_message_handlers[request](&dev, &msg, fd);

		switch (ret) {
		case RTE_VHOST_MSG_RESULT_ERR:
			RTE_LOG(ERR, VHOST_CONFIG,
				"Processing %s failed.\n",
				vhost_message_str[request]);
			handled = true;
			break;
		case RTE_VHOST_MSG_RESULT_OK:
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"Processing %s succeeded.\n",
				vhost_message_str[request]);
			handled = true;
			break;
		case RTE_VHOST_MSG_RESULT_REPLY:
			RTE_LOG(DEBUG, VHOST_CONFIG,
				"Processing %s succeeded and needs reply.\n",
				vhost_message_str[request]);
			send_vhost_reply(fd, &msg);
			handled = true;
			break;
		default:
			break;
		}
	}

skip_to_post_handle:
	if (ret != RTE_VHOST_MSG_RESULT_ERR &&
			dev->extern_ops.post_msg_handle) {
		ret = (*dev->extern_ops.post_msg_handle)(dev->vid,
				(void *)&msg);
		switch (ret) {
		case RTE_VHOST_MSG_RESULT_REPLY:
			send_vhost_reply(fd, &msg);
			/* Fall-through */
		case RTE_VHOST_MSG_RESULT_ERR:
		case RTE_VHOST_MSG_RESULT_OK:
			handled = true;
		case RTE_VHOST_MSG_RESULT_NOT_HANDLED:
		default:
			break;
		}
	}

	if (unlock_required)
		vhost_user_unlock_all_queue_pairs(dev);

	/* If message was not handled at this stage, treat it as an error */
	if (!handled) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"vhost message (req: %d) was not handled.\n", request);
		ret = RTE_VHOST_MSG_RESULT_ERR;
	}

	/*
	 * If the request required a reply that was already sent,
	 * this optional reply-ack won't be sent as the
	 * VHOST_USER_NEED_REPLY was cleared in send_vhost_reply().
	 */
	if (msg.flags & VHOST_USER_NEED_REPLY) {
		msg.payload.u64 = ret == RTE_VHOST_MSG_RESULT_ERR;
		msg.size = sizeof(msg.payload.u64);
		msg.fd_num = 0;
		send_vhost_reply(fd, &msg);
	} else if (ret == RTE_VHOST_MSG_RESULT_ERR) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"vhost message handling failed.\n");
		return -1;
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
			msg.request.master == VHOST_USER_SET_VRING_CALL) {
		if (vdpa_dev->ops->dev_conf)
			vdpa_dev->ops->dev_conf(dev->vid);
		dev->flags |= VIRTIO_DEV_VDPA_CONFIGURED;
	}

	return 0;
}

static int process_slave_message_reply(struct virtio_net *dev,
				       const struct VhostUserMsg *msg)
{
	struct VhostUserMsg msg_reply;
	int ret;

	if ((msg->flags & VHOST_USER_NEED_REPLY) == 0)
		return 0;

	if (read_vhost_message(dev->slave_req_fd, &msg_reply) < 0) {
		ret = -1;
		goto out;
	}

	if (msg_reply.request.slave != msg->request.slave) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Received unexpected msg type (%u), expected %u\n",
			msg_reply.request.slave, msg->request.slave);
		ret = -1;
		goto out;
	}

	ret = msg_reply.payload.u64 ? -1 : 0;

out:
	rte_spinlock_unlock(&dev->slave_req_lock);
	return ret;
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

	ret = send_vhost_message(dev->slave_req_fd, &msg);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
				"Failed to send IOTLB miss message (%d)\n",
				ret);
		return ret;
	}

	return 0;
}

static int vhost_user_slave_set_vring_host_notifier(struct virtio_net *dev,
						    int index, int fd,
						    uint64_t offset,
						    uint64_t size)
{
	int ret;
	struct VhostUserMsg msg = {
		.request.slave = VHOST_USER_SLAVE_VRING_HOST_NOTIFIER_MSG,
		.flags = VHOST_USER_VERSION | VHOST_USER_NEED_REPLY,
		.size = sizeof(msg.payload.area),
		.payload.area = {
			.u64 = index & VHOST_USER_VRING_IDX_MASK,
			.size = size,
			.offset = offset,
		},
	};

	if (fd < 0)
		msg.payload.area.u64 |= VHOST_USER_VRING_NOFD_MASK;
	else {
		msg.fds[0] = fd;
		msg.fd_num = 1;
	}

	ret = send_vhost_slave_message(dev, &msg);
	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to set host notifier (%d)\n", ret);
		return ret;
	}

	return process_slave_message_reply(dev, &msg);
}

int rte_vhost_host_notifier_ctrl(int vid, bool enable)
{
	struct virtio_net *dev;
	struct rte_vdpa_device *vdpa_dev;
	int vfio_device_fd, did, ret = 0;
	uint64_t offset, size;
	unsigned int i;

	dev = get_device(vid);
	if (!dev)
		return -ENODEV;

	did = dev->vdpa_dev_id;
	if (did < 0)
		return -EINVAL;

	if (!(dev->features & (1ULL << VIRTIO_F_VERSION_1)) ||
	    !(dev->features & (1ULL << VHOST_USER_F_PROTOCOL_FEATURES)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD)) ||
	    !(dev->protocol_features &
			(1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER)))
		return -ENOTSUP;

	vdpa_dev = rte_vdpa_get_device(did);
	if (!vdpa_dev)
		return -ENODEV;

	RTE_FUNC_PTR_OR_ERR_RET(vdpa_dev->ops->get_vfio_device_fd, -ENOTSUP);
	RTE_FUNC_PTR_OR_ERR_RET(vdpa_dev->ops->get_notify_area, -ENOTSUP);

	vfio_device_fd = vdpa_dev->ops->get_vfio_device_fd(vid);
	if (vfio_device_fd < 0)
		return -ENOTSUP;

	if (enable) {
		for (i = 0; i < dev->nr_vring; i++) {
			if (vdpa_dev->ops->get_notify_area(vid, i, &offset,
					&size) < 0) {
				ret = -ENOTSUP;
				goto disable;
			}

			if (vhost_user_slave_set_vring_host_notifier(dev, i,
					vfio_device_fd, offset, size) < 0) {
				ret = -EFAULT;
				goto disable;
			}
		}
	} else {
disable:
		for (i = 0; i < dev->nr_vring; i++) {
			vhost_user_slave_set_vring_host_notifier(dev, i, -1,
					0, 0);
		}
	}

	return ret;
}
