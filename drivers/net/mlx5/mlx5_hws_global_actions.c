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

static void
global_actions_array_cleanup(struct mlx5_priv *priv,
			     struct mlx5_hws_global_actions_array *array,
			     const char *name)
{
	for (int i = 0; i < MLX5_HWS_GLOBAL_ACTION_MAX; ++i) {
		for (int j = 0; j < MLX5DR_TABLE_TYPE_MAX; ++j) {
			int ret;

			if (array->arr[i][j] == NULL)
				continue;

			ret = mlx5dr_action_destroy(array->arr[i][j]);
			if (ret != 0)
				DRV_LOG(ERR, "port %u failed to free %s HWS action",
					priv->dev_data->port_id,
					name);
			array->arr[i][j] = NULL;
		}
	}
}

void
mlx5_hws_global_actions_cleanup(struct mlx5_priv *priv)
{
	rte_spinlock_lock(&priv->hw_global_actions.lock);

	global_actions_array_cleanup(priv, &priv->hw_global_actions.drop, "drop");
	global_actions_array_cleanup(priv, &priv->hw_global_actions.tag, "tag");
	global_actions_array_cleanup(priv, &priv->hw_global_actions.pop_vlan, "pop_vlan");
	global_actions_array_cleanup(priv, &priv->hw_global_actions.push_vlan, "push_vlan");
	global_actions_array_cleanup(priv,
				     &priv->hw_global_actions.send_to_kernel,
				     "send_to_kernel");

	rte_spinlock_unlock(&priv->hw_global_actions.lock);
}

typedef struct mlx5dr_action *(*global_action_create_t)(struct mlx5dr_context *ctx,
							uint32_t action_flags,
							void *user_data);

static struct mlx5dr_action *
action_create_drop_cb(struct mlx5dr_context *ctx,
		      uint32_t action_flags,
		      void *user_data __rte_unused)
{
	return mlx5dr_action_create_dest_drop(ctx, action_flags);
}

static struct mlx5dr_action *
action_create_tag_cb(struct mlx5dr_context *ctx,
		     uint32_t action_flags,
		     void *user_data __rte_unused)
{
	return mlx5dr_action_create_tag(ctx, action_flags);
}

static struct mlx5dr_action *
action_create_pop_vlan_cb(struct mlx5dr_context *ctx,
			  uint32_t action_flags,
			  void *user_data __rte_unused)
{
	return mlx5dr_action_create_pop_vlan(ctx, action_flags);
}

static struct mlx5dr_action *
action_create_push_vlan_cb(struct mlx5dr_context *ctx,
			   uint32_t action_flags,
			   void *user_data __rte_unused)
{
	return mlx5dr_action_create_push_vlan(ctx, action_flags);
}

static struct mlx5dr_action *
action_create_send_to_kernel_cb(struct mlx5dr_context *ctx,
				uint32_t action_flags,
				void *user_data)
{
	uint16_t priority = (uint16_t)(uintptr_t)user_data;

	return mlx5dr_action_create_dest_root(ctx, priority, action_flags);
}

static struct mlx5dr_action *
global_action_get(struct mlx5_priv *priv,
		  struct mlx5_hws_global_actions_array *array,
		  const char *name,
		  enum mlx5dr_table_type table_type,
		  bool is_root,
		  global_action_create_t create_cb,
		  void *user_data)
{
	enum mlx5dr_action_flags action_flags;
	struct mlx5dr_action *action = NULL;
	int ret;

	ret = mlx5dr_table_type_to_action_flags(table_type, is_root, &action_flags);
	if (ret < 0)
		return NULL;

	rte_spinlock_lock(&priv->hw_global_actions.lock);

	action = array->arr[!is_root][table_type];
	if (action != NULL)
		goto unlock_ret;

	action = create_cb(priv->dr_ctx, action_flags, user_data);
	if (action == NULL) {
		DRV_LOG(ERR, "port %u failed to create %s HWS action",
			priv->dev_data->port_id,
			name);
		goto unlock_ret;
	}

	array->arr[!is_root][table_type] = action;

unlock_ret:
	rte_spinlock_unlock(&priv->hw_global_actions.lock);
	return action;
}

struct mlx5dr_action *
mlx5_hws_global_action_drop_get(struct mlx5_priv *priv,
				enum mlx5dr_table_type table_type,
				bool is_root)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.drop,
				 "drop",
				 table_type,
				 is_root,
				 action_create_drop_cb,
				 NULL);
}

struct mlx5dr_action *
mlx5_hws_global_action_tag_get(struct mlx5_priv *priv,
			       enum mlx5dr_table_type table_type,
			       bool is_root)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.tag,
				 "tag",
				 table_type,
				 is_root,
				 action_create_tag_cb,
				 NULL);
}

struct mlx5dr_action *
mlx5_hws_global_action_pop_vlan_get(struct mlx5_priv *priv,
				    enum mlx5dr_table_type table_type,
				    bool is_root)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.pop_vlan,
				 "pop_vlan",
				 table_type,
				 is_root,
				 action_create_pop_vlan_cb,
				 NULL);
}

struct mlx5dr_action *
mlx5_hws_global_action_push_vlan_get(struct mlx5_priv *priv,
				     enum mlx5dr_table_type table_type,
				     bool is_root)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.push_vlan,
				 "push_vlan",
				 table_type,
				 is_root,
				 action_create_push_vlan_cb,
				 NULL);
}

struct mlx5dr_action *
mlx5_hws_global_action_send_to_kernel_get(struct mlx5_priv *priv,
					   enum mlx5dr_table_type table_type,
					   uint16_t priority)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.send_to_kernel,
				 "send_to_kernel",
				 table_type,
				 false, /* send-to-kernel is non-root only */
				 action_create_send_to_kernel_cb,
				 (void *)(uintptr_t)priority);
}
