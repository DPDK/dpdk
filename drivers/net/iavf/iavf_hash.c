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
#include <rte_ethdev_driver.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_eth_ctrl.h>
#include <rte_tailq.h>
#include <rte_flow_driver.h>

#include "iavf_log.h"
#include "iavf.h"
#include "iavf_generic_flow.h"

enum iavf_pattern_hint_type {
	IAVF_PATTERN_HINT_NONE,
	IAVF_PATTERN_HINT_IPV4,
	IAVF_PATTERN_HINT_IPV4_UDP,
	IAVF_PATTERN_HINT_IPV4_TCP,
	IAVF_PATTERN_HINT_IPV4_SCTP,
	IAVF_PATTERN_HINT_IPV6,
	IAVF_PATTERN_HINT_IPV6_UDP,
	IAVF_PATTERN_HINT_IPV6_TCP,
	IAVF_PATTERN_HINT_IPV6_SCTP,
};

struct iavf_pattern_match_type {
	enum iavf_pattern_hint_type phint_type;
};

struct iavf_hash_match_type {
	enum iavf_pattern_hint_type phint_type;
	uint64_t hash_type;
	struct virtchnl_proto_hdrs *proto_hdrs;
};

struct iavf_rss_meta {
	struct virtchnl_proto_hdrs *proto_hdrs;
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

struct iavf_pattern_match_type phint_empty = {
	IAVF_PATTERN_HINT_NONE};
struct iavf_pattern_match_type phint_eth_ipv4 = {
	IAVF_PATTERN_HINT_IPV4};
struct iavf_pattern_match_type phint_eth_ipv4_udp = {
	IAVF_PATTERN_HINT_IPV4_UDP};
struct iavf_pattern_match_type phint_eth_ipv4_tcp = {
	IAVF_PATTERN_HINT_IPV4_TCP};
struct iavf_pattern_match_type phint_eth_ipv4_sctp = {
	IAVF_PATTERN_HINT_IPV4_SCTP};
struct iavf_pattern_match_type phint_eth_ipv4_gtpu_eh = {
	IAVF_PATTERN_HINT_IPV4_UDP};
struct iavf_pattern_match_type phint_eth_ipv4_esp = {
	IAVF_PATTERN_HINT_IPV4};
struct iavf_pattern_match_type phint_eth_ipv4_ah = {
	IAVF_PATTERN_HINT_IPV4};
struct iavf_pattern_match_type phint_eth_ipv4_l2tpv3 = {
	IAVF_PATTERN_HINT_IPV4};
struct iavf_pattern_match_type phint_eth_ipv4_pfcp = {
	IAVF_PATTERN_HINT_IPV4_UDP};
struct iavf_pattern_match_type phint_eth_ipv6 = {
	IAVF_PATTERN_HINT_IPV6};
struct iavf_pattern_match_type phint_eth_ipv6_udp = {
	IAVF_PATTERN_HINT_IPV6_UDP};
struct iavf_pattern_match_type phint_eth_ipv6_tcp = {
	IAVF_PATTERN_HINT_IPV6_TCP};
struct iavf_pattern_match_type phint_eth_ipv6_sctp = {
	IAVF_PATTERN_HINT_IPV6_SCTP};
struct iavf_pattern_match_type phint_eth_ipv6_esp = {
	IAVF_PATTERN_HINT_IPV6};
struct iavf_pattern_match_type phint_eth_ipv6_ah = {
	IAVF_PATTERN_HINT_IPV6};
struct iavf_pattern_match_type phint_eth_ipv6_l2tpv3 = {
	IAVF_PATTERN_HINT_IPV6};
struct iavf_pattern_match_type phint_eth_ipv6_pfcp = {
	IAVF_PATTERN_HINT_IPV6_UDP};

/**
 * Supported pattern for hash.
 * The first member is pattern item type,
 * the second member is input set mask,
 * the third member is pattern hint for hash.
 */
static struct iavf_pattern_match_item iavf_hash_pattern_list[] = {
	{iavf_pattern_eth_ipv4, IAVF_INSET_NONE, &phint_eth_ipv4},
	{iavf_pattern_eth_ipv4_udp, IAVF_INSET_NONE, &phint_eth_ipv4_udp},
	{iavf_pattern_eth_ipv4_tcp, IAVF_INSET_NONE, &phint_eth_ipv4_tcp},
	{iavf_pattern_eth_ipv4_sctp, IAVF_INSET_NONE, &phint_eth_ipv4_sctp},
	{iavf_pattern_eth_ipv6, IAVF_INSET_NONE, &phint_eth_ipv6},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4, IAVF_INSET_NONE,
						&phint_eth_ipv4_gtpu_eh},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_udp, IAVF_INSET_NONE,
						&phint_eth_ipv4_gtpu_eh},
	{iavf_pattern_eth_ipv4_gtpu_eh_ipv4_tcp, IAVF_INSET_NONE,
						&phint_eth_ipv4_gtpu_eh},
	{iavf_pattern_eth_ipv4_esp, IAVF_INSET_NONE, &phint_eth_ipv4_esp},
	{iavf_pattern_eth_ipv4_ah, IAVF_INSET_NONE, &phint_eth_ipv4_ah},
	{iavf_pattern_eth_ipv4_l2tpv3, IAVF_INSET_NONE,
						&phint_eth_ipv4_l2tpv3},
	{iavf_pattern_eth_ipv4_pfcp, IAVF_INSET_NONE, &phint_eth_ipv4_pfcp},
	{iavf_pattern_eth_ipv6_udp, IAVF_INSET_NONE, &phint_eth_ipv6_udp},
	{iavf_pattern_eth_ipv6_tcp, IAVF_INSET_NONE, &phint_eth_ipv6_tcp},
	{iavf_pattern_eth_ipv6_sctp, IAVF_INSET_NONE, &phint_eth_ipv6_sctp},
	{iavf_pattern_eth_ipv6_esp, IAVF_INSET_NONE, &phint_eth_ipv6_esp},
	{iavf_pattern_eth_ipv6_ah, IAVF_INSET_NONE, &phint_eth_ipv6_ah},
	{iavf_pattern_eth_ipv6_l2tpv3, IAVF_INSET_NONE,
						&phint_eth_ipv6_l2tpv3},
	{iavf_pattern_eth_ipv6_pfcp, IAVF_INSET_NONE, &phint_eth_ipv6_pfcp},
	{iavf_pattern_empty, IAVF_INSET_NONE, &phint_empty},
};

#define	GTP_EH_PDU_LINK_UP		1
#define	GTP_EH_PDU_LINK_DWN		0

#define TUNNEL_LEVEL_OUTER		0
#define TUNNEL_LEVEL_FIRST_INNER	1

#define PROTO_COUNT_ONE			1
#define PROTO_COUNT_TWO			2
#define PROTO_COUNT_THREE		3

#define BUFF_NOUSED			0
#define FIELD_FOR_PROTO_ONLY		0

#define proto_hint_eth_src { \
	VIRTCHNL_PROTO_HDR_ETH, VIRTCHNL_PROTO_HDR_ETH_SRC, {BUFF_NOUSED } }

#define proto_hint_eth_dst { \
	VIRTCHNL_PROTO_HDR_ETH, VIRTCHNL_PROTO_HDR_ETH_DST, {BUFF_NOUSED } }

#define proto_hint_eth_only { \
	VIRTCHNL_PROTO_HDR_ETH, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_eth { \
	VIRTCHNL_PROTO_HDR_ETH, \
	VIRTCHNL_PROTO_HDR_ETH_SRC | VIRTCHNL_PROTO_HDR_ETH_DST, \
	{BUFF_NOUSED } }

#define proto_hint_svlan { \
	VIRTCHNL_PROTO_HDR_S_VLAN, VIRTCHNL_PROTO_HDR_S_VLAN_ID, \
	{BUFF_NOUSED } }

#define proto_hint_cvlan { \
	VIRTCHNL_PROTO_HDR_C_VLAN, VIRTCHNL_PROTO_HDR_C_VLAN_ID, \
	{BUFF_NOUSED } }

#define proto_hint_ipv4_src { \
	VIRTCHNL_PROTO_HDR_IPV4, VIRTCHNL_PROTO_HDR_IPV4_SRC, {BUFF_NOUSED } }

#define proto_hint_ipv4_dst { \
	VIRTCHNL_PROTO_HDR_IPV4, VIRTCHNL_PROTO_HDR_IPV4_DST, {BUFF_NOUSED } }

#define proto_hint_ipv4_only { \
	VIRTCHNL_PROTO_HDR_IPV4, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_ipv4 { \
	VIRTCHNL_PROTO_HDR_IPV4, \
	VIRTCHNL_PROTO_HDR_IPV4_SRC | VIRTCHNL_PROTO_HDR_IPV4_DST, \
	{BUFF_NOUSED } }

#define proto_hint_udp_src_port { \
	VIRTCHNL_PROTO_HDR_UDP, VIRTCHNL_PROTO_HDR_UDP_SRC_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_udp_dst_port { \
	VIRTCHNL_PROTO_HDR_UDP, VIRTCHNL_PROTO_HDR_UDP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_udp_only { \
	VIRTCHNL_PROTO_HDR_UDP, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_udp { \
	VIRTCHNL_PROTO_HDR_UDP, \
	VIRTCHNL_PROTO_HDR_UDP_SRC_PORT | VIRTCHNL_PROTO_HDR_UDP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_tcp_src_port { \
	VIRTCHNL_PROTO_HDR_TCP, VIRTCHNL_PROTO_HDR_TCP_SRC_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_tcp_dst_port { \
	VIRTCHNL_PROTO_HDR_TCP, VIRTCHNL_PROTO_HDR_TCP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_tcp_only { \
	VIRTCHNL_PROTO_HDR_TCP, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_tcp { \
	VIRTCHNL_PROTO_HDR_TCP, \
	VIRTCHNL_PROTO_HDR_TCP_SRC_PORT | VIRTCHNL_PROTO_HDR_TCP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_sctp_src_port { \
	VIRTCHNL_PROTO_HDR_SCTP, VIRTCHNL_PROTO_HDR_SCTP_SRC_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_sctp_dst_port { \
	VIRTCHNL_PROTO_HDR_SCTP, VIRTCHNL_PROTO_HDR_SCTP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_sctp_only { \
	VIRTCHNL_PROTO_HDR_SCTP, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_sctp { \
	VIRTCHNL_PROTO_HDR_SCTP, \
	VIRTCHNL_PROTO_HDR_SCTP_SRC_PORT | VIRTCHNL_PROTO_HDR_SCTP_DST_PORT, \
	{BUFF_NOUSED } }

#define proto_hint_ipv6_src { \
	VIRTCHNL_PROTO_HDR_IPV6, VIRTCHNL_PROTO_HDR_IPV6_SRC, {BUFF_NOUSED } }

#define proto_hint_ipv6_dst { \
	VIRTCHNL_PROTO_HDR_IPV6, VIRTCHNL_PROTO_HDR_IPV6_DST, {BUFF_NOUSED } }

#define proto_hint_ipv6_only { \
	VIRTCHNL_PROTO_HDR_IPV6, FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_ipv6 { \
	VIRTCHNL_PROTO_HDR_IPV6, \
	VIRTCHNL_PROTO_HDR_IPV6_SRC | VIRTCHNL_PROTO_HDR_IPV6_DST, \
	{BUFF_NOUSED } }

#define proto_hint_gtpu_up_only { \
	VIRTCHNL_PROTO_HDR_GTPU_EH_PDU_UP, \
	FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_gtpu_dwn_only { \
	VIRTCHNL_PROTO_HDR_GTPU_EH_PDU_DWN, \
	FIELD_FOR_PROTO_ONLY, {BUFF_NOUSED } }

#define proto_hint_esp { \
	VIRTCHNL_PROTO_HDR_ESP, \
	VIRTCHNL_PROTO_HDR_ESP_SPI, {BUFF_NOUSED } }

#define proto_hint_ah { \
	VIRTCHNL_PROTO_HDR_AH, \
	VIRTCHNL_PROTO_HDR_AH_SPI, {BUFF_NOUSED } }

#define proto_hint_l2tpv3 { \
	VIRTCHNL_PROTO_HDR_L2TPV3, \
	VIRTCHNL_PROTO_HDR_L2TPV3_SESS_ID, {BUFF_NOUSED } }

#define proto_hint_pfcp { \
	VIRTCHNL_PROTO_HDR_PFCP, VIRTCHNL_PROTO_HDR_PFCP_SEID, {BUFF_NOUSED } }

struct virtchnl_proto_hdrs hdrs_hint_eth_src = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_eth_src }
};

struct virtchnl_proto_hdrs hdrs_hint_eth_dst = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_eth_dst }
};

struct virtchnl_proto_hdrs hdrs_hint_eth = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_eth }
};

struct virtchnl_proto_hdrs hdrs_hint_svlan = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_svlan }
};

struct virtchnl_proto_hdrs hdrs_hint_cvlan = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_cvlan }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv4_src }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv4_dst }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_gtpu_up = {
	TUNNEL_LEVEL_FIRST_INNER, PROTO_COUNT_TWO, {proto_hint_gtpu_up_only,
	proto_hint_ipv4_src }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_gtpu_dwn = {
	TUNNEL_LEVEL_FIRST_INNER, PROTO_COUNT_TWO, {proto_hint_gtpu_dwn_only,
	proto_hint_ipv4_dst }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_esp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_esp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_ah = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_ah }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_l2tpv3 = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_l2tpv3 }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_pfcp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_pfcp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4 = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv4 }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_udp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4,
	proto_hint_udp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_tcp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4,
	proto_hint_tcp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_src_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_src,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_dst_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_dst,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4_only,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv4_sctp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv4,
	proto_hint_sctp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv6_src }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv6_dst }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_esp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_only,
	proto_hint_esp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_ah = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_only,
	proto_hint_ah }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_l2tpv3 = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_only,
	proto_hint_l2tpv3 }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_pfcp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_only,
	proto_hint_pfcp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6 = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_ONE, {proto_hint_ipv6 }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_udp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_udp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_udp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_udp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_udp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6,
	proto_hint_udp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_tcp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_tcp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_tcp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_tcp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_tcp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6,
	proto_hint_tcp }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_src_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_src,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_dst_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6_dst,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_sctp_src_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_sctp_src_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_sctp_dst_port = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_udp_only,
	proto_hint_sctp_dst_port }
};

struct virtchnl_proto_hdrs hdrs_hint_ipv6_sctp = {
	TUNNEL_LEVEL_OUTER, PROTO_COUNT_TWO, {proto_hint_ipv6,
	proto_hint_sctp }
};

/**
 * The first member is pattern hint type,
 * the second member is hash type,
 * the third member is virtchnl protocol hdrs.
 */
struct iavf_hash_match_type iavf_hash_type_list[] = {
	/* IPV4 */
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_L2_SRC_ONLY, &hdrs_hint_eth_src},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_L2_DST_ONLY, &hdrs_hint_eth_dst},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_ETH | ETH_RSS_L2_SRC_ONLY,
						&hdrs_hint_eth_src},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_ETH | ETH_RSS_L2_DST_ONLY,
						&hdrs_hint_eth_dst},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_ETH, &hdrs_hint_eth},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_S_VLAN, &hdrs_hint_svlan},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_C_VLAN, &hdrs_hint_cvlan},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_IPV4 | ETH_RSS_L3_SRC_ONLY,
						&hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_IPV4 | ETH_RSS_L3_DST_ONLY,
						&hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_ESP, &hdrs_hint_ipv4_esp},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_AH, &hdrs_hint_ipv4_ah},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_L2TPV3, &hdrs_hint_ipv4_l2tpv3},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_IPV4, &hdrs_hint_ipv4},
	/* IPV4 UDP */
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_src_udp_src_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_src_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_GTPU,
					&hdrs_hint_ipv4_src_gtpu_up},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_dst_udp_src_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_dst_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_DST_ONLY | ETH_RSS_GTPU,
					&hdrs_hint_ipv4_dst_gtpu_dwn},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_src_udp_src_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_src_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_GTPU,
		&hdrs_hint_ipv4_src_gtpu_up},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_dst_udp_src_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_dst_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_GTPU,
		&hdrs_hint_ipv4_dst_gtpu_dwn},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_udp_src_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_PFCP,
					&hdrs_hint_ipv4_pfcp},
	{IAVF_PATTERN_HINT_IPV4_UDP, ETH_RSS_NONFRAG_IPV4_UDP,
					&hdrs_hint_ipv4_udp},
	/* IPV4 TCP */
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_src_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_src_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_dst_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_dst_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_src_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_src_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_dst_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_dst_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_TCP, ETH_RSS_NONFRAG_IPV4_TCP,
					&hdrs_hint_ipv4_tcp},
	/* IPV4 SCTP */
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_src_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_src_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_dst_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_dst_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_src_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_src_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv4_src},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv4_dst_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv4_dst_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv4_dst},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv4_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv4_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV4_SCTP, ETH_RSS_NONFRAG_IPV4_SCTP,
					&hdrs_hint_ipv4_sctp},
	/* IPV6 */
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_L2_SRC_ONLY, &hdrs_hint_eth_src},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_L2_DST_ONLY, &hdrs_hint_eth_dst},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_IPV6 | ETH_RSS_L2_SRC_ONLY,
					&hdrs_hint_eth_src},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_IPV6 | ETH_RSS_L2_DST_ONLY,
					&hdrs_hint_eth_dst},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_ETH, &hdrs_hint_eth},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_S_VLAN, &hdrs_hint_svlan},
	{IAVF_PATTERN_HINT_IPV4, ETH_RSS_C_VLAN, &hdrs_hint_cvlan},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_IPV6 | ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_IPV6 | ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_ESP, &hdrs_hint_ipv6_esp},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_AH, &hdrs_hint_ipv6_ah},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_L2TPV3, &hdrs_hint_ipv6_l2tpv3},
	{IAVF_PATTERN_HINT_IPV6, ETH_RSS_IPV6, &hdrs_hint_ipv6},
	/* IPV6 UDP */
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_src_udp_src_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_src_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_dst_udp_src_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_dst_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_src_udp_src_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_src_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_dst_udp_src_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_dst_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_udp_src_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_udp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_PFCP,
					&hdrs_hint_ipv6_pfcp},
	{IAVF_PATTERN_HINT_IPV6_UDP, ETH_RSS_NONFRAG_IPV6_UDP,
					&hdrs_hint_ipv6_udp},
	/* IPV6 TCP */
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_src_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_src_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_dst_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_dst_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_src_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_src_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_dst_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_dst_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_tcp_src_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_tcp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_TCP, ETH_RSS_NONFRAG_IPV6_TCP,
					&hdrs_hint_ipv6_tcp},
	/* IPV6 SCTP */
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_src_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_src_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_SRC_ONLY,
					&hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_dst_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_dst_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L3_DST_ONLY,
					&hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_src_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_SRC_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_src_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_SRC_ONLY, &hdrs_hint_ipv6_src},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_SRC_ONLY,
		&hdrs_hint_ipv6_dst_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_DST_ONLY | ETH_RSS_L4_DST_ONLY,
		&hdrs_hint_ipv6_dst_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP |
		ETH_RSS_L3_DST_ONLY, &hdrs_hint_ipv6_dst},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L4_SRC_ONLY,
					&hdrs_hint_ipv6_sctp_src_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_L4_DST_ONLY,
					&hdrs_hint_ipv6_sctp_dst_port},
	{IAVF_PATTERN_HINT_IPV6_SCTP, ETH_RSS_NONFRAG_IPV6_SCTP,
					&hdrs_hint_ipv6_sctp},
};

struct virtchnl_proto_hdrs *iavf_hash_default_hdrs[] = {
	&hdrs_hint_ipv4,
	&hdrs_hint_ipv4_udp,
	&hdrs_hint_ipv4_tcp,
	&hdrs_hint_ipv4_sctp,
	&hdrs_hint_ipv6,
	&hdrs_hint_ipv6_udp,
	&hdrs_hint_ipv6_tcp,
	&hdrs_hint_ipv6_sctp,
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

static int
iavf_hash_default_set(struct iavf_adapter *ad, bool add)
{
	struct virtchnl_rss_cfg *rss_cfg;
	uint16_t i;
	int ret;

	rss_cfg = rte_zmalloc("iavf rss rule",
			      sizeof(struct virtchnl_rss_cfg), 0);
	if (!rss_cfg)
		return -ENOMEM;

	for (i = 0; i < RTE_DIM(iavf_hash_default_hdrs); i++) {
		rss_cfg->proto_hdrs = *iavf_hash_default_hdrs[i];
		rss_cfg->rss_algorithm = VIRTCHNL_RSS_ALG_TOEPLITZ_ASYMMETRIC;

		ret = iavf_add_del_rss_cfg(ad, rss_cfg, add);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to %s RSS configure",
				    add ? "add" : "delete");
			rte_free(rss_cfg);
			return ret;
		}
	}

	return ret;
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

	ret = iavf_hash_default_set(ad, true);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to set default RSS");
		iavf_unregister_parser(parser, ad);
	}

	return ret;
}

static int
iavf_hash_check_inset(const struct rte_flow_item pattern[],
		      struct rte_flow_error *error)
{
	const struct rte_flow_item *item = pattern;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Not support range");
			return -rte_errno;
		}
	}

	return 0;
}

static uint64_t
iavf_hash_refine_type(uint64_t rss_type, const struct rte_flow_item pattern[])
{
	const struct rte_flow_item *item;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_GTP_PSC) {
			const struct rte_flow_item_gtp_psc *psc = item->spec;

			if (psc && (psc->pdu_type == GTP_EH_PDU_LINK_UP ||
				    psc->pdu_type == GTP_EH_PDU_LINK_DWN)) {
				rss_type |= ETH_RSS_GTPU;
			}
		}
	}

	return rss_type;
}

static int
iavf_hash_parse_action(struct iavf_pattern_match_item *pattern_match_item,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       void **meta, struct rte_flow_error *error)
{
	struct iavf_rss_meta *rss_meta = (struct iavf_rss_meta *)*meta;
	uint32_t type_list_len = RTE_DIM(iavf_hash_type_list);
	struct iavf_hash_match_type *type_match_item;
	enum rte_flow_action_type action_type;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_action *action;
	bool item_found = false;
	uint64_t rss_type;
	uint16_t i;

	struct iavf_pattern_match_type *tt = (struct iavf_pattern_match_type *)
		(pattern_match_item->meta);

	/* Supported action is RSS. */
	for (action = actions; action->type !=
		RTE_FLOW_ACTION_TYPE_END; action++) {
		action_type = action->type;
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = action->conf;
			rss_type = rss->types;

			/**
			 * Check simultaneous use of SRC_ONLY and DST_ONLY
			 * of the same level.
			 */
			rss_type = rte_eth_rss_hf_refine(rss_type);

			/**
			 * Refine the hash type base on some specific item of
			 * the pattern, such as identify the gtpu hash.
			 */
			rss_type = iavf_hash_refine_type(rss_type, pattern);

			/* Check if pattern is empty. */
			if (pattern_match_item->pattern_list !=
				iavf_pattern_empty && rss->func ==
				RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Not supported flow");

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

			/* Check hash function and save it to rss_meta. */
			if (rss->func == RTE_ETH_HASH_FUNCTION_SIMPLE_XOR)
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_XOR_ASYMMETRIC;
			else if (rss->func ==
				 RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ)
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_TOEPLITZ_SYMMETRIC;
			else
				rss_meta->rss_algorithm =
					VIRTCHNL_RSS_ALG_TOEPLITZ_ASYMMETRIC;

			type_match_item =
			rte_zmalloc("iavf_type_match_item",
				    sizeof(struct iavf_hash_match_type), 0);
			if (!type_match_item) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_HANDLE,
						   NULL,
						   "No memory for type_match_item");
				return -ENOMEM;
			}

			/* Find matched proto hdrs according to hash type. */
			for (i = 0; i < type_list_len; i++) {
				struct iavf_hash_match_type *ht_map =
					&iavf_hash_type_list[i];
				if (rss_type == ht_map->hash_type &&
				    tt->phint_type == ht_map->phint_type) {
					type_match_item->hash_type =
						ht_map->hash_type;
					type_match_item->proto_hdrs =
						ht_map->proto_hdrs;
					rss_meta->proto_hdrs =
						type_match_item->proto_hdrs;
					item_found = true;
				}
			}

			rte_free(type_match_item);

			if (!item_found)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Not supported flow");
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

	ret = iavf_hash_check_inset(pattern, error);
	if (ret)
		goto error;

	/* Check rss action. */
	ret = iavf_hash_parse_action(pattern_match_item, pattern, actions,
				     (void **)&rss_meta_ptr, error);

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

	rss_cfg->proto_hdrs = *rss_meta->proto_hdrs;
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
	struct virtchnl_rss_cfg *rss_cfg;
	int ret = 0;

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
	if (iavf_hash_default_set(ad, false))
		PMD_DRV_LOG(ERR, "fail to delete default RSS");

	iavf_unregister_parser(&iavf_hash_parser, ad);
}

static void
iavf_hash_free(struct rte_flow *flow)
{
	rte_free(flow->rule);
}
