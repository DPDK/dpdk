/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include "mlx5_flow_os.h"
#include "mlx5_win_ext.h"

/**
 * Verify the @p attributes will be correctly understood by the NIC and store
 * them in the @p flow if everything is correct.
 *
 * @param[in] dev
 *   Pointer to dev struct.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   - 0 on success and non root table (not a valid option for Windows yet).
 *   - 1 on success and root table.
 *   - a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_os_validate_flow_attributes(struct rte_eth_dev *dev,
				      const struct rte_flow_attr *attributes,
				      bool external,
				      struct rte_flow_error *error)
{
	int ret = 1;

	RTE_SET_USED(dev);
	RTE_SET_USED(external);
	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups are not supported");
	if (attributes->priority)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL,
					  "priorities are not supported");
	if (attributes->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL,
					  "transfer not supported");
	if (!(attributes->ingress))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL, "must specify ingress only");
	return ret;
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
 *   0 on success, or errno on failure.
 */
int
mlx5_flow_os_create_flow_matcher(void *ctx,
				 void *attr,
				 void *table,
				 void **matcher)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(attr);
	RTE_SET_USED(table);
	*matcher = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
int
mlx5_flow_os_destroy_flow_matcher(void *matcher)
{
	RTE_SET_USED(matcher);
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or errno on failure.
 */
int
mlx5_flow_os_create_flow_action_dest_devx_tir(struct mlx5_devx_obj *tir,
					      void **action)
{
	RTE_SET_USED(tir);
	*action = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
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
int
mlx5_flow_os_destroy_flow_action(void *action)
{
	RTE_SET_USED(action);
	rte_errno = ENOTSUP;
	return -rte_errno;
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
 *   0 on success, or errno on failure.
 */
int
mlx5_flow_os_create_flow(void *matcher, void *match_value,
			 size_t num_actions,
			 void *actions[], void **flow)
{
	RTE_SET_USED(matcher);
	RTE_SET_USED(match_value);
	RTE_SET_USED(num_actions);
	RTE_SET_USED(actions);
	*flow = NULL;
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Destroy flow rule.
 *
 * @param[in] drv_flow_ptr
 *   Pointer to flow rule object.
 *
 * @return
 *   0 on success, errno on failure.
 */
int
mlx5_flow_os_destroy_flow(void *drv_flow_ptr)
{
	RTE_SET_USED(dev_flow_ptr);
	rte_errno = ENOTSUP;
	return -rte_errno;
}
