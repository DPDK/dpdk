/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_COMMON_PCI_H__
#define __NFP_COMMON_PCI_H__

#include <bus_pci_driver.h>

/* Initialization function for the driver called during device probing. */
typedef int (nfp_class_driver_probe_t)(struct rte_pci_device *dev);

/* Uninitialization function for the driver called during hot-unplugging. */
typedef int (nfp_class_driver_remove_t)(struct rte_pci_device *dev);

enum nfp_class {
	NFP_CLASS_ETH,
	NFP_CLASS_VDPA,
	NFP_CLASS_INVALID,
};

/* Describing a nfp common class driver. */
struct nfp_class_driver {
	TAILQ_ENTRY(nfp_class_driver) next;
	enum nfp_class drv_class;            /**< Class of this driver. */
	const char *name;                    /**< Driver name. */
	const struct rte_pci_id *id_table;   /**< ID table, NULL terminated. */
	uint32_t drv_flags;                  /**< Flags RTE_PCI_DRV_*. */
	nfp_class_driver_probe_t *probe;     /**< Device probe function. */
	nfp_class_driver_remove_t *remove;   /**< Device remove function. */
};

/**
 * Register a nfp device driver.
 *
 * @param driver
 *   A pointer to a nfp_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void
nfp_class_driver_register(struct nfp_class_driver *driver);

#endif /* __NFP_COMMON_PCI_H__ */
