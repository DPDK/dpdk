/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_MATCHER_H_
#define MLX5DR_MATCHER_H_

/* Max supported match template */
#define MLX5DR_MATCHER_MAX_MT_ROOT 1

/* We calculated that concatenating a collision table to the main table with
 * 3% of the main table rows will be enough resources for high insertion
 * success probability.
 *
 * The calculation: log2(2^x * 3 / 100) = log2(2^x) + log2(3/100) = x - 5.05 ~ 5
 */
#define MLX5DR_MATCHER_ASSURED_ROW_RATIO 5
/* Thrashold to determine if amount of rules require a collision table */
#define MLX5DR_MATCHER_ASSURED_RULES_TH 10
/* Required depth of an assured collision table */
#define MLX5DR_MATCHER_ASSURED_COL_TBL_DEPTH 4
/* Required depth of the main large table */
#define MLX5DR_MATCHER_ASSURED_MAIN_TBL_DEPTH 2

enum mlx5dr_matcher_flags {
	MLX5DR_MATCHER_FLAGS_RANGE_DEFINER	= 1 << 0,
	MLX5DR_MATCHER_FLAGS_HASH_DEFINER	= 1 << 1,
	MLX5DR_MATCHER_FLAGS_COLLISION		= 1 << 2,
	MLX5DR_MATCHER_FLAGS_RESIZABLE		= 1 << 3,
	MLX5DR_MATCHER_FLAGS_COMPARE		= 1 << 4,
	MLX5DR_MATCHER_FLAGS_STE_ARRAY		= 1 << 5,
};

struct mlx5dr_match_template {
	struct rte_flow_item *items;
	struct mlx5dr_definer *definer;
	struct mlx5dr_definer *range_definer;
	struct mlx5dr_definer_fc *fc;
	struct mlx5dr_definer_fc *fcr;
	uint16_t fc_sz;
	uint16_t fcr_sz;
	uint64_t item_flags;
	uint8_t vport_item_id;
	enum mlx5dr_match_template_flags flags;
};

struct mlx5dr_matcher_match_ste {
	struct mlx5dr_pool_chunk ste;
	struct mlx5dr_devx_obj *rtc_0;
	struct mlx5dr_devx_obj *rtc_1;
	struct mlx5dr_pool *pool;
	/* Currently not support FDB aliased */
	struct mlx5dr_devx_obj *aliased_rtc_0;
};

struct mlx5dr_matcher_action_ste {
	struct mlx5dr_pool_chunk ste;
	struct mlx5dr_pool_chunk stc;
	struct mlx5dr_devx_obj *rtc_0;
	struct mlx5dr_devx_obj *rtc_1;
	struct mlx5dr_pool *pool;
	uint8_t max_stes;
};

struct mlx5dr_matcher_resize_data {
	struct mlx5dr_pool_chunk ste;
	struct mlx5dr_pool_chunk stc;
	struct mlx5dr_devx_obj *action_ste_rtc_0;
	struct mlx5dr_devx_obj *action_ste_rtc_1;
	struct mlx5dr_pool *action_ste_pool;
	uint8_t max_stes;
	LIST_ENTRY(mlx5dr_matcher_resize_data) next;
};

struct mlx5dr_matcher {
	struct mlx5dr_table *tbl;
	struct mlx5dr_matcher_attr attr;
	struct mlx5dv_flow_matcher *dv_matcher;
	struct mlx5dr_match_template *mt;
	uint8_t num_of_mt;
	struct mlx5dr_action_template *at;
	uint8_t num_of_at;
	/* enum mlx5dr_matcher_flags */
	uint8_t flags;
	struct mlx5dr_devx_obj *end_ft;
	struct mlx5dr_matcher *col_matcher;
	struct mlx5dr_matcher *resize_dst;
	struct mlx5dr_matcher_match_ste match_ste;
	struct mlx5dr_matcher_action_ste action_ste;
	struct mlx5dr_definer *hash_definer;
	LIST_ENTRY(mlx5dr_matcher) next;
	LIST_HEAD(resize_data_head, mlx5dr_matcher_resize_data) resize_data;
};

static inline bool
mlx5dr_matcher_mt_is_jumbo(struct mlx5dr_match_template *mt)
{
	return mlx5dr_definer_is_jumbo(mt->definer);
}

static inline bool
mlx5dr_matcher_mt_is_range(struct mlx5dr_match_template *mt)
{
	return (!!mt->range_definer);
}

static inline bool mlx5dr_matcher_is_resizable(struct mlx5dr_matcher *matcher)
{
	return !!(matcher->flags & MLX5DR_MATCHER_FLAGS_RESIZABLE);
}

static inline bool mlx5dr_matcher_is_in_resize(struct mlx5dr_matcher *matcher)
{
	return !!matcher->resize_dst;
}

static inline bool
mlx5dr_matcher_is_compare(struct mlx5dr_matcher *matcher)
{
	return !!(matcher->flags & MLX5DR_MATCHER_FLAGS_COMPARE);
}

static inline bool mlx5dr_matcher_req_fw_wqe(struct mlx5dr_matcher *matcher)
{
	/* Currently HWS doesn't support hash different from match or range */
	return unlikely(matcher->flags &
			(MLX5DR_MATCHER_FLAGS_HASH_DEFINER |
			 MLX5DR_MATCHER_FLAGS_RANGE_DEFINER |
			 MLX5DR_MATCHER_FLAGS_COMPARE));
}

int mlx5dr_matcher_conv_items_to_prm(uint64_t *match_buf,
				     struct rte_flow_item *items,
				     uint8_t *match_criteria,
				     bool is_value);

int mlx5dr_matcher_create_aliased_obj(struct mlx5dr_context *ctx,
				      struct ibv_context *ibv_owner,
				      struct ibv_context *ibv_allowed,
				      uint16_t vhca_id_to_be_accessed,
				      uint32_t aliased_object_id,
				      uint16_t object_type,
				      struct mlx5dr_devx_obj **obj);

static inline bool mlx5dr_matcher_is_insert_by_idx(struct mlx5dr_matcher *matcher)
{
	return matcher->attr.insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX;
}

static inline bool mlx5dr_matcher_is_always_hit(struct mlx5dr_matcher *matcher)
{
	return matcher->attr.match_mode == MLX5DR_MATCHER_MATCH_MODE_ALWAYS_HIT;
}

int mlx5dr_matcher_free_rtc_pointing(struct mlx5dr_context *ctx,
				     uint32_t fw_ft_type,
				     enum mlx5dr_table_type type,
				     struct mlx5dr_devx_obj *devx_obj);

int mlx5dr_matcher_validate_compare_attr(struct mlx5dr_matcher *matcher);

#endif /* MLX5DR_MATCHER_H_ */
