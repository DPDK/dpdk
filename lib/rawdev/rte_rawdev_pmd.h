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
 * @param dev_private_size
 *   The size of the structure pointed to by dev_info->dev_private
 *
 * @return
 *   Returns 0 on success, negative error code on failure
 */
typedef int (*rawdev_info_get_t)(struct rte_rawdev *dev,
				  rte_rawdev_obj_t dev_info,
				  size_t dev_private_size);

/**
 * Configure a device.
 *
 * @param dev
 *   Raw device pointer
 * @param config
 *   Void object containing device specific configuration
 * @param config_size
 *   Size of the memory allocated for the configuration
 *
 * @return
 *   Returns 0 on success
 */
typedef int (*rawdev_configure_t)(const struct rte_rawdev *dev,
				  rte_rawdev_obj_t config,
				  size_t config_size);

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
 * @param queue_conf_size
 *   Size of the memory allocated for the configuration
 *
 * @return
 *   Returns 0 on success, negative errno on failure
 */
typedef int (*rawdev_queue_conf_get_t)(struct rte_rawdev *dev,
					uint16_t queue_id,
					rte_rawdev_obj_t queue_conf,
					size_t queue_conf_size);

/**
 * Setup an raw queue.
 *
 * @param dev
 *   Raw device pointer
 * @param queue_id
 *   Rawqueue index
 * @param queue_conf
 *   Rawqueue configuration structure
 * @param queue_conf_size
 *   Size of the memory allocated for the configuration
 *
 * @return
 *   Returns 0 on success.
 */
typedef int (*rawdev_queue_setup_t)(struct rte_rawdev *dev,
				    uint16_t queue_id,
				    rte_rawdev_obj_t queue_conf,
				    size_t queue_conf_size);

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
 * Get the count of number of queues configured on this device.
 *
 * Another way to fetch this information is to fetch the device configuration.
 * But, that assumes that the device configuration managed by the driver has
 * that kind of information.
 *
 * This function helps in getting queue count supported, independently. It
 * can help in cases where iterator needs to be implemented.
 *
 * @param dev
 *   Raw device pointer
 * @return
 *   Number of queues; 0 is assumed to be a valid response.
 *
 */
typedef uint16_t (*rawdev_queue_count_t)(struct rte_rawdev *dev);

/**
 * Enqueue an array of raw buffers to the device.
 *
 * Buffer being used is opaque - it can be obtained from mempool or from
 * any other source. Interpretation of buffer is responsibility of driver.
 *
 * @param dev
 *   Raw device pointer
 * @param buffers
 *   array of buffers
 * @param count
 *   number of buffers passed
 * @param context
 *   an opaque object representing context of the call; for example, an
 *   application can pass information about the queues on which enqueue needs
 *   to be done. Or, the enqueue operation might be passed reference to an
 *   object containing a callback (agreed upon between application and driver).
 *
 * @return
 *   >=0 Count of buffers successfully enqueued (0: no buffers enqueued)
 *   <0 Error count in case of error
 */
typedef int (*rawdev_enqueue_bufs_t)(struct rte_rawdev *dev,
				     struct rte_rawdev_buf **buffers,
				     unsigned int count,
				     rte_rawdev_obj_t context);

/**
 * Dequeue an array of raw buffers from the device.
 *
 * @param dev
 *   Raw device pointer
 * @param buffers
 *   array of buffers
 * @param count
 *   Max buffers expected to be dequeued
 * @param context
 *   an opaque object representing context of the call. Based on this object,
 *   the application and driver can coordinate for dequeue operation involving
 *   agreed upon semantics. For example, queue information/id on which Dequeue
 *   needs to be performed.
 * @return
 *   >0, ~0: Count of buffers returned
 *   <0: Error
 *   Whether short dequeue is success or failure is decided between app and
 *   driver.
 */
typedef int (*rawdev_dequeue_bufs_t)(struct rte_rawdev *dev,
				     struct rte_rawdev_buf **buffers,
				     unsigned int count,
				     rte_rawdev_obj_t context);

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

/**
 * Get an attribute value from implementation.
 * Attribute is an opaque handle agreed upon between application and PMD.
 *
 * @param dev
 *   Raw device pointer
 * @param attr_name
 *   Opaque object representing an attribute in implementation.
 * @param attr_value [out]
 *   Opaque response to the attribute value. In case of error, this remains
 *   untouched. This is double pointer of void type.
 * @return
 *   0 for success
 *  !0 Error; attr_value remains untouched in case of error.
 */
typedef int (*rawdev_get_attr_t)(struct rte_rawdev *dev,
				 const char *attr_name,
				 uint64_t *attr_value);

/**
 * Set an attribute value.
 * Attribute is an opaque handle agreed upon between application and PMD.
 *
 * @param dev
 *   Raw device pointer
 * @param attr_name
 *   Opaque object representing an attribute in implementation.
 * @param attr_value
 *   Value of the attribute represented by attr_name
 * @return
 *   0 for success
 *  !0 Error
 */
typedef int (*rawdev_set_attr_t)(struct rte_rawdev *dev,
				 const char *attr_name,
				 const uint64_t attr_value);

/**
 * Retrieve a set of statistics from device.
 * Note: Being a raw device, the stats are specific to the device being
 * implemented thus represented as xstats.
 *
 * @param dev
 *   Raw device pointer
 * @param ids
 *   The stat ids to retrieve
 * @param values
 *   The returned stat values
 * @param n
 *   The number of id values and entries in the values array
 * @return
 *   The number of stat values successfully filled into the values array
 */
typedef int (*rawdev_xstats_get_t)(const struct rte_rawdev *dev,
		const unsigned int ids[], uint64_t values[], unsigned int n);

/**
 * Resets the statistic values in xstats for the device.
 */
typedef int (*rawdev_xstats_reset_t)(struct rte_rawdev *dev,
		const uint32_t ids[],
		uint32_t nb_ids);

/**
 * Get names of extended stats of an raw device
 *
 * @param dev
 *   Raw device pointer
 * @param xstats_names
 *   Array of name values to be filled in
 * @param size
 *   Number of values in the xstats_names array
 * @return
 *   When size >= the number of stats, return the number of stat values filled
 *   into the array.
 *   When size < the number of available stats, return the number of stats
 *   values, and do not fill in any data into xstats_names.
 */
typedef int (*rawdev_xstats_get_names_t)(const struct rte_rawdev *dev,
		struct rte_rawdev_xstats_name *xstats_names,
		unsigned int size);

/**
 * Get value of one stats and optionally return its id
 *
 * @param dev
 *   Raw device pointer
 * @param name
 *   The name of the stat to retrieve
 * @param id
 *   Pointer to an unsigned int where we store the stat-id.
 *   This pointer may be null if the id is not required.
 * @return
 *   The value of the stat, or (uint64_t)-1 if the stat is not found.
 *   If the stat is not found, the id value will be returned as (unsigned)-1,
 *   if id pointer is non-NULL
 */
typedef uint64_t (*rawdev_xstats_get_by_name_t)(const struct rte_rawdev *dev,
						const char *name,
						unsigned int *id);

/**
 * Get firmware/device-stack status.
 * Implementation to allocate buffer for returning information.
 *
 * @param dev
 *   Raw device pointer
 * @param status_info
 *   void block containing device specific status information
 * @return
 *   0 for success,
 *   !0 for failure, with undefined value in `status_info`
 */
typedef int (*rawdev_firmware_status_get_t)(struct rte_rawdev *dev,
					    rte_rawdev_obj_t status_info);

/**
 * Get firmware version information
 *
 * @param dev
 *   Raw device pointer
 * @param version_info
 *   void pointer to version information returned by device
 * @return
 *   0 for success,
 *   !0 for failure, with undefined value in `version_info`
 */
typedef int (*rawdev_firmware_version_get_t)(struct rte_rawdev *dev,
					     rte_rawdev_obj_t version_info);

/**
 * Load firmware from a buffer (DMA'able)
 *
 * @param dev
 *   Raw device pointer
 * @param firmware_buf
 *   Pointer to firmware image
 * @return
 *   >0, ~0: for successful load
 *   <0: for failure
 *
 * @see Application may use 'firmware_version_get` for ascertaining successful
 * load
 */
typedef int (*rawdev_firmware_load_t)(struct rte_rawdev *dev,
				      rte_rawdev_obj_t firmware_buf);

/**
 * Unload firmware
 *
 * @param dev
 *   Raw device pointer
 * @return
 *   >0, ~0 for successful unloading
 *   <0 for failure in unloading
 *
 * Note: Application can use the `firmware_status_get` or
 * `firmware_version_get` to get result of unload.
 */
typedef int (*rawdev_firmware_unload_t)(struct rte_rawdev *dev);

/**
 * Start rawdev selftest
 *
 * @return
 *   Return 0 on success
 */
typedef int (*rawdev_selftest_t)(uint16_t dev_id);

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
	/**< Get the number of queues attached to the device */
	rawdev_queue_count_t queue_count;

	/**< Enqueue an array of raw buffers to device. */
	rawdev_enqueue_bufs_t enqueue_bufs;
	/**< Dequeue an array of raw buffers from device. */
	/** TODO: Callback based enqueue and dequeue support */
	rawdev_dequeue_bufs_t dequeue_bufs;

	/* Dump internal information */
	rawdev_dump_t dump;

	/**< Get an attribute managed by the implementation */
	rawdev_get_attr_t attr_get;
	/**< Set an attribute managed by the implementation */
	rawdev_set_attr_t attr_set;

	/**< Get extended device statistics. */
	rawdev_xstats_get_t xstats_get;
	/**< Get names of extended stats. */
	rawdev_xstats_get_names_t xstats_get_names;
	/**< Get one value by name. */
	rawdev_xstats_get_by_name_t xstats_get_by_name;
	/**< Reset the statistics values in xstats. */
	rawdev_xstats_reset_t xstats_reset;

	/**< Obtain firmware status */
	rawdev_firmware_status_get_t firmware_status_get;
	/**< Obtain firmware version information */
	rawdev_firmware_version_get_t firmware_version_get;
	/**< Load firmware */
	rawdev_firmware_load_t firmware_load;
	/**< Unload firmware */
	rawdev_firmware_unload_t firmware_unload;

	/**< Device selftest function */
	rawdev_selftest_t dev_selftest;
};

/**
 * Allocates a new rawdev slot for an raw device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param name
 *   Unique identifier name for each device
 * @param dev_private_size
 *   Size of private data memory allocated within rte_rawdev object.
 *   Set to 0 to disable internal memory allocation and allow for
 *   self-allocation.
 * @param socket_id
 *   Socket to allocate resources on.
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
struct rte_rawdev *
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
int
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
struct rte_rawdev *
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
int
rte_rawdev_pmd_uninit(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RAWDEV_PMD_H_ */
