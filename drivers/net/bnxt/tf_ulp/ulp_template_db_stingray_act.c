/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Thu Oct 15 17:28:37 2020 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

/* Mapper templates for header act list */
struct bnxt_ulp_mapper_tmpl_info ulp_stingray_act_tmpl_list[] = {
	/* act-ing-[dec_ttl, count, nat]:1 */
	/* act_tid: 1, stingray, ingress */
	[1] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 0
	},
	/* act-ing-[drop, pop_vlan, push_vlan, dec_ttl, count, vxlan_decap]:2 */
	/* act_tid: 2, stingray, ingress */
	[2] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 3,
	.start_tbl_idx = 6
	},
	/* act-ing-[mark, rss, count, pop_vlan, vxlan_decap]:3 */
	/* act_tid: 3, stingray, ingress */
	[3] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 3,
	.start_tbl_idx = 9
	},
	/* act_egr-[vxlan_encap, count]:4 */
	/* act_tid: 4, stingray, egress */
	[4] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 12
	},
	/* act-egr-[dec_ttl, count, nat]:5 */
	/* act_tid: 5, stingray, egress */
	[5] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 6,
	.start_tbl_idx = 18
	},
	/* act-egr-[drop, push_vlan, dec_ttl, count]:6 */
	/* act_tid: 6, stingray, egress */
	[6] = {
	.device_name = BNXT_ULP_DEVICE_ID_STINGRAY,
	.num_tbls = 5,
	.start_tbl_idx = 24
	}
};

struct bnxt_ulp_mapper_tbl_info ulp_stingray_act_tbl_list[] = {
	{ /* act_tid: 1, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.result_start_idx = 0,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 1, stingray, table: int_act_modify_ipv4_src_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_RX,
	.result_start_idx = 1,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0
	},
	{ /* act_tid: 1, stingray, table: int_act_modify_ipv4_dst_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_RX,
	.result_start_idx = 2,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0
	},
	{ /* act_tid: 1, stingray, table: int_encap_mac_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.result_start_idx = 3,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR
	},
	{ /* act_tid: 1, stingray, table: ext_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.result_start_idx = 15,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 1, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.result_start_idx = 41,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 2, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.result_start_idx = 67,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 2, stingray, table: ext_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.result_start_idx = 68,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 2, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.result_start_idx = 94,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 3, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_RX,
	.result_start_idx = 120,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 3, stingray, table: ext_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.direction = TF_DIR_RX,
	.result_start_idx = 121,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 3, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_RX,
	.result_start_idx = 147,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 4, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.result_start_idx = 173,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 4, stingray, table: int_sp_smac_ipv4_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV4_FLAG,
	.direction = TF_DIR_TX,
	.result_start_idx = 174,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR
	},
	{ /* act_tid: 4, stingray, table: int_sp_smac_ipv6_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_COMP_FIELD_IS_SET,
	.cond_operand = BNXT_ULP_CF_IDX_ACT_ENCAP_IPV6_FLAG,
	.direction = TF_DIR_TX,
	.result_start_idx = 177,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 3,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_SP_PTR
	},
	{ /* act_tid: 4, stingray, table: int_tun_encap_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_64B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.direction = TF_DIR_TX,
	.result_start_idx = 180,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0
	},
	{ /* act_tid: 4, stingray, table: ext_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.direction = TF_DIR_TX,
	.result_start_idx = 192,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 4, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.result_start_idx = 230,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 5, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.result_start_idx = 256,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 5, stingray, table: int_act_modify_ipv4_src_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_SRC,
	.direction = TF_DIR_TX,
	.result_start_idx = 257,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0
	},
	{ /* act_tid: 5, stingray, table: int_act_modify_ipv4_dst_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_SET_IPV4_DST,
	.direction = TF_DIR_TX,
	.result_start_idx = 258,
	.result_bit_size = 32,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0
	},
	{ /* act_tid: 5, stingray, table: int_encap_mac_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.result_start_idx = 259,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_WR_GLB_REGFILE,
	.tbl_operand = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR
	},
	{ /* act_tid: 5, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.result_start_idx = 271,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 5, stingray, table: ext_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.direction = TF_DIR_TX,
	.result_start_idx = 297,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 6, stingray, table: int_flow_counter_tbl_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_STATS_64,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_INT_COUNT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_COUNT,
	.direction = TF_DIR_TX,
	.result_start_idx = 334,
	.result_bit_size = 64,
	.result_num_fields = 1,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0
	},
	{ /* act_tid: 6, stingray, table: int_vtag_encap_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_ACT_ENCAP_16B,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.result_start_idx = 335,
	.result_bit_size = 0,
	.result_num_fields = 0,
	.encap_num_fields = 12,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0
	},
	{ /* act_tid: 6, stingray, table: int_full_act_record_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_FULL_ACT_RECORD,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_INT,
	.direction = TF_DIR_TX,
	.result_start_idx = 347,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 6, stingray, table: ext_full_act_record_no_tag_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_NOT_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.result_start_idx = 373,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 0,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	},
	{ /* act_tid: 6, stingray, table: ext_full_act_record_one_tag_0 */
	.resource_func = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type = TF_TBL_TYPE_EXT,
	.resource_sub_type =
		BNXT_ULP_RESOURCE_SUB_TYPE_INDEX_TABLE_NORMAL,
	.mem_type_opcode = BNXT_ULP_MEM_TYPE_OPC_EXECUTE_IF_EXT,
	.cond_opcode = BNXT_ULP_COND_OPC_ACTION_BIT_IS_SET,
	.cond_operand = BNXT_ULP_ACTION_BIT_PUSH_VLAN,
	.direction = TF_DIR_TX,
	.result_start_idx = 399,
	.result_bit_size = 128,
	.result_num_fields = 26,
	.encap_num_fields = 11,
	.mark_db_opcode = BNXT_ULP_MARK_DB_OPC_NOP,
	.tbl_opcode = BNXT_ULP_INDEX_TBL_OPC_ALLOC_WR_REGFILE,
	.tbl_operand = BNXT_ULP_REGFILE_INDEX_MAIN_ACTION_PTR
	}
};

struct bnxt_ulp_mapper_result_field_info ulp_stingray_act_result_field_list[] = {
	/* act_tid: 1, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 1, stingray, table: int_act_modify_ipv4_src_0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	/* act_tid: 1, stingray, table: int_act_modify_ipv4_dst_0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	/* act_tid: 1, stingray, table: int_encap_mac_record_0 */
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "spare",
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 1, stingray, table: ext_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 1, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 2, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 2, stingray, table: ext_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
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
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
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
	/* act_tid: 2, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
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
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
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
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 3, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 3, stingray, table: ext_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
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
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 3, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VNIC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VNIC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
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
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 4, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 4, stingray, table: int_sp_smac_ipv4_0 */
	{
	.description = "smac",
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ipv4_src_addr",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "reserved",
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 4, stingray, table: int_sp_smac_ipv6_0 */
	{
	.description = "smac",
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ipv6_src_addr",
	.field_bit_size = 128,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "reserved",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 4, stingray, table: int_tun_encap_record_0 */
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_TUN_TYPE_VXLAN,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L4_TYPE_UDP_CSUM,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_l2_dmac",
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_vtag",
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
	.description = "encap_ip",
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
	.description = "encap_udp",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_tun",
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
	/* act_tid: 4, stingray, table: ext_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_TUN_TYPE_VXLAN,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L4_TYPE_UDP_CSUM,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_l2_dmac",
	.field_bit_size = 48,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_vtag",
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
	.description = "encap_ip",
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
	.description = "encap_udp",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "encap_tun",
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
	/* act_tid: 4, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 5, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 5, stingray, table: int_act_modify_ipv4_src_0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	/* act_tid: 5, stingray, table: int_act_modify_ipv4_dst_0 */
	{
	.description = "ipv4_addr",
	.field_bit_size = 32,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	/* act_tid: 5, stingray, table: int_encap_mac_record_0 */
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "spare",
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 5, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_GLB_REGFILE,
	.result_operand = {
		(BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR >> 8) & 0xff,
		BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 5, stingray, table: ext_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_DST_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_dst_port",
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
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_MODIFY_IPV4_SRC_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tcp_src_port",
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
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
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
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_L2_EN_YES,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 6, stingray, table: int_flow_counter_tbl_0 */
	{
	.description = "count",
	.field_bit_size = 64,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 6, stingray, table: int_vtag_encap_record_0 */
	{
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "spare",
	.field_bit_size = 80,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 6, stingray, table: int_full_act_record_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_ENCAP_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
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
	.description = "hit",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "type",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	/* act_tid: 6, stingray, table: ext_full_act_record_no_tag_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
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
	/* act_tid: 6, stingray, table: ext_full_act_record_one_tag_0 */
	{
	.description = "flow_cntr_ptr",
	.field_bit_size = 14,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_REGFILE,
	.result_operand = {
		(BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 >> 8) & 0xff,
		BNXT_ULP_REGFILE_INDEX_FLOW_CNTR_PTR_0 & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "age_enable",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "agg_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "rate_cntr_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "flow_cntr_en",
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
	.description = "flow_cntr_ext",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_key",
	.field_bit_size = 8,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_mir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcpflags_match",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_ptr",
	.field_bit_size = 11,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "encap_rec_int",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "dst_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_dst_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "src_ip_ptr",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tcp_src_port",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "meter_id",
	.field_bit_size = 10,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "tl3_rdir",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "l3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "tl3_ttl_dec",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_COMP_FIELD,
	.result_operand = {
		(BNXT_ULP_CF_IDX_ACT_T_DEC_TTL >> 8) & 0xff,
		BNXT_ULP_CF_IDX_ACT_T_DEC_TTL & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "decap_func",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vnic_or_vport",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_VPORT >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_VPORT & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "pop_vlan",
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
	.description = "meter",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "mirror",
	.field_bit_size = 2,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "drop",
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
	.description = "ecv_tun_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l4_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l3_type",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_l2_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_vtag_type",
	.field_bit_size = 4,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {
		BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "ecv_custom_en",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "ecv_valid",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_CONSTANT,
	.result_operand = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_tpid",
	.field_bit_size = 16,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_vid",
	.field_bit_size = 12,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	},
	{
	.description = "vtag_de",
	.field_bit_size = 1,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ZERO
	},
	{
	.description = "vtag_pcp",
	.field_bit_size = 3,
	.result_opcode = BNXT_ULP_MAPPER_OPC_SET_TO_ACT_PROP,
	.result_operand = {
		(BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP >> 8) & 0xff,
		BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP & 0xff,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
	}
};
