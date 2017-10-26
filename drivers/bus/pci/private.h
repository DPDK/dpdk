/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 6WIND. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of 6WIND nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
