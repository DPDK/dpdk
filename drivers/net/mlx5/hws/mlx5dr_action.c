/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

#define WIRE_PORT 0xFFFF

#define MLX5DR_ACTION_METER_INIT_COLOR_OFFSET 1

/* This is the maximum allowed action order for each table type:
 *	 TX: POP_VLAN, CTR, ASO_METER, AS_CT, PUSH_VLAN, MODIFY, ENCAP, Term
 *	 RX: TAG, DECAP, POP_VLAN, CTR, ASO_METER, ASO_CT, PUSH_VLAN, MODIFY,
 *	     ENCAP, Term
 *	FDB: DECAP, POP_VLAN, CTR, ASO_METER, ASO_CT, PUSH_VLAN, MODIFY,
 *	     ENCAP, Term
 */
static const uint32_t action_order_arr[MLX5DR_TABLE_TYPE_MAX][MLX5DR_ACTION_TYP_MAX] = {
	[MLX5DR_TABLE_TYPE_NIC_RX] = {
		BIT(MLX5DR_ACTION_TYP_TAG),
		BIT(MLX5DR_ACTION_TYP_TNL_L2_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_TNL_L3_TO_L2),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_CTR),
		BIT(MLX5DR_ACTION_TYP_ASO_METER),
		BIT(MLX5DR_ACTION_TYP_ASO_CT),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_FT) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_TIR) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT),
		BIT(MLX5DR_ACTION_TYP_LAST),
	},
	[MLX5DR_TABLE_TYPE_NIC_TX] = {
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_CTR),
		BIT(MLX5DR_ACTION_TYP_ASO_METER),
		BIT(MLX5DR_ACTION_TYP_ASO_CT),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_FT) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT),
		BIT(MLX5DR_ACTION_TYP_LAST),
	},
	[MLX5DR_TABLE_TYPE_FDB] = {
		BIT(MLX5DR_ACTION_TYP_TNL_L2_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_TNL_L3_TO_L2),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_CTR),
		BIT(MLX5DR_ACTION_TYP_ASO_METER),
		BIT(MLX5DR_ACTION_TYP_ASO_CT),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_FT) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_VPORT) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT),
		BIT(MLX5DR_ACTION_TYP_LAST),
	},
};

static int mlx5dr_action_get_shared_stc_nic(struct mlx5dr_context *ctx,
					    enum mlx5dr_context_shared_stc_type stc_type,
					    uint8_t tbl_type)
{
	struct mlx5dr_cmd_stc_modify_attr stc_attr = {0};
	struct mlx5dr_action_shared_stc *shared_stc;
	int ret;

	pthread_spin_lock(&ctx->ctrl_lock);
	if (ctx->common_res[tbl_type].shared_stc[stc_type]) {
		ctx->common_res[tbl_type].shared_stc[stc_type]->refcount++;
		pthread_spin_unlock(&ctx->ctrl_lock);
		return 0;
	}

	shared_stc = simple_calloc(1, sizeof(*shared_stc));
	if (!shared_stc) {
		DR_LOG(ERR, "Failed to allocate memory for shared STCs");
		rte_errno = ENOMEM;
		goto unlock_and_out;
	}
	switch (stc_type) {
	case MLX5DR_CONTEXT_SHARED_STC_DECAP:
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_REMOVE;
		stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW5;
		stc_attr.remove_header.decap = 0;
		stc_attr.remove_header.start_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		stc_attr.remove_header.end_anchor = MLX5_HEADER_ANCHOR_IPV6_IPV4;
		break;
	case MLX5DR_CONTEXT_SHARED_STC_POP:
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_REMOVE_WORDS;
		stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW5;
		stc_attr.remove_words.start_anchor = MLX5_HEADER_ANCHOR_FIRST_VLAN_START;
		stc_attr.remove_words.num_of_words = MLX5DR_ACTION_HDR_LEN_L2_VLAN;
		break;
	default:
		DR_LOG(ERR, "No such type : stc_type\n");
		assert(false);
		rte_errno = EINVAL;
		goto unlock_and_out;
	}

	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &shared_stc->remove_header);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate shared decap l2 STC");
		goto free_shared_stc;
	}

	ctx->common_res[tbl_type].shared_stc[stc_type] = shared_stc;
	ctx->common_res[tbl_type].shared_stc[stc_type]->refcount = 1;

	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

free_shared_stc:
	simple_free(shared_stc);
unlock_and_out:
	pthread_spin_unlock(&ctx->ctrl_lock);
	return rte_errno;
}

static void mlx5dr_action_put_shared_stc_nic(struct mlx5dr_context *ctx,
					     enum mlx5dr_context_shared_stc_type stc_type,
					     uint8_t tbl_type)
{
	struct mlx5dr_action_shared_stc *shared_stc;

	pthread_spin_lock(&ctx->ctrl_lock);
	if (--ctx->common_res[tbl_type].shared_stc[stc_type]->refcount) {
		pthread_spin_unlock(&ctx->ctrl_lock);
		return;
	}

	shared_stc = ctx->common_res[tbl_type].shared_stc[stc_type];

	mlx5dr_action_free_single_stc(ctx, tbl_type, &shared_stc->remove_header);
	simple_free(shared_stc);
	ctx->common_res[tbl_type].shared_stc[stc_type] = NULL;
	pthread_spin_unlock(&ctx->ctrl_lock);
}

static int mlx5dr_action_get_shared_stc(struct mlx5dr_action *action,
					enum mlx5dr_context_shared_stc_type stc_type)
{
	struct mlx5dr_context *ctx = action->ctx;
	int ret;

	if (stc_type >= MLX5DR_CONTEXT_SHARED_STC_MAX) {
		assert(false);
		rte_errno = EINVAL;
		return rte_errno;
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_RX);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate memory for RX shared STCs (type: %d)",
			       stc_type);
			return ret;
		}
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_TX);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate memory for TX shared STCs(type: %d)",
			       stc_type);
			goto clean_nic_rx_stc;
		}
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate memory for FDB shared STCs (type: %d)",
			       stc_type);
			goto clean_nic_tx_stc;
		}
	}

	return 0;

clean_nic_tx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_TX);
clean_nic_rx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_RX);

	return ret;
}

static void mlx5dr_action_put_shared_stc(struct mlx5dr_action *action,
					 enum mlx5dr_context_shared_stc_type stc_type)
{
	struct mlx5dr_context *ctx = action->ctx;

	if (stc_type >= MLX5DR_CONTEXT_SHARED_STC_MAX) {
		assert(false);
		return;
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_RX);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_NIC_TX);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB);
}

static void mlx5dr_action_print_combo(enum mlx5dr_action_type *user_actions)
{
	DR_LOG(ERR, "Invalid action_type sequence");
	while (*user_actions != MLX5DR_ACTION_TYP_LAST) {
		DR_LOG(ERR, "%s", mlx5dr_debug_action_type_to_str(*user_actions));
		user_actions++;
	}
}

bool mlx5dr_action_check_combo(enum mlx5dr_action_type *user_actions,
			       enum mlx5dr_table_type table_type)
{
	const uint32_t *order_arr = action_order_arr[table_type];
	uint8_t order_idx = 0;
	uint8_t user_idx = 0;
	bool valid_combo;

	while (order_arr[order_idx] != BIT(MLX5DR_ACTION_TYP_LAST)) {
		/* User action order validated move to next user action */
		if (BIT(user_actions[user_idx]) & order_arr[order_idx])
			user_idx++;

		/* Iterate to the next supported action in the order */
		order_idx++;
	}

	/* Combination is valid if all user action were processed */
	valid_combo = user_actions[user_idx] == MLX5DR_ACTION_TYP_LAST;
	if (!valid_combo)
		mlx5dr_action_print_combo(user_actions);

	return valid_combo;
}

int mlx5dr_action_root_build_attr(struct mlx5dr_rule_action rule_actions[],
				  uint32_t num_actions,
				  struct mlx5dv_flow_action_attr *attr)
{
	struct mlx5dr_action *action;
	uint32_t i;

	for (i = 0; i < num_actions; i++) {
		action = rule_actions[i].action;

		switch (action->type) {
		case MLX5DR_ACTION_TYP_FT:
		case MLX5DR_ACTION_TYP_TIR:
			attr[i].type = MLX5DV_FLOW_ACTION_DEST_DEVX;
			attr[i].obj = action->devx_obj;
			break;
		case MLX5DR_ACTION_TYP_TAG:
			attr[i].type = MLX5DV_FLOW_ACTION_TAG;
			attr[i].tag_value = rule_actions[i].tag.value;
			break;
#ifdef HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS
		case MLX5DR_ACTION_TYP_MISS:
			attr[i].type = MLX5DV_FLOW_ACTION_DEFAULT_MISS;
			break;
#endif
		case MLX5DR_ACTION_TYP_DROP:
			attr[i].type = MLX5DV_FLOW_ACTION_DROP;
			break;
		case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
		case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
		case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
		case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
		case MLX5DR_ACTION_TYP_MODIFY_HDR:
			attr[i].type = MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
			attr[i].action = action->flow_action;
			break;
#ifdef HAVE_IBV_FLOW_DEVX_COUNTERS
		case MLX5DR_ACTION_TYP_CTR:
			attr[i].type = MLX5DV_FLOW_ACTION_COUNTERS_DEVX;
			attr[i].obj = action->devx_obj;

			if (rule_actions[i].counter.offset) {
				DR_LOG(ERR, "Counter offset not supported over root");
				rte_errno = ENOTSUP;
				return rte_errno;
			}
			break;
#endif
		default:
			DR_LOG(ERR, "Found unsupported action type: %d", action->type);
			rte_errno = ENOTSUP;
			return rte_errno;
		}
	}

	return 0;
}

static bool mlx5dr_action_fixup_stc_attr(struct mlx5dr_cmd_stc_modify_attr *stc_attr,
					 struct mlx5dr_cmd_stc_modify_attr *fixup_stc_attr,
					 enum mlx5dr_table_type table_type,
					 bool is_mirror)
{
	struct mlx5dr_devx_obj *devx_obj;
	bool use_fixup = false;
	uint32_t fw_tbl_type;

	fw_tbl_type = mlx5dr_table_get_res_fw_ft_type(table_type, is_mirror);

	switch (stc_attr->action_type) {
	case MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_STE_TABLE:
		if (!is_mirror)
			devx_obj = mlx5dr_pool_chunk_get_base_devx_obj(stc_attr->ste_table.ste_pool,
								       &stc_attr->ste_table.ste);
		else
			devx_obj =
			mlx5dr_pool_chunk_get_base_devx_obj_mirror(stc_attr->ste_table.ste_pool,
								   &stc_attr->ste_table.ste);

		*fixup_stc_attr = *stc_attr;
		fixup_stc_attr->ste_table.ste_obj_id = devx_obj->id;
		use_fixup = true;
		break;

	case MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_VPORT:
		if (stc_attr->vport.vport_num != WIRE_PORT)
			break;

		if (fw_tbl_type == FS_FT_FDB_RX) {
			/* The FW doesn't allow to go back to wire in RX, so change it to DROP */
			fixup_stc_attr->action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
			fixup_stc_attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
			fixup_stc_attr->stc_offset = stc_attr->stc_offset;
		} else if (fw_tbl_type == FS_FT_FDB_TX) {
			/*The FW doesn't allow to go to wire in the TX by JUMP_TO_VPORT*/
			fixup_stc_attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_UPLINK;
			fixup_stc_attr->action_offset = stc_attr->action_offset;
			fixup_stc_attr->stc_offset = stc_attr->stc_offset;
			fixup_stc_attr->vport.vport_num = 0;
			fixup_stc_attr->vport.esw_owner_vhca_id = stc_attr->vport.esw_owner_vhca_id;
		}
		use_fixup = true;
		break;

	default:
		break;
	}

	return use_fixup;
}

int mlx5dr_action_alloc_single_stc(struct mlx5dr_context *ctx,
				   struct mlx5dr_cmd_stc_modify_attr *stc_attr,
				   uint32_t table_type,
				   struct mlx5dr_pool_chunk *stc)
{
	struct mlx5dr_cmd_stc_modify_attr cleanup_stc_attr = {0};
	struct mlx5dr_pool *stc_pool = ctx->stc_pool[table_type];
	struct mlx5dr_cmd_stc_modify_attr fixup_stc_attr = {0};
	struct mlx5dr_devx_obj *devx_obj_0;
	bool use_fixup;
	int ret;

	ret = mlx5dr_pool_chunk_alloc(stc_pool, stc);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate single action STC");
		return ret;
	}

	stc_attr->stc_offset = stc->offset;
	devx_obj_0 = mlx5dr_pool_chunk_get_base_devx_obj(stc_pool, stc);

	/* According to table/action limitation change the stc_attr */
	use_fixup = mlx5dr_action_fixup_stc_attr(stc_attr, &fixup_stc_attr, table_type, false);
	ret = mlx5dr_cmd_stc_modify(devx_obj_0, use_fixup ? &fixup_stc_attr : stc_attr);
	if (ret) {
		DR_LOG(ERR, "Failed to modify STC action_type %d tbl_type %d",
		       stc_attr->action_type, table_type);
		goto free_chunk;
	}

	/* Modify the FDB peer */
	if (table_type == MLX5DR_TABLE_TYPE_FDB) {
		struct mlx5dr_devx_obj *devx_obj_1;

		devx_obj_1 = mlx5dr_pool_chunk_get_base_devx_obj_mirror(stc_pool, stc);

		use_fixup = mlx5dr_action_fixup_stc_attr(stc_attr, &fixup_stc_attr,
							 table_type, true);
		ret = mlx5dr_cmd_stc_modify(devx_obj_1, use_fixup ? &fixup_stc_attr : stc_attr);
		if (ret) {
			DR_LOG(ERR, "Failed to modify peer STC action_type %d tbl_type %d",
			       stc_attr->action_type, table_type);
			goto clean_devx_obj_0;
		}
	}

	return 0;

clean_devx_obj_0:
	cleanup_stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
	cleanup_stc_attr.action_offset = MLX5DR_ACTION_OFFSET_HIT;
	cleanup_stc_attr.stc_offset = stc->offset;
	mlx5dr_cmd_stc_modify(devx_obj_0, &cleanup_stc_attr);
free_chunk:
	mlx5dr_pool_chunk_free(stc_pool, stc);
	return rte_errno;
}

void mlx5dr_action_free_single_stc(struct mlx5dr_context *ctx,
				   uint32_t table_type,
				   struct mlx5dr_pool_chunk *stc)
{
	struct mlx5dr_pool *stc_pool = ctx->stc_pool[table_type];
	struct mlx5dr_cmd_stc_modify_attr stc_attr = {0};
	struct mlx5dr_devx_obj *devx_obj;

	/* Modify the STC not to point to an object */
	stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_HIT;
	stc_attr.stc_offset = stc->offset;
	devx_obj = mlx5dr_pool_chunk_get_base_devx_obj(stc_pool, stc);
	mlx5dr_cmd_stc_modify(devx_obj, &stc_attr);

	if (table_type == MLX5DR_TABLE_TYPE_FDB) {
		devx_obj = mlx5dr_pool_chunk_get_base_devx_obj_mirror(stc_pool, stc);
		mlx5dr_cmd_stc_modify(devx_obj, &stc_attr);
	}

	mlx5dr_pool_chunk_free(stc_pool, stc);
}

static uint32_t mlx5dr_action_get_mh_stc_type(__be64 pattern)
{
	uint8_t action_type = MLX5_GET(set_action_in, &pattern, action_type);

	switch (action_type) {
	case MLX5_MODIFICATION_TYPE_SET:
		return MLX5_IFC_STC_ACTION_TYPE_SET;
	case MLX5_MODIFICATION_TYPE_ADD:
		return MLX5_IFC_STC_ACTION_TYPE_ADD;
	case MLX5_MODIFICATION_TYPE_COPY:
		return MLX5_IFC_STC_ACTION_TYPE_COPY;
	default:
		assert(false);
		DR_LOG(ERR, "Unsupported action type: 0x%x\n", action_type);
		rte_errno = ENOTSUP;
		return MLX5_IFC_STC_ACTION_TYPE_NOP;
	}
}

static void mlx5dr_action_fill_stc_attr(struct mlx5dr_action *action,
					struct mlx5dr_devx_obj *obj,
					struct mlx5dr_cmd_stc_modify_attr *attr)
{
	switch (action->type) {
	case MLX5DR_ACTION_TYP_TAG:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_TAG;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		break;
	case MLX5DR_ACTION_TYP_DROP:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		break;
	case MLX5DR_ACTION_TYP_MISS:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_ALLOW;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		/* TODO Need to support default miss for FDB */
		break;
	case MLX5DR_ACTION_TYP_CTR:
		attr->id = obj->id;
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_COUNTER;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW0;
		break;
	case MLX5DR_ACTION_TYP_TIR:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_TIR;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_tir_num = obj->id;
		break;
	case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_MODIFY_HDR:
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		if (action->modify_header.num_of_actions == 1) {
			attr->modify_action.data = action->modify_header.single_action;
			attr->action_type = mlx5dr_action_get_mh_stc_type(attr->modify_action.data);

			if (attr->action_type == MLX5_IFC_STC_ACTION_TYPE_ADD ||
			    attr->action_type == MLX5_IFC_STC_ACTION_TYPE_SET)
				MLX5_SET(set_action_in, &attr->modify_action.data, data, 0);
		} else {
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_ACC_MODIFY_LIST;
			attr->modify_header.arg_id = action->modify_header.arg_obj->id;
			attr->modify_header.pattern_id = action->modify_header.pattern_obj->id;
		}
		break;
	case MLX5DR_ACTION_TYP_FT:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_table_id = obj->id;
		break;
	case MLX5DR_ACTION_TYP_DEST_ROOT:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_table_id = action->root_tbl.sa->id;
		break;
	case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_REMOVE;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		attr->remove_header.decap = 1;
		attr->remove_header.start_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->remove_header.end_anchor = MLX5_HEADER_ANCHOR_INNER_MAC;
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_INSERT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->insert_header.encap = 1;
		attr->insert_header.insert_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->insert_header.arg_id = action->reformat.arg_obj->id;
		attr->insert_header.header_size = action->reformat.header_size;
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_INSERT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->insert_header.encap = 1;
		attr->insert_header.insert_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->insert_header.arg_id = action->reformat.arg_obj->id;
		attr->insert_header.header_size = action->reformat.header_size;
		break;
	case MLX5DR_ACTION_TYP_ASO_METER:
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_ASO;
		attr->aso.aso_type = ASO_OPC_MOD_POLICER;
		attr->aso.devx_obj_id = obj->id;
		attr->aso.return_reg_id = action->aso.return_reg_id;
		break;
	case MLX5DR_ACTION_TYP_ASO_CT:
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_ASO;
		attr->aso.aso_type = ASO_OPC_MOD_CONNECTION_TRACKING;
		attr->aso.devx_obj_id = obj->id;
		attr->aso.return_reg_id = action->aso.return_reg_id;
		break;
	case MLX5DR_ACTION_TYP_VPORT:
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_VPORT;
		attr->vport.vport_num = action->vport.vport_num;
		attr->vport.esw_owner_vhca_id =	action->vport.esw_owner_vhca_id;
		break;
	case MLX5DR_ACTION_TYP_POP_VLAN:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_REMOVE_WORDS;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		attr->remove_words.start_anchor = MLX5_HEADER_ANCHOR_FIRST_VLAN_START;
		attr->remove_words.num_of_words = MLX5DR_ACTION_HDR_LEN_L2_VLAN / 2;
		break;
	case MLX5DR_ACTION_TYP_PUSH_VLAN:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_INSERT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->insert_header.encap = 0;
		attr->insert_header.is_inline = 1;
		attr->insert_header.insert_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->insert_header.insert_offset = MLX5DR_ACTION_HDR_LEN_L2_MACS;
		attr->insert_header.header_size = MLX5DR_ACTION_HDR_LEN_L2_VLAN;
		break;
	default:
		DR_LOG(ERR, "Invalid action type %d", action->type);
		assert(false);
	}
}

static int
mlx5dr_action_create_stcs(struct mlx5dr_action *action,
			  struct mlx5dr_devx_obj *obj)
{
	struct mlx5dr_cmd_stc_modify_attr stc_attr = {0};
	struct mlx5dr_context *ctx = action->ctx;
	int ret;

	mlx5dr_action_fill_stc_attr(action, obj, &stc_attr);

	/* Block unsupported parallel devx obj modify over the same base */
	pthread_spin_lock(&ctx->ctrl_lock);

	/* Allocate STC for RX */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_NIC_RX,
						     &action->stc[MLX5DR_TABLE_TYPE_NIC_RX]);
		if (ret)
			goto out_err;
	}

	/* Allocate STC for TX */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_NIC_TX,
						     &action->stc[MLX5DR_TABLE_TYPE_NIC_TX]);
		if (ret)
			goto free_nic_rx_stc;
	}

	/* Allocate STC for FDB */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_FDB,
						     &action->stc[MLX5DR_TABLE_TYPE_FDB]);
		if (ret)
			goto free_nic_tx_stc;
	}

	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

free_nic_tx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX)
		mlx5dr_action_free_single_stc(ctx,
					      MLX5DR_TABLE_TYPE_NIC_TX,
					      &action->stc[MLX5DR_TABLE_TYPE_NIC_TX]);
free_nic_rx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX)
		mlx5dr_action_free_single_stc(ctx,
					      MLX5DR_TABLE_TYPE_NIC_RX,
					      &action->stc[MLX5DR_TABLE_TYPE_NIC_RX]);
out_err:
	pthread_spin_unlock(&ctx->ctrl_lock);
	return rte_errno;
}

static void
mlx5dr_action_destroy_stcs(struct mlx5dr_action *action)
{
	struct mlx5dr_context *ctx = action->ctx;

	/* Block unsupported parallel devx obj modify over the same base */
	pthread_spin_lock(&ctx->ctrl_lock);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_NIC_RX,
					      &action->stc[MLX5DR_TABLE_TYPE_NIC_RX]);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_NIC_TX,
					      &action->stc[MLX5DR_TABLE_TYPE_NIC_TX]);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB]);

	pthread_spin_unlock(&ctx->ctrl_lock);
}

static bool
mlx5dr_action_is_root_flags(uint32_t flags)
{
	return flags & (MLX5DR_ACTION_FLAG_ROOT_RX |
			MLX5DR_ACTION_FLAG_ROOT_TX |
			MLX5DR_ACTION_FLAG_ROOT_FDB);
}

static bool
mlx5dr_action_is_hws_flags(uint32_t flags)
{
	return flags & (MLX5DR_ACTION_FLAG_HWS_RX |
			MLX5DR_ACTION_FLAG_HWS_TX |
			MLX5DR_ACTION_FLAG_HWS_FDB);
}

static struct mlx5dr_action *
mlx5dr_action_create_generic(struct mlx5dr_context *ctx,
			     uint32_t flags,
			     enum mlx5dr_action_type action_type)
{
	struct mlx5dr_action *action;

	if (!mlx5dr_action_is_root_flags(flags) &&
	    !mlx5dr_action_is_hws_flags(flags)) {
		DR_LOG(ERR, "Action flags must specify root or non root (HWS)");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (mlx5dr_action_is_hws_flags(flags) &&
	    !(ctx->flags & MLX5DR_CONTEXT_FLAG_HWS_SUPPORT)) {
		DR_LOG(ERR, "Cannot create HWS action since HWS is not supported");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = simple_calloc(1, sizeof(*action));
	if (!action) {
		DR_LOG(ERR, "Failed to allocate memory for action [%d]", action_type);
		rte_errno = ENOMEM;
		return NULL;
	}

	action->ctx = ctx;
	action->flags = flags;
	action->type = action_type;

	return action;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_table(struct mlx5dr_context *ctx,
				struct mlx5dr_table *tbl,
				uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_table_is_root(tbl)) {
		DR_LOG(ERR, "Root table cannot be set as destination");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (mlx5dr_action_is_hws_flags(flags) &&
	    mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Same action cannot be used for root and non root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_FT);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (mlx5dr_context_shared_gvmi_used(ctx))
			action->devx_obj = tbl->local_ft->obj;
		else
			action->devx_obj = tbl->ft->obj;
	} else {
		ret = mlx5dr_action_create_stcs(action, tbl->ft);
		if (ret)
			goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static int mlx5dr_action_get_dest_tir_obj(struct mlx5dr_context *ctx,
					  struct mlx5dr_action *action,
					  struct mlx5dr_devx_obj *obj,
					  struct mlx5dr_devx_obj **ret_obj)
{
	int ret;

	if (mlx5dr_context_shared_gvmi_used(ctx)) {
		ret = mlx5dr_matcher_create_aliased_obj(ctx,
							ctx->local_ibv_ctx,
							ctx->ibv_ctx,
							ctx->caps->vhca_id,
							obj->id,
							MLX5_GENERAL_OBJ_TYPE_TIR_ALIAS,
							&action->alias.devx_obj);
		if (ret) {
			DR_LOG(ERR, "Failed to create tir alias");
			return rte_errno;
		}
		*ret_obj = action->alias.devx_obj;
	} else {
		*ret_obj = obj;
	}

	return 0;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_tir(struct mlx5dr_context *ctx,
			      struct mlx5dr_devx_obj *obj,
			      uint32_t flags,
			      bool is_local)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_hws_flags(flags) &&
	    mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Same action cannot be used for root and non root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (!is_local) {
		DR_LOG(ERR, "TIR should be created on local ibv_device, flags: 0x%x",
		       flags);
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_TIR);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		action->devx_obj = obj->obj;
	} else {
		struct mlx5dr_devx_obj *cur_obj = NULL; /*compilation warn*/

		ret = mlx5dr_action_get_dest_tir_obj(ctx, action, obj, &cur_obj);
		if (ret) {
			DR_LOG(ERR, "Failed to create tir alias (flags: %d)", flags);
			goto free_action;
		}

		ret = mlx5dr_action_create_stcs(action, cur_obj);
		if (ret)
			goto clean_obj;
	}

	return action;

clean_obj:
	mlx5dr_cmd_destroy_obj(action->alias.devx_obj);
free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_drop(struct mlx5dr_context *ctx,
			       uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_DROP);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_hws_flags(flags)) {
		ret = mlx5dr_action_create_stcs(action, NULL);
		if (ret)
			goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_default_miss(struct mlx5dr_context *ctx,
				  uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_MISS);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_hws_flags(flags)) {
		ret = mlx5dr_action_create_stcs(action, NULL);
		if (ret)
			goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_tag(struct mlx5dr_context *ctx,
			 uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_TAG);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_hws_flags(flags)) {
		ret = mlx5dr_action_create_stcs(action, NULL);
		if (ret)
			goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static struct mlx5dr_action *
mlx5dr_action_create_aso(struct mlx5dr_context *ctx,
			 enum mlx5dr_action_type action_type,
			 struct mlx5dr_devx_obj *devx_obj,
			 uint8_t return_reg_id,
			 uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "ASO action cannot be used over root table");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, action_type);
	if (!action)
		return NULL;

	action->aso.devx_obj = devx_obj;
	action->aso.return_reg_id = return_reg_id;

	ret = mlx5dr_action_create_stcs(action, devx_obj);
	if (ret)
		goto free_action;

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_aso_meter(struct mlx5dr_context *ctx,
			       struct mlx5dr_devx_obj *devx_obj,
			       uint8_t return_reg_id,
			       uint32_t flags)
{
	return mlx5dr_action_create_aso(ctx, MLX5DR_ACTION_TYP_ASO_METER,
					devx_obj, return_reg_id, flags);
}

struct mlx5dr_action *
mlx5dr_action_create_aso_ct(struct mlx5dr_context *ctx,
			    struct mlx5dr_devx_obj *devx_obj,
			    uint8_t return_reg_id,
			    uint32_t flags)
{
	return mlx5dr_action_create_aso(ctx, MLX5DR_ACTION_TYP_ASO_CT,
					devx_obj, return_reg_id, flags);
}

struct mlx5dr_action *
mlx5dr_action_create_counter(struct mlx5dr_context *ctx,
			     struct mlx5dr_devx_obj *obj,
			     uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_hws_flags(flags) &&
	    mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Same action cannot be used for root and non root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_CTR);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		action->devx_obj = obj->obj;
	} else {
		ret = mlx5dr_action_create_stcs(action, obj);
		if (ret)
			goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static int mlx5dr_action_create_dest_vport_hws(struct mlx5dr_context *ctx,
					       struct mlx5dr_action *action,
					       uint32_t ib_port_num)
{
	struct mlx5dr_cmd_query_vport_caps vport_caps = {0};
	int ret;

	ret = mlx5dr_cmd_query_ib_port(ctx->ibv_ctx, &vport_caps, ib_port_num);
	if (ret) {
		DR_LOG(ERR, "Failed querying port %d\n", ib_port_num);
		return ret;
	}
	action->vport.vport_num = vport_caps.vport_num;
	action->vport.esw_owner_vhca_id = vport_caps.esw_owner_vhca_id;

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed creating stc for port %d\n", ib_port_num);
		return ret;
	}

	return 0;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_vport(struct mlx5dr_context *ctx,
				uint32_t ib_port_num,
				uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (!(flags & MLX5DR_ACTION_FLAG_HWS_FDB)) {
		DR_LOG(ERR, "Vport action is supported for FDB only\n");
		rte_errno = EINVAL;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_VPORT);
	if (!action)
		return NULL;

	ret = mlx5dr_action_create_dest_vport_hws(ctx, action, ib_port_num);
	if (ret) {
		DR_LOG(ERR, "Failed to create vport action HWS\n");
		goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_push_vlan(struct mlx5dr_context *ctx, uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Push vlan action not supported for root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_PUSH_VLAN);
	if (!action)
		return NULL;

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed creating stc for push vlan\n");
		goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_pop_vlan(struct mlx5dr_context *ctx, uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Pop vlan action not supported for root");
		rte_errno = ENOTSUP;
		return NULL;
	}
	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_POP_VLAN);
	if (!action)
		return NULL;

	ret = mlx5dr_action_get_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_POP);
	if (ret) {
		DR_LOG(ERR, "Failed to create remove stc for reformat");
		goto free_action;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed creating stc for pop vlan\n");
		goto free_shared;
	}

	return action;

free_shared:
	mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_POP);
free_action:
	simple_free(action);
	return NULL;
}

static int
mlx5dr_action_conv_reformat_type_to_action(uint32_t reformat_type,
					   enum mlx5dr_action_type *action_type)
{
	switch (reformat_type) {
	case MLX5DR_ACTION_REFORMAT_TYPE_TNL_L2_TO_L2:
		*action_type = MLX5DR_ACTION_TYP_TNL_L2_TO_L2;
		break;
	case MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L2:
		*action_type = MLX5DR_ACTION_TYP_L2_TO_TNL_L2;
		break;
	case MLX5DR_ACTION_REFORMAT_TYPE_TNL_L3_TO_L2:
		*action_type = MLX5DR_ACTION_TYP_TNL_L3_TO_L2;
		break;
	case MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L3:
		*action_type = MLX5DR_ACTION_TYP_L2_TO_TNL_L3;
		break;
	default:
		DR_LOG(ERR, "Invalid reformat type requested");
		rte_errno = ENOTSUP;
		return rte_errno;
	}
	return 0;
}

static void
mlx5dr_action_conv_reformat_to_verbs(uint32_t action_type,
				     uint32_t *verb_reformat_type)
{
	switch (action_type) {
	case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2;
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL;
		break;
	case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2;
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL;
		break;
	}
}

static int
mlx5dr_action_conv_flags_to_ft_type(uint32_t flags, enum mlx5dv_flow_table_type *ft_type)
{
	if (flags & (MLX5DR_ACTION_FLAG_ROOT_RX | MLX5DR_ACTION_FLAG_HWS_RX)) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	} else if (flags & (MLX5DR_ACTION_FLAG_ROOT_TX | MLX5DR_ACTION_FLAG_HWS_TX)) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX;
#ifdef HAVE_MLX5DV_FLOW_MATCHER_FT_TYPE
	} else if (flags & (MLX5DR_ACTION_FLAG_ROOT_FDB | MLX5DR_ACTION_FLAG_HWS_FDB)) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
#endif
	} else {
		rte_errno = ENOTSUP;
		return 1;
	}

	return 0;
}

static int
mlx5dr_action_create_reformat_root(struct mlx5dr_action *action,
				   size_t data_sz,
				   void *data)
{
	enum mlx5dv_flow_table_type ft_type = 0; /*fix compilation warn*/
	uint32_t verb_reformat_type = 0;
	struct ibv_context *ibv_ctx;
	int ret;

	/* Convert action to FT type and verbs reformat type */
	ret = mlx5dr_action_conv_flags_to_ft_type(action->flags, &ft_type);
	if (ret)
		return rte_errno;

	mlx5dr_action_conv_reformat_to_verbs(action->type, &verb_reformat_type);

	/* Create the reformat type for root table */
	ibv_ctx = mlx5dr_context_get_local_ibv(action->ctx);
	action->flow_action =
		mlx5_glue->dv_create_flow_action_packet_reformat_root(ibv_ctx,
								      data_sz,
								      data,
								      verb_reformat_type,
								      ft_type);
	if (!action->flow_action) {
		rte_errno = errno;
		return rte_errno;
	}

	return 0;
}

static int mlx5dr_action_handle_reformat_args(struct mlx5dr_context *ctx,
					      size_t data_sz,
					      void *data,
					      uint32_t bulk_size,
					      struct mlx5dr_action *action)
{
	uint32_t args_log_size;
	int ret;

	if (data_sz % 2 != 0) {
		DR_LOG(ERR, "Data size should be multiply of 2");
		rte_errno = EINVAL;
		return rte_errno;
	}
	action->reformat.header_size = data_sz;

	args_log_size = mlx5dr_arg_data_size_to_arg_log_size(data_sz);
	if (args_log_size >= MLX5DR_ARG_CHUNK_SIZE_MAX) {
		DR_LOG(ERR, "Data size is bigger than supported");
		rte_errno = EINVAL;
		return rte_errno;
	}
	args_log_size += bulk_size;

	if (!mlx5dr_arg_is_valid_arg_request_size(ctx, args_log_size)) {
		DR_LOG(ERR, "Arg size %d does not fit FW requests",
		       args_log_size);
		rte_errno = EINVAL;
		return rte_errno;
	}

	action->reformat.arg_obj = mlx5dr_cmd_arg_create(ctx->ibv_ctx,
							 args_log_size,
							 ctx->pd_num);
	if (!action->reformat.arg_obj) {
		DR_LOG(ERR, "Failed to create arg for reformat");
		return rte_errno;
	}

	/* When INLINE need to write the arg data */
	if (action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		ret = mlx5dr_arg_write_inline_arg_data(ctx,
						       action->reformat.arg_obj->id,
						       data,
						       data_sz);
		if (ret) {
			DR_LOG(ERR, "Failed to write inline arg for reformat");
			goto free_arg;
		}
	}

	return 0;

free_arg:
	mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
	return ret;
}

static int mlx5dr_action_handle_l2_to_tunnel_l2(struct mlx5dr_context *ctx,
						size_t data_sz,
						void *data,
						uint32_t bulk_size,
						struct mlx5dr_action *action)
{
	int ret;

	ret = mlx5dr_action_handle_reformat_args(ctx, data_sz, data, bulk_size,
						 action);
	if (ret) {
		DR_LOG(ERR, "Failed to create args for reformat");
		return ret;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed to create stc for reformat");
		goto free_arg;
	}

	return 0;

free_arg:
	mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
	return ret;
}

static int mlx5dr_action_get_shared_stc_offset(struct mlx5dr_context_common_res *common_res,
					       enum mlx5dr_context_shared_stc_type stc_type)
{
	return common_res->shared_stc[stc_type]->remove_header.offset;
}

static int mlx5dr_action_handle_l2_to_tunnel_l3(struct mlx5dr_context *ctx,
						size_t data_sz,
						void *data,
						uint32_t bulk_size,
						struct mlx5dr_action *action)
{
	int ret;

	ret = mlx5dr_action_handle_reformat_args(ctx, data_sz, data, bulk_size,
						 action);
	if (ret) {
		DR_LOG(ERR, "Failed to create args for reformat");
		return ret;
	}

	/* The action is remove-l2-header + insert-l3-header */
	ret = mlx5dr_action_get_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP);
	if (ret) {
		DR_LOG(ERR, "Failed to create remove stc for reformat");
		goto free_arg;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed to create insert stc for reformat");
		goto down_shared;
	}

	return 0;

down_shared:
	mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP);
free_arg:
	mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
	return ret;
}

static void mlx5dr_action_prepare_decap_l3_actions(size_t data_sz,
						   uint8_t *mh_data,
						   int *num_of_actions)
{
	int actions;
	uint32_t i;

	/* Remove L2L3 outer headers */
	MLX5_SET(stc_ste_param_remove, mh_data, action_type,
		 MLX5_MODIFICATION_TYPE_REMOVE);
	MLX5_SET(stc_ste_param_remove, mh_data, decap, 0x1);
	MLX5_SET(stc_ste_param_remove, mh_data, remove_start_anchor,
		 MLX5_HEADER_ANCHOR_PACKET_START);
	MLX5_SET(stc_ste_param_remove, mh_data, remove_end_anchor,
		 MLX5_HEADER_ANCHOR_INNER_IPV6_IPV4);
	mh_data += MLX5DR_ACTION_DOUBLE_SIZE; /* Assume every action is 2 dw */
	actions = 1;

	/* Add the new header using inline action 4Byte at a time, the header
	 * is added in reversed order to the beginning of the packet to avoid
	 * incorrect parsing by the HW. Since header is 14B or 18B an extra
	 * two bytes are padded and later removed.
	 */
	for (i = 0; i < data_sz / MLX5DR_ACTION_INLINE_DATA_SIZE + 1; i++) {
		MLX5_SET(stc_ste_param_insert, mh_data, action_type,
			 MLX5_MODIFICATION_TYPE_INSERT);
		MLX5_SET(stc_ste_param_insert, mh_data, inline_data, 0x1);
		MLX5_SET(stc_ste_param_insert, mh_data, insert_anchor,
			 MLX5_HEADER_ANCHOR_PACKET_START);
		MLX5_SET(stc_ste_param_insert, mh_data, insert_size, 2);
		mh_data += MLX5DR_ACTION_DOUBLE_SIZE;
		actions++;
	}

	/* Remove first 2 extra bytes */
	MLX5_SET(stc_ste_param_remove_words, mh_data, action_type,
		 MLX5_MODIFICATION_TYPE_REMOVE_WORDS);
	MLX5_SET(stc_ste_param_remove_words, mh_data, remove_start_anchor,
		 MLX5_HEADER_ANCHOR_PACKET_START);
	/* The hardware expects here size in words (2 bytes) */
	MLX5_SET(stc_ste_param_remove_words, mh_data, remove_size, 1);
	actions++;

	*num_of_actions = actions;
}

static int
mlx5dr_action_handle_tunnel_l3_to_l2(struct mlx5dr_context *ctx,
				     size_t data_sz,
				     void *data,
				     uint32_t bulk_size,
				     struct mlx5dr_action *action)
{
	uint8_t mh_data[MLX5DR_ACTION_REFORMAT_DATA_SIZE] = {0};
	int num_of_actions;
	int mh_data_size;
	int ret;

	if (data_sz != MLX5DR_ACTION_HDR_LEN_L2 &&
	    data_sz != MLX5DR_ACTION_HDR_LEN_L2_W_VLAN) {
		DR_LOG(ERR, "Data size is not supported for decap-l3\n");
		rte_errno = EINVAL;
		return rte_errno;
	}

	mlx5dr_action_prepare_decap_l3_actions(data_sz, mh_data, &num_of_actions);

	mh_data_size = num_of_actions * MLX5DR_MODIFY_ACTION_SIZE;

	ret = mlx5dr_pat_arg_create_modify_header(ctx, action, mh_data_size,
						  (__be64 *)mh_data, bulk_size);
	if (ret) {
		DR_LOG(ERR, "Failed allocating modify-header for decap-l3\n");
		return ret;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret)
		goto free_mh_obj;

	if (action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		mlx5dr_action_prepare_decap_l3_data(data, mh_data, num_of_actions);
		ret = mlx5dr_arg_write_inline_arg_data(ctx,
						       action->modify_header.arg_obj->id,
						       (uint8_t *)mh_data,
						       num_of_actions *
						       MLX5DR_MODIFY_ACTION_SIZE);
		if (ret) {
			DR_LOG(ERR, "Failed writing INLINE arg decap_l3");
			goto clean_stc;
		}
	}

	return 0;

clean_stc:
	mlx5dr_action_destroy_stcs(action);
free_mh_obj:
	mlx5dr_pat_arg_destroy_modify_header(ctx, action);
	return ret;
}

static int
mlx5dr_action_create_reformat_hws(struct mlx5dr_context *ctx,
				  size_t data_sz,
				  void *data,
				  uint32_t bulk_size,
				  struct mlx5dr_action *action)
{
	int ret;

	switch (action->type) {
	case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
		ret = mlx5dr_action_create_stcs(action, NULL);
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
		ret = mlx5dr_action_handle_l2_to_tunnel_l2(ctx, data_sz, data, bulk_size, action);
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
		ret = mlx5dr_action_handle_l2_to_tunnel_l3(ctx, data_sz, data, bulk_size, action);
		break;
	case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
		ret = mlx5dr_action_handle_tunnel_l3_to_l2(ctx, data_sz, data, bulk_size, action);
		break;

	default:
		assert(false);
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	return ret;
}

struct mlx5dr_action *
mlx5dr_action_create_reformat(struct mlx5dr_context *ctx,
			      enum mlx5dr_action_reformat_type reformat_type,
			      size_t data_sz,
			      void *inline_data,
			      uint32_t log_bulk_size,
			      uint32_t flags)
{
	enum mlx5dr_action_type action_type;
	struct mlx5dr_action *action;
	int ret;

	ret = mlx5dr_action_conv_reformat_type_to_action(reformat_type, &action_type);
	if (ret)
		return NULL;

	action = mlx5dr_action_create_generic(ctx, flags, action_type);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (log_bulk_size) {
			DR_LOG(ERR, "Bulk reformat not supported over root");
			rte_errno = ENOTSUP;
			goto free_action;
		}

		ret = mlx5dr_action_create_reformat_root(action, data_sz, inline_data);
		if (ret)
			goto free_action;

		return action;
	}

	if (!mlx5dr_action_is_hws_flags(flags) ||
	    ((flags & MLX5DR_ACTION_FLAG_SHARED) && log_bulk_size)) {
		DR_LOG(ERR, "Reformat flags don't fit HWS (flags: %x0x)\n",
			flags);
		rte_errno = EINVAL;
		goto free_action;
	}

	ret = mlx5dr_action_create_reformat_hws(ctx, data_sz, inline_data, log_bulk_size, action);
	if (ret) {
		DR_LOG(ERR, "Failed to create reformat.\n");
		rte_errno = EINVAL;
		goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static int
mlx5dr_action_create_modify_header_root(struct mlx5dr_action *action,
					size_t actions_sz,
					__be64 *actions)
{
	enum mlx5dv_flow_table_type ft_type = 0;
	struct ibv_context *local_ibv_ctx;
	int ret;

	ret = mlx5dr_action_conv_flags_to_ft_type(action->flags, &ft_type);
	if (ret)
		return rte_errno;

	local_ibv_ctx = mlx5dr_context_get_local_ibv(action->ctx);

	action->flow_action =
		mlx5_glue->dv_create_flow_action_modify_header_root(local_ibv_ctx,
								    actions_sz,
								    (uint64_t *)actions,
								    ft_type);
	if (!action->flow_action) {
		rte_errno = errno;
		return rte_errno;
	}

	return 0;
}

struct mlx5dr_action *
mlx5dr_action_create_modify_header(struct mlx5dr_context *ctx,
				   size_t pattern_sz,
				   __be64 pattern[],
				   uint32_t log_bulk_size,
				   uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_MODIFY_HDR);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (log_bulk_size) {
			DR_LOG(ERR, "Bulk modify-header not supported over root");
			rte_errno = ENOTSUP;
			goto free_action;
		}
		ret = mlx5dr_action_create_modify_header_root(action, pattern_sz, pattern);
		if (ret)
			goto free_action;

		return action;
	}

	if (!mlx5dr_action_is_hws_flags(flags) ||
	    ((flags & MLX5DR_ACTION_FLAG_SHARED) && log_bulk_size)) {
		DR_LOG(ERR, "Flags don't fit hws (flags: %x0x, log_bulk_size: %d)\n",
			flags, log_bulk_size);
		rte_errno = EINVAL;
		goto free_action;
	}

	if (!mlx5dr_pat_arg_verify_actions(pattern, pattern_sz / MLX5DR_MODIFY_ACTION_SIZE)) {
		DR_LOG(ERR, "One of the actions is not supported\n");
		rte_errno = EINVAL;
		goto free_action;
	}

	if (pattern_sz / MLX5DR_MODIFY_ACTION_SIZE == 1) {
		/* Optimize single modiy action to be used inline */
		action->modify_header.single_action = pattern[0];
		action->modify_header.num_of_actions = 1;
		action->modify_header.single_action_type =
			MLX5_GET(set_action_in, pattern, action_type);
	} else {
		/* Use multi action pattern and argument */
		ret = mlx5dr_pat_arg_create_modify_header(ctx, action, pattern_sz,
							  pattern, log_bulk_size);
		if (ret) {
			DR_LOG(ERR, "Failed allocating modify-header\n");
			goto free_action;
		}
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret)
		goto free_mh_obj;

	return action;

free_mh_obj:
	if (action->modify_header.num_of_actions > 1)
		mlx5dr_pat_arg_destroy_modify_header(ctx, action);
free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_root(struct mlx5dr_context *ctx,
			       uint16_t priority,
			       uint32_t flags)
{
	struct mlx5dv_steering_anchor_attr attr = {0};
	struct mlx5dv_steering_anchor *sa;
	struct mlx5dr_action *action;
	int ret;

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Action flags must be only non root (HWS)");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (mlx5dr_context_shared_gvmi_used(ctx)) {
		DR_LOG(ERR, "Cannot use this action in shared GVMI context");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (mlx5dr_action_conv_flags_to_ft_type(flags, &attr.ft_type))
		return NULL;

	attr.priority = priority;

	sa = mlx5_glue->create_steering_anchor(ctx->ibv_ctx, &attr);
	if (!sa) {
		DR_LOG(ERR, "Creation of steering anchor failed");
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_DEST_ROOT);
	if (!action)
		goto free_steering_anchor;

	action->root_tbl.sa = sa;

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret)
		goto free_action;

	return action;

free_action:
	simple_free(action);
free_steering_anchor:
	mlx5_glue->destroy_steering_anchor(sa);
	return NULL;
}

static void mlx5dr_action_destroy_hws(struct mlx5dr_action *action)
{
	switch (action->type) {
	case MLX5DR_ACTION_TYP_TIR:
		mlx5dr_action_destroy_stcs(action);
		if (mlx5dr_context_shared_gvmi_used(action->ctx))
			mlx5dr_cmd_destroy_obj(action->alias.devx_obj);
		break;
	case MLX5DR_ACTION_TYP_MISS:
	case MLX5DR_ACTION_TYP_TAG:
	case MLX5DR_ACTION_TYP_DROP:
	case MLX5DR_ACTION_TYP_CTR:
	case MLX5DR_ACTION_TYP_FT:
	case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
	case MLX5DR_ACTION_TYP_ASO_METER:
	case MLX5DR_ACTION_TYP_ASO_CT:
	case MLX5DR_ACTION_TYP_PUSH_VLAN:
		mlx5dr_action_destroy_stcs(action);
		break;
	case MLX5DR_ACTION_TYP_DEST_ROOT:
		mlx5dr_action_destroy_stcs(action);
		mlx5_glue->destroy_steering_anchor(action->root_tbl.sa);
		break;
	case MLX5DR_ACTION_TYP_POP_VLAN:
		mlx5dr_action_destroy_stcs(action);
		mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_POP);
		break;
	case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_MODIFY_HDR:
		mlx5dr_action_destroy_stcs(action);
		if (action->modify_header.num_of_actions > 1)
			mlx5dr_pat_arg_destroy_modify_header(action->ctx, action);
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
		mlx5dr_action_destroy_stcs(action);
		mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP);
		mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
		break;
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
		mlx5dr_action_destroy_stcs(action);
		mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
		break;
	}
}

static void mlx5dr_action_destroy_root(struct mlx5dr_action *action)
{
	switch (action->type) {
	case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
	case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
	case MLX5DR_ACTION_TYP_MODIFY_HDR:
		ibv_destroy_flow_action(action->flow_action);
		break;
	}
}

int mlx5dr_action_destroy(struct mlx5dr_action *action)
{
	if (mlx5dr_action_is_root_flags(action->flags))
		mlx5dr_action_destroy_root(action);
	else
		mlx5dr_action_destroy_hws(action);

	simple_free(action);
	return 0;
}

/* Called under pthread_spin_lock(&ctx->ctrl_lock) */
int mlx5dr_action_get_default_stc(struct mlx5dr_context *ctx,
				  uint8_t tbl_type)
{
	struct mlx5dr_cmd_stc_modify_attr stc_attr = {0};
	struct mlx5dr_action_default_stc *default_stc;
	int ret;

	if (ctx->common_res[tbl_type].default_stc) {
		ctx->common_res[tbl_type].default_stc->refcount++;
		return 0;
	}

	default_stc = simple_calloc(1, sizeof(*default_stc));
	if (!default_stc) {
		DR_LOG(ERR, "Failed to allocate memory for default STCs");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_NOP;
	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW0;
	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &default_stc->nop_ctr);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate default counter STC");
		goto free_default_stc;
	}

	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW5;
	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &default_stc->nop_dw5);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate default NOP DW5 STC");
		goto free_nop_ctr;
	}

	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW6;
	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &default_stc->nop_dw6);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate default NOP DW6 STC");
		goto free_nop_dw5;
	}

	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW7;
	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &default_stc->nop_dw7);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate default NOP DW7 STC");
		goto free_nop_dw6;
	}

	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_HIT;
	if (!mlx5dr_context_shared_gvmi_used(ctx)) {
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_ALLOW;
	} else {
		/* On shared gvmi the default hit behavior is jump to alias end ft */
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;
		stc_attr.dest_table_id = ctx->gvmi_res[tbl_type].aliased_end_ft->id;
	}

	ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr, tbl_type,
					     &default_stc->default_hit);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate default allow STC");
		goto free_nop_dw7;
	}

	ctx->common_res[tbl_type].default_stc = default_stc;
	ctx->common_res[tbl_type].default_stc->refcount++;

	return 0;

free_nop_dw7:
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw7);
free_nop_dw6:
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw6);
free_nop_dw5:
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw5);
free_nop_ctr:
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_ctr);
free_default_stc:
	simple_free(default_stc);
	return rte_errno;
}

void mlx5dr_action_put_default_stc(struct mlx5dr_context *ctx,
				   uint8_t tbl_type)
{
	struct mlx5dr_action_default_stc *default_stc;

	default_stc = ctx->common_res[tbl_type].default_stc;

	default_stc = ctx->common_res[tbl_type].default_stc;
	if (--default_stc->refcount)
		return;

	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->default_hit);
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw7);
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw6);
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_dw5);
	mlx5dr_action_free_single_stc(ctx, tbl_type, &default_stc->nop_ctr);
	simple_free(default_stc);
	ctx->common_res[tbl_type].default_stc = NULL;
}

static void mlx5dr_action_modify_write(struct mlx5dr_send_engine *queue,
				       uint32_t arg_idx,
				       uint8_t *arg_data,
				       uint16_t num_of_actions)
{
	mlx5dr_arg_write(queue, NULL, arg_idx, arg_data,
			 num_of_actions * MLX5DR_MODIFY_ACTION_SIZE);
}

void
mlx5dr_action_prepare_decap_l3_data(uint8_t *src, uint8_t *dst,
				    uint16_t num_of_actions)
{
	uint8_t *e_src;
	int i;

	/* num_of_actions = remove l3l2 + 4/5 inserts + remove extra 2 bytes
	 * copy from end of src to the start of dst.
	 * move to the end, 2 is the leftover from 14B or 18B
	 */
	if (num_of_actions == DECAP_L3_NUM_ACTIONS_W_NO_VLAN)
		e_src = src + MLX5DR_ACTION_HDR_LEN_L2;
	else
		e_src = src + MLX5DR_ACTION_HDR_LEN_L2_W_VLAN;

	/* Move dst over the first remove action + zero data */
	dst += MLX5DR_ACTION_DOUBLE_SIZE;
	/* Move dst over the first insert ctrl action */
	dst += MLX5DR_ACTION_DOUBLE_SIZE / 2;
	/* Actions:
	 * no vlan: r_h-insert_4b-insert_4b-insert_4b-insert_4b-remove_2b.
	 * with vlan: r_h-insert_4b-insert_4b-insert_4b-insert_4b-insert_4b-remove_2b.
	 * the loop is without the last insertion.
	 */
	for (i = 0; i < num_of_actions - 3; i++) {
		e_src -= MLX5DR_ACTION_INLINE_DATA_SIZE;
		memcpy(dst, e_src, MLX5DR_ACTION_INLINE_DATA_SIZE); /* data */
		dst += MLX5DR_ACTION_DOUBLE_SIZE;
	}
	/* Copy the last 2 bytes after a gap of 2 bytes which will be removed */
	e_src -= MLX5DR_ACTION_INLINE_DATA_SIZE / 2;
	dst += MLX5DR_ACTION_INLINE_DATA_SIZE / 2;
	memcpy(dst, e_src, 2);
}

static struct mlx5dr_actions_wqe_setter *
mlx5dr_action_setter_find_first(struct mlx5dr_actions_wqe_setter *setter,
				uint8_t req_flags)
{
	/* Use a new setter if requested flags are taken */
	while (setter->flags & req_flags)
		setter++;

	/* Use current setter in required flags are not used */
	return setter;
}

static void
mlx5dr_action_apply_stc(struct mlx5dr_actions_apply_data *apply,
			enum mlx5dr_action_stc_idx stc_idx,
			uint8_t action_idx)
{
	struct mlx5dr_action *action = apply->rule_action[action_idx].action;

	apply->wqe_ctrl->stc_ix[stc_idx] =
		htobe32(action->stc[apply->tbl_type].offset);
}

static void
mlx5dr_action_setter_push_vlan(struct mlx5dr_actions_apply_data *apply,
			       struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;

	rule_action = &apply->rule_action[setter->idx_double];
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = rule_action->push_vlan.vlan_hdr;

	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW6, setter->idx_double);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;
}

static void
mlx5dr_action_setter_modify_header(struct mlx5dr_actions_apply_data *apply,
				   struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	struct mlx5dr_action *action;
	uint32_t arg_sz, arg_idx;
	uint8_t *single_action;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action;
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW6, setter->idx_double);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;

	if (action->modify_header.num_of_actions == 1) {
		if (action->modify_header.single_action_type ==
		    MLX5_MODIFICATION_TYPE_COPY) {
			apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = 0;
			return;
		}

		if (action->flags & MLX5DR_ACTION_FLAG_SHARED)
			single_action = (uint8_t *)&action->modify_header.single_action;
		else
			single_action = rule_action->modify_header.data;

		apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] =
			*(__be32 *)MLX5_ADDR_OF(set_action_in, single_action, data);
	} else {
		/* Argument offset multiple with number of args per these actions */
		arg_sz = mlx5dr_arg_get_arg_size(action->modify_header.num_of_actions);
		arg_idx = rule_action->modify_header.offset * arg_sz;

		apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(arg_idx);

		if (!(action->flags & MLX5DR_ACTION_FLAG_SHARED)) {
			apply->require_dep = 1;
			mlx5dr_action_modify_write(apply->queue,
						   action->modify_header.arg_obj->id + arg_idx,
						   rule_action->modify_header.data,
						   action->modify_header.num_of_actions);
		}
	}
}

static void
mlx5dr_action_setter_insert_ptr(struct mlx5dr_actions_apply_data *apply,
				struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	uint32_t arg_idx, arg_sz;

	rule_action = &apply->rule_action[setter->idx_double];

	/* Argument offset multiple on args required for header size */
	arg_sz = mlx5dr_arg_data_size_to_arg_size(rule_action->action->reformat.header_size);
	arg_idx = rule_action->reformat.offset * arg_sz;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(arg_idx);

	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW6, setter->idx_double);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	if (!(rule_action->action->flags & MLX5DR_ACTION_FLAG_SHARED)) {
		apply->require_dep = 1;
		mlx5dr_arg_write(apply->queue, NULL,
				 rule_action->action->reformat.arg_obj->id + arg_idx,
				 rule_action->reformat.data,
				 rule_action->action->reformat.header_size);
	}
}

static void
mlx5dr_action_setter_tnl_l3_to_l2(struct mlx5dr_actions_apply_data *apply,
				  struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	struct mlx5dr_action *action;
	uint32_t arg_sz, arg_idx;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action;

	/* Argument offset multiple on args required for num of actions */
	arg_sz = mlx5dr_arg_get_arg_size(action->modify_header.num_of_actions);
	arg_idx = rule_action->reformat.offset * arg_sz;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(arg_idx);

	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW6, setter->idx_double);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	if (!(action->flags & MLX5DR_ACTION_FLAG_SHARED)) {
		apply->require_dep = 1;
		mlx5dr_arg_decapl3_write(apply->queue,
					 action->modify_header.arg_obj->id + arg_idx,
					 rule_action->reformat.data,
					 action->modify_header.num_of_actions);
	}
}

static void
mlx5dr_action_setter_aso(struct mlx5dr_actions_apply_data *apply,
			 struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	uint32_t exe_aso_ctrl;
	uint32_t offset;

	rule_action = &apply->rule_action[setter->idx_double];

	switch (rule_action->action->type) {
	case MLX5DR_ACTION_TYP_ASO_METER:
		/* exe_aso_ctrl format:
		 * [STC only and reserved bits 29b][init_color 2b][meter_id 1b]
		 */
		offset = rule_action->aso_meter.offset / MLX5_ASO_METER_NUM_PER_OBJ;
		exe_aso_ctrl = rule_action->aso_meter.offset % MLX5_ASO_METER_NUM_PER_OBJ;
		exe_aso_ctrl |= rule_action->aso_meter.init_color <<
				MLX5DR_ACTION_METER_INIT_COLOR_OFFSET;
		break;
	case MLX5DR_ACTION_TYP_ASO_CT:
		/* exe_aso_ctrl CT format:
		 * [STC only and reserved bits 31b][direction 1b]
		 */
		offset = rule_action->aso_ct.offset / MLX5_ASO_CT_NUM_PER_OBJ;
		exe_aso_ctrl = rule_action->aso_ct.direction;
		break;
	default:
		DR_LOG(ERR, "Unsupported ASO action type: %d", rule_action->action->type);
		rte_errno = ENOTSUP;
		return;
	}

	/* aso_object_offset format: [24B] */
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = htobe32(offset);
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(exe_aso_ctrl);

	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW6, setter->idx_double);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;
}

static void
mlx5dr_action_setter_tag(struct mlx5dr_actions_apply_data *apply,
			 struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;

	rule_action = &apply->rule_action[setter->idx_single];
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = htobe32(rule_action->tag.value);
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW5, setter->idx_single);
}

static void
mlx5dr_action_setter_ctrl_ctr(struct mlx5dr_actions_apply_data *apply,
			      struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;

	rule_action = &apply->rule_action[setter->idx_ctr];
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW0] = htobe32(rule_action->counter.offset);
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_CTRL, setter->idx_ctr);
}

static void
mlx5dr_action_setter_single(struct mlx5dr_actions_apply_data *apply,
			    struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_DW5, setter->idx_single);
}

static void
mlx5dr_action_setter_single_double_pop(struct mlx5dr_actions_apply_data *apply,
				       __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW5] =
		htobe32(mlx5dr_action_get_shared_stc_offset(apply->common_res,
						    MLX5DR_CONTEXT_SHARED_STC_POP));
}

static void
mlx5dr_action_setter_hit(struct mlx5dr_actions_apply_data *apply,
			 struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_HIT_LSB] = 0;
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_HIT, setter->idx_hit);
}

static void
mlx5dr_action_setter_default_hit(struct mlx5dr_actions_apply_data *apply,
				 __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_HIT_LSB] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_HIT] =
		htobe32(apply->common_res->default_stc->default_hit.offset);
}

static void
mlx5dr_action_setter_hit_next_action(struct mlx5dr_actions_apply_data *apply,
				     __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_HIT_LSB] = htobe32(apply->next_direct_idx << 6);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_HIT] = htobe32(apply->jump_to_action_stc);
}

static void
mlx5dr_action_setter_common_decap(struct mlx5dr_actions_apply_data *apply,
				  __rte_unused struct mlx5dr_actions_wqe_setter *setter)
{
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW5] =
		htobe32(mlx5dr_action_get_shared_stc_offset(apply->common_res,
							    MLX5DR_CONTEXT_SHARED_STC_DECAP));
}

int mlx5dr_action_template_process(struct mlx5dr_action_template *at)
{
	struct mlx5dr_actions_wqe_setter *start_setter = at->setters + 1;
	enum mlx5dr_action_type *action_type = at->action_type_arr;
	struct mlx5dr_actions_wqe_setter *setter = at->setters;
	struct mlx5dr_actions_wqe_setter *pop_setter = NULL;
	struct mlx5dr_actions_wqe_setter *last_setter;
	int i;

	/* Note: Given action combination must be valid */

	/* Check if action were already processed */
	if (at->num_of_action_stes)
		return 0;

	for (i = 0; i < MLX5DR_ACTION_MAX_STE; i++)
		setter[i].set_hit = &mlx5dr_action_setter_hit_next_action;

	/* The same action template setters can be used with jumbo or match
	 * STE, to support both cases we reseve the first setter for cases
	 * with jumbo STE to allow jump to the first action STE.
	 * This extra setter can be reduced in some cases on rule creation.
	 */
	setter = start_setter;
	last_setter = start_setter;

	for (i = 0; i < at->num_actions; i++) {
		switch (action_type[i]) {
		case MLX5DR_ACTION_TYP_DROP:
		case MLX5DR_ACTION_TYP_TIR:
		case MLX5DR_ACTION_TYP_FT:
		case MLX5DR_ACTION_TYP_DEST_ROOT:
		case MLX5DR_ACTION_TYP_VPORT:
		case MLX5DR_ACTION_TYP_MISS:
			/* Hit action */
			last_setter->flags |= ASF_HIT;
			last_setter->set_hit = &mlx5dr_action_setter_hit;
			last_setter->idx_hit = i;
			break;

		case MLX5DR_ACTION_TYP_POP_VLAN:
			/* Single remove header to header */
			if (pop_setter) {
				/* We have 2 pops, use the shared */
				pop_setter->set_single = &mlx5dr_action_setter_single_double_pop;
				break;
			}
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_SINGLE1 | ASF_MODIFY);
			setter->flags |= ASF_SINGLE1 | ASF_REPARSE | ASF_REMOVE;
			setter->set_single = &mlx5dr_action_setter_single;
			setter->idx_single = i;
			pop_setter = setter;
			break;

		case MLX5DR_ACTION_TYP_PUSH_VLAN:
			/* Double insert inline */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_REPARSE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_push_vlan;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_MODIFY_HDR:
			/* Double modify header list */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_MODIFY | ASF_REPARSE;
			setter->set_double = &mlx5dr_action_setter_modify_header;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_ASO_METER:
		case MLX5DR_ACTION_TYP_ASO_CT:
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE);
			setter->flags |= ASF_DOUBLE;
			setter->set_double = &mlx5dr_action_setter_aso;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_TNL_L2_TO_L2:
			/* Single remove header to header */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_SINGLE1 | ASF_MODIFY);
			setter->flags |= ASF_SINGLE1 | ASF_REMOVE | ASF_REPARSE;
			setter->set_single = &mlx5dr_action_setter_single;
			setter->idx_single = i;
			break;

		case MLX5DR_ACTION_TYP_L2_TO_TNL_L2:
			/* Double insert header with pointer */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE);
			setter->flags |= ASF_DOUBLE | ASF_REPARSE;
			setter->set_double = &mlx5dr_action_setter_insert_ptr;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_L2_TO_TNL_L3:
			/* Single remove + Double insert header with pointer */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_SINGLE1 | ASF_DOUBLE);
			setter->flags |= ASF_SINGLE1 | ASF_DOUBLE | ASF_REPARSE | ASF_REMOVE;
			setter->set_double = &mlx5dr_action_setter_insert_ptr;
			setter->idx_double = i;
			setter->set_single = &mlx5dr_action_setter_common_decap;
			setter->idx_single = i;
			break;

		case MLX5DR_ACTION_TYP_TNL_L3_TO_L2:
			/* Double modify header list with remove and push inline */
			setter = mlx5dr_action_setter_find_first(last_setter,
								 ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_MODIFY | ASF_REPARSE;
			setter->set_double = &mlx5dr_action_setter_tnl_l3_to_l2;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_TAG:
			/* Single TAG action, search for any room from the start */
			setter = mlx5dr_action_setter_find_first(start_setter, ASF_SINGLE1);
			setter->flags |= ASF_SINGLE1;
			setter->set_single = &mlx5dr_action_setter_tag;
			setter->idx_single = i;
			break;

		case MLX5DR_ACTION_TYP_CTR:
			/* Control counter action
			 * TODO: Current counter executed first. Support is needed
			 *	 for single ation counter action which is done last.
			 *	 Example: Decap + CTR
			 */
			setter = mlx5dr_action_setter_find_first(start_setter, ASF_CTR);
			setter->flags |= ASF_CTR;
			setter->set_ctr = &mlx5dr_action_setter_ctrl_ctr;
			setter->idx_ctr = i;
			break;

		default:
			DR_LOG(ERR, "Unsupported action type: %d", action_type[i]);
			rte_errno = ENOTSUP;
			assert(false);
			return rte_errno;
		}

		last_setter = RTE_MAX(setter, last_setter);
	}

	/* Set default hit on the last STE if no hit action provided */
	if (!(last_setter->flags & ASF_HIT))
		last_setter->set_hit = &mlx5dr_action_setter_default_hit;

	at->num_of_action_stes = last_setter - start_setter + 1;

	/* Check if action template doesn't require any action DWs */
	at->only_term = (at->num_of_action_stes == 1) &&
		!(last_setter->flags & ~(ASF_CTR | ASF_HIT));

	return 0;
}

struct mlx5dr_action_template *
mlx5dr_action_template_create(const enum mlx5dr_action_type action_type[])
{
	struct mlx5dr_action_template *at;
	uint8_t num_actions = 0;
	int i;

	at = simple_calloc(1, sizeof(*at));
	if (!at) {
		DR_LOG(ERR, "Failed to allocate action template");
		rte_errno = ENOMEM;
		return NULL;
	}

	while (action_type[num_actions++] != MLX5DR_ACTION_TYP_LAST)
		;

	at->num_actions = num_actions - 1;
	at->action_type_arr = simple_calloc(num_actions, sizeof(*action_type));
	if (!at->action_type_arr) {
		DR_LOG(ERR, "Failed to allocate action type array");
		rte_errno = ENOMEM;
		goto free_at;
	}

	for (i = 0; i < num_actions; i++)
		at->action_type_arr[i] = action_type[i];

	return at;

free_at:
	simple_free(at);
	return NULL;
}

int mlx5dr_action_template_destroy(struct mlx5dr_action_template *at)
{
	simple_free(at->action_type_arr);
	simple_free(at);
	return 0;
}
