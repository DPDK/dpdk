/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2026 NVIDIA Corporation & Affiliates
 */

#include "mlx5_hws_global_actions.h"

#include "mlx5.h"
#include "mlx5_flow.h"

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
	global_actions_array_cleanup(priv, &priv->hw_global_actions.nat64_6to4, "nat64_6to4");
	global_actions_array_cleanup(priv, &priv->hw_global_actions.nat64_4to6, "nat64_4to6");
	global_actions_array_cleanup(priv, &priv->hw_global_actions.def_miss, "def_miss");

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
action_create_nat64_cb(struct mlx5dr_context *ctx,
		       uint32_t action_flags,
		       void *user_data)
{
	struct mlx5dr_action_nat64_attr *attr = user_data;

	/* NAT64 action must always be marked as shared. */
	return mlx5dr_action_create_nat64(ctx, attr,
					  action_flags | MLX5DR_ACTION_FLAG_SHARED);
}

static struct mlx5dr_action *
action_create_def_miss_cb(struct mlx5dr_context *ctx,
			  uint32_t action_flags,
			  void *user_data __rte_unused)
{
	return mlx5dr_action_create_default_miss(ctx, action_flags);
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

struct mlx5dr_action *
mlx5_hws_global_action_nat64_get(struct mlx5_priv *priv,
				 enum mlx5dr_table_type table_type,
				 enum rte_flow_nat64_type nat64_type)
{
	struct mlx5_hws_global_actions_array *array;
	uint8_t regs[MLX5_FLOW_NAT64_REGS_MAX];
	struct mlx5dr_action_nat64_attr attr;
	const char *name;

	for (uint32_t i = 0; i < MLX5_FLOW_NAT64_REGS_MAX; i++)
		regs[i] = mlx5_convert_reg_to_field(priv->sh->registers.nat64_regs[i]);

	attr.num_of_registers = MLX5_FLOW_NAT64_REGS_MAX;
	attr.registers = regs;

	if (nat64_type == RTE_FLOW_NAT64_6TO4) {
		attr.flags = MLX5DR_ACTION_NAT64_V6_TO_V4 | MLX5DR_ACTION_NAT64_BACKUP_ADDR;
		array = &priv->hw_global_actions.nat64_6to4;
		name = "nat64_6to4";
	} else {
		attr.flags = MLX5DR_ACTION_NAT64_V4_TO_V6 | MLX5DR_ACTION_NAT64_BACKUP_ADDR;
		array = &priv->hw_global_actions.nat64_4to6;
		name = "nat64_4to6";
	}

	return global_action_get(priv, array, name, table_type,
				 false, action_create_nat64_cb, &attr);
}

struct mlx5dr_action *
mlx5_hws_global_action_def_miss_get(struct mlx5_priv *priv,
				    enum mlx5dr_table_type table_type,
				    bool is_root)
{
	return global_action_get(priv,
				 &priv->hw_global_actions.def_miss,
				 "def_miss",
				 table_type,
				 is_root,
				 action_create_def_miss_cb,
				 NULL);
}
