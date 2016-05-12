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

#ifndef _VIRTIO_NET_H_
#define _VIRTIO_NET_H_

/**
 * @file
 * Interface to vhost net
 */

#include <stdint.h>
#include <linux/vhost.h>
#include <linux/virtio_ring.h>
#include <linux/virtio_net.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <linux/if.h>

#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_ether.h>

#define RTE_VHOST_USER_CLIENT		(1ULL << 0)
#define RTE_VHOST_USER_NO_RECONNECT	(1ULL << 1)

/* Enum for virtqueue management. */
enum {VIRTIO_RXQ, VIRTIO_TXQ, VIRTIO_QNUM};

/**
 * Device and vring operations.
 *
 * Make sure to set VIRTIO_DEV_RUNNING to the device flags in new_device and
 * remove it in destroy_device.
 *
 */
struct virtio_net_device_ops {
	int (*new_device)(int vid);		/**< Add device. */
	void (*destroy_device)(int vid);	/**< Remove device. */

	int (*vring_state_changed)(int vid, uint16_t queue_id, int enable);	/**< triggered when a vring is enabled or disabled */

	void *reserved[5]; /**< Reserved for future extension */
};

/**
 *  Disable features in feature_mask. Returns 0 on success.
 */
int rte_vhost_feature_disable(uint64_t feature_mask);

/**
 *  Enable features in feature_mask. Returns 0 on success.
 */
int rte_vhost_feature_enable(uint64_t feature_mask);

/* Returns currently supported vhost features */
uint64_t rte_vhost_feature_get(void);

int rte_vhost_enable_guest_notification(int vid, uint16_t queue_id, int enable);

/**
 * Register vhost driver. path could be different for multiple
 * instance support.
 */
int rte_vhost_driver_register(const char *path, uint64_t flags);

/* Unregister vhost driver. This is only meaningful to vhost user. */
int rte_vhost_driver_unregister(const char *path);

/* Register callbacks. */
int rte_vhost_driver_callback_register(struct virtio_net_device_ops const * const);
/* Start vhost driver session blocking loop. */
int rte_vhost_driver_session_start(void);

/**
 * Get the numa node from which the virtio net device's memory
 * is allocated.
 *
 * @param vid
 *  virtio-net device ID
 *
 * @return
 *  The numa node, -1 on failure
 */
int rte_vhost_get_numa_node(int vid);

/**
 * Get the number of queues the device supports.
 *
 * @param vid
 *  virtio-net device ID
 *
 * @return
 *  The number of queues, 0 on failure
 */
uint32_t rte_vhost_get_queue_num(int vid);

/**
 * Get the virtio net device's ifname. For vhost-cuse, ifname is the
 * path of the char device. For vhost-user, ifname is the vhost-user
 * socket file path.
 *
 * @param vid
 *  virtio-net device ID
 * @param buf
 *  The buffer to stored the queried ifname
 * @param len
 *  The length of buf
 *
 * @return
 *  0 on success, -1 on failure
 */
int rte_vhost_get_ifname(int vid, char *buf, size_t len);

/**
 * Get how many avail entries are left in the queue
 *
 * @param vid
 *  virtio-net device ID
 * @param queue_id
 *  virtio queue index
 *
 * @return
 *  num of avail entires left
 */
uint16_t rte_vhost_avail_entries(int vid, uint16_t queue_id);

/**
 * This function adds buffers to the virtio devices RX virtqueue. Buffers can
 * be received from the physical port or from another virtual device. A packet
 * count is returned to indicate the number of packets that were succesfully
 * added to the RX queue.
 * @param vid
 *  virtio-net device ID
 * @param queue_id
 *  virtio queue index in mq case
 * @param pkts
 *  array to contain packets to be enqueued
 * @param count
 *  packets num to be enqueued
 * @return
 *  num of packets enqueued
 */
uint16_t rte_vhost_enqueue_burst(int vid, uint16_t queue_id,
	struct rte_mbuf **pkts, uint16_t count);

/**
 * This function gets guest buffers from the virtio device TX virtqueue,
 * construct host mbufs, copies guest buffer content to host mbufs and
 * store them in pkts to be processed.
 * @param vid
 *  virtio-net device
 * @param queue_id
 *  virtio queue index in mq case
 * @param mbuf_pool
 *  mbuf_pool where host mbuf is allocated.
 * @param pkts
 *  array to contain packets to be dequeued
 * @param count
 *  packets num to be dequeued
 * @return
 *  num of packets dequeued
 */
uint16_t rte_vhost_dequeue_burst(int vid, uint16_t queue_id,
	struct rte_mempool *mbuf_pool, struct rte_mbuf **pkts, uint16_t count);

#endif /* _VIRTIO_NET_H_ */
