/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

uint16_t ulp_act_sig_tbl[BNXT_ULP_ACT_SIG_TBL_MAX_SZ] = {
	[BNXT_ULP_ACT_HID_0002] = 1,
	[BNXT_ULP_ACT_HID_0022] = 2,
	[BNXT_ULP_ACT_HID_0026] = 3,
	[BNXT_ULP_ACT_HID_0006] = 4,
	[BNXT_ULP_ACT_HID_0009] = 5,
	[BNXT_ULP_ACT_HID_0029] = 6,
	[BNXT_ULP_ACT_HID_002d] = 7,
	[BNXT_ULP_ACT_HID_004b] = 8,
	[BNXT_ULP_ACT_HID_004a] = 9,
	[BNXT_ULP_ACT_HID_004f] = 10,
	[BNXT_ULP_ACT_HID_004e] = 11,
	[BNXT_ULP_ACT_HID_006c] = 12,
	[BNXT_ULP_ACT_HID_0070] = 13,
	[BNXT_ULP_ACT_HID_0021] = 14,
	[BNXT_ULP_ACT_HID_0025] = 15,
	[BNXT_ULP_ACT_HID_0043] = 16,
	[BNXT_ULP_ACT_HID_0042] = 17,
	[BNXT_ULP_ACT_HID_0047] = 18,
	[BNXT_ULP_ACT_HID_0046] = 19,
	[BNXT_ULP_ACT_HID_0064] = 20,
	[BNXT_ULP_ACT_HID_0068] = 21,
	[BNXT_ULP_ACT_HID_00a1] = 22,
	[BNXT_ULP_ACT_HID_00df] = 23
};

struct bnxt_ulp_act_match_info ulp_act_match_list[] = {
	[1] = {
	.act_hid = BNXT_ULP_ACT_HID_0002,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[2] = {
	.act_hid = BNXT_ULP_ACT_HID_0022,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[3] = {
	.act_hid = BNXT_ULP_ACT_HID_0026,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[4] = {
	.act_hid = BNXT_ULP_ACT_HID_0006,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_DROP |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[5] = {
	.act_hid = BNXT_ULP_ACT_HID_0009,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[6] = {
	.act_hid = BNXT_ULP_ACT_HID_0029,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[7] = {
	.act_hid = BNXT_ULP_ACT_HID_002d,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[8] = {
	.act_hid = BNXT_ULP_ACT_HID_004b,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[9] = {
	.act_hid = BNXT_ULP_ACT_HID_004a,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[10] = {
	.act_hid = BNXT_ULP_ACT_HID_004f,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[11] = {
	.act_hid = BNXT_ULP_ACT_HID_004e,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[12] = {
	.act_hid = BNXT_ULP_ACT_HID_006c,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[13] = {
	.act_hid = BNXT_ULP_ACT_HID_0070,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_RSS |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[14] = {
	.act_hid = BNXT_ULP_ACT_HID_0021,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[15] = {
	.act_hid = BNXT_ULP_ACT_HID_0025,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[16] = {
	.act_hid = BNXT_ULP_ACT_HID_0043,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[17] = {
	.act_hid = BNXT_ULP_ACT_HID_0042,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[18] = {
	.act_hid = BNXT_ULP_ACT_HID_0047,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[19] = {
	.act_hid = BNXT_ULP_ACT_HID_0046,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[20] = {
	.act_hid = BNXT_ULP_ACT_HID_0064,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[21] = {
	.act_hid = BNXT_ULP_ACT_HID_0068,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_ACTION_BIT_COUNT |
		BNXT_ULP_ACTION_BIT_POP_VLAN |
		BNXT_ULP_ACTION_BIT_DEC_TTL |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 1
	},
	[22] = {
	.act_hid = BNXT_ULP_ACT_HID_00a1,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_DECAP |
		BNXT_ULP_ACTION_BIT_MARK |
		BNXT_ULP_ACTION_BIT_VNIC |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.act_tid = 2
	},
	[23] = {
	.act_hid = BNXT_ULP_ACT_HID_00df,
	.act_sig = { .bits =
		BNXT_ULP_ACTION_BIT_VXLAN_ENCAP |
		BNXT_ULP_ACTION_BIT_VPORT |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.act_tid = 3
	}
};

struct bnxt_ulp_mapper_tbl_list_info ulp_act_tmpl_list[] = {
	[((1 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 2,
	.start_tbl_idx = 0,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((2 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 2,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((3 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 3,
	.start_tbl_idx = 3,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_act_tbl_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 1,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 27,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 53,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 56,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 59,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 12,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_act_result_field_list[] = {
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_COUNT >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_COUNT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_POP_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_BIT,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_DROP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_DROP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_DECAP_FUNC_THRU_TUN,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_TUN_TYPE_VXLAN,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L4_TYPE_UDP_CSUM,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 0,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ENCAP_ACT_PROP_SZ,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN & 0xff,
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
};
