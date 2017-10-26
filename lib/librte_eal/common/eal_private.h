/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef _EAL_PRIVATE_H_
#define _EAL_PRIVATE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <rte_pci.h>

/**
 * Initialize the memzone subsystem (private to eal).
 *
 * @return
 *   - 0 on success
 *   - Negative on error
 */
int rte_eal_memzone_init(void);

/**
 * Common log initialization function (private to eal).  Determines
 * where log data is written when no call to rte_openlog_stream is
 * in effect.
 *
 * @param default_log
 *   The default log stream to be used.
 * @return
 *   - 0 on success
 *   - Negative on error
 */
void eal_log_set_default(FILE *default_log);

/**
 * Fill configuration with number of physical and logical processors
 *
 * This function is private to EAL.
 *
 * Parse /proc/cpuinfo to get the number of physical and logical
 * processors on the machine.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_cpu_init(void);

/**
 * Map memory
 *
 * This function is private to EAL.
 *
 * Fill configuration structure with these infos, and return 0 on success.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_memory_init(void);

/**
 * Configure timers
 *
 * This function is private to EAL.
 *
 * Mmap memory areas used by HPET (high precision event timer) that will
 * provide our time reference, and configure the TSC frequency also for it
 * to be used as a reference.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_timer_init(void);

/**
 * Init the default log stream
 *
 * This function is private to EAL.
 *
 * @return
 *   0 on success, negative on error
 */
int rte_eal_log_init(const char *id, int facility);

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
 *     The PCI Bus-Device-Function address to probe.
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
 *     The PCI Bus-Device-Function address to close.
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_pci_detach(const struct rte_pci_addr *addr);

/**
 * Find the name of a PCI device.
 */
void pci_name_set(struct rte_pci_device *dev);

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
 * Init tail queues for non-EAL library structures. This is to allow
 * the rings, mempools, etc. lists to be shared among multiple processes
 *
 * This function is private to EAL
 *
 * @return
 *    0 on success, negative on error
 */
int rte_eal_tailqs_init(void);

/**
 * Init interrupt handling.
 *
 * This function is private to EAL.
 *
 * @return
 *  0 on success, negative on error
 */
int rte_eal_intr_init(void);

/**
 * Init alarm mechanism. This is to allow a callback be called after
 * specific time.
 *
 * This function is private to EAL.
 *
 * @return
 *  0 on success, negative on error
 */
int rte_eal_alarm_init(void);

/**
 * Function is to check if the kernel module(like, vfio, vfio_iommu_type1,
 * etc.) loaded.
 *
 * @param module_name
 *	The module's name which need to be checked
 *
 * @return
 *	-1 means some error happens(NULL pointer or open failure)
 *	0  means the module not loaded
 *	1  means the module loaded
 */
int rte_eal_check_module(const char *module_name);

/**
 * Get cpu core_id.
 *
 * This function is private to the EAL.
 */
unsigned eal_cpu_core_id(unsigned lcore_id);

/**
 * Check if cpu is present.
 *
 * This function is private to the EAL.
 */
int eal_cpu_detected(unsigned lcore_id);

/**
 * Set TSC frequency from precise value or estimation
 *
 * This function is private to the EAL.
 */
void set_tsc_freq(void);

/**
 * Get precise TSC frequency from system
 *
 * This function is private to the EAL.
 */
uint64_t get_tsc_freq(void);

/**
 * Get TSC frequency if the architecture supports.
 *
 * This function is private to the EAL.
 *
 * @return
 *   The number of TSC cycles in one second.
 *   Returns zero if the architecture support is not available.
 */
uint64_t get_tsc_freq_arch(void);

/**
 * Prepare physical memory mapping
 * i.e. hugepages on Linux and
 *      contigmem on BSD.
 *
 * This function is private to the EAL.
 */
int rte_eal_hugepage_init(void);

/**
 * Creates memory mapping in secondary process
 * i.e. hugepages on Linux and
 *      contigmem on BSD.
 *
 * This function is private to the EAL.
 */
int rte_eal_hugepage_attach(void);

/**
 * Find a bus capable of identifying a device.
 *
 * @param str
 *   A device identifier (PCI address, virtual PMD name, ...).
 *
 * @return
 *   A valid bus handle if found.
 *   NULL if no bus is able to parse this device.
 */
struct rte_bus *rte_bus_find_by_device_name(const char *str);

#endif /* _EAL_PRIVATE_H_ */
