/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_BWC_H_
#define MLX5DR_BWC_H_

#define MLX5DR_BWC_MATCHER_INIT_SIZE_LOG 1
#define MLX5DR_BWC_MATCHER_SIZE_LOG_STEP 1
#define MLX5DR_BWC_MATCHER_REHASH_PERCENT_TH 70
#define MLX5DR_BWC_MATCHER_REHASH_BURST_TH 32
#define MLX5DR_BWC_MATCHER_ATTACH_AT_NUM 255

struct mlx5dr_bwc_matcher {
	struct mlx5dr_matcher *matcher;
	struct mlx5dr_match_template *mt;
	struct mlx5dr_action_template *at[MLX5DR_BWC_MATCHER_ATTACH_AT_NUM];
	uint8_t num_of_at;
	uint32_t priority;
	uint8_t size_log;
	RTE_ATOMIC(uint32_t)num_of_rules; /* atomically accessed */
	LIST_HEAD(rule_head, mlx5dr_bwc_rule) * rules;
};

struct mlx5dr_bwc_rule {
	struct mlx5dr_bwc_matcher *bwc_matcher;
	struct mlx5dr_rule *rule;
	struct rte_flow_item *flow_items;
	uint16_t bwc_queue_idx;
	LIST_ENTRY(mlx5dr_bwc_rule) next;
};

#endif /* MLX5DR_BWC_H_ */
