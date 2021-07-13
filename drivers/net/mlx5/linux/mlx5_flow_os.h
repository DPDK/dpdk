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

/**
 * Create flow action: packet reformat.
 *
 * @param[in] ctx
 *   Pointer to relevant device context.
 * @param[in] domain
 *   Pointer to domain handler.
 * @param[in] resource
 *   Pointer to action data resource.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_packet_reformat(void *ctx, void *domain,
						void *resource, void **action)
{
	struct mlx5_flow_dv_encap_decap_resource *res =
			(struct mlx5_flow_dv_encap_decap_resource *)resource;

	*action = mlx5_glue->dv_create_flow_action_packet_reformat
					(ctx, res->reformat_type, res->ft_type,
					 domain, res->flags, res->size,
					 (res->size ? res->buf : NULL));
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: modify header.
 *
 * @param[in] ctx
 *   Pointer to relevant device context.
 * @param[in] domain
 *   Pointer to domain handler.
 * @param[in] resource
 *   Pointer to action data resource.
 * @param[in] actions_len
 *   Total length of actions data in resource.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_modify_header(void *ctx, void *domain,
					      void *resource,
					      uint32_t actions_len,
					      void **action)
{
	struct mlx5_flow_dv_modify_hdr_resource *res =
			(struct mlx5_flow_dv_modify_hdr_resource *)resource;

	*action = mlx5_glue->dv_create_flow_action_modify_header
					(ctx, res->ft_type, domain, res->root ?
					 MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL : 0,
					 actions_len, (uint64_t *)res->actions);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: destination flow table.
 *
 * @param[in] tbl_obj
 *   Pointer to destination table object.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_dest_flow_tbl(void *tbl_obj, void **action)
{
	*action = mlx5_glue->dr_create_flow_action_dest_flow_tbl(tbl_obj);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: destination port.
 *
 * @param[in] domain
 *   Pointer to domain handler.
 * @param[in] port_id
 *   Destination port ID.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_dest_port(void *domain, uint32_t port_id,
					  void **action)
{
	/*
	 * Depending on rdma_core version the glue routine calls
	 * either mlx5dv_dr_action_create_dest_ib_port(domain, dev_port)
	 * or mlx5dv_dr_action_create_dest_vport(domain, vport_id).
	 */
	*action = mlx5_glue->dr_create_flow_action_dest_port(domain, port_id);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: push vlan.
 *
 * @param[in] domain
 *   Pointer to domain handler.
 * @param[in] vlan_tag
 *   VLAN tag value.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_push_vlan(void *domain, rte_be32_t vlan_tag,
					  void **action)
{
	*action = mlx5_glue->dr_create_flow_action_push_vlan(domain, vlan_tag);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: count.
 *
 * @param[in] cnt_obj
 *   Pointer to DevX counter object.
 * @param[in] offset
 *   Offset of counter in array.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_count(void *cnt_obj, uint16_t offset,
				      void **action)
{
	*action = mlx5_glue->dv_create_flow_action_counter(cnt_obj, offset);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: tag.
 *
 * @param[in] tag
 *   Tag value.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_tag(uint32_t tag, void **action)
{
	*action = mlx5_glue->dv_create_flow_action_tag(tag);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: drop.
 *
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_drop(void **action)
{
	*action = mlx5_glue->dr_create_flow_action_drop();
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: default miss.
 *
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_default_miss(void **action)
{
	*action = mlx5_glue->dr_create_flow_action_default_miss();
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: dest_devx_tir
 *
 * @param[in] tir
 *   Pointer to DevX tir object
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_dest_devx_tir(struct mlx5_devx_obj *tir,
					      void **action)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	*action = mlx5_glue->dv_create_flow_action_dest_devx_tir(tir->obj);
	return (*action) ? 0 : -1;
#else
	/* If no DV support - skip the operation and return success */
	RTE_SET_USED(tir);
	*action = 0;
	return 0;
#endif
}

/**
 * Create flow action: sampler
 *
 * @param[in] attr
 *   Pointer to sampler attribute
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_os_flow_dr_create_flow_action_sampler
			(struct mlx5dv_dr_flow_sampler_attr *attr,
			void **action)
{
	*action = mlx5_glue->dr_create_flow_action_sampler(attr);
	return (*action) ? 0 : -1;
}

/**
 * Create flow action: dest_array
 *
 * @param[in] domain
 *   Pointer to relevant domain.
 * @param[in] num_dest
 *   Number of destinations array.
 * @param[in] dests
 *   Array of destination attributes.
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or -1 on failure and errno is set.
 */
static inline int
mlx5_os_flow_dr_create_flow_action_dest_array
			(void *domain,
			 size_t num_dest,
			 struct mlx5dv_dr_action_dest_attr *dests[],
			 void **action)
{
	*action = mlx5_glue->dr_create_flow_action_dest_array(
						domain, num_dest, dests);
	return (*action) ? 0 : -1;
}

/**
 * Destroy flow action.
 *
 * @param[in] action
 *   Pointer to action object to destroy.
 *
 * @return
 *   0 on success, or the value of errno on failure.
 */
static inline int
mlx5_flow_os_destroy_flow_action(void *action)
{
	return mlx5_glue->destroy_flow_action(action);
}

/**
 * OS wrapper over Verbs API.
 * Adjust flow priority based on the highest layer and the request priority.
 *
 * @param[in] dev
 *    Pointer to the Ethernet device structure.
 * @param[in] priority
 *    The rule base priority.
 * @param[in] subpriority
 *    The priority based on the items.
 *
 * @return
 *    The new priority.
 */
static inline uint32_t
mlx5_os_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
			  uint32_t subpriority)
{
	return mlx5_flow_adjust_priority(dev, priority, subpriority);
}

static inline int
mlx5_os_flow_dr_sync_domain(void *domain, uint32_t flags)
{
	return mlx5_glue->dr_sync_domain(domain, flags);
}
#endif /* RTE_PMD_MLX5_FLOW_OS_H_ */
