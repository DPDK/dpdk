/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 RehiveTech. All rights reserved.
 */

#ifndef BUS_VDEV_DRIVER_H
#define BUS_VDEV_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_bus_vdev.h>
#include <rte_compat.h>
#include <dev_driver.h>
#include <rte_devargs.h>

struct rte_vdev_device {
	RTE_TAILQ_ENTRY(rte_vdev_device) next;      /**< Next attached vdev */
	struct rte_device device;               /**< Inherit core device */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_vdev_device.
 */
#define RTE_DEV_TO_VDEV(ptr) \
	container_of(ptr, struct rte_vdev_device, device)

#define RTE_DEV_TO_VDEV_CONST(ptr) \
	container_of(ptr, const struct rte_vdev_device, device)

#define RTE_ETH_DEV_TO_VDEV(eth_dev)	RTE_DEV_TO_VDEV((eth_dev)->device)

static inline const char *
rte_vdev_device_name(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.name)
		return dev->device.name;
	return NULL;
}

static inline const char *
rte_vdev_device_args(const struct rte_vdev_device *dev)
{
	if (dev && dev->device.devargs)
		return dev->device.devargs->args;
	return "";
}

/**
 * Probe function called for each virtual device driver once.
 */
typedef int (rte_vdev_probe_t)(struct rte_vdev_device *dev);

/**
 * Remove function called for each virtual device driver once.
 */
typedef int (rte_vdev_remove_t)(struct rte_vdev_device *dev);

/**
 * Driver-specific DMA mapping. After a successful call the device
 * will be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the Virtual device.
 * @param addr
 *   Starting virtual address of memory to be mapped.
 * @param iova
 *   Starting IOVA address of memory to be mapped.
 * @param len
 *   Length of memory segment being mapped.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int (rte_vdev_dma_map_t)(struct rte_vdev_device *dev, void *addr,
			    uint64_t iova, size_t len);

/**
 * Driver-specific DMA un-mapping. After a successful call the device
 * will not be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the Virtual device.
 * @param addr
 *   Starting virtual address of memory to be unmapped.
 * @param iova
 *   Starting IOVA address of memory to be unmapped.
 * @param len
 *   Length of memory segment being unmapped.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int (rte_vdev_dma_unmap_t)(struct rte_vdev_device *dev, void *addr,
			      uint64_t iova, size_t len);

/**
 * A virtual device driver abstraction.
 */
struct rte_vdev_driver {
	RTE_TAILQ_ENTRY(rte_vdev_driver) next; /**< Next in list. */
	struct rte_driver driver;        /**< Inherited general driver. */
	rte_vdev_probe_t *probe;         /**< Virtual device probe function. */
	rte_vdev_remove_t *remove;       /**< Virtual device remove function. */
	rte_vdev_dma_map_t *dma_map;     /**< Virtual device DMA map function. */
	rte_vdev_dma_unmap_t *dma_unmap; /**< Virtual device DMA unmap function. */
	uint32_t drv_flags;              /**< Flags RTE_VDEV_DRV_*. */
};

/** Device driver needs IOVA as VA and cannot work with IOVA as PA */
#define RTE_VDEV_DRV_NEED_IOVA_AS_VA 0x0001

/**
 * Register a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void rte_vdev_register(struct rte_vdev_driver *driver);

/**
 * Unregister a virtual device driver.
 *
 * @param driver
 *   A pointer to a rte_vdev_driver structure describing the driver
 *   to be unregistered.
 */
__rte_internal
void rte_vdev_unregister(struct rte_vdev_driver *driver);

#define RTE_PMD_REGISTER_VDEV(nm, vdrv)\
static const char *vdrvinit_ ## nm ## _alias;\
RTE_INIT(vdrvinitfn_ ##vdrv)\
{\
	(vdrv).driver.name = RTE_STR(nm);\
	(vdrv).driver.alias = vdrvinit_ ## nm ## _alias;\
	rte_vdev_register(&vdrv);\
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#define RTE_PMD_REGISTER_ALIAS(nm, alias)\
static const char *vdrvinit_ ## nm ## _alias = RTE_STR(alias)

#ifdef __cplusplus
}
#endif

#endif /* BUS_VDEV_DRIVER_H */
