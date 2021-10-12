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

#define MLX5_NAMESIZE IF_NAMESIZE

#define PCI_DRV_FLAGS  (RTE_PCI_DRV_INTR_LSC | \
			RTE_PCI_DRV_INTR_RMV | \
			RTE_PCI_DRV_PROBE_AGAIN)

enum mlx5_sw_parsing_offloads {
#ifdef HAVE_IBV_MLX5_MOD_SWP
	MLX5_SW_PARSING_CAP      = MLX5DV_SW_PARSING,
	MLX5_SW_PARSING_CSUM_CAP = MLX5DV_SW_PARSING_CSUM,
	MLX5_SW_PARSING_TSO_CAP  = MLX5DV_SW_PARSING_LSO,
#else
	MLX5_SW_PARSING_CAP      = 0,
	MLX5_SW_PARSING_CSUM_CAP = 0,
	MLX5_SW_PARSING_TSO_CAP  = 0,
#endif
};
#endif /* RTE_PMD_MLX5_OS_H_ */
