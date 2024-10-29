/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 6WIND S.A.
 */

#ifndef _RTE_VFIO_H_
#define _RTE_VFIO_H_

/**
 * @file
 * RTE VFIO. This library provides various VFIO related utility functions.
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RTE_EXEC_ENV_LINUX

#include <linux/vfio.h>

#define RTE_VFIO_DIR "/dev/vfio"
#define RTE_VFIO_CONTAINER_PATH "/dev/vfio/vfio"
#define RTE_VFIO_GROUP_FMT "/dev/vfio/%u"
#define RTE_VFIO_NOIOMMU_GROUP_FMT "/dev/vfio/noiommu-%u"
#define RTE_VFIO_NOIOMMU_MODE      \
	"/sys/module/vfio/parameters/enable_unsafe_noiommu_mode"

#else /* ! RTE_EXEC_ENV_LINUX */

/* we don't need an actual definition, only pointer is used */
struct vfio_device_info;

#endif /* RTE_EXEC_ENV_LINUX */

#define RTE_VFIO_DEFAULT_CONTAINER_FD (-1)

/**
 * Setup vfio_cfg for the device identified by its address.
 * It discovers the configured I/O MMU groups or sets a new one for the device.
 * If a new groups is assigned, the DMA mapping is performed.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
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
int rte_vfio_setup_device(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * Release a device mapped to a VFIO-managed I/O MMU group.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
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
int rte_vfio_release_device(const char *sysfs_base, const char *dev_addr, int fd);

/**
 * Enable a VFIO-related kmod.
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param modname
 *   kernel module name.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
int rte_vfio_enable(const char *modname);

/**
 * Check whether a VFIO-related kmod is enabled.
 *
 * This function is only relevant to Linux.
 *
 * @param modname
 *   kernel module name.
 *
 * @return
 *   1 if true.
 *   0 otherwise.
 */
int rte_vfio_is_enabled(const char *modname);

/**
 * Whether VFIO NOIOMMU mode is enabled.
 *
 * This function is only relevant to Linux.
 *
 * @return
 *   1 if true.
 *   0 if false.
 *   <0 for errors.
 */
int rte_vfio_noiommu_is_enabled(void);

/**
 * Remove group fd from internal VFIO group fd array/
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param vfio_group_fd
 *   VFIO Group FD.
 *
 * @return
 *   0 on success.
 *   <0 on failure.
 */
int
rte_vfio_clear_group(int vfio_group_fd);

/**
 * Parse IOMMU group number for a device
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
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
int
rte_vfio_get_group_num(const char *sysfs_base,
		      const char *dev_addr, int *iommu_group_num);

/**
 * Get device information
 *
 * This function is only relevant to Linux and will return an error on BSD.
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
__rte_experimental
int
rte_vfio_get_device_info(const char *sysfs_base, const char *dev_addr,
		int *vfio_dev_fd, struct vfio_device_info *device_info);

/**
 * Open a new VFIO container fd
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @return
 *  > 0 container fd
 *  < 0 for errors
 */
int
rte_vfio_get_container_fd(void);

/**
 * Open VFIO group fd or get an existing one
 *
 * This function is only relevant to linux and will return
 * an error on BSD.
 *
 * @param iommu_group_num
 *   iommu group number
 *
 * @return
 *  > 0 group fd
 *  < 0 for errors
 */
int
rte_vfio_get_group_fd(int iommu_group_num);

/**
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
int
rte_vfio_container_create(void);

/**
 * Destroy the container, unbind all vfio groups within it.
 *
 * @param container_fd
 *   the container fd to destroy
 *
 * @return
 *    0 if successful
 *   <0 if failed
 */
int
rte_vfio_container_destroy(int container_fd);

/**
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
int
rte_vfio_container_group_bind(int container_fd, int iommu_group_num);

/**
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
int
rte_vfio_container_group_unbind(int container_fd, int iommu_group_num);

/**
 * Perform DMA mapping for devices in a container.
 *
 * @param container_fd
 *   the specified container fd. Use RTE_VFIO_DEFAULT_CONTAINER_FD to
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
int
rte_vfio_container_dma_map(int container_fd, uint64_t vaddr,
		uint64_t iova, uint64_t len);

/**
 * Perform DMA unmapping for devices in a container.
 *
 * @param container_fd
 *   the specified container fd. Use RTE_VFIO_DEFAULT_CONTAINER_FD to
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
int
rte_vfio_container_dma_unmap(int container_fd, uint64_t vaddr,
		uint64_t iova, uint64_t len);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_VFIO_H_ */
