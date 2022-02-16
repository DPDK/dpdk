/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_VHOST_ASYNC_H_
#define _RTE_VHOST_ASYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_compat.h>
#include <rte_mbuf.h>

/**
 * Register an async channel for a vhost queue
 *
 * @param vid
 *  vhost device id async channel to be attached to
 * @param queue_id
 *  vhost queue id async channel to be attached to
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_register(int vid, uint16_t queue_id);

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
 * @return
 *  0 on success, -1 on failures
 */
__rte_experimental
int rte_vhost_async_channel_register_thread_unsafe(int vid, uint16_t queue_id);

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
 * @param dma_id
 *  the identifier of DMA device
 * @param vchan_id
 *  the identifier of virtual DMA channel
 * @return
 *  num of packets enqueued
 */
__rte_experimental
uint16_t rte_vhost_submit_enqueue_burst(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count, int16_t dma_id,
		uint16_t vchan_id);

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
 * @param dma_id
 *  the identifier of DMA device
 * @param vchan_id
 *  the identifier of virtual DMA channel
 * @return
 *  num of packets returned
 */
__rte_experimental
uint16_t rte_vhost_poll_enqueue_completed(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count, int16_t dma_id,
		uint16_t vchan_id);

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
 * @param dma_id
 *  the identifier of DMA device
 * @param vchan_id
 *  the identifier of virtual DMA channel
 * @return
 *  Number of packets returned
 */
__rte_experimental
uint16_t rte_vhost_clear_queue_thread_unsafe(int vid, uint16_t queue_id,
		struct rte_mbuf **pkts, uint16_t count, int16_t dma_id,
		uint16_t vchan_id);

/**
 * The DMA vChannels used in asynchronous data path must be configured
 * first. So this function needs to be called before enabling DMA
 * acceleration for vring. If this function fails, the given DMA vChannel
 * cannot be used in asynchronous data path.
 *
 * DMA devices used in data-path must belong to DMA devices given in this
 * function. Application is free to use DMA devices passed to this function
 * for non-vhost scenarios, but will have to ensure the Vhost library is not
 * using the channel at the same time.
 *
 * @param dma_id
 *  the identifier of DMA device
 * @param vchan_id
 *  the identifier of virtual DMA channel
 * @return
 *  0 on success, and -1 on failure
 */
__rte_experimental
int rte_vhost_async_dma_configure(int16_t dma_id, uint16_t vchan_id);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VHOST_ASYNC_H_ */
