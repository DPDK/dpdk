/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

const char *mlx5dr_debug_action_type_str[] = {
	[MLX5DR_ACTION_TYP_LAST] = "LAST",
	[MLX5DR_ACTION_TYP_TNL_L2_TO_L2] = "TNL_L2_TO_L2",
	[MLX5DR_ACTION_TYP_L2_TO_TNL_L2] = "L2_TO_TNL_L2",
	[MLX5DR_ACTION_TYP_TNL_L3_TO_L2] = "TNL_L3_TO_L2",
	[MLX5DR_ACTION_TYP_L2_TO_TNL_L3] = "L2_TO_TNL_L3",
	[MLX5DR_ACTION_TYP_DROP] = "DROP",
	[MLX5DR_ACTION_TYP_TIR] = "TIR",
	[MLX5DR_ACTION_TYP_FT] = "FT",
	[MLX5DR_ACTION_TYP_CTR] = "CTR",
	[MLX5DR_ACTION_TYP_TAG] = "TAG",
	[MLX5DR_ACTION_TYP_MODIFY_HDR] = "MODIFY_HDR",
	[MLX5DR_ACTION_TYP_VPORT] = "VPORT",
	[MLX5DR_ACTION_TYP_MISS] = "DEFAULT_MISS",
	[MLX5DR_ACTION_TYP_POP_VLAN] = "POP_VLAN",
	[MLX5DR_ACTION_TYP_PUSH_VLAN] = "PUSH_VLAN",
	[MLX5DR_ACTION_TYP_ASO_METER] = "ASO_METER",
	[MLX5DR_ACTION_TYP_ASO_CT] = "ASO_CT",
	[MLX5DR_ACTION_TYP_DEST_ROOT] = "DEST_ROOT",
};

static_assert(ARRAY_SIZE(mlx5dr_debug_action_type_str) == MLX5DR_ACTION_TYP_MAX,
	      "Missing mlx5dr_debug_action_type_str");

const char *mlx5dr_debug_action_type_to_str(enum mlx5dr_action_type action_type)
{
	return mlx5dr_debug_action_type_str[action_type];
}

static int
mlx5dr_debug_dump_matcher_template_definer(FILE *f,
					   void *parent_obj,
					   struct mlx5dr_definer *definer,
					   enum mlx5dr_debug_res_type type)
{
	int i, ret;

	if (!definer)
		return 0;

	ret = fprintf(f, "%d,0x%" PRIx64 ",0x%" PRIx64 ",%d,%d,",
		      type,
		      (uint64_t)(uintptr_t)definer,
		      (uint64_t)(uintptr_t)parent_obj,
		      definer->obj->id,
		      definer->type);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	for (i = 0; i < DW_SELECTORS; i++) {
		ret = fprintf(f, "0x%x%s", definer->dw_selector[i],
			      (i == DW_SELECTORS - 1) ? "," : "-");
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}
	}

	for (i = 0; i < BYTE_SELECTORS; i++) {
		ret = fprintf(f, "0x%x%s", definer->byte_selector[i],
			      (i == BYTE_SELECTORS - 1) ? "," : "-");
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}
	}

	for (i = 0; i < MLX5DR_JUMBO_TAG_SZ; i++) {
		ret = fprintf(f, "%02x", definer->mask.jumbo[i]);
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}
	}

	ret = fprintf(f, "\n");
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int
mlx5dr_debug_dump_matcher_match_template(FILE *f, struct mlx5dr_matcher *matcher)
{
	bool is_root = matcher->tbl->level == MLX5DR_ROOT_LEVEL;
	enum mlx5dr_debug_res_type type;
	int i, ret;

	for (i = 0; i < matcher->num_of_mt; i++) {
		struct mlx5dr_match_template *mt = &matcher->mt[i];

		ret = fprintf(f, "%d,0x%" PRIx64 ",0x%" PRIx64 ",%d,%d,%d\n",
			      MLX5DR_DEBUG_RES_TYPE_MATCHER_MATCH_TEMPLATE,
			      (uint64_t)(uintptr_t)mt,
			      (uint64_t)(uintptr_t)matcher,
			      is_root ? 0 : mt->fc_sz,
			      mt->flags,
			      is_root ? 0 : mt->fcr_sz);
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}

		type = MLX5DR_DEBUG_RES_TYPE_MATCHER_TEMPLATE_MATCH_DEFINER;
		ret = mlx5dr_debug_dump_matcher_template_definer(f, mt, mt->definer, type);
		if (ret)
			return ret;

		type = MLX5DR_DEBUG_RES_TYPE_MATCHER_TEMPLATE_RANGE_DEFINER;
		ret = mlx5dr_debug_dump_matcher_template_definer(f, mt, mt->range_definer, type);
		if (ret)
			return ret;
	}

	type = MLX5DR_DEBUG_RES_TYPE_MATCHER_TEMPLATE_HASH_DEFINER;
	ret = mlx5dr_debug_dump_matcher_template_definer(f, matcher, matcher->hash_definer, type);
	if (ret)
		return ret;

	return 0;
}

static int
mlx5dr_debug_dump_matcher_action_template(FILE *f, struct mlx5dr_matcher *matcher)
{
	bool is_root = matcher->tbl->level == MLX5DR_ROOT_LEVEL;
	enum mlx5dr_action_type action_type;
	int i, j, ret;

	for (i = 0; i < matcher->num_of_at; i++) {
		struct mlx5dr_action_template *at = &matcher->at[i];

		ret = fprintf(f, "%d,0x%" PRIx64 ",0x%" PRIx64 ",%d,%d,%d",
			      MLX5DR_DEBUG_RES_TYPE_MATCHER_ACTION_TEMPLATE,
			      (uint64_t)(uintptr_t)at,
			      (uint64_t)(uintptr_t)matcher,
			      at->only_term ? 0 : 1,
			      is_root ? 0 : at->num_of_action_stes,
			      at->num_actions);
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}

		for (j = 0; j < at->num_actions; j++) {
			action_type = at->action_type_arr[j];
			ret = fprintf(f, ",%s", mlx5dr_debug_action_type_to_str(action_type));
			if (ret < 0) {
				rte_errno = EINVAL;
				return rte_errno;
			}
		}

		fprintf(f, "\n");
	}

	return 0;
}

static int
mlx5dr_debug_dump_matcher_attr(FILE *f, struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_matcher_attr *attr = &matcher->attr;
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",%d,%d,%d,%d,%d,%d,%d,%d\n",
		      MLX5DR_DEBUG_RES_TYPE_MATCHER_ATTR,
		      (uint64_t)(uintptr_t)matcher,
		      attr->priority,
		      attr->mode,
		      attr->table.sz_row_log,
		      attr->table.sz_col_log,
		      attr->optimize_using_rule_idx,
		      attr->optimize_flow_src,
		      attr->insert_mode,
		      attr->distribute_mode);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_debug_dump_matcher(FILE *f, struct mlx5dr_matcher *matcher)
{
	bool is_shared = mlx5dr_context_shared_gvmi_used(matcher->tbl->ctx);
	bool is_root = matcher->tbl->level == MLX5DR_ROOT_LEVEL;
	enum mlx5dr_table_type tbl_type = matcher->tbl->type;
	struct mlx5dr_cmd_ft_query_attr ft_attr = {0};
	struct mlx5dr_devx_obj *ste_0, *ste_1 = NULL;
	struct mlx5dr_pool_chunk *ste;
	struct mlx5dr_pool *ste_pool;
	uint64_t icm_addr_0 = 0;
	uint64_t icm_addr_1 = 0;
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",0x%" PRIx64 ",%d,%d,0x%" PRIx64,
		      MLX5DR_DEBUG_RES_TYPE_MATCHER,
		      (uint64_t)(uintptr_t)matcher,
		      (uint64_t)(uintptr_t)matcher->tbl,
		      matcher->num_of_mt,
		      is_root ? 0 : matcher->end_ft->id,
		      matcher->col_matcher ? (uint64_t)(uintptr_t)matcher->col_matcher : 0);
	if (ret < 0)
		goto out_err;

	ste = &matcher->match_ste.ste;
	ste_pool = matcher->match_ste.pool;
	if (ste_pool) {
		ste_0 = mlx5dr_pool_chunk_get_base_devx_obj(ste_pool, ste);
		if (tbl_type == MLX5DR_TABLE_TYPE_FDB)
			ste_1 = mlx5dr_pool_chunk_get_base_devx_obj_mirror(ste_pool, ste);
	} else {
		ste_0 = NULL;
		ste_1 = NULL;
	}

	ret = fprintf(f, ",%d,%d,%d,%d",
		      matcher->match_ste.rtc_0 ? matcher->match_ste.rtc_0->id : 0,
		      ste_0 ? (int)ste_0->id : -1,
		      matcher->match_ste.rtc_1 ? matcher->match_ste.rtc_1->id : 0,
		      ste_1 ? (int)ste_1->id : -1);
	if (ret < 0)
		goto out_err;

	ste = &matcher->action_ste.ste;
	ste_pool = matcher->action_ste.pool;
	if (ste_pool) {
		ste_0 = mlx5dr_pool_chunk_get_base_devx_obj(ste_pool, ste);
		if (tbl_type == MLX5DR_TABLE_TYPE_FDB)
			ste_1 = mlx5dr_pool_chunk_get_base_devx_obj_mirror(ste_pool, ste);
	} else {
		ste_0 = NULL;
		ste_1 = NULL;
	}

	if (!is_root) {
		ft_attr.type = matcher->tbl->fw_ft_type;
		ret = mlx5dr_cmd_flow_table_query(matcher->end_ft,
						  &ft_attr,
						  &icm_addr_0,
						  &icm_addr_1);
		if (ret)
			return ret;
	}

	ret = fprintf(f, ",%d,%d,%d,%d,%d,0x%" PRIx64 ",0x%" PRIx64 "\n",
		      matcher->action_ste.rtc_0 ? matcher->action_ste.rtc_0->id : 0,
		      ste_0 ? (int)ste_0->id : -1,
		      matcher->action_ste.rtc_1 ? matcher->action_ste.rtc_1->id : 0,
		      ste_1 ? (int)ste_1->id : -1,
		      is_shared && !is_root ?
		      matcher->match_ste.aliased_rtc_0->id : 0,
		      mlx5dr_debug_icm_to_idx(icm_addr_0),
		      mlx5dr_debug_icm_to_idx(icm_addr_1));
	if (ret < 0)
		goto out_err;

	ret = mlx5dr_debug_dump_matcher_attr(f, matcher);
	if (ret)
		return ret;

	ret = mlx5dr_debug_dump_matcher_match_template(f, matcher);
	if (ret)
		return ret;

	ret = mlx5dr_debug_dump_matcher_action_template(f, matcher);
	if (ret)
		return ret;

	return 0;

out_err:
	rte_errno = EINVAL;
	return rte_errno;
}

static int mlx5dr_debug_dump_table(FILE *f, struct mlx5dr_table *tbl)
{
	bool is_shared = mlx5dr_context_shared_gvmi_used(tbl->ctx);
	bool is_root = tbl->level == MLX5DR_ROOT_LEVEL;
	struct mlx5dr_cmd_ft_query_attr ft_attr = {0};
	struct mlx5dr_matcher *matcher;
	uint64_t local_icm_addr_0 = 0;
	uint64_t local_icm_addr_1 = 0;
	uint64_t icm_addr_0 = 0;
	uint64_t icm_addr_1 = 0;
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",0x%" PRIx64 ",%d,%d,%d,%d,%d",
		      MLX5DR_DEBUG_RES_TYPE_TABLE,
		      (uint64_t)(uintptr_t)tbl,
		      (uint64_t)(uintptr_t)tbl->ctx,
		      is_root ? 0 : tbl->ft->id,
		      tbl->type,
		      is_root ? 0 : tbl->fw_ft_type,
		      tbl->level,
		      is_shared && !is_root ? tbl->local_ft->id : 0);
	if (ret < 0)
		goto out_err;

	if (!is_root) {
		ft_attr.type = tbl->fw_ft_type;
		ret = mlx5dr_cmd_flow_table_query(tbl->ft,
						  &ft_attr,
						  &icm_addr_0,
						  &icm_addr_1);
		if (ret)
			return ret;

		if (is_shared) {
			ft_attr.type = tbl->fw_ft_type;
			ret = mlx5dr_cmd_flow_table_query(tbl->local_ft,
							  &ft_attr,
							  &local_icm_addr_0,
							  &local_icm_addr_1);
			if (ret)
				return ret;
		}
	}

	ret = fprintf(f, ",0x%" PRIx64 ",0x%" PRIx64 ",0x%" PRIx64 ",0x%" PRIx64 "\n",
		      mlx5dr_debug_icm_to_idx(icm_addr_0),
		      mlx5dr_debug_icm_to_idx(icm_addr_1),
		      mlx5dr_debug_icm_to_idx(local_icm_addr_0),
		      mlx5dr_debug_icm_to_idx(local_icm_addr_1));
	if (ret < 0)
		goto out_err;

	LIST_FOREACH(matcher, &tbl->head, next) {
		ret = mlx5dr_debug_dump_matcher(f, matcher);
		if (ret)
			return ret;
	}

	return 0;

out_err:
	rte_errno = EINVAL;
	return rte_errno;
}

static int
mlx5dr_debug_dump_context_send_engine(FILE *f, struct mlx5dr_context *ctx)
{
	struct mlx5dr_send_engine *send_queue;
	int ret, i, j;

	for (i = 0; i < (int)ctx->queues; i++) {
		send_queue = &ctx->send_queue[i];
		ret = fprintf(f, "%d,0x%" PRIx64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			      MLX5DR_DEBUG_RES_TYPE_CONTEXT_SEND_ENGINE,
			      (uint64_t)(uintptr_t)ctx,
			      i,
			      send_queue->used_entries,
			      send_queue->th_entries,
			      send_queue->rings,
			      send_queue->num_entries,
			      send_queue->err,
			      send_queue->completed.ci,
			      send_queue->completed.pi,
			      send_queue->completed.mask);
		if (ret < 0) {
			rte_errno = EINVAL;
			return rte_errno;
		}

		for (j = 0; j < MLX5DR_NUM_SEND_RINGS; j++) {
			struct mlx5dr_send_ring *send_ring = &send_queue->send_ring[j];
			struct mlx5dr_send_ring_cq *cq = &send_ring->send_cq;
			struct mlx5dr_send_ring_sq *sq = &send_ring->send_sq;

			ret = fprintf(f, "%d,0x%" PRIx64 ",%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
				      MLX5DR_DEBUG_RES_TYPE_CONTEXT_SEND_RING,
				      (uint64_t)(uintptr_t)ctx,
				      j,
				      i,
				      cq->cqn,
				      cq->cons_index,
				      cq->ncqe_mask,
				      cq->buf_sz,
				      cq->ncqe,
				      cq->cqe_log_sz,
				      cq->poll_wqe,
				      cq->cqe_sz,
				      sq->sqn,
				      sq->obj->id,
				      sq->cur_post,
				      sq->buf_mask);
			if (ret < 0) {
				rte_errno = EINVAL;
				return rte_errno;
			}
		}
	}

	return 0;
}

static int mlx5dr_debug_dump_context_caps(FILE *f, struct mlx5dr_context *ctx)
{
	struct mlx5dr_cmd_query_caps *caps = ctx->caps;
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",%s,%d,%d,%d,%d,",
		      MLX5DR_DEBUG_RES_TYPE_CONTEXT_CAPS,
		      (uint64_t)(uintptr_t)ctx,
		      caps->fw_ver,
		      caps->wqe_based_update,
		      caps->ste_format,
		      caps->ste_alloc_log_max,
		      caps->log_header_modify_argument_max_alloc);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	ret = fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		      caps->flex_protocols,
		      caps->rtc_reparse_mode,
		      caps->rtc_index_mode,
		      caps->ste_alloc_log_gran,
		      caps->stc_alloc_log_max,
		      caps->stc_alloc_log_gran,
		      caps->rtc_log_depth_max,
		      caps->format_select_gtpu_dw_0,
		      caps->format_select_gtpu_dw_1,
		      caps->format_select_gtpu_dw_2,
		      caps->format_select_gtpu_ext_dw_0,
		      caps->nic_ft.max_level,
		      caps->nic_ft.reparse,
		      caps->fdb_ft.max_level,
		      caps->fdb_ft.reparse,
		      caps->log_header_modify_argument_granularity);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_debug_dump_context_attr(FILE *f, struct mlx5dr_context *ctx)
{
	int ret;

	ret = fprintf(f, "%u,0x%" PRIx64 ",%d,%zu,%d,%s,%d,%d\n",
		      MLX5DR_DEBUG_RES_TYPE_CONTEXT_ATTR,
		      (uint64_t)(uintptr_t)ctx,
		      ctx->pd_num,
		      ctx->queues,
		      ctx->send_queue->num_entries,
		      mlx5dr_context_shared_gvmi_used(ctx) ?
		      mlx5_glue->get_device_name(ctx->ibv_ctx->device) : "None",
		      ctx->caps->vhca_id,
		      mlx5dr_context_shared_gvmi_used(ctx) ?
		      ctx->caps->shared_vhca_id : 0xffff);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_debug_dump_context_info(FILE *f, struct mlx5dr_context *ctx)
{
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",%d,%s,%s\n",
		      MLX5DR_DEBUG_RES_TYPE_CONTEXT,
		      (uint64_t)(uintptr_t)ctx,
		      ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT,
		      mlx5_glue->get_device_name(mlx5dr_context_get_local_ibv(ctx)->device),
		      DEBUG_VERSION);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	ret = mlx5dr_debug_dump_context_attr(f, ctx);
	if (ret)
		return ret;

	ret = mlx5dr_debug_dump_context_caps(f, ctx);
	if (ret)
		return ret;

	return 0;
}

static int
mlx5dr_debug_dump_context_stc_resource(FILE *f,
				       struct mlx5dr_context *ctx,
				       uint32_t tbl_type,
				       struct mlx5dr_pool_resource *resource)
{
	int ret;

	ret = fprintf(f, "%d,0x%" PRIx64 ",%u,%u\n",
		      MLX5DR_DEBUG_RES_TYPE_CONTEXT_STC,
		      (uint64_t)(uintptr_t)ctx,
		      tbl_type,
		      resource->base_id);
	if (ret < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_debug_dump_context_stc(FILE *f, struct mlx5dr_context *ctx)
{
	struct mlx5dr_pool *stc_pool;
	int ret;
	int i;

	for (i = 0; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		stc_pool = ctx->stc_pool[i];

		if (!stc_pool)
			continue;

		if (stc_pool->resource[0] != NULL) {
			ret = mlx5dr_debug_dump_context_stc_resource(f, ctx, i,
								     stc_pool->resource[0]);
			if (ret)
				return ret;
		}

		if (i == MLX5DR_TABLE_TYPE_FDB && stc_pool->mirror_resource[0] != NULL) {
			ret = mlx5dr_debug_dump_context_stc_resource(f, ctx, i,
								     stc_pool->mirror_resource[0]);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int mlx5dr_debug_dump_context(FILE *f, struct mlx5dr_context *ctx)
{
	struct mlx5dr_table *tbl;
	int ret;

	ret = mlx5dr_debug_dump_context_info(f, ctx);
	if (ret)
		return ret;

	ret = mlx5dr_debug_dump_context_send_engine(f, ctx);
	if (ret)
		return ret;

	ret = mlx5dr_debug_dump_context_stc(f, ctx);
	if (ret)
		return ret;

	LIST_FOREACH(tbl, &ctx->head, next) {
		ret = mlx5dr_debug_dump_table(f, tbl);
		if (ret)
			return ret;
	}

	return 0;
}

int mlx5dr_debug_dump(struct mlx5dr_context *ctx, FILE *f)
{
	int ret;

	if (!f || !ctx) {
		rte_errno = EINVAL;
		return -rte_errno;
	}

	pthread_spin_lock(&ctx->ctrl_lock);
	ret = mlx5dr_debug_dump_context(f, ctx);
	pthread_spin_unlock(&ctx->ctrl_lock);

	return -ret;
}
