/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Tue Dec  1 10:17:11 2020 */

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
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MIRROR_TBL << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.result_num_entries      = 16,
	.result_num_bytes        = 16,
	.result_byte_order       = BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MIRROR_TBL << 1 |
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
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MIRROR_TBL << 1 |
		BNXT_ULP_DIRECTION_INGRESS] = {
	.act_bitmask             = BNXT_ULP_ACT_BIT_SHARED_SAMPLE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_GENERIC_TABLE_MIRROR_TBL << 1 |
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
	[2076] = 5,
	[2078] = 6,
	[2080] = 7,
	[2082] = 8,
	[2084] = 9,
	[2086] = 10,
	[2088] = 11,
	[2090] = 12,
	[2102] = 13,
	[2104] = 14,
	[2106] = 15,
	[2108] = 16,
	[2110] = 17,
	[2112] = 18,
	[2114] = 19,
	[2116] = 20,
	[2118] = 21,
	[2176] = 0,
	[2177] = 1,
	[2178] = 2,
	[2180] = 3,
	[2182] = 4,
	[2204] = 8,
	[2206] = 9,
	[2208] = 10,
	[2210] = 11,
	[2212] = 12,
	[2214] = 13,
	[2216] = 14,
	[2218] = 15,
	[2230] = 16,
	[2232] = 17,
	[2234] = 18,
	[2236] = 19,
	[2238] = 20,
	[2240] = 21,
	[2242] = 22,
	[2244] = 23,
	[2246] = 24,
	[2256] = 5,
	[2260] = 6,
	[2264] = 7,
	[2304] = 0,
	[2305] = 1,
	[2306] = 2,
	[2308] = 3,
	[2310] = 4,
	[2312] = 5,
	[2314] = 6,
	[2316] = 7,
	[2318] = 8,
	[2320] = 9,
	[2322] = 10,
	[2324] = 11,
	[2326] = 12,
	[2328] = 13,
	[2330] = 14,
	[2358] = 15,
	[2360] = 16,
	[2362] = 17,
	[2364] = 18,
	[2366] = 19,
	[2368] = 20,
	[2370] = 21,
	[2372] = 22,
	[2374] = 23,
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
	[2716] = 8,
	[2718] = 9,
	[2720] = 10,
	[2722] = 11,
	[2724] = 12,
	[2726] = 13,
	[2728] = 14,
	[2730] = 15,
	[2760] = 16,
	[2762] = 17,
	[2764] = 18,
	[2766] = 19,
	[2768] = 5,
	[2772] = 6,
	[2776] = 7,
	[2816] = 0,
	[2817] = 1,
	[2818] = 2,
	[2820] = 3,
	[2822] = 4,
	[2824] = 5,
	[2826] = 6,
	[2828] = 7,
	[2830] = 8,
	[2832] = 9,
	[2834] = 10,
	[2836] = 11,
	[2838] = 12,
	[2840] = 13,
	[2842] = 14,
	[2888] = 15,
	[2890] = 16,
	[2892] = 17,
	[2894] = 18,
	[2944] = 0,
	[2945] = 1,
	[2946] = 2,
	[2948] = 3,
	[2950] = 4,
	[2952] = 8,
	[2954] = 9,
	[2956] = 10,
	[2958] = 11,
	[2960] = 12,
	[2962] = 13,
	[2964] = 14,
	[2966] = 15,
	[2968] = 16,
	[2970] = 17,
	[3016] = 18,
	[3018] = 19,
	[3020] = 20,
	[3022] = 21,
	[3024] = 5,
	[3028] = 6,
	[3032] = 7
};

