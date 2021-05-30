/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Tue Dec  1 17:07:12 2020 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Mapper templates for header act list */
struct bnxt_ulp_mapper_tmpl_info ulp_stingray_act_tmpl_list[] = {
	/* act_tid: 1, stingray, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 4,
	.start_tbl_idx = 0,
	.reject_info = {
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_FALSE,
		.cond_start_idx = 0,
		.cond_nums = 0 }
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_stingray_act_tbl_list[] = {
	{ /* act_tid: 1, stingray, table: int_flow_counter_tbl.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.direction = TF_DIR_RX,
	.execute_info = {
		.cond_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 0,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0
	},
	{ /* act_tid: 1, stingray, table: int_vtag_encap_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.execute_info = {
		.cond_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_OR,
		.cond_start_idx = 1,
		.cond_nums = 1 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_ENCAP_PTR_0,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 1,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12
	},
	{ /* act_tid: 1, stingray, table: int_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.execute_info = {
		.cond_goto = 1,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 13,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	},
	{ /* act_tid: 1, stingray, table: ext_full_act_record.0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_RX,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.execute_info = {
		.cond_goto = 0,
		.cond_list_opcode = BNXT_ULP_COND_LIST_OPC_TRUE,
		.cond_start_idx = 2,
		.cond_nums = 0 },
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_RF_IDX_MAIN_ACTION_PTR,
	.accept_opcode = BNXT_ULP_ACCEPT_OPC_ALWAYS,
	.fdb_opcode = BNXT_ULP_FDB_OPC_PUSH,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.result_start_idx = 39,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0
	}
};

struct bnxt_ulp_mapper_cond_info ulp_stingray_act_cond_list[] = {
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_COUNT
	},
	{
	.cond_opcode = BNXT_ULP_COND_OPC_ACT_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACT_BIT_PUSH_VLAN
	}
};

struct bnxt_ulp_mapper_field_info ulp_stingray_act_result_field_list[] = {
	/* act_tid: 1, stingray, table: int_flow_counter_tbl.0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, stingray, table: int_vtag_encap_record.0 */
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		ULP_SR_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		1}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff}
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff}
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff}
	},
	{
	.description = "spare",
	.field_bit_size = 80,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, stingray, table: int_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_ENCAP_PTR_0 & 0xff}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		ULP_SR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
		ULP_SR_SYM_DECAP_FUNC_NONE}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	/* act_tid: 1, stingray, table: ext_full_act_record.0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_FLOW_CNTR_PTR_0 & 0xff}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "flow_cntr_en",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MODIFY_IPV4_DST_PTR_0 & 0xff}
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_DST & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_DST & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_RF,
	.field_opr1 = {
		(BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_RF_IDX_MODIFY_IPV4_SRC_PTR_0 & 0xff}
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_SET_TP_SRC & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC & 0xff},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_CF,
	.field_opr1 = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_ACT_BIT,
	.field_cond_opr = {
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 56) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 48) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 40) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 32) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 24) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 16) & 0xff,
		((uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP >> 8) & 0xff,
		(uint64_t)BNXT_ULP_ACT_BIT_VXLAN_DECAP & 0xff},
	.field_src1 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr1 = {
		ULP_SR_SYM_DECAP_FUNC_THRU_TUN},
	.field_src2 = BNXT_ULP_FIELD_SRC_CONST,
	.field_opr2 = {
		ULP_SR_SYM_DECAP_FUNC_NONE}
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ACT_PROP,
	.field_opr1 = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
	.field_src1 = BNXT_ULP_FIELD_SRC_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.field_cond_src = BNXT_ULP_FIELD_COND_SRC_TRUE,
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
	}
};
