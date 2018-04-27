/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _RTE_COMPRESSDEV_H_
#define _RTE_COMPRESSDEV_H_

/**
 * @file rte_compressdev.h
 *
 * RTE Compression Device APIs
 *
 * Defines comp device APIs for the provisioning of compression operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

/**  comp device information */
struct rte_compressdev_info {
	const char *driver_name;		/**< Driver name. */
};

/**
 * Get the compress device name given a device identifier.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - Returns compress device name.
 *   - Returns NULL if compress device is not present.
 */
const char * __rte_experimental
rte_compressdev_name_get(uint8_t dev_id);

/**
 * Get the total number of compress devices that have been successfully
 * initialised.
 *
 * @return
 *   - The total number of usable compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_count(void);

/**
 * Get number and identifiers of attached comp devices that
 * use the same compress driver.
 *
 * @param driver_name
 *   Driver name
 * @param devices
 *   Output devices identifiers
 * @param nb_devices
 *   Maximal number of devices
 *
 * @return
 *   Returns number of attached compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
		uint8_t nb_devices);

/*
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 if returned is the dev_id value is out of range.
 */
int __rte_experimental
rte_compressdev_socket_id(uint8_t dev_id);

/** Compress device configuration structure */
struct rte_compressdev_config {
	int socket_id;
};

/**
 * Configure a device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param dev_id
 *   Compress device identifier
 * @param config
 *   The compress device configuration
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int __rte_experimental
rte_compressdev_configure(uint8_t dev_id,
			struct rte_compressdev_config *config);

/**
 * Start a device.
 *
 * The device start step is called after configuring the device and setting up
 * its queue pairs.
 * On success, data-path functions exported by the API (enqueue/dequeue, etc)
 * can be invoked.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - 0: Success, device started.
 *   - <0: Error code of the driver device start function.
 */
int __rte_experimental
rte_compressdev_start(uint8_t dev_id);

/**
 * Stop a device. The device can be restarted with a call to
 * rte_compressdev_start()
 *
 * @param dev_id
 *   Compress device identifier
 */
void __rte_experimental
rte_compressdev_stop(uint8_t dev_id);

/**
 * Close an device.
 * The memory allocated in the device gets freed.
 * After calling this function, in order to use
 * the device again, it is required to
 * configure the device again.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 */
int __rte_experimental
rte_compressdev_close(uint8_t dev_id);

/**
 * Retrieve the contextual information of a device.
 *
 * @param dev_id
 *   Compress device identifier
 * @param dev_info
 *   A pointer to a structure of type *rte_compressdev_info*
 *   to be filled with the contextual information of the device
 *
 * @note The capabilities field of dev_info is set to point to the first
 * element of an array of struct rte_compressdev_capabilities.
 * The element after the last valid element has it's op field set to
 * RTE_COMP_ALGO_LIST_END.
 */
void __rte_experimental
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_COMPRESSDEV_H_ */
