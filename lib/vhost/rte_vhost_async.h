/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_VHOST_ASYNC_H_
#define _RTE_VHOST_ASYNC_H_

#include "rte_vhost.h"

/**
 * iovec iterator
 */
struct rte_vhost_iov_iter {
	/** offset to the first byte of interesting data */
	size_t offset;
	/** total bytes of data in this iterator */
	size_t count;
	/** pointer to the iovec array */
	struct iovec *iov;
	/** number of iovec in this iterator */
	unsigned long nr_segs;
};

/**
 * dma transfer descriptor pair
 */
struct rte_vhost_async_desc {
	/** source memory iov_iter */
	struct rte_vhost_iov_iter *src;
	/** destination memory iov_iter */
	struct rte_vhost_iov_iter *dst;
};

/**
 * dma transfer status
 */
struct rte_vhost_async_status {
	/** An array of application specific data for source memory */
	uintptr_t *src_opaque_data;
	/** An array of application specific data for destination memory */
	uintptr_t *dst_opaque_data;
};

/**
 * dma operation callbacks to be implemented by applications
 */
struct rte_vhost_async_channel_ops {
	/**
	 * instruct async engines to perform copies for a batch of packets
	 *
	 * @param vid
	 *  id of vhost device to perform data copies
	 * @param queue_id
	 *  queue id to perform data copies
	 * @param descs
	 *  an array of DMA transfer memory descriptors
	 * @param opaque_data
	 *  opaque data pair sending to DMA engine
	 * @param count
	 *  number of elements in the "descs" array
	 * @return
	 *  number of descs processed, negative value means error
	 */
	int32_t (*transfer_data)(int vid, uint16_t queue_id,
		struct rte_vhost_async_desc *descs,
		struct rte_vhost_async_status *opaque_data,
		uint16_t count);
	/**
	 * check copy-completed packets from the async engine
	 * @param vid
	 *  id of vhost device to check copy completion
	 * @param queue_id
	 *  queue id to check copy completion
	 * @param opaque_data
	 *  buffer to receive the opaque data pair from DMA engine
	 * @param max_packets
	 *  max number of packets could be completed
	 * @return
	 *  number of async descs completed, negative value means error
	 */
	int32_t (*check_completed_copies)(int vid, uint16_t queue_id,
		struct rte_vhost_async_status *opaque_data,
		uint16_t max_packets);
};

/**
 * inflight async packet information
 */
struct async_inflight_info {
	struct rte_mbuf *mbuf;
	uint16_t descs; /* num of descs inflight */
	uint16_t nr_buffers; /* num of buffers inflight for packed ring */
};

/**
 *  async channel features
 */
enum {
	RTE_VHOST_ASYNC_INORDER = 1U << 0,
};

/**
 *  async channel configuration
 */
struct rte_vhost_async_config {
	uint32_t async_threshold;
	uint32_t features;
	uint32_t rsvd[2];
};

/**
 * Register an async channel for a vhost queue
 *
 * @param vid
 *  vhost device id async channel to be attached to
 * @param queue_id
 *  vhost queue id async channel to be attached to
 * @param config
 *  Async channel configuration structure
 * @param ops
 *  Async channel operation callbacks
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_register(int vid, uint16_t queue_id,
	struct rte_vhost_async_config config,
	struct rte_vhost_async_channel_ops *ops);

/**
 * Unregister an async channel for a vhost queue
 *
 * @param vid
 *  vhost device id async channel to be detached from
 * @param queue_id
 *  vhost queue id async channel to be detached from
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_unregister(int vid, uint16_t queue_id);

/**
 * Register an async channel for a vhost queue without performing any
 * locking
 *
 * @note This function does not perform any locking, and is only safe to
 *       call in vhost callback functions.
 *
 * @param vid
 *  vhost device id async channel to be attached to
 * @param queue_id
 *  vhost queue id async channel to be attached to
 * @param config
 *  Async channel configuration
 * @param ops
 *  Async channel operation callbacks
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_register_thread_unsafe(int vid, uint16_t queue_id,
	struct rte_vhost_async_config config,
	struct rte_vhost_async_channel_ops *ops);

/**
 * Unregister an async channel for a vhost queue without performing any
 * locking
 *
 * @note This function does not perform any locking, and is only safe to
 *       call in vhost callback functions.
 *
 * @param vid
 *  vhost device id async channel to be detached from
 * @param queue_id
 *  vhost queue id async channel to be detached from
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_unregister_thread_unsafe(int vid,
		uint16_t queue_id);

/**
 * This function submits enqueue data to async engine. Successfully
 * enqueued packets can be transfer completed or being occupied by DMA
 * engines, when this API returns. Transfer completed packets are returned
 * in comp_pkts, so users need to guarantee its size is greater than or
 * equal to the size of pkts; for packets that are successfully enqueued
 * but not transfer completed, users should poll transfer status by
 * rte_vhost_poll_enqueue_completed().
 *
 * @param vid
 *  id of vhost device to enqueue data
 * @param queue_id
 *  queue id to enqueue data
 * @param pkts
 *  array of packets to be enqueued
 * @param count
 *  packets num to be enqueued
 * @param comp_pkts
 *  empty array to get transfer completed packets. Users need to
 *  guarantee its size is greater than or equal to that of pkts
 * @param comp_count
 *  num of packets that are transfer completed, when this API returns.
 *  If no packets are transfer completed, its value is set to 0.
 * @return
 *  num of packets enqueued, including in-flight and transfer completed
 */
__rte_experimental
uint16_t rte_vhost_submit_enqueue_burst(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count,
		struct rte_mbuf **comp_pkts, uint32_t *comp_count);

/**
 * This function checks async completion status for a specific vhost
 * device queue. Packets which finish copying (enqueue) operation
 * will be returned in an array.
 *
 * @param vid
 *  id of vhost device to enqueue data
 * @param queue_id
 *  queue id to enqueue data
 * @param pkts
 *  blank array to get return packet pointer
 * @param count
 *  size of the packet array
 * @return
 *  num of packets returned
 */
__rte_experimental
uint16_t rte_vhost_poll_enqueue_completed(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count);

/**
 * This function returns the amount of in-flight packets for the vhost
 * queue which uses async channel acceleration.
 *
 * @param vid
 *  id of vhost device to enqueue data
 * @param queue_id
 *  queue id to enqueue data
 * @return
 *  the amount of in-flight packets on success; -1 on failure
 */
__rte_experimental
int rte_vhost_async_get_inflight(int vid, uint16_t queue_id);

/**
 * This function checks async completion status and clear packets for
 * a specific vhost device queue. Packets which are inflight will be
 * returned in an array.
 *
 * @note This function does not perform any locking
 *
 * @param vid
 *  ID of vhost device to clear data
 * @param queue_id
 *  Queue id to clear data
 * @param pkts
 *  Blank array to get return packet pointer
 * @param count
 *  Size of the packet array
 * @return
 *  Number of packets returned
 */
__rte_experimental
uint16_t rte_vhost_clear_queue_thread_unsafe(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count);

#endif /* _RTE_VHOST_ASYNC_H_ */
