/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_PMD_H
#define RTE_MLDEV_PMD_H

/**
 * @file
 *
 * ML Device PMD interface
 *
 * @note
 * These APIs are for MLDEV PMDs only and user applications should not call them directly.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <rte_common.h>
#include <rte_compat.h>
#include <rte_mldev.h>
#include <rte_mldev_core.h>

/**
 * @internal
 *
 * Initialisation parameters for ML devices.
 */
struct rte_ml_dev_pmd_init_params {
	/** Socket to use for memory allocation. */
	uint8_t socket_id;

	/** Size of device private data. */
	uint64_t private_data_size;
};

struct rte_ml_dev;

/**
 * @internal
 *
 * Get the ML device pointer for the device. Assumes a valid device index.
 *
 * @param dev_id
 *	Device ID value to select the device structure.
 *
 * @return
 *	The rte_ml_dev pointer for the given device ID.
 */
__rte_internal
struct rte_ml_dev *
rte_ml_dev_pmd_get_dev(int16_t dev_id);

/**
 * @internal
 *
 * Get the rte_ml_dev structure device pointer for the named device.
 *
 * @param name
 *	Device name to select the device structure.
 *
 * @return
 *	The rte_ml_dev pointer for the given device ID.
 */
__rte_internal
struct rte_ml_dev *
rte_ml_dev_pmd_get_named_dev(const char *name);

/**
 * @internal
 *
 * Allocates a new mldev slot for an ML device and returns the pointer to that slot for use.
 * Function for internal use by dummy drivers.
 *
 * @param name
 *	Unique identifier name for each device.
 * @param socket_id
 *	Socket to allocate resources.
 *
 * @return
 *	Slot in the rte_ml_dev_devices array for a new device.
 */
__rte_internal
struct rte_ml_dev *
rte_ml_dev_pmd_allocate(const char *name, uint8_t socket_id);

/**
 * @internal
 *
 * Release the specified mldev device.
 *
 * @param dev
 *	ML device.
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
__rte_internal
int
rte_ml_dev_pmd_release(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * PMD assist function to provide boiler plate code for ML driver to create and allocate resources
 * for a new ML PMD device instance.
 *
 * @param name
 *	ML device name.
 * @param device
 *	Base device handle.
 * @param params
 *	PMD initialisation parameters.
 *
 * @return
 *	- ML device instance on success.
 *	- NULL on failure.
 */
__rte_internal
struct rte_ml_dev *
rte_ml_dev_pmd_create(const char *name, struct rte_device *device,
		      struct rte_ml_dev_pmd_init_params *params);

/**
 * @internal
 *
 * PMD assist function to provide boiler plate code for ML driver to destroy and free resources
 * associated with a ML PMD device instance.
 *
 * @param mldev
 *	ML device instance.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
__rte_internal
int
rte_ml_dev_pmd_destroy(struct rte_ml_dev *mldev);

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_PMD_H */
