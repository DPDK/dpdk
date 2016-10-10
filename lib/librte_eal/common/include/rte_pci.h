/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
/*   BSD LICENSE
 *
 *   Copyright 2013-2014 6WIND S.A.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef _RTE_PCI_H_
#define _RTE_PCI_H_

/**
 * @file
 *
 * RTE PCI Interface
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

TAILQ_HEAD(pci_device_list, rte_pci_device); /**< PCI devices in D-linked Q. */
TAILQ_HEAD(pci_driver_list, rte_pci_driver); /**< PCI drivers in D-linked Q. */

extern struct pci_driver_list pci_driver_list; /**< Global list of PCI drivers. */
extern struct pci_device_list pci_device_list; /**< Global list of PCI devices. */

/** Pathname of PCI devices directory. */
const char *pci_get_sysfs_path(void);

/** Formatting string for PCI device identifier: Ex: 0000:00:01.0 */
#define PCI_PRI_FMT "%.4" PRIx16 ":%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8
#define PCI_PRI_STR_SIZE sizeof("XXXX:XX:XX.X")

/** Short formatting string, without domain, for PCI device: Ex: 00:01.0 */
#define PCI_SHORT_PRI_FMT "%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8

/** Nb. of values in PCI device identifier format string. */
#define PCI_FMT_NVAL 4

/** Nb. of values in PCI resource format. */
#define PCI_RESOURCE_FMT_NVAL 3

/** Maximum number of PCI resources. */
#define PCI_MAX_RESOURCE 6

/**
 * A structure describing an ID for a PCI driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_pci_id {
	uint32_t class_id;            /**< Class ID (class, subclass, pi) or RTE_CLASS_ANY_ID. */
	uint16_t vendor_id;           /**< Vendor ID or PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or PCI_ANY_ID. */
	uint16_t subsystem_vendor_id; /**< Subsystem vendor ID or PCI_ANY_ID. */
	uint16_t subsystem_device_id; /**< Subsystem device ID or PCI_ANY_ID. */
};

/**
 * A structure describing the location of a PCI device.
 */
struct rte_pci_addr {
	uint16_t domain;                /**< Device domain */
	uint8_t bus;                    /**< Device bus */
	uint8_t devid;                  /**< Device ID */
	uint8_t function;               /**< Device function. */
};

struct rte_devargs;

enum rte_kernel_driver {
	RTE_KDRV_UNKNOWN = 0,
	RTE_KDRV_IGB_UIO,
	RTE_KDRV_VFIO,
	RTE_KDRV_UIO_GENERIC,
	RTE_KDRV_NIC_UIO,
	RTE_KDRV_NONE,
};

/**
 * A structure describing a PCI device.
 */
struct rte_pci_device {
	TAILQ_ENTRY(rte_pci_device) next;       /**< Next probed PCI device. */
	struct rte_device device;               /**< Inherit core device */
	struct rte_pci_addr addr;               /**< PCI location. */
	struct rte_pci_id id;                   /**< PCI ID. */
	struct rte_mem_resource mem_resource[PCI_MAX_RESOURCE];
						/**< PCI Memory Resource */
	struct rte_intr_handle intr_handle;     /**< Interrupt handle */
	struct rte_pci_driver *driver;          /**< Associated driver */
	uint16_t max_vfs;                       /**< sriov enable if not zero */
	enum rte_kernel_driver kdrv;            /**< Kernel driver passthrough */
};

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

struct rte_pci_driver;

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
	TAILQ_ENTRY(rte_pci_driver) next;       /**< Next in list. */
	struct rte_driver driver;               /**< Inherit core driver. */
	pci_probe_t *probe;                     /**< Device Probe function. */
	pci_remove_t *remove;                   /**< Device Remove function. */
	const struct rte_pci_id *id_table;	/**< ID table, NULL terminated. */
	uint32_t drv_flags;                     /**< Flags contolling handling of device. */
};

/** Device needs PCI BAR mapping (done with either IGB_UIO or VFIO) */
#define RTE_PCI_DRV_NEED_MAPPING 0x0001
/** Device needs to be unbound even if no module is provided */
#define RTE_PCI_DRV_FORCE_UNBIND 0x0004
/** Device driver supports link state interrupt */
#define RTE_PCI_DRV_INTR_LSC	0x0008
/** Device driver supports detaching capability */
#define RTE_PCI_DRV_DETACHABLE	0x0010

/**
 * A structure describing a PCI mapping.
 */
struct pci_map {
	void *addr;
	char *path;
	uint64_t offset;
	uint64_t size;
	uint64_t phaddr;
};

/**
 * A structure describing a mapped PCI resource.
 * For multi-process we need to reproduce all PCI mappings in secondary
 * processes, so save them in a tailq.
 */
struct mapped_pci_resource {
	TAILQ_ENTRY(mapped_pci_resource) next;

	struct rte_pci_addr pci_addr;
	char path[PATH_MAX];
	int nb_maps;
	struct pci_map maps[PCI_MAX_RESOURCE];
};

/** mapped pci device list */
TAILQ_HEAD(mapped_pci_res_list, mapped_pci_resource);

/**< Internal use only - Macro used by pci addr parsing functions **/
#define GET_PCIADDR_FIELD(in, fd, lim, dlm)                   \
do {                                                               \
	unsigned long val;                                      \
	char *end;                                              \
	errno = 0;                                              \
	val = strtoul((in), &end, 16);                          \
	if (errno != 0 || end[0] != (dlm) || val > (lim))       \
		return -EINVAL;                                 \
	(fd) = (typeof (fd))val;                                \
	(in) = end + 1;                                         \
} while(0)

/**
 * Utility function to produce a PCI Bus-Device-Function value
 * given a string representation. Assumes that the BDF is provided without
 * a domain prefix (i.e. domain returned is always 0)
 *
 * @param input
 *	The input string to be parsed. Should have the format XX:XX.X
 * @param dev_addr
 *	The PCI Bus-Device-Function address to be returned. Domain will always be
 *	returned as 0
 * @return
 *  0 on success, negative on error.
 */
static inline int
eal_parse_pci_BDF(const char *input, struct rte_pci_addr *dev_addr)
{
	dev_addr->domain = 0;
	GET_PCIADDR_FIELD(input, dev_addr->bus, UINT8_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->devid, UINT8_MAX, '.');
	GET_PCIADDR_FIELD(input, dev_addr->function, UINT8_MAX, 0);
	return 0;
}

/**
 * Utility function to produce a PCI Bus-Device-Function value
 * given a string representation. Assumes that the BDF is provided including
 * a domain prefix.
 *
 * @param input
 *	The input string to be parsed. Should have the format XXXX:XX:XX.X
 * @param dev_addr
 *	The PCI Bus-Device-Function address to be returned
 * @return
 *  0 on success, negative on error.
 */
static inline int
eal_parse_pci_DomBDF(const char *input, struct rte_pci_addr *dev_addr)
{
	GET_PCIADDR_FIELD(input, dev_addr->domain, UINT16_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->bus, UINT8_MAX, ':');
	GET_PCIADDR_FIELD(input, dev_addr->devid, UINT8_MAX, '.');
	GET_PCIADDR_FIELD(input, dev_addr->function, UINT8_MAX, 0);
	return 0;
}
#undef GET_PCIADDR_FIELD

/**
 * Utility function to write a pci device name, this device name can later be
 * used to retrieve the corresponding rte_pci_addr using eal_parse_pci_*
 * BDF helpers.
 *
 * @param addr
 *	The PCI Bus-Device-Function address
 * @param output
 *	The output buffer string
 * @param size
 *	The output buffer size
 */
static inline void
rte_eal_pci_device_name(const struct rte_pci_addr *addr,
		    char *output, size_t size)
{
	RTE_VERIFY(size >= PCI_PRI_STR_SIZE);
	RTE_VERIFY(snprintf(output, size, PCI_PRI_FMT,
			    addr->domain, addr->bus,
			    addr->devid, addr->function) >= 0);
}

/* Compare two PCI device addresses. */
/**
 * Utility function to compare two PCI device addresses.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to compare
 * @param addr2
 *	The PCI Bus-Device-Function address to compare
 * @return
 *	0 on equal PCI address.
 *	Positive on addr is greater than addr2.
 *	Negative on addr is less than addr2, or error.
 */
static inline int
rte_eal_compare_pci_addr(const struct rte_pci_addr *addr,
			 const struct rte_pci_addr *addr2)
{
	uint64_t dev_addr, dev_addr2;

	if ((addr == NULL) || (addr2 == NULL))
		return -1;

	dev_addr = (addr->domain << 24) | (addr->bus << 16) |
				(addr->devid << 8) | addr->function;
	dev_addr2 = (addr2->domain << 24) | (addr2->bus << 16) |
				(addr2->devid << 8) | addr2->function;

	if (dev_addr > dev_addr2)
		return 1;
	else if (dev_addr < dev_addr2)
		return -1;
	else
		return 0;
}

/**
 * Scan the content of the PCI bus, and the devices in the devices
 * list
 *
 * @return
 *  0 on success, negative on error
 */
int rte_eal_pci_scan(void);

/**
 * Probe the PCI bus for registered drivers.
 *
 * Scan the content of the PCI bus, and call the probe() function for
 * all registered drivers that have a matching entry in its id_table
 * for discovered devices.
 *
 * @return
 *   - 0 on success.
 *   - Negative on error.
 */
int rte_eal_pci_probe(void);

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
int rte_eal_pci_map_device(struct rte_pci_device *dev);

/**
 * Unmap this device
 *
 * @param dev
 *   A pointer to a rte_pci_device structure describing the device
 *   to use
 */
void rte_eal_pci_unmap_device(struct rte_pci_device *dev);

/**
 * @internal
 * Map a particular resource from a file.
 *
 * @param requested_addr
 *      The starting address for the new mapping range.
 * @param fd
 *      The file descriptor.
 * @param offset
 *      The offset for the mapping range.
 * @param size
 *      The size for the mapping range.
 * @param additional_flags
 *      The additional flags for the mapping range.
 * @return
 *   - On success, the function returns a pointer to the mapped area.
 *   - On error, the value MAP_FAILED is returned.
 */
void *pci_map_resource(void *requested_addr, int fd, off_t offset,
		size_t size, int additional_flags);

/**
 * @internal
 * Unmap a particular resource.
 *
 * @param requested_addr
 *      The address for the unmapping range.
 * @param size
 *      The size for the unmapping range.
 */
void pci_unmap_resource(void *requested_addr, size_t size);

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
int rte_eal_pci_probe_one(const struct rte_pci_addr *addr);

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
int rte_eal_pci_detach(const struct rte_pci_addr *addr);

/**
 * Dump the content of the PCI bus.
 *
 * @param f
 *   A pointer to a file for output
 */
void rte_eal_pci_dump(FILE *f);

/**
 * Register a PCI driver.
 *
 * @param driver
 *   A pointer to a rte_pci_driver structure describing the driver
 *   to be registered.
 */
void rte_eal_pci_register(struct rte_pci_driver *driver);

/** Helper for PCI device registration from driver (eth, crypto) instance */
#define RTE_PMD_REGISTER_PCI(nm, pci_drv) \
RTE_INIT(pciinitfn_ ##nm); \
static void pciinitfn_ ##nm(void) \
{\
	(pci_drv).driver.name = RTE_STR(nm);\
	rte_eal_pci_register(&pci_drv); \
} \
RTE_PMD_EXPORT_NAME(nm, __COUNTER__)

/**
 * Unregister a PCI driver.
 *
 * @param driver
 *   A pointer to a rte_pci_driver structure describing the driver
 *   to be unregistered.
 */
void rte_eal_pci_unregister(struct rte_pci_driver *driver);

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
int rte_eal_pci_read_config(const struct rte_pci_device *device,
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
int rte_eal_pci_write_config(const struct rte_pci_device *device,
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
int rte_eal_pci_ioport_map(struct rte_pci_device *dev, int bar,
			   struct rte_pci_ioport *p);

/**
 * Release any resources used in a rte_pci_ioport object.
 *
 * @param p
 *   The rte_pci_ioport object to be uninitialized.
 * @return
 *  0 on success, negative on error.
 */
int rte_eal_pci_ioport_unmap(struct rte_pci_ioport *p);

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
void rte_eal_pci_ioport_read(struct rte_pci_ioport *p,
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
void rte_eal_pci_ioport_write(struct rte_pci_ioport *p,
			      const void *data, size_t len, off_t offset);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PCI_H_ */
