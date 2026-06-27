/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_FLOW_DEFINE_H__
#define __SXE2_FLOW_DEFINE_H__
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_flow_driver.h>

#include "sxe2_osal.h"
#include "sxe2_flow_public.h"

#define SXE2_FLOW_ETH_TYPE_MIN        (1500)

enum sxe2_expansion {
	SXE2_EXPANSION_ERROR = 0,
	SXE2_EXPANSION_OUTER_ETH,
	SXE2_EXPANSION_OUTER_VLAN,
	SXE2_EXPANSION_OUTER_QINQ,
	SXE2_EXPANSION_OUTER_IPV4,
	SXE2_EXPANSION_OUTER_IPV4_FRAG_EXT,
	SXE2_EXPANSION_OUTER_IPV6,
	SXE2_EXPANSION_OUTER_IPV6_FRAG_EXT,
	SXE2_EXPANSION_OUTER_UDP,
	SXE2_EXPANSION_OUTER_TCP,
	SXE2_EXPANSION_OUTER_SCTP,

	SXE2_EXPANSION_VXLAN,
	SXE2_EXPANSION_VXLAN_GPE,
	SXE2_EXPANSION_GRE,
	SXE2_EXPANSION_NVGRE,
	SXE2_EXPANSION_GENEVE,
	SXE2_EXPANSION_GTPU,
	SXE2_EXPANSION_IPIP,
	SXE2_EXPANSION_OUTER_END,

	SXE2_EXPANSION_ETH,
	SXE2_EXPANSION_VLAN,
	SXE2_EXPANSION_IPV4,
	SXE2_EXPANSION_IPV4_FRAG_EXT,
	SXE2_EXPANSION_IPV6,
	SXE2_EXPANSION_IPV6_FRAG_EXT,
	SXE2_EXPANSION_UDP,
	SXE2_EXPANSION_TCP,
	SXE2_EXPANSION_SCTP,

	SXE2_EXPANSION_END,
	SXE2_EXPANSION_MAX,
};

enum sxe2_flow_udp_tunnel_protocol {
	SXE2_FLOW_UDP_TUNNEL_PROTOCOL_VXLAN,
	SXE2_FLOW_UDP_TUNNEL_PROTOCOL_VXLAN_GPE,
	SXE2_FLOW_UDP_TUNNEL_PROTOCOL_GENEVE,
	SXE2_FLOW_UDP_TUNNEL_PROTOCOL_GTP_U,
	SXE2_FLOW_UDP_TUNNEL_PROTOCOL_NVGRE,
	SXE2_FLOW_UDP_TUNNEL_MAX,
};

enum {
	SXE2_FLOW_ETH_TYPE_IPV4 = 0x0800,
	SXE2_FLOW_ETH_TYPE_IPV6 = 0x86DD,
	SXE2_FLOW_IP_PROTOCOL_GRE = 0x2F,
	SXE2_FLOW_IP_PROTOCOL_IPV4 = 0x04,
	SXE2_FLOW_IP_PROTOCOL_IPV6 = 0x29,
	SXE2_FLOW_IP_PROTOCOL_ETH = 0x3B,
	SXE2_FLOW_IP_PROTOCOL_UDP = 0x11,
	SXE2_FLOW_IP_PROTOCOL_TCP = 0x06,
	SXE2_FLOW_IP_PROTOCOL_SCTP = 0x84,
};

union sxe2_flow_item_raw {
	struct sxe2_flow_item item;
	uint8_t raw[sizeof(struct sxe2_flow_item)];
};

struct sxe2_flow {
	TAILQ_ENTRY(sxe2_flow) next;
	enum sxe2_flow_engine_type engine_type;
	struct sxe2_flow_pattern pattern_outer;
	struct sxe2_flow_pattern pattern_inner;
	uint8_t has_mask;
	uint8_t has_spec;
	uint8_t has_hdr;
	struct sxe2_flow_meta meta;
	struct sxe2_flow_action action;
	uint32_t flow_id;
	int32_t create_err;
	DECLARE_BITMAP(flow_type, SXE2_EXPANSION_MAX);
};

TAILQ_HEAD(sxe2_flow_list_t, sxe2_flow);

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next;
	struct sxe2_flow_list_t sxe2_flow_list;
};
TAILQ_HEAD(rte_flow_list_t, rte_flow);

struct sxe2_fnav_cid_mgr {
	TAILQ_ENTRY(sxe2_fnav_cid_mgr) next;
	uint16_t stat_index;
	uint32_t user_id;
	uint32_t driver_id;
	uint32_t count_type;
	uint64_t hits;
	uint64_t bytes;
};
TAILQ_HEAD(sxe2_fnav_cid_mgr_list_t, sxe2_fnav_cid_mgr);

struct sxe2_fnav_count_resource {
	uint32_t count_type;
	uint32_t global_index;
	struct sxe2_fnav_cid_mgr_list_t fnav_cid_mgr_list;
};

struct sxe2_flow_context {
	struct rte_flow_list_t rte_flow_list;
	rte_spinlock_t flow_list_lock;
	struct sxe2_fnav_count_resource hw_res;
	uint32_t fnav_inited;
};
#define SXE2_INVALID_RSS_ATTR	\
			(RTE_ETH_RSS_L3_PRE40 | RTE_ETH_RSS_L3_PRE56 | RTE_ETH_RSS_L3_PRE96)
#define SXE2_VALID_RSS_IPV4_L4	 \
			(RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
			 RTE_ETH_RSS_NONFRAG_IPV4_SCTP)

#define SXE2_VALID_RSS_IPV6_L4	\
			(RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
			 RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
#define SXE2_VALID_RSS_IPV4	\
			(RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4  | \
			 RTE_ETH_RSS_NONFRAG_IPV4_OTHER | SXE2_VALID_RSS_IPV4_L4)
#define SXE2_VALID_RSS_IPV6	\
			(RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | \
			 RTE_ETH_RSS_NONFRAG_IPV6_OTHER | SXE2_VALID_RSS_IPV6_L4)

#define SXE2_VALID_RSS_L3	(SXE2_VALID_RSS_IPV4 | SXE2_VALID_RSS_IPV6)
#define SXE2_VALID_RSS_L4	(SXE2_VALID_RSS_IPV4_L4 | SXE2_VALID_RSS_IPV6_L4)

#endif /* __SXE2_FLOW_DEFINE_H__ */
