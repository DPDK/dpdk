/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _RTE_COMPRESSDEV_INTERNAL_H_
#define _RTE_COMPRESSDEV_INTERNAL_H_

/* rte_compressdev_internal.h
 * This file holds Compressdev private data structures.
 */
#include <rte_log.h>

#define RTE_COMPRESSDEV_NAME_MAX_LEN	(64)
/**< Max length of name of comp PMD */

/* Logging Macros */
extern int compressdev_logtype;
#define COMPRESSDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, compressdev_logtype, "%s(): "fmt "\n", \
			__func__, ##args)


/** The data structure associated with each comp device. */
struct rte_compressdev {
	struct rte_compressdev_data *data;
	/**< Pointer to device data */
	struct rte_compressdev_ops *dev_ops;
	/**< Functions exported by PMD */
	uint64_t feature_flags;
	/**< Supported features */
	struct rte_device *device;
	/**< Backing device */

	uint8_t driver_id;
	/**< comp driver identifier*/

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
