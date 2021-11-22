/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Fri Nov 12 19:33:52 2021 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Specifies parameters for the cache and shared tables */
struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.result_num_entries      = 2048,
	.result_num_bytes        = 8,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.result_num_entries      = 2048,
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
	.result_num_entries      = 512,
	.result_num_bytes        = 8,
	.key_num_bytes           = 10,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.result_num_entries      = 512,
	.result_num_bytes        = 8,
	.key_num_bytes           = 10,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
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
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.result_num_entries      = 256,
	.result_num_bytes        = 7,
	.key_num_bytes           = 3,
	.num_buckets             = 8,
	.hash_tbl_entries        = 1024,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.result_num_entries      = 256,
	.result_num_bytes        = 7,
	.key_num_bytes           = 3,
	.num_buckets             = 8,
	.hash_tbl_entries        = 1024,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_SOURCE_PROPERTY_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 10,
	.num_buckets             = 4,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
				BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "INGRESS GEN_TABLE_SOURCE_PROPERTY_CACHE",
	.result_num_entries      = 128,
	.result_num_bytes        = 6,
	.key_num_bytes           = 10,
	.num_buckets             = 4,
	.hash_tbl_entries        = 512,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_VXLAN_ENCAP_REC_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 17,
	.num_buckets             = 8,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_VXLAN_ENCAP_REC_CACHE",
	.result_num_entries      = 4096,
	.result_num_bytes        = 6,
	.key_num_bytes           = 17,
	.num_buckets             = 8,
	.hash_tbl_entries        = 16384,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_SOCKET_DIRECT_CACHE",
	.result_num_entries      = 16,
	.result_num_bytes        = 14,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_SOCKET_DIRECT_CACHE",
	.result_num_entries      = 16,
	.result_num_bytes        = 14,
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
	.key_info_list           = ulp_thor_act_key_info_list,
	.key_info_list_size      = ULP_THOR_ACT_KEY_INFO_LIST_SIZE,
	.ident_list              = ulp_thor_act_ident_list,
	.ident_list_size         = ULP_THOR_ACT_IDENT_LIST_SIZE,
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
	.key_byte_order          = BNXT_ULP_BYTE_ORDER_LE,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE,
	.encap_byte_order        = BNXT_ULP_BYTE_ORDER_BE,
	.wc_key_byte_order       = BNXT_ULP_BYTE_ORDER_BE,
	.em_byte_order           = BNXT_ULP_BYTE_ORDER_LE,
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
	.dynamic_sram_en         = 0,
	.dev_tbls                = ulp_template_wh_plus_tbls
	},
	[BNXT_ULP_DEVICE_ID_THOR] = {
	.description             = "Thor",
	.key_byte_order          = BNXT_ULP_BYTE_ORDER_LE,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE,
	.encap_byte_order        = BNXT_ULP_BYTE_ORDER_BE,
	.wc_key_byte_order       = BNXT_ULP_BYTE_ORDER_BE,
	.em_byte_order           = BNXT_ULP_BYTE_ORDER_BE,
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
	.byte_count_mask         = 0x00000007ffffffff,
	.packet_count_mask       = 0xfffffff800000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 35,
	.dynamic_pad_en          = 1,
	.dynamic_sram_en         = 1,
	.dyn_encap_list_size     = 4,
	.dyn_encap_sizes         = {{64, TF_TBL_TYPE_ACT_ENCAP_8B},
					{128, TF_TBL_TYPE_ACT_ENCAP_16B},
					{256, TF_TBL_TYPE_ACT_ENCAP_32B},
					{512, TF_TBL_TYPE_ACT_ENCAP_64B}},
	.dyn_modify_list_size    = 4,
	.dyn_modify_sizes        = {{64, TF_TBL_TYPE_ACT_MODIFY_8B},
					{128, TF_TBL_TYPE_ACT_MODIFY_16B},
					{256, TF_TBL_TYPE_ACT_MODIFY_32B},
					{512, TF_TBL_TYPE_ACT_MODIFY_64B}},
	.em_blk_size_bits        = 100,
	.em_blk_align_bits       = 128,
	.em_key_align_bytes      = 80,
	.wc_slice_width          = 160,
	.wc_max_slices           = 4,
	.wc_mode_list            = {0x0000000c, 0x0000000e,
					0x0000000f, 0x0000000f},
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
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_SOCKET_DIRECT
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_SOCKET_DIRECT
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
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6648
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6648
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
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
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
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
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
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
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
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
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
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
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
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
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
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
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
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_METADATA_PROF_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_PROF_FUNC_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
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
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
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
	.count                   = 15
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
	.count                   = 272
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
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
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
	.count                   = 2048
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
	.count                   = 272
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
	.count                   = 4096
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 272
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
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
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
	.count                   = 2048
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
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 272
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
	.count                   = 4096
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
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
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 26624
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 6144
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 3340
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 3340
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 528
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 512
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
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
	[4116] = 5,
	[4118] = 6,
	[4120] = 7,
	[4122] = 8,
	[4124] = 9,
	[4126] = 10,
	[4128] = 11,
	[4130] = 12,
	[4132] = 13,
	[4134] = 14,
	[4170] = 15,
	[4172] = 16,
	[4174] = 17,
	[4176] = 18,
	[4190] = 19,
	[4191] = 20,
	[4192] = 21,
	[4193] = 22,
	[4224] = 0,
	[4225] = 1,
	[4227] = 20,
	[4229] = 21,
	[4231] = 22,
	[4244] = 2,
	[4246] = 3,
	[4248] = 4,
	[4250] = 5,
	[4252] = 6,
	[4254] = 7,
	[4256] = 8,
	[4258] = 9,
	[4260] = 10,
	[4262] = 11,
	[4265] = 23,
	[4267] = 24,
	[4269] = 25,
	[4271] = 26,
	[4273] = 27,
	[4275] = 28,
	[4277] = 29,
	[4279] = 30,
	[4298] = 12,
	[4300] = 13,
	[4302] = 14,
	[4304] = 15,
	[4318] = 16,
	[4319] = 17,
	[4320] = 18,
	[4321] = 19,
	[4352] = 0,
	[4353] = 1,
	[4355] = 20,
	[4357] = 21,
	[4359] = 22,
	[4372] = 2,
	[4373] = 23,
	[4374] = 3,
	[4375] = 24,
	[4376] = 4,
	[4377] = 25,
	[4378] = 5,
	[4379] = 26,
	[4380] = 6,
	[4381] = 27,
	[4382] = 7,
	[4383] = 28,
	[4384] = 8,
	[4385] = 29,
	[4386] = 9,
	[4387] = 30,
	[4388] = 10,
	[4389] = 31,
	[4390] = 11,
	[4391] = 32,
	[4426] = 12,
	[4428] = 13,
	[4430] = 14,
	[4432] = 15,
	[4446] = 16,
	[4447] = 17,
	[4448] = 18,
	[4449] = 19,
	[4480] = 0,
	[4481] = 1,
	[4483] = 20,
	[4485] = 21,
	[4487] = 22,
	[4500] = 2,
	[4502] = 3,
	[4504] = 4,
	[4506] = 5,
	[4508] = 6,
	[4510] = 7,
	[4512] = 8,
	[4514] = 9,
	[4516] = 10,
	[4518] = 11,
	[4521] = 23,
	[4523] = 24,
	[4525] = 25,
	[4527] = 26,
	[4529] = 27,
	[4531] = 28,
	[4533] = 29,
	[4535] = 30,
	[4537] = 31,
	[4539] = 32,
	[4541] = 33,
	[4543] = 34,
	[4545] = 35,
	[4547] = 36,
	[4549] = 37,
	[4551] = 38,
	[4553] = 39,
	[4554] = 12,
	[4556] = 13,
	[4558] = 14,
	[4560] = 15,
	[4574] = 16,
	[4575] = 17,
	[4576] = 18,
	[4577] = 19,
	[4608] = 0,
	[4609] = 1,
	[4611] = 20,
	[4613] = 21,
	[4615] = 22,
	[4628] = 2,
	[4629] = 23,
	[4630] = 3,
	[4631] = 24,
	[4632] = 4,
	[4633] = 25,
	[4634] = 5,
	[4635] = 26,
	[4636] = 6,
	[4637] = 27,
	[4638] = 7,
	[4639] = 28,
	[4640] = 8,
	[4641] = 29,
	[4642] = 9,
	[4643] = 30,
	[4644] = 10,
	[4645] = 31,
	[4646] = 11,
	[4647] = 32,
	[4665] = 33,
	[4667] = 34,
	[4669] = 35,
	[4671] = 36,
	[4673] = 37,
	[4675] = 38,
	[4677] = 39,
	[4679] = 40,
	[4681] = 41,
	[4682] = 12,
	[4684] = 13,
	[4686] = 14,
	[4688] = 15,
	[4702] = 16,
	[4703] = 17,
	[4704] = 18,
	[4705] = 19,
	[4736] = 0,
	[4737] = 1,
	[4739] = 20,
	[4741] = 21,
	[4743] = 22,
	[4756] = 2,
	[4758] = 3,
	[4760] = 4,
	[4762] = 5,
	[4764] = 6,
	[4766] = 7,
	[4768] = 8,
	[4770] = 9,
	[4772] = 10,
	[4774] = 11,
	[4777] = 23,
	[4779] = 24,
	[4781] = 25,
	[4783] = 26,
	[4785] = 27,
	[4787] = 28,
	[4789] = 29,
	[4791] = 30,
	[4810] = 12,
	[4811] = 31,
	[4812] = 13,
	[4813] = 32,
	[4814] = 14,
	[4815] = 33,
	[4816] = 15,
	[4817] = 34,
	[4830] = 16,
	[4831] = 17,
	[4832] = 18,
	[4833] = 19,
	[4864] = 0,
	[4865] = 1,
	[4867] = 20,
	[4869] = 21,
	[4871] = 22,
	[4884] = 2,
	[4885] = 23,
	[4886] = 3,
	[4887] = 24,
	[4888] = 4,
	[4889] = 25,
	[4890] = 5,
	[4891] = 26,
	[4892] = 6,
	[4893] = 27,
	[4894] = 7,
	[4895] = 28,
	[4896] = 8,
	[4897] = 29,
	[4898] = 9,
	[4899] = 30,
	[4900] = 10,
	[4901] = 31,
	[4902] = 11,
	[4903] = 32,
	[4938] = 12,
	[4939] = 33,
	[4940] = 13,
	[4941] = 34,
	[4942] = 14,
	[4943] = 35,
	[4944] = 15,
	[4945] = 36,
	[4958] = 16,
	[4959] = 17,
	[4960] = 18,
	[4961] = 19,
	[4992] = 0,
	[4993] = 1,
	[4995] = 20,
	[4997] = 21,
	[4999] = 22,
	[5003] = 33,
	[5005] = 34,
	[5007] = 35,
	[5009] = 36,
	[5011] = 37,
	[5012] = 2,
	[5013] = 23,
	[5014] = 3,
	[5015] = 24,
	[5016] = 4,
	[5017] = 25,
	[5018] = 5,
	[5019] = 26,
	[5020] = 6,
	[5021] = 27,
	[5022] = 7,
	[5023] = 28,
	[5024] = 8,
	[5025] = 29,
	[5026] = 9,
	[5027] = 30,
	[5028] = 10,
	[5029] = 31,
	[5030] = 11,
	[5031] = 32,
	[5066] = 12,
	[5068] = 13,
	[5070] = 14,
	[5072] = 15,
	[5086] = 16,
	[5087] = 17,
	[5088] = 18,
	[5089] = 19,
	[6144] = 0,
	[6145] = 1,
	[6146] = 2,
	[6148] = 3,
	[6150] = 4,
	[6184] = 5,
	[6186] = 6,
	[6188] = 7,
	[6190] = 8,
	[6192] = 9,
	[6194] = 10,
	[6196] = 11,
	[6198] = 12,
	[6272] = 0,
	[6273] = 1,
	[6274] = 2,
	[6276] = 3,
	[6278] = 4,
	[6292] = 5,
	[6294] = 6,
	[6296] = 7,
	[6298] = 8,
	[6300] = 9,
	[6302] = 10,
	[6304] = 11,
	[6306] = 12,
	[6308] = 13,
	[6310] = 14,
	[6400] = 0,
	[6401] = 1,
	[6402] = 2,
	[6404] = 3,
	[6406] = 4,
	[6440] = 8,
	[6442] = 9,
	[6444] = 10,
	[6446] = 11,
	[6448] = 12,
	[6450] = 13,
	[6452] = 14,
	[6454] = 15,
	[6482] = 5,
	[6486] = 6,
	[6490] = 7,
	[6528] = 0,
	[6529] = 1,
	[6530] = 2,
	[6532] = 3,
	[6534] = 4,
	[6548] = 8,
	[6550] = 9,
	[6552] = 10,
	[6554] = 11,
	[6556] = 12,
	[6558] = 13,
	[6560] = 14,
	[6562] = 15,
	[6564] = 16,
	[6566] = 17,
	[6610] = 5,
	[6614] = 6,
	[6618] = 7,
	[6656] = 0,
	[6657] = 1,
	[6658] = 2,
	[6660] = 3,
	[6662] = 4,
	[6696] = 5,
	[6698] = 6,
	[6700] = 7,
	[6702] = 8,
	[6704] = 9,
	[6706] = 10,
	[6708] = 11,
	[6710] = 12,
	[6712] = 13,
	[6714] = 14,
	[6716] = 15,
	[6718] = 16,
	[6720] = 17,
	[6722] = 18,
	[6724] = 19,
	[6726] = 20,
	[6728] = 21,
	[6784] = 0,
	[6785] = 1,
	[6786] = 2,
	[6788] = 3,
	[6790] = 4,
	[6804] = 5,
	[6806] = 6,
	[6808] = 7,
	[6810] = 8,
	[6812] = 9,
	[6814] = 10,
	[6816] = 11,
	[6818] = 12,
	[6820] = 13,
	[6822] = 14,
	[6840] = 15,
	[6842] = 16,
	[6844] = 17,
	[6846] = 18,
	[6848] = 19,
	[6850] = 20,
	[6852] = 21,
	[6854] = 22,
	[6856] = 23,
	[6912] = 0,
	[6913] = 1,
	[6914] = 2,
	[6916] = 3,
	[6918] = 4,
	[6952] = 5,
	[6954] = 6,
	[6956] = 7,
	[6958] = 8,
	[6960] = 9,
	[6962] = 10,
	[6964] = 11,
	[6966] = 12,
	[6986] = 13,
	[6988] = 14,
	[6990] = 15,
	[6992] = 16,
	[7040] = 0,
	[7041] = 1,
	[7042] = 2,
	[7044] = 3,
	[7046] = 4,
	[7060] = 5,
	[7062] = 6,
	[7064] = 7,
	[7066] = 8,
	[7068] = 9,
	[7070] = 10,
	[7072] = 11,
	[7074] = 12,
	[7076] = 13,
	[7078] = 14,
	[7114] = 15,
	[7116] = 16,
	[7118] = 17,
	[7120] = 18,
	[7168] = 0,
	[7169] = 1,
	[7170] = 2,
	[7172] = 3,
	[7174] = 4,
	[7208] = 8,
	[7210] = 9,
	[7212] = 10,
	[7214] = 11,
	[7216] = 12,
	[7218] = 13,
	[7220] = 14,
	[7222] = 15,
	[7224] = 16,
	[7226] = 17,
	[7228] = 18,
	[7230] = 19,
	[7232] = 20,
	[7234] = 21,
	[7236] = 22,
	[7238] = 23,
	[7240] = 24,
	[7250] = 5,
	[7254] = 6,
	[7258] = 7,
	[7296] = 0,
	[7297] = 1,
	[7298] = 2,
	[7300] = 3,
	[7302] = 4,
	[7316] = 8,
	[7318] = 9,
	[7320] = 10,
	[7322] = 11,
	[7324] = 12,
	[7326] = 13,
	[7328] = 14,
	[7330] = 15,
	[7332] = 16,
	[7334] = 17,
	[7352] = 18,
	[7354] = 19,
	[7356] = 20,
	[7358] = 21,
	[7360] = 22,
	[7362] = 23,
	[7364] = 24,
	[7366] = 25,
	[7368] = 26,
	[7378] = 5,
	[7382] = 6,
	[7386] = 7,
	[7424] = 0,
	[7425] = 1,
	[7426] = 2,
	[7428] = 3,
	[7430] = 4,
	[7464] = 8,
	[7466] = 9,
	[7468] = 10,
	[7470] = 11,
	[7472] = 12,
	[7474] = 13,
	[7476] = 14,
	[7478] = 15,
	[7498] = 16,
	[7500] = 17,
	[7502] = 18,
	[7504] = 19,
	[7506] = 5,
	[7510] = 6,
	[7514] = 7,
	[7552] = 0,
	[7553] = 1,
	[7554] = 2,
	[7556] = 3,
	[7558] = 4,
	[7572] = 8,
	[7574] = 9,
	[7576] = 10,
	[7578] = 11,
	[7580] = 12,
	[7582] = 13,
	[7584] = 14,
	[7586] = 15,
	[7588] = 16,
	[7590] = 17,
	[7626] = 18,
	[7628] = 19,
	[7630] = 20,
	[7632] = 21,
	[7634] = 5,
	[7638] = 6,
	[7642] = 7
};
