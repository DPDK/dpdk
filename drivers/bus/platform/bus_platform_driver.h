/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef BUS_PLATFORM_DRIVER_H
#define BUS_PLATFORM_DRIVER_H

/**
 * @file
 * Platform bus interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <dev_driver.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_os.h>
#include <rte_vfio.h>

/* Forward declarations */
struct rte_platform_bus;
struct rte_platform_device;
struct rte_platform_driver;

/**
 * Initialization function for the driver called during platform device probing.
 *
 * @param pdev
 *   Pointer to the platform device.
 * @return
 *   0 on success, negative value otherwise.
 */
typedef int (rte_platform_probe_t)(struct rte_platform_device *pdev);

/**
 * Removal function for the driver called during platform device removal.
 *
 * @param pdev
 *   Pointer to the platform device.
 * @return
 *   0 on success, negative value otherwise.
 */
typedef int (rte_platform_remove_t)(struct rte_platform_device *pdev);

/**
 * Driver specific DMA mapping.
 *
 * @param pdev
 *   Pointer to the platform device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 * @return
 *   - 0 on success, negative value and rte_errno is set otherwise.
 */
typedef int (rte_platform_dma_map_t)(struct rte_platform_device *pdev, void *addr, uint64_t iova,
				     size_t len);

/**
 * Driver specific DMA unmapping.
 *
 * @param pdev
 *   Pointer to the platform device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 * @return
 *   - 0 on success, negative value and rte_errno is set otherwise.
 */
typedef int (rte_platform_dma_unmap_t)(struct rte_platform_device *pdev, void *addr, uint64_t iova,
				       size_t len);

/**
 * A structure describing a platform device resource.
 */
struct rte_platform_resource {
	char *name; /**< Resource name specified via reg-names prop in device-tree */
	struct rte_mem_resource mem; /**< Memory resource */
};

/**
 * A structure describing a platform device.
 */
struct rte_platform_device {
	RTE_TAILQ_ENTRY(rte_platform_device) next; /**< Next attached platform device */
	struct rte_device device; /**< Core device */
	struct rte_platform_driver *driver; /**< Matching device driver */
	char name[RTE_DEV_NAME_MAX_LEN]; /**< Device name */
	unsigned int num_resource; /**< Number of device resources */
	struct rte_platform_resource *resource; /**< Device resources */
	int dev_fd; /**< VFIO device fd */
};

/**
 * A structure describing a platform device driver.
 */
struct rte_platform_driver {
	RTE_TAILQ_ENTRY(rte_platform_driver) next; /**< Next available platform driver */
	struct rte_driver driver; /**< Core driver */
	rte_platform_probe_t *probe;  /**< Device probe function */
	rte_platform_remove_t *remove; /**< Device remove function */
	rte_platform_dma_map_t *dma_map; /**< Device DMA map function */
	rte_platform_dma_unmap_t *dma_unmap; /**< Device DMA unmap function */
	uint32_t drv_flags; /**< Driver flags RTE_PLATFORM_DRV_* */
};

/** Device driver needs IOVA as VA and cannot work with IOVA as PA */
#define RTE_PLATFORM_DRV_NEED_IOVA_AS_VA 0x0001

/**
 * @internal
 * Helper macros used to convert core device to platform device.
 */
#define RTE_DEV_TO_PLATFORM_DEV(ptr) \
	container_of(ptr, struct rte_platform_device, device)

#define RTE_DEV_TO_PLATFORM_DEV_CONST(ptr) \
	container_of(ptr, const struct rte_platform_device, device)

/** Helper for platform driver registration. */
#define RTE_PMD_REGISTER_PLATFORM(nm, platform_drv) \
static const char *pdrvinit_ ## nm ## _alias; \
RTE_INIT(pdrvinitfn_ ##nm) \
{ \
	(platform_drv).driver.name = RTE_STR(nm); \
	(platform_drv).driver.alias = pdrvinit_ ## nm ## _alias; \
	rte_platform_register(&(platform_drv)); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/** Helper for setting platform driver alias. */
#define RTE_PMD_REGISTER_ALIAS(nm, alias) \
static const char *pdrvinit_ ## nm ## _alias = RTE_STR(alias)

#ifdef VFIO_PRESENT

/**
 * Register a platform device driver.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param pdrv
 *   A pointer to a rte_platform_driver structure describing driver to be registered.
 */
__rte_internal
void rte_platform_register(struct rte_platform_driver *pdrv);

/**
 * Unregister a platform device driver.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param pdrv
 *   A pointer to a rte_platform_driver structure describing driver to be unregistered.
 */
__rte_internal
void rte_platform_unregister(struct rte_platform_driver *pdrv);

#else

__rte_internal
static inline void
rte_platform_register(struct rte_platform_driver *pdrv __rte_unused)
{
}

__rte_internal
static inline void
rte_platform_unregister(struct rte_platform_driver *pdrv __rte_unused)
{
}

#endif /* VFIO_PRESENT */

#ifdef __cplusplus
}
#endif

#endif /* BUS_PLATFORM_DRIVER_H */
