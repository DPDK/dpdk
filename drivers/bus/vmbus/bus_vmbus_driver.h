/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#ifndef BUS_VMBUS_DRIVER_H
#define BUS_VMBUS_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_bus_vmbus.h>
#include <rte_compat.h>
#include <dev_driver.h>

struct vmbus_channel;
struct vmbus_mon_page;

/** Maximum number of VMBUS resources. */
enum hv_uio_map {
	HV_TXRX_RING_MAP = 0,
	HV_INT_PAGE_MAP,
	HV_MON_PAGE_MAP,
	HV_RECV_BUF_MAP,
	HV_SEND_BUF_MAP
};
#define VMBUS_MAX_RESOURCE 5

/**
 * A structure describing a VMBUS device.
 */
struct rte_vmbus_device {
	RTE_TAILQ_ENTRY(rte_vmbus_device) next; /**< Next probed VMBUS device */
	const struct rte_vmbus_driver *driver; /**< Associated driver */
	struct rte_device device;              /**< Inherit core device */
	rte_uuid_t device_id;		       /**< VMBUS device id */
	rte_uuid_t class_id;		       /**< VMBUS device type */
	uint32_t relid;			       /**< id for primary */
	uint8_t monitor_id;		       /**< monitor page */
	int uio_num;			       /**< UIO device number */
	uint32_t *int_page;		       /**< VMBUS interrupt page */
	struct vmbus_channel *primary;	       /**< VMBUS primary channel */
	struct vmbus_mon_page *monitor_page;   /**< VMBUS monitor page */

	struct rte_intr_handle *intr_handle;    /**< Interrupt handle */
	struct rte_mem_resource resource[VMBUS_MAX_RESOURCE];
};

/**
 * Initialization function for the driver called during VMBUS probing.
 */
typedef int (vmbus_probe_t)(struct rte_vmbus_driver *,
			    struct rte_vmbus_device *);

/**
 * Initialization function for the driver called during hot plugging.
 */
typedef int (vmbus_remove_t)(struct rte_vmbus_device *);

/**
 * A structure describing a VMBUS driver.
 */
struct rte_vmbus_driver {
	RTE_TAILQ_ENTRY(rte_vmbus_driver) next; /**< Next in list. */
	struct rte_driver driver;
	vmbus_probe_t *probe;               /**< Device Probe function. */
	vmbus_remove_t *remove;             /**< Device Remove function. */

	const rte_uuid_t *id_table;	    /**< ID table. */
};

/**
 * Register a VMBUS driver.
 *
 * @param driver
 *   A pointer to a rte_vmbus_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void rte_vmbus_register(struct rte_vmbus_driver *driver);

/**
 * Unregister a VMBUS driver.
 *
 * @param driver
 *   A pointer to a rte_vmbus_driver structure describing the driver
 *   to be unregistered.
 */
__rte_internal
void rte_vmbus_unregister(struct rte_vmbus_driver *driver);

/** Helper for VMBUS device registration from driver instance */
#define RTE_PMD_REGISTER_VMBUS(nm, vmbus_drv)		\
	RTE_INIT(vmbusinitfn_ ##nm)			\
	{						\
		(vmbus_drv).driver.name = RTE_STR(nm);	\
		rte_vmbus_register(&vmbus_drv);		\
	}						\
	RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

#ifdef __cplusplus
}
#endif

#endif /* BUS_VMBUS_DRIVER_H */
