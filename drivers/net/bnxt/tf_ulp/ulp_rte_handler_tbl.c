/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#include "ulp_template_db_enum.h"
#include "ulp_template_struct.h"
#include "ulp_rte_parser.h"

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
	.proto_act_func          = ulp_rte_port_act_handler
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
	},
	[RTE_FLOW_ACTION_TYPE_SAMPLE] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_sample_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_port_act_handler
	},
	[RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_rte_port_act_handler
	},
};

struct bnxt_ulp_rte_act_info ulp_vendor_act_info[] = {
	[BNXT_RTE_FLOW_ACTION_TYPE_END - BNXT_RTE_FLOW_ACTION_TYPE_END] = {
	.act_type                = BNXT_ULP_ACT_TYPE_END,
	.proto_act_func          = NULL
	},
	[BNXT_RTE_FLOW_ACTION_TYPE_VXLAN_DECAP - BNXT_RTE_FLOW_ACTION_TYPE_END] = {
	.act_type                = BNXT_ULP_ACT_TYPE_SUPPORTED,
	.proto_act_func          = ulp_vendor_vxlan_decap_act_handler
	},
	[BNXT_RTE_FLOW_ACTION_TYPE_LAST - BNXT_RTE_FLOW_ACTION_TYPE_END] = {
	.act_type                = BNXT_ULP_ACT_TYPE_NOT_SUPPORTED,
	.proto_act_func          = NULL
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
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_item_any_handler
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
	.proto_hdr_func          = ulp_rte_port_hdr_handler
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
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_icmp_hdr_handler
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
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_gre_hdr_handler
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
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_icmp6_hdr_handler
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
	},
	[RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_port_hdr_handler
	},
	[RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_port_hdr_handler
	}
};

struct bnxt_ulp_rte_hdr_info ulp_vendor_hdr_info[] = {
	[BNXT_RTE_FLOW_ITEM_TYPE_END - BNXT_RTE_FLOW_ITEM_TYPE_END] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_END,
	.proto_hdr_func          = NULL
	},
	[BNXT_RTE_FLOW_ITEM_TYPE_VXLAN_DECAP - BNXT_RTE_FLOW_ITEM_TYPE_END] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_SUPPORTED,
	.proto_hdr_func          = ulp_rte_vendor_vxlan_decap_hdr_handler
	},
	[BNXT_RTE_FLOW_ITEM_TYPE_LAST - BNXT_RTE_FLOW_ITEM_TYPE_END] = {
	.hdr_type                = BNXT_ULP_HDR_TYPE_NOT_SUPPORTED,
	.proto_hdr_func          = NULL
	},

};
