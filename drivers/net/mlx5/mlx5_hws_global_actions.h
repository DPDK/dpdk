/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#ifndef RTE_PMD_MLX5_HWS_GLOBAL_ACTIONS_H_
#define RTE_PMD_MLX5_HWS_GLOBAL_ACTIONS_H_

#include <stdint.h>

#include <rte_spinlock.h>

#include "hws/mlx5dr.h"

struct mlx5_priv;

enum mlx5_hws_global_action_index {
	MLX5_HWS_GLOBAL_ACTION_ROOT,
	MLX5_HWS_GLOBAL_ACTION_NON_ROOT,
	MLX5_HWS_GLOBAL_ACTION_MAX,
};

struct mlx5_hws_global_actions_array {
	struct mlx5dr_action *arr[MLX5_HWS_GLOBAL_ACTION_MAX][MLX5DR_TABLE_TYPE_MAX];
};

struct mlx5_hws_global_actions {
	struct mlx5_hws_global_actions_array drop;
	struct mlx5_hws_global_actions_array tag;
	struct mlx5_hws_global_actions_array pop_vlan;
	struct mlx5_hws_global_actions_array push_vlan;
	struct mlx5_hws_global_actions_array send_to_kernel;
	rte_spinlock_t lock;
};

void mlx5_hws_global_actions_init(struct mlx5_priv *priv);

void mlx5_hws_global_actions_cleanup(struct mlx5_priv *priv);

struct mlx5dr_action *mlx5_hws_global_action_drop_get(struct mlx5_priv *priv,
						      enum mlx5dr_table_type table_type,
						      bool is_root);

struct mlx5dr_action *mlx5_hws_global_action_tag_get(struct mlx5_priv *priv,
						     enum mlx5dr_table_type table_type,
						     bool is_root);

struct mlx5dr_action *mlx5_hws_global_action_pop_vlan_get(struct mlx5_priv *priv,
							  enum mlx5dr_table_type table_type,
							  bool is_root);

struct mlx5dr_action *mlx5_hws_global_action_push_vlan_get(struct mlx5_priv *priv,
							   enum mlx5dr_table_type table_type,
							   bool is_root);

struct mlx5dr_action *mlx5_hws_global_action_send_to_kernel_get(struct mlx5_priv *priv,
								 enum mlx5dr_table_type table_type,
								 uint16_t priority);

#endif /* !RTE_PMD_MLX5_HWS_GLOBAL_ACTIONS_H_ */
