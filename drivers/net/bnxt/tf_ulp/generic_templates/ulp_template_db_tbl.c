/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2025 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

const struct bnxt_ulp_generic_tbl_params ulp_wh_plus_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32768,
	.result_num_bytes = 16,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32768,
	.result_num_bytes = 16,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 13,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 13,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 23,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 7,
	.key_num_bytes = 3,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 18,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 30,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	}
};

const struct bnxt_ulp_generic_tbl_params ulp_thor_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32768,
	.result_num_bytes = 18,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32768,
	.result_num_bytes = 18,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 14,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 23,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 23,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 7,
	.key_num_bytes = 3,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 8,
	.key_num_bytes = 11,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 18,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 30,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 6,
	.key_num_bytes = 22,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 8192,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 10,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 512,
	.result_num_bytes = 6,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 62,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 14,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 14,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 12,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 12,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 8,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 8,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 6,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 128,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 6,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 128,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 16,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 64,
	.result_num_bytes = 11,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 256,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 6,
	.key_num_bytes = 56,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 6,
	.key_num_bytes = 56,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 8,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 6,
	.key_num_bytes = 6,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 6,
	.key_num_bytes = 6,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	}
};

const struct bnxt_ulp_generic_tbl_params ulp_thor2_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 12,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_CNTXT_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 12,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROFILE_TCAM",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 5,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 12,
	.key_num_bytes = 17,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MAC_ADDR_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MAC_ADDR_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 22,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PORT_TABLE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PORT_TABLE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 22,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 23,
	.key_num_bytes = 3,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 11,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 8192,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 8,
	.key_num_bytes = 11,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 18,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOCKET_DIRECT_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOCKET_DIRECT_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_VXLAN_ENCAP_IPV6_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 30,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SOURCE_PROPERTY_IPV6_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 8,
	.key_num_bytes = 23,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_OUTER_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_OUTER_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 1024,
	.result_num_bytes = 18,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 512,
	.result_num_bytes = 13,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 2048,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_METER_PROFILE_TBL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_METER_PROFILE_TBL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_CHAIN_ID_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_CHAIN_ID_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 6,
	.key_num_bytes = 15,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 8192,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_L2_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SRV6_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SRV6_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 6,
	.key_num_bytes = 87,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 8192,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_RSS_PARAMS << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_RSS_PARAMS",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 64,
	.result_num_bytes = 11,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TABLE_SCOPE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TABLE_SCOPE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 64,
	.result_num_bytes = 11,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GENEVE_ENCAP_REC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 4096,
	.result_num_bytes = 8,
	.key_num_bytes = 62,
	.partial_key_num_bytes = 0,
	.num_buckets = 8,
	.hash_tbl_entries = 16384,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 14,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROTO_HEADER << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROTO_HEADER",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 14,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 12,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_EM_FLOW_CONFLICT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_EM_FLOW_CONFLICT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 12,
	.key_num_bytes = 10,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 1024,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 8,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HDR_OVERLAP << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HDR_OVERLAP",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 256,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 8,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_SHARED_MIRROR",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_KEY_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 8,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 128,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 32,
	.result_num_bytes = 8,
	.key_num_bytes = 4,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 128,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 16,
	.result_num_bytes = 6,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_FLOW_CHAIN_L2_CNTXT",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 64,
	.result_num_bytes = 13,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 256,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_TUNNEL_GPARSE_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_TUNNEL_GPARSE_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 8,
	.key_num_bytes = 56,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_MULTI_FLOW_TUNNEL_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 128,
	.result_num_bytes = 8,
	.key_num_bytes = 56,
	.partial_key_num_bytes = 0,
	.num_buckets = 4,
	.hash_tbl_entries = 512,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROF_FUNC_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_PROF_FUNC_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GROUP_MISS_ACTION_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GROUP_MISS_ACTION_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_METER_STATS_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_SHARED_METER_STATS_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_HASH_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 2048,
	.result_num_bytes = 9,
	.key_num_bytes = 2,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_HOT_UPGRADE_TCAM_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 8,
	.result_num_bytes = 4,
	.key_num_bytes = 1,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_GLOBAL_REGFILES_CACHE << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS GENERIC_TABLE_GLOBAL_REGFILES_CACHE",
	.gen_tbl_type = BNXT_ULP_GEN_TBL_TYPE_SIMPLE_LIST,
	.result_num_entries = 0,
	.result_num_bytes = 0,
	.key_num_bytes = 0,
	.partial_key_num_bytes = 0,
	.num_buckets = 0,
	.hash_tbl_entries = 0,
	.result_byte_order = BNXT_ULP_BYTE_ORDER_LE,
	}
};

const struct bnxt_ulp_allocator_tbl_params ulp_wh_plus_allocator_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 0,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 0,
	}
};

const struct bnxt_ulp_allocator_tbl_params ulp_thor_allocator_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 32,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 32,
	}
};

const struct bnxt_ulp_allocator_tbl_params ulp_thor2_allocator_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.name = "INGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 32,
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_ALLOCATOR_TABLE_JUMP_INDEX << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.name = "EGRESS ALLOCATOR_TABLE_JUMP_INDEX",
	.num_entries = 32,
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
	.key_ext_list            = ulp_wh_plus_class_key_ext_list,
	.key_ext_list_size       = ULP_WH_PLUS_CLASS_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_wh_plus_class_ident_list,
	.ident_list_size         = ULP_WH_PLUS_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_wh_plus_class_cond_list,
	.cond_list_size          = ULP_WH_PLUS_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_wh_plus_class_result_field_list,
	.result_field_list_size  = ULP_WH_PLUS_CLASS_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_wh_plus_class_cond_oper_list,
	.cond_oper_list_size     = ULP_WH_PLUS_CLASS_COND_OPER_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_wh_plus_act_tmpl_list,
	.tmpl_list_size          = ULP_WH_PLUS_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_wh_plus_act_tbl_list,
	.tbl_list_size           = ULP_WH_PLUS_ACT_TBL_LIST_SIZE,
	.key_info_list           = ulp_wh_plus_act_key_info_list,
	.key_info_list_size      = ULP_WH_PLUS_ACT_KEY_INFO_LIST_SIZE,
	.key_ext_list            = ulp_wh_plus_act_key_ext_list,
	.key_ext_list_size       = ULP_WH_PLUS_ACT_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_wh_plus_act_ident_list,
	.ident_list_size         = ULP_WH_PLUS_ACT_IDENT_LIST_SIZE,
	.cond_list               = ulp_wh_plus_act_cond_list,
	.cond_list_size          = ULP_WH_PLUS_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_wh_plus_act_result_field_list,
	.result_field_list_size  = ULP_WH_PLUS_ACT_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_wh_plus_act_cond_oper_list,
	.cond_oper_list_size     = ULP_WH_PLUS_ACT_COND_OPER_LIST_SIZE
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
	.key_ext_list            = ulp_thor_class_key_ext_list,
	.key_ext_list_size       = ULP_THOR_CLASS_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_thor_class_ident_list,
	.ident_list_size         = ULP_THOR_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_thor_class_cond_list,
	.cond_list_size          = ULP_THOR_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_thor_class_result_field_list,
	.result_field_list_size  = ULP_THOR_CLASS_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_thor_class_cond_oper_list,
	.cond_oper_list_size     = ULP_THOR_CLASS_COND_OPER_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_thor_act_tmpl_list,
	.tmpl_list_size          = ULP_THOR_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_thor_act_tbl_list,
	.tbl_list_size           = ULP_THOR_ACT_TBL_LIST_SIZE,
	.key_info_list           = ulp_thor_act_key_info_list,
	.key_info_list_size      = ULP_THOR_ACT_KEY_INFO_LIST_SIZE,
	.key_ext_list            = ulp_thor_act_key_ext_list,
	.key_ext_list_size       = ULP_THOR_ACT_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_thor_act_ident_list,
	.ident_list_size         = ULP_THOR_ACT_IDENT_LIST_SIZE,
	.cond_list               = ulp_thor_act_cond_list,
	.cond_list_size          = ULP_THOR_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_thor_act_result_field_list,
	.result_field_list_size  = ULP_THOR_ACT_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_thor_act_cond_oper_list,
	.cond_oper_list_size     = ULP_THOR_ACT_COND_OPER_LIST_SIZE
	}
};

/* device tables */
const struct bnxt_ulp_template_device_tbls ulp_template_thor2_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_thor2_class_tmpl_list,
	.tmpl_list_size          = ULP_THOR2_CLASS_TMPL_LIST_SIZE,
	.tbl_list                = ulp_thor2_class_tbl_list,
	.tbl_list_size           = ULP_THOR2_CLASS_TBL_LIST_SIZE,
	.key_info_list           = ulp_thor2_class_key_info_list,
	.key_info_list_size      = ULP_THOR2_CLASS_KEY_INFO_LIST_SIZE,
	.key_ext_list            = ulp_thor2_class_key_ext_list,
	.key_ext_list_size       = ULP_THOR2_CLASS_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_thor2_class_ident_list,
	.ident_list_size         = ULP_THOR2_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_thor2_class_cond_list,
	.cond_list_size          = ULP_THOR2_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_thor2_class_result_field_list,
	.result_field_list_size  = ULP_THOR2_CLASS_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_thor2_class_cond_oper_list,
	.cond_oper_list_size     = ULP_THOR2_CLASS_COND_OPER_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_thor2_act_tmpl_list,
	.tmpl_list_size          = ULP_THOR2_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_thor2_act_tbl_list,
	.tbl_list_size           = ULP_THOR2_ACT_TBL_LIST_SIZE,
	.key_info_list           = ulp_thor2_act_key_info_list,
	.key_info_list_size      = ULP_THOR2_ACT_KEY_INFO_LIST_SIZE,
	.key_ext_list            = ulp_thor2_act_key_ext_list,
	.key_ext_list_size       = ULP_THOR2_ACT_KEY_EXT_LIST_SIZE,
	.ident_list              = ulp_thor2_act_ident_list,
	.ident_list_size         = ULP_THOR2_ACT_IDENT_LIST_SIZE,
	.cond_list               = ulp_thor2_act_cond_list,
	.cond_list_size          = ULP_THOR2_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_thor2_act_result_field_list,
	.result_field_list_size  = ULP_THOR2_ACT_RESULT_FIELD_LIST_SIZE,
	.cond_oper_list          = ulp_thor2_act_cond_oper_list,
	.cond_oper_list_size     = ULP_THOR2_ACT_COND_OPER_LIST_SIZE
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
	.gen_tbl_params          = ulp_wh_plus_generic_tbl_params,
	.allocator_tbl_params    = ulp_wh_plus_allocator_tbl_params,
	.dev_tbls                = ulp_template_wh_plus_tbls,
	.dev_features            = BNXT_ULP_DEV_FT_STAT_SW_AGG | BNXT_ULP_DEV_FT_STAT_PARENT_AGG
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
	.gen_tbl_params          = ulp_thor_generic_tbl_params,
	.allocator_tbl_params    = ulp_thor_allocator_tbl_params,
	.dev_tbls                = ulp_template_thor_tbls,
	.dev_features            = BNXT_ULP_DEV_FT_STAT_SW_AGG | BNXT_ULP_DEV_FT_STAT_PARENT_AGG
	},
	[BNXT_ULP_DEVICE_ID_THOR2] = {
	.description             = "Thor2",
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
	.em_blk_size_bits        = 256,
	.em_blk_align_bits       = 128,
	.em_key_align_bytes      = 16,
	.wc_slice_width          = 172,
	.wc_max_slices           = 4,
	.wc_mode_list            = {0x00000004, 0x00000005,
					0x00000000, 0x00000006},
	.wc_mod_list_max_size    = 4,
	.wc_ctl_size_bits        = 3,
	.gen_tbl_params          = ulp_thor2_generic_tbl_params,
	.allocator_tbl_params    = ulp_thor2_allocator_tbl_params,
	.dev_tbls                = ulp_template_thor2_tbls
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
	.flags                   = BNXT_ULP_APP_CAP_L2_ETYPE,
	.default_priority        = 0,
	.max_def_priority        = 0,
	.min_flow_priority       = 65535,
	.max_flow_priority       = 0,
	.vxlan_port              = 4789,
	.vxlan_ip_port           = 0,
	.feature_bits            = BNXT_ULP_FEATURE_BIT_NON_VFR_MODE,
	.default_class_bits      = 0,
	.default_act_bits        = BNXT_ULP_ACT_BIT_NON_GENERIC
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR,
	.flags                   = 0,
	.default_priority        = 0,
	.max_def_priority        = 0,
	.min_flow_priority       = 65535,
	.max_flow_priority       = 0,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.num_key_recipes_per_dir = 256,
	.feature_bits            = BNXT_ULP_FEATURE_BIT_PORT_DMAC |
			BNXT_ULP_FEATURE_BIT_PARENT_DMAC |
			BNXT_ULP_FEATURE_BIT_MULTI_TUNNEL_FLOW |
			BNXT_ULP_FEATURE_BIT_SOCKET_DIRECT |
			BNXT_ULP_FEATURE_BIT_NON_VFR_MODE,
	.default_class_bits      = 0,
	.default_act_bits        = BNXT_ULP_ACT_BIT_GENERIC
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_THOR2,
	.flags                   = 0,
	.default_priority        = 0,
	.max_def_priority        = 0,
	.min_flow_priority       = 0,
	.max_flow_priority       = 65535,
	.vxlan_port              = 0,
	.vxlan_ip_port           = 0,
	.max_pools               = 1,
	.em_multiplier           = 16,
	.num_rx_flows            = 768000,
	.num_tx_flows            = 768000,
	.act_rx_max_sz           = 256,
	.act_tx_max_sz           = 256,
	.em_rx_key_max_sz        = 112,
	.em_tx_key_max_sz        = 112,
	.pbl_page_sz_in_bytes    = 4096,
	.num_key_recipes_per_dir = 256,
	.feature_bits            = BNXT_ULP_FEATURE_BIT_PORT_DMAC |
			BNXT_ULP_FEATURE_BIT_PARENT_DMAC |
			BNXT_ULP_FEATURE_BIT_MULTI_TUNNEL_FLOW |
			BNXT_ULP_FEATURE_BIT_MULTI_INSTANCE |
			BNXT_ULP_FEATURE_BIT_SOCKET_DIRECT |
			BNXT_ULP_FEATURE_BIT_SPECIAL_VXLAN |
			BNXT_ULP_FEATURE_BIT_HOT_UPGRADE |
			BNXT_ULP_FEATURE_BIT_NON_VFR_MODE |
			BNXT_ULP_FEATURE_BIT_UNICAST_ONLY |
			BNXT_ULP_FEATURE_BIT_RX_MISS_SEND_TO_PF,
	.default_class_bits      = 0,
	.default_act_bits        = BNXT_ULP_ACT_BIT_GENERIC
	}
};

/* List of unnamed app tf resources required to be reserved per app/device */
struct bnxt_ulp_resource_resv_info ulp_app_resource_resv_list[] = {};

/* List of global app tf resources required to be reserved per app/device */
struct bnxt_ulp_glb_resource_info ulp_app_glb_resource_tbl[] = {};

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
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VF_2_VFR_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ANY_2_VF_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
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
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
	.session_type            = BNXT_ULP_SESSION_TYPE_DEFAULT,
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_EM_PROF,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_VFR_EM_PROF_ID_0,
	.direction               = TF_DIR_RX
	},
	{
	.app_id                  = 0,
	.device_id               = BNXT_ULP_DEVICE_ID_WH_PLUS,
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
	.count                   = 512
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
	[BNXT_ULP_ACT_PROP_IDX_RSS_FUNC] =
		BNXT_ULP_ACT_PROP_SZ_RSS_FUNC,
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
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_CIR] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_CIR,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_EIR] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_EIR,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_CBS] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_CBS,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_EBS] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_EBS,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_RFC2698] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_RFC2698,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_PM] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_PM,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_EBND] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_EBND,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_CBND] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_CBND,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_EBSM] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_EBSM,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_CBSM] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_CBSM,
	[BNXT_ULP_ACT_PROP_IDX_METER_INST_CF] =
		BNXT_ULP_ACT_PROP_SZ_METER_INST_CF,
	[BNXT_ULP_ACT_PROP_IDX_GOTO_CHAINID] =
		BNXT_ULP_ACT_PROP_SZ_GOTO_CHAINID,
	[BNXT_ULP_ACT_PROP_IDX_SET_TTL] =
		BNXT_ULP_ACT_PROP_SZ_SET_TTL,
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};
