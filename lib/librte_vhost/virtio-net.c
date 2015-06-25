/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#include <linux/vhost.h>
#include <linux/virtio_net.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <sys/socket.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_virtio_net.h>

#include "vhost-net.h"
#include "virtio-net.h"

/*
 * Device linked list structure for configuration.
 */
struct virtio_net_config_ll {
	struct virtio_net dev;			/* Virtio device.*/
	struct virtio_net_config_ll *next;	/* Next dev on linked list.*/
};

/* device ops to add/remove device to/from data core. */
struct virtio_net_device_ops const *notify_ops;
/* root address of the linked list of managed virtio devices */
static struct virtio_net_config_ll *ll_root;

/* Features supported by this lib. */
#define VHOST_SUPPORTED_FEATURES ((1ULL << VIRTIO_NET_F_MRG_RXBUF) | \
				(1ULL << VIRTIO_NET_F_CTRL_VQ) | \
				(1ULL << VIRTIO_NET_F_CTRL_RX) | \
				(1ULL << VHOST_F_LOG_ALL))
static uint64_t VHOST_FEATURES = VHOST_SUPPORTED_FEATURES;


/*
 * Converts QEMU virtual address to Vhost virtual address. This function is
 * used to convert the ring addresses to our address space.
 */
static uint64_t
qva_to_vva(struct virtio_net *dev, uint64_t qemu_va)
{
	struct virtio_memory_regions *region;
	uint64_t vhost_va = 0;
	uint32_t regionidx = 0;

	/* Find the region where the address lives. */
	for (regionidx = 0; regionidx < dev->mem->nregions; regionidx++) {
		region = &dev->mem->regions[regionidx];
		if ((qemu_va >= region->userspace_address) &&
			(qemu_va <= region->userspace_address +
			region->memory_size)) {
			vhost_va = qemu_va + region->guest_phys_address +
				region->address_offset -
				region->userspace_address;
			break;
		}
	}
	return vhost_va;
}


/*
 * Retrieves an entry from the devices configuration linked list.
 */
static struct virtio_net_config_ll *
get_config_ll_entry(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* Loop through linked list until the device_fh is found. */
	while (ll_dev != NULL) {
		if (ll_dev->dev.device_fh == ctx.fh)
			return ll_dev;
		ll_dev = ll_dev->next;
	}

	return NULL;
}

/*
 * Searches the configuration core linked list and
 * retrieves the device if it exists.
 */
struct virtio_net *
get_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev;

	ll_dev = get_config_ll_entry(ctx);

	if (ll_dev)
		return &ll_dev->dev;

	RTE_LOG(ERR, VHOST_CONFIG,
		"(%"PRIu64") Device not found in linked list.\n", ctx.fh);
	return NULL;
}

/*
 * Add entry containing a device to the device configuration linked list.
 */
static void
add_config_ll_entry(struct virtio_net_config_ll *new_ll_dev)
{
	struct virtio_net_config_ll *ll_dev = ll_root;

	/* If ll_dev == NULL then this is the first device so go to else */
	if (ll_dev) {
		/* If the 1st device_fh != 0 then we insert our device here. */
		if (ll_dev->dev.device_fh != 0) {
			new_ll_dev->dev.device_fh = 0;
			new_ll_dev->next = ll_dev;
			ll_root = new_ll_dev;
		} else {
			/*
			 * Increment through the ll until we find un unused
			 * device_fh. Insert the device at that entry.
			 */
			while ((ll_dev->next != NULL) &&
				(ll_dev->dev.device_fh ==
					(ll_dev->next->dev.device_fh - 1)))
				ll_dev = ll_dev->next;

			new_ll_dev->dev.device_fh = ll_dev->dev.device_fh + 1;
			new_ll_dev->next = ll_dev->next;
			ll_dev->next = new_ll_dev;
		}
	} else {
		ll_root = new_ll_dev;
		ll_root->dev.device_fh = 0;
	}

}

/*
 * Unmap any memory, close any file descriptors and
 * free any memory owned by a device.
 */
static void
cleanup_device(struct virtio_net *dev)
{
	/* Unmap QEMU memory file if mapped. */
	if (dev->mem) {
		munmap((void *)(uintptr_t)dev->mem->mapped_address,
			(size_t)dev->mem->mapped_size);
		free(dev->mem);
	}

	/* Close any event notifiers opened by device. */
	if ((int)dev->virtqueue[VIRTIO_RXQ]->callfd >= 0)
		close((int)dev->virtqueue[VIRTIO_RXQ]->callfd);
	if ((int)dev->virtqueue[VIRTIO_RXQ]->kickfd >= 0)
		close((int)dev->virtqueue[VIRTIO_RXQ]->kickfd);
	if ((int)dev->virtqueue[VIRTIO_TXQ]->callfd >= 0)
		close((int)dev->virtqueue[VIRTIO_TXQ]->callfd);
	if ((int)dev->virtqueue[VIRTIO_TXQ]->kickfd >= 0)
		close((int)dev->virtqueue[VIRTIO_TXQ]->kickfd);
}

/*
 * Release virtqueues and device memory.
 */
static void
free_device(struct virtio_net_config_ll *ll_dev)
{
	/* Free any malloc'd memory */
	rte_free(ll_dev->dev.virtqueue[VIRTIO_RXQ]);
	rte_free(ll_dev->dev.virtqueue[VIRTIO_TXQ]);
	rte_free(ll_dev);
}

/*
 * Remove an entry from the device configuration linked list.
 */
static struct virtio_net_config_ll *
rm_config_ll_entry(struct virtio_net_config_ll *ll_dev,
	struct virtio_net_config_ll *ll_dev_last)
{
	/* First remove the device and then clean it up. */
	if (ll_dev == ll_root) {
		ll_root = ll_dev->next;
		cleanup_device(&ll_dev->dev);
		free_device(ll_dev);
		return ll_root;
	} else {
		if (likely(ll_dev_last != NULL)) {
			ll_dev_last->next = ll_dev->next;
			cleanup_device(&ll_dev->dev);
			free_device(ll_dev);
			return ll_dev_last->next;
		} else {
			cleanup_device(&ll_dev->dev);
			free_device(ll_dev);
			RTE_LOG(ERR, VHOST_CONFIG,
				"Remove entry from config_ll failed\n");
			return NULL;
		}
	}
}

/*
 *  Initialise all variables in device structure.
 */
static void
init_device(struct virtio_net *dev)
{
	uint64_t vq_offset;

	/*
	 * Virtqueues have already been malloced so
	 * we don't want to set them to NULL.
	 */
	vq_offset = offsetof(struct virtio_net, mem);

	/* Set everything to 0. */
	memset((void *)(uintptr_t)((uint64_t)(uintptr_t)dev + vq_offset), 0,
		(sizeof(struct virtio_net) - (size_t)vq_offset));
	memset(dev->virtqueue[VIRTIO_RXQ], 0, sizeof(struct vhost_virtqueue));
	memset(dev->virtqueue[VIRTIO_TXQ], 0, sizeof(struct vhost_virtqueue));

	dev->virtqueue[VIRTIO_RXQ]->kickfd = (eventfd_t)-1;
	dev->virtqueue[VIRTIO_RXQ]->callfd = (eventfd_t)-1;
	dev->virtqueue[VIRTIO_TXQ]->kickfd = (eventfd_t)-1;
	dev->virtqueue[VIRTIO_TXQ]->callfd = (eventfd_t)-1;

	/* Backends are set to -1 indicating an inactive device. */
	dev->virtqueue[VIRTIO_RXQ]->backend = VIRTIO_DEV_STOPPED;
	dev->virtqueue[VIRTIO_TXQ]->backend = VIRTIO_DEV_STOPPED;
}

/*
 * Function is called from the CUSE open function. The device structure is
 * initialised and a new entry is added to the device configuration linked
 * list.
 */
static int
new_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *new_ll_dev;
	struct vhost_virtqueue *virtqueue_rx, *virtqueue_tx;

	/* Setup device and virtqueues. */
	new_ll_dev = rte_malloc(NULL, sizeof(struct virtio_net_config_ll), 0);
	if (new_ll_dev == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for dev.\n",
			ctx.fh);
		return -1;
	}

	virtqueue_rx = rte_malloc(NULL, sizeof(struct vhost_virtqueue), 0);
	if (virtqueue_rx == NULL) {
		rte_free(new_ll_dev);
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for rxq.\n",
			ctx.fh);
		return -1;
	}

	virtqueue_tx = rte_malloc(NULL, sizeof(struct vhost_virtqueue), 0);
	if (virtqueue_tx == NULL) {
		rte_free(virtqueue_rx);
		rte_free(new_ll_dev);
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to allocate memory for txq.\n",
			ctx.fh);
		return -1;
	}

	new_ll_dev->dev.virtqueue[VIRTIO_RXQ] = virtqueue_rx;
	new_ll_dev->dev.virtqueue[VIRTIO_TXQ] = virtqueue_tx;

	/* Initialise device and virtqueues. */
	init_device(&new_ll_dev->dev);

	new_ll_dev->next = NULL;

	/* Add entry to device configuration linked list. */
	add_config_ll_entry(new_ll_dev);

	return new_ll_dev->dev.device_fh;
}

/*
 * Function is called from the CUSE release function. This function will
 * cleanup the device and remove it from device configuration linked list.
 */
static void
destroy_device(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev_cur_ctx, *ll_dev_last = NULL;
	struct virtio_net_config_ll *ll_dev_cur = ll_root;

	/* Find the linked list entry for the device to be removed. */
	ll_dev_cur_ctx = get_config_ll_entry(ctx);
	while (ll_dev_cur != NULL) {
		/*
		 * If the device is found or
		 * a device that doesn't exist is found then it is removed.
		 */
		if (ll_dev_cur == ll_dev_cur_ctx) {
			/*
			 * If the device is running on a data core then call
			 * the function to remove it from the data core.
			 */
			if ((ll_dev_cur->dev.flags & VIRTIO_DEV_RUNNING))
				notify_ops->destroy_device(&(ll_dev_cur->dev));
			ll_dev_cur = rm_config_ll_entry(ll_dev_cur,
					ll_dev_last);
		} else {
			ll_dev_last = ll_dev_cur;
			ll_dev_cur = ll_dev_cur->next;
		}
	}
}

static void
set_ifname(struct vhost_device_ctx ctx,
	const char *if_name, unsigned int if_len)
{
	struct virtio_net *dev;
	unsigned int len;

	dev = get_device(ctx);
	if (dev == NULL)
		return;

	len = if_len > sizeof(dev->ifname) ?
		sizeof(dev->ifname) : if_len;

	strncpy(dev->ifname, if_name, len);
}


/*
 * Called from CUSE IOCTL: VHOST_SET_OWNER
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
static int
set_owner(struct vhost_device_ctx ctx)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_RESET_OWNER
 */
static int
reset_owner(struct vhost_device_ctx ctx)
{
	struct virtio_net_config_ll *ll_dev;

	ll_dev = get_config_ll_entry(ctx);

	cleanup_device(&ll_dev->dev);
	init_device(&ll_dev->dev);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_FEATURES
 * The features that we support are requested.
 */
static int
get_features(struct vhost_device_ctx ctx, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* Send our supported features. */
	*pu = VHOST_FEATURES;
	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_FEATURES
 * We receive the negotiated features supported by us and the virtio device.
 */
static int
set_features(struct vhost_device_ctx ctx, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;
	if (*pu & ~VHOST_FEATURES)
		return -1;

	/* Store the negotiated feature list for the device. */
	dev->features = *pu;

	/* Set the vhost_hlen depending on if VIRTIO_NET_F_MRG_RXBUF is set. */
	if (dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF)) {
		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") Mergeable RX buffers enabled\n",
			dev->device_fh);
		dev->virtqueue[VIRTIO_RXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr_mrg_rxbuf);
		dev->virtqueue[VIRTIO_TXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		LOG_DEBUG(VHOST_CONFIG,
			"(%"PRIu64") Mergeable RX buffers disabled\n",
			dev->device_fh);
		dev->virtqueue[VIRTIO_RXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr);
		dev->virtqueue[VIRTIO_TXQ]->vhost_hlen =
			sizeof(struct virtio_net_hdr);
	}
	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_NUM
 * The virtio device sends us the size of the descriptor ring.
 */
static int
set_vring_num(struct vhost_device_ctx ctx, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->size = state->num;

	return 0;
}

/*
 * Reallocate virtio_det and vhost_virtqueue data structure to make them on the
 * same numa node as the memory of vring descriptor.
 */
#ifdef RTE_LIBRTE_VHOST_NUMA
static struct virtio_net*
numa_realloc(struct virtio_net *dev, int index)
{
	int oldnode, newnode;
	struct virtio_net_config_ll *old_ll_dev, *new_ll_dev = NULL;
	struct vhost_virtqueue *old_vq, *new_vq = NULL;
	int ret;
	int realloc_dev = 0, realloc_vq = 0;

	old_ll_dev = (struct virtio_net_config_ll *)dev;
	old_vq = dev->virtqueue[index];

	ret  = get_mempolicy(&newnode, NULL, 0, old_vq->desc,
			MPOL_F_NODE | MPOL_F_ADDR);
	ret = ret | get_mempolicy(&oldnode, NULL, 0, old_ll_dev,
			MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Unable to get vring desc or dev numa information.\n");
		return dev;
	}
	if (oldnode != newnode)
		realloc_dev = 1;

	ret = get_mempolicy(&oldnode, NULL, 0, old_vq,
			MPOL_F_NODE | MPOL_F_ADDR);
	if (ret) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Unable to get vq numa information.\n");
		return dev;
	}
	if (oldnode != newnode)
		realloc_vq = 1;

	if (realloc_dev == 0 && realloc_vq == 0)
		return dev;

	if (realloc_dev)
		new_ll_dev = rte_malloc_socket(NULL,
			sizeof(struct virtio_net_config_ll), 0, newnode);
	if (realloc_vq)
		new_vq = rte_malloc_socket(NULL,
			sizeof(struct vhost_virtqueue), 0, newnode);
	if (!new_ll_dev && !new_vq)
		return dev;

	if (realloc_vq)
		memcpy(new_vq, old_vq, sizeof(*new_vq));
	if (realloc_dev)
		memcpy(new_ll_dev, old_ll_dev, sizeof(*new_ll_dev));
	(new_ll_dev ? new_ll_dev : old_ll_dev)->dev.virtqueue[index] =
		new_vq ? new_vq : old_vq;
	if (realloc_vq)
		rte_free(old_vq);
	if (realloc_dev) {
		if (ll_root == old_ll_dev)
			ll_root = new_ll_dev;
		else {
			struct virtio_net_config_ll *prev = ll_root;
			while (prev->next != old_ll_dev)
				prev = prev->next;
			prev->next = new_ll_dev;
			new_ll_dev->next = old_ll_dev->next;
		}
		rte_free(old_ll_dev);
	}

	return realloc_dev ? &new_ll_dev->dev : dev;
}
#else
static struct virtio_net*
numa_realloc(struct virtio_net *dev, int index __rte_unused)
{
	return dev;
}
#endif

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_ADDR
 * The virtio device sends us the desc, used and avail ring addresses.
 * This function then converts these to our address space.
 */
static int
set_vring_addr(struct vhost_device_ctx ctx, struct vhost_vring_addr *addr)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* addr->index refers to the queue index. The txq 1, rxq is 0. */
	vq = dev->virtqueue[addr->index];

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	vq->desc = (struct vring_desc *)(uintptr_t)qva_to_vva(dev,
			addr->desc_user_addr);
	if (vq->desc == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find desc ring address.\n",
			dev->device_fh);
		return -1;
	}

	dev = numa_realloc(dev, addr->index);
	vq = dev->virtqueue[addr->index];

	vq->avail = (struct vring_avail *)(uintptr_t)qva_to_vva(dev,
			addr->avail_user_addr);
	if (vq->avail == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find avail ring address.\n",
			dev->device_fh);
		return -1;
	}

	vq->used = (struct vring_used *)(uintptr_t)qva_to_vva(dev,
			addr->used_user_addr);
	if (vq->used == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%"PRIu64") Failed to find used ring address.\n",
			dev->device_fh);
		return -1;
	}

	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address desc: %p\n",
			dev->device_fh, vq->desc);
	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address avail: %p\n",
			dev->device_fh, vq->avail);
	LOG_DEBUG(VHOST_CONFIG, "(%"PRIu64") mapped address used: %p\n",
			dev->device_fh, vq->used);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_BASE
 * The virtio device sends us the available ring last used index.
 */
static int
set_vring_base(struct vhost_device_ctx ctx, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->last_used_idx = state->num;
	dev->virtqueue[state->index]->last_used_idx_res = state->num;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_VRING_BASE
 * We send the virtio device our available ring last used index.
 */
static int
get_vring_base(struct vhost_device_ctx ctx, uint32_t index,
	struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	state->index = index;
	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	state->num = dev->virtqueue[state->index]->last_used_idx;

	return 0;
}


/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_CALL
 * The virtio device sends an eventfd to interrupt the guest. This fd gets
 * copied into our process space.
 */
static int
set_vring_call(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];

	if ((int)vq->callfd >= 0)
		close((int)vq->callfd);

	vq->callfd = file->fd;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_KICK
 * The virtio device sends an eventfd that it can use to notify us.
 * This fd gets copied into our process space.
 */
static int
set_vring_kick(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];

	if ((int)vq->kickfd >= 0)
		close((int)vq->kickfd);

	vq->kickfd = file->fd;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_NET_SET_BACKEND
 * To complete device initialisation when the virtio driver is loaded,
 * we are provided with a valid fd for a tap device (not used by us).
 * If this happens then we can add the device to a data core.
 * When the virtio driver is removed we get fd=-1.
 * At that point we remove the device from the data core.
 * The device will still exist in the device configuration linked list.
 */
static int
set_backend(struct vhost_device_ctx ctx, struct vhost_vring_file *file)
{
	struct virtio_net *dev;

	dev = get_device(ctx);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[file->index]->backend = file->fd;

	/*
	 * If the device isn't already running and both backend fds are set,
	 * we add the device.
	 */
	if (!(dev->flags & VIRTIO_DEV_RUNNING)) {
		if (((int)dev->virtqueue[VIRTIO_TXQ]->backend != VIRTIO_DEV_STOPPED) &&
			((int)dev->virtqueue[VIRTIO_RXQ]->backend != VIRTIO_DEV_STOPPED)) {
			return notify_ops->new_device(dev);
		}
	/* Otherwise we remove it. */
	} else
		if (file->fd == VIRTIO_DEV_STOPPED)
			notify_ops->destroy_device(dev);
	return 0;
}

/*
 * Function pointers are set for the device operations to allow CUSE to call
 * functions when an IOCTL, device_add or device_release is received.
 */
static const struct vhost_net_device_ops vhost_device_ops = {
	.new_device = new_device,
	.destroy_device = destroy_device,

	.set_ifname = set_ifname,

	.get_features = get_features,
	.set_features = set_features,

	.set_vring_num = set_vring_num,
	.set_vring_addr = set_vring_addr,
	.set_vring_base = set_vring_base,
	.get_vring_base = get_vring_base,

	.set_vring_kick = set_vring_kick,
	.set_vring_call = set_vring_call,

	.set_backend = set_backend,

	.set_owner = set_owner,
	.reset_owner = reset_owner,
};

/*
 * Called by main to setup callbacks when registering CUSE device.
 */
struct vhost_net_device_ops const *
get_virtio_net_callbacks(void)
{
	return &vhost_device_ops;
}

int rte_vhost_enable_guest_notification(struct virtio_net *dev,
	uint16_t queue_id, int enable)
{
	if (enable) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"guest notification isn't supported.\n");
		return -1;
	}

	dev->virtqueue[queue_id]->used->flags =
		enable ? 0 : VRING_USED_F_NO_NOTIFY;
	return 0;
}

uint64_t rte_vhost_feature_get(void)
{
	return VHOST_FEATURES;
}

int rte_vhost_feature_disable(uint64_t feature_mask)
{
	VHOST_FEATURES = VHOST_FEATURES & ~feature_mask;
	return 0;
}

int rte_vhost_feature_enable(uint64_t feature_mask)
{
	if ((feature_mask & VHOST_SUPPORTED_FEATURES) == feature_mask) {
		VHOST_FEATURES = VHOST_FEATURES | feature_mask;
		return 0;
	}
	return -1;
}

/*
 * Register ops so that we can add/remove device to data core.
 */
int
rte_vhost_driver_callback_register(struct virtio_net_device_ops const * const ops)
{
	notify_ops = ops;

	return 0;
}
