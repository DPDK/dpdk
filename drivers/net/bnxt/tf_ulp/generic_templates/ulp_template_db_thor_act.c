/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
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
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 0,
		.cond_nums = 0 }
	},
	/* act_tid: 2, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 10,
	.start_tbl_idx = 9,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 9,
		.cond_nums = 0 }
	},
	/* act_tid: 3, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 6,
	.start_tbl_idx = 19,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 13,
		.cond_nums = 0 }
	},
	/* act_tid: 4, ingress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 7,
	.start_tbl_idx = 25,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 18,
		.cond_nums = 0 }
	},
	/* act_tid: 5, ingress */
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 20,
	.start_tbl_idx = 32,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 25,
		.cond_nums = 0 }
	},
	/* act_tid: 6, egress */
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 7,
	.start_tbl_idx = 52,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 40,
		.cond_nums = 0 }
	},
	/* act_tid: 7, egress */
	[7] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 6,
	.start_tbl_idx = 59,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 47,
		.cond_nums = 0 }
	},
	/* act_tid: 8, egress */
	[8] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 15,
	.start_tbl_idx = 65,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 52,
		.cond_nums = 0 }
	},
	/* act_tid: 9, egress */
	[9] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 5,
	.start_tbl_idx = 80,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 68,
		.cond_nums = 0 }
	},
	/* act_tid: 10, egress */
	[10] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 11,
	.start_tbl_idx = 85,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 71,
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
		.cond_start_idx = 0,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 1,
		.cond_nums = 1 },
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
		.cond_start_idx = 2,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 1,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 3,
		.cond_nums = 1 },
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
		.cond_start_idx = 4,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
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
		.cond_start_idx = 5,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 1,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 6,
		.cond_nums = 3 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 48,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 9,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 95,
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
		.cond_start_idx = 9,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 112,
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
		.cond_start_idx = 9,
		.cond_nums = 1 },
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
		.cond_start_idx = 10,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 2,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 10,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 2, , table: control.mirror_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 11,
		.cond_nums = 1 },
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
		.cond_start_idx = 12,
		.cond_nums = 0 },
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
		.cond_start_idx = 12,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 125,
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
		.cond_start_idx = 12,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 130,
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
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 131,
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
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 144,
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
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_INC,
	.key_start_idx = 3,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
	.key_num_fields = 1,
	.result_start_idx = 149,
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
		.cond_start_idx = 13,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 4,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 14,
		.cond_nums = 1 },
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
		.cond_start_idx = 15,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 151,
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
		.cond_start_idx = 16,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 152,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 17,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 199,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 246,
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
		.cond_start_idx = 18,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 5,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 19,
		.cond_nums = 1 },
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
		.cond_start_idx = 20,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 263,
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
		.cond_start_idx = 21,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_RSS_VNIC,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 264,
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
		.cond_start_idx = 22,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_VNIC_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_RSS_VNIC,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 264,
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
		.cond_start_idx = 23,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 264,
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
		.cond_start_idx = 25,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 277,
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
		.cond_start_idx = 25,
		.cond_nums = 2 },
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
		.cond_start_idx = 27,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 28,
		.cond_nums = 1 },
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
		.cond_start_idx = 29,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PROFILE_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 290,
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
		.cond_start_idx = 29,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.key_start_idx = 7,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.result_start_idx = 301,
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
		.cond_start_idx = 29,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 30,
		.cond_nums = 1 },
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
		.cond_start_idx = 31,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 31,
		.cond_nums = 1 },
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
		.cond_start_idx = 32,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 303,
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
		.cond_start_idx = 32,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.key_start_idx = 10,
	.blob_key_bit_size = 32,
	.key_bit_size = 32,
	.key_num_fields = 1,
	.result_start_idx = 308,
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
		.cond_start_idx = 32,
		.cond_nums = 1 },
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
		.cond_start_idx = 33,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 34,
		.cond_nums = 1 },
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
		.cond_start_idx = 35,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 36,
		.cond_nums = 1 },
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
		.cond_start_idx = 37,
		.cond_nums = 0 },
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
		.cond_start_idx = 37,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
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
		.cond_start_idx = 38,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_RD_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
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
		.cond_start_idx = 40,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_METER_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.result_start_idx = 311,
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
		.cond_start_idx = 40,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 14,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 41,
		.cond_nums = 1 },
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
		.cond_start_idx = 42,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 316,
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
		.cond_start_idx = 43,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 317,
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
		.cond_start_idx = 44,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 328,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 45,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 375,
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
		.cond_start_idx = 47,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 392,
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
		.cond_start_idx = 47,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 15,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 48,
		.cond_nums = 1 },
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
		.cond_start_idx = 49,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 405,
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
		.cond_start_idx = 50,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 406,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 51,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 453,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 52,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 500,
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
		.cond_start_idx = 52,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 16,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
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
		.cond_start_idx = 53,
		.cond_nums = 1 },
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
		.cond_start_idx = 54,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 517,
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
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 55,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 17,
	.blob_key_bit_size = 80,
	.key_bit_size = 80,
	.key_num_fields = 2,
	.ident_start_idx = 15,
	.ident_nums = 1
	},
	{ /* act_tid: 8, , table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 56,
		.cond_nums = 1 },
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
		.cond_start_idx = 57,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.record_size = 16,
	.result_start_idx = 518,
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
		.cond_start_idx = 58,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 19,
	.blob_key_bit_size = 80,
	.key_bit_size = 80,
	.key_num_fields = 2,
	.result_start_idx = 521,
	.result_bit_size = 48,
	.result_num_fields = 2
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
		.cond_start_idx = 58,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 32,
	.result_start_idx = 523,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3
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
		.cond_start_idx = 59,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 21,
	.blob_key_bit_size = 136,
	.key_bit_size = 136,
	.key_num_fields = 5,
	.ident_start_idx = 16,
	.ident_nums = 1
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
		.cond_start_idx = 61,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 526,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
	},
	{ /* act_tid: 8, , table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 63,
		.cond_nums = 1 },
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
		.cond_start_idx = 64,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 573,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 30
	},
	{ /* act_tid: 8, , table: vxlan_encap_rec_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 66,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 26,
	.blob_key_bit_size = 136,
	.key_bit_size = 136,
	.key_num_fields = 5,
	.result_start_idx = 603,
	.result_bit_size = 48,
	.result_num_fields = 2
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
		.cond_start_idx = 66,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 605,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 30
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
		.cond_start_idx = 68,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 635,
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
		.cond_start_idx = 68,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.key_start_idx = 31,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
	.key_num_fields = 1,
	.ident_start_idx = 17,
	.ident_nums = 1
	},
	{ /* act_tid: 9, , table: control.mirror */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 69,
		.cond_nums = 1 },
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
		.cond_start_idx = 70,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 652,
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
		.cond_start_idx = 71,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 653,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 71,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 700,
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
		.cond_start_idx = 71,
		.cond_nums = 1 },
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
		.cond_start_idx = 72,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_NOP,
	.key_start_idx = 32,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
	.key_num_fields = 1,
	.ident_start_idx = 18,
	.ident_nums = 1
	},
	{ /* act_tid: 10, , table: control.mirror_del_exist_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 72,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 10, , table: control.mirror_ref_cnt_chk */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 1023,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 73,
		.cond_nums = 1 },
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
		.cond_start_idx = 74,
		.cond_nums = 0 },
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
		.cond_start_idx = 74,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 717,
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
		.cond_start_idx = 74,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 722,
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
		.cond_start_idx = 75,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.result_start_idx = 723,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 47
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
		.cond_start_idx = 75,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 770,
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
		.cond_start_idx = 75,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 787,
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
		.cond_start_idx = 75,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.ref_cnt_opcode = BNXT_ULP_REF_CNT_OPC_INC,
	.key_start_idx = 33,
	.blob_key_bit_size = 4,
	.key_bit_size = 4,
	.key_num_fields = 1,
	.result_start_idx = 792,
	.result_bit_size = 36,
	.result_num_fields = 2
	}
};

struct bnxt_ulp_mapper_cond_info ulp_thor_act_cond_list[] = {
	/* cond_execute: act_tid: 1, shared_meter_tbl_cache.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER
	},
	/* cond_execute: act_tid: 1, control.meter_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 1, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 1, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 1, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 1, mod_record.ing_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 1, mod_record.ing_no_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_DST
	},
	/* cond_execute: act_tid: 2, control.delete_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 2, control.mirror_del_exist_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 2, control.mirror_ref_cnt_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 2, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 3, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 3, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 3, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 3, mod_record.ing_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 3, mod_record.ing_no_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 4, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 4, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 4, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 4, vnic_interface_rss_config.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: act_tid: 4, vnic_interface_queue_config.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_QUEUE
	},
	/* cond_execute: act_tid: 4, int_compact_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_QUEUE
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: act_tid: 5, control.create_check */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_UPDATE
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 5, meter_profile_tbl_cache.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER_PROFILE
	},
	/* cond_execute: act_tid: 5, control.shared_meter_profile_0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, control.meter_created_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, control.shared_meter_profile_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, control.delete_check */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 5, meter_profile_tbl_cache.del_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_METER_PROFILE
	},
	/* cond_execute: act_tid: 5, control.mtr_prof_ref_cnt_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.del_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, control.shared_mtr_ref_cnt_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 5, shared_meter_tbl_cache.rd_update */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_METER
	},
	/* cond_execute: act_tid: 5, meter_tbl.update_rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_NOT_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_PROP_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID_UPDATE
	},
	/* cond_execute: act_tid: 6, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 6, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 6, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 6, int_vtag_encap_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 6, mod_record.dec_ttl_egr */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 6, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 7, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 7, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 7, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 7, mod_record.ing_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 7, mod_record.ing_no_ttl */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DEC_TTL
	},
	/* cond_execute: act_tid: 8, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 8, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 8, source_property_cache.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG
	},
	/* cond_execute: act_tid: 8, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, sp_smac_ipv4.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG
	},
	/* cond_execute: act_tid: 8, sp_smac_ipv6.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG
	},
	/* cond_execute: act_tid: 8, vxlan_encap_rec_cache.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, mod_record.ing_l2write */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_MAC_DST
	},
	/* cond_execute: act_tid: 8, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 8, int_tun_encap_record.ipv4_vxlan */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 8, int_tun_encap_record.ipv6_vxlan */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ENC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_VXLAN
	},
	/* cond_execute: act_tid: 9, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 9, control.mirror */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 9, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 10, control.delete_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_DELETE
	},
	/* cond_execute: act_tid: 10, control.mirror_del_exist_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 10, control.mirror_ref_cnt_chk */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	},
	/* cond_execute: act_tid: 10, int_flow_counter_tbl.0 */
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
	/* act_tid: 9, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
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
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
		}
	}
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_THOR_SYM_DECAP_FUNC_NONE}
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
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_THOR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_THOR_SYM_DECAP_FUNC_NONE}
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
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
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_bit_size = 16,
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
	.description = "metadata_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_rsvd",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_op",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "metadata_prof",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.description = "enc_ipv6_vtc",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv6_zero",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv6_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv6_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv6_daddr",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
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
	.description = "enc_ipv4_ihl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_tos",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_pkt_id",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_frag",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_ttl",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_proto",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "enc_ipv4_daddr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	{
	.description = "ivlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ivlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_tpid",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_pri",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_de",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ovlan_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_pfid",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "alt_vid",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_rsvd",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "ttl_il3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_new_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_ex_prot",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "tun_mv",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "reserved",
	.field_bit_size = 0,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_dmac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l2_smac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv6",
	.field_bit_size = 128,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_sip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l3_dip_ipv4",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
	},
	{
	.description = "l4_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SKIP
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
	.ident_bit_size = 16,
	.ident_bit_pos = 32
	},
	/* act_tid: 8, , table: vxlan_encap_rec_cache.rd */
	{
	.description = "enc_rec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.ident_bit_size = 16,
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
