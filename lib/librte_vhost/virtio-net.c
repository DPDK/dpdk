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
#include <assert.h>
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

#define MAX_VHOST_DEVICE	1024
static struct virtio_net *vhost_devices[MAX_VHOST_DEVICE];

/* device ops to add/remove device to/from data core. */
struct virtio_net_device_ops const *notify_ops;

#define VHOST_USER_F_PROTOCOL_FEATURES	30

/* Features supported by this lib. */
#define VHOST_SUPPORTED_FEATURES ((1ULL << VIRTIO_NET_F_MRG_RXBUF) | \
				(1ULL << VIRTIO_NET_F_CTRL_VQ) | \
				(1ULL << VIRTIO_NET_F_CTRL_RX) | \
				(1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) | \
				(VHOST_SUPPORTS_MQ)            | \
				(1ULL << VIRTIO_F_VERSION_1)   | \
				(1ULL << VHOST_F_LOG_ALL)      | \
				(1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
				(1ULL << VIRTIO_NET_F_HOST_TSO4) | \
				(1ULL << VIRTIO_NET_F_HOST_TSO6) | \
				(1ULL << VIRTIO_NET_F_CSUM)    | \
				(1ULL << VIRTIO_NET_F_GUEST_CSUM) | \
				(1ULL << VIRTIO_NET_F_GUEST_TSO4) | \
				(1ULL << VIRTIO_NET_F_GUEST_TSO6))

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

struct virtio_net *
get_device(int vid)
{
	struct virtio_net *dev = vhost_devices[vid];

	if (unlikely(!dev)) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) device not found.\n", vid);
	}

	return dev;
}

static void
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
static void
cleanup_device(struct virtio_net *dev, int destroy)
{
	uint32_t i;

	vhost_backend_cleanup(dev);

	for (i = 0; i < dev->virt_qp_nb; i++) {
		cleanup_vq(dev->virtqueue[i * VIRTIO_QNUM + VIRTIO_RXQ], destroy);
		cleanup_vq(dev->virtqueue[i * VIRTIO_QNUM + VIRTIO_TXQ], destroy);
	}
}

/*
 * Release virtqueues and device memory.
 */
static void
free_device(struct virtio_net *dev)
{
	uint32_t i;

	for (i = 0; i < dev->virt_qp_nb; i++)
		rte_free(dev->virtqueue[i * VIRTIO_QNUM]);

	rte_free(dev);
}

static void
init_vring_queue(struct vhost_virtqueue *vq, int qp_idx)
{
	memset(vq, 0, sizeof(struct vhost_virtqueue));

	vq->kickfd = VIRTIO_UNINITIALIZED_EVENTFD;
	vq->callfd = VIRTIO_UNINITIALIZED_EVENTFD;

	/* Backends are set to -1 indicating an inactive device. */
	vq->backend = -1;

	/* always set the default vq pair to enabled */
	if (qp_idx == 0)
		vq->enabled = 1;
}

static void
init_vring_queue_pair(struct virtio_net *dev, uint32_t qp_idx)
{
	uint32_t base_idx = qp_idx * VIRTIO_QNUM;

	init_vring_queue(dev->virtqueue[base_idx + VIRTIO_RXQ], qp_idx);
	init_vring_queue(dev->virtqueue[base_idx + VIRTIO_TXQ], qp_idx);
}

static void
reset_vring_queue(struct vhost_virtqueue *vq, int qp_idx)
{
	int callfd;

	callfd = vq->callfd;
	init_vring_queue(vq, qp_idx);
	vq->callfd = callfd;
}

static void
reset_vring_queue_pair(struct virtio_net *dev, uint32_t qp_idx)
{
	uint32_t base_idx = qp_idx * VIRTIO_QNUM;

	reset_vring_queue(dev->virtqueue[base_idx + VIRTIO_RXQ], qp_idx);
	reset_vring_queue(dev->virtqueue[base_idx + VIRTIO_TXQ], qp_idx);
}

static int
alloc_vring_queue_pair(struct virtio_net *dev, uint32_t qp_idx)
{
	struct vhost_virtqueue *virtqueue = NULL;
	uint32_t virt_rx_q_idx = qp_idx * VIRTIO_QNUM + VIRTIO_RXQ;
	uint32_t virt_tx_q_idx = qp_idx * VIRTIO_QNUM + VIRTIO_TXQ;

	virtqueue = rte_malloc(NULL,
			       sizeof(struct vhost_virtqueue) * VIRTIO_QNUM, 0);
	if (virtqueue == NULL) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"Failed to allocate memory for virt qp:%d.\n", qp_idx);
		return -1;
	}

	dev->virtqueue[virt_rx_q_idx] = virtqueue;
	dev->virtqueue[virt_tx_q_idx] = virtqueue + VIRTIO_TXQ;

	init_vring_queue_pair(dev, qp_idx);

	dev->virt_qp_nb += 1;

	return 0;
}

/*
 * Reset some variables in device structure, while keeping few
 * others untouched, such as vid, ifname, virt_qp_nb: they
 * should be same unless the device is removed.
 */
static void
reset_device(struct virtio_net *dev)
{
	uint32_t i;

	dev->features = 0;
	dev->protocol_features = 0;
	dev->flags = 0;

	for (i = 0; i < dev->virt_qp_nb; i++)
		reset_vring_queue_pair(dev, i);
}

/*
 * Function is called from the CUSE open function. The device structure is
 * initialised and a new entry is added to the device configuration linked
 * list.
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
		return -1;
	}

	vhost_devices[i] = dev;
	dev->vid = i;

	return i;
}

/*
 * Function is called from the CUSE release function. This function will
 * cleanup the device and remove it from device configuration linked list.
 */
void
vhost_destroy_device(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		notify_ops->destroy_device(vid);
	}

	cleanup_device(dev, 1);
	free_device(dev);

	vhost_devices[vid] = NULL;
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


/*
 * Called from CUSE IOCTL: VHOST_SET_OWNER
 * This function just returns success at the moment unless
 * the device hasn't been initialised.
 */
int
vhost_set_owner(int vid)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_RESET_OWNER
 */
int
vhost_reset_owner(int vid)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	if (dev->flags & VIRTIO_DEV_RUNNING) {
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		notify_ops->destroy_device(vid);
	}

	cleanup_device(dev, 0);
	reset_device(dev);
	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_FEATURES
 * The features that we support are requested.
 */
int
vhost_get_features(int vid, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(vid);
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
int
vhost_set_features(int vid, uint64_t *pu)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;
	if (*pu & ~VHOST_FEATURES)
		return -1;

	dev->features = *pu;
	if (dev->features &
		((1 << VIRTIO_NET_F_MRG_RXBUF) | (1ULL << VIRTIO_F_VERSION_1))) {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else {
		dev->vhost_hlen = sizeof(struct virtio_net_hdr);
	}
	LOG_DEBUG(VHOST_CONFIG,
		"(%d) mergeable RX buffers %s, virtio 1 %s\n",
		dev->vid,
		(dev->features & (1 << VIRTIO_NET_F_MRG_RXBUF)) ? "on" : "off",
		(dev->features & (1ULL << VIRTIO_F_VERSION_1)) ? "on" : "off");

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_NUM
 * The virtio device sends us the size of the descriptor ring.
 */
int
vhost_set_vring_num(int vid, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->size = state->num;

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
	int ret;

	/*
	 * vq is allocated on pairs, we should try to do realloc
	 * on first queue of one queue pair only.
	 */
	if (index % VIRTIO_QNUM != 0)
		return dev;

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
		vq = rte_malloc_socket(NULL, sizeof(*vq) * VIRTIO_QNUM, 0,
				       newnode);
		if (!vq)
			return dev;

		memcpy(vq, old_vq, sizeof(*vq) * VIRTIO_QNUM);
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
	dev->virtqueue[index + 1] = vq + 1;
	vhost_devices[dev->vid] = dev;

	return dev;
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
int
vhost_set_vring_addr(int vid, struct vhost_vring_addr *addr)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if ((dev == NULL) || (dev->mem == NULL))
		return -1;

	/* addr->index refers to the queue index. The txq 1, rxq is 0. */
	vq = dev->virtqueue[addr->index];

	/* The addresses are converted from QEMU virtual to Vhost virtual. */
	vq->desc = (struct vring_desc *)(uintptr_t)qva_to_vva(dev,
			addr->desc_user_addr);
	if (vq->desc == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to find desc ring address.\n",
			dev->vid);
		return -1;
	}

	dev = numa_realloc(dev, addr->index);
	vq = dev->virtqueue[addr->index];

	vq->avail = (struct vring_avail *)(uintptr_t)qva_to_vva(dev,
			addr->avail_user_addr);
	if (vq->avail == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to find avail ring address.\n",
			dev->vid);
		return -1;
	}

	vq->used = (struct vring_used *)(uintptr_t)qva_to_vva(dev,
			addr->used_user_addr);
	if (vq->used == 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"(%d) failed to find used ring address.\n",
			dev->vid);
		return -1;
	}

	if (vq->last_used_idx != vq->used->idx) {
		RTE_LOG(WARNING, VHOST_CONFIG,
			"last_used_idx (%u) and vq->used->idx (%u) mismatches; "
			"some packets maybe resent for Tx and dropped for Rx\n",
			vq->last_used_idx, vq->used->idx);
		vq->last_used_idx     = vq->used->idx;
	}

	vq->log_guest_addr = addr->log_guest_addr;

	LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address desc: %p\n",
			dev->vid, vq->desc);
	LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address avail: %p\n",
			dev->vid, vq->avail);
	LOG_DEBUG(VHOST_CONFIG, "(%d) mapped address used: %p\n",
			dev->vid, vq->used);
	LOG_DEBUG(VHOST_CONFIG, "(%d) log_guest_addr: %" PRIx64 "\n",
			dev->vid, vq->log_guest_addr);

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_BASE
 * The virtio device sends us the available ring last used index.
 */
int
vhost_set_vring_base(int vid, struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	/* State->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[state->index]->last_used_idx = state->num;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_GET_VRING_BASE
 * We send the virtio device our available ring last used index.
 */
int
vhost_get_vring_base(int vid, uint32_t index,
	struct vhost_vring_state *state)
{
	struct virtio_net *dev;

	dev = get_device(vid);
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
int
vhost_set_vring_call(int vid, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;
	uint32_t cur_qp_idx = file->index / VIRTIO_QNUM;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	/*
	 * FIXME: VHOST_SET_VRING_CALL is the first per-vring message
	 * we get, so we do vring queue pair allocation here.
	 */
	if (cur_qp_idx + 1 > dev->virt_qp_nb) {
		if (alloc_vring_queue_pair(dev, cur_qp_idx) < 0)
			return -1;
	}

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];
	assert(vq != NULL);

	if (vq->callfd >= 0)
		close(vq->callfd);

	vq->callfd = file->fd;

	return 0;
}

/*
 * Called from CUSE IOCTL: VHOST_SET_VRING_KICK
 * The virtio device sends an eventfd that it can use to notify us.
 * This fd gets copied into our process space.
 */
int
vhost_set_vring_kick(int vid, struct vhost_vring_file *file)
{
	struct virtio_net *dev;
	struct vhost_virtqueue *vq;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	vq = dev->virtqueue[file->index];

	if (vq->kickfd >= 0)
		close(vq->kickfd);

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
int
vhost_set_backend(int vid, struct vhost_vring_file *file)
{
	struct virtio_net *dev;

	dev = get_device(vid);
	if (dev == NULL)
		return -1;

	/* file->index refers to the queue index. The txq is 1, rxq is 0. */
	dev->virtqueue[file->index]->backend = file->fd;

	/*
	 * If the device isn't already running and both backend fds are set,
	 * we add the device.
	 */
	if (!(dev->flags & VIRTIO_DEV_RUNNING)) {
		if (dev->virtqueue[VIRTIO_TXQ]->backend != VIRTIO_DEV_STOPPED &&
		    dev->virtqueue[VIRTIO_RXQ]->backend != VIRTIO_DEV_STOPPED) {
			if (notify_ops->new_device(vid) < 0)
				return -1;
			dev->flags |= VIRTIO_DEV_RUNNING;
		}
	} else if (file->fd == VIRTIO_DEV_STOPPED) {
		dev->flags &= ~VIRTIO_DEV_RUNNING;
		notify_ops->destroy_device(vid);
	}

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
			"(%d) failed to query numa node: %d\n", vid, ret);
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

	return dev->virt_qp_nb;
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

	if (dev == NULL)
		return -1;

	if (enable) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"guest notification isn't supported.\n");
		return -1;
	}

	dev->virtqueue[queue_id]->used->flags = VRING_USED_F_NO_NOTIFY;
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
