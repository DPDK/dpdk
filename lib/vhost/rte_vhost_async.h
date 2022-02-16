/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_VHOST_ASYNC_H_
#define _RTE_VHOST_ASYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "rte_vhost.h"

/**
 * iovec
 */
struct rte_vhost_iovec {
	void *src_addr;
	void *dst_addr;
	size_t len;
};

/**
 * iovec iterator
 */
struct rte_vhost_iov_iter {
	/** pointer to the iovec array */
	struct rte_vhost_iovec *iov;
	/** number of iovec in this iterator */
	unsigned long nr_segs;
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
	 * @param iov_iter
	 *  an array of IOV iterators
	 * @param opaque_data
	 *  opaque data pair sending to DMA engine
	 * @param count
	 *  number of elements in the "descs" array
	 * @return
	 *  number of IOV iterators processed, negative value means error
	 */
	int32_t (*transfer_data)(int vid, uint16_t queue_id,
		struct rte_vhost_iov_iter *iov_iter,
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
 *  async channel features
 */
enum {
	RTE_VHOST_ASYNC_INORDER = 1U << 0,
};

/**
 *  async channel configuration
 */
struct rte_vhost_async_config {
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
 * This function submits enqueue packets to async copy engine. Users
 * need to poll transfer status by rte_vhost_poll_enqueue_completed()
 * for successfully enqueued packets.
 *
 * @param vid
 *  id of vhost device to enqueue data
 * @param queue_id
 *  queue id to enqueue data
 * @param pkts
 *  array of packets to be enqueued
 * @param count
 *  packets num to be enqueued
 * @return
 *  num of packets enqueued
 */
__rte_experimental
uint16_t rte_vhost_submit_enqueue_burst(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count);

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

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VHOST_ASYNC_H_ */
