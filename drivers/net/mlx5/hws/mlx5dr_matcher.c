/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

enum mlx5dr_matcher_rtc_type {
	DR_MATCHER_RTC_TYPE_MATCH,
	DR_MATCHER_RTC_TYPE_STE_ARRAY,
	DR_MATCHER_RTC_TYPE_MAX,
};

static const char * const mlx5dr_matcher_rtc_type_str[] = {
	[DR_MATCHER_RTC_TYPE_MATCH] = "MATCH",
	[DR_MATCHER_RTC_TYPE_STE_ARRAY] = "STE_ARRAY",
	[DR_MATCHER_RTC_TYPE_MAX] = "UNKNOWN",
};

static const char *mlx5dr_matcher_rtc_type_to_str(enum mlx5dr_matcher_rtc_type rtc_type)
{
	if (rtc_type > DR_MATCHER_RTC_TYPE_MAX)
		rtc_type = DR_MATCHER_RTC_TYPE_MAX;
	return mlx5dr_matcher_rtc_type_str[rtc_type];
}

static bool mlx5dr_matcher_requires_col_tbl(uint8_t log_num_of_rules)
{
	/* Collision table concatenation is done only for large rule tables */
	return log_num_of_rules > MLX5DR_MATCHER_ASSURED_RULES_TH;
}

static uint8_t mlx5dr_matcher_rules_to_tbl_depth(uint8_t log_num_of_rules)
{
	if (mlx5dr_matcher_requires_col_tbl(log_num_of_rules))
		return MLX5DR_MATCHER_ASSURED_MAIN_TBL_DEPTH;

	/* For small rule tables we use a single deep table to assure insertion */
	return RTE_MIN(log_num_of_rules, MLX5DR_MATCHER_ASSURED_COL_TBL_DEPTH);
}

static void mlx5dr_matcher_destroy_end_ft(struct mlx5dr_matcher *matcher)
{
	mlx5dr_table_destroy_default_ft(matcher->tbl, matcher->end_ft);
}

int mlx5dr_matcher_free_rtc_pointing(struct mlx5dr_context *ctx,
				     uint32_t fw_ft_type,
				     enum mlx5dr_table_type type,
				     struct mlx5dr_devx_obj *devx_obj)
{
	int ret;

	if (type != MLX5DR_TABLE_TYPE_FDB && !mlx5dr_context_shared_gvmi_used(ctx))
		return 0;

	ret = mlx5dr_table_ft_set_next_rtc(devx_obj, fw_ft_type, NULL, NULL);
	if (ret)
		DR_LOG(ERR, "Failed to disconnect previous RTC");

	return ret;
}

static int mlx5dr_matcher_shared_point_end_ft(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};
	int ret;

	mlx5dr_cmd_set_attr_connect_miss_tbl(matcher->tbl->ctx,
					     matcher->tbl->fw_ft_type,
					     matcher->tbl->type,
					     &ft_attr);

	ret = mlx5dr_cmd_flow_table_modify(matcher->end_ft, &ft_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to connect new matcher to default miss alias RTC");
		return ret;
	}

	ret = mlx5dr_matcher_free_rtc_pointing(matcher->tbl->ctx,
					       matcher->tbl->fw_ft_type,
					       matcher->tbl->type,
					       matcher->end_ft);

	return ret;
}

static int mlx5dr_matcher_shared_create_alias_rtc(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	int ret;

	ret = mlx5dr_matcher_create_aliased_obj(ctx,
						ctx->ibv_ctx,
						ctx->local_ibv_ctx,
						ctx->caps->shared_vhca_id,
						matcher->match_ste.rtc_0->id,
						MLX5_GENERAL_OBJ_TYPE_RTC,
						&matcher->match_ste.aliased_rtc_0);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate alias RTC");
		return ret;
	}
	return 0;
}

static int mlx5dr_matcher_create_init_shared(struct mlx5dr_matcher *matcher)
{
	if (!mlx5dr_context_shared_gvmi_used(matcher->tbl->ctx))
		return 0;

	if (mlx5dr_matcher_shared_point_end_ft(matcher)) {
		DR_LOG(ERR, "Failed to point shared matcher end flow table");
		return rte_errno;
	}

	if (mlx5dr_matcher_shared_create_alias_rtc(matcher)) {
		DR_LOG(ERR, "Failed to create alias RTC");
		return rte_errno;
	}

	return 0;
}

static void mlx5dr_matcher_create_uninit_shared(struct mlx5dr_matcher *matcher)
{
	if (!mlx5dr_context_shared_gvmi_used(matcher->tbl->ctx))
		return;

	if (matcher->match_ste.aliased_rtc_0) {
		mlx5dr_cmd_destroy_obj(matcher->match_ste.aliased_rtc_0);
		matcher->match_ste.aliased_rtc_0 = NULL;
	}
}

static int mlx5dr_matcher_create_end_ft(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_table *tbl = matcher->tbl;

	matcher->end_ft = mlx5dr_table_create_default_ft(tbl->ctx->ibv_ctx, tbl);
	if (!matcher->end_ft) {
		DR_LOG(ERR, "Failed to create matcher end flow table");
		return rte_errno;
	}
	return 0;
}

static uint32_t
mlx5dr_matcher_connect_get_rtc0(struct mlx5dr_matcher *matcher)
{
	if (!matcher->match_ste.aliased_rtc_0)
		return matcher->match_ste.rtc_0->id;
	else
		return matcher->match_ste.aliased_rtc_0->id;
}

/* The function updates tbl->local_ft to the first RTC or 0 if no more matchers */
static int mlx5dr_matcher_shared_update_local_ft(struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_ft_modify_attr cur_ft_attr = {0};
	struct mlx5dr_matcher *first_matcher;
	int ret;

	if (!mlx5dr_context_shared_gvmi_used(tbl->ctx))
		return 0;

	first_matcher = LIST_FIRST(&tbl->head);
	if (!first_matcher) {
		/* local ft no longer points to any RTC, drop refcount */
		ret = mlx5dr_matcher_free_rtc_pointing(tbl->ctx,
						       tbl->fw_ft_type,
						       tbl->type,
						       tbl->local_ft);
		if (ret)
			DR_LOG(ERR, "Failed to clear local FT to prev alias RTC");

		return ret;
	}

	/* point local_ft to the first RTC */
	cur_ft_attr.modify_fs = MLX5_IFC_MODIFY_FLOW_TABLE_RTC_ID;
	cur_ft_attr.type = tbl->fw_ft_type;
	cur_ft_attr.rtc_id_0 = mlx5dr_matcher_connect_get_rtc0(first_matcher);

	ret = mlx5dr_cmd_flow_table_modify(tbl->local_ft, &cur_ft_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to point local FT to alias RTC");
		return ret;
	}

	return 0;
}

static int mlx5dr_matcher_connect(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_matcher *prev = NULL;
	struct mlx5dr_matcher *next = NULL;
	struct mlx5dr_matcher *tmp_matcher;
	int ret;

	/* Find location in matcher list */
	if (LIST_EMPTY(&tbl->head)) {
		LIST_INSERT_HEAD(&tbl->head, matcher, next);
		goto connect;
	}

	LIST_FOREACH(tmp_matcher, &tbl->head, next) {
		if (tmp_matcher->attr.priority > matcher->attr.priority) {
			next = tmp_matcher;
			break;
		}
		prev = tmp_matcher;
	}

	if (next)
		LIST_INSERT_BEFORE(next, matcher, next);
	else
		LIST_INSERT_AFTER(prev, matcher, next);

connect:
	if (next) {
		/* Connect to next RTC */
		ret = mlx5dr_table_ft_set_next_rtc(matcher->end_ft,
						   tbl->fw_ft_type,
						   next->match_ste.rtc_0,
						   next->match_ste.rtc_1);
		if (ret) {
			DR_LOG(ERR, "Failed to connect new matcher to next RTC");
			goto remove_from_list;
		}
	} else {
		/* Connect last matcher to next miss_tbl if exists */
		ret = mlx5dr_table_connect_to_miss_table(tbl, tbl->default_miss.miss_tbl);
		if (ret) {
			DR_LOG(ERR, "Failed connect new matcher to miss_tbl");
			goto remove_from_list;
		}
	}

	/* Connect to previous FT */
	ret = mlx5dr_table_ft_set_next_rtc(prev ? prev->end_ft : tbl->ft,
					   tbl->fw_ft_type,
					   matcher->match_ste.rtc_0,
					   matcher->match_ste.rtc_1);
	if (ret) {
		DR_LOG(ERR, "Failed to connect new matcher to previous FT");
		goto remove_from_list;
	}

	ret = mlx5dr_matcher_shared_update_local_ft(tbl);
	if (ret) {
		DR_LOG(ERR, "Failed to update local_ft anchor in shared table");
		goto remove_from_list;
	}

	/* Reset next miss FT to default (drop refcount) */
	ret = mlx5dr_table_ft_set_default_next_ft(tbl, prev ? prev->end_ft : tbl->ft);
	if (ret) {
		DR_LOG(ERR, "Failed to reset matcher ft default miss");
		goto remove_from_list;
	}

	if (!prev) {
		/* Update tables missing to current matcher in the table */
		ret = mlx5dr_table_update_connected_miss_tables(tbl);
		if (ret) {
			DR_LOG(ERR, "Fatal error, failed to update connected miss table");
			goto remove_from_list;
		}
	}

	return 0;

remove_from_list:
	LIST_REMOVE(matcher, next);
	return ret;
}

static int mlx5dr_matcher_disconnect(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_matcher *tmp_matcher, *prev_matcher;
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_devx_obj *prev_ft;
	struct mlx5dr_matcher *next;
	int ret;

	prev_ft = tbl->ft;
	prev_matcher = LIST_FIRST(&tbl->head);
	LIST_FOREACH(tmp_matcher, &tbl->head, next) {
		if (tmp_matcher == matcher)
			break;

		prev_ft = tmp_matcher->end_ft;
		prev_matcher = tmp_matcher;
	}

	next = matcher->next.le_next;

	LIST_REMOVE(matcher, next);

	if (next) {
		/* Connect previous end FT to next RTC */
		ret = mlx5dr_table_ft_set_next_rtc(prev_ft,
						   tbl->fw_ft_type,
						   next->match_ste.rtc_0,
						   next->match_ste.rtc_1);
		if (ret) {
			DR_LOG(ERR, "Failed to disconnect matcher");
			goto matcher_reconnect;
		}
	} else {
		ret = mlx5dr_table_connect_to_miss_table(tbl, tbl->default_miss.miss_tbl);
		if (ret) {
			DR_LOG(ERR, "Failed to disconnect last matcher");
			goto matcher_reconnect;
		}
	}

	ret = mlx5dr_matcher_shared_update_local_ft(tbl);
	if (ret) {
		DR_LOG(ERR, "Failed to update local_ft in shared table");
		goto matcher_reconnect;
	}

	/* Removing first matcher, update connected miss tables if exists */
	if (prev_ft == tbl->ft) {
		ret = mlx5dr_table_update_connected_miss_tables(tbl);
		if (ret) {
			DR_LOG(ERR, "Fatal error, failed to update connected miss table");
			goto matcher_reconnect;
		}
	}

	ret = mlx5dr_table_ft_set_default_next_ft(tbl, prev_ft);
	if (ret) {
		DR_LOG(ERR, "Fatal error, failed to restore matcher ft default miss");
		goto matcher_reconnect;
	}

	return 0;

matcher_reconnect:
	if (LIST_EMPTY(&tbl->head))
		LIST_INSERT_HEAD(&matcher->tbl->head, matcher, next);
	else
		LIST_INSERT_AFTER(prev_matcher, matcher, next);

	return ret;
}

static bool mlx5dr_matcher_supp_fw_wqe(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_cmd_query_caps *caps = matcher->tbl->ctx->caps;

	if (matcher->flags & MLX5DR_MATCHER_FLAGS_HASH_DEFINER) {
		if (matcher->hash_definer->type == MLX5DR_DEFINER_TYPE_MATCH &&
		    !IS_BIT_SET(caps->supp_ste_format_gen_wqe, MLX5_IFC_RTC_STE_FORMAT_8DW)) {
			DR_LOG(ERR, "Gen WQE MATCH format not supported");
			return false;
		}

		if (matcher->hash_definer->type == MLX5DR_DEFINER_TYPE_JUMBO) {
			DR_LOG(ERR, "Gen WQE JUMBO format not supported");
			return false;
		}
	}

	if (matcher->attr.insert_mode != MLX5DR_MATCHER_INSERT_BY_HASH ||
	    matcher->attr.distribute_mode != MLX5DR_MATCHER_DISTRIBUTE_BY_HASH) {
		DR_LOG(ERR, "Gen WQE must be inserted and distribute by hash");
		return false;
	}

	if ((matcher->flags & MLX5DR_MATCHER_FLAGS_RANGE_DEFINER) &&
	    !IS_BIT_SET(caps->supp_ste_format_gen_wqe, MLX5_IFC_RTC_STE_FORMAT_RANGE)) {
		DR_LOG(INFO, "Extended match gen wqe RANGE format not supported");
		return false;
	}

	if (!(caps->supp_type_gen_wqe & MLX5_GENERATE_WQE_TYPE_FLOW_UPDATE)) {
		DR_LOG(ERR, "Gen WQE command not supporting GTA");
		return false;
	}

	if (!caps->rtc_max_hash_def_gen_wqe) {
		DR_LOG(ERR, "Hash definer not supported");
		return false;
	}

	return true;
}

static void mlx5dr_matcher_set_rtc_attr_sz(struct mlx5dr_matcher *matcher,
					   struct mlx5dr_cmd_rtc_create_attr *rtc_attr,
					   enum mlx5dr_matcher_rtc_type rtc_type,
					   bool is_mirror)
{
	enum mlx5dr_matcher_flow_src flow_src = matcher->attr.optimize_flow_src;
	bool is_match_rtc = rtc_type == DR_MATCHER_RTC_TYPE_MATCH;
	struct mlx5dr_pool_chunk *ste = &matcher->action_ste.ste;

	if ((flow_src == MLX5DR_MATCHER_FLOW_SRC_VPORT && !is_mirror) ||
	    (flow_src == MLX5DR_MATCHER_FLOW_SRC_WIRE && is_mirror)) {
		/* Optimize FDB RTC */
		rtc_attr->log_size = 0;
		rtc_attr->log_depth = 0;
	} else {
		/* Keep original values */
		rtc_attr->log_size = is_match_rtc ? matcher->attr.table.sz_row_log : ste->order;
		rtc_attr->log_depth = is_match_rtc ? matcher->attr.table.sz_col_log : 0;
	}
}

int mlx5dr_matcher_create_aliased_obj(struct mlx5dr_context *ctx,
				      struct ibv_context *ibv_owner,
				      struct ibv_context *ibv_allowed,
				      uint16_t vhca_id_to_be_accessed,
				      uint32_t aliased_object_id,
				      uint16_t object_type,
				      struct mlx5dr_devx_obj **obj)
{
	struct mlx5dr_cmd_allow_other_vhca_access_attr allow_attr = {0};
	struct mlx5dr_cmd_alias_obj_create_attr alias_attr = {0};
	char key[ACCESS_KEY_LEN];
	int ret;
	int i;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return 0;

	for (i = 0; i < ACCESS_KEY_LEN; i++)
		key[i] = rte_rand() & 0xFF;

	memcpy(allow_attr.access_key, key, ACCESS_KEY_LEN);
	allow_attr.obj_type = object_type;
	allow_attr.obj_id = aliased_object_id;

	ret = mlx5dr_cmd_allow_other_vhca_access(ibv_owner, &allow_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to allow RTC to be aliased");
		return ret;
	}

	memcpy(alias_attr.access_key, key, ACCESS_KEY_LEN);
	alias_attr.obj_id = aliased_object_id;
	alias_attr.obj_type = object_type;
	alias_attr.vhca_id = vhca_id_to_be_accessed;
	*obj = mlx5dr_cmd_alias_obj_create(ibv_allowed, &alias_attr);
	if (!*obj) {
		DR_LOG(ERR, "Failed to create alias object");
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_matcher_create_rtc(struct mlx5dr_matcher *matcher,
				     enum mlx5dr_matcher_rtc_type rtc_type)
{
	struct mlx5dr_matcher_attr *attr = &matcher->attr;
	struct mlx5dr_cmd_rtc_create_attr rtc_attr = {0};
	struct mlx5dr_match_template *mt = matcher->mt;
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_action_default_stc *default_stc;
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_devx_obj **rtc_0, **rtc_1;
	struct mlx5dr_pool *ste_pool, *stc_pool;
	struct mlx5dr_devx_obj *devx_obj;
	struct mlx5dr_pool_chunk *ste;
	int ret;

	switch (rtc_type) {
	case DR_MATCHER_RTC_TYPE_MATCH:
		rtc_0 = &matcher->match_ste.rtc_0;
		rtc_1 = &matcher->match_ste.rtc_1;
		ste_pool = matcher->match_ste.pool;
		ste = &matcher->match_ste.ste;
		ste->order = attr->table.sz_col_log + attr->table.sz_row_log;

		/* Add additional rows due to additional range STE */
		if (mlx5dr_matcher_mt_is_range(mt))
			ste->order++;

		rtc_attr.log_size = attr->table.sz_row_log;
		rtc_attr.log_depth = attr->table.sz_col_log;
		rtc_attr.is_frst_jumbo = mlx5dr_matcher_mt_is_jumbo(mt);
		rtc_attr.is_scnd_range = mlx5dr_matcher_mt_is_range(mt);
		rtc_attr.miss_ft_id = matcher->end_ft->id;

		if (attr->insert_mode == MLX5DR_MATCHER_INSERT_BY_HASH) {
			/* The usual Hash Table */
			rtc_attr.update_index_mode = MLX5_IFC_RTC_STE_UPDATE_MODE_BY_HASH;

			if (matcher->hash_definer) {
				/* Specify definer_id_0 is used for hashing */
				rtc_attr.fw_gen_wqe = true;
				rtc_attr.num_hash_definer = 1;
				rtc_attr.match_definer_0 =
					mlx5dr_definer_get_id(matcher->hash_definer);
			} else {
				/* The first mt is used since all share the same definer */
				rtc_attr.match_definer_0 = mlx5dr_definer_get_id(mt->definer);

				/* This is tricky, instead of passing two definers for
				 * match and range, we specify that this RTC uses a hash
				 * definer, this will allow us to use any range definer
				 * since only first STE is used for hashing anyways.
				 */
				if (matcher->flags & MLX5DR_MATCHER_FLAGS_RANGE_DEFINER) {
					rtc_attr.fw_gen_wqe = true;
					rtc_attr.num_hash_definer = 1;
				}
			}
		} else if (attr->insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX) {
			rtc_attr.update_index_mode = MLX5_IFC_RTC_STE_UPDATE_MODE_BY_OFFSET;
			rtc_attr.num_hash_definer = 1;

			if (attr->distribute_mode == MLX5DR_MATCHER_DISTRIBUTE_BY_HASH) {
				/* Hash Split Table */
				rtc_attr.access_index_mode = MLX5_IFC_RTC_STE_ACCESS_MODE_BY_HASH;
				rtc_attr.match_definer_0 = mlx5dr_definer_get_id(mt->definer);
			} else if (attr->distribute_mode == MLX5DR_MATCHER_DISTRIBUTE_BY_LINEAR) {
				/* Linear Lookup Table */
				rtc_attr.access_index_mode = MLX5_IFC_RTC_STE_ACCESS_MODE_LINEAR;
				rtc_attr.match_definer_0 = ctx->caps->linear_match_definer;
			}
		}

		/* Match pool requires implicit allocation */
		ret = mlx5dr_pool_chunk_alloc(ste_pool, ste);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate STE for %s RTC",
			       mlx5dr_matcher_rtc_type_to_str(rtc_type));
			return ret;
		}
		break;

	case DR_MATCHER_RTC_TYPE_STE_ARRAY:
		rtc_0 = &matcher->action_ste.rtc_0;
		rtc_1 = &matcher->action_ste.rtc_1;
		ste_pool = matcher->action_ste.pool;
		ste = &matcher->action_ste.ste;
		ste->order = rte_log2_u32(matcher->action_ste.max_stes) +
			     attr->table.sz_row_log;
		rtc_attr.log_size = ste->order;
		rtc_attr.log_depth = 0;
		rtc_attr.update_index_mode = MLX5_IFC_RTC_STE_UPDATE_MODE_BY_OFFSET;
		/* The action STEs use the default always hit definer */
		rtc_attr.match_definer_0 = ctx->caps->trivial_match_definer;
		rtc_attr.is_frst_jumbo = false;
		rtc_attr.miss_ft_id = 0;
		break;

	default:
		DR_LOG(ERR, "HWS Invalid RTC type");
		rte_errno = EINVAL;
		return rte_errno;
	}

	devx_obj = mlx5dr_pool_chunk_get_base_devx_obj(ste_pool, ste);

	rtc_attr.pd = ctx->pd_num;
	rtc_attr.ste_base = devx_obj->id;
	rtc_attr.ste_offset = ste->offset;
	rtc_attr.reparse_mode = mlx5dr_context_get_reparse_mode(ctx);
	rtc_attr.table_type = mlx5dr_table_get_res_fw_ft_type(tbl->type, false);
	mlx5dr_matcher_set_rtc_attr_sz(matcher, &rtc_attr, rtc_type, false);

	/* STC is a single resource (devx_obj), use any STC for the ID */
	stc_pool = ctx->stc_pool[tbl->type];
	default_stc = ctx->common_res[tbl->type].default_stc;
	devx_obj = mlx5dr_pool_chunk_get_base_devx_obj(stc_pool, &default_stc->default_hit);
	rtc_attr.stc_base = devx_obj->id;

	*rtc_0 = mlx5dr_cmd_rtc_create(ctx->ibv_ctx, &rtc_attr);
	if (!*rtc_0) {
		DR_LOG(ERR, "Failed to create matcher RTC of type %s",
		       mlx5dr_matcher_rtc_type_to_str(rtc_type));
		goto free_ste;
	}

	if (tbl->type == MLX5DR_TABLE_TYPE_FDB) {
		devx_obj = mlx5dr_pool_chunk_get_base_devx_obj_mirror(ste_pool, ste);
		rtc_attr.ste_base = devx_obj->id;
		rtc_attr.table_type = mlx5dr_table_get_res_fw_ft_type(tbl->type, true);

		devx_obj = mlx5dr_pool_chunk_get_base_devx_obj_mirror(stc_pool, &default_stc->default_hit);
		rtc_attr.stc_base = devx_obj->id;
		mlx5dr_matcher_set_rtc_attr_sz(matcher, &rtc_attr, rtc_type, true);

		*rtc_1 = mlx5dr_cmd_rtc_create(ctx->ibv_ctx, &rtc_attr);
		if (!*rtc_1) {
			DR_LOG(ERR, "Failed to create peer matcher RTC of type %s",
			       mlx5dr_matcher_rtc_type_to_str(rtc_type));
			goto destroy_rtc_0;
		}
	}

	return 0;

destroy_rtc_0:
	mlx5dr_cmd_destroy_obj(*rtc_0);
free_ste:
	if (rtc_type == DR_MATCHER_RTC_TYPE_MATCH)
		mlx5dr_pool_chunk_free(ste_pool, ste);
	return rte_errno;
}

static void mlx5dr_matcher_destroy_rtc(struct mlx5dr_matcher *matcher,
				       enum mlx5dr_matcher_rtc_type rtc_type)
{
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_devx_obj *rtc_0, *rtc_1;
	struct mlx5dr_pool_chunk *ste;
	struct mlx5dr_pool *ste_pool;

	switch (rtc_type) {
	case DR_MATCHER_RTC_TYPE_MATCH:
		rtc_0 = matcher->match_ste.rtc_0;
		rtc_1 = matcher->match_ste.rtc_1;
		ste_pool = matcher->match_ste.pool;
		ste = &matcher->match_ste.ste;
		break;
	case DR_MATCHER_RTC_TYPE_STE_ARRAY:
		rtc_0 = matcher->action_ste.rtc_0;
		rtc_1 = matcher->action_ste.rtc_1;
		ste_pool = matcher->action_ste.pool;
		ste = &matcher->action_ste.ste;
		break;
	default:
		return;
	}

	if (tbl->type == MLX5DR_TABLE_TYPE_FDB)
		mlx5dr_cmd_destroy_obj(rtc_1);

	mlx5dr_cmd_destroy_obj(rtc_0);
	if (rtc_type == DR_MATCHER_RTC_TYPE_MATCH)
		mlx5dr_pool_chunk_free(ste_pool, ste);
}

static int
mlx5dr_matcher_check_attr_sz(struct mlx5dr_cmd_query_caps *caps,
			     struct mlx5dr_matcher_attr *attr)
{
	if (attr->table.sz_col_log > caps->rtc_log_depth_max) {
		DR_LOG(ERR, "Matcher depth exceeds limit %d", caps->rtc_log_depth_max);
		goto not_supported;
	}

	if (attr->table.sz_col_log + attr->table.sz_row_log > caps->ste_alloc_log_max) {
		DR_LOG(ERR, "Total matcher size exceeds limit %d", caps->ste_alloc_log_max);
		goto not_supported;
	}

	if (attr->table.sz_col_log + attr->table.sz_row_log < caps->ste_alloc_log_gran) {
		DR_LOG(ERR, "Total matcher size below limit %d", caps->ste_alloc_log_gran);
		goto not_supported;
	}

	return 0;

not_supported:
	rte_errno = EOPNOTSUPP;
	return rte_errno;
}

static void mlx5dr_matcher_set_pool_attr(struct mlx5dr_pool_attr *attr,
					 struct mlx5dr_matcher *matcher)
{
	switch (matcher->attr.optimize_flow_src) {
	case MLX5DR_MATCHER_FLOW_SRC_VPORT:
		attr->opt_type = MLX5DR_POOL_OPTIMIZE_ORIG;
		break;
	case MLX5DR_MATCHER_FLOW_SRC_WIRE:
		attr->opt_type = MLX5DR_POOL_OPTIMIZE_MIRROR;
		break;
	default:
		break;
	}
}

static int mlx5dr_matcher_check_and_process_at(struct mlx5dr_matcher *matcher,
					       struct mlx5dr_action_template *at)
{
	bool valid;
	int ret;

	/* Check if action combinabtion is valid */
	valid = mlx5dr_action_check_combo(at->action_type_arr, matcher->tbl->type);
	if (!valid) {
		DR_LOG(ERR, "Invalid combination in action template");
		rte_errno = EINVAL;
		return rte_errno;
	}

	/* Process action template to setters */
	ret = mlx5dr_action_template_process(at);
	if (ret) {
		DR_LOG(ERR, "Failed to process action template");
		return ret;
	}

	return 0;
}

static int mlx5dr_matcher_bind_at(struct mlx5dr_matcher *matcher)
{
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(matcher->mt);
	struct mlx5dr_cmd_stc_modify_attr stc_attr = {0};
	struct mlx5dr_table *tbl = matcher->tbl;
	struct mlx5dr_pool_attr pool_attr = {0};
	struct mlx5dr_context *ctx = tbl->ctx;
	uint32_t required_stes;
	int i, ret;

	if (matcher->flags & MLX5DR_MATCHER_FLAGS_COLLISION)
		return 0;

	for (i = 0; i < matcher->num_of_at; i++) {
		struct mlx5dr_action_template *at = &matcher->at[i];

		ret = mlx5dr_matcher_check_and_process_at(matcher, at);
		if (ret) {
			DR_LOG(ERR, "Invalid at %d", i);
			return rte_errno;
		}

		required_stes = at->num_of_action_stes - (!is_jumbo || at->only_term);
		matcher->action_ste.max_stes = RTE_MAX(matcher->action_ste.max_stes, required_stes);

		/* Future: Optimize reparse */
	}

	/* There are no additioanl STEs required for matcher */
	if (!matcher->action_ste.max_stes)
		return 0;

	if (mlx5dr_matcher_req_fw_wqe(matcher)) {
		DR_LOG(ERR, "FW extended matcher cannot be binded to complex at");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	/* Allocate action STE mempool */
	pool_attr.table_type = tbl->type;
	pool_attr.pool_type = MLX5DR_POOL_TYPE_STE;
	pool_attr.flags = MLX5DR_POOL_FLAGS_FOR_STE_ACTION_POOL;
	pool_attr.alloc_log_sz = rte_log2_u32(matcher->action_ste.max_stes) +
				 matcher->attr.table.sz_row_log;
	mlx5dr_matcher_set_pool_attr(&pool_attr, matcher);
	matcher->action_ste.pool = mlx5dr_pool_create(ctx, &pool_attr);
	if (!matcher->action_ste.pool) {
		DR_LOG(ERR, "Failed to create action ste pool");
		return rte_errno;
	}

	/* Allocate action RTC */
	ret = mlx5dr_matcher_create_rtc(matcher, DR_MATCHER_RTC_TYPE_STE_ARRAY);
	if (ret) {
		DR_LOG(ERR, "Failed to create action RTC");
		goto free_ste_pool;
	}

	/* Allocate STC for jumps to STE */
	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_HIT;
	stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_STE_TABLE;
	stc_attr.reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;
	stc_attr.ste_table.ste = matcher->action_ste.ste;
	stc_attr.ste_table.ste_pool = matcher->action_ste.pool;
	stc_attr.ste_table.match_definer_id = ctx->caps->trivial_match_definer;

	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl->type,
					     &matcher->action_ste.stc);
	if (ret) {
		DR_LOG(ERR, "Failed to create action jump to table STC");
		goto free_rtc;
	}

	return 0;

free_rtc:
	mlx5dr_matcher_destroy_rtc(matcher, DR_MATCHER_RTC_TYPE_STE_ARRAY);
free_ste_pool:
	mlx5dr_pool_destroy(matcher->action_ste.pool);
	return rte_errno;
}

static void mlx5dr_matcher_unbind_at(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_table *tbl = matcher->tbl;

	if (!matcher->action_ste.max_stes || matcher->flags & MLX5DR_MATCHER_FLAGS_COLLISION)
		return;

	mlx5dr_action_free_single_stc(tbl->ctx, tbl->type, &matcher->action_ste.stc);
	mlx5dr_matcher_destroy_rtc(matcher, DR_MATCHER_RTC_TYPE_STE_ARRAY);
	mlx5dr_pool_destroy(matcher->action_ste.pool);
}

static int mlx5dr_matcher_bind_mt(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_pool_attr pool_attr = {0};
	int ret;

	/* Calculate match, range and hash definers */
	ret = mlx5dr_definer_matcher_init(ctx, matcher);
	if (ret) {
		DR_LOG(ERR, "Failed to set matcher templates with match definers");
		return ret;
	}

	if (mlx5dr_matcher_req_fw_wqe(matcher) &&
	    !mlx5dr_matcher_supp_fw_wqe(matcher)) {
		DR_LOG(ERR, "Matcher requires FW WQE which is not supported");
		rte_errno = ENOTSUP;
		ret = rte_errno;
		goto uninit_match_definer;
	}

	/* Create an STE pool per matcher*/
	pool_attr.table_type = matcher->tbl->type;
	pool_attr.pool_type = MLX5DR_POOL_TYPE_STE;
	pool_attr.flags = MLX5DR_POOL_FLAGS_FOR_MATCHER_STE_POOL;
	pool_attr.alloc_log_sz = matcher->attr.table.sz_col_log +
				 matcher->attr.table.sz_row_log;
	/* Add additional rows due to additional range STE */
	if (matcher->flags & MLX5DR_MATCHER_FLAGS_RANGE_DEFINER)
		pool_attr.alloc_log_sz++;
	mlx5dr_matcher_set_pool_attr(&pool_attr, matcher);

	matcher->match_ste.pool = mlx5dr_pool_create(ctx, &pool_attr);
	if (!matcher->match_ste.pool) {
		DR_LOG(ERR, "Failed to allocate matcher STE pool");
		ret = ENOTSUP;
		goto uninit_match_definer;
	}

	return 0;

uninit_match_definer:
	mlx5dr_definer_matcher_uninit(matcher);
	return ret;
}

static void mlx5dr_matcher_unbind_mt(struct mlx5dr_matcher *matcher)
{
	mlx5dr_pool_destroy(matcher->match_ste.pool);
	mlx5dr_definer_matcher_uninit(matcher);
}

static int
mlx5dr_matcher_validate_insert_mode(struct mlx5dr_cmd_query_caps *caps,
				    struct mlx5dr_matcher *matcher,
				    bool is_root)
{
	struct mlx5dr_matcher_attr *attr = &matcher->attr;

	if (is_root) {
		if (attr->mode != MLX5DR_MATCHER_RESOURCE_MODE_RULE) {
			DR_LOG(ERR, "Root matcher supports only rule resource mode");
			goto not_supported;
		}
		if (attr->insert_mode != MLX5DR_MATCHER_INSERT_BY_HASH) {
			DR_LOG(ERR, "Root matcher supports only insert by hash mode");
			goto not_supported;
		}
		if (attr->distribute_mode != MLX5DR_MATCHER_DISTRIBUTE_BY_HASH) {
			DR_LOG(ERR, "Root matcher supports only distribute by hash mode");
			goto not_supported;
		}
		if (attr->optimize_flow_src) {
			DR_LOG(ERR, "Root matcher can't specify FDB direction");
			goto not_supported;
		}
	}

	switch (attr->insert_mode) {
	case MLX5DR_MATCHER_INSERT_BY_HASH:
		if (matcher->attr.distribute_mode != MLX5DR_MATCHER_DISTRIBUTE_BY_HASH) {
			DR_LOG(ERR, "Invalid matcher distribute mode");
			goto not_supported;
		}
		break;

	case MLX5DR_MATCHER_INSERT_BY_INDEX:
		if (attr->table.sz_col_log) {
			DR_LOG(ERR, "Matcher with INSERT_BY_INDEX supports only Nx1 table size");
			goto not_supported;
		}

		if (attr->distribute_mode == MLX5DR_MATCHER_DISTRIBUTE_BY_HASH) {
			/* Hash Split Table */
			if (!caps->rtc_hash_split_table) {
				DR_LOG(ERR, "FW doesn't support insert by index and hash distribute");
				goto not_supported;
			}
		} else if (attr->distribute_mode == MLX5DR_MATCHER_DISTRIBUTE_BY_LINEAR) {
			/* Linear Lookup Table */
			if (!caps->rtc_linear_lookup_table ||
			    !IS_BIT_SET(caps->access_index_mode,
					MLX5_IFC_RTC_STE_ACCESS_MODE_LINEAR)) {
				DR_LOG(ERR, "FW doesn't support insert by index and linear distribute");
				goto not_supported;
			}

			if (attr->table.sz_row_log > MLX5_IFC_RTC_LINEAR_LOOKUP_TBL_LOG_MAX) {
				DR_LOG(ERR, "Matcher with linear distribute: rows exceed limit %d",
				       MLX5_IFC_RTC_LINEAR_LOOKUP_TBL_LOG_MAX);
				goto not_supported;
			}
		} else {
			DR_LOG(ERR, "Matcher has unsupported distribute mode");
			goto not_supported;
		}
		break;

	default:
		DR_LOG(ERR, "Matcher has unsupported insert mode");
		goto not_supported;
	}

	return 0;

not_supported:
	rte_errno = EOPNOTSUPP;
	return rte_errno;
}

static int
mlx5dr_matcher_process_attr(struct mlx5dr_cmd_query_caps *caps,
			    struct mlx5dr_matcher *matcher,
			    bool is_root)
{
	struct mlx5dr_matcher_attr *attr = &matcher->attr;

	if (mlx5dr_matcher_validate_insert_mode(caps, matcher, is_root))
		goto not_supported;

	if (is_root) {
		if (attr->optimize_flow_src) {
			DR_LOG(ERR, "Root matcher can't specify FDB direction");
			goto not_supported;
		}
		if (attr->max_num_of_at_attach) {
			DR_LOG(ERR, "Root matcher does not support at attaching");
			goto not_supported;
		}
		return 0;
	}

	if (matcher->tbl->type != MLX5DR_TABLE_TYPE_FDB  && attr->optimize_flow_src) {
		DR_LOG(ERR, "NIC domain doesn't support flow_src");
		goto not_supported;
	}

	/* Convert number of rules to the required depth */
	if (attr->mode == MLX5DR_MATCHER_RESOURCE_MODE_RULE &&
	    attr->insert_mode == MLX5DR_MATCHER_INSERT_BY_HASH)
		attr->table.sz_col_log = mlx5dr_matcher_rules_to_tbl_depth(attr->rule.num_log);

	return mlx5dr_matcher_check_attr_sz(caps, attr);

not_supported:
	rte_errno = EOPNOTSUPP;
	return rte_errno;
}

static int mlx5dr_matcher_create_and_connect(struct mlx5dr_matcher *matcher)
{
	int ret;

	/* Select and create the definers for current matcher */
	ret = mlx5dr_matcher_bind_mt(matcher);
	if (ret)
		return ret;

	/* Calculate and verify action combination */
	ret = mlx5dr_matcher_bind_at(matcher);
	if (ret)
		goto unbind_mt;

	/* Create matcher end flow table anchor */
	ret = mlx5dr_matcher_create_end_ft(matcher);
	if (ret)
		goto unbind_at;

	/* Allocate the RTC for the new matcher */
	ret = mlx5dr_matcher_create_rtc(matcher, DR_MATCHER_RTC_TYPE_MATCH);
	if (ret)
		goto destroy_end_ft;

	/* Allocate and set shared resources */
	ret = mlx5dr_matcher_create_init_shared(matcher);
	if (ret)
		goto destroy_rtc;

	/* Connect the matcher to the matcher list */
	ret = mlx5dr_matcher_connect(matcher);
	if (ret)
		goto destroy_shared;

	return 0;

destroy_shared:
	mlx5dr_matcher_create_uninit_shared(matcher);
destroy_rtc:
	mlx5dr_matcher_destroy_rtc(matcher, DR_MATCHER_RTC_TYPE_MATCH);
destroy_end_ft:
	mlx5dr_matcher_destroy_end_ft(matcher);
unbind_at:
	mlx5dr_matcher_unbind_at(matcher);
unbind_mt:
	mlx5dr_matcher_unbind_mt(matcher);
	return ret;
}

static void mlx5dr_matcher_destroy_and_disconnect(struct mlx5dr_matcher *matcher)
{
	mlx5dr_matcher_disconnect(matcher);
	mlx5dr_matcher_create_uninit_shared(matcher);
	mlx5dr_matcher_destroy_rtc(matcher, DR_MATCHER_RTC_TYPE_MATCH);
	mlx5dr_matcher_destroy_end_ft(matcher);
	mlx5dr_matcher_unbind_at(matcher);
	mlx5dr_matcher_unbind_mt(matcher);
}

static int
mlx5dr_matcher_create_col_matcher(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_matcher *col_matcher;
	int ret;

	if (matcher->attr.mode != MLX5DR_MATCHER_RESOURCE_MODE_RULE ||
	    matcher->attr.insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX)
		return 0;

	if (!mlx5dr_matcher_requires_col_tbl(matcher->attr.rule.num_log))
		return 0;

	col_matcher = simple_calloc(1, sizeof(*matcher));
	if (!col_matcher) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	col_matcher->tbl = matcher->tbl;
	col_matcher->mt = matcher->mt;
	col_matcher->at = matcher->at;
	col_matcher->num_of_at = matcher->num_of_at;
	col_matcher->num_of_mt = matcher->num_of_mt;
	col_matcher->hash_definer = matcher->hash_definer;
	col_matcher->attr.priority = matcher->attr.priority;
	col_matcher->flags = matcher->flags;
	col_matcher->flags |= MLX5DR_MATCHER_FLAGS_COLLISION;
	col_matcher->attr.mode = MLX5DR_MATCHER_RESOURCE_MODE_HTABLE;
	col_matcher->attr.optimize_flow_src = matcher->attr.optimize_flow_src;
	col_matcher->attr.table.sz_row_log = matcher->attr.rule.num_log;
	col_matcher->attr.table.sz_col_log = MLX5DR_MATCHER_ASSURED_COL_TBL_DEPTH;
	if (col_matcher->attr.table.sz_row_log > MLX5DR_MATCHER_ASSURED_ROW_RATIO)
		col_matcher->attr.table.sz_row_log -= MLX5DR_MATCHER_ASSURED_ROW_RATIO;

	col_matcher->attr.max_num_of_at_attach = matcher->attr.max_num_of_at_attach;

	ret = mlx5dr_matcher_process_attr(ctx->caps, col_matcher, false);
	if (ret)
		goto free_col_matcher;

	ret = mlx5dr_matcher_create_and_connect(col_matcher);
	if (ret)
		goto free_col_matcher;

	matcher->col_matcher = col_matcher;

	return 0;

free_col_matcher:
	simple_free(col_matcher);
	DR_LOG(ERR, "Failed to create assured collision matcher");
	return ret;
}

static void
mlx5dr_matcher_destroy_col_matcher(struct mlx5dr_matcher *matcher)
{
	if (matcher->attr.mode != MLX5DR_MATCHER_RESOURCE_MODE_RULE ||
	    matcher->attr.insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX)
		return;

	if (matcher->col_matcher) {
		mlx5dr_matcher_destroy_and_disconnect(matcher->col_matcher);
		simple_free(matcher->col_matcher);
	}
}

static int mlx5dr_matcher_init(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	int ret;

	pthread_spin_lock(&ctx->ctrl_lock);

	/* Allocate matcher resource and connect to the packet pipe */
	ret = mlx5dr_matcher_create_and_connect(matcher);
	if (ret)
		goto unlock_err;

	/* Create additional matcher for collision handling */
	ret = mlx5dr_matcher_create_col_matcher(matcher);
	if (ret)
		goto destory_and_disconnect;

	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

destory_and_disconnect:
	mlx5dr_matcher_destroy_and_disconnect(matcher);
unlock_err:
	pthread_spin_unlock(&ctx->ctrl_lock);
	return ret;
}

static int mlx5dr_matcher_uninit(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;

	pthread_spin_lock(&ctx->ctrl_lock);
	mlx5dr_matcher_destroy_col_matcher(matcher);
	mlx5dr_matcher_destroy_and_disconnect(matcher);
	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;
}

static int mlx5dr_matcher_init_root(struct mlx5dr_matcher *matcher)
{
	enum mlx5dr_table_type type = matcher->tbl->type;
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dv_flow_matcher_attr attr = {0};
	struct mlx5dv_flow_match_parameters *mask;
	struct mlx5_flow_attr flow_attr = {0};
	struct rte_flow_error rte_error;
	struct rte_flow_item *item;
	uint8_t match_criteria;
	int ret;

#ifdef HAVE_MLX5DV_FLOW_MATCHER_FT_TYPE
	attr.comp_mask = MLX5DV_FLOW_MATCHER_MASK_FT_TYPE;

	switch (type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
		attr.ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
		break;
	case MLX5DR_TABLE_TYPE_NIC_TX:
		attr.ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX;
		break;
	case MLX5DR_TABLE_TYPE_FDB:
		attr.ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
		break;
	default:
		assert(0);
		break;
	}
#endif

	if (matcher->attr.priority > UINT16_MAX) {
		DR_LOG(ERR, "Root matcher priority exceeds allowed limit");
		rte_errno = EINVAL;
		return rte_errno;
	}

	/* We need the port id in case of matching representor */
	item = matcher->mt[0].items;
	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (item->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR ||
		    item->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT) {
			ret = flow_hw_get_port_id_from_ctx(ctx, &flow_attr.port_id);
			if (ret) {
				DR_LOG(ERR, "Failed to get port id for dev %s",
				       ctx->ibv_ctx->device->name);
				rte_errno = EINVAL;
				return rte_errno;
			}
		}
		++item;
	}

	mask = simple_calloc(1, MLX5_ST_SZ_BYTES(fte_match_param) +
			     offsetof(struct mlx5dv_flow_match_parameters, match_buf));
	if (!mask) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	flow_attr.tbl_type = type;

	/* On root table matcher, only a single match template is supported */
	ret = flow_dv_translate_items_hws(matcher->mt[0].items,
					  &flow_attr, mask->match_buf,
					  MLX5_SET_MATCHER_HS_M, NULL,
					  &match_criteria,
					  &rte_error);
	if (ret) {
		DR_LOG(ERR, "Failed to convert items to PRM [%s]", rte_error.message);
		goto free_mask;
	}

	mask->match_sz = MLX5_ST_SZ_BYTES(fte_match_param);
	attr.match_mask = mask;
	attr.match_criteria_enable = match_criteria;
	attr.type = IBV_FLOW_ATTR_NORMAL;
	attr.priority = matcher->attr.priority;

	matcher->dv_matcher =
		mlx5_glue->dv_create_flow_matcher_root(mlx5dr_context_get_local_ibv(ctx),
						       &attr);
	if (!matcher->dv_matcher) {
		DR_LOG(ERR, "Failed to create DV flow matcher");
		rte_errno = errno;
		goto free_mask;
	}

	simple_free(mask);

	pthread_spin_lock(&ctx->ctrl_lock);
	LIST_INSERT_HEAD(&matcher->tbl->head, matcher, next);
	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

free_mask:
	simple_free(mask);
	return rte_errno;
}

static int mlx5dr_matcher_uninit_root(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	int ret;

	pthread_spin_lock(&ctx->ctrl_lock);
	LIST_REMOVE(matcher, next);
	pthread_spin_unlock(&ctx->ctrl_lock);

	ret = mlx5_glue->dv_destroy_flow_matcher_root(matcher->dv_matcher);
	if (ret) {
		DR_LOG(ERR, "Failed to Destroy DV flow matcher");
		rte_errno = errno;
	}

	return ret;
}

int mlx5dr_matcher_attach_at(struct mlx5dr_matcher *matcher,
			     struct mlx5dr_action_template *at)
{
	bool is_jumbo = mlx5dr_matcher_mt_is_jumbo(matcher->mt);
	uint32_t required_stes;
	int ret;

	if (!matcher->attr.max_num_of_at_attach) {
		DR_LOG(ERR, "Num of current at (%d) exceed allowed value",
		       matcher->num_of_at);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	ret = mlx5dr_matcher_check_and_process_at(matcher, at);
	if (ret)
		return -rte_errno;

	required_stes = at->num_of_action_stes - (!is_jumbo || at->only_term);
	if (matcher->action_ste.max_stes < required_stes) {
		DR_LOG(ERR, "Required STEs [%d] exceeds initial action template STE [%d]",
		       required_stes, matcher->action_ste.max_stes);
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	matcher->at[matcher->num_of_at] = *at;
	matcher->num_of_at += 1;
	matcher->attr.max_num_of_at_attach -= 1;

	if (matcher->col_matcher)
		matcher->col_matcher->num_of_at = matcher->num_of_at;

	return 0;
}

static int
mlx5dr_matcher_set_templates(struct mlx5dr_matcher *matcher,
			     struct mlx5dr_match_template *mt[],
			     uint8_t num_of_mt,
			     struct mlx5dr_action_template *at[],
			     uint8_t num_of_at)
{
	bool is_root = mlx5dr_table_is_root(matcher->tbl);
	int i;

	if (!num_of_mt || !num_of_at) {
		DR_LOG(ERR, "Number of action/match template cannot be zero");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (is_root && num_of_mt > MLX5DR_MATCHER_MAX_MT_ROOT) {
		DR_LOG(ERR, "Number of match templates exceeds limit");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	matcher->mt = simple_calloc(num_of_mt, sizeof(*matcher->mt));
	if (!matcher->mt) {
		DR_LOG(ERR, "Failed to allocate match template array");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	matcher->at = simple_calloc(num_of_at + matcher->attr.max_num_of_at_attach,
				    sizeof(*matcher->at));
	if (!matcher->at) {
		DR_LOG(ERR, "Failed to allocate action template array");
		rte_errno = ENOMEM;
		goto free_mt;
	}

	for (i = 0; i < num_of_mt; i++)
		matcher->mt[i] = *mt[i];

	for (i = 0; i < num_of_at; i++)
		matcher->at[i] = *at[i];

	matcher->num_of_mt = num_of_mt;
	matcher->num_of_at = num_of_at;

	return 0;

free_mt:
	simple_free(matcher->mt);
	return rte_errno;
}

static void
mlx5dr_matcher_unset_templates(struct mlx5dr_matcher *matcher)
{
	simple_free(matcher->at);
	simple_free(matcher->mt);
}

struct mlx5dr_matcher *
mlx5dr_matcher_create(struct mlx5dr_table *tbl,
		      struct mlx5dr_match_template *mt[],
		      uint8_t num_of_mt,
		      struct mlx5dr_action_template *at[],
		      uint8_t num_of_at,
		      struct mlx5dr_matcher_attr *attr)
{
	bool is_root = mlx5dr_table_is_root(tbl);
	struct mlx5dr_matcher *matcher;
	int ret;

	matcher = simple_calloc(1, sizeof(*matcher));
	if (!matcher) {
		rte_errno = ENOMEM;
		return NULL;
	}

	matcher->tbl = tbl;
	matcher->attr = *attr;

	ret = mlx5dr_matcher_process_attr(tbl->ctx->caps, matcher, is_root);
	if (ret)
		goto free_matcher;

	ret = mlx5dr_matcher_set_templates(matcher, mt, num_of_mt, at, num_of_at);
	if (ret)
		goto free_matcher;

	if (is_root)
		ret = mlx5dr_matcher_init_root(matcher);
	else
		ret = mlx5dr_matcher_init(matcher);

	if (ret) {
		DR_LOG(ERR, "Failed to initialise matcher: %d", ret);
		goto unset_templates;
	}

	return matcher;

unset_templates:
	mlx5dr_matcher_unset_templates(matcher);
free_matcher:
	simple_free(matcher);
	return NULL;
}

int mlx5dr_matcher_destroy(struct mlx5dr_matcher *matcher)
{
	if (mlx5dr_table_is_root(matcher->tbl))
		mlx5dr_matcher_uninit_root(matcher);
	else
		mlx5dr_matcher_uninit(matcher);

	mlx5dr_matcher_unset_templates(matcher);
	simple_free(matcher);
	return 0;
}

struct mlx5dr_match_template *
mlx5dr_match_template_create(const struct rte_flow_item items[],
			     enum mlx5dr_match_template_flags flags)
{
	struct mlx5dr_match_template *mt;
	struct rte_flow_error error;
	int ret, len;

	if (flags > MLX5DR_MATCH_TEMPLATE_FLAG_RELAXED_MATCH) {
		DR_LOG(ERR, "Unsupported match template flag provided");
		rte_errno = EINVAL;
		return NULL;
	}

	mt = simple_calloc(1, sizeof(*mt));
	if (!mt) {
		DR_LOG(ERR, "Failed to allocate match template");
		rte_errno = ENOMEM;
		return NULL;
	}

	mt->flags = flags;

	/* Duplicate the user given items */
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, NULL, 0, items, &error);
	if (ret <= 0) {
		DR_LOG(ERR, "Unable to process items (%s): %s",
		       error.message ? error.message : "unspecified",
		       strerror(rte_errno));
		goto free_template;
	}

	len = RTE_ALIGN(ret, 16);
	mt->items = simple_calloc(1, len);
	if (!mt->items) {
		DR_LOG(ERR, "Failed to allocate item copy");
		rte_errno = ENOMEM;
		goto free_template;
	}

	ret = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, mt->items, ret, items, &error);
	if (ret <= 0)
		goto free_dst;

	return mt;

free_dst:
	simple_free(mt->items);
free_template:
	simple_free(mt);
	return NULL;
}

int mlx5dr_match_template_destroy(struct mlx5dr_match_template *mt)
{
	simple_free(mt->items);
	simple_free(mt);
	return 0;
}
