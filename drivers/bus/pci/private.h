/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 6WIND S.A.
 */

#ifndef _PCI_PRIVATE_H_
#define _PCI_PRIVATE_H_

#include <stdbool.h>
#include <stdio.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>

struct rte_pci_driver;
struct rte_pci_device;

/**
 * Probe the PCI bus
 *
 * @return
 *   - 0 on success.
 *   - !0 on error.
 */
int
rte_pci_probe(void);

/**
 * Scan the content of the PCI bus, and the devices in the devices
 * list
 *
 * @return
 *  0 on success, negative on error
 */
int rte_pci_scan(void);

/**
 * Probe the single PCI device.
 *
 * Scan the content of the PCI bus, and find the pci device specified by pci
 * address, then call the probe() function for registered driver that has a
 * matching entry in its id_table for discovered device.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to probe.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_pci_probe_one(const struct rte_pci_addr *addr);

/**
 * Close the single PCI device.
 *
 * Scan the content of the PCI bus, and find the pci device specified by pci
 * address, then call the remove() function for registered driver that has a
 * matching entry in its id_table for discovered device.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to close.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_pci_detach(const struct rte_pci_addr *addr);

/**
 * Find the name of a PCI device.
 */
void
pci_name_set(struct rte_pci_device *dev);

/**
 * Add a PCI device to the PCI Bus (append to PCI Device list). This function
 * also updates the bus references of the PCI Device (and the generic device
 * object embedded within.
 *
 * @param pci_dev
 *	PCI device to add
 * @return void
 */
void rte_pci_add_device(struct rte_pci_device *pci_dev);

/**
 * Insert a PCI device in the PCI Bus at a particular location in the device
 * list. It also updates the PCI Bus reference of the new devices to be
 * inserted.
 *
 * @param exist_pci_dev
 *	Existing PCI device in PCI Bus
 * @param new_pci_dev
 *	PCI device to be added before exist_pci_dev
 * @return void
 */
void rte_pci_insert_device(struct rte_pci_device *exist_pci_dev,
		struct rte_pci_device *new_pci_dev);

/**
 * Remove a PCI device from the PCI Bus. This sets to NULL the bus references
 * in the PCI device object as well as the generic device object.
 *
 * @param pci_device
 *	PCI device to be removed from PCI Bus
 * @return void
 */
void rte_pci_remove_device(struct rte_pci_device *pci_device);

/**
 * Update a pci device object by asking the kernel for the latest information.
 *
 * This function is private to EAL.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to look for
 * @return
 *   - 0 on success.
 *   - negative on error.
 */
int pci_update_device(const struct rte_pci_addr *addr);

/**
 * Unbind kernel driver for this device
 *
 * This function is private to EAL.
 *
 * @return
 *   0 on success, negative on error
 */
int pci_unbind_kernel_driver(struct rte_pci_device *dev);

/**
 * Map the PCI resource of a PCI device in virtual memory
 *
 * This function is private to EAL.
 *
 * @return
 *   0 on success, negative on error
 */
int pci_uio_map_resource(struct rte_pci_device *dev);

/**
 * Unmap the PCI resource of a PCI device
 *
 * This function is private to EAL.
 */
void pci_uio_unmap_resource(struct rte_pci_device *dev);

/**
 * Allocate uio resource for PCI device
 *
 * This function is private to EAL.
 *
 * @param dev
 *   PCI device to allocate uio resource
 * @param uio_res
 *   Pointer to uio resource.
 *   If the function returns 0, the pointer will be filled.
 * @return
 *   0 on success, negative on error
 */
int pci_uio_alloc_resource(struct rte_pci_device *dev,
		struct mapped_pci_resource **uio_res);

/**
 * Free uio resource for PCI device
 *
 * This function is private to EAL.
 *
 * @param dev
 *   PCI device to free uio resource
 * @param uio_res
 *   Pointer to uio resource.
 */
void pci_uio_free_resource(struct rte_pci_device *dev,
		struct mapped_pci_resource *uio_res);

/**
 * Map device memory to uio resource
 *
 * This function is private to EAL.
 *
 * @param dev
 *   PCI device that has memory information.
 * @param res_idx
 *   Memory resource index of the PCI device.
 * @param uio_res
 *  uio resource that will keep mapping information.
 * @param map_idx
 *   Mapping information index of the uio resource.
 * @return
 *   0 on success, negative on error
 */
int pci_uio_map_resource_by_index(struct rte_pci_device *dev, int res_idx,
		struct mapped_pci_resource *uio_res, int map_idx);

/*
 * Match the PCI Driver and Device using the ID Table
 *
 * @param pci_drv
 *      PCI driver from which ID table would be extracted
 * @param pci_dev
 *      PCI device to match against the driver
 * @return
 *      1 for successful match
 *      0 for unsuccessful match
 */
int
rte_pci_match(const struct rte_pci_driver *pci_drv,
	      const struct rte_pci_device *pci_dev);

/**
 * Get iommu class of PCI devices on the bus.
 * And return their preferred iova mapping mode.
 *
 * @return
 *   - enum rte_iova_mode.
 */
enum rte_iova_mode
rte_pci_get_iommu_class(void);

#endif /* _PCI_PRIVATE_H_ */
