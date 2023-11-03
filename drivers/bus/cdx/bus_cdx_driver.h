/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2023, Advanced Micro Devices, Inc.
 */

#ifndef BUS_CDX_DRIVER_H
#define BUS_CDX_DRIVER_H

/**
 * @file
 * AMD CDX bus interface
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <inttypes.h>
#include <linux/types.h>

#include <bus_driver.h>
#include <dev_driver.h>
#include <rte_interrupts.h>

/* Forward declarations */
struct rte_cdx_device;
struct rte_cdx_driver;
struct rte_cdx_bus;

#define RTE_CDX_BUS_DEVICES_PATH "/sys/bus/cdx/devices"

#define RTE_CDX_MAX_RESOURCE 4

/** Any CDX device identifier (vendor, device). */
#define RTE_CDX_ANY_ID (0xffff)

#define RTE_PMD_REGISTER_CDX_TABLE(name, table) \
static const char DRV_EXP_TAG(name, cdx_tbl_export)[] __rte_used = \
RTE_STR(table)

/** Device needs resource mapping */
#define RTE_CDX_DRV_NEED_MAPPING 0x0001

/**
 * A structure describing an ID for a CDX driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_cdx_id {
	uint16_t vendor_id;			/**< Vendor ID. */
	uint16_t device_id;			/**< Device ID. */
};

/**
 * A structure describing a CDX device.
 */
struct rte_cdx_device {
	RTE_TAILQ_ENTRY(rte_cdx_device) next;	/**< Next probed CDX device. */
	struct rte_device device;		/**< Inherit core device */
	struct rte_cdx_driver *driver;		/**< CDX driver used in probing */
	char name[RTE_DEV_NAME_MAX_LEN];	/**< Device name */
	struct rte_cdx_id id;			/**< CDX ID. */
	struct rte_mem_resource mem_resource[RTE_CDX_MAX_RESOURCE];
						/**< CDX Memory Resource */
	struct rte_intr_handle *intr_handle;	/**< Interrupt handle */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_cdx_device.
 */
#define RTE_DEV_TO_CDX_DEV(ptr) \
	container_of(ptr, struct rte_cdx_device, device)

#define RTE_DEV_TO_CDX_DEV_CONST(ptr) \
	container_of(ptr, const struct rte_cdx_device, device)

#define RTE_ETH_DEV_TO_CDX_DEV(eth_dev)	RTE_DEV_TO_CDX_DEV((eth_dev)->device)

#ifdef __cplusplus
/** C++ macro used to help building up tables of device IDs. */
#define RTE_CDX_DEVICE(vend, dev)	\
	(vend),				\
	(dev)
#else
/** Macro used to help building up tables of device IDs. */
#define RTE_CDX_DEVICE(vend, dev)	\
	.vendor_id = (vend),		\
	.device_id = (dev)
#endif

/**
 * Initialisation function for the driver called during CDX probing.
 */
typedef int (rte_cdx_probe_t)(struct rte_cdx_driver *, struct rte_cdx_device *);

/**
 * Uninitialisation function for the driver called during hotplugging.
 */
typedef int (rte_cdx_remove_t)(struct rte_cdx_device *);

/**
 * A structure describing a CDX driver.
 */
struct rte_cdx_driver {
	RTE_TAILQ_ENTRY(rte_cdx_driver) next;	/**< Next in list. */
	struct rte_driver driver;		/**< Inherit core driver. */
	struct rte_cdx_bus *bus;		/**< CDX bus reference. */
	rte_cdx_probe_t *probe;			/**< Device probe function. */
	rte_cdx_remove_t *remove;		/**< Device remove function. */
	const struct rte_cdx_id *id_table;	/**< ID table, NULL terminated. */
	uint32_t drv_flags;			/**< Flags RTE_CDX_DRV_*. */
};

/**
 * Map the CDX device resources in user space virtual memory address.
 *
 * @param dev
 *   A pointer to a rte_cdx_device structure describing the device
 *   to use.
 *
 * @return
 *   0 on success, <0 on error.
 */
__rte_internal
int rte_cdx_map_device(struct rte_cdx_device *dev);

/**
 * Unmap this device.
 *
 * @param dev
 *   A pointer to a rte_cdx_device structure describing the device
 *   to use.
 */
__rte_internal
void rte_cdx_unmap_device(struct rte_cdx_device *dev);

/**
 * Register a CDX driver.
 *
 * @param driver
 *   A pointer to a rte_cdx_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void rte_cdx_register(struct rte_cdx_driver *driver);

/**
 * Helper for CDX device registration from driver (eth, crypto, raw) instance.
 */
#define RTE_PMD_REGISTER_CDX(nm, cdx_drv) \
	RTE_INIT(cdxinitfn_ ##nm) \
	{\
		(cdx_drv).driver.name = RTE_STR(nm);\
		rte_cdx_register(&cdx_drv); \
	} \
	RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/**
 * Enables VFIO Interrupts for CDX bus devices.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 *
 *  @return
 *  0 on success, -1 on error.
 */
__rte_internal
int rte_cdx_vfio_intr_enable(const struct rte_intr_handle *intr_handle);

/**
 * Disable VFIO Interrupts for CDX bus devices.
 *
 * @param intr_handle
 *   Pointer to the interrupt handle.
 *
 *  @return
 *  0 on success, -1 on error.
 */
__rte_internal
int rte_cdx_vfio_intr_disable(const struct rte_intr_handle *intr_handle);

/**
 * Enable Bus Mastering for CDX bus devices.
 *
 * @param dev
 *   Pointer to the cdx device.
 *
 * @return
 *   0 on success, -1 on error.
 */
__rte_internal
int rte_cdx_vfio_bm_enable(struct rte_cdx_device *dev);

/**
 * Disable Bus Mastering for CDX bus devices.
 *
 * @param dev
 *   Pointer to the cdx device.
 *
 * @return
 *   0 on success, -1 on error.
 */
__rte_internal
int rte_cdx_vfio_bm_disable(struct rte_cdx_device *dev);

/**
 * Unregister a CDX driver.
 *
 * @param driver
 *   A pointer to a rte_cdx_driver structure describing the driver
 *   to be unregistered.
 */
__rte_internal
void rte_cdx_unregister(struct rte_cdx_driver *driver);

#ifdef __cplusplus
}
#endif

#endif /* BUS_CDX_DRIVER_H */
