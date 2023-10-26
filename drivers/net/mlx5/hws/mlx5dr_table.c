/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

static void mlx5dr_table_init_next_ft_attr(struct mlx5dr_table *tbl,
					   struct mlx5dr_cmd_ft_create_attr *ft_attr)
{
	ft_attr->type = tbl->fw_ft_type;
	if (tbl->type == MLX5DR_TABLE_TYPE_FDB)
		ft_attr->level = tbl->ctx->caps->fdb_ft.max_level - 1;
	else
		ft_attr->level = tbl->ctx->caps->nic_ft.max_level - 1;
	ft_attr->rtc_valid = true;
}

/* Call this under ctx->ctrl_lock */
static int
mlx5dr_table_up_default_fdb_miss_tbl(struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_ft_create_attr ft_attr = {0};
	struct mlx5dr_cmd_set_fte_attr fte_attr = {0};
	struct mlx5dr_cmd_forward_tbl *default_miss;
	struct mlx5dr_cmd_set_fte_dest dest = {0};
	struct mlx5dr_context *ctx = tbl->ctx;
	uint8_t tbl_type = tbl->type;

	if (tbl->type != MLX5DR_TABLE_TYPE_FDB)
		return 0;

	if (ctx->common_res[tbl_type].default_miss) {
		ctx->common_res[tbl_type].default_miss->refcount++;
		return 0;
	}

	ft_attr.type = tbl->fw_ft_type;
	ft_attr.level = tbl->ctx->caps->fdb_ft.max_level; /* The last level */
	ft_attr.rtc_valid = false;

	dest.destination_type = MLX5_FLOW_DESTINATION_TYPE_VPORT;
	dest.destination_id = ctx->caps->eswitch_manager_vport_number;
	fte_attr.action_flags = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	fte_attr.dests_num = 1;
	fte_attr.dests = &dest;

	default_miss = mlx5dr_cmd_forward_tbl_create(mlx5dr_context_get_local_ibv(ctx),
						     &ft_attr, &fte_attr);
	if (!default_miss) {
		DR_LOG(ERR, "Failed to default miss table type: 0x%x", tbl_type);
		return rte_errno;
	}

	ctx->common_res[tbl_type].default_miss = default_miss;
	ctx->common_res[tbl_type].default_miss->refcount++;
	return 0;
}

/* Called under pthread_spin_lock(&ctx->ctrl_lock) */
static void mlx5dr_table_down_default_fdb_miss_tbl(struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_forward_tbl *default_miss;
	struct mlx5dr_context *ctx = tbl->ctx;
	uint8_t tbl_type = tbl->type;

	if (tbl->type != MLX5DR_TABLE_TYPE_FDB)
		return;

	default_miss = ctx->common_res[tbl_type].default_miss;
	if (--default_miss->refcount)
		return;

	mlx5dr_cmd_forward_tbl_destroy(default_miss);
	ctx->common_res[tbl_type].default_miss = NULL;
}

static int
mlx5dr_table_connect_to_default_miss_tbl(struct mlx5dr_table *tbl,
					 struct mlx5dr_devx_obj *ft)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};
	int ret;

	assert(tbl->type == MLX5DR_TABLE_TYPE_FDB);

	mlx5dr_cmd_set_attr_connect_miss_tbl(tbl->ctx,
					     tbl->fw_ft_type,
					     tbl->type,
					     &ft_attr);

	/* Connect to next */
	ret = mlx5dr_cmd_flow_table_modify(ft, &ft_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to connect FT to default FDB FT");
		return ret;
	}

	return 0;
}

struct mlx5dr_devx_obj *
mlx5dr_table_create_default_ft(struct ibv_context *ibv,
			       struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_ft_create_attr ft_attr = {0};
	struct mlx5dr_devx_obj *ft_obj;
	int ret;

	mlx5dr_table_init_next_ft_attr(tbl, &ft_attr);

	ft_obj = mlx5dr_cmd_flow_table_create(ibv, &ft_attr);
	if (ft_obj && tbl->type == MLX5DR_TABLE_TYPE_FDB) {
		/* Take/create ref over the default miss */
		ret = mlx5dr_table_up_default_fdb_miss_tbl(tbl);
		if (ret) {
			DR_LOG(ERR, "Failed to get default fdb miss");
			goto free_ft_obj;
		}
		ret = mlx5dr_table_connect_to_default_miss_tbl(tbl, ft_obj);
		if (ret) {
			DR_LOG(ERR, "Failed connecting to default miss tbl");
			goto down_miss_tbl;
		}
	}

	return ft_obj;

down_miss_tbl:
	mlx5dr_table_down_default_fdb_miss_tbl(tbl);
free_ft_obj:
	mlx5dr_cmd_destroy_obj(ft_obj);
	return NULL;
}

static int
mlx5dr_table_init_check_hws_support(struct mlx5dr_context *ctx,
				    struct mlx5dr_table *tbl)
{
	if (!(ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT)) {
		DR_LOG(ERR, "HWS not supported, cannot create mlx5dr_table");
		rte_errno = EOPNOTSUPP;
		return rte_errno;
	}

	if (mlx5dr_context_shared_gvmi_used(ctx) && tbl->type == MLX5DR_TABLE_TYPE_FDB) {
		DR_LOG(ERR, "FDB with shared port resources is not supported");
		rte_errno = EOPNOTSUPP;
		return rte_errno;
	}

	return 0;
}

static int
mlx5dr_table_shared_gvmi_resource_create(struct mlx5dr_context *ctx,
					 enum mlx5dr_table_type type,
					 struct mlx5dr_context_shared_gvmi_res *gvmi_res)
{
	struct mlx5dr_cmd_ft_create_attr ft_attr = {0};
	uint32_t calculated_ft_id;
	int ret;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return 0;

	ft_attr.type = mlx5dr_table_get_res_fw_ft_type(type, false);
	ft_attr.level = ctx->caps->nic_ft.max_level - 1;
	ft_attr.rtc_valid = true;

	gvmi_res->end_ft =
		mlx5dr_cmd_flow_table_create(mlx5dr_context_get_local_ibv(ctx),
					     &ft_attr);
	if (!gvmi_res->end_ft) {
		DR_LOG(ERR, "Failed to create end-ft");
		return rte_errno;
	}

	calculated_ft_id =
		mlx5dr_table_get_res_fw_ft_type(type, false) << FT_ID_FT_TYPE_OFFSET;
	calculated_ft_id |= gvmi_res->end_ft->id;

	/* create alias to that FT */
	ret = mlx5dr_matcher_create_aliased_obj(ctx,
						ctx->local_ibv_ctx,
						ctx->ibv_ctx,
						ctx->caps->vhca_id,
						calculated_ft_id,
						MLX5_GENERAL_OBJ_TYPE_FT_ALIAS,
						&gvmi_res->aliased_end_ft);
	if (ret) {
		DR_LOG(ERR, "Failed to create alias end-ft");
		goto free_end_ft;
	}

	return 0;

free_end_ft:
	mlx5dr_cmd_destroy_obj(gvmi_res->end_ft);

	return rte_errno;
}

static void
mlx5dr_table_shared_gvmi_resourse_destroy(struct mlx5dr_context *ctx,
					  struct mlx5dr_context_shared_gvmi_res *gvmi_res)
{
	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return;

	if (gvmi_res->aliased_end_ft) {
		mlx5dr_cmd_destroy_obj(gvmi_res->aliased_end_ft);
		gvmi_res->aliased_end_ft = NULL;
	}
	if (gvmi_res->end_ft) {
		mlx5dr_cmd_destroy_obj(gvmi_res->end_ft);
		gvmi_res->end_ft = NULL;
	}
}

/* called under spinlock ctx->ctrl_lock */
static struct mlx5dr_context_shared_gvmi_res *
mlx5dr_table_get_shared_gvmi_res(struct mlx5dr_context *ctx, enum mlx5dr_table_type type)
{
	int ret;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return NULL;

	if (ctx->gvmi_res[type].aliased_end_ft) {
		ctx->gvmi_res[type].refcount++;
		return &ctx->gvmi_res[type];
	}

	ret = mlx5dr_table_shared_gvmi_resource_create(ctx, type, &ctx->gvmi_res[type]);
	if (ret) {
		DR_LOG(ERR, "Failed to create shared gvmi res for type: %d", type);
		goto out;
	}

	ctx->gvmi_res[type].refcount = 1;

	return &ctx->gvmi_res[type];

out:
	return NULL;
}

/* called under spinlock ctx->ctrl_lock */
static void mlx5dr_table_put_shared_gvmi_res(struct mlx5dr_table *tbl)
{
	struct mlx5dr_context *ctx = tbl->ctx;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return;

	if (--ctx->gvmi_res[tbl->type].refcount)
		return;

	mlx5dr_table_shared_gvmi_resourse_destroy(ctx, &ctx->gvmi_res[tbl->type]);
}

static void mlx5dr_table_uninit_shared_ctx_res(struct mlx5dr_table *tbl)
{
	struct mlx5dr_context *ctx = tbl->ctx;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return;

	mlx5dr_cmd_destroy_obj(tbl->local_ft);

	mlx5dr_table_put_shared_gvmi_res(tbl);
}

/* called under spin_lock ctx->ctrl_lock */
static int mlx5dr_table_init_shared_ctx_res(struct mlx5dr_context *ctx, struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};
	int ret;

	if (!mlx5dr_context_shared_gvmi_used(ctx))
		return 0;

	/* create local-ft for root access */
	tbl->local_ft =
		mlx5dr_table_create_default_ft(mlx5dr_context_get_local_ibv(ctx), tbl);
	if (!tbl->local_ft) {
		DR_LOG(ERR, "Failed to create local-ft");
		return rte_errno;
	}

	if (!mlx5dr_table_get_shared_gvmi_res(tbl->ctx, tbl->type)) {
		DR_LOG(ERR, "Failed to shared gvmi resources");
		goto clean_local_ft;
	}

	/* On shared gvmi the default behavior is jump to alias end ft */
	mlx5dr_cmd_set_attr_connect_miss_tbl(tbl->ctx,
					     tbl->fw_ft_type,
					     tbl->type,
					     &ft_attr);

	ret = mlx5dr_cmd_flow_table_modify(tbl->ft, &ft_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to point table to its default miss");
		goto clean_shared_res;
	}

	return 0;

clean_shared_res:
	mlx5dr_table_put_shared_gvmi_res(tbl);
clean_local_ft:
	mlx5dr_table_destroy_default_ft(tbl, tbl->local_ft);
	return rte_errno;
}

void mlx5dr_table_destroy_default_ft(struct mlx5dr_table *tbl,
				     struct mlx5dr_devx_obj *ft_obj)
{
	mlx5dr_cmd_destroy_obj(ft_obj);
	mlx5dr_table_down_default_fdb_miss_tbl(tbl);
}

static int mlx5dr_table_init(struct mlx5dr_table *tbl)
{
	struct mlx5dr_context *ctx = tbl->ctx;
	int ret;

	if (mlx5dr_table_is_root(tbl))
		return 0;

	ret = mlx5dr_table_init_check_hws_support(ctx, tbl);
	if (ret)
		return ret;

	switch (tbl->type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
		tbl->fw_ft_type = FS_FT_NIC_RX;
		break;
	case MLX5DR_TABLE_TYPE_NIC_TX:
		tbl->fw_ft_type = FS_FT_NIC_TX;
		break;
	case MLX5DR_TABLE_TYPE_FDB:
		tbl->fw_ft_type = FS_FT_FDB;
		break;
	default:
		assert(0);
		break;
	}

	pthread_spin_lock(&ctx->ctrl_lock);
	tbl->ft = mlx5dr_table_create_default_ft(tbl->ctx->ibv_ctx, tbl);
	if (!tbl->ft) {
		DR_LOG(ERR, "Failed to create flow table devx object");
		pthread_spin_unlock(&ctx->ctrl_lock);
		return rte_errno;
	}

	ret = mlx5dr_table_init_shared_ctx_res(ctx, tbl);
	if (ret)
		goto tbl_destroy;

	ret = mlx5dr_action_get_default_stc(ctx, tbl->type);
	if (ret)
		goto free_shared_ctx;

	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

free_shared_ctx:
	mlx5dr_table_uninit_shared_ctx_res(tbl);
tbl_destroy:
	mlx5dr_table_destroy_default_ft(tbl, tbl->ft);
	pthread_spin_unlock(&ctx->ctrl_lock);
	return rte_errno;
}

static void mlx5dr_table_uninit(struct mlx5dr_table *tbl)
{
	if (mlx5dr_table_is_root(tbl))
		return;
	pthread_spin_lock(&tbl->ctx->ctrl_lock);
	mlx5dr_action_put_default_stc(tbl->ctx, tbl->type);
	mlx5dr_table_uninit_shared_ctx_res(tbl);
	mlx5dr_table_destroy_default_ft(tbl, tbl->ft);
	pthread_spin_unlock(&tbl->ctx->ctrl_lock);
}

struct mlx5dr_table *mlx5dr_table_create(struct mlx5dr_context *ctx,
					 struct mlx5dr_table_attr *attr)
{
	struct mlx5dr_table *tbl;
	int ret;

	if (attr->type > MLX5DR_TABLE_TYPE_FDB) {
		DR_LOG(ERR, "Invalid table type %d", attr->type);
		return NULL;
	}

	tbl = simple_calloc(1, sizeof(*tbl));
	if (!tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}

	tbl->ctx = ctx;
	tbl->type = attr->type;
	tbl->level = attr->level;

	ret = mlx5dr_table_init(tbl);
	if (ret) {
		DR_LOG(ERR, "Failed to initialise table");
		goto free_tbl;
	}

	pthread_spin_lock(&ctx->ctrl_lock);
	LIST_INSERT_HEAD(&ctx->head, tbl, next);
	pthread_spin_unlock(&ctx->ctrl_lock);

	return tbl;

free_tbl:
	simple_free(tbl);
	return NULL;
}

int mlx5dr_table_destroy(struct mlx5dr_table *tbl)
{
	struct mlx5dr_context *ctx = tbl->ctx;
	pthread_spin_lock(&ctx->ctrl_lock);
	if (!LIST_EMPTY(&tbl->head)) {
		DR_LOG(ERR, "Cannot destroy table containing matchers");
		rte_errno = EBUSY;
		goto unlock_err;
	}

	if (!LIST_EMPTY(&tbl->default_miss.head)) {
		DR_LOG(ERR, "Cannot destroy table pointed by default miss");
		rte_errno = EBUSY;
		goto unlock_err;
	}

	LIST_REMOVE(tbl, next);
	pthread_spin_unlock(&ctx->ctrl_lock);
	mlx5dr_table_uninit(tbl);
	simple_free(tbl);

	return 0;

unlock_err:
	pthread_spin_unlock(&ctx->ctrl_lock);
	return -rte_errno;
}

static struct mlx5dr_devx_obj *
mlx5dr_table_get_last_ft(struct mlx5dr_table *tbl)
{
	struct mlx5dr_devx_obj *last_ft = tbl->ft;
	struct mlx5dr_matcher *matcher;

	LIST_FOREACH(matcher, &tbl->head, next)
		last_ft = matcher->end_ft;

	return last_ft;
}

int mlx5dr_table_ft_set_default_next_ft(struct mlx5dr_table *tbl,
					struct mlx5dr_devx_obj *ft_obj)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};
	int ret;

	/* Due to FW limitation, resetting the flow table to default action will
	 * disconnect RTC when ignore_flow_level_rtc_valid is not supported.
	 */
	if (!tbl->ctx->caps->nic_ft.ignore_flow_level_rtc_valid)
		return 0;

	if (tbl->type == MLX5DR_TABLE_TYPE_FDB)
		return mlx5dr_table_connect_to_default_miss_tbl(tbl, ft_obj);

	ft_attr.type = tbl->fw_ft_type;
	ft_attr.modify_fs = MLX5_IFC_MODIFY_FLOW_TABLE_MISS_ACTION;
	ft_attr.table_miss_action = MLX5_IFC_MODIFY_FLOW_TABLE_MISS_ACTION_DEFAULT;

	ret = mlx5dr_cmd_flow_table_modify(ft_obj, &ft_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to set FT default miss action");
		return ret;
	}

	return 0;
}

int mlx5dr_table_ft_set_next_rtc(struct mlx5dr_devx_obj *ft,
				 uint32_t fw_ft_type,
				 struct mlx5dr_devx_obj *rtc_0,
				 struct mlx5dr_devx_obj *rtc_1)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};

	ft_attr.modify_fs = MLX5_IFC_MODIFY_FLOW_TABLE_RTC_ID;
	ft_attr.type = fw_ft_type;
	ft_attr.rtc_id_0 = rtc_0 ? rtc_0->id : 0;
	ft_attr.rtc_id_1 = rtc_1 ? rtc_1->id : 0;

	return mlx5dr_cmd_flow_table_modify(ft, &ft_attr);
}

static int mlx5dr_table_ft_set_next_ft(struct mlx5dr_devx_obj *ft,
				       uint32_t fw_ft_type,
				       uint32_t next_ft_id)
{
	struct mlx5dr_cmd_ft_modify_attr ft_attr = {0};

	ft_attr.modify_fs = MLX5_IFC_MODIFY_FLOW_TABLE_MISS_ACTION;
	ft_attr.table_miss_action = MLX5_IFC_MODIFY_FLOW_TABLE_MISS_ACTION_GOTO_TBL;
	ft_attr.type = fw_ft_type;
	ft_attr.table_miss_id = next_ft_id;

	return mlx5dr_cmd_flow_table_modify(ft, &ft_attr);
}

int mlx5dr_table_update_connected_miss_tables(struct mlx5dr_table *dst_tbl)
{
	struct mlx5dr_table *src_tbl;
	int ret;

	if (LIST_EMPTY(&dst_tbl->default_miss.head))
		return 0;

	LIST_FOREACH(src_tbl, &dst_tbl->default_miss.head, default_miss.next) {
		ret = mlx5dr_table_connect_to_miss_table(src_tbl, dst_tbl);
		if (ret) {
			DR_LOG(ERR, "Failed to update source miss table, unexpected behavior");
			return ret;
		}
	}

	return 0;
}

int mlx5dr_table_connect_to_miss_table(struct mlx5dr_table *src_tbl,
				       struct mlx5dr_table *dst_tbl)
{
	struct mlx5dr_devx_obj *last_ft;
	struct mlx5dr_matcher *matcher;
	int ret;

	last_ft = mlx5dr_table_get_last_ft(src_tbl);

	if (dst_tbl) {
		if (LIST_EMPTY(&dst_tbl->head)) {
			/* Connect src_tbl last_ft to dst_tbl start anchor */
			ret = mlx5dr_table_ft_set_next_ft(last_ft,
							  src_tbl->fw_ft_type,
							  dst_tbl->ft->id);
			if (ret)
				return ret;

			/* Reset last_ft RTC to default RTC */
			ret = mlx5dr_table_ft_set_next_rtc(last_ft,
							   src_tbl->fw_ft_type,
							   NULL, NULL);
			if (ret)
				return ret;
		} else {
			/* Connect src_tbl last_ft to first matcher RTC */
			matcher = LIST_FIRST(&dst_tbl->head);
			ret = mlx5dr_table_ft_set_next_rtc(last_ft,
							   src_tbl->fw_ft_type,
							   matcher->match_ste.rtc_0,
							   matcher->match_ste.rtc_1);
			if (ret)
				return ret;

			/* Reset next miss FT to default */
			ret = mlx5dr_table_ft_set_default_next_ft(src_tbl, last_ft);
			if (ret)
				return ret;
		}
	} else {
		/* Reset next miss FT to default */
		ret = mlx5dr_table_ft_set_default_next_ft(src_tbl, last_ft);
		if (ret)
			return ret;

		/* Reset last_ft RTC to default RTC */
		ret = mlx5dr_table_ft_set_next_rtc(last_ft,
						   src_tbl->fw_ft_type,
						   NULL, NULL);
		if (ret)
			return ret;
	}

	src_tbl->default_miss.miss_tbl = dst_tbl;

	return 0;
}

static int mlx5dr_table_set_default_miss_not_valid(struct mlx5dr_table *tbl,
						   struct mlx5dr_table *miss_tbl)
{
	if (!tbl->ctx->caps->nic_ft.ignore_flow_level_rtc_valid ||
	    mlx5dr_context_shared_gvmi_used(tbl->ctx)) {
		DR_LOG(ERR, "Default miss table is not supported");
		rte_errno = EOPNOTSUPP;
		return -rte_errno;
	}

	if (mlx5dr_table_is_root(tbl) ||
	    (miss_tbl && mlx5dr_table_is_root(miss_tbl)) ||
	    (miss_tbl && miss_tbl->type != tbl->type) ||
	    (miss_tbl && tbl->default_miss.miss_tbl)) {
		DR_LOG(ERR, "Invalid arguments");
		rte_errno = EINVAL;
		return -rte_errno;
	}

	return 0;
}

int mlx5dr_table_set_default_miss(struct mlx5dr_table *tbl,
				  struct mlx5dr_table *miss_tbl)
{
	struct mlx5dr_context *ctx = tbl->ctx;
	int ret;

	ret = mlx5dr_table_set_default_miss_not_valid(tbl, miss_tbl);
	if (ret)
		return ret;

	pthread_spin_lock(&ctx->ctrl_lock);

	ret = mlx5dr_table_connect_to_miss_table(tbl, miss_tbl);
	if (ret)
		goto out;

	if (miss_tbl)
		LIST_INSERT_HEAD(&miss_tbl->default_miss.head, tbl, default_miss.next);
	else
		LIST_REMOVE(tbl, default_miss.next);

	pthread_spin_unlock(&ctx->ctrl_lock);
	return 0;
out:
	pthread_spin_unlock(&ctx->ctrl_lock);
	return -ret;
}
