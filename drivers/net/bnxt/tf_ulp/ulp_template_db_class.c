/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

uint16_t ulp_class_sig_tbl[BNXT_ULP_CLASS_SIG_TBL_MAX_SZ] = {
	[BNXT_ULP_CLASS_HID_0080] = 1,
	[BNXT_ULP_CLASS_HID_0087] = 2,
	[BNXT_ULP_CLASS_HID_0000] = 3
};

struct bnxt_ulp_class_match_info ulp_class_match_list[] = {
	[1] = {
	.class_hid = BNXT_ULP_CLASS_HID_0080,
	.hdr_sig = { .bits =
		BNXT_ULP_HDR_BIT_O_ETH |
		BNXT_ULP_HDR_BIT_O_IPV4 |
		BNXT_ULP_HDR_BIT_O_UDP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.field_sig = { .bits =
		BNXT_ULP_HF8_BITMASK_O_IPV4_SRC_ADDR |
		BNXT_ULP_HF8_BITMASK_O_IPV4_DST_ADDR |
		BNXT_ULP_HF8_BITMASK_O_UDP_SRC_PORT |
		BNXT_ULP_HF8_BITMASK_O_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 8,
	.act_vnic = 0,
	.wc_pri = 0
	},
	[2] = {
	.class_hid = BNXT_ULP_CLASS_HID_0087,
	.hdr_sig = { .bits =
		BNXT_ULP_HDR_BIT_O_ETH |
		BNXT_ULP_HDR_BIT_O_IPV4 |
		BNXT_ULP_HDR_BIT_O_UDP |
		BNXT_ULP_HDR_BIT_T_VXLAN |
		BNXT_ULP_HDR_BIT_I_ETH |
		BNXT_ULP_HDR_BIT_I_IPV4 |
		BNXT_ULP_HDR_BIT_I_UDP |
		BNXT_ULP_FLOW_DIR_BITMASK_ING },
	.field_sig = { .bits =
		BNXT_ULP_HF9_BITMASK_I_IPV4_SRC_ADDR |
		BNXT_ULP_HF9_BITMASK_I_IPV4_DST_ADDR |
		BNXT_ULP_HF9_BITMASK_I_UDP_SRC_PORT |
		BNXT_ULP_HF9_BITMASK_I_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 9,
	.act_vnic = 0,
	.wc_pri = 0
	},
	[3] = {
	.class_hid = BNXT_ULP_CLASS_HID_0000,
	.hdr_sig = { .bits =
		BNXT_ULP_HDR_BIT_O_ETH |
		BNXT_ULP_HDR_BIT_O_IPV4 |
		BNXT_ULP_HDR_BIT_O_UDP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.field_sig = { .bits =
		BNXT_ULP_HF10_BITMASK_O_IPV4_SRC_ADDR |
		BNXT_ULP_HF10_BITMASK_O_IPV4_DST_ADDR |
		BNXT_ULP_HF10_BITMASK_O_UDP_SRC_PORT |
		BNXT_ULP_HF10_BITMASK_O_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 10,
	.act_vnic = 0,
	.wc_pri = 0
	}
};

struct bnxt_ulp_mapper_tbl_list_info ulp_class_tmpl_list[] = {
	[((1 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 0,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((2 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 2,
	.start_tbl_idx = 4,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((3 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 6,
	.start_tbl_idx = 6,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((4 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 12,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((5 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 3,
	.start_tbl_idx = 17,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((6 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 3,
	.start_tbl_idx = 20,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((7 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 23,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[((8 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 24,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((9 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 29,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[((10 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 34,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_class_tbl_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.result_start_idx = 0,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 0,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 26,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.result_start_idx = 39,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.result_start_idx = 40,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.result_start_idx = 41,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 13,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 67,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.result_start_idx = 80,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.result_start_idx = 92,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 26,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 118,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.result_start_idx = 131,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 39,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 157,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 52,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 170,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 65,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 183,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 196,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_VF_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 197,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_VF_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.result_start_idx = 198,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_VFR_FLAG,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 78,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 224,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.result_start_idx = 237,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.result_start_idx = 249,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 91,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 275,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.result_start_idx = 288,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 104,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 314,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 117,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 327,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_VFR_CFA_ACTION,
	.direction = TF_DIR_TX,
	.result_start_idx = 340,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 130,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 366,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 132,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 367,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 145,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 380,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 148,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 381,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 190,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 389,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 201,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 398,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 203,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 399,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 216,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 412,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 219,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 413,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 261,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 421,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 272,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 430,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 274,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 431,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 287,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 444,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 290,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 445,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 6,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EXT_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 332,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 453,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 6,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	}
};

struct bnxt_ulp_mapper_class_key_field_info ulp_class_key_field_list[] = {
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_PHY_PORT_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x02}
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x02}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF8_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_CLASS_TID >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_CLASS_TID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_TYPE_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L2_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 9,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 251,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_O_UDP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF8_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_O_IPV4_SRC_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 24,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF9_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_CLASS_TID >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_CLASS_TID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_TYPE_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L2_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TL4_HDR_TYPE_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TL4_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TL3_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TL2_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 9,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 251,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_I_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_I_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_I_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_I_UDP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_I_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_I_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_I_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_I_IPV4_SRC_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 24,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF10_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TUN_HDR_TYPE_NONE,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_CLASS_TID >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_CLASS_TID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_TYPE_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L4_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L2_HDR_VALID_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 9,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 251,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_UDP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_UDP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_IPV4_SRC_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 24,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.spec_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_class_result_field_list[] = {
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_VNIC >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_VNIC & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_L2_CNTXT_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_L2_CNTXT_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_PHY_PORT_VPORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_VPORT & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
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
		BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x81, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 80,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(BNXT_ULP_SYM_WH_PLUS_LOOPBACK_PORT >> 8) & 0xff,
		BNXT_ULP_SYM_WH_PLUS_LOOPBACK_PORT & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_VF_FUNC_VNIC >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_VNIC & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_L2_CNTXT_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_L2_CNTXT_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_VNIC >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_VNIC & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
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
		BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x81, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x02}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 80,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_PHY_PORT_VPORT >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_VPORT & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_VF_FUNC_VNIC >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_VNIC & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(BNXT_ULP_SYM_WH_PLUS_LOOPBACK_PORT >> 8) & 0xff,
		BNXT_ULP_SYM_WH_PLUS_LOOPBACK_PORT & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00f9 >> 8) & 0xff,
		0x00f9 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x15, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 33,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 9,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00c5 >> 8) & 0xff,
		0x00c5 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00f9 >> 8) & 0xff,
		0x00f9 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x15, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 33,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 9,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00c5 >> 8) & 0xff,
		0x00c5 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 6,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR & 0xff,
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
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00f9 >> 8) & 0xff,
		0x00f9 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x15, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0 & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 33,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 9,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		(0x00c5 >> 8) & 0xff,
		0x00c5 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	}
};

struct bnxt_ulp_mapper_ident_info ulp_ident_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_EM_PROFILE_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	}
};
