/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

struct bnxt_ulp_mapper_tbl_list_info ulp_class_wh_plus_tmpl_list[] = {
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 6,
	.start_tbl_idx = 0,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 6,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 13,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 20,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 1,
	.start_tbl_idx = 27,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_DEFAULT
	},
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 28,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[7] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 32,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[8] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 36,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[9] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 41,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[10] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 46,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[11] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 51,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[12] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 56,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[13] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 60,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[14] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 64,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[15] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 68,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[16] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 72,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[17] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 76,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[18] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 80,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[19] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 85,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[20] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 90,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[21] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 95,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[22] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 100,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	},
	[23] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 4,
	.start_tbl_idx = 104,
	.flow_db_table_type = BNXT_ULP_FDB_TYPE_REGULAR
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_class_wh_plus_tbl_list[] = {
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
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 0,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 26,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 0,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 27,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.result_start_idx = 40,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.result_start_idx = 41,
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
	.result_start_idx = 42,
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
	.result_start_idx = 43,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_VFR_MODE,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 14,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 69,
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
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_NOT_SET,
	.cond_operand = BNXT_ULP_CF_IDX_VFR_MODE,
	.direction = TF_DIR_TX,
	.key_start_idx = 27,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 82,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 1,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.cond_opcode = BNXT_ULP_COND_OPCODE_COMP_FIELD_NOT_SET,
	.cond_operand = BNXT_ULP_CF_IDX_VFR_MODE,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 28,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 83,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 96,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 97,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 98,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_COMP_FIELD,
	.index_operand = BNXT_ULP_CF_IDX_DRV_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_8B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_TX,
	.result_start_idx = 99,
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
	.result_start_idx = 111,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 41,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 137,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 42,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 137,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
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
	.result_start_idx = 150,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 55,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 176,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 68,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 189,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 81,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 202,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 2,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 82,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 203,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_LKUP_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 216,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_CONSTANT,
	.index_operand = BNXT_ULP_SYM_VF_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 217,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_CONSTANT,
	.index_operand = BNXT_ULP_SYM_VF_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_TX,
	.result_start_idx = 218,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_CONSTANT,
	.index_operand = BNXT_ULP_SYM_VF_FUNC_PARIF
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TYPE_NORMAL,
	.direction = TF_DIR_RX,
	.result_start_idx = 219,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_VFR_FLAG,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_ALLOCATE,
	.index_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 95,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 245,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
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
	.result_start_idx = 258,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.index_opcode = BNXT_ULP_INDEX_OPCODE_GLOBAL,
	.index_operand = BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 108,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 284,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 3,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 121,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 297,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 4,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_1,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 124,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 298,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 167,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 306,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 178,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 315,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 5,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 191,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 328,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 6,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_1,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 194,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 329,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 7,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 237,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 337,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 7,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 248,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 346,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 7,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 249,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 347,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 8,
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
	.key_start_idx = 262,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 360,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 8,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 265,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 361,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 9,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 308,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 369,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 9,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 319,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 378,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 9,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 320,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 379,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 10,
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
	.key_start_idx = 333,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 392,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 10,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 336,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 393,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 11,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 379,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 401,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 11,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 390,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 410,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 11,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 391,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 411,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 12,
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
	.key_start_idx = 404,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 424,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 12,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 407,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 425,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 13,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 450,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 433,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 13,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 461,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 442,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 13,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 462,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 443,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 14,
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
	.key_start_idx = 475,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 456,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 14,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 478,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 457,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 15,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 521,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 465,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 15,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 532,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 474,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 15,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 545,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 487,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 16,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 548,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 488,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 17,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 591,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 496,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 17,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 602,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 505,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 17,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 615,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 518,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 18,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 618,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 519,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 19,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 661,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 527,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 19,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 672,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 536,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 19,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 685,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 549,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 20,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 688,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 550,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 21,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 731,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 558,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 21,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 742,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 567,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 21,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 755,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 580,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 22,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 758,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 581,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 23,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 801,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 589,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 23,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 812,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 598,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 23,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 825,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 611,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 24,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 828,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 612,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 25,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 871,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 620,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 25,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_SKIP,
	.key_start_idx = 882,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 629,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 25,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.key_start_idx = 895,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 642,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 26,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 898,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 643,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 27,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.key_start_idx = 941,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 651,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 27,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 952,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 660,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 27,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 953,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 661,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 28,
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
	.key_start_idx = 966,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 674,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 28,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 969,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 675,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 29,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1012,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 683,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 29,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 1023,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 692,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 29,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1024,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 693,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 30,
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
	.key_start_idx = 1037,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 706,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 30,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1040,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 707,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 31,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1083,
	.blob_key_bit_size = 200,
	.key_bit_size = 200,
	.key_num_fields = 11,
	.result_start_idx = 715,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 31,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 1094,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 724,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 31,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1095,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 725,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 32,
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
	.key_start_idx = 1108,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 738,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 32,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1111,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 739,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 33,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1154,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 747,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 33,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 1165,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 756,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 33,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1166,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 757,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 34,
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
	.key_start_idx = 1179,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 770,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 34,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1182,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 771,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 35,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1225,
	.blob_key_bit_size = 392,
	.key_bit_size = 392,
	.key_num_fields = 11,
	.result_start_idx = 779,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 35,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_SET_IF_MARK_ACTION,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_UPDATE,
	.key_start_idx = 1236,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 788,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 35,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 1249,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 801,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 36,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1252,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 802,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 37,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1295,
	.blob_key_bit_size = 104,
	.key_bit_size = 104,
	.key_num_fields = 7,
	.result_start_idx = 810,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 37,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_SEARCH_IF_HIT_UPDATE,
	.key_start_idx = 1302,
	.blob_key_bit_size = 167,
	.key_bit_size = 167,
	.key_num_fields = 13,
	.result_start_idx = 819,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0,
	.ident_start_idx = 37,
	.ident_nums = 1,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CACHE_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM,
	.direction = TF_DIR_TX,
	.key_start_idx = 1315,
	.blob_key_bit_size = 16,
	.key_bit_size = 16,
	.key_num_fields = 3,
	.result_start_idx = 832,
	.result_bit_size = 10,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.ident_start_idx = 38,
	.ident_nums = 1
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_TX,
	.priority = BNXT_ULP_PRIORITY_LEVEL_0,
	.srch_b4_alloc = BNXT_ULP_SEARCH_BEFORE_ALLOC_NO,
	.key_start_idx = 1318,
	.blob_key_bit_size = 81,
	.key_bit_size = 81,
	.key_num_fields = 43,
	.result_start_idx = 833,
	.result_bit_size = 38,
	.result_num_fields = 8,
	.encap_num_fields = 0,
	.ident_start_idx = 39,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INT_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_TX,
	.key_start_idx = 1361,
	.blob_key_bit_size = 104,
	.key_bit_size = 104,
	.key_num_fields = 7,
	.result_start_idx = 841,
	.result_bit_size = 64,
	.result_num_fields = 9,
	.encap_num_fields = 0,
	.ident_start_idx = 39,
	.ident_nums = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPCODE_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES
	}
};

struct bnxt_ulp_mapper_class_key_field_info ulp_class_wh_plus_key_field_list[] = {
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_PHY_PORT_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_PHY_PORT_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_VF_FUNC_SVIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_VF_FUNC_SVIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF6_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF6_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF6_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF6_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF6_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF6_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF6_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF6_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF7_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF7_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF7_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF7_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF7_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF7_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 48,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF7_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF7_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF8_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF8_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF9_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF9_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF9_IDX_O_IPV4_SRC_ADDR & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF10_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF10_IDX_O_IPV6_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF11_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF11_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF11_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_SVIF_INDEX & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF11_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF11_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF11_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF11_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF11_IDX_O_IPV6_SRC_ADDR & 0xff,
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF12_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF12_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF12_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF12_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF12_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF12_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF12_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF12_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_UDP_SRC_PORT & 0xff,
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
		(BNXT_ULP_HF12_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF12_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF12_IDX_O_IPV4_SRC_ADDR & 0xff,
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF13_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF13_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF13_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF13_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF13_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF13_IDX_O_IPV4_SRC_ADDR & 0xff,
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF14_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF14_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF14_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF14_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_UDP_SRC_PORT & 0xff,
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
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF14_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF14_IDX_O_IPV6_SRC_ADDR & 0xff,
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF15_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF15_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF15_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 1,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 7,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.spec_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF15_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF15_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF15_IDX_O_IPV6_SRC_ADDR & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF16_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF16_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF16_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF16_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF16_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_OO_VLAN_VID & 0xff,
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
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
		(BNXT_ULP_HF16_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF16_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF17_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF17_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF17_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF17_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF17_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_OO_VLAN_VID & 0xff,
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
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 2,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_TL3_HDR_TYPE_IPV6,
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
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF17_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF17_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
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
		(BNXT_ULP_HF18_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF18_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF18_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_SVIF_INDEX & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF18_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF18_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_O_UDP_SRC_PORT & 0xff,
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
		(BNXT_ULP_HF18_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF18_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF18_IDX_O_IPV4_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF19_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF19_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF19_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_SVIF_INDEX & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF19_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF19_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF19_IDX_O_IPV4_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_O_IPV4_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 32,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF19_IDX_O_IPV4_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF19_IDX_O_IPV4_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF20_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF20_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF20_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_SVIF_INDEX & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF20_IDX_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_O_UDP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF20_IDX_O_UDP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_O_UDP_SRC_PORT & 0xff,
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
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF20_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF20_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF20_IDX_O_IPV6_SRC_ADDR & 0xff,
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
		(BNXT_ULP_HF21_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_HF21_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF21_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_SVIF_INDEX & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 3,
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
		(BNXT_ULP_HF21_IDX_O_TCP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_O_TCP_DST_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 16,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF21_IDX_O_TCP_SRC_PORT >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_O_TCP_SRC_PORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_IP_PROTO_TCP,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF21_IDX_O_IPV6_DST_ADDR >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_O_IPV6_DST_ADDR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 128,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO,
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF21_IDX_O_IPV6_SRC_ADDR >> 8) & 0xff,
		BNXT_ULP_HF21_IDX_O_IPV6_SRC_ADDR & 0xff,
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF22_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF22_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF22_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF22_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF22_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF22_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF22_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF22_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 12,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF23_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_OO_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF23_IDX_OO_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_OO_VLAN_VID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF23_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF23_IDX_O_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_O_ETH_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 8,
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.mask_operand = {
		(BNXT_ULP_HF23_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_SVIF_INDEX & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF23_IDX_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_SVIF_INDEX & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.spec_operand = {
		(BNXT_ULP_CF_IDX_O_VTAG_NUM >> 8) & 0xff,
		BNXT_ULP_CF_IDX_O_VTAG_NUM & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {
		BNXT_ULP_SYM_L3_HDR_TYPE_IPV6,
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
	.mask_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.mask_operand = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
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
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.spec_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 7,
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
	.spec_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_HDR_FIELD,
	.spec_operand = {
		(BNXT_ULP_HF23_IDX_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_HF23_IDX_O_ETH_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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

struct bnxt_ulp_mapper_result_field_info ulp_class_wh_plus_result_field_list[] = {
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_VF_FUNC_PARIF,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
		(0x0005 >> 8) & 0xff,
		0x0005 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
		(0x0005 >> 8) & 0xff,
		0x0005 & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
		(0x0031 >> 8) & 0xff,
		0x0031 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID & 0xff,
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
		(0x0031 >> 8) & 0xff,
		0x0031 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
	.result_operand = {0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0185 >> 8) & 0xff,
		0x0185 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
		(0x0003 >> 8) & 0xff,
		0x0003 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0061 >> 8) & 0xff,
		0x0061 & 0xff,
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
	.field_bit_size = 7,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_IF_COMP_FIELD_THEN_CF_ELSE_CF,
	.result_operand = {
		(BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP >> 8) & 0xff,
		BNXT_ULP_CF_IDX_MATCH_PORT_IS_VFREP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_true = {
		(BNXT_ULP_CF_IDX_LOOPBACK_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_LOOPBACK_PARIF & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.result_operand_false = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_PARIF >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_PARIF & 0xff,
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
		(0x0003 >> 8) & 0xff,
		0x0003 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.field_bit_size = 5,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x0c, 0x00, 0x00, 0x00, 0x00, 0x00,
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
		(0x0061 >> 8) & 0xff,
		0x0061 & 0xff,
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

struct bnxt_ulp_mapper_ident_info ulp_class_wh_plus_ident_list[] = {
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_REGFILE_INDEX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 0
	},
	{
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
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

