/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_RULE_H_
#define MLX5DR_RULE_H_

enum {
	MLX5DR_STE_CTRL_SZ = 20,
	MLX5DR_ACTIONS_SZ = 12,
	MLX5DR_MATCH_TAG_SZ = 32,
	MLX5DR_JUMBO_TAG_SZ = 44,
	MLX5DR_STE_SZ = 64,
};

enum mlx5dr_rule_status {
	MLX5DR_RULE_STATUS_UNKNOWN,
	MLX5DR_RULE_STATUS_CREATING,
	MLX5DR_RULE_STATUS_CREATED,
	MLX5DR_RULE_STATUS_DELETING,
	MLX5DR_RULE_STATUS_DELETED,
	MLX5DR_RULE_STATUS_FAILING,
	MLX5DR_RULE_STATUS_FAILED,
};

struct mlx5dr_rule_match_tag {
	union {
		uint8_t jumbo[MLX5DR_JUMBO_TAG_SZ];
		struct {
			uint8_t reserved[MLX5DR_ACTIONS_SZ];
			uint8_t match[MLX5DR_MATCH_TAG_SZ];
		};
	};
};

struct mlx5dr_rule {
	struct mlx5dr_matcher *matcher;
	union {
		struct mlx5dr_rule_match_tag tag;
		/* Pointer to tag to store more than one tag */
		struct mlx5dr_rule_match_tag *tag_ptr;
		struct ibv_flow *flow;
	};
	uint32_t rtc_0; /* The RTC into which the STE was inserted */
	uint32_t rtc_1; /* The RTC into which the STE was inserted */
	int action_ste_idx; /* STE array index */
	uint8_t status; /* enum mlx5dr_rule_status */
	uint8_t pending_wqes;
};

void mlx5dr_rule_free_action_ste_idx(struct mlx5dr_rule *rule);

#endif /* MLX5DR_RULE_H_ */
