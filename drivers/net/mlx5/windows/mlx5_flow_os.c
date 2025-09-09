/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include "mlx5_flow_os.h"
#include "mlx5_win_ext.h"

#include <rte_thread.h>

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
	struct mlx5dv_flow_matcher_attr *mattr;

	RTE_SET_USED(table);
	*matcher = NULL;
	mattr = attr;
	if (mattr->type != IBV_FLOW_ATTR_NORMAL) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	struct mlx5_matcher *mlx5_matcher =
		mlx5_malloc(MLX5_MEM_ZERO,
		       sizeof(struct mlx5_matcher) +
		       MLX5_ST_SZ_BYTES(fte_match_param),
		       0, SOCKET_ID_ANY);
	if (!mlx5_matcher) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	mlx5_matcher->ctx = ctx;
	memcpy(&mlx5_matcher->attr, attr, sizeof(mlx5_matcher->attr));
	memcpy(&mlx5_matcher->match_buf,
	       mattr->match_mask->match_buf,
	       MLX5_ST_SZ_BYTES(fte_match_param));
	*matcher = mlx5_matcher;
	return 0;
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
	mlx5_free(matcher);
	return 0;
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
	struct mlx5_action *mlx5_action =
		mlx5_malloc(MLX5_MEM_ZERO,
		       sizeof(struct mlx5_action),
		       0, SOCKET_ID_ANY);

	if (!mlx5_action) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	mlx5_action->type = MLX5_FLOW_CONTEXT_DEST_TYPE_TIR;
	mlx5_action->dest_tir.id = tir->id;
	*action = mlx5_action;
	return 0;
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
	mlx5_free(action);
	return 0;
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
	struct mlx5_action *action;
	size_t i;
	struct mlx5_matcher *mlx5_matcher = matcher;
	struct mlx5_flow_dv_match_params *mlx5_match_value = match_value;
	uint32_t in[MLX5_ST_SZ_DW(devx_fs_rule_add_in)] = {0};
	void *matcher_c = MLX5_ADDR_OF(devx_fs_rule_add_in, in,
				       match_criteria);
	void *matcher_v = MLX5_ADDR_OF(devx_fs_rule_add_in, in,
				       match_value);

	MLX5_ASSERT(mlx5_matcher->ctx);
	memcpy(matcher_c, mlx5_matcher->match_buf,
	       mlx5_match_value->size);
	/* Use mlx5_match_value->size for match criteria */
	memcpy(matcher_v, mlx5_match_value->buf,
	       mlx5_match_value->size);
	for (i = 0; i < num_actions; i++) {
		action = actions[i];
		switch (action->type) {
		case MLX5_FLOW_CONTEXT_DEST_TYPE_TIR:
			MLX5_SET(devx_fs_rule_add_in, in,
				 dest.destination_type,
				 MLX5_FLOW_CONTEXT_DEST_TYPE_TIR);
			MLX5_SET(devx_fs_rule_add_in, in,
				 dest.destination_id,
				 action->dest_tir.id);
			break;
		default:
			break;
		}
		MLX5_SET(devx_fs_rule_add_in, in, match_criteria_enable,
			 MLX5_MATCH_OUTER_HEADERS);
	}
	*flow = mlx5_glue->devx_fs_rule_add(mlx5_matcher->ctx, in, sizeof(in));
	return (*flow) ? 0 : -1;
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
	return mlx5_glue->devx_fs_rule_del(drv_flow_ptr);
}

struct mlx5_workspace_thread {
	HANDLE	thread_handle;
	struct mlx5_flow_workspace *mlx5_ws;
	struct mlx5_workspace_thread *next;
};

/**
 * Static pointer array for multi thread support of mlx5_flow_workspace.
 */
static struct mlx5_workspace_thread *curr;
static struct mlx5_workspace_thread *first;
rte_thread_key ws_tls_index;
static pthread_mutex_t lock_thread_list;

static bool
mlx5_is_thread_alive(HANDLE thread_handle)
{
	DWORD result = WaitForSingleObject(thread_handle, 0);

	if (result == WAIT_OBJECT_0)
		return false;
	return true;
}

static int
mlx5_get_current_thread(HANDLE *p_handle)
{
	BOOL ret = DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
		GetCurrentProcess(), p_handle, 0, 0, DUPLICATE_SAME_ACCESS);

	if (!ret) {
		RTE_LOG_WIN32_ERR("DuplicateHandle()");
		return -1;
	}
	return 0;
}

static void
mlx5_clear_thread_list(void)
{
	struct mlx5_workspace_thread *temp = first;
	struct mlx5_workspace_thread *next, *prev = NULL;
	HANDLE curr_thread;

	if (!temp)
		return;
	if (mlx5_get_current_thread(&curr_thread)) {
		DRV_LOG(ERR, "Failed to get current thread "
			"handle.");
		return;
	}
	while (temp) {
		next = temp->next;
		if (temp->thread_handle != curr_thread &&
		    !mlx5_is_thread_alive(temp->thread_handle)) {
			if (temp == first) {
				if (curr == temp)
					curr = temp->next;
				first = temp->next;
			} else if (temp == curr) {
				curr = prev;
			}
			flow_release_workspace(temp->mlx5_ws);
			CloseHandle(temp->thread_handle);
			free(temp);
			if (prev)
				prev->next = next;
			temp = next;
			continue;
		}
		prev = temp;
		temp = temp->next;
	}
	CloseHandle(curr_thread);
}

/**
 * Release workspaces before exit.
 */
void
mlx5_flow_os_release_workspace(void)
{
	mlx5_clear_thread_list();
	if (first) {
		MLX5_ASSERT(!first->next);
		flow_release_workspace(first->mlx5_ws);
		free(first);
	}
	rte_thread_key_delete(ws_tls_index);
	pthread_mutex_destroy(&lock_thread_list);
}

static int
mlx5_add_workspace_to_list(struct mlx5_flow_workspace *data)
{
	HANDLE curr_thread;
	struct mlx5_workspace_thread *temp = calloc(1, sizeof(*temp));

	if (!temp) {
		DRV_LOG(ERR, "Failed to allocate thread workspace "
			"memory.");
		return -1;
	}
	if (mlx5_get_current_thread(&curr_thread)) {
		DRV_LOG(ERR, "Failed to get current thread "
			"handle.");
		free(temp);
		return -1;
	}
	temp->mlx5_ws = data;
	temp->thread_handle = curr_thread;
	pthread_mutex_lock(&lock_thread_list);
	mlx5_clear_thread_list();
	if (!first) {
		first = temp;
		curr = temp;
	} else {
		curr->next = temp;
		curr = curr->next;
	}
	pthread_mutex_unlock(&lock_thread_list);
	return 0;
}

int
mlx5_flow_os_init_workspace_once(void)
{
	int err = rte_thread_key_create(&ws_tls_index, NULL);

	if (err) {
		DRV_LOG(ERR, "Can't create flow workspace data thread key.");
		return -rte_errno;
	}
	pthread_mutex_init(&lock_thread_list, NULL);
	return 0;
}

void *
mlx5_flow_os_get_specific_workspace(void)
{
	return rte_thread_value_get(ws_tls_index);
}

int
mlx5_flow_os_set_specific_workspace(struct mlx5_flow_workspace *data)
{
	int err = 0;
	int old_err = rte_errno;

	rte_errno = 0;
	if (!rte_thread_value_get(ws_tls_index)) {
		if (rte_errno) {
			DRV_LOG(ERR, "Failed checking specific workspace.");
			rte_errno = old_err;
			return -1;
		}
		/*
		 * set_specific_workspace when current value is NULL
		 * can happen only once per thread, mark this thread in
		 * linked list to be able to release resources later on.
		 */
		err = mlx5_add_workspace_to_list(data);
		if (err) {
			DRV_LOG(ERR, "Failed adding workspace to list.");
			rte_errno = old_err;
			return -1;
		}
	}
	if (rte_thread_value_set(ws_tls_index, data)) {
		DRV_LOG(ERR, "Failed setting specific workspace.");
		err = -1;
	}
	rte_errno = old_err;
	return err;
}

void
mlx5_flow_os_workspace_gc_add(struct mlx5_flow_workspace *ws)
{
	RTE_SET_USED(ws);
}

int
mlx5_flow_os_validate_item_esp(const struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
			    uint64_t item_flags,
			    uint8_t target_protocol,
			    bool allow_seq,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_esp *mask = item->mask;
	const struct rte_flow_item_esp *spec = item->spec;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	static const struct rte_flow_item_esp mlx5_flow_item_esp_mask = {
		.hdr = {
			.spi = RTE_BE32(0xffffffff),
			.seq = RTE_BE32(0xffffffff),
		},
	};
	int ret;

	if (!(item_flags & l3m))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 is mandatory to filter on L4");
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (target_protocol != 0xff && target_protocol != IPPROTO_ESP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with ESP layer");
	if (!mask)
		mask = &rte_flow_item_esp_mask;
	if (spec && (spec->hdr.spi & mask->hdr.spi))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "matching on spi field in esp is not"
					  " supported on Windows");
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 allow_seq ? (const uint8_t *)&mlx5_flow_item_esp_mask :
			     (const uint8_t *)&rte_flow_item_esp_mask,
		 sizeof(struct rte_flow_item_esp), MLX5_ITEM_RANGE_NOT_ACCEPTED,
		 error);
	if (ret < 0)
		return ret;
	return 0;
}
