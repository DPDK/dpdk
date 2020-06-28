/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_FLOW_OS_H_
#define RTE_PMD_MLX5_FLOW_OS_H_

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
