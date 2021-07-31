/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Thu May 13 18:15:56 2021 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Specifies parameters for the cache and shared tables */
struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.result_num_entries      = 256,
	.result_num_bytes        = 8,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.result_num_entries      = 256,
	.result_num_bytes        = 8,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_PROFILE_TCAM",
	.result_num_entries      = 16384,
	.result_num_bytes        = 18,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_PROFILE_TCAM",
	.result_num_entries      = 16384,
	.result_num_bytes        = 18,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_SHARED_MIRROR",
	.result_num_entries      = 16,
	.result_num_bytes        = 8,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_SHARED_MIRROR",
	.result_num_entries      = 16,
	.result_num_bytes        = 8,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.result_num_entries      = 256,
	.result_num_bytes        = 8,
	.key_num_bytes           = 10,
	.num_buckets             = 8,
	.hash_tbl_entries        = 1024,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.result_num_entries      = 256,
	.result_num_bytes        = 8,
	.key_num_bytes           = 10,
	.num_buckets             = 8,
	.hash_tbl_entries        = 1024,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_PORT_TABLE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 19,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_PORT_TABLE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 19,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	}
};

/* device tables */
const struct bnxt_ulp_template_device_tbls ulp_template_wh_plus_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_wh_plus_class_tmpl_list,
	.tmpl_list_size          = ULP_WH_PLUS_CLASS_TMPL_LIST_SIZE,
	.tbl_list                = ulp_wh_plus_class_tbl_list,
	.tbl_list_size           = ULP_WH_PLUS_CLASS_TBL_LIST_SIZE,
	.key_info_list           = ulp_wh_plus_class_key_info_list,
	.key_info_list_size      = ULP_WH_PLUS_CLASS_KEY_INFO_LIST_SIZE,
	.ident_list              = ulp_wh_plus_class_ident_list,
	.ident_list_size         = ULP_WH_PLUS_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_wh_plus_class_cond_list,
	.cond_list_size          = ULP_WH_PLUS_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_wh_plus_class_result_field_list,
	.result_field_list_size  = ULP_WH_PLUS_CLASS_RESULT_FIELD_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_wh_plus_act_tmpl_list,
	.tmpl_list_size          = ULP_WH_PLUS_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_wh_plus_act_tbl_list,
	.tbl_list_size           = ULP_WH_PLUS_ACT_TBL_LIST_SIZE,
	.key_info_list           = ulp_wh_plus_act_key_info_list,
	.key_info_list_size      = ULP_WH_PLUS_ACT_KEY_INFO_LIST_SIZE,
	.ident_list              = ulp_wh_plus_act_ident_list,
	.ident_list_size         = ULP_WH_PLUS_ACT_IDENT_LIST_SIZE,
	.cond_list               = ulp_wh_plus_act_cond_list,
	.cond_list_size          = ULP_WH_PLUS_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_wh_plus_act_result_field_list,
	.result_field_list_size  = ULP_WH_PLUS_ACT_RESULT_FIELD_LIST_SIZE
	}
};

/* device tables */
const struct bnxt_ulp_template_device_tbls ulp_template_thor_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_thor_class_tmpl_list,
	.tmpl_list_size          = ULP_THOR_CLASS_TMPL_LIST_SIZE,
	.tbl_list                = ulp_thor_class_tbl_list,
	.tbl_list_size           = ULP_THOR_CLASS_TBL_LIST_SIZE,
	.key_info_list           = ulp_thor_class_key_info_list,
	.key_info_list_size      = ULP_THOR_CLASS_KEY_INFO_LIST_SIZE,
	.ident_list              = ulp_thor_class_ident_list,
	.ident_list_size         = ULP_THOR_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_thor_class_cond_list,
	.cond_list_size          = ULP_THOR_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_thor_class_result_field_list,
	.result_field_list_size  = ULP_THOR_CLASS_RESULT_FIELD_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_thor_act_tmpl_list,
	.tmpl_list_size          = ULP_THOR_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_thor_act_tbl_list,
	.tbl_list_size           = ULP_THOR_ACT_TBL_LIST_SIZE,
	.cond_list               = ulp_thor_act_cond_list,
	.cond_list_size          = ULP_THOR_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_thor_act_result_field_list,
	.result_field_list_size  = ULP_THOR_ACT_RESULT_FIELD_LIST_SIZE
	}
};

/* List of device specific parameters */
struct bnxt_ulp_device_params ulp_device_params[BNXT_ULP_DEVICE_ID_LAST] = {
	[BNXT_ULP_DEVICE_ID_WH_PLUS] = {
	.description             = "Whitney_Plus",
	.byte_order              = BNXT_ULP_BYTE_ORDER_LE,
	.encap_byte_swap         = 1,
	.int_flow_db_num_entries = 16384,
	.ext_flow_db_num_entries = 32768,
	.mark_db_lfid_entries    = 65536,
	.mark_db_gfid_entries    = 65536,
	.flow_count_db_entries   = 16384,
	.fdb_parent_flow_entries = 2,
	.num_resources_per_flow  = 8,
	.num_phy_ports           = 2,
	.ext_cntr_table_type     = 0,
	.byte_count_mask         = 0x0000000fffffffff,
	.packet_count_mask       = 0xfffffff000000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 36,
	.dynamic_pad_en          = 0,
	.dev_tbls                = ulp_template_wh_plus_tbls
	},
	[BNXT_ULP_DEVICE_ID_THOR] = {
	.description             = "Thor",
	.byte_order              = BNXT_ULP_BYTE_ORDER_LE,
	.encap_byte_swap         = 1,
	.int_flow_db_num_entries = 16384,
	.ext_flow_db_num_entries = 32768,
	.mark_db_lfid_entries    = 0,
	.mark_db_gfid_entries    = 0,
	.flow_count_db_entries   = 16384,
	.fdb_parent_flow_entries = 2,
	.num_resources_per_flow  = 8,
	.num_phy_ports           = 2,
	.ext_cntr_table_type     = 0,
	.byte_count_mask         = 0x00000007ffffffff,
	.packet_count_mask       = 0xfffffff800000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 35,
	.dynamic_pad_en          = 1,
	.em_blk_size_bits        = 100,
	.em_blk_align_bits       = 128,
	.em_key_align_bytes      = 80,
	.wc_slice_width          = 160,
	.wc_max_slices           = 4,
	.wc_mode_list            = {0x0000000c, 0x0000000e, 0x0000000f, 0x0000000f},
	.wc_mod_list_max_size    = 4,
	.wc_ctl_size_bits        = 32,
	.dev_tbls                = ulp_template_thor_tbls
	}
};

/* Provides act_bitmask */
struct bnxt_ulp_shared_act_info ulp_shared_act_info[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.act_bitmask             = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.act_bitmask             = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	}
};

/* List of device specific parameters */
struct bnxt_ulp_app_capabilities_info ulp_app_cap_info_list[] = {
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = 0
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = 0
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	}
};

/* List of unnamed app tf resources required to be reserved per app/device */
struct bnxt_ulp_resource_resv_info ulp_app_resource_resv_list[] = {
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	}
};

/* List of global app tf resources required to be reserved per app/device */
struct bnxt_ulp_glb_resource_info ulp_app_glb_resource_tbl[] = {
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	}
};

/* List of global tf resources required to be reserved per app/device */
struct bnxt_ulp_glb_resource_info ulp_glb_resource_tbl[] = {
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	}
};

/* List of tf resources required to be reserved per app/device */
struct bnxt_ulp_resource_resv_info ulp_resource_resv_list[] = {
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 2048
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 31
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 64
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 64
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 300
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13200
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 26
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 26
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 64
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 100
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 200
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 110
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 528
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 528
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	}
};

uint32_t ulp_act_prop_map_table[] = {
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_SZ] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_SZ,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_TYPE] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_TYPE,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG_NUM] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG_NUM,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L3_TYPE] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L3_TYPE,
	[BNXT_ULP_ACT_PROP_IDX_MPLS_POP_NUM] =
		BNXT_ULP_ACT_PROP_SZ_MPLS_POP_NUM,
	[BNXT_ULP_ACT_PROP_IDX_MPLS_PUSH_NUM] =
		BNXT_ULP_ACT_PROP_SZ_MPLS_PUSH_NUM,
	[BNXT_ULP_ACT_PROP_IDX_PORT_ID] =
		BNXT_ULP_ACT_PROP_SZ_PORT_ID,
	[BNXT_ULP_ACT_PROP_IDX_VNIC] =
		BNXT_ULP_ACT_PROP_SZ_VNIC,
	[BNXT_ULP_ACT_PROP_IDX_VPORT] =
		BNXT_ULP_ACT_PROP_SZ_VPORT,
	[BNXT_ULP_ACT_PROP_IDX_MARK] =
		BNXT_ULP_ACT_PROP_SZ_MARK,
	[BNXT_ULP_ACT_PROP_IDX_COUNT] =
		BNXT_ULP_ACT_PROP_SZ_COUNT,
	[BNXT_ULP_ACT_PROP_IDX_METER] =
		BNXT_ULP_ACT_PROP_SZ_METER,
	[BNXT_ULP_ACT_PROP_IDX_SET_MAC_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_MAC_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_MAC_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_MAC_DST,
	[BNXT_ULP_ACT_PROP_IDX_PUSH_VLAN] =
		BNXT_ULP_ACT_PROP_SZ_PUSH_VLAN,
	[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_PCP] =
		BNXT_ULP_ACT_PROP_SZ_SET_VLAN_PCP,
	[BNXT_ULP_ACT_PROP_IDX_SET_VLAN_VID] =
		BNXT_ULP_ACT_PROP_SZ_SET_VLAN_VID,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV4_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV4_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV4_DST,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV6_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_IPV6_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_IPV6_DST,
	[BNXT_ULP_ACT_PROP_IDX_SET_TP_SRC] =
		BNXT_ULP_ACT_PROP_SZ_SET_TP_SRC,
	[BNXT_ULP_ACT_PROP_IDX_SET_TP_DST] =
		BNXT_ULP_ACT_PROP_SZ_SET_TP_DST,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_0] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_0,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_1] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_1,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_2] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_2,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_3] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_3,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_4] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_4,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_5] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_5,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_6] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_6,
	[BNXT_ULP_ACT_PROP_IDX_OF_PUSH_MPLS_7] =
		BNXT_ULP_ACT_PROP_SZ_OF_PUSH_MPLS_7,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_DMAC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_DMAC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_L2_SMAC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_L2_SMAC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_VTAG] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_VTAG,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_IP_SRC] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_IP_SRC,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_UDP] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_UDP,
	[BNXT_ULP_ACT_PROP_IDX_ENCAP_TUN] =
		BNXT_ULP_ACT_PROP_SZ_ENCAP_TUN,
	[BNXT_ULP_ACT_PROP_IDX_JUMP] =
		BNXT_ULP_ACT_PROP_SZ_JUMP,
	[BNXT_ULP_ACT_PROP_IDX_SHARED_HANDLE] =
		BNXT_ULP_ACT_PROP_SZ_SHARED_HANDLE,
	[BNXT_ULP_ACT_PROP_IDX_RSS_TYPES] =
		BNXT_ULP_ACT_PROP_SZ_RSS_TYPES,
	[BNXT_ULP_ACT_PROP_IDX_RSS_LEVEL] =
		BNXT_ULP_ACT_PROP_SZ_RSS_LEVEL,
	[BNXT_ULP_ACT_PROP_IDX_RSS_KEY_LEN] =
		BNXT_ULP_ACT_PROP_SZ_RSS_KEY_LEN,
	[BNXT_ULP_ACT_PROP_IDX_RSS_KEY] =
		BNXT_ULP_ACT_PROP_SZ_RSS_KEY,
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

uint8_t ulp_glb_field_tbl[] = {
	[2048] = 0,
	[2049] = 1,
	[2050] = 2,
	[2052] = 3,
	[2054] = 4,
	[2088] = 5,
	[2090] = 6,
	[2092] = 7,
	[2094] = 8,
	[2096] = 9,
	[2098] = 10,
	[2100] = 11,
	[2102] = 12,
	[2176] = 0,
	[2177] = 1,
	[2178] = 2,
	[2180] = 3,
	[2182] = 4,
	[2196] = 5,
	[2198] = 6,
	[2200] = 7,
	[2202] = 8,
	[2204] = 9,
	[2206] = 10,
	[2208] = 11,
	[2210] = 12,
	[2212] = 13,
	[2214] = 14,
	[2304] = 0,
	[2305] = 1,
	[2306] = 2,
	[2308] = 3,
	[2310] = 4,
	[2344] = 8,
	[2346] = 9,
	[2348] = 10,
	[2350] = 11,
	[2352] = 12,
	[2354] = 13,
	[2356] = 14,
	[2358] = 15,
	[2386] = 5,
	[2390] = 6,
	[2394] = 7,
	[2432] = 0,
	[2433] = 1,
	[2434] = 2,
	[2436] = 3,
	[2438] = 4,
	[2452] = 8,
	[2454] = 9,
	[2456] = 10,
	[2458] = 11,
	[2460] = 12,
	[2462] = 13,
	[2464] = 14,
	[2466] = 15,
	[2468] = 16,
	[2470] = 17,
	[2514] = 5,
	[2518] = 6,
	[2522] = 7,
	[2560] = 0,
	[2561] = 1,
	[2562] = 2,
	[2564] = 3,
	[2566] = 4,
	[2600] = 5,
	[2602] = 6,
	[2604] = 7,
	[2606] = 8,
	[2608] = 9,
	[2610] = 10,
	[2612] = 11,
	[2614] = 12,
	[2616] = 13,
	[2618] = 14,
	[2620] = 15,
	[2622] = 16,
	[2624] = 17,
	[2626] = 18,
	[2628] = 19,
	[2630] = 20,
	[2632] = 21,
	[2688] = 0,
	[2689] = 1,
	[2690] = 2,
	[2692] = 3,
	[2694] = 4,
	[2708] = 5,
	[2710] = 6,
	[2712] = 7,
	[2714] = 8,
	[2716] = 9,
	[2718] = 10,
	[2720] = 11,
	[2722] = 12,
	[2724] = 13,
	[2726] = 14,
	[2744] = 15,
	[2746] = 16,
	[2748] = 17,
	[2750] = 18,
	[2752] = 19,
	[2754] = 20,
	[2756] = 21,
	[2758] = 22,
	[2760] = 23,
	[2816] = 0,
	[2817] = 1,
	[2818] = 2,
	[2820] = 3,
	[2822] = 4,
	[2856] = 5,
	[2858] = 6,
	[2860] = 7,
	[2862] = 8,
	[2864] = 9,
	[2866] = 10,
	[2868] = 11,
	[2870] = 12,
	[2890] = 13,
	[2892] = 14,
	[2894] = 15,
	[2896] = 16,
	[2944] = 0,
	[2945] = 1,
	[2946] = 2,
	[2948] = 3,
	[2950] = 4,
	[2964] = 5,
	[2966] = 6,
	[2968] = 7,
	[2970] = 8,
	[2972] = 9,
	[2974] = 10,
	[2976] = 11,
	[2978] = 12,
	[2980] = 13,
	[2982] = 14,
	[3018] = 15,
	[3020] = 16,
	[3022] = 17,
	[3024] = 18,
	[3072] = 0,
	[3073] = 1,
	[3074] = 2,
	[3076] = 3,
	[3078] = 4,
	[3112] = 8,
	[3114] = 9,
	[3116] = 10,
	[3118] = 11,
	[3120] = 12,
	[3122] = 13,
	[3124] = 14,
	[3126] = 15,
	[3128] = 16,
	[3130] = 17,
	[3132] = 18,
	[3134] = 19,
	[3136] = 20,
	[3138] = 21,
	[3140] = 22,
	[3142] = 23,
	[3144] = 24,
	[3154] = 5,
	[3158] = 6,
	[3162] = 7,
	[3200] = 0,
	[3201] = 1,
	[3202] = 2,
	[3204] = 3,
	[3206] = 4,
	[3220] = 8,
	[3222] = 9,
	[3224] = 10,
	[3226] = 11,
	[3228] = 12,
	[3230] = 13,
	[3232] = 14,
	[3234] = 15,
	[3236] = 16,
	[3238] = 17,
	[3256] = 18,
	[3258] = 19,
	[3260] = 20,
	[3262] = 21,
	[3264] = 22,
	[3266] = 23,
	[3268] = 24,
	[3270] = 25,
	[3272] = 26,
	[3282] = 5,
	[3286] = 6,
	[3290] = 7,
	[3328] = 0,
	[3329] = 1,
	[3330] = 2,
	[3332] = 3,
	[3334] = 4,
	[3368] = 8,
	[3370] = 9,
	[3372] = 10,
	[3374] = 11,
	[3376] = 12,
	[3378] = 13,
	[3380] = 14,
	[3382] = 15,
	[3402] = 16,
	[3404] = 17,
	[3406] = 18,
	[3408] = 19,
	[3410] = 5,
	[3414] = 6,
	[3418] = 7,
	[3456] = 0,
	[3457] = 1,
	[3458] = 2,
	[3460] = 3,
	[3462] = 4,
	[3476] = 8,
	[3478] = 9,
	[3480] = 10,
	[3482] = 11,
	[3484] = 12,
	[3486] = 13,
	[3488] = 14,
	[3490] = 15,
	[3492] = 16,
	[3494] = 17,
	[3530] = 18,
	[3532] = 19,
	[3534] = 20,
	[3536] = 21,
	[3538] = 5,
	[3542] = 6,
	[3546] = 7,
	[3584] = 0,
	[3585] = 1,
	[3586] = 2,
	[3588] = 3,
	[3590] = 4,
	[3604] = 5,
	[3606] = 6,
	[3608] = 7,
	[3610] = 8,
	[3612] = 9,
	[3614] = 10,
	[3616] = 11,
	[3618] = 12,
	[3620] = 13,
	[3622] = 14,
	[3658] = 15,
	[3660] = 16,
	[3662] = 17,
	[3664] = 18,
	[3678] = 19,
	[3679] = 20,
	[3680] = 21,
	[3681] = 22,
	[4096] = 0,
	[4097] = 1,
	[4098] = 2,
	[4100] = 3,
	[4102] = 4,
	[4136] = 5,
	[4138] = 6,
	[4140] = 7,
	[4142] = 8,
	[4144] = 9,
	[4146] = 10,
	[4148] = 11,
	[4150] = 12,
	[4224] = 0,
	[4225] = 1,
	[4226] = 2,
	[4228] = 3,
	[4230] = 4,
	[4244] = 5,
	[4246] = 6,
	[4248] = 7,
	[4250] = 8,
	[4252] = 9,
	[4254] = 10,
	[4256] = 11,
	[4258] = 12,
	[4260] = 13,
	[4262] = 14,
	[4352] = 0,
	[4353] = 1,
	[4354] = 2,
	[4356] = 3,
	[4358] = 4,
	[4392] = 8,
	[4394] = 9,
	[4396] = 10,
	[4398] = 11,
	[4400] = 12,
	[4402] = 13,
	[4404] = 14,
	[4406] = 15,
	[4434] = 5,
	[4438] = 6,
	[4442] = 7,
	[4480] = 0,
	[4481] = 1,
	[4482] = 2,
	[4484] = 3,
	[4486] = 4,
	[4500] = 8,
	[4502] = 9,
	[4504] = 10,
	[4506] = 11,
	[4508] = 12,
	[4510] = 13,
	[4512] = 14,
	[4514] = 15,
	[4516] = 16,
	[4518] = 17,
	[4562] = 5,
	[4566] = 6,
	[4570] = 7,
	[4608] = 0,
	[4609] = 1,
	[4610] = 2,
	[4612] = 3,
	[4614] = 4,
	[4648] = 5,
	[4650] = 6,
	[4652] = 7,
	[4654] = 8,
	[4656] = 9,
	[4658] = 10,
	[4660] = 11,
	[4662] = 12,
	[4664] = 13,
	[4666] = 14,
	[4668] = 15,
	[4670] = 16,
	[4672] = 17,
	[4674] = 18,
	[4676] = 19,
	[4678] = 20,
	[4680] = 21,
	[4736] = 0,
	[4737] = 1,
	[4738] = 2,
	[4740] = 3,
	[4742] = 4,
	[4756] = 5,
	[4758] = 6,
	[4760] = 7,
	[4762] = 8,
	[4764] = 9,
	[4766] = 10,
	[4768] = 11,
	[4770] = 12,
	[4772] = 13,
	[4774] = 14,
	[4792] = 15,
	[4794] = 16,
	[4796] = 17,
	[4798] = 18,
	[4800] = 19,
	[4802] = 20,
	[4804] = 21,
	[4806] = 22,
	[4808] = 23,
	[4864] = 0,
	[4865] = 1,
	[4866] = 2,
	[4868] = 3,
	[4870] = 4,
	[4904] = 5,
	[4906] = 6,
	[4908] = 7,
	[4910] = 8,
	[4912] = 9,
	[4914] = 10,
	[4916] = 11,
	[4918] = 12,
	[4938] = 13,
	[4940] = 14,
	[4942] = 15,
	[4944] = 16,
	[4992] = 0,
	[4993] = 1,
	[4994] = 2,
	[4996] = 3,
	[4998] = 4,
	[5012] = 5,
	[5014] = 6,
	[5016] = 7,
	[5018] = 8,
	[5020] = 9,
	[5022] = 10,
	[5024] = 11,
	[5026] = 12,
	[5028] = 13,
	[5030] = 14,
	[5066] = 15,
	[5068] = 16,
	[5070] = 17,
	[5072] = 18,
	[5120] = 0,
	[5121] = 1,
	[5122] = 2,
	[5124] = 3,
	[5126] = 4,
	[5160] = 8,
	[5162] = 9,
	[5164] = 10,
	[5166] = 11,
	[5168] = 12,
	[5170] = 13,
	[5172] = 14,
	[5174] = 15,
	[5176] = 16,
	[5178] = 17,
	[5180] = 18,
	[5182] = 19,
	[5184] = 20,
	[5186] = 21,
	[5188] = 22,
	[5190] = 23,
	[5192] = 24,
	[5202] = 5,
	[5206] = 6,
	[5210] = 7,
	[5248] = 0,
	[5249] = 1,
	[5250] = 2,
	[5252] = 3,
	[5254] = 4,
	[5268] = 8,
	[5270] = 9,
	[5272] = 10,
	[5274] = 11,
	[5276] = 12,
	[5278] = 13,
	[5280] = 14,
	[5282] = 15,
	[5284] = 16,
	[5286] = 17,
	[5304] = 18,
	[5306] = 19,
	[5308] = 20,
	[5310] = 21,
	[5312] = 22,
	[5314] = 23,
	[5316] = 24,
	[5318] = 25,
	[5320] = 26,
	[5330] = 5,
	[5334] = 6,
	[5338] = 7,
	[5376] = 0,
	[5377] = 1,
	[5378] = 2,
	[5380] = 3,
	[5382] = 4,
	[5416] = 8,
	[5418] = 9,
	[5420] = 10,
	[5422] = 11,
	[5424] = 12,
	[5426] = 13,
	[5428] = 14,
	[5430] = 15,
	[5450] = 16,
	[5452] = 17,
	[5454] = 18,
	[5456] = 19,
	[5458] = 5,
	[5462] = 6,
	[5466] = 7,
	[5504] = 0,
	[5505] = 1,
	[5506] = 2,
	[5508] = 3,
	[5510] = 4,
	[5524] = 8,
	[5526] = 9,
	[5528] = 10,
	[5530] = 11,
	[5532] = 12,
	[5534] = 13,
	[5536] = 14,
	[5538] = 15,
	[5540] = 16,
	[5542] = 17,
	[5578] = 18,
	[5580] = 19,
	[5582] = 20,
	[5584] = 21,
	[5586] = 5,
	[5590] = 6,
	[5594] = 7
};

