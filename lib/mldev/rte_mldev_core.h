/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_INTERNAL_H
#define RTE_MLDEV_INTERNAL_H

/**
 * @file
 *
 * MLDEV internal header
 *
 * This file contains MLDEV private data structures and macros.
 *
 * @note
 * These APIs are for MLDEV PMDs and library only.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <dev_driver.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_mldev.h>

/* Device state */
#define ML_DEV_DETACHED (0)
#define ML_DEV_ATTACHED (1)

/**
 * @internal
 *
 * The data part, with no function pointers, associated with each device. This structure is safe to
 * place in shared memory to be common among different processes in a multi-process configuration.
 */
struct rte_ml_dev_data {
	/** Device ID for this instance. */
	int16_t dev_id;

	/** Socket ID where memory is allocated. */
	int16_t socket_id;

	/** Device state: STOPPED(0) / STARTED(1) */
	__extension__ uint8_t dev_started : 1;

	/** Number of device queue pairs. */
	uint16_t nb_queue_pairs;

	/** Number of ML models. */
	uint16_t nb_models;

	/** Array of pointers to queue pairs. */
	void **queue_pairs;

	/** Array of pointers to ML models. */
	void **models;

	/** PMD-specific private data. */
	void *dev_private;

	/** Unique identifier name. */
	char name[RTE_ML_STR_MAX];
};

/**
 * @internal
 *
 * The data structure associated with each ML device.
 */
struct rte_ml_dev {
	/** Pointer to device data. */
	struct rte_ml_dev_data *data;

	/** Backing RTE device. */
	struct rte_device *device;

	/** Flag indicating the device is attached. */
	__extension__ uint8_t attached : 1;
} __rte_cache_aligned;

/**
 * @internal
 *
 * Global structure used for maintaining state of allocated ML devices.
 */
struct rte_ml_dev_global {
	/** Device information array. */
	struct rte_ml_dev *devs;

	/** Device private data array. */
	struct rte_ml_dev_data **data;

	/** Number of devices found. */
	uint8_t nb_devs;

	/** Maximum number of devices. */
	uint8_t max_devs;
};

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_INTERNAL_H */
