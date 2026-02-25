/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#include "mlx5_hws_global_actions.h"

#include "mlx5.h"

void
mlx5_hws_global_actions_init(struct mlx5_priv *priv)
{
	rte_spinlock_init(&priv->hw_global_actions.lock);
}

void
mlx5_hws_global_actions_cleanup(struct mlx5_priv *priv)
{
	rte_spinlock_lock(&priv->hw_global_actions.lock);

	for (int i = 0; i < MLX5_HWS_GLOBAL_ACTION_MAX; ++i) {
		for (int j = 0; j < MLX5DR_TABLE_TYPE_MAX; ++j) {
			int ret;

			if (priv->hw_global_actions.drop.arr[i][j] == NULL)
				continue;

			ret = mlx5dr_action_destroy(priv->hw_global_actions.drop.arr[i][j]);
			if (ret != 0)
				DRV_LOG(ERR, "port %u failed to free HWS action",
					priv->dev_data->port_id);
			priv->hw_global_actions.drop.arr[i][j] = NULL;
		}
	}

	rte_spinlock_unlock(&priv->hw_global_actions.lock);
}

struct mlx5dr_action *
mlx5_hws_global_action_drop_get(struct mlx5_priv *priv,
				enum mlx5dr_table_type table_type,
				bool is_root)
{
	enum mlx5dr_action_flags action_flags;
	struct mlx5dr_action *action = NULL;
	int ret;

	ret = mlx5dr_table_type_to_action_flags(table_type, is_root, &action_flags);
	if (ret < 0)
		return NULL;

	rte_spinlock_lock(&priv->hw_global_actions.lock);

	action = priv->hw_global_actions.drop.arr[!is_root][table_type];
	if (action != NULL)
		goto unlock_ret;

	action = mlx5dr_action_create_dest_drop(priv->dr_ctx, action_flags);
	if (action == NULL) {
		DRV_LOG(ERR, "port %u failed to create drop HWS action", priv->dev_data->port_id);
		goto unlock_ret;
	}

	priv->hw_global_actions.drop.arr[!is_root][table_type] = action;

unlock_ret:
	rte_spinlock_unlock(&priv->hw_global_actions.lock);
	return action;
}
