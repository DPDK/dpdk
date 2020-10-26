/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

struct bnxt_ulp_mapper_tbl_list_info ulp_stingray_act_tmpl_list[] = {
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 0
	},
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 3,
	.start_tbl_idx = 6
	},
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 3,
	.start_tbl_idx = 9
	},
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 12
	},
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 18
	},
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 5,
	.start_tbl_idx = 24
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_stingray_act_tbl_list[] = {
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
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 1,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 2,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 3,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 15,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 41,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 67,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 68,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 94,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 120,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 121,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 147,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 173,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 174,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 177,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.result_start_idx = 180,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 192,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 230,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 256,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 257,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 258,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 259,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 271,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 297,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 334,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 335,
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
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 347,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 373,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPCODE_EXECUTE_IF_EXT,
	.cond_opcode = BNXT_ULP_COND_OPCODE_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.result_start_idx = 399,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_stingray_act_result_field_list[] = {
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
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
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_HDR_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_HDR_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_VXLAN_DECAP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 64,
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_HDR_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_ACT_BIT_THEN_ACT_PROP_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACTION_BIT_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_HDR_BIT_THEN_CONST_ELSE_CONST,
	.result_operand = {
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {0x0b, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 4,
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
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.field_bit_size = 64,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 80,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 1,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
};
