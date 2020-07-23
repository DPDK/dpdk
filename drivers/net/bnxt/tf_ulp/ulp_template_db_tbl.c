/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_db_field.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

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
	[BNXT_ULP_ACT_PROP_IDX_LAST] =
		BNXT_ULP_ACT_PROP_SZ_LAST
};

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
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
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

struct bnxt_ulp_cache_tbl_params ulp_cache_tbl_params[] = {
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM << 1 |
		TF_DIR_RX] = {
	.num_entries             = 16384
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_L2_CNTXT_TCAM << 1 |
		TF_DIR_TX] = {
	.num_entries             = 16384
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM << 1 |
		TF_DIR_RX] = {
	.num_entries             = 16384
	},
	[BNXT_ULP_RESOURCE_SUB_TYPE_CACHE_TYPE_PROFILE_TCAM << 1 |
		TF_DIR_TX] = {
	.num_entries             = 16384
	}
};

struct bnxt_ulp_device_params ulp_device_params[BNXT_ULP_DEVICE_ID_LAST] = {
	[BNXT_ULP_DEVICE_ID_WH_PLUS] = {
	.flow_mem_type           = BNXT_ULP_FLOW_MEM_TYPE_INT,
	.byte_order              = BNXT_ULP_BYTE_ORDER_LE,
	.encap_byte_swap         = 1,
	.flow_db_num_entries     = 16384,
	.mark_db_lfid_entries    = 65536,
	.mark_db_gfid_entries    = 0,
	.flow_count_db_entries   = 16384,
	.num_resources_per_flow  = 8,
	.num_phy_ports           = 2,
	.ext_cntr_table_type     = 0,
	.byte_count_mask         = 0x0000000fffffffff,
	.packet_count_mask       = 0xffffffff00000000,
	.byte_count_shift        = 0,
	.packet_count_shift      = 36
	}
};

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
	.glb_regfile_index = BNXT_ULP_GLB_REGFILE_INDEX_VXLAN_PROF_FUNC_ID,
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

uint32_t bnxt_ulp_encap_vtag_map[] = {
	BNXT_ULP_SYM_ECV_VTAG_TYPE_NOP,
	BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_1_ENCAP_PRI,
	BNXT_ULP_SYM_ECV_VTAG_TYPE_ADD_2_ENCAP_PRI
};

uint32_t ulp_glb_template_tbl[] = {
	BNXT_ULP_DF_TPL_LOOPBACK_ACTION_REC
};
