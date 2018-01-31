/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _RTE_RAWDEV_PMD_H_
#define _RTE_RAWDEV_PMD_H_

/** @file
 * RTE RAW PMD APIs
 *
 * @note
 * Driver facing APIs for a raw device. These are not to be called directly by
 * any application.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_common.h>

#include "rte_rawdev.h"

extern int librawdev_logtype;

/* Logging Macros */
#define RTE_RDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, librawdev_logtype, "%s(): " fmt "\n", \
		__func__, ##args)

#define RTE_RDEV_ERR(fmt, args...) \
	RTE_RDEV_LOG(ERR, fmt, ## args)
#define RTE_RDEV_DEBUG(fmt, args...) \
	RTE_RDEV_LOG(DEBUG, fmt, ## args)
#define RTE_RDEV_INFO(fmt, args...) \
	RTE_RDEV_LOG(INFO, fmt, ## args)


/* Macros to check for valid device */
#define RTE_RAWDEV_VALID_DEVID_OR_ERR_RET(dev_id, retval) do { \
	if (!rte_rawdev_pmd_is_valid_dev((dev_id))) { \
		RTE_RDEV_ERR("Invalid dev_id=%d", dev_id); \
		return retval; \
	} \
} while (0)

#define RTE_RAWDEV_VALID_DEVID_OR_RET(dev_id) do { \
	if (!rte_rawdev_pmd_is_valid_dev((dev_id))) { \
		RTE_RDEV_ERR("Invalid dev_id=%d", dev_id); \
		return; \
	} \
} while (0)

#define RTE_RAWDEV_DETACHED  (0)
#define RTE_RAWDEV_ATTACHED  (1)

/* Global structure used for maintaining state of allocated raw devices.
 *
 * TODO: Can be expanded to <type of raw device>:<count> in future.
 *       Applications should be able to select from a number of type of raw
 *       devices which were detected or attached to this DPDK instance.
 */
struct rte_rawdev_global {
	/**< Number of devices found */
	uint16_t nb_devs;
};

extern struct rte_rawdev_global *rte_rawdev_globals;
/** Pointer to global raw devices data structure. */
extern struct rte_rawdev *rte_rawdevs;
/** The pool of rte_rawdev structures. */

/**
 * Get the rte_rawdev structure device pointer for the named device.
 *
 * @param name
 *   device name to select the device structure.
 *
 * @return
 *   - The rte_rawdev structure pointer for the given device ID.
 */
static inline struct rte_rawdev *
rte_rawdev_pmd_get_named_dev(const char *name)
{
	struct rte_rawdev *dev;
	unsigned int i;

	if (name == NULL)
		return NULL;

	for (i = 0; i < RTE_RAWDEV_MAX_DEVS; i++) {
		dev = &rte_rawdevs[i];
		if ((dev->attached == RTE_RAWDEV_ATTACHED) &&
		   (strcmp(dev->name, name) == 0))
			return dev;
	}

	return NULL;
}

/**
 * Validate if the raw device index is a valid attached raw device.
 *
 * @param dev_id
 *   raw device index.
 *
 * @return
 *   - If the device index is valid (1) or not (0).
 */
static inline unsigned
rte_rawdev_pmd_is_valid_dev(uint8_t dev_id)
{
	struct rte_rawdev *dev;

	if (dev_id >= RTE_RAWDEV_MAX_DEVS)
		return 0;

	dev = &rte_rawdevs[dev_id];
	if (dev->attached != RTE_RAWDEV_ATTACHED)
		return 0;
	else
		return 1;
}

/**
 * Definitions of all functions exported by a driver through the
 * the generic structure of type *rawdev_ops* supplied in the
 * *rte_rawdev* structure associated with a device.
 */

/**
 * Get device information of a device.
 *
 * @param dev
 *   Raw device pointer
 * @param dev_info
 *   Raw device information structure
 *
 * @return
 *   Returns 0 on success
 */
typedef void (*rawdev_info_get_t)(struct rte_rawdev *dev,
				  rte_rawdev_obj_t dev_info);

/**
 * Configure a device.
 *
 * @param dev
 *   Raw device pointer
 * @param config
 *   Void object containing device specific configuration
 *
 * @return
 *   Returns 0 on success
 */
typedef int (*rawdev_configure_t)(const struct rte_rawdev *dev,
				  rte_rawdev_obj_t config);

/**
 * Start a configured device.
 *
 * @param dev
 *   Raw device pointer
 *
 * @return
 *   Returns 0 on success
 */
typedef int (*rawdev_start_t)(struct rte_rawdev *dev);

/**
 * Stop a configured device.
 *
 * @param dev
 *   Raw device pointer
 */
typedef void (*rawdev_stop_t)(struct rte_rawdev *dev);

/**
 * Close a configured device.
 *
 * @param dev
 *   Raw device pointer
 *
 * @return
 * - 0 on success
 * - (-EAGAIN) if can't close as device is busy
 */
typedef int (*rawdev_close_t)(struct rte_rawdev *dev);

/**
 * Reset a configured device.
 *
 * @param dev
 *   Raw device pointer
 * @return
 *   0 for success
 *   !0 for failure
 */
typedef int (*rawdev_reset_t)(struct rte_rawdev *dev);

/**
 * Retrieve the current raw queue configuration.
 *
 * @param dev
 *   Raw device pointer
 * @param queue_id
 *   Raw device queue index
 * @param[out] queue_conf
 *   Raw device queue configuration structure
 *
 */
typedef void (*rawdev_queue_conf_get_t)(struct rte_rawdev *dev,
					uint16_t queue_id,
					rte_rawdev_obj_t queue_conf);

/**
 * Setup an raw queue.
 *
 * @param dev
 *   Raw device pointer
 * @param queue_id
 *   Rawqueue index
 * @param queue_conf
 *   Rawqueue configuration structure
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*rawdev_queue_setup_t)(struct rte_rawdev *dev,
				    uint16_t queue_id,
				    rte_rawdev_obj_t queue_conf);

/**
 * Release resources allocated by given raw queue.
 *
 * @param dev
 *   Raw device pointer
 * @param queue_id
 *   Raw queue index
 *
 */
typedef int (*rawdev_queue_release_t)(struct rte_rawdev *dev,
				      uint16_t queue_id);

/**
 * Dump internal information
 *
 * @param dev
 *   Raw device pointer
 * @param f
 *   A pointer to a file for output
 * @return
 *   0 for success,
 *   !0 Error
 *
 */
typedef int (*rawdev_dump_t)(struct rte_rawdev *dev, FILE *f);

/** Rawdevice operations function pointer table */
struct rte_rawdev_ops {
	/**< Get device info. */
	rawdev_info_get_t dev_info_get;
	/**< Configure device. */
	rawdev_configure_t dev_configure;
	/**< Start device. */
	rawdev_start_t dev_start;
	/**< Stop device. */
	rawdev_stop_t dev_stop;
	/**< Close device. */
	rawdev_close_t dev_close;
	/**< Reset device. */
	rawdev_reset_t dev_reset;

	/**< Get raw queue configuration. */
	rawdev_queue_conf_get_t queue_def_conf;
	/**< Set up an raw queue. */
	rawdev_queue_setup_t queue_setup;
	/**< Release an raw queue. */
	rawdev_queue_release_t queue_release;

	/* Dump internal information */
	rawdev_dump_t dump;
};

/**
 * Allocates a new rawdev slot for an raw device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param name
 *   Unique identifier name for each device
 * @param dev_private_size
 *   Private data allocated within rte_rawdev object.
 * @param socket_id
 *   Socket to allocate resources on.
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
struct rte_rawdev * __rte_experimental
rte_rawdev_pmd_allocate(const char *name, size_t dev_private_size,
			int socket_id);

/**
 * Release the specified rawdev device.
 *
 * @param rawdev
 * The *rawdev* pointer is the address of the *rte_rawdev* structure.
 * @return
 *   - 0 on success, negative on error
 */
int __rte_experimental
rte_rawdev_pmd_release(struct rte_rawdev *rawdev);

/**
 * Creates a new raw device and returns the pointer to that device.
 *
 * @param name
 *   Pointer to a character array containing name of the device
 * @param dev_private_size
 *   Size of raw PMDs private data
 * @param socket_id
 *   Socket to allocate resources on.
 *
 * @return
 *   - Raw device pointer if device is successfully created.
 *   - NULL if device cannot be created.
 */
struct rte_rawdev * __rte_experimental
rte_rawdev_pmd_init(const char *name, size_t dev_private_size,
		    int socket_id);

/**
 * Destroy a raw device
 *
 * @param name
 *   Name of the device
 * @return
 *   - 0 on success, negative on error
 */
int __rte_experimental
rte_rawdev_pmd_uninit(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RAWDEV_PMD_H_ */
