/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

bool mlx5dr_context_cap_dynamic_reparse(struct mlx5dr_context *ctx)
{
	return IS_BIT_SET(ctx->caps->rtc_reparse_mode, MLX5_IFC_RTC_REPARSE_BY_STC);
}

uint8_t mlx5dr_context_get_reparse_mode(struct mlx5dr_context *ctx)
{
	/* Prefer to use dynamic reparse, reparse only specific actions */
	if (mlx5dr_context_cap_dynamic_reparse(ctx))
		return MLX5_IFC_RTC_REPARSE_NEVER;

	/* Otherwise use less efficient static */
	return MLX5_IFC_RTC_REPARSE_ALWAYS;
}

static int mlx5dr_context_pools_init(struct mlx5dr_context *ctx)
{
	struct mlx5dr_pool_attr pool_attr = {0};
	uint8_t max_log_sz;
	int i;

	if (mlx5dr_pat_init_pattern_cache(&ctx->pattern_cache))
		return rte_errno;

	if (mlx5dr_definer_init_cache(&ctx->definer_cache))
		goto uninit_pat_cache;

	/* Create an STC pool per FT type */
	pool_attr.pool_type = MLX5DR_POOL_TYPE_STC;
	pool_attr.flags = MLX5DR_POOL_FLAGS_FOR_STC_POOL;
	max_log_sz = RTE_MIN(MLX5DR_POOL_STC_LOG_SZ, ctx->caps->stc_alloc_log_max);
	pool_attr.alloc_log_sz = RTE_MAX(max_log_sz, ctx->caps->stc_alloc_log_gran);

	for (i = 0; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		pool_attr.table_type = i;
		ctx->stc_pool[i] = mlx5dr_pool_create(ctx, &pool_attr);
		if (!ctx->stc_pool[i]) {
			DR_LOG(ERR, "Failed to allocate STC pool [%d]", i);
			goto free_stc_pools;
		}
	}

	return 0;

free_stc_pools:
	for (i = 0; i < MLX5DR_TABLE_TYPE_MAX; i++)
		if (ctx->stc_pool[i])
			mlx5dr_pool_destroy(ctx->stc_pool[i]);

	mlx5dr_definer_uninit_cache(ctx->definer_cache);

uninit_pat_cache:
	mlx5dr_pat_uninit_pattern_cache(ctx->pattern_cache);
	return rte_errno;
}

static void mlx5dr_context_pools_uninit(struct mlx5dr_context *ctx)
{
	int i;

	for (i = 0; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		if (ctx->stc_pool[i])
			mlx5dr_pool_destroy(ctx->stc_pool[i]);
	}

	mlx5dr_definer_uninit_cache(ctx->definer_cache);
	mlx5dr_pat_uninit_pattern_cache(ctx->pattern_cache);
}

static int mlx5dr_context_init_pd(struct mlx5dr_context *ctx,
				  struct ibv_pd *pd)
{
	struct mlx5dv_pd mlx5_pd = {0};
	struct mlx5dv_obj obj;
	int ret;

	if (pd) {
		ctx->pd = pd;
	} else {
		ctx->pd = mlx5_glue->alloc_pd(ctx->ibv_ctx);
		if (!ctx->pd) {
			DR_LOG(ERR, "Failed to allocate PD");
			rte_errno = errno;
			return rte_errno;
		}
		ctx->flags |= MLX5DR_CONTEXT_FLAG_PRIVATE_PD;
	}

	obj.pd.in = ctx->pd;
	obj.pd.out = &mlx5_pd;

	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_PD);
	if (ret)
		goto free_private_pd;

	ctx->pd_num = mlx5_pd.pdn;

	return 0;

free_private_pd:
	if (ctx->flags & MLX5DR_CONTEXT_FLAG_PRIVATE_PD)
		mlx5_glue->dealloc_pd(ctx->pd);

	return ret;
}

static int mlx5dr_context_uninit_pd(struct mlx5dr_context *ctx)
{
	if (ctx->flags & MLX5DR_CONTEXT_FLAG_PRIVATE_PD)
		return mlx5_glue->dealloc_pd(ctx->pd);

	return 0;
}

static void mlx5dr_context_check_hws_supp(struct mlx5dr_context *ctx)
{
	struct mlx5dr_cmd_query_caps *caps = ctx->caps;

	/* HWS not supported on device / FW */
	if (!caps->wqe_based_update) {
		DR_LOG(INFO, "Required HWS WQE based insertion cap not supported");
		return;
	}

	/* Current solution requires all rules to set reparse bit */
	if ((!caps->nic_ft.reparse ||
	     (!caps->fdb_ft.reparse && caps->eswitch_manager)) ||
	    !IS_BIT_SET(caps->rtc_reparse_mode, MLX5_IFC_RTC_REPARSE_ALWAYS)) {
		DR_LOG(INFO, "Required HWS reparse cap not supported");
		return;
	}

	/* FW/HW must support 8DW STE */
	if (!IS_BIT_SET(caps->ste_format, MLX5_IFC_RTC_STE_FORMAT_8DW)) {
		DR_LOG(INFO, "Required HWS STE format not supported");
		return;
	}

	/* Adding rules by hash and by offset are requirements */
	if (!IS_BIT_SET(caps->rtc_index_mode, MLX5_IFC_RTC_STE_UPDATE_MODE_BY_HASH) ||
	    !IS_BIT_SET(caps->rtc_index_mode, MLX5_IFC_RTC_STE_UPDATE_MODE_BY_OFFSET)) {
		DR_LOG(INFO, "Required HWS RTC update mode not supported");
		return;
	}

	/* Support for SELECT definer ID is required */
	if (!IS_BIT_SET(caps->definer_format_sup, MLX5_IFC_DEFINER_FORMAT_ID_SELECT)) {
		DR_LOG(INFO, "Required HWS Dynamic definer not supported");
		return;
	}

	ctx->flags |= MLX5DR_CONTEXT_FLAG_HWS_SUPPORT;
}

static int mlx5dr_context_init_hws(struct mlx5dr_context *ctx,
				   struct mlx5dr_context_attr *attr)
{
	int ret;

	mlx5dr_context_check_hws_supp(ctx);

	if (!(ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT))
		return 0;

	ret = mlx5dr_context_init_pd(ctx, attr->pd);
	if (ret)
		return ret;

	ret = mlx5dr_context_pools_init(ctx);
	if (ret)
		goto uninit_pd;

	ret = mlx5dr_send_queues_open(ctx, attr->queues, attr->queue_size);
	if (ret)
		goto pools_uninit;

	return 0;

pools_uninit:
	mlx5dr_context_pools_uninit(ctx);
uninit_pd:
	mlx5dr_context_uninit_pd(ctx);
	return ret;
}

static void mlx5dr_context_uninit_hws(struct mlx5dr_context *ctx)
{
	if (!(ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT))
		return;

	mlx5dr_send_queues_close(ctx);
	mlx5dr_context_pools_uninit(ctx);
	mlx5dr_context_uninit_pd(ctx);
}

static int mlx5dr_context_init_shared_ctx(struct mlx5dr_context *ctx,
					  struct ibv_context *ibv_ctx,
					  struct mlx5dr_context_attr *attr)
{
	struct mlx5dr_cmd_query_caps shared_caps = {0};
	int ret;

	if (!attr->shared_ibv_ctx) {
		ctx->ibv_ctx = ibv_ctx;
	} else {
		ctx->ibv_ctx = attr->shared_ibv_ctx;
		ctx->local_ibv_ctx = ibv_ctx;
		ret = mlx5dr_cmd_query_caps(attr->shared_ibv_ctx, &shared_caps);
		if (ret || !shared_caps.cross_vhca_resources) {
			DR_LOG(INFO, "No cross_vhca_resources cap for shared ibv");
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		ctx->caps->shared_vhca_id = shared_caps.vhca_id;
	}

	if (ctx->local_ibv_ctx && !ctx->caps->cross_vhca_resources) {
		DR_LOG(INFO, "No cross_vhca_resources cap for local ibv");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	return 0;
}

struct mlx5dr_context *mlx5dr_context_open(struct ibv_context *ibv_ctx,
					   struct mlx5dr_context_attr *attr)
{
	struct mlx5dr_context *ctx;
	int ret;

	ctx = simple_calloc(1, sizeof(*ctx));
	if (!ctx) {
		rte_errno = ENOMEM;
		return NULL;
	}

	pthread_spin_init(&ctx->ctrl_lock, PTHREAD_PROCESS_PRIVATE);

	ctx->caps = simple_calloc(1, sizeof(*ctx->caps));
	if (!ctx->caps)
		goto free_ctx;

	ret = mlx5dr_cmd_query_caps(ibv_ctx, ctx->caps);
	if (ret)
		goto free_caps;

	if (mlx5dr_context_init_shared_ctx(ctx, ibv_ctx, attr))
		goto free_caps;

	ret = mlx5dr_context_init_hws(ctx, attr);
	if (ret)
		goto free_caps;

	return ctx;

free_caps:
	simple_free(ctx->caps);
free_ctx:
	simple_free(ctx);
	return NULL;
}

int mlx5dr_context_close(struct mlx5dr_context *ctx)
{
	mlx5dr_context_uninit_hws(ctx);
	simple_free(ctx->caps);
	pthread_spin_destroy(&ctx->ctrl_lock);
	simple_free(ctx);
	return 0;
}
