/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 6WIND S.A.
 */

#ifndef _DEV_VFIO_H_
#define _DEV_VFIO_H_

/**
 * @file
 * @internal
 *
 * VFIO device API.
 *
 * This library provides VFIO related utility functions for use by drivers.
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_compat.h>
#include <rte_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEV_VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define DEV_VFIO_GROUP_FMT "/dev/vfio/%u"

/* we don't need an actual definition, only pointer is used */
struct vfio_device_info;

#define DEV_VFIO_DEFAULT_CONTAINER_FD (-1)

/**
 * @enum dev_vfio_module
 * VFIO kernel modules.
 *
 * These values identify kernel modules used by VFIO.
 */
enum dev_vfio_module {
	DEV_VFIO_MODULE_VFIO,     /**< Core VFIO module. */
	DEV_VFIO_MODULE_VFIO_PCI, /**< VFIO PCI module. */
};

/**
 * @enum dev_vfio_mode
 * Enumeration of VFIO operational modes.
 *
 * These modes define how VFIO devices are accessed.
 *
 * - DEV_VFIO_MODE_NONE: VFIO is not enabled.
 * - DEV_VFIO_MODE_GROUP: Legacy group mode.
 */
enum dev_vfio_mode {
	DEV_VFIO_MODE_NONE = 0, /**< VFIO not enabled */
	DEV_VFIO_MODE_GROUP,    /**< Group mode */
};

/**
 * @enum dev_vfio_iova_mode
 * IOVA modes.
 *
 * These modes describe IOVA remapping capability.
 *
 * - DEV_VFIO_IOVA_MODE_UNKNOWN: IOVA mode is unknown.
 * - DEV_VFIO_IOVA_MODE_VA: IOVA addresses can be remapped.
 * - DEV_VFIO_IOVA_MODE_PA: IOVA addresses cannot be remapped.
 */
enum dev_vfio_iova_mode {
	DEV_VFIO_IOVA_MODE_UNKNOWN = 0, /**< IOVA mode not determined */
	DEV_VFIO_IOVA_MODE_VA,          /**< IOVA addresses can be remapped */
	DEV_VFIO_IOVA_MODE_PA,          /**< IOVA addresses cannot be remapped */
};

/**
 * @internal
 * Set up a device managed by VFIO driver.
 *
 * If the device was not previously assigned to a container using
 * `dev_vfio_container_assign_device()`, default container will be used.
 *
 * @param sysfs_base
 *   Sysfs path prefix.
 * @param dev_addr
 *   Device identifier.
 * @param vfio_dev_fd
 *   Pointer to where VFIO device file descriptor will be stored.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENODEV  - Device not managed by VFIO.
 * - ENOSPC  - No space in VFIO container to track the device.
 * - EINVAL  - Invalid parameters.
 * - EIO     - Error during underlying VFIO operations.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 * - ENOMEM  - Memory allocation failed for device tracking.
 */
__rte_internal
int dev_vfio_setup_device(const char *sysfs_base, const char *dev_addr, int *vfio_dev_fd);

/**
 * @internal
 * Release a device managed by VFIO driver.
 *
 * @note As a result of this function, all internal resources used by the device will be released,
 *       so if the device was using a non-default container, it will need to be reassigned to the
 *       container before it can be used again.
 *
 * @param sysfs_base
 *   Sysfs path prefix.
 * @param dev_addr
 *   Device identifier.
 * @param fd
 *   A previously set up VFIO file descriptor.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENOENT  - Device not found in any container.
 * - EINVAL  - Invalid parameters.
 * - EIO     - Error during underlying VFIO operations.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int dev_vfio_release_device(const char *sysfs_base, const char *dev_addr, int fd);

/**
 * @internal
 * Initialize VFIO.
 *
 * In case of success, `dev_vfio_get_mode()` can be used to retrieve the VFIO mode in use.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - EINVAL  - Invalid parameters.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Operation not supported.
 */
__rte_internal
int dev_vfio_enable(void);

/**
 * @internal
 * Cleanup VFIO resources.
 */
__rte_internal
void dev_vfio_cleanup(void);

/**
 * @internal
 * Check whether a VFIO module is loaded.
 *
 * @param module
 *   VFIO module to check.
 *
 * @return
 *   1 if the requested module is loaded.
 *   0 otherwise.
 */
__rte_internal
int dev_vfio_module_is_loaded(enum dev_vfio_module module);

/**
 * @internal
 * Check whether VFIO was initialized.
 *
 * @return
 *   1 if VFIO was initialized.
 *   0 otherwise.
 */
__rte_internal
int dev_vfio_is_enabled(void);

/**
 * @internal
 * Get current VFIO mode.
 *
 *   VFIO mode currently in use.
 */
__rte_internal
enum dev_vfio_mode
dev_vfio_get_mode(void);

/**
 * @internal
 * Get current VFIO IOVA mode.
 *
 * @return
 *   VFIO IOVA mode currently in use.
 */
__rte_internal
enum dev_vfio_iova_mode
dev_vfio_get_iova_mode(void);

/**
 * @internal
 * Parse IOMMU group number for a device.
 *
 * @param sysfs_base
 *   Sysfs path prefix.
 * @param dev_addr
 *   Device identifier.
 * @param iommu_group_num
 *   Pointer to where IOMMU group number will be stored.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENODEV  - Device not managed by VFIO.
 * - EINVAL  - Invalid parameters.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_get_group_num(const char *sysfs_base, const char *dev_addr, int *iommu_group_num);

/**
 * @internal
 * Get device information.
 *
 * This function retrieves VFIO device information from an already opened
 * device. The device must be opened with `dev_vfio_setup_device()` first.
 *
 * @param vfio_dev_fd
 *   VFIO device fd (must be a valid, already opened fd).
 *
 * @param device_info
 *   Pointer to device information structure to be filled.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - EINVAL  - Invalid parameters.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_get_device_info(int vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * @internal
 * Get the default VFIO container file descriptor.
 *
 * @return
 *   Non-negative container file descriptor on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_get_container_fd(void);

/**
 * @internal
 * Create a new VFIO container for device assignment and DMA mapping.
 *
 * @note Any newly allocated DPDK memory will not be mapped into these
 *       containers by default, user needs to manage DMA mappings for
 *       any container created by this API.
 *
 * @note When creating containers using this API, the container will only be
 *       available in the process that has created it. Sharing containers and
 *       devices between multiple processes is not supported.
 *
 * @return
 *   Non-negative container file descriptor on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENOSPC  - Maximum number of containers reached.
 * - EIO     - Underlying VFIO operation failed.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_container_create(void);

/**
 * @internal
 * Destroy a VFIO container and unmap all devices assigned to it.
 *
 * @param container_fd
 *   File descriptor of container to destroy.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENODEV  - Container not managed by VFIO.
 * - EINVAL  - Invalid container file descriptor.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_container_destroy(int container_fd);

/**
 * @internal
 *
 * Assign a device to a VFIO container.
 *
 * Doing so will cause `dev_vfio_setup_device()` call to set up the device with
 * the VFIO container specified in this assign operation.
 *
 * @param vfio_container_fd
 *   VFIO container file descriptor.
 * @param sysfs_base
 *   Sysfs path prefix.
 * @param dev_addr
 *   Device identifier.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - ENODEV  - Device not managed by VFIO.
 * - EEXIST  - Device already assigned to the container.
 * - ENOSPC  - No space in VFIO container to assign device.
 * - EINVAL  - Invalid container file descriptor.
 * - EIO     - Error during underlying VFIO operations.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 * - ENOMEM  - Memory allocation failed for device tracking.
 */
__rte_internal
int
dev_vfio_container_assign_device(int vfio_container_fd, const char *sysfs_base,
		const char *dev_addr);

/**
 * @internal
 * Perform DMA mapping for devices in a container.
 *
 * @param container_fd
 *   Container file descriptor. Use DEV_VFIO_DEFAULT_CONTAINER_FD to use the default container.
 * @param vaddr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - EIO     - DMA mapping operation failed.
 * - EINVAL  - Invalid parameters.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_container_dma_map(int container_fd, uint64_t vaddr, uint64_t iova, uint64_t len);

/**
 * @internal
 * Perform DMA unmapping for devices in a container.
 *
 * @param container_fd
 *   Container file descriptor. Use DEV_VFIO_DEFAULT_CONTAINER_FD to use the default container.
 * @param vaddr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 *
 * @return
 *   0 on success.
 *   <0 on failure, rte_errno is set.
 *
 * Possible rte_errno values include:
 * - EIO     - DMA unmapping operation failed.
 * - EINVAL  - Invalid parameters.
 * - ENXIO   - VFIO support not initialized.
 * - ENOTSUP - Unsupported VFIO mode.
 */
__rte_internal
int
dev_vfio_container_dma_unmap(int container_fd, uint64_t vaddr, uint64_t iova, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif /* _DEV_VFIO_H_ */
