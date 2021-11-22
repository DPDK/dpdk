/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_BBDEV_PMD_H_
#define _RTE_BBDEV_PMD_H_

/**
 * @file rte_bbdev_pmd.h
 *
 * Wireless base band driver-facing APIs.
 *
 * This API provides the mechanism for device drivers to register with the
 * bbdev interface. User applications should not use this API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <rte_log.h>

#include "rte_bbdev.h"

/** Suggested value for SW based devices */
#define RTE_BBDEV_DEFAULT_MAX_NB_QUEUES RTE_MAX_LCORE

/** Suggested value for SW based devices */
#define RTE_BBDEV_QUEUE_SIZE_LIMIT 16384

/**
 * @internal
 * Allocates a new slot for a bbdev and returns the pointer to that slot
 * for the driver to use.
 *
 * @param name
 *   Unique identifier name for each bbdev device
 *
 * @return
 *   - Slot in the rte_bbdev array for a new device;
 */
struct rte_bbdev *
rte_bbdev_allocate(const char *name);

/**
 * @internal
 * Release the specified bbdev.
 *
 * @param bbdev
 *   The *bbdev* pointer is the address of the *rte_bbdev* structure.
 * @return
 *   - 0 on success, negative on error
 */
int
rte_bbdev_release(struct rte_bbdev *bbdev);

/**
 * Get the device structure for a named device.
 *
 * @param name
 *   Name of the device
 *
 * @return
 *   - The device structure pointer, or
 *   - NULL otherwise
 *
 */
struct rte_bbdev *
rte_bbdev_get_named_dev(const char *name);

/**
 * Definitions of all functions exported by a driver through the generic
 * structure of type *rte_bbdev_ops* supplied in the *rte_bbdev* structure
 * associated with a device.
 */

/** @internal Function used to configure device memory. */
typedef int (*rte_bbdev_setup_queues_t)(struct rte_bbdev *dev,
		uint16_t num_queues, int socket_id);

/** @internal Function used to configure interrupts for a device. */
typedef int (*rte_bbdev_intr_enable_t)(struct rte_bbdev *dev);

/** @internal Function to allocate and configure a device queue. */
typedef int (*rte_bbdev_queue_setup_t)(struct rte_bbdev *dev,
		uint16_t queue_id, const struct rte_bbdev_queue_conf *conf);

/*
 * @internal
 * Function to release memory resources allocated for a device queue.
 */
typedef int (*rte_bbdev_queue_release_t)(struct rte_bbdev *dev,
		uint16_t queue_id);

/** @internal Function to start a configured device. */
typedef int (*rte_bbdev_start_t)(struct rte_bbdev *dev);

/** @internal Function to stop a device. */
typedef void (*rte_bbdev_stop_t)(struct rte_bbdev *dev);

/** @internal Function to close a device. */
typedef int (*rte_bbdev_close_t)(struct rte_bbdev *dev);

/** @internal Function to start a device queue. */
typedef int (*rte_bbdev_queue_start_t)(struct rte_bbdev *dev,
		uint16_t queue_id);

/** @internal Function to stop a device queue. */
typedef int (*rte_bbdev_queue_stop_t)(struct rte_bbdev *dev, uint16_t queue_id);

/** @internal Function to read stats from a device. */
typedef void (*rte_bbdev_stats_get_t)(struct rte_bbdev *dev,
		struct rte_bbdev_stats *stats);

/** @internal Function to reset stats on a device. */
typedef void (*rte_bbdev_stats_reset_t)(struct rte_bbdev *dev);

/** @internal Function to retrieve specific information of a device. */
typedef void (*rte_bbdev_info_get_t)(struct rte_bbdev *dev,
		struct rte_bbdev_driver_info *dev_info);

/*
 * @internal
 * Function to enable interrupt for next op on a queue of a device.
 */
typedef int (*rte_bbdev_queue_intr_enable_t)(struct rte_bbdev *dev,
				    uint16_t queue_id);

/*
 * @internal
 * Function to disable interrupt for next op on a queue of a device.
 */
typedef int (*rte_bbdev_queue_intr_disable_t)(struct rte_bbdev *dev,
				    uint16_t queue_id);

/**
 * Operations implemented by drivers. Fields marked as "Required" must be
 * provided by a driver for a device to have basic functionality. "Optional"
 * fields are for non-vital operations
 */
struct rte_bbdev_ops {
	/** Allocate and configure device memory. Optional. */
	rte_bbdev_setup_queues_t setup_queues;
	/** Configure interrupts. Optional. */
	rte_bbdev_intr_enable_t intr_enable;
	/** Start device. Optional. */
	rte_bbdev_start_t start;
	/** Stop device. Optional. */
	rte_bbdev_stop_t stop;
	/** Close device. Optional. */
	rte_bbdev_close_t close;

	/** Get device info. Required. */
	rte_bbdev_info_get_t info_get;
	/** Get device statistics. Optional. */
	rte_bbdev_stats_get_t stats_get;
	/** Reset device statistics. Optional. */
	rte_bbdev_stats_reset_t stats_reset;

	/** Set up a device queue. Required. */
	rte_bbdev_queue_setup_t queue_setup;
	/** Release a queue. Required. */
	rte_bbdev_queue_release_t queue_release;
	/** Start a queue. Optional. */
	rte_bbdev_queue_start_t queue_start;
	/** Stop a queue pair. Optional. */
	rte_bbdev_queue_stop_t queue_stop;

	/** Enable queue interrupt. Optional */
	rte_bbdev_queue_intr_enable_t queue_intr_enable;
	/** Disable queue interrupt. Optional */
	rte_bbdev_queue_intr_disable_t queue_intr_disable;
};

/**
 * Executes all the user application registered callbacks for the specific
 * device and event type.
 *
 * @param dev
 *   Pointer to the device structure.
 * @param event
 *   Event type.
 * @param ret_param
 *   To pass data back to user application.
 */
void
rte_bbdev_pmd_callback_process(struct rte_bbdev *dev,
	enum rte_bbdev_event_type event, void *ret_param);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BBDEV_PMD_H_ */
