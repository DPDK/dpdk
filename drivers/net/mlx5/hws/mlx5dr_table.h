/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_TABLE_H_
#define MLX5DR_TABLE_H_

#define MLX5DR_ROOT_LEVEL 0

struct mlx5dr_default_miss {
	/* My miss table */
	struct mlx5dr_table *miss_tbl;
	LIST_ENTRY(mlx5dr_table) next;
	/* Tables missing to my table */
	LIST_HEAD(miss_table_head, mlx5dr_table) head;
};

struct mlx5dr_table {
	struct mlx5dr_context *ctx;
	struct mlx5dr_devx_obj *ft;
	struct mlx5dr_devx_obj *local_ft;
	enum mlx5dr_table_type type;
	uint32_t fw_ft_type;
	uint32_t level;
	LIST_HEAD(matcher_head, mlx5dr_matcher) head;
	LIST_HEAD(isolated_matchers_head, mlx5dr_matcher) isolated_matchers;
	LIST_ENTRY(mlx5dr_table) next;
	struct mlx5dr_default_miss default_miss;
};

static inline bool mlx5dr_table_is_fw_fdb_any(uint32_t fw_tbl_type)
{
	if (fw_tbl_type == FS_FT_FDB_TX || fw_tbl_type == FS_FT_FDB_RX ||
	    fw_tbl_type == FS_FT_FDB_UNIFIED)
		return true;

	return false;
}

static inline bool mlx5dr_table_is_fdb_any(enum mlx5dr_table_type tbl_type)
{
	if (tbl_type == MLX5DR_TABLE_TYPE_FDB ||
	    tbl_type == MLX5DR_TABLE_TYPE_FDB_TX ||
	    tbl_type == MLX5DR_TABLE_TYPE_FDB_RX ||
	    tbl_type == MLX5DR_TABLE_TYPE_FDB_UNIFIED)
		return true;

	return false;
}

static inline bool mlx5dr_table_fdb_no_unified(enum mlx5dr_table_type tbl_type)
{
	if (mlx5dr_table_is_fdb_any(tbl_type) &&
	    tbl_type != MLX5DR_TABLE_TYPE_FDB_UNIFIED)
		return true;

	return false;
}

static inline
uint32_t mlx5dr_table_get_res_fw_ft_type(enum mlx5dr_table_type tbl_type,
					 bool is_mirror)
{
	switch (tbl_type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
		return FS_FT_NIC_RX;
	case MLX5DR_TABLE_TYPE_NIC_TX:
		return FS_FT_NIC_TX;
	case MLX5DR_TABLE_TYPE_FDB:
	case MLX5DR_TABLE_TYPE_FDB_RX:
	case MLX5DR_TABLE_TYPE_FDB_TX:
		return is_mirror ? FS_FT_FDB_TX : FS_FT_FDB_RX;
	case MLX5DR_TABLE_TYPE_FDB_UNIFIED:
		return FS_FT_FDB_UNIFIED;
	default:
		assert(0);
		return 0;
	}
}

static inline bool mlx5dr_table_is_root(struct mlx5dr_table *tbl)
{
	return (tbl->level == MLX5DR_ROOT_LEVEL);
}

static inline
void mlx5dr_table_get_fw_ft_type(enum mlx5dr_table_type type,
				 uint8_t *ret_type)
{
	switch (type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
		*ret_type = FS_FT_NIC_RX;
		break;
	case MLX5DR_TABLE_TYPE_NIC_TX:
		*ret_type = FS_FT_NIC_TX;
		break;
	case MLX5DR_TABLE_TYPE_FDB:
	case MLX5DR_TABLE_TYPE_FDB_RX:
	case MLX5DR_TABLE_TYPE_FDB_TX:
	case MLX5DR_TABLE_TYPE_FDB_UNIFIED:
		*ret_type = FS_FT_FDB;
		break;
	default:
		assert(0);
	}
}

struct mlx5dr_devx_obj *mlx5dr_table_create_default_ft(struct ibv_context *ibv,
						       struct mlx5dr_table *tbl);

void mlx5dr_table_destroy_default_ft(struct mlx5dr_table *tbl,
				     struct mlx5dr_devx_obj *ft_obj);

int mlx5dr_table_connect_to_miss_table(struct mlx5dr_table *src_tbl,
				       struct mlx5dr_table *dst_tbl,
				       bool only_update_last_ft);

int mlx5dr_table_update_connected_miss_tables(struct mlx5dr_table *dst_tbl);

int mlx5dr_table_ft_set_default_next_ft(struct mlx5dr_table *tbl,
					struct mlx5dr_devx_obj *ft_obj);

int mlx5dr_table_ft_set_next_rtc(struct mlx5dr_devx_obj *ft,
				 uint32_t fw_ft_type,
				 struct mlx5dr_devx_obj *rtc_0,
				 struct mlx5dr_devx_obj *rtc_1);

int mlx5dr_table_connect_src_ft_to_miss_table(struct mlx5dr_table *src_tbl,
					      struct mlx5dr_devx_obj *ft,
					      struct mlx5dr_table *dst_tbl);

#endif /* MLX5DR_TABLE_H_ */
