/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2019 Broadcom
 * All rights reserved.
 */

/*
 * date: Mon Mar  9 02:37:53 2020
 * version: 0.0
 */

#include "ulp_template_db.h"
#include "ulp_template_struct.h"

uint32_t ulp_act_prop_map_table[] = {
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_TYPE,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_NUM,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L3_TYPE,
	[BNXT_ULP_ACT_PROP_IDX_MPLS_POP_NUM] =
		BNXT_ULP_ACT_PROP_SZ_MPLS_POP_NUM,
	[BNXT_ULP_ACT_PROP_IDX_MPLS_PUSH_NUM] =
		BNXT_ULP_ACT_PROP_SZ_MPLS_PUSH_NUM,
	[BNXT_ULP_ACT_PROP_IDX_VNIC] =
		BNXT_ULP_ACT_PROP_SZ_VNIC,
	[BNXT_ULP_ACT_PROP_IDX_VPORT] =
		BNXT_ULP_ACT_PROP_SZ_VPORT,
	[BNXT_ULP_ACT_PROP_IDX_MARK] =
		BNXT_ULP_ACT_PROP_SZ_MARK,
	[BNXT_ULP_ACT_PROP_IDX_COUNT] =
		BNXT_ULP_ACT_PROP_SZ_COUNT,
	[BNXT_ULP_ACT_PROP_IDX_METER] =
		BNXT_ULP_ACT_PROP_SZ_METER,
	[BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_MAC_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_MAC_DST,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_VLAN] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_VLAN,
	[BNXT_ULP_ACT_PROP_IDX_OF_SET_VLAN_PCP] =
		BNXT_ULP_ACT_PROP_SZ_OF_SET_VLAN_PCP,
	[BNXT_ULP_ACT_PROP_IDX_OF_SET_VLAN_VID] =
		BNXT_ULP_ACT_PROP_SZ_OF_SET_VLAN_VID,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV6_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV6_DST,
	[BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_TP_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_TP_DST,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_0] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_0,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_1] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_1,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_2] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_2,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_3] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_3,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_4] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_4,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_5] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_5,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_6] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_6,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_7] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_7,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_UDP,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN,
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

struct bnxt_ulp_device_params ulp_device_params[] = {
	[BNXT_ULP_DEVICE_ID_WH_PLUS] = {
		.global_fid_enable       = BNXT_ULP_SYM_YES,
		.byte_order              = (enum bnxt_ulp_byte_order)
						BNXT_ULP_SYM_LITTLE_ENDIAN,
		.encap_byte_swap         = 1,
		.lfid_entries            = 16384,
		.lfid_entry_size         = 4,
		.gfid_entries            = 65536,
		.gfid_entry_size         = 4,
		.num_flows               = 32768,
		.num_resources_per_flow  = 8
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_act_result_field_list[] = {
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {BNXT_ULP_SYM_DECAP_FUNC_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_ACT_PROP,
	.result_operand = {(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_RESULT_OPC_SET_TO_CONSTANT,
	.result_operand = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
};
