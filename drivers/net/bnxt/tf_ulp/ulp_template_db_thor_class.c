/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Mon Apr  5 11:35:38 2021 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header class list */
struct bnxt_ulp_mapper_tmpl_info ulp_thor_class_tmpl_list[] = {
	/* class_tid: 1, thor, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 47,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 0,
		.cond_nums = 0 }
	},
	/* class_tid: 2, thor, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 13,
	.start_tbl_idx = 47,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 17,
		.cond_nums = 0 }
	},
	/* class_tid: 3, thor, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 1,
	.start_tbl_idx = 60,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 20,
		.cond_nums = 0 }
	},
	/* class_tid: 4, thor, ingress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 9,
	.start_tbl_idx = 61,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 20,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_thor_class_tbl_list[] = {
	{ /* class_tid: 1, thor, table: port_table.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 0,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 0,
	.blob_key_bit_size = 10,
	.key_bit_size = 10,
	.key_num_fields = 1,
	.ident_start_idx = 0,
	.ident_nums = 3
	},
	{ /* class_tid: 1, thor, table: control.check_gre */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 17,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 0,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: l2_cntxt_tcam_cache.gre_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 1,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.ident_start_idx = 3,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.gre_hit */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 42,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 1,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: l2_cntxt_tcam.gre */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 2,
	.blob_key_bit_size = 213,
	.key_bit_size = 213,
	.key_num_fields = 21,
	.result_start_idx = 0,
	.result_bit_size = 43,
	.result_num_fields = 6,
	.ident_start_idx = 3,
	.ident_nums = 1
	},
	{ /* class_tid: 1, thor, table: l2_cntxt_tcam_cache.gre_wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 23,
	.blob_key_bit_size = 8,
	.key_bit_size = 8,
	.key_num_fields = 1,
	.result_start_idx = 6,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 1, thor, table: fkb_select.gre */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 10,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: profile_tcam.gre */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 24,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 116,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: wm.gre */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 2,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 67,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 124,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: wm.gre_low */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 3,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 181,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 129,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: mac_addr_cache.gre_frag_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 3,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 295,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.gre_frag_mac_hit */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 3,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 300,
	.blob_key_bit_size = 213,
	.key_bit_size = 213,
	.key_num_fields = 21,
	.result_start_idx = 134,
	.result_bit_size = 43,
	.result_num_fields = 6,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: mac_addr_cache.gre_frag_wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 321,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.result_start_idx = 140,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 1, thor, table: fkb_select.gre_frag */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 144,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: profile_tcam.gre_frag */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 4,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 326,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 250,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: wm.gre_frag */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 29,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 4,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 369,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 258,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: wm.gre_frag_low */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 28,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 5,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 483,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 263,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: mac_addr_cache.non_gre_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 5,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 597,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.non_gre_mac */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 5,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 6,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 602,
	.blob_key_bit_size = 213,
	.key_bit_size = 213,
	.key_num_fields = 21,
	.result_start_idx = 268,
	.result_bit_size = 43,
	.result_num_fields = 6,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: mac_addr_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 6,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 623,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.result_start_idx = 274,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 1, thor, table: control.icmpv4_test */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 8,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 6,
		.cond_nums = 2 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.icmpv4_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 8,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 628,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 4,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.icmpv4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 8,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: fkb_select.icmpv4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 9,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 278,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: profile_tcam.icmpv4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 9,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 631,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 384,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 4,
	.ident_nums = 1
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.icmpv4_wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 9,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 674,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 392,
	.result_bit_size = 82,
	.result_num_fields = 7
	},
	{ /* class_tid: 1, thor, table: wm.icmpv4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 17,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 9,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 677,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 399,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: wm.icmpv4_low */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 16,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 10,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 791,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 404,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: control.icmpv6_test */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 8,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 10,
		.cond_nums = 2 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.icmpv6_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 12,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 905,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 5,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.icmpv6 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 12,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: fkb_select.icmpv6 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 409,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: profile_tcam.icmpv6 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 908,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 515,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 5,
	.ident_nums = 1
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.icmpv6_wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 13,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 951,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 523,
	.result_bit_size = 82,
	.result_num_fields = 7
	},
	{ /* class_tid: 1, thor, table: wm.icmpv6 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 9,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 13,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 954,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 530,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: wm.icmpv6_low */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 8,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 14,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 1068,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 535,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.l3_l4_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 14,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1182,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 6,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: control.l3_l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 14,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: fkb_select.l3_l4_wm */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 540,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: profile_tcam.l3_l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1185,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 646,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 6,
	.ident_nums = 0
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 15,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1228,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 654,
	.result_bit_size = 82,
	.result_num_fields = 7
	},
	{ /* class_tid: 1, thor, table: wm.l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 2,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 15,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 1231,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 661,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: wm.l4_low */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM_LOW,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 16,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 1345,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 666,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 1, thor, table: control.check_rss_action */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 16,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: control.rss_config */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 17,
		.cond_nums = 0 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_RSS_CONFIG,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC },
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 2, thor, table: port_table.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 17,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1459,
	.blob_key_bit_size = 10,
	.key_bit_size = 10,
	.key_num_fields = 1,
	.ident_start_idx = 6,
	.ident_nums = 3
	},
	{ /* class_tid: 2, thor, table: mac_addr_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 17,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1460,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.ident_start_idx = 9,
	.ident_nums = 0
	},
	{ /* class_tid: 2, thor, table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 17,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 2, thor, table: l2_cntxt_tcam.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1465,
	.blob_key_bit_size = 213,
	.key_bit_size = 213,
	.key_num_fields = 21,
	.result_start_idx = 671,
	.result_bit_size = 43,
	.result_num_fields = 6,
	.ident_start_idx = 9,
	.ident_nums = 1
	},
	{ /* class_tid: 2, thor, table: mac_addr_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1486,
	.blob_key_bit_size = 73,
	.key_bit_size = 73,
	.key_num_fields = 5,
	.result_start_idx = 677,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 2, thor, table: profile_tcam_cache.l3_l4_rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 18,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1491,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 10,
	.ident_nums = 0
	},
	{ /* class_tid: 2, thor, table: control.l3_l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 4,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 18,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 2, thor, table: fkb_select.l3_l4_wm */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 19,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 681,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 2, thor, table: profile_tcam.l3_l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 19,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1494,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 787,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 10,
	.ident_nums = 0
	},
	{ /* class_tid: 2, thor, table: profile_tcam_cache.wr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 19,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1537,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 795,
	.result_bit_size = 82,
	.result_num_fields = 7
	},
	{ /* class_tid: 2, thor, table: wm.l4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type = TF_TCAM_TBL_TYPE_WC_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 19,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_TCAM_INDEX_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 1540,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 802,
	.result_bit_size = 38,
	.result_num_fields = 5
	},
	{ /* class_tid: 2, thor, table: control.check_rss_action */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 19,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 2, thor, table: control.rss_config */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 20,
		.cond_nums = 0 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_RSS_CONFIG,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC },
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 3, thor, table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 20,
		.cond_nums = 0 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 4, thor, table: control.get_parent_mac_addr */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 20,
		.cond_nums = 0 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_GET_PARENT_MAC_ADDR,
		.func_dst_opr = BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC },
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 4, thor, table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_AND,
		.cond_start_idx = 20,
		.cond_nums = 1 },
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.func_info = {
		.func_opc = BNXT_ULP_FUNC_OPC_EQ,
		.func_src1 = BNXT_ULP_FUNC_SRC_COMP_FIELD,
		.func_opr1 = BNXT_ULP_CF_IDX_PHY_PORT_VPORT,
		.func_src2 = BNXT_ULP_FUNC_SRC_CONST,
		.func_opr2 = 1,
		.func_dst_opr = BNXT_ULP_RF_IDX_CC },
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 4, thor, table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 807,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* class_tid: 4, thor, table: port_table.wr_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 3,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1654,
	.blob_key_bit_size = 10,
	.key_bit_size = 10,
	.key_num_fields = 1,
	.result_start_idx = 824,
	.result_bit_size = 152,
	.result_num_fields = 5
	},
	{ /* class_tid: 4, thor, table: int_full_act_record.1 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.shared_session = BNXT_ULP_SHARED_SESSION_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 829,
	.result_bit_size = 128,
	.result_num_fields = 17
	},
	{ /* class_tid: 4, thor, table: port_table.wr_1 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1655,
	.blob_key_bit_size = 10,
	.key_bit_size = 10,
	.key_num_fields = 1,
	.result_start_idx = 846,
	.result_bit_size = 152,
	.result_num_fields = 5
	},
	{ /* class_tid: 4, thor, table: port_table.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 1656,
	.blob_key_bit_size = 10,
	.key_bit_size = 10,
	.key_num_fields = 1,
	.ident_start_idx = 10,
	.ident_nums = 1
	},
	{ /* class_tid: 4, thor, table: parif_def_arec_ptr.ing_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_DFLT_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 851,
	.result_bit_size = 32,
	.result_num_fields = 1
	},
	{ /* class_tid: 4, thor, table: parif_def_err_arec_ptr.ing_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IF_TABLE,
	.resource_type = TF_IF_TBL_TYPE_PROF_PARIF_ERR_ACT_REC_PTR,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 21,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_IF_TBL_OPC_WR_COMP_FIELD,
	.tbl_operand = BNXT_ULP_CF_IDX_PHY_PORT_PARIF,
	.fdb_opcode = BNXT_ULP_FDB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 852,
	.result_bit_size = 32,
	.result_num_fields = 1
	}
};

struct bnxt_ulp_mapper_cond_info ulp_thor_class_cond_list[] = {
	/* cond_execute: class_tid: 1, control.check_gre */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_T_GRE
	},
	/* cond_execute: class_tid: 1, control.gre_hit */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, wm.gre */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG
	},
	/* cond_execute: class_tid: 1, control.gre_frag_mac_hit */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, wm.gre_frag */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG
	},
	/* cond_execute: class_tid: 1, control.non_gre_mac */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, control.icmpv4_test */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV4
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_ICMP
	},
	/* cond_execute: class_tid: 1, control.icmpv4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, wm.icmpv4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG
	},
	/* cond_execute: class_tid: 1, control.icmpv6_test */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_IPV6
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_HDR_BIT_IS_SET,
	.cond_operand = BNXT_ULP_HDR_BIT_O_ICMP
	},
	/* cond_execute: class_tid: 1, control.icmpv6 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, wm.icmpv6 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG
	},
	/* cond_execute: class_tid: 1, control.l3_l4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, wm.l4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_CF_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_WC_IS_HA_HIGH_REG
	},
	/* cond_execute: class_tid: 1, control.check_rss_action */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: class_tid: 2, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 2, control.l3_l4 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 2, control.check_rss_action */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_RSS
	},
	/* cond_execute: class_tid: 4, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_CC
	}
};

struct bnxt_ulp_mapper_key_info ulp_thor_class_key_info_list[] = {
	/* class_tid: 1, thor, table: port_table.rd */
	{
	.field_info_mask = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam_cache.gre_rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.gre */
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_GRE}
		}
	},
	{
	.field_info_mask = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam_cache.gre_wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam.gre */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: wm.gre */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		47}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: wm.gre_low */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		47}
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.gre_frag_rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
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
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
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
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		2}
		},
	.field_info_spec = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.gre_frag_wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
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
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam.gre_frag */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		16}
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		16}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: wm.gre_frag */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		47}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: wm.gre_frag_low */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		47}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.non_gre_rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
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
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		2}
		},
	.field_info_spec = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv4_rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv4 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_TYPE_ICMP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv4_wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: wm.icmpv4 */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			1},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		58}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: wm.icmpv4_low */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			1},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		58}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv6_rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv6 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_TYPE_ICMP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv6_wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: wm.icmpv6 */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			1},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		58}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: wm.icmpv6_low */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			1},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		58}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.l3_l4_rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam.l3_l4 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L3_HDR_TYPE_IPV4},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: wm.l4 */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_IP_PROTO_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_IP_PROTO_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 1, thor, table: wm.l4_low */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_IP_PROTO_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_IP_PROTO_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 2, thor, table: port_table.rd */
	{
	.field_info_mask = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	/* class_tid: 2, thor, table: mac_addr_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 2, thor, table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
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
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		2}
		},
	.field_info_spec = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 2, thor, table: mac_addr_cache.wr */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		},
	.field_info_spec = {
		.description = "tun_hdr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_TUN_HDR_TYPE_NONE}
		}
	},
	{
	.field_info_mask = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "one_tag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_FIELD_BIT,
		.field_opr1 = {
		(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr3 = {
		(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
		BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
		}
	},
	/* class_tid: 2, thor, table: profile_tcam_cache.l3_l4_rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 2, thor, table: profile_tcam.l3_l4 */
	{
	.field_info_mask = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L4_HDR_TYPE_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L3_HDR_TYPE_IPV4},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
		1}
		}
	},
	/* class_tid: 2, thor, table: profile_tcam_cache.wr */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 2, thor, table: wm.l4 */
	{
	.field_info_mask = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "wc_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
		(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr2 = {
			(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr3 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl3.l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tl4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3.prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_IP_PROTO_TCP},
		.field_src3 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr3 = {
		ULP_THOR_SYM_IP_PROTO_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l3.l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		},
	.field_info_spec = {
		.description = "l4.dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
		.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
		.field_opr1 = {
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src3 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr3 = {
		(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
		BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		},
	.field_info_spec = {
		.description = "l4.err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_SKIP
		}
	},
	/* class_tid: 4, thor, table: port_table.wr_0 */
	{
	.field_info_mask = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	/* class_tid: 4, thor, table: port_table.wr_1 */
	{
	.field_info_mask = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	},
	/* class_tid: 4, thor, table: port_table.rd */
	{
	.field_info_mask = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_ONES,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "dev.port_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
		(BNXT_ULP_CF_IDX_DEV_PORT_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DEV_PORT_ID & 0xff}
		}
	}
};

struct bnxt_ulp_mapper_field_info ulp_thor_class_result_field_list[] = {
	/* class_tid: 1, thor, table: l2_cntxt_tcam.gre */
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1 & 0xff}
	},
	{
	.description = "ctxt_meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "def_ctxt_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	},
	{
	.description = "ctxt_opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_CTXT_OPCODE_NORMAL_FLOW}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam_cache.gre_wr */
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
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: fkb_select.gre */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.gre */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: wm.gre */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: wm.gre_low */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
	},
	{
	.description = "ctxt_meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "def_ctxt_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	},
	{
	.description = "ctxt_opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_CTXT_OPCODE_NORMAL_FLOW}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.gre_frag_wr */
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
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: fkb_select.gre_frag */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.gre_frag */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: wm.gre_frag */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: wm.gre_frag_low */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
	},
	{
	.description = "ctxt_meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "def_ctxt_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	},
	{
	.description = "ctxt_opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_CTXT_OPCODE_NORMAL_FLOW}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	/* class_tid: 1, thor, table: mac_addr_cache.wr */
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
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: fkb_select.icmpv4 */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv4_wr */
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
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: wm.icmpv4 */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: wm.icmpv4_low */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: fkb_select.icmpv6 */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv6 */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.icmpv6_wr */
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
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: wm.icmpv6 */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: wm.icmpv6_low */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: fkb_select.l3_l4_wm */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.l3_l4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.wr */
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
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: wm.l4 */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 1, thor, table: wm.l4_low */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 2, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0 & 0xff}
	},
	{
	.description = "ctxt_meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "def_ctxt_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	},
	{
	.description = "ctxt_opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	ULP_THOR_SYM_CTXT_OPCODE_NORMAL_FLOW}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_PHY_PORT >> 8) & 0xff,
	BNXT_ULP_RF_IDX_PHY_PORT & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
	(BNXT_ULP_CF_IDX_PHY_PORT_PARIF >> 8) & 0xff,
	BNXT_ULP_CF_IDX_PHY_PORT_PARIF & 0xff}
	},
	/* class_tid: 2, thor, table: mac_addr_cache.wr */
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
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, thor, table: fkb_select.l3_l4_wm */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, thor, table: profile_tcam.l3_l4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1_THEN_SRC2_ELSE_SRC3,
	.field_src1 = BNXT_ULP_FIELD_SRC_HDR_BIT,
	.field_opr1 = {
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
	((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
	(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr2 = {
		(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0 & 0xff},
	.field_src3 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr3 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1 & 0xff}
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, thor, table: profile_tcam_cache.wr */
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
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, thor, table: wm.l4 */
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	/* class_tid: 4, thor, table: int_full_act_record.0 */
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_PORT_TABLE,
	.field_opr1 = {
		(BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC >> 8) & 0xff,
		BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC & 0xff}
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
	/* class_tid: 4, thor, table: port_table.wr_0 */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drv_func.mac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drv_func.parent.mac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
	},
	{
	.description = "phy_port",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "default_arec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0 & 0xff}
	},
	/* class_tid: 4, thor, table: int_full_act_record.1 */
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
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
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_PORT_TABLE,
	.field_opr1 = {
		(BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC >> 8) & 0xff,
		BNXT_ULP_PORT_TABLE_DRV_FUNC_PARENT_VNIC & 0xff}
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
	/* class_tid: 4, thor, table: port_table.wr_1 */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drv_func.mac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drv_func.parent.mac",
	.field_bit_size = 48,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC & 0xff}
	},
	{
	.description = "phy_port",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
	1}
	},
	{
	.description = "default_arec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
	(BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1 >> 8) & 0xff,
	BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1 & 0xff}
	},
	/* class_tid: 4, thor, table: parif_def_arec_ptr.ing_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	},
	/* class_tid: 4, thor, table: parif_def_err_arec_ptr.ing_0 */
	{
	.description = "act_rec_ptr",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_SRC1,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
	(BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR >> 8) & 0xff,
	BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR & 0xff}
	}
};

struct bnxt_ulp_mapper_ident_info ulp_thor_class_ident_list[] = {
	/* class_tid: 1, thor, table: port_table.rd */
	{
	.description = "default_arec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR,
	.ident_bit_size = 16,
	.ident_bit_pos = 136
	},
	{
	.description = "drv_func.parent.mac",
	.regfile_idx = BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC,
	.ident_bit_size = 48,
	.ident_bit_pos = 80
	},
	{
	.description = "phy_port",
	.regfile_idx = BNXT_ULP_RF_IDX_PHY_PORT,
	.ident_bit_size = 8,
	.ident_bit_pos = 128
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.gre */
	{
	.description = "l2_cntxt_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 29
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv4 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 23
	},
	/* class_tid: 1, thor, table: profile_tcam.icmpv6 */
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 23
	},
	/* class_tid: 2, thor, table: port_table.rd */
	{
	.description = "default_arec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR,
	.ident_bit_size = 16,
	.ident_bit_pos = 136
	},
	{
	.description = "drv_func.parent.mac",
	.regfile_idx = BNXT_ULP_RF_IDX_DRV_FUNC_PARENT_MAC,
	.ident_bit_size = 48,
	.ident_bit_pos = 80
	},
	{
	.description = "phy_port",
	.regfile_idx = BNXT_ULP_RF_IDX_PHY_PORT,
	.ident_bit_size = 8,
	.ident_bit_pos = 128
	},
	/* class_tid: 2, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 29
	},
	/* class_tid: 4, thor, table: port_table.rd */
	{
	.description = "default_arec_ptr",
	.regfile_idx = BNXT_ULP_RF_IDX_DEFAULT_AREC_PTR,
	.ident_bit_size = 16,
	.ident_bit_pos = 136
	}
};

