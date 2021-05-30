/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Fri Feb 12 13:05:14 2021 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header class list */
struct bnxt_ulp_mapper_tmpl_info ulp_thor_class_tmpl_list[] = {
	/* class_tid: 1, thor, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 11,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 0,
		.cond_nums = 0 }
	},
	/* class_tid: 2, thor, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_THOR,
	.num_tbls = 1,
	.start_tbl_idx = 11,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 2,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_thor_class_tbl_list[] = {
	{ /* class_tid: 1, thor, table: mac_addr_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 0,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 0,
	.blob_key_bit_size = 56,
	.key_bit_size = 56,
	.key_num_fields = 2,
	.result_start_idx = 0,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 1, thor, table: control.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 3,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 0,
		.cond_nums = 1 },
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
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
		.cond_start_idx = 1,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.pri_opcode  = BNXT_ULP_PRI_OPC_CONST,
	.pri_operand = 0,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 2,
	.blob_key_bit_size = 213,
	.key_bit_size = 213,
	.key_num_fields = 21,
	.result_start_idx = 4,
	.result_bit_size = 43,
	.result_num_fields = 6,
	.ident_start_idx = 0,
	.ident_nums = 1
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
		.cond_start_idx = 1,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_HASH,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 23,
	.blob_key_bit_size = 56,
	.key_bit_size = 56,
	.key_num_fields = 2,
	.result_start_idx = 10,
	.result_bit_size = 62,
	.result_num_fields = 4
	},
	{ /* class_tid: 1, thor, table: profile_tcam_cache.rd */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_GENERIC_TABLE,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 1,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_READ,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 25,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.ident_start_idx = 1,
	.ident_nums = 5
	},
	{ /* class_tid: 1, thor, table: control.1 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_CTRL_TABLE,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 5,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 1,
		.cond_nums = 1 },
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_ALLOC_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	{ /* class_tid: 1, thor, table: fkb_select.wm */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_WC_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_WC_KEY_ID_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 14,
	.result_bit_size = 106,
	.result_num_fields = 106
	},
	{ /* class_tid: 1, thor, table: fkb_select.em */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EM_FKB,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 1,
		.cond_false_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_EM_KEY_ID_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 120,
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
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_TCAM_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_RID_REGFILE,
	.fdb_operand = BNXT_ULP_RF_IDX_RID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_NO,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 28,
	.blob_key_bit_size = 94,
	.key_bit_size = 94,
	.key_num_fields = 43,
	.result_start_idx = 226,
	.result_bit_size = 33,
	.result_num_fields = 8,
	.ident_start_idx = 6,
	.ident_nums = 2
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
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_GENERIC_TBL_OPC_WRITE,
	.gen_tbl_lkup_type = BNXT_ULP_GENERIC_TBL_LKUP_TYPE_INDEX,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.key_start_idx = 71,
	.blob_key_bit_size = 14,
	.key_bit_size = 14,
	.key_num_fields = 3,
	.result_start_idx = 234,
	.result_bit_size = 82,
	.result_num_fields = 7
	},
	{ /* class_tid: 1, thor, table: em.ipv4 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type = TF_MEM_INTERNAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.critical_resource = BNXT_ULP_CRITICAL_RESOURCE_YES,
	.byte_order = BNXT_ULP_BYTE_ORDER_BE,
	.key_start_idx = 74,
	.blob_key_bit_size = 0,
	.key_bit_size = 0,
	.key_num_fields = 114,
	.result_start_idx = 241,
	.result_bit_size = 0,
	.result_num_fields = 6
	},
	{ /* class_tid: 2, thor, table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_true_goto  = 0,
		.cond_false_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH_FID,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.byte_order = BNXT_ULP_BYTE_ORDER_LE,
	.result_start_idx = 247,
	.result_bit_size = 128,
	.result_num_fields = 17
	}
};

struct bnxt_ulp_mapper_cond_info ulp_thor_class_cond_list[] = {
	/* cond_execute: class_tid: 1, control.0 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	},
	/* cond_execute: class_tid: 1, control.1 */
	{
	.cond_opcode = BNXT_ULP_COND_OPC_RF_IS_SET,
	.cond_operand = BNXT_ULP_RF_IDX_GENERIC_TBL_MISS
	}
};

struct bnxt_ulp_mapper_key_info ulp_thor_class_key_info_list[] = {
	/* class_tid: 1, thor, table: mac_addr_cache.rd */
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.field_info_mask = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ivlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_tpid_sel",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_ovlan_vid",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mac1_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac0_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tunnel_id",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "llc",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "roce",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "mpass_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_SVIF_INDEX >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_SVIF_INDEX & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		},
	.field_info_spec = {
		.description = "mac_addr",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_ETH_DMAC >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_ETH_DMAC & 0xff}
		}
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.rd */
	{
	.field_info_mask = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
			(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_L4_HDR_TYPE_TCP},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L4_HDR_TYPE_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_L4_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "ieh",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_IPV4 & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_L3_HDR_TYPE_IPV4},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_L3_HDR_TYPE_IPV6}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_L3_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "l2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_L2_HDR_VALID_YES}
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_flags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tun_hdr_err",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tun_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_is_udp_tcp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl4_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_dst",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_ipv6_cmp_src",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_isIP",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl3_hdr_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_two_vtags",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_vtag_present",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_uc_mc_bc",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "tl2_hdr_type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_hdr_valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "hrec_next",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
			(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "agg_error",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "metadata",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		},
	.field_info_spec = {
		.description = "pkt_type_0",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "pkt_type_1",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "valid",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "recycle_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
		}
	},
	{
	.field_info_mask = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "prof_func_id",
		.field_bit_size = 7,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
		.field_opr1 = {
			(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
			BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "hdr_sig_id",
		.field_bit_size = 5,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CF,
		.field_opr1 = {
			(BNXT_ULP_CF_IDX_HDR_SIG_ID >> 8) & 0xff,
			BNXT_ULP_CF_IDX_HDR_SIG_ID & 0xff}
		}
	},
	/* class_tid: 1, thor, table: em.ipv4 */
	{
	.field_info_mask = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "em_profile_id",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
			(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
			BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_cntxt_id",
		.field_bit_size = 10,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_RF,
		.field_opr1 = {
			(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
			BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "parif",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "spif",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "svif",
		.field_bit_size = 11,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "lcos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "meta",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "rcyc_cnt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "loopback",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
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
		.description = "tl3_sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
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
		.description = "tl3_sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
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
		.description = "tl3_dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
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
		.description = "tl3_dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl3_l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl3_l3err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tl4_err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tl4_err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tuntype",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tflags",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tids",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tid",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tctxts",
		.field_bit_size = 24,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "tctxt",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "tqos",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "terr",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_l2type",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_dmac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_smac",
		.field_bit_size = 48,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_dt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_sa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_nvt",
		.field_bit_size = 2,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ovp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ovd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ovv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ovt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ivp",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ivd",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ivv",
		.field_bit_size = 12,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l2_ivt",
		.field_bit_size = 3,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l2_etype",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_l3type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3_sip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_IPV4_SRC_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
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
		.description = "l3_sip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
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
		.description = "l3_sip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		},
	.field_info_spec = {
		.description = "l3_dip.ipv4",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_IPV4_DST_ADDR & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
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
		.description = "l3_dip.ipv6",
		.field_bit_size = 128,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
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
		.description = "l3_dip_selcmp.ipv6",
		.field_bit_size = 72,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ttl",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_prot",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			ULP_THOR_SYM_IP_PROTO_TCP},
		.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr2 = {
			ULP_THOR_SYM_IP_PROTO_UDP}
		}
	},
	{
	.field_info_mask = {
		.description = "l3_fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l3_fid.ipv4",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l3_fid.ipv6",
		.field_bit_size = 20,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_qos",
		.field_bit_size = 8,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_nonext",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_esp",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_auth",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_dest",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_rthdr",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_hop",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_ieh_1frag",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_df",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_l3err.ipv4",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l3_l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l3_l3err.ipv6",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_l4type",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff}
		},
	.field_info_spec = {
		.description = "l4_src",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_SRC_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_UDP_SRC_PORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		},
	.field_info_spec = {
		.description = "l4_dst",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_HDR_BIT,
		.field_cond_opr = {
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 56) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 48) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 40) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 32) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 24) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 16) & 0xff,
			((uint64_t)BNXT_ULP_HDR_BIT_O_TCP >> 8) & 0xff,
			(uint64_t)BNXT_ULP_HDR_BIT_O_TCP & 0xff},
		.field_src1 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr1 = {
			(BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_TCP_DST_PORT & 0xff},
		.field_src2 = BNXT_ULP_FIELD_SRC_HF,
		.field_opr2 = {
			(BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT >> 8) & 0xff,
			BNXT_ULP_GLB_HF_ID_O_UDP_DST_PORT & 0xff}
		}
	},
	{
	.field_info_mask = {
		.description = "l4_flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_flags",
		.field_bit_size = 9,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_seq",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_ack",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_win",
		.field_bit_size = 16,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_pa",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_opt",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_tcpts",
		.field_bit_size = 1,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_tsval",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff,
			0xff,
			0xff,
			0xff}
		},
	.field_info_spec = {
		.description = "l4_txecr",
		.field_bit_size = 32,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	},
	{
	.field_info_mask = {
		.description = "l4_err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
		.field_opr1 = {
			0xff}
		},
	.field_info_spec = {
		.description = "l4_err",
		.field_bit_size = 4,
		.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
		.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
		.field_src1 = BNXT_ULP_FIELD_SRC_SKIP
		}
	}
};

struct bnxt_ulp_mapper_field_info ulp_thor_class_result_field_list[] = {
	/* class_tid: 1, thor, table: mac_addr_cache.rd */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "prof_func_id",
	.field_bit_size = 7,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
		(BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID >> 8) & 0xff,
		BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID & 0xff}
	},
	{
	.description = "ctxt_meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "def_ctxt_data",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_GLB_RF,
	.field_opr1 = {
		(4 >> 8) & 0xff,
		4 & 0xff}
	},
	{
	.description = "ctxt_opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		ULP_THOR_SYM_CTXT_OPCODE_NORMAL_FLOW}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "parif",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: mac_addr_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
		BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "l2_cntxt_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "l2_cntxt_id",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_L2_CNTXT_ID_0 & 0xff}
	},
	{
	.description = "src_property_ptr",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: fkb_select.wm */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: fkb_select.em */
	{
	.description = "l2_cntxt_id.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "parif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "spif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "svif.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "lcos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rcyc_cnt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "loopback.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tuntype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tflags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tids.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tctxt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tqos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "terr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_l2type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dmac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_smac.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_dt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_sa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_nvt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ovt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivd.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivv.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_ivt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l2_etype.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_sip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_sip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_dip.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_dip_selcmp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_prot.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l3_fid.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_qos.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_nonext.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_esp.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_auth.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_dest.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_rthdr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_hop.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ieh_1frag.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_df.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_l3err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_l4type.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_src.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l4_dst.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "l4_flags.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_seq.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_ack.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_win.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_pa.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_opt.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tcpts.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_tsval.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_txecr.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l4_err.en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam.l3_l4 */
	{
	.description = "wc_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "wc_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_type",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "em_key_id",
	.field_bit_size = 6,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_KEY_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_KEY_ID_0 & 0xff}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_search_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "pl_byp_lkup_en",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.wr */
	{
	.description = "rid",
	.field_bit_size = 32,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_RID >> 8) & 0xff,
		BNXT_ULP_RF_IDX_RID & 0xff}
	},
	{
	.description = "profile_tcam_index",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_PROFILE_TCAM_INDEX_0 & 0xff}
	},
	{
	.description = "em_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "em_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_EM_KEY_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_EM_KEY_ID_0 & 0xff}
	},
	{
	.description = "wc_profile_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_WC_PROFILE_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_WC_PROFILE_ID_0 & 0xff}
	},
	{
	.description = "wc_key_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_WC_KEY_ID_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_WC_KEY_ID_0 & 0xff}
	},
	{
	.description = "flow_sig_id",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_FLOW_SIG_ID >> 8) & 0xff,
		BNXT_ULP_CF_IDX_FLOW_SIG_ID & 0xff}
	},
	/* class_tid: 1, thor, table: em.ipv4 */
	{
	.description = "valid",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "strength",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		3}
	},
	{
	.description = "arec_ptr_or_md",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_MAIN_ACTION_PTR >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MAIN_ACTION_PTR & 0xff}
	},
	{
	.description = "opcode",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meta_prof",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ctxt_data",
	.field_bit_size = 14,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* class_tid: 2, thor, table: int_full_act_record.0 */
	{
	.description = "sp_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mod_rec_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd1",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rsvd0",
	.field_bit_size = 8,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 5,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 10,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_op",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "stats_ptr",
	.field_bit_size = 16,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 11,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_DRV_FUNC_VNIC >> 8) & 0xff,
		BNXT_ULP_CF_IDX_DRV_FUNC_VNIC & 0xff}
	},
	{
	.description = "use_default",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 4,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "cnd_copy",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vlan_dlt_rpt",
	.field_bit_size = 2,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 3,
	.field_opc = BNXT_ULP_FIELD_OPC_COND_OP,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	}
};

struct bnxt_ulp_mapper_ident_info ulp_thor_class_ident_list[] = {
	/* class_tid: 1, thor, table: l2_cntxt_tcam.0 */
	{
	.description = "l2_cntxt_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.regfile_idx = BNXT_ULP_RF_IDX_L2_CNTXT_ID_0,
	.ident_bit_size = 10,
	.ident_bit_pos = 29
	},
	/* class_tid: 1, thor, table: profile_tcam_cache.rd */
	{
	.description = "em_key_id",
	.regfile_idx = BNXT_ULP_RF_IDX_EM_KEY_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 50
	},
	{
	.description = "em_profile_id",
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 42
	},
	{
	.description = "flow_sig_id",
	.regfile_idx = BNXT_ULP_RF_IDX_FLOW_SIG_ID,
	.ident_bit_size = 8,
	.ident_bit_pos = 74
	},
	{
	.description = "wc_key_id",
	.regfile_idx = BNXT_ULP_RF_IDX_WC_KEY_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 66
	},
	{
	.description = "wc_profile_id",
	.regfile_idx = BNXT_ULP_RF_IDX_WC_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 58
	},
	/* class_tid: 1, thor, table: profile_tcam.l3_l4 */
	{
	.description = "wc_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_WC_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_WC_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 6
	},
	{
	.description = "em_profile_id",
	.resource_func = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.ident_type = TF_IDENT_TYPE_EM_PROF,
	.regfile_idx = BNXT_ULP_RF_IDX_EM_PROFILE_ID_0,
	.ident_bit_size = 8,
	.ident_bit_pos = 23
	}
};
