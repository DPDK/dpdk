/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_debug.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "ice_logs.h"
#include "base/ice_type.h"
#include "base/ice_flow.h"
#include "ice_ethdev.h"
#include "ice_generic_flow.h"

#define ICE_PHINT_NONE				0
#define ICE_PHINT_VLAN				BIT_ULL(0)
#define ICE_PHINT_PPPOE				BIT_ULL(1)
#define ICE_PHINT_GTPU				BIT_ULL(2)
#define ICE_PHINT_GTPU_EH			BIT_ULL(3)
#define	ICE_PHINT_GTPU_EH_DWN			BIT_ULL(4)
#define	ICE_PHINT_GTPU_EH_UP			BIT_ULL(5)
#define ICE_PHINT_RAW				BIT_ULL(6)

#define ICE_GTPU_EH_DWNLINK	0
#define ICE_GTPU_EH_UPLINK	1

#define ICE_IPV4_PROT		BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_PROT)
#define ICE_IPV6_PROT		BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PROT)

#define VALID_RSS_IPV4_L4	(RTE_ETH_RSS_NONFRAG_IPV4_UDP	| \
				 RTE_ETH_RSS_NONFRAG_IPV4_TCP	| \
				 RTE_ETH_RSS_NONFRAG_IPV4_SCTP)

#define VALID_RSS_IPV6_L4	(RTE_ETH_RSS_NONFRAG_IPV6_UDP	| \
				 RTE_ETH_RSS_NONFRAG_IPV6_TCP	| \
				 RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

#define VALID_RSS_IPV4		(RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 | \
				 VALID_RSS_IPV4_L4)
#define VALID_RSS_IPV6		(RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | \
				 VALID_RSS_IPV6_L4)
#define VALID_RSS_L3		(VALID_RSS_IPV4 | VALID_RSS_IPV6)
#define VALID_RSS_L4		(VALID_RSS_IPV4_L4 | VALID_RSS_IPV6_L4)

#define VALID_RSS_ATTR		(RTE_ETH_RSS_L3_SRC_ONLY	| \
				 RTE_ETH_RSS_L3_DST_ONLY	| \
				 RTE_ETH_RSS_L4_SRC_ONLY	| \
				 RTE_ETH_RSS_L4_DST_ONLY	| \
				 RTE_ETH_RSS_L2_SRC_ONLY	| \
				 RTE_ETH_RSS_L2_DST_ONLY	| \
				 RTE_ETH_RSS_L3_PRE32	| \
				 RTE_ETH_RSS_L3_PRE48	| \
				 RTE_ETH_RSS_L3_PRE64)

#define INVALID_RSS_ATTR	(RTE_ETH_RSS_L3_PRE40	| \
				 RTE_ETH_RSS_L3_PRE56	| \
				 RTE_ETH_RSS_L3_PRE96)

struct ice_rss_meta {
	uint8_t hash_function;
	struct ice_rss_hash_cfg cfg;
	struct ice_rss_raw_cfg raw;
};

struct ice_hash_flow_cfg {
	bool simple_xor;
	struct ice_rss_cfg rss_cfg;
};

static int
ice_hash_init(struct ice_adapter *ad);

static int
ice_hash_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error);

static int
ice_hash_destroy(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error);

static void
ice_hash_uninit(struct ice_adapter *ad);

static void
ice_hash_free(struct rte_flow *flow);

static int
ice_hash_parse_pattern_action(struct ice_adapter *ad,
			struct ice_pattern_match_item *array,
			uint32_t array_len,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			uint32_t priority,
			void **meta,
			struct rte_flow_error *error);

/* Rss configuration template */
struct ice_rss_hash_cfg ipv4_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV4 |
	ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_ETH | ICE_FLOW_HASH_IPV4,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv4_udp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV4 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_UDP,
	ICE_FLOW_HASH_ETH | ICE_HASH_UDP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv4_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV4 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_TCP,
	ICE_FLOW_HASH_ETH | ICE_HASH_TCP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv4_sctp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV4 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_SCTP,
	ICE_FLOW_HASH_ETH | ICE_HASH_SCTP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv6_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV6 |
	ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_ETH | ICE_FLOW_HASH_IPV6,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv6_frag_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV6 |
	ICE_FLOW_SEG_HDR_IPV_FRAG,
	ICE_FLOW_HASH_ETH | ICE_FLOW_HASH_IPV6,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv6_udp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV6 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_UDP,
	ICE_FLOW_HASH_ETH | ICE_HASH_UDP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv6_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV6 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_TCP,
	ICE_FLOW_HASH_ETH | ICE_HASH_TCP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg ipv6_sctp_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_IPV6 |
	ICE_FLOW_SEG_HDR_IPV_OTHER | ICE_FLOW_SEG_HDR_SCTP,
	ICE_FLOW_HASH_ETH | ICE_HASH_SCTP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg outer_ipv4_inner_ipv4_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_IPV4,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};
struct ice_rss_hash_cfg outer_ipv4_inner_ipv4_udp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_UDP,
	ICE_HASH_UDP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};

struct ice_rss_hash_cfg outer_ipv4_inner_ipv4_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_TCP,
	ICE_HASH_TCP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};

struct ice_rss_hash_cfg outer_ipv6_inner_ipv4_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_IPV4,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};

struct ice_rss_hash_cfg outer_ipv6_inner_ipv4_udp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_UDP,
	ICE_HASH_UDP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};

struct ice_rss_hash_cfg outer_ipv6_inner_ipv4_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_TCP,
	ICE_HASH_TCP_IPV4 | ICE_IPV4_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};

struct ice_rss_hash_cfg outer_ipv4_inner_ipv6_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_IPV6,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};

struct ice_rss_hash_cfg outer_ipv4_inner_ipv6_udp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_UDP,
	ICE_HASH_UDP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};

struct ice_rss_hash_cfg outer_ipv4_inner_ipv6_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_TCP,
	ICE_HASH_TCP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV4,
	0
};

struct ice_rss_hash_cfg outer_ipv6_inner_ipv6_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER,
	ICE_FLOW_HASH_IPV6,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};
struct ice_rss_hash_cfg outer_ipv6_inner_ipv6_udp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_UDP,
	ICE_HASH_UDP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};

struct ice_rss_hash_cfg outer_ipv6_inner_ipv6_tcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_TCP,
	ICE_HASH_TCP_IPV6 | ICE_IPV6_PROT,
	ICE_RSS_INNER_HEADERS_W_OUTER_IPV6,
	0
};

struct ice_rss_hash_cfg eth_ipv4_esp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_ESP,
	ICE_FLOW_HASH_ESP_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv4_udp_esp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_NAT_T_ESP,
	ICE_FLOW_HASH_NAT_T_ESP_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv4_ah_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_AH,
	ICE_FLOW_HASH_AH_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv4_l2tpv3_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_L2TPV3,
	ICE_FLOW_HASH_L2TPV3_SESS_ID,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv4_pfcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV4 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_PFCP_SESSION,
	ICE_FLOW_HASH_PFCP_SEID,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv6_esp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_ESP,
	ICE_FLOW_HASH_ESP_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv6_udp_esp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_NAT_T_ESP,
	ICE_FLOW_HASH_NAT_T_ESP_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv6_ah_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_AH,
	ICE_FLOW_HASH_AH_SPI,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv6_l2tpv3_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_L2TPV3,
	ICE_FLOW_HASH_L2TPV3_SESS_ID,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_ipv6_pfcp_tmplt = {
	ICE_FLOW_SEG_HDR_IPV6 | ICE_FLOW_SEG_HDR_IPV_OTHER |
	ICE_FLOW_SEG_HDR_PFCP_SESSION,
	ICE_FLOW_HASH_PFCP_SEID,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg pppoe_tmplt = {
	ICE_FLOW_SEG_HDR_ETH,
	ICE_FLOW_HASH_ETH | ICE_FLOW_HASH_PPPOE_SESS_ID,
	ICE_RSS_OUTER_HEADERS,
	0
};

struct ice_rss_hash_cfg empty_tmplt = {
	ICE_FLOW_SEG_HDR_NONE,
	0,
	ICE_RSS_ANY_HEADERS,
	0
};

struct ice_rss_hash_cfg eth_tmplt = {
	ICE_FLOW_SEG_HDR_ETH | ICE_FLOW_SEG_HDR_ETH_NON_IP,
	ICE_FLOW_HASH_ETH,
	ICE_RSS_OUTER_HEADERS,
	0
};

/* IPv4 */
#define ICE_RSS_TYPE_ETH_IPV4		(RTE_ETH_RSS_ETH | RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_FRAG_IPV4 | \
					 RTE_ETH_RSS_IPV4_CHKSUM)
#define ICE_RSS_TYPE_ETH_IPV4_UDP	(ICE_RSS_TYPE_ETH_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_ETH_IPV4_TCP	(ICE_RSS_TYPE_ETH_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_ETH_IPV4_SCTP	(ICE_RSS_TYPE_ETH_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_IPV4		RTE_ETH_RSS_IPV4
#define ICE_RSS_TYPE_IPV4_UDP		(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_UDP)
#define ICE_RSS_TYPE_IPV4_TCP		(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_TCP)
#define ICE_RSS_TYPE_IPV4_SCTP		(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_SCTP)

/* IPv6 */
#define ICE_RSS_TYPE_ETH_IPV6		(RTE_ETH_RSS_ETH | RTE_ETH_RSS_IPV6)
#define ICE_RSS_TYPE_ETH_IPV6_FRAG	(RTE_ETH_RSS_ETH | RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_FRAG_IPV6)
#define ICE_RSS_TYPE_ETH_IPV6_UDP	(ICE_RSS_TYPE_ETH_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_ETH_IPV6_TCP	(ICE_RSS_TYPE_ETH_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_ETH_IPV6_SCTP	(ICE_RSS_TYPE_ETH_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define ICE_RSS_TYPE_IPV6		RTE_ETH_RSS_IPV6
#define ICE_RSS_TYPE_IPV6_UDP		(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_UDP)
#define ICE_RSS_TYPE_IPV6_TCP		(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_TCP)
#define ICE_RSS_TYPE_IPV6_SCTP		(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

/* VLAN IPV4 */
#define ICE_RSS_TYPE_VLAN_IPV4		(ICE_RSS_TYPE_IPV4 | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN | \
					 RTE_ETH_RSS_FRAG_IPV4)
#define ICE_RSS_TYPE_VLAN_IPV4_UDP	(ICE_RSS_TYPE_IPV4_UDP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define ICE_RSS_TYPE_VLAN_IPV4_TCP	(ICE_RSS_TYPE_IPV4_TCP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define ICE_RSS_TYPE_VLAN_IPV4_SCTP	(ICE_RSS_TYPE_IPV4_SCTP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
/* VLAN IPv6 */
#define ICE_RSS_TYPE_VLAN_IPV6		(ICE_RSS_TYPE_IPV6 | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define ICE_RSS_TYPE_VLAN_IPV6_FRAG	(ICE_RSS_TYPE_IPV6 | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN | \
					 RTE_ETH_RSS_FRAG_IPV6)
#define ICE_RSS_TYPE_VLAN_IPV6_UDP	(ICE_RSS_TYPE_IPV6_UDP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define ICE_RSS_TYPE_VLAN_IPV6_TCP	(ICE_RSS_TYPE_IPV6_TCP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define ICE_RSS_TYPE_VLAN_IPV6_SCTP	(ICE_RSS_TYPE_IPV6_SCTP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)

/* GTPU IPv4 */
#define ICE_RSS_TYPE_GTPU_IPV4		(ICE_RSS_TYPE_IPV4 | \
					 RTE_ETH_RSS_GTPU)
#define ICE_RSS_TYPE_GTPU_IPV4_UDP	(ICE_RSS_TYPE_IPV4_UDP | \
					 RTE_ETH_RSS_GTPU)
#define ICE_RSS_TYPE_GTPU_IPV4_TCP	(ICE_RSS_TYPE_IPV4_TCP | \
					 RTE_ETH_RSS_GTPU)
/* GTPU IPv6 */
#define ICE_RSS_TYPE_GTPU_IPV6		(ICE_RSS_TYPE_IPV6 | \
					 RTE_ETH_RSS_GTPU)
#define ICE_RSS_TYPE_GTPU_IPV6_UDP	(ICE_RSS_TYPE_IPV6_UDP | \
					 RTE_ETH_RSS_GTPU)
#define ICE_RSS_TYPE_GTPU_IPV6_TCP	(ICE_RSS_TYPE_IPV6_TCP | \
					 RTE_ETH_RSS_GTPU)

/* PPPOE */
#define ICE_RSS_TYPE_PPPOE		(RTE_ETH_RSS_ETH | RTE_ETH_RSS_PPPOE)

/* PPPOE IPv4 */
#define ICE_RSS_TYPE_PPPOE_IPV4		(ICE_RSS_TYPE_IPV4 | \
					 ICE_RSS_TYPE_PPPOE)
#define ICE_RSS_TYPE_PPPOE_IPV4_UDP	(ICE_RSS_TYPE_IPV4_UDP | \
					 ICE_RSS_TYPE_PPPOE)
#define ICE_RSS_TYPE_PPPOE_IPV4_TCP	(ICE_RSS_TYPE_IPV4_TCP | \
					 ICE_RSS_TYPE_PPPOE)

/* PPPOE IPv6 */
#define ICE_RSS_TYPE_PPPOE_IPV6		(ICE_RSS_TYPE_IPV6 | \
					 ICE_RSS_TYPE_PPPOE)
#define ICE_RSS_TYPE_PPPOE_IPV6_UDP	(ICE_RSS_TYPE_IPV6_UDP | \
					 ICE_RSS_TYPE_PPPOE)
#define ICE_RSS_TYPE_PPPOE_IPV6_TCP	(ICE_RSS_TYPE_IPV6_TCP | \
					 ICE_RSS_TYPE_PPPOE)

/* ESP, AH, L2TPV3 and PFCP */
#define ICE_RSS_TYPE_IPV4_ESP		(RTE_ETH_RSS_ESP | RTE_ETH_RSS_IPV4)
#define ICE_RSS_TYPE_IPV6_ESP		(RTE_ETH_RSS_ESP | RTE_ETH_RSS_IPV6)
#define ICE_RSS_TYPE_IPV4_AH		(RTE_ETH_RSS_AH | RTE_ETH_RSS_IPV4)
#define ICE_RSS_TYPE_IPV6_AH		(RTE_ETH_RSS_AH | RTE_ETH_RSS_IPV6)
#define ICE_RSS_TYPE_IPV4_L2TPV3	(RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_IPV4)
#define ICE_RSS_TYPE_IPV6_L2TPV3	(RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_IPV6)
#define ICE_RSS_TYPE_IPV4_PFCP		(RTE_ETH_RSS_PFCP | RTE_ETH_RSS_IPV4)
#define ICE_RSS_TYPE_IPV6_PFCP		(RTE_ETH_RSS_PFCP | RTE_ETH_RSS_IPV6)

/* MAC */
#define ICE_RSS_TYPE_ETH		RTE_ETH_RSS_ETH

/**
 * Supported pattern for hash.
 * The first member is pattern item type,
 * the second member is input set mask,
 * the third member is ice_rss_hash_cfg template.
 */
static struct ice_pattern_match_item ice_hash_pattern_list[] = {
	/* IPV4 */
	{pattern_raw,				ICE_INSET_NONE,				ICE_INSET_NONE,	NULL},
	{pattern_eth_ipv4,			ICE_RSS_TYPE_ETH_IPV4,		ICE_INSET_NONE,	&ipv4_tmplt},
	{pattern_eth_ipv4_udp,			ICE_RSS_TYPE_ETH_IPV4_UDP,	ICE_INSET_NONE,	&ipv4_udp_tmplt},
	{pattern_eth_ipv4_tcp,			ICE_RSS_TYPE_ETH_IPV4_TCP,	ICE_INSET_NONE,	&ipv4_tcp_tmplt},
	{pattern_eth_ipv4_sctp,			ICE_RSS_TYPE_ETH_IPV4_SCTP,	ICE_INSET_NONE,	&ipv4_sctp_tmplt},
	{pattern_eth_vlan_ipv4,			ICE_RSS_TYPE_VLAN_IPV4,		ICE_INSET_NONE,	&ipv4_tmplt},
	{pattern_eth_vlan_ipv4_udp,		ICE_RSS_TYPE_VLAN_IPV4_UDP,	ICE_INSET_NONE,	&ipv4_udp_tmplt},
	{pattern_eth_vlan_ipv4_tcp,		ICE_RSS_TYPE_VLAN_IPV4_TCP,	ICE_INSET_NONE,	&ipv4_tcp_tmplt},
	{pattern_eth_vlan_ipv4_sctp,		ICE_RSS_TYPE_VLAN_IPV4_SCTP,	ICE_INSET_NONE,	&ipv4_sctp_tmplt},
	{pattern_eth_ipv4_gtpu_ipv4,		ICE_RSS_TYPE_GTPU_IPV4,		ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_tmplt},
	{pattern_eth_ipv4_gtpu_ipv4_udp,	ICE_RSS_TYPE_GTPU_IPV4_UDP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_udp_tmplt},
	{pattern_eth_ipv4_gtpu_ipv4_tcp,	ICE_RSS_TYPE_GTPU_IPV4_TCP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_tcp_tmplt},
	{pattern_eth_ipv6_gtpu_ipv4,		ICE_RSS_TYPE_GTPU_IPV4,		ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_tmplt},
	{pattern_eth_ipv6_gtpu_ipv4_udp,	ICE_RSS_TYPE_GTPU_IPV4_UDP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_udp_tmplt},
	{pattern_eth_ipv6_gtpu_ipv4_tcp,	ICE_RSS_TYPE_GTPU_IPV4_TCP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_tcp_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv4,		ICE_RSS_TYPE_GTPU_IPV4,		ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv4_udp,	ICE_RSS_TYPE_GTPU_IPV4_UDP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_udp_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv4_tcp,	ICE_RSS_TYPE_GTPU_IPV4_TCP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv4_tcp_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv4,		ICE_RSS_TYPE_GTPU_IPV4,		ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv4_udp,	ICE_RSS_TYPE_GTPU_IPV4_UDP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_udp_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv4_tcp,	ICE_RSS_TYPE_GTPU_IPV4_TCP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv4_tcp_tmplt},
	{pattern_eth_pppoes_ipv4,		ICE_RSS_TYPE_PPPOE_IPV4,	ICE_INSET_NONE,	&ipv4_tmplt},
	{pattern_eth_pppoes_ipv4_udp,		ICE_RSS_TYPE_PPPOE_IPV4_UDP,	ICE_INSET_NONE,	&ipv4_udp_tmplt},
	{pattern_eth_pppoes_ipv4_tcp,		ICE_RSS_TYPE_PPPOE_IPV4_TCP,	ICE_INSET_NONE,	&ipv4_tcp_tmplt},
	{pattern_eth_ipv4_esp,			ICE_RSS_TYPE_IPV4_ESP,		ICE_INSET_NONE,	&eth_ipv4_esp_tmplt},
	{pattern_eth_ipv4_udp_esp,		ICE_RSS_TYPE_IPV4_ESP,		ICE_INSET_NONE,	&eth_ipv4_udp_esp_tmplt},
	{pattern_eth_ipv4_ah,			ICE_RSS_TYPE_IPV4_AH,		ICE_INSET_NONE,	&eth_ipv4_ah_tmplt},
	{pattern_eth_ipv4_l2tp,			ICE_RSS_TYPE_IPV4_L2TPV3,	ICE_INSET_NONE,	&eth_ipv4_l2tpv3_tmplt},
	{pattern_eth_ipv4_pfcp,			ICE_RSS_TYPE_IPV4_PFCP,		ICE_INSET_NONE,	&eth_ipv4_pfcp_tmplt},
	/* IPV6 */
	{pattern_eth_ipv6,			ICE_RSS_TYPE_ETH_IPV6,		ICE_INSET_NONE,	&ipv6_tmplt},
	{pattern_eth_ipv6_frag_ext,		ICE_RSS_TYPE_ETH_IPV6_FRAG,	ICE_INSET_NONE,	&ipv6_frag_tmplt},
	{pattern_eth_ipv6_udp,			ICE_RSS_TYPE_ETH_IPV6_UDP,	ICE_INSET_NONE,	&ipv6_udp_tmplt},
	{pattern_eth_ipv6_tcp,			ICE_RSS_TYPE_ETH_IPV6_TCP,	ICE_INSET_NONE,	&ipv6_tcp_tmplt},
	{pattern_eth_ipv6_sctp,			ICE_RSS_TYPE_ETH_IPV6_SCTP,	ICE_INSET_NONE,	&ipv6_sctp_tmplt},
	{pattern_eth_vlan_ipv6,			ICE_RSS_TYPE_VLAN_IPV6,		ICE_INSET_NONE,	&ipv6_tmplt},
	{pattern_eth_vlan_ipv6_frag_ext,	ICE_RSS_TYPE_VLAN_IPV6_FRAG,	ICE_INSET_NONE, &ipv6_frag_tmplt},
	{pattern_eth_vlan_ipv6_udp,		ICE_RSS_TYPE_VLAN_IPV6_UDP,	ICE_INSET_NONE,	&ipv6_udp_tmplt},
	{pattern_eth_vlan_ipv6_tcp,		ICE_RSS_TYPE_VLAN_IPV6_TCP,	ICE_INSET_NONE,	&ipv6_tcp_tmplt},
	{pattern_eth_vlan_ipv6_sctp,		ICE_RSS_TYPE_VLAN_IPV6_SCTP,	ICE_INSET_NONE,	&ipv6_sctp_tmplt},
	{pattern_eth_ipv4_gtpu_ipv6,		ICE_RSS_TYPE_GTPU_IPV6,		ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_tmplt},
	{pattern_eth_ipv4_gtpu_ipv6_udp,	ICE_RSS_TYPE_GTPU_IPV6_UDP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_udp_tmplt},
	{pattern_eth_ipv4_gtpu_ipv6_tcp,	ICE_RSS_TYPE_GTPU_IPV6_TCP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_tcp_tmplt},
	{pattern_eth_ipv6_gtpu_ipv6,		ICE_RSS_TYPE_GTPU_IPV6,		ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_tmplt},
	{pattern_eth_ipv6_gtpu_ipv6_udp,	ICE_RSS_TYPE_GTPU_IPV6_UDP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_udp_tmplt},
	{pattern_eth_ipv6_gtpu_ipv6_tcp,	ICE_RSS_TYPE_GTPU_IPV6_TCP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_tcp_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv6,		ICE_RSS_TYPE_GTPU_IPV6,		ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv6_udp,	ICE_RSS_TYPE_GTPU_IPV6_UDP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_udp_tmplt},
	{pattern_eth_ipv4_gtpu_eh_ipv6_tcp,	ICE_RSS_TYPE_GTPU_IPV6_TCP,	ICE_INSET_NONE,	&outer_ipv4_inner_ipv6_tcp_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv6,		ICE_RSS_TYPE_GTPU_IPV6,		ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv6_udp,	ICE_RSS_TYPE_GTPU_IPV6_UDP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_udp_tmplt},
	{pattern_eth_ipv6_gtpu_eh_ipv6_tcp,	ICE_RSS_TYPE_GTPU_IPV6_TCP,	ICE_INSET_NONE,	&outer_ipv6_inner_ipv6_tcp_tmplt},
	{pattern_eth_pppoes_ipv6,		ICE_RSS_TYPE_PPPOE_IPV6,	ICE_INSET_NONE,	&ipv6_tmplt},
	{pattern_eth_pppoes_ipv6_udp,		ICE_RSS_TYPE_PPPOE_IPV6_UDP,	ICE_INSET_NONE,	&ipv6_udp_tmplt},
	{pattern_eth_pppoes_ipv6_tcp,		ICE_RSS_TYPE_PPPOE_IPV6_TCP,	ICE_INSET_NONE,	&ipv6_tcp_tmplt},
	{pattern_eth_ipv6_esp,			ICE_RSS_TYPE_IPV6_ESP,		ICE_INSET_NONE,	&eth_ipv6_esp_tmplt},
	{pattern_eth_ipv6_udp_esp,		ICE_RSS_TYPE_IPV6_ESP,		ICE_INSET_NONE,	&eth_ipv6_udp_esp_tmplt},
	{pattern_eth_ipv6_ah,			ICE_RSS_TYPE_IPV6_AH,		ICE_INSET_NONE,	&eth_ipv6_ah_tmplt},
	{pattern_eth_ipv6_l2tp,			ICE_RSS_TYPE_IPV6_L2TPV3,	ICE_INSET_NONE,	&eth_ipv6_l2tpv3_tmplt},
	{pattern_eth_ipv6_pfcp,			ICE_RSS_TYPE_IPV6_PFCP,		ICE_INSET_NONE,	&eth_ipv6_pfcp_tmplt},
	/* PPPOE */
	{pattern_eth_pppoes,			ICE_RSS_TYPE_PPPOE,		ICE_INSET_NONE,	&pppoe_tmplt},
	/* MAC */
	{pattern_ethertype,			ICE_RSS_TYPE_ETH,		ICE_INSET_NONE, &eth_tmplt},
	/* EMPTY */
	{pattern_empty,				ICE_INSET_NONE,			ICE_INSET_NONE,	&empty_tmplt},
};

static struct ice_flow_engine ice_hash_engine = {
	.init = ice_hash_init,
	.create = ice_hash_create,
	.destroy = ice_hash_destroy,
	.uninit = ice_hash_uninit,
	.free = ice_hash_free,
	.type = ICE_FLOW_ENGINE_HASH,
};

/* Register parser for os package. */
static struct ice_flow_parser ice_hash_parser = {
	.engine = &ice_hash_engine,
	.array = ice_hash_pattern_list,
	.array_len = RTE_DIM(ice_hash_pattern_list),
	.parse_pattern_action = ice_hash_parse_pattern_action,
	.stage = ICE_FLOW_STAGE_RSS,
};

RTE_INIT(ice_hash_engine_init)
{
	struct ice_flow_engine *engine = &ice_hash_engine;
	ice_register_flow_engine(engine);
}

static int
ice_hash_init(struct ice_adapter *ad)
{
	struct ice_flow_parser *parser = NULL;

	if (ad->hw.dcf_enabled)
		return 0;

	parser = &ice_hash_parser;

	return ice_register_parser(parser, ad);
}

static int
ice_hash_parse_pattern(const struct rte_flow_item pattern[], uint64_t *phint,
		       struct rte_flow_error *error)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_gtp_psc *psc;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not support range");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_RAW:
			*phint |= ICE_PHINT_RAW;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			*phint |= ICE_PHINT_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_PPPOES:
			*phint |= ICE_PHINT_PPPOE;
			break;
		case RTE_FLOW_ITEM_TYPE_GTPU:
			*phint |= ICE_PHINT_GTPU;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			*phint |= ICE_PHINT_GTPU_EH;
			psc = item->spec;
			if (!psc)
				break;
			else if (psc->hdr.type == ICE_GTPU_EH_UPLINK)
				*phint |= ICE_PHINT_GTPU_EH_UP;
			else if (psc->hdr.type == ICE_GTPU_EH_DWNLINK)
				*phint |= ICE_PHINT_GTPU_EH_DWN;
			break;
		default:
			break;
		}
	}

	return 0;
}

static int
ice_hash_parse_raw_pattern(struct ice_adapter *ad,
				const struct rte_flow_item *item,
				struct ice_rss_meta *meta)
{
	const struct rte_flow_item_raw *raw_spec, *raw_mask;
	struct ice_parser_profile prof;
	struct ice_parser_result rslt;
	uint16_t spec_len, pkt_len;
	uint8_t *pkt_buf, *msk_buf;
	uint8_t tmp_val = 0;
	uint8_t tmp_c = 0;
	int i, j;

	if (ad->psr == NULL)
		return -rte_errno;

	raw_spec = item->spec;
	raw_mask = item->mask;

	spec_len = strlen((char *)(uintptr_t)raw_spec->pattern);
	if (strlen((char *)(uintptr_t)raw_mask->pattern) !=
		spec_len)
		return -rte_errno;

	pkt_len = spec_len / 2;

	pkt_buf = rte_zmalloc(NULL, pkt_len, 0);
	if (!pkt_buf)
		return -ENOMEM;

	msk_buf = rte_zmalloc(NULL, pkt_len, 0);
	if (!msk_buf)
		return -ENOMEM;

	/* convert string to int array */
	for (i = 0, j = 0; i < spec_len; i += 2, j++) {
		tmp_c = raw_spec->pattern[i];
		if (tmp_c >= 'a' && tmp_c <= 'f')
			tmp_val = tmp_c - 'a' + 10;
		if (tmp_c >= 'A' && tmp_c <= 'F')
			tmp_val = tmp_c - 'A' + 10;
		if (tmp_c >= '0' && tmp_c <= '9')
			tmp_val = tmp_c - '0';

		tmp_c = raw_spec->pattern[i + 1];
		if (tmp_c >= 'a' && tmp_c <= 'f')
			pkt_buf[j] = tmp_val * 16 + tmp_c - 'a' + 10;
		if (tmp_c >= 'A' && tmp_c <= 'F')
			pkt_buf[j] = tmp_val * 16 + tmp_c - 'A' + 10;
		if (tmp_c >= '0' && tmp_c <= '9')
			pkt_buf[j] = tmp_val * 16 + tmp_c - '0';

		tmp_c = raw_mask->pattern[i];
		if (tmp_c >= 'a' && tmp_c <= 'f')
			tmp_val = tmp_c - 0x57;
		if (tmp_c >= 'A' && tmp_c <= 'F')
			tmp_val = tmp_c - 0x37;
		if (tmp_c >= '0' && tmp_c <= '9')
			tmp_val = tmp_c - '0';

		tmp_c = raw_mask->pattern[i + 1];
		if (tmp_c >= 'a' && tmp_c <= 'f')
			msk_buf[j] = tmp_val * 16 + tmp_c - 'a' + 10;
		if (tmp_c >= 'A' && tmp_c <= 'F')
			msk_buf[j] = tmp_val * 16 + tmp_c - 'A' + 10;
		if (tmp_c >= '0' && tmp_c <= '9')
			msk_buf[j] = tmp_val * 16 + tmp_c - '0';
	}

	if (ice_parser_run(ad->psr, pkt_buf, pkt_len, &rslt))
		return -rte_errno;

	if (ice_parser_profile_init(&rslt, pkt_buf, msk_buf,
		pkt_len, ICE_BLK_RSS, true, &prof))
		return -rte_errno;

	rte_memcpy(&meta->raw.prof, &prof, sizeof(prof));

	rte_free(pkt_buf);
	rte_free(msk_buf);
	return 0;
}

static void
ice_refine_hash_cfg_l234(struct ice_rss_hash_cfg *hash_cfg,
			 uint64_t rss_type)
{
	uint32_t *addl_hdrs = &hash_cfg->addl_hdrs;
	uint64_t *hash_flds = &hash_cfg->hash_flds;

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_ETH) {
		if (!(rss_type & RTE_ETH_RSS_ETH))
			*hash_flds &= ~ICE_FLOW_HASH_ETH;
		if (rss_type & RTE_ETH_RSS_L2_SRC_ONLY)
			*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_ETH_DA));
		else if (rss_type & RTE_ETH_RSS_L2_DST_ONLY)
			*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_ETH_SA));
		*addl_hdrs &= ~ICE_FLOW_SEG_HDR_ETH;
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_ETH_NON_IP) {
		if (rss_type & RTE_ETH_RSS_ETH)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_ETH_TYPE);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_VLAN) {
		if (rss_type & RTE_ETH_RSS_C_VLAN)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_C_VLAN);
		else if (rss_type & RTE_ETH_RSS_S_VLAN)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_S_VLAN);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_PPPOE) {
		if (!(rss_type & RTE_ETH_RSS_PPPOE))
			*hash_flds &= ~ICE_FLOW_HASH_PPPOE_SESS_ID;
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_IPV4) {
		if (rss_type &
		   (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
		    RTE_ETH_RSS_NONFRAG_IPV4_UDP |
		    RTE_ETH_RSS_NONFRAG_IPV4_TCP |
		    RTE_ETH_RSS_NONFRAG_IPV4_SCTP)) {
			if (rss_type & RTE_ETH_RSS_FRAG_IPV4) {
				*addl_hdrs |= ICE_FLOW_SEG_HDR_IPV_FRAG;
				*addl_hdrs &= ~(ICE_FLOW_SEG_HDR_IPV_OTHER);
				*hash_flds |=
					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_ID);
			}
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_DA));
			else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_SA));
			else if (rss_type &
				(RTE_ETH_RSS_L4_SRC_ONLY |
				RTE_ETH_RSS_L4_DST_ONLY))
				*hash_flds &= ~ICE_FLOW_HASH_IPV4;
		} else {
			*hash_flds &= ~ICE_FLOW_HASH_IPV4;
		}

		if (rss_type & RTE_ETH_RSS_IPV4_CHKSUM)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_IPV4_CHKSUM);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_IPV6) {
		if (rss_type &
		   (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
		    RTE_ETH_RSS_NONFRAG_IPV6_UDP |
		    RTE_ETH_RSS_NONFRAG_IPV6_TCP |
		    RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
			if (rss_type & RTE_ETH_RSS_FRAG_IPV6)
				*hash_flds |=
					BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_ID);
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA));
			else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA));
			else if (rss_type &
				(RTE_ETH_RSS_L4_SRC_ONLY |
				RTE_ETH_RSS_L4_DST_ONLY))
				*hash_flds &= ~ICE_FLOW_HASH_IPV6;
		} else {
			*hash_flds &= ~ICE_FLOW_HASH_IPV6;
		}

		if (rss_type & RTE_ETH_RSS_L3_PRE32) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE32_SA));
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE32_DA));
			} else {
				*hash_flds &= ~ICE_FLOW_HASH_IPV6;
				*hash_flds |= ICE_FLOW_HASH_IPV6_PRE32;
			}
		}
		if (rss_type & RTE_ETH_RSS_L3_PRE48) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE48_SA));
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE48_DA));
			} else {
				*hash_flds &= ~ICE_FLOW_HASH_IPV6;
				*hash_flds |= ICE_FLOW_HASH_IPV6_PRE48;
			}
		}
		if (rss_type & RTE_ETH_RSS_L3_PRE64) {
			if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_SA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE64_SA));
			} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_DA));
				*hash_flds |= (BIT_ULL(ICE_FLOW_FIELD_IDX_IPV6_PRE64_DA));
			} else {
				*hash_flds &= ~ICE_FLOW_HASH_IPV6;
				*hash_flds |= ICE_FLOW_HASH_IPV6_PRE64;
			}
		}
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_UDP) {
		if (rss_type &
		   (RTE_ETH_RSS_NONFRAG_IPV4_UDP |
		    RTE_ETH_RSS_NONFRAG_IPV6_UDP)) {
			if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_DST_PORT));
			else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_SRC_PORT));
			else if (rss_type &
				(RTE_ETH_RSS_L3_SRC_ONLY |
				  RTE_ETH_RSS_L3_DST_ONLY))
				*hash_flds &= ~ICE_FLOW_HASH_UDP_PORT;
		} else {
			*hash_flds &= ~ICE_FLOW_HASH_UDP_PORT;
		}

		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_UDP_CHKSUM);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_TCP) {
		if (rss_type &
		   (RTE_ETH_RSS_NONFRAG_IPV4_TCP |
		    RTE_ETH_RSS_NONFRAG_IPV6_TCP)) {
			if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_DST_PORT));
			else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_SRC_PORT));
			else if (rss_type &
				(RTE_ETH_RSS_L3_SRC_ONLY |
				  RTE_ETH_RSS_L3_DST_ONLY))
				*hash_flds &= ~ICE_FLOW_HASH_TCP_PORT;
		} else {
			*hash_flds &= ~ICE_FLOW_HASH_TCP_PORT;
		}

		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_TCP_CHKSUM);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_SCTP) {
		if (rss_type &
		   (RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
		    RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
			if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_DST_PORT));
			else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
				*hash_flds &= ~(BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT));
			else if (rss_type &
				(RTE_ETH_RSS_L3_SRC_ONLY |
				  RTE_ETH_RSS_L3_DST_ONLY))
				*hash_flds &= ~ICE_FLOW_HASH_SCTP_PORT;
		} else {
			*hash_flds &= ~ICE_FLOW_HASH_SCTP_PORT;
		}

		if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
			*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_SCTP_CHKSUM);
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_L2TPV3) {
		if (!(rss_type & RTE_ETH_RSS_L2TPV3))
			*hash_flds &= ~ICE_FLOW_HASH_L2TPV3_SESS_ID;
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_ESP) {
		if (!(rss_type & RTE_ETH_RSS_ESP))
			*hash_flds &= ~ICE_FLOW_HASH_ESP_SPI;
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_AH) {
		if (!(rss_type & RTE_ETH_RSS_AH))
			*hash_flds &= ~ICE_FLOW_HASH_AH_SPI;
	}

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_PFCP_SESSION) {
		if (!(rss_type & RTE_ETH_RSS_PFCP))
			*hash_flds &= ~ICE_FLOW_HASH_PFCP_SEID;
	}
}

static void
ice_refine_proto_hdrs_by_pattern(struct ice_rss_hash_cfg *hash_cfg,
				 uint64_t phint)
{
	uint32_t *addl_hdrs = &hash_cfg->addl_hdrs;
	if (phint & ICE_PHINT_VLAN)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_VLAN;

	if (phint & ICE_PHINT_PPPOE)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_PPPOE;

	if (phint & ICE_PHINT_GTPU_EH_DWN)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_GTPU_DWN;
	else if (phint & ICE_PHINT_GTPU_EH_UP)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_GTPU_UP;
	else if (phint & ICE_PHINT_GTPU_EH)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_GTPU_EH;
	else if (phint & ICE_PHINT_GTPU)
		*addl_hdrs |= ICE_FLOW_SEG_HDR_GTPU_IP;
}

static void
ice_refine_hash_cfg_gtpu(struct ice_rss_hash_cfg *hash_cfg,
			 uint64_t rss_type)
{
	uint32_t *addl_hdrs = &hash_cfg->addl_hdrs;
	uint64_t *hash_flds = &hash_cfg->hash_flds;

	/* update hash field for gtpu eh/gtpu dwn/gtpu up. */
	if (!(rss_type & RTE_ETH_RSS_GTPU))
		return;

	if (*addl_hdrs & ICE_FLOW_SEG_HDR_GTPU_DWN)
		*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_GTPU_DWN_TEID);
	else if (*addl_hdrs & ICE_FLOW_SEG_HDR_GTPU_UP)
		*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_GTPU_UP_TEID);
	else if (*addl_hdrs & ICE_FLOW_SEG_HDR_GTPU_EH)
		*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_GTPU_EH_TEID);
	else if (*addl_hdrs & ICE_FLOW_SEG_HDR_GTPU_IP)
		*hash_flds |= BIT_ULL(ICE_FLOW_FIELD_IDX_GTPU_IP_TEID);
}

static void ice_refine_hash_cfg(struct ice_rss_hash_cfg *hash_cfg,
				uint64_t rss_type, uint64_t phint)
{
	ice_refine_proto_hdrs_by_pattern(hash_cfg, phint);
	ice_refine_hash_cfg_l234(hash_cfg, rss_type);
	ice_refine_hash_cfg_gtpu(hash_cfg, rss_type);
}

static uint64_t invalid_rss_comb[] = {
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	RTE_ETH_RSS_L3_PRE40 |
	RTE_ETH_RSS_L3_PRE56 |
	RTE_ETH_RSS_L3_PRE96
};

struct rss_attr_type {
	uint64_t attr;
	uint64_t type;
};

static struct rss_attr_type rss_attr_to_valid_type[] = {
	{RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY,	RTE_ETH_RSS_ETH},
	{RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY,	VALID_RSS_L3},
	{RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY,	VALID_RSS_L4},
	/* current ipv6 prefix only supports prefix 64 bits*/
	{RTE_ETH_RSS_L3_PRE32,				VALID_RSS_IPV6},
	{RTE_ETH_RSS_L3_PRE48,				VALID_RSS_IPV6},
	{RTE_ETH_RSS_L3_PRE64,				VALID_RSS_IPV6},
	{INVALID_RSS_ATTR,				0}
};

static bool
ice_any_invalid_rss_type(enum rte_eth_hash_function rss_func,
			 uint64_t rss_type, uint64_t allow_rss_type)
{
	uint32_t i;

	/**
	 * Check if l3/l4 SRC/DST_ONLY is set for SYMMETRIC_TOEPLITZ
	 * hash function.
	 */
	if (rss_func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
		if (rss_type & (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY |
		    RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY))
			return true;

		if (!(rss_type &
		   (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_IPV6 |
		    RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_FRAG_IPV6 |
		    RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV6_UDP |
		    RTE_ETH_RSS_NONFRAG_IPV4_TCP | RTE_ETH_RSS_NONFRAG_IPV6_TCP |
		    RTE_ETH_RSS_NONFRAG_IPV4_SCTP | RTE_ETH_RSS_NONFRAG_IPV6_SCTP)))
			return true;
	}

	/* check invalid combination */
	for (i = 0; i < RTE_DIM(invalid_rss_comb); i++) {
		if (__builtin_popcountll(rss_type & invalid_rss_comb[i]) > 1)
			return true;
	}

	/* check invalid RSS attribute */
	for (i = 0; i < RTE_DIM(rss_attr_to_valid_type); i++) {
		struct rss_attr_type *rat = &rss_attr_to_valid_type[i];

		if (rat->attr & rss_type && !(rat->type & rss_type))
			return true;
	}

	/* check not allowed RSS type */
	rss_type &= ~VALID_RSS_ATTR;

	return ((rss_type & allow_rss_type) != rss_type);
}

static int
ice_hash_parse_action(struct ice_pattern_match_item *pattern_match_item,
		const struct rte_flow_action actions[],
		uint64_t pattern_hint, struct ice_rss_meta *rss_meta,
		struct rte_flow_error *error)
{
	struct ice_rss_hash_cfg *cfg = pattern_match_item->meta;
	enum rte_flow_action_type action_type;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_action *action;
	uint64_t rss_type;

	/* Supported action is RSS. */
	for (action = actions; action->type !=
		RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = action->conf;
			rss_type = rss->types;

			/* Check hash function and save it to rss_meta. */
			if (pattern_match_item->pattern_list !=
			    pattern_empty && rss->func ==
			    RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Not supported flow");
			} else if (rss->func ==
				   RTE_ETH_HASH_FUNCTION_SIMPLE_XOR){
				rss_meta->hash_function =
				RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
				return 0;
			} else if (rss->func ==
				   RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
				rss_meta->hash_function =
				RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;
				if (pattern_hint == ICE_PHINT_RAW)
					rss_meta->raw.symm = true;
				else
					cfg->symm = true;
			}

			if (rss->level)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS encapsulation level is not supported");

			if (rss->key_len)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS key_len is not supported");

			if (rss->queue)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a non-NULL RSS queue is not supported");

			/* If pattern type is raw, no need to refine rss type */
			if (pattern_hint == ICE_PHINT_RAW)
				break;

			/**
			 * Check simultaneous use of SRC_ONLY and DST_ONLY
			 * of the same level.
			 */
			rss_type = rte_eth_rss_hf_refine(rss_type);

			if (ice_any_invalid_rss_type(rss->func, rss_type,
					pattern_match_item->input_set_mask_o))
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					action, "RSS type not supported");

			rss_meta->cfg = *cfg;
			ice_refine_hash_cfg(&rss_meta->cfg,
					    rss_type, pattern_hint);
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			break;

		default:
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Invalid action.");
			return -rte_errno;
		}
	}

	return 0;
}

static int
ice_hash_parse_pattern_action(__rte_unused struct ice_adapter *ad,
			struct ice_pattern_match_item *array,
			uint32_t array_len,
			const struct rte_flow_item pattern[],
			const struct rte_flow_action actions[],
			uint32_t priority,
			void **meta,
			struct rte_flow_error *error)
{
	int ret = 0;
	struct ice_pattern_match_item *pattern_match_item;
	struct ice_rss_meta *rss_meta_ptr;
	uint64_t phint = ICE_PHINT_NONE;

	if (priority >= 1)
		return -rte_errno;

	rss_meta_ptr = rte_zmalloc(NULL, sizeof(*rss_meta_ptr), 0);
	if (!rss_meta_ptr) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"No memory for rss_meta_ptr");
		return -ENOMEM;
	}

	/* Check rss supported pattern and find matched pattern. */
	pattern_match_item = ice_search_pattern_match_item(ad, pattern, array,
							   array_len, error);
	if (!pattern_match_item) {
		ret = -rte_errno;
		goto error;
	}

	ret = ice_hash_parse_pattern(pattern, &phint, error);
	if (ret)
		goto error;

	if (phint == ICE_PHINT_RAW) {
		rss_meta_ptr->raw.raw_ena = true;
		ret = ice_hash_parse_raw_pattern(ad, pattern, rss_meta_ptr);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					   "Parse raw pattern failed");
			goto error;
		}
	}

	/* Check rss action. */
	ret = ice_hash_parse_action(pattern_match_item, actions, phint,
				    rss_meta_ptr, error);

error:
	if (!ret && meta)
		*meta = rss_meta_ptr;
	else
		rte_free(rss_meta_ptr);
	rte_free(pattern_match_item);

	return ret;
}

static int
ice_hash_add_raw_cfg(struct ice_adapter *ad,
		struct ice_rss_raw_cfg *cfg, u16 vsi_handle)
{
	struct ice_parser_profile *prof = &cfg->prof;
	struct ice_rss_prof_info *rss_prof;
	struct ice_hw *hw = &ad->hw;
	int i, ptg, ret;
	u64 id;

	id = (u64)ice_find_first_bit(prof->ptypes, UINT16_MAX);

	ptg = hw->blk[ICE_BLK_RSS].xlt1.t[id];
	rss_prof = &ad->rss_prof_info[ptg];
	/* check if ptg already has profile */
	if (rss_prof->prof.fv_num) {
		for (i = 0; i < ICE_MAX_FV_WORDS; i++) {
			if (rss_prof->prof.fv[i].proto_id !=
			    prof->fv[i].proto_id ||
			    rss_prof->prof.fv[i].offset !=
			    prof->fv[i].offset)
				break;
		}

		/* current profile is matched, check symmetric hash */
		if (i == ICE_MAX_FV_WORDS) {
			if (rss_prof->symm != cfg->symm)
				goto update_symm;

			return 0;
		}

		/* current profile is not matched, remove it */
		ret = ice_rem_prof_id_flow(hw, ICE_BLK_RSS,
					   ice_get_hw_vsi_num(hw, vsi_handle),
					   id);
		if (ret) {
			PMD_DRV_LOG(ERR, "remove RSS flow failed\n");
			return ret;
		}

		ret = ice_rem_prof(hw, ICE_BLK_RSS, id);
		if (ret) {
			PMD_DRV_LOG(ERR, "remove RSS profile failed\n");
			return ret;
		}
	}

	/* add new profile */
	ret = ice_flow_set_hw_prof(hw, vsi_handle, 0, prof, ICE_BLK_RSS);
	if (ret) {
		PMD_DRV_LOG(ERR, "HW profile add failed\n");
		return ret;
	}

	rss_prof->symm = cfg->symm;
	ice_memcpy(&rss_prof->prof, prof,
		   sizeof(struct ice_parser_profile),
		   ICE_NONDMA_TO_NONDMA);

update_symm:
	ice_rss_update_raw_symm(hw, cfg, id);
	return 0;
}

static int
ice_hash_create(struct ice_adapter *ad,
		struct rte_flow *flow,
		void *meta,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;
	uint32_t reg;
	struct ice_hash_flow_cfg *filter_ptr;
	struct ice_rss_meta *rss_meta = (struct ice_rss_meta *)meta;
	uint8_t hash_function = rss_meta->hash_function;

	filter_ptr = rte_zmalloc("ice_rss_filter",
				sizeof(struct ice_hash_flow_cfg), 0);
	if (!filter_ptr) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"No memory for filter_ptr");
		return -ENOMEM;
	}

	if (hash_function == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR) {
		/* Enable registers for simple_xor hash function. */
		reg = ICE_READ_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id));
		reg = (reg & (~VSIQF_HASH_CTL_HASH_SCHEME_M)) |
			(2 << VSIQF_HASH_CTL_HASH_SCHEME_S);
		ICE_WRITE_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id), reg);

		filter_ptr->simple_xor = 1;

		goto out;
	} else {
		if (rss_meta->raw.raw_ena) {
			memcpy(&filter_ptr->rss_cfg.raw, &rss_meta->raw,
			       sizeof(struct ice_rss_raw_cfg));
			ret = ice_hash_add_raw_cfg(ad, &rss_meta->raw,
						   pf->main_vsi->idx);
			if (ret) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "rss flow create fail");
				goto error;
			}
		} else {
			memcpy(&filter_ptr->rss_cfg.hash, &rss_meta->cfg,
			       sizeof(struct ice_rss_hash_cfg));
			ret = ice_add_rss_cfg_wrap(pf, vsi->idx,
						   &filter_ptr->rss_cfg.hash);
			if (ret) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "rss flow create fail");
				goto error;
			}
		}
	}

out:
	flow->rule = filter_ptr;
	rte_free(meta);
	return 0;

error:
	rte_free(filter_ptr);
	rte_free(meta);
	return -rte_errno;
}

static int
ice_hash_rem_raw_cfg(struct ice_adapter *ad,
			struct ice_parser_profile *prof,
		    u16 vsi_handle)
{
	struct ice_hw *hw = &ad->hw;
	int ptg, ret;
	u16 vsig;
	u64 id;

	id = (u64)ice_find_first_bit(prof->ptypes, 0xFFFF);

	ptg = hw->blk[ICE_BLK_RSS].xlt1.t[id];

	memset(&ad->rss_prof_info[ptg], 0,
		sizeof(struct ice_rss_prof_info));

	/* check if vsig is already removed */
	ret = ice_vsig_find_vsi(hw, ICE_BLK_RSS,
		ice_get_hw_vsi_num(hw, vsi_handle), &vsig);
	if (!ret && vsig) {
		ret = ice_rem_prof_id_flow(hw, ICE_BLK_RSS,
					   ice_get_hw_vsi_num(hw, vsi_handle),
					   id);
		if (ret)
			goto err;

		ret = ice_rem_prof(hw, ICE_BLK_RSS, id);
		if (ret)
			goto err;
	}

	return 0;

err:
	PMD_DRV_LOG(ERR, "HW profile remove failed\n");
	return ret;
}

static int
ice_hash_destroy(struct ice_adapter *ad,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(ad);
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;
	uint32_t reg;
	struct ice_hash_flow_cfg *filter_ptr;

	filter_ptr = (struct ice_hash_flow_cfg *)flow->rule;

	if (filter_ptr->simple_xor == 1) {
		/* Return to symmetric_toeplitz state. */
		reg = ICE_READ_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id));
		reg = (reg & (~VSIQF_HASH_CTL_HASH_SCHEME_M)) |
			(1 << VSIQF_HASH_CTL_HASH_SCHEME_S);
		ICE_WRITE_REG(hw, VSIQF_HASH_CTL(vsi->vsi_id), reg);
	} else {
		if (filter_ptr->rss_cfg.raw.raw_ena) {
			ret =
			ice_hash_rem_raw_cfg(ad, &filter_ptr->rss_cfg.raw.prof,
					     pf->main_vsi->idx);
			if (ret) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "rss flow destroy fail");
				goto error;
			}
		} else {
			ret = ice_rem_rss_cfg_wrap(pf, vsi->idx,
						   &filter_ptr->rss_cfg.hash);
			/* Fixme: Ignore the error if a rule does not exist.
			 * Currently a rule for inputset change or symm turn
			 * on/off will overwrite an exist rule, while
			 * application still have 2 rte_flow handles.
			 **/
			if (ret && ret != ICE_ERR_DOES_NOT_EXIST) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "rss flow destroy fail");
				goto error;
			}
		}
	}

	rte_free(filter_ptr);
	return 0;

error:
	rte_free(filter_ptr);
	return -rte_errno;
}

static void
ice_hash_uninit(struct ice_adapter *ad)
{
	if (ad->hw.dcf_enabled)
		return;

	ice_unregister_parser(&ice_hash_parser, ad);
}

static void
ice_hash_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
}
