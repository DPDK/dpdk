/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_FLOW_OS_H_
#define RTE_PMD_MLX5_FLOW_OS_H_

#include "mlx5_flow.h"

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
extern const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops;
#endif

/**
 * Get OS enforced flow type. MLX5_FLOW_TYPE_MAX means "non enforced type".
 *
 * @return
 *   Flow type (MLX5_FLOW_TYPE_MAX)
 */
static inline enum mlx5_flow_drv_type
mlx5_flow_os_get_type(void)
{
	return MLX5_FLOW_TYPE_MAX;
}

/**
 * Check if item type is supported.
 *
 * @param item
 *   Item type to check.
 *
 * @return
 *   True is this item type is supported, false if not supported.
 */
static inline bool
mlx5_flow_os_item_supported(int item __rte_unused)
{
	return true;
}

/**
 * Check if action type is supported.
 *
 * @param action
 *   Action type to check.
 *
 * @return
 *   True is this action type is supported, false if not supported.
 */
static inline bool
mlx5_flow_os_action_supported(int action __rte_unused)
{
	return true;
}

#endif /* RTE_PMD_MLX5_FLOW_OS_H_ */
