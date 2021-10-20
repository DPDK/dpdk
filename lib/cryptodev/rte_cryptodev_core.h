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
 * The pool of rte_cryptodev structures.
 */
extern struct rte_cryptodev *rte_cryptodevs;

#endif /* _RTE_CRYPTODEV_CORE_H_ */
