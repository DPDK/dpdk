/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */
#include <rte_errno.h>
#include <rte_log.h>

#include <rte_string_fns.h>
#include <rte_eal_memconfig.h>

#include "private.h"

/* The functions below are not implemented on Windows,
 * but need to be defined for compilation purposes
 */

/* Map pci device */
int
rte_pci_map_device(struct rte_pci_device *dev __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return 0;
}

/* Unmap pci device */
void
rte_pci_unmap_device(struct rte_pci_device *dev __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
}

int
pci_update_device(const struct rte_pci_addr *addr __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return 0;
}

/* Read PCI config space. */
int
rte_pci_read_config(const struct rte_pci_device *dev __rte_unused,
	void *buf __rte_unused, size_t len __rte_unused,
	off_t offset __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return 0;
}

/* Write PCI config space. */
int
rte_pci_write_config(const struct rte_pci_device *dev __rte_unused,
	const void *buf __rte_unused, size_t len __rte_unused,
	off_t offset __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return 0;
}

enum rte_iova_mode
pci_device_iova_mode(const struct rte_pci_driver *pdrv __rte_unused,
		const struct rte_pci_device *pdev __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return RTE_IOVA_DC;
}

int
rte_pci_ioport_map(struct rte_pci_device *dev __rte_unused,
	int bar __rte_unused, struct rte_pci_ioport *p __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return -1;
}


void
rte_pci_ioport_read(struct rte_pci_ioport *p __rte_unused,
	void *data __rte_unused, size_t len __rte_unused,
	off_t offset __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
}

int
rte_pci_ioport_unmap(struct rte_pci_ioport *p __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return -1;
}

bool
pci_device_iommu_support_va(const struct rte_pci_device *dev __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return false;
}

void
rte_pci_ioport_write(struct rte_pci_ioport *p __rte_unused,
		const void *data __rte_unused, size_t len __rte_unused,
		off_t offset __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
}


/* remap the PCI resource of a PCI device in anonymous virtual memory */
int
pci_uio_remap_resource(struct rte_pci_device *dev __rte_unused)
{
	/* This function is not implemented on Windows.
	 * We really should short-circuit the call to these functions by
	 * clearing the RTE_PCI_DRV_NEED_MAPPING flag
	 * in the rte_pci_driver flags.
	 */
	return -1;
}
/*
 * Scan the contents of the PCI bus
 * and add all network class devices into the devices list.
 */
int
rte_pci_scan(void)
{
	return 0;
}
