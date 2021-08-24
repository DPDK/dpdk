/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_BUS_AUXILIARY_H
#define RTE_BUS_AUXILIARY_H

/**
 * @file
 *
 * Auxiliary Bus Interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_kvargs.h>

#define RTE_BUS_AUXILIARY_NAME "auxiliary"

/* Forward declarations */
struct rte_auxiliary_driver;
struct rte_auxiliary_bus;
struct rte_auxiliary_device;

/**
 * Match function for the driver to decide if device can be handled.
 *
 * @param name
 *   Pointer to the auxiliary device name.
 * @return
 *   Whether the driver can handle the auxiliary device.
 */
typedef bool(rte_auxiliary_match_t)(const char *name);

/**
 * Initialization function for the driver called during auxiliary probing.
 *
 * @param drv
 *   Pointer to the auxiliary driver.
 * @param dev
 *   Pointer to the auxiliary device.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int(rte_auxiliary_probe_t)(struct rte_auxiliary_driver *drv,
				    struct rte_auxiliary_device *dev);

/**
 * Uninitialization function for the driver called during hotplugging.
 *
 * @param dev
 *   Pointer to the auxiliary device.
 * @return
 *   - 0 On success.
 *   - Negative value and rte_errno is set otherwise.
 */
typedef int (rte_auxiliary_remove_t)(struct rte_auxiliary_device *dev);

/**
 * Driver-specific DMA mapping. After a successful call the device
 * will be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the auxiliary device.
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
typedef int (rte_auxiliary_dma_map_t)(struct rte_auxiliary_device *dev,
				       void *addr, uint64_t iova, size_t len);

/**
 * Driver-specific DMA un-mapping. After a successful call the device
 * will not be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the auxiliary device.
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
typedef int (rte_auxiliary_dma_unmap_t)(struct rte_auxiliary_device *dev,
					 void *addr, uint64_t iova, size_t len);

/**
 * A structure describing an auxiliary device.
 */
struct rte_auxiliary_device {
	RTE_TAILQ_ENTRY(rte_auxiliary_device) next; /**< Next probed device. */
	struct rte_device device;                 /**< Inherit core device */
	char name[RTE_DEV_NAME_MAX_LEN + 1];      /**< ASCII device name */
	struct rte_intr_handle intr_handle;       /**< Interrupt handle */
	struct rte_auxiliary_driver *driver;      /**< Device driver */
};

/**
 * A structure describing an auxiliary driver.
 */
struct rte_auxiliary_driver {
	RTE_TAILQ_ENTRY(rte_auxiliary_driver) next; /**< Next in list. */
	struct rte_driver driver;             /**< Inherit core driver. */
	struct rte_auxiliary_bus *bus;        /**< Auxiliary bus reference. */
	rte_auxiliary_match_t *match;         /**< Device match function. */
	rte_auxiliary_probe_t *probe;         /**< Device probe function. */
	rte_auxiliary_remove_t *remove;       /**< Device remove function. */
	rte_auxiliary_dma_map_t *dma_map;     /**< Device DMA map function. */
	rte_auxiliary_dma_unmap_t *dma_unmap; /**< Device DMA unmap function. */
	uint32_t drv_flags;                   /**< Flags RTE_AUXILIARY_DRV_*. */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_auxiliary_device.
 */
#define RTE_DEV_TO_AUXILIARY(ptr) \
	container_of(ptr, struct rte_auxiliary_device, device)

#define RTE_DEV_TO_AUXILIARY_CONST(ptr) \
	container_of(ptr, const struct rte_auxiliary_device, device)

#define RTE_ETH_DEV_TO_AUXILIARY(eth_dev) \
	RTE_DEV_TO_AUXILIARY((eth_dev)->device)

/** Device driver needs IOVA as VA and cannot work with IOVA as PA */
#define RTE_AUXILIARY_DRV_NEED_IOVA_AS_VA 0x002

/**
 * Register an auxiliary driver.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param driver
 *   A pointer to a rte_auxiliary_driver structure describing the driver
 *   to be registered.
 */
__rte_experimental
void rte_auxiliary_register(struct rte_auxiliary_driver *driver);

/** Helper for auxiliary device registration from driver instance */
#define RTE_PMD_REGISTER_AUXILIARY(nm, auxiliary_drv) \
	RTE_INIT(auxiliaryinitfn_ ##nm) \
	{ \
		(auxiliary_drv).driver.name = RTE_STR(nm); \
		rte_auxiliary_register(&(auxiliary_drv)); \
	} \
	RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/**
 * Unregister an auxiliary driver.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice.
 *
 * @param driver
 *   A pointer to a rte_auxiliary_driver structure describing the driver
 *   to be unregistered.
 */
__rte_experimental
void rte_auxiliary_unregister(struct rte_auxiliary_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* RTE_BUS_AUXILIARY_H */
