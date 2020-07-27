/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef _MLX5_COMMON_PCI_H_
#define _MLX5_COMMON_PCI_H_

/**
 * @file
 *
 * RTE Mellanox PCI Driver Interface
 * Mellanox ConnectX PCI device supports multiple class (net/vdpa/regex)
 * devices. This layer enables creating such multiple class of devices on a
 * single PCI device by allowing to bind multiple class specific device
 * driver to attach to mlx5_pci driver.
 *
 * -----------    ------------    -------------
 * |   mlx5  |    |   mlx5   |    |   mlx5    |
 * | net pmd |    | vdpa pmd |    | regex pmd |
 * -----------    ------------    -------------
 *      \              |                 /
 *       \             |                /
 *        \       --------------       /
 *         \______|   mlx5     |_____ /
 *                | pci common |
 *                --------------
 *                     |
 *                 -----------
 *                 |   mlx5  |
 *                 | pci dev |
 *                 -----------
 *
 * - mlx5 pci driver binds to mlx5 PCI devices defined by PCI
 *   ID table of all related mlx5 PCI devices.
 * - mlx5 class driver such as net, vdpa, regex PMD defines its
 *   specific PCI ID table and mlx5 bus driver probes matching
 *   class drivers.
 * - mlx5 pci bus driver is cental place that validates supported
 *   class combinations.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <rte_pci.h>
#include <rte_bus_pci.h>

#include <mlx5_common.h>

void mlx5_common_pci_init(void);

/**
 * A structure describing a mlx5 pci driver.
 */
struct mlx5_pci_driver {
	struct rte_pci_driver pci_driver;	/**< Inherit core pci driver. */
	uint32_t driver_class;	/**< Class of this driver, enum mlx5_class */
	TAILQ_ENTRY(mlx5_pci_driver) next;
};

/**
 * Register a mlx5_pci device driver.
 *
 * @param driver
 *   A pointer to a mlx5_pci_driver structure describing the driver
 *   to be registered.
 */
__rte_internal
void
mlx5_pci_driver_register(struct mlx5_pci_driver *driver);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _MLX5_COMMON_PCI_H_ */
