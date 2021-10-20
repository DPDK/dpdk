/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _RTE_CRYPTODEV_CORE_H_
#define _RTE_CRYPTODEV_CORE_H_

/**
 * @file
 *
 * RTE Crypto Device internal header.
 *
 * This header contains internal data types. But they are still part of the
 * public API because they are used by inline functions in the published API.
 *
 * Applications should not use these directly.
 *
 */

typedef uint16_t (*dequeue_pkt_burst_t)(void *qp,
		struct rte_crypto_op **ops,	uint16_t nb_ops);
/**< Dequeue processed packets from queue pair of a device. */

typedef uint16_t (*enqueue_pkt_burst_t)(void *qp,
		struct rte_crypto_op **ops,	uint16_t nb_ops);
/**< Enqueue packets for processing on queue pair of a device. */

/**
 * @internal
 * Structure used to hold opaque pointers to internal ethdev Rx/Tx
 * queues data.
 * The main purpose to expose these pointers at all - allow compiler
 * to fetch this data for fast-path cryptodev inline functions in advance.
 */
struct rte_cryptodev_qpdata {
	/** points to array of internal queue pair data pointers. */
	void **data;
	/** points to array of enqueue callback data pointers */
	struct rte_cryptodev_cb_rcu *enq_cb;
	/** points to array of dequeue callback data pointers */
	struct rte_cryptodev_cb_rcu *deq_cb;
};

struct rte_crypto_fp_ops {
	/** PMD enqueue burst function. */
	enqueue_pkt_burst_t enqueue_burst;
	/** PMD dequeue burst function. */
	dequeue_pkt_burst_t dequeue_burst;
	/** Internal queue pair data pointers. */
	struct rte_cryptodev_qpdata qp;
	/** Reserved for future ops. */
	uintptr_t reserved[3];
} __rte_cache_aligned;

extern struct rte_crypto_fp_ops rte_crypto_fp_ops[RTE_CRYPTO_MAX_DEVS];

/**
 * @internal
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_cryptodev_data {
	uint8_t dev_id;
	/**< Device ID for this instance */
	uint8_t socket_id;
	/**< Socket ID where memory is allocated */
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	__extension__
	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	struct rte_mempool *session_pool;
	/**< Session memory pool */
	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs. */

	void *dev_private;
	/**< PMD-specific private data */
} __rte_cache_aligned;


/** @internal The data structure associated with each crypto device. */
struct rte_cryptodev {
	dequeue_pkt_burst_t dequeue_burst;
	/**< Pointer to PMD receive function. */
	enqueue_pkt_burst_t enqueue_burst;
	/**< Pointer to PMD transmit function. */

	struct rte_cryptodev_data *data;
	/**< Pointer to device data */
	struct rte_cryptodev_ops *dev_ops;
	/**< Functions exported by PMD */
	uint64_t feature_flags;
	/**< Feature flags exposes HW/SW features for the given device */
	struct rte_device *device;
	/**< Backing device */

	uint8_t driver_id;
	/**< Crypto driver identifier*/

	struct rte_cryptodev_cb_list link_intr_cbs;
	/**< User application callback for interrupts if present */

	void *security_ctx;
	/**< Context for security ops */

	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */

	struct rte_cryptodev_cb_rcu *enq_cbs;
	/**< User application callback for pre enqueue processing */

	struct rte_cryptodev_cb_rcu *deq_cbs;
	/**< User application callback for post dequeue processing */
} __rte_cache_aligned;

/**
 * The pool of rte_cryptodev structures.
 */
extern struct rte_cryptodev *rte_cryptodevs;

#endif /* _RTE_CRYPTODEV_CORE_H_ */
