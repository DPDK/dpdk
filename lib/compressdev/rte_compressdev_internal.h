/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _RTE_COMPRESSDEV_INTERNAL_H_
#define _RTE_COMPRESSDEV_INTERNAL_H_

/* rte_compressdev_internal.h
 * This file holds Compressdev private data structures.
 */
#include <rte_log.h>

#include "rte_comp.h"

#define RTE_COMPRESSDEV_NAME_MAX_LEN	(64)
/**< Max length of name of comp PMD */

/* Logging Macros */
extern int compressdev_logtype;
#define COMPRESSDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, compressdev_logtype, "%s(): "fmt "\n", \
			__func__, ##args)

/**
 * Dequeue processed packets from queue pair of a device.
 *
 * @param qp
 *   The queue pair from which to retrieve
 *   processed operations.
 * @param ops
 *   The address of an array of pointers to
 *   *rte_comp_op* structures that must be
 *   large enough to store *nb_ops* pointers in it
 * @param nb_ops
 *   The maximum number of operations to dequeue
 * @return
 *   - The number of operations actually dequeued, which is the number
 *   of pointers to *rte_comp_op* structures effectively supplied to the
 *   *ops* array.
 */
typedef uint16_t (*compressdev_dequeue_pkt_burst_t)(void *qp,
		struct rte_comp_op **ops, uint16_t nb_ops);

/**
 * Enqueue a burst of operations for processing.
 *
 * @param qp
 *   The queue pair on which operations
 *   are to be enqueued for processing
 * @param ops
 *   The address of an array of *nb_ops* pointers
 *   to *rte_comp_op* structures which contain
 *   the operations to be processed
 * @param nb_ops
 *   The number of operations to process
 * @return
 *   The number of operations actually enqueued on the device. The return
 *   value can be less than the value of the *nb_ops* parameter when the
 *   comp devices queue is full or if invalid parameters are specified in
 *   a *rte_comp_op*.
 */

typedef uint16_t (*compressdev_enqueue_pkt_burst_t)(void *qp,
		struct rte_comp_op **ops, uint16_t nb_ops);

/** The data structure associated with each comp device. */
struct rte_compressdev {
	compressdev_dequeue_pkt_burst_t dequeue_burst;
	/**< Pointer to PMD receive function */
	compressdev_enqueue_pkt_burst_t enqueue_burst;
	/**< Pointer to PMD transmit function */

	struct rte_compressdev_data *data;
	/**< Pointer to device data */
	struct rte_compressdev_ops *dev_ops;
	/**< Functions exported by PMD */
	uint64_t feature_flags;
	/**< Supported features */
	struct rte_device *device;
	/**< Backing device */

	__extension__
	uint8_t attached : 1;
	/**< Flag indicating the device is attached */
} __rte_cache_aligned;

/**
 *
 * The data part, with no function pointers, associated with each device.
 *
 * This structure is safe to place in shared memory to be common among
 * different processes in a multi-process configuration.
 */
struct rte_compressdev_data {
	uint8_t dev_id;
	/**< Compress device identifier */
	uint8_t socket_id;
	/**< Socket identifier where memory is allocated */
	char name[RTE_COMPRESSDEV_NAME_MAX_LEN];
	/**< Unique identifier name */

	__extension__
	uint8_t dev_started : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */

	void **queue_pairs;
	/**< Array of pointers to queue pairs. */
	uint16_t nb_queue_pairs;
	/**< Number of device queue pairs */

	void *dev_private;
	/**< PMD-specific private data */
} __rte_cache_aligned;
#endif
