/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2024 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header act list */
struct bnxt_ulp_mapper_tmpl_info ulp_thor_act_tmpl_list[] = {
	/* act_tid: 1, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 9,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 0,
		.cond_nums = 1 }
	},
	/* act_tid: 2, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 10,
	.start_tbl_idx = 9,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 14,
		.cond_nums = 0 }
	},
	/* act_tid: 3, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 6,
	.start_tbl_idx = 19,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 18,
		.cond_nums = 1 }
	},
	/* act_tid: 4, ingress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 7,
	.start_tbl_idx = 25,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 24,
		.cond_nums = 2 }
	},
	/* act_tid: 5, ingress */
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 20,
	.start_tbl_idx = 32,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 33,
		.cond_nums = 0 }
	},
	/* act_tid: 6, egress */
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 7,
	.start_tbl_idx = 52,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 48,
		.cond_nums = 1 }
	},
	/* act_tid: 7, egress */
	[7] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 6,
	.start_tbl_idx = 59,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 56,
		.cond_nums = 1 }
	},
	/* act_tid: 8, egress */
	[8] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 25,
	.start_tbl_idx = 65,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 62,
		.cond_nums = 1 }
	},
	/* act_tid: 9, egress */
	[9] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 5,
	.start_tbl_idx = 90,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 94,
		.cond_nums = 1 }
	},
	/* act_tid: 10, egress */
	[10] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 11,
	.start_tbl_idx = 95,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 98,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_thor_act_tbl_list[] = {
	{ /* act_tid: 1, , table: shared_meter_tbl_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 1,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 0,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 0,
	.ident_nums = 1
	},
	{ /* act_tid: 1, , table: control.meter_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 2,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 1, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 3,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 1,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 1,
	.ident_nums = 1
	},
	{ /* act_tid: 1, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 4,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 1, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 5,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 1, , table: mod_record.ing_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 6,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 1,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 25
	},
	{ /* act_tid: 1, , table: mod_record.ing_no_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_LIST_AND,
		.cond_start_idx = 0,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 26,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 18
	},
	{ /* act_tid: 1, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 10,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 44,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 1, , table: int_compact_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_COMPACT_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 12,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 61,
	.result_bit_size = 64,
	.result_num_fields = 13
	},
	{ /* act_tid: 2, , table: control.delete_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 14,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 2, , table: shared_mirror_record.del_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 2,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 2,
	.ident_nums = 1
	},
	{ /* act_tid: 2, , table: control.mirror_del_exist_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 15,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 2, , table: control.mirror_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 16,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_REF_CNT,
		.func_src2 = BNXT_ULP_FUNC_SRC_CONST,
		.func_opr2 = 1,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* act_tid: 2, , table: control.create */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 17,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 2, , table: mirror_tbl.alloc */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 17,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 74,
	.result_bit_size = 32,
	.result_num_fields = 5
	},
	{ /* act_tid: 2, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 17,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 79,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 2, , table: int_compact_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_COMPACT_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 80,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0
	},
	{ /* act_tid: 2, , table: mirror_tbl.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 93,
	.result_bit_size = 32,
	.result_num_fields = 5
	},
	{ /* act_tid: 2, , table: shared_mirror_record.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_INC,
	.key_start_idx = 3,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 98,
	.result_bit_size = 36,
	.result_num_fields = 2
	},
	{ /* act_tid: 3, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 19,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 4,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 3,
	.ident_nums = 1
	},
	{ /* act_tid: 3, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 20,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 3, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 21,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 100,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 3, , table: mod_record.ing_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 22,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 101,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 31
	},
	{ /* act_tid: 3, , table: mod_record.ing_no_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 23,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 132,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 24
	},
	{ /* act_tid: 3, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 24,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 156,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 4, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 26,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 5,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 4,
	.ident_nums = 1
	},
	{ /* act_tid: 4, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 27,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 4, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 28,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 173,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 4, , table: vnic_interface_rss_config.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_RSS,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 29,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_RSS_VNIC,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 174,
	.result_bit_size = 0,
	.result_num_fields = 0
	},
	{ /* act_tid: 4, , table: vnic_interface_queue_config.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_VNIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_VNIC_TABLE_QUEUE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 30,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_RSS_VNIC,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 174,
	.result_bit_size = 0,
	.result_num_fields = 0
	},
	{ /* act_tid: 4, , table: int_compact_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_COMPACT_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 31,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 174,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0
	},
	{ /* act_tid: 4, , table: int_compact_act_record.1 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_COMPACT_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 33,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 187,
	.result_bit_size = 64,
	.result_num_fields = 13,
	.encap_num_fields = 0
	},
	{ /* act_tid: 5, , table: control.create_check */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 11,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 33,
		.cond_nums = 2 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 5, , table: meter_profile_tbl_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 35,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 6,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 5,
	.ident_nums = 0
	},
	{ /* act_tid: 5, , table: control.shared_meter_profile_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 36,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 5, , table: meter_profile_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_METER_PROF,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 37,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 200,
	.result_bit_size = 65,
	.result_num_fields = 11
	},
	{ /* act_tid: 5, , table: meter_profile_tbl_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_METER_PROF,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 37,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.key_start_idx = 7,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.result_start_idx = 211,
	.result_bit_size = 42,
	.result_num_fields = 2
	},
	{ /* act_tid: 5, , table: shared_meter_tbl_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 37,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 8,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 5,
	.ident_nums = 0
	},
	{ /* act_tid: 5, , table: control.meter_created_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 38,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 5, , table: meter_profile_tbl_cache.rd2 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 39,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.key_start_idx = 9,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 5,
	.ident_nums = 1
	},
	{ /* act_tid: 5, , table: control.shared_meter_profile_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 39,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 5, , table: meter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_METER_INST,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 213,
	.result_bit_size = 64,
	.result_num_fields = 5
	},
	{ /* act_tid: 5, , table: shared_meter_tbl_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.key_start_idx = 10,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.result_start_idx = 218,
	.result_bit_size = 74,
	.result_num_fields = 3
	},
	{ /* act_tid: 5, , table: control.delete_check */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 5,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 40,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 5, , table: meter_profile_tbl_cache.del_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 41,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 11,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 6,
	.ident_nums = 1
	},
	{ /* act_tid: 5, , table: control.mtr_prof_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 42,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_REF_CNT,
		.func_src2 = BNXT_ULP_FUNC_SRC_CONST,
		.func_opr2 = 1,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* act_tid: 5, , table: shared_meter_tbl_cache.del_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 43,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 12,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 7,
	.ident_nums = 1
	},
	{ /* act_tid: 5, , table: control.shared_mtr_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 44,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_REF_CNT,
		.func_src2 = BNXT_ULP_FUNC_SRC_CONST,
		.func_opr2 = 1,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* act_tid: 5, , table: control.update_check */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 45,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 5, , table: shared_meter_tbl_cache.rd_update */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 45,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 13,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.ident_start_idx = 8,
	.ident_nums = 1
	},
	{ /* act_tid: 5, , table: meter_tbl.update_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_METER_INST,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 46,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ident_start_idx = 9,
	.ident_nums = 3,
	.result_bit_size = 64
	},
	{ /* act_tid: 5, , table: meter_tbl.update_wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_METER_INST,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 48,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.result_start_idx = 221,
	.result_bit_size = 64,
	.result_num_fields = 5
	},
	{ /* act_tid: 6, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 49,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 14,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 12,
	.ident_nums = 1
	},
	{ /* act_tid: 6, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 50,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 6, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 51,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 226,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 6, , table: int_vtag_encap_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 52,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 227,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* act_tid: 6, , table: mod_record.dec_ttl_egr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 53,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 238,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 23
	},
	{ /* act_tid: 6, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 54,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 261,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 6, , table: int_compact_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_COMPACT_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 56,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 278,
	.result_bit_size = 64,
	.result_num_fields = 13
	},
	{ /* act_tid: 7, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 57,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 15,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 13,
	.ident_nums = 1
	},
	{ /* act_tid: 7, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 58,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 7, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 59,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 291,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 7, , table: mod_record.ing_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 60,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 292,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 31
	},
	{ /* act_tid: 7, , table: mod_record.ing_no_ttl */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 61,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 323,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 24
	},
	{ /* act_tid: 7, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 62,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 347,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 8, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 63,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 16,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 14,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 64,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 8, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 65,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 364,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 8, , table: source_property_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 66,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 17,
	.blob_key_bit_size = 85,
	.key_bit_size = 85,
	.key_num_fields = 3,
	.ident_start_idx = 15,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.sp_rec_v4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 67,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 8, , table: sp_smac_ipv4.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 68,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.record_size = 16,
	.result_start_idx = 365,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3
	},
	{ /* act_tid: 8, , table: source_property_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 69,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 20,
	.blob_key_bit_size = 85,
	.key_bit_size = 85,
	.key_num_fields = 3,
	.result_start_idx = 368,
	.result_bit_size = 64,
	.result_num_fields = 2
	},
	{ /* act_tid: 8, , table: source_property_ipv6_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 69,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 23,
	.blob_key_bit_size = 176,
	.key_bit_size = 176,
	.key_num_fields = 2,
	.ident_start_idx = 16,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.sp_rec_v6 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 70,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 8, , table: sp_smac_ipv6.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 71,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 32,
	.result_start_idx = 370,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3
	},
	{ /* act_tid: 8, , table: source_property_ipv6_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 72,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 25,
	.blob_key_bit_size = 176,
	.key_bit_size = 176,
	.key_num_fields = 2,
	.result_start_idx = 373,
	.result_bit_size = 48,
	.result_num_fields = 2
	},
	{ /* act_tid: 8, , table: mod_record.ing_l2write */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 72,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 375,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 18
	},
	{ /* act_tid: 8, , table: vxlan_encap_rec_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 74,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 27,
	.blob_key_bit_size = 141,
	.key_bit_size = 141,
	.key_num_fields = 6,
	.ident_start_idx = 17,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 76,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 33,
	.blob_key_bit_size = 237,
	.key_bit_size = 237,
	.key_num_fields = 6,
	.ident_start_idx = 18,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.vxlan_v6_encap */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 5,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 78,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 8, , table: int_tun_encap_record.ipv4_vxlan */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 79,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 393,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 25
	},
	{ /* act_tid: 8, , table: int_tun_encap_record.ipv6_vxlan */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 81,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 418,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 23
	},
	{ /* act_tid: 8, , table: vxlan_encap_rec_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 83,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 39,
	.blob_key_bit_size = 141,
	.key_bit_size = 141,
	.key_num_fields = 6,
	.result_start_idx = 441,
	.result_bit_size = 64,
	.result_num_fields = 2
	},
	{ /* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 85,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 45,
	.blob_key_bit_size = 237,
	.key_bit_size = 237,
	.key_num_fields = 6,
	.result_start_idx = 443,
	.result_bit_size = 64,
	.result_num_fields = 2
	},
	{ /* act_tid: 8, , table: geneve_encap_rec_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 87,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 51,
	.blob_key_bit_size = 493,
	.key_bit_size = 493,
	.key_num_fields = 15,
	.ident_start_idx = 19,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.geneve_encap */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 88,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 8, , table: int_geneve_encap_record.ipv4_geneve */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 89,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 445,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 31
	},
	{ /* act_tid: 8, , table: int_geneve_encap_record.ipv6_geneve */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 91,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 476,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 29
	},
	{ /* act_tid: 8, , table: geneve_encap_rec_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 93,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 66,
	.blob_key_bit_size = 493,
	.key_bit_size = 493,
	.key_num_fields = 15,
	.result_start_idx = 505,
	.result_bit_size = 64,
	.result_num_fields = 2
	},
	{ /* act_tid: 8, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 94,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 507,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 9, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 2,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 95,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 81,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 20,
	.ident_nums = 1
	},
	{ /* act_tid: 9, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 96,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 9, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 97,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 524,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 9, , table: mod_record.vf_2_vf */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 98,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 525,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 20
	},
	{ /* act_tid: 9, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 98,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 545,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 10, , table: control.delete_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 98,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 10, , table: shared_mirror_record.del_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 99,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 82,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 21,
	.ident_nums = 1
	},
	{ /* act_tid: 10, , table: control.mirror_del_exist_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 99,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 10, , table: control.mirror_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 100,
		.cond_nums = 1 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_DELETE_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_REGFILE,
		.func_opr1 = BNXT_ULP_RF_IDX_REF_CNT,
		.func_src2 = BNXT_ULP_FUNC_SRC_CONST,
		.func_opr2 = 1,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC }
	},
	{ /* act_tid: 10, , table: control.create */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 101,
		.cond_nums = 0 },
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID
	},
	{ /* act_tid: 10, , table: mirror_tbl.alloc */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 101,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 562,
	.result_bit_size = 32,
	.result_num_fields = 5
	},
	{ /* act_tid: 10, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 101,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 567,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 10, , table: mod_record.vf_2_vf */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 102,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 568,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 20
	},
	{ /* act_tid: 10, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 102,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 588,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* act_tid: 10, , table: mirror_tbl.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 102,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 605,
	.result_bit_size = 32,
	.result_num_fields = 5
	},
	{ /* act_tid: 10, , table: shared_mirror_record.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 102,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.key_recipe_opcode = BNXT_ULP_KEY_RECIPE_OPC_NOP,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_INC,
	.key_start_idx = 83,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 610,
	.result_bit_size = 36,
	.result_num_fields = 2
	}
};

struct bnxt_ulp_mapper_cond_list_info ulp_thor_act_cond_oper_list[] = {
	/* cond_execute: act_tid: 1, mod_record.ing_no_ttl:7*/
	{
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 7,
		.cond_nums = 1
	},
	/* cond_execute: act_tid: 1, mod_record.ing_no_ttl:7*/
	{
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 8,
		.cond_nums = 2
	}
};

struct bnxt_ulp_mapper_cond_info ulp_thor_act_cond_list[] = {
	/* cond_reject: thor, act_tid: 1 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 1, shared_meter_tbl_cache.rd:1*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER
	},
	/* cond_execute: act_tid: 1, control.meter_chk:2*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 1, shared_mirror_record.rd:3*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 1, control.mirror:4*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 1, int_flow_counter_tbl.0:5*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 1, mod_record.ing_ttl:6*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 1, mod_record.ing_no_ttl:7*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 1, mod_record.ing_no_ttl:7*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_DST
	},
	/* field_cond: act_tid: 1, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_VXLAN_DECAP
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_GENEVE_DECAP
	},
	/* field_cond: act_tid: 1, int_compact_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_VXLAN_DECAP
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_GENEVE_DECAP
	},
	/* cond_execute: act_tid: 2, control.delete_chk:14*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 2, control.mirror_del_exist_chk:15*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 2, control.mirror_ref_cnt_chk:16*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 2, int_flow_counter_tbl.0:17*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_reject: thor, act_tid: 3 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 3, shared_mirror_record.rd:19*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 3, control.mirror:20*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 3, int_flow_counter_tbl.0:21*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 3, mod_record.ing_ttl:22*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 3, mod_record.ing_no_ttl:23*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_reject: thor, act_tid: 4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_QUEUE
	},
	/* cond_execute: act_tid: 4, shared_mirror_record.rd:26*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 4, control.mirror:27*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 4, int_flow_counter_tbl.0:28*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 4, vnic_interface_rss_config.0:29*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: act_tid: 4, vnic_interface_queue_config.0:30*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_QUEUE
	},
	/* cond_execute: act_tid: 4, int_compact_act_record.0:31*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_QUEUE
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: act_tid: 5, control.create_check:33*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_UPDATE
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 5, meter_profile_tbl_cache.rd:35*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER_PROFILE
	},
	/* cond_execute: act_tid: 5, control.shared_meter_profile_0:36*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.rd:37*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, control.meter_created_chk:38*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, control.shared_meter_profile_chk:39*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, control.delete_check:40*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 5, meter_profile_tbl_cache.del_chk:41*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER_PROFILE
	},
	/* cond_execute: act_tid: 5, control.mtr_prof_ref_cnt_chk:42*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.del_chk:43*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, control.shared_mtr_ref_cnt_chk:44*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.rd_update:45*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, meter_tbl.update_rd:46*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_NOT_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_PROP_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID_UPDATE
	},
	/* cond_reject: thor, act_tid: 6 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 6, shared_mirror_record.rd:49*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 6, control.mirror:50*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 6, int_flow_counter_tbl.0:51*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 6, int_vtag_encap_record.0:52*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 6, mod_record.dec_ttl_egr:53*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 6, int_full_act_record.0:54*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_reject: thor, act_tid: 7 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 7, shared_mirror_record.rd:57*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 7, control.mirror:58*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 7, int_flow_counter_tbl.0:59*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 7, mod_record.ing_ttl:60*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 7, mod_record.ing_no_ttl:61*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_reject: thor, act_tid: 8 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 8, shared_mirror_record.rd:63*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 8, control.mirror:64*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, int_flow_counter_tbl.0:65*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 8, source_property_cache.rd:66*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG
	},
	/* cond_execute: act_tid: 8, control.sp_rec_v4:67*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, sp_smac_ipv4.0:68*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG
	},
	/* cond_execute: act_tid: 8, source_property_ipv6_cache.rd:69*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG
	},
	/* cond_execute: act_tid: 8, control.sp_rec_v6:70*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, sp_smac_ipv6.0:71*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG
	},
	/* cond_execute: act_tid: 8, mod_record.ing_l2write:72*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_DST
	},
	/* cond_execute: act_tid: 8, vxlan_encap_rec_cache.rd:74*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, vxlan_encap_ipv6_rec_cache.rd:76*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, control.vxlan_v6_encap:78*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, int_tun_encap_record.ipv4_vxlan:79*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, int_tun_encap_record.ipv6_vxlan:81*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, vxlan_encap_rec_cache.wr:83*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, vxlan_encap_ipv6_rec_cache.wr:85*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, geneve_encap_rec_cache.rd:87*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_GENEVE
	},
	/* cond_execute: act_tid: 8, control.geneve_encap:88*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, int_geneve_encap_record.ipv4_geneve:89*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_GENEVE
	},
	/* cond_execute: act_tid: 8, int_geneve_encap_record.ipv6_geneve:91*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_GENEVE
	},
	/* cond_execute: act_tid: 8, geneve_encap_rec_cache.wr:93*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_GENEVE
	},
	/* cond_reject: thor, act_tid: 9 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_MULTIPLE_PORT
	},
	/* cond_execute: act_tid: 9, shared_mirror_record.rd:95*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 9, control.mirror:96*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 9, int_flow_counter_tbl.0:97*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 10, control.delete_chk:98*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 10, control.mirror_del_exist_chk:99*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 10, control.mirror_ref_cnt_chk:100*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 10, int_flow_counter_tbl.0:101*/
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	}
};

struct bnxt_ulp_mapper_key_info ulp_thor_act_key_info_list[] = {
	/* act_tid: 1, , table: shared_meter_tbl_cache.rd */
	{
	.field_info_mask = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER & 0xff}
		}
	},
	/* act_tid: 1, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 2, , table: shared_mirror_record.del_chk */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 2, , table: shared_mirror_record.wr */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
		}
	},
	/* act_tid: 3, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 4, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.rd */
	{
	.field_info_mask = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.wr */
	{
	.field_info_mask = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.rd */
	{
	.field_info_mask = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_INST_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_INST_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.rd2 */
	{
	.field_info_mask = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.wr */
	{
	.field_info_mask = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_INST_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_INST_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.del_chk */
	{
	.field_info_mask = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_profile_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.del_chk */
	{
	.field_info_mask = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_INST_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_INST_ID & 0xff}
		}
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.rd_update */
	{
	.field_info_mask = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "sw_meter_id",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_METER_INST_ID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_METER_INST_ID & 0xff}
		}
	},
	/* act_tid: 6, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 7, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 8, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 8, , table: source_property_cache.rd */
	{
	.field_info_mask = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_src_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_src_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV4_SADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_SADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: source_property_cache.wr */
	{
	.field_info_mask = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_src_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_src_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV4_SADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_SADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: source_property_ipv6_cache.rd */
	{
	.field_info_mask = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_src_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_src_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV6_SADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_SADDR & 0xff}
		}
	},
	/* act_tid: 8, , table: source_property_ipv6_cache.wr */
	{
	.field_info_mask = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_src_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_src_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV6_SADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_SADDR & 0xff}
		}
	},
	/* act_tid: 8, , table: vxlan_encap_rec_cache.rd */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.rd */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: vxlan_encap_rec_cache.wr */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.wr */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: geneve_encap_rec_cache.rd */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr2 = {
		(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr2 = {
		(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ver_opt_len_o_c_rsvd0",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ver_opt_len_o_c_rsvd0",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "proto_type",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "proto_type",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w0",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w0",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w1",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w1",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w2",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w2",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w3",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w3",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w5",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w5",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 8, , table: geneve_encap_rec_cache.wr */
	{
	.field_info_mask = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv4_dst_addr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr2 = {
		(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ipv6_dst_addr",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV6 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr2 = {
		(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_sport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "udp_dport",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "ver_opt_len_o_c_rsvd0",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "ver_opt_len_o_c_rsvd0",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "proto_type",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "proto_type",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "vni",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_VNI >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_VNI & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w0",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w0",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w1",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w1",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w2",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w2",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w3",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w3",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "opt_w5",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "opt_w5",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
		.field_opr1 = {
		(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 >> 8) & 0xff,
		BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tbl_scope",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	/* act_tid: 9, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 10, , table: shared_mirror_record.del_chk */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
		.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE & 0xff}
		}
	},
	/* act_tid: 10, , table: shared_mirror_record.wr */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
		}
	}
};

struct bnxt_ulp_mapper_field_info ulp_thor_act_key_ext_list[] = {
};

struct bnxt_ulp_mapper_field_info ulp_thor_act_result_field_list[] = {
	/* act_tid: 1, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, , table: mod_record.ing_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 1, , table: mod_record.ing_no_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 1, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_TERNARY_LIST,
	.field_src1 = BNXT_ULP_FIELD_SRC_LIST_OR,
	.field_opr1 = {
		(10 >> 8) & 0xff,
		10 & 0xff,
		(2 >> 8) & 0xff,
		2 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_METER & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_METER_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PTR_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_VLAN_DEL_RPT_STRIP_OUTER},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_DROP & 0xff}
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 1, , table: int_compact_act_record.0 */
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_TERNARY_LIST,
	.field_src1 = BNXT_ULP_FIELD_SRC_LIST_OR,
	.field_opr1 = {
		(12 >> 8) & 0xff,
		12 & 0xff,
		(2 >> 8) & 0xff,
		2 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_METER >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_METER & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_METER_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PTR_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_VLAN_DEL_RPT_STRIP_OUTER},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_DROP & 0xff}
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: mirror_tbl.alloc */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 13,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ignore_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "copy_ing_or_egr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 2, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: int_compact_act_record.0 */
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: mirror_tbl.wr */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 13,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ignore_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "copy_ing_or_egr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 2, , table: shared_mirror_record.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "mirror_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
	},
	/* act_tid: 3, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 3, , table: mod_record.ing_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 3, , table: mod_record.ing_no_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 3, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 4, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: vnic_interface_rss_config.0 */
	/* act_tid: 4, , table: vnic_interface_queue_config.0 */
	/* act_tid: 4, , table: int_compact_act_record.0 */
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RSS_VNIC >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RSS_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: int_compact_act_record.1 */
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 5, , table: meter_profile_tbl.0 */
	{
	.description = "cf",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_CF >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_CF & 0xff}
	},
	{
	.description = "pm",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM & 0xff}
	},
	{
	.description = "rfc2698",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698 >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698 & 0xff}
	},
	{
	.description = "cbsm",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBSM >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBSM & 0xff}
	},
	{
	.description = "ebsm",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBSM >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBSM & 0xff}
	},
	{
	.description = "cbnd",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND & 0xff}
	},
	{
	.description = "ebnd",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND & 0xff}
	},
	{
	.description = "cbs",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS & 0xff}
	},
	{
	.description = "ebs",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS & 0xff}
	},
	{
	.description = "cir",
	.field_bit_size = 17,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR & 0xff}
	},
	{
	.description = "eir",
	.field_bit_size = 17,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR & 0xff}
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "meter_profile_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 & 0xff}
	},
	/* act_tid: 5, , table: meter_tbl.0 */
	{
	.description = "bkt_c",
	.field_bit_size = 27,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(134217727 >> 24) & 0xff,
	(134217727 >> 16) & 0xff,
	(134217727 >> 8) & 0xff,
	134217727 & 0xff}
	},
	{
	.description = "bkt_e",
	.field_bit_size = 27,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(134217727 >> 24) & 0xff,
	(134217727 >> 16) & 0xff,
	(134217727 >> 8) & 0xff,
	134217727 & 0xff}
	},
	{
	.description = "mtr_val",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL & 0xff}
	},
	{
	.description = "ecn_rmp_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN & 0xff}
	},
	{
	.description = "meter_profile",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 & 0xff}
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "meter_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_METER_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PTR_0 & 0xff}
	},
	{
	.description = "sw_meter_profile_id",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID & 0xff}
	},
	/* act_tid: 5, , table: meter_tbl.update_wr */
	{
	.description = "bkt_c",
	.field_bit_size = 27,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(134217727 >> 24) & 0xff,
	(134217727 >> 16) & 0xff,
	(134217727 >> 8) & 0xff,
	134217727 & 0xff}
	},
	{
	.description = "bkt_e",
	.field_bit_size = 27,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(134217727 >> 24) & 0xff,
	(134217727 >> 16) & 0xff,
	(134217727 >> 8) & 0xff,
	134217727 & 0xff}
	},
	{
	.description = "mtr_val",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL_UPDATE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL_UPDATE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr3 = {
	(BNXT_ULP_RF_IDX_RF_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RF_0 & 0xff}
	},
	{
	.description = "ecn_rmp_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN_UPDATE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN_UPDATE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr3 = {
	(BNXT_ULP_RF_IDX_RF_1 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RF_1 & 0xff}
	},
	{
	.description = "meter_profile",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0 & 0xff}
	},
	/* act_tid: 6, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 6, , table: int_vtag_encap_record.0 */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff}
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff}
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff}
	},
	/* act_tid: 6, , table: mod_record.dec_ttl_egr */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 6, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_DROP & 0xff}
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 6, , table: int_compact_act_record.0 */
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_DROP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_DROP & 0xff}
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 7, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 7, , table: mod_record.ing_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 7, , table: mod_record.ing_no_ttl */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV6_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_IPV4_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 7, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_COUNT >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_COUNT & 0xff}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 8, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 8, , table: sp_smac_ipv4.0 */
	{
	.description = "smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
	},
	{
	.description = "ipv4_src_addr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_SADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_SADDR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 8, , table: source_property_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_SP_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_SP_PTR & 0xff}
	},
	/* act_tid: 8, , table: sp_smac_ipv6.0 */
	{
	.description = "smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_SMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_SMAC & 0xff}
	},
	{
	.description = "ipv6_src_addr",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_SADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_SADDR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 8, , table: source_property_ipv6_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_SP_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_SP_PTR & 0xff}
	},
	/* act_tid: 8, , table: mod_record.ing_l2write */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SET_MAC_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr2 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 8, , table: int_tun_encap_record.ipv4_vxlan */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_VALID_YES}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L2_EN_YES}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_TUN_TYPE_VXLAN}
	},
	{
	.description = "enc_eth_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
	},
	{
	.description = "enc_o_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_o_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_ihl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_IHL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_IHL & 0xff}
	},
	{
	.description = "enc_ipv4_tos",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_TOS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TOS & 0xff}
	},
	{
	.description = "enc_ipv4_pkt_id",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_PKT_ID >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PKT_ID & 0xff}
	},
	{
	.description = "enc_ipv4_frag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_FRAG >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_FRAG & 0xff}
	},
	{
	.description = "enc_ipv4_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TTL & 0xff}
	},
	{
	.description = "enc_ipv4_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PROTO & 0xff}
	},
	{
	.description = "enc_ipv4_daddr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff}
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
	},
	{
	.description = "enc_vxlan_flags",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_FLAGS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_FLAGS & 0xff}
	},
	{
	.description = "enc_vxlan_rsvd0",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 & 0xff}
	},
	{
	.description = "enc_vxlan_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
	},
	{
	.description = "enc_vxlan_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 & 0xff}
	},
	/* act_tid: 8, , table: int_tun_encap_record.ipv6_vxlan */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_VALID_YES}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L2_EN_YES}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_TUN_TYPE_VXLAN}
	},
	{
	.description = "enc_eth_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
	},
	{
	.description = "enc_o_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_o_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_vtc",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW & 0xff}
	},
	{
	.description = "enc_ipv6_zero",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "enc_ipv6_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_PROTO & 0xff}
	},
	{
	.description = "enc_ipv6_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_TTL & 0xff}
	},
	{
	.description = "enc_ipv6_daddr",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff}
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
	},
	{
	.description = "enc_vxlan_flags",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_FLAGS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_FLAGS & 0xff}
	},
	{
	.description = "enc_vxlan_rsvd0",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 & 0xff}
	},
	{
	.description = "enc_vxlan_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff}
	},
	{
	.description = "enc_vxlan_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 & 0xff}
	},
	/* act_tid: 8, , table: vxlan_encap_rec_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "enc_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	/* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "enc_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	/* act_tid: 8, , table: int_geneve_encap_record.ipv4_geneve */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_VALID_YES}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L2_EN_YES}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_TUN_TYPE_NGE}
	},
	{
	.description = "enc_eth_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
	},
	{
	.description = "enc_o_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_o_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_ihl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_IHL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_IHL & 0xff}
	},
	{
	.description = "enc_ipv4_tos",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_TOS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TOS & 0xff}
	},
	{
	.description = "enc_ipv4_pkt_id",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_PKT_ID >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PKT_ID & 0xff}
	},
	{
	.description = "enc_ipv4_frag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_FRAG >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_FRAG & 0xff}
	},
	{
	.description = "enc_ipv4_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TTL & 0xff}
	},
	{
	.description = "enc_ipv4_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PROTO & 0xff}
	},
	{
	.description = "enc_ipv4_daddr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV4_DADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_DADDR & 0xff}
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
	},
	{
	.description = "enc_geneve_ver_opt_len_o_c_rsvd0",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 & 0xff}
	},
	{
	.description = "enc_geneve_proto_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE & 0xff}
	},
	{
	.description = "enc_geneve_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_VNI & 0xff}
	},
	{
	.description = "enc_geneve_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_RSVD1 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w0",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w1",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w2",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w3",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w5",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 & 0xff}
	},
	/* act_tid: 8, , table: int_geneve_encap_record.ipv6_geneve */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_VALID_YES}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L2_EN_YES}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_ECV_TUN_TYPE_NGE}
	},
	{
	.description = "enc_eth_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_ETH_DMAC >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_ETH_DMAC & 0xff}
	},
	{
	.description = "enc_o_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_o_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OO_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_O_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_O_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_tag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TCI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TCI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_i_vlan_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_OI_VLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_I_VLAN_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_I_VLAN_TYPE & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_vtc",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW & 0xff}
	},
	{
	.description = "enc_ipv6_zero",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "enc_ipv6_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_PROTO & 0xff}
	},
	{
	.description = "enc_ipv6_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_TTL & 0xff}
	},
	{
	.description = "enc_ipv6_daddr",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_IPV6_DADDR >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_DADDR & 0xff}
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff}
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff}
	},
	{
	.description = "enc_geneve_ver_opt_len_o_c_rsvd0",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_VER_OPT_LEN_O_C_RSVD0 & 0xff}
	},
	{
	.description = "enc_geneve_proto_type",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_PROTO_TYPE & 0xff}
	},
	{
	.description = "enc_geneve_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_VNI & 0xff}
	},
	{
	.description = "enc_geneve_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_RSVD1 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w0",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W0 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w1",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W1 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w2",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W2 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w3",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W3 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W4 & 0xff}
	},
	{
	.description = "enc_geneve_opt_w5",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr1 = {
	(BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_GENEVE_OPT_W5 & 0xff}
	},
	/* act_tid: 8, , table: geneve_encap_rec_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "enc_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	/* act_tid: 8, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_SP_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_SP_PTR & 0xff}
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 9, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 9, , table: mod_record.vf_2_vf */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_PORT_TABLE,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID & 0xff,
		(BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA >> 8) & 0xff,
		BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA & 0xff}
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 9, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(ULP_THOR_SYM_LOOPBACK_PORT >> 8) & 0xff,
	ULP_THOR_SYM_LOOPBACK_PORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_SHARED_SAMPLE & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr2 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 10, , table: mirror_tbl.alloc */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 13,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ignore_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "copy_ing_or_egr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 10, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 10, , table: mod_record.vf_2_vf */
	{
	.description = "metadata_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "rem_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rem_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ivlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rep_add_ovlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ttl_update",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tun_md_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv6_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_ipv4_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_sport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dport_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_PORT_TABLE,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_ACT_PORT_ID & 0xff,
		(BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA >> 8) & 0xff,
		BNXT_ULP_PORT_TABLE_VF_FUNC_METADATA & 0xff}
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 10, , table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_PTR & 0xff}
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	(ULP_THOR_SYM_LOOPBACK_PORT >> 8) & 0xff,
	ULP_THOR_SYM_LOOPBACK_PORT & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cond_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_del_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 10, , table: mirror_tbl.wr */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "reserved",
	.field_bit_size = 13,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ignore_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "copy_ing_or_egr",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* act_tid: 10, , table: shared_mirror_record.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
	BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "mirror_id",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
	}
};

struct bnxt_ulp_mapper_ident_info ulp_thor_act_ident_list[] = {
	/* act_tid: 1, , table: shared_meter_tbl_cache.rd */
	{
	.description = "meter_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_METER_PTR_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 32
	},
	/* act_tid: 1, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 2, , table: shared_mirror_record.del_chk */
	{
	.description = "rid",
	.regfile_idx = BNXT_ULP_RF_IDX_RID,
	.ident_bit_size = 32,
	.ident_bit_pos = 0
	},
	/* act_tid: 3, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 4, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.rd2 */
	{
	.description = "meter_profile_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 32
	},
	/* act_tid: 5, , table: meter_profile_tbl_cache.del_chk */
	{
	.description = "rid",
	.regfile_idx = BNXT_ULP_RF_IDX_RID,
	.ident_bit_size = 32,
	.ident_bit_pos = 0
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.del_chk */
	{
	.description = "rid",
	.regfile_idx = BNXT_ULP_RF_IDX_RID,
	.ident_bit_size = 32,
	.ident_bit_pos = 0
	},
	/* act_tid: 5, , table: shared_meter_tbl_cache.rd_update */
	{
	.description = "meter_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_METER_PTR_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 32
	},
	/* act_tid: 5, , table: meter_tbl.update_rd */
	{
	.description = "ecn_rmp_en",
	.regfile_idx = BNXT_ULP_RF_IDX_RF_1,
	.ident_bit_size = 1,
	.ident_bit_pos = 55
	},
	{
	.description = "meter_profile",
	.regfile_idx = BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 56
	},
	{
	.description = "mtr_val",
	.regfile_idx = BNXT_ULP_RF_IDX_RF_0,
	.ident_bit_size = 1,
	.ident_bit_pos = 54
	},
	/* act_tid: 6, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 7, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: source_property_cache.rd */
	{
	.description = "sp_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.ident_bit_size = 32,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: source_property_ipv6_cache.rd */
	{
	.description = "sp_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.ident_bit_size = 16,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: vxlan_encap_rec_cache.rd */
	{
	.description = "enc_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.ident_bit_size = 32,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: vxlan_encap_ipv6_rec_cache.rd */
	{
	.description = "enc_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.ident_bit_size = 32,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: geneve_encap_rec_cache.rd */
	{
	.description = "enc_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.ident_bit_size = 32,
	.ident_bit_pos = 32
	},
	/* act_tid: 9, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 4,
	.ident_bit_pos = 32
	},
	/* act_tid: 10, , table: shared_mirror_record.del_chk */
	{
	.description = "rid",
	.regfile_idx = BNXT_ULP_RF_IDX_RID,
	.ident_bit_size = 32,
	.ident_bit_pos = 0
	}
};
