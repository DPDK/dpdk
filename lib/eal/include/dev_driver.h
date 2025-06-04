/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Red Hat, Inc.
 */

#ifndef DEV_DRIVER_H
#define DEV_DRIVER_H

#include <rte_common.h>
#include <rte_dev.h>

/**
 * A structure describing a device driver.
 */
struct rte_driver {
	RTE_TAILQ_ENTRY(rte_driver) next; /**< Next in list. */
	const char *name;                   /**< Driver name. */
	const char *alias;              /**< Driver alias. */
};

/**
 * A structure describing a generic device.
 */
struct rte_device {
	RTE_TAILQ_ENTRY(rte_device) next; /**< Next device */
	const char *name;             /**< Device name */
	const char *bus_info;         /**< Device bus specific information */
	const struct rte_driver *driver; /**< Driver assigned after probing */
	const struct rte_bus *bus;    /**< Bus handle assigned on scan */
	int numa_node;                /**< NUMA node connection */
	struct rte_devargs *devargs;  /**< Arguments for latest probing */
};

#ifdef RTE_TOOLCHAIN_MSVC
#define RTE_PMD_EXPORT_SYMBOL(type, name) \
__pragma(comment(linker, "/include:" RTE_STR(name))) type name
#else
#define RTE_PMD_EXPORT_SYMBOL(type, name) \
__attribute__((used)) type name
#endif

#define RTE_PMD_EXPORT_NAME(name) \
RTE_PMD_EXPORT_SYMBOL(const char, this_pmd_name ## name)[] = RTE_STR(name)

#define DRV_EXP_TAG(name, tag) __##name##_##tag

#define RTE_PMD_REGISTER_PCI_TABLE(name, table) \
RTE_PMD_EXPORT_SYMBOL(const char, DRV_EXP_TAG(name, pci_tbl_export))[] = RTE_STR(table)

#define RTE_PMD_REGISTER_PARAM_STRING(name, str) \
RTE_PMD_EXPORT_SYMBOL(const char, DRV_EXP_TAG(name, param_string_export))[] = str

/**
 * Advertise the list of kernel modules required to run this driver
 *
 * This string lists the kernel modules required for the devices
 * associated to a PMD. The format of each line of the string is:
 * "<device-pattern> <kmod-expression>".
 *
 * The possible formats for the device pattern are:
 *   "*"                     all devices supported by this driver
 *   "pci:*"                 all PCI devices supported by this driver
 *   "pci:v8086:d*:sv*:sd*"  all PCI devices supported by this driver
 *                           whose vendor id is 0x8086.
 *
 * The format of the kernel modules list is a parenthesized expression
 * containing logical-and (&) and logical-or (|).
 *
 * The device pattern and the kmod expression are separated by a space.
 *
 * Example:
 * - "* igb_uio | uio_pci_generic | vfio"
 */
#define RTE_PMD_REGISTER_KMOD_DEP(name, str) \
RTE_PMD_EXPORT_SYMBOL(const char, DRV_EXP_TAG(name, kmod_dep_export))[] = str

#endif /* DEV_DRIVER_H */
