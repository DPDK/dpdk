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

enum mlx5dr_rule_move_state {
	MLX5DR_RULE_RESIZE_STATE_IDLE,
	MLX5DR_RULE_RESIZE_STATE_WRITING,
	MLX5DR_RULE_RESIZE_STATE_DELETING,
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

struct mlx5dr_rule_resize_info {
	struct mlx5dr_pool *action_ste_pool;
	uint32_t rtc_0;
	uint32_t rtc_1;
	uint32_t rule_idx;
	uint8_t state;
	uint8_t max_stes;
	uint8_t ctrl_seg[MLX5DR_WQE_SZ_GTA_CTRL]; /* Ctrl segment of STE: 48 bytes */
	uint8_t data_seg[MLX5DR_WQE_SZ_GTA_DATA]; /* Data segment of STE: 64 bytes */
};

struct mlx5dr_rule {
	struct mlx5dr_matcher *matcher;
	union {
		struct mlx5dr_rule_match_tag tag;
		/* Pointer to tag to store more than one tag */
		struct mlx5dr_rule_match_tag *tag_ptr;
		struct ibv_flow *flow;
		struct mlx5dr_rule_resize_info *resize_info;
	};
	uint32_t rtc_0; /* The RTC into which the STE was inserted */
	uint32_t rtc_1; /* The RTC into which the STE was inserted */
	int action_ste_idx; /* STE array index */
	uint8_t status; /* enum mlx5dr_rule_status */
	uint8_t pending_wqes;
};

void mlx5dr_rule_free_action_ste_idx(struct mlx5dr_rule *rule);

int mlx5dr_rule_move_hws_remove(struct mlx5dr_rule *rule,
				void *queue, void *user_data);

int mlx5dr_rule_move_hws_add(struct mlx5dr_rule *rule,
			     struct mlx5dr_rule_attr *attr);

bool mlx5dr_rule_move_in_progress(struct mlx5dr_rule *rule);

void mlx5dr_rule_clear_resize_info(struct mlx5dr_rule *rule);

int mlx5dr_rule_create_root_no_comp(struct mlx5dr_rule *rule,
				    const struct rte_flow_item items[],
				    uint8_t num_actions,
				    struct mlx5dr_rule_action rule_actions[]);

int mlx5dr_rule_destroy_root_no_comp(struct mlx5dr_rule *rule);

#endif /* MLX5DR_RULE_H_ */
