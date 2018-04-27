/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#ifndef _RTE_BUS_PCI_H_
#define _RTE_BUS_PCI_H_

/**
 * @file
 *
 * RTE PCI Bus Interface
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_debug.h>
#include <rte_interrupts.h>
#include <rte_dev.h>
#include <rte_bus.h>
#include <rte_pci.h>

/** Pathname of PCI devices directory. */
const char *rte_pci_get_sysfs_path(void);

/* Forward declarations */
struct rte_pci_device;
struct rte_pci_driver;

/** List of PCI devices */
TAILQ_HEAD(rte_pci_device_list, rte_pci_device);
/** List of PCI drivers */
TAILQ_HEAD(rte_pci_driver_list, rte_pci_driver);

/* PCI Bus iterators */
#define FOREACH_DEVICE_ON_PCIBUS(p)	\
		TAILQ_FOREACH(p, &(rte_pci_bus.device_list), next)

#define FOREACH_DRIVER_ON_PCIBUS(p)	\
		TAILQ_FOREACH(p, &(rte_pci_bus.driver_list), next)

struct rte_devargs;

/**
 * A structure describing a PCI device.
 */
struct rte_pci_device {
	TAILQ_ENTRY(rte_pci_device) next;   /**< Next probed PCI device. */
	struct rte_device device;           /**< Inherit core device */
	struct rte_pci_addr addr;           /**< PCI location. */
	struct rte_pci_id id;               /**< PCI ID. */
	struct rte_mem_resource mem_resource[PCI_MAX_RESOURCE];
					    /**< PCI Memory Resource */
	struct rte_intr_handle intr_handle; /**< Interrupt handle */
	struct rte_pci_driver *driver;      /**< Associated driver */
	uint16_t max_vfs;                   /**< sriov enable if not zero */
	enum rte_kernel_driver kdrv;        /**< Kernel driver passthrough */
	char name[PCI_PRI_STR_SIZE+1];      /**< PCI location (ASCII) */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_pci_device.
 */
#define RTE_DEV_TO_PCI(ptr) container_of(ptr, struct rte_pci_device, device)

#define RTE_DEV_TO_PCI_CONST(ptr) \
	container_of(ptr, const struct rte_pci_device, device)

#define RTE_ETH_DEV_TO_PCI(eth_dev)	RTE_DEV_TO_PCI((eth_dev)->device)

/** Any PCI device identifier (vendor, device, ...) */
#define PCI_ANY_ID (0xffff)
#define RTE_CLASS_ANY_ID (0xffffff)

#ifdef __cplusplus
/** C++ macro used to help building up tables of device IDs */
#define RTE_PCI_DEVICE(vend, dev) \
	RTE_CLASS_ANY_ID,         \
	(vend),                   \
	(dev),                    \
	PCI_ANY_ID,               \
	PCI_ANY_ID
#else
/** Macro used to help building up tables of device IDs */
#define RTE_PCI_DEVICE(vend, dev)          \
	.class_id = RTE_CLASS_ANY_ID,      \
	.vendor_id = (vend),               \
	.device_id = (dev),                \
	.subsystem_vendor_id = PCI_ANY_ID, \
	.subsystem_device_id = PCI_ANY_ID
#endif

/**
 * Initialisation function for the driver called during PCI probing.
 */
typedef int (pci_probe_t)(struct rte_pci_driver *, struct rte_pci_device *);

/**
 * Uninitialisation function for the driver called during hotplugging.
 */
typedef int (pci_remove_t)(struct rte_pci_device *);

/**
 * A structure describing a PCI driver.
 */
struct rte_pci_driver {
	TAILQ_ENTRY(rte_pci_driver) next;  /**< Next in list. */
	struct rte_driver driver;          /**< Inherit core driver. */
	struct rte_pci_bus *bus;           /**< PCI bus reference. */
	pci_probe_t *probe;                /**< Device Probe function. */
	pci_remove_t *remove;              /**< Device Remove function. */
	const struct rte_pci_id *id_table; /**< ID table, NULL terminated. */
	uint32_t drv_flags;                /**< Flags contolling handling of device. */
};

/**
 * Structure describing the PCI bus
 */
struct rte_pci_bus {
	struct rte_bus bus;               /**< Inherit the generic class */
	struct rte_pci_device_list device_list;  /**< List of PCI devices */
	struct rte_pci_driver_list driver_list;  /**< List of PCI drivers */
};

/** Device needs PCI BAR mapping (done with either IGB_UIO or VFIO) */
#define RTE_PCI_DRV_NEED_MAPPING 0x0001
/** Device driver supports link state interrupt */
#define RTE_PCI_DRV_INTR_LSC	0x0008
/** Device driver supports device removal interrupt */
#define RTE_PCI_DRV_INTR_RMV 0x0010
/** Device driver needs to keep mapped resources if unsupported dev detected */
#define RTE_PCI_DRV_KEEP_MAPPED_RES 0x0020
/** Device driver supports IOVA as VA */
#define RTE_PCI_DRV_IOVA_AS_VA 0X0040

/**
 * Map the PCI device resources in user space virtual memory address
 *
 * Note that driver should not call this function when flag
 * RTE_PCI_DRV_NEED_MAPPING is set, as EAL will do that for
 * you when it's on.
 *
 * @param dev
 *   A pointer to a rte_pci_device structure describing the device
 *   to use
 *
 * @return
 *   0 on success, negative on error and positive if no driver
 *   is found for the device.
 */
int rte_pci_map_device(struct rte_pci_device *dev);

/**
 * Unmap this device
 *
 * @param dev
 *   A pointer to a rte_pci_device structure describing the device
 *   to use
 */
void rte_pci_unmap_device(struct rte_pci_device *dev);

/**
 * Dump the content of the PCI bus.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_pci_dump(FILE *f);

/**
 * Register a PCI driver.
 *
 * @param driver
 *   A pointer to a rte_pci_driver structure describing the driver
 *   to be registered.
 */
void rte_pci_register(struct rte_pci_driver *driver);

/** Helper for PCI device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_PCI(nm, pci_drv) \
RTE_INIT(pciinitfn_ ##nm); \
static void pciinitfn_ ##nm(void) \
{\
	(pci_drv).driver.name = RTE_STR(nm);\
	rte_pci_register(&pci_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/**
 * Unregister a PCI driver.
 *
 * @param driver
 *   A pointer to a rte_pci_driver structure describing the driver
 *   to be unregistered.
 */
void rte_pci_unregister(struct rte_pci_driver *driver);

/**
 * Read PCI config space.
 *
 * @param device
 *   A pointer to a rte_pci_device structure describing the device
 *   to use
 * @param buf
 *   A data buffer where the bytes should be read into
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into PCI config space
 */
int rte_pci_read_config(const struct rte_pci_device *device,
		void *buf, size_t len, off_t offset);

/**
 * Write PCI config space.
 *
 * @param device
 *   A pointer to a rte_pci_device structure describing the device
 *   to use
 * @param buf
 *   A data buffer containing the bytes should be written
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into PCI config space
 */
int rte_pci_write_config(const struct rte_pci_device *device,
		const void *buf, size_t len, off_t offset);

/**
 * A structure used to access io resources for a pci device.
 * rte_pci_ioport is arch, os, driver specific, and should not be used outside
 * of pci ioport api.
 */
struct rte_pci_ioport {
	struct rte_pci_device *dev;
	uint64_t base;
	uint64_t len; /* only filled for memory mapped ports */
};

/**
 * Initialize a rte_pci_ioport object for a pci device io resource.
 *
 * This object is then used to gain access to those io resources (see below).
 *
 * @param dev
 *   A pointer to a rte_pci_device structure describing the device
 *   to use.
 * @param bar
 *   Index of the io pci resource we want to access.
 * @param p
 *   The rte_pci_ioport object to be initialized.
 * @return
 *  0 on success, negative on error.
 */
int rte_pci_ioport_map(struct rte_pci_device *dev, int bar,
		struct rte_pci_ioport *p);

/**
 * Release any resources used in a rte_pci_ioport object.
 *
 * @param p
 *   The rte_pci_ioport object to be uninitialized.
 * @return
 *  0 on success, negative on error.
 */
int rte_pci_ioport_unmap(struct rte_pci_ioport *p);

/**
 * Read from a io pci resource.
 *
 * @param p
 *   The rte_pci_ioport object from which we want to read.
 * @param data
 *   A data buffer where the bytes should be read into
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into the pci io resource.
 */
void rte_pci_ioport_read(struct rte_pci_ioport *p,
		void *data, size_t len, off_t offset);

/**
 * Write to a io pci resource.
 *
 * @param p
 *   The rte_pci_ioport object to which we want to write.
 * @param data
 *   A data buffer where the bytes should be read into
 * @param len
 *   The length of the data buffer.
 * @param offset
 *   The offset into the pci io resource.
 */
void rte_pci_ioport_write(struct rte_pci_ioport *p,
		const void *data, size_t len, off_t offset);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_BUS_PCI_H_ */
