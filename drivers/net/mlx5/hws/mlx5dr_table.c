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
	struct mlx5dr_cmd_forward_tbl *default_miss;
	struct mlx5dr_context *ctx = tbl->ctx;
	uint8_t tbl_type = tbl->type;
	uint32_t vport;

	if (tbl->type != MLX5DR_TABLE_TYPE_FDB)
		return 0;

	if (ctx->common_res[tbl_type].default_miss) {
		ctx->common_res[tbl_type].default_miss->refcount++;
		return 0;
	}

	ft_attr.type = tbl->fw_ft_type;
	ft_attr.level = tbl->ctx->caps->fdb_ft.max_level; /* The last level */
	ft_attr.rtc_valid = false;

	assert(ctx->caps->eswitch_manager);
	vport = ctx->caps->eswitch_manager_vport_number;

	default_miss = mlx5dr_cmd_miss_ft_create(ctx->ibv_ctx, &ft_attr, vport);
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

	mlx5dr_cmd_miss_ft_destroy(default_miss);

	simple_free(default_miss);
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
		return errno;
	}

	return 0;
}

struct mlx5dr_devx_obj *
mlx5dr_table_create_default_ft(struct mlx5dr_table *tbl)
{
	struct mlx5dr_cmd_ft_create_attr ft_attr = {0};
	struct mlx5dr_devx_obj *ft_obj;
	int ret;

	mlx5dr_table_init_next_ft_attr(tbl, &ft_attr);

	ft_obj = mlx5dr_cmd_flow_table_create(tbl->ctx->ibv_ctx, &ft_attr);
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

	if (!(tbl->ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT)) {
		DR_LOG(ERR, "HWS not supported, cannot create mlx5dr_table");
		rte_errno = EOPNOTSUPP;
		return rte_errno;
	}

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
	tbl->ft = mlx5dr_table_create_default_ft(tbl);
	if (!tbl->ft) {
		DR_LOG(ERR, "Failed to create flow table devx object");
		pthread_spin_unlock(&ctx->ctrl_lock);
		return rte_errno;
	}

	ret = mlx5dr_action_get_default_stc(ctx, tbl->type);
	if (ret)
		goto tbl_destroy;
	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

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

	tbl = simple_malloc(sizeof(*tbl));
	if (!tbl) {
		rte_errno = ENOMEM;
		return NULL;
	}

	tbl->ctx = ctx;
	tbl->type = attr->type;
	tbl->level = attr->level;
	LIST_INIT(&tbl->head);

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
	LIST_REMOVE(tbl, next);
	pthread_spin_unlock(&ctx->ctrl_lock);
	mlx5dr_table_uninit(tbl);
	simple_free(tbl);

	return 0;
}
