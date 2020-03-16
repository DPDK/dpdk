/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#ifndef _RTE_PCI_H_
#define _RTE_PCI_H_

/**
 * @file
 *
 * RTE PCI Library
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <limits.h>
#include <sys/queue.h>
#include <inttypes.h>

/** Formatting string for PCI device identifier: Ex: 0000:00:01.0 */
#define PCI_PRI_FMT "%.4" PRIx16 ":%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8
#define PCI_PRI_STR_SIZE sizeof("XXXXXXXX:XX:XX.X")

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
	uint32_t class_id;            /**< Class ID or RTE_CLASS_ANY_ID. */
	uint16_t vendor_id;           /**< Vendor ID or PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or PCI_ANY_ID. */
	uint16_t subsystem_vendor_id; /**< Subsystem vendor ID or PCI_ANY_ID. */
	uint16_t subsystem_device_id; /**< Subsystem device ID or PCI_ANY_ID. */
};

/**
 * A structure describing the location of a PCI device.
 */
struct rte_pci_addr {
	uint32_t domain;                /**< Device domain */
	uint8_t bus;                    /**< Device bus */
	uint8_t devid;                  /**< Device ID */
	uint8_t function;               /**< Device function. */
};

/** Any PCI device identifier (vendor, device, ...) */
#define PCI_ANY_ID (0xffff)
#define RTE_CLASS_ANY_ID (0xffffff)

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

struct pci_msix_table {
	int bar_index;
	uint32_t offset;
	uint32_t size;
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
	struct pci_msix_table msix_table;
};


/** mapped pci device list */
TAILQ_HEAD(mapped_pci_res_list, mapped_pci_resource);

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
void rte_pci_device_name(const struct rte_pci_addr *addr,
		     char *output, size_t size);

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
int rte_pci_addr_cmp(const struct rte_pci_addr *addr,
		     const struct rte_pci_addr *addr2);


/**
 * Utility function to parse a string into a PCI location.
 *
 * @param str
 *	The string to parse
 * @param addr
 *	The reference to the structure where the location
 *	is stored.
 * @return
 *	0 on success
 *	<0 otherwise
 */
int rte_pci_addr_parse(const char *str, struct rte_pci_addr *addr);

/**
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
 * Unmap a particular resource.
 *
 * @param requested_addr
 *      The address for the unmapping range.
 * @param size
 *      The size for the unmapping range.
 */
void pci_unmap_resource(void *requested_addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PCI_H_ */
