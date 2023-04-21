/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2023 Broadcom
 * All rights reserved.
 */

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
	.key_num_bytes           = 12,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.result_num_entries      = 512,
	.result_num_bytes        = 8,
	.key_num_bytes           = 12,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_PORT_TABLE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 21,
	.key_num_bytes           = 0,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_PORT_TABLE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 21,
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
	.result_num_entries      = 4096,
	.result_num_bytes        = 6,
	.key_num_bytes           = 10,
	.num_buckets             = 4,
	.hash_tbl_entries        = 8192,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
				BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_SOURCE_PROPERTY_CACHE",
	.result_num_entries      = 128,
	.result_num_bytes        = 6,
	.key_num_bytes           = 10,
	.num_buckets             = 4,
	.hash_tbl_entries        = 512,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_L2_ENCAP_REC_CACHE",
	.result_num_entries      = 4096,
	.result_num_bytes        = 6,
	.key_num_bytes           = 14,
	.num_buckets             = 4,
	.hash_tbl_entries        = 8192,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_L2_ENCAP_REC_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 14,
	.num_buckets             = 4,
	.hash_tbl_entries        = 0,
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
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 22,
	.num_buckets             = 4,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
				BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.result_num_entries      = 2048,
	.result_num_bytes        = 6,
	.key_num_bytes           = 22,
	.num_buckets             = 4,
	.hash_tbl_entries        = 8192,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 29,
	.num_buckets             = 8,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.result_num_entries      = 4096,
	.result_num_bytes        = 6,
	.key_num_bytes           = 29,
	.num_buckets             = 8,
	.hash_tbl_entries        = 16384,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_SRV6_ENCAP_REC_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 6,
	.key_num_bytes           = 29,
	.num_buckets             = 8,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_SRV6_ENCAP_REC_CACHE",
	.result_num_entries      = 2048,
	.result_num_bytes        = 6,
	.key_num_bytes           = 86,
	.num_buckets             = 4,
	.hash_tbl_entries        = 8192,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_OUTER_TUNNEL_CACHE",
	.result_num_entries      = 4096,
	.result_num_bytes        = 4,
	.key_num_bytes           = 32,
	.num_buckets             = 4,
	.hash_tbl_entries        = 16384,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_OUTER_TUNNEL_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 4,
	.key_num_bytes           = 32,
	.num_buckets             = 8,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.result_num_entries      = 512,
	.result_num_bytes        = 8,
	.key_num_bytes           = 4,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.result_num_entries      = 512,
	.result_num_bytes        = 8,
	.key_num_bytes           = 4,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 10,
	.key_num_bytes           = 4,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.result_num_entries      = 1024,
	.result_num_bytes        = 10,
	.key_num_bytes           = 4,
	.num_buckets             = 8,
	.hash_tbl_entries        = 2048,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGISTER_TBL << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GENERIC_TABLE_GLOBAL_REGISTER_TBL",
	.result_num_entries      = 256,
	.result_num_bytes        = 8,
	.key_num_bytes           = 3,
	.num_buckets             = 4,
	.hash_tbl_entries        = 1024,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGISTER_TBL << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GENERIC_TABLE_GLOBAL_REGISTER_TBL",
	.result_num_entries      = 0,
	.result_num_bytes        = 8,
	.key_num_bytes           = 3,
	.num_buckets             = 0,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name                    = "INGRESS GEN_TABLE_CHAIN_ID_CACHE",
	.result_num_entries      = 0,
	.result_num_bytes        = 4,
	.key_num_bytes           = 4,
	.num_buckets             = 4,
	.hash_tbl_entries        = 0,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name                    = "EGRESS GEN_TABLE_CHAIN_ID_CACHE",
	.result_num_entries      = 64,
	.result_num_bytes        = 4,
	.key_num_bytes           = 4,
	.num_buckets             = 4,
	.hash_tbl_entries        = 256,
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
	.wc_dynamic_pad_en       = 1,
	.em_dynamic_pad_en       = 0,
	.dynamic_sram_en         = 0,
	.wc_slice_width          = 80,
	.wc_max_slices           = 4,
	.wc_mode_list            = {0x00000000, 0x00000002,
					0x00000003, 0x00000003},
	.wc_mod_list_max_size    = 4,
	.wc_ctl_size_bits        = 16,
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
	.wc_dynamic_pad_en       = 1,
	.em_dynamic_pad_en       = 1,
	.dynamic_sram_en         = 1,
	.dyn_encap_list_size     = 5,
	.dyn_encap_sizes         = {{64, TF_TBL_TYPE_ACT_ENCAP_8B},
					{128, TF_TBL_TYPE_ACT_ENCAP_16B},
					{256, TF_TBL_TYPE_ACT_ENCAP_32B},
					{512, TF_TBL_TYPE_ACT_ENCAP_64B},
					{1024, TF_TBL_TYPE_ACT_ENCAP_128B}},
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
	.flags                   = 0,
	.vxlan_port              = 4789,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = 0,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = 0,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
					BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT |
					BNXT_ULP_APP_CAP_SRV6,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = 0,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
					BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT |
					BNXT_ULP_APP_CAP_SRV6,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 0,
	.ha_pool_id              = 3,
	.ha_reg_cnt              = 7,
	.ha_reg_state            = 8
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_SOCKET_DIRECT,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 0,
	.ha_pool_id              = 3,
	.ha_reg_cnt              = 7,
	.ha_reg_state            = 8
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_HA_DYNAMIC,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 1,
	.ha_pool_id              = 4,
	.ha_reg_cnt              = 9,
	.ha_reg_state            = 10
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_SHARED_EN |
				   BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_HOT_UPGRADE_EN |
				   BNXT_ULP_APP_CAP_SOCKET_DIRECT |
				   BNXT_ULP_APP_CAP_HA_DYNAMIC,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 1,
	.ha_pool_id              = 4,
	.ha_reg_cnt              = 9,
	.ha_reg_state            = 10
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
				   BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT |
					BNXT_ULP_APP_CAP_BC_MC_SUPPORT,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT |
					BNXT_ULP_APP_CAP_BC_MC_SUPPORT,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_SHARED_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_SHARED_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_SHARED_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
					BNXT_ULP_APP_CAP_SHARED_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 250
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_BC_MC_SUPPORT |
							BNXT_ULP_APP_CAP_IP_TOS_PROTO_SUPPORT
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
							BNXT_ULP_APP_CAP_SHARED_EN |
							BNXT_ULP_APP_CAP_HOT_UPGRADE_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 0,
	.ha_pool_id              = 5,
	.ha_reg_cnt              = 7,
	.ha_reg_state            = 8
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY |
							BNXT_ULP_APP_CAP_SHARED_EN |
							BNXT_ULP_APP_CAP_HOT_UPGRADE_EN,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.upgrade_fw_update       = 0,
	.ha_pool_id              = 5,
	.ha_reg_cnt              = 7,
	.ha_reg_state            = 8
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = BNXT_ULP_APP_CAP_UNICAST_ONLY,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0
	}
};

/* List of unnamed app tf resources required to be reserved per app/device */
struct bnxt_ulp_resource_resv_info ulp_app_resource_resv_list[] = {
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1792
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 896
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1792
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6860
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1792
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 896
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1792
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 7168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1792
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4096
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6860
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 7168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1792
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4096
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6520
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6520
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6520
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 392
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6860
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 6860
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_WC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 704
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED_OWC,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 704
	}
};

/* List of global app tf resources required to be reserved per app/device */
struct bnxt_ulp_glb_resource_info ulp_app_glb_resource_tbl[] = {
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_10,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_PROF_FUNC_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_L2_CNTXT_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_APP_GLB_AREC_PTR_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_SHARED,
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
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VF_2_VFR_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_8B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_MODIFY_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_METADATA_PROF_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VF_2_VFR_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_8B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_MODIFY_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_DROP_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VF_2_VFR_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_8B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_MODIFY_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_8,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_9,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_METADATA_PROF_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_METADATA_PROF_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_6,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_7,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GRE_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_EM_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_5,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_2,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_3,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_4,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_KEY_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_WC_PROFILE_ID_1,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_METADATA_PROF_0,
	.direction               = TF_DIR_TX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_KEY_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	}
};

/* List of tf resources required to be reserved per app/device */
struct bnxt_ulp_resource_resv_info ulp_resource_resv_list[] = {
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METER_PROF,
	.count                   = 256
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METER_INST,
	.count                   = 1023
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 31
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 64
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 100
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 11264
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 24
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 128
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 256
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 1,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 11264
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 256
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 4
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 11264
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 24
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 48
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 128
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 16
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 2,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 11264
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 26624
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 3,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 6144
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 48
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 1
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 1
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 12
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 3576
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 3576
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 4,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 28
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 28
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 28
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 12
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 192
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 512
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 192
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 512
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 16
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 1
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT_NON_HA,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 2
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 5,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 26624
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 6,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 6144
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 31
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 64
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 100
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 7,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 16
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 128
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 128
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 16
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 256
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 4
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 8,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 2048
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 256
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 128
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 588
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 2048
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 128
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 128
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 128
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 256
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 6144
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 512
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 32
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	},
	{
	.app_id                  = 9,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 31
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 64
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 2048
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 100
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 10,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 7168
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 7168
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 26624
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 1024
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2048
	},
	{
	.app_id                  = 11,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 6144
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 62
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4080
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 4080
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 128
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_64B,
	.count                   = 512
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 64
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 1024
	},
	{
	.app_id                  = 12,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 4096
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 511
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 15
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC,
	.count                   = 255
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 422
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 88
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 13168
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 148
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 191
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 6912
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_MODIFY_IPV4,
	.count                   = 1023
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 511
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.count                   = 223
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_8B,
	.count                   = 255
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 488
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV6,
	.count                   = 511
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 1
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 292
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 144
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 960
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 928
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 15232
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_TBL_SCOPE,
	.count                   = 1
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 64
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 1024
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 1024
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1000
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 64
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 64
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 2
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 64
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 16
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 2048
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 2048
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 8
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_64B,
	.count                   = 1000
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_SP_SMAC_IPV4,
	.count                   = 100
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 64
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 32
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 2032
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 13,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METER_PROF,
	.count                   = 256
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METER_INST,
	.count                   = 1023
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 31
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_HIGH,
	.count                   = 272
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 6
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_RX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_L2_CTXT_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_WC_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.count                   = 63
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.count                   = 8192
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_STATS_64,
	.count                   = 8192
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_MIRROR_CONFIG,
	.count                   = 5
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_EM_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_WC_FKB,
	.count                   = 32
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_L2_CTXT_TCAM_LOW,
	.count                   = 272
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_PROF_TCAM,
	.count                   = 128
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_TCAM_TABLE,
	.resource_type           = TF_TCAM_TBL_TYPE_WC_TCAM,
	.count                   = 4096
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_EM_TABLE,
	.resource_type           = TF_EM_TBL_TYPE_EM_RECORD,
	.count                   = 16384
	},
	{
	.app_id                  = 14,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.direction               = TF_DIR_TX,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_METADATA,
	.count                   = 1
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
	[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE_NUM] =
		BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE_NUM,
	[BNXT_ULP_ACT_PROP_IDX_RSS_QUEUE] =
		BNXT_ULP_ACT_PROP_SZ_RSS_QUEUE,
	[BNXT_ULP_ACT_PROP_IDX_QUEUE_INDEX] =
		BNXT_ULP_ACT_PROP_SZ_QUEUE_INDEX,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID_UPDATE] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID_UPDATE,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_ID] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_ID,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CIR] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_CIR,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EIR] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_EIR,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBS] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBS,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBS] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBS,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_RFC2698] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_RFC2698,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_PM] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_PM,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBND] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBND,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBND] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBND,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_EBSM] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_EBSM,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CBSM] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_CBSM,
	[BNXT_ULP_ACT_PROP_IDX_METER_PROF_CF] =
		BNXT_ULP_ACT_PROP_SZ_METER_PROF_CF,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_ID] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_ID,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN_UPDATE] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_ECN_RMP_EN_UPDATE,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_ECN_RMP_EN] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_ECN_RMP_EN,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL_UPDATE] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_MTR_VAL_UPDATE,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_MTR_VAL] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_MTR_VAL,
	[BNXT_ULP_ACT_PROP_IDX_GOTO_CHAINID] =
		BNXT_ULP_ACT_PROP_SZ_GOTO_CHAINID,
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

uint8_t ulp_glb_field_tbl[] = {
	[4096] = 0,
	[4097] = 1,
	[4102] = 2,
	[4104] = 3,
	[4106] = 4,
	[4140] = 5,
	[4142] = 6,
	[4144] = 7,
	[4146] = 8,
	[4148] = 9,
	[4150] = 10,
	[4152] = 11,
	[4154] = 12,
	[4224] = 0,
	[4225] = 1,
	[4230] = 2,
	[4232] = 3,
	[4234] = 4,
	[4248] = 5,
	[4250] = 6,
	[4252] = 7,
	[4254] = 8,
	[4256] = 9,
	[4258] = 10,
	[4260] = 11,
	[4262] = 12,
	[4264] = 13,
	[4266] = 14,
	[4352] = 0,
	[4353] = 1,
	[4358] = 2,
	[4360] = 3,
	[4362] = 4,
	[4396] = 8,
	[4398] = 9,
	[4400] = 10,
	[4402] = 11,
	[4404] = 12,
	[4406] = 13,
	[4408] = 14,
	[4410] = 15,
	[4452] = 5,
	[4456] = 6,
	[4460] = 7,
	[4480] = 0,
	[4481] = 1,
	[4486] = 2,
	[4488] = 3,
	[4490] = 4,
	[4504] = 8,
	[4506] = 9,
	[4508] = 10,
	[4510] = 11,
	[4512] = 12,
	[4514] = 13,
	[4516] = 14,
	[4518] = 15,
	[4520] = 16,
	[4522] = 17,
	[4580] = 5,
	[4584] = 6,
	[4588] = 7,
	[4608] = 0,
	[4609] = 1,
	[4614] = 2,
	[4616] = 3,
	[4618] = 4,
	[4652] = 5,
	[4654] = 6,
	[4656] = 7,
	[4658] = 8,
	[4660] = 9,
	[4662] = 10,
	[4664] = 11,
	[4666] = 12,
	[4682] = 13,
	[4684] = 14,
	[4686] = 15,
	[4688] = 16,
	[4690] = 17,
	[4692] = 18,
	[4694] = 19,
	[4696] = 20,
	[4698] = 21,
	[4736] = 0,
	[4737] = 1,
	[4742] = 2,
	[4744] = 3,
	[4746] = 4,
	[4760] = 5,
	[4762] = 6,
	[4764] = 7,
	[4766] = 8,
	[4768] = 9,
	[4770] = 10,
	[4772] = 11,
	[4774] = 12,
	[4776] = 13,
	[4778] = 14,
	[4810] = 15,
	[4812] = 16,
	[4814] = 17,
	[4816] = 18,
	[4818] = 19,
	[4820] = 20,
	[4822] = 21,
	[4824] = 22,
	[4826] = 23,
	[4864] = 0,
	[4865] = 1,
	[4870] = 2,
	[4872] = 3,
	[4874] = 4,
	[4908] = 5,
	[4910] = 6,
	[4912] = 7,
	[4914] = 8,
	[4916] = 9,
	[4918] = 10,
	[4920] = 11,
	[4922] = 12,
	[4956] = 13,
	[4958] = 14,
	[4960] = 15,
	[4962] = 16,
	[4992] = 0,
	[4993] = 1,
	[4998] = 2,
	[5000] = 3,
	[5002] = 4,
	[5016] = 5,
	[5018] = 6,
	[5020] = 7,
	[5022] = 8,
	[5024] = 9,
	[5026] = 10,
	[5028] = 11,
	[5030] = 12,
	[5032] = 13,
	[5034] = 14,
	[5084] = 15,
	[5086] = 16,
	[5088] = 17,
	[5090] = 18,
	[5120] = 0,
	[5121] = 1,
	[5126] = 2,
	[5128] = 3,
	[5130] = 4,
	[5164] = 8,
	[5166] = 9,
	[5168] = 10,
	[5170] = 11,
	[5172] = 12,
	[5174] = 13,
	[5176] = 14,
	[5178] = 15,
	[5194] = 16,
	[5196] = 17,
	[5198] = 18,
	[5200] = 19,
	[5202] = 20,
	[5204] = 21,
	[5206] = 22,
	[5208] = 23,
	[5210] = 24,
	[5220] = 5,
	[5224] = 6,
	[5228] = 7,
	[5248] = 0,
	[5249] = 1,
	[5254] = 2,
	[5256] = 3,
	[5258] = 4,
	[5272] = 8,
	[5274] = 9,
	[5276] = 10,
	[5278] = 11,
	[5280] = 12,
	[5282] = 13,
	[5284] = 14,
	[5286] = 15,
	[5288] = 16,
	[5290] = 17,
	[5322] = 18,
	[5324] = 19,
	[5326] = 20,
	[5328] = 21,
	[5330] = 22,
	[5332] = 23,
	[5334] = 24,
	[5336] = 25,
	[5338] = 26,
	[5348] = 5,
	[5352] = 6,
	[5356] = 7,
	[5376] = 0,
	[5377] = 1,
	[5382] = 2,
	[5384] = 3,
	[5386] = 4,
	[5420] = 8,
	[5422] = 9,
	[5424] = 10,
	[5426] = 11,
	[5428] = 12,
	[5430] = 13,
	[5432] = 14,
	[5434] = 15,
	[5468] = 16,
	[5470] = 17,
	[5472] = 18,
	[5474] = 19,
	[5476] = 5,
	[5480] = 6,
	[5484] = 7,
	[5504] = 0,
	[5505] = 1,
	[5510] = 2,
	[5512] = 3,
	[5514] = 4,
	[5528] = 8,
	[5530] = 9,
	[5532] = 10,
	[5534] = 11,
	[5536] = 12,
	[5538] = 13,
	[5540] = 14,
	[5542] = 15,
	[5544] = 16,
	[5546] = 17,
	[5596] = 18,
	[5598] = 19,
	[5600] = 20,
	[5602] = 21,
	[5604] = 5,
	[5608] = 6,
	[5612] = 7,
	[5632] = 0,
	[5633] = 1,
	[5638] = 2,
	[5640] = 3,
	[5642] = 4,
	[5656] = 5,
	[5658] = 6,
	[5660] = 7,
	[5662] = 8,
	[5664] = 9,
	[5666] = 10,
	[5668] = 11,
	[5670] = 12,
	[5672] = 13,
	[5674] = 14,
	[5724] = 15,
	[5726] = 16,
	[5728] = 17,
	[5730] = 18,
	[5744] = 19,
	[5745] = 20,
	[5746] = 21,
	[5747] = 22,
	[8192] = 0,
	[8193] = 1,
	[8198] = 2,
	[8200] = 3,
	[8202] = 4,
	[8236] = 5,
	[8238] = 6,
	[8240] = 7,
	[8242] = 8,
	[8244] = 9,
	[8246] = 10,
	[8248] = 11,
	[8250] = 12,
	[8284] = 13,
	[8286] = 14,
	[8288] = 15,
	[8290] = 16,
	[8304] = 17,
	[8305] = 18,
	[8306] = 19,
	[8307] = 20,
	[8320] = 0,
	[8321] = 1,
	[8326] = 2,
	[8328] = 3,
	[8330] = 4,
	[8344] = 5,
	[8346] = 6,
	[8348] = 7,
	[8350] = 8,
	[8352] = 9,
	[8354] = 10,
	[8356] = 11,
	[8358] = 12,
	[8360] = 13,
	[8362] = 14,
	[8412] = 15,
	[8414] = 16,
	[8416] = 17,
	[8418] = 18,
	[8432] = 19,
	[8433] = 20,
	[8434] = 21,
	[8435] = 22,
	[8448] = 0,
	[8449] = 1,
	[8455] = 18,
	[8457] = 19,
	[8459] = 20,
	[8492] = 2,
	[8493] = 21,
	[8494] = 3,
	[8495] = 22,
	[8496] = 4,
	[8497] = 23,
	[8498] = 5,
	[8499] = 24,
	[8500] = 6,
	[8501] = 25,
	[8502] = 7,
	[8503] = 26,
	[8504] = 8,
	[8505] = 27,
	[8506] = 9,
	[8507] = 28,
	[8540] = 10,
	[8542] = 11,
	[8544] = 12,
	[8546] = 13,
	[8560] = 14,
	[8561] = 15,
	[8562] = 16,
	[8563] = 17,
	[8576] = 0,
	[8577] = 1,
	[8583] = 20,
	[8585] = 21,
	[8587] = 22,
	[8600] = 2,
	[8602] = 3,
	[8604] = 4,
	[8606] = 5,
	[8608] = 6,
	[8610] = 7,
	[8612] = 8,
	[8614] = 9,
	[8616] = 10,
	[8618] = 11,
	[8621] = 23,
	[8623] = 24,
	[8625] = 25,
	[8627] = 26,
	[8629] = 27,
	[8631] = 28,
	[8633] = 29,
	[8635] = 30,
	[8668] = 12,
	[8670] = 13,
	[8672] = 14,
	[8674] = 15,
	[8688] = 16,
	[8689] = 17,
	[8690] = 18,
	[8691] = 19,
	[8704] = 0,
	[8705] = 1,
	[8711] = 18,
	[8713] = 19,
	[8715] = 20,
	[8729] = 21,
	[8731] = 22,
	[8733] = 23,
	[8735] = 24,
	[8737] = 25,
	[8739] = 26,
	[8741] = 27,
	[8743] = 28,
	[8745] = 29,
	[8747] = 30,
	[8748] = 2,
	[8750] = 3,
	[8752] = 4,
	[8754] = 5,
	[8756] = 6,
	[8758] = 7,
	[8760] = 8,
	[8762] = 9,
	[8796] = 10,
	[8798] = 11,
	[8800] = 12,
	[8802] = 13,
	[8816] = 14,
	[8817] = 15,
	[8818] = 16,
	[8819] = 17,
	[8832] = 0,
	[8833] = 1,
	[8839] = 20,
	[8841] = 21,
	[8843] = 22,
	[8856] = 2,
	[8857] = 23,
	[8858] = 3,
	[8859] = 24,
	[8860] = 4,
	[8861] = 25,
	[8862] = 5,
	[8863] = 26,
	[8864] = 6,
	[8865] = 27,
	[8866] = 7,
	[8867] = 28,
	[8868] = 8,
	[8869] = 29,
	[8870] = 9,
	[8871] = 30,
	[8872] = 10,
	[8873] = 31,
	[8874] = 11,
	[8875] = 32,
	[8924] = 12,
	[8926] = 13,
	[8928] = 14,
	[8930] = 15,
	[8944] = 16,
	[8945] = 17,
	[8946] = 18,
	[8947] = 19,
	[8960] = 0,
	[8961] = 1,
	[8967] = 18,
	[8969] = 19,
	[8971] = 20,
	[9004] = 2,
	[9005] = 21,
	[9006] = 3,
	[9007] = 22,
	[9008] = 4,
	[9009] = 23,
	[9010] = 5,
	[9011] = 24,
	[9012] = 6,
	[9013] = 25,
	[9014] = 7,
	[9015] = 26,
	[9016] = 8,
	[9017] = 27,
	[9018] = 9,
	[9019] = 28,
	[9035] = 29,
	[9037] = 30,
	[9039] = 31,
	[9041] = 32,
	[9043] = 33,
	[9045] = 34,
	[9047] = 35,
	[9049] = 36,
	[9051] = 37,
	[9052] = 10,
	[9054] = 11,
	[9056] = 12,
	[9058] = 13,
	[9072] = 14,
	[9073] = 15,
	[9074] = 16,
	[9075] = 17,
	[9088] = 0,
	[9089] = 1,
	[9095] = 20,
	[9097] = 21,
	[9099] = 22,
	[9112] = 2,
	[9114] = 3,
	[9116] = 4,
	[9118] = 5,
	[9120] = 6,
	[9122] = 7,
	[9124] = 8,
	[9126] = 9,
	[9128] = 10,
	[9130] = 11,
	[9133] = 23,
	[9135] = 24,
	[9137] = 25,
	[9139] = 26,
	[9141] = 27,
	[9143] = 28,
	[9145] = 29,
	[9147] = 30,
	[9163] = 31,
	[9165] = 32,
	[9167] = 33,
	[9169] = 34,
	[9171] = 35,
	[9173] = 36,
	[9175] = 37,
	[9177] = 38,
	[9179] = 39,
	[9180] = 12,
	[9182] = 13,
	[9184] = 14,
	[9186] = 15,
	[9200] = 16,
	[9201] = 17,
	[9202] = 18,
	[9203] = 19,
	[9216] = 0,
	[9217] = 1,
	[9223] = 18,
	[9225] = 19,
	[9227] = 20,
	[9241] = 21,
	[9243] = 22,
	[9245] = 23,
	[9247] = 24,
	[9249] = 25,
	[9251] = 26,
	[9253] = 27,
	[9255] = 28,
	[9257] = 29,
	[9259] = 30,
	[9260] = 2,
	[9262] = 3,
	[9264] = 4,
	[9266] = 5,
	[9268] = 6,
	[9270] = 7,
	[9272] = 8,
	[9274] = 9,
	[9291] = 31,
	[9293] = 32,
	[9295] = 33,
	[9297] = 34,
	[9299] = 35,
	[9301] = 36,
	[9303] = 37,
	[9305] = 38,
	[9307] = 39,
	[9308] = 10,
	[9310] = 11,
	[9312] = 12,
	[9314] = 13,
	[9328] = 14,
	[9329] = 15,
	[9330] = 16,
	[9331] = 17,
	[9344] = 0,
	[9345] = 1,
	[9351] = 20,
	[9353] = 21,
	[9355] = 22,
	[9368] = 2,
	[9369] = 23,
	[9370] = 3,
	[9371] = 24,
	[9372] = 4,
	[9373] = 25,
	[9374] = 5,
	[9375] = 26,
	[9376] = 6,
	[9377] = 27,
	[9378] = 7,
	[9379] = 28,
	[9380] = 8,
	[9381] = 29,
	[9382] = 9,
	[9383] = 30,
	[9384] = 10,
	[9385] = 31,
	[9386] = 11,
	[9387] = 32,
	[9419] = 33,
	[9421] = 34,
	[9423] = 35,
	[9425] = 36,
	[9427] = 37,
	[9429] = 38,
	[9431] = 39,
	[9433] = 40,
	[9435] = 41,
	[9436] = 12,
	[9438] = 13,
	[9440] = 14,
	[9442] = 15,
	[9456] = 16,
	[9457] = 17,
	[9458] = 18,
	[9459] = 19,
	[9472] = 0,
	[9473] = 1,
	[9479] = 18,
	[9481] = 19,
	[9483] = 20,
	[9516] = 2,
	[9517] = 21,
	[9518] = 3,
	[9519] = 22,
	[9520] = 4,
	[9521] = 23,
	[9522] = 5,
	[9523] = 24,
	[9524] = 6,
	[9525] = 25,
	[9526] = 7,
	[9527] = 26,
	[9528] = 8,
	[9529] = 27,
	[9530] = 9,
	[9531] = 28,
	[9564] = 10,
	[9565] = 29,
	[9566] = 11,
	[9567] = 30,
	[9568] = 12,
	[9569] = 31,
	[9570] = 13,
	[9571] = 32,
	[9584] = 14,
	[9585] = 15,
	[9586] = 16,
	[9587] = 17,
	[9600] = 0,
	[9601] = 1,
	[9607] = 20,
	[9609] = 21,
	[9611] = 22,
	[9624] = 2,
	[9626] = 3,
	[9628] = 4,
	[9630] = 5,
	[9632] = 6,
	[9634] = 7,
	[9636] = 8,
	[9638] = 9,
	[9640] = 10,
	[9642] = 11,
	[9645] = 23,
	[9647] = 24,
	[9649] = 25,
	[9651] = 26,
	[9653] = 27,
	[9655] = 28,
	[9657] = 29,
	[9659] = 30,
	[9692] = 12,
	[9693] = 31,
	[9694] = 13,
	[9695] = 32,
	[9696] = 14,
	[9697] = 33,
	[9698] = 15,
	[9699] = 34,
	[9712] = 16,
	[9713] = 17,
	[9714] = 18,
	[9715] = 19,
	[9728] = 0,
	[9729] = 1,
	[9735] = 18,
	[9737] = 19,
	[9739] = 20,
	[9753] = 21,
	[9755] = 22,
	[9757] = 23,
	[9759] = 24,
	[9761] = 25,
	[9763] = 26,
	[9765] = 27,
	[9767] = 28,
	[9769] = 29,
	[9771] = 30,
	[9772] = 2,
	[9774] = 3,
	[9776] = 4,
	[9778] = 5,
	[9780] = 6,
	[9782] = 7,
	[9784] = 8,
	[9786] = 9,
	[9820] = 10,
	[9821] = 31,
	[9822] = 11,
	[9823] = 32,
	[9824] = 12,
	[9825] = 33,
	[9826] = 13,
	[9827] = 34,
	[9840] = 14,
	[9841] = 15,
	[9842] = 16,
	[9843] = 17,
	[9856] = 0,
	[9857] = 1,
	[9863] = 20,
	[9865] = 21,
	[9867] = 22,
	[9880] = 2,
	[9881] = 23,
	[9882] = 3,
	[9883] = 24,
	[9884] = 4,
	[9885] = 25,
	[9886] = 5,
	[9887] = 26,
	[9888] = 6,
	[9889] = 27,
	[9890] = 7,
	[9891] = 28,
	[9892] = 8,
	[9893] = 29,
	[9894] = 9,
	[9895] = 30,
	[9896] = 10,
	[9897] = 31,
	[9898] = 11,
	[9899] = 32,
	[9948] = 12,
	[9949] = 33,
	[9950] = 13,
	[9951] = 34,
	[9952] = 14,
	[9953] = 35,
	[9954] = 15,
	[9955] = 36,
	[9968] = 16,
	[9969] = 17,
	[9970] = 18,
	[9971] = 19,
	[9984] = 0,
	[9985] = 1,
	[9991] = 18,
	[9993] = 19,
	[9995] = 20,
	[9999] = 31,
	[10001] = 32,
	[10003] = 33,
	[10005] = 34,
	[10007] = 35,
	[10009] = 21,
	[10011] = 22,
	[10013] = 23,
	[10015] = 24,
	[10017] = 25,
	[10019] = 26,
	[10021] = 27,
	[10023] = 28,
	[10025] = 29,
	[10027] = 30,
	[10028] = 2,
	[10030] = 3,
	[10032] = 4,
	[10034] = 5,
	[10036] = 6,
	[10038] = 7,
	[10040] = 8,
	[10042] = 9,
	[10076] = 10,
	[10078] = 11,
	[10080] = 12,
	[10082] = 13,
	[10096] = 14,
	[10097] = 15,
	[10098] = 16,
	[10099] = 17,
	[10112] = 0,
	[10113] = 1,
	[10119] = 20,
	[10121] = 21,
	[10123] = 22,
	[10127] = 33,
	[10129] = 34,
	[10131] = 35,
	[10133] = 36,
	[10135] = 37,
	[10136] = 2,
	[10137] = 23,
	[10138] = 3,
	[10139] = 24,
	[10140] = 4,
	[10141] = 25,
	[10142] = 5,
	[10143] = 26,
	[10144] = 6,
	[10145] = 27,
	[10146] = 7,
	[10147] = 28,
	[10148] = 8,
	[10149] = 29,
	[10150] = 9,
	[10151] = 30,
	[10152] = 10,
	[10153] = 31,
	[10154] = 11,
	[10155] = 32,
	[10204] = 12,
	[10206] = 13,
	[10208] = 14,
	[10210] = 15,
	[10224] = 16,
	[10225] = 17,
	[10226] = 18,
	[10227] = 19,
	[12288] = 0,
	[12289] = 1,
	[12294] = 2,
	[12296] = 3,
	[12298] = 4,
	[12332] = 5,
	[12334] = 6,
	[12336] = 7,
	[12338] = 8,
	[12340] = 9,
	[12342] = 10,
	[12344] = 11,
	[12346] = 12,
	[12416] = 0,
	[12417] = 1,
	[12422] = 2,
	[12424] = 3,
	[12426] = 4,
	[12440] = 5,
	[12442] = 6,
	[12444] = 7,
	[12446] = 8,
	[12448] = 9,
	[12450] = 10,
	[12452] = 11,
	[12454] = 12,
	[12456] = 13,
	[12458] = 14,
	[12544] = 0,
	[12545] = 1,
	[12550] = 2,
	[12552] = 3,
	[12554] = 4,
	[12588] = 8,
	[12590] = 9,
	[12592] = 10,
	[12594] = 11,
	[12596] = 12,
	[12598] = 13,
	[12600] = 14,
	[12602] = 15,
	[12644] = 5,
	[12648] = 6,
	[12652] = 7,
	[12672] = 0,
	[12673] = 1,
	[12678] = 2,
	[12680] = 3,
	[12682] = 4,
	[12696] = 8,
	[12698] = 9,
	[12700] = 10,
	[12702] = 11,
	[12704] = 12,
	[12706] = 13,
	[12708] = 14,
	[12710] = 15,
	[12712] = 16,
	[12714] = 17,
	[12772] = 5,
	[12776] = 6,
	[12780] = 7,
	[12800] = 0,
	[12801] = 1,
	[12806] = 2,
	[12808] = 3,
	[12810] = 4,
	[12844] = 5,
	[12846] = 6,
	[12848] = 7,
	[12850] = 8,
	[12852] = 9,
	[12854] = 10,
	[12856] = 11,
	[12858] = 12,
	[12874] = 13,
	[12876] = 14,
	[12878] = 15,
	[12880] = 16,
	[12882] = 17,
	[12884] = 18,
	[12886] = 19,
	[12888] = 20,
	[12890] = 21,
	[12928] = 0,
	[12929] = 1,
	[12934] = 2,
	[12936] = 3,
	[12938] = 4,
	[12952] = 5,
	[12954] = 6,
	[12956] = 7,
	[12958] = 8,
	[12960] = 9,
	[12962] = 10,
	[12964] = 11,
	[12966] = 12,
	[12968] = 13,
	[12970] = 14,
	[13002] = 15,
	[13004] = 16,
	[13006] = 17,
	[13008] = 18,
	[13010] = 19,
	[13012] = 20,
	[13014] = 21,
	[13016] = 22,
	[13018] = 23,
	[13056] = 0,
	[13057] = 1,
	[13062] = 2,
	[13064] = 3,
	[13066] = 4,
	[13100] = 5,
	[13102] = 6,
	[13104] = 7,
	[13106] = 8,
	[13108] = 9,
	[13110] = 10,
	[13112] = 11,
	[13114] = 12,
	[13148] = 13,
	[13150] = 14,
	[13152] = 15,
	[13154] = 16,
	[13184] = 0,
	[13185] = 1,
	[13190] = 2,
	[13192] = 3,
	[13194] = 4,
	[13208] = 5,
	[13210] = 6,
	[13212] = 7,
	[13214] = 8,
	[13216] = 9,
	[13218] = 10,
	[13220] = 11,
	[13222] = 12,
	[13224] = 13,
	[13226] = 14,
	[13276] = 15,
	[13278] = 16,
	[13280] = 17,
	[13282] = 18,
	[13312] = 0,
	[13313] = 1,
	[13318] = 2,
	[13320] = 3,
	[13322] = 4,
	[13356] = 8,
	[13358] = 9,
	[13360] = 10,
	[13362] = 11,
	[13364] = 12,
	[13366] = 13,
	[13368] = 14,
	[13370] = 15,
	[13386] = 16,
	[13388] = 17,
	[13390] = 18,
	[13392] = 19,
	[13394] = 20,
	[13396] = 21,
	[13398] = 22,
	[13400] = 23,
	[13402] = 24,
	[13412] = 5,
	[13416] = 6,
	[13420] = 7,
	[13440] = 0,
	[13441] = 1,
	[13446] = 2,
	[13448] = 3,
	[13450] = 4,
	[13464] = 8,
	[13466] = 9,
	[13468] = 10,
	[13470] = 11,
	[13472] = 12,
	[13474] = 13,
	[13476] = 14,
	[13478] = 15,
	[13480] = 16,
	[13482] = 17,
	[13514] = 18,
	[13516] = 19,
	[13518] = 20,
	[13520] = 21,
	[13522] = 22,
	[13524] = 23,
	[13526] = 24,
	[13528] = 25,
	[13530] = 26,
	[13540] = 5,
	[13544] = 6,
	[13548] = 7,
	[13568] = 0,
	[13569] = 1,
	[13574] = 2,
	[13576] = 3,
	[13578] = 4,
	[13612] = 8,
	[13614] = 9,
	[13616] = 10,
	[13618] = 11,
	[13620] = 12,
	[13622] = 13,
	[13624] = 14,
	[13626] = 15,
	[13660] = 16,
	[13662] = 17,
	[13664] = 18,
	[13666] = 19,
	[13668] = 5,
	[13672] = 6,
	[13676] = 7,
	[13696] = 0,
	[13697] = 1,
	[13702] = 2,
	[13704] = 3,
	[13706] = 4,
	[13720] = 8,
	[13722] = 9,
	[13724] = 10,
	[13726] = 11,
	[13728] = 12,
	[13730] = 13,
	[13732] = 14,
	[13734] = 15,
	[13736] = 16,
	[13738] = 17,
	[13788] = 18,
	[13790] = 19,
	[13792] = 20,
	[13794] = 21,
	[13796] = 5,
	[13800] = 6,
	[13804] = 7
};
