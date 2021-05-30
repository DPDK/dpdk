/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

/* date: Thu Oct 15 17:28:37 2020 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"
#include "ulp_template_db_tbl.h"

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
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

uint8_t ulp_glb_field_tbl[211] = {
	[0] = 0,
	[1] = 0,
	[2] = 0,
	[3] = 0,
	[4] = 0,
	[5] = 0,
	[6] = 0,
	[7] = 0,
	[8] = 0,
	[9] = 0,
	[10] = 0,
	[11] = 0,
	[12] = 0,
	[13] = 0,
	[14] = 0,
	[15] = 0,
	[16] = 0,
	[17] = 0,
	[18] = 0,
	[19] = 0,
	[20] = 0,
	[21] = 0,
	[22] = 0,
	[23] = 0,
	[24] = 0,
	[25] = 0,
	[26] = 0,
	[27] = 0,
	[28] = 0,
	[29] = 0,
	[30] = 0,
	[31] = 0,
	[32] = 0,
	[33] = 0,
	[34] = 0,
	[35] = 0,
	[36] = 0,
	[37] = 0,
	[38] = 0,
	[39] = 0,
	[40] = 0,
	[41] = 0,
	[42] = 0,
	[43] = 0,
	[44] = 0,
	[45] = 0,
	[46] = 0,
	[47] = 0,
	[48] = 0,
	[49] = 0,
	[50] = 0,
	[51] = 0,
	[52] = 0,
	[53] = 0,
	[54] = 0,
	[55] = 0,
	[56] = 0,
	[57] = 0,
	[58] = 0,
	[59] = 0,
	[60] = 0,
	[61] = 0,
	[62] = 0,
	[63] = 0,
	[64] = 0,
	[65] = 0,
	[66] = 0,
	[67] = 0,
	[68] = 0,
	[69] = 0,
	[70] = 0,
	[71] = 0,
	[72] = 0,
	[73] = 0,
	[74] = 0,
	[75] = 0,
	[76] = 0,
	[77] = 0,
	[78] = 0,
	[79] = 0,
	[80] = 0,
	[81] = 0,
	[82] = 0,
	[83] = 0,
	[84] = 0,
	[85] = 0,
	[86] = 0,
	[87] = 0,
	[88] = 0,
	[89] = 0,
	[90] = 0,
	[91] = 0,
	[92] = 0,
	[93] = 0,
	[94] = 0,
	[95] = 0,
	[96] = 0,
	[97] = 0,
	[98] = 0,
	[99] = 0,
	[100] = 0,
	[101] = 0,
	[102] = 0,
	[103] = 0,
	[104] = 0,
	[105] = 0,
	[106] = 0,
	[107] = 0,
	[108] = 0,
	[109] = 0,
	[110] = 0,
	[111] = 0,
	[112] = 0,
	[113] = 0,
	[114] = 0,
	[115] = 0,
	[116] = 0,
	[117] = 0,
	[118] = 0,
	[119] = 0,
	[120] = 0,
	[121] = 0,
	[122] = 0,
	[123] = 0,
	[124] = 0,
	[125] = 0,
	[126] = 0,
	[127] = 0,
	/* svif.index */
	[128] = 1,
	/* o_eth.dmac */
	[129] = 2,
	[130] = 0,
	/* o_eth.smac */
	[131] = 3,
	[132] = 0,
	/* o_eth.type */
	[133] = 4,
	[134] = 0,
	/* o_ipv4.ver */
	[135] = 11,
	[136] = 0,
	/* o_ipv4.tos */
	[137] = 12,
	[138] = 0,
	/* o_ipv4.len */
	[139] = 13,
	[140] = 0,
	/* o_ipv4.frag_id */
	[141] = 14,
	[142] = 0,
	/* o_ipv4.frag_off */
	[143] = 15,
	[144] = 0,
	/* o_ipv4.ttl */
	[145] = 16,
	[146] = 0,
	/* o_ipv4.proto_id */
	[147] = 17,
	[148] = 0,
	/* o_ipv4.csum */
	[149] = 18,
	[150] = 0,
	/* o_ipv4.src_addr */
	[151] = 19,
	[152] = 0,
	/* o_ipv4.dst_addr */
	[153] = 20,
	[154] = 0,
	[155] = 0,
	[156] = 0,
	[157] = 0,
	[158] = 0,
	[159] = 0,
	[160] = 0,
	[161] = 0,
	[162] = 0,
	[163] = 0,
	[164] = 0,
	[165] = 0,
	[166] = 0,
	[167] = 0,
	[168] = 0,
	[169] = 0,
	[170] = 0,
	[171] = 0,
	[172] = 0,
	[173] = 0,
	[174] = 0,
	/* o_tcp.src_port */
	[175] = 21,
	[176] = 0,
	/* o_tcp.dst_port */
	[177] = 22,
	[178] = 0,
	/* o_tcp.sent_seq */
	[179] = 23,
	[180] = 0,
	/* o_tcp.recv_ack */
	[181] = 24,
	[182] = 0,
	/* o_tcp.data_off */
	[183] = 25,
	[184] = 0,
	/* o_tcp.tcp_flags */
	[185] = 26,
	[186] = 0,
	/* o_tcp.rx_win */
	[187] = 27,
	[188] = 0,
	/* o_tcp.csum */
	[189] = 28,
	[190] = 0,
	/* o_tcp.urp */
	[191] = 29,
	[192] = 0,
	[193] = 0,
	[194] = 0,
	[195] = 0,
	[196] = 0,
	[197] = 0,
	[198] = 0,
	[199] = 0,
	[200] = 0,
	/* oo_vlan.cfi_pri */
	[201] = 5,
	/* oi_vlan.cfi_pri */
	[202] = 8,
	[203] = 0,
	[204] = 0,
	/* oo_vlan.vid */
	[205] = 6,
	/* oi_vlan.vid */
	[206] = 9,
	[207] = 0,
	[208] = 0,
	/* oo_vlan.type */
	[209] = 7,
	/* oi_vlan.type */
	[210] = 10
};

/*
 * This structure has to be indexed based on the rte_flow_action_type that is
 * part of DPDK. The below array is list of parsing functions for each of the
 * flow actions that are supported.
 */
struct bnxt_ulp_rte_act_info ulp_act_info[] = {
	[RTE_FLOW_ACTION_TYPE_END] = {
	.act_type                = BNXT_ULP_ACT_TYPE_END,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VOID] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_void_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_PASSTHRU] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_JUMP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_jump_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_MARK] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_mark_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_FLAG] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_QUEUE] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DROP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_drop_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_COUNT] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_count_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_RSS] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_rss_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_PF] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_pf_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_VF] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_vf_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_PHY_PORT] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_phy_port_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_PORT_ID] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_port_id_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_METER] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SECURITY] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_MPLS_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_DEC_MPLS_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_NW_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_DEC_NW_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_COPY_TTL_OUT] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_COPY_TTL_IN] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_POP_VLAN] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_of_pop_vlan_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_of_push_vlan_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_of_set_vlan_vid_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_of_set_vlan_pcp_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_OF_POP_MPLS] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_MPLS] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_vxlan_encap_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_VXLAN_DECAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_vxlan_decap_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_NVGRE_DECAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_RAW_ENCAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_RAW_DECAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_set_ipv4_src_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV4_DST] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_set_ipv4_dst_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_IPV6_DST] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_TP_SRC] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_set_tp_src_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_SET_TP_DST] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_set_tp_dst_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_MAC_SWAP] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_dec_ttl_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_SET_TTL] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_MAC_SRC] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_SET_MAC_DST] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_INC_TCP_ACK] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	},
	[RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
	}
};

/* Specifies parameters for the generic tables */
struct bnxt_ulp_generic_tbl_params ulp_generic_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM << 1 |
		TF_DIR_RX] = {
	.result_num_entries	= 16384,
	.result_byte_size	= 6,
	.result_byte_order	= BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM << 1 |
		TF_DIR_TX] = {
	.result_num_entries	= 16384,
	.result_byte_size	= 6,
	.result_byte_order	= BNXT_ULP_BYTE_ORDER_LE

	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM << 1 |
		TF_DIR_RX] = {
	.result_num_entries	= 16384,
	.result_byte_size	= 6,
	.result_byte_order	= BNXT_ULP_BYTE_ORDER_LE
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM << 1 |
		TF_DIR_TX] = {
	.result_num_entries	= 16384,
	.result_byte_size	= 6,
	.result_byte_order	= BNXT_ULP_BYTE_ORDER_LE
	}
};

/* device tables */
const struct ulp_template_device_tbls ulp_template_stingray_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_stingray_class_tmpl_list,
	.tbl_list                = ulp_stingray_class_tbl_list,
	.key_field_list          = ulp_stingray_class_key_field_list,
	.result_field_list       = ulp_stingray_class_result_field_list,
	.ident_list              = ulp_stingray_class_ident_list
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_stingray_act_tmpl_list,
	.tbl_list                = ulp_stingray_act_tbl_list,
	.result_field_list       = ulp_stingray_act_result_field_list
	}
};

/* device tables */
const struct ulp_template_device_tbls ulp_template_wh_plus_tbls[] = {
	[BNXT_ULP_TEMPLATE_TYPE_CLASS] = {
	.tmpl_list               = ulp_wh_plus_class_tmpl_list,
	.tbl_list                = ulp_wh_plus_class_tbl_list,
	.key_field_list          = ulp_wh_plus_class_key_field_list,
	.result_field_list       = ulp_wh_plus_class_result_field_list,
	.ident_list              = ulp_wh_plus_class_ident_list
	},
	[BNXT_ULP_TEMPLATE_TYPE_ACTION] = {
	.tmpl_list               = ulp_wh_plus_act_tmpl_list,
	.tbl_list                = ulp_wh_plus_act_tbl_list,
	.result_field_list       = ulp_wh_plus_act_result_field_list
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
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[1] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_GLB_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	[2] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_FULL_ACT_RECORD,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_GLB_LB_AREC_PTR,
	.direction               = TF_DIR_TX
	},
	[3] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[4] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_L2_PROF_FUNC_ID,
	.direction               = TF_DIR_TX
	},
	[5] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_IDENTIFIER,
	.resource_type           = TF_IDENT_TYPE_PROF_FUNC,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID,
	.direction               = TF_DIR_RX
	},
	[6] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_RX
	},
	[7] = {
	.resource_func           = BNXT_ULP_RESOURCE_FUNC_INDEX_TABLE,
	.resource_type           = TF_TBL_TYPE_ACT_ENCAP_16B,
	.glb_regfile_index       = BNXT_ULP_GLB_REGFILE_INDEX_ENCAP_MAC_PTR,
	.direction               = TF_DIR_TX
	}
};

/*
 * This table has to be indexed based on the rte_flow_item_type that is part of
 * DPDK. The below array is list of parsing functions for each of the flow items
 * that are supported.
 */
struct bnxt_ulp_rte_hdr_info ulp_hdr_info[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_END,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VOID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_void_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_INVERT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ANY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PF] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_pf_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_VF] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vf_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_PHY_PORT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_phy_port_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_PORT_ID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_port_id_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_RAW] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_eth_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vlan_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_ipv4_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_ipv6_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_ICMP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_udp_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_tcp_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vxlan_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_E_TAG] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_NVGRE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_MPLS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GRE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_FUZZY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTPC] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTPU] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ESP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GENEVE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN_GPE] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_IPV6_EXT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_NS] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_NA] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_SLA_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_ICMP6_ND_OPT_TLA_ETH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_MARK] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_META] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GRE_KEY] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_GTP_PSC] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOES] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOED] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_NSH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_IGMP] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_AH] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},
	[RTE_FLOW_ITEM_TYPE_HIGIG2] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	}
};

/*
 * The parser uses this table to map vtags_num to CFA encapsulation VTAG
 * encoding. It then takes the result and stores it in act_prop[encap_vtag_type]
 */
uint32_t bnxt_ulp_encap_vtag_map[] = {
	BNXT_ULP_SYM_ECV_VTAG_TYPE_NOP,
	BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
	BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_2_ENCAP_PRI
};

/* Lists global action records */
uint32_t ulp_glb_template_tbl[] = {
	BNXT_ULP_DF_TPL_LOOPBACK_ACTION_REC
};

