/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
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

#include "iavf_log.h"
#include "iavf.h"
#include "iavf_generic_flow.h"

#define IAVF_PHINT_NONE				0
#define IAVF_PHINT_GTPU				BIT_ULL(0)
#define IAVF_PHINT_GTPU_EH			BIT_ULL(1)
#define	IAVF_PHINT_GTPU_EH_DWN			BIT_ULL(2)
#define	IAVF_PHINT_GTPU_EH_UP			BIT_ULL(3)
#define IAVF_PHINT_OUTER_IPV4			BIT_ULL(4)
#define IAVF_PHINT_OUTER_IPV6			BIT_ULL(5)
#define IAVF_PHINT_GRE				BIT_ULL(6)
/* the second IP header of GTPoGRE */
#define IAVF_PHINT_MID_IPV4			BIT_ULL(7)
#define IAVF_PHINT_MID_IPV6			BIT_ULL(8)
/* L2TPv2 */
#define IAVF_PHINT_L2TPV2			BIT_ULL(9)
#define IAVF_PHINT_L2TPV2_LEN			BIT_ULL(10)

#define IAVF_PHINT_GTPU_MSK	(IAVF_PHINT_GTPU	| \
				 IAVF_PHINT_GTPU_EH	| \
				 IAVF_PHINT_GTPU_EH_DWN	| \
				 IAVF_PHINT_GTPU_EH_UP)

#define IAVF_PHINT_LAYERS_MSK	(IAVF_PHINT_OUTER_IPV4	| \
				 IAVF_PHINT_OUTER_IPV6)

#define IAVF_GTPU_EH_DWNLINK	0
#define IAVF_GTPU_EH_UPLINK	1

struct iavf_hash_match_type {
	uint64_t hash_type;
	struct virtchnl_proto_hdrs *proto_hdrs;
	uint64_t pattern_hint;
};

struct iavf_rss_meta {
	struct virtchnl_proto_hdrs proto_hdrs;
	enum virtchnl_rss_algorithm rss_algorithm;
};

struct iavf_hash_flow_cfg {
	struct virtchnl_rss_cfg *rss_cfg;
	bool simple_xor;
};

static int
iavf_hash_init(struct iavf_adapter *ad);
static int
iavf_hash_create(struct iavf_adapter *ad, struct rte_flow *flow, void *meta,
		 struct rte_flow_error *error);
static int
iavf_hash_destroy(struct iavf_adapter *ad, struct rte_flow *flow,
		  struct rte_flow_error *error);
static void
iavf_hash_uninit(struct iavf_adapter *ad);
static void
iavf_hash_free(struct rte_flow *flow);
static int
iavf_hash_parse_pattern_action(struct iavf_adapter *ad,
			       struct iavf_pattern_match_item *array,
			       uint32_t array_len,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       void **meta,
			       struct rte_flow_error *error);

#define FIELD_SELECTOR(proto_hdr_field) \
		(1UL << ((proto_hdr_field) & PROTO_HDR_FIELD_MASK))
#define BUFF_NOUSED			0

#define proto_hdr_eth { \
	VIRTCHNL_PROTO_HDR_ETH, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_ETH_SRC) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_ETH_DST), {BUFF_NOUSED} }

#define proto_hdr_svlan { \
	VIRTCHNL_PROTO_HDR_S_VLAN, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_S_VLAN_ID), {BUFF_NOUSED} }

#define proto_hdr_cvlan { \
	VIRTCHNL_PROTO_HDR_C_VLAN, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_C_VLAN_ID), {BUFF_NOUSED} }

#define proto_hdr_ipv4 { \
	VIRTCHNL_PROTO_HDR_IPV4, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV4_SRC) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV4_DST), {BUFF_NOUSED} }

#define proto_hdr_ipv4_with_prot { \
	VIRTCHNL_PROTO_HDR_IPV4, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV4_SRC) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV4_DST) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV4_PROT), {BUFF_NOUSED} }

#define proto_hdr_ipv6 { \
	VIRTCHNL_PROTO_HDR_IPV6, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_SRC) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_DST), {BUFF_NOUSED} }

#define proto_hdr_ipv6_frag { \
	VIRTCHNL_PROTO_HDR_IPV6_EH_FRAG, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_EH_FRAG_PKID), {BUFF_NOUSED} }

#define proto_hdr_ipv6_with_prot { \
	VIRTCHNL_PROTO_HDR_IPV6, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_SRC) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_DST) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_IPV6_PROT), {BUFF_NOUSED} }

#define proto_hdr_udp { \
	VIRTCHNL_PROTO_HDR_UDP, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_UDP_SRC_PORT) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_UDP_DST_PORT), {BUFF_NOUSED} }

#define proto_hdr_tcp { \
	VIRTCHNL_PROTO_HDR_TCP, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_TCP_SRC_PORT) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_TCP_DST_PORT), {BUFF_NOUSED} }

#define proto_hdr_sctp { \
	VIRTCHNL_PROTO_HDR_SCTP, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_SCTP_SRC_PORT) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_SCTP_DST_PORT), {BUFF_NOUSED} }

#define proto_hdr_esp { \
	VIRTCHNL_PROTO_HDR_ESP, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_ESP_SPI), {BUFF_NOUSED} }

#define proto_hdr_ah { \
	VIRTCHNL_PROTO_HDR_AH, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_AH_SPI), {BUFF_NOUSED} }

#define proto_hdr_l2tpv3 { \
	VIRTCHNL_PROTO_HDR_L2TPV3, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_L2TPV3_SESS_ID), {BUFF_NOUSED} }

#define proto_hdr_pfcp { \
	VIRTCHNL_PROTO_HDR_PFCP, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_PFCP_SEID), {BUFF_NOUSED} }

#define proto_hdr_gtpc { \
	VIRTCHNL_PROTO_HDR_GTPC, 0, {BUFF_NOUSED} }

#define proto_hdr_ecpri { \
	VIRTCHNL_PROTO_HDR_ECPRI, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_ECPRI_PC_RTC_ID), {BUFF_NOUSED} }

#define proto_hdr_l2tpv2 { \
	VIRTCHNL_PROTO_HDR_L2TPV2, \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_L2TPV2_SESS_ID) | \
	FIELD_SELECTOR(VIRTCHNL_PROTO_HDR_L2TPV2_LEN_SESS_ID), {BUFF_NOUSED} }

#define proto_hdr_ppp { \
	VIRTCHNL_PROTO_HDR_PPP, 0, {BUFF_NOUSED} }

#define TUNNEL_LEVEL_OUTER		0
#define TUNNEL_LEVEL_INNER		1

/* proto_hdrs template */
struct virtchnl_proto_hdrs outer_ipv4_tmplt = {
	TUNNEL_LEVEL_OUTER, 4,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan, proto_hdr_ipv4}
};

struct virtchnl_proto_hdrs outer_ipv4_udp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan,
	 proto_hdr_ipv4_with_prot,
	 proto_hdr_udp}
};

struct virtchnl_proto_hdrs outer_ipv4_tcp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan,
	 proto_hdr_ipv4_with_prot,
	 proto_hdr_tcp}
};

struct virtchnl_proto_hdrs outer_ipv4_sctp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan, proto_hdr_ipv4,
	 proto_hdr_sctp}
};

struct virtchnl_proto_hdrs outer_ipv6_tmplt = {
	TUNNEL_LEVEL_OUTER, 4,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan, proto_hdr_ipv6}
};

struct virtchnl_proto_hdrs outer_ipv6_frag_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan,
	 proto_hdr_ipv6, proto_hdr_ipv6_frag}
};

struct virtchnl_proto_hdrs outer_ipv6_udp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan,
	 proto_hdr_ipv6_with_prot,
	 proto_hdr_udp}
};

struct virtchnl_proto_hdrs outer_ipv6_tcp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan,
	 proto_hdr_ipv6_with_prot,
	 proto_hdr_tcp}
};

struct virtchnl_proto_hdrs outer_ipv6_sctp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth, proto_hdr_svlan, proto_hdr_cvlan, proto_hdr_ipv6,
	 proto_hdr_sctp}
};

struct virtchnl_proto_hdrs inner_ipv4_tmplt = {
	TUNNEL_LEVEL_INNER, 1, {proto_hdr_ipv4}
};

struct virtchnl_proto_hdrs inner_ipv4_udp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv4_with_prot, proto_hdr_udp}
};

struct virtchnl_proto_hdrs inner_ipv4_tcp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv4_with_prot, proto_hdr_tcp}
};

struct virtchnl_proto_hdrs second_inner_ipv4_tmplt = {
	2, 1, {proto_hdr_ipv4}
};

struct virtchnl_proto_hdrs second_inner_ipv4_udp_tmplt = {
	2, 2, {proto_hdr_ipv4_with_prot, proto_hdr_udp}
};

struct virtchnl_proto_hdrs second_inner_ipv4_tcp_tmplt = {
	2, 2, {proto_hdr_ipv4_with_prot, proto_hdr_tcp}
};

struct virtchnl_proto_hdrs second_inner_ipv6_tmplt = {
	2, 1, {proto_hdr_ipv6}
};

struct virtchnl_proto_hdrs second_inner_ipv6_udp_tmplt = {
	2, 2, {proto_hdr_ipv6_with_prot, proto_hdr_udp}
};

struct virtchnl_proto_hdrs second_inner_ipv6_tcp_tmplt = {
	2, 2, {proto_hdr_ipv6_with_prot, proto_hdr_tcp}
};

struct virtchnl_proto_hdrs inner_ipv4_sctp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv4, proto_hdr_sctp}
};

struct virtchnl_proto_hdrs inner_ipv6_tmplt = {
	TUNNEL_LEVEL_INNER, 1, {proto_hdr_ipv6}
};

struct virtchnl_proto_hdrs inner_ipv6_udp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv6_with_prot, proto_hdr_udp}
};

struct virtchnl_proto_hdrs inner_ipv6_tcp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv6_with_prot, proto_hdr_tcp}
};

struct virtchnl_proto_hdrs inner_ipv6_sctp_tmplt = {
	TUNNEL_LEVEL_INNER, 2, {proto_hdr_ipv6, proto_hdr_sctp}
};

struct virtchnl_proto_hdrs ipv4_esp_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv4, proto_hdr_esp}
};

struct virtchnl_proto_hdrs ipv4_udp_esp_tmplt = {
	TUNNEL_LEVEL_OUTER, 3,
	{proto_hdr_ipv4, proto_hdr_udp, proto_hdr_esp}
};

struct virtchnl_proto_hdrs ipv4_ah_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv4, proto_hdr_ah}
};

struct virtchnl_proto_hdrs ipv6_esp_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv6, proto_hdr_esp}
};

struct virtchnl_proto_hdrs ipv6_udp_esp_tmplt = {
	TUNNEL_LEVEL_OUTER, 3,
	{proto_hdr_ipv6, proto_hdr_udp, proto_hdr_esp}
};

struct virtchnl_proto_hdrs ipv6_ah_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv6, proto_hdr_ah}
};

struct virtchnl_proto_hdrs ipv4_l2tpv3_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv4, proto_hdr_l2tpv3}
};

struct virtchnl_proto_hdrs ipv6_l2tpv3_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv6, proto_hdr_l2tpv3}
};

struct virtchnl_proto_hdrs ipv4_pfcp_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv4, proto_hdr_pfcp}
};

struct virtchnl_proto_hdrs ipv6_pfcp_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_ipv6, proto_hdr_pfcp}
};

struct virtchnl_proto_hdrs ipv4_udp_gtpc_tmplt = {
	TUNNEL_LEVEL_OUTER, 3, {proto_hdr_ipv4, proto_hdr_udp, proto_hdr_gtpc}
};

struct virtchnl_proto_hdrs ipv6_udp_gtpc_tmplt = {
	TUNNEL_LEVEL_OUTER, 3, {proto_hdr_ipv6, proto_hdr_udp, proto_hdr_gtpc}
};

struct virtchnl_proto_hdrs eth_ecpri_tmplt = {
	TUNNEL_LEVEL_OUTER, 2, {proto_hdr_eth, proto_hdr_ecpri}
};

struct virtchnl_proto_hdrs ipv4_ecpri_tmplt = {
	TUNNEL_LEVEL_OUTER, 3, {proto_hdr_ipv4, proto_hdr_udp, proto_hdr_ecpri}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv4_tmplt = {
	TUNNEL_LEVEL_INNER, 3,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv4}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv6_tmplt = {
	TUNNEL_LEVEL_INNER, 3,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv6}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv4_udp_tmplt = {
	TUNNEL_LEVEL_INNER, 4,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv4_with_prot,
	 proto_hdr_udp}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv4_tcp_tmplt = {
	TUNNEL_LEVEL_INNER, 4,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv4_with_prot,
	 proto_hdr_tcp}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv6_udp_tmplt = {
	TUNNEL_LEVEL_INNER, 4,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv6_with_prot,
	 proto_hdr_udp}
};

struct virtchnl_proto_hdrs udp_l2tpv2_ppp_ipv6_tcp_tmplt = {
	TUNNEL_LEVEL_INNER, 4,
	{proto_hdr_l2tpv2,
	 proto_hdr_ppp,
	 proto_hdr_ipv6_with_prot,
	 proto_hdr_tcp}
};

struct virtchnl_proto_hdrs ipv4_l2tpv2_tmplt = {
	TUNNEL_LEVEL_OUTER, 4,
	{proto_hdr_eth,
	 proto_hdr_ipv4,
	 proto_hdr_udp,
	 proto_hdr_l2tpv2}
};

struct virtchnl_proto_hdrs ipv6_l2tpv2_tmplt = {
	TUNNEL_LEVEL_OUTER, 4,
	{proto_hdr_eth,
	 proto_hdr_ipv6,
	 proto_hdr_udp,
	 proto_hdr_l2tpv2}
};

struct virtchnl_proto_hdrs ipv4_l2tpv2_ppp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth,
	 proto_hdr_ipv4,
	 proto_hdr_udp,
	 proto_hdr_l2tpv2,
	 proto_hdr_ppp}
};

struct virtchnl_proto_hdrs ipv6_l2tpv2_ppp_tmplt = {
	TUNNEL_LEVEL_OUTER, 5,
	{proto_hdr_eth,
	 proto_hdr_ipv6,
	 proto_hdr_udp,
	 proto_hdr_l2tpv2,
	 proto_hdr_ppp}
};

/* rss type super set */

/* IPv4 outer */
#define IAVF_RSS_TYPE_OUTER_IPV4	(RTE_ETH_RSS_ETH | RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_FRAG_IPV4 | \
					 RTE_ETH_RSS_IPV4_CHKSUM)
#define IAVF_RSS_TYPE_OUTER_IPV4_UDP	(IAVF_RSS_TYPE_OUTER_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define IAVF_RSS_TYPE_OUTER_IPV4_TCP	(IAVF_RSS_TYPE_OUTER_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define IAVF_RSS_TYPE_OUTER_IPV4_SCTP	(IAVF_RSS_TYPE_OUTER_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
					 RTE_ETH_RSS_L4_CHKSUM)
/* IPv6 outer */
#define IAVF_RSS_TYPE_OUTER_IPV6	(RTE_ETH_RSS_ETH | RTE_ETH_RSS_IPV6)
#define IAVF_RSS_TYPE_OUTER_IPV6_FRAG	(IAVF_RSS_TYPE_OUTER_IPV6 | \
					 RTE_ETH_RSS_FRAG_IPV6)
#define IAVF_RSS_TYPE_OUTER_IPV6_UDP	(IAVF_RSS_TYPE_OUTER_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define IAVF_RSS_TYPE_OUTER_IPV6_TCP	(IAVF_RSS_TYPE_OUTER_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
					 RTE_ETH_RSS_L4_CHKSUM)
#define IAVF_RSS_TYPE_OUTER_IPV6_SCTP	(IAVF_RSS_TYPE_OUTER_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
					 RTE_ETH_RSS_L4_CHKSUM)
/* VLAN IPV4 */
#define IAVF_RSS_TYPE_VLAN_IPV4		(IAVF_RSS_TYPE_OUTER_IPV4 | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV4_UDP	(IAVF_RSS_TYPE_OUTER_IPV4_UDP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV4_TCP	(IAVF_RSS_TYPE_OUTER_IPV4_TCP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV4_SCTP	(IAVF_RSS_TYPE_OUTER_IPV4_SCTP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
/* VLAN IPv6 */
#define IAVF_RSS_TYPE_VLAN_IPV6		(IAVF_RSS_TYPE_OUTER_IPV6 | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV6_FRAG	(IAVF_RSS_TYPE_OUTER_IPV6_FRAG | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV6_UDP	(IAVF_RSS_TYPE_OUTER_IPV6_UDP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV6_TCP	(IAVF_RSS_TYPE_OUTER_IPV6_TCP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
#define IAVF_RSS_TYPE_VLAN_IPV6_SCTP	(IAVF_RSS_TYPE_OUTER_IPV6_SCTP | \
					 RTE_ETH_RSS_S_VLAN | RTE_ETH_RSS_C_VLAN)
/* IPv4 inner */
#define IAVF_RSS_TYPE_INNER_IPV4	RTE_ETH_RSS_IPV4
#define IAVF_RSS_TYPE_INNER_IPV4_UDP	(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_UDP)
#define IAVF_RSS_TYPE_INNER_IPV4_TCP	(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_TCP)
#define IAVF_RSS_TYPE_INNER_IPV4_SCTP	(RTE_ETH_RSS_IPV4 | \
					 RTE_ETH_RSS_NONFRAG_IPV4_SCTP)
/* IPv6 inner */
#define IAVF_RSS_TYPE_INNER_IPV6	RTE_ETH_RSS_IPV6
#define IAVF_RSS_TYPE_INNER_IPV6_UDP	(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_UDP)
#define IAVF_RSS_TYPE_INNER_IPV6_TCP	(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_TCP)
#define IAVF_RSS_TYPE_INNER_IPV6_SCTP	(RTE_ETH_RSS_IPV6 | \
					 RTE_ETH_RSS_NONFRAG_IPV6_SCTP)
/* GTPU IPv4 */
#define IAVF_RSS_TYPE_GTPU_IPV4		(IAVF_RSS_TYPE_INNER_IPV4 | \
					 RTE_ETH_RSS_GTPU)
#define IAVF_RSS_TYPE_GTPU_IPV4_UDP	(IAVF_RSS_TYPE_INNER_IPV4_UDP | \
					 RTE_ETH_RSS_GTPU)
#define IAVF_RSS_TYPE_GTPU_IPV4_TCP	(IAVF_RSS_TYPE_INNER_IPV4_TCP | \
					 RTE_ETH_RSS_GTPU)
/* GTPU IPv6 */
#define IAVF_RSS_TYPE_GTPU_IPV6		(IAVF_RSS_TYPE_INNER_IPV6 | \
					 RTE_ETH_RSS_GTPU)
#define IAVF_RSS_TYPE_GTPU_IPV6_UDP	(IAVF_RSS_TYPE_INNER_IPV6_UDP | \
					 RTE_ETH_RSS_GTPU)
#define IAVF_RSS_TYPE_GTPU_IPV6_TCP	(IAVF_RSS_TYPE_INNER_IPV6_TCP | \
					 RTE_ETH_RSS_GTPU)
/* ESP, AH, L2TPV3 and PFCP */
#define IAVF_RSS_TYPE_IPV4_ESP		(RTE_ETH_RSS_ESP | RTE_ETH_RSS_IPV4)
#define IAVF_RSS_TYPE_IPV4_AH		(RTE_ETH_RSS_AH | RTE_ETH_RSS_IPV4)
#define IAVF_RSS_TYPE_IPV6_ESP		(RTE_ETH_RSS_ESP | RTE_ETH_RSS_IPV6)
#define IAVF_RSS_TYPE_IPV6_AH		(RTE_ETH_RSS_AH | RTE_ETH_RSS_IPV6)
#define IAVF_RSS_TYPE_IPV4_L2TPV3	(RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_IPV4)
#define IAVF_RSS_TYPE_IPV6_L2TPV3	(RTE_ETH_RSS_L2TPV3 | RTE_ETH_RSS_IPV6)
#define IAVF_RSS_TYPE_IPV4_PFCP		(RTE_ETH_RSS_PFCP | RTE_ETH_RSS_IPV4)
#define IAVF_RSS_TYPE_IPV6_PFCP		(RTE_ETH_RSS_PFCP | RTE_ETH_RSS_IPV6)

/* L2TPv2 */
#define IAVF_RSS_TYPE_ETH_L2TPV2	(RTE_ETH_RSS_ETH | RTE_ETH_RSS_L2TPV2)

/**
 * Supported pattern for hash.
 * The first member is pattern item type,
 * the second member is input set mask,
 * the third member is virtchnl_proto_hdrs template
 */
static struct iavf_pattern_match_item iavf_hash_pattern_list[] = {
	/* IPv4 */
	{iavf_pattern_eth_ipv4,				IAVF_RSS_TYPE_OUTER_IPV4,	&outer_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_udp,			IAVF_RSS_TYPE_OUTER_IPV4_UDP,	&outer_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_tcp,			IAVF_RSS_TYPE_OUTER_IPV4_TCP,	&outer_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_sctp,			IAVF_RSS_TYPE_OUTER_IPV4_SCTP,	&outer_ipv4_sctp_tmplt},
	{iavf_pattern_eth_vlan_ipv4,			IAVF_RSS_TYPE_VLAN_IPV4,	&outer_ipv4_tmplt},
	{iavf_pattern_eth_vlan_ipv4_udp,		IAVF_RSS_TYPE_VLAN_IPV4_UDP,	&outer_ipv4_udp_tmplt},
	{iavf_pattern_eth_vlan_ipv4_tcp,		IAVF_RSS_TYPE_VLAN_IPV4_TCP,	&outer_ipv4_tcp_tmplt},
	{iavf_pattern_eth_vlan_ipv4_sctp,		IAVF_RSS_TYPE_VLAN_IPV4_SCTP,	&outer_ipv4_sctp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu,			RTE_ETH_RSS_IPV4,			&outer_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4_udp,		IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv4_tcp,		IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4,		IAVF_RSS_TYPE_GTPU_IPV4,	&second_inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4_udp,	IAVF_RSS_TYPE_GTPU_IPV4_UDP,	&second_inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv4_tcp,	IAVF_RSS_TYPE_GTPU_IPV4_TCP,	&second_inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_esp,			IAVF_RSS_TYPE_IPV4_ESP,		&ipv4_esp_tmplt},
	{iavf_pattern_eth_ipv4_udp_esp,			IAVF_RSS_TYPE_IPV4_ESP,		&ipv4_udp_esp_tmplt},
	{iavf_pattern_eth_ipv4_ah,			IAVF_RSS_TYPE_IPV4_AH,		&ipv4_ah_tmplt},
	{iavf_pattern_eth_ipv4_l2tpv3,			IAVF_RSS_TYPE_IPV4_L2TPV3,	&ipv4_l2tpv3_tmplt},
	{iavf_pattern_eth_ipv4_pfcp,			IAVF_RSS_TYPE_IPV4_PFCP,	&ipv4_pfcp_tmplt},
	{iavf_pattern_eth_ipv4_gtpc,			RTE_ETH_RSS_IPV4,			&ipv4_udp_gtpc_tmplt},
	{iavf_pattern_eth_ecpri,			RTE_ETH_RSS_ECPRI,			&eth_ecpri_tmplt},
	{iavf_pattern_eth_ipv4_ecpri,			RTE_ETH_RSS_ECPRI,			&ipv4_ecpri_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4,		IAVF_RSS_TYPE_INNER_IPV4,	&inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4,		IAVF_RSS_TYPE_INNER_IPV4, &inner_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_tcp,	IAVF_RSS_TYPE_INNER_IPV4_TCP, &inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_tcp,	IAVF_RSS_TYPE_INNER_IPV4_TCP, &inner_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_udp,	IAVF_RSS_TYPE_INNER_IPV4_UDP, &inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_udp,	IAVF_RSS_TYPE_INNER_IPV4_UDP, &inner_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2,		IAVF_RSS_TYPE_ETH_L2TPV2,	&ipv4_l2tpv2_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp,		IAVF_RSS_TYPE_ETH_L2TPV2,	&ipv4_l2tpv2_ppp_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4,	IAVF_RSS_TYPE_INNER_IPV4,	&udp_l2tpv2_ppp_ipv4_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4_udp,	IAVF_RSS_TYPE_INNER_IPV4_UDP,	&udp_l2tpv2_ppp_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv4_tcp,	IAVF_RSS_TYPE_INNER_IPV4_TCP,	&udp_l2tpv2_ppp_ipv4_tcp_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4,	IAVF_RSS_TYPE_INNER_IPV4,	&udp_l2tpv2_ppp_ipv4_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4_udp,	IAVF_RSS_TYPE_INNER_IPV4_UDP,	&udp_l2tpv2_ppp_ipv4_udp_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv4_tcp,	IAVF_RSS_TYPE_INNER_IPV4_TCP,	&udp_l2tpv2_ppp_ipv4_tcp_tmplt},

	/* IPv6 */
	{iavf_pattern_eth_ipv6,				IAVF_RSS_TYPE_OUTER_IPV6,	&outer_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_frag_ext,		IAVF_RSS_TYPE_OUTER_IPV6_FRAG,	&outer_ipv6_frag_tmplt},
	{iavf_pattern_eth_ipv6_udp,			IAVF_RSS_TYPE_OUTER_IPV6_UDP,	&outer_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_tcp,			IAVF_RSS_TYPE_OUTER_IPV6_TCP,	&outer_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_sctp,			IAVF_RSS_TYPE_OUTER_IPV6_SCTP,	&outer_ipv6_sctp_tmplt},
	{iavf_pattern_eth_vlan_ipv6,			IAVF_RSS_TYPE_VLAN_IPV6,	&outer_ipv6_tmplt},
	{iavf_pattern_eth_vlan_ipv6_frag_ext,		IAVF_RSS_TYPE_OUTER_IPV6_FRAG,	&outer_ipv6_frag_tmplt},
	{iavf_pattern_eth_vlan_ipv6_udp,		IAVF_RSS_TYPE_VLAN_IPV6_UDP,	&outer_ipv6_udp_tmplt},
	{iavf_pattern_eth_vlan_ipv6_tcp,		IAVF_RSS_TYPE_VLAN_IPV6_TCP,	&outer_ipv6_tcp_tmplt},
	{iavf_pattern_eth_vlan_ipv6_sctp,		IAVF_RSS_TYPE_VLAN_IPV6_SCTP,	&outer_ipv6_sctp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu,			RTE_ETH_RSS_IPV6,			&outer_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6_udp,		IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_ipv6_tcp,		IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv4_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv4_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6,		IAVF_RSS_TYPE_GTPU_IPV6,	&second_inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6_udp,	IAVF_RSS_TYPE_GTPU_IPV6_UDP,	&second_inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_gtpu_eh_ipv6_tcp,	IAVF_RSS_TYPE_GTPU_IPV6_TCP,	&second_inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_esp,			IAVF_RSS_TYPE_IPV6_ESP,		&ipv6_esp_tmplt},
	{iavf_pattern_eth_ipv6_udp_esp,			IAVF_RSS_TYPE_IPV6_ESP,		&ipv6_udp_esp_tmplt},
	{iavf_pattern_eth_ipv6_ah,			IAVF_RSS_TYPE_IPV6_AH,		&ipv6_ah_tmplt},
	{iavf_pattern_eth_ipv6_l2tpv3,			IAVF_RSS_TYPE_IPV6_L2TPV3,	&ipv6_l2tpv3_tmplt},
	{iavf_pattern_eth_ipv6_pfcp,			IAVF_RSS_TYPE_IPV6_PFCP,	&ipv6_pfcp_tmplt},
	{iavf_pattern_eth_ipv6_gtpc,			RTE_ETH_RSS_IPV6,			&ipv6_udp_gtpc_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6,		IAVF_RSS_TYPE_INNER_IPV6,	&inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6,		IAVF_RSS_TYPE_INNER_IPV6, &inner_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_tcp,	IAVF_RSS_TYPE_INNER_IPV6_TCP, &inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_tcp,	IAVF_RSS_TYPE_INNER_IPV6_TCP, &inner_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv4_gre_ipv6_udp,	IAVF_RSS_TYPE_INNER_IPV6_UDP, &inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_gre_ipv6_udp,	IAVF_RSS_TYPE_INNER_IPV6_UDP, &inner_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2,		IAVF_RSS_TYPE_ETH_L2TPV2,	&ipv6_l2tpv2_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp,		IAVF_RSS_TYPE_ETH_L2TPV2,	&ipv6_l2tpv2_ppp_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6,	IAVF_RSS_TYPE_INNER_IPV6,	&udp_l2tpv2_ppp_ipv6_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6_udp,	IAVF_RSS_TYPE_INNER_IPV6_UDP,	&udp_l2tpv2_ppp_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv4_udp_l2tpv2_ppp_ipv6_tcp,	IAVF_RSS_TYPE_INNER_IPV6_TCP,	&udp_l2tpv2_ppp_ipv6_tcp_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6,	IAVF_RSS_TYPE_INNER_IPV6,	&udp_l2tpv2_ppp_ipv6_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6_udp,	IAVF_RSS_TYPE_INNER_IPV6_UDP,	&udp_l2tpv2_ppp_ipv6_udp_tmplt},
	{iavf_pattern_eth_ipv6_udp_l2tpv2_ppp_ipv6_tcp,	IAVF_RSS_TYPE_INNER_IPV6_TCP,	&udp_l2tpv2_ppp_ipv6_tcp_tmplt},

};

static struct iavf_flow_engine iavf_hash_engine = {
	.init = iavf_hash_init,
	.create = iavf_hash_create,
	.destroy = iavf_hash_destroy,
	.uninit = iavf_hash_uninit,
	.free = iavf_hash_free,
	.type = IAVF_FLOW_ENGINE_HASH,
};

/* Register parser for comms package. */
static struct iavf_flow_parser iavf_hash_parser = {
	.engine = &iavf_hash_engine,
	.array = iavf_hash_pattern_list,
	.array_len = RTE_DIM(iavf_hash_pattern_list),
	.parse_pattern_action = iavf_hash_parse_pattern_action,
	.stage = IAVF_FLOW_STAGE_RSS,
};

int
iavf_rss_hash_set(struct iavf_adapter *ad, uint64_t rss_hf, bool add)
{
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(ad);
	struct virtchnl_rss_cfg rss_cfg;

#define IAVF_RSS_HF_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP)

	rss_cfg.rss_algorithm = VIRTCHNL_RSS_ALG_TOEPLITZ_ASYMMETRIC;
	if (rss_hf & RTE_ETH_RSS_IPV4) {
		rss_cfg.proto_hdrs = inner_ipv4_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_UDP) {
		rss_cfg.proto_hdrs = inner_ipv4_udp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_TCP) {
		rss_cfg.proto_hdrs = inner_ipv4_tcp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV4_SCTP) {
		rss_cfg.proto_hdrs = inner_ipv4_sctp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_IPV6) {
		rss_cfg.proto_hdrs = inner_ipv6_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_UDP) {
		rss_cfg.proto_hdrs = inner_ipv6_udp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_TCP) {
		rss_cfg.proto_hdrs = inner_ipv6_tcp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	if (rss_hf & RTE_ETH_RSS_NONFRAG_IPV6_SCTP) {
		rss_cfg.proto_hdrs = inner_ipv6_sctp_tmplt;
		iavf_add_del_rss_cfg(ad, &rss_cfg, add);
	}

	vf->rss_hf = rss_hf & IAVF_RSS_HF_ALL;
	return 0;
}

RTE_INIT(iavf_hash_engine_init)
{
	struct iavf_flow_engine *engine = &iavf_hash_engine;

	iavf_register_flow_engine(engine);
}

static int
iavf_hash_init(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct iavf_flow_parser *parser;
	int ret;

	if (vf->vf_reset)
		return -EIO;

	if (!vf->vf_res)
		return -EINVAL;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF))
		return -ENOTSUP;

	parser = &iavf_hash_parser;

	ret = iavf_register_parser(parser, ad);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to register hash parser");
		return ret;
	}

	return ret;
}

static int
iavf_hash_parse_pattern(const struct rte_flow_item pattern[], uint64_t *phint,
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_gtp_psc *psc;
	const struct rte_flow_item_ecpri *ecpri;
	struct rte_ecpri_common_hdr ecpri_common;
	const struct rte_flow_item_l2tpv2 *l2tpv2;
	uint16_t flags_version;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Not support range");
			return -rte_errno;
		}

		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			if (!(*phint & IAVF_PHINT_GTPU_MSK) &&
			    !(*phint & IAVF_PHINT_GRE) &&
			    !(*phint & IAVF_PHINT_L2TPV2))
				*phint |= IAVF_PHINT_OUTER_IPV4;
			if ((*phint & IAVF_PHINT_GRE) && !(*phint & IAVF_PHINT_GTPU_MSK))
				*phint |= IAVF_PHINT_MID_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			if (!(*phint & IAVF_PHINT_GTPU_MSK) &&
			    !(*phint & IAVF_PHINT_GRE) &&
			    !(*phint & IAVF_PHINT_L2TPV2))
				*phint |= IAVF_PHINT_OUTER_IPV6;
			if ((*phint & IAVF_PHINT_GRE) && !(*phint & IAVF_PHINT_GTPU_MSK))
				*phint |= IAVF_PHINT_MID_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_GTPU:
			*phint |= IAVF_PHINT_GTPU;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			*phint |= IAVF_PHINT_GTPU_EH;
			psc = item->spec;
			if (!psc)
				break;
			else if (psc->hdr.type == IAVF_GTPU_EH_UPLINK)
				*phint |= IAVF_PHINT_GTPU_EH_UP;
			else if (psc->hdr.type == IAVF_GTPU_EH_DWNLINK)
				*phint |= IAVF_PHINT_GTPU_EH_DWN;
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			ecpri = item->spec;
			if (!ecpri)
				break;

			ecpri_common.u32 = rte_be_to_cpu_32(ecpri->hdr.common.u32);

			if (ecpri_common.type !=
				 RTE_ECPRI_MSG_TYPE_IQ_DATA) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Unsupported common type.");
				return -rte_errno;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			*phint |= IAVF_PHINT_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_L2TPV2:
			l2tpv2 = item->spec;

			if (l2tpv2) {
				flags_version =
					rte_be_to_cpu_16(l2tpv2->hdr.common.flags_version);
				if (flags_version & IAVF_L2TPV2_FLAGS_LEN)
					*phint |= IAVF_PHINT_L2TPV2_LEN;
				else
					*phint |= IAVF_PHINT_L2TPV2;
			} else {
				*phint |= IAVF_PHINT_L2TPV2;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

#define REFINE_PROTO_FLD(op, fld) \
	VIRTCHNL_##op##_PROTO_HDR_FIELD(hdr, VIRTCHNL_PROTO_HDR_##fld)
#define REPALCE_PROTO_FLD(fld_1, fld_2) \
do { \
	REFINE_PROTO_FLD(DEL, fld_1);	\
	REFINE_PROTO_FLD(ADD, fld_2);	\
} while (0)

static void
iavf_hash_add_fragment_hdr(struct virtchnl_proto_hdrs *hdrs, int layer)
{
	struct virtchnl_proto_hdr *hdr1;
	struct virtchnl_proto_hdr *hdr2;
	int i;

	if (layer < 0 || layer > hdrs->count)
		return;

	/* shift headers layer */
	for (i = hdrs->count; i >= layer; i--) {
		hdr1 = &hdrs->proto_hdr[i];
		hdr2 = &hdrs->proto_hdr[i - 1];
		*hdr1 = *hdr2;
	}

	/* adding dummy fragment header */
	hdr1 = &hdrs->proto_hdr[layer];
	VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, IPV4_FRAG);
	hdrs->count = ++layer;
}

/* refine proto hdrs base on l2, l3, l4 rss type */
static void
iavf_refine_proto_hdrs_l234(struct virtchnl_proto_hdrs *proto_hdrs,
			    uint64_t rss_type)
{
	struct virtchnl_proto_hdr *hdr;
	int i;

	for (i = 0; i < proto_hdrs->count; i++) {
		hdr = &proto_hdrs->proto_hdr[i];
		switch (hdr->type) {
		case VIRTCHNL_PROTO_HDR_ETH:
			if (!(rss_type & RTE_ETH_RSS_ETH))
				hdr->field_selector = 0;
			else if (rss_type & RTE_ETH_RSS_L2_SRC_ONLY)
				REFINE_PROTO_FLD(DEL, ETH_DST);
			else if (rss_type & RTE_ETH_RSS_L2_DST_ONLY)
				REFINE_PROTO_FLD(DEL, ETH_SRC);
			break;
		case VIRTCHNL_PROTO_HDR_IPV4:
			if (rss_type &
			    (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			     RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			     RTE_ETH_RSS_NONFRAG_IPV4_SCTP)) {
				if (rss_type & RTE_ETH_RSS_FRAG_IPV4) {
					iavf_hash_add_fragment_hdr(proto_hdrs, i + 1);
				} else if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
					REFINE_PROTO_FLD(DEL, IPV4_DST);
				} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
					REFINE_PROTO_FLD(DEL, IPV4_SRC);
				} else if (rss_type &
					   (RTE_ETH_RSS_L4_SRC_ONLY |
					    RTE_ETH_RSS_L4_DST_ONLY)) {
					REFINE_PROTO_FLD(DEL, IPV4_DST);
					REFINE_PROTO_FLD(DEL, IPV4_SRC);
				}
			} else {
				hdr->field_selector = 0;
			}

			if (rss_type & RTE_ETH_RSS_IPV4_CHKSUM)
				REFINE_PROTO_FLD(ADD, IPV4_CHKSUM);

			break;
		case VIRTCHNL_PROTO_HDR_IPV4_FRAG:
			if (rss_type &
			    (RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			     RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			     RTE_ETH_RSS_NONFRAG_IPV4_SCTP)) {
				if (rss_type & RTE_ETH_RSS_FRAG_IPV4)
					REFINE_PROTO_FLD(ADD, IPV4_FRAG_PKID);
			} else {
				hdr->field_selector = 0;
			}

			if (rss_type & RTE_ETH_RSS_IPV4_CHKSUM)
				REFINE_PROTO_FLD(ADD, IPV4_CHKSUM);

			break;
		case VIRTCHNL_PROTO_HDR_IPV6:
			if (rss_type &
			    (RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
			     RTE_ETH_RSS_NONFRAG_IPV6_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV6_TCP |
			     RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
				if (rss_type & RTE_ETH_RSS_L3_SRC_ONLY) {
					REFINE_PROTO_FLD(DEL, IPV6_DST);
				} else if (rss_type & RTE_ETH_RSS_L3_DST_ONLY) {
					REFINE_PROTO_FLD(DEL, IPV6_SRC);
				} else if (rss_type &
					   (RTE_ETH_RSS_L4_SRC_ONLY |
					    RTE_ETH_RSS_L4_DST_ONLY)) {
					REFINE_PROTO_FLD(DEL, IPV6_DST);
					REFINE_PROTO_FLD(DEL, IPV6_SRC);
				}
			} else {
				hdr->field_selector = 0;
			}
			if (rss_type & RTE_ETH_RSS_L3_PRE64) {
				if (REFINE_PROTO_FLD(TEST, IPV6_SRC))
					REPALCE_PROTO_FLD(IPV6_SRC,
							  IPV6_PREFIX64_SRC);
				if (REFINE_PROTO_FLD(TEST, IPV6_DST))
					REPALCE_PROTO_FLD(IPV6_DST,
							  IPV6_PREFIX64_DST);
			}
			break;
		case VIRTCHNL_PROTO_HDR_IPV6_EH_FRAG:
			if (rss_type & RTE_ETH_RSS_FRAG_IPV6)
				REFINE_PROTO_FLD(ADD, IPV6_EH_FRAG_PKID);
			else
				hdr->field_selector = 0;

			break;
		case VIRTCHNL_PROTO_HDR_UDP:
			if (rss_type &
			    (RTE_ETH_RSS_NONFRAG_IPV4_UDP |
			     RTE_ETH_RSS_NONFRAG_IPV6_UDP)) {
				if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
					REFINE_PROTO_FLD(DEL, UDP_DST_PORT);
				else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
					REFINE_PROTO_FLD(DEL, UDP_SRC_PORT);
				else if (rss_type &
					 (RTE_ETH_RSS_L3_SRC_ONLY |
					  RTE_ETH_RSS_L3_DST_ONLY))
					hdr->field_selector = 0;
			} else {
				hdr->field_selector = 0;
			}

			if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
				REFINE_PROTO_FLD(ADD, UDP_CHKSUM);
			break;
		case VIRTCHNL_PROTO_HDR_TCP:
			if (rss_type &
			    (RTE_ETH_RSS_NONFRAG_IPV4_TCP |
			     RTE_ETH_RSS_NONFRAG_IPV6_TCP)) {
				if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
					REFINE_PROTO_FLD(DEL, TCP_DST_PORT);
				else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
					REFINE_PROTO_FLD(DEL, TCP_SRC_PORT);
				else if (rss_type &
					 (RTE_ETH_RSS_L3_SRC_ONLY |
					  RTE_ETH_RSS_L3_DST_ONLY))
					hdr->field_selector = 0;
			} else {
				hdr->field_selector = 0;
			}

			if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
				REFINE_PROTO_FLD(ADD, TCP_CHKSUM);
			break;
		case VIRTCHNL_PROTO_HDR_SCTP:
			if (rss_type &
			    (RTE_ETH_RSS_NONFRAG_IPV4_SCTP |
			     RTE_ETH_RSS_NONFRAG_IPV6_SCTP)) {
				if (rss_type & RTE_ETH_RSS_L4_SRC_ONLY)
					REFINE_PROTO_FLD(DEL, SCTP_DST_PORT);
				else if (rss_type & RTE_ETH_RSS_L4_DST_ONLY)
					REFINE_PROTO_FLD(DEL, SCTP_SRC_PORT);
				else if (rss_type &
					 (RTE_ETH_RSS_L3_SRC_ONLY |
					  RTE_ETH_RSS_L3_DST_ONLY))
					hdr->field_selector = 0;
			} else {
				hdr->field_selector = 0;
			}

			if (rss_type & RTE_ETH_RSS_L4_CHKSUM)
				REFINE_PROTO_FLD(ADD, SCTP_CHKSUM);
			break;
		case VIRTCHNL_PROTO_HDR_S_VLAN:
			if (!(rss_type & RTE_ETH_RSS_S_VLAN))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_C_VLAN:
			if (!(rss_type & RTE_ETH_RSS_C_VLAN))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_L2TPV3:
			if (!(rss_type & RTE_ETH_RSS_L2TPV3))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_ESP:
			if (!(rss_type & RTE_ETH_RSS_ESP))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_AH:
			if (!(rss_type & RTE_ETH_RSS_AH))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_PFCP:
			if (!(rss_type & RTE_ETH_RSS_PFCP))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_ECPRI:
			if (!(rss_type & RTE_ETH_RSS_ECPRI))
				hdr->field_selector = 0;
			break;
		case VIRTCHNL_PROTO_HDR_L2TPV2:
			if (!(rss_type & RTE_ETH_RSS_L2TPV2))
				hdr->field_selector = 0;
			break;
		default:
			break;
		}
	}
}

/* refine proto hdrs base on gtpu rss type */
static void
iavf_refine_proto_hdrs_gtpu(struct virtchnl_proto_hdrs *proto_hdrs,
			    uint64_t rss_type)
{
	struct virtchnl_proto_hdr *hdr;
	int i;

	if (!(rss_type & RTE_ETH_RSS_GTPU))
		return;

	for (i = 0; i < proto_hdrs->count; i++) {
		hdr = &proto_hdrs->proto_hdr[i];
		switch (hdr->type) {
		case VIRTCHNL_PROTO_HDR_GTPU_IP:
			REFINE_PROTO_FLD(ADD, GTPU_IP_TEID);
			break;
		default:
			break;
		}
	}
}

static void
iavf_refine_proto_hdrs_by_pattern(struct virtchnl_proto_hdrs *proto_hdrs,
				  uint64_t phint)
{
	struct virtchnl_proto_hdr *hdr1;
	struct virtchnl_proto_hdr *hdr2;
	int i, shift_count = 1;
	int tun_lvl = proto_hdrs->tunnel_level;

	if (!(phint & IAVF_PHINT_GTPU_MSK) && !(phint & IAVF_PHINT_GRE))
		return;

	while (tun_lvl) {
		if (phint & IAVF_PHINT_LAYERS_MSK)
			shift_count = 2;

		/* shift headers layer */
		for (i = proto_hdrs->count - 1 + shift_count;
		     i > shift_count - 1; i--) {
			hdr1 = &proto_hdrs->proto_hdr[i];
			hdr2 = &proto_hdrs->proto_hdr[i - shift_count];
			*hdr1 = *hdr2;
		}

		if (shift_count == 1) {
			/* adding tunnel header at layer 0 */
			hdr1 = &proto_hdrs->proto_hdr[0];
		} else {
			/* adding tunnel header and outer ip header */
			hdr1 = &proto_hdrs->proto_hdr[1];
			hdr2 = &proto_hdrs->proto_hdr[0];
			hdr2->field_selector = 0;
			proto_hdrs->count++;
			tun_lvl--;

			if (tun_lvl == TUNNEL_LEVEL_OUTER) {
				if (phint & IAVF_PHINT_OUTER_IPV4)
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr2, IPV4);
				else if (phint & IAVF_PHINT_OUTER_IPV6)
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr2, IPV6);
			} else if (tun_lvl == TUNNEL_LEVEL_INNER) {
				if (phint & IAVF_PHINT_MID_IPV4)
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr2, IPV4);
				else if (phint & IAVF_PHINT_MID_IPV6)
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr2, IPV6);
			}
		}

		hdr1->field_selector = 0;
		proto_hdrs->count++;

		if (phint & IAVF_PHINT_GTPU_EH_DWN)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GTPU_EH_PDU_DWN);
		else if (phint & IAVF_PHINT_GTPU_EH_UP)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GTPU_EH_PDU_UP);
		else if (phint & IAVF_PHINT_GTPU_EH)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GTPU_EH);
		else if (phint & IAVF_PHINT_GTPU)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GTPU_IP);

		if (phint & IAVF_PHINT_GRE) {
			if (phint & IAVF_PHINT_GTPU) {
				/* if GTPoGRE, add GRE header at the outer tunnel  */
				if (tun_lvl == TUNNEL_LEVEL_OUTER)
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GRE);
			} else {
					VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, GRE);
			}
		}
	}
	proto_hdrs->tunnel_level = tun_lvl;
}

static void
iavf_refine_proto_hdrs_l2tpv2(struct virtchnl_proto_hdrs *proto_hdrs,
			      uint64_t phint)
{
	struct virtchnl_proto_hdr *hdr, *hdr1;
	int i;

	if (!(phint & IAVF_PHINT_L2TPV2) && !(phint & IAVF_PHINT_L2TPV2_LEN))
		return;

	if (proto_hdrs->tunnel_level == TUNNEL_LEVEL_INNER) {
		/* shift headers layer */
		for (i = proto_hdrs->count - 1 + 1; i > 0; i--)
			proto_hdrs->proto_hdr[i] = proto_hdrs->proto_hdr[i - 1];

		/* adding outer ip header at layer 0 */
		hdr1 = &proto_hdrs->proto_hdr[0];
		hdr1->field_selector = 0;
		proto_hdrs->count++;
		proto_hdrs->tunnel_level = TUNNEL_LEVEL_OUTER;
		if (phint & IAVF_PHINT_OUTER_IPV4)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, IPV4);
		else if (phint & IAVF_PHINT_OUTER_IPV6)
			VIRTCHNL_SET_PROTO_HDR_TYPE(hdr1, IPV6);
	} else {
		for (i = 0; i < proto_hdrs->count; i++) {
			hdr = &proto_hdrs->proto_hdr[i];
			if (hdr->type == VIRTCHNL_PROTO_HDR_L2TPV2) {
				if (phint & IAVF_PHINT_L2TPV2) {
					REFINE_PROTO_FLD(DEL, L2TPV2_LEN_SESS_ID);
				} else if (phint & IAVF_PHINT_L2TPV2_LEN) {
					REFINE_PROTO_FLD(DEL, L2TPV2_SESS_ID);
				}
			}
		}
	}

}

static void iavf_refine_proto_hdrs(struct virtchnl_proto_hdrs *proto_hdrs,
				   uint64_t rss_type, uint64_t phint)
{
	iavf_refine_proto_hdrs_l234(proto_hdrs, rss_type);
	iavf_refine_proto_hdrs_by_pattern(proto_hdrs, phint);
	iavf_refine_proto_hdrs_gtpu(proto_hdrs, rss_type);
	iavf_refine_proto_hdrs_l2tpv2(proto_hdrs, phint);
}

static uint64_t invalid_rss_comb[] = {
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	RTE_ETH_RSS_L3_PRE32 | RTE_ETH_RSS_L3_PRE40 |
	RTE_ETH_RSS_L3_PRE48 | RTE_ETH_RSS_L3_PRE56 |
	RTE_ETH_RSS_L3_PRE96
};

struct rss_attr_type {
	uint64_t attr;
	uint64_t type;
};

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
				 RTE_ETH_RSS_L3_PRE64)

#define INVALID_RSS_ATTR	(RTE_ETH_RSS_L3_PRE32	| \
				 RTE_ETH_RSS_L3_PRE40	| \
				 RTE_ETH_RSS_L3_PRE48	| \
				 RTE_ETH_RSS_L3_PRE56	| \
				 RTE_ETH_RSS_L3_PRE96)

static struct rss_attr_type rss_attr_to_valid_type[] = {
	{RTE_ETH_RSS_L2_SRC_ONLY | RTE_ETH_RSS_L2_DST_ONLY,	RTE_ETH_RSS_ETH},
	{RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY,	VALID_RSS_L3},
	{RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY,	VALID_RSS_L4},
	/* current ipv6 prefix only supports prefix 64 bits*/
	{RTE_ETH_RSS_L3_PRE64,				VALID_RSS_IPV6},
	{INVALID_RSS_ATTR,				0}
};

static bool
iavf_any_invalid_rss_type(enum rte_eth_hash_function rss_func,
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
iavf_hash_parse_action(struct iavf_pattern_match_item *match_item,
		       const struct rte_flow_action actions[],
		       uint64_t pattern_hint, struct iavf_rss_meta *rss_meta,
		       struct rte_flow_error *error)
{
	struct virtchnl_proto_hdrs *proto_hdrs;
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

			if (rss->func ==
			    RTE_ETH_HASH_FUNCTION_SIMPLE_XOR){
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_XOR_ASYMMETRIC;
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"function simple_xor is not supported");
			} else if (rss->func ==
				   RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_TOEPLITZ_SYMMETRIC;
			} else {
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_TOEPLITZ_ASYMMETRIC;
			}

			if (rss->level)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS encapsulation level is not supported");

			if (rss->key_len)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a nonzero RSS key_len is not supported");

			if (rss->queue_num)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"a non-NULL RSS queue is not supported");

			/**
			 * Check simultaneous use of SRC_ONLY and DST_ONLY
			 * of the same level.
			 */
			rss_type = rte_eth_rss_hf_refine(rss_type);

			if (iavf_any_invalid_rss_type(rss->func, rss_type,
					match_item->input_set_mask))
				return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						action, "RSS type not supported");
			proto_hdrs = match_item->meta;
			rss_meta->proto_hdrs = *proto_hdrs;
			iavf_refine_proto_hdrs(&rss_meta->proto_hdrs,
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
iavf_hash_parse_pattern_action(__rte_unused struct iavf_adapter *ad,
			       struct iavf_pattern_match_item *array,
			       uint32_t array_len,
			       const struct rte_flow_item pattern[],
			       const struct rte_flow_action actions[],
			       void **meta,
			       struct rte_flow_error *error)
{
	struct iavf_pattern_match_item *pattern_match_item;
	struct iavf_rss_meta *rss_meta_ptr;
	uint64_t phint = IAVF_PHINT_NONE;
	int ret = 0;

	rss_meta_ptr = rte_zmalloc(NULL, sizeof(*rss_meta_ptr), 0);
	if (!rss_meta_ptr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for rss_meta_ptr");
		return -ENOMEM;
	}

	/* Check rss supported pattern and find matched pattern. */
	pattern_match_item =
		iavf_search_pattern_match_item(pattern, array, array_len,
					       error);
	if (!pattern_match_item) {
		ret = -rte_errno;
		goto error;
	}

	ret = iavf_hash_parse_pattern(pattern, &phint, error);
	if (ret)
		goto error;

	ret = iavf_hash_parse_action(pattern_match_item, actions, phint,
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
iavf_hash_create(__rte_unused struct iavf_adapter *ad,
		 __rte_unused struct rte_flow *flow, void *meta,
		 __rte_unused struct rte_flow_error *error)
{
	struct iavf_rss_meta *rss_meta = (struct iavf_rss_meta *)meta;
	struct virtchnl_rss_cfg *rss_cfg;
	int ret = 0;

	rss_cfg = rte_zmalloc("iavf rss rule",
			      sizeof(struct virtchnl_rss_cfg), 0);
	if (!rss_cfg) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "No memory for rss rule");
		return -ENOMEM;
	}

	rss_cfg->proto_hdrs = rss_meta->proto_hdrs;
	rss_cfg->rss_algorithm = rss_meta->rss_algorithm;

	ret = iavf_add_del_rss_cfg(ad, rss_cfg, true);
	if (!ret) {
		flow->rule = rss_cfg;
	} else {
		PMD_DRV_LOG(ERR, "fail to add RSS configure");
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to add rss rule.");
		rte_free(rss_cfg);
		return -rte_errno;
	}

	rte_free(meta);

	return ret;
}

static int
iavf_hash_destroy(__rte_unused struct iavf_adapter *ad,
		  struct rte_flow *flow,
		  __rte_unused struct rte_flow_error *error)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct virtchnl_rss_cfg *rss_cfg;
	int ret = 0;

	if (vf->vf_reset)
		return 0;

	rss_cfg = (struct virtchnl_rss_cfg *)flow->rule;

	ret = iavf_add_del_rss_cfg(ad, rss_cfg, false);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to del RSS configure");
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to delete rss rule.");
		return -rte_errno;
	}
	return ret;
}

static void
iavf_hash_uninit(struct iavf_adapter *ad)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(ad);
	struct rte_eth_rss_conf *rss_conf;

	if (vf->vf_reset)
		return;

	if (!vf->vf_res)
		return;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF))
		return;

	rss_conf = &ad->dev_data->dev_conf.rx_adv_conf.rss_conf;
	if (iavf_rss_hash_set(ad, rss_conf->rss_hf, false))
		PMD_DRV_LOG(ERR, "fail to delete default RSS");

	iavf_unregister_parser(&iavf_hash_parser, ad);
}

static void
iavf_hash_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
}
