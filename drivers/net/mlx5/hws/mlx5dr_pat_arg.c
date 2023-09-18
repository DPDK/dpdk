/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

enum mlx5dr_arg_chunk_size
mlx5dr_arg_data_size_to_arg_log_size(uint16_t data_size)
{
	/* Return the roundup of log2(data_size) */
	if (data_size <= MLX5DR_ARG_DATA_SIZE)
		return MLX5DR_ARG_CHUNK_SIZE_1;
	if (data_size <= MLX5DR_ARG_DATA_SIZE * 2)
		return MLX5DR_ARG_CHUNK_SIZE_2;
	if (data_size <= MLX5DR_ARG_DATA_SIZE * 4)
		return MLX5DR_ARG_CHUNK_SIZE_3;
	if (data_size <= MLX5DR_ARG_DATA_SIZE * 8)
		return MLX5DR_ARG_CHUNK_SIZE_4;

	return MLX5DR_ARG_CHUNK_SIZE_MAX;
}

uint32_t mlx5dr_arg_data_size_to_arg_size(uint16_t data_size)
{
	return BIT(mlx5dr_arg_data_size_to_arg_log_size(data_size));
}

enum mlx5dr_arg_chunk_size
mlx5dr_arg_get_arg_log_size(uint16_t num_of_actions)
{
	return mlx5dr_arg_data_size_to_arg_log_size(num_of_actions *
						    MLX5DR_MODIFY_ACTION_SIZE);
}

uint32_t mlx5dr_arg_get_arg_size(uint16_t num_of_actions)
{
	return BIT(mlx5dr_arg_get_arg_log_size(num_of_actions));
}

/* Cache and cache element handling */
int mlx5dr_pat_init_pattern_cache(struct mlx5dr_pattern_cache **cache)
{
	struct mlx5dr_pattern_cache *new_cache;

	new_cache = simple_calloc(1, sizeof(*new_cache));
	if (!new_cache) {
		rte_errno = ENOMEM;
		return rte_errno;
	}
	LIST_INIT(&new_cache->head);
	pthread_spin_init(&new_cache->lock, PTHREAD_PROCESS_PRIVATE);

	*cache = new_cache;

	return 0;
}

void mlx5dr_pat_uninit_pattern_cache(struct mlx5dr_pattern_cache *cache)
{
	simple_free(cache);
}

static bool mlx5dr_pat_compare_pattern(int cur_num_of_actions,
				       __be64 cur_actions[],
				       int num_of_actions,
				       __be64 actions[])
{
	int i;

	if (cur_num_of_actions != num_of_actions)
		return false;

	for (i = 0; i < num_of_actions; i++) {
		u8 action_id =
			MLX5_GET(set_action_in, &actions[i], action_type);

		if (action_id == MLX5_MODIFICATION_TYPE_COPY ||
		    action_id == MLX5_MODIFICATION_TYPE_ADD_FIELD) {
			if (actions[i] != cur_actions[i])
				return false;
		} else {
			/* Compare just the control, not the values */
			if ((__be32)actions[i] !=
			    (__be32)cur_actions[i])
				return false;
		}
	}

	return true;
}

static struct mlx5dr_pattern_cache_item *
mlx5dr_pat_find_cached_pattern(struct mlx5dr_pattern_cache *cache,
			       uint16_t num_of_actions,
			       __be64 *actions)
{
	struct mlx5dr_pattern_cache_item *cached_pat;

	LIST_FOREACH(cached_pat, &cache->head, next) {
		if (mlx5dr_pat_compare_pattern(cached_pat->mh_data.num_of_actions,
					       (__be64 *)cached_pat->mh_data.data,
					       num_of_actions,
					       actions))
			return cached_pat;
	}

	return NULL;
}

static struct mlx5dr_pattern_cache_item *
mlx5dr_pat_get_existing_cached_pattern(struct mlx5dr_pattern_cache *cache,
				       uint16_t num_of_actions,
				       __be64 *actions)
{
	struct mlx5dr_pattern_cache_item *cached_pattern;

	cached_pattern = mlx5dr_pat_find_cached_pattern(cache, num_of_actions, actions);
	if (cached_pattern) {
		/* LRU: move it to be first in the list */
		LIST_REMOVE(cached_pattern, next);
		LIST_INSERT_HEAD(&cache->head, cached_pattern, next);
		cached_pattern->refcount++;
	}

	return cached_pattern;
}

static struct mlx5dr_pattern_cache_item *
mlx5dr_pat_add_pattern_to_cache(struct mlx5dr_pattern_cache *cache,
				struct mlx5dr_devx_obj *pattern_obj,
				uint16_t num_of_actions,
				__be64 *actions)
{
	struct mlx5dr_pattern_cache_item *cached_pattern;

	cached_pattern = simple_calloc(1, sizeof(*cached_pattern));
	if (!cached_pattern) {
		DR_LOG(ERR, "Failed to allocate cached_pattern");
		rte_errno = ENOMEM;
		return NULL;
	}

	cached_pattern->mh_data.num_of_actions = num_of_actions;
	cached_pattern->mh_data.pattern_obj = pattern_obj;
	cached_pattern->mh_data.data =
		simple_malloc(num_of_actions * MLX5DR_MODIFY_ACTION_SIZE);
	if (!cached_pattern->mh_data.data) {
		DR_LOG(ERR, "Failed to allocate mh_data.data");
		rte_errno = ENOMEM;
		goto free_cached_obj;
	}

	memcpy(cached_pattern->mh_data.data, actions,
	       num_of_actions * MLX5DR_MODIFY_ACTION_SIZE);

	LIST_INSERT_HEAD(&cache->head, cached_pattern, next);
	cached_pattern->refcount = 1;

	return cached_pattern;

free_cached_obj:
	simple_free(cached_pattern);
	return NULL;
}

static struct mlx5dr_pattern_cache_item *
mlx5dr_pat_find_cached_pattern_by_obj(struct mlx5dr_pattern_cache *cache,
				      struct mlx5dr_devx_obj *pat_obj)
{
	struct mlx5dr_pattern_cache_item *cached_pattern;

	LIST_FOREACH(cached_pattern, &cache->head, next) {
		if (cached_pattern->mh_data.pattern_obj->id == pat_obj->id)
			return cached_pattern;
	}

	return NULL;
}

static void
mlx5dr_pat_remove_pattern(struct mlx5dr_pattern_cache_item *cached_pattern)
{
	LIST_REMOVE(cached_pattern, next);
	simple_free(cached_pattern->mh_data.data);
	simple_free(cached_pattern);
}

void mlx5dr_pat_put_pattern(struct mlx5dr_context *ctx,
			    struct mlx5dr_devx_obj *pat_obj)
{
	struct mlx5dr_pattern_cache *cache = ctx->pattern_cache;
	struct mlx5dr_pattern_cache_item *cached_pattern;

	pthread_spin_lock(&cache->lock);
	cached_pattern = mlx5dr_pat_find_cached_pattern_by_obj(cache, pat_obj);
	if (!cached_pattern) {
		DR_LOG(ERR, "Failed to find pattern according to action with pt");
		assert(false);
		goto out;
	}

	if (--cached_pattern->refcount)
		goto out;

	mlx5dr_pat_remove_pattern(cached_pattern);
	mlx5dr_cmd_destroy_obj(pat_obj);

out:
	pthread_spin_unlock(&cache->lock);
}

struct mlx5dr_devx_obj *
mlx5dr_pat_get_pattern(struct mlx5dr_context *ctx,
		       __be64 *pattern, size_t pattern_sz)
{
	uint16_t num_of_actions = pattern_sz / MLX5DR_MODIFY_ACTION_SIZE;
	struct mlx5dr_pattern_cache_item *cached_pattern;
	struct mlx5dr_devx_obj *pat_obj = NULL;

	pthread_spin_lock(&ctx->pattern_cache->lock);

	cached_pattern = mlx5dr_pat_get_existing_cached_pattern(ctx->pattern_cache,
								num_of_actions,
								pattern);
	if (cached_pattern) {
		pat_obj = cached_pattern->mh_data.pattern_obj;
		goto out_unlock;
	}

	pat_obj = mlx5dr_cmd_header_modify_pattern_create(ctx->ibv_ctx,
							      pattern_sz,
							      (uint8_t *)pattern);
	if (!pat_obj) {
		DR_LOG(ERR, "Failed to create pattern FW object");
		goto out_unlock;
	}

	cached_pattern = mlx5dr_pat_add_pattern_to_cache(ctx->pattern_cache,
							 pat_obj,
							 num_of_actions,
							 pattern);
	if (!cached_pattern) {
		DR_LOG(ERR, "Failed to add pattern to cache");
		goto clean_pattern;
	}

	pthread_spin_unlock(&ctx->pattern_cache->lock);
	return pat_obj;

clean_pattern:
	mlx5dr_cmd_destroy_obj(pat_obj);
	pat_obj = NULL;
out_unlock:
	pthread_spin_unlock(&ctx->pattern_cache->lock);
	return pat_obj;
}

static void
mlx5d_arg_init_send_attr(struct mlx5dr_send_engine_post_attr *send_attr,
			 void *comp_data,
			 uint32_t arg_idx)
{
	send_attr->opcode = MLX5DR_WQE_OPCODE_TBL_ACCESS;
	send_attr->opmod = MLX5DR_WQE_GTA_OPMOD_MOD_ARG;
	send_attr->len = MLX5DR_WQE_SZ_GTA_CTRL + MLX5DR_WQE_SZ_GTA_DATA;
	send_attr->id = arg_idx;
	send_attr->user_data = comp_data;
}

void mlx5dr_arg_decapl3_write(struct mlx5dr_send_engine *queue,
			      uint32_t arg_idx,
			      uint8_t *arg_data,
			      uint16_t num_of_actions)
{
	struct mlx5dr_send_engine_post_attr send_attr = {0};
	struct mlx5dr_wqe_gta_data_seg_arg *wqe_arg;
	struct mlx5dr_send_engine_post_ctrl ctrl;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	size_t wqe_len;

	mlx5d_arg_init_send_attr(&send_attr, NULL, arg_idx);

	ctrl = mlx5dr_send_engine_post_start(queue);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_ctrl, &wqe_len);
	memset(wqe_ctrl, 0, wqe_len);
	mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_arg, &wqe_len);
	mlx5dr_action_prepare_decap_l3_data(arg_data, (uint8_t *)wqe_arg,
					    num_of_actions);
	mlx5dr_send_engine_post_end(&ctrl, &send_attr);
}

void mlx5dr_arg_write(struct mlx5dr_send_engine *queue,
		      void *comp_data,
		      uint32_t arg_idx,
		      uint8_t *arg_data,
		      size_t data_size)
{
	struct mlx5dr_send_engine_post_attr send_attr = {0};
	struct mlx5dr_wqe_gta_data_seg_arg *wqe_arg;
	struct mlx5dr_send_engine_post_ctrl ctrl;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	int i, full_iter, leftover;
	size_t wqe_len;

	mlx5d_arg_init_send_attr(&send_attr, comp_data, arg_idx);

	/* Each WQE can hold 64B of data, it might require multiple iteration */
	full_iter = data_size / MLX5DR_ARG_DATA_SIZE;
	leftover = data_size & (MLX5DR_ARG_DATA_SIZE - 1);

	for (i = 0; i < full_iter; i++) {
		ctrl = mlx5dr_send_engine_post_start(queue);
		mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_ctrl, &wqe_len);
		memset(wqe_ctrl, 0, wqe_len);
		mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_arg, &wqe_len);
		memcpy(wqe_arg, arg_data, wqe_len);
		send_attr.id = arg_idx++;
		mlx5dr_send_engine_post_end(&ctrl, &send_attr);

		/* Move to next argument data */
		arg_data += MLX5DR_ARG_DATA_SIZE;
	}

	if (leftover) {
		ctrl = mlx5dr_send_engine_post_start(queue);
		mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_ctrl, &wqe_len);
		memset(wqe_ctrl, 0, wqe_len);
		mlx5dr_send_engine_post_req_wqe(&ctrl, (void *)&wqe_arg, &wqe_len);
		memcpy(wqe_arg, arg_data, leftover);
		send_attr.id = arg_idx;
		mlx5dr_send_engine_post_end(&ctrl, &send_attr);
	}
}

int mlx5dr_arg_write_inline_arg_data(struct mlx5dr_context *ctx,
				     uint32_t arg_idx,
				     uint8_t *arg_data,
				     size_t data_size)
{
	struct mlx5dr_send_engine *queue;
	int ret;

	pthread_spin_lock(&ctx->ctrl_lock);

	/* Get the control queue */
	queue = &ctx->send_queue[ctx->queues - 1];

	mlx5dr_arg_write(queue, arg_data, arg_idx, arg_data, data_size);

	mlx5dr_send_engine_flush_queue(queue);

	/* Poll for completion */
	ret = mlx5dr_send_queue_action(ctx, ctx->queues - 1,
				       MLX5DR_SEND_QUEUE_ACTION_DRAIN_SYNC);

	if (ret)
		DR_LOG(ERR, "Failed to drain arg queue");

	pthread_spin_unlock(&ctx->ctrl_lock);

	return ret;
}

bool mlx5dr_arg_is_valid_arg_request_size(struct mlx5dr_context *ctx,
					  uint32_t arg_size)
{
	if (arg_size < ctx->caps->log_header_modify_argument_granularity ||
	    arg_size > ctx->caps->log_header_modify_argument_max_alloc) {
		return false;
	}
	return true;
}

struct mlx5dr_devx_obj *
mlx5dr_arg_create(struct mlx5dr_context *ctx,
		  uint8_t *data,
		  size_t data_sz,
		  uint32_t log_bulk_sz,
		  bool write_data)
{
	struct mlx5dr_devx_obj *arg_obj;
	uint16_t single_arg_log_sz;
	uint16_t multi_arg_log_sz;
	int ret;

	single_arg_log_sz = mlx5dr_arg_data_size_to_arg_log_size(data_sz);
	multi_arg_log_sz = single_arg_log_sz + log_bulk_sz;

	if (single_arg_log_sz >= MLX5DR_ARG_CHUNK_SIZE_MAX) {
		DR_LOG(ERR, "Requested single arg %u not supported", single_arg_log_sz);
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (!mlx5dr_arg_is_valid_arg_request_size(ctx, multi_arg_log_sz)) {
		DR_LOG(ERR, "Argument log size %d not supported by FW", multi_arg_log_sz);
		rte_errno = ENOTSUP;
		return NULL;
	}

	/* Alloc bulk of args */
	arg_obj = mlx5dr_cmd_arg_create(ctx->ibv_ctx, multi_arg_log_sz, ctx->pd_num);
	if (!arg_obj) {
		DR_LOG(ERR, "Failed allocating arg in order: %d", multi_arg_log_sz);
		return NULL;
	}

	if (write_data) {
		ret = mlx5dr_arg_write_inline_arg_data(ctx,
						       arg_obj->id,
						       data, data_sz);
		if (ret) {
			DR_LOG(ERR, "Failed writing arg data");
			mlx5dr_cmd_destroy_obj(arg_obj);
			return NULL;
		}
	}

	return arg_obj;
}

struct mlx5dr_devx_obj *
mlx5dr_arg_create_modify_header_arg(struct mlx5dr_context *ctx,
				    __be64 *data,
				    uint8_t num_of_actions,
				    uint32_t log_bulk_sz,
				    bool write_data)
{
	size_t data_sz = num_of_actions * MLX5DR_MODIFY_ACTION_SIZE;
	struct mlx5dr_devx_obj *arg_obj;

	arg_obj = mlx5dr_arg_create(ctx,
				    (uint8_t *)data,
				    data_sz,
				    log_bulk_sz,
				    write_data);
	if (!arg_obj)
		DR_LOG(ERR, "Failed creating modify header arg");

	return arg_obj;
}

bool mlx5dr_pat_verify_actions(__be64 pattern[], size_t sz)
{
	size_t i;

	for (i = 0; i < sz / MLX5DR_MODIFY_ACTION_SIZE; i++) {
		u8 action_id =
			MLX5_GET(set_action_in, &pattern[i], action_type);
		if (action_id >= MLX5_MODIFICATION_TYPE_MAX) {
			DR_LOG(ERR, "Invalid action %u", action_id);
			return false;
		}
	}

	return true;
}
