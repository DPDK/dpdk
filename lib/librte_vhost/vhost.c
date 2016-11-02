/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2016 Intel Corporation. All rights reserved.
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
#ifdef RTE_LIBRTE_VHOST_NUMA
#include <numaif.h>
#endif

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_string_fns.h>
#include <rte_memory.h>
#include <rte_malloc.h>
#include <rte_virtio_net.h>

#include "vhost.h"

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
				(1ULL << VIRTIO_NET_F_GUEST_TSO6) | \
				(1ULL << VIRTIO_RING_F_INDIRECT_DESC))

uint64_t VHOST_FEATURES = VHOST_SUPPORTED_FEATURES;

struct virtio_net *vhost_devices[MAX_VHOST_DEVICE];

/* device ops to add/remove device to/from data core. */
struct virtio_net_device_ops const *notify_ops;

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
void
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
	struct vhost_virtqueue *rxq, *txq;

	for (i = 0; i < dev->virt_qp_nb; i++) {
		rxq = dev->virtqueue[i * VIRTIO_QNUM + VIRTIO_RXQ];
		txq = dev->virtqueue[i * VIRTIO_QNUM + VIRTIO_TXQ];

		rte_free(rxq->shadow_used_ring);
		rte_free(txq->shadow_used_ring);

		/* rxq and txq are allocated together as queue-pair */
		rte_free(rxq);
	}

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

	TAILQ_INIT(&vq->zmbuf_list);
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

int
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
void
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
		return -1;
	}

	vhost_devices[i] = dev;
	dev->vid = i;

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

void
vhost_enable_dequeue_zero_copy(int vid)
{
	struct virtio_net *dev = get_device(vid);

	if (dev == NULL)
		return;

	dev->dequeue_zero_copy = 1;
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
