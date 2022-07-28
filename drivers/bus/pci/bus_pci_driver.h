/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#ifndef BUS_PCI_DRIVER_H
#define BUS_PCI_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_bus_pci.h>
#include <dev_driver.h>
#include <rte_compat.h>

/** Pathname of PCI devices directory. */
__rte_internal
const char *rte_pci_get_sysfs_path(void);

enum rte_pci_kernel_driver {
	RTE_PCI_KDRV_UNKNOWN = 0,  /* may be misc UIO or bifurcated driver */
	RTE_PCI_KDRV_IGB_UIO,      /* igb_uio for Linux */
	RTE_PCI_KDRV_VFIO,         /* VFIO for Linux */
	RTE_PCI_KDRV_UIO_GENERIC,  /* uio_pci_generic for Linux */
	RTE_PCI_KDRV_NIC_UIO,      /* nic_uio for FreeBSD */
	RTE_PCI_KDRV_NONE,         /* no attached driver */
	RTE_PCI_KDRV_NET_UIO,      /* NetUIO for Windows */
};

/**
 * A structure describing a PCI device.
 */
struct rte_pci_device {
	RTE_TAILQ_ENTRY(rte_pci_device) next;   /**< Next probed PCI device. */
	struct rte_device device;           /**< Inherit core device */
	struct rte_pci_addr addr;           /**< PCI location. */
	struct rte_pci_id id;               /**< PCI ID. */
	struct rte_mem_resource mem_resource[PCI_MAX_RESOURCE];
					    /**< PCI Memory Resource */
	struct rte_intr_handle *intr_handle; /**< Interrupt handle */
	struct rte_pci_driver *driver;      /**< PCI driver used in probing */
	uint16_t max_vfs;                   /**< sriov enable if not zero */
	enum rte_pci_kernel_driver kdrv;    /**< Kernel driver passthrough */
	char name[PCI_PRI_STR_SIZE+1];      /**< PCI location (ASCII) */
	char *bus_info;                     /**< PCI bus specific info */
	struct rte_intr_handle *vfio_req_intr_handle;
				/**< Handler of VFIO request interrupt */
};

/**
 * @internal
 * Helper macro for drivers that need to convert to struct rte_pci_device.
 */
#define RTE_DEV_TO_PCI(ptr) container_of(ptr, struct rte_pci_device, device)

#define RTE_DEV_TO_PCI_CONST(ptr) \
	container_of(ptr, const struct rte_pci_device, device)

#define RTE_ETH_DEV_TO_PCI(eth_dev)	RTE_DEV_TO_PCI((eth_dev)->device)

#ifdef __cplusplus
/** C++ macro used to help building up tables of device IDs */
#define RTE_PCI_DEVICE(vend, dev) \
	RTE_CLASS_ANY_ID,         \
	(vend),                   \
	(dev),                    \
	RTE_PCI_ANY_ID,           \
	RTE_PCI_ANY_ID
#else
/** Macro used to help building up tables of device IDs */
#define RTE_PCI_DEVICE(vend, dev)          \
	.class_id = RTE_CLASS_ANY_ID,      \
	.vendor_id = (vend),               \
	.device_id = (dev),                \
	.subsystem_vendor_id = RTE_PCI_ANY_ID, \
	.subsystem_device_id = RTE_PCI_ANY_ID
#endif

/**
 * Initialisation function for the driver called during PCI probing.
 */
typedef int (rte_pci_probe_t)(struct rte_pci_driver *, struct rte_pci_device *);

/**
 * Uninitialisation function for the driver called during hotplugging.
 */
typedef int (rte_pci_remove_t)(struct rte_pci_device *);

/**
 * Driver-specific DMA mapping. After a successful call the device
 * will be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the PCI device.
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
typedef int (pci_dma_map_t)(struct rte_pci_device *dev, void *addr,
			    uint64_t iova, size_t len);

/**
 * Driver-specific DMA un-mapping. After a successful call the device
 * will not be able to read/write from/to this segment.
 *
 * @param dev
 *   Pointer to the PCI device.
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
typedef int (pci_dma_unmap_t)(struct rte_pci_device *dev, void *addr,
			      uint64_t iova, size_t len);

/**
 * A structure describing a PCI driver.
 */
struct rte_pci_driver {
	RTE_TAILQ_ENTRY(rte_pci_driver) next;  /**< Next in list. */
	struct rte_driver driver;          /**< Inherit core driver. */
	rte_pci_probe_t *probe;            /**< Device probe function. */
	rte_pci_remove_t *remove;          /**< Device remove function. */
	pci_dma_map_t *dma_map;		   /**< device dma map function. */
	pci_dma_unmap_t *dma_unmap;	   /**< device dma unmap function. */
	const struct rte_pci_id *id_table; /**< ID table, NULL terminated. */
	uint32_t drv_flags;                /**< Flags RTE_PCI_DRV_*. */
};

/** Device needs PCI BAR mapping (done with either IGB_UIO or VFIO) */
#define RTE_PCI_DRV_NEED_MAPPING 0x0001
/** Device needs PCI BAR mapping with enabled write combining (wc) */
#define RTE_PCI_DRV_WC_ACTIVATE 0x0002
/** Device already probed can be probed again to check for new ports. */
#define RTE_PCI_DRV_PROBE_AGAIN 0x0004
/** Device driver supports link state interrupt */
#define RTE_PCI_DRV_INTR_LSC	0x0008
/** Device driver supports device removal interrupt */
#define RTE_PCI_DRV_INTR_RMV 0x0010
/** Device driver needs to keep mapped resources if unsupported dev detected */
#define RTE_PCI_DRV_KEEP_MAPPED_RES 0x0020
/** Device driver needs IOVA as VA and cannot work with IOVA as PA */
#define RTE_PCI_DRV_NEED_IOVA_AS_VA 0x0040

/**
 * Register a PCI driver.
 *
 * @param driver
 *   A pointer to a rte_pci_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void rte_pci_register(struct rte_pci_driver *driver);

/** Helper for PCI device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_PCI(nm, pci_drv) \
RTE_INIT(pciinitfn_ ##nm) \
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
__rte_internal
void rte_pci_unregister(struct rte_pci_driver *driver);

/*
 * A structure used to access io resources for a pci device.
 * rte_pci_ioport is arch, os, driver specific, and should not be used outside
 * of pci ioport api.
 */
struct rte_pci_ioport {
	struct rte_pci_device *dev;
	uint64_t base;
	uint64_t len; /* only filled for memory mapped ports */
};

#ifdef __cplusplus
}
#endif

#endif /* BUS_PCI_DRIVER_H */
