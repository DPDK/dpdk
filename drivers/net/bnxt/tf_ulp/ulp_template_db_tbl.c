/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Fri Dec  4 19:01:47 2020 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_template_db_tbl.h"

/* Specifies parameters for the cache and shared tables */
struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.result_num_entries      = 16384,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_L2_CNTXT_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.result_num_entries      = 16384,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.result_num_entries      = 16384,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_PROFILE_TCAM << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.result_num_entries      = 16384,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.result_num_entries      = 16,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_SHARED_MIRROR << 1 |
		BNXT_ULP_DIRECTION_EGRESS] = {
	.result_num_entries      = 16,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	}
};

/* device tables */
const struct bnxt_ulp_template_device_tbls ulp_template_stingray_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_stingray_class_tmpl_list,
	.tmpl_list_size          = ULP_STINGRAY_CLASS_TMPL_LIST_SIZE,
	.tbl_list                = ulp_stingray_class_tbl_list,
	.tbl_list_size           = ULP_STINGRAY_CLASS_TBL_LIST_SIZE,
	.key_info_list           = ulp_stingray_class_key_info_list,
	.key_info_list_size      = ULP_STINGRAY_CLASS_KEY_INFO_LIST_SIZE,
	.ident_list              = ulp_stingray_class_ident_list,
	.ident_list_size         = ULP_STINGRAY_CLASS_IDENT_LIST_SIZE,
	.cond_list               = ulp_stingray_class_cond_list,
	.cond_list_size          = ULP_STINGRAY_CLASS_COND_LIST_SIZE,
	.result_field_list       = ulp_stingray_class_result_field_list,
	.result_field_list_size  = ULP_STINGRAY_CLASS_RESULT_FIELD_LIST_SIZE
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_stingray_act_tmpl_list,
	.tmpl_list_size          = ULP_STINGRAY_ACT_TMPL_LIST_SIZE,
	.tbl_list                = ulp_stingray_act_tbl_list,
	.tbl_list_size           = ULP_STINGRAY_ACT_TBL_LIST_SIZE,
	.cond_list               = ulp_stingray_act_cond_list,
	.cond_list_size          = ULP_STINGRAY_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_stingray_act_result_field_list,
	.result_field_list_size  = ULP_STINGRAY_ACT_RESULT_FIELD_LIST_SIZE
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
	.cond_list               = ulp_wh_plus_act_cond_list,
	.cond_list_size          = ULP_WH_PLUS_ACT_COND_LIST_SIZE,
	.result_field_list       = ulp_wh_plus_act_result_field_list,
	.result_field_list_size  = ULP_WH_PLUS_ACT_RESULT_FIELD_LIST_SIZE
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
	.packet_count_mask       = 0xffffffff00000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 36,
	.dev_tbls                = ulp_template_wh_plus_tbls
	},
	[BNXT_ULP_DEVICE_ID_STINGRAY] = {
	.description             = "Stingray",
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
	.packet_count_mask       = 0xffffffff00000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 36,
	.dev_tbls                = ulp_template_stingray_tbls
	}
};

/* List of device specific parameters */
struct bnxt_ulp_glb_resource_info ulp_glb_resource_tbl[] = {
	[0] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[1] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	[2] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	[3] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[4] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	[5] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[6] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	[7] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_RF_IDX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	}
};

/* Lists global action records */
uint32_t ulp_glb_template_tbl[] = {
	BNXT_ULP_DF_TPL_LOOPBACK_ACTION_REC
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
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

uint8_t ulp_glb_field_tbl[] = {
	[2048] = 0,
	[2049] = 1,
	[2050] = 2,
	[2052] = 3,
	[2054] = 4,
	[2056] = 5,
	[2058] = 6,
	[2060] = 7,
	[2062] = 8,
	[2064] = 9,
	[2066] = 10,
	[2068] = 11,
	[2070] = 12,
	[2072] = 13,
	[2074] = 14,
	[2102] = 15,
	[2104] = 16,
	[2106] = 17,
	[2108] = 18,
	[2110] = 19,
	[2112] = 20,
	[2114] = 21,
	[2116] = 22,
	[2118] = 23,
	[2176] = 0,
	[2177] = 1,
	[2178] = 2,
	[2180] = 3,
	[2182] = 4,
	[2184] = 5,
	[2186] = 6,
	[2188] = 7,
	[2190] = 8,
	[2192] = 9,
	[2194] = 10,
	[2196] = 11,
	[2198] = 12,
	[2200] = 13,
	[2202] = 14,
	[2248] = 15,
	[2250] = 16,
	[2252] = 17,
	[2254] = 18,
	[2304] = 0,
	[2305] = 1,
	[2306] = 2,
	[2308] = 3,
	[2310] = 4,
	[2332] = 5,
	[2334] = 6,
	[2336] = 7,
	[2338] = 8,
	[2340] = 9,
	[2342] = 10,
	[2344] = 11,
	[2346] = 12,
	[2358] = 13,
	[2360] = 14,
	[2362] = 15,
	[2364] = 16,
	[2366] = 17,
	[2368] = 18,
	[2370] = 19,
	[2372] = 20,
	[2374] = 21,
	[2432] = 0,
	[2433] = 1,
	[2434] = 2,
	[2436] = 3,
	[2438] = 4,
	[2460] = 5,
	[2462] = 6,
	[2464] = 7,
	[2466] = 8,
	[2468] = 9,
	[2470] = 10,
	[2472] = 11,
	[2474] = 12,
	[2504] = 13,
	[2506] = 14,
	[2508] = 15,
	[2510] = 16,
	[2560] = 0,
	[2561] = 1,
	[2562] = 2,
	[2564] = 3,
	[2566] = 4,
	[2568] = 8,
	[2570] = 9,
	[2572] = 10,
	[2574] = 11,
	[2576] = 12,
	[2578] = 13,
	[2580] = 14,
	[2582] = 15,
	[2584] = 16,
	[2586] = 17,
	[2614] = 18,
	[2616] = 19,
	[2618] = 20,
	[2620] = 21,
	[2622] = 22,
	[2624] = 23,
	[2626] = 24,
	[2628] = 25,
	[2630] = 26,
	[2640] = 5,
	[2644] = 6,
	[2648] = 7,
	[2688] = 0,
	[2689] = 1,
	[2690] = 2,
	[2692] = 3,
	[2694] = 4,
	[2696] = 8,
	[2698] = 9,
	[2700] = 10,
	[2702] = 11,
	[2704] = 12,
	[2706] = 13,
	[2708] = 14,
	[2710] = 15,
	[2712] = 16,
	[2714] = 17,
	[2760] = 18,
	[2762] = 19,
	[2764] = 20,
	[2766] = 21,
	[2768] = 5,
	[2772] = 6,
	[2776] = 7,
	[2816] = 0,
	[2817] = 1,
	[2818] = 2,
	[2820] = 3,
	[2822] = 4,
	[2844] = 8,
	[2846] = 9,
	[2848] = 10,
	[2850] = 11,
	[2852] = 12,
	[2854] = 13,
	[2856] = 14,
	[2858] = 15,
	[2870] = 16,
	[2872] = 17,
	[2874] = 18,
	[2876] = 19,
	[2878] = 20,
	[2880] = 21,
	[2882] = 22,
	[2884] = 23,
	[2886] = 24,
	[2896] = 5,
	[2900] = 6,
	[2904] = 7,
	[2944] = 0,
	[2945] = 1,
	[2946] = 2,
	[2948] = 3,
	[2950] = 4,
	[2972] = 8,
	[2974] = 9,
	[2976] = 10,
	[2978] = 11,
	[2980] = 12,
	[2982] = 13,
	[2984] = 14,
	[2986] = 15,
	[3016] = 16,
	[3018] = 17,
	[3020] = 18,
	[3022] = 19,
	[3024] = 5,
	[3028] = 6,
	[3032] = 7
};

