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

#ifdef __cplusplus
extern "C" {
#endif

#define DEV_VFIO_DIR "/dev/vfio"
#define DEV_VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define DEV_VFIO_GROUP_FMT "/dev/vfio/%u"
#define DEV_VFIO_NOIOMMU_GROUP_FMT "/dev/vfio/noiommu-%u"
#define DEV_VFIO_NOIOMMU_MODE      \
	"/sys/module/vfio/parameters/enable_unsafe_noiommu_mode"

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
 * @internal
 * Setup vfio_cfg for the device identified by its address.
 * It discovers the configured I/O MMU groups or sets a new one for the device.
 * If a new groups is assigned, the DMA mapping is performed.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param vfio_dev_fd
 *   VFIO fd.
 *
 * @param device_info
 *   Device information.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 *   >1 if the device cannot be managed this way.
 */
__rte_internal
int dev_vfio_setup_device(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * @internal
 * Release a device mapped to a VFIO-managed I/O MMU group.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param fd
 *   VFIO fd.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
__rte_internal
int dev_vfio_release_device(const char *sysfs_base, const char *dev_addr, int fd);

/**
 * @internal
 * Initialize VFIO.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
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
 * Whether VFIO NOIOMMU mode is enabled.
 *
 * @return
 *   1 if true.
 *   0 if false.
 *   <0 for errors.
 */
__rte_internal
int dev_vfio_noiommu_is_enabled(void);

/**
 * @internal
 * Remove group fd from internal VFIO group fd array.
 *
 * @param vfio_group_fd
 *   VFIO Group FD.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
__rte_internal
int
dev_vfio_clear_group(int vfio_group_fd);

/**
 * @internal
 * Parse IOMMU group number for a device.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param iommu_group_num
 *   iommu group number
 *
 * @return
 *  >0 on success
 *   0 for non-existent group or VFIO
 *  <0 for errors
 */
__rte_internal
int
dev_vfio_get_group_num(const char *sysfs_base, const char *dev_addr, int *iommu_group_num);

/**
 * @internal
 * Get device information.
 *
 * @param sysfs_base
 *   sysfs path prefix.
 *
 * @param dev_addr
 *   device location.
 *
 * @param vfio_dev_fd
 *   VFIO fd.
 *
 * @param device_info
 *   Device information.
 *
 * @return
 *   0 on success.
 *  <0 on failure.
 */
__rte_internal
int
dev_vfio_get_device_info(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * @internal
 * Get the default VFIO container fd
 *
 * @return
 *  > 0 default container fd
 *  < 0 if VFIO is not enabled or not supported
 */
__rte_internal
int
dev_vfio_get_container_fd(void);

/**
 * @internal
 * Open VFIO group fd or get an existing one.
 *
 * @param iommu_group_num
 *   iommu group number
 *
 * @return
 *  > 0 group fd
 *  < 0 for errors
 */
__rte_internal
int
dev_vfio_get_group_fd(int iommu_group_num);

/**
 * @internal
 * Create a new container for device binding.
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
 *   the container fd if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_create(void);

/**
 * @internal
 * Destroy the container, unbind all vfio groups within it.
 *
 * @param container_fd
 *   the container fd to destroy
 *
 * @return
 *    0 if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_destroy(int container_fd);

/**
 * @internal
 * Bind a IOMMU group to a container.
 *
 * @param container_fd
 *   the container's fd
 *
 * @param iommu_group_num
 *   the iommu group number to bind to container
 *
 * @return
 *   group fd if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_group_bind(int container_fd, int iommu_group_num);

/**
 * @internal
 * Unbind a IOMMU group from a container.
 *
 * @param container_fd
 *   the container fd of container
 *
 * @param iommu_group_num
 *   the iommu group number to delete from container
 *
 * @return
 *    0 if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_group_unbind(int container_fd, int iommu_group_num);

/**
 * @internal
 * Perform DMA mapping for devices in a container.
 *
 * @param container_fd
 *   the specified container fd. Use DEV_VFIO_DEFAULT_CONTAINER_FD to
 *   use the default container.
 *
 * @param vaddr
 *   Starting virtual address of memory to be mapped.
 *
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 *
 * @param len
 *   Length of memory segment being mapped.
 *
 * @return
 *    0 if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_dma_map(int container_fd, uint64_t vaddr, uint64_t iova, uint64_t len);

/**
 * @internal
 * Perform DMA unmapping for devices in a container.
 *
 * @param container_fd
 *   the specified container fd. Use DEV_VFIO_DEFAULT_CONTAINER_FD to
 *   use the default container.
 *
 * @param vaddr
 *   Starting virtual address of memory to be unmapped.
 *
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 *
 * @param len
 *   Length of memory segment being unmapped.
 *
 * @return
 *    0 if successful
 *   <0 if failed
 */
__rte_internal
int
dev_vfio_container_dma_unmap(int container_fd, uint64_t vaddr, uint64_t iova, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif /* _DEV_VFIO_H_ */
