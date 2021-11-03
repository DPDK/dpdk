/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Fri Oct  8 11:41:10 2021 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header act list */
struct bnxt_ulp_mapper_tmpl_info ulp_wh_plus_act_tmpl_list[] = {
	/* act_tid: 1, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 0,
		.cond_nums = 9 }
	},
	/* act_tid: 2, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 5,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 14,
		.cond_nums = 0 }
	},
	/* act_tid: 3, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 12,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 15,
		.cond_nums = 0 }
	},
	/* act_tid: 4, egress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 5,
	.start_tbl_idx = 19,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 21,
		.cond_nums = 0 }
	},
	/* act_tid: 5, egress */
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 7,
	.start_tbl_idx = 24,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 29,
		.cond_nums = 0 }
	},
	/* act_tid: 6, egress */
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.num_tbls = 6,
	.start_tbl_idx = 31,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 35,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_wh_plus_act_tbl_list[] = {
	{ /* act_tid: 1, , table: shared_mirror_record.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TBL_TYPE_MIRROR_CONFIG,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 9,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.key_start_idx = 0,
	.blob_key_bit_size = 1,
	.key_bit_size = 1,
	.key_num_fields = 1,
	.ident_start_idx = 0,
	.ident_nums = 1
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
		.cond_start_idx = 10,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 1, , table: int_vtag_encap_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_8B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 11,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.record_size = 8,
	.result_start_idx = 1,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* act_tid: 1, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 12,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 12,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* act_tid: 1, , table: ext_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 13,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 38,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
	},
	{ /* act_tid: 2, , table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 14,
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
		.cond_start_idx = 14,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 75,
	.result_bit_size = 32,
	.result_num_fields = 6
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
		.cond_start_idx = 14,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 81,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 2, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 82,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* act_tid: 2, , table: ext_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 108,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
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
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MIRROR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 145,
	.result_bit_size = 32,
	.result_num_fields = 6
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
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.key_start_idx = 1,
	.blob_key_bit_size = 1,
	.key_bit_size = 1,
	.key_num_fields = 1,
	.result_start_idx = 151,
	.result_bit_size = 34,
	.result_num_fields = 2
	},
	{ /* act_tid: 3, , table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 15,
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
		.cond_start_idx = 16,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 153,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 3, , table: act_modify_ipv4_src.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
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
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 154,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* act_tid: 3, , table: act_modify_ipv4_dst.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 18,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 155,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* act_tid: 3, , table: int_encap_mac_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 19,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.record_size = 16,
	.result_start_idx = 156,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* act_tid: 3, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 19,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 167,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* act_tid: 3, , table: ext_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 20,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 193,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
	},
	{ /* act_tid: 4, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 21,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 230,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 4, , table: int_vtag_encap_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 22,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 8,
	.result_start_idx = 231,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* act_tid: 4, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 24,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 242,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* act_tid: 4, , table: ext_full_act_record.no_tag */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 25,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 268,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
	},
	{ /* act_tid: 4, , table: ext_full_act_record.one_tag */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 27,
		.cond_nums = 2 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 305,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
	},
	{ /* act_tid: 5, , table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1023,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 29,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP
	},
	{ /* act_tid: 5, , table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 30,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 342,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 5, , table: act_modify_ipv4_src.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 31,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 343,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* act_tid: 5, , table: act_modify_ipv4_dst.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 32,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 344,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* act_tid: 5, , table: int_encap_mac_record.dummy */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 33,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.record_size = 16,
	.result_start_idx = 345,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 11
	},
	{ /* act_tid: 5, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 33,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 356,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* act_tid: 5, , table: ext_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 34,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 382,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11
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
		.cond_start_idx = 35,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 419,
	.result_bit_size = 64,
	.result_num_fields = 1
	},
	{ /* act_tid: 6, , table: sp_smac_ipv4.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 36,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 16,
	.result_start_idx = 420,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 2
	},
	{ /* act_tid: 6, , table: sp_smac_ipv6.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 37,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_SP_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 24,
	.result_start_idx = 422,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 2
	},
	{ /* act_tid: 6, , table: int_tun_encap_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 38,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.record_size = 64,
	.result_start_idx = 424,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 30
	},
	{ /* act_tid: 6, , table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 39,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 454,
	.result_bit_size = 128,
	.result_num_fields = 26
	},
	{ /* act_tid: 6, , table: ext_full_act_record_vxlan.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 40,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.result_start_idx = 480,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 30
	}
};

struct bnxt_ulp_mapper_cond_info ulp_wh_plus_act_cond_list[] = {
	/* cond_reject: wh_plus, act_tid: 1 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV6_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_TP_SRC
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_DST
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV6_DST
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_TP_DST
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_VLAN_VID
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_VLAN_PCP
	},
	/* cond_execute: act_tid: 1, shared_mirror_record.rd */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	/* cond_execute: act_tid: 1, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 1, int_vtag_encap_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 1, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 1, ext_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	/* cond_execute: act_tid: 2, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 3, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 3, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 3, act_modify_ipv4_src.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_SRC
	},
	/* cond_execute: act_tid: 3, act_modify_ipv4_dst.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_DST
	},
	/* cond_execute: act_tid: 3, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 3, ext_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	/* cond_execute: act_tid: 4, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 4, int_vtag_encap_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 4, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 4, ext_full_act_record.no_tag */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 4, ext_full_act_record.one_tag */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	},
	/* cond_execute: act_tid: 5, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: act_tid: 5, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 5, act_modify_ipv4_src.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_SRC
	},
	/* cond_execute: act_tid: 5, act_modify_ipv4_dst.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_SET_IPV4_DST
	},
	/* cond_execute: act_tid: 5, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 5, ext_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	},
	/* cond_execute: act_tid: 6, int_flow_counter_tbl.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	/* cond_execute: act_tid: 6, sp_smac_ipv4.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG
	},
	/* cond_execute: act_tid: 6, sp_smac_ipv6.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG
	},
	/* cond_execute: act_tid: 6, int_tun_encap_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 6, int_full_act_record.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_NOT_SET,
	},
	/* cond_execute: act_tid: 6, ext_full_act_record_vxlan.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_EXT_MEM_IS_SET,
	}
};

struct bnxt_ulp_mapper_key_info ulp_wh_plus_act_key_info_list[] = {
	/* act_tid: 1, , table: shared_mirror_record.rd */
	{
	.field_info_mask = {
		.description = "shared_index",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 1,
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
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "shared_index",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_MIRROR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MIRROR_PTR_0 & 0xff}
		}
	}
};

struct bnxt_ulp_mapper_field_info ulp_wh_plus_act_result_field_list[] = {
	/* act_tid: 1, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, , table: int_vtag_encap_record.0 */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
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
	/* act_tid: 1, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
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
	ULP_WP_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_NONE}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff}
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, , table: ext_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
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
	ULP_WP_SYM_DECAP_FUNC_THRU_TUN},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_NONE}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff}
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MIRROR_ID_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MIRROR_ID_0 & 0xff}
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
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ign_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2,
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1}
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 2, , table: ext_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff}
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2,
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	1}
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.description = "enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ign_drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "reserved",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "sp_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_PLUS_SRC2_POST,
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	(1 >> 8) & 0xff,
	1 & 0xff}
	},
	/* act_tid: 3, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 3, , table: act_modify_ipv4_src.0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff}
	},
	/* act_tid: 3, , table: act_modify_ipv4_dst.0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff}
	},
	/* act_tid: 3, , table: int_encap_mac_record.0 */
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_L2_EN_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 3, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_TL2},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_L2}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 3, , table: ext_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR & 0xff}
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_TL2},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_L2}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: int_vtag_encap_record.0 */
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
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
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
	/* act_tid: 4, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: ext_full_act_record.no_tag */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 4, , table: ext_full_act_record.one_tag */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_ACT_BIT_POP_VLAN & 0xff}
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	ULP_WP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
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
	/* act_tid: 5, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 5, , table: act_modify_ipv4_src.0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff}
	},
	/* act_tid: 5, , table: act_modify_ipv4_dst.0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff}
	},
	/* act_tid: 5, , table: int_encap_mac_record.dummy */
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_L2_EN_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 5, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_TL2},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_L2}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 5, , table: ext_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR & 0xff}
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
	BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_TL2},
	.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr3 = {
	ULP_WP_SYM_DECAP_FUNC_THRU_L2}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 6, , table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 6, , table: sp_smac_ipv4.0 */
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
	/* act_tid: 6, , table: sp_smac_ipv6.0 */
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
	/* act_tid: 6, , table: int_tun_encap_record.0 */
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	ULP_WP_SYM_ECV_L2_EN_YES}
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
	ULP_WP_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_TUN_TYPE_VXLAN}
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
	(BNXT_ULP_ENC_FIELD_IPV4_IHL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_IHL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_tos",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_TOS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TOS & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_pkt_id",
	.field_bit_size = 16,
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
	(BNXT_ULP_ENC_FIELD_IPV4_PKT_ID >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PKT_ID & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_frag",
	.field_bit_size = 16,
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
	(BNXT_ULP_ENC_FIELD_IPV4_FRAG >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_FRAG & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_ttl",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TTL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_proto",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PROTO & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_daddr",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_vtc",
	.field_bit_size = 32,
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
	(BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_zero",
	.field_bit_size = 16,
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
	.field_src2 = BNXT_ULP_FIELD_SRC_ZERO,
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_proto",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV6_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_PROTO & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_ttl",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV6_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_TTL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_daddr",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_UDP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_UDP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_flags",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_FLAGS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_FLAGS & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_rsvd0",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	/* act_tid: 6, , table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 6, , table: ext_full_act_record_vxlan.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
	BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
	(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
	BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
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
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_VALID_YES}
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
	ULP_WP_SYM_ECV_L2_EN_YES}
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
	ULP_WP_SYM_ECV_L4_TYPE_UDP_CSUM}
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_WP_SYM_ECV_TUN_TYPE_VXLAN}
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
	(BNXT_ULP_ENC_FIELD_IPV4_IHL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_IHL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_tos",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_TOS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TOS & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_pkt_id",
	.field_bit_size = 16,
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
	(BNXT_ULP_ENC_FIELD_IPV4_PKT_ID >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PKT_ID & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_frag",
	.field_bit_size = 16,
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
	(BNXT_ULP_ENC_FIELD_IPV4_FRAG >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_FRAG & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_ttl",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_TTL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_proto",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV4_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV4_PROTO & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv4_daddr",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_vtc",
	.field_bit_size = 32,
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
	(BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_VTC_FLOW & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_zero",
	.field_bit_size = 16,
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
	.field_src2 = BNXT_ULP_FIELD_SRC_ZERO,
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_proto",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV6_PROTO >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_PROTO & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_ttl",
	.field_bit_size = 8,
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
	(BNXT_ULP_ENC_FIELD_IPV6_TTL >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_IPV6_TTL & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_ipv6_daddr",
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
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_udp_sport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_UDP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_UDP_SPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_SPORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_udp_dport",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_UDP >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_UDP & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_UDP_DPORT >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_UDP_DPORT & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_flags",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_FLAGS >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_FLAGS & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_rsvd0",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_vni",
	.field_bit_size = 24,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_VNI >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_VNI & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	},
	{
	.description = "enc_vxlan_rsvd1",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_ENC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_T_VXLAN & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_ENC_FIELD,
	.field_opr2 = {
	(BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 >> 8) & 0xff,
	BNXT_ULP_ENC_FIELD_VXLAN_RSVD1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_SKIP
	}
};

struct bnxt_ulp_mapper_ident_info ulp_wh_plus_act_ident_list[] = {
	/* act_tid: 1, , table: shared_mirror_record.rd */
	{
	.description = "mirror_id",
	.regfile_idx = BNXT_ULP_RF_IDX_MIRROR_ID_0,
	.ident_bit_size = 2,
	.ident_bit_pos = 32
	}
};
