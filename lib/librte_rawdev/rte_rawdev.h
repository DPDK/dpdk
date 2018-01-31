/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _RTE_RAWDEV_H_
#define _RTE_RAWDEV_H_

/**
 * @file rte_rawdev.h
 *
 * Generic device abstraction APIs.
 *
 * This API allow applications to configure and use generic devices having
 * no specific type already available in DPDK.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_memory.h>
#include <rte_errno.h>

/* Rawdevice object - essentially a void to be typecasted by implementation */
typedef void *rte_rawdev_obj_t;

/**
 * Get the total number of raw devices that have been successfully
 * initialised.
 *
 * @return
 *   The total number of usable raw devices.
 */
uint8_t __rte_experimental
rte_rawdev_count(void);

/**
 * Get the device identifier for the named raw device.
 *
 * @param name
 *   Raw device name to select the raw device identifier.
 *
 * @return
 *   Returns raw device identifier on success.
 *   - <0: Failure to find named raw device.
 */
uint16_t __rte_experimental
rte_rawdev_get_dev_id(const char *name);

/**
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   The identifier of the device.
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -(-EINVAL)  dev_id value is out of range.
 */
int __rte_experimental
rte_rawdev_socket_id(uint16_t dev_id);

/**
 * Raw device information forward declaration
 */
struct rte_rawdev_info;

/**
 * Retrieve the contextual information of a raw device.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param[out] dev_info
 *   A pointer to a structure of type *rte_rawdev_info* to be filled with the
 *   contextual information of the device.
 *
 * @return
 *   - 0: Success, driver updates the contextual information of the raw device
 *   - <0: Error code returned by the driver info get function.
 *
 */
int __rte_experimental
rte_rawdev_info_get(uint16_t dev_id, struct rte_rawdev_info *dev_info);

/**
 * Configure a raw device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * The caller may use rte_rawdev_info_get() to get the capability of each
 * resources available for this raw device.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param dev_conf
 *   The raw device configuration structure encapsulated into rte_rawdev_info
 *   object.
 *   It is assumed that the opaque object has enough information which the
 *   driver/implementation can use to configure the device. It is also assumed
 *   that once the configuration is done, a `queue_id` type field can be used
 *   to refer to some arbitrary internal representation of a queue.
 *
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int __rte_experimental
rte_rawdev_configure(uint16_t dev_id, struct rte_rawdev_info *dev_conf);


/**
 * Retrieve the current configuration information of a raw queue designated
 * by its *queue_id* from the raw driver for a raw device.
 *
 * This function intended to be used in conjunction with rte_raw_queue_setup()
 * where caller needs to set up the queue by overriding few default values.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the raw queue to get the configuration information.
 *   The value must be in the range [0, nb_raw_queues - 1]
 *   previously supplied to rte_rawdev_configure().
 * @param[out] queue_conf
 *   The pointer to the default raw queue configuration data.
 * @return
 *   - 0: Success, driver updates the default raw queue configuration data.
 *   - <0: Error code returned by the driver info get function.
 *
 * @see rte_raw_queue_setup()
 *
 */
int __rte_experimental
rte_rawdev_queue_conf_get(uint16_t dev_id,
			  uint16_t queue_id,
			  rte_rawdev_obj_t queue_conf);

/**
 * Allocate and set up a raw queue for a raw device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the raw queue to setup. The value must be in the range
 *   [0, nb_raw_queues - 1] previously supplied to rte_rawdev_configure().
 * @param queue_conf
 *   The pointer to the configuration data to be used for the raw queue.
 *   NULL value is allowed, in which case default configuration	used.
 *
 * @see rte_rawdev_queue_conf_get()
 *
 * @return
 *   - 0: Success, raw queue correctly set up.
 *   - <0: raw queue configuration failed
 */
int __rte_experimental
rte_rawdev_queue_setup(uint16_t dev_id,
		       uint16_t queue_id,
		       rte_rawdev_obj_t queue_conf);

/**
 * Release and deallocate a raw queue from a raw device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_id
 *   The index of the raw queue to release. The value must be in the range
 *   [0, nb_raw_queues - 1] previously supplied to rte_rawdev_configure().
 *
 * @see rte_rawdev_queue_conf_get()
 *
 * @return
 *   - 0: Success, raw queue released.
 *   - <0: raw queue configuration failed
 */
int __rte_experimental
rte_rawdev_queue_release(uint16_t dev_id, uint16_t queue_id);
/**
 * Get the number of raw queues on a specific raw device
 *
 * @param dev_id
 *   Raw device identifier.
 * @return
 *   - The number of configured raw queues
 */
uint16_t __rte_experimental
rte_rawdev_queue_count(uint16_t dev_id);

/**
 * Start a raw device.
 *
 * The device start step is the last one and consists of setting the raw
 * queues to start accepting the raws and schedules to raw ports.
 *
 * On success, all basic functions exported by the API (raw enqueue,
 * raw dequeue and so on) can be invoked.
 *
 * @param dev_id
 *   Raw device identifier
 * @return
 *   - 0: Success, device started.
 *   < 0: Failure
 */
int __rte_experimental
rte_rawdev_start(uint16_t dev_id);

/**
 * Stop a raw device. The device can be restarted with a call to
 * rte_rawdev_start()
 *
 * @param dev_id
 *   Raw device identifier.
 */
void __rte_experimental
rte_rawdev_stop(uint16_t dev_id);

/**
 * Close a raw device. The device cannot be restarted after this call.
 *
 * @param dev_id
 *   Raw device identifier
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 *  - (-EAGAIN) if device is busy
 */
int __rte_experimental
rte_rawdev_close(uint16_t dev_id);

/**
 * Reset a raw device.
 * This is different from cycle of rte_rawdev_start->rte_rawdev_stop in the
 * sense similar to hard or soft reset.
 *
 * @param dev_id
 *   Raw device identifiers
 * @return
 *   0 for sucessful reset,
 *  !0 for failure in resetting
 */
int __rte_experimental
rte_rawdev_reset(uint16_t dev_id);

#define RTE_RAWDEV_NAME_MAX_LEN	(64)
/**< @internal Max length of name of raw PMD */



/** @internal
 * The data structure associated with each raw device.
 * It is a placeholder for PMD specific data, encapsulating only information
 * related to framework.
 */
struct rte_rawdev {
	/**< Socket ID where memory is allocated */
	int socket_id;
	/**< Device ID for this instance */
	uint16_t dev_id;
	/**< Functions exported by PMD */
	const struct rte_rawdev_ops *dev_ops;
	/**< Device info. supplied during device initialization */
	struct rte_device *device;
	/**< Driver info. supplied by probing */
	const char *driver_name;

	RTE_STD_C11
	/**< Flag indicating the device is attached */
	uint8_t attached : 1;
	/**< Device state: STARTED(1)/STOPPED(0) */
	uint8_t started : 1;

	/**< PMD-specific private data */
	rte_rawdev_obj_t dev_private;
	/**< Device name */
	char name[RTE_RAWDEV_NAME_MAX_LEN];
} __rte_cache_aligned;

/** @internal The pool of rte_rawdev structures. */
extern struct rte_rawdev *rte_rawdevs;


struct rte_rawdev_info {
	/**< Name of driver handling this device */
	const char *driver_name;
	/**< Device encapsulation */
	struct rte_device *device;
	/**< Socket ID where memory is allocated */
	int socket_id;
	/**< PMD-specific private data */
	rte_rawdev_obj_t dev_private;
};

struct rte_rawdev_buf {
	/**< Opaque buffer reference */
	void *buf_addr;
};

/**
 * Dump internal information about *dev_id* to the FILE* provided in *f*.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @param f
 *   A pointer to a file for output
 *
 * @return
 *   - 0: on success
 *   - <0: on failure.
 */
int __rte_experimental
rte_rawdev_dump(uint16_t dev_id, FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RAWDEV_H_ */
