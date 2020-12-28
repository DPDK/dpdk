/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_OS_H_
#define RTE_PMD_MLX5_OS_H_

#include "mlx5_win_ext.h"

enum {
	MLX5_FS_NAME_MAX = MLX5_DEVX_DEVICE_NAME_SIZE + 1,
	MLX5_FS_PATH_MAX = MLX5_DEVX_DEVICE_PNP_SIZE + 1
};

#define PCI_DRV_FLAGS 0

#define MLX5_NAMESIZE MLX5_FS_NAME_MAX

#endif /* RTE_PMD_MLX5_OS_H_ */
