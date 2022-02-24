/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_FLOW_OS_H_
#define RTE_PMD_MLX5_FLOW_OS_H_

#include "mlx5_flow.h"
#include "mlx5_malloc.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
extern const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops;
extern const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops;
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
	return MLX5_FLOW_TYPE_DV;
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
mlx5_flow_os_item_supported(int item)
{
	switch (item) {
	case RTE_FLOW_ITEM_TYPE_END:
	case RTE_FLOW_ITEM_TYPE_VOID:
	case RTE_FLOW_ITEM_TYPE_ETH:
	case RTE_FLOW_ITEM_TYPE_IPV4:
	case RTE_FLOW_ITEM_TYPE_UDP:
	case RTE_FLOW_ITEM_TYPE_TCP:
	case RTE_FLOW_ITEM_TYPE_IPV6:
	case RTE_FLOW_ITEM_TYPE_VLAN:
		return true;
	default:
		return false;
	}
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
mlx5_flow_os_action_supported(int action)
{
	switch (action) {
	case RTE_FLOW_ACTION_TYPE_END:
	case RTE_FLOW_ACTION_TYPE_VOID:
	case RTE_FLOW_ACTION_TYPE_QUEUE:
	case RTE_FLOW_ACTION_TYPE_RSS:
		return true;
	default:
		return false;
	}
}

/**
 * Create flow table.
 *
 * @param[in] domain
 *   Pointer to relevant domain.
 * @param[in] table_id
 *   Table ID.
 * @param[out] table
 *   NULL (no table object required)
 *
 * @return
 *   0 if table_id is 0, negative value otherwise and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_tbl(void *domain, uint32_t table_id, void **table)
{
	RTE_SET_USED(domain);
	*table = NULL;
	if (table_id) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}

/**
 * Destroy flow table.
 *
 * @param table
 *   Pointer to table to destroy.
 *
 * @return
 *   0 on success (silently ignored).
 */
static inline int
mlx5_flow_os_destroy_flow_tbl(void *table)
{
	RTE_SET_USED(table);
	/* Silently ignore */
	return 0;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_packet_reformat(void *ctx, void *domain,
						void *resource, void **action)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(domain);
	RTE_SET_USED(resource);
	RTE_SET_USED(action);
	rte_errno = ENOTSUP;
	return -rte_errno;
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
mlx5_flow_os_create_flow_action_modify_header(void *ctx,
					      void *domain,
					      void *resource,
					      uint32_t actions_len,
					      void **action)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(domain);
	RTE_SET_USED(resource);
	RTE_SET_USED(actions_len);
	RTE_SET_USED(action);
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_dest_flow_tbl(void *tbl_obj, void **action)
{
	RTE_SET_USED(tbl_obj);
	RTE_SET_USED(action);
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_dest_port(void *domain, uint32_t port_id,
					  void **action)
{
	RTE_SET_USED(domain);
	RTE_SET_USED(port_id);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_push_vlan(void *domain, rte_be32_t vlan_tag,
					  void **action)
{
	RTE_SET_USED(domain);
	RTE_SET_USED(vlan_tag);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_count(void *cnt_obj, uint16_t offset,
				      void **action)
{
	RTE_SET_USED(cnt_obj);
	RTE_SET_USED(offset);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_tag(uint32_t tag, void **action)
{
	RTE_SET_USED(tag);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Create flow action: drop.
 *
 * @param[out] action
 *   Pointer to a valid action on success, NULL otherwise.
 *
 * @return
 *   0 on success, or negative value on failure and errno is set.
 */
static inline int
mlx5_flow_os_create_flow_action_drop(void **action)
{
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Create flow action: default miss.
 *
 * @param[out] action
 *   NULL action pointer.
 *
 * @return
 *   0 as success.
 */
static inline int
mlx5_flow_os_create_flow_action_default_miss(void **action)
{
	*action = 0;
	/* Silently ignore */
	return 0;
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
	RTE_SET_USED(attr);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
	RTE_SET_USED(domain);
	RTE_SET_USED(num_dest);
	RTE_SET_USED(dests);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * OS stub for mlx5_flow_adjust_priority() API.
 * Windows only supports flow priority 0 that cannot be adjusted.
 *
 * @param[in] dev
 *    Pointer to the Ethernet device structure.
 * @param[in] priority
 *    The rule base priority.
 * @param[in] subpriority
 *    The priority based on the items.
 *
 * @return
 *    0
 */
static inline uint32_t
mlx5_os_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
			  uint32_t subpriority)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(priority);
	RTE_SET_USED(subpriority);
	return 0;
}

static inline int
mlx5_os_flow_dr_sync_domain(void *domain, uint32_t flags)
{
	RTE_SET_USED(domain);
	RTE_SET_USED(flags);
	errno = ENOTSUP;
	return errno;
}

int mlx5_flow_os_validate_flow_attributes(struct rte_eth_dev *dev,
					const struct rte_flow_attr *attributes,
					bool external,
					struct rte_flow_error *error);
int mlx5_flow_os_create_flow_matcher(void *ctx,
				     void *attr,
				     void *table,
				     void **matcher);
int mlx5_flow_os_destroy_flow_matcher(void *matcher);
int mlx5_flow_os_create_flow_action_dest_devx_tir(struct mlx5_devx_obj *tir,
						  void **action);
int mlx5_flow_os_destroy_flow_action(void *action);
int mlx5_flow_os_create_flow(void *matcher, void *match_value,
			     size_t num_actions,
			     void *actions[], void **flow);
int mlx5_flow_os_destroy_flow(void *drv_flow_ptr);
#endif /* RTE_PMD_MLX5_FLOW_OS_H_ */
