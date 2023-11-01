/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#ifndef MLX5DR_ACTION_H_
#define MLX5DR_ACTION_H_

/* Max number of STEs needed for a rule (including match) */
#define MLX5DR_ACTION_MAX_STE 20

/* Max number of internal subactions of ipv6_ext */
#define MLX5DR_ACTION_IPV6_EXT_MAX_SA 4

enum mlx5dr_action_stc_idx {
	MLX5DR_ACTION_STC_IDX_CTRL = 0,
	MLX5DR_ACTION_STC_IDX_HIT = 1,
	MLX5DR_ACTION_STC_IDX_DW5 = 2,
	MLX5DR_ACTION_STC_IDX_DW6 = 3,
	MLX5DR_ACTION_STC_IDX_DW7 = 4,
	MLX5DR_ACTION_STC_IDX_MAX = 5,
	/* STC Jumvo STE combo: CTR, Hit */
	MLX5DR_ACTION_STC_IDX_LAST_JUMBO_STE = 1,
	/* STC combo1: CTR, SINGLE, DOUBLE, Hit */
	MLX5DR_ACTION_STC_IDX_LAST_COMBO1 = 3,
	/* STC combo2: CTR, 3 x SINGLE, Hit */
	MLX5DR_ACTION_STC_IDX_LAST_COMBO2 = 4,
};

enum mlx5dr_action_offset {
	MLX5DR_ACTION_OFFSET_DW0 = 0,
	MLX5DR_ACTION_OFFSET_DW5 = 5,
	MLX5DR_ACTION_OFFSET_DW6 = 6,
	MLX5DR_ACTION_OFFSET_DW7 = 7,
	MLX5DR_ACTION_OFFSET_HIT = 3,
	MLX5DR_ACTION_OFFSET_HIT_LSB = 4,
};

enum {
	MLX5DR_ACTION_DOUBLE_SIZE = 8,
	MLX5DR_ACTION_INLINE_DATA_SIZE = 4,
	MLX5DR_ACTION_HDR_LEN_L2_MACS = 12,
	MLX5DR_ACTION_HDR_LEN_L2_VLAN = 4,
	MLX5DR_ACTION_HDR_LEN_L2_ETHER = 2,
	MLX5DR_ACTION_HDR_LEN_L2 = (MLX5DR_ACTION_HDR_LEN_L2_MACS +
				    MLX5DR_ACTION_HDR_LEN_L2_ETHER),
	MLX5DR_ACTION_HDR_LEN_L2_W_VLAN = (MLX5DR_ACTION_HDR_LEN_L2 +
					   MLX5DR_ACTION_HDR_LEN_L2_VLAN),
	MLX5DR_ACTION_REFORMAT_DATA_SIZE = 64,
	DECAP_L3_NUM_ACTIONS_W_NO_VLAN = 6,
	DECAP_L3_NUM_ACTIONS_W_VLAN = 7,
};

enum mlx5dr_action_setter_flag {
	ASF_SINGLE1 = 1 << 0,
	ASF_SINGLE2 = 1 << 1,
	ASF_SINGLE3 = 1 << 2,
	ASF_DOUBLE = ASF_SINGLE2 | ASF_SINGLE3,
	ASF_INSERT = 1 << 3,
	ASF_REMOVE = 1 << 4,
	ASF_MODIFY = 1 << 5,
	ASF_CTR = 1 << 6,
	ASF_HIT = 1 << 7,
};

enum mlx5dr_action_stc_reparse {
	MLX5DR_ACTION_STC_REPARSE_DEFAULT,
	MLX5DR_ACTION_STC_REPARSE_ON,
	MLX5DR_ACTION_STC_REPARSE_OFF,
};

struct mlx5dr_action_default_stc {
	struct mlx5dr_pool_chunk nop_ctr;
	struct mlx5dr_pool_chunk nop_dw5;
	struct mlx5dr_pool_chunk nop_dw6;
	struct mlx5dr_pool_chunk nop_dw7;
	struct mlx5dr_pool_chunk default_hit;
	uint32_t refcount;
};

struct mlx5dr_action_shared_stc {
	struct mlx5dr_pool_chunk remove_header;
	uint32_t refcount;
};

struct mlx5dr_actions_apply_data {
	struct mlx5dr_send_engine *queue;
	struct mlx5dr_rule_action *rule_action;
	uint32_t *wqe_data;
	struct mlx5dr_wqe_gta_ctrl_seg *wqe_ctrl;
	uint32_t jump_to_action_stc;
	struct mlx5dr_context_common_res *common_res;
	enum mlx5dr_table_type tbl_type;
	uint32_t next_direct_idx;
	uint8_t require_dep;
};

struct mlx5dr_actions_wqe_setter;

typedef void (*mlx5dr_action_setter_fp)
	(struct mlx5dr_actions_apply_data *apply,
	 struct mlx5dr_actions_wqe_setter *setter);

struct mlx5dr_actions_wqe_setter {
	mlx5dr_action_setter_fp set_single;
	mlx5dr_action_setter_fp set_double;
	mlx5dr_action_setter_fp set_hit;
	mlx5dr_action_setter_fp set_ctr;
	uint8_t idx_single;
	uint8_t idx_double;
	uint8_t idx_ctr;
	uint8_t idx_hit;
	uint8_t flags;
	uint8_t extra_data;
};

struct mlx5dr_action_template {
	struct mlx5dr_actions_wqe_setter setters[MLX5DR_ACTION_MAX_STE];
	enum mlx5dr_action_type *action_type_arr;
	uint8_t num_of_action_stes;
	uint8_t num_actions;
	uint8_t only_term;
};

struct mlx5dr_action {
	uint8_t type;
	uint8_t flags;
	struct mlx5dr_context *ctx;
	union {
		struct {
			struct mlx5dr_pool_chunk stc[MLX5DR_TABLE_TYPE_MAX];
			union {
				struct {
					struct mlx5dr_devx_obj *pat_obj;
					struct mlx5dr_devx_obj *arg_obj;
					__be64 single_action;
					uint8_t num_of_patterns;
					uint8_t single_action_type;
					uint8_t num_of_actions;
					uint8_t max_num_of_actions;
					uint8_t require_reparse;
				} modify_header;
				struct {
					struct mlx5dr_devx_obj *arg_obj;
					uint32_t header_size;
					uint16_t max_hdr_sz;
					uint8_t num_of_hdrs;
					uint8_t anchor;
					uint8_t offset;
					bool encap;
					uint8_t require_reparse;
				} reformat;
				struct {
					struct mlx5dr_action
						*action[MLX5DR_ACTION_IPV6_EXT_MAX_SA];
				} ipv6_route_ext;
				struct {
					struct mlx5dr_devx_obj *devx_obj;
					uint8_t return_reg_id;
				} aso;
				struct {
					uint16_t vport_num;
					uint16_t esw_owner_vhca_id;
				} vport;
				struct {
					struct mlx5dr_devx_obj *devx_obj;
				} alias;
				struct {
					struct mlx5dv_steering_anchor *sa;
				} root_tbl;
				struct {
					struct mlx5dr_devx_obj *devx_obj;
				} devx_dest;
				struct {
					struct mlx5dr_cmd_forward_tbl *fw_island;
					size_t num_dest;
					struct mlx5dr_cmd_set_fte_dest *dest_list;
				} dest_array;
				struct {
					uint8_t type;
					uint8_t start_anchor;
					uint8_t end_anchor;
					uint8_t num_of_words;
					bool decap;
				} remove_header;
			};
		};

		struct ibv_flow_action *flow_action;
		struct mlx5dv_devx_obj *devx_obj;
		struct ibv_qp *qp;
	};
};

int mlx5dr_action_root_build_attr(struct mlx5dr_rule_action rule_actions[],
				  uint32_t num_actions,
				  struct mlx5dv_flow_action_attr *attr);

int mlx5dr_action_get_default_stc(struct mlx5dr_context *ctx,
				  uint8_t tbl_type);

void mlx5dr_action_put_default_stc(struct mlx5dr_context *ctx,
				   uint8_t tbl_type);

void mlx5dr_action_prepare_decap_l3_data(uint8_t *src, uint8_t *dst,
					 uint16_t num_of_actions);

int mlx5dr_action_template_process(struct mlx5dr_action_template *at);

bool mlx5dr_action_check_combo(enum mlx5dr_action_type *user_actions,
			       enum mlx5dr_table_type table_type);

int mlx5dr_action_alloc_single_stc(struct mlx5dr_context *ctx,
				   struct mlx5dr_cmd_stc_modify_attr *stc_attr,
				   uint32_t table_type,
				   struct mlx5dr_pool_chunk *stc);

void mlx5dr_action_free_single_stc(struct mlx5dr_context *ctx,
				   uint32_t table_type,
				   struct mlx5dr_pool_chunk *stc);

static inline void
mlx5dr_action_setter_default_single(struct mlx5dr_actions_apply_data *apply,
				    __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW5] =
		htobe32(apply->common_res->default_stc->nop_dw5.offset);
}

static inline void
mlx5dr_action_setter_default_double(struct mlx5dr_actions_apply_data *apply,
				    __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] =
		htobe32(apply->common_res->default_stc->nop_dw6.offset);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] =
		htobe32(apply->common_res->default_stc->nop_dw7.offset);
}

static inline void
mlx5dr_action_setter_default_ctr(struct mlx5dr_actions_apply_data *apply,
				 __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW0] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_CTRL] =
		htobe32(apply->common_res->default_stc->nop_ctr.offset);
}

static inline void
mlx5dr_action_apply_setter(struct mlx5dr_actions_apply_data *apply,
			   struct mlx5dr_actions_wqe_setter *setter,
			   bool is_jumbo)
{
	uint8_t num_of_actions;

	/* Set control counter */
	if (setter->flags & ASF_CTR)
		setter->set_ctr(apply, setter);
	else
		mlx5dr_action_setter_default_ctr(apply, setter);

	/* Set single and double on match */
	if (!is_jumbo) {
		if (setter->flags & ASF_SINGLE1)
			setter->set_single(apply, setter);
		else
			mlx5dr_action_setter_default_single(apply, setter);

		if (setter->flags & ASF_DOUBLE)
			setter->set_double(apply, setter);
		else
			mlx5dr_action_setter_default_double(apply, setter);

		num_of_actions = setter->flags & ASF_DOUBLE ?
			MLX5DR_ACTION_STC_IDX_LAST_COMBO1 :
			MLX5DR_ACTION_STC_IDX_LAST_COMBO2;
	} else {
		apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
		apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
		apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = 0;
		apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW5] = 0;
		apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] = 0;
		apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;
		num_of_actions = MLX5DR_ACTION_STC_IDX_LAST_JUMBO_STE;
	}

	/* Set next/final hit action */
	setter->set_hit(apply, setter);

	/* Set number of actions */
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_CTRL] |=
		htobe32(num_of_actions << 29);
}

#endif /* MLX5DR_ACTION_H_ */
