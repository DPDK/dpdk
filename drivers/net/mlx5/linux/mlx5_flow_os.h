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

/**
 * Create flow rule.
 *
 * @param[in] matcher
 *   Pointer to match mask structure.
 * @param[in] match_value
 *   Pointer to match value structure.
 * @param[in] num_actions
 *   Number of actions in flow rule.
 * @param[in] actions
 *   Pointer to array of flow rule actions.
 * @param[out] flow
 *   Pointer to a valid flow rule object on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow(void *matcher, void *match_value,
			 size_t num_actions, void *actions[], void **flow)
{
	*flow = mlx5_glue->dv_create_flow(matcher, match_value,
					  num_actions, actions);
	return (*flow) ? 0 : -1;
}

/**
 * Destroy flow rule.
 *
 * @param[in] drv_flow_ptr
 *   Pointer to flow rule object.
 *
 * @return
 *   0 on success, or the value of errno on failure.
 */
static inline int
mlx5_flow_os_destroy_flow(void *drv_flow_ptr)
{
	return mlx5_glue->dv_destroy_flow(drv_flow_ptr);
}

/**
 * Create flow table.
 *
 * @param[in] domain
 *   Pointer to relevant domain.
 * @param[in] table_id
 *   Table ID.
 * @param[out] table
 *   Pointer to a valid flow table object on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_tbl(void *domain, uint32_t table_id, void **table)
{
	*table = mlx5_glue->dr_create_flow_tbl(domain, table_id);
	return (*table) ? 0 : -1;
}

/**
 * Destroy flow table.
 *
 * @param[in] table
 *   Pointer to table object to destroy.
 *
 * @return
 *   0 on success, or the value of errno on failure.
 */
static inline int
mlx5_flow_os_destroy_flow_tbl(void *table)
{
	return mlx5_glue->dr_destroy_flow_tbl(table);
}

/**
 * Create flow matcher in a flow table.
 *
 * @param[in] ctx
 *   Pointer to relevant device context.
 * @param[in] attr
 *   Pointer to relevant attributes.
 * @param[in] table
 *   Pointer to table object.
 * @param[out] matcher
 *   Pointer to a valid flow matcher object on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_matcher(void *ctx, void *attr, void *table,
				 void **matcher)
{
	*matcher = mlx5_glue->dv_create_flow_matcher(ctx, attr, table);
	return (*matcher) ? 0 : -1;
}

/**
 * Destroy flow matcher.
 *
 * @param[in] matcher
 *   Pointer to matcher object to destroy.
 *
 * @return
 *   0 on success, or the value of errno on failure.
 */
static inline int
mlx5_flow_os_destroy_flow_matcher(void *matcher)
{
	return mlx5_glue->dv_destroy_flow_matcher(matcher);
}

#endif /* RTE_PMD_MLX5_FLOW_OS_H_ */
