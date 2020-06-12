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
	[BNXT_ULP_CLASS_HID_0000] = 2,
	[BNXT_ULP_CLASS_HID_0087] = 3
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
		BNXT_ULP_HF0_BITMASK_O_IPV4_SRC_ADDR |
		BNXT_ULP_HF0_BITMASK_O_IPV4_DST_ADDR |
		BNXT_ULP_HF0_BITMASK_O_UDP_SRC_PORT |
		BNXT_ULP_HF0_BITMASK_O_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 0,
	.act_vnic = 0,
	.wc_pri = 0
	},
	[2] = {
	.class_hid = BNXT_ULP_CLASS_HID_0000,
	.hdr_sig = { .bits =
		BNXT_ULP_HDR_BIT_O_ETH |
		BNXT_ULP_HDR_BIT_O_IPV4 |
		BNXT_ULP_HDR_BIT_O_UDP |
		BNXT_ULP_FLOW_DIR_BITMASK_EGR },
	.field_sig = { .bits =
		BNXT_ULP_HF1_BITMASK_O_IPV4_SRC_ADDR |
		BNXT_ULP_HF1_BITMASK_O_IPV4_DST_ADDR |
		BNXT_ULP_HF1_BITMASK_O_UDP_SRC_PORT |
		BNXT_ULP_HF1_BITMASK_O_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 1,
	.act_vnic = 0,
	.wc_pri = 0
	},
	[3] = {
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
		BNXT_ULP_HF2_BITMASK_I_IPV4_SRC_ADDR |
		BNXT_ULP_HF2_BITMASK_I_IPV4_DST_ADDR |
		BNXT_ULP_HF2_BITMASK_I_UDP_SRC_PORT |
		BNXT_ULP_HF2_BITMASK_I_UDP_DST_PORT |
		BNXT_ULP_MATCH_TYPE_BITMASK_EM },
	.class_tid = 2,
	.act_vnic = 0,
	.wc_pri = 0
	}
};

struct bnxt_ulp_mapper_tbl_list_info ulp_class_tmpl_list[] = {
	[((0 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 0
	},
	[((1 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 5
	},
	[((2 << BNXT_ULP_LOG2_MAX_NUM_DEV) |
		BNXT_ULP_DEVICE_ID_WH_PLUS)] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 10
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_class_tbl_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 0,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 0,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 2,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 1,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 15,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 14,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 18,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 15,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 60,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 23,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 71,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 32,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 73,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 33,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 86,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 46,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 89,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 47,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 131,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 55,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 142,
	.blob_key_bit_size = 12,
	.key_bit_size = 12,
	.key_num_fields = 2,
	.result_start_idx = 64,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 144,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 65,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 157,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 78,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 160,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 42,
	.result_start_idx = 79,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 6,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_EXTERNAL,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_NOT_USED,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_NOT_USED,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 202,
	.blob_key_bit_size = 448,
	.key_bit_size = 448,
	.key_num_fields = 11,
	.result_start_idx = 87,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 6,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_NOT_USED
	}
};

struct bnxt_ulp_mapper_class_key_field_info ulp_class_key_field_list[] = {
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF0_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF0_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF0_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF0_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF0_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_O_UDP_SRC_PORT & 0xff,
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
		(BNXT_ULP_HF0_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF0_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF0_IDX_O_IPV4_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF1_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF1_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF1_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF1_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF1_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_O_UDP_SRC_PORT & 0xff,
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
		(BNXT_ULP_HF1_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF1_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF1_IDX_O_IPV4_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF2_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF2_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF2_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_SVIF_INDEX & 0xff,
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
		(BNXT_ULP_HF2_IDX_I_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_I_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF2_IDX_I_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_I_UDP_SRC_PORT & 0xff,
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
		(BNXT_ULP_HF2_IDX_I_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_I_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF2_IDX_I_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF2_IDX_I_IPV4_SRC_ADDR & 0xff,
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
