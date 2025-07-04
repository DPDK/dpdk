/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

#define WIRE_PORT 0xFFFF

#define MLX5DR_ACTION_METER_INIT_COLOR_OFFSET 1
/* Header removal size limited to 128B (64 words) */
#define MLX5DR_ACTION_REMOVE_HEADER_MAX_SIZE 128

static struct mlx5dr_devx_obj *
mlx5dr_action_get_stc_obj_by_tbl_type(enum mlx5dr_table_type table_type,
				      struct mlx5dr_pool *stc_pool,
				      struct mlx5dr_pool_chunk *stc,
				      bool is_mirror);

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
		BIT(MLX5DR_ACTION_TYP_REMOVE_HEADER) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_CTR),
		BIT(MLX5DR_ACTION_TYP_ASO_METER),
		BIT(MLX5DR_ACTION_TYP_ASO_CT),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_NAT64),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_INSERT_HEADER) |
		BIT(MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_TBL) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_TIR) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT) |
		BIT(MLX5DR_ACTION_TYP_DEST_ARRAY) |
		BIT(MLX5DR_ACTION_TYP_JUMP_TO_MATCHER),
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
		BIT(MLX5DR_ACTION_TYP_NAT64),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_INSERT_HEADER) |
		BIT(MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_TBL) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT) |
		BIT(MLX5DR_ACTION_TYP_JUMP_TO_MATCHER),
		BIT(MLX5DR_ACTION_TYP_LAST),
	},
	[MLX5DR_TABLE_TYPE_FDB] = {
		BIT(MLX5DR_ACTION_TYP_REMOVE_HEADER) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2) |
		BIT(MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_POP_VLAN),
		BIT(MLX5DR_ACTION_TYP_CTR),
		BIT(MLX5DR_ACTION_TYP_ASO_METER),
		BIT(MLX5DR_ACTION_TYP_ASO_CT),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_PUSH_VLAN),
		BIT(MLX5DR_ACTION_TYP_NAT64),
		BIT(MLX5DR_ACTION_TYP_MODIFY_HDR),
		BIT(MLX5DR_ACTION_TYP_INSERT_HEADER) |
		BIT(MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2) |
		BIT(MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3),
		BIT(MLX5DR_ACTION_TYP_TBL) |
		BIT(MLX5DR_ACTION_TYP_MISS) |
		BIT(MLX5DR_ACTION_TYP_VPORT) |
		BIT(MLX5DR_ACTION_TYP_DROP) |
		BIT(MLX5DR_ACTION_TYP_DEST_ROOT) |
		BIT(MLX5DR_ACTION_TYP_DEST_ARRAY) |
		BIT(MLX5DR_ACTION_TYP_JUMP_TO_MATCHER),
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
	case MLX5DR_CONTEXT_SHARED_STC_DECAP_L3:
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_REMOVE;
		stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW5;
		stc_attr.reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;
		stc_attr.remove_header.decap = 0;
		stc_attr.remove_header.start_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		stc_attr.remove_header.end_anchor = MLX5_HEADER_ANCHOR_IPV6_IPV4;
		break;
	case MLX5DR_CONTEXT_SHARED_STC_DOUBLE_POP:
		stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_REMOVE_WORDS;
		stc_attr.action_offset = MLX5DR_ACTION_OFFSET_DW5;
		stc_attr.reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		stc_attr.remove_words.start_anchor = MLX5_HEADER_ANCHOR_FIRST_VLAN_START;
		stc_attr.remove_words.num_of_words = MLX5DR_ACTION_HDR_LEN_L2_VLAN;
		break;
	default:
		DR_LOG(ERR, "No such type : stc_type");
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

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type,
						       MLX5DR_TABLE_TYPE_FDB_RX);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate FDB_RX shared STCs (type: %d)",
			       stc_type);
			goto clean_nic_fdb_stc;
		}
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type,
						       MLX5DR_TABLE_TYPE_FDB_TX);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate FDB_TX shared STCs (type: %d)",
			       stc_type);
			goto clean_nic_fdb_rx_stc;
		}
	}

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) {
		ret = mlx5dr_action_get_shared_stc_nic(ctx, stc_type,
						       MLX5DR_TABLE_TYPE_FDB_UNIFIED);
		if (ret) {
			DR_LOG(ERR, "Failed to allocate FDB_UNIFIED shared STCs (type: %d)",
			       stc_type);
			goto clean_nic_fdb_tx_stc;
		}
	}

	return 0;

clean_nic_fdb_tx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB_TX);
clean_nic_fdb_rx_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB_RX);
clean_nic_fdb_stc:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB);
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

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB_RX);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB_TX);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED)
		mlx5dr_action_put_shared_stc_nic(ctx, stc_type, MLX5DR_TABLE_TYPE_FDB_UNIFIED);
}

static void
mlx5dr_action_create_nat64_zero_all_addr(uint8_t **action_ptr, bool is_v4_to_v6)
{
	if (is_v4_to_v6) {
		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_SIPV4);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_DIPV4);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	} else {
		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_SIPV6_127_96);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_SIPV6_95_64);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_SIPV6_63_32);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_SIPV6_31_0);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_DIPV6_127_96);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_DIPV6_95_64);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_DIPV6_63_32);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(set_action_in, *action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, *action_ptr, field, MLX5_MODI_OUT_DIPV6_31_0);
		MLX5_SET(set_action_in, *action_ptr, data, 0);
		*action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}
}

static struct mlx5dr_action *
mlx5dr_action_create_nat64_copy_state(struct mlx5dr_context *ctx,
				      struct mlx5dr_action_nat64_attr *attr,
				      uint32_t flags)
{
	__be64 modify_action_data[MLX5DR_ACTION_NAT64_MAX_MODIFY_ACTIONS];
	struct mlx5dr_action_mh_pattern pat[2];
	struct mlx5dr_action *action;
	uint32_t packet_len_field;
	uint8_t *action_ptr;
	uint32_t tos_field;
	uint32_t tos_size;
	uint32_t src_addr;
	uint32_t dst_addr;
	bool is_v4_to_v6;
	uint32_t ecn;

	is_v4_to_v6 = attr->flags & MLX5DR_ACTION_NAT64_V4_TO_V6;

	if (is_v4_to_v6) {
		packet_len_field = MLX5_MODI_OUT_IPV4_TOTAL_LEN;
		tos_field = MLX5_MODI_OUT_IP_DSCP;
		tos_size = 6;
		ecn = MLX5_MODI_OUT_IP_ECN;
		src_addr = MLX5_MODI_OUT_SIPV4;
		dst_addr = MLX5_MODI_OUT_DIPV4;
	} else {
		packet_len_field = MLX5_MODI_OUT_IPV6_PAYLOAD_LEN;
		tos_field = MLX5_MODI_OUT_IPV6_TRAFFIC_CLASS;
		tos_size = 8;
		ecn = 0;
		src_addr = MLX5_MODI_OUT_SIPV6_31_0;
		dst_addr = MLX5_MODI_OUT_DIPV6_31_0;
	}

	memset(modify_action_data, 0, sizeof(modify_action_data));
	action_ptr = (uint8_t *)modify_action_data;

	if (attr->flags & MLX5DR_ACTION_NAT64_BACKUP_ADDR) {
		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field, src_addr);
		MLX5_SET(copy_action_in, action_ptr, dst_field,
			 attr->registers[MLX5DR_ACTION_NAT64_REG_SRC_IP]);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field, dst_addr);
		MLX5_SET(copy_action_in, action_ptr, dst_field,
			 attr->registers[MLX5DR_ACTION_NAT64_REG_DST_IP]);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}

	/* | 8 bit - 8 bit     - 16 bit     |
	 * | TOS   - protocol  - packet-len |
	 */
	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field, packet_len_field);
	MLX5_SET(copy_action_in, action_ptr, dst_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_offset, 0);/* 16 bits in the lsb */
	MLX5_SET(copy_action_in, action_ptr, length, 16);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field, MLX5_MODI_OUT_IP_PROTOCOL);
	MLX5_SET(copy_action_in, action_ptr, dst_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_offset, 16);
	MLX5_SET(copy_action_in, action_ptr, length, 8);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field, tos_field);
	MLX5_SET(copy_action_in, action_ptr, dst_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_offset, 24);
	MLX5_SET(copy_action_in, action_ptr, length, tos_size);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	/* in ipv4 TOS = {dscp (6bits) - ecn (2bits) }*/
	if (ecn) {
		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field, ecn);
		MLX5_SET(copy_action_in, action_ptr, dst_field,
			attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
		MLX5_SET(copy_action_in, action_ptr, dst_offset, 24 + tos_size);
		MLX5_SET(copy_action_in, action_ptr, length, MLX5DR_ACTION_NAT64_ECN_SIZE);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}

	/* set sip and dip to 0, in order to have new csum */
	mlx5dr_action_create_nat64_zero_all_addr(&action_ptr, is_v4_to_v6);

	pat[0].data = modify_action_data;
	pat[0].sz = (action_ptr - (uint8_t *)modify_action_data);

	action = mlx5dr_action_create_modify_header(ctx, 1, pat, 0, flags);
	if (!action) {
		DR_LOG(ERR, "Failed to create copy for NAT64: action_sz: %zu, flags: 0x%x\n",
		       pat[0].sz, flags);
		return NULL;
	}

	return action;
}

static struct mlx5dr_action *
mlx5dr_action_create_nat64_repalce_state(struct mlx5dr_context *ctx,
					 struct mlx5dr_action_nat64_attr *attr,
					 uint32_t flags)
{
	uint32_t address_prefix[MLX5DR_ACTION_NAT64_HEADER_MINUS_ONE] = {0};
	__be64 modify_action_data[MLX5DR_ACTION_NAT64_MAX_MODIFY_ACTIONS];
	struct mlx5dr_action_mh_pattern pat[2];
	static struct mlx5dr_action *action;
	uint8_t header_size_in_dw;
	uint8_t *action_ptr;
	uint32_t eth_type;
	bool is_v4_to_v6;
	uint32_t ip_ver;
	int i;

	is_v4_to_v6 = attr->flags & MLX5DR_ACTION_NAT64_V4_TO_V6;

	if (is_v4_to_v6) {
		uint32_t nat64_well_known_pref[] = {0x00010000,
						    0x9bff6400, 0x0, 0x0, 0x0,
						    0x9bff6400, 0x0, 0x0, 0x0};

		header_size_in_dw = MLX5DR_ACTION_NAT64_IPV6_HEADER;
		ip_ver = MLX5DR_ACTION_NAT64_IPV6_VER;
		eth_type = RTE_ETHER_TYPE_IPV6;
		memcpy(address_prefix, nat64_well_known_pref,
		       MLX5DR_ACTION_NAT64_HEADER_MINUS_ONE * sizeof(uint32_t));
	} else {
		/* In order to fix HW csum issue, make the prefix ready */
		uint32_t ipv4_pref[] = {0x0, 0xffba0000, 0x0, 0x0, 0x0};

		header_size_in_dw = MLX5DR_ACTION_NAT64_IPV4_HEADER;
		ip_ver = MLX5DR_ACTION_NAT64_IPV4_VER;
		eth_type = RTE_ETHER_TYPE_IPV4;
		memcpy(address_prefix, ipv4_pref,
		       MLX5DR_ACTION_NAT64_IPV4_HEADER * sizeof(uint32_t));
	}

	memset(modify_action_data, 0, sizeof(modify_action_data));
	action_ptr = (uint8_t *)modify_action_data;

	MLX5_SET(set_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
	MLX5_SET(set_action_in, action_ptr, field, MLX5_MODI_OUT_ETHERTYPE);
	MLX5_SET(set_action_in, action_ptr, length, 16);
	MLX5_SET(set_action_in, action_ptr, data, eth_type);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	/* push empty header with ipv6 as version */
	MLX5_SET(stc_ste_param_insert, action_ptr, action_type,
		 MLX5_MODIFICATION_TYPE_INSERT);
	MLX5_SET(stc_ste_param_insert, action_ptr, inline_data, 0x1);
	MLX5_SET(stc_ste_param_insert, action_ptr, insert_anchor,
		 MLX5_HEADER_ANCHOR_IPV6_IPV4);
	MLX5_SET(stc_ste_param_insert, action_ptr, insert_size, 2);
	MLX5_SET(stc_ste_param_insert, action_ptr, insert_argument, ip_ver);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	for (i = 0; i < header_size_in_dw - 1; i++) {
		MLX5_SET(stc_ste_param_insert, action_ptr, action_type,
				MLX5_MODIFICATION_TYPE_INSERT);
		MLX5_SET(stc_ste_param_insert, action_ptr, inline_data, 0x1);
		MLX5_SET(stc_ste_param_insert, action_ptr, insert_anchor,
				MLX5_HEADER_ANCHOR_IPV6_IPV4);
		MLX5_SET(stc_ste_param_insert, action_ptr, insert_size, 2);
		MLX5_SET(stc_ste_param_insert, action_ptr, insert_argument,
			 htobe32(address_prefix[i]));
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}

	/* Remove orig src/dst addr (8 bytes, 4 words) */
	MLX5_SET(stc_ste_param_remove, action_ptr, action_type,
		 MLX5_MODIFICATION_TYPE_REMOVE);
	MLX5_SET(stc_ste_param_remove, action_ptr, remove_start_anchor,
		 MLX5_HEADER_ANCHOR_IPV6_IPV4);
	MLX5_SET(stc_ste_param_remove, action_ptr, remove_end_anchor,
		 MLX5_HEADER_ANCHOR_TCP_UDP);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	pat[0].data = modify_action_data;
	pat[0].sz = action_ptr - (uint8_t *)modify_action_data;

	action = mlx5dr_action_create_modify_header(ctx, 1, pat, 0, flags);
	if (!action) {
		DR_LOG(ERR, "Failed to create action: action_sz: %zu flags: 0x%x\n",
		       pat[0].sz, flags);
		return NULL;
	}

	return action;
}

static struct mlx5dr_action *
mlx5dr_action_create_nat64_copy_proto_state(struct mlx5dr_context *ctx,
					    struct mlx5dr_action_nat64_attr *attr,
					    uint32_t flags)
{
	__be64 modify_action_data[MLX5DR_ACTION_NAT64_MAX_MODIFY_ACTIONS];
	struct mlx5dr_action_mh_pattern pat[2];
	struct mlx5dr_action *action;
	uint8_t *action_ptr;

	memset(modify_action_data, 0, sizeof(modify_action_data));
	action_ptr = (uint8_t *)modify_action_data;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_field,
		 MLX5_MODI_OUT_IP_PROTOCOL);
	MLX5_SET(copy_action_in, action_ptr, src_offset, 16);
	MLX5_SET(copy_action_in, action_ptr, dst_offset, 0);
	MLX5_SET(copy_action_in, action_ptr, length, 8);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	pat[0].data = modify_action_data;
	pat[0].sz = action_ptr - (uint8_t *)modify_action_data;

	action = mlx5dr_action_create_modify_header_reparse(ctx, 1, pat, 0, flags,
							    MLX5DR_ACTION_STC_REPARSE_ON);
	if (!action) {
		DR_LOG(ERR, "Failed to create action: action_sz: %zu, flags: 0x%x\n",
		       pat[0].sz, flags);
		return NULL;
	}

	return action;
}

static struct mlx5dr_action *
mlx5dr_action_create_nat64_copy_back_state(struct mlx5dr_context *ctx,
					   struct mlx5dr_action_nat64_attr *attr,
					   uint32_t flags)
{
	__be64 modify_action_data[MLX5DR_ACTION_NAT64_MAX_MODIFY_ACTIONS];
	struct mlx5dr_action_mh_pattern pat[2];
	struct mlx5dr_action *action;
	uint32_t packet_len_field;
	uint32_t packet_len_add;
	uint8_t *action_ptr;
	uint32_t tos_field;
	uint32_t ttl_field;
	uint32_t tos_size;
	uint32_t src_addr;
	uint32_t dst_addr;
	bool is_v4_to_v6;
	uint32_t ecn;

	is_v4_to_v6 = attr->flags & MLX5DR_ACTION_NAT64_V4_TO_V6;

	if (is_v4_to_v6) {
		packet_len_field = MLX5_MODI_OUT_IPV6_PAYLOAD_LEN;
		 /* 2' comp to 20, to get -20 in add operation */
		packet_len_add = MLX5DR_ACTION_NAT64_DEC_20;
		ttl_field = MLX5_MODI_OUT_IPV6_HOPLIMIT;
		src_addr = MLX5_MODI_OUT_SIPV6_31_0;
		dst_addr = MLX5_MODI_OUT_DIPV6_31_0;
		tos_field = MLX5_MODI_OUT_IPV6_TRAFFIC_CLASS;
		tos_size = 8;
		ecn = 0;
	} else {
		packet_len_field = MLX5_MODI_OUT_IPV4_TOTAL_LEN;
		/* ipv4 len is including 20 bytes of the header, so add 20 over ipv6 len */
		packet_len_add = MLX5DR_ACTION_NAT64_ADD_20;
		ttl_field = MLX5_MODI_OUT_IPV4_TTL;
		src_addr = MLX5_MODI_OUT_SIPV4;
		dst_addr = MLX5_MODI_OUT_DIPV4;
		tos_field = MLX5_MODI_OUT_IP_DSCP;
		tos_size = 6;
		ecn = MLX5_MODI_OUT_IP_ECN;
	}

	memset(modify_action_data, 0, sizeof(modify_action_data));
	action_ptr = (uint8_t *)modify_action_data;

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_field,
		 packet_len_field);
	MLX5_SET(copy_action_in, action_ptr, src_offset, 32);
	MLX5_SET(copy_action_in, action_ptr, length, 16);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	MLX5_SET(set_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_SET);
	MLX5_SET(set_action_in, action_ptr, field, ttl_field);
	MLX5_SET(set_action_in, action_ptr, length, 8);
	MLX5_SET(set_action_in, action_ptr, data, MLX5DR_ACTION_NAT64_TTL_DEFAULT_VAL);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	/* copy TOS */
	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, action_ptr, src_field,
		 attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
	MLX5_SET(copy_action_in, action_ptr, dst_field, tos_field);
	MLX5_SET(copy_action_in, action_ptr, src_offset, 24 + (ecn ?
							       MLX5DR_ACTION_NAT64_ECN_SIZE : 0));
	MLX5_SET(copy_action_in, action_ptr, length, tos_size);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	if (ecn) {
		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field,
			attr->registers[MLX5DR_ACTION_NAT64_REG_CONTROL]);
		MLX5_SET(copy_action_in, action_ptr, dst_field, ecn);
		MLX5_SET(copy_action_in, action_ptr, src_offset, 24);
		MLX5_SET(copy_action_in, action_ptr, length, MLX5DR_ACTION_NAT64_ECN_SIZE);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}

	MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_NOP);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	/* if required Copy original addresses */
	if (attr->flags & MLX5DR_ACTION_NAT64_BACKUP_ADDR) {
		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field,
			 attr->registers[MLX5DR_ACTION_NAT64_REG_SRC_IP]);
		MLX5_SET(copy_action_in, action_ptr, dst_field, src_addr);
		MLX5_SET(copy_action_in, action_ptr, src_offset, 0);
		MLX5_SET(copy_action_in, action_ptr, length, 32);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

		MLX5_SET(copy_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, action_ptr, src_field,
			 attr->registers[MLX5DR_ACTION_NAT64_REG_DST_IP]);
		MLX5_SET(copy_action_in, action_ptr, dst_field, dst_addr);
		MLX5_SET(copy_action_in, action_ptr, src_offset, 0);
		MLX5_SET(copy_action_in, action_ptr, length, 32);
		action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;
	}

	/* take/add off 20 bytes ipv4/6 from/to the total size */
	MLX5_SET(set_action_in, action_ptr, action_type, MLX5_MODIFICATION_TYPE_ADD);
	MLX5_SET(set_action_in, action_ptr, field, packet_len_field);
	MLX5_SET(set_action_in, action_ptr, data, packet_len_add);
	MLX5_SET(set_action_in, action_ptr, length, 16);
	action_ptr += MLX5DR_ACTION_DOUBLE_SIZE;

	pat[0].data = modify_action_data;
	pat[0].sz = action_ptr - (uint8_t *)modify_action_data;

	action = mlx5dr_action_create_modify_header(ctx, 1, pat, 0, flags);
	if (!action) {
		DR_LOG(ERR, "Failed to create action: action_sz: %zu, flags: 0x%x\n",
		       pat[0].sz, flags);
		return NULL;
	}

	return action;
}

static void mlx5dr_action_print_combo(enum mlx5dr_action_type *user_actions)
{
	DR_LOG(ERR, "Invalid action_type sequence");
	while (*user_actions != MLX5DR_ACTION_TYP_LAST) {
		DR_LOG(ERR, "%s", mlx5dr_debug_action_type_to_str(*user_actions));
		user_actions++;
	}
}

static const uint32_t *
mlx5dr_action_get_order_entry(enum mlx5dr_table_type table_type)
{
	switch (table_type) {
	case MLX5DR_TABLE_TYPE_NIC_RX:
		return action_order_arr[MLX5DR_TABLE_TYPE_NIC_RX];
	case MLX5DR_TABLE_TYPE_NIC_TX:
		return action_order_arr[MLX5DR_TABLE_TYPE_NIC_TX];
	case MLX5DR_TABLE_TYPE_FDB:
	case MLX5DR_TABLE_TYPE_FDB_RX:
	case MLX5DR_TABLE_TYPE_FDB_TX:
	case MLX5DR_TABLE_TYPE_FDB_UNIFIED:
		return action_order_arr[MLX5DR_TABLE_TYPE_FDB];
	default:
		assert(0);
		DR_LOG(ERR, "no such type: %d", table_type);
		return NULL;
	}
}

bool mlx5dr_action_check_combo(enum mlx5dr_action_type *user_actions,
			       enum mlx5dr_table_type table_type)
{
	const uint32_t *order_arr = mlx5dr_action_get_order_entry(table_type);
	uint8_t order_idx = 0;
	uint8_t user_idx = 0;
	bool valid_combo;

	if (!order_arr)
		return false;

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
		case MLX5DR_ACTION_TYP_TBL:
			attr[i].type = MLX5DV_FLOW_ACTION_DEST_DEVX;
			attr[i].obj = action->dest_tbl.devx_obj->obj;
			break;
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
		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
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

static bool
mlx5dr_action_fixup_stc_attr(struct mlx5dr_context *ctx,
			     struct mlx5dr_cmd_stc_modify_attr *stc_attr,
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
		devx_obj = mlx5dr_action_get_stc_obj_by_tbl_type(table_type,
								 stc_attr->ste_table.ste_pool,
								 &stc_attr->ste_table.ste,
								 is_mirror);

		*fixup_stc_attr = *stc_attr;
		fixup_stc_attr->ste_table.ste_obj_id = devx_obj->id;
		use_fixup = true;
		break;

	case MLX5_IFC_STC_ACTION_TYPE_ALLOW:
		if (mlx5dr_table_is_fw_fdb_any(fw_tbl_type)) {
			fixup_stc_attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_VPORT;
			fixup_stc_attr->action_offset = stc_attr->action_offset;
			fixup_stc_attr->stc_offset = stc_attr->stc_offset;
			fixup_stc_attr->vport.esw_owner_vhca_id = ctx->caps->vhca_id;
			fixup_stc_attr->vport.vport_num = ctx->caps->eswitch_manager_vport_number;
			fixup_stc_attr->vport.eswitch_owner_vhca_id_valid =
				ctx->caps->merged_eswitch;
			use_fixup = true;
		}
		break;

	case MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_VPORT:
		if (stc_attr->vport.vport_num != WIRE_PORT)
			break;

		if (mlx5dr_table_is_fw_fdb_any(fw_tbl_type)) {
			/* The FW doesn't allow to go to wire in the TX/RX by JUMP_TO_VPORT */
			fixup_stc_attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_UPLINK;
			fixup_stc_attr->action_offset = stc_attr->action_offset;
			fixup_stc_attr->stc_offset = stc_attr->stc_offset;
			fixup_stc_attr->vport.vport_num = 0;
			fixup_stc_attr->vport.esw_owner_vhca_id = stc_attr->vport.esw_owner_vhca_id;
			fixup_stc_attr->vport.eswitch_owner_vhca_id_valid =
				stc_attr->vport.eswitch_owner_vhca_id_valid;
		}
		use_fixup = true;
		break;
	case MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_TIR:
		/* TIR is allowed on RX side, requires mask in case of FDB */
		if (fw_tbl_type == FS_FT_FDB_TX) {
			fixup_stc_attr->action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
			fixup_stc_attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
			fixup_stc_attr->stc_offset = stc_attr->stc_offset;
			use_fixup = true;
		}
		break;
	default:
		break;
	}

	return use_fixup;
}

static struct mlx5dr_devx_obj *
mlx5dr_action_get_stc_obj_by_tbl_type(enum mlx5dr_table_type table_type,
				      struct mlx5dr_pool *stc_pool,
				      struct mlx5dr_pool_chunk *stc,
				      bool is_mirror)
{
	if (table_type == MLX5DR_TABLE_TYPE_FDB_TX ||
	    (is_mirror && table_type == MLX5DR_TABLE_TYPE_FDB)) /* Optimized ORIG in FDB_TX */
		return mlx5dr_pool_chunk_get_base_devx_obj_mirror(stc_pool, stc);
	else
		return mlx5dr_pool_chunk_get_base_devx_obj(stc_pool, stc);
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
	enum mlx5dr_table_type type;
	bool use_fixup;
	int ret;

	ret = mlx5dr_pool_chunk_alloc(stc_pool, stc);
	if (ret) {
		DR_LOG(ERR, "Failed to allocate single action STC");
		return ret;
	}

	stc_attr->stc_offset = stc->offset;

	/* Dynamic reparse not supported, overwrite and use default */
	if (!mlx5dr_context_cap_dynamic_reparse(ctx))
		stc_attr->reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;

	type = (enum mlx5dr_table_type)table_type;
	devx_obj_0 = mlx5dr_action_get_stc_obj_by_tbl_type(type, stc_pool, stc, false);

	/* According to table/action limitation change the stc_attr */
	use_fixup = mlx5dr_action_fixup_stc_attr(ctx, stc_attr, &fixup_stc_attr,
						 (enum mlx5dr_table_type)table_type,
						 (table_type == MLX5DR_TABLE_TYPE_FDB_TX));
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

		use_fixup = mlx5dr_action_fixup_stc_attr(ctx, stc_attr,
							 &fixup_stc_attr,
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
	enum mlx5dr_table_type type;

	/* Modify the STC not to point to an object */
	stc_attr.action_type = MLX5_IFC_STC_ACTION_TYPE_DROP;
	stc_attr.action_offset = MLX5DR_ACTION_OFFSET_HIT;
	stc_attr.stc_offset = stc->offset;
	type = (enum mlx5dr_table_type)table_type;
	devx_obj = mlx5dr_action_get_stc_obj_by_tbl_type(type, stc_pool, stc, false);

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
	case MLX5_MODIFICATION_TYPE_ADD_FIELD:
		return MLX5_IFC_STC_ACTION_TYPE_ADD_FIELD;
	default:
		assert(false);
		DR_LOG(ERR, "Unsupported action type: 0x%x", action_type);
		rte_errno = ENOTSUP;
		return MLX5_IFC_STC_ACTION_TYPE_NOP;
	}
}

static void mlx5dr_action_fill_stc_attr(struct mlx5dr_action *action,
					struct mlx5dr_devx_obj *obj,
					struct mlx5dr_cmd_stc_modify_attr *attr)
{
	attr->reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;

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
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_MODIFY_HDR:
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;
		if (action->modify_header.require_reparse)
			attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;

		if (action->modify_header.num_of_actions == 1) {
			attr->modify_action.data = action->modify_header.single_action;
			attr->action_type = mlx5dr_action_get_mh_stc_type(attr->modify_action.data);

			if (attr->action_type == MLX5_IFC_STC_ACTION_TYPE_ADD ||
			    attr->action_type == MLX5_IFC_STC_ACTION_TYPE_SET)
				MLX5_SET(set_action_in, &attr->modify_action.data, data, 0);
		} else {
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_ACC_MODIFY_LIST;
			attr->modify_header.arg_id = action->modify_header.arg_obj->id;
			attr->modify_header.pattern_id = action->modify_header.pat_obj->id;
		}
		break;
	case MLX5DR_ACTION_TYP_TBL:
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_table_id = obj->id;
		/* Only for unified FDB Rx case */
		if (mlx5dr_context_cap_stc(action->ctx,
		    MLX5_IFC_STC_ACTION_TYPE_JUMP_FLOW_TABLE_FDB_RX_BIT_INDEX) &&
		    action->dest_tbl.type == MLX5DR_TABLE_TYPE_FDB_RX)
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FLOW_TABLE_FDB_RX;
		else
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;

		break;
	case MLX5DR_ACTION_TYP_DEST_ARRAY:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_table_id = obj->id;
		break;
	case MLX5DR_ACTION_TYP_DEST_ROOT:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_FT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->dest_table_id = action->root_tbl.sa->id;
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_REMOVE;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		attr->remove_header.decap = 1;
		attr->remove_header.start_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->remove_header.end_anchor = MLX5_HEADER_ANCHOR_INNER_MAC;
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
	case MLX5DR_ACTION_TYP_INSERT_HEADER:
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		if (!action->reformat.require_reparse)
			attr->reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;

		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_INSERT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->insert_header.encap = action->reformat.encap;
		attr->insert_header.push_esp = action->reformat.push_esp;
		attr->insert_header.insert_anchor = action->reformat.anchor;
		attr->insert_header.arg_id = action->reformat.arg_obj->id;
		attr->insert_header.header_size = action->reformat.header_size;
		attr->insert_header.insert_offset = action->reformat.offset;
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
		attr->vport.eswitch_owner_vhca_id_valid = action->ctx->caps->merged_eswitch;
		break;
	case MLX5DR_ACTION_TYP_POP_VLAN:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_REMOVE_WORDS;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		attr->remove_words.start_anchor = MLX5_HEADER_ANCHOR_FIRST_VLAN_START;
		attr->remove_words.num_of_words = MLX5DR_ACTION_HDR_LEN_L2_VLAN / 2;
		break;
	case MLX5DR_ACTION_TYP_PUSH_VLAN:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_INSERT;
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW6;
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		attr->insert_header.encap = 0;
		attr->insert_header.push_esp = 0;
		attr->insert_header.is_inline = 1;
		attr->insert_header.insert_anchor = MLX5_HEADER_ANCHOR_PACKET_START;
		attr->insert_header.insert_offset = MLX5DR_ACTION_HDR_LEN_L2_MACS;
		attr->insert_header.header_size = MLX5DR_ACTION_HDR_LEN_L2_VLAN;
		break;
	case MLX5DR_ACTION_TYP_REMOVE_HEADER:
		if (action->remove_header.type == MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_HEADER) {
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_HEADER_REMOVE;
			attr->remove_header.decap = action->remove_header.decap;
			attr->remove_header.start_anchor = action->remove_header.start_anchor;
			attr->remove_header.end_anchor = action->remove_header.end_anchor;
		} else {
			attr->action_type = MLX5_IFC_STC_ACTION_TYPE_REMOVE_WORDS;
			attr->remove_words.start_anchor = action->remove_header.start_anchor;
			attr->remove_words.num_of_words = action->remove_header.num_of_words;
		}
		attr->action_offset = MLX5DR_ACTION_OFFSET_DW5;
		attr->reparse_mode = MLX5_IFC_STC_REPARSE_ALWAYS;
		break;
	case MLX5DR_ACTION_TYP_JUMP_TO_MATCHER:
		attr->action_type = MLX5_IFC_STC_ACTION_TYPE_JUMP_TO_STE_TABLE;
		attr->action_offset = MLX5DR_ACTION_OFFSET_HIT;
		attr->ste_table.ste = action->jump_to_matcher.matcher->match_ste.ste;
		attr->ste_table.ste_pool = action->jump_to_matcher.matcher->match_ste.pool;
		attr->ste_table.match_definer_id = action->ctx->caps->trivial_match_definer;
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
			goto free_stcs_rx;
	}

	/* Allocate STC for FDB */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_FDB,
						     &action->stc[MLX5DR_TABLE_TYPE_FDB]);
		if (ret)
			goto free_stcs_tx;
	}

	/* Allocate STC for FDB-RX */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_FDB_RX,
						     &action->stc[MLX5DR_TABLE_TYPE_FDB_RX]);
		if (ret)
			goto free_stcs_fdb;
	}

	/* Allocate STC for FDB-TX */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_FDB_TX,
						     &action->stc[MLX5DR_TABLE_TYPE_FDB_TX]);
		if (ret)
			goto free_stcs_fdb_rx;
	}

	/* Allocate STC for FDB Unified */
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) {
		ret = mlx5dr_action_alloc_single_stc(ctx, &stc_attr,
						     MLX5DR_TABLE_TYPE_FDB_UNIFIED,
						     &action->stc[MLX5DR_TABLE_TYPE_FDB_UNIFIED]);
		if (ret)
			goto free_stcs_fdb_tx;
	}

	pthread_spin_unlock(&ctx->ctrl_lock);

	return 0;

free_stcs_fdb_tx:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB_TX,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB_TX]);
free_stcs_fdb_rx:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB_RX,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB_RX]);
free_stcs_fdb:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB]);
free_stcs_tx:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_TX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_NIC_TX,
					      &action->stc[MLX5DR_TABLE_TYPE_NIC_TX]);
free_stcs_rx:
	if (action->flags & MLX5DR_ACTION_FLAG_HWS_RX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_NIC_RX,
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

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_RX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB_RX,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB_RX]);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_TX)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB_TX,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB_TX]);

	if (action->flags & MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED)
		mlx5dr_action_free_single_stc(ctx, MLX5DR_TABLE_TYPE_FDB_UNIFIED,
					      &action->stc[MLX5DR_TABLE_TYPE_FDB_UNIFIED]);

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
			MLX5DR_ACTION_FLAG_HWS_FDB |
			MLX5DR_ACTION_FLAG_HWS_FDB_RX |
			MLX5DR_ACTION_FLAG_HWS_FDB_TX |
			MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED);
}

static struct mlx5dr_action *
mlx5dr_action_create_generic_bulk(struct mlx5dr_context *ctx,
				  uint32_t flags,
				  enum mlx5dr_action_type action_type,
				  uint8_t bulk_sz)
{
	struct mlx5dr_action *action;
	int i;

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

	action = simple_calloc(bulk_sz, sizeof(*action));
	if (!action) {
		DR_LOG(ERR, "Failed to allocate memory for action [%d]", action_type);
		rte_errno = ENOMEM;
		return NULL;
	}

	for (i = 0; i < bulk_sz; i++) {
		action[i].ctx = ctx;
		action[i].flags = flags;
		action[i].type = action_type;
	}

	return action;
}

static struct mlx5dr_action *
mlx5dr_action_create_generic(struct mlx5dr_context *ctx,
			     uint32_t flags,
			     enum mlx5dr_action_type action_type)
{
	return mlx5dr_action_create_generic_bulk(ctx, flags, action_type, 1);
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

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_TBL);
	if (!action)
		return NULL;

	action->dest_tbl.type = tbl->type;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (mlx5dr_context_shared_gvmi_used(ctx))
			action->dest_tbl.devx_obj = tbl->local_ft;
		else
			action->dest_tbl.devx_obj = tbl->ft;
	} else {
		action->dest_tbl.devx_obj = tbl->ft;

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

	if ((flags & (MLX5DR_ACTION_FLAG_ROOT_FDB |
		      MLX5DR_ACTION_FLAG_HWS_FDB_TX |
		      MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED)) ||
	    ((flags & (MLX5DR_ACTION_FLAG_HWS_FDB |
		       MLX5DR_ACTION_FLAG_HWS_FDB_RX)) &&
	    !ctx->caps->fdb_tir_stc)) {
		DR_LOG(ERR, "TIR action not support on FDB or FDB_TX or UNIFIED");
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

		action->devx_dest.devx_obj = cur_obj;
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

	if (flags & (MLX5DR_ACTION_FLAG_HWS_FDB_TX | MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED)) {
		DR_LOG(ERR, "TAG action not supported for UNIFIED or FDB_TX");
		rte_errno = ENOTSUP;
		return NULL;
	}

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

struct mlx5dr_action *
mlx5dr_action_create_last(struct mlx5dr_context *ctx,
			  uint32_t flags)
{
	return mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_LAST);
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
		DR_LOG(ERR, "Failed querying port %d", ib_port_num);
		return ret;
	}
	action->vport.vport_num = vport_caps.vport_num;
	action->vport.esw_owner_vhca_id = vport_caps.esw_owner_vhca_id;

	if (!ctx->caps->merged_eswitch &&
	    action->vport.esw_owner_vhca_id != ctx->caps->vhca_id) {
		DR_LOG(ERR, "Not merged-eswitch (%d), not allowed to send to other vhca_id (%d)",
		       ctx->caps->vhca_id, action->vport.esw_owner_vhca_id);
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed creating stc for port %d", ib_port_num);
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

	if (!(flags & (MLX5DR_ACTION_FLAG_HWS_FDB | MLX5DR_ACTION_FLAG_HWS_FDB_RX |
		       MLX5DR_ACTION_FLAG_HWS_FDB_TX |
		       MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED))) {
		DR_LOG(ERR, "Vport action is supported for FDB only");
		rte_errno = EINVAL;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_VPORT);
	if (!action)
		return NULL;

	ret = mlx5dr_action_create_dest_vport_hws(ctx, action, ib_port_num);
	if (ret) {
		DR_LOG(ERR, "Failed to create vport action HWS");
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
		DR_LOG(ERR, "Failed creating stc for push vlan");
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

	ret = mlx5dr_action_get_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DOUBLE_POP);
	if (ret) {
		DR_LOG(ERR, "Failed to create remove stc for reformat");
		goto free_action;
	}

	ret = mlx5dr_action_create_stcs(action, NULL);
	if (ret) {
		DR_LOG(ERR, "Failed creating stc for pop vlan");
		goto free_shared;
	}

	return action;

free_shared:
	mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DOUBLE_POP);
free_action:
	simple_free(action);
	return NULL;
}

static int
mlx5dr_action_conv_reformat_to_verbs(uint32_t action_type,
				     uint32_t *verb_reformat_type)
{
	switch (action_type) {
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2;
		return 0;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL;
		return 0;
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2;
		return 0;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		*verb_reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL;
		return 0;
	default:
		DR_LOG(ERR, "Invalid root reformat action type");
		rte_errno = EINVAL;
		return rte_errno;
	}
}

static int
mlx5dr_action_conv_root_flags_to_dv_ft(uint32_t flags,
				       enum mlx5dv_flow_table_type *ft_type)
{
	uint8_t is_rx, is_tx, is_fdb;

	is_rx = !!(flags & MLX5DR_ACTION_FLAG_ROOT_RX);
	is_tx = !!(flags & MLX5DR_ACTION_FLAG_ROOT_TX);
	is_fdb = !!(flags & MLX5DR_ACTION_FLAG_ROOT_FDB);

	if (is_rx + is_tx + is_fdb != 1) {
		DR_LOG(ERR, "Root action flags must be converted to a single ft type");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (is_rx) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	} else if (is_tx) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX;
#ifdef HAVE_MLX5DV_FLOW_MATCHER_FT_TYPE
	} else if (is_fdb) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
#endif
	} else {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	return 0;
}

static int
mlx5dr_action_conv_hws_flags_to_dv_ft(uint32_t flags,
				     enum mlx5dv_flow_table_type *ft_type)
{
	uint8_t is_rx, is_tx, is_fdb;

	is_rx = !!(flags & MLX5DR_ACTION_FLAG_HWS_RX);
	is_tx = !!(flags & MLX5DR_ACTION_FLAG_HWS_TX);
	is_fdb = !!(flags & (MLX5DR_ACTION_FLAG_HWS_FDB |
			     MLX5DR_ACTION_FLAG_HWS_FDB_RX |
			     MLX5DR_ACTION_FLAG_HWS_FDB_TX |
			     MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED));

	if (is_rx + is_tx + is_fdb != 1) {
		DR_LOG(ERR, "Action flags must be converted to a single ft type");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}

	if (is_rx) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	} else if (is_tx) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX;
#ifdef HAVE_MLX5DV_FLOW_MATCHER_FT_TYPE
	} else if (is_fdb) {
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
#endif
	} else {
		rte_errno = ENOTSUP;
		return -rte_errno;
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
	ret = mlx5dr_action_conv_root_flags_to_dv_ft(action->flags, &ft_type);
	if (ret)
		return ret;

	ret = mlx5dr_action_conv_reformat_to_verbs(action->type, &verb_reformat_type);
	if (ret)
		return rte_errno;

	/* Create the reformat type for root table */
	ibv_ctx = mlx5dr_context_get_local_ibv(action->ctx);
	action->flow_action =
		mlx5_glue->dv_create_flow_action_packet_reformat_root(ibv_ctx,
								      data_sz,
								      data,
								      verb_reformat_type,
								      ft_type);
	if (!action->flow_action) {
		DR_LOG(ERR, "Failed to create dv_create_flow reformat");
		rte_errno = errno;
		return rte_errno;
	}

	return 0;
}

static int
mlx5dr_action_handle_insert_with_ptr(struct mlx5dr_action *action,
				     uint8_t num_of_hdrs,
				     struct mlx5dr_action_reformat_header *hdrs,
				     uint32_t log_bulk_sz, uint32_t reparse)
{
	struct mlx5dr_devx_obj *arg_obj;
	size_t max_sz = 0;
	int ret, i;

	for (i = 0; i < num_of_hdrs; i++) {
		if (hdrs[i].sz % W_SIZE != 0) {
			DR_LOG(ERR, "Header data size should be in WORD granularity");
			rte_errno = EINVAL;
			return rte_errno;
		}
		max_sz = RTE_MAX(hdrs[i].sz, max_sz);
	}

	/* Allocate single shared arg object for all headers */
	arg_obj = mlx5dr_arg_create(action->ctx,
				    hdrs->data,
				    max_sz,
				    log_bulk_sz,
				    action->flags & MLX5DR_ACTION_FLAG_SHARED);
	if (!arg_obj)
		return rte_errno;

	for (i = 0; i < num_of_hdrs; i++) {
		action[i].reformat.arg_obj = arg_obj;
		action[i].reformat.header_size = hdrs[i].sz;
		action[i].reformat.num_of_hdrs = num_of_hdrs;
		action[i].reformat.max_hdr_sz = max_sz;

		if (action[i].type == MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2 ||
		    action[i].type == MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3) {
			action[i].reformat.anchor = MLX5_HEADER_ANCHOR_PACKET_START;
			action[i].reformat.offset = 0;
			action[i].reformat.encap = 1;
			action[i].reformat.push_esp = 0;
		}

		if (likely(reparse == MLX5DR_ACTION_STC_REPARSE_DEFAULT))
			action[i].reformat.require_reparse = true;
		else if (reparse == MLX5DR_ACTION_STC_REPARSE_ON)
			action[i].reformat.require_reparse = true;

		ret = mlx5dr_action_create_stcs(&action[i], NULL);
		if (ret) {
			DR_LOG(ERR, "Failed to create stc for reformat");
			goto free_stc;
		}
	}

	return 0;

free_stc:
	while (i--)
		mlx5dr_action_destroy_stcs(&action[i]);

	mlx5dr_cmd_destroy_obj(arg_obj);
	return ret;
}

static int
mlx5dr_action_handle_l2_to_tunnel_l3(struct mlx5dr_action *action,
				     uint8_t num_of_hdrs,
				     struct mlx5dr_action_reformat_header *hdrs,
				     uint32_t log_bulk_sz)
{
	int ret;

	/* The action is remove-l2-header + insert-l3-header */
	ret = mlx5dr_action_get_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP_L3);
	if (ret) {
		DR_LOG(ERR, "Failed to create remove stc for reformat");
		return ret;
	}

	/* Reuse the insert with pointer for the L2L3 header */
	ret = mlx5dr_action_handle_insert_with_ptr(action,
						   num_of_hdrs,
						   hdrs,
						   log_bulk_sz,
						   MLX5DR_ACTION_STC_REPARSE_DEFAULT);
	if (ret)
		goto put_shared_stc;

	return 0;

put_shared_stc:
	mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP_L3);
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
mlx5dr_action_handle_tunnel_l3_to_l2(struct mlx5dr_action *action,
				     uint8_t num_of_hdrs,
				     struct mlx5dr_action_reformat_header *hdrs,
				     uint32_t log_bulk_sz)
{
	uint8_t mh_data[MLX5DR_ACTION_REFORMAT_DATA_SIZE] = {0};
	struct mlx5dr_devx_obj *arg_obj, *pat_obj;
	struct mlx5dr_context *ctx = action->ctx;
	int num_of_actions;
	int mh_data_size;
	int ret, i;

	for (i = 0; i < num_of_hdrs; i++) {
		if (hdrs[i].sz != MLX5DR_ACTION_HDR_LEN_L2 &&
		    hdrs[i].sz != MLX5DR_ACTION_HDR_LEN_L2_W_VLAN) {
			DR_LOG(ERR, "Data size is not supported for decap-l3");
			rte_errno = EINVAL;
			return rte_errno;
		}
	}

	/* Create a full modify header action list in case shared */
	mlx5dr_action_prepare_decap_l3_actions(hdrs->sz, mh_data, &num_of_actions);

	if (action->flags & MLX5DR_ACTION_FLAG_SHARED)
		mlx5dr_action_prepare_decap_l3_data(hdrs->data, mh_data, num_of_actions);

	/* All DecapL3 cases require the same max arg size */
	arg_obj = mlx5dr_arg_create_modify_header_arg(ctx,
						      (__be64 *)mh_data,
						      num_of_actions,
						      log_bulk_sz,
						      action->flags & MLX5DR_ACTION_FLAG_SHARED);
	if (!arg_obj)
		return rte_errno;

	for (i = 0; i < num_of_hdrs; i++) {
		memset(mh_data, 0, MLX5DR_ACTION_REFORMAT_DATA_SIZE);
		mlx5dr_action_prepare_decap_l3_actions(hdrs[i].sz, mh_data, &num_of_actions);
		mh_data_size = num_of_actions * MLX5DR_MODIFY_ACTION_SIZE;

		pat_obj = mlx5dr_pat_get_pattern(ctx, (__be64 *)mh_data, mh_data_size);
		if (!pat_obj) {
			DR_LOG(ERR, "Failed to allocate pattern for DecapL3");
			goto free_stc_and_pat;
		}

		action[i].modify_header.max_num_of_actions = num_of_actions;
		action[i].modify_header.num_of_actions = num_of_actions;
		action[i].modify_header.num_of_patterns = num_of_hdrs;
		action[i].modify_header.arg_obj = arg_obj;
		action[i].modify_header.pat_obj = pat_obj;
		action[i].modify_header.require_reparse =
			mlx5dr_pat_require_reparse((__be64 *)mh_data, num_of_actions);

		ret = mlx5dr_action_create_stcs(&action[i], NULL);
		if (ret) {
			mlx5dr_pat_put_pattern(ctx, pat_obj);
			goto free_stc_and_pat;
		}
	}

	return 0;


free_stc_and_pat:
	while (i--) {
		mlx5dr_action_destroy_stcs(&action[i]);
		mlx5dr_pat_put_pattern(ctx, action[i].modify_header.pat_obj);
	}

	mlx5dr_cmd_destroy_obj(arg_obj);
	return 0;
}

static int
mlx5dr_action_create_reformat_hws(struct mlx5dr_action *action,
				  uint8_t num_of_hdrs,
				  struct mlx5dr_action_reformat_header *hdrs,
				  uint32_t bulk_size)
{
	int ret;

	switch (action->type) {
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		ret = mlx5dr_action_create_stcs(action, NULL);
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		ret = mlx5dr_action_handle_insert_with_ptr(action, num_of_hdrs, hdrs, bulk_size,
							   MLX5DR_ACTION_STC_REPARSE_DEFAULT);
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		ret = mlx5dr_action_handle_l2_to_tunnel_l3(action, num_of_hdrs, hdrs, bulk_size);
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
		ret = mlx5dr_action_handle_tunnel_l3_to_l2(action, num_of_hdrs, hdrs, bulk_size);
		break;
	default:
		DR_LOG(ERR, "Invalid HWS reformat action type");
		rte_errno = EINVAL;
		return rte_errno;
	}

	return ret;
}

struct mlx5dr_action *
mlx5dr_action_create_reformat(struct mlx5dr_context *ctx,
			      enum mlx5dr_action_type reformat_type,
			      uint8_t num_of_hdrs,
			      struct mlx5dr_action_reformat_header *hdrs,
			      uint32_t log_bulk_size,
			      uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (!num_of_hdrs) {
		DR_LOG(ERR, "Reformat num_of_hdrs cannot be zero");
		rte_errno = EINVAL;
		return NULL;
	}

	action = mlx5dr_action_create_generic_bulk(ctx, flags, reformat_type, num_of_hdrs);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (log_bulk_size) {
			DR_LOG(ERR, "Bulk reformat not supported over root");
			rte_errno = ENOTSUP;
			goto free_action;
		}

		ret = mlx5dr_action_create_reformat_root(action,
							 hdrs ? hdrs->sz : 0,
							 hdrs ? hdrs->data : NULL);
		if (ret) {
			DR_LOG(ERR, "Failed to create root reformat action");
			goto free_action;
		}

		return action;
	}

	if (!mlx5dr_action_is_hws_flags(flags) ||
	    ((flags & MLX5DR_ACTION_FLAG_SHARED) && (log_bulk_size || num_of_hdrs > 1))) {
		DR_LOG(ERR, "Reformat flags don't fit HWS (flags: 0x%x)", flags);
		rte_errno = EINVAL;
		goto free_action;
	}

	ret = mlx5dr_action_create_reformat_hws(action, num_of_hdrs, hdrs, log_bulk_size);
	if (ret) {
		DR_LOG(ERR, "Failed to create HWS reformat action");
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

	ret = mlx5dr_action_conv_root_flags_to_dv_ft(action->flags, &ft_type);
	if (ret)
		return ret;

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

static int
mlx5dr_action_create_modify_header_hws(struct mlx5dr_action *action,
				       uint8_t num_of_patterns,
				       struct mlx5dr_action_mh_pattern *pattern,
				       uint32_t log_bulk_size,
				       uint32_t reparse)
{
	struct mlx5dr_devx_obj *pat_obj, *arg_obj = NULL;
	struct mlx5dr_context *ctx = action->ctx;
	uint16_t num_actions, max_mh_actions = 0;
	int i, ret;

	/* Calculate maximum number of mh actions for shared arg allocation */
	for (i = 0; i < num_of_patterns; i++)
		max_mh_actions = RTE_MAX(max_mh_actions, pattern[i].sz / MLX5DR_MODIFY_ACTION_SIZE);

	/* Allocate single shared arg for all patterns based on the max size */
	if (max_mh_actions > 1) {
		arg_obj = mlx5dr_arg_create_modify_header_arg(ctx,
							      pattern->data,
							      max_mh_actions,
							      log_bulk_size,
							      action->flags &
							      MLX5DR_ACTION_FLAG_SHARED);
		if (!arg_obj)
			return rte_errno;
	}

	for (i = 0; i < num_of_patterns; i++) {
		if (!mlx5dr_pat_verify_actions(pattern[i].data, pattern[i].sz)) {
			DR_LOG(ERR, "Fail to verify pattern modify actions");
			rte_errno = EINVAL;
			goto free_stc_and_pat;
		}

		num_actions = pattern[i].sz / MLX5DR_MODIFY_ACTION_SIZE;
		action[i].modify_header.num_of_patterns = num_of_patterns;
		action[i].modify_header.max_num_of_actions = max_mh_actions;
		action[i].modify_header.num_of_actions = num_actions;

		if (likely(reparse == MLX5DR_ACTION_STC_REPARSE_DEFAULT))
			action[i].modify_header.require_reparse =
				mlx5dr_pat_require_reparse(pattern[i].data, num_actions);
		else if (reparse == MLX5DR_ACTION_STC_REPARSE_ON)
			action[i].modify_header.require_reparse = true;

		if (num_actions == 1) {
			pat_obj = NULL;
			/* Optimize single modify action to be used inline */
			action[i].modify_header.single_action = pattern[i].data[0];
			action[i].modify_header.single_action_type =
				MLX5_GET(set_action_in, pattern[i].data, action_type);
		} else {
			/* Multiple modify actions require a pattern */
			pat_obj = mlx5dr_pat_get_pattern(ctx, pattern[i].data, pattern[i].sz);
			if (!pat_obj) {
				DR_LOG(ERR, "Failed to allocate pattern for modify header");
				goto free_stc_and_pat;
			}

			action[i].modify_header.arg_obj = arg_obj;
			action[i].modify_header.pat_obj = pat_obj;
		}
		/* Allocate STC for each action representing a header */
		ret = mlx5dr_action_create_stcs(&action[i], NULL);
		if (ret) {
			if (pat_obj)
				mlx5dr_pat_put_pattern(ctx, pat_obj);
			goto free_stc_and_pat;
		}
	}

	return 0;

free_stc_and_pat:
	while (i--) {
		mlx5dr_action_destroy_stcs(&action[i]);
		if (action[i].modify_header.pat_obj)
			mlx5dr_pat_put_pattern(ctx, action[i].modify_header.pat_obj);
	}

	if (arg_obj)
		mlx5dr_cmd_destroy_obj(arg_obj);

	return rte_errno;
}

struct mlx5dr_action *
mlx5dr_action_create_modify_header_reparse(struct mlx5dr_context *ctx,
					   uint8_t num_of_patterns,
					   struct mlx5dr_action_mh_pattern *patterns,
					   uint32_t log_bulk_size,
					   uint32_t flags, uint32_t reparse)
{
	struct mlx5dr_action *action;
	int ret;

	if (!num_of_patterns) {
		DR_LOG(ERR, "Invalid number of patterns");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic_bulk(ctx, flags,
						   MLX5DR_ACTION_TYP_MODIFY_HDR,
						   num_of_patterns);
	if (!action)
		return NULL;

	if (mlx5dr_action_is_root_flags(flags)) {
		if (log_bulk_size) {
			DR_LOG(ERR, "Bulk modify-header not supported over root");
			rte_errno = ENOTSUP;
			goto free_action;
		}

		if (num_of_patterns != 1) {
			DR_LOG(ERR, "Only a single pattern supported over root");
			rte_errno = ENOTSUP;
			goto free_action;
		}

		ret = mlx5dr_action_create_modify_header_root(action,
							      patterns->sz,
							      patterns->data);
		if (ret)
			goto free_action;

		return action;
	}

	if ((flags & MLX5DR_ACTION_FLAG_SHARED) && (log_bulk_size || num_of_patterns > 1)) {
		DR_LOG(ERR, "Action cannot be shared with requested pattern or size");
		rte_errno = EINVAL;
		goto free_action;
	}

	ret = mlx5dr_action_create_modify_header_hws(action,
						     num_of_patterns,
						     patterns,
						     log_bulk_size,
						     reparse);
	if (ret)
		goto free_action;

	return action;

free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_modify_header(struct mlx5dr_context *ctx,
				   uint8_t num_of_patterns,
				   struct mlx5dr_action_mh_pattern *patterns,
				   uint32_t log_bulk_size,
				   uint32_t flags)
{
	return mlx5dr_action_create_modify_header_reparse(ctx, num_of_patterns, patterns,
							  log_bulk_size, flags,
							  MLX5DR_ACTION_STC_REPARSE_DEFAULT);
}
static struct mlx5dr_devx_obj *
mlx5dr_action_dest_array_process_reformat(struct mlx5dr_context *ctx,
					  enum mlx5dr_action_type type,
					  void *reformat_data,
					  size_t reformat_data_sz)
{
	struct mlx5dr_cmd_packet_reformat_create_attr pr_attr = {0};
	struct mlx5dr_devx_obj *reformat_devx_obj;

	if (!reformat_data || !reformat_data_sz) {
		DR_LOG(ERR, "Empty reformat action or data");
		rte_errno = EINVAL;
		return NULL;
	}

	switch (type) {
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		pr_attr.type = MLX5_PACKET_REFORMAT_CONTEXT_REFORMAT_TYPE_L2_TO_L2_TUNNEL;
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		pr_attr.type = MLX5_PACKET_REFORMAT_CONTEXT_REFORMAT_TYPE_L2_TO_L3_TUNNEL;
		break;
	default:
		DR_LOG(ERR, "Invalid value for reformat type");
		rte_errno = EINVAL;
		return NULL;
	}
	pr_attr.reformat_param_0 = 0;
	pr_attr.data_sz = reformat_data_sz;
	pr_attr.data = reformat_data;

	reformat_devx_obj = mlx5dr_cmd_packet_reformat_create(ctx->ibv_ctx, &pr_attr);
	if (!reformat_devx_obj)
		return NULL;

	return reformat_devx_obj;
}

struct mlx5dr_action *
mlx5dr_action_create_dest_array(struct mlx5dr_context *ctx,
				size_t num_dest,
				struct mlx5dr_action_dest_attr *dests,
				uint32_t flags)
{
	struct mlx5dr_cmd_set_fte_dest *dest_list = NULL;
	struct mlx5dr_devx_obj *packet_reformat = NULL;
	struct mlx5dr_cmd_ft_create_attr ft_attr = {0};
	struct mlx5dr_cmd_set_fte_attr fte_attr = {0};
	struct mlx5dr_cmd_forward_tbl *fw_island;
	enum mlx5dr_table_type table_type;
	struct mlx5dr_action *action;
	uint32_t i;
	int ret;

	if (num_dest <= 1) {
		rte_errno = EINVAL;
		DR_LOG(ERR, "Action must have multiple dests");
		return NULL;
	}

	if (!(flags & MLX5DR_ACTION_FLAG_SHARED)) {
		DR_LOG(ERR, "Action flags not supported, should include SHARED");
		rte_errno = ENOTSUP;
		return NULL;
	}

	flags = flags & ~MLX5DR_ACTION_FLAG_SHARED;

	if (flags == MLX5DR_ACTION_FLAG_HWS_RX) {
		ft_attr.type = FS_FT_NIC_RX;
		ft_attr.level = MLX5_IFC_MULTI_PATH_FT_MAX_LEVEL - 1;
		table_type = MLX5DR_TABLE_TYPE_NIC_RX;
	} else if (flags & (MLX5DR_ACTION_FLAG_HWS_FDB | MLX5DR_ACTION_FLAG_HWS_FDB_RX |
			    MLX5DR_ACTION_FLAG_HWS_FDB_TX |
			    MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED)) {
		ft_attr.type = FS_FT_FDB;
		ft_attr.level = ctx->caps->fdb_ft.max_level - 1;
		table_type = MLX5DR_TABLE_TYPE_FDB;
	} else {
		DR_LOG(ERR, "Action flags not supported");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (mlx5dr_context_shared_gvmi_used(ctx)) {
		DR_LOG(ERR, "Cannot use this action in shared GVMI context");
		rte_errno = ENOTSUP;
		return NULL;
	}

	dest_list = simple_calloc(num_dest, sizeof(*dest_list));
	if (!dest_list) {
		DR_LOG(ERR, "Failed to allocate memory for destinations");
		rte_errno = ENOMEM;
		return NULL;
	}

	for (i = 0; i < num_dest; i++) {
		enum mlx5dr_action_type *action_type = dests[i].action_type;

		if (!mlx5dr_action_check_combo(dests[i].action_type, table_type)) {
			DR_LOG(ERR, "Invalid combination of actions");
			rte_errno = EINVAL;
			goto free_dest_list;
		}

		for (; *action_type != MLX5DR_ACTION_TYP_LAST; action_type++) {
			switch (*action_type) {
			case MLX5DR_ACTION_TYP_TBL:
				dest_list[i].destination_type =
					MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
				dest_list[i].destination_id = dests[i].dest->dest_tbl.devx_obj->id;
				fte_attr.action_flags |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
				fte_attr.ignore_flow_level = 1;
				break;
			case MLX5DR_ACTION_TYP_MISS:
				if (!mlx5dr_table_is_fdb_any(table_type)) {
					DR_LOG(ERR, "Miss action supported for FDB only");
					rte_errno = ENOTSUP;
					goto free_dest_list;
				}
				dest_list[i].destination_type = MLX5_FLOW_DESTINATION_TYPE_VPORT;
				dest_list[i].destination_id =
					ctx->caps->eswitch_manager_vport_number;
				fte_attr.action_flags |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
				break;
			case MLX5DR_ACTION_TYP_VPORT:
				dest_list[i].destination_type = MLX5_FLOW_DESTINATION_TYPE_VPORT;
				dest_list[i].destination_id = dests[i].dest->vport.vport_num;
				fte_attr.action_flags |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
				if (ctx->caps->merged_eswitch) {
					dest_list[i].ext_flags |=
						MLX5DR_CMD_EXT_DEST_ESW_OWNER_VHCA_ID;
					dest_list[i].esw_owner_vhca_id =
						dests[i].dest->vport.esw_owner_vhca_id;
				}
				break;
			case MLX5DR_ACTION_TYP_TIR:
				dest_list[i].destination_type = MLX5_FLOW_DESTINATION_TYPE_TIR;
				dest_list[i].destination_id = dests[i].dest->devx_dest.devx_obj->id;
				fte_attr.action_flags |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
				break;
			case MLX5DR_ACTION_TYP_DROP:
				dest_list[i].destination_type = MLX5_FLOW_DESTINATION_TYPE_NOP;
				fte_attr.action_flags |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
				break;
			case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
			case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
				packet_reformat = mlx5dr_action_dest_array_process_reformat
							(ctx,
							 *action_type,
							 dests[i].reformat.reformat_data,
							 dests[i].reformat.reformat_data_sz);
				if (!packet_reformat)
					goto free_dest_list;

				dest_list[i].ext_flags |= MLX5DR_CMD_EXT_DEST_REFORMAT;
				dest_list[i].ext_reformat = packet_reformat;
				ft_attr.reformat_en = true;
				fte_attr.extended_dest = 1;
				break;
			default:
				DR_LOG(ERR, "Unsupported action in dest_array");
				rte_errno = ENOTSUP;
				goto free_dest_list;
			}
		}
	}
	fte_attr.dests_num = num_dest;
	fte_attr.dests = dest_list;

	fw_island = mlx5dr_cmd_forward_tbl_create(ctx->ibv_ctx, &ft_attr, &fte_attr);
	if (!fw_island)
		goto free_dest_list;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_DEST_ARRAY);
	if (!action)
		goto destroy_fw_island;

	ret = mlx5dr_action_create_stcs(action, fw_island->ft);
	if (ret)
		goto free_action;

	action->dest_array.fw_island = fw_island;
	action->dest_array.num_dest = num_dest;
	action->dest_array.dest_list = dest_list;

	return action;

free_action:
	simple_free(action);
destroy_fw_island:
	mlx5dr_cmd_forward_tbl_destroy(fw_island);
free_dest_list:
	for (i = 0; i < num_dest; i++) {
		if (dest_list[i].ext_reformat)
			mlx5dr_cmd_destroy_obj(dest_list[i].ext_reformat);
	}
	simple_free(dest_list);
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

	if (mlx5dr_action_conv_hws_flags_to_dv_ft(flags, &attr.ft_type))
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

static struct mlx5dr_action *
mlx5dr_action_create_insert_header_reparse(struct mlx5dr_context *ctx,
					   uint8_t num_of_hdrs,
					   struct mlx5dr_action_insert_header *hdrs,
					   uint32_t log_bulk_size,
					   uint32_t flags, uint32_t reparse)
{
	struct mlx5dr_action_reformat_header *reformat_hdrs;
	struct mlx5dr_action *action;
	int i, ret;

	if (!num_of_hdrs) {
		DR_LOG(ERR, "Reformat num_of_hdrs cannot be zero");
		rte_errno = EINVAL;
		return NULL;
	}

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Dynamic reformat action not supported over root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (!mlx5dr_action_is_hws_flags(flags) ||
	    ((flags & MLX5DR_ACTION_FLAG_SHARED) && (log_bulk_size || num_of_hdrs > 1))) {
		DR_LOG(ERR, "Reformat flags don't fit HWS (flags: 0x%x)", flags);
		rte_errno = EINVAL;
		return NULL;
	}

	action = mlx5dr_action_create_generic_bulk(ctx, flags,
						   MLX5DR_ACTION_TYP_INSERT_HEADER,
						   num_of_hdrs);
	if (!action)
		return NULL;

	reformat_hdrs = simple_calloc(num_of_hdrs, sizeof(*reformat_hdrs));
	if (!reformat_hdrs) {
		DR_LOG(ERR, "Failed to allocate memory for reformat_hdrs");
		rte_errno = ENOMEM;
		goto free_action;
	}

	for (i = 0; i < num_of_hdrs; i++) {
		if (hdrs[i].offset % W_SIZE != 0) {
			DR_LOG(ERR, "Header offset should be in WORD granularity");
			rte_errno = EINVAL;
			goto free_reformat_hdrs;
		}

		action[i].reformat.anchor = hdrs[i].anchor;
		action[i].reformat.encap = hdrs[i].encap;
		action[i].reformat.push_esp = hdrs[i].push_esp;
		action[i].reformat.offset = hdrs[i].offset;
		reformat_hdrs[i].sz = hdrs[i].hdr.sz;
		reformat_hdrs[i].data = hdrs[i].hdr.data;
	}

	ret = mlx5dr_action_handle_insert_with_ptr(action, num_of_hdrs,
						   reformat_hdrs, log_bulk_size,
						   reparse);
	if (ret) {
		DR_LOG(ERR, "Failed to create HWS reformat action");
		goto free_reformat_hdrs;
	}

	simple_free(reformat_hdrs);

	return action;

free_reformat_hdrs:
	simple_free(reformat_hdrs);
free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_insert_header(struct mlx5dr_context *ctx,
				   uint8_t num_of_hdrs,
				   struct mlx5dr_action_insert_header *hdrs,
				   uint32_t log_bulk_size,
				   uint32_t flags)
{
	return mlx5dr_action_create_insert_header_reparse(ctx, num_of_hdrs, hdrs,
							  log_bulk_size, flags,
							  MLX5DR_ACTION_STC_REPARSE_DEFAULT);
}

struct mlx5dr_action *
mlx5dr_action_create_remove_header(struct mlx5dr_context *ctx,
				   struct mlx5dr_action_remove_header_attr *attr,
				   uint32_t flags)
{
	struct mlx5dr_action *action;

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Remove header action not supported over root");
		rte_errno = ENOTSUP;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_REMOVE_HEADER);
	if (!action)
		return NULL;

	switch (attr->type) {
	case MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_HEADER:
		action->remove_header.type = MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_HEADER;
		action->remove_header.start_anchor = attr->by_anchor.start_anchor;
		action->remove_header.end_anchor = attr->by_anchor.end_anchor;
		action->remove_header.decap = attr->by_anchor.decap;
		break;
	case MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_OFFSET:
		if (attr->by_offset.size % W_SIZE != 0) {
			DR_LOG(ERR, "Invalid size, HW supports header remove in WORD granularity");
			rte_errno = EINVAL;
			goto free_action;
		}

		if (attr->by_offset.size > MLX5DR_ACTION_REMOVE_HEADER_MAX_SIZE) {
			DR_LOG(ERR, "Header removal size limited to %u bytes",
			       MLX5DR_ACTION_REMOVE_HEADER_MAX_SIZE);
			rte_errno = EINVAL;
			goto free_action;
		}

		action->remove_header.type = MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_OFFSET;
		action->remove_header.start_anchor = attr->by_offset.start_anchor;
		action->remove_header.num_of_words = attr->by_offset.size / W_SIZE;
		break;
	default:
		DR_LOG(ERR, "Unsupported remove header type %u", attr->type);
		rte_errno = ENOTSUP;
		goto free_action;
	}

	if (mlx5dr_action_create_stcs(action, NULL))
		goto free_action;

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static void *
mlx5dr_action_create_pop_ipv6_route_ext_mhdr1(struct mlx5dr_action *action)
{
	struct mlx5dr_action_mh_pattern pattern;
	__be64 cmd[3] = {0};
	uint16_t mod_id;

	mod_id = flow_hw_get_ipv6_route_ext_mod_id_from_ctx(action->ctx, 0);
	if (!mod_id) {
		rte_errno = EINVAL;
		return NULL;
	}

	/*
	 * Backup ipv6_route_ext.next_hdr to ipv6_route_ext.seg_left.
	 * Next_hdr will be copied to ipv6.protocol after pop done.
	 */
	MLX5_SET(copy_action_in, &cmd[0], action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, &cmd[0], length, 8);
	MLX5_SET(copy_action_in, &cmd[0], src_offset, 24);
	MLX5_SET(copy_action_in, &cmd[0], src_field, mod_id);
	MLX5_SET(copy_action_in, &cmd[0], dst_field, mod_id);

	/* Add nop between the continuous same modify field id */
	MLX5_SET(copy_action_in, &cmd[1], action_type, MLX5_MODIFICATION_TYPE_NOP);

	/* Clear next_hdr for right checksum */
	MLX5_SET(set_action_in, &cmd[2], action_type, MLX5_MODIFICATION_TYPE_SET);
	MLX5_SET(set_action_in, &cmd[2], length, 8);
	MLX5_SET(set_action_in, &cmd[2], offset, 24);
	MLX5_SET(set_action_in, &cmd[2], field, mod_id);

	pattern.data = cmd;
	pattern.sz = sizeof(cmd);

	return mlx5dr_action_create_modify_header_reparse(action->ctx, 1, &pattern, 0,
							  action->flags,
							  MLX5DR_ACTION_STC_REPARSE_ON);
}

static void *
mlx5dr_action_create_pop_ipv6_route_ext_mhdr2(struct mlx5dr_action *action)
{
	enum mlx5_modification_field field[MLX5_ST_SZ_DW(definer_hl_ipv6_addr)] = {
		MLX5_MODI_OUT_DIPV6_127_96,
		MLX5_MODI_OUT_DIPV6_95_64,
		MLX5_MODI_OUT_DIPV6_63_32,
		MLX5_MODI_OUT_DIPV6_31_0
	};
	struct mlx5dr_action_mh_pattern pattern;
	__be64 cmd[5] = {0};
	uint16_t mod_id;
	uint32_t i;

	/* Copy ipv6_route_ext[first_segment].dst_addr by flex parser to ipv6.dst_addr */
	for (i = 0; i < MLX5_ST_SZ_DW(definer_hl_ipv6_addr); i++) {
		mod_id = flow_hw_get_ipv6_route_ext_mod_id_from_ctx(action->ctx, i + 1);
		if (!mod_id) {
			rte_errno = EINVAL;
			return NULL;
		}

		MLX5_SET(copy_action_in, &cmd[i], action_type, MLX5_MODIFICATION_TYPE_COPY);
		MLX5_SET(copy_action_in, &cmd[i], dst_field, field[i]);
		MLX5_SET(copy_action_in, &cmd[i], src_field, mod_id);
	}

	mod_id = flow_hw_get_ipv6_route_ext_mod_id_from_ctx(action->ctx, 0);
	if (!mod_id) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Restore next_hdr from seg_left for flex parser identifying */
	MLX5_SET(copy_action_in, &cmd[4], action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, &cmd[4], length, 8);
	MLX5_SET(copy_action_in, &cmd[4], dst_offset, 24);
	MLX5_SET(copy_action_in, &cmd[4], src_field, mod_id);
	MLX5_SET(copy_action_in, &cmd[4], dst_field, mod_id);

	pattern.data = cmd;
	pattern.sz = sizeof(cmd);

	return mlx5dr_action_create_modify_header_reparse(action->ctx, 1, &pattern, 0,
							  action->flags,
							  MLX5DR_ACTION_STC_REPARSE_ON);
}

static void *
mlx5dr_action_create_pop_ipv6_route_ext_mhdr3(struct mlx5dr_action *action)
{
	uint8_t cmd[MLX5DR_MODIFY_ACTION_SIZE] = {0};
	struct mlx5dr_action_mh_pattern pattern;
	uint16_t mod_id;

	mod_id = flow_hw_get_ipv6_route_ext_mod_id_from_ctx(action->ctx, 0);
	if (!mod_id) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Copy ipv6_route_ext.next_hdr to ipv6.protocol */
	MLX5_SET(copy_action_in, cmd, action_type, MLX5_MODIFICATION_TYPE_COPY);
	MLX5_SET(copy_action_in, cmd, length, 8);
	MLX5_SET(copy_action_in, cmd, src_offset, 24);
	MLX5_SET(copy_action_in, cmd, src_field, mod_id);
	MLX5_SET(copy_action_in, cmd, dst_field, MLX5_MODI_OUT_IP_PROTOCOL);

	pattern.data = (__be64 *)cmd;
	pattern.sz = sizeof(cmd);

	return mlx5dr_action_create_modify_header_reparse(action->ctx, 1, &pattern, 0,
							  action->flags,
							  MLX5DR_ACTION_STC_REPARSE_OFF);
}

static int
mlx5dr_action_create_pop_ipv6_route_ext(struct mlx5dr_action *action)
{
	uint8_t anchor_id = flow_hw_get_ipv6_route_ext_anchor_from_ctx(action->ctx);
	struct mlx5dr_action_remove_header_attr hdr_attr;
	uint32_t i;

	if (!anchor_id) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	action->ipv6_route_ext.action[0] =
		mlx5dr_action_create_pop_ipv6_route_ext_mhdr1(action);
	action->ipv6_route_ext.action[1] =
		mlx5dr_action_create_pop_ipv6_route_ext_mhdr2(action);
	action->ipv6_route_ext.action[2] =
		mlx5dr_action_create_pop_ipv6_route_ext_mhdr3(action);

	hdr_attr.by_anchor.decap = 1;
	hdr_attr.by_anchor.start_anchor = anchor_id;
	hdr_attr.by_anchor.end_anchor = MLX5_HEADER_ANCHOR_TCP_UDP;
	hdr_attr.type = MLX5DR_ACTION_REMOVE_HEADER_TYPE_BY_HEADER;
	action->ipv6_route_ext.action[3] =
		mlx5dr_action_create_remove_header(action->ctx, &hdr_attr, action->flags);

	if (!action->ipv6_route_ext.action[0] || !action->ipv6_route_ext.action[1] ||
	    !action->ipv6_route_ext.action[2] || !action->ipv6_route_ext.action[3]) {
		DR_LOG(ERR, "Failed to create ipv6_route_ext pop subaction");
		goto err;
	}

	return 0;

err:
	for (i = 0; i < MLX5DR_ACTION_IPV6_EXT_MAX_SA; i++)
		if (action->ipv6_route_ext.action[i])
			mlx5dr_action_destroy(action->ipv6_route_ext.action[i]);

	return rte_errno;
}

static void *
mlx5dr_action_create_push_ipv6_route_ext_mhdr1(struct mlx5dr_action *action)
{
	uint8_t cmd[MLX5DR_MODIFY_ACTION_SIZE] = {0};
	struct mlx5dr_action_mh_pattern pattern;

	/* Set ipv6.protocol to IPPROTO_ROUTING */
	MLX5_SET(set_action_in, cmd, action_type, MLX5_MODIFICATION_TYPE_SET);
	MLX5_SET(set_action_in, cmd, length, 8);
	MLX5_SET(set_action_in, cmd, field, MLX5_MODI_OUT_IP_PROTOCOL);
	MLX5_SET(set_action_in, cmd, data, IPPROTO_ROUTING);

	pattern.data = (__be64 *)cmd;
	pattern.sz = sizeof(cmd);

	return mlx5dr_action_create_modify_header(action->ctx, 1, &pattern, 0,
						  action->flags | MLX5DR_ACTION_FLAG_SHARED);
}

static void *
mlx5dr_action_create_push_ipv6_route_ext_mhdr2(struct mlx5dr_action *action,
					       uint32_t bulk_size,
					       uint8_t *data)
{
	enum mlx5_modification_field field[MLX5_ST_SZ_DW(definer_hl_ipv6_addr)] = {
		MLX5_MODI_OUT_DIPV6_127_96,
		MLX5_MODI_OUT_DIPV6_95_64,
		MLX5_MODI_OUT_DIPV6_63_32,
		MLX5_MODI_OUT_DIPV6_31_0
	};
	struct mlx5dr_action_mh_pattern pattern;
	uint32_t *ipv6_dst_addr = NULL;
	uint8_t seg_left, next_hdr;
	__be64 cmd[5] = {0};
	uint16_t mod_id;
	uint32_t i;

	/* Fetch the last IPv6 address in the segment list */
	if (action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		seg_left = MLX5_GET(header_ipv6_routing_ext, data, segments_left) - 1;
		ipv6_dst_addr = (uint32_t *)data + MLX5_ST_SZ_DW(header_ipv6_routing_ext) +
				seg_left * MLX5_ST_SZ_DW(definer_hl_ipv6_addr);
	}

	/* Copy IPv6 destination address from ipv6_route_ext.last_segment */
	for (i = 0; i < MLX5_ST_SZ_DW(definer_hl_ipv6_addr); i++) {
		MLX5_SET(set_action_in, &cmd[i], action_type, MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, &cmd[i], field, field[i]);
		if (action->flags & MLX5DR_ACTION_FLAG_SHARED)
			MLX5_SET(set_action_in, &cmd[i], data, be32toh(*ipv6_dst_addr++));
	}

	mod_id = flow_hw_get_ipv6_route_ext_mod_id_from_ctx(action->ctx, 0);
	if (!mod_id) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* Set ipv6_route_ext.next_hdr since initially pushed as 0 for right checksum */
	MLX5_SET(set_action_in, &cmd[4], action_type, MLX5_MODIFICATION_TYPE_SET);
	MLX5_SET(set_action_in, &cmd[4], length, 8);
	MLX5_SET(set_action_in, &cmd[4], offset, 24);
	MLX5_SET(set_action_in, &cmd[4], field, mod_id);
	if (action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		next_hdr = MLX5_GET(header_ipv6_routing_ext, data, next_hdr);
		MLX5_SET(set_action_in, &cmd[4], data, next_hdr);
	}

	pattern.data = cmd;
	pattern.sz = sizeof(cmd);

	return mlx5dr_action_create_modify_header(action->ctx, 1, &pattern,
						  bulk_size, action->flags);
}

static int
mlx5dr_action_create_push_ipv6_route_ext(struct mlx5dr_action *action,
					 struct mlx5dr_action_reformat_header *hdr,
					 uint32_t bulk_size)
{
	struct mlx5dr_action_insert_header insert_hdr = { {0} };
	uint8_t header[MLX5_PUSH_MAX_LEN];
	uint32_t i;

	if (!hdr || !hdr->sz || hdr->sz > MLX5_PUSH_MAX_LEN ||
	    ((action->flags & MLX5DR_ACTION_FLAG_SHARED) && !hdr->data)) {
		DR_LOG(ERR, "Invalid ipv6_route_ext header");
		rte_errno = EINVAL;
		return rte_errno;
	}

	if (action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		memcpy(header, hdr->data, hdr->sz);
		/* Clear ipv6_route_ext.next_hdr for right checksum */
		MLX5_SET(header_ipv6_routing_ext, header, next_hdr, 0);
	}

	insert_hdr.anchor = MLX5_HEADER_ANCHOR_TCP_UDP;
	insert_hdr.encap = 1;
	insert_hdr.hdr.sz = hdr->sz;
	insert_hdr.hdr.data = header;
	action->ipv6_route_ext.action[0] =
		mlx5dr_action_create_insert_header_reparse(action->ctx, 1, &insert_hdr,
							    bulk_size, action->flags,
							    MLX5DR_ACTION_STC_REPARSE_OFF);
	action->ipv6_route_ext.action[1] =
		mlx5dr_action_create_push_ipv6_route_ext_mhdr1(action);
	action->ipv6_route_ext.action[2] =
		mlx5dr_action_create_push_ipv6_route_ext_mhdr2(action, bulk_size, hdr->data);

	if (!action->ipv6_route_ext.action[0] ||
	    !action->ipv6_route_ext.action[1] ||
	    !action->ipv6_route_ext.action[2]) {
		DR_LOG(ERR, "Failed to create ipv6_route_ext push subaction");
		goto err;
	}

	return 0;

err:
	for (i = 0; i < MLX5DR_ACTION_IPV6_EXT_MAX_SA; i++)
		if (action->ipv6_route_ext.action[i])
			mlx5dr_action_destroy(action->ipv6_route_ext.action[i]);

	return rte_errno;
}

struct mlx5dr_action *
mlx5dr_action_create_reformat_ipv6_ext(struct mlx5dr_context *ctx,
				       enum mlx5dr_action_type action_type,
				       struct mlx5dr_action_reformat_header *hdr,
				       uint32_t log_bulk_size,
				       uint32_t flags)
{
	struct mlx5dr_action *action;
	int ret;

	if (!mlx5dr_action_is_hws_flags(flags) ||
	    ((flags & MLX5DR_ACTION_FLAG_SHARED) && log_bulk_size)) {
		DR_LOG(ERR, "IPv6 extension flags don't fit HWS (flags: 0x%x)", flags);
		rte_errno = EINVAL;
		return NULL;
	}

	action = mlx5dr_action_create_generic(ctx, flags, action_type);
	if (!action) {
		rte_errno = ENOMEM;
		return NULL;
	}

	switch (action_type) {
	case MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT:
		if (!(flags & MLX5DR_ACTION_FLAG_SHARED)) {
			DR_LOG(ERR, "Pop ipv6_route_ext must be shared");
			rte_errno = EINVAL;
			goto free_action;
		}

		ret = mlx5dr_action_create_pop_ipv6_route_ext(action);
		break;
	case MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT:
		if (!mlx5dr_context_cap_dynamic_reparse(ctx)) {
			DR_LOG(ERR, "IPv6 routing extension push actions is not supported");
			rte_errno = ENOTSUP;
			goto free_action;
		}

		ret = mlx5dr_action_create_push_ipv6_route_ext(action, hdr, log_bulk_size);
		break;
	default:
		DR_LOG(ERR, "Unsupported action type %d\n", action_type);
		rte_errno = ENOTSUP;
		goto free_action;
	}

	if (ret) {
		DR_LOG(ERR, "Failed to create IPv6 extension reformat action");
		goto free_action;
	}

	return action;

free_action:
	simple_free(action);
	return NULL;
}

static bool
mlx5dr_action_nat64_validate_param(struct mlx5dr_action_nat64_attr *attr,
				   uint32_t flags)
{
	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Nat64 action not supported for root");
		rte_errno = ENOTSUP;
		return false;
	}

	if (!(flags & MLX5DR_ACTION_FLAG_SHARED)) {
		DR_LOG(ERR, "Nat64 action must be with SHARED flag");
		rte_errno = EINVAL;
		return false;
	}

	if (attr->num_of_registers > MLX5DR_ACTION_NAT64_REG_MAX) {
		DR_LOG(ERR, "Nat64 action doesn't support more than %d registers",
		       MLX5DR_ACTION_NAT64_REG_MAX);
		rte_errno = EINVAL;
		return false;
	}

	if (attr->flags & MLX5DR_ACTION_NAT64_BACKUP_ADDR &&
	    attr->num_of_registers != MLX5DR_ACTION_NAT64_REG_MAX) {
		DR_LOG(ERR, "Nat64 backup addr requires %d registers",
		       MLX5DR_ACTION_NAT64_REG_MAX);
		rte_errno = EINVAL;
		return false;
	}

	if (!(attr->flags & MLX5DR_ACTION_NAT64_V4_TO_V6 ||
	      attr->flags & MLX5DR_ACTION_NAT64_V6_TO_V4)) {
		DR_LOG(ERR, "Nat64 backup addr requires one mode at least");
		rte_errno = EINVAL;
		return false;
	}

	return true;
}

struct mlx5dr_action *
mlx5dr_action_create_nat64(struct mlx5dr_context *ctx,
			   struct mlx5dr_action_nat64_attr *attr,
			   uint32_t flags)
{
	struct mlx5dr_action *action;

	if (!mlx5dr_action_nat64_validate_param(attr, flags))
		return NULL;

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_NAT64);
	if (!action)
		return NULL;

	action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY] =
		mlx5dr_action_create_nat64_copy_state(ctx, attr, flags);
	if (!action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY]) {
		DR_LOG(ERR, "Nat64 failed creating copy state");
		goto free_action;
	}

	action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_REPLACE] =
		mlx5dr_action_create_nat64_repalce_state(ctx, attr, flags);
	if (!action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_REPLACE]) {
		DR_LOG(ERR, "Nat64 failed creating replace state");
		goto free_copy;
	}
	action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY_PROTOCOL] =
		mlx5dr_action_create_nat64_copy_proto_state(ctx, attr, flags);
	if (!action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY_PROTOCOL]) {
		DR_LOG(ERR, "Nat64 failed creating copy protocol state");
		goto free_replace;
	}

	action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPYBACK] =
		mlx5dr_action_create_nat64_copy_back_state(ctx, attr, flags);
	if (!action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPYBACK]) {
		DR_LOG(ERR, "Nat64 failed creating copyback state");
		goto free_copy_proto;
	}

	return action;

free_copy_proto:
	mlx5dr_action_destroy(action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY_PROTOCOL]);
free_replace:
	mlx5dr_action_destroy(action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_REPLACE]);
free_copy:
	mlx5dr_action_destroy(action->nat64.stages[MLX5DR_ACTION_NAT64_STAGE_COPY]);
free_action:
	simple_free(action);
	return NULL;
}

struct mlx5dr_action *
mlx5dr_action_create_jump_to_matcher(struct mlx5dr_context *ctx,
				     struct mlx5dr_action_jump_to_matcher_attr *attr,
				     uint32_t flags)
{
	struct mlx5dr_matcher *matcher = attr->matcher;
	struct mlx5dr_matcher_attr *m_attr;
	struct mlx5dr_action *action;

	if (attr->type != MLX5DR_ACTION_JUMP_TO_MATCHER_BY_INDEX) {
		DR_LOG(ERR, "Only jump to matcher by index is supported");
		goto enotsup;
	}

	if (mlx5dr_action_is_root_flags(flags)) {
		DR_LOG(ERR, "Action flags must be only non root (HWS)");
		goto enotsup;
	}

	if (mlx5dr_table_is_root(matcher->tbl)) {
		DR_LOG(ERR, "Root matcher cannot be set as destination");
		goto enotsup;
	}

	m_attr = &matcher->attr;

	if (!(matcher->flags & MLX5DR_MATCHER_FLAGS_STE_ARRAY) &&
	    (m_attr->resizable || m_attr->table.sz_col_log || m_attr->table.sz_row_log)) {
		DR_LOG(ERR, "Only STE array or matcher of size 1 can be set as destination");
		goto enotsup;
	}

	action = mlx5dr_action_create_generic(ctx, flags, MLX5DR_ACTION_TYP_JUMP_TO_MATCHER);
	if (!action)
		return NULL;

	action->jump_to_matcher.matcher = matcher;

	if (mlx5dr_action_create_stcs(action, NULL)) {
		DR_LOG(ERR, "Failed to create action jump to matcher STC");
		simple_free(action);
		return NULL;
	}

	return action;

enotsup:
	rte_errno = ENOTSUP;
	return NULL;
}

static void mlx5dr_action_destroy_hws(struct mlx5dr_action *action)
{
	struct mlx5dr_devx_obj *obj = NULL;
	uint32_t i;

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
	case MLX5DR_ACTION_TYP_TBL:
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
	case MLX5DR_ACTION_TYP_ASO_METER:
	case MLX5DR_ACTION_TYP_ASO_CT:
	case MLX5DR_ACTION_TYP_PUSH_VLAN:
	case MLX5DR_ACTION_TYP_REMOVE_HEADER:
	case MLX5DR_ACTION_TYP_VPORT:
	case MLX5DR_ACTION_TYP_JUMP_TO_MATCHER:
		mlx5dr_action_destroy_stcs(action);
		break;
	case MLX5DR_ACTION_TYP_DEST_ROOT:
		mlx5dr_action_destroy_stcs(action);
		mlx5_glue->destroy_steering_anchor(action->root_tbl.sa);
		break;
	case MLX5DR_ACTION_TYP_POP_VLAN:
		mlx5dr_action_destroy_stcs(action);
		mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DOUBLE_POP);
		break;
	case MLX5DR_ACTION_TYP_DEST_ARRAY:
		mlx5dr_action_destroy_stcs(action);
		mlx5dr_cmd_forward_tbl_destroy(action->dest_array.fw_island);
		for (i = 0; i < action->dest_array.num_dest; i++) {
			if (action->dest_array.dest_list[i].ext_reformat)
				mlx5dr_cmd_destroy_obj
					(action->dest_array.dest_list[i].ext_reformat);
		}
		simple_free(action->dest_array.dest_list);
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_MODIFY_HDR:
		for (i = 0; i < action->modify_header.num_of_patterns; i++) {
			mlx5dr_action_destroy_stcs(&action[i]);
			if (action[i].modify_header.num_of_actions > 1) {
				mlx5dr_pat_put_pattern(action[i].ctx,
						       action[i].modify_header.pat_obj);
				/* Save shared arg object if was used to free */
				if (action[i].modify_header.arg_obj)
					obj = action[i].modify_header.arg_obj;
			}
		}
		if (obj)
			mlx5dr_cmd_destroy_obj(obj);
		break;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		mlx5dr_action_put_shared_stc(action, MLX5DR_CONTEXT_SHARED_STC_DECAP_L3);
		for (i = 0; i < action->reformat.num_of_hdrs; i++)
			mlx5dr_action_destroy_stcs(&action[i]);
		mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
		break;
	case MLX5DR_ACTION_TYP_INSERT_HEADER:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		for (i = 0; i < action->reformat.num_of_hdrs; i++)
			mlx5dr_action_destroy_stcs(&action[i]);
		mlx5dr_cmd_destroy_obj(action->reformat.arg_obj);
		break;
	case MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT:
	case MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT:
		for (i = 0; i < MLX5DR_ACTION_IPV6_EXT_MAX_SA; i++)
			if (action->ipv6_route_ext.action[i])
				mlx5dr_action_destroy(action->ipv6_route_ext.action[i]);
		break;
	case MLX5DR_ACTION_TYP_NAT64:
		for (i = 0; i < MLX5DR_ACTION_NAT64_STAGES; i++)
			mlx5dr_action_destroy(action->nat64.stages[i]);
		break;
	case MLX5DR_ACTION_TYP_LAST:
		break;
	default:
		DR_LOG(ERR, "Not supported action type: %d", action->type);
		assert(false);
	}
}

static void mlx5dr_action_destroy_root(struct mlx5dr_action *action)
{
	switch (action->type) {
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
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
	stc_attr.reparse_mode = MLX5_IFC_STC_REPARSE_IGNORE;
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

static int mlx5dr_action_get_shared_stc_offset(struct mlx5dr_context_common_res *common_res,
					       enum mlx5dr_context_shared_stc_type stc_type)
{
	return common_res->shared_stc[stc_type]->remove_header.offset;
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
	uint32_t stc_idx, arg_sz, arg_idx;
	struct mlx5dr_action *action;
	uint8_t *single_action;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action + rule_action->modify_header.pattern_idx;

	stc_idx = htobe32(action->stc[apply->tbl_type].offset);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] = stc_idx;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;

	if (action->modify_header.num_of_actions == 1) {
		if (action->modify_header.single_action_type ==
		    MLX5_MODIFICATION_TYPE_COPY ||
		    action->modify_header.single_action_type ==
		    MLX5_MODIFICATION_TYPE_ADD_FIELD) {
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
		arg_sz = mlx5dr_arg_get_arg_size(action->modify_header.max_num_of_actions);
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
mlx5dr_action_setter_nat64(struct mlx5dr_actions_apply_data *apply,
			   struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	struct mlx5dr_action *cur_stage_action;
	struct mlx5dr_action *action;
	uint32_t stc_idx;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action;
	cur_stage_action = action->nat64.stages[setter->stage_idx];

	stc_idx = htobe32(cur_stage_action->stc[apply->tbl_type].offset);

	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] = stc_idx;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = 0;
}

static void
mlx5dr_action_setter_insert_ptr(struct mlx5dr_actions_apply_data *apply,
				struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	uint32_t stc_idx, arg_idx, arg_sz;
	struct mlx5dr_action *action;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action + rule_action->reformat.hdr_idx;

	/* Argument offset multiple on args required for header size */
	arg_sz = mlx5dr_arg_data_size_to_arg_size(action->reformat.max_hdr_sz);
	arg_idx = rule_action->reformat.offset * arg_sz;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(arg_idx);

	stc_idx = htobe32(action->stc[apply->tbl_type].offset);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] = stc_idx;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW7] = 0;

	if (!(action->flags & MLX5DR_ACTION_FLAG_SHARED)) {
		apply->require_dep = 1;
		mlx5dr_arg_write(apply->queue, NULL,
				 action->reformat.arg_obj->id + arg_idx,
				 rule_action->reformat.data,
				 action->reformat.header_size);
	}
}

static void
mlx5dr_action_setter_tnl_l3_to_l2(struct mlx5dr_actions_apply_data *apply,
				  struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;
	uint32_t stc_idx, arg_sz, arg_idx;
	struct mlx5dr_action *action;

	rule_action = &apply->rule_action[setter->idx_double];
	action = rule_action->action + rule_action->reformat.hdr_idx;

	/* Argument offset multiple on args required for num of actions */
	arg_sz = mlx5dr_arg_get_arg_size(action->modify_header.max_num_of_actions);
	arg_idx = rule_action->reformat.offset * arg_sz;

	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW6] = 0;
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW7] = htobe32(arg_idx);

	stc_idx = htobe32(action->stc[apply->tbl_type].offset);
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW6] = stc_idx;
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
							    MLX5DR_CONTEXT_SHARED_STC_DOUBLE_POP));
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
mlx5dr_action_setter_hit_matcher(struct mlx5dr_actions_apply_data *apply,
				 struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action;

	rule_action = &apply->rule_action[setter->idx_hit];

	apply->wqe_data[MLX5DR_ACTION_OFFSET_HIT_LSB] =
		htobe32(rule_action->jump_to_matcher.offset << 6);
	mlx5dr_action_apply_stc(apply, MLX5DR_ACTION_STC_IDX_HIT, setter->idx_hit);
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
							    MLX5DR_CONTEXT_SHARED_STC_DECAP_L3));
}

static void
mlx5dr_action_setter_ipv6_route_ext_gen_push_mhdr(uint8_t *data, void *mh_data)
{
	uint8_t *action_ptr = mh_data;
	uint32_t *ipv6_dst_addr;
	uint8_t seg_left;
	uint32_t i;

	/* Fetch the last IPv6 address in the segment list which is the next hop */
	seg_left = MLX5_GET(header_ipv6_routing_ext, data, segments_left) - 1;
	ipv6_dst_addr = (uint32_t *)data + MLX5_ST_SZ_DW(header_ipv6_routing_ext)
			+ seg_left * MLX5_ST_SZ_DW(definer_hl_ipv6_addr);

	/* Load next hop IPv6 address in reverse order to ipv6.dst_address */
	for (i = 0; i < MLX5_ST_SZ_DW(definer_hl_ipv6_addr); i++) {
		MLX5_SET(set_action_in, action_ptr, data, be32toh(*ipv6_dst_addr++));
		action_ptr += MLX5DR_MODIFY_ACTION_SIZE;
	}

	/* Set ipv6_route_ext.next_hdr per user input */
	MLX5_SET(set_action_in, action_ptr, data, *data);
}

static void
mlx5dr_action_setter_ipv6_route_ext_mhdr(struct mlx5dr_actions_apply_data *apply,
					 struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action = apply->rule_action;
	struct mlx5dr_actions_wqe_setter tmp_setter = {0};
	struct mlx5dr_rule_action tmp_rule_action;
	__be64 cmd[MLX5_SRV6_SAMPLE_NUM] = {0};
	struct mlx5dr_action *ipv6_ext_action;
	uint8_t *header;

	header = rule_action[setter->idx_double].ipv6_ext.header;
	ipv6_ext_action = rule_action[setter->idx_double].action;
	tmp_rule_action.action = ipv6_ext_action->ipv6_route_ext.action[setter->extra_data];

	if (tmp_rule_action.action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		tmp_rule_action.modify_header.offset = 0;
		tmp_rule_action.modify_header.pattern_idx = 0;
		tmp_rule_action.modify_header.data = NULL;
	} else {
		/*
		 * Copy ipv6_dst from ipv6_route_ext.last_seg.
		 * Set ipv6_route_ext.next_hdr.
		 */
		mlx5dr_action_setter_ipv6_route_ext_gen_push_mhdr(header, cmd);
		tmp_rule_action.modify_header.data = (uint8_t *)cmd;
		tmp_rule_action.modify_header.pattern_idx = 0;
		tmp_rule_action.modify_header.offset =
			rule_action[setter->idx_double].ipv6_ext.offset;
	}

	apply->rule_action = &tmp_rule_action;

	/* Reuse regular */
	mlx5dr_action_setter_modify_header(apply, &tmp_setter);

	/* Swap rule actions from backup */
	apply->rule_action = rule_action;
}

static void
mlx5dr_action_setter_ipv6_route_ext_insert_ptr(struct mlx5dr_actions_apply_data *apply,
					       struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action = apply->rule_action;
	struct mlx5dr_actions_wqe_setter tmp_setter = {0};
	struct mlx5dr_rule_action tmp_rule_action;
	struct mlx5dr_action *ipv6_ext_action;
	uint8_t header[MLX5_PUSH_MAX_LEN];

	ipv6_ext_action = rule_action[setter->idx_double].action;
	tmp_rule_action.action = ipv6_ext_action->ipv6_route_ext.action[setter->extra_data];

	if (tmp_rule_action.action->flags & MLX5DR_ACTION_FLAG_SHARED) {
		tmp_rule_action.reformat.offset = 0;
		tmp_rule_action.reformat.hdr_idx = 0;
		tmp_rule_action.reformat.data = NULL;
	} else {
		memcpy(header, rule_action[setter->idx_double].ipv6_ext.header,
		       tmp_rule_action.action->reformat.header_size);
		/* Clear ipv6_route_ext.next_hdr for right checksum */
		MLX5_SET(header_ipv6_routing_ext, header, next_hdr, 0);
		tmp_rule_action.reformat.data = header;
		tmp_rule_action.reformat.hdr_idx = 0;
		tmp_rule_action.reformat.offset =
			rule_action[setter->idx_double].ipv6_ext.offset;
	}

	apply->rule_action = &tmp_rule_action;

	/* Reuse regular */
	mlx5dr_action_setter_insert_ptr(apply, &tmp_setter);

	/* Swap rule actions from backup */
	apply->rule_action = rule_action;
}

static void
mlx5dr_action_setter_ipv6_route_ext_pop(struct mlx5dr_actions_apply_data *apply,
					struct mlx5dr_actions_wqe_setter *setter)
{
	struct mlx5dr_rule_action *rule_action = &apply->rule_action[setter->idx_single];
	uint8_t idx = MLX5DR_ACTION_IPV6_EXT_MAX_SA - 1;
	struct mlx5dr_action *action;

	/* Pop the ipv6_route_ext as set_single logic */
	action = rule_action->action->ipv6_route_ext.action[idx];
	apply->wqe_data[MLX5DR_ACTION_OFFSET_DW5] = 0;
	apply->wqe_ctrl->stc_ix[MLX5DR_ACTION_STC_IDX_DW5] =
		htobe32(action->stc[apply->tbl_type].offset);
}

int mlx5dr_action_template_process(struct mlx5dr_action_template *at)
{
	struct mlx5dr_actions_wqe_setter *start_setter = at->setters + 1;
	enum mlx5dr_action_type *action_type = at->action_type_arr;
	struct mlx5dr_actions_wqe_setter *setter = at->setters;
	struct mlx5dr_actions_wqe_setter *pop_setter = NULL;
	struct mlx5dr_actions_wqe_setter *last_setter;
	int i, j;

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
		case MLX5DR_ACTION_TYP_TBL:
		case MLX5DR_ACTION_TYP_DEST_ROOT:
		case MLX5DR_ACTION_TYP_DEST_ARRAY:
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
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_SINGLE1 | ASF_MODIFY | ASF_INSERT);
			setter->flags |= ASF_SINGLE1 | ASF_REMOVE;
			setter->set_single = &mlx5dr_action_setter_single;
			setter->idx_single = i;
			pop_setter = setter;
			break;

		case MLX5DR_ACTION_TYP_PUSH_VLAN:
			/* Double insert inline */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_INSERT;
			setter->set_double = &mlx5dr_action_setter_push_vlan;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT:
			/*
			 * Backup ipv6_route_ext.next_hdr to ipv6_route_ext.seg_left.
			 * Set ipv6_route_ext.next_hdr to 0 for checksum bug.
			 */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_mhdr;
			setter->idx_double = i;
			setter->extra_data = 0;
			setter++;

			/*
			 * Restore ipv6_route_ext.next_hdr from ipv6_route_ext.seg_left.
			 * Load the final destination address from flex parser sample 1->4.
			 */
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_mhdr;
			setter->idx_double = i;
			setter->extra_data = 1;
			setter++;

			/* Set the ipv6.protocol per ipv6_route_ext.next_hdr */
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_mhdr;
			setter->idx_double = i;
			setter->extra_data = 2;
			/* Pop ipv6_route_ext */
			setter->flags |= ASF_SINGLE1 | ASF_REMOVE;
			setter->set_single = &mlx5dr_action_setter_ipv6_route_ext_pop;
			setter->idx_single = i;
			at->need_dep_write = true;
			break;

		case MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT:
			/* Insert ipv6_route_ext with next_hdr as 0 due to checksum bug */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_INSERT;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_insert_ptr;
			setter->idx_double = i;
			setter->extra_data = 0;
			setter++;

			/* Set ipv6.protocol as IPPROTO_ROUTING: 0x2b */
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_mhdr;
			setter->idx_double = i;
			setter->extra_data = 1;
			setter++;

			/*
			 * Load the right ipv6_route_ext.next_hdr per user input buffer.
			 * Load the next dest_addr from the ipv6_route_ext.seg_list[last].
			 */
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_ipv6_route_ext_mhdr;
			setter->idx_double = i;
			setter->extra_data = 2;
			at->need_dep_write = true;
			break;

		case MLX5DR_ACTION_TYP_MODIFY_HDR:
			/* Double modify header list */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_MODIFY;
			setter->set_double = &mlx5dr_action_setter_modify_header;
			setter->idx_double = i;
			at->need_dep_write = true;
			break;

		case MLX5DR_ACTION_TYP_ASO_METER:
		case MLX5DR_ACTION_TYP_ASO_CT:
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE);
			setter->flags |= ASF_DOUBLE;
			setter->set_double = &mlx5dr_action_setter_aso;
			setter->idx_double = i;
			break;

		case MLX5DR_ACTION_TYP_REMOVE_HEADER:
		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
			/* Single remove header to header */
			setter = mlx5dr_action_setter_find_first(last_setter,
					ASF_SINGLE1 | ASF_MODIFY | ASF_INSERT);
			setter->flags |= ASF_SINGLE1 | ASF_REMOVE;
			setter->set_single = &mlx5dr_action_setter_single;
			setter->idx_single = i;
			break;

		case MLX5DR_ACTION_TYP_INSERT_HEADER:
		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
			/* Double insert header with pointer */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_INSERT;
			setter->set_double = &mlx5dr_action_setter_insert_ptr;
			setter->idx_double = i;
			at->need_dep_write = true;
			break;

		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
			/* Single remove + Double insert header with pointer */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_SINGLE1 | ASF_DOUBLE);
			setter->flags |= ASF_SINGLE1 | ASF_DOUBLE;
			setter->set_double = &mlx5dr_action_setter_insert_ptr;
			setter->idx_double = i;
			setter->set_single = &mlx5dr_action_setter_common_decap;
			setter->idx_single = i;
			at->need_dep_write = true;
			break;

		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
			/* Double modify header list with remove and push inline */
			setter = mlx5dr_action_setter_find_first(last_setter, ASF_DOUBLE | ASF_REMOVE);
			setter->flags |= ASF_DOUBLE | ASF_MODIFY | ASF_INSERT;
			setter->set_double = &mlx5dr_action_setter_tnl_l3_to_l2;
			setter->idx_double = i;
			at->need_dep_write = true;
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

		case MLX5DR_ACTION_TYP_NAT64:
			/* NAT64 requires 3 setters, each of them does specific modify header */
			for (j = 0; j < MLX5DR_ACTION_NAT64_STAGES; j++) {
				setter = mlx5dr_action_setter_find_first(last_setter,
									 ASF_DOUBLE | ASF_REMOVE);
				setter->flags |= ASF_DOUBLE | ASF_MODIFY;
				setter->set_double = &mlx5dr_action_setter_nat64;
				setter->idx_double = i;
				/* The stage indicates which modify-header to push */
				setter->stage_idx = j;
			}
			break;

		case MLX5DR_ACTION_TYP_JUMP_TO_MATCHER:
			last_setter->flags |= ASF_HIT;
			last_setter->set_hit = &mlx5dr_action_setter_hit_matcher;
			last_setter->idx_hit = i;
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
mlx5dr_action_template_create(const enum mlx5dr_action_type action_type[],
			      uint32_t flags)
{
	struct mlx5dr_action_template *at;
	uint8_t num_actions = 0;
	int i;

	if (flags > MLX5DR_ACTION_TEMPLATE_FLAG_RELAXED_ORDER) {
		DR_LOG(ERR, "Unsupported action template flag provided");
		rte_errno = EINVAL;
		return NULL;
	}

	at = simple_calloc(1, sizeof(*at));
	if (!at) {
		DR_LOG(ERR, "Failed to allocate action template");
		rte_errno = ENOMEM;
		return NULL;
	}

	at->flags = flags;

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
