/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_OS_H_
#define RTE_PMD_MLX5_OS_H_

#include <net/if.h>

/* verb enumerations translations to local enums. */
enum {
	DEV_SYSFS_NAME_MAX = IBV_SYSFS_NAME_MAX + 1,
	DEV_SYSFS_PATH_MAX = IBV_SYSFS_PATH_MAX + 1
};

#define PCI_DRV_FLAGS  (RTE_PCI_DRV_INTR_LSC | \
			RTE_PCI_DRV_INTR_RMV | \
			RTE_PCI_DRV_PROBE_AGAIN)

/* mlx5_ethdev_os.c */

int mlx5_get_ifname(const struct rte_eth_dev *dev, char (*ifname)[IF_NAMESIZE]);
#endif /* RTE_PMD_MLX5_OS_H_ */
