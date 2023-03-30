/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_byteorder.h>
#include <cmdline_parse.h>
#include <cmdline_parse_etheraddr.h>
#include <cmdline_parse_string.h>
#include <cmdline_parse_num.h>
#include <rte_flow.h>
#include <rte_hexdump.h>
#include <rte_vxlan.h>
#include <rte_gre.h>
#include <rte_mpls.h>
#include <rte_gtp.h>
#include <rte_geneve.h>

#include "testpmd.h"

/** Parser token indices. */
enum index {
	/* Special tokens. */
	ZERO = 0,
	END,
	START_SET,
	END_SET,

	/* Common tokens. */
	COMMON_INTEGER,
	COMMON_UNSIGNED,
	COMMON_PREFIX,
	COMMON_BOOLEAN,
	COMMON_STRING,
	COMMON_HEX,
	COMMON_FILE_PATH,
	COMMON_MAC_ADDR,
	COMMON_IPV4_ADDR,
	COMMON_IPV6_ADDR,
	COMMON_RULE_ID,
	COMMON_PORT_ID,
	COMMON_GROUP_ID,
	COMMON_PRIORITY_LEVEL,
	COMMON_INDIRECT_ACTION_ID,
	COMMON_PROFILE_ID,
	COMMON_POLICY_ID,
	COMMON_FLEX_HANDLE,
	COMMON_FLEX_TOKEN,
	COMMON_PATTERN_TEMPLATE_ID,
	COMMON_ACTIONS_TEMPLATE_ID,
	COMMON_TABLE_ID,
	COMMON_QUEUE_ID,

	/* TOP-level command. */
	ADD,

	/* Top-level command. */
	SET,
	/* Sub-leve commands. */
	SET_RAW_ENCAP,
	SET_RAW_DECAP,
	SET_RAW_INDEX,
	SET_SAMPLE_ACTIONS,
	SET_SAMPLE_INDEX,

	/* Top-level command. */
	FLOW,
	/* Sub-level commands. */
	INFO,
	CONFIGURE,
	PATTERN_TEMPLATE,
	ACTIONS_TEMPLATE,
	TABLE,
	INDIRECT_ACTION,
	VALIDATE,
	CREATE,
	DESTROY,
	FLUSH,
	DUMP,
	QUERY,
	LIST,
	AGED,
	ISOLATE,
	TUNNEL,
	FLEX,
	QUEUE,
	PUSH,
	PULL,

	/* Flex arguments */
	FLEX_ITEM_INIT,
	FLEX_ITEM_CREATE,
	FLEX_ITEM_DESTROY,

	/* Pattern template arguments. */
	PATTERN_TEMPLATE_CREATE,
	PATTERN_TEMPLATE_DESTROY,
	PATTERN_TEMPLATE_CREATE_ID,
	PATTERN_TEMPLATE_DESTROY_ID,
	PATTERN_TEMPLATE_RELAXED_MATCHING,
	PATTERN_TEMPLATE_INGRESS,
	PATTERN_TEMPLATE_EGRESS,
	PATTERN_TEMPLATE_TRANSFER,
	PATTERN_TEMPLATE_SPEC,

	/* Actions template arguments. */
	ACTIONS_TEMPLATE_CREATE,
	ACTIONS_TEMPLATE_DESTROY,
	ACTIONS_TEMPLATE_CREATE_ID,
	ACTIONS_TEMPLATE_DESTROY_ID,
	ACTIONS_TEMPLATE_INGRESS,
	ACTIONS_TEMPLATE_EGRESS,
	ACTIONS_TEMPLATE_TRANSFER,
	ACTIONS_TEMPLATE_SPEC,
	ACTIONS_TEMPLATE_MASK,

	/* Queue arguments. */
	QUEUE_CREATE,
	QUEUE_DESTROY,
	QUEUE_AGED,
	QUEUE_INDIRECT_ACTION,

	/* Queue create arguments. */
	QUEUE_CREATE_POSTPONE,
	QUEUE_TEMPLATE_TABLE,
	QUEUE_PATTERN_TEMPLATE,
	QUEUE_ACTIONS_TEMPLATE,
	QUEUE_RULE_ID,

	/* Queue destroy arguments. */
	QUEUE_DESTROY_ID,
	QUEUE_DESTROY_POSTPONE,

	/* Queue indirect action arguments */
	QUEUE_INDIRECT_ACTION_CREATE,
	QUEUE_INDIRECT_ACTION_UPDATE,
	QUEUE_INDIRECT_ACTION_DESTROY,
	QUEUE_INDIRECT_ACTION_QUERY,
	QUEUE_INDIRECT_ACTION_QUERY_UPDATE,

	/* Queue indirect action create arguments */
	QUEUE_INDIRECT_ACTION_CREATE_ID,
	QUEUE_INDIRECT_ACTION_INGRESS,
	QUEUE_INDIRECT_ACTION_EGRESS,
	QUEUE_INDIRECT_ACTION_TRANSFER,
	QUEUE_INDIRECT_ACTION_CREATE_POSTPONE,
	QUEUE_INDIRECT_ACTION_SPEC,

	/* Queue indirect action update arguments */
	QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE,

	/* Queue indirect action destroy arguments */
	QUEUE_INDIRECT_ACTION_DESTROY_ID,
	QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE,

	/* Queue indirect action query arguments */
	QUEUE_INDIRECT_ACTION_QUERY_POSTPONE,

	/* Queue indirect action query_update arguments */
	QUEUE_INDIRECT_ACTION_QU_MODE,

	/* Push arguments. */
	PUSH_QUEUE,

	/* Pull arguments. */
	PULL_QUEUE,

	/* Table arguments. */
	TABLE_CREATE,
	TABLE_DESTROY,
	TABLE_CREATE_ID,
	TABLE_DESTROY_ID,
	TABLE_INSERTION_TYPE,
	TABLE_INSERTION_TYPE_NAME,
	TABLE_HASH_FUNC,
	TABLE_HASH_FUNC_NAME,
	TABLE_GROUP,
	TABLE_PRIORITY,
	TABLE_INGRESS,
	TABLE_EGRESS,
	TABLE_TRANSFER,
	TABLE_TRANSFER_WIRE_ORIG,
	TABLE_TRANSFER_VPORT_ORIG,
	TABLE_RULES_NUMBER,
	TABLE_PATTERN_TEMPLATE,
	TABLE_ACTIONS_TEMPLATE,

	/* Tunnel arguments. */
	TUNNEL_CREATE,
	TUNNEL_CREATE_TYPE,
	TUNNEL_LIST,
	TUNNEL_DESTROY,
	TUNNEL_DESTROY_ID,

	/* Destroy arguments. */
	DESTROY_RULE,

	/* Query arguments. */
	QUERY_ACTION,

	/* List arguments. */
	LIST_GROUP,

	/* Destroy aged flow arguments. */
	AGED_DESTROY,

	/* Validate/create arguments. */
	VC_GROUP,
	VC_PRIORITY,
	VC_INGRESS,
	VC_EGRESS,
	VC_TRANSFER,
	VC_TUNNEL_SET,
	VC_TUNNEL_MATCH,

	/* Dump arguments */
	DUMP_ALL,
	DUMP_ONE,

	/* Configure arguments */
	CONFIG_QUEUES_NUMBER,
	CONFIG_QUEUES_SIZE,
	CONFIG_COUNTERS_NUMBER,
	CONFIG_AGING_OBJECTS_NUMBER,
	CONFIG_METERS_NUMBER,
	CONFIG_CONN_TRACK_NUMBER,
	CONFIG_QUOTAS_NUMBER,
	CONFIG_FLAGS,
	CONFIG_HOST_PORT,

	/* Indirect action arguments */
	INDIRECT_ACTION_CREATE,
	INDIRECT_ACTION_UPDATE,
	INDIRECT_ACTION_DESTROY,
	INDIRECT_ACTION_QUERY,
	INDIRECT_ACTION_QUERY_UPDATE,

	/* Indirect action create arguments */
	INDIRECT_ACTION_CREATE_ID,
	INDIRECT_ACTION_INGRESS,
	INDIRECT_ACTION_EGRESS,
	INDIRECT_ACTION_TRANSFER,
	INDIRECT_ACTION_SPEC,

	/* Indirect action destroy arguments */
	INDIRECT_ACTION_DESTROY_ID,

	/* Indirect action query-and-update arguments */
	INDIRECT_ACTION_QU_MODE,
	INDIRECT_ACTION_QU_MODE_NAME,

	/* Validate/create pattern. */
	ITEM_PATTERN,
	ITEM_PARAM_IS,
	ITEM_PARAM_SPEC,
	ITEM_PARAM_LAST,
	ITEM_PARAM_MASK,
	ITEM_PARAM_PREFIX,
	ITEM_NEXT,
	ITEM_END,
	ITEM_VOID,
	ITEM_INVERT,
	ITEM_ANY,
	ITEM_ANY_NUM,
	ITEM_PORT_ID,
	ITEM_PORT_ID_ID,
	ITEM_MARK,
	ITEM_MARK_ID,
	ITEM_RAW,
	ITEM_RAW_RELATIVE,
	ITEM_RAW_SEARCH,
	ITEM_RAW_OFFSET,
	ITEM_RAW_LIMIT,
	ITEM_RAW_PATTERN,
	ITEM_RAW_PATTERN_HEX,
	ITEM_ETH,
	ITEM_ETH_DST,
	ITEM_ETH_SRC,
	ITEM_ETH_TYPE,
	ITEM_ETH_HAS_VLAN,
	ITEM_VLAN,
	ITEM_VLAN_TCI,
	ITEM_VLAN_PCP,
	ITEM_VLAN_DEI,
	ITEM_VLAN_VID,
	ITEM_VLAN_INNER_TYPE,
	ITEM_VLAN_HAS_MORE_VLAN,
	ITEM_IPV4,
	ITEM_IPV4_VER_IHL,
	ITEM_IPV4_TOS,
	ITEM_IPV4_ID,
	ITEM_IPV4_FRAGMENT_OFFSET,
	ITEM_IPV4_TTL,
	ITEM_IPV4_PROTO,
	ITEM_IPV4_SRC,
	ITEM_IPV4_DST,
	ITEM_IPV6,
	ITEM_IPV6_TC,
	ITEM_IPV6_FLOW,
	ITEM_IPV6_PROTO,
	ITEM_IPV6_HOP,
	ITEM_IPV6_SRC,
	ITEM_IPV6_DST,
	ITEM_IPV6_HAS_FRAG_EXT,
	ITEM_IPV6_ROUTING_EXT,
	ITEM_IPV6_ROUTING_EXT_TYPE,
	ITEM_IPV6_ROUTING_EXT_NEXT_HDR,
	ITEM_IPV6_ROUTING_EXT_SEG_LEFT,
	ITEM_ICMP,
	ITEM_ICMP_TYPE,
	ITEM_ICMP_CODE,
	ITEM_ICMP_IDENT,
	ITEM_ICMP_SEQ,
	ITEM_UDP,
	ITEM_UDP_SRC,
	ITEM_UDP_DST,
	ITEM_TCP,
	ITEM_TCP_SRC,
	ITEM_TCP_DST,
	ITEM_TCP_FLAGS,
	ITEM_SCTP,
	ITEM_SCTP_SRC,
	ITEM_SCTP_DST,
	ITEM_SCTP_TAG,
	ITEM_SCTP_CKSUM,
	ITEM_VXLAN,
	ITEM_VXLAN_VNI,
	ITEM_VXLAN_LAST_RSVD,
	ITEM_E_TAG,
	ITEM_E_TAG_GRP_ECID_B,
	ITEM_NVGRE,
	ITEM_NVGRE_TNI,
	ITEM_MPLS,
	ITEM_MPLS_LABEL,
	ITEM_MPLS_TC,
	ITEM_MPLS_S,
	ITEM_GRE,
	ITEM_GRE_PROTO,
	ITEM_GRE_C_RSVD0_VER,
	ITEM_GRE_C_BIT,
	ITEM_GRE_K_BIT,
	ITEM_GRE_S_BIT,
	ITEM_FUZZY,
	ITEM_FUZZY_THRESH,
	ITEM_GTP,
	ITEM_GTP_FLAGS,
	ITEM_GTP_MSG_TYPE,
	ITEM_GTP_TEID,
	ITEM_GTPC,
	ITEM_GTPU,
	ITEM_GENEVE,
	ITEM_GENEVE_VNI,
	ITEM_GENEVE_PROTO,
	ITEM_GENEVE_OPTLEN,
	ITEM_VXLAN_GPE,
	ITEM_VXLAN_GPE_VNI,
	ITEM_ARP_ETH_IPV4,
	ITEM_ARP_ETH_IPV4_SHA,
	ITEM_ARP_ETH_IPV4_SPA,
	ITEM_ARP_ETH_IPV4_THA,
	ITEM_ARP_ETH_IPV4_TPA,
	ITEM_IPV6_EXT,
	ITEM_IPV6_EXT_NEXT_HDR,
	ITEM_IPV6_FRAG_EXT,
	ITEM_IPV6_FRAG_EXT_NEXT_HDR,
	ITEM_IPV6_FRAG_EXT_FRAG_DATA,
	ITEM_IPV6_FRAG_EXT_ID,
	ITEM_ICMP6,
	ITEM_ICMP6_TYPE,
	ITEM_ICMP6_CODE,
	ITEM_ICMP6_ECHO_REQUEST,
	ITEM_ICMP6_ECHO_REQUEST_ID,
	ITEM_ICMP6_ECHO_REQUEST_SEQ,
	ITEM_ICMP6_ECHO_REPLY,
	ITEM_ICMP6_ECHO_REPLY_ID,
	ITEM_ICMP6_ECHO_REPLY_SEQ,
	ITEM_ICMP6_ND_NS,
	ITEM_ICMP6_ND_NS_TARGET_ADDR,
	ITEM_ICMP6_ND_NA,
	ITEM_ICMP6_ND_NA_TARGET_ADDR,
	ITEM_ICMP6_ND_OPT,
	ITEM_ICMP6_ND_OPT_TYPE,
	ITEM_ICMP6_ND_OPT_SLA_ETH,
	ITEM_ICMP6_ND_OPT_SLA_ETH_SLA,
	ITEM_ICMP6_ND_OPT_TLA_ETH,
	ITEM_ICMP6_ND_OPT_TLA_ETH_TLA,
	ITEM_META,
	ITEM_META_DATA,
	ITEM_GRE_KEY,
	ITEM_GRE_KEY_VALUE,
	ITEM_GRE_OPTION,
	ITEM_GRE_OPTION_CHECKSUM,
	ITEM_GRE_OPTION_KEY,
	ITEM_GRE_OPTION_SEQUENCE,
	ITEM_GTP_PSC,
	ITEM_GTP_PSC_QFI,
	ITEM_GTP_PSC_PDU_T,
	ITEM_PPPOES,
	ITEM_PPPOED,
	ITEM_PPPOE_SEID,
	ITEM_PPPOE_PROTO_ID,
	ITEM_HIGIG2,
	ITEM_HIGIG2_CLASSIFICATION,
	ITEM_HIGIG2_VID,
	ITEM_TAG,
	ITEM_TAG_DATA,
	ITEM_TAG_INDEX,
	ITEM_L2TPV3OIP,
	ITEM_L2TPV3OIP_SESSION_ID,
	ITEM_ESP,
	ITEM_ESP_SPI,
	ITEM_AH,
	ITEM_AH_SPI,
	ITEM_PFCP,
	ITEM_PFCP_S_FIELD,
	ITEM_PFCP_SEID,
	ITEM_ECPRI,
	ITEM_ECPRI_COMMON,
	ITEM_ECPRI_COMMON_TYPE,
	ITEM_ECPRI_COMMON_TYPE_IQ_DATA,
	ITEM_ECPRI_COMMON_TYPE_RTC_CTRL,
	ITEM_ECPRI_COMMON_TYPE_DLY_MSR,
	ITEM_ECPRI_MSG_IQ_DATA_PCID,
	ITEM_ECPRI_MSG_RTC_CTRL_RTCID,
	ITEM_ECPRI_MSG_DLY_MSR_MSRID,
	ITEM_GENEVE_OPT,
	ITEM_GENEVE_OPT_CLASS,
	ITEM_GENEVE_OPT_TYPE,
	ITEM_GENEVE_OPT_LENGTH,
	ITEM_GENEVE_OPT_DATA,
	ITEM_INTEGRITY,
	ITEM_INTEGRITY_LEVEL,
	ITEM_INTEGRITY_VALUE,
	ITEM_CONNTRACK,
	ITEM_POL_PORT,
	ITEM_POL_METER,
	ITEM_POL_POLICY,
	ITEM_PORT_REPRESENTOR,
	ITEM_PORT_REPRESENTOR_PORT_ID,
	ITEM_REPRESENTED_PORT,
	ITEM_REPRESENTED_PORT_ETHDEV_PORT_ID,
	ITEM_FLEX,
	ITEM_FLEX_ITEM_HANDLE,
	ITEM_FLEX_PATTERN_HANDLE,
	ITEM_L2TPV2,
	ITEM_L2TPV2_TYPE,
	ITEM_L2TPV2_TYPE_DATA,
	ITEM_L2TPV2_TYPE_DATA_L,
	ITEM_L2TPV2_TYPE_DATA_S,
	ITEM_L2TPV2_TYPE_DATA_O,
	ITEM_L2TPV2_TYPE_DATA_L_S,
	ITEM_L2TPV2_TYPE_CTRL,
	ITEM_L2TPV2_MSG_DATA_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_L_LENGTH,
	ITEM_L2TPV2_MSG_DATA_L_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_L_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_S_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_S_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_S_NS,
	ITEM_L2TPV2_MSG_DATA_S_NR,
	ITEM_L2TPV2_MSG_DATA_O_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_O_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_O_OFFSET,
	ITEM_L2TPV2_MSG_DATA_L_S_LENGTH,
	ITEM_L2TPV2_MSG_DATA_L_S_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_L_S_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_L_S_NS,
	ITEM_L2TPV2_MSG_DATA_L_S_NR,
	ITEM_L2TPV2_MSG_CTRL_LENGTH,
	ITEM_L2TPV2_MSG_CTRL_TUNNEL_ID,
	ITEM_L2TPV2_MSG_CTRL_SESSION_ID,
	ITEM_L2TPV2_MSG_CTRL_NS,
	ITEM_L2TPV2_MSG_CTRL_NR,
	ITEM_PPP,
	ITEM_PPP_ADDR,
	ITEM_PPP_CTRL,
	ITEM_PPP_PROTO_ID,
	ITEM_METER,
	ITEM_METER_COLOR,
	ITEM_METER_COLOR_NAME,
	ITEM_QUOTA,
	ITEM_QUOTA_STATE,
	ITEM_QUOTA_STATE_NAME,
	ITEM_AGGR_AFFINITY,
	ITEM_AGGR_AFFINITY_VALUE,

	/* Validate/create actions. */
	ACTIONS,
	ACTION_NEXT,
	ACTION_END,
	ACTION_VOID,
	ACTION_PASSTHRU,
	ACTION_SKIP_CMAN,
	ACTION_JUMP,
	ACTION_JUMP_GROUP,
	ACTION_MARK,
	ACTION_MARK_ID,
	ACTION_FLAG,
	ACTION_QUEUE,
	ACTION_QUEUE_INDEX,
	ACTION_DROP,
	ACTION_COUNT,
	ACTION_COUNT_ID,
	ACTION_RSS,
	ACTION_RSS_FUNC,
	ACTION_RSS_LEVEL,
	ACTION_RSS_FUNC_DEFAULT,
	ACTION_RSS_FUNC_TOEPLITZ,
	ACTION_RSS_FUNC_SIMPLE_XOR,
	ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ,
	ACTION_RSS_TYPES,
	ACTION_RSS_TYPE,
	ACTION_RSS_KEY,
	ACTION_RSS_KEY_LEN,
	ACTION_RSS_QUEUES,
	ACTION_RSS_QUEUE,
	ACTION_PF,
	ACTION_VF,
	ACTION_VF_ORIGINAL,
	ACTION_VF_ID,
	ACTION_PORT_ID,
	ACTION_PORT_ID_ORIGINAL,
	ACTION_PORT_ID_ID,
	ACTION_METER,
	ACTION_METER_COLOR,
	ACTION_METER_COLOR_TYPE,
	ACTION_METER_COLOR_GREEN,
	ACTION_METER_COLOR_YELLOW,
	ACTION_METER_COLOR_RED,
	ACTION_METER_ID,
	ACTION_METER_MARK,
	ACTION_METER_PROFILE,
	ACTION_METER_PROFILE_ID2PTR,
	ACTION_METER_POLICY,
	ACTION_METER_POLICY_ID2PTR,
	ACTION_METER_COLOR_MODE,
	ACTION_METER_INIT_COLOR,
	ACTION_METER_STATE,
	ACTION_OF_DEC_NW_TTL,
	ACTION_OF_POP_VLAN,
	ACTION_OF_PUSH_VLAN,
	ACTION_OF_PUSH_VLAN_ETHERTYPE,
	ACTION_OF_SET_VLAN_VID,
	ACTION_OF_SET_VLAN_VID_VLAN_VID,
	ACTION_OF_SET_VLAN_PCP,
	ACTION_OF_SET_VLAN_PCP_VLAN_PCP,
	ACTION_OF_POP_MPLS,
	ACTION_OF_POP_MPLS_ETHERTYPE,
	ACTION_OF_PUSH_MPLS,
	ACTION_OF_PUSH_MPLS_ETHERTYPE,
	ACTION_VXLAN_ENCAP,
	ACTION_VXLAN_DECAP,
	ACTION_NVGRE_ENCAP,
	ACTION_NVGRE_DECAP,
	ACTION_L2_ENCAP,
	ACTION_L2_DECAP,
	ACTION_MPLSOGRE_ENCAP,
	ACTION_MPLSOGRE_DECAP,
	ACTION_MPLSOUDP_ENCAP,
	ACTION_MPLSOUDP_DECAP,
	ACTION_SET_IPV4_SRC,
	ACTION_SET_IPV4_SRC_IPV4_SRC,
	ACTION_SET_IPV4_DST,
	ACTION_SET_IPV4_DST_IPV4_DST,
	ACTION_SET_IPV6_SRC,
	ACTION_SET_IPV6_SRC_IPV6_SRC,
	ACTION_SET_IPV6_DST,
	ACTION_SET_IPV6_DST_IPV6_DST,
	ACTION_SET_TP_SRC,
	ACTION_SET_TP_SRC_TP_SRC,
	ACTION_SET_TP_DST,
	ACTION_SET_TP_DST_TP_DST,
	ACTION_MAC_SWAP,
	ACTION_DEC_TTL,
	ACTION_SET_TTL,
	ACTION_SET_TTL_TTL,
	ACTION_SET_MAC_SRC,
	ACTION_SET_MAC_SRC_MAC_SRC,
	ACTION_SET_MAC_DST,
	ACTION_SET_MAC_DST_MAC_DST,
	ACTION_INC_TCP_SEQ,
	ACTION_INC_TCP_SEQ_VALUE,
	ACTION_DEC_TCP_SEQ,
	ACTION_DEC_TCP_SEQ_VALUE,
	ACTION_INC_TCP_ACK,
	ACTION_INC_TCP_ACK_VALUE,
	ACTION_DEC_TCP_ACK,
	ACTION_DEC_TCP_ACK_VALUE,
	ACTION_RAW_ENCAP,
	ACTION_RAW_DECAP,
	ACTION_RAW_ENCAP_INDEX,
	ACTION_RAW_ENCAP_INDEX_VALUE,
	ACTION_RAW_DECAP_INDEX,
	ACTION_RAW_DECAP_INDEX_VALUE,
	ACTION_SET_TAG,
	ACTION_SET_TAG_DATA,
	ACTION_SET_TAG_INDEX,
	ACTION_SET_TAG_MASK,
	ACTION_SET_META,
	ACTION_SET_META_DATA,
	ACTION_SET_META_MASK,
	ACTION_SET_IPV4_DSCP,
	ACTION_SET_IPV4_DSCP_VALUE,
	ACTION_SET_IPV6_DSCP,
	ACTION_SET_IPV6_DSCP_VALUE,
	ACTION_AGE,
	ACTION_AGE_TIMEOUT,
	ACTION_AGE_UPDATE,
	ACTION_AGE_UPDATE_TIMEOUT,
	ACTION_AGE_UPDATE_TOUCH,
	ACTION_SAMPLE,
	ACTION_SAMPLE_RATIO,
	ACTION_SAMPLE_INDEX,
	ACTION_SAMPLE_INDEX_VALUE,
	ACTION_INDIRECT,
	ACTION_SHARED_INDIRECT,
	INDIRECT_ACTION_PORT,
	INDIRECT_ACTION_ID2PTR,
	ACTION_MODIFY_FIELD,
	ACTION_MODIFY_FIELD_OP,
	ACTION_MODIFY_FIELD_OP_VALUE,
	ACTION_MODIFY_FIELD_DST_TYPE,
	ACTION_MODIFY_FIELD_DST_TYPE_VALUE,
	ACTION_MODIFY_FIELD_DST_LEVEL,
	ACTION_MODIFY_FIELD_DST_LEVEL_VALUE,
	ACTION_MODIFY_FIELD_DST_OFFSET,
	ACTION_MODIFY_FIELD_SRC_TYPE,
	ACTION_MODIFY_FIELD_SRC_TYPE_VALUE,
	ACTION_MODIFY_FIELD_SRC_LEVEL,
	ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE,
	ACTION_MODIFY_FIELD_SRC_OFFSET,
	ACTION_MODIFY_FIELD_SRC_VALUE,
	ACTION_MODIFY_FIELD_SRC_POINTER,
	ACTION_MODIFY_FIELD_WIDTH,
	ACTION_CONNTRACK,
	ACTION_CONNTRACK_UPDATE,
	ACTION_CONNTRACK_UPDATE_DIR,
	ACTION_CONNTRACK_UPDATE_CTX,
	ACTION_POL_G,
	ACTION_POL_Y,
	ACTION_POL_R,
	ACTION_PORT_REPRESENTOR,
	ACTION_PORT_REPRESENTOR_PORT_ID,
	ACTION_REPRESENTED_PORT,
	ACTION_REPRESENTED_PORT_ETHDEV_PORT_ID,
	ACTION_SEND_TO_KERNEL,
	ACTION_QUOTA_CREATE,
	ACTION_QUOTA_CREATE_LIMIT,
	ACTION_QUOTA_CREATE_MODE,
	ACTION_QUOTA_CREATE_MODE_NAME,
	ACTION_QUOTA_QU,
	ACTION_QUOTA_QU_LIMIT,
	ACTION_QUOTA_QU_UPDATE_OP,
	ACTION_QUOTA_QU_UPDATE_OP_NAME,
};

/** Maximum size for pattern in struct rte_flow_item_raw. */
#define ITEM_RAW_PATTERN_SIZE 512

/** Maximum size for GENEVE option data pattern in bytes. */
#define ITEM_GENEVE_OPT_DATA_SIZE 124

/** Storage size for struct rte_flow_item_raw including pattern. */
#define ITEM_RAW_SIZE \
	(sizeof(struct rte_flow_item_raw) + ITEM_RAW_PATTERN_SIZE)

/** Maximum size for external pattern in struct rte_flow_action_modify_data. */
#define ACTION_MODIFY_PATTERN_SIZE 32

/** Storage size for struct rte_flow_action_modify_field including pattern. */
#define ACTION_MODIFY_SIZE \
	(sizeof(struct rte_flow_action_modify_field) + \
	ACTION_MODIFY_PATTERN_SIZE)

/** Maximum number of queue indices in struct rte_flow_action_rss. */
#define ACTION_RSS_QUEUE_NUM 128

/** Storage for struct rte_flow_action_rss including external data. */
struct action_rss_data {
	struct rte_flow_action_rss conf;
	uint8_t key[RSS_HASH_KEY_LENGTH];
	uint16_t queue[ACTION_RSS_QUEUE_NUM];
};

/** Maximum data size in struct rte_flow_action_raw_encap. */
#define ACTION_RAW_ENCAP_MAX_DATA 512
#define RAW_ENCAP_CONFS_MAX_NUM 8

/** Storage for struct rte_flow_action_raw_encap. */
struct raw_encap_conf {
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint8_t preserve[ACTION_RAW_ENCAP_MAX_DATA];
	size_t size;
};

struct raw_encap_conf raw_encap_confs[RAW_ENCAP_CONFS_MAX_NUM];

/** Storage for struct rte_flow_action_raw_encap including external data. */
struct action_raw_encap_data {
	struct rte_flow_action_raw_encap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint8_t preserve[ACTION_RAW_ENCAP_MAX_DATA];
	uint16_t idx;
};

/** Storage for struct rte_flow_action_raw_decap. */
struct raw_decap_conf {
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	size_t size;
};

struct raw_decap_conf raw_decap_confs[RAW_ENCAP_CONFS_MAX_NUM];

/** Storage for struct rte_flow_action_raw_decap including external data. */
struct action_raw_decap_data {
	struct rte_flow_action_raw_decap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint16_t idx;
};

struct vxlan_encap_conf vxlan_encap_conf = {
	.select_ipv4 = 1,
	.select_vlan = 0,
	.select_tos_ttl = 0,
	.vni = "\x00\x00\x00",
	.udp_src = 0,
	.udp_dst = RTE_BE16(RTE_VXLAN_DEFAULT_PORT),
	.ipv4_src = RTE_IPV4(127, 0, 0, 1),
	.ipv4_dst = RTE_IPV4(255, 255, 255, 255),
	.ipv6_src = "\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x01",
	.ipv6_dst = "\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x11\x11",
	.vlan_tci = 0,
	.ip_tos = 0,
	.ip_ttl = 255,
	.eth_src = "\x00\x00\x00\x00\x00\x00",
	.eth_dst = "\xff\xff\xff\xff\xff\xff",
};

/** Maximum number of items in struct rte_flow_action_vxlan_encap. */
#define ACTION_VXLAN_ENCAP_ITEMS_NUM 6

/** Storage for struct rte_flow_action_vxlan_encap including external data. */
struct action_vxlan_encap_data {
	struct rte_flow_action_vxlan_encap conf;
	struct rte_flow_item items[ACTION_VXLAN_ENCAP_ITEMS_NUM];
	struct rte_flow_item_eth item_eth;
	struct rte_flow_item_vlan item_vlan;
	union {
		struct rte_flow_item_ipv4 item_ipv4;
		struct rte_flow_item_ipv6 item_ipv6;
	};
	struct rte_flow_item_udp item_udp;
	struct rte_flow_item_vxlan item_vxlan;
};

struct nvgre_encap_conf nvgre_encap_conf = {
	.select_ipv4 = 1,
	.select_vlan = 0,
	.tni = "\x00\x00\x00",
	.ipv4_src = RTE_IPV4(127, 0, 0, 1),
	.ipv4_dst = RTE_IPV4(255, 255, 255, 255),
	.ipv6_src = "\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x01",
	.ipv6_dst = "\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x11\x11",
	.vlan_tci = 0,
	.eth_src = "\x00\x00\x00\x00\x00\x00",
	.eth_dst = "\xff\xff\xff\xff\xff\xff",
};

/** Maximum number of items in struct rte_flow_action_nvgre_encap. */
#define ACTION_NVGRE_ENCAP_ITEMS_NUM 5

/** Storage for struct rte_flow_action_nvgre_encap including external data. */
struct action_nvgre_encap_data {
	struct rte_flow_action_nvgre_encap conf;
	struct rte_flow_item items[ACTION_NVGRE_ENCAP_ITEMS_NUM];
	struct rte_flow_item_eth item_eth;
	struct rte_flow_item_vlan item_vlan;
	union {
		struct rte_flow_item_ipv4 item_ipv4;
		struct rte_flow_item_ipv6 item_ipv6;
	};
	struct rte_flow_item_nvgre item_nvgre;
};

struct l2_encap_conf l2_encap_conf;

struct l2_decap_conf l2_decap_conf;

struct mplsogre_encap_conf mplsogre_encap_conf;

struct mplsogre_decap_conf mplsogre_decap_conf;

struct mplsoudp_encap_conf mplsoudp_encap_conf;

struct mplsoudp_decap_conf mplsoudp_decap_conf;

struct rte_flow_action_conntrack conntrack_context;

#define ACTION_SAMPLE_ACTIONS_NUM 10
#define RAW_SAMPLE_CONFS_MAX_NUM 8
/** Storage for struct rte_flow_action_sample including external data. */
struct action_sample_data {
	struct rte_flow_action_sample conf;
	uint32_t idx;
};
/** Storage for struct rte_flow_action_sample. */
struct raw_sample_conf {
	struct rte_flow_action data[ACTION_SAMPLE_ACTIONS_NUM];
};
struct raw_sample_conf raw_sample_confs[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_mark sample_mark[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_queue sample_queue[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_count sample_count[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_port_id sample_port_id[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_raw_encap sample_encap[RAW_SAMPLE_CONFS_MAX_NUM];
struct action_vxlan_encap_data sample_vxlan_encap[RAW_SAMPLE_CONFS_MAX_NUM];
struct action_nvgre_encap_data sample_nvgre_encap[RAW_SAMPLE_CONFS_MAX_NUM];
struct action_rss_data sample_rss_data[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_vf sample_vf[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_ethdev sample_port_representor[RAW_SAMPLE_CONFS_MAX_NUM];
struct rte_flow_action_ethdev sample_represented_port[RAW_SAMPLE_CONFS_MAX_NUM];

static const char *const modify_field_ops[] = {
	"set", "add", "sub", NULL
};

static const char *const modify_field_ids[] = {
	"start", "mac_dst", "mac_src",
	"vlan_type", "vlan_id", "mac_type",
	"ipv4_dscp", "ipv4_ttl", "ipv4_src", "ipv4_dst",
	"ipv6_dscp", "ipv6_hoplimit", "ipv6_src", "ipv6_dst",
	"tcp_port_src", "tcp_port_dst",
	"tcp_seq_num", "tcp_ack_num", "tcp_flags",
	"udp_port_src", "udp_port_dst",
	"vxlan_vni", "geneve_vni", "gtp_teid",
	"tag", "mark", "meta", "pointer", "value",
	"ipv4_ecn", "ipv6_ecn", "gtp_psc_qfi", "meter_color",
	"ipv6_proto",
	"flex_item",
	"hash_result", NULL
};

static const char *const meter_colors[] = {
	"green", "yellow", "red", "all", NULL
};

static const char *const table_insertion_types[] = {
	"pattern", "index", NULL
};

static const char *const table_hash_funcs[] = {
	"default", "linear", "crc32", "crc16", NULL
};

#define RAW_IPSEC_CONFS_MAX_NUM 8

/** Maximum number of subsequent tokens and arguments on the stack. */
#define CTX_STACK_SIZE 16

/** Parser context. */
struct context {
	/** Stack of subsequent token lists to process. */
	const enum index *next[CTX_STACK_SIZE];
	/** Arguments for stacked tokens. */
	const void *args[CTX_STACK_SIZE];
	enum index curr; /**< Current token index. */
	enum index prev; /**< Index of the last token seen. */
	int next_num; /**< Number of entries in next[]. */
	int args_num; /**< Number of entries in args[]. */
	uint32_t eol:1; /**< EOL has been detected. */
	uint32_t last:1; /**< No more arguments. */
	portid_t port; /**< Current port ID (for completions). */
	uint32_t objdata; /**< Object-specific data. */
	void *object; /**< Address of current object for relative offsets. */
	void *objmask; /**< Object a full mask must be written to. */
};

/** Token argument. */
struct arg {
	uint32_t hton:1; /**< Use network byte ordering. */
	uint32_t sign:1; /**< Value is signed. */
	uint32_t bounded:1; /**< Value is bounded. */
	uintmax_t min; /**< Minimum value if bounded. */
	uintmax_t max; /**< Maximum value if bounded. */
	uint32_t offset; /**< Relative offset from ctx->object. */
	uint32_t size; /**< Field size. */
	const uint8_t *mask; /**< Bit-mask to use instead of offset/size. */
};

/** Parser token definition. */
struct token {
	/** Type displayed during completion (defaults to "TOKEN"). */
	const char *type;
	/** Help displayed during completion (defaults to token name). */
	const char *help;
	/** Private data used by parser functions. */
	const void *priv;
	/**
	 * Lists of subsequent tokens to push on the stack. Each call to the
	 * parser consumes the last entry of that stack.
	 */
	const enum index *const *next;
	/** Arguments stack for subsequent tokens that need them. */
	const struct arg *const *args;
	/**
	 * Token-processing callback, returns -1 in case of error, the
	 * length of the matched string otherwise. If NULL, attempts to
	 * match the token name.
	 *
	 * If buf is not NULL, the result should be stored in it according
	 * to context. An error is returned if not large enough.
	 */
	int (*call)(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size);
	/**
	 * Callback that provides possible values for this token, used for
	 * completion. Returns -1 in case of error, the number of possible
	 * values otherwise. If NULL, the token name is used.
	 *
	 * If buf is not NULL, entry index ent is written to buf and the
	 * full length of the entry is returned (same behavior as
	 * snprintf()).
	 */
	int (*comp)(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size);
	/** Mandatory token name, no default value. */
	const char *name;
};

/** Static initializer for the next field. */
#define NEXT(...) (const enum index *const []){ __VA_ARGS__, NULL, }

/** Static initializer for a NEXT() entry. */
#define NEXT_ENTRY(...) (const enum index []){ __VA_ARGS__, ZERO, }

/** Static initializer for the args field. */
#define ARGS(...) (const struct arg *const []){ __VA_ARGS__, NULL, }

/** Static initializer for ARGS() to target a field. */
#define ARGS_ENTRY(s, f) \
	(&(const struct arg){ \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Static initializer for ARGS() to target a bit-field. */
#define ARGS_ENTRY_BF(s, f, b) \
	(&(const struct arg){ \
		.size = sizeof(s), \
		.mask = (const void *)&(const s){ .f = (1 << (b)) - 1 }, \
	})

/** Static initializer for ARGS() to target a field with limits. */
#define ARGS_ENTRY_BOUNDED(s, f, i, a) \
	(&(const struct arg){ \
		.bounded = 1, \
		.min = (i), \
		.max = (a), \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Static initializer for ARGS() to target an arbitrary bit-mask. */
#define ARGS_ENTRY_MASK(s, f, m) \
	(&(const struct arg){ \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
		.mask = (const void *)(m), \
	})

/** Same as ARGS_ENTRY_MASK() using network byte ordering for the value. */
#define ARGS_ENTRY_MASK_HTON(s, f, m) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
		.mask = (const void *)(m), \
	})

/** Static initializer for ARGS() to target a pointer. */
#define ARGS_ENTRY_PTR(s, f) \
	(&(const struct arg){ \
		.size = sizeof(*((s *)0)->f), \
	})

/** Static initializer for ARGS() with arbitrary offset and size. */
#define ARGS_ENTRY_ARB(o, s) \
	(&(const struct arg){ \
		.offset = (o), \
		.size = (s), \
	})

/** Same as ARGS_ENTRY_ARB() with bounded values. */
#define ARGS_ENTRY_ARB_BOUNDED(o, s, i, a) \
	(&(const struct arg){ \
		.bounded = 1, \
		.min = (i), \
		.max = (a), \
		.offset = (o), \
		.size = (s), \
	})

/** Same as ARGS_ENTRY() using network byte ordering. */
#define ARGS_ENTRY_HTON(s, f) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = offsetof(s, f), \
		.size = sizeof(((s *)0)->f), \
	})

/** Same as ARGS_ENTRY_HTON() for a single argument, without structure. */
#define ARG_ENTRY_HTON(s) \
	(&(const struct arg){ \
		.hton = 1, \
		.offset = 0, \
		.size = sizeof(s), \
	})

/** Parser output buffer layout expected by cmd_flow_parsed(). */
struct buffer {
	enum index command; /**< Flow command. */
	portid_t port; /**< Affected port ID. */
	queueid_t queue; /** Async queue ID. */
	bool postpone; /** Postpone async operation */
	union {
		struct {
			struct rte_flow_port_attr port_attr;
			uint32_t nb_queue;
			struct rte_flow_queue_attr queue_attr;
		} configure; /**< Configuration arguments. */
		struct {
			uint32_t *template_id;
			uint32_t template_id_n;
		} templ_destroy; /**< Template destroy arguments. */
		struct {
			uint32_t id;
			struct rte_flow_template_table_attr attr;
			uint32_t *pat_templ_id;
			uint32_t pat_templ_id_n;
			uint32_t *act_templ_id;
			uint32_t act_templ_id_n;
		} table; /**< Table arguments. */
		struct {
			uint32_t *table_id;
			uint32_t table_id_n;
		} table_destroy; /**< Template destroy arguments. */
		struct {
			uint32_t *action_id;
			uint32_t action_id_n;
		} ia_destroy; /**< Indirect action destroy arguments. */
		struct {
			uint32_t action_id;
			enum rte_flow_query_update_mode qu_mode;
		} ia; /* Indirect action query arguments */
		struct {
			uint32_t table_id;
			uint32_t pat_templ_id;
			uint32_t rule_id;
			uint32_t act_templ_id;
			struct rte_flow_attr attr;
			struct tunnel_ops tunnel_ops;
			struct rte_flow_item *pattern;
			struct rte_flow_action *actions;
			struct rte_flow_action *masks;
			uint32_t pattern_n;
			uint32_t actions_n;
			uint8_t *data;
		} vc; /**< Validate/create arguments. */
		struct {
			uint32_t *rule;
			uint32_t rule_n;
		} destroy; /**< Destroy arguments. */
		struct {
			char file[128];
			bool mode;
			uint32_t rule;
		} dump; /**< Dump arguments. */
		struct {
			uint32_t rule;
			struct rte_flow_action action;
		} query; /**< Query arguments. */
		struct {
			uint32_t *group;
			uint32_t group_n;
		} list; /**< List arguments. */
		struct {
			int set;
		} isolate; /**< Isolated mode arguments. */
		struct {
			int destroy;
		} aged; /**< Aged arguments. */
		struct {
			uint32_t policy_id;
		} policy;/**< Policy arguments. */
		struct {
			uint16_t token;
			uintptr_t uintptr;
			char filename[128];
		} flex; /**< Flex arguments*/
	} args; /**< Command arguments. */
};

/** Private data for pattern items. */
struct parse_item_priv {
	enum rte_flow_item_type type; /**< Item type. */
	uint32_t size; /**< Size of item specification structure. */
};

#define PRIV_ITEM(t, s) \
	(&(const struct parse_item_priv){ \
		.type = RTE_FLOW_ITEM_TYPE_ ## t, \
		.size = s, \
	})

/** Private data for actions. */
struct parse_action_priv {
	enum rte_flow_action_type type; /**< Action type. */
	uint32_t size; /**< Size of action configuration structure. */
};

#define PRIV_ACTION(t, s) \
	(&(const struct parse_action_priv){ \
		.type = RTE_FLOW_ACTION_TYPE_ ## t, \
		.size = s, \
	})

static const enum index next_flex_item[] = {
	FLEX_ITEM_INIT,
	FLEX_ITEM_CREATE,
	FLEX_ITEM_DESTROY,
	ZERO,
};

static const enum index next_config_attr[] = {
	CONFIG_QUEUES_NUMBER,
	CONFIG_QUEUES_SIZE,
	CONFIG_COUNTERS_NUMBER,
	CONFIG_AGING_OBJECTS_NUMBER,
	CONFIG_METERS_NUMBER,
	CONFIG_CONN_TRACK_NUMBER,
	CONFIG_QUOTAS_NUMBER,
	CONFIG_FLAGS,
	CONFIG_HOST_PORT,
	END,
	ZERO,
};

static const enum index next_pt_subcmd[] = {
	PATTERN_TEMPLATE_CREATE,
	PATTERN_TEMPLATE_DESTROY,
	ZERO,
};

static const enum index next_pt_attr[] = {
	PATTERN_TEMPLATE_CREATE_ID,
	PATTERN_TEMPLATE_RELAXED_MATCHING,
	PATTERN_TEMPLATE_INGRESS,
	PATTERN_TEMPLATE_EGRESS,
	PATTERN_TEMPLATE_TRANSFER,
	PATTERN_TEMPLATE_SPEC,
	ZERO,
};

static const enum index next_pt_destroy_attr[] = {
	PATTERN_TEMPLATE_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_at_subcmd[] = {
	ACTIONS_TEMPLATE_CREATE,
	ACTIONS_TEMPLATE_DESTROY,
	ZERO,
};

static const enum index next_at_attr[] = {
	ACTIONS_TEMPLATE_CREATE_ID,
	ACTIONS_TEMPLATE_INGRESS,
	ACTIONS_TEMPLATE_EGRESS,
	ACTIONS_TEMPLATE_TRANSFER,
	ACTIONS_TEMPLATE_SPEC,
	ZERO,
};

static const enum index next_at_destroy_attr[] = {
	ACTIONS_TEMPLATE_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_table_subcmd[] = {
	TABLE_CREATE,
	TABLE_DESTROY,
	ZERO,
};

static const enum index next_table_attr[] = {
	TABLE_CREATE_ID,
	TABLE_GROUP,
	TABLE_INSERTION_TYPE,
	TABLE_HASH_FUNC,
	TABLE_PRIORITY,
	TABLE_INGRESS,
	TABLE_EGRESS,
	TABLE_TRANSFER,
	TABLE_TRANSFER_WIRE_ORIG,
	TABLE_TRANSFER_VPORT_ORIG,
	TABLE_RULES_NUMBER,
	TABLE_PATTERN_TEMPLATE,
	TABLE_ACTIONS_TEMPLATE,
	END,
	ZERO,
};

static const enum index next_table_destroy_attr[] = {
	TABLE_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_queue_subcmd[] = {
	QUEUE_CREATE,
	QUEUE_DESTROY,
	QUEUE_AGED,
	QUEUE_INDIRECT_ACTION,
	ZERO,
};

static const enum index next_queue_destroy_attr[] = {
	QUEUE_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_qia_subcmd[] = {
	QUEUE_INDIRECT_ACTION_CREATE,
	QUEUE_INDIRECT_ACTION_UPDATE,
	QUEUE_INDIRECT_ACTION_DESTROY,
	QUEUE_INDIRECT_ACTION_QUERY,
	QUEUE_INDIRECT_ACTION_QUERY_UPDATE,
	ZERO,
};

static const enum index next_qia_create_attr[] = {
	QUEUE_INDIRECT_ACTION_CREATE_ID,
	QUEUE_INDIRECT_ACTION_INGRESS,
	QUEUE_INDIRECT_ACTION_EGRESS,
	QUEUE_INDIRECT_ACTION_TRANSFER,
	QUEUE_INDIRECT_ACTION_CREATE_POSTPONE,
	QUEUE_INDIRECT_ACTION_SPEC,
	ZERO,
};

static const enum index next_qia_update_attr[] = {
	QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE,
	QUEUE_INDIRECT_ACTION_SPEC,
	ZERO,
};

static const enum index next_qia_destroy_attr[] = {
	QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE,
	QUEUE_INDIRECT_ACTION_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_qia_query_attr[] = {
	QUEUE_INDIRECT_ACTION_QUERY_POSTPONE,
	END,
	ZERO,
};

static const enum index next_ia_create_attr[] = {
	INDIRECT_ACTION_CREATE_ID,
	INDIRECT_ACTION_INGRESS,
	INDIRECT_ACTION_EGRESS,
	INDIRECT_ACTION_TRANSFER,
	INDIRECT_ACTION_SPEC,
	ZERO,
};

static const enum index next_ia[] = {
	INDIRECT_ACTION_ID2PTR,
	ACTION_NEXT,
	ZERO
};

static const enum index next_qia_qu_attr[] = {
	QUEUE_INDIRECT_ACTION_QU_MODE,
	QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE,
	INDIRECT_ACTION_SPEC,
	ZERO
};

static const enum index next_ia_qu_attr[] = {
	INDIRECT_ACTION_QU_MODE,
	INDIRECT_ACTION_SPEC,
	ZERO
};

static const enum index next_dump_subcmd[] = {
	DUMP_ALL,
	DUMP_ONE,
	ZERO,
};

static const enum index next_ia_subcmd[] = {
	INDIRECT_ACTION_CREATE,
	INDIRECT_ACTION_UPDATE,
	INDIRECT_ACTION_DESTROY,
	INDIRECT_ACTION_QUERY,
	INDIRECT_ACTION_QUERY_UPDATE,
	ZERO,
};

static const enum index next_vc_attr[] = {
	VC_GROUP,
	VC_PRIORITY,
	VC_INGRESS,
	VC_EGRESS,
	VC_TRANSFER,
	VC_TUNNEL_SET,
	VC_TUNNEL_MATCH,
	ITEM_PATTERN,
	ZERO,
};

static const enum index next_destroy_attr[] = {
	DESTROY_RULE,
	END,
	ZERO,
};

static const enum index next_dump_attr[] = {
	COMMON_FILE_PATH,
	END,
	ZERO,
};

static const enum index next_list_attr[] = {
	LIST_GROUP,
	END,
	ZERO,
};

static const enum index next_aged_attr[] = {
	AGED_DESTROY,
	END,
	ZERO,
};

static const enum index next_ia_destroy_attr[] = {
	INDIRECT_ACTION_DESTROY_ID,
	END,
	ZERO,
};

static const enum index next_async_insert_subcmd[] = {
	QUEUE_PATTERN_TEMPLATE,
	QUEUE_RULE_ID,
	ZERO,
};

static const enum index item_param[] = {
	ITEM_PARAM_IS,
	ITEM_PARAM_SPEC,
	ITEM_PARAM_LAST,
	ITEM_PARAM_MASK,
	ITEM_PARAM_PREFIX,
	ZERO,
};

static const enum index next_item[] = {
	ITEM_END,
	ITEM_VOID,
	ITEM_INVERT,
	ITEM_ANY,
	ITEM_PORT_ID,
	ITEM_MARK,
	ITEM_RAW,
	ITEM_ETH,
	ITEM_VLAN,
	ITEM_IPV4,
	ITEM_IPV6,
	ITEM_ICMP,
	ITEM_UDP,
	ITEM_TCP,
	ITEM_SCTP,
	ITEM_VXLAN,
	ITEM_E_TAG,
	ITEM_NVGRE,
	ITEM_MPLS,
	ITEM_GRE,
	ITEM_FUZZY,
	ITEM_GTP,
	ITEM_GTPC,
	ITEM_GTPU,
	ITEM_GENEVE,
	ITEM_VXLAN_GPE,
	ITEM_ARP_ETH_IPV4,
	ITEM_IPV6_EXT,
	ITEM_IPV6_FRAG_EXT,
	ITEM_IPV6_ROUTING_EXT,
	ITEM_ICMP6,
	ITEM_ICMP6_ECHO_REQUEST,
	ITEM_ICMP6_ECHO_REPLY,
	ITEM_ICMP6_ND_NS,
	ITEM_ICMP6_ND_NA,
	ITEM_ICMP6_ND_OPT,
	ITEM_ICMP6_ND_OPT_SLA_ETH,
	ITEM_ICMP6_ND_OPT_TLA_ETH,
	ITEM_META,
	ITEM_GRE_KEY,
	ITEM_GRE_OPTION,
	ITEM_GTP_PSC,
	ITEM_PPPOES,
	ITEM_PPPOED,
	ITEM_PPPOE_PROTO_ID,
	ITEM_HIGIG2,
	ITEM_TAG,
	ITEM_L2TPV3OIP,
	ITEM_ESP,
	ITEM_AH,
	ITEM_PFCP,
	ITEM_ECPRI,
	ITEM_GENEVE_OPT,
	ITEM_INTEGRITY,
	ITEM_CONNTRACK,
	ITEM_PORT_REPRESENTOR,
	ITEM_REPRESENTED_PORT,
	ITEM_FLEX,
	ITEM_L2TPV2,
	ITEM_PPP,
	ITEM_METER,
	ITEM_QUOTA,
	ITEM_AGGR_AFFINITY,
	END_SET,
	ZERO,
};

static const enum index item_fuzzy[] = {
	ITEM_FUZZY_THRESH,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_any[] = {
	ITEM_ANY_NUM,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_port_id[] = {
	ITEM_PORT_ID_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_mark[] = {
	ITEM_MARK_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_raw[] = {
	ITEM_RAW_RELATIVE,
	ITEM_RAW_SEARCH,
	ITEM_RAW_OFFSET,
	ITEM_RAW_LIMIT,
	ITEM_RAW_PATTERN,
	ITEM_RAW_PATTERN_HEX,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_eth[] = {
	ITEM_ETH_DST,
	ITEM_ETH_SRC,
	ITEM_ETH_TYPE,
	ITEM_ETH_HAS_VLAN,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_vlan[] = {
	ITEM_VLAN_TCI,
	ITEM_VLAN_PCP,
	ITEM_VLAN_DEI,
	ITEM_VLAN_VID,
	ITEM_VLAN_INNER_TYPE,
	ITEM_VLAN_HAS_MORE_VLAN,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv4[] = {
	ITEM_IPV4_VER_IHL,
	ITEM_IPV4_TOS,
	ITEM_IPV4_ID,
	ITEM_IPV4_FRAGMENT_OFFSET,
	ITEM_IPV4_TTL,
	ITEM_IPV4_PROTO,
	ITEM_IPV4_SRC,
	ITEM_IPV4_DST,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv6[] = {
	ITEM_IPV6_TC,
	ITEM_IPV6_FLOW,
	ITEM_IPV6_PROTO,
	ITEM_IPV6_HOP,
	ITEM_IPV6_SRC,
	ITEM_IPV6_DST,
	ITEM_IPV6_HAS_FRAG_EXT,
	ITEM_IPV6_ROUTING_EXT,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv6_routing_ext[] = {
	ITEM_IPV6_ROUTING_EXT_TYPE,
	ITEM_IPV6_ROUTING_EXT_NEXT_HDR,
	ITEM_IPV6_ROUTING_EXT_SEG_LEFT,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp[] = {
	ITEM_ICMP_TYPE,
	ITEM_ICMP_CODE,
	ITEM_ICMP_IDENT,
	ITEM_ICMP_SEQ,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_udp[] = {
	ITEM_UDP_SRC,
	ITEM_UDP_DST,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_tcp[] = {
	ITEM_TCP_SRC,
	ITEM_TCP_DST,
	ITEM_TCP_FLAGS,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_sctp[] = {
	ITEM_SCTP_SRC,
	ITEM_SCTP_DST,
	ITEM_SCTP_TAG,
	ITEM_SCTP_CKSUM,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_vxlan[] = {
	ITEM_VXLAN_VNI,
	ITEM_VXLAN_LAST_RSVD,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_e_tag[] = {
	ITEM_E_TAG_GRP_ECID_B,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_nvgre[] = {
	ITEM_NVGRE_TNI,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_mpls[] = {
	ITEM_MPLS_LABEL,
	ITEM_MPLS_TC,
	ITEM_MPLS_S,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gre[] = {
	ITEM_GRE_PROTO,
	ITEM_GRE_C_RSVD0_VER,
	ITEM_GRE_C_BIT,
	ITEM_GRE_K_BIT,
	ITEM_GRE_S_BIT,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gre_key[] = {
	ITEM_GRE_KEY_VALUE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gre_option[] = {
	ITEM_GRE_OPTION_CHECKSUM,
	ITEM_GRE_OPTION_KEY,
	ITEM_GRE_OPTION_SEQUENCE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gtp[] = {
	ITEM_GTP_FLAGS,
	ITEM_GTP_MSG_TYPE,
	ITEM_GTP_TEID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_geneve[] = {
	ITEM_GENEVE_VNI,
	ITEM_GENEVE_PROTO,
	ITEM_GENEVE_OPTLEN,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_vxlan_gpe[] = {
	ITEM_VXLAN_GPE_VNI,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_arp_eth_ipv4[] = {
	ITEM_ARP_ETH_IPV4_SHA,
	ITEM_ARP_ETH_IPV4_SPA,
	ITEM_ARP_ETH_IPV4_THA,
	ITEM_ARP_ETH_IPV4_TPA,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv6_ext[] = {
	ITEM_IPV6_EXT_NEXT_HDR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv6_frag_ext[] = {
	ITEM_IPV6_FRAG_EXT_NEXT_HDR,
	ITEM_IPV6_FRAG_EXT_FRAG_DATA,
	ITEM_IPV6_FRAG_EXT_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6[] = {
	ITEM_ICMP6_TYPE,
	ITEM_ICMP6_CODE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_echo_request[] = {
	ITEM_ICMP6_ECHO_REQUEST_ID,
	ITEM_ICMP6_ECHO_REQUEST_SEQ,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_echo_reply[] = {
	ITEM_ICMP6_ECHO_REPLY_ID,
	ITEM_ICMP6_ECHO_REPLY_SEQ,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_nd_ns[] = {
	ITEM_ICMP6_ND_NS_TARGET_ADDR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_nd_na[] = {
	ITEM_ICMP6_ND_NA_TARGET_ADDR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_nd_opt[] = {
	ITEM_ICMP6_ND_OPT_TYPE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_nd_opt_sla_eth[] = {
	ITEM_ICMP6_ND_OPT_SLA_ETH_SLA,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6_nd_opt_tla_eth[] = {
	ITEM_ICMP6_ND_OPT_TLA_ETH_TLA,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_meta[] = {
	ITEM_META_DATA,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gtp_psc[] = {
	ITEM_GTP_PSC_QFI,
	ITEM_GTP_PSC_PDU_T,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_pppoed[] = {
	ITEM_PPPOE_SEID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_pppoes[] = {
	ITEM_PPPOE_SEID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_pppoe_proto_id[] = {
	ITEM_NEXT,
	ZERO,
};

static const enum index item_higig2[] = {
	ITEM_HIGIG2_CLASSIFICATION,
	ITEM_HIGIG2_VID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_esp[] = {
	ITEM_ESP_SPI,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ah[] = {
	ITEM_AH_SPI,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_pfcp[] = {
	ITEM_PFCP_S_FIELD,
	ITEM_PFCP_SEID,
	ITEM_NEXT,
	ZERO,
};

static const enum index next_set_raw[] = {
	SET_RAW_INDEX,
	ITEM_ETH,
	ZERO,
};

static const enum index item_tag[] = {
	ITEM_TAG_DATA,
	ITEM_TAG_INDEX,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv3oip[] = {
	ITEM_L2TPV3OIP_SESSION_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ecpri[] = {
	ITEM_ECPRI_COMMON,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ecpri_common[] = {
	ITEM_ECPRI_COMMON_TYPE,
	ZERO,
};

static const enum index item_ecpri_common_type[] = {
	ITEM_ECPRI_COMMON_TYPE_IQ_DATA,
	ITEM_ECPRI_COMMON_TYPE_RTC_CTRL,
	ITEM_ECPRI_COMMON_TYPE_DLY_MSR,
	ZERO,
};

static const enum index item_geneve_opt[] = {
	ITEM_GENEVE_OPT_CLASS,
	ITEM_GENEVE_OPT_TYPE,
	ITEM_GENEVE_OPT_LENGTH,
	ITEM_GENEVE_OPT_DATA,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_integrity[] = {
	ITEM_INTEGRITY_LEVEL,
	ITEM_INTEGRITY_VALUE,
	ZERO,
};

static const enum index item_integrity_lv[] = {
	ITEM_INTEGRITY_LEVEL,
	ITEM_INTEGRITY_VALUE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_port_representor[] = {
	ITEM_PORT_REPRESENTOR_PORT_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_represented_port[] = {
	ITEM_REPRESENTED_PORT_ETHDEV_PORT_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_flex[] = {
	ITEM_FLEX_PATTERN_HANDLE,
	ITEM_FLEX_ITEM_HANDLE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2[] = {
	ITEM_L2TPV2_TYPE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type[] = {
	ITEM_L2TPV2_TYPE_DATA,
	ITEM_L2TPV2_TYPE_DATA_L,
	ITEM_L2TPV2_TYPE_DATA_S,
	ITEM_L2TPV2_TYPE_DATA_O,
	ITEM_L2TPV2_TYPE_DATA_L_S,
	ITEM_L2TPV2_TYPE_CTRL,
	ZERO,
};

static const enum index item_l2tpv2_type_data[] = {
	ITEM_L2TPV2_MSG_DATA_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_SESSION_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type_data_l[] = {
	ITEM_L2TPV2_MSG_DATA_L_LENGTH,
	ITEM_L2TPV2_MSG_DATA_L_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_L_SESSION_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type_data_s[] = {
	ITEM_L2TPV2_MSG_DATA_S_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_S_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_S_NS,
	ITEM_L2TPV2_MSG_DATA_S_NR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type_data_o[] = {
	ITEM_L2TPV2_MSG_DATA_O_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_O_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_O_OFFSET,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type_data_l_s[] = {
	ITEM_L2TPV2_MSG_DATA_L_S_LENGTH,
	ITEM_L2TPV2_MSG_DATA_L_S_TUNNEL_ID,
	ITEM_L2TPV2_MSG_DATA_L_S_SESSION_ID,
	ITEM_L2TPV2_MSG_DATA_L_S_NS,
	ITEM_L2TPV2_MSG_DATA_L_S_NR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_l2tpv2_type_ctrl[] = {
	ITEM_L2TPV2_MSG_CTRL_LENGTH,
	ITEM_L2TPV2_MSG_CTRL_TUNNEL_ID,
	ITEM_L2TPV2_MSG_CTRL_SESSION_ID,
	ITEM_L2TPV2_MSG_CTRL_NS,
	ITEM_L2TPV2_MSG_CTRL_NR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ppp[] = {
	ITEM_PPP_ADDR,
	ITEM_PPP_CTRL,
	ITEM_PPP_PROTO_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_meter[] = {
	ITEM_METER_COLOR,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_quota[] = {
	ITEM_QUOTA_STATE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_aggr_affinity[] = {
	ITEM_AGGR_AFFINITY_VALUE,
	ITEM_NEXT,
	ZERO,
};

static const enum index next_action[] = {
	ACTION_END,
	ACTION_VOID,
	ACTION_PASSTHRU,
	ACTION_SKIP_CMAN,
	ACTION_JUMP,
	ACTION_MARK,
	ACTION_FLAG,
	ACTION_QUEUE,
	ACTION_DROP,
	ACTION_COUNT,
	ACTION_RSS,
	ACTION_PF,
	ACTION_VF,
	ACTION_PORT_ID,
	ACTION_METER,
	ACTION_METER_COLOR,
	ACTION_METER_MARK,
	ACTION_OF_DEC_NW_TTL,
	ACTION_OF_POP_VLAN,
	ACTION_OF_PUSH_VLAN,
	ACTION_OF_SET_VLAN_VID,
	ACTION_OF_SET_VLAN_PCP,
	ACTION_OF_POP_MPLS,
	ACTION_OF_PUSH_MPLS,
	ACTION_VXLAN_ENCAP,
	ACTION_VXLAN_DECAP,
	ACTION_NVGRE_ENCAP,
	ACTION_NVGRE_DECAP,
	ACTION_L2_ENCAP,
	ACTION_L2_DECAP,
	ACTION_MPLSOGRE_ENCAP,
	ACTION_MPLSOGRE_DECAP,
	ACTION_MPLSOUDP_ENCAP,
	ACTION_MPLSOUDP_DECAP,
	ACTION_SET_IPV4_SRC,
	ACTION_SET_IPV4_DST,
	ACTION_SET_IPV6_SRC,
	ACTION_SET_IPV6_DST,
	ACTION_SET_TP_SRC,
	ACTION_SET_TP_DST,
	ACTION_MAC_SWAP,
	ACTION_DEC_TTL,
	ACTION_SET_TTL,
	ACTION_SET_MAC_SRC,
	ACTION_SET_MAC_DST,
	ACTION_INC_TCP_SEQ,
	ACTION_DEC_TCP_SEQ,
	ACTION_INC_TCP_ACK,
	ACTION_DEC_TCP_ACK,
	ACTION_RAW_ENCAP,
	ACTION_RAW_DECAP,
	ACTION_SET_TAG,
	ACTION_SET_META,
	ACTION_SET_IPV4_DSCP,
	ACTION_SET_IPV6_DSCP,
	ACTION_AGE,
	ACTION_AGE_UPDATE,
	ACTION_SAMPLE,
	ACTION_INDIRECT,
	ACTION_SHARED_INDIRECT,
	ACTION_MODIFY_FIELD,
	ACTION_CONNTRACK,
	ACTION_CONNTRACK_UPDATE,
	ACTION_PORT_REPRESENTOR,
	ACTION_REPRESENTED_PORT,
	ACTION_SEND_TO_KERNEL,
	ACTION_QUOTA_CREATE,
	ACTION_QUOTA_QU,
	ZERO,
};

static const enum index action_quota_create[] = {
	ACTION_QUOTA_CREATE_LIMIT,
	ACTION_QUOTA_CREATE_MODE,
	ACTION_NEXT,
	ZERO
};

static const enum index action_quota_update[] = {
	ACTION_QUOTA_QU_LIMIT,
	ACTION_QUOTA_QU_UPDATE_OP,
	ACTION_NEXT,
	ZERO
};

static const enum index action_mark[] = {
	ACTION_MARK_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_queue[] = {
	ACTION_QUEUE_INDEX,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_count[] = {
	ACTION_COUNT_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_rss[] = {
	ACTION_RSS_FUNC,
	ACTION_RSS_LEVEL,
	ACTION_RSS_TYPES,
	ACTION_RSS_KEY,
	ACTION_RSS_KEY_LEN,
	ACTION_RSS_QUEUES,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_vf[] = {
	ACTION_VF_ORIGINAL,
	ACTION_VF_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_port_id[] = {
	ACTION_PORT_ID_ORIGINAL,
	ACTION_PORT_ID_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_meter[] = {
	ACTION_METER_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_meter_color[] = {
	ACTION_METER_COLOR_TYPE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_meter_mark[] = {
	ACTION_METER_PROFILE,
	ACTION_METER_POLICY,
	ACTION_METER_COLOR_MODE,
	ACTION_METER_INIT_COLOR,
	ACTION_METER_STATE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_push_vlan[] = {
	ACTION_OF_PUSH_VLAN_ETHERTYPE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_set_vlan_vid[] = {
	ACTION_OF_SET_VLAN_VID_VLAN_VID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_set_vlan_pcp[] = {
	ACTION_OF_SET_VLAN_PCP_VLAN_PCP,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_pop_mpls[] = {
	ACTION_OF_POP_MPLS_ETHERTYPE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_push_mpls[] = {
	ACTION_OF_PUSH_MPLS_ETHERTYPE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv4_src[] = {
	ACTION_SET_IPV4_SRC_IPV4_SRC,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_mac_src[] = {
	ACTION_SET_MAC_SRC_MAC_SRC,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv4_dst[] = {
	ACTION_SET_IPV4_DST_IPV4_DST,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv6_src[] = {
	ACTION_SET_IPV6_SRC_IPV6_SRC,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv6_dst[] = {
	ACTION_SET_IPV6_DST_IPV6_DST,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_tp_src[] = {
	ACTION_SET_TP_SRC_TP_SRC,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_tp_dst[] = {
	ACTION_SET_TP_DST_TP_DST,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ttl[] = {
	ACTION_SET_TTL_TTL,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_jump[] = {
	ACTION_JUMP_GROUP,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_mac_dst[] = {
	ACTION_SET_MAC_DST_MAC_DST,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_inc_tcp_seq[] = {
	ACTION_INC_TCP_SEQ_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_dec_tcp_seq[] = {
	ACTION_DEC_TCP_SEQ_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_inc_tcp_ack[] = {
	ACTION_INC_TCP_ACK_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_dec_tcp_ack[] = {
	ACTION_DEC_TCP_ACK_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_raw_encap[] = {
	ACTION_RAW_ENCAP_INDEX,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_raw_decap[] = {
	ACTION_RAW_DECAP_INDEX,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_tag[] = {
	ACTION_SET_TAG_DATA,
	ACTION_SET_TAG_INDEX,
	ACTION_SET_TAG_MASK,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_meta[] = {
	ACTION_SET_META_DATA,
	ACTION_SET_META_MASK,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv4_dscp[] = {
	ACTION_SET_IPV4_DSCP_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_set_ipv6_dscp[] = {
	ACTION_SET_IPV6_DSCP_VALUE,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_age[] = {
	ACTION_AGE,
	ACTION_AGE_TIMEOUT,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_age_update[] = {
	ACTION_AGE_UPDATE,
	ACTION_AGE_UPDATE_TIMEOUT,
	ACTION_AGE_UPDATE_TOUCH,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_sample[] = {
	ACTION_SAMPLE,
	ACTION_SAMPLE_RATIO,
	ACTION_SAMPLE_INDEX,
	ACTION_NEXT,
	ZERO,
};

static const enum index next_action_sample[] = {
	ACTION_QUEUE,
	ACTION_RSS,
	ACTION_MARK,
	ACTION_COUNT,
	ACTION_PORT_ID,
	ACTION_RAW_ENCAP,
	ACTION_VXLAN_ENCAP,
	ACTION_NVGRE_ENCAP,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_modify_field_dst[] = {
	ACTION_MODIFY_FIELD_DST_LEVEL,
	ACTION_MODIFY_FIELD_DST_OFFSET,
	ACTION_MODIFY_FIELD_SRC_TYPE,
	ZERO,
};

static const enum index action_modify_field_src[] = {
	ACTION_MODIFY_FIELD_SRC_LEVEL,
	ACTION_MODIFY_FIELD_SRC_OFFSET,
	ACTION_MODIFY_FIELD_SRC_VALUE,
	ACTION_MODIFY_FIELD_SRC_POINTER,
	ACTION_MODIFY_FIELD_WIDTH,
	ZERO,
};

static const enum index action_update_conntrack[] = {
	ACTION_CONNTRACK_UPDATE_DIR,
	ACTION_CONNTRACK_UPDATE_CTX,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_port_representor[] = {
	ACTION_PORT_REPRESENTOR_PORT_ID,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_represented_port[] = {
	ACTION_REPRESENTED_PORT_ETHDEV_PORT_ID,
	ACTION_NEXT,
	ZERO,
};

static int parse_set_raw_encap_decap(struct context *, const struct token *,
				     const char *, unsigned int,
				     void *, unsigned int);
static int parse_set_sample_action(struct context *, const struct token *,
				   const char *, unsigned int,
				   void *, unsigned int);
static int parse_set_init(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int
parse_flex_handle(struct context *, const struct token *,
		  const char *, unsigned int, void *, unsigned int);
static int parse_init(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_vc(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_vc_spec(struct context *, const struct token *,
			 const char *, unsigned int, void *, unsigned int);
static int parse_vc_conf(struct context *, const struct token *,
			 const char *, unsigned int, void *, unsigned int);
static int parse_vc_conf_timeout(struct context *, const struct token *,
				 const char *, unsigned int, void *,
				 unsigned int);
static int parse_vc_item_ecpri_type(struct context *, const struct token *,
				    const char *, unsigned int,
				    void *, unsigned int);
static int parse_vc_item_l2tpv2_type(struct context *, const struct token *,
				    const char *, unsigned int,
				    void *, unsigned int);
static int parse_vc_action_meter_color_type(struct context *,
					const struct token *,
					const char *, unsigned int, void *,
					unsigned int);
static int parse_vc_action_rss(struct context *, const struct token *,
			       const char *, unsigned int, void *,
			       unsigned int);
static int parse_vc_action_rss_func(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_rss_type(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_rss_queue(struct context *, const struct token *,
				     const char *, unsigned int, void *,
				     unsigned int);
static int parse_vc_action_vxlan_encap(struct context *, const struct token *,
				       const char *, unsigned int, void *,
				       unsigned int);
static int parse_vc_action_nvgre_encap(struct context *, const struct token *,
				       const char *, unsigned int, void *,
				       unsigned int);
static int parse_vc_action_l2_encap(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_l2_decap(struct context *, const struct token *,
				    const char *, unsigned int, void *,
				    unsigned int);
static int parse_vc_action_mplsogre_encap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsogre_decap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsoudp_encap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_mplsoudp_decap(struct context *,
					  const struct token *, const char *,
					  unsigned int, void *, unsigned int);
static int parse_vc_action_raw_encap(struct context *,
				     const struct token *, const char *,
				     unsigned int, void *, unsigned int);
static int parse_vc_action_raw_decap(struct context *,
				     const struct token *, const char *,
				     unsigned int, void *, unsigned int);
static int parse_vc_action_raw_encap_index(struct context *,
					   const struct token *, const char *,
					   unsigned int, void *, unsigned int);
static int parse_vc_action_raw_decap_index(struct context *,
					   const struct token *, const char *,
					   unsigned int, void *, unsigned int);
static int parse_vc_action_set_meta(struct context *ctx,
				    const struct token *token, const char *str,
				    unsigned int len, void *buf,
					unsigned int size);
static int parse_vc_action_sample(struct context *ctx,
				    const struct token *token, const char *str,
				    unsigned int len, void *buf,
				    unsigned int size);
static int
parse_vc_action_sample_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_op(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_id(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_modify_field_level(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size);
static int
parse_vc_action_conntrack_update(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size);
static int parse_destroy(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_flush(struct context *, const struct token *,
		       const char *, unsigned int,
		       void *, unsigned int);
static int parse_dump(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_query(struct context *, const struct token *,
		       const char *, unsigned int,
		       void *, unsigned int);
static int parse_action(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_list(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_aged(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_isolate(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_configure(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_template(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int parse_template_destroy(struct context *, const struct token *,
				  const char *, unsigned int,
				  void *, unsigned int);
static int parse_table(struct context *, const struct token *,
		       const char *, unsigned int, void *, unsigned int);
static int parse_table_destroy(struct context *, const struct token *,
			       const char *, unsigned int,
			       void *, unsigned int);
static int parse_qo(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_qo_destroy(struct context *, const struct token *,
			    const char *, unsigned int,
			    void *, unsigned int);
static int parse_qia(struct context *, const struct token *,
		     const char *, unsigned int,
		     void *, unsigned int);
static int parse_qia_destroy(struct context *, const struct token *,
			     const char *, unsigned int,
			     void *, unsigned int);
static int parse_push(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_pull(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_tunnel(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_flex(struct context *, const struct token *,
		      const char *, unsigned int, void *, unsigned int);
static int parse_int(struct context *, const struct token *,
		     const char *, unsigned int,
		     void *, unsigned int);
static int parse_prefix(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_boolean(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_string(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_hex(struct context *ctx, const struct token *token,
			const char *str, unsigned int len,
			void *buf, unsigned int size);
static int parse_string0(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
static int parse_mac_addr(struct context *, const struct token *,
			  const char *, unsigned int,
			  void *, unsigned int);
static int parse_ipv4_addr(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_ipv6_addr(struct context *, const struct token *,
			   const char *, unsigned int,
			   void *, unsigned int);
static int parse_port(struct context *, const struct token *,
		      const char *, unsigned int,
		      void *, unsigned int);
static int parse_ia(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_ia_destroy(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size);
static int parse_ia_id2ptr(struct context *ctx, const struct token *token,
			   const char *str, unsigned int len, void *buf,
			   unsigned int size);
static int parse_ia_port(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size);
static int parse_mp(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_meter_profile_id2ptr(struct context *ctx,
				      const struct token *token,
				      const char *str, unsigned int len,
				      void *buf, unsigned int size);
static int parse_meter_policy_id2ptr(struct context *ctx,
				     const struct token *token,
				     const char *str, unsigned int len,
				     void *buf, unsigned int size);
static int parse_meter_color(struct context *ctx, const struct token *token,
			     const char *str, unsigned int len, void *buf,
			     unsigned int size);
static int parse_insertion_table_type(struct context *ctx, const struct token *token,
				      const char *str, unsigned int len, void *buf,
				      unsigned int size);
static int parse_hash_table_type(struct context *ctx, const struct token *token,
				 const char *str, unsigned int len, void *buf,
				 unsigned int size);
static int
parse_quota_state_name(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len, void *buf,
		       unsigned int size);
static int
parse_quota_mode_name(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size);
static int
parse_quota_update_name(struct context *ctx, const struct token *token,
			const char *str, unsigned int len, void *buf,
			unsigned int size);
static int
parse_qu_mode_name(struct context *ctx, const struct token *token,
		   const char *str, unsigned int len, void *buf,
		   unsigned int size);
static int comp_none(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);
static int comp_boolean(struct context *, const struct token *,
			unsigned int, char *, unsigned int);
static int comp_action(struct context *, const struct token *,
		       unsigned int, char *, unsigned int);
static int comp_port(struct context *, const struct token *,
		     unsigned int, char *, unsigned int);
static int comp_rule_id(struct context *, const struct token *,
			unsigned int, char *, unsigned int);
static int comp_vc_action_rss_type(struct context *, const struct token *,
				   unsigned int, char *, unsigned int);
static int comp_vc_action_rss_queue(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_set_raw_index(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_sample_index(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_modify_field_op(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_set_modify_field_id(struct context *, const struct token *,
			      unsigned int, char *, unsigned int);
static int comp_pattern_template_id(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_actions_template_id(struct context *, const struct token *,
				    unsigned int, char *, unsigned int);
static int comp_table_id(struct context *, const struct token *,
			 unsigned int, char *, unsigned int);
static int comp_queue_id(struct context *, const struct token *,
			 unsigned int, char *, unsigned int);
static int comp_meter_color(struct context *, const struct token *,
			    unsigned int, char *, unsigned int);
static int comp_insertion_table_type(struct context *, const struct token *,
				     unsigned int, char *, unsigned int);
static int comp_hash_table_type(struct context *, const struct token *,
				unsigned int, char *, unsigned int);
static int
comp_quota_state_name(struct context *ctx, const struct token *token,
		      unsigned int ent, char *buf, unsigned int size);
static int
comp_quota_mode_name(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size);
static int
comp_quota_update_name(struct context *ctx, const struct token *token,
		       unsigned int ent, char *buf, unsigned int size);
static int
comp_qu_mode_name(struct context *ctx, const struct token *token,
		  unsigned int ent, char *buf, unsigned int size);

/** Token definitions. */
static const struct token token_list[] = {
	/* Special tokens. */
	[ZERO] = {
		.name = "ZERO",
		.help = "null entry, abused as the entry point",
		.next = NEXT(NEXT_ENTRY(FLOW, ADD)),
	},
	[END] = {
		.name = "",
		.type = "RETURN",
		.help = "command may end here",
	},
	[START_SET] = {
		.name = "START_SET",
		.help = "null entry, abused as the entry point for set",
		.next = NEXT(NEXT_ENTRY(SET)),
	},
	[END_SET] = {
		.name = "end_set",
		.type = "RETURN",
		.help = "set command may end here",
	},
	/* Common tokens. */
	[COMMON_INTEGER] = {
		.name = "{int}",
		.type = "INTEGER",
		.help = "integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_UNSIGNED] = {
		.name = "{unsigned}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_PREFIX] = {
		.name = "{prefix}",
		.type = "PREFIX",
		.help = "prefix length for bit-mask",
		.call = parse_prefix,
		.comp = comp_none,
	},
	[COMMON_BOOLEAN] = {
		.name = "{boolean}",
		.type = "BOOLEAN",
		.help = "any boolean value",
		.call = parse_boolean,
		.comp = comp_boolean,
	},
	[COMMON_STRING] = {
		.name = "{string}",
		.type = "STRING",
		.help = "fixed string",
		.call = parse_string,
		.comp = comp_none,
	},
	[COMMON_HEX] = {
		.name = "{hex}",
		.type = "HEX",
		.help = "fixed string",
		.call = parse_hex,
	},
	[COMMON_FILE_PATH] = {
		.name = "{file path}",
		.type = "STRING",
		.help = "file path",
		.call = parse_string0,
		.comp = comp_none,
	},
	[COMMON_MAC_ADDR] = {
		.name = "{MAC address}",
		.type = "MAC-48",
		.help = "standard MAC address notation",
		.call = parse_mac_addr,
		.comp = comp_none,
	},
	[COMMON_IPV4_ADDR] = {
		.name = "{IPv4 address}",
		.type = "IPV4 ADDRESS",
		.help = "standard IPv4 address notation",
		.call = parse_ipv4_addr,
		.comp = comp_none,
	},
	[COMMON_IPV6_ADDR] = {
		.name = "{IPv6 address}",
		.type = "IPV6 ADDRESS",
		.help = "standard IPv6 address notation",
		.call = parse_ipv6_addr,
		.comp = comp_none,
	},
	[COMMON_RULE_ID] = {
		.name = "{rule id}",
		.type = "RULE ID",
		.help = "rule identifier",
		.call = parse_int,
		.comp = comp_rule_id,
	},
	[COMMON_PORT_ID] = {
		.name = "{port_id}",
		.type = "PORT ID",
		.help = "port identifier",
		.call = parse_port,
		.comp = comp_port,
	},
	[COMMON_GROUP_ID] = {
		.name = "{group_id}",
		.type = "GROUP ID",
		.help = "group identifier",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_PRIORITY_LEVEL] = {
		.name = "{level}",
		.type = "PRIORITY",
		.help = "priority level",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_INDIRECT_ACTION_ID] = {
		.name = "{indirect_action_id}",
		.type = "INDIRECT_ACTION_ID",
		.help = "indirect action id",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_PROFILE_ID] = {
		.name = "{profile_id}",
		.type = "PROFILE_ID",
		.help = "profile id",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_POLICY_ID] = {
		.name = "{policy_id}",
		.type = "POLICY_ID",
		.help = "policy id",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_FLEX_TOKEN] = {
		.name = "{flex token}",
		.type = "flex token",
		.help = "flex token",
		.call = parse_int,
		.comp = comp_none,
	},
	[COMMON_FLEX_HANDLE] = {
		.name = "{flex handle}",
		.type = "FLEX HANDLE",
		.help = "fill flex item data",
		.call = parse_flex_handle,
		.comp = comp_none,
	},
	[COMMON_PATTERN_TEMPLATE_ID] = {
		.name = "{pattern_template_id}",
		.type = "PATTERN_TEMPLATE_ID",
		.help = "pattern template id",
		.call = parse_int,
		.comp = comp_pattern_template_id,
	},
	[COMMON_ACTIONS_TEMPLATE_ID] = {
		.name = "{actions_template_id}",
		.type = "ACTIONS_TEMPLATE_ID",
		.help = "actions template id",
		.call = parse_int,
		.comp = comp_actions_template_id,
	},
	[COMMON_TABLE_ID] = {
		.name = "{table_id}",
		.type = "TABLE_ID",
		.help = "table id",
		.call = parse_int,
		.comp = comp_table_id,
	},
	[COMMON_QUEUE_ID] = {
		.name = "{queue_id}",
		.type = "QUEUE_ID",
		.help = "queue id",
		.call = parse_int,
		.comp = comp_queue_id,
	},
	/* Top-level command. */
	[FLOW] = {
		.name = "flow",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage ingress/egress flow rules",
		.next = NEXT(NEXT_ENTRY
			     (INFO,
			      CONFIGURE,
			      PATTERN_TEMPLATE,
			      ACTIONS_TEMPLATE,
			      TABLE,
			      INDIRECT_ACTION,
			      VALIDATE,
			      CREATE,
			      DESTROY,
			      FLUSH,
			      DUMP,
			      LIST,
			      AGED,
			      QUERY,
			      ISOLATE,
			      TUNNEL,
			      FLEX,
			      QUEUE,
			      PUSH,
			      PULL)),
		.call = parse_init,
	},
	/* Top-level command. */
	[INFO] = {
		.name = "info",
		.help = "get information about flow engine",
		.next = NEXT(NEXT_ENTRY(END),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_configure,
	},
	/* Top-level command. */
	[CONFIGURE] = {
		.name = "configure",
		.help = "configure flow engine",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_configure,
	},
	/* Configure arguments. */
	[CONFIG_QUEUES_NUMBER] = {
		.name = "queues_number",
		.help = "number of queues",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.nb_queue)),
	},
	[CONFIG_QUEUES_SIZE] = {
		.name = "queues_size",
		.help = "number of elements in queues",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.queue_attr.size)),
	},
	[CONFIG_COUNTERS_NUMBER] = {
		.name = "counters_number",
		.help = "number of counters",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.nb_counters)),
	},
	[CONFIG_AGING_OBJECTS_NUMBER] = {
		.name = "aging_counters_number",
		.help = "number of aging objects",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.nb_aging_objects)),
	},
	[CONFIG_QUOTAS_NUMBER] = {
		.name = "quotas_number",
		.help = "number of quotas",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
				     args.configure.port_attr.nb_quotas)),
	},
	[CONFIG_METERS_NUMBER] = {
		.name = "meters_number",
		.help = "number of meters",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.nb_meters)),
	},
	[CONFIG_CONN_TRACK_NUMBER] = {
		.name = "conn_tracks_number",
		.help = "number of connection trackings",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.nb_conn_tracks)),
	},
	[CONFIG_FLAGS] = {
		.name = "flags",
		.help = "configuration flags",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.flags)),
	},
	[CONFIG_HOST_PORT] = {
		.name = "host_port",
		.help = "host port for shared objects",
		.next = NEXT(next_config_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.configure.port_attr.host_port_id)),
	},
	/* Top-level command. */
	[PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage pattern templates",
		.next = NEXT(next_pt_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_template,
	},
	/* Sub-level commands. */
	[PATTERN_TEMPLATE_CREATE] = {
		.name = "create",
		.help = "create pattern template",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[PATTERN_TEMPLATE_DESTROY] = {
		.name = "destroy",
		.help = "destroy pattern template",
		.next = NEXT(NEXT_ENTRY(PATTERN_TEMPLATE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_template_destroy,
	},
	/* Pattern template arguments. */
	[PATTERN_TEMPLATE_CREATE_ID] = {
		.name = "pattern_template_id",
		.help = "specify a pattern template id to create",
		.next = NEXT(next_pt_attr,
			     NEXT_ENTRY(COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.pat_templ_id)),
	},
	[PATTERN_TEMPLATE_DESTROY_ID] = {
		.name = "pattern_template",
		.help = "specify a pattern template id to destroy",
		.next = NEXT(next_pt_destroy_attr,
			     NEXT_ENTRY(COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.templ_destroy.template_id)),
		.call = parse_template_destroy,
	},
	[PATTERN_TEMPLATE_RELAXED_MATCHING] = {
		.name = "relaxed",
		.help = "is matching relaxed",
		.next = NEXT(next_pt_attr,
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct buffer,
			     args.vc.attr.reserved, 1)),
	},
	[PATTERN_TEMPLATE_INGRESS] = {
		.name = "ingress",
		.help = "attribute pattern to ingress",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[PATTERN_TEMPLATE_EGRESS] = {
		.name = "egress",
		.help = "attribute pattern to egress",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[PATTERN_TEMPLATE_TRANSFER] = {
		.name = "transfer",
		.help = "attribute pattern to transfer",
		.next = NEXT(next_pt_attr),
		.call = parse_template,
	},
	[PATTERN_TEMPLATE_SPEC] = {
		.name = "template",
		.help = "specify item to create pattern template",
		.next = NEXT(next_item),
	},
	/* Top-level command. */
	[ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage actions templates",
		.next = NEXT(next_at_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_template,
	},
	/* Sub-level commands. */
	[ACTIONS_TEMPLATE_CREATE] = {
		.name = "create",
		.help = "create actions template",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[ACTIONS_TEMPLATE_DESTROY] = {
		.name = "destroy",
		.help = "destroy actions template",
		.next = NEXT(NEXT_ENTRY(ACTIONS_TEMPLATE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_template_destroy,
	},
	/* Actions template arguments. */
	[ACTIONS_TEMPLATE_CREATE_ID] = {
		.name = "actions_template_id",
		.help = "specify an actions template id to create",
		.next = NEXT(NEXT_ENTRY(ACTIONS_TEMPLATE_MASK),
			     NEXT_ENTRY(ACTIONS_TEMPLATE_SPEC),
			     NEXT_ENTRY(COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.act_templ_id)),
	},
	[ACTIONS_TEMPLATE_DESTROY_ID] = {
		.name = "actions_template",
		.help = "specify an actions template id to destroy",
		.next = NEXT(next_at_destroy_attr,
			     NEXT_ENTRY(COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.templ_destroy.template_id)),
		.call = parse_template_destroy,
	},
	[ACTIONS_TEMPLATE_INGRESS] = {
		.name = "ingress",
		.help = "attribute actions to ingress",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[ACTIONS_TEMPLATE_EGRESS] = {
		.name = "egress",
		.help = "attribute actions to egress",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[ACTIONS_TEMPLATE_TRANSFER] = {
		.name = "transfer",
		.help = "attribute actions to transfer",
		.next = NEXT(next_at_attr),
		.call = parse_template,
	},
	[ACTIONS_TEMPLATE_SPEC] = {
		.name = "template",
		.help = "specify action to create actions template",
		.next = NEXT(next_action),
		.call = parse_template,
	},
	[ACTIONS_TEMPLATE_MASK] = {
		.name = "mask",
		.help = "specify action mask to create actions template",
		.next = NEXT(next_action),
		.call = parse_template,
	},
	/* Top-level command. */
	[TABLE] = {
		.name = "template_table",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage template tables",
		.next = NEXT(next_table_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_table,
	},
	/* Sub-level commands. */
	[TABLE_CREATE] = {
		.name = "create",
		.help = "create template table",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_DESTROY] = {
		.name = "destroy",
		.help = "destroy template table",
		.next = NEXT(NEXT_ENTRY(TABLE_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_table_destroy,
	},
	/* Table  arguments. */
	[TABLE_CREATE_ID] = {
		.name = "table_id",
		.help = "specify table id to create",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.table.id)),
	},
	[TABLE_DESTROY_ID] = {
		.name = "table",
		.help = "specify table id to destroy",
		.next = NEXT(next_table_destroy_attr,
			     NEXT_ENTRY(COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.table_destroy.table_id)),
		.call = parse_table_destroy,
	},
	[TABLE_INSERTION_TYPE] = {
		.name = "insertion_type",
		.help = "specify insertion type",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(TABLE_INSERTION_TYPE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.table.attr.insertion_type)),
	},
	[TABLE_INSERTION_TYPE_NAME] = {
		.name = "insertion_type_name",
		.help = "insertion type name",
		.call = parse_insertion_table_type,
		.comp = comp_insertion_table_type,
	},
	[TABLE_HASH_FUNC] = {
		.name = "hash_func",
		.help = "specify hash calculation function",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(TABLE_HASH_FUNC_NAME)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.table.attr.hash_func)),
	},
	[TABLE_HASH_FUNC_NAME] = {
		.name = "hash_func_name",
		.help = "hash calculation function name",
		.call = parse_hash_table_type,
		.comp = comp_hash_table_type,
	},
	[TABLE_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_table_attr, NEXT_ENTRY(COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.table.attr.flow_attr.group)),
	},
	[TABLE_PRIORITY] = {
		.name = "priority",
		.help = "specify a priority level",
		.next = NEXT(next_table_attr, NEXT_ENTRY(COMMON_PRIORITY_LEVEL)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.table.attr.flow_attr.priority)),
	},
	[TABLE_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_TRANSFER_WIRE_ORIG] = {
		.name = "wire_orig",
		.help = "affect rule direction to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_TRANSFER_VPORT_ORIG] = {
		.name = "vport_orig",
		.help = "affect rule direction to transfer",
		.next = NEXT(next_table_attr),
		.call = parse_table,
	},
	[TABLE_RULES_NUMBER] = {
		.name = "rules_number",
		.help = "number of rules in table",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.table.attr.nb_flows)),
	},
	[TABLE_PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.help = "specify pattern template id",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(COMMON_PATTERN_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.table.pat_templ_id)),
		.call = parse_table,
	},
	[TABLE_ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.help = "specify actions template id",
		.next = NEXT(next_table_attr,
			     NEXT_ENTRY(COMMON_ACTIONS_TEMPLATE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.table.act_templ_id)),
		.call = parse_table,
	},
	/* Top-level command. */
	[QUEUE] = {
		.name = "queue",
		.help = "queue a flow rule operation",
		.next = NEXT(next_queue_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_qo,
	},
	/* Sub-level commands. */
	[QUEUE_CREATE] = {
		.name = "create",
		.help = "create a flow rule",
		.next = NEXT(NEXT_ENTRY(QUEUE_TEMPLATE_TABLE),
			     NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
		.call = parse_qo,
	},
	[QUEUE_DESTROY] = {
		.name = "destroy",
		.help = "destroy a flow rule",
		.next = NEXT(NEXT_ENTRY(QUEUE_DESTROY_ID),
			     NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
		.call = parse_qo_destroy,
	},
	[QUEUE_AGED] = {
		.name = "aged",
		.help = "list and destroy aged flows",
		.next = NEXT(next_aged_attr, NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
		.call = parse_aged,
	},
	[QUEUE_INDIRECT_ACTION] = {
		.name = "indirect_action",
		.help = "queue indirect actions",
		.next = NEXT(next_qia_subcmd, NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
		.call = parse_qia,
	},
	/* Queue  arguments. */
	[QUEUE_TEMPLATE_TABLE] = {
		.name = "template_table",
		.help = "specify table id",
		.next = NEXT(next_async_insert_subcmd,
			     NEXT_ENTRY(COMMON_TABLE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.vc.table_id)),
		.call = parse_qo,
	},
	[QUEUE_PATTERN_TEMPLATE] = {
		.name = "pattern_template",
		.help = "specify pattern template index",
		.next = NEXT(NEXT_ENTRY(QUEUE_ACTIONS_TEMPLATE),
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.vc.pat_templ_id)),
		.call = parse_qo,
	},
	[QUEUE_ACTIONS_TEMPLATE] = {
		.name = "actions_template",
		.help = "specify actions template index",
		.next = NEXT(NEXT_ENTRY(QUEUE_CREATE_POSTPONE),
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.vc.act_templ_id)),
		.call = parse_qo,
	},
	[QUEUE_RULE_ID] = {
		.name = "rule_index",
		.help = "specify flow rule index",
		.next = NEXT(NEXT_ENTRY(QUEUE_ACTIONS_TEMPLATE),
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct buffer,
					args.vc.rule_id)),
		.call = parse_qo,
	},
	[QUEUE_CREATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone create operation",
		.next = NEXT(NEXT_ENTRY(ITEM_PATTERN),
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
		.call = parse_qo,
	},
	[QUEUE_DESTROY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone destroy operation",
		.next = NEXT(NEXT_ENTRY(QUEUE_DESTROY_ID),
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
		.call = parse_qo_destroy,
	},
	[QUEUE_DESTROY_ID] = {
		.name = "rule",
		.help = "specify rule id to destroy",
		.next = NEXT(next_queue_destroy_attr,
			NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.destroy.rule)),
		.call = parse_qo_destroy,
	},
	/* Queue indirect action arguments */
	[QUEUE_INDIRECT_ACTION_CREATE] = {
		.name = "create",
		.help = "create indirect action",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[QUEUE_INDIRECT_ACTION_UPDATE] = {
		.name = "update",
		.help = "update indirect action",
		.next = NEXT(next_qia_update_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
		.call = parse_qia,
	},
	[QUEUE_INDIRECT_ACTION_DESTROY] = {
		.name = "destroy",
		.help = "destroy indirect action",
		.next = NEXT(next_qia_destroy_attr),
		.call = parse_qia_destroy,
	},
	[QUEUE_INDIRECT_ACTION_QUERY] = {
		.name = "query",
		.help = "query indirect action",
		.next = NEXT(next_qia_query_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.action_id)),
		.call = parse_qia,
	},
	/* Indirect action destroy arguments. */
	[QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone destroy operation",
		.next = NEXT(next_qia_destroy_attr,
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
	},
	[QUEUE_INDIRECT_ACTION_DESTROY_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to destroy",
		.next = NEXT(next_qia_destroy_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.ia_destroy.action_id)),
		.call = parse_qia_destroy,
	},
	[QUEUE_INDIRECT_ACTION_QUERY_UPDATE] = {
		.name = "query_update",
		.help = "indirect query [and|or] update action",
		.next = NEXT(next_qia_qu_attr, NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.action_id)),
		.call = parse_qia
	},
	[QUEUE_INDIRECT_ACTION_QU_MODE] = {
		.name = "mode",
		.help = "indirect query [and|or] update action",
		.next = NEXT(next_qia_qu_attr,
			     NEXT_ENTRY(INDIRECT_ACTION_QU_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.qu_mode)),
		.call = parse_qia
	},
	/* Indirect action update arguments. */
	[QUEUE_INDIRECT_ACTION_UPDATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone update operation",
		.next = NEXT(next_qia_update_attr,
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
	},
	/* Indirect action update arguments. */
	[QUEUE_INDIRECT_ACTION_QUERY_POSTPONE] = {
		.name = "postpone",
		.help = "postpone query operation",
		.next = NEXT(next_qia_query_attr,
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
	},
	/* Indirect action create arguments. */
	[QUEUE_INDIRECT_ACTION_CREATE_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to create",
		.next = NEXT(next_qia_create_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
	},
	[QUEUE_INDIRECT_ACTION_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[QUEUE_INDIRECT_ACTION_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[QUEUE_INDIRECT_ACTION_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_qia_create_attr),
		.call = parse_qia,
	},
	[QUEUE_INDIRECT_ACTION_CREATE_POSTPONE] = {
		.name = "postpone",
		.help = "postpone create operation",
		.next = NEXT(next_qia_create_attr,
			     NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY(struct buffer, postpone)),
	},
	[QUEUE_INDIRECT_ACTION_SPEC] = {
		.name = "action",
		.help = "specify action to create indirect handle",
		.next = NEXT(next_action),
	},
	/* Top-level command. */
	[PUSH] = {
		.name = "push",
		.help = "push enqueued operations",
		.next = NEXT(NEXT_ENTRY(PUSH_QUEUE), NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_push,
	},
	/* Sub-level commands. */
	[PUSH_QUEUE] = {
		.name = "queue",
		.help = "specify queue id",
		.next = NEXT(NEXT_ENTRY(END), NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
	},
	/* Top-level command. */
	[PULL] = {
		.name = "pull",
		.help = "pull flow operations results",
		.next = NEXT(NEXT_ENTRY(PULL_QUEUE), NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_pull,
	},
	/* Sub-level commands. */
	[PULL_QUEUE] = {
		.name = "queue",
		.help = "specify queue id",
		.next = NEXT(NEXT_ENTRY(END), NEXT_ENTRY(COMMON_QUEUE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, queue)),
	},
	/* Top-level command. */
	[INDIRECT_ACTION] = {
		.name = "indirect_action",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage indirect actions",
		.next = NEXT(next_ia_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_ia,
	},
	/* Sub-level commands. */
	[INDIRECT_ACTION_CREATE] = {
		.name = "create",
		.help = "create indirect action",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_UPDATE] = {
		.name = "update",
		.help = "update indirect action",
		.next = NEXT(NEXT_ENTRY(INDIRECT_ACTION_SPEC),
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_DESTROY] = {
		.name = "destroy",
		.help = "destroy indirect action",
		.next = NEXT(NEXT_ENTRY(INDIRECT_ACTION_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_ia_destroy,
	},
	[INDIRECT_ACTION_QUERY] = {
		.name = "query",
		.help = "query indirect action",
		.next = NEXT(NEXT_ENTRY(END),
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.action_id)),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_QUERY_UPDATE] = {
		.name = "query_update",
		.help = "query [and|or] update",
		.next = NEXT(next_ia_qu_attr, NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.action_id)),
		.call = parse_ia
	},
	[INDIRECT_ACTION_QU_MODE] = {
		.name = "mode",
		.help = "query_update mode",
		.next = NEXT(next_ia_qu_attr,
			     NEXT_ENTRY(INDIRECT_ACTION_QU_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.ia.qu_mode)),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_QU_MODE_NAME] = {
		.name = "mode_name",
		.help = "query-update mode name",
		.call = parse_qu_mode_name,
		.comp = comp_qu_mode_name,
	},
	[VALIDATE] = {
		.name = "validate",
		.help = "check whether a flow rule can be created",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_vc,
	},
	[CREATE] = {
		.name = "create",
		.help = "create a flow rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_vc,
	},
	[DESTROY] = {
		.name = "destroy",
		.help = "destroy specific flow rules",
		.next = NEXT(NEXT_ENTRY(DESTROY_RULE),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_destroy,
	},
	[FLUSH] = {
		.name = "flush",
		.help = "destroy all flow rules",
		.next = NEXT(NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_flush,
	},
	[DUMP] = {
		.name = "dump",
		.help = "dump single/all flow rules to file",
		.next = NEXT(next_dump_subcmd, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_dump,
	},
	[QUERY] = {
		.name = "query",
		.help = "query an existing flow rule",
		.next = NEXT(NEXT_ENTRY(QUERY_ACTION),
			     NEXT_ENTRY(COMMON_RULE_ID),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.query.action.type),
			     ARGS_ENTRY(struct buffer, args.query.rule),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_query,
	},
	[LIST] = {
		.name = "list",
		.help = "list existing flow rules",
		.next = NEXT(next_list_attr, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_list,
	},
	[AGED] = {
		.name = "aged",
		.help = "list and destroy aged flows",
		.next = NEXT(next_aged_attr, NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_aged,
	},
	[ISOLATE] = {
		.name = "isolate",
		.help = "restrict ingress traffic to the defined flow rules",
		.next = NEXT(NEXT_ENTRY(COMMON_BOOLEAN),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.isolate.set),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_isolate,
	},
	[FLEX] = {
		.name = "flex_item",
		.help = "flex item API",
		.next = NEXT(next_flex_item),
		.call = parse_flex,
	},
	[FLEX_ITEM_INIT] = {
		.name = "init",
		.help = "flex item init",
		.args = ARGS(ARGS_ENTRY(struct buffer, args.flex.token),
			     ARGS_ENTRY(struct buffer, port)),
		.next = NEXT(NEXT_ENTRY(COMMON_FLEX_TOKEN),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.call = parse_flex
	},
	[FLEX_ITEM_CREATE] = {
		.name = "create",
		.help = "flex item create",
		.args = ARGS(ARGS_ENTRY(struct buffer, args.flex.filename),
			     ARGS_ENTRY(struct buffer, args.flex.token),
			     ARGS_ENTRY(struct buffer, port)),
		.next = NEXT(NEXT_ENTRY(COMMON_FILE_PATH),
			     NEXT_ENTRY(COMMON_FLEX_TOKEN),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.call = parse_flex
	},
	[FLEX_ITEM_DESTROY] = {
		.name = "destroy",
		.help = "flex item destroy",
		.args = ARGS(ARGS_ENTRY(struct buffer, args.flex.token),
			     ARGS_ENTRY(struct buffer, port)),
		.next = NEXT(NEXT_ENTRY(COMMON_FLEX_TOKEN),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.call = parse_flex
	},
	[TUNNEL] = {
		.name = "tunnel",
		.help = "new tunnel API",
		.next = NEXT(NEXT_ENTRY
			     (TUNNEL_CREATE, TUNNEL_LIST, TUNNEL_DESTROY)),
		.call = parse_tunnel,
	},
	/* Tunnel arguments. */
	[TUNNEL_CREATE] = {
		.name = "create",
		.help = "create new tunnel object",
		.next = NEXT(NEXT_ENTRY(TUNNEL_CREATE_TYPE),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	[TUNNEL_CREATE_TYPE] = {
		.name = "type",
		.help = "create new tunnel",
		.next = NEXT(NEXT_ENTRY(COMMON_FILE_PATH)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, type)),
		.call = parse_tunnel,
	},
	[TUNNEL_DESTROY] = {
		.name = "destroy",
		.help = "destroy tunnel",
		.next = NEXT(NEXT_ENTRY(TUNNEL_DESTROY_ID),
			     NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	[TUNNEL_DESTROY_ID] = {
		.name = "id",
		.help = "tunnel identifier to destroy",
		.next = NEXT(NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_tunnel,
	},
	[TUNNEL_LIST] = {
		.name = "list",
		.help = "list existing tunnels",
		.next = NEXT(NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	/* Destroy arguments. */
	[DESTROY_RULE] = {
		.name = "rule",
		.help = "specify a rule identifier",
		.next = NEXT(next_destroy_attr, NEXT_ENTRY(COMMON_RULE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer, args.destroy.rule)),
		.call = parse_destroy,
	},
	/* Dump arguments. */
	[DUMP_ALL] = {
		.name = "all",
		.help = "dump all",
		.next = NEXT(next_dump_attr),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.dump.file)),
		.call = parse_dump,
	},
	[DUMP_ONE] = {
		.name = "rule",
		.help = "dump one rule",
		.next = NEXT(next_dump_attr, NEXT_ENTRY(COMMON_RULE_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.dump.file),
				ARGS_ENTRY(struct buffer, args.dump.rule)),
		.call = parse_dump,
	},
	/* Query arguments. */
	[QUERY_ACTION] = {
		.name = "{action}",
		.type = "ACTION",
		.help = "action to query, must be part of the rule",
		.call = parse_action,
		.comp = comp_action,
	},
	/* List arguments. */
	[LIST_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_list_attr, NEXT_ENTRY(COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer, args.list.group)),
		.call = parse_list,
	},
	[AGED_DESTROY] = {
		.name = "destroy",
		.help = "specify aged flows need be destroyed",
		.call = parse_aged,
		.comp = comp_none,
	},
	/* Validate/create attributes. */
	[VC_GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, group)),
		.call = parse_vc,
	},
	[VC_PRIORITY] = {
		.name = "priority",
		.help = "specify a priority level",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_PRIORITY_LEVEL)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, priority)),
		.call = parse_vc,
	},
	[VC_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[VC_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[VC_TRANSFER] = {
		.name = "transfer",
		.help = "apply rule directly to endpoints found in pattern",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[VC_TUNNEL_SET] = {
		.name = "tunnel_set",
		.help = "tunnel steer rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_vc,
	},
	[VC_TUNNEL_MATCH] = {
		.name = "tunnel_match",
		.help = "tunnel match rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_vc,
	},
	/* Validate/create pattern. */
	[ITEM_PATTERN] = {
		.name = "pattern",
		.help = "submit a list of pattern items",
		.next = NEXT(next_item),
		.call = parse_vc,
	},
	[ITEM_PARAM_IS] = {
		.name = "is",
		.help = "match value perfectly (with full bit-mask)",
		.call = parse_vc_spec,
	},
	[ITEM_PARAM_SPEC] = {
		.name = "spec",
		.help = "match value according to configured bit-mask",
		.call = parse_vc_spec,
	},
	[ITEM_PARAM_LAST] = {
		.name = "last",
		.help = "specify upper bound to establish a range",
		.call = parse_vc_spec,
	},
	[ITEM_PARAM_MASK] = {
		.name = "mask",
		.help = "specify bit-mask with relevant bits set to one",
		.call = parse_vc_spec,
	},
	[ITEM_PARAM_PREFIX] = {
		.name = "prefix",
		.help = "generate bit-mask from a prefix length",
		.call = parse_vc_spec,
	},
	[ITEM_NEXT] = {
		.name = "/",
		.help = "specify next pattern item",
		.next = NEXT(next_item),
	},
	[ITEM_END] = {
		.name = "end",
		.help = "end list of pattern items",
		.priv = PRIV_ITEM(END, 0),
		.next = NEXT(NEXT_ENTRY(ACTIONS, END)),
		.call = parse_vc,
	},
	[ITEM_VOID] = {
		.name = "void",
		.help = "no-op pattern item",
		.priv = PRIV_ITEM(VOID, 0),
		.next = NEXT(NEXT_ENTRY(ITEM_NEXT)),
		.call = parse_vc,
	},
	[ITEM_INVERT] = {
		.name = "invert",
		.help = "perform actions when pattern does not match",
		.priv = PRIV_ITEM(INVERT, 0),
		.next = NEXT(NEXT_ENTRY(ITEM_NEXT)),
		.call = parse_vc,
	},
	[ITEM_ANY] = {
		.name = "any",
		.help = "match any protocol for the current layer",
		.priv = PRIV_ITEM(ANY, sizeof(struct rte_flow_item_any)),
		.next = NEXT(item_any),
		.call = parse_vc,
	},
	[ITEM_ANY_NUM] = {
		.name = "num",
		.help = "number of layers covered",
		.next = NEXT(item_any, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_any, num)),
	},
	[ITEM_PORT_ID] = {
		.name = "port_id",
		.help = "match traffic from/to a given DPDK port ID",
		.priv = PRIV_ITEM(PORT_ID,
				  sizeof(struct rte_flow_item_port_id)),
		.next = NEXT(item_port_id),
		.call = parse_vc,
	},
	[ITEM_PORT_ID_ID] = {
		.name = "id",
		.help = "DPDK port ID",
		.next = NEXT(item_port_id, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_port_id, id)),
	},
	[ITEM_MARK] = {
		.name = "mark",
		.help = "match traffic against value set in previously matched rule",
		.priv = PRIV_ITEM(MARK, sizeof(struct rte_flow_item_mark)),
		.next = NEXT(item_mark),
		.call = parse_vc,
	},
	[ITEM_MARK_ID] = {
		.name = "id",
		.help = "Integer value to match against",
		.next = NEXT(item_mark, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_mark, id)),
	},
	[ITEM_RAW] = {
		.name = "raw",
		.help = "match an arbitrary byte string",
		.priv = PRIV_ITEM(RAW, ITEM_RAW_SIZE),
		.next = NEXT(item_raw),
		.call = parse_vc,
	},
	[ITEM_RAW_RELATIVE] = {
		.name = "relative",
		.help = "look for pattern after the previous item",
		.next = NEXT(item_raw, NEXT_ENTRY(COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   relative, 1)),
	},
	[ITEM_RAW_SEARCH] = {
		.name = "search",
		.help = "search pattern from offset (see also limit)",
		.next = NEXT(item_raw, NEXT_ENTRY(COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   search, 1)),
	},
	[ITEM_RAW_OFFSET] = {
		.name = "offset",
		.help = "absolute or relative offset for pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(COMMON_INTEGER), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, offset)),
	},
	[ITEM_RAW_LIMIT] = {
		.name = "limit",
		.help = "search area limit for start of pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, limit)),
	},
	[ITEM_RAW_PATTERN] = {
		.name = "pattern",
		.help = "byte string to look for",
		.next = NEXT(item_raw,
			     NEXT_ENTRY(COMMON_STRING),
			     NEXT_ENTRY(ITEM_PARAM_IS,
					ITEM_PARAM_SPEC,
					ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, pattern),
			     ARGS_ENTRY(struct rte_flow_item_raw, length),
			     ARGS_ENTRY_ARB(sizeof(struct rte_flow_item_raw),
					    ITEM_RAW_PATTERN_SIZE)),
	},
	[ITEM_RAW_PATTERN_HEX] = {
		.name = "pattern_hex",
		.help = "hex string to look for",
		.next = NEXT(item_raw,
			     NEXT_ENTRY(COMMON_HEX),
			     NEXT_ENTRY(ITEM_PARAM_IS,
					ITEM_PARAM_SPEC,
					ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, pattern),
			     ARGS_ENTRY(struct rte_flow_item_raw, length),
			     ARGS_ENTRY_ARB(sizeof(struct rte_flow_item_raw),
					    ITEM_RAW_PATTERN_SIZE)),
	},
	[ITEM_ETH] = {
		.name = "eth",
		.help = "match Ethernet header",
		.priv = PRIV_ITEM(ETH, sizeof(struct rte_flow_item_eth)),
		.next = NEXT(item_eth),
		.call = parse_vc,
	},
	[ITEM_ETH_DST] = {
		.name = "dst",
		.help = "destination MAC",
		.next = NEXT(item_eth, NEXT_ENTRY(COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.dst_addr)),
	},
	[ITEM_ETH_SRC] = {
		.name = "src",
		.help = "source MAC",
		.next = NEXT(item_eth, NEXT_ENTRY(COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.src_addr)),
	},
	[ITEM_ETH_TYPE] = {
		.name = "type",
		.help = "EtherType",
		.next = NEXT(item_eth, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, hdr.ether_type)),
	},
	[ITEM_ETH_HAS_VLAN] = {
		.name = "has_vlan",
		.help = "packet header contains VLAN",
		.next = NEXT(item_eth, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_eth,
					   has_vlan, 1)),
	},
	[ITEM_VLAN] = {
		.name = "vlan",
		.help = "match 802.1Q/ad VLAN tag",
		.priv = PRIV_ITEM(VLAN, sizeof(struct rte_flow_item_vlan)),
		.next = NEXT(item_vlan),
		.call = parse_vc,
	},
	[ITEM_VLAN_TCI] = {
		.name = "tci",
		.help = "tag control information",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan, hdr.vlan_tci)),
	},
	[ITEM_VLAN_PCP] = {
		.name = "pcp",
		.help = "priority code point",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\xe0\x00")),
	},
	[ITEM_VLAN_DEI] = {
		.name = "dei",
		.help = "drop eligible indicator",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\x10\x00")),
	},
	[ITEM_VLAN_VID] = {
		.name = "vid",
		.help = "VLAN identifier",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  hdr.vlan_tci, "\x0f\xff")),
	},
	[ITEM_VLAN_INNER_TYPE] = {
		.name = "inner_type",
		.help = "inner EtherType",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan,
					     hdr.eth_proto)),
	},
	[ITEM_VLAN_HAS_MORE_VLAN] = {
		.name = "has_more_vlan",
		.help = "packet header contains another VLAN",
		.next = NEXT(item_vlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_vlan,
					   has_more_vlan, 1)),
	},
	[ITEM_IPV4] = {
		.name = "ipv4",
		.help = "match IPv4 header",
		.priv = PRIV_ITEM(IPV4, sizeof(struct rte_flow_item_ipv4)),
		.next = NEXT(item_ipv4),
		.call = parse_vc,
	},
	[ITEM_IPV4_VER_IHL] = {
		.name = "version_ihl",
		.help = "match header length",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ipv4,
				     hdr.version_ihl)),
	},
	[ITEM_IPV4_TOS] = {
		.name = "tos",
		.help = "type of service",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.type_of_service)),
	},
	[ITEM_IPV4_ID] = {
		.name = "packet_id",
		.help = "fragment packet id",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.packet_id)),
	},
	[ITEM_IPV4_FRAGMENT_OFFSET] = {
		.name = "fragment_offset",
		.help = "fragmentation flags and fragment offset",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.fragment_offset)),
	},
	[ITEM_IPV4_TTL] = {
		.name = "ttl",
		.help = "time to live",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.time_to_live)),
	},
	[ITEM_IPV4_PROTO] = {
		.name = "proto",
		.help = "next protocol ID",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.next_proto_id)),
	},
	[ITEM_IPV4_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.src_addr)),
	},
	[ITEM_IPV4_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.dst_addr)),
	},
	[ITEM_IPV6] = {
		.name = "ipv6",
		.help = "match IPv6 header",
		.priv = PRIV_ITEM(IPV6, sizeof(struct rte_flow_item_ipv6)),
		.next = NEXT(item_ipv6),
		.call = parse_vc,
	},
	[ITEM_IPV6_TC] = {
		.name = "tc",
		.help = "traffic class",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x0f\xf0\x00\x00")),
	},
	[ITEM_IPV6_FLOW] = {
		.name = "flow",
		.help = "flow label",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x00\x0f\xff\xff")),
	},
	[ITEM_IPV6_PROTO] = {
		.name = "proto",
		.help = "protocol (next header)",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.proto)),
	},
	[ITEM_IPV6_HOP] = {
		.name = "hop",
		.help = "hop limit",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.hop_limits)),
	},
	[ITEM_IPV6_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.src_addr)),
	},
	[ITEM_IPV6_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.dst_addr)),
	},
	[ITEM_IPV6_HAS_FRAG_EXT] = {
		.name = "has_frag_ext",
		.help = "fragment packet attribute",
		.next = NEXT(item_ipv6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_ipv6,
					   has_frag_ext, 1)),
	},
	[ITEM_IPV6_ROUTING_EXT] = {
		.name = "ipv6_routing_ext",
		.help = "match IPv6 routing extension header",
		.priv = PRIV_ITEM(IPV6_ROUTING_EXT,
				  sizeof(struct rte_flow_item_ipv6_routing_ext)),
		.next = NEXT(item_ipv6_routing_ext),
		.call = parse_vc,
	},
	[ITEM_IPV6_ROUTING_EXT_TYPE] = {
		.name = "ext_type",
		.help = "match IPv6 routing extension header type",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.type)),
	},
	[ITEM_IPV6_ROUTING_EXT_NEXT_HDR] = {
		.name = "ext_next_hdr",
		.help = "match IPv6 routing extension header next header type",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.next_hdr)),
	},
	[ITEM_IPV6_ROUTING_EXT_SEG_LEFT] = {
		.name = "ext_seg_left",
		.help = "match IPv6 routing extension header segment left",
		.next = NEXT(item_ipv6_routing_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_routing_ext,
					     hdr.segments_left)),
	},
	[ITEM_ICMP] = {
		.name = "icmp",
		.help = "match ICMP header",
		.priv = PRIV_ITEM(ICMP, sizeof(struct rte_flow_item_icmp)),
		.next = NEXT(item_icmp),
		.call = parse_vc,
	},
	[ITEM_ICMP_TYPE] = {
		.name = "type",
		.help = "ICMP packet type",
		.next = NEXT(item_icmp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_type)),
	},
	[ITEM_ICMP_CODE] = {
		.name = "code",
		.help = "ICMP packet code",
		.next = NEXT(item_icmp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_code)),
	},
	[ITEM_ICMP_IDENT] = {
		.name = "ident",
		.help = "ICMP packet identifier",
		.next = NEXT(item_icmp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_ident)),
	},
	[ITEM_ICMP_SEQ] = {
		.name = "seq",
		.help = "ICMP packet sequence number",
		.next = NEXT(item_icmp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_seq_nb)),
	},
	[ITEM_UDP] = {
		.name = "udp",
		.help = "match UDP header",
		.priv = PRIV_ITEM(UDP, sizeof(struct rte_flow_item_udp)),
		.next = NEXT(item_udp),
		.call = parse_vc,
	},
	[ITEM_UDP_SRC] = {
		.name = "src",
		.help = "UDP source port",
		.next = NEXT(item_udp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_udp,
					     hdr.src_port)),
	},
	[ITEM_UDP_DST] = {
		.name = "dst",
		.help = "UDP destination port",
		.next = NEXT(item_udp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_udp,
					     hdr.dst_port)),
	},
	[ITEM_TCP] = {
		.name = "tcp",
		.help = "match TCP header",
		.priv = PRIV_ITEM(TCP, sizeof(struct rte_flow_item_tcp)),
		.next = NEXT(item_tcp),
		.call = parse_vc,
	},
	[ITEM_TCP_SRC] = {
		.name = "src",
		.help = "TCP source port",
		.next = NEXT(item_tcp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.src_port)),
	},
	[ITEM_TCP_DST] = {
		.name = "dst",
		.help = "TCP destination port",
		.next = NEXT(item_tcp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.dst_port)),
	},
	[ITEM_TCP_FLAGS] = {
		.name = "flags",
		.help = "TCP flags",
		.next = NEXT(item_tcp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.tcp_flags)),
	},
	[ITEM_SCTP] = {
		.name = "sctp",
		.help = "match SCTP header",
		.priv = PRIV_ITEM(SCTP, sizeof(struct rte_flow_item_sctp)),
		.next = NEXT(item_sctp),
		.call = parse_vc,
	},
	[ITEM_SCTP_SRC] = {
		.name = "src",
		.help = "SCTP source port",
		.next = NEXT(item_sctp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.src_port)),
	},
	[ITEM_SCTP_DST] = {
		.name = "dst",
		.help = "SCTP destination port",
		.next = NEXT(item_sctp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.dst_port)),
	},
	[ITEM_SCTP_TAG] = {
		.name = "tag",
		.help = "validation tag",
		.next = NEXT(item_sctp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.tag)),
	},
	[ITEM_SCTP_CKSUM] = {
		.name = "cksum",
		.help = "checksum",
		.next = NEXT(item_sctp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.cksum)),
	},
	[ITEM_VXLAN] = {
		.name = "vxlan",
		.help = "match VXLAN header",
		.priv = PRIV_ITEM(VXLAN, sizeof(struct rte_flow_item_vxlan)),
		.next = NEXT(item_vxlan),
		.call = parse_vc,
	},
	[ITEM_VXLAN_VNI] = {
		.name = "vni",
		.help = "VXLAN identifier",
		.next = NEXT(item_vxlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan, hdr.vni)),
	},
	[ITEM_VXLAN_LAST_RSVD] = {
		.name = "last_rsvd",
		.help = "VXLAN last reserved bits",
		.next = NEXT(item_vxlan, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan,
					     hdr.rsvd1)),
	},
	[ITEM_E_TAG] = {
		.name = "e_tag",
		.help = "match E-Tag header",
		.priv = PRIV_ITEM(E_TAG, sizeof(struct rte_flow_item_e_tag)),
		.next = NEXT(item_e_tag),
		.call = parse_vc,
	},
	[ITEM_E_TAG_GRP_ECID_B] = {
		.name = "grp_ecid_b",
		.help = "GRP and E-CID base",
		.next = NEXT(item_e_tag, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_e_tag,
						  rsvd_grp_ecid_b,
						  "\x3f\xff")),
	},
	[ITEM_NVGRE] = {
		.name = "nvgre",
		.help = "match NVGRE header",
		.priv = PRIV_ITEM(NVGRE, sizeof(struct rte_flow_item_nvgre)),
		.next = NEXT(item_nvgre),
		.call = parse_vc,
	},
	[ITEM_NVGRE_TNI] = {
		.name = "tni",
		.help = "virtual subnet ID",
		.next = NEXT(item_nvgre, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_nvgre, tni)),
	},
	[ITEM_MPLS] = {
		.name = "mpls",
		.help = "match MPLS header",
		.priv = PRIV_ITEM(MPLS, sizeof(struct rte_flow_item_mpls)),
		.next = NEXT(item_mpls),
		.call = parse_vc,
	},
	[ITEM_MPLS_LABEL] = {
		.name = "label",
		.help = "MPLS label",
		.next = NEXT(item_mpls, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\xff\xff\xf0")),
	},
	[ITEM_MPLS_TC] = {
		.name = "tc",
		.help = "MPLS Traffic Class",
		.next = NEXT(item_mpls, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\x00\x00\x0e")),
	},
	[ITEM_MPLS_S] = {
		.name = "s",
		.help = "MPLS Bottom-of-Stack",
		.next = NEXT(item_mpls, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\x00\x00\x01")),
	},
	[ITEM_GRE] = {
		.name = "gre",
		.help = "match GRE header",
		.priv = PRIV_ITEM(GRE, sizeof(struct rte_flow_item_gre)),
		.next = NEXT(item_gre),
		.call = parse_vc,
	},
	[ITEM_GRE_PROTO] = {
		.name = "protocol",
		.help = "GRE protocol type",
		.next = NEXT(item_gre, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     protocol)),
	},
	[ITEM_GRE_C_RSVD0_VER] = {
		.name = "c_rsvd0_ver",
		.help =
			"checksum (1b), undefined (1b), key bit (1b),"
			" sequence number (1b), reserved 0 (9b),"
			" version (3b)",
		.next = NEXT(item_gre, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     c_rsvd0_ver)),
	},
	[ITEM_GRE_C_BIT] = {
		.name = "c_bit",
		.help = "checksum bit (C)",
		.next = NEXT(item_gre, NEXT_ENTRY(COMMON_BOOLEAN),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x80\x00\x00\x00")),
	},
	[ITEM_GRE_S_BIT] = {
		.name = "s_bit",
		.help = "sequence number bit (S)",
		.next = NEXT(item_gre, NEXT_ENTRY(COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x10\x00\x00\x00")),
	},
	[ITEM_GRE_K_BIT] = {
		.name = "k_bit",
		.help = "key bit (K)",
		.next = NEXT(item_gre, NEXT_ENTRY(COMMON_BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x20\x00\x00\x00")),
	},
	[ITEM_FUZZY] = {
		.name = "fuzzy",
		.help = "fuzzy pattern match, expect faster than default",
		.priv = PRIV_ITEM(FUZZY,
				sizeof(struct rte_flow_item_fuzzy)),
		.next = NEXT(item_fuzzy),
		.call = parse_vc,
	},
	[ITEM_FUZZY_THRESH] = {
		.name = "thresh",
		.help = "match accuracy threshold",
		.next = NEXT(item_fuzzy, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_fuzzy,
					thresh)),
	},
	[ITEM_GTP] = {
		.name = "gtp",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTP, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[ITEM_GTP_FLAGS] = {
		.name = "v_pt_rsv_flags",
		.help = "GTP flags",
		.next = NEXT(item_gtp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp,
					hdr.gtp_hdr_info)),
	},
	[ITEM_GTP_MSG_TYPE] = {
		.name = "msg_type",
		.help = "GTP message type",
		.next = NEXT(item_gtp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp, hdr.msg_type)),
	},
	[ITEM_GTP_TEID] = {
		.name = "teid",
		.help = "tunnel endpoint identifier",
		.next = NEXT(item_gtp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gtp, hdr.teid)),
	},
	[ITEM_GTPC] = {
		.name = "gtpc",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTPC, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[ITEM_GTPU] = {
		.name = "gtpu",
		.help = "match GTP header",
		.priv = PRIV_ITEM(GTPU, sizeof(struct rte_flow_item_gtp)),
		.next = NEXT(item_gtp),
		.call = parse_vc,
	},
	[ITEM_GENEVE] = {
		.name = "geneve",
		.help = "match GENEVE header",
		.priv = PRIV_ITEM(GENEVE, sizeof(struct rte_flow_item_geneve)),
		.next = NEXT(item_geneve),
		.call = parse_vc,
	},
	[ITEM_GENEVE_VNI] = {
		.name = "vni",
		.help = "virtual network identifier",
		.next = NEXT(item_geneve, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve, vni)),
	},
	[ITEM_GENEVE_PROTO] = {
		.name = "protocol",
		.help = "GENEVE protocol type",
		.next = NEXT(item_geneve, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve,
					     protocol)),
	},
	[ITEM_GENEVE_OPTLEN] = {
		.name = "optlen",
		.help = "GENEVE options length in dwords",
		.next = NEXT(item_geneve, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_geneve,
						  ver_opt_len_o_c_rsvd0,
						  "\x3f\x00")),
	},
	[ITEM_VXLAN_GPE] = {
		.name = "vxlan-gpe",
		.help = "match VXLAN-GPE header",
		.priv = PRIV_ITEM(VXLAN_GPE,
				  sizeof(struct rte_flow_item_vxlan_gpe)),
		.next = NEXT(item_vxlan_gpe),
		.call = parse_vc,
	},
	[ITEM_VXLAN_GPE_VNI] = {
		.name = "vni",
		.help = "VXLAN-GPE identifier",
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     hdr.vni)),
	},
	[ITEM_ARP_ETH_IPV4] = {
		.name = "arp_eth_ipv4",
		.help = "match ARP header for Ethernet/IPv4",
		.priv = PRIV_ITEM(ARP_ETH_IPV4,
				  sizeof(struct rte_flow_item_arp_eth_ipv4)),
		.next = NEXT(item_arp_eth_ipv4),
		.call = parse_vc,
	},
	[ITEM_ARP_ETH_IPV4_SHA] = {
		.name = "sha",
		.help = "sender hardware address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(COMMON_MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_sha)),
	},
	[ITEM_ARP_ETH_IPV4_SPA] = {
		.name = "spa",
		.help = "sender IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_sip)),
	},
	[ITEM_ARP_ETH_IPV4_THA] = {
		.name = "tha",
		.help = "target hardware address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(COMMON_MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_tha)),
	},
	[ITEM_ARP_ETH_IPV4_TPA] = {
		.name = "tpa",
		.help = "target IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(COMMON_IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     hdr.arp_data.arp_tip)),
	},
	[ITEM_IPV6_EXT] = {
		.name = "ipv6_ext",
		.help = "match presence of any IPv6 extension header",
		.priv = PRIV_ITEM(IPV6_EXT,
				  sizeof(struct rte_flow_item_ipv6_ext)),
		.next = NEXT(item_ipv6_ext),
		.call = parse_vc,
	},
	[ITEM_IPV6_EXT_NEXT_HDR] = {
		.name = "next_hdr",
		.help = "next header",
		.next = NEXT(item_ipv6_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_ext,
					     next_hdr)),
	},
	[ITEM_IPV6_FRAG_EXT] = {
		.name = "ipv6_frag_ext",
		.help = "match presence of IPv6 fragment extension header",
		.priv = PRIV_ITEM(IPV6_FRAG_EXT,
				sizeof(struct rte_flow_item_ipv6_frag_ext)),
		.next = NEXT(item_ipv6_frag_ext),
		.call = parse_vc,
	},
	[ITEM_IPV6_FRAG_EXT_NEXT_HDR] = {
		.name = "next_hdr",
		.help = "next header",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ipv6_frag_ext,
					hdr.next_header)),
	},
	[ITEM_IPV6_FRAG_EXT_FRAG_DATA] = {
		.name = "frag_data",
		.help = "fragment flags and offset",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_frag_ext,
					     hdr.frag_data)),
	},
	[ITEM_IPV6_FRAG_EXT_ID] = {
		.name = "packet_id",
		.help = "fragment packet id",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_frag_ext,
					     hdr.id)),
	},
	[ITEM_ICMP6] = {
		.name = "icmp6",
		.help = "match any ICMPv6 header",
		.priv = PRIV_ITEM(ICMP6, sizeof(struct rte_flow_item_icmp6)),
		.next = NEXT(item_icmp6),
		.call = parse_vc,
	},
	[ITEM_ICMP6_TYPE] = {
		.name = "type",
		.help = "ICMPv6 type",
		.next = NEXT(item_icmp6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     type)),
	},
	[ITEM_ICMP6_CODE] = {
		.name = "code",
		.help = "ICMPv6 code",
		.next = NEXT(item_icmp6, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     code)),
	},
	[ITEM_ICMP6_ECHO_REQUEST] = {
		.name = "icmp6_echo_request",
		.help = "match ICMPv6 echo request",
		.priv = PRIV_ITEM(ICMP6_ECHO_REQUEST,
				  sizeof(struct rte_flow_item_icmp6_echo)),
		.next = NEXT(item_icmp6_echo_request),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ECHO_REQUEST_ID] = {
		.name = "ident",
		.help = "ICMPv6 echo request identifier",
		.next = NEXT(item_icmp6_echo_request, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.identifier)),
	},
	[ITEM_ICMP6_ECHO_REQUEST_SEQ] = {
		.name = "seq",
		.help = "ICMPv6 echo request sequence",
		.next = NEXT(item_icmp6_echo_request, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.sequence)),
	},
	[ITEM_ICMP6_ECHO_REPLY] = {
		.name = "icmp6_echo_reply",
		.help = "match ICMPv6 echo reply",
		.priv = PRIV_ITEM(ICMP6_ECHO_REPLY,
				  sizeof(struct rte_flow_item_icmp6_echo)),
		.next = NEXT(item_icmp6_echo_reply),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ECHO_REPLY_ID] = {
		.name = "ident",
		.help = "ICMPv6 echo reply identifier",
		.next = NEXT(item_icmp6_echo_reply, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.identifier)),
	},
	[ITEM_ICMP6_ECHO_REPLY_SEQ] = {
		.name = "seq",
		.help = "ICMPv6 echo reply sequence",
		.next = NEXT(item_icmp6_echo_reply, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_echo,
					     hdr.sequence)),
	},
	[ITEM_ICMP6_ND_NS] = {
		.name = "icmp6_nd_ns",
		.help = "match ICMPv6 neighbor discovery solicitation",
		.priv = PRIV_ITEM(ICMP6_ND_NS,
				  sizeof(struct rte_flow_item_icmp6_nd_ns)),
		.next = NEXT(item_icmp6_nd_ns),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ND_NS_TARGET_ADDR] = {
		.name = "target_addr",
		.help = "target address",
		.next = NEXT(item_icmp6_nd_ns, NEXT_ENTRY(COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_ns,
					     target_addr)),
	},
	[ITEM_ICMP6_ND_NA] = {
		.name = "icmp6_nd_na",
		.help = "match ICMPv6 neighbor discovery advertisement",
		.priv = PRIV_ITEM(ICMP6_ND_NA,
				  sizeof(struct rte_flow_item_icmp6_nd_na)),
		.next = NEXT(item_icmp6_nd_na),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ND_NA_TARGET_ADDR] = {
		.name = "target_addr",
		.help = "target address",
		.next = NEXT(item_icmp6_nd_na, NEXT_ENTRY(COMMON_IPV6_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_na,
					     target_addr)),
	},
	[ITEM_ICMP6_ND_OPT] = {
		.name = "icmp6_nd_opt",
		.help = "match presence of any ICMPv6 neighbor discovery"
			" option",
		.priv = PRIV_ITEM(ICMP6_ND_OPT,
				  sizeof(struct rte_flow_item_icmp6_nd_opt)),
		.next = NEXT(item_icmp6_nd_opt),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ND_OPT_TYPE] = {
		.name = "type",
		.help = "ND option type",
		.next = NEXT(item_icmp6_nd_opt, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6_nd_opt,
					     type)),
	},
	[ITEM_ICMP6_ND_OPT_SLA_ETH] = {
		.name = "icmp6_nd_opt_sla_eth",
		.help = "match ICMPv6 neighbor discovery source Ethernet"
			" link-layer address option",
		.priv = PRIV_ITEM
			(ICMP6_ND_OPT_SLA_ETH,
			 sizeof(struct rte_flow_item_icmp6_nd_opt_sla_eth)),
		.next = NEXT(item_icmp6_nd_opt_sla_eth),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ND_OPT_SLA_ETH_SLA] = {
		.name = "sla",
		.help = "source Ethernet LLA",
		.next = NEXT(item_icmp6_nd_opt_sla_eth,
			     NEXT_ENTRY(COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_icmp6_nd_opt_sla_eth, sla)),
	},
	[ITEM_ICMP6_ND_OPT_TLA_ETH] = {
		.name = "icmp6_nd_opt_tla_eth",
		.help = "match ICMPv6 neighbor discovery target Ethernet"
			" link-layer address option",
		.priv = PRIV_ITEM
			(ICMP6_ND_OPT_TLA_ETH,
			 sizeof(struct rte_flow_item_icmp6_nd_opt_tla_eth)),
		.next = NEXT(item_icmp6_nd_opt_tla_eth),
		.call = parse_vc,
	},
	[ITEM_ICMP6_ND_OPT_TLA_ETH_TLA] = {
		.name = "tla",
		.help = "target Ethernet LLA",
		.next = NEXT(item_icmp6_nd_opt_tla_eth,
			     NEXT_ENTRY(COMMON_MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_icmp6_nd_opt_tla_eth, tla)),
	},
	[ITEM_META] = {
		.name = "meta",
		.help = "match metadata header",
		.priv = PRIV_ITEM(META, sizeof(struct rte_flow_item_meta)),
		.next = NEXT(item_meta),
		.call = parse_vc,
	},
	[ITEM_META_DATA] = {
		.name = "data",
		.help = "metadata value",
		.next = NEXT(item_meta, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_MASK(struct rte_flow_item_meta,
					     data, "\xff\xff\xff\xff")),
	},
	[ITEM_GRE_KEY] = {
		.name = "gre_key",
		.help = "match GRE key",
		.priv = PRIV_ITEM(GRE_KEY, sizeof(rte_be32_t)),
		.next = NEXT(item_gre_key),
		.call = parse_vc,
	},
	[ITEM_GRE_KEY_VALUE] = {
		.name = "value",
		.help = "key value",
		.next = NEXT(item_gre_key, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
	},
	[ITEM_GRE_OPTION] = {
		.name = "gre_option",
		.help = "match GRE optional fields",
		.priv = PRIV_ITEM(GRE_OPTION,
				  sizeof(struct rte_flow_item_gre_opt)),
		.next = NEXT(item_gre_option),
		.call = parse_vc,
	},
	[ITEM_GRE_OPTION_CHECKSUM] = {
		.name = "checksum",
		.help = "match GRE checksum",
		.next = NEXT(item_gre_option, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     checksum_rsvd.checksum)),
	},
	[ITEM_GRE_OPTION_KEY] = {
		.name = "key",
		.help = "match GRE key",
		.next = NEXT(item_gre_option, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     key.key)),
	},
	[ITEM_GRE_OPTION_SEQUENCE] = {
		.name = "sequence",
		.help = "match GRE sequence",
		.next = NEXT(item_gre_option, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre_opt,
					     sequence.sequence)),
	},
	[ITEM_GTP_PSC] = {
		.name = "gtp_psc",
		.help = "match GTP extension header with type 0x85",
		.priv = PRIV_ITEM(GTP_PSC,
				sizeof(struct rte_flow_item_gtp_psc)),
		.next = NEXT(item_gtp_psc),
		.call = parse_vc,
	},
	[ITEM_GTP_PSC_QFI] = {
		.name = "qfi",
		.help = "QoS flow identifier",
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_gtp_psc,
					hdr.qfi, 6)),
	},
	[ITEM_GTP_PSC_PDU_T] = {
		.name = "pdu_t",
		.help = "PDU type",
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_gtp_psc,
					hdr.type, 4)),
	},
	[ITEM_PPPOES] = {
		.name = "pppoes",
		.help = "match PPPoE session header",
		.priv = PRIV_ITEM(PPPOES, sizeof(struct rte_flow_item_pppoe)),
		.next = NEXT(item_pppoes),
		.call = parse_vc,
	},
	[ITEM_PPPOED] = {
		.name = "pppoed",
		.help = "match PPPoE discovery header",
		.priv = PRIV_ITEM(PPPOED, sizeof(struct rte_flow_item_pppoe)),
		.next = NEXT(item_pppoed),
		.call = parse_vc,
	},
	[ITEM_PPPOE_SEID] = {
		.name = "seid",
		.help = "session identifier",
		.next = NEXT(item_pppoes, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pppoe,
					session_id)),
	},
	[ITEM_PPPOE_PROTO_ID] = {
		.name = "pppoe_proto_id",
		.help = "match PPPoE session protocol identifier",
		.priv = PRIV_ITEM(PPPOE_PROTO_ID,
				sizeof(struct rte_flow_item_pppoe_proto_id)),
		.next = NEXT(item_pppoe_proto_id, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_item_pppoe_proto_id, proto_id)),
		.call = parse_vc,
	},
	[ITEM_HIGIG2] = {
		.name = "higig2",
		.help = "matches higig2 header",
		.priv = PRIV_ITEM(HIGIG2,
				sizeof(struct rte_flow_item_higig2_hdr)),
		.next = NEXT(item_higig2),
		.call = parse_vc,
	},
	[ITEM_HIGIG2_CLASSIFICATION] = {
		.name = "classification",
		.help = "matches classification of higig2 header",
		.next = NEXT(item_higig2, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_higig2_hdr,
					hdr.ppt1.classification)),
	},
	[ITEM_HIGIG2_VID] = {
		.name = "vid",
		.help = "matches vid of higig2 header",
		.next = NEXT(item_higig2, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_higig2_hdr,
					hdr.ppt1.vid)),
	},
	[ITEM_TAG] = {
		.name = "tag",
		.help = "match tag value",
		.priv = PRIV_ITEM(TAG, sizeof(struct rte_flow_item_tag)),
		.next = NEXT(item_tag),
		.call = parse_vc,
	},
	[ITEM_TAG_DATA] = {
		.name = "data",
		.help = "tag value to match",
		.next = NEXT(item_tag, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tag, data)),
	},
	[ITEM_TAG_INDEX] = {
		.name = "index",
		.help = "index of tag array to match",
		.next = NEXT(item_tag, NEXT_ENTRY(COMMON_UNSIGNED),
			     NEXT_ENTRY(ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tag, index)),
	},
	[ITEM_L2TPV3OIP] = {
		.name = "l2tpv3oip",
		.help = "match L2TPv3 over IP header",
		.priv = PRIV_ITEM(L2TPV3OIP,
				  sizeof(struct rte_flow_item_l2tpv3oip)),
		.next = NEXT(item_l2tpv3oip),
		.call = parse_vc,
	},
	[ITEM_L2TPV3OIP_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv3oip, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv3oip,
					     session_id)),
	},
	[ITEM_ESP] = {
		.name = "esp",
		.help = "match ESP header",
		.priv = PRIV_ITEM(ESP, sizeof(struct rte_flow_item_esp)),
		.next = NEXT(item_esp),
		.call = parse_vc,
	},
	[ITEM_ESP_SPI] = {
		.name = "spi",
		.help = "security policy index",
		.next = NEXT(item_esp, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_esp,
				hdr.spi)),
	},
	[ITEM_AH] = {
		.name = "ah",
		.help = "match AH header",
		.priv = PRIV_ITEM(AH, sizeof(struct rte_flow_item_ah)),
		.next = NEXT(item_ah),
		.call = parse_vc,
	},
	[ITEM_AH_SPI] = {
		.name = "spi",
		.help = "security parameters index",
		.next = NEXT(item_ah, NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ah, spi)),
	},
	[ITEM_PFCP] = {
		.name = "pfcp",
		.help = "match pfcp header",
		.priv = PRIV_ITEM(PFCP, sizeof(struct rte_flow_item_pfcp)),
		.next = NEXT(item_pfcp),
		.call = parse_vc,
	},
	[ITEM_PFCP_S_FIELD] = {
		.name = "s_field",
		.help = "S field",
		.next = NEXT(item_pfcp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pfcp,
				s_field)),
	},
	[ITEM_PFCP_SEID] = {
		.name = "seid",
		.help = "session endpoint identifier",
		.next = NEXT(item_pfcp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pfcp, seid)),
	},
	[ITEM_ECPRI] = {
		.name = "ecpri",
		.help = "match eCPRI header",
		.priv = PRIV_ITEM(ECPRI, sizeof(struct rte_flow_item_ecpri)),
		.next = NEXT(item_ecpri),
		.call = parse_vc,
	},
	[ITEM_ECPRI_COMMON] = {
		.name = "common",
		.help = "eCPRI common header",
		.next = NEXT(item_ecpri_common),
	},
	[ITEM_ECPRI_COMMON_TYPE] = {
		.name = "type",
		.help = "type of common header",
		.next = NEXT(item_ecpri_common_type),
		.args = ARGS(ARG_ENTRY_HTON(struct rte_flow_item_ecpri)),
	},
	[ITEM_ECPRI_COMMON_TYPE_IQ_DATA] = {
		.name = "iq_data",
		.help = "Type #0: IQ Data",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_IQ_DATA_PCID,
					ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[ITEM_ECPRI_MSG_IQ_DATA_PCID] = {
		.name = "pc_id",
		.help = "Physical Channel ID",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_IQ_DATA_PCID,
				ITEM_ECPRI_COMMON, ITEM_NEXT),
				NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type0.pc_id)),
	},
	[ITEM_ECPRI_COMMON_TYPE_RTC_CTRL] = {
		.name = "rtc_ctrl",
		.help = "Type #2: Real-Time Control Data",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_RTC_CTRL_RTCID,
					ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[ITEM_ECPRI_MSG_RTC_CTRL_RTCID] = {
		.name = "rtc_id",
		.help = "Real-Time Control Data ID",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_RTC_CTRL_RTCID,
				ITEM_ECPRI_COMMON, ITEM_NEXT),
				NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type2.rtc_id)),
	},
	[ITEM_ECPRI_COMMON_TYPE_DLY_MSR] = {
		.name = "delay_measure",
		.help = "Type #5: One-Way Delay Measurement",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_DLY_MSR_MSRID,
					ITEM_NEXT)),
		.call = parse_vc_item_ecpri_type,
	},
	[ITEM_ECPRI_MSG_DLY_MSR_MSRID] = {
		.name = "msr_id",
		.help = "Measurement ID",
		.next = NEXT(NEXT_ENTRY(ITEM_ECPRI_MSG_DLY_MSR_MSRID,
				ITEM_ECPRI_COMMON, ITEM_NEXT),
				NEXT_ENTRY(COMMON_UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type5.msr_id)),
	},
	[ITEM_GENEVE_OPT] = {
		.name = "geneve-opt",
		.help = "GENEVE header option",
		.priv = PRIV_ITEM(GENEVE_OPT,
				  sizeof(struct rte_flow_item_geneve_opt) +
				  ITEM_GENEVE_OPT_DATA_SIZE),
		.next = NEXT(item_geneve_opt),
		.call = parse_vc,
	},
	[ITEM_GENEVE_OPT_CLASS]	= {
		.name = "class",
		.help = "GENEVE option class",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve_opt,
					     option_class)),
	},
	[ITEM_GENEVE_OPT_TYPE] = {
		.name = "type",
		.help = "GENEVE option type",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_geneve_opt,
					option_type)),
	},
	[ITEM_GENEVE_OPT_LENGTH] = {
		.name = "length",
		.help = "GENEVE option data length (in 32b words)",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_BOUNDED(
				struct rte_flow_item_geneve_opt, option_len,
				0, 31)),
	},
	[ITEM_GENEVE_OPT_DATA] = {
		.name = "data",
		.help = "GENEVE option data pattern",
		.next = NEXT(item_geneve_opt, NEXT_ENTRY(COMMON_HEX),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_geneve_opt, data),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB
				(sizeof(struct rte_flow_item_geneve_opt),
				ITEM_GENEVE_OPT_DATA_SIZE)),
	},
	[ITEM_INTEGRITY] = {
		.name = "integrity",
		.help = "match packet integrity",
		.priv = PRIV_ITEM(INTEGRITY,
				  sizeof(struct rte_flow_item_integrity)),
		.next = NEXT(item_integrity),
		.call = parse_vc,
	},
	[ITEM_INTEGRITY_LEVEL] = {
		.name = "level",
		.help = "integrity level",
		.next = NEXT(item_integrity_lv, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_integrity, level)),
	},
	[ITEM_INTEGRITY_VALUE] = {
		.name = "value",
		.help = "integrity value",
		.next = NEXT(item_integrity_lv, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_integrity, value)),
	},
	[ITEM_CONNTRACK] = {
		.name = "conntrack",
		.help = "conntrack state",
		.next = NEXT(NEXT_ENTRY(ITEM_NEXT), NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_conntrack, flags)),
	},
	[ITEM_PORT_REPRESENTOR] = {
		.name = "port_representor",
		.help = "match traffic entering the embedded switch from the given ethdev",
		.priv = PRIV_ITEM(PORT_REPRESENTOR,
				  sizeof(struct rte_flow_item_ethdev)),
		.next = NEXT(item_port_representor),
		.call = parse_vc,
	},
	[ITEM_PORT_REPRESENTOR_PORT_ID] = {
		.name = "port_id",
		.help = "ethdev port ID",
		.next = NEXT(item_port_representor, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ethdev, port_id)),
	},
	[ITEM_REPRESENTED_PORT] = {
		.name = "represented_port",
		.help = "match traffic entering the embedded switch from the entity represented by the given ethdev",
		.priv = PRIV_ITEM(REPRESENTED_PORT,
				  sizeof(struct rte_flow_item_ethdev)),
		.next = NEXT(item_represented_port),
		.call = parse_vc,
	},
	[ITEM_REPRESENTED_PORT_ETHDEV_PORT_ID] = {
		.name = "ethdev_port_id",
		.help = "ethdev port ID",
		.next = NEXT(item_represented_port, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ethdev, port_id)),
	},
	[ITEM_FLEX] = {
		.name = "flex",
		.help = "match flex header",
		.priv = PRIV_ITEM(FLEX, sizeof(struct rte_flow_item_flex)),
		.next = NEXT(item_flex),
		.call = parse_vc,
	},
	[ITEM_FLEX_ITEM_HANDLE] = {
		.name = "item",
		.help = "flex item handle",
		.next = NEXT(item_flex, NEXT_ENTRY(COMMON_FLEX_HANDLE),
			     NEXT_ENTRY(ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_flex, handle)),
	},
	[ITEM_FLEX_PATTERN_HANDLE] = {
		.name = "pattern",
		.help = "flex pattern handle",
		.next = NEXT(item_flex, NEXT_ENTRY(COMMON_FLEX_HANDLE),
			     NEXT_ENTRY(ITEM_PARAM_IS)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_flex, pattern)),
	},
	[ITEM_L2TPV2] = {
		.name = "l2tpv2",
		.help = "match L2TPv2 header",
		.priv = PRIV_ITEM(L2TPV2, sizeof(struct rte_flow_item_l2tpv2)),
		.next = NEXT(item_l2tpv2),
		.call = parse_vc,
	},
	[ITEM_L2TPV2_TYPE] = {
		.name = "type",
		.help = "type of l2tpv2",
		.next = NEXT(item_l2tpv2_type),
		.args = ARGS(ARG_ENTRY_HTON(struct rte_flow_item_l2tpv2)),
	},
	[ITEM_L2TPV2_TYPE_DATA] = {
		.name = "data",
		.help = "Type #7: data message without any options",
		.next = NEXT(item_l2tpv2_type_data),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_DATA_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type7.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type7.session_id)),
	},
	[ITEM_L2TPV2_TYPE_DATA_L] = {
		.name = "data_l",
		.help = "Type #6: data message with length option",
		.next = NEXT(item_l2tpv2_type_data_l),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_DATA_L_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.length)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_l,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type6.session_id)),
	},
	[ITEM_L2TPV2_TYPE_DATA_S] = {
		.name = "data_s",
		.help = "Type #5: data message with ns, nr option",
		.next = NEXT(item_l2tpv2_type_data_s),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_DATA_S_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_S_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.session_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_S_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.ns)),
	},
	[ITEM_L2TPV2_MSG_DATA_S_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_data_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.nr)),
	},
	[ITEM_L2TPV2_TYPE_DATA_O] = {
		.name = "data_o",
		.help = "Type #4: data message with offset option",
		.next = NEXT(item_l2tpv2_type_data_o),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_DATA_O_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type4.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_O_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type5.session_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_O_OFFSET] = {
		.name = "offset_size",
		.help = "the size of offset padding",
		.next = NEXT(item_l2tpv2_type_data_o,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type4.offset_size)),
	},
	[ITEM_L2TPV2_TYPE_DATA_L_S] = {
		.name = "data_l_s",
		.help = "Type #3: data message contains length, ns, nr "
			"options",
		.next = NEXT(item_l2tpv2_type_data_l_s),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_DATA_L_S_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.length)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_S_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_S_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.session_id)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_S_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.ns)),
	},
	[ITEM_L2TPV2_MSG_DATA_L_S_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_data_l_s,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.nr)),
	},
	[ITEM_L2TPV2_TYPE_CTRL] = {
		.name = "control",
		.help = "Type #3: conrtol message contains length, ns, nr "
			"options",
		.next = NEXT(item_l2tpv2_type_ctrl),
		.call = parse_vc_item_l2tpv2_type,
	},
	[ITEM_L2TPV2_MSG_CTRL_LENGTH] = {
		.name = "length",
		.help = "message length",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.length)),
	},
	[ITEM_L2TPV2_MSG_CTRL_TUNNEL_ID] = {
		.name = "tunnel_id",
		.help = "tunnel identifier",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.tunnel_id)),
	},
	[ITEM_L2TPV2_MSG_CTRL_SESSION_ID] = {
		.name = "session_id",
		.help = "session identifier",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.session_id)),
	},
	[ITEM_L2TPV2_MSG_CTRL_NS] = {
		.name = "ns",
		.help = "sequence number for message",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.ns)),
	},
	[ITEM_L2TPV2_MSG_CTRL_NR] = {
		.name = "nr",
		.help = "sequence number for next receive message",
		.next = NEXT(item_l2tpv2_type_ctrl,
			     NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_l2tpv2,
					     hdr.type3.nr)),
	},
	[ITEM_PPP] = {
		.name = "ppp",
		.help = "match PPP header",
		.priv = PRIV_ITEM(PPP, sizeof(struct rte_flow_item_ppp)),
		.next = NEXT(item_ppp),
		.call = parse_vc,
	},
	[ITEM_PPP_ADDR] = {
		.name = "addr",
		.help = "PPP address",
		.next = NEXT(item_ppp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp, hdr.addr)),
	},
	[ITEM_PPP_CTRL] = {
		.name = "ctrl",
		.help = "PPP control",
		.next = NEXT(item_ppp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp, hdr.ctrl)),
	},
	[ITEM_PPP_PROTO_ID] = {
		.name = "proto_id",
		.help = "PPP protocol identifier",
		.next = NEXT(item_ppp, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ppp,
					hdr.proto_id)),
	},
	[ITEM_METER] = {
		.name = "meter",
		.help = "match meter color",
		.priv = PRIV_ITEM(METER_COLOR,
				  sizeof(struct rte_flow_item_meter_color)),
		.next = NEXT(item_meter),
		.call = parse_vc,
	},
	[ITEM_METER_COLOR] = {
		.name = "color",
		.help = "meter color",
		.next = NEXT(item_meter,
			     NEXT_ENTRY(ITEM_METER_COLOR_NAME),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_meter_color,
					color)),
	},
	[ITEM_METER_COLOR_NAME] = {
		.name = "color_name",
		.help = "meter color name",
		.call = parse_meter_color,
		.comp = comp_meter_color,
	},
	[ITEM_QUOTA] = {
		.name = "quota",
		.help = "match quota",
		.priv = PRIV_ITEM(QUOTA, sizeof(struct rte_flow_item_quota)),
		.next = NEXT(item_quota),
		.call = parse_vc
	},
	[ITEM_QUOTA_STATE] = {
		.name = "quota_state",
		.help = "quota state",
		.next = NEXT(item_quota, NEXT_ENTRY(ITEM_QUOTA_STATE_NAME),
			     NEXT_ENTRY(ITEM_PARAM_SPEC, ITEM_PARAM_MASK)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_quota, state))
	},
	[ITEM_QUOTA_STATE_NAME] = {
		.name = "state_name",
		.help = "quota state name",
		.call = parse_quota_state_name,
		.comp = comp_quota_state_name
	},
	/* Validate/create actions. */
	[ACTIONS] = {
		.name = "actions",
		.help = "submit a list of associated actions",
		.next = NEXT(next_action),
		.call = parse_vc,
	},
	[ACTION_NEXT] = {
		.name = "/",
		.help = "specify next action",
		.next = NEXT(next_action),
	},
	[ACTION_END] = {
		.name = "end",
		.help = "end list of actions",
		.priv = PRIV_ACTION(END, 0),
		.call = parse_vc,
	},
	[ACTION_VOID] = {
		.name = "void",
		.help = "no-op action",
		.priv = PRIV_ACTION(VOID, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_PASSTHRU] = {
		.name = "passthru",
		.help = "let subsequent rule process matched packets",
		.priv = PRIV_ACTION(PASSTHRU, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_SKIP_CMAN] = {
		.name = "skip_cman",
		.help = "bypass cman on received packets",
		.priv = PRIV_ACTION(SKIP_CMAN, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_JUMP] = {
		.name = "jump",
		.help = "redirect traffic to a given group",
		.priv = PRIV_ACTION(JUMP, sizeof(struct rte_flow_action_jump)),
		.next = NEXT(action_jump),
		.call = parse_vc,
	},
	[ACTION_JUMP_GROUP] = {
		.name = "group",
		.help = "group to redirect traffic to",
		.next = NEXT(action_jump, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_jump, group)),
		.call = parse_vc_conf,
	},
	[ACTION_MARK] = {
		.name = "mark",
		.help = "attach 32 bit value to packets",
		.priv = PRIV_ACTION(MARK, sizeof(struct rte_flow_action_mark)),
		.next = NEXT(action_mark),
		.call = parse_vc,
	},
	[ACTION_MARK_ID] = {
		.name = "id",
		.help = "32 bit value to return with packets",
		.next = NEXT(action_mark, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_mark, id)),
		.call = parse_vc_conf,
	},
	[ACTION_FLAG] = {
		.name = "flag",
		.help = "flag packets",
		.priv = PRIV_ACTION(FLAG, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_QUEUE] = {
		.name = "queue",
		.help = "assign packets to a given queue index",
		.priv = PRIV_ACTION(QUEUE,
				    sizeof(struct rte_flow_action_queue)),
		.next = NEXT(action_queue),
		.call = parse_vc,
	},
	[ACTION_QUEUE_INDEX] = {
		.name = "index",
		.help = "queue index to use",
		.next = NEXT(action_queue, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_queue, index)),
		.call = parse_vc_conf,
	},
	[ACTION_DROP] = {
		.name = "drop",
		.help = "drop packets (note: passthru has priority)",
		.priv = PRIV_ACTION(DROP, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_COUNT] = {
		.name = "count",
		.help = "enable counters for this rule",
		.priv = PRIV_ACTION(COUNT,
				    sizeof(struct rte_flow_action_count)),
		.next = NEXT(action_count),
		.call = parse_vc,
	},
	[ACTION_COUNT_ID] = {
		.name = "identifier",
		.help = "counter identifier to use",
		.next = NEXT(action_count, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_count, id)),
		.call = parse_vc_conf,
	},
	[ACTION_RSS] = {
		.name = "rss",
		.help = "spread packets among several queues",
		.priv = PRIV_ACTION(RSS, sizeof(struct action_rss_data)),
		.next = NEXT(action_rss),
		.call = parse_vc_action_rss,
	},
	[ACTION_RSS_FUNC] = {
		.name = "func",
		.help = "RSS hash function to apply",
		.next = NEXT(action_rss,
			     NEXT_ENTRY(ACTION_RSS_FUNC_DEFAULT,
					ACTION_RSS_FUNC_TOEPLITZ,
					ACTION_RSS_FUNC_SIMPLE_XOR,
					ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ)),
	},
	[ACTION_RSS_FUNC_DEFAULT] = {
		.name = "default",
		.help = "default hash function",
		.call = parse_vc_action_rss_func,
	},
	[ACTION_RSS_FUNC_TOEPLITZ] = {
		.name = "toeplitz",
		.help = "Toeplitz hash function",
		.call = parse_vc_action_rss_func,
	},
	[ACTION_RSS_FUNC_SIMPLE_XOR] = {
		.name = "simple_xor",
		.help = "simple XOR hash function",
		.call = parse_vc_action_rss_func,
	},
	[ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ] = {
		.name = "symmetric_toeplitz",
		.help = "Symmetric Toeplitz hash function",
		.call = parse_vc_action_rss_func,
	},
	[ACTION_RSS_LEVEL] = {
		.name = "level",
		.help = "encapsulation level for \"types\"",
		.next = NEXT(action_rss, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, level),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     level))),
	},
	[ACTION_RSS_TYPES] = {
		.name = "types",
		.help = "specific RSS hash types",
		.next = NEXT(action_rss, NEXT_ENTRY(ACTION_RSS_TYPE)),
	},
	[ACTION_RSS_TYPE] = {
		.name = "{type}",
		.help = "RSS hash type",
		.call = parse_vc_action_rss_type,
		.comp = comp_vc_action_rss_type,
	},
	[ACTION_RSS_KEY] = {
		.name = "key",
		.help = "RSS hash key",
		.next = NEXT(action_rss, NEXT_ENTRY(COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key),
			      sizeof(((struct rte_flow_action_rss *)0)->key)),
			     ARGS_ENTRY_ARB
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key_len),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     key_len)),
			     ARGS_ENTRY(struct action_rss_data, key)),
	},
	[ACTION_RSS_KEY_LEN] = {
		.name = "key_len",
		.help = "RSS hash key length in bytes",
		.next = NEXT(action_rss, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
			     (offsetof(struct action_rss_data, conf) +
			      offsetof(struct rte_flow_action_rss, key_len),
			      sizeof(((struct rte_flow_action_rss *)0)->
				     key_len),
			      0,
			      RSS_HASH_KEY_LENGTH)),
	},
	[ACTION_RSS_QUEUES] = {
		.name = "queues",
		.help = "queue indices to use",
		.next = NEXT(action_rss, NEXT_ENTRY(ACTION_RSS_QUEUE)),
		.call = parse_vc_conf,
	},
	[ACTION_RSS_QUEUE] = {
		.name = "{queue}",
		.help = "queue index",
		.call = parse_vc_action_rss_queue,
		.comp = comp_vc_action_rss_queue,
	},
	[ACTION_PF] = {
		.name = "pf",
		.help = "direct traffic to physical function",
		.priv = PRIV_ACTION(PF, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_VF] = {
		.name = "vf",
		.help = "direct traffic to a virtual function ID",
		.priv = PRIV_ACTION(VF, sizeof(struct rte_flow_action_vf)),
		.next = NEXT(action_vf),
		.call = parse_vc,
	},
	[ACTION_VF_ORIGINAL] = {
		.name = "original",
		.help = "use original VF ID if possible",
		.next = NEXT(action_vf, NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_vf,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_VF_ID] = {
		.name = "id",
		.help = "VF ID",
		.next = NEXT(action_vf, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_vf, id)),
		.call = parse_vc_conf,
	},
	[ACTION_PORT_ID] = {
		.name = "port_id",
		.help = "direct matching traffic to a given DPDK port ID",
		.priv = PRIV_ACTION(PORT_ID,
				    sizeof(struct rte_flow_action_port_id)),
		.next = NEXT(action_port_id),
		.call = parse_vc,
	},
	[ACTION_PORT_ID_ORIGINAL] = {
		.name = "original",
		.help = "use original DPDK port ID if possible",
		.next = NEXT(action_port_id, NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_port_id,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_PORT_ID_ID] = {
		.name = "id",
		.help = "DPDK port ID",
		.next = NEXT(action_port_id, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_port_id, id)),
		.call = parse_vc_conf,
	},
	[ACTION_METER] = {
		.name = "meter",
		.help = "meter the directed packets at given id",
		.priv = PRIV_ACTION(METER,
				    sizeof(struct rte_flow_action_meter)),
		.next = NEXT(action_meter),
		.call = parse_vc,
	},
	[ACTION_METER_COLOR] = {
		.name = "color",
		.help = "meter color for the packets",
		.priv = PRIV_ACTION(METER_COLOR,
				sizeof(struct rte_flow_action_meter_color)),
		.next = NEXT(action_meter_color),
		.call = parse_vc,
	},
	[ACTION_METER_COLOR_TYPE] = {
		.name = "type",
		.help = "specific meter color",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT),
				NEXT_ENTRY(ACTION_METER_COLOR_GREEN,
					ACTION_METER_COLOR_YELLOW,
					ACTION_METER_COLOR_RED)),
	},
	[ACTION_METER_COLOR_GREEN] = {
		.name = "green",
		.help = "meter color green",
		.call = parse_vc_action_meter_color_type,
	},
	[ACTION_METER_COLOR_YELLOW] = {
		.name = "yellow",
		.help = "meter color yellow",
		.call = parse_vc_action_meter_color_type,
	},
	[ACTION_METER_COLOR_RED] = {
		.name = "red",
		.help = "meter color red",
		.call = parse_vc_action_meter_color_type,
	},
	[ACTION_METER_ID] = {
		.name = "mtr_id",
		.help = "meter id to use",
		.next = NEXT(action_meter, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter, mtr_id)),
		.call = parse_vc_conf,
	},
	[ACTION_METER_MARK] = {
		.name = "meter_mark",
		.help = "meter the directed packets using profile and policy",
		.priv = PRIV_ACTION(METER_MARK,
				    sizeof(struct rte_flow_action_meter_mark)),
		.next = NEXT(action_meter_mark),
		.call = parse_vc,
	},
	[ACTION_METER_PROFILE] = {
		.name = "mtr_profile",
		.help = "meter profile id to use",
		.next = NEXT(NEXT_ENTRY(ACTION_METER_PROFILE_ID2PTR)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[ACTION_METER_PROFILE_ID2PTR] = {
		.name = "{mtr_profile_id}",
		.type = "PROFILE_ID",
		.help = "meter profile id",
		.next = NEXT(action_meter_mark),
		.call = parse_meter_profile_id2ptr,
		.comp = comp_none,
	},
	[ACTION_METER_POLICY] = {
		.name = "mtr_policy",
		.help = "meter policy id to use",
		.next = NEXT(NEXT_ENTRY(ACTION_METER_POLICY_ID2PTR)),
		ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
	},
	[ACTION_METER_POLICY_ID2PTR] = {
		.name = "{mtr_policy_id}",
		.type = "POLICY_ID",
		.help = "meter policy id",
		.next = NEXT(action_meter_mark),
		.call = parse_meter_policy_id2ptr,
		.comp = comp_none,
	},
	[ACTION_METER_COLOR_MODE] = {
		.name = "mtr_color_mode",
		.help = "meter color awareness mode",
		.next = NEXT(action_meter_mark, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter_mark, color_mode)),
		.call = parse_vc_conf,
	},
	[ACTION_METER_INIT_COLOR] = {
		.name = "mtr_init_color",
		.help = "meter initial color",
		.next = NEXT(action_meter_mark, NEXT_ENTRY(ITEM_METER_COLOR_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter_mark, init_color)),
	},
	[ACTION_METER_STATE] = {
		.name = "mtr_state",
		.help = "meter state",
		.next = NEXT(action_meter_mark, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter_mark, state)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_DEC_NW_TTL] = {
		.name = "of_dec_nw_ttl",
		.help = "OpenFlow's OFPAT_DEC_NW_TTL",
		.priv = PRIV_ACTION(OF_DEC_NW_TTL, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_OF_POP_VLAN] = {
		.name = "of_pop_vlan",
		.help = "OpenFlow's OFPAT_POP_VLAN",
		.priv = PRIV_ACTION(OF_POP_VLAN, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_OF_PUSH_VLAN] = {
		.name = "of_push_vlan",
		.help = "OpenFlow's OFPAT_PUSH_VLAN",
		.priv = PRIV_ACTION
			(OF_PUSH_VLAN,
			 sizeof(struct rte_flow_action_of_push_vlan)),
		.next = NEXT(action_of_push_vlan),
		.call = parse_vc,
	},
	[ACTION_OF_PUSH_VLAN_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_push_vlan, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_push_vlan,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_SET_VLAN_VID] = {
		.name = "of_set_vlan_vid",
		.help = "OpenFlow's OFPAT_SET_VLAN_VID",
		.priv = PRIV_ACTION
			(OF_SET_VLAN_VID,
			 sizeof(struct rte_flow_action_of_set_vlan_vid)),
		.next = NEXT(action_of_set_vlan_vid),
		.call = parse_vc,
	},
	[ACTION_OF_SET_VLAN_VID_VLAN_VID] = {
		.name = "vlan_vid",
		.help = "VLAN id",
		.next = NEXT(action_of_set_vlan_vid,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_set_vlan_vid,
			      vlan_vid)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_SET_VLAN_PCP] = {
		.name = "of_set_vlan_pcp",
		.help = "OpenFlow's OFPAT_SET_VLAN_PCP",
		.priv = PRIV_ACTION
			(OF_SET_VLAN_PCP,
			 sizeof(struct rte_flow_action_of_set_vlan_pcp)),
		.next = NEXT(action_of_set_vlan_pcp),
		.call = parse_vc,
	},
	[ACTION_OF_SET_VLAN_PCP_VLAN_PCP] = {
		.name = "vlan_pcp",
		.help = "VLAN priority",
		.next = NEXT(action_of_set_vlan_pcp,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_set_vlan_pcp,
			      vlan_pcp)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_POP_MPLS] = {
		.name = "of_pop_mpls",
		.help = "OpenFlow's OFPAT_POP_MPLS",
		.priv = PRIV_ACTION(OF_POP_MPLS,
				    sizeof(struct rte_flow_action_of_pop_mpls)),
		.next = NEXT(action_of_pop_mpls),
		.call = parse_vc,
	},
	[ACTION_OF_POP_MPLS_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_pop_mpls, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_pop_mpls,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_PUSH_MPLS] = {
		.name = "of_push_mpls",
		.help = "OpenFlow's OFPAT_PUSH_MPLS",
		.priv = PRIV_ACTION
			(OF_PUSH_MPLS,
			 sizeof(struct rte_flow_action_of_push_mpls)),
		.next = NEXT(action_of_push_mpls),
		.call = parse_vc,
	},
	[ACTION_OF_PUSH_MPLS_ETHERTYPE] = {
		.name = "ethertype",
		.help = "EtherType",
		.next = NEXT(action_of_push_mpls, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_of_push_mpls,
			      ethertype)),
		.call = parse_vc_conf,
	},
	[ACTION_VXLAN_ENCAP] = {
		.name = "vxlan_encap",
		.help = "VXLAN encapsulation, uses configuration set by \"set"
			" vxlan\"",
		.priv = PRIV_ACTION(VXLAN_ENCAP,
				    sizeof(struct action_vxlan_encap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_vxlan_encap,
	},
	[ACTION_VXLAN_DECAP] = {
		.name = "vxlan_decap",
		.help = "Performs a decapsulation action by stripping all"
			" headers of the VXLAN tunnel network overlay from the"
			" matched flow.",
		.priv = PRIV_ACTION(VXLAN_DECAP, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_NVGRE_ENCAP] = {
		.name = "nvgre_encap",
		.help = "NVGRE encapsulation, uses configuration set by \"set"
			" nvgre\"",
		.priv = PRIV_ACTION(NVGRE_ENCAP,
				    sizeof(struct action_nvgre_encap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_nvgre_encap,
	},
	[ACTION_NVGRE_DECAP] = {
		.name = "nvgre_decap",
		.help = "Performs a decapsulation action by stripping all"
			" headers of the NVGRE tunnel network overlay from the"
			" matched flow.",
		.priv = PRIV_ACTION(NVGRE_DECAP, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_L2_ENCAP] = {
		.name = "l2_encap",
		.help = "l2 encap, uses configuration set by"
			" \"set l2_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_l2_encap,
	},
	[ACTION_L2_DECAP] = {
		.name = "l2_decap",
		.help = "l2 decap, uses configuration set by"
			" \"set l2_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_l2_decap,
	},
	[ACTION_MPLSOGRE_ENCAP] = {
		.name = "mplsogre_encap",
		.help = "mplsogre encapsulation, uses configuration set by"
			" \"set mplsogre_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_mplsogre_encap,
	},
	[ACTION_MPLSOGRE_DECAP] = {
		.name = "mplsogre_decap",
		.help = "mplsogre decapsulation, uses configuration set by"
			" \"set mplsogre_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_mplsogre_decap,
	},
	[ACTION_MPLSOUDP_ENCAP] = {
		.name = "mplsoudp_encap",
		.help = "mplsoudp encapsulation, uses configuration set by"
			" \"set mplsoudp_encap\"",
		.priv = PRIV_ACTION(RAW_ENCAP,
				    sizeof(struct action_raw_encap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_mplsoudp_encap,
	},
	[ACTION_MPLSOUDP_DECAP] = {
		.name = "mplsoudp_decap",
		.help = "mplsoudp decapsulation, uses configuration set by"
			" \"set mplsoudp_decap\"",
		.priv = PRIV_ACTION(RAW_DECAP,
				    sizeof(struct action_raw_decap_data)),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_mplsoudp_decap,
	},
	[ACTION_SET_IPV4_SRC] = {
		.name = "set_ipv4_src",
		.help = "Set a new IPv4 source address in the outermost"
			" IPv4 header",
		.priv = PRIV_ACTION(SET_IPV4_SRC,
			sizeof(struct rte_flow_action_set_ipv4)),
		.next = NEXT(action_set_ipv4_src),
		.call = parse_vc,
	},
	[ACTION_SET_IPV4_SRC_IPV4_SRC] = {
		.name = "ipv4_addr",
		.help = "new IPv4 source address to set",
		.next = NEXT(action_set_ipv4_src, NEXT_ENTRY(COMMON_IPV4_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv4, ipv4_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_IPV4_DST] = {
		.name = "set_ipv4_dst",
		.help = "Set a new IPv4 destination address in the outermost"
			" IPv4 header",
		.priv = PRIV_ACTION(SET_IPV4_DST,
			sizeof(struct rte_flow_action_set_ipv4)),
		.next = NEXT(action_set_ipv4_dst),
		.call = parse_vc,
	},
	[ACTION_SET_IPV4_DST_IPV4_DST] = {
		.name = "ipv4_addr",
		.help = "new IPv4 destination address to set",
		.next = NEXT(action_set_ipv4_dst, NEXT_ENTRY(COMMON_IPV4_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv4, ipv4_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_IPV6_SRC] = {
		.name = "set_ipv6_src",
		.help = "Set a new IPv6 source address in the outermost"
			" IPv6 header",
		.priv = PRIV_ACTION(SET_IPV6_SRC,
			sizeof(struct rte_flow_action_set_ipv6)),
		.next = NEXT(action_set_ipv6_src),
		.call = parse_vc,
	},
	[ACTION_SET_IPV6_SRC_IPV6_SRC] = {
		.name = "ipv6_addr",
		.help = "new IPv6 source address to set",
		.next = NEXT(action_set_ipv6_src, NEXT_ENTRY(COMMON_IPV6_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv6, ipv6_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_IPV6_DST] = {
		.name = "set_ipv6_dst",
		.help = "Set a new IPv6 destination address in the outermost"
			" IPv6 header",
		.priv = PRIV_ACTION(SET_IPV6_DST,
			sizeof(struct rte_flow_action_set_ipv6)),
		.next = NEXT(action_set_ipv6_dst),
		.call = parse_vc,
	},
	[ACTION_SET_IPV6_DST_IPV6_DST] = {
		.name = "ipv6_addr",
		.help = "new IPv6 destination address to set",
		.next = NEXT(action_set_ipv6_dst, NEXT_ENTRY(COMMON_IPV6_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			(struct rte_flow_action_set_ipv6, ipv6_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TP_SRC] = {
		.name = "set_tp_src",
		.help = "set a new source port number in the outermost"
			" TCP/UDP header",
		.priv = PRIV_ACTION(SET_TP_SRC,
			sizeof(struct rte_flow_action_set_tp)),
		.next = NEXT(action_set_tp_src),
		.call = parse_vc,
	},
	[ACTION_SET_TP_SRC_TP_SRC] = {
		.name = "port",
		.help = "new source port number to set",
		.next = NEXT(action_set_tp_src, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_tp, port)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TP_DST] = {
		.name = "set_tp_dst",
		.help = "set a new destination port number in the outermost"
			" TCP/UDP header",
		.priv = PRIV_ACTION(SET_TP_DST,
			sizeof(struct rte_flow_action_set_tp)),
		.next = NEXT(action_set_tp_dst),
		.call = parse_vc,
	},
	[ACTION_SET_TP_DST_TP_DST] = {
		.name = "port",
		.help = "new destination port number to set",
		.next = NEXT(action_set_tp_dst, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_tp, port)),
		.call = parse_vc_conf,
	},
	[ACTION_MAC_SWAP] = {
		.name = "mac_swap",
		.help = "Swap the source and destination MAC addresses"
			" in the outermost Ethernet header",
		.priv = PRIV_ACTION(MAC_SWAP, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_DEC_TTL] = {
		.name = "dec_ttl",
		.help = "decrease network TTL if available",
		.priv = PRIV_ACTION(DEC_TTL, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_SET_TTL] = {
		.name = "set_ttl",
		.help = "set ttl value",
		.priv = PRIV_ACTION(SET_TTL,
			sizeof(struct rte_flow_action_set_ttl)),
		.next = NEXT(action_set_ttl),
		.call = parse_vc,
	},
	[ACTION_SET_TTL_TTL] = {
		.name = "ttl_value",
		.help = "new ttl value to set",
		.next = NEXT(action_set_ttl, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_ttl, ttl_value)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_MAC_SRC] = {
		.name = "set_mac_src",
		.help = "set source mac address",
		.priv = PRIV_ACTION(SET_MAC_SRC,
			sizeof(struct rte_flow_action_set_mac)),
		.next = NEXT(action_set_mac_src),
		.call = parse_vc,
	},
	[ACTION_SET_MAC_SRC_MAC_SRC] = {
		.name = "mac_addr",
		.help = "new source mac address",
		.next = NEXT(action_set_mac_src, NEXT_ENTRY(COMMON_MAC_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_mac, mac_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_MAC_DST] = {
		.name = "set_mac_dst",
		.help = "set destination mac address",
		.priv = PRIV_ACTION(SET_MAC_DST,
			sizeof(struct rte_flow_action_set_mac)),
		.next = NEXT(action_set_mac_dst),
		.call = parse_vc,
	},
	[ACTION_SET_MAC_DST_MAC_DST] = {
		.name = "mac_addr",
		.help = "new destination mac address to set",
		.next = NEXT(action_set_mac_dst, NEXT_ENTRY(COMMON_MAC_ADDR)),
		.args = ARGS(ARGS_ENTRY_HTON
			     (struct rte_flow_action_set_mac, mac_addr)),
		.call = parse_vc_conf,
	},
	[ACTION_INC_TCP_SEQ] = {
		.name = "inc_tcp_seq",
		.help = "increase TCP sequence number",
		.priv = PRIV_ACTION(INC_TCP_SEQ, sizeof(rte_be32_t)),
		.next = NEXT(action_inc_tcp_seq),
		.call = parse_vc,
	},
	[ACTION_INC_TCP_SEQ_VALUE] = {
		.name = "value",
		.help = "the value to increase TCP sequence number by",
		.next = NEXT(action_inc_tcp_seq, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[ACTION_DEC_TCP_SEQ] = {
		.name = "dec_tcp_seq",
		.help = "decrease TCP sequence number",
		.priv = PRIV_ACTION(DEC_TCP_SEQ, sizeof(rte_be32_t)),
		.next = NEXT(action_dec_tcp_seq),
		.call = parse_vc,
	},
	[ACTION_DEC_TCP_SEQ_VALUE] = {
		.name = "value",
		.help = "the value to decrease TCP sequence number by",
		.next = NEXT(action_dec_tcp_seq, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[ACTION_INC_TCP_ACK] = {
		.name = "inc_tcp_ack",
		.help = "increase TCP acknowledgment number",
		.priv = PRIV_ACTION(INC_TCP_ACK, sizeof(rte_be32_t)),
		.next = NEXT(action_inc_tcp_ack),
		.call = parse_vc,
	},
	[ACTION_INC_TCP_ACK_VALUE] = {
		.name = "value",
		.help = "the value to increase TCP acknowledgment number by",
		.next = NEXT(action_inc_tcp_ack, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[ACTION_DEC_TCP_ACK] = {
		.name = "dec_tcp_ack",
		.help = "decrease TCP acknowledgment number",
		.priv = PRIV_ACTION(DEC_TCP_ACK, sizeof(rte_be32_t)),
		.next = NEXT(action_dec_tcp_ack),
		.call = parse_vc,
	},
	[ACTION_DEC_TCP_ACK_VALUE] = {
		.name = "value",
		.help = "the value to decrease TCP acknowledgment number by",
		.next = NEXT(action_dec_tcp_ack, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
		.call = parse_vc_conf,
	},
	[ACTION_RAW_ENCAP] = {
		.name = "raw_encap",
		.help = "encapsulation data, defined by set raw_encap",
		.priv = PRIV_ACTION(RAW_ENCAP,
			sizeof(struct action_raw_encap_data)),
		.next = NEXT(action_raw_encap),
		.call = parse_vc_action_raw_encap,
	},
	[ACTION_RAW_ENCAP_INDEX] = {
		.name = "index",
		.help = "the index of raw_encap_confs",
		.next = NEXT(NEXT_ENTRY(ACTION_RAW_ENCAP_INDEX_VALUE)),
	},
	[ACTION_RAW_ENCAP_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_raw_encap_index,
		.comp = comp_set_raw_index,
	},
	[ACTION_RAW_DECAP] = {
		.name = "raw_decap",
		.help = "decapsulation data, defined by set raw_encap",
		.priv = PRIV_ACTION(RAW_DECAP,
			sizeof(struct action_raw_decap_data)),
		.next = NEXT(action_raw_decap),
		.call = parse_vc_action_raw_decap,
	},
	[ACTION_RAW_DECAP_INDEX] = {
		.name = "index",
		.help = "the index of raw_encap_confs",
		.next = NEXT(NEXT_ENTRY(ACTION_RAW_DECAP_INDEX_VALUE)),
	},
	[ACTION_RAW_DECAP_INDEX_VALUE] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_raw_decap_index,
		.comp = comp_set_raw_index,
	},
	[ACTION_MODIFY_FIELD] = {
		.name = "modify_field",
		.help = "modify destination field with data from source field",
		.priv = PRIV_ACTION(MODIFY_FIELD, ACTION_MODIFY_SIZE),
		.next = NEXT(NEXT_ENTRY(ACTION_MODIFY_FIELD_OP)),
		.call = parse_vc,
	},
	[ACTION_MODIFY_FIELD_OP] = {
		.name = "op",
		.help = "operation type",
		.next = NEXT(NEXT_ENTRY(ACTION_MODIFY_FIELD_DST_TYPE),
			NEXT_ENTRY(ACTION_MODIFY_FIELD_OP_VALUE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_OP_VALUE] = {
		.name = "{operation}",
		.help = "operation type value",
		.call = parse_vc_modify_field_op,
		.comp = comp_set_modify_field_op,
	},
	[ACTION_MODIFY_FIELD_DST_TYPE] = {
		.name = "dst_type",
		.help = "destination field type",
		.next = NEXT(action_modify_field_dst,
			NEXT_ENTRY(ACTION_MODIFY_FIELD_DST_TYPE_VALUE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_DST_TYPE_VALUE] = {
		.name = "{dst_type}",
		.help = "destination field type value",
		.call = parse_vc_modify_field_id,
		.comp = comp_set_modify_field_id,
	},
	[ACTION_MODIFY_FIELD_DST_LEVEL] = {
		.name = "dst_level",
		.help = "destination field level",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(ACTION_MODIFY_FIELD_DST_LEVEL_VALUE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_DST_LEVEL_VALUE] = {
		.name = "{dst_level}",
		.help = "destination field level value",
		.call = parse_vc_modify_field_level,
		.comp = comp_none,
	},
	[ACTION_MODIFY_FIELD_DST_OFFSET] = {
		.name = "dst_offset",
		.help = "destination field bit offset",
		.next = NEXT(action_modify_field_dst,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					dst.offset)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_SRC_TYPE] = {
		.name = "src_type",
		.help = "source field type",
		.next = NEXT(action_modify_field_src,
			NEXT_ENTRY(ACTION_MODIFY_FIELD_SRC_TYPE_VALUE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_SRC_TYPE_VALUE] = {
		.name = "{src_type}",
		.help = "source field type value",
		.call = parse_vc_modify_field_id,
		.comp = comp_set_modify_field_id,
	},
	[ACTION_MODIFY_FIELD_SRC_LEVEL] = {
		.name = "src_level",
		.help = "source field level",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE] = {
		.name = "{src_level}",
		.help = "source field level value",
		.call = parse_vc_modify_field_level,
		.comp = comp_none,
	},
	[ACTION_MODIFY_FIELD_SRC_OFFSET] = {
		.name = "src_offset",
		.help = "source field bit offset",
		.next = NEXT(action_modify_field_src,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.offset)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_SRC_VALUE] = {
		.name = "src_value",
		.help = "source immediate value",
		.next = NEXT(NEXT_ENTRY(ACTION_MODIFY_FIELD_WIDTH),
			     NEXT_ENTRY(COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.value)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_SRC_POINTER] = {
		.name = "src_ptr",
		.help = "pointer to source immediate value",
		.next = NEXT(NEXT_ENTRY(ACTION_MODIFY_FIELD_WIDTH),
			     NEXT_ENTRY(COMMON_HEX)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					src.pvalue),
			     ARGS_ENTRY_ARB(0, 0),
			     ARGS_ENTRY_ARB
				(sizeof(struct rte_flow_action_modify_field),
				 ACTION_MODIFY_PATTERN_SIZE)),
		.call = parse_vc_conf,
	},
	[ACTION_MODIFY_FIELD_WIDTH] = {
		.name = "width",
		.help = "number of bits to copy",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT),
			NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_modify_field,
					width)),
		.call = parse_vc_conf,
	},
	[ACTION_SEND_TO_KERNEL] = {
		.name = "send_to_kernel",
		.help = "send packets to kernel",
		.priv = PRIV_ACTION(SEND_TO_KERNEL, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	/* Top level command. */
	[SET] = {
		.name = "set",
		.help = "set raw encap/decap/sample data",
		.type = "set raw_encap|raw_decap <index> <pattern>"
				" or set sample_actions <index> <action>",
		.next = NEXT(NEXT_ENTRY
			     (SET_RAW_ENCAP,
			      SET_RAW_DECAP,
			      SET_SAMPLE_ACTIONS)),
		.call = parse_set_init,
	},
	/* Sub-level commands. */
	[SET_RAW_ENCAP] = {
		.name = "raw_encap",
		.help = "set raw encap data",
		.next = NEXT(next_set_raw),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct buffer, port),
				 sizeof(((struct buffer *)0)->port),
				 0, RAW_ENCAP_CONFS_MAX_NUM - 1)),
		.call = parse_set_raw_encap_decap,
	},
	[SET_RAW_DECAP] = {
		.name = "raw_decap",
		.help = "set raw decap data",
		.next = NEXT(next_set_raw),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct buffer, port),
				 sizeof(((struct buffer *)0)->port),
				 0, RAW_ENCAP_CONFS_MAX_NUM - 1)),
		.call = parse_set_raw_encap_decap,
	},
	[SET_RAW_INDEX] = {
		.name = "{index}",
		.type = "COMMON_UNSIGNED",
		.help = "index of raw_encap/raw_decap data",
		.next = NEXT(next_item),
		.call = parse_port,
	},
	[SET_SAMPLE_INDEX] = {
		.name = "{index}",
		.type = "UNSIGNED",
		.help = "index of sample actions",
		.next = NEXT(next_action_sample),
		.call = parse_port,
	},
	[SET_SAMPLE_ACTIONS] = {
		.name = "sample_actions",
		.help = "set sample actions list",
		.next = NEXT(NEXT_ENTRY(SET_SAMPLE_INDEX)),
		.args = ARGS(ARGS_ENTRY_ARB_BOUNDED
				(offsetof(struct buffer, port),
				 sizeof(((struct buffer *)0)->port),
				 0, RAW_SAMPLE_CONFS_MAX_NUM - 1)),
		.call = parse_set_sample_action,
	},
	[ACTION_SET_TAG] = {
		.name = "set_tag",
		.help = "set tag",
		.priv = PRIV_ACTION(SET_TAG,
			sizeof(struct rte_flow_action_set_tag)),
		.next = NEXT(action_set_tag),
		.call = parse_vc,
	},
	[ACTION_SET_TAG_INDEX] = {
		.name = "index",
		.help = "index of tag array",
		.next = NEXT(action_set_tag, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_set_tag, index)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TAG_DATA] = {
		.name = "data",
		.help = "tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_tag, data)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TAG_MASK] = {
		.name = "mask",
		.help = "mask for tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_tag, mask)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_META] = {
		.name = "set_meta",
		.help = "set metadata",
		.priv = PRIV_ACTION(SET_META,
			sizeof(struct rte_flow_action_set_meta)),
		.next = NEXT(action_set_meta),
		.call = parse_vc_action_set_meta,
	},
	[ACTION_SET_META_DATA] = {
		.name = "data",
		.help = "metadata value",
		.next = NEXT(action_set_meta, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_meta, data)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_META_MASK] = {
		.name = "mask",
		.help = "mask for metadata value",
		.next = NEXT(action_set_meta, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_meta, mask)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_IPV4_DSCP] = {
		.name = "set_ipv4_dscp",
		.help = "set DSCP value",
		.priv = PRIV_ACTION(SET_IPV4_DSCP,
			sizeof(struct rte_flow_action_set_dscp)),
		.next = NEXT(action_set_ipv4_dscp),
		.call = parse_vc,
	},
	[ACTION_SET_IPV4_DSCP_VALUE] = {
		.name = "dscp_value",
		.help = "new IPv4 DSCP value to set",
		.next = NEXT(action_set_ipv4_dscp, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_dscp, dscp)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_IPV6_DSCP] = {
		.name = "set_ipv6_dscp",
		.help = "set DSCP value",
		.priv = PRIV_ACTION(SET_IPV6_DSCP,
			sizeof(struct rte_flow_action_set_dscp)),
		.next = NEXT(action_set_ipv6_dscp),
		.call = parse_vc,
	},
	[ACTION_SET_IPV6_DSCP_VALUE] = {
		.name = "dscp_value",
		.help = "new IPv6 DSCP value to set",
		.next = NEXT(action_set_ipv6_dscp, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_dscp, dscp)),
		.call = parse_vc_conf,
	},
	[ACTION_AGE] = {
		.name = "age",
		.help = "set a specific metadata header",
		.next = NEXT(action_age),
		.priv = PRIV_ACTION(AGE,
			sizeof(struct rte_flow_action_age)),
		.call = parse_vc,
	},
	[ACTION_AGE_TIMEOUT] = {
		.name = "timeout",
		.help = "flow age timeout value",
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_age,
					   timeout, 24)),
		.next = NEXT(action_age, NEXT_ENTRY(COMMON_UNSIGNED)),
		.call = parse_vc_conf,
	},
	[ACTION_AGE_UPDATE] = {
		.name = "age_update",
		.help = "update aging parameter",
		.next = NEXT(action_age_update),
		.priv = PRIV_ACTION(AGE,
				    sizeof(struct rte_flow_update_age)),
		.call = parse_vc,
	},
	[ACTION_AGE_UPDATE_TIMEOUT] = {
		.name = "timeout",
		.help = "age timeout update value",
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_update_age,
					   timeout, 24)),
		.next = NEXT(action_age_update, NEXT_ENTRY(COMMON_UNSIGNED)),
		.call = parse_vc_conf_timeout,
	},
	[ACTION_AGE_UPDATE_TOUCH] = {
		.name = "touch",
		.help = "this flow is touched",
		.next = NEXT(action_age_update, NEXT_ENTRY(COMMON_BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_update_age,
					   touch, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_SAMPLE] = {
		.name = "sample",
		.help = "set a sample action",
		.next = NEXT(action_sample),
		.priv = PRIV_ACTION(SAMPLE,
			sizeof(struct action_sample_data)),
		.call = parse_vc_action_sample,
	},
	[ACTION_SAMPLE_RATIO] = {
		.name = "ratio",
		.help = "flow sample ratio value",
		.next = NEXT(action_sample, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY_ARB
			     (offsetof(struct action_sample_data, conf) +
			      offsetof(struct rte_flow_action_sample, ratio),
			      sizeof(((struct rte_flow_action_sample *)0)->
				     ratio))),
	},
	[ACTION_SAMPLE_INDEX] = {
		.name = "index",
		.help = "the index of sample actions list",
		.next = NEXT(NEXT_ENTRY(ACTION_SAMPLE_INDEX_VALUE)),
	},
	[ACTION_SAMPLE_INDEX_VALUE] = {
		.name = "{index}",
		.type = "COMMON_UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_sample_index,
		.comp = comp_set_sample_index,
	},
	[ACTION_CONNTRACK] = {
		.name = "conntrack",
		.help = "create a conntrack object",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.priv = PRIV_ACTION(CONNTRACK,
				    sizeof(struct rte_flow_action_conntrack)),
		.call = parse_vc,
	},
	[ACTION_CONNTRACK_UPDATE] = {
		.name = "conntrack_update",
		.help = "update a conntrack object",
		.next = NEXT(action_update_conntrack),
		.priv = PRIV_ACTION(CONNTRACK,
				    sizeof(struct rte_flow_modify_conntrack)),
		.call = parse_vc,
	},
	[ACTION_CONNTRACK_UPDATE_DIR] = {
		.name = "dir",
		.help = "update a conntrack object direction",
		.next = NEXT(action_update_conntrack),
		.call = parse_vc_action_conntrack_update,
	},
	[ACTION_CONNTRACK_UPDATE_CTX] = {
		.name = "ctx",
		.help = "update a conntrack object context",
		.next = NEXT(action_update_conntrack),
		.call = parse_vc_action_conntrack_update,
	},
	[ACTION_PORT_REPRESENTOR] = {
		.name = "port_representor",
		.help = "at embedded switch level, send matching traffic to the given ethdev",
		.priv = PRIV_ACTION(PORT_REPRESENTOR,
				    sizeof(struct rte_flow_action_ethdev)),
		.next = NEXT(action_port_representor),
		.call = parse_vc,
	},
	[ACTION_PORT_REPRESENTOR_PORT_ID] = {
		.name = "port_id",
		.help = "ethdev port ID",
		.next = NEXT(action_port_representor,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_ethdev,
					port_id)),
		.call = parse_vc_conf,
	},
	[ACTION_REPRESENTED_PORT] = {
		.name = "represented_port",
		.help = "at embedded switch level, send matching traffic to the entity represented by the given ethdev",
		.priv = PRIV_ACTION(REPRESENTED_PORT,
				sizeof(struct rte_flow_action_ethdev)),
		.next = NEXT(action_represented_port),
		.call = parse_vc,
	},
	[ACTION_REPRESENTED_PORT_ETHDEV_PORT_ID] = {
		.name = "ethdev_port_id",
		.help = "ethdev port ID",
		.next = NEXT(action_represented_port,
			     NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_ethdev,
					port_id)),
		.call = parse_vc_conf,
	},
	/* Indirect action destroy arguments. */
	[INDIRECT_ACTION_DESTROY_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to destroy",
		.next = NEXT(next_ia_destroy_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.ia_destroy.action_id)),
		.call = parse_ia_destroy,
	},
	/* Indirect action create arguments. */
	[INDIRECT_ACTION_CREATE_ID] = {
		.name = "action_id",
		.help = "specify a indirect action id to create",
		.next = NEXT(next_ia_create_attr,
			     NEXT_ENTRY(COMMON_INDIRECT_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
	},
	[ACTION_INDIRECT] = {
		.name = "indirect",
		.help = "apply indirect action by id",
		.priv = PRIV_ACTION(INDIRECT, 0),
		.next = NEXT(next_ia),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
		.call = parse_vc,
	},
	[ACTION_SHARED_INDIRECT] = {
		.name = "shared_indirect",
		.help = "apply indirect action by id and port",
		.priv = PRIV_ACTION(INDIRECT, 0),
		.next = NEXT(NEXT_ENTRY(INDIRECT_ACTION_PORT)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t)),
			     ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
		.call = parse_vc,
	},
	[INDIRECT_ACTION_PORT] = {
		.name = "{indirect_action_port}",
		.type = "INDIRECT_ACTION_PORT",
		.help = "indirect action port",
		.next = NEXT(NEXT_ENTRY(INDIRECT_ACTION_ID2PTR)),
		.call = parse_ia_port,
		.comp = comp_none,
	},
	[INDIRECT_ACTION_ID2PTR] = {
		.name = "{action_id}",
		.type = "INDIRECT_ACTION_ID",
		.help = "indirect action id",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_ia_id2ptr,
		.comp = comp_none,
	},
	[INDIRECT_ACTION_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_ia_create_attr),
		.call = parse_ia,
	},
	[INDIRECT_ACTION_SPEC] = {
		.name = "action",
		.help = "specify action to create indirect handle",
		.next = NEXT(next_action),
	},
	[ACTION_POL_G] = {
		.name = "g_actions",
		.help = "submit a list of associated actions for green",
		.next = NEXT(next_action),
		.call = parse_mp,
	},
	[ACTION_POL_Y] = {
		.name = "y_actions",
		.help = "submit a list of associated actions for yellow",
		.next = NEXT(next_action),
	},
	[ACTION_POL_R] = {
		.name = "r_actions",
		.help = "submit a list of associated actions for red",
		.next = NEXT(next_action),
	},
	[ACTION_QUOTA_CREATE] = {
		.name = "quota_create",
		.help = "create quota action",
		.priv = PRIV_ACTION(QUOTA,
				    sizeof(struct rte_flow_action_quota)),
		.next = NEXT(action_quota_create),
		.call = parse_vc
	},
	[ACTION_QUOTA_CREATE_LIMIT] = {
		.name = "limit",
		.help = "quota limit",
		.next = NEXT(action_quota_create, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_quota, quota)),
		.call = parse_vc_conf
	},
	[ACTION_QUOTA_CREATE_MODE] = {
		.name = "mode",
		.help = "quota mode",
		.next = NEXT(action_quota_create,
			     NEXT_ENTRY(ACTION_QUOTA_CREATE_MODE_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_quota, mode)),
		.call = parse_vc_conf
	},
	[ACTION_QUOTA_CREATE_MODE_NAME] = {
		.name = "mode_name",
		.help = "quota mode name",
		.call = parse_quota_mode_name,
		.comp = comp_quota_mode_name
	},
	[ACTION_QUOTA_QU] = {
		.name = "quota_update",
		.help = "update quota action",
		.priv = PRIV_ACTION(QUOTA,
				    sizeof(struct rte_flow_update_quota)),
		.next = NEXT(action_quota_update),
		.call = parse_vc
	},
	[ACTION_QUOTA_QU_LIMIT] = {
		.name = "limit",
		.help = "quota limit",
		.next = NEXT(action_quota_update, NEXT_ENTRY(COMMON_UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_update_quota, quota)),
		.call = parse_vc_conf
	},
	[ACTION_QUOTA_QU_UPDATE_OP] = {
		.name = "update_op",
		.help = "query update op SET|ADD",
		.next = NEXT(action_quota_update,
			     NEXT_ENTRY(ACTION_QUOTA_QU_UPDATE_OP_NAME)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_update_quota, op)),
		.call = parse_vc_conf
	},
	[ACTION_QUOTA_QU_UPDATE_OP_NAME] = {
		.name = "update_op_name",
		.help = "quota update op name",
		.call = parse_quota_update_name,
		.comp = comp_quota_update_name
	},

	/* Top-level command. */
	[ADD] = {
		.name = "add",
		.type = "port meter policy {port_id} {arg}",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(ITEM_POL_PORT)),
		.call = parse_init,
	},
	/* Sub-level commands. */
	[ITEM_POL_PORT] = {
		.name = "port",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(ITEM_POL_METER)),
	},
	[ITEM_POL_METER] = {
		.name = "meter",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(ITEM_POL_POLICY)),
	},
	[ITEM_POL_POLICY] = {
		.name = "policy",
		.help = "add port meter policy",
		.next = NEXT(NEXT_ENTRY(ACTION_POL_R),
				NEXT_ENTRY(ACTION_POL_Y),
				NEXT_ENTRY(ACTION_POL_G),
				NEXT_ENTRY(COMMON_POLICY_ID),
				NEXT_ENTRY(COMMON_PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.policy.policy_id),
				ARGS_ENTRY(struct buffer, port)),
		.call = parse_mp,
	},
	[ITEM_AGGR_AFFINITY] = {
		.name = "aggr_affinity",
		.help = "match on the aggregated port receiving the packets",
		.priv = PRIV_ITEM(AGGR_AFFINITY,
				  sizeof(struct rte_flow_item_aggr_affinity)),
		.next = NEXT(item_aggr_affinity),
		.call = parse_vc,
	},
	[ITEM_AGGR_AFFINITY_VALUE] = {
		.name = "affinity",
		.help = "aggregated affinity value",
		.next = NEXT(item_aggr_affinity, NEXT_ENTRY(COMMON_UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_aggr_affinity,
					affinity)),
	},
};

/** Remove and return last entry from argument stack. */
static const struct arg *
pop_args(struct context *ctx)
{
	return ctx->args_num ? ctx->args[--ctx->args_num] : NULL;
}

/** Add entry on top of the argument stack. */
static int
push_args(struct context *ctx, const struct arg *arg)
{
	if (ctx->args_num == CTX_STACK_SIZE)
		return -1;
	ctx->args[ctx->args_num++] = arg;
	return 0;
}

/** Spread value into buffer according to bit-mask. */
static size_t
arg_entry_bf_fill(void *dst, uintmax_t val, const struct arg *arg)
{
	uint32_t i = arg->size;
	uint32_t end = 0;
	int sub = 1;
	int add = 0;
	size_t len = 0;

	if (!arg->mask)
		return 0;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (!arg->hton) {
		i = 0;
		end = arg->size;
		sub = 0;
		add = 1;
	}
#endif
	while (i != end) {
		unsigned int shift = 0;
		uint8_t *buf = (uint8_t *)dst + arg->offset + (i -= sub);

		for (shift = 0; arg->mask[i] >> shift; ++shift) {
			if (!(arg->mask[i] & (1 << shift)))
				continue;
			++len;
			if (!dst)
				continue;
			*buf &= ~(1 << shift);
			*buf |= (val & 1) << shift;
			val >>= 1;
		}
		i += add;
	}
	return len;
}

/** Compare a string with a partial one of a given length. */
static int
strcmp_partial(const char *full, const char *partial, size_t partial_len)
{
	int r = strncmp(full, partial, partial_len);

	if (r)
		return r;
	if (strlen(full) <= partial_len)
		return 0;
	return full[partial_len];
}

/**
 * Parse a prefix length and generate a bit-mask.
 *
 * Last argument (ctx->args) is retrieved to determine mask size, storage
 * location and whether the result must use network byte ordering.
 */
static int
parse_prefix(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	static const uint8_t conv[] = "\x00\x80\xc0\xe0\xf0\xf8\xfc\xfe\xff";
	char *end;
	uintmax_t u;
	unsigned int bytes;
	unsigned int extra;

	(void)token;
	/* Argument is expected. */
	if (!arg)
		return -1;
	errno = 0;
	u = strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		goto error;
	if (arg->mask) {
		uintmax_t v = 0;

		extra = arg_entry_bf_fill(NULL, 0, arg);
		if (u > extra)
			goto error;
		if (!ctx->object)
			return len;
		extra -= u;
		while (u--)
			(v <<= 1, v |= 1);
		v <<= extra;
		if (!arg_entry_bf_fill(ctx->object, v, arg) ||
		    !arg_entry_bf_fill(ctx->objmask, -1, arg))
			goto error;
		return len;
	}
	bytes = u / 8;
	extra = u % 8;
	size = arg->size;
	if (bytes > size || bytes + !!extra > size)
		goto error;
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (!arg->hton) {
		memset((uint8_t *)buf + size - bytes, 0xff, bytes);
		memset(buf, 0x00, size - bytes);
		if (extra)
			((uint8_t *)buf)[size - bytes - 1] = conv[extra];
	} else
#endif
	{
		memset(buf, 0xff, bytes);
		memset((uint8_t *)buf + bytes, 0x00, size - bytes);
		if (extra)
			((uint8_t *)buf)[bytes] = conv[extra];
	}
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/** Default parsing function for token name matching. */
static int
parse_default(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	(void)ctx;
	(void)buf;
	(void)size;
	if (strcmp_partial(token->name, str, len))
		return -1;
	return len;
}

/** Parse flow command, initialize output buffer for subsequent tokens. */
static int
parse_init(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	/* Initialize buffer. */
	memset(out, 0x00, sizeof(*out));
	memset((uint8_t *)out + sizeof(*out), 0x22, size - sizeof(*out));
	ctx->objdata = 0;
	ctx->object = out;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for indirect action commands. */
static int
parse_ia(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != INDIRECT_ACTION)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case INDIRECT_ACTION_CREATE:
	case INDIRECT_ACTION_UPDATE:
	case INDIRECT_ACTION_QUERY_UPDATE:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.attr.group = UINT32_MAX;
		/* fallthrough */
	case INDIRECT_ACTION_QUERY:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case INDIRECT_ACTION_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case INDIRECT_ACTION_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case INDIRECT_ACTION_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case INDIRECT_ACTION_QU_MODE:
		return len;
	default:
		return -1;
	}
}


/** Parse tokens for indirect action destroy command. */
static int
parse_ia_destroy(struct context *ctx, const struct token *token,
		 const char *str, unsigned int len,
		 void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *action_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command || out->command == INDIRECT_ACTION) {
		if (ctx->curr != INDIRECT_ACTION_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.ia_destroy.action_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	action_id = out->args.ia_destroy.action_id
		    + out->args.ia_destroy.action_id_n++;
	if ((uint8_t *)action_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = action_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for indirect action commands. */
static int
parse_qia(struct context *ctx, const struct token *token,
	  const char *str, unsigned int len,
	  void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != QUEUE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case QUEUE_INDIRECT_ACTION:
		return len;
	case QUEUE_INDIRECT_ACTION_CREATE:
	case QUEUE_INDIRECT_ACTION_UPDATE:
	case QUEUE_INDIRECT_ACTION_QUERY_UPDATE:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.attr.group = UINT32_MAX;
		/* fallthrough */
	case QUEUE_INDIRECT_ACTION_QUERY:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case QUEUE_INDIRECT_ACTION_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case QUEUE_INDIRECT_ACTION_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case QUEUE_INDIRECT_ACTION_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case QUEUE_INDIRECT_ACTION_CREATE_POSTPONE:
		return len;
	case QUEUE_INDIRECT_ACTION_QU_MODE:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for indirect action destroy command. */
static int
parse_qia_destroy(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len,
		  void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *action_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command || out->command == QUEUE) {
		if (ctx->curr != QUEUE_INDIRECT_ACTION_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.ia_destroy.action_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	switch (ctx->curr) {
	case QUEUE_INDIRECT_ACTION:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case QUEUE_INDIRECT_ACTION_DESTROY_ID:
		action_id = out->args.ia_destroy.action_id
				+ out->args.ia_destroy.action_id_n++;
		if ((uint8_t *)action_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = action_id;
		ctx->objmask = NULL;
		return len;
	case QUEUE_INDIRECT_ACTION_DESTROY_POSTPONE:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for meter policy action commands. */
static int
parse_mp(struct context *ctx, const struct token *token,
	const char *str, unsigned int len,
	void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != ITEM_POL_POLICY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case ACTION_POL_G:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					sizeof(double));
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for validate/create commands. */
static int
parse_vc(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint8_t *data;
	uint32_t data_size;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != VALIDATE && ctx->curr != CREATE &&
		    ctx->curr != PATTERN_TEMPLATE_CREATE &&
		    ctx->curr != ACTIONS_TEMPLATE_CREATE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	ctx->objdata = 0;
	switch (ctx->curr) {
	default:
		ctx->object = &out->args.vc.attr;
		break;
	case VC_TUNNEL_SET:
	case VC_TUNNEL_MATCH:
		ctx->object = &out->args.vc.tunnel_ops;
		break;
	}
	ctx->objmask = NULL;
	switch (ctx->curr) {
	case VC_GROUP:
	case VC_PRIORITY:
		return len;
	case VC_TUNNEL_SET:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.actions = 1;
		return len;
	case VC_TUNNEL_MATCH:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.items = 1;
		return len;
	case VC_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case VC_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case VC_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case ITEM_PATTERN:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.pattern;
		ctx->objmask = NULL;
		return len;
	case ITEM_END:
		if ((out->command == VALIDATE || out->command == CREATE) &&
		    ctx->last)
			return -1;
		if (out->command == PATTERN_TEMPLATE_CREATE &&
		    !ctx->last)
			return -1;
		break;
	case ACTIONS:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.pattern +
						out->args.vc.pattern_n),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	default:
		if (!token->priv)
			return -1;
		break;
	}
	if (!out->args.vc.actions) {
		const struct parse_item_priv *priv = token->priv;
		struct rte_flow_item *item =
			out->args.vc.pattern + out->args.vc.pattern_n;

		data_size = priv->size * 3; /* spec, last, mask */
		data = (void *)RTE_ALIGN_FLOOR((uintptr_t)
					       (out->args.vc.data - data_size),
					       sizeof(double));
		if ((uint8_t *)item + sizeof(*item) > data)
			return -1;
		*item = (struct rte_flow_item){
			.type = priv->type,
		};
		++out->args.vc.pattern_n;
		ctx->object = item;
		ctx->objmask = NULL;
	} else {
		const struct parse_action_priv *priv = token->priv;
		struct rte_flow_action *action =
			out->args.vc.actions + out->args.vc.actions_n;

		data_size = priv->size; /* configuration */
		data = (void *)RTE_ALIGN_FLOOR((uintptr_t)
					       (out->args.vc.data - data_size),
					       sizeof(double));
		if ((uint8_t *)action + sizeof(*action) > data)
			return -1;
		*action = (struct rte_flow_action){
			.type = priv->type,
			.conf = data_size ? data : NULL,
		};
		++out->args.vc.actions_n;
		ctx->object = action;
		ctx->objmask = NULL;
	}
	memset(data, 0, data_size);
	out->args.vc.data = data;
	ctx->objdata = data_size;
	return len;
}

/** Parse pattern item parameter type. */
static int
parse_vc_spec(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_item *item;
	uint32_t data_size;
	int index;
	int objmask = 0;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Parse parameter types. */
	switch (ctx->curr) {
		static const enum index prefix[] = NEXT_ENTRY(COMMON_PREFIX);

	case ITEM_PARAM_IS:
		index = 0;
		objmask = 1;
		break;
	case ITEM_PARAM_SPEC:
		index = 0;
		break;
	case ITEM_PARAM_LAST:
		index = 1;
		break;
	case ITEM_PARAM_PREFIX:
		/* Modify next token to expect a prefix. */
		if (ctx->next_num < 2)
			return -1;
		ctx->next[ctx->next_num - 2] = prefix;
		/* Fall through. */
	case ITEM_PARAM_MASK:
		index = 2;
		break;
	default:
		return -1;
	}
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->args.vc.pattern_n)
		return -1;
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	data_size = ctx->objdata / 3; /* spec, last, mask */
	/* Point to selected object. */
	ctx->object = out->args.vc.data + (data_size * index);
	if (objmask) {
		ctx->objmask = out->args.vc.data + (data_size * 2); /* mask */
		item->mask = ctx->objmask;
	} else
		ctx->objmask = NULL;
	/* Update relevant item pointer. */
	*((const void **[]){ &item->spec, &item->last, &item->mask })[index] =
		ctx->object;
	return len;
}

/** Parse action configuration field. */
static int
parse_vc_conf(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct buffer *out = buf;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	return len;
}

/** Parse action configuration field. */
static int
parse_vc_conf_timeout(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len,
		      void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_update_age *update;

	(void)size;
	if (ctx->curr != ACTION_AGE_UPDATE_TIMEOUT)
		return -1;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Update the timeout is valid. */
	update = (struct rte_flow_update_age *)out->args.vc.data;
	update->timeout_valid = 1;
	return len;
}

/** Parse eCPRI common header type field. */
static int
parse_vc_item_ecpri_type(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_item_ecpri *ecpri;
	struct rte_flow_item_ecpri *ecpri_mask;
	struct rte_flow_item *item;
	uint32_t data_size;
	uint8_t msg_type;
	struct buffer *out = buf;
	const struct arg *arg;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case ITEM_ECPRI_COMMON_TYPE_IQ_DATA:
		msg_type = RTE_ECPRI_MSG_TYPE_IQ_DATA;
		break;
	case ITEM_ECPRI_COMMON_TYPE_RTC_CTRL:
		msg_type = RTE_ECPRI_MSG_TYPE_RTC_CTRL;
		break;
	case ITEM_ECPRI_COMMON_TYPE_DLY_MSR:
		msg_type = RTE_ECPRI_MSG_TYPE_DLY_MSR;
		break;
	default:
		return -1;
	}
	if (!ctx->object)
		return len;
	arg = pop_args(ctx);
	if (!arg)
		return -1;
	ecpri = (struct rte_flow_item_ecpri *)out->args.vc.data;
	ecpri->hdr.common.type = msg_type;
	data_size = ctx->objdata / 3; /* spec, last, mask */
	ecpri_mask = (struct rte_flow_item_ecpri *)(out->args.vc.data +
						    (data_size * 2));
	ecpri_mask->hdr.common.type = 0xFF;
	if (arg->hton) {
		ecpri->hdr.common.u32 = rte_cpu_to_be_32(ecpri->hdr.common.u32);
		ecpri_mask->hdr.common.u32 =
				rte_cpu_to_be_32(ecpri_mask->hdr.common.u32);
	}
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	item->spec = ecpri;
	item->mask = ecpri_mask;
	return len;
}

/** Parse L2TPv2 common header type field. */
static int
parse_vc_item_l2tpv2_type(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct rte_flow_item_l2tpv2 *l2tpv2;
	struct rte_flow_item_l2tpv2 *l2tpv2_mask;
	struct rte_flow_item *item;
	uint32_t data_size;
	uint16_t msg_type = 0;
	struct buffer *out = buf;
	const struct arg *arg;

	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case ITEM_L2TPV2_TYPE_DATA:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA;
		break;
	case ITEM_L2TPV2_TYPE_DATA_L:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_L;
		break;
	case ITEM_L2TPV2_TYPE_DATA_S:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_S;
		break;
	case ITEM_L2TPV2_TYPE_DATA_O:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_O;
		break;
	case ITEM_L2TPV2_TYPE_DATA_L_S:
		msg_type |= RTE_L2TPV2_MSG_TYPE_DATA_L_S;
		break;
	case ITEM_L2TPV2_TYPE_CTRL:
		msg_type |= RTE_L2TPV2_MSG_TYPE_CONTROL;
		break;
	default:
		return -1;
	}
	if (!ctx->object)
		return len;
	arg = pop_args(ctx);
	if (!arg)
		return -1;
	l2tpv2 = (struct rte_flow_item_l2tpv2 *)out->args.vc.data;
	l2tpv2->hdr.common.flags_version |= msg_type;
	data_size = ctx->objdata / 3; /* spec, last, mask */
	l2tpv2_mask = (struct rte_flow_item_l2tpv2 *)(out->args.vc.data +
						    (data_size * 2));
	l2tpv2_mask->hdr.common.flags_version = 0xFFFF;
	if (arg->hton) {
		l2tpv2->hdr.common.flags_version =
			rte_cpu_to_be_16(l2tpv2->hdr.common.flags_version);
		l2tpv2_mask->hdr.common.flags_version =
		    rte_cpu_to_be_16(l2tpv2_mask->hdr.common.flags_version);
	}
	item = &out->args.vc.pattern[out->args.vc.pattern_n - 1];
	item->spec = l2tpv2;
	item->mask = l2tpv2_mask;
	return len;
}

/** Parse meter color action type. */
static int
parse_vc_action_meter_color_type(struct context *ctx, const struct token *token,
				const char *str, unsigned int len,
				void *buf, unsigned int size)
{
	struct rte_flow_action *action_data;
	struct rte_flow_action_meter_color *conf;
	enum rte_color color;

	(void)buf;
	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case ACTION_METER_COLOR_GREEN:
		color = RTE_COLOR_GREEN;
	break;
	case ACTION_METER_COLOR_YELLOW:
		color = RTE_COLOR_YELLOW;
	break;
	case ACTION_METER_COLOR_RED:
		color = RTE_COLOR_RED;
	break;
	default:
		return -1;
	}

	if (!ctx->object)
		return len;
	action_data = ctx->object;
	conf = (struct rte_flow_action_meter_color *)
					(uintptr_t)(action_data->conf);
	conf->color = color;
	return len;
}

/** Parse RSS action. */
static int
parse_vc_action_rss(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_rss_data *action_rss_data;
	unsigned int i;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Set up default configuration. */
	action_rss_data = ctx->object;
	*action_rss_data = (struct action_rss_data){
		.conf = (struct rte_flow_action_rss){
			.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
			.level = 0,
			.types = rss_hf,
			.key_len = 0,
			.queue_num = RTE_MIN(nb_rxq, ACTION_RSS_QUEUE_NUM),
			.key = NULL,
			.queue = action_rss_data->queue,
		},
		.queue = { 0 },
	};
	for (i = 0; i < action_rss_data->conf.queue_num; ++i)
		action_rss_data->queue[i] = i;
	action->conf = &action_rss_data->conf;
	return ret;
}

/**
 * Parse func field for RSS action.
 *
 * The RTE_ETH_HASH_FUNCTION_* value to assign is derived from the
 * ACTION_RSS_FUNC_* index that called this function.
 */
static int
parse_vc_action_rss_func(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct action_rss_data *action_rss_data;
	enum rte_eth_hash_function func;

	(void)buf;
	(void)size;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	switch (ctx->curr) {
	case ACTION_RSS_FUNC_DEFAULT:
		func = RTE_ETH_HASH_FUNCTION_DEFAULT;
		break;
	case ACTION_RSS_FUNC_TOEPLITZ:
		func = RTE_ETH_HASH_FUNCTION_TOEPLITZ;
		break;
	case ACTION_RSS_FUNC_SIMPLE_XOR:
		func = RTE_ETH_HASH_FUNCTION_SIMPLE_XOR;
		break;
	case ACTION_RSS_FUNC_SYMMETRIC_TOEPLITZ:
		func = RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ;
		break;
	default:
		return -1;
	}
	if (!ctx->object)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.func = func;
	return len;
}

/**
 * Parse type field for RSS action.
 *
 * Valid tokens are type field names and the "end" token.
 */
static int
parse_vc_action_rss_type(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	static const enum index next[] = NEXT_ENTRY(ACTION_RSS_TYPE);
	struct action_rss_data *action_rss_data;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != ACTION_RSS_TYPE)
		return -1;
	if (!(ctx->objdata >> 16) && ctx->object) {
		action_rss_data = ctx->object;
		action_rss_data->conf.types = 0;
	}
	if (!strcmp_partial("end", str, len)) {
		ctx->objdata &= 0xffff;
		return len;
	}
	for (i = 0; rss_type_table[i].str; ++i)
		if (!strcmp_partial(rss_type_table[i].str, str, len))
			break;
	if (!rss_type_table[i].str)
		return -1;
	ctx->objdata = 1 << 16 | (ctx->objdata & 0xffff);
	/* Repeat token. */
	if (ctx->next_num == RTE_DIM(ctx->next))
		return -1;
	ctx->next[ctx->next_num++] = next;
	if (!ctx->object)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.types |= rss_type_table[i].rss_type;
	return len;
}

/**
 * Parse queue field for RSS action.
 *
 * Valid tokens are queue indices and the "end" token.
 */
static int
parse_vc_action_rss_queue(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	static const enum index next[] = NEXT_ENTRY(ACTION_RSS_QUEUE);
	struct action_rss_data *action_rss_data;
	const struct arg *arg;
	int ret;
	int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != ACTION_RSS_QUEUE)
		return -1;
	i = ctx->objdata >> 16;
	if (!strcmp_partial("end", str, len)) {
		ctx->objdata &= 0xffff;
		goto end;
	}
	if (i >= ACTION_RSS_QUEUE_NUM)
		return -1;
	arg = ARGS_ENTRY_ARB(offsetof(struct action_rss_data, queue) +
			     i * sizeof(action_rss_data->queue[i]),
			     sizeof(action_rss_data->queue[i]));
	if (push_args(ctx, arg))
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	++i;
	ctx->objdata = i << 16 | (ctx->objdata & 0xffff);
	/* Repeat token. */
	if (ctx->next_num == RTE_DIM(ctx->next))
		return -1;
	ctx->next[ctx->next_num++] = next;
end:
	if (!ctx->object)
		return len;
	action_rss_data = ctx->object;
	action_rss_data->conf.queue_num = i;
	action_rss_data->conf.queue = i ? action_rss_data->queue : NULL;
	return len;
}

/** Setup VXLAN encap configuration. */
static int
parse_setup_vxlan_encap_data(struct action_vxlan_encap_data *action_vxlan_encap_data)
{
	/* Set up default configuration. */
	*action_vxlan_encap_data = (struct action_vxlan_encap_data){
		.conf = (struct rte_flow_action_vxlan_encap){
			.definition = action_vxlan_encap_data->items,
		},
		.items = {
			{
				.type = RTE_FLOW_ITEM_TYPE_ETH,
				.spec = &action_vxlan_encap_data->item_eth,
				.mask = &rte_flow_item_eth_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &action_vxlan_encap_data->item_vlan,
				.mask = &rte_flow_item_vlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_IPV4,
				.spec = &action_vxlan_encap_data->item_ipv4,
				.mask = &rte_flow_item_ipv4_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_UDP,
				.spec = &action_vxlan_encap_data->item_udp,
				.mask = &rte_flow_item_udp_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VXLAN,
				.spec = &action_vxlan_encap_data->item_vxlan,
				.mask = &rte_flow_item_vxlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		},
		.item_eth.hdr.ether_type = 0,
		.item_vlan = {
			.hdr.vlan_tci = vxlan_encap_conf.vlan_tci,
			.hdr.eth_proto = 0,
		},
		.item_ipv4.hdr = {
			.src_addr = vxlan_encap_conf.ipv4_src,
			.dst_addr = vxlan_encap_conf.ipv4_dst,
		},
		.item_udp.hdr = {
			.src_port = vxlan_encap_conf.udp_src,
			.dst_port = vxlan_encap_conf.udp_dst,
		},
		.item_vxlan.hdr.flags = 0,
	};
	memcpy(action_vxlan_encap_data->item_eth.hdr.dst_addr.addr_bytes,
	       vxlan_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(action_vxlan_encap_data->item_eth.hdr.src_addr.addr_bytes,
	       vxlan_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	if (!vxlan_encap_conf.select_ipv4) {
		memcpy(&action_vxlan_encap_data->item_ipv6.hdr.src_addr,
		       &vxlan_encap_conf.ipv6_src,
		       sizeof(vxlan_encap_conf.ipv6_src));
		memcpy(&action_vxlan_encap_data->item_ipv6.hdr.dst_addr,
		       &vxlan_encap_conf.ipv6_dst,
		       sizeof(vxlan_encap_conf.ipv6_dst));
		action_vxlan_encap_data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &action_vxlan_encap_data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (!vxlan_encap_conf.select_vlan)
		action_vxlan_encap_data->items[1].type =
			RTE_FLOW_ITEM_TYPE_VOID;
	if (vxlan_encap_conf.select_tos_ttl) {
		if (vxlan_encap_conf.select_ipv4) {
			static struct rte_flow_item_ipv4 ipv4_mask_tos;

			memcpy(&ipv4_mask_tos, &rte_flow_item_ipv4_mask,
			       sizeof(ipv4_mask_tos));
			ipv4_mask_tos.hdr.type_of_service = 0xff;
			ipv4_mask_tos.hdr.time_to_live = 0xff;
			action_vxlan_encap_data->item_ipv4.hdr.type_of_service =
					vxlan_encap_conf.ip_tos;
			action_vxlan_encap_data->item_ipv4.hdr.time_to_live =
					vxlan_encap_conf.ip_ttl;
			action_vxlan_encap_data->items[2].mask =
							&ipv4_mask_tos;
		} else {
			static struct rte_flow_item_ipv6 ipv6_mask_tos;

			memcpy(&ipv6_mask_tos, &rte_flow_item_ipv6_mask,
			       sizeof(ipv6_mask_tos));
			ipv6_mask_tos.hdr.vtc_flow |=
				RTE_BE32(0xfful << RTE_IPV6_HDR_TC_SHIFT);
			ipv6_mask_tos.hdr.hop_limits = 0xff;
			action_vxlan_encap_data->item_ipv6.hdr.vtc_flow |=
				rte_cpu_to_be_32
					((uint32_t)vxlan_encap_conf.ip_tos <<
					 RTE_IPV6_HDR_TC_SHIFT);
			action_vxlan_encap_data->item_ipv6.hdr.hop_limits =
					vxlan_encap_conf.ip_ttl;
			action_vxlan_encap_data->items[2].mask =
							&ipv6_mask_tos;
		}
	}
	memcpy(action_vxlan_encap_data->item_vxlan.hdr.vni, vxlan_encap_conf.vni,
	       RTE_DIM(vxlan_encap_conf.vni));
	return 0;
}

/** Parse VXLAN encap action. */
static int
parse_vc_action_vxlan_encap(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_vxlan_encap_data *action_vxlan_encap_data;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	action_vxlan_encap_data = ctx->object;
	parse_setup_vxlan_encap_data(action_vxlan_encap_data);
	action->conf = &action_vxlan_encap_data->conf;
	return ret;
}

/** Setup NVGRE encap configuration. */
static int
parse_setup_nvgre_encap_data(struct action_nvgre_encap_data *action_nvgre_encap_data)
{
	/* Set up default configuration. */
	*action_nvgre_encap_data = (struct action_nvgre_encap_data){
		.conf = (struct rte_flow_action_nvgre_encap){
			.definition = action_nvgre_encap_data->items,
		},
		.items = {
			{
				.type = RTE_FLOW_ITEM_TYPE_ETH,
				.spec = &action_nvgre_encap_data->item_eth,
				.mask = &rte_flow_item_eth_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_VLAN,
				.spec = &action_nvgre_encap_data->item_vlan,
				.mask = &rte_flow_item_vlan_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_IPV4,
				.spec = &action_nvgre_encap_data->item_ipv4,
				.mask = &rte_flow_item_ipv4_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_NVGRE,
				.spec = &action_nvgre_encap_data->item_nvgre,
				.mask = &rte_flow_item_nvgre_mask,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		},
		.item_eth.hdr.ether_type = 0,
		.item_vlan = {
			.hdr.vlan_tci = nvgre_encap_conf.vlan_tci,
			.hdr.eth_proto = 0,
		},
		.item_ipv4.hdr = {
		       .src_addr = nvgre_encap_conf.ipv4_src,
		       .dst_addr = nvgre_encap_conf.ipv4_dst,
		},
		.item_nvgre.c_k_s_rsvd0_ver = RTE_BE16(0x2000),
		.item_nvgre.protocol = RTE_BE16(RTE_ETHER_TYPE_TEB),
		.item_nvgre.flow_id = 0,
	};
	memcpy(action_nvgre_encap_data->item_eth.hdr.dst_addr.addr_bytes,
	       nvgre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(action_nvgre_encap_data->item_eth.hdr.src_addr.addr_bytes,
	       nvgre_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	if (!nvgre_encap_conf.select_ipv4) {
		memcpy(&action_nvgre_encap_data->item_ipv6.hdr.src_addr,
		       &nvgre_encap_conf.ipv6_src,
		       sizeof(nvgre_encap_conf.ipv6_src));
		memcpy(&action_nvgre_encap_data->item_ipv6.hdr.dst_addr,
		       &nvgre_encap_conf.ipv6_dst,
		       sizeof(nvgre_encap_conf.ipv6_dst));
		action_nvgre_encap_data->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &action_nvgre_encap_data->item_ipv6,
			.mask = &rte_flow_item_ipv6_mask,
		};
	}
	if (!nvgre_encap_conf.select_vlan)
		action_nvgre_encap_data->items[1].type =
			RTE_FLOW_ITEM_TYPE_VOID;
	memcpy(action_nvgre_encap_data->item_nvgre.tni, nvgre_encap_conf.tni,
	       RTE_DIM(nvgre_encap_conf.tni));
	return 0;
}

/** Parse NVGRE encap action. */
static int
parse_vc_action_nvgre_encap(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_nvgre_encap_data *action_nvgre_encap_data;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	action_nvgre_encap_data = ctx->object;
	parse_setup_nvgre_encap_data(action_nvgre_encap_data);
	action->conf = &action_nvgre_encap_data->conf;
	return ret;
}

/** Parse l2 encap action. */
static int
parse_vc_action_l2_encap(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = mplsoudp_encap_conf.vlan_tci,
		.hdr.eth_proto = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
	};
	header = action_encap_data->data;
	if (l2_encap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (l2_encap_conf.select_ipv4)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       l2_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       l2_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (l2_encap_conf.select_vlan) {
		if (l2_encap_conf.select_ipv4)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse l2 decap action. */
static int
parse_vc_action_l2_decap(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len,
			 void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = mplsoudp_encap_conf.vlan_tci,
		.hdr.eth_proto = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (l2_decap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (l2_decap_conf.select_vlan) {
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

#define ETHER_TYPE_MPLS_UNICAST 0x8847

/** Parse MPLSOGRE encap action. */
static int
parse_vc_action_mplsogre_encap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = mplsogre_encap_conf.vlan_tci,
		.hdr.eth_proto = 0,
	};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.src_addr = mplsogre_encap_conf.ipv4_src,
			.dst_addr = mplsogre_encap_conf.ipv4_dst,
			.next_proto_id = IPPROTO_GRE,
			.version_ihl = RTE_IPV4_VHL_DEF,
			.time_to_live = IPDEFTTL,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_GRE,
			.hop_limits = IPDEFTTL,
		},
	};
	struct rte_flow_item_gre gre = {
		.protocol = rte_cpu_to_be_16(ETHER_TYPE_MPLS_UNICAST),
	};
	struct rte_flow_item_mpls mpls = {
		.ttl = 0,
	};
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
		.preserve = {},
	};
	header = action_encap_data->data;
	if (mplsogre_encap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       mplsogre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       mplsogre_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (mplsogre_encap_conf.select_ipv4) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(&ipv6.hdr.src_addr,
		       &mplsogre_encap_conf.ipv6_src,
		       sizeof(mplsogre_encap_conf.ipv6_src));
		memcpy(&ipv6.hdr.dst_addr,
		       &mplsogre_encap_conf.ipv6_dst,
		       sizeof(mplsogre_encap_conf.ipv6_dst));
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &gre, sizeof(gre));
	header += sizeof(gre);
	memcpy(mpls.label_tc_s, mplsogre_encap_conf.label,
	       RTE_DIM(mplsogre_encap_conf.label));
	mpls.label_tc_s[2] |= 0x1;
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse MPLSOGRE decap action. */
static int
parse_vc_action_mplsogre_decap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {.hdr.vlan_tci = 0};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.next_proto_id = IPPROTO_GRE,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_GRE,
		},
	};
	struct rte_flow_item_gre gre = {
		.protocol = rte_cpu_to_be_16(ETHER_TYPE_MPLS_UNICAST),
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (mplsogre_decap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       mplsogre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       mplsogre_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (mplsogre_encap_conf.select_ipv4) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &gre, sizeof(gre));
	header += sizeof(gre);
	memset(&mpls, 0, sizeof(mpls));
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

/** Parse MPLSOUDP encap action. */
static int
parse_vc_action_mplsoudp_encap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_encap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {
		.hdr.vlan_tci = mplsoudp_encap_conf.vlan_tci,
		.hdr.eth_proto = 0,
	};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.src_addr = mplsoudp_encap_conf.ipv4_src,
			.dst_addr = mplsoudp_encap_conf.ipv4_dst,
			.next_proto_id = IPPROTO_UDP,
			.version_ihl = RTE_IPV4_VHL_DEF,
			.time_to_live = IPDEFTTL,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_UDP,
			.hop_limits = IPDEFTTL,
		},
	};
	struct rte_flow_item_udp udp = {
		.hdr = {
			.src_port = mplsoudp_encap_conf.udp_src,
			.dst_port = mplsoudp_encap_conf.udp_dst,
		},
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_encap_data = ctx->object;
	*action_encap_data = (struct action_raw_encap_data) {
		.conf = (struct rte_flow_action_raw_encap){
			.data = action_encap_data->data,
		},
		.data = {},
		.preserve = {},
	};
	header = action_encap_data->data;
	if (mplsoudp_encap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       mplsoudp_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (mplsoudp_encap_conf.select_ipv4) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(&ipv6.hdr.src_addr,
		       &mplsoudp_encap_conf.ipv6_src,
		       sizeof(mplsoudp_encap_conf.ipv6_src));
		memcpy(&ipv6.hdr.dst_addr,
		       &mplsoudp_encap_conf.ipv6_dst,
		       sizeof(mplsoudp_encap_conf.ipv6_dst));
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &udp, sizeof(udp));
	header += sizeof(udp);
	memcpy(mpls.label_tc_s, mplsoudp_encap_conf.label,
	       RTE_DIM(mplsoudp_encap_conf.label));
	mpls.label_tc_s[2] |= 0x1;
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_encap_data->conf.size = header -
		action_encap_data->data;
	action->conf = &action_encap_data->conf;
	return ret;
}

/** Parse MPLSOUDP decap action. */
static int
parse_vc_action_mplsoudp_decap(struct context *ctx, const struct token *token,
			       const char *str, unsigned int len,
			       void *buf, unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_decap_data;
	struct rte_flow_item_eth eth = { .hdr.ether_type = 0, };
	struct rte_flow_item_vlan vlan = {.hdr.vlan_tci = 0};
	struct rte_flow_item_ipv4 ipv4 = {
		.hdr =  {
			.next_proto_id = IPPROTO_UDP,
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_UDP,
		},
	};
	struct rte_flow_item_udp udp = {
		.hdr = {
			.dst_port = rte_cpu_to_be_16(6635),
		},
	};
	struct rte_flow_item_mpls mpls;
	uint8_t *header;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_decap_data = ctx->object;
	*action_decap_data = (struct action_raw_decap_data) {
		.conf = (struct rte_flow_action_raw_decap){
			.data = action_decap_data->data,
		},
		.data = {},
	};
	header = action_decap_data->data;
	if (mplsoudp_decap_conf.select_vlan)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.hdr.dst_addr.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.hdr.src_addr.addr_bytes,
	       mplsoudp_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth.hdr, sizeof(struct rte_ether_hdr));
	header += sizeof(struct rte_ether_hdr);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.hdr.eth_proto = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan.hdr, sizeof(struct rte_vlan_hdr));
		header += sizeof(struct rte_vlan_hdr);
	}
	if (mplsoudp_encap_conf.select_ipv4) {
		memcpy(header, &ipv4, sizeof(ipv4));
		header += sizeof(ipv4);
	} else {
		memcpy(header, &ipv6, sizeof(ipv6));
		header += sizeof(ipv6);
	}
	memcpy(header, &udp, sizeof(udp));
	header += sizeof(udp);
	memset(&mpls, 0, sizeof(mpls));
	memcpy(header, &mpls, sizeof(mpls));
	header += sizeof(mpls);
	action_decap_data->conf.size = header -
		action_decap_data->data;
	action->conf = &action_decap_data->conf;
	return ret;
}

static int
parse_vc_action_raw_decap_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_raw_decap_data *action_raw_decap_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct buffer *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_raw_decap_data, idx),
		 sizeof(((struct action_raw_decap_data *)0)->idx),
		 0, RAW_ENCAP_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg))
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (!ctx->object)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_raw_decap_data = ctx->object;
	idx = action_raw_decap_data->idx;
	action_raw_decap_data->conf.data = raw_decap_confs[idx].data;
	action_raw_decap_data->conf.size = raw_decap_confs[idx].size;
	action->conf = &action_raw_decap_data->conf;
	return len;
}


static int
parse_vc_action_raw_encap_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_raw_encap_data *action_raw_encap_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct buffer *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (ctx->curr != ACTION_RAW_ENCAP_INDEX_VALUE)
		return -1;
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_raw_encap_data, idx),
		 sizeof(((struct action_raw_encap_data *)0)->idx),
		 0, RAW_ENCAP_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg))
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (!ctx->object)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_raw_encap_data = ctx->object;
	idx = action_raw_encap_data->idx;
	action_raw_encap_data->conf.data = raw_encap_confs[idx].data;
	action_raw_encap_data->conf.size = raw_encap_confs[idx].size;
	action_raw_encap_data->conf.preserve = NULL;
	action->conf = &action_raw_encap_data->conf;
	return len;
}

static int
parse_vc_action_raw_encap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_encap_data *action_raw_encap_data = NULL;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_raw_encap_data = ctx->object;
	action_raw_encap_data->conf.data = raw_encap_confs[0].data;
	action_raw_encap_data->conf.preserve = NULL;
	action_raw_encap_data->conf.size = raw_encap_confs[0].size;
	action->conf = &action_raw_encap_data->conf;
	return ret;
}

static int
parse_vc_action_raw_decap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len, void *buf,
			  unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_raw_decap_data *action_raw_decap_data = NULL;
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_raw_decap_data = ctx->object;
	action_raw_decap_data->conf.data = raw_decap_confs[0].data;
	action_raw_decap_data->conf.size = raw_decap_confs[0].size;
	action->conf = &action_raw_decap_data->conf;
	return ret;
}

static int
parse_vc_action_set_meta(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	ret = rte_flow_dynf_metadata_register();
	if (ret < 0)
		return -1;
	return len;
}

static int
parse_vc_action_sample(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_action *action;
	struct action_sample_data *action_sample_data = NULL;
	static struct rte_flow_action end_action = {
		RTE_FLOW_ACTION_TYPE_END, 0
	};
	int ret;

	ret = parse_vc(ctx, token, str, len, buf, size);
	if (ret < 0)
		return ret;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return ret;
	if (!out->args.vc.actions_n)
		return -1;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	/* Point to selected object. */
	ctx->object = out->args.vc.data;
	ctx->objmask = NULL;
	/* Copy the headers to the buffer. */
	action_sample_data = ctx->object;
	action_sample_data->conf.actions = &end_action;
	action->conf = &action_sample_data->conf;
	return ret;
}

static int
parse_vc_action_sample_index(struct context *ctx, const struct token *token,
				const char *str, unsigned int len, void *buf,
				unsigned int size)
{
	struct action_sample_data *action_sample_data;
	struct rte_flow_action *action;
	const struct arg *arg;
	struct buffer *out = buf;
	int ret;
	uint16_t idx;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (ctx->curr != ACTION_SAMPLE_INDEX_VALUE)
		return -1;
	arg = ARGS_ENTRY_ARB_BOUNDED
		(offsetof(struct action_sample_data, idx),
		 sizeof(((struct action_sample_data *)0)->idx),
		 0, RAW_SAMPLE_CONFS_MAX_NUM - 1);
	if (push_args(ctx, arg))
		return -1;
	ret = parse_int(ctx, token, str, len, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		return -1;
	}
	if (!ctx->object)
		return len;
	action = &out->args.vc.actions[out->args.vc.actions_n - 1];
	action_sample_data = ctx->object;
	idx = action_sample_data->idx;
	action_sample_data->conf.actions = raw_sample_confs[idx].data;
	action->conf = &action_sample_data->conf;
	return len;
}

/** Parse operation for modify_field command. */
static int
parse_vc_modify_field_op(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action_modify_field;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != ACTION_MODIFY_FIELD_OP_VALUE)
		return -1;
	for (i = 0; modify_field_ops[i]; ++i)
		if (!strcmp_partial(modify_field_ops[i], str, len))
			break;
	if (!modify_field_ops[i])
		return -1;
	if (!ctx->object)
		return len;
	action_modify_field = ctx->object;
	action_modify_field->operation = (enum rte_flow_modify_op)i;
	return len;
}

/** Parse id for modify_field command. */
static int
parse_vc_modify_field_id(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action_modify_field;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	if (ctx->curr != ACTION_MODIFY_FIELD_DST_TYPE_VALUE &&
		ctx->curr != ACTION_MODIFY_FIELD_SRC_TYPE_VALUE)
		return -1;
	for (i = 0; modify_field_ids[i]; ++i)
		if (!strcmp_partial(modify_field_ids[i], str, len))
			break;
	if (!modify_field_ids[i])
		return -1;
	if (!ctx->object)
		return len;
	action_modify_field = ctx->object;
	if (ctx->curr == ACTION_MODIFY_FIELD_DST_TYPE_VALUE)
		action_modify_field->dst.field = (enum rte_flow_field_id)i;
	else
		action_modify_field->src.field = (enum rte_flow_field_id)i;
	return len;
}

/** Parse level for modify_field command. */
static int
parse_vc_modify_field_level(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct rte_flow_action_modify_field *action;
	struct flex_item *fp = NULL;
	uint32_t val;
	struct buffer *out = buf;
	char *end;

	(void)token;
	(void)size;
	if (ctx->curr != ACTION_MODIFY_FIELD_DST_LEVEL_VALUE &&
		ctx->curr != ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE)
		return -1;
	if (!ctx->object)
		return len;
	action = ctx->object;
	errno = 0;
	val = strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		return -1;
	/* No need to validate action template mask value */
	if (out->args.vc.masks) {
		if (ctx->curr == ACTION_MODIFY_FIELD_DST_LEVEL_VALUE)
			action->dst.level = val;
		else
			action->src.level = val;
		return len;
	}
	if ((ctx->curr == ACTION_MODIFY_FIELD_DST_LEVEL_VALUE &&
		action->dst.field == RTE_FLOW_FIELD_FLEX_ITEM) ||
		(ctx->curr == ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE &&
		action->src.field == RTE_FLOW_FIELD_FLEX_ITEM)) {
		if (val >= FLEX_MAX_PARSERS_NUM) {
			printf("Bad flex item handle\n");
			return -1;
		}
		fp = flex_items[ctx->port][val];
		if (!fp) {
			printf("Bad flex item handle\n");
			return -1;
		}
	}
	if (ctx->curr == ACTION_MODIFY_FIELD_DST_LEVEL_VALUE) {
		if (action->dst.field != RTE_FLOW_FIELD_FLEX_ITEM)
			action->dst.level = val;
		else
			action->dst.flex_handle = fp->flex_handle;
	} else if (ctx->curr == ACTION_MODIFY_FIELD_SRC_LEVEL_VALUE) {
		if (action->src.field != RTE_FLOW_FIELD_FLEX_ITEM)
			action->src.level = val;
		else
			action->src.flex_handle = fp->flex_handle;
	}
	return len;
}

/** Parse the conntrack update, not a rte_flow_action. */
static int
parse_vc_action_conntrack_update(struct context *ctx, const struct token *token,
			 const char *str, unsigned int len, void *buf,
			 unsigned int size)
{
	struct buffer *out = buf;
	struct rte_flow_modify_conntrack *ct_modify = NULL;

	(void)size;
	if (ctx->curr != ACTION_CONNTRACK_UPDATE_CTX &&
	    ctx->curr != ACTION_CONNTRACK_UPDATE_DIR)
		return -1;
	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	ct_modify = (struct rte_flow_modify_conntrack *)out->args.vc.data;
	if (ctx->curr == ACTION_CONNTRACK_UPDATE_DIR) {
		ct_modify->new_ct.is_original_dir =
				conntrack_context.is_original_dir;
		ct_modify->direction = 1;
	} else {
		uint32_t old_dir;

		old_dir = ct_modify->new_ct.is_original_dir;
		memcpy(&ct_modify->new_ct, &conntrack_context,
		       sizeof(conntrack_context));
		ct_modify->new_ct.is_original_dir = old_dir;
		ct_modify->state = 1;
	}
	return len;
}

/** Parse tokens for destroy command. */
static int
parse_destroy(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.destroy.rule =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	if (((uint8_t *)(out->args.destroy.rule + out->args.destroy.rule_n) +
	     sizeof(*out->args.destroy.rule)) > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = out->args.destroy.rule + out->args.destroy.rule_n++;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for flush command. */
static int
parse_flush(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != FLUSH)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for dump command. */
static int
parse_dump(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != DUMP)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	}
	switch (ctx->curr) {
	case DUMP_ALL:
	case DUMP_ONE:
		out->args.dump.mode = (ctx->curr == DUMP_ALL) ? true : false;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for query command. */
static int
parse_query(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != QUERY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse action names. */
static int
parse_action(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct buffer *out = buf;
	const struct arg *arg = pop_args(ctx);
	unsigned int i;

	(void)size;
	/* Argument is expected. */
	if (!arg)
		return -1;
	/* Parse action name. */
	for (i = 0; next_action[i]; ++i) {
		const struct parse_action_priv *priv;

		token = &token_list[next_action[i]];
		if (strcmp_partial(token->name, str, len))
			continue;
		priv = token->priv;
		if (!priv)
			goto error;
		if (out)
			memcpy((uint8_t *)ctx->object + arg->offset,
			       &priv->type,
			       arg->size);
		return len;
	}
error:
	push_args(ctx, arg);
	return -1;
}

/** Parse tokens for list command. */
static int
parse_list(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != LIST)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.list.group =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	if (((uint8_t *)(out->args.list.group + out->args.list.group_n) +
	     sizeof(*out->args.list.group)) > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = out->args.list.group + out->args.list.group_n++;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for list all aged flows command. */
static int
parse_aged(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command || out->command == QUEUE) {
		if (ctx->curr != AGED && ctx->curr != QUEUE_AGED)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	if (ctx->curr == AGED_DESTROY)
		out->args.aged.destroy = 1;
	return len;
}

/** Parse tokens for isolate command. */
static int
parse_isolate(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != ISOLATE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for info/configure command. */
static int
parse_configure(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != INFO && ctx->curr != CONFIGURE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	}
	return len;
}

/** Parse tokens for template create command. */
static int
parse_template(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != PATTERN_TEMPLATE &&
		    ctx->curr != ACTIONS_TEMPLATE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case PATTERN_TEMPLATE_CREATE:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.pat_templ_id = UINT32_MAX;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case PATTERN_TEMPLATE_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case PATTERN_TEMPLATE_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case PATTERN_TEMPLATE_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case ACTIONS_TEMPLATE_CREATE:
		out->args.vc.act_templ_id = UINT32_MAX;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case ACTIONS_TEMPLATE_SPEC:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	case ACTIONS_TEMPLATE_MASK:
		out->args.vc.masks =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.actions +
						out->args.vc.actions_n),
					       sizeof(double));
		ctx->object = out->args.vc.masks;
		ctx->objmask = NULL;
		return len;
	case ACTIONS_TEMPLATE_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case ACTIONS_TEMPLATE_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case ACTIONS_TEMPLATE_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for template destroy command. */
static int
parse_template_destroy(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len,
		       void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *template_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command ||
		out->command == PATTERN_TEMPLATE ||
		out->command == ACTIONS_TEMPLATE) {
		if (ctx->curr != PATTERN_TEMPLATE_DESTROY &&
			ctx->curr != ACTIONS_TEMPLATE_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.templ_destroy.template_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	template_id = out->args.templ_destroy.template_id
		    + out->args.templ_destroy.template_id_n++;
	if ((uint8_t *)template_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = template_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for table create command. */
static int
parse_table(struct context *ctx, const struct token *token,
	    const char *str, unsigned int len,
	    void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *template_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != TABLE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	}
	switch (ctx->curr) {
	case TABLE_CREATE:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.table.id = UINT32_MAX;
		return len;
	case TABLE_PATTERN_TEMPLATE:
		out->args.table.pat_templ_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		template_id = out->args.table.pat_templ_id
				+ out->args.table.pat_templ_id_n++;
		if ((uint8_t *)template_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = template_id;
		ctx->objmask = NULL;
		return len;
	case TABLE_ACTIONS_TEMPLATE:
		out->args.table.act_templ_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.table.pat_templ_id +
						out->args.table.pat_templ_id_n),
					       sizeof(double));
		template_id = out->args.table.act_templ_id
				+ out->args.table.act_templ_id_n++;
		if ((uint8_t *)template_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = template_id;
		ctx->objmask = NULL;
		return len;
	case TABLE_INGRESS:
		out->args.table.attr.flow_attr.ingress = 1;
		return len;
	case TABLE_EGRESS:
		out->args.table.attr.flow_attr.egress = 1;
		return len;
	case TABLE_TRANSFER:
		out->args.table.attr.flow_attr.transfer = 1;
		return len;
	case TABLE_TRANSFER_WIRE_ORIG:
		if (!out->args.table.attr.flow_attr.transfer)
			return -1;
		out->args.table.attr.specialize = RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG;
		return len;
	case TABLE_TRANSFER_VPORT_ORIG:
		if (!out->args.table.attr.flow_attr.transfer)
			return -1;
		out->args.table.attr.specialize = RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for table destroy command. */
static int
parse_table_destroy(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len,
		    void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *table_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command || out->command == TABLE) {
		if (ctx->curr != TABLE_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.table_destroy.table_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	table_id = out->args.table_destroy.table_id
		    + out->args.table_destroy.table_id_n++;
	if ((uint8_t *)table_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = table_id;
	ctx->objmask = NULL;
	return len;
}

/** Parse tokens for queue create commands. */
static int
parse_qo(struct context *ctx, const struct token *token,
	 const char *str, unsigned int len,
	 void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != QUEUE)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
		return len;
	}
	switch (ctx->curr) {
	case QUEUE_CREATE:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.rule_id = UINT32_MAX;
		return len;
	case QUEUE_TEMPLATE_TABLE:
	case QUEUE_PATTERN_TEMPLATE:
	case QUEUE_ACTIONS_TEMPLATE:
	case QUEUE_CREATE_POSTPONE:
	case QUEUE_RULE_ID:
		return len;
	case ITEM_PATTERN:
		out->args.vc.pattern =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		ctx->object = out->args.vc.pattern;
		ctx->objmask = NULL;
		return len;
	case ACTIONS:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)
					       (out->args.vc.pattern +
						out->args.vc.pattern_n),
					       sizeof(double));
		ctx->object = out->args.vc.actions;
		ctx->objmask = NULL;
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for queue destroy command. */
static int
parse_qo_destroy(struct context *ctx, const struct token *token,
		 const char *str, unsigned int len,
		 void *buf, unsigned int size)
{
	struct buffer *out = buf;
	uint32_t *flow_id;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command || out->command == QUEUE) {
		if (ctx->curr != QUEUE_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.destroy.rule =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	switch (ctx->curr) {
	case QUEUE_DESTROY_ID:
		flow_id = out->args.destroy.rule
				+ out->args.destroy.rule_n++;
		if ((uint8_t *)flow_id > (uint8_t *)out + size)
			return -1;
		ctx->objdata = 0;
		ctx->object = flow_id;
		ctx->objmask = NULL;
		return len;
	case QUEUE_DESTROY_POSTPONE:
		return len;
	default:
		return -1;
	}
}

/** Parse tokens for push queue command. */
static int
parse_push(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != PUSH)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
	}
	return len;
}

/** Parse tokens for pull command. */
static int
parse_pull(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != PULL)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.vc.data = (uint8_t *)out + size;
	}
	return len;
}

static int
parse_flex(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (out->command == ZERO) {
		if (ctx->curr != FLEX)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	} else {
		switch (ctx->curr) {
		default:
			break;
		case FLEX_ITEM_INIT:
		case FLEX_ITEM_CREATE:
		case FLEX_ITEM_DESTROY:
			out->command = ctx->curr;
			break;
		}
	}

	return len;
}

static int
parse_tunnel(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	if (!out->command) {
		if (ctx->curr != TUNNEL)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
	} else {
		switch (ctx->curr) {
		default:
			break;
		case TUNNEL_CREATE:
		case TUNNEL_DESTROY:
		case TUNNEL_LIST:
			out->command = ctx->curr;
			break;
		case TUNNEL_CREATE_TYPE:
		case TUNNEL_DESTROY_ID:
			ctx->object = &out->args.vc.tunnel_ops;
			break;
		}
	}

	return len;
}

/**
 * Parse signed/unsigned integers 8 to 64-bit long.
 *
 * Last argument (ctx->args) is retrieved to determine integer type and
 * storage location.
 */
static int
parse_int(struct context *ctx, const struct token *token,
	  const char *str, unsigned int len,
	  void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	uintmax_t u;
	char *end;

	(void)token;
	/* Argument is expected. */
	if (!arg)
		return -1;
	errno = 0;
	u = arg->sign ?
		(uintmax_t)strtoimax(str, &end, 0) :
		strtoumax(str, &end, 0);
	if (errno || (size_t)(end - str) != len)
		goto error;
	if (arg->bounded &&
	    ((arg->sign && ((intmax_t)u < (intmax_t)arg->min ||
			    (intmax_t)u > (intmax_t)arg->max)) ||
	     (!arg->sign && (u < arg->min || u > arg->max))))
		goto error;
	if (!ctx->object)
		return len;
	if (arg->mask) {
		if (!arg_entry_bf_fill(ctx->object, u, arg) ||
		    !arg_entry_bf_fill(ctx->objmask, -1, arg))
			goto error;
		return len;
	}
	buf = (uint8_t *)ctx->object + arg->offset;
	size = arg->size;
	if (u > RTE_LEN2MASK(size * CHAR_BIT, uint64_t))
		return -1;
objmask:
	switch (size) {
	case sizeof(uint8_t):
		*(uint8_t *)buf = u;
		break;
	case sizeof(uint16_t):
		*(uint16_t *)buf = arg->hton ? rte_cpu_to_be_16(u) : u;
		break;
	case sizeof(uint8_t [3]):
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		if (!arg->hton) {
			((uint8_t *)buf)[0] = u;
			((uint8_t *)buf)[1] = u >> 8;
			((uint8_t *)buf)[2] = u >> 16;
			break;
		}
#endif
		((uint8_t *)buf)[0] = u >> 16;
		((uint8_t *)buf)[1] = u >> 8;
		((uint8_t *)buf)[2] = u;
		break;
	case sizeof(uint32_t):
		*(uint32_t *)buf = arg->hton ? rte_cpu_to_be_32(u) : u;
		break;
	case sizeof(uint64_t):
		*(uint64_t *)buf = arg->hton ? rte_cpu_to_be_64(u) : u;
		break;
	default:
		goto error;
	}
	if (ctx->objmask && buf != (uint8_t *)ctx->objmask + arg->offset) {
		u = -1;
		buf = (uint8_t *)ctx->objmask + arg->offset;
		goto objmask;
	}
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse a string.
 *
 * Three arguments (ctx->args) are retrieved from the stack to store data,
 * its actual length and address (in that order).
 */
static int
parse_string(struct context *ctx, const struct token *token,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);
	const struct arg *arg_len = pop_args(ctx);
	const struct arg *arg_addr = pop_args(ctx);
	char tmp[16]; /* Ought to be enough. */
	int ret;

	/* Arguments are expected. */
	if (!arg_data)
		return -1;
	if (!arg_len) {
		push_args(ctx, arg_data);
		return -1;
	}
	if (!arg_addr) {
		push_args(ctx, arg_len);
		push_args(ctx, arg_data);
		return -1;
	}
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask || size < len)
		goto error;
	if (!ctx->object)
		return len;
	/* Let parse_int() fill length information first. */
	ret = snprintf(tmp, sizeof(tmp), "%u", len);
	if (ret < 0)
		goto error;
	push_args(ctx, arg_len);
	ret = parse_int(ctx, token, tmp, ret, NULL, 0);
	if (ret < 0) {
		pop_args(ctx);
		goto error;
	}
	buf = (uint8_t *)ctx->object + arg_data->offset;
	/* Output buffer is not necessarily NUL-terminated. */
	memcpy(buf, str, len);
	memset((uint8_t *)buf + len, 0x00, size - len);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg_data->offset, 0xff, len);
	/* Save address if requested. */
	if (arg_addr->size) {
		memcpy((uint8_t *)ctx->object + arg_addr->offset,
		       (void *[]){
			(uint8_t *)ctx->object + arg_data->offset
		       },
		       arg_addr->size);
		if (ctx->objmask)
			memcpy((uint8_t *)ctx->objmask + arg_addr->offset,
			       (void *[]){
				(uint8_t *)ctx->objmask + arg_data->offset
			       },
			       arg_addr->size);
	}
	return len;
error:
	push_args(ctx, arg_addr);
	push_args(ctx, arg_len);
	push_args(ctx, arg_data);
	return -1;
}

static int
parse_hex_string(const char *src, uint8_t *dst, uint32_t *size)
{
	const uint8_t *head = dst;
	uint32_t left;

	if (*size == 0)
		return -1;

	left = *size;

	/* Convert chars to bytes */
	while (left) {
		char tmp[3], *end = tmp;
		uint32_t read_lim = left & 1 ? 1 : 2;

		snprintf(tmp, read_lim + 1, "%s", src);
		*dst = strtoul(tmp, &end, 16);
		if (*end) {
			*dst = 0;
			*size = (uint32_t)(dst - head);
			return -1;
		}
		left -= read_lim;
		src += read_lim;
		dst++;
	}
	*dst = 0;
	*size = (uint32_t)(dst - head);
	return 0;
}

static int
parse_hex(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);
	const struct arg *arg_len = pop_args(ctx);
	const struct arg *arg_addr = pop_args(ctx);
	char tmp[16]; /* Ought to be enough. */
	int ret;
	unsigned int hexlen = len;
	unsigned int length = 256;
	uint8_t hex_tmp[length];

	/* Arguments are expected. */
	if (!arg_data)
		return -1;
	if (!arg_len) {
		push_args(ctx, arg_data);
		return -1;
	}
	if (!arg_addr) {
		push_args(ctx, arg_len);
		push_args(ctx, arg_data);
		return -1;
	}
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask)
		goto error;
	if (!ctx->object)
		return len;

	/* translate bytes string to array. */
	if (str[0] == '0' && ((str[1] == 'x') ||
			(str[1] == 'X'))) {
		str += 2;
		hexlen -= 2;
	}
	if (hexlen > length)
		goto error;
	ret = parse_hex_string(str, hex_tmp, &hexlen);
	if (ret < 0)
		goto error;
	/* Check the converted binary fits into data buffer. */
	if (hexlen > size)
		goto error;
	/* Let parse_int() fill length information first. */
	ret = snprintf(tmp, sizeof(tmp), "%u", hexlen);
	if (ret < 0)
		goto error;
	/* Save length if requested. */
	if (arg_len->size) {
		push_args(ctx, arg_len);
		ret = parse_int(ctx, token, tmp, ret, NULL, 0);
		if (ret < 0) {
			pop_args(ctx);
			goto error;
		}
	}
	buf = (uint8_t *)ctx->object + arg_data->offset;
	/* Output buffer is not necessarily NUL-terminated. */
	memcpy(buf, hex_tmp, hexlen);
	memset((uint8_t *)buf + hexlen, 0x00, size - hexlen);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg_data->offset,
					0xff, hexlen);
	/* Save address if requested. */
	if (arg_addr->size) {
		memcpy((uint8_t *)ctx->object + arg_addr->offset,
		       (void *[]){
			(uint8_t *)ctx->object + arg_data->offset
		       },
		       arg_addr->size);
		if (ctx->objmask)
			memcpy((uint8_t *)ctx->objmask + arg_addr->offset,
			       (void *[]){
				(uint8_t *)ctx->objmask + arg_data->offset
			       },
			       arg_addr->size);
	}
	return len;
error:
	push_args(ctx, arg_addr);
	push_args(ctx, arg_len);
	push_args(ctx, arg_data);
	return -1;

}

/**
 * Parse a zero-ended string.
 */
static int
parse_string0(struct context *ctx, const struct token *token __rte_unused,
	     const char *str, unsigned int len,
	     void *buf, unsigned int size)
{
	const struct arg *arg_data = pop_args(ctx);

	/* Arguments are expected. */
	if (!arg_data)
		return -1;
	size = arg_data->size;
	/* Bit-mask fill is not supported. */
	if (arg_data->mask || size < len + 1)
		goto error;
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg_data->offset;
	strncpy(buf, str, len);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg_data->offset, 0xff, len);
	return len;
error:
	push_args(ctx, arg_data);
	return -1;
}

/**
 * Parse a MAC address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_mac_addr(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	struct rte_ether_addr tmp;
	int ret;

	(void)token;
	/* Argument is expected. */
	if (!arg)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (!arg->hton)
		goto error;
	ret = cmdline_parse_etheraddr(NULL, str, &tmp, size);
	if (ret < 0 || (unsigned int)ret != len)
		goto error;
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse an IPv4 address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_ipv4_addr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	char str2[len + 1];
	struct in_addr tmp;
	int ret;

	/* Argument is expected. */
	if (!arg)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (!arg->hton)
		goto error;
	memcpy(str2, str, len);
	str2[len] = '\0';
	ret = inet_pton(AF_INET, str2, &tmp);
	if (ret != 1) {
		/* Attempt integer parsing. */
		push_args(ctx, arg);
		return parse_int(ctx, token, str, len, buf, size);
	}
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/**
 * Parse an IPv6 address.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_ipv6_addr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	char str2[len + 1];
	struct in6_addr tmp;
	int ret;

	(void)token;
	/* Argument is expected. */
	if (!arg)
		return -1;
	size = arg->size;
	/* Bit-mask fill is not supported. */
	if (arg->mask || size != sizeof(tmp))
		goto error;
	/* Only network endian is supported. */
	if (!arg->hton)
		goto error;
	memcpy(str2, str, len);
	str2[len] = '\0';
	ret = inet_pton(AF_INET6, str2, &tmp);
	if (ret != 1)
		goto error;
	if (!ctx->object)
		return len;
	buf = (uint8_t *)ctx->object + arg->offset;
	memcpy(buf, &tmp, size);
	if (ctx->objmask)
		memset((uint8_t *)ctx->objmask + arg->offset, 0xff, size);
	return len;
error:
	push_args(ctx, arg);
	return -1;
}

/** Boolean values (even indices stand for false). */
static const char *const boolean_name[] = {
	"0", "1",
	"false", "true",
	"no", "yes",
	"N", "Y",
	"off", "on",
	NULL,
};

/**
 * Parse a boolean value.
 *
 * Last argument (ctx->args) is retrieved to determine storage size and
 * location.
 */
static int
parse_boolean(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	int ret;

	/* Argument is expected. */
	if (!arg)
		return -1;
	for (i = 0; boolean_name[i]; ++i)
		if (!strcmp_partial(boolean_name[i], str, len))
			break;
	/* Process token as integer. */
	if (boolean_name[i])
		str = i & 1 ? "1" : "0";
	push_args(ctx, arg);
	ret = parse_int(ctx, token, str, strlen(str), buf, size);
	return ret > 0 ? (int)len : ret;
}

/** Parse port and update context. */
static int
parse_port(struct context *ctx, const struct token *token,
	   const char *str, unsigned int len,
	   void *buf, unsigned int size)
{
	struct buffer *out = &(struct buffer){ .port = 0 };
	int ret;

	if (buf)
		out = buf;
	else {
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		size = sizeof(*out);
	}
	ret = parse_int(ctx, token, str, len, out, size);
	if (ret >= 0)
		ctx->port = out->port;
	if (!buf)
		ctx->object = NULL;
	return ret;
}

/** Parse tokens for shared indirect actions. */
static int
parse_ia_port(struct context *ctx, const struct token *token,
	      const char *str, unsigned int len,
	      void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	uint32_t id;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set indirect action */
	if (action)
		action->conf = (void *)(uintptr_t)id;
	return ret;
}

static int
parse_ia_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	uint32_t id;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set indirect action */
	if (action) {
		portid_t port_id = ctx->port;
		if (ctx->prev == INDIRECT_ACTION_PORT)
			port_id = (portid_t)(uintptr_t)action->conf;
		action->conf = port_action_handle_get_by_id(port_id, id);
		ret = (action->conf) ? ret : -1;
	}
	return ret;
}

static int
parse_meter_profile_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	struct rte_flow_action_meter_mark *meter;
	struct rte_flow_meter_profile *profile = NULL;
	uint32_t id = 0;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set meter profile */
	if (action) {
		meter = (struct rte_flow_action_meter_mark *)
			(uintptr_t)(action->conf);
		profile = port_meter_profile_get_by_id(ctx->port, id);
		meter->profile = profile;
		ret = (profile) ? ret : -1;
	}
	return ret;
}

static int
parse_meter_policy_id2ptr(struct context *ctx, const struct token *token,
		const char *str, unsigned int len,
		void *buf, unsigned int size)
{
	struct rte_flow_action *action = ctx->object;
	struct rte_flow_action_meter_mark *meter;
	struct rte_flow_meter_policy *policy = NULL;
	uint32_t id = 0;
	int ret;

	(void)buf;
	(void)size;
	ctx->objdata = 0;
	ctx->object = &id;
	ctx->objmask = NULL;
	ret = parse_int(ctx, token, str, len, ctx->object, sizeof(id));
	ctx->object = action;
	if (ret != (int)len)
		return ret;
	/* set meter policy */
	if (action) {
		meter = (struct rte_flow_action_meter_mark *)
			(uintptr_t)(action->conf);
		policy = port_meter_policy_get_by_id(ctx->port, id);
		meter->policy = policy;
		ret = (policy) ? ret : -1;
	}
	return ret;
}

/** Parse set command, initialize output buffer for subsequent tokens. */
static int
parse_set_raw_encap_decap(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	ctx->objdata = 0;
	ctx->objmask = NULL;
	ctx->object = out;
	if (!out->command)
		return -1;
	out->command = ctx->curr;
	/* For encap/decap we need is pattern */
	out->args.vc.pattern = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	return len;
}

/** Parse set command, initialize output buffer for subsequent tokens. */
static int
parse_set_sample_action(struct context *ctx, const struct token *token,
			  const char *str, unsigned int len,
			  void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	ctx->objdata = 0;
	ctx->objmask = NULL;
	ctx->object = out;
	if (!out->command)
		return -1;
	out->command = ctx->curr;
	/* For sampler we need is actions */
	out->args.vc.actions = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	return len;
}

/**
 * Parse set raw_encap/raw_decap command,
 * initialize output buffer for subsequent tokens.
 */
static int
parse_set_init(struct context *ctx, const struct token *token,
	       const char *str, unsigned int len,
	       void *buf, unsigned int size)
{
	struct buffer *out = buf;

	/* Token name must match. */
	if (parse_default(ctx, token, str, len, NULL, 0) < 0)
		return -1;
	/* Nothing else to do if there is no buffer. */
	if (!out)
		return len;
	/* Make sure buffer is large enough. */
	if (size < sizeof(*out))
		return -1;
	/* Initialize buffer. */
	memset(out, 0x00, sizeof(*out));
	memset((uint8_t *)out + sizeof(*out), 0x22, size - sizeof(*out));
	ctx->objdata = 0;
	ctx->object = out;
	ctx->objmask = NULL;
	if (!out->command) {
		if (ctx->curr != SET)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		out->args.vc.data = (uint8_t *)out + size;
		ctx->object  = (void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
						       sizeof(double));
	}
	return len;
}

/*
 * Replace testpmd handles in a flex flow item with real values.
 */
static int
parse_flex_handle(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len,
		  void *buf, unsigned int size)
{
	struct rte_flow_item_flex *spec, *mask;
	const struct rte_flow_item_flex *src_spec, *src_mask;
	const struct arg *arg = pop_args(ctx);
	uint32_t offset;
	uint16_t handle;
	int ret;

	if (!arg) {
		printf("Bad environment\n");
		return -1;
	}
	offset = arg->offset;
	push_args(ctx, arg);
	ret = parse_int(ctx, token, str, len, buf, size);
	if (ret <= 0 || !ctx->object)
		return ret;
	if (ctx->port >= RTE_MAX_ETHPORTS) {
		printf("Bad port\n");
		return -1;
	}
	if (offset == offsetof(struct rte_flow_item_flex, handle)) {
		const struct flex_item *fp;
		spec = ctx->object;
		handle = (uint16_t)(uintptr_t)spec->handle;
		if (handle >= FLEX_MAX_PARSERS_NUM) {
			printf("Bad flex item handle\n");
			return -1;
		}
		fp = flex_items[ctx->port][handle];
		if (!fp) {
			printf("Bad flex item handle\n");
			return -1;
		}
		spec->handle = fp->flex_handle;
		mask = spec + 2; /* spec, last, mask */
		mask->handle = fp->flex_handle;
	} else if (offset == offsetof(struct rte_flow_item_flex, pattern)) {
		handle = (uint16_t)(uintptr_t)
			((struct rte_flow_item_flex *)ctx->object)->pattern;
		if (handle >= FLEX_MAX_PATTERNS_NUM) {
			printf("Bad pattern handle\n");
			return -1;
		}
		src_spec = &flex_patterns[handle].spec;
		src_mask = &flex_patterns[handle].mask;
		spec = ctx->object;
		mask = spec + 2; /* spec, last, mask */
		/* fill flow rule spec and mask parameters */
		spec->length = src_spec->length;
		spec->pattern = src_spec->pattern;
		mask->length = src_mask->length;
		mask->pattern = src_mask->pattern;
	} else {
		printf("Bad arguments - unknown flex item offset\n");
		return -1;
	}
	return ret;
}

/** Parse Meter color name */
static int
parse_meter_color(struct context *ctx, const struct token *token,
		  const char *str, unsigned int len, void *buf,
		  unsigned int size)
{
	struct rte_flow_item_meter_color *meter_color;
	unsigned int i;

	(void)token;
	(void)buf;
	(void)size;
	for (i = 0; meter_colors[i]; ++i)
		if (!strcmp_partial(meter_colors[i], str, len))
			break;
	if (!meter_colors[i])
		return -1;
	if (!ctx->object)
		return len;
	meter_color = ctx->object;
	meter_color->color = (enum rte_color)i;
	return len;
}

/** Parse Insertion Table Type name */
static int
parse_insertion_table_type(struct context *ctx, const struct token *token,
			   const char *str, unsigned int len, void *buf,
			   unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	char tmp[2];
	int ret;

	(void)size;
	/* Argument is expected. */
	if (!arg)
		return -1;
	for (i = 0; table_insertion_types[i]; ++i)
		if (!strcmp_partial(table_insertion_types[i], str, len))
			break;
	if (!table_insertion_types[i])
		return -1;
	push_args(ctx, arg);
	snprintf(tmp, sizeof(tmp), "%u", i);
	ret = parse_int(ctx, token, tmp, strlen(tmp), buf, sizeof(i));
	return ret > 0 ? (int)len : ret;
}

/** Parse Hash Calculation Table Type name */
static int
parse_hash_table_type(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size)
{
	const struct arg *arg = pop_args(ctx);
	unsigned int i;
	char tmp[2];
	int ret;

	(void)size;
	/* Argument is expected. */
	if (!arg)
		return -1;
	for (i = 0; table_hash_funcs[i]; ++i)
		if (!strcmp_partial(table_hash_funcs[i], str, len))
			break;
	if (!table_hash_funcs[i])
		return -1;
	push_args(ctx, arg);
	snprintf(tmp, sizeof(tmp), "%u", i);
	ret = parse_int(ctx, token, tmp, strlen(tmp), buf, sizeof(i));
	return ret > 0 ? (int)len : ret;
}

static int
parse_name_to_index(struct context *ctx, const struct token *token,
		    const char *str, unsigned int len, void *buf,
		    unsigned int size,
		    const char *const names[], size_t names_size, uint32_t *dst)
{
	int ret;
	uint32_t i;

	RTE_SET_USED(token);
	RTE_SET_USED(buf);
	RTE_SET_USED(size);
	if (!ctx->object)
		return len;
	for (i = 0; i < names_size; i++) {
		if (!names[i])
			continue;
		ret = strcmp_partial(names[i], str,
				     RTE_MIN(len, strlen(names[i])));
		if (!ret) {
			*dst = i;
			return len;
		}
	}
	return -1;
}

static const char *const quota_mode_names[] = {
	NULL,
	[RTE_FLOW_QUOTA_MODE_PACKET] = "packet",
	[RTE_FLOW_QUOTA_MODE_L2] = "l2",
	[RTE_FLOW_QUOTA_MODE_L3] = "l3"
};

static const char *const quota_state_names[] = {
	[RTE_FLOW_QUOTA_STATE_PASS] = "pass",
	[RTE_FLOW_QUOTA_STATE_BLOCK] = "block"
};

static const char *const quota_update_names[] = {
	[RTE_FLOW_UPDATE_QUOTA_SET] = "set",
	[RTE_FLOW_UPDATE_QUOTA_ADD] = "add"
};

static const char *const query_update_mode_names[] = {
	[RTE_FLOW_QU_QUERY_FIRST] = "query_first",
	[RTE_FLOW_QU_UPDATE_FIRST] = "update_first"
};

static int
parse_quota_state_name(struct context *ctx, const struct token *token,
		       const char *str, unsigned int len, void *buf,
		       unsigned int size)
{
	struct rte_flow_item_quota *quota = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_state_names,
				   RTE_DIM(quota_state_names),
				   (uint32_t *)&quota->state);
}

static int
parse_quota_mode_name(struct context *ctx, const struct token *token,
		      const char *str, unsigned int len, void *buf,
		      unsigned int size)
{
	struct rte_flow_action_quota *quota = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_mode_names,
				   RTE_DIM(quota_mode_names),
				   (uint32_t *)&quota->mode);
}

static int
parse_quota_update_name(struct context *ctx, const struct token *token,
			const char *str, unsigned int len, void *buf,
			unsigned int size)
{
	struct rte_flow_update_quota *update = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   quota_update_names,
				   RTE_DIM(quota_update_names),
				   (uint32_t *)&update->op);
}

static int
parse_qu_mode_name(struct context *ctx, const struct token *token,
		   const char *str, unsigned int len, void *buf,
		   unsigned int size)
{
	struct buffer *out = ctx->object;

	return parse_name_to_index(ctx, token, str, len, buf, size,
				   query_update_mode_names,
				   RTE_DIM(query_update_mode_names),
				   (uint32_t *)&out->args.ia.qu_mode);
}

/** No completion. */
static int
comp_none(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	(void)ctx;
	(void)token;
	(void)ent;
	(void)buf;
	(void)size;
	return 0;
}

/** Complete boolean values. */
static int
comp_boolean(struct context *ctx, const struct token *token,
	     unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i;

	(void)ctx;
	(void)token;
	for (i = 0; boolean_name[i]; ++i)
		if (buf && i == ent)
			return strlcpy(buf, boolean_name[i], size);
	if (buf)
		return -1;
	return i;
}

/** Complete action names. */
static int
comp_action(struct context *ctx, const struct token *token,
	    unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i;

	(void)ctx;
	(void)token;
	for (i = 0; next_action[i]; ++i)
		if (buf && i == ent)
			return strlcpy(buf, token_list[next_action[i]].name,
				       size);
	if (buf)
		return -1;
	return i;
}

/** Complete available ports. */
static int
comp_port(struct context *ctx, const struct token *token,
	  unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	portid_t p;

	(void)ctx;
	(void)token;
	RTE_ETH_FOREACH_DEV(p) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", p);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Complete available rule IDs. */
static int
comp_rule_id(struct context *ctx, const struct token *token,
	     unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	struct rte_port *port;
	struct port_flow *pf;

	(void)token;
	if (port_id_is_invalid(ctx->port, DISABLED_WARN) ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	port = &ports[ctx->port];
	for (pf = port->flow_list; pf != NULL; pf = pf->next) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", pf->id);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Complete type field for RSS action. */
static int
comp_vc_action_rss_type(struct context *ctx, const struct token *token,
			unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i;

	(void)ctx;
	(void)token;
	for (i = 0; rss_type_table[i].str; ++i)
		;
	if (!buf)
		return i + 1;
	if (ent < i)
		return strlcpy(buf, rss_type_table[ent].str, size);
	if (ent == i)
		return snprintf(buf, size, "end");
	return -1;
}

/** Complete queue field for RSS action. */
static int
comp_vc_action_rss_queue(struct context *ctx, const struct token *token,
			 unsigned int ent, char *buf, unsigned int size)
{
	(void)ctx;
	(void)token;
	if (!buf)
		return nb_rxq + 1;
	if (ent < nb_rxq)
		return snprintf(buf, size, "%u", ent);
	if (ent == nb_rxq)
		return snprintf(buf, size, "end");
	return -1;
}

/** Complete index number for set raw_encap/raw_decap commands. */
static int
comp_set_raw_index(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	uint16_t idx = 0;
	uint16_t nb = 0;

	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	for (idx = 0; idx < RAW_ENCAP_CONFS_MAX_NUM; ++idx) {
		if (buf && idx == ent)
			return snprintf(buf, size, "%u", idx);
		++nb;
	}
	return nb;
}

/** Complete index number for set raw_encap/raw_decap commands. */
static int
comp_set_sample_index(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	uint16_t idx = 0;
	uint16_t nb = 0;

	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	for (idx = 0; idx < RAW_SAMPLE_CONFS_MAX_NUM; ++idx) {
		if (buf && idx == ent)
			return snprintf(buf, size, "%u", idx);
		++nb;
	}
	return nb;
}

/** Complete operation for modify_field command. */
static int
comp_set_modify_field_op(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (!buf)
		return RTE_DIM(modify_field_ops);
	if (ent < RTE_DIM(modify_field_ops) - 1)
		return strlcpy(buf, modify_field_ops[ent], size);
	return -1;
}

/** Complete field id for modify_field command. */
static int
comp_set_modify_field_id(struct context *ctx, const struct token *token,
		   unsigned int ent, char *buf, unsigned int size)
{
	const char *name;

	RTE_SET_USED(token);
	if (!buf)
		return RTE_DIM(modify_field_ids);
	if (ent >= RTE_DIM(modify_field_ids) - 1)
		return -1;
	name = modify_field_ids[ent];
	if (ctx->curr == ACTION_MODIFY_FIELD_SRC_TYPE ||
	    (strcmp(name, "pointer") && strcmp(name, "value")))
		return strlcpy(buf, name, size);
	return -1;
}

/** Complete available pattern template IDs. */
static int
comp_pattern_template_id(struct context *ctx, const struct token *token,
			 unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	struct rte_port *port;
	struct port_template *pt;

	(void)token;
	if (port_id_is_invalid(ctx->port, DISABLED_WARN) ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	port = &ports[ctx->port];
	for (pt = port->pattern_templ_list; pt != NULL; pt = pt->next) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", pt->id);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Complete available actions template IDs. */
static int
comp_actions_template_id(struct context *ctx, const struct token *token,
			 unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	struct rte_port *port;
	struct port_template *pt;

	(void)token;
	if (port_id_is_invalid(ctx->port, DISABLED_WARN) ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	port = &ports[ctx->port];
	for (pt = port->actions_templ_list; pt != NULL; pt = pt->next) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", pt->id);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Complete available table IDs. */
static int
comp_table_id(struct context *ctx, const struct token *token,
	      unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	struct rte_port *port;
	struct port_table *pt;

	(void)token;
	if (port_id_is_invalid(ctx->port, DISABLED_WARN) ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	port = &ports[ctx->port];
	for (pt = port->table_list; pt != NULL; pt = pt->next) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", pt->id);
		++i;
	}
	if (buf)
		return -1;
	return i;
}

/** Complete available queue IDs. */
static int
comp_queue_id(struct context *ctx, const struct token *token,
	      unsigned int ent, char *buf, unsigned int size)
{
	unsigned int i = 0;
	struct rte_port *port;

	(void)token;
	if (port_id_is_invalid(ctx->port, DISABLED_WARN) ||
	    ctx->port == (portid_t)RTE_PORT_ALL)
		return -1;
	port = &ports[ctx->port];
	for (i = 0; i < port->queue_nb; i++) {
		if (buf && i == ent)
			return snprintf(buf, size, "%u", i);
	}
	if (buf)
		return -1;
	return i;
}

static int
comp_names_to_index(struct context *ctx, const struct token *token,
		    unsigned int ent, char *buf, unsigned int size,
		    const char *const names[], size_t names_size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (!buf)
		return names_size;
	if (names[ent] && ent < names_size)
		return rte_strscpy(buf, names[ent], size);
	return -1;

}

/** Complete available Meter colors. */
static int
comp_meter_color(struct context *ctx, const struct token *token,
		 unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (!buf)
		return RTE_DIM(meter_colors);
	if (ent < RTE_DIM(meter_colors) - 1)
		return strlcpy(buf, meter_colors[ent], size);
	return -1;
}

/** Complete available Insertion Table types. */
static int
comp_insertion_table_type(struct context *ctx, const struct token *token,
			  unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (!buf)
		return RTE_DIM(table_insertion_types);
	if (ent < RTE_DIM(table_insertion_types) - 1)
		return rte_strscpy(buf, table_insertion_types[ent], size);
	return -1;
}

/** Complete available Hash Calculation Table types. */
static int
comp_hash_table_type(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size)
{
	RTE_SET_USED(ctx);
	RTE_SET_USED(token);
	if (!buf)
		return RTE_DIM(table_hash_funcs);
	if (ent < RTE_DIM(table_hash_funcs) - 1)
		return rte_strscpy(buf, table_hash_funcs[ent], size);
	return -1;
}

static int
comp_quota_state_name(struct context *ctx, const struct token *token,
		      unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_state_names,
				   RTE_DIM(quota_state_names));
}

static int
comp_quota_mode_name(struct context *ctx, const struct token *token,
		     unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_mode_names,
				   RTE_DIM(quota_mode_names));
}

static int
comp_quota_update_name(struct context *ctx, const struct token *token,
		       unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   quota_update_names,
				   RTE_DIM(quota_update_names));
}

static int
comp_qu_mode_name(struct context *ctx, const struct token *token,
		  unsigned int ent, char *buf, unsigned int size)
{
	return comp_names_to_index(ctx, token, ent, buf, size,
				   query_update_mode_names,
				   RTE_DIM(query_update_mode_names));
}

/** Internal context. */
static struct context cmd_flow_context;

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_flow;
cmdline_parse_inst_t cmd_set_raw;

/** Initialize context. */
static void
cmd_flow_context_init(struct context *ctx)
{
	/* A full memset() is not necessary. */
	ctx->curr = ZERO;
	ctx->prev = ZERO;
	ctx->next_num = 0;
	ctx->args_num = 0;
	ctx->eol = 0;
	ctx->last = 0;
	ctx->port = 0;
	ctx->objdata = 0;
	ctx->object = NULL;
	ctx->objmask = NULL;
}

/** Parse a token (cmdline API). */
static int
cmd_flow_parse(cmdline_parse_token_hdr_t *hdr, const char *src, void *result,
	       unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token;
	const enum index *list;
	int len;
	int i;

	(void)hdr;
	token = &token_list[ctx->curr];
	/* Check argument length. */
	ctx->eol = 0;
	ctx->last = 1;
	for (len = 0; src[len]; ++len)
		if (src[len] == '#' || isspace(src[len]))
			break;
	if (!len)
		return -1;
	/* Last argument and EOL detection. */
	for (i = len; src[i]; ++i)
		if (src[i] == '#' || src[i] == '\r' || src[i] == '\n')
			break;
		else if (!isspace(src[i])) {
			ctx->last = 0;
			break;
		}
	for (; src[i]; ++i)
		if (src[i] == '\r' || src[i] == '\n') {
			ctx->eol = 1;
			break;
		}
	/* Initialize context if necessary. */
	if (!ctx->next_num) {
		if (!token->next)
			return 0;
		ctx->next[ctx->next_num++] = token->next[0];
	}
	/* Process argument through candidates. */
	ctx->prev = ctx->curr;
	list = ctx->next[ctx->next_num - 1];
	for (i = 0; list[i]; ++i) {
		const struct token *next = &token_list[list[i]];
		int tmp;

		ctx->curr = list[i];
		if (next->call)
			tmp = next->call(ctx, next, src, len, result, size);
		else
			tmp = parse_default(ctx, next, src, len, result, size);
		if (tmp == -1 || tmp != len)
			continue;
		token = next;
		break;
	}
	if (!list[i])
		return -1;
	--ctx->next_num;
	/* Push subsequent tokens if any. */
	if (token->next)
		for (i = 0; token->next[i]; ++i) {
			if (ctx->next_num == RTE_DIM(ctx->next))
				return -1;
			ctx->next[ctx->next_num++] = token->next[i];
		}
	/* Push arguments if any. */
	if (token->args)
		for (i = 0; token->args[i]; ++i) {
			if (ctx->args_num == RTE_DIM(ctx->args))
				return -1;
			ctx->args[ctx->args_num++] = token->args[i];
		}
	return len;
}

int
flow_parse(const char *src, void *result, unsigned int size,
	   struct rte_flow_attr **attr,
	   struct rte_flow_item **pattern, struct rte_flow_action **actions)
{
	int ret;
	struct context saved_flow_ctx = cmd_flow_context;

	cmd_flow_context_init(&cmd_flow_context);
	do {
		ret = cmd_flow_parse(NULL, src, result, size);
		if (ret > 0) {
			src += ret;
			while (isspace(*src))
				src++;
		}
	} while (ret > 0 && strlen(src));
	cmd_flow_context = saved_flow_ctx;
	*attr = &((struct buffer *)result)->args.vc.attr;
	*pattern = ((struct buffer *)result)->args.vc.pattern;
	*actions = ((struct buffer *)result)->args.vc.actions;
	return (ret >= 0 && !strlen(src)) ? 0 : -1;
}

/** Return number of completion entries (cmdline API). */
static int
cmd_flow_complete_get_nb(cmdline_parse_token_hdr_t *hdr)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->curr];
	const enum index *list;
	int i;

	(void)hdr;
	/* Count number of tokens in current list. */
	if (ctx->next_num)
		list = ctx->next[ctx->next_num - 1];
	else
		list = token->next[0];
	for (i = 0; list[i]; ++i)
		;
	if (!i)
		return 0;
	/*
	 * If there is a single token, use its completion callback, otherwise
	 * return the number of entries.
	 */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, 0, NULL, 0);
	}
	return i;
}

/** Return a completion entry (cmdline API). */
static int
cmd_flow_complete_get_elt(cmdline_parse_token_hdr_t *hdr, int index,
			  char *dst, unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->curr];
	const enum index *list;
	int i;

	(void)hdr;
	/* Count number of tokens in current list. */
	if (ctx->next_num)
		list = ctx->next[ctx->next_num - 1];
	else
		list = token->next[0];
	for (i = 0; list[i]; ++i)
		;
	if (!i)
		return -1;
	/* If there is a single token, use its completion callback. */
	token = &token_list[list[0]];
	if (i == 1 && token->comp) {
		/* Save index for cmd_flow_get_help(). */
		ctx->prev = list[0];
		return token->comp(ctx, token, index, dst, size) < 0 ? -1 : 0;
	}
	/* Otherwise make sure the index is valid and use defaults. */
	if (index >= i)
		return -1;
	token = &token_list[list[index]];
	strlcpy(dst, token->name, size);
	/* Save index for cmd_flow_get_help(). */
	ctx->prev = list[index];
	return 0;
}

/** Populate help strings for current token (cmdline API). */
static int
cmd_flow_get_help(cmdline_parse_token_hdr_t *hdr, char *dst, unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->prev];

	(void)hdr;
	if (!size)
		return -1;
	/* Set token type and update global help with details. */
	strlcpy(dst, (token->type ? token->type : "TOKEN"), size);
	if (token->help)
		cmd_flow.help_str = token->help;
	else
		cmd_flow.help_str = token->name;
	return 0;
}

/** Token definition template (cmdline API). */
static struct cmdline_token_hdr cmd_flow_token_hdr = {
	.ops = &(struct cmdline_token_ops){
		.parse = cmd_flow_parse,
		.complete_get_nb = cmd_flow_complete_get_nb,
		.complete_get_elt = cmd_flow_complete_get_elt,
		.get_help = cmd_flow_get_help,
	},
	.offset = 0,
};

/** Populate the next dynamic token. */
static void
cmd_flow_tok(cmdline_parse_token_hdr_t **hdr,
	     cmdline_parse_token_hdr_t **hdr_inst)
{
	struct context *ctx = &cmd_flow_context;

	/* Always reinitialize context before requesting the first token. */
	if (!(hdr_inst - cmd_flow.tokens))
		cmd_flow_context_init(ctx);
	/* Return NULL when no more tokens are expected. */
	if (!ctx->next_num && ctx->curr) {
		*hdr = NULL;
		return;
	}
	/* Determine if command should end here. */
	if (ctx->eol && ctx->last && ctx->next_num) {
		const enum index *list = ctx->next[ctx->next_num - 1];
		int i;

		for (i = 0; list[i]; ++i) {
			if (list[i] != END)
				continue;
			*hdr = NULL;
			return;
		}
	}
	*hdr = &cmd_flow_token_hdr;
}

/** Dispatch parsed buffer to function calls. */
static void
cmd_flow_parsed(const struct buffer *in)
{
	switch (in->command) {
	case INFO:
		port_flow_get_info(in->port);
		break;
	case CONFIGURE:
		port_flow_configure(in->port,
				    &in->args.configure.port_attr,
				    in->args.configure.nb_queue,
				    &in->args.configure.queue_attr);
		break;
	case PATTERN_TEMPLATE_CREATE:
		port_flow_pattern_template_create(in->port,
				in->args.vc.pat_templ_id,
				&((const struct rte_flow_pattern_template_attr) {
					.relaxed_matching = in->args.vc.attr.reserved,
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.pattern);
		break;
	case PATTERN_TEMPLATE_DESTROY:
		port_flow_pattern_template_destroy(in->port,
				in->args.templ_destroy.template_id_n,
				in->args.templ_destroy.template_id);
		break;
	case ACTIONS_TEMPLATE_CREATE:
		port_flow_actions_template_create(in->port,
				in->args.vc.act_templ_id,
				&((const struct rte_flow_actions_template_attr) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions,
				in->args.vc.masks);
		break;
	case ACTIONS_TEMPLATE_DESTROY:
		port_flow_actions_template_destroy(in->port,
				in->args.templ_destroy.template_id_n,
				in->args.templ_destroy.template_id);
		break;
	case TABLE_CREATE:
		port_flow_template_table_create(in->port, in->args.table.id,
			&in->args.table.attr, in->args.table.pat_templ_id_n,
			in->args.table.pat_templ_id, in->args.table.act_templ_id_n,
			in->args.table.act_templ_id);
		break;
	case TABLE_DESTROY:
		port_flow_template_table_destroy(in->port,
					in->args.table_destroy.table_id_n,
					in->args.table_destroy.table_id);
		break;
	case QUEUE_CREATE:
		port_queue_flow_create(in->port, in->queue, in->postpone,
			in->args.vc.table_id, in->args.vc.rule_id,
			in->args.vc.pat_templ_id, in->args.vc.act_templ_id,
			in->args.vc.pattern, in->args.vc.actions);
		break;
	case QUEUE_DESTROY:
		port_queue_flow_destroy(in->port, in->queue, in->postpone,
					in->args.destroy.rule_n,
					in->args.destroy.rule);
		break;
	case PUSH:
		port_queue_flow_push(in->port, in->queue);
		break;
	case PULL:
		port_queue_flow_pull(in->port, in->queue);
		break;
	case QUEUE_AGED:
		port_queue_flow_aged(in->port, in->queue,
				     in->args.aged.destroy);
		break;
	case QUEUE_INDIRECT_ACTION_CREATE:
		port_queue_action_handle_create(
				in->port, in->queue, in->postpone,
				in->args.vc.attr.group,
				&((const struct rte_flow_indir_action_conf) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions);
		break;
	case QUEUE_INDIRECT_ACTION_DESTROY:
		port_queue_action_handle_destroy(in->port,
					   in->queue, in->postpone,
					   in->args.ia_destroy.action_id_n,
					   in->args.ia_destroy.action_id);
		break;
	case QUEUE_INDIRECT_ACTION_UPDATE:
		port_queue_action_handle_update(in->port,
						in->queue, in->postpone,
						in->args.vc.attr.group,
						in->args.vc.actions);
		break;
	case QUEUE_INDIRECT_ACTION_QUERY:
		port_queue_action_handle_query(in->port,
					       in->queue, in->postpone,
					       in->args.ia.action_id);
		break;
	case QUEUE_INDIRECT_ACTION_QUERY_UPDATE:
		port_queue_action_handle_query_update(in->port, in->queue,
						      in->postpone,
						      in->args.ia.action_id,
						      in->args.ia.qu_mode,
						      in->args.vc.actions);
		break;
	case INDIRECT_ACTION_CREATE:
		port_action_handle_create(
				in->port, in->args.vc.attr.group,
				&((const struct rte_flow_indir_action_conf) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions);
		break;
	case INDIRECT_ACTION_DESTROY:
		port_action_handle_destroy(in->port,
					   in->args.ia_destroy.action_id_n,
					   in->args.ia_destroy.action_id);
		break;
	case INDIRECT_ACTION_UPDATE:
		port_action_handle_update(in->port, in->args.vc.attr.group,
					  in->args.vc.actions);
		break;
	case INDIRECT_ACTION_QUERY:
		port_action_handle_query(in->port, in->args.ia.action_id);
		break;
	case INDIRECT_ACTION_QUERY_UPDATE:
		port_action_handle_query_update(in->port,
						in->args.ia.action_id,
						in->args.ia.qu_mode,
						in->args.vc.actions);
		break;
	case VALIDATE:
		port_flow_validate(in->port, &in->args.vc.attr,
				   in->args.vc.pattern, in->args.vc.actions,
				   &in->args.vc.tunnel_ops);
		break;
	case CREATE:
		port_flow_create(in->port, &in->args.vc.attr,
				 in->args.vc.pattern, in->args.vc.actions,
				 &in->args.vc.tunnel_ops);
		break;
	case DESTROY:
		port_flow_destroy(in->port, in->args.destroy.rule_n,
				  in->args.destroy.rule);
		break;
	case FLUSH:
		port_flow_flush(in->port);
		break;
	case DUMP_ONE:
	case DUMP_ALL:
		port_flow_dump(in->port, in->args.dump.mode,
				in->args.dump.rule, in->args.dump.file);
		break;
	case QUERY:
		port_flow_query(in->port, in->args.query.rule,
				&in->args.query.action);
		break;
	case LIST:
		port_flow_list(in->port, in->args.list.group_n,
			       in->args.list.group);
		break;
	case ISOLATE:
		port_flow_isolate(in->port, in->args.isolate.set);
		break;
	case AGED:
		port_flow_aged(in->port, in->args.aged.destroy);
		break;
	case TUNNEL_CREATE:
		port_flow_tunnel_create(in->port, &in->args.vc.tunnel_ops);
		break;
	case TUNNEL_DESTROY:
		port_flow_tunnel_destroy(in->port, in->args.vc.tunnel_ops.id);
		break;
	case TUNNEL_LIST:
		port_flow_tunnel_list(in->port);
		break;
	case ACTION_POL_G:
		port_meter_policy_add(in->port, in->args.policy.policy_id,
					in->args.vc.actions);
		break;
	case FLEX_ITEM_CREATE:
		flex_item_create(in->port, in->args.flex.token,
				 in->args.flex.filename);
		break;
	case FLEX_ITEM_DESTROY:
		flex_item_destroy(in->port, in->args.flex.token);
		break;
	default:
		break;
	}
}

/** Token generator and output processing callback (cmdline API). */
static void
cmd_flow_cb(void *arg0, struct cmdline *cl, void *arg2)
{
	if (cl == NULL)
		cmd_flow_tok(arg0, arg2);
	else
		cmd_flow_parsed(arg0);
}

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_flow = {
	.f = cmd_flow_cb,
	.data = NULL, /**< Unused. */
	.help_str = NULL, /**< Updated by cmd_flow_get_help(). */
	.tokens = {
		NULL,
	}, /**< Tokens are returned by cmd_flow_tok(). */
};

/** set cmd facility. Reuse cmd flow's infrastructure as much as possible. */

static void
update_fields(uint8_t *buf, struct rte_flow_item *item, uint16_t next_proto)
{
	struct rte_ipv4_hdr *ipv4;
	struct rte_ether_hdr *eth;
	struct rte_ipv6_hdr *ipv6;
	struct rte_vxlan_hdr *vxlan;
	struct rte_vxlan_gpe_hdr *gpe;
	struct rte_flow_item_nvgre *nvgre;
	uint32_t ipv6_vtc_flow;

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		eth = (struct rte_ether_hdr *)buf;
		if (next_proto)
			eth->ether_type = rte_cpu_to_be_16(next_proto);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		ipv4 = (struct rte_ipv4_hdr *)buf;
		if (!ipv4->version_ihl)
			ipv4->version_ihl = RTE_IPV4_VHL_DEF;
		if (next_proto && ipv4->next_proto_id == 0)
			ipv4->next_proto_id = (uint8_t)next_proto;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		ipv6 = (struct rte_ipv6_hdr *)buf;
		if (next_proto && ipv6->proto == 0)
			ipv6->proto = (uint8_t)next_proto;
		ipv6_vtc_flow = rte_be_to_cpu_32(ipv6->vtc_flow);
		ipv6_vtc_flow &= 0x0FFFFFFF; /*< reset version bits. */
		ipv6_vtc_flow |= 0x60000000; /*< set ipv6 version. */
		ipv6->vtc_flow = rte_cpu_to_be_32(ipv6_vtc_flow);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		vxlan = (struct rte_vxlan_hdr *)buf;
		vxlan->vx_flags = 0x08;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		gpe = (struct rte_vxlan_gpe_hdr *)buf;
		gpe->vx_flags = 0x0C;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		nvgre = (struct rte_flow_item_nvgre *)buf;
		nvgre->protocol = rte_cpu_to_be_16(0x6558);
		nvgre->c_k_s_rsvd0_ver = rte_cpu_to_be_16(0x2000);
		break;
	default:
		break;
	}
}

/** Helper of get item's default mask. */
static const void *
flow_item_default_mask(const struct rte_flow_item *item)
{
	const void *mask = NULL;
	static rte_be32_t gre_key_default_mask = RTE_BE32(UINT32_MAX);
	static struct rte_flow_item_ipv6_routing_ext ipv6_routing_ext_default_mask = {
		.hdr = {
			.next_hdr = 0xff,
			.type = 0xff,
			.segments_left = 0xff,
		},
	};

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ANY:
		mask = &rte_flow_item_any_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_ID:
		mask = &rte_flow_item_port_id_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_RAW:
		mask = &rte_flow_item_raw_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ETH:
		mask = &rte_flow_item_eth_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		mask = &rte_flow_item_vlan_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		mask = &rte_flow_item_ipv4_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		mask = &rte_flow_item_ipv6_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP:
		mask = &rte_flow_item_icmp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		mask = &rte_flow_item_udp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		mask = &rte_flow_item_tcp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		mask = &rte_flow_item_sctp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		mask = &rte_flow_item_vxlan_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		mask = &rte_flow_item_vxlan_gpe_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_E_TAG:
		mask = &rte_flow_item_e_tag_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		mask = &rte_flow_item_nvgre_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		mask = &rte_flow_item_mpls_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		mask = &rte_flow_item_gre_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		mask = &gre_key_default_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_META:
		mask = &rte_flow_item_meta_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_FUZZY:
		mask = &rte_flow_item_fuzzy_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP:
		mask = &rte_flow_item_gtp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP_PSC:
		mask = &rte_flow_item_gtp_psc_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		mask = &rte_flow_item_geneve_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
		mask = &rte_flow_item_geneve_opt_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PPPOE_PROTO_ID:
		mask = &rte_flow_item_pppoe_proto_id_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
		mask = &rte_flow_item_l2tpv3oip_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		mask = &rte_flow_item_esp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_AH:
		mask = &rte_flow_item_ah_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PFCP:
		mask = &rte_flow_item_pfcp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
	case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		mask = &rte_flow_item_ethdev_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_L2TPV2:
		mask = &rte_flow_item_l2tpv2_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_PPP:
		mask = &rte_flow_item_ppp_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_METER_COLOR:
		mask = &rte_flow_item_meter_color_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
		mask = &ipv6_routing_ext_default_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY:
		mask = &rte_flow_item_aggr_affinity_mask;
		break;
	default:
		break;
	}
	return mask;
}

/** Dispatch parsed buffer to function calls. */
static void
cmd_set_raw_parsed_sample(const struct buffer *in)
{
	uint32_t n = in->args.vc.actions_n;
	uint32_t i = 0;
	struct rte_flow_action *action = NULL;
	struct rte_flow_action *data = NULL;
	const struct rte_flow_action_rss *rss = NULL;
	size_t size = 0;
	uint16_t idx = in->port; /* We borrow port field as index */
	uint32_t max_size = sizeof(struct rte_flow_action) *
						ACTION_SAMPLE_ACTIONS_NUM;

	RTE_ASSERT(in->command == SET_SAMPLE_ACTIONS);
	data = (struct rte_flow_action *)&raw_sample_confs[idx].data;
	memset(data, 0x00, max_size);
	for (; i <= n - 1; i++) {
		action = in->args.vc.actions + i;
		if (action->type == RTE_FLOW_ACTION_TYPE_END)
			break;
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_MARK:
			size = sizeof(struct rte_flow_action_mark);
			rte_memcpy(&sample_mark[idx],
				(const void *)action->conf, size);
			action->conf = &sample_mark[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			size = sizeof(struct rte_flow_action_count);
			rte_memcpy(&sample_count[idx],
				(const void *)action->conf, size);
			action->conf = &sample_count[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			size = sizeof(struct rte_flow_action_queue);
			rte_memcpy(&sample_queue[idx],
				(const void *)action->conf, size);
			action->conf = &sample_queue[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			size = sizeof(struct rte_flow_action_rss);
			rss = action->conf;
			rte_memcpy(&sample_rss_data[idx].conf,
				   (const void *)rss, size);
			if (rss->key_len && rss->key) {
				sample_rss_data[idx].conf.key =
						sample_rss_data[idx].key;
				rte_memcpy((void *)((uintptr_t)
					   sample_rss_data[idx].conf.key),
					   (const void *)rss->key,
					   sizeof(uint8_t) * rss->key_len);
			}
			if (rss->queue_num && rss->queue) {
				sample_rss_data[idx].conf.queue =
						sample_rss_data[idx].queue;
				rte_memcpy((void *)((uintptr_t)
					   sample_rss_data[idx].conf.queue),
					   (const void *)rss->queue,
					   sizeof(uint16_t) * rss->queue_num);
			}
			action->conf = &sample_rss_data[idx].conf;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			size = sizeof(struct rte_flow_action_raw_encap);
			rte_memcpy(&sample_encap[idx],
				(const void *)action->conf, size);
			action->conf = &sample_encap[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			size = sizeof(struct rte_flow_action_port_id);
			rte_memcpy(&sample_port_id[idx],
				(const void *)action->conf, size);
			action->conf = &sample_port_id[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_PF:
			break;
		case RTE_FLOW_ACTION_TYPE_VF:
			size = sizeof(struct rte_flow_action_vf);
			rte_memcpy(&sample_vf[idx],
					(const void *)action->conf, size);
			action->conf = &sample_vf[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			size = sizeof(struct rte_flow_action_vxlan_encap);
			parse_setup_vxlan_encap_data(&sample_vxlan_encap[idx]);
			action->conf = &sample_vxlan_encap[idx].conf;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			size = sizeof(struct rte_flow_action_nvgre_encap);
			parse_setup_nvgre_encap_data(&sample_nvgre_encap[idx]);
			action->conf = &sample_nvgre_encap[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			size = sizeof(struct rte_flow_action_ethdev);
			rte_memcpy(&sample_port_representor[idx],
					(const void *)action->conf, size);
			action->conf = &sample_port_representor[idx];
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			size = sizeof(struct rte_flow_action_ethdev);
			rte_memcpy(&sample_represented_port[idx],
					(const void *)action->conf, size);
			action->conf = &sample_represented_port[idx];
			break;
		default:
			fprintf(stderr, "Error - Not supported action\n");
			return;
		}
		rte_memcpy(data, action, sizeof(struct rte_flow_action));
		data++;
	}
}

/** Dispatch parsed buffer to function calls. */
static void
cmd_set_raw_parsed(const struct buffer *in)
{
	uint32_t n = in->args.vc.pattern_n;
	int i = 0;
	struct rte_flow_item *item = NULL;
	size_t size = 0;
	uint8_t *data = NULL;
	uint8_t *data_tail = NULL;
	size_t *total_size = NULL;
	uint16_t upper_layer = 0;
	uint16_t proto = 0;
	uint16_t idx = in->port; /* We borrow port field as index */
	int gtp_psc = -1; /* GTP PSC option index. */
	const void *src_spec;

	if (in->command == SET_SAMPLE_ACTIONS)
		return cmd_set_raw_parsed_sample(in);
	RTE_ASSERT(in->command == SET_RAW_ENCAP ||
		   in->command == SET_RAW_DECAP);
	if (in->command == SET_RAW_ENCAP) {
		total_size = &raw_encap_confs[idx].size;
		data = (uint8_t *)&raw_encap_confs[idx].data;
	} else {
		total_size = &raw_decap_confs[idx].size;
		data = (uint8_t *)&raw_decap_confs[idx].data;
	}
	*total_size = 0;
	memset(data, 0x00, ACTION_RAW_ENCAP_MAX_DATA);
	/* process hdr from upper layer to low layer (L3/L4 -> L2). */
	data_tail = data + ACTION_RAW_ENCAP_MAX_DATA;
	for (i = n - 1 ; i >= 0; --i) {
		const struct rte_flow_item_gtp *gtp;
		const struct rte_flow_item_geneve_opt *opt;
		struct rte_flow_item_ipv6_routing_ext *ext;

		item = in->args.vc.pattern + i;
		if (item->spec == NULL)
			item->spec = flow_item_default_mask(item);
		src_spec = item->spec;
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			size = sizeof(struct rte_ether_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size = sizeof(struct rte_vlan_hdr);
			proto = RTE_ETHER_TYPE_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			size = sizeof(struct rte_ipv4_hdr);
			proto = RTE_ETHER_TYPE_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			size = sizeof(struct rte_ipv6_hdr);
			proto = RTE_ETHER_TYPE_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
			ext = (struct rte_flow_item_ipv6_routing_ext *)(uintptr_t)item->spec;
			if (!ext->hdr.hdr_len) {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext->hdr.segments_left << 4);
				ext->hdr.hdr_len = ext->hdr.segments_left << 1;
			} else {
				size = sizeof(struct rte_ipv6_routing_ext) +
					(ext->hdr.hdr_len << 3);
			}
			proto = IPPROTO_ROUTING;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			size = sizeof(struct rte_udp_hdr);
			proto = 0x11;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size = sizeof(struct rte_tcp_hdr);
			proto = 0x06;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			size = sizeof(struct rte_vxlan_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			size = sizeof(struct rte_vxlan_gpe_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			size = sizeof(struct rte_gre_hdr);
			proto = 0x2F;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			size = sizeof(rte_be32_t);
			proto = 0x0;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			size = sizeof(struct rte_mpls_hdr);
			proto = 0x0;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			size = sizeof(struct rte_flow_item_nvgre);
			proto = 0x2F;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			size = sizeof(struct rte_geneve_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
			opt = (const struct rte_flow_item_geneve_opt *)
								item->spec;
			size = offsetof(struct rte_flow_item_geneve_opt,
					option_len) + sizeof(uint8_t);
			if (opt->option_len && opt->data) {
				*total_size += opt->option_len *
					       sizeof(uint32_t);
				rte_memcpy(data_tail - (*total_size),
					   opt->data,
					   opt->option_len * sizeof(uint32_t));
			}
			break;
		case RTE_FLOW_ITEM_TYPE_L2TPV3OIP:
			size = sizeof(rte_be32_t);
			proto = 0x73;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			size = sizeof(struct rte_esp_hdr);
			proto = 0x32;
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			size = sizeof(struct rte_flow_item_ah);
			proto = 0x33;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			if (gtp_psc < 0) {
				size = sizeof(struct rte_gtp_hdr);
				break;
			}
			if (gtp_psc != i + 1) {
				fprintf(stderr,
					"Error - GTP PSC does not follow GTP\n");
				goto error;
			}
			gtp = item->spec;
			if (gtp->hdr.s == 1 || gtp->hdr.pn == 1) {
				/* Only E flag should be set. */
				fprintf(stderr,
					"Error - GTP unsupported flags\n");
				goto error;
			} else {
				struct rte_gtp_hdr_ext_word ext_word = {
					.next_ext = 0x85
				};

				/* We have to add GTP header extra word. */
				*total_size += sizeof(ext_word);
				rte_memcpy(data_tail - (*total_size),
					   &ext_word, sizeof(ext_word));
			}
			size = sizeof(struct rte_gtp_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			if (gtp_psc >= 0) {
				fprintf(stderr,
					"Error - Multiple GTP PSC items\n");
				goto error;
			} else {
				const struct rte_flow_item_gtp_psc
					*opt = item->spec;
				struct rte_gtp_psc_generic_hdr *hdr;
				size_t hdr_size = RTE_ALIGN(sizeof(*hdr),
							 sizeof(int32_t));

				*total_size += hdr_size;
				hdr = (typeof(hdr))(data_tail - (*total_size));
				memset(hdr, 0, hdr_size);
				*hdr = opt->hdr;
				hdr->ext_hdr_len = 1;
				gtp_psc = i;
				size = 0;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_PFCP:
			size = sizeof(struct rte_flow_item_pfcp);
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX:
			if (item->spec != NULL) {
				size = ((const struct rte_flow_item_flex *)item->spec)->length;
				src_spec = ((const struct rte_flow_item_flex *)item->spec)->pattern;
			} else {
				size = 0;
				src_spec = NULL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
			size = 0;
			if (item->spec) {
				const struct rte_flow_item_gre_opt
					*opt = item->spec;
				if (opt->checksum_rsvd.checksum) {
					*total_size +=
						sizeof(opt->checksum_rsvd);
					rte_memcpy(data_tail - (*total_size),
						   &opt->checksum_rsvd,
						   sizeof(opt->checksum_rsvd));
				}
				if (opt->key.key) {
					*total_size += sizeof(opt->key.key);
					rte_memcpy(data_tail - (*total_size),
						   &opt->key.key,
						   sizeof(opt->key.key));
				}
				if (opt->sequence.sequence) {
					*total_size += sizeof(opt->sequence.sequence);
					rte_memcpy(data_tail - (*total_size),
						   &opt->sequence.sequence,
						   sizeof(opt->sequence.sequence));
				}
			}
			proto = 0x2F;
			break;
		default:
			fprintf(stderr, "Error - Not supported item\n");
			goto error;
		}
		if (size) {
			*total_size += size;
			rte_memcpy(data_tail - (*total_size), src_spec, size);
			/* update some fields which cannot be set by cmdline */
			update_fields((data_tail - (*total_size)), item,
				      upper_layer);
			upper_layer = proto;
		}
	}
	if (verbose_level & 0x1)
		printf("total data size is %zu\n", (*total_size));
	RTE_ASSERT((*total_size) <= ACTION_RAW_ENCAP_MAX_DATA);
	memmove(data, (data_tail - (*total_size)), *total_size);
	return;

error:
	*total_size = 0;
	memset(data, 0x00, ACTION_RAW_ENCAP_MAX_DATA);
}

/** Populate help strings for current token (cmdline API). */
static int
cmd_set_raw_get_help(cmdline_parse_token_hdr_t *hdr, char *dst,
		     unsigned int size)
{
	struct context *ctx = &cmd_flow_context;
	const struct token *token = &token_list[ctx->prev];

	(void)hdr;
	if (!size)
		return -1;
	/* Set token type and update global help with details. */
	snprintf(dst, size, "%s", (token->type ? token->type : "TOKEN"));
	if (token->help)
		cmd_set_raw.help_str = token->help;
	else
		cmd_set_raw.help_str = token->name;
	return 0;
}

/** Token definition template (cmdline API). */
static struct cmdline_token_hdr cmd_set_raw_token_hdr = {
	.ops = &(struct cmdline_token_ops){
		.parse = cmd_flow_parse,
		.complete_get_nb = cmd_flow_complete_get_nb,
		.complete_get_elt = cmd_flow_complete_get_elt,
		.get_help = cmd_set_raw_get_help,
	},
	.offset = 0,
};

/** Populate the next dynamic token. */
static void
cmd_set_raw_tok(cmdline_parse_token_hdr_t **hdr,
	     cmdline_parse_token_hdr_t **hdr_inst)
{
	struct context *ctx = &cmd_flow_context;

	/* Always reinitialize context before requesting the first token. */
	if (!(hdr_inst - cmd_set_raw.tokens)) {
		cmd_flow_context_init(ctx);
		ctx->curr = START_SET;
	}
	/* Return NULL when no more tokens are expected. */
	if (!ctx->next_num && (ctx->curr != START_SET)) {
		*hdr = NULL;
		return;
	}
	/* Determine if command should end here. */
	if (ctx->eol && ctx->last && ctx->next_num) {
		const enum index *list = ctx->next[ctx->next_num - 1];
		int i;

		for (i = 0; list[i]; ++i) {
			if (list[i] != END)
				continue;
			*hdr = NULL;
			return;
		}
	}
	*hdr = &cmd_set_raw_token_hdr;
}

/** Token generator and output processing callback (cmdline API). */
static void
cmd_set_raw_cb(void *arg0, struct cmdline *cl, void *arg2)
{
	if (cl == NULL)
		cmd_set_raw_tok(arg0, arg2);
	else
		cmd_set_raw_parsed(arg0);
}

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_set_raw = {
	.f = cmd_set_raw_cb,
	.data = NULL, /**< Unused. */
	.help_str = NULL, /**< Updated by cmd_flow_get_help(). */
	.tokens = {
		NULL,
	}, /**< Tokens are returned by cmd_flow_tok(). */
};

/* *** display raw_encap/raw_decap buf */
struct cmd_show_set_raw_result {
	cmdline_fixed_string_t cmd_show;
	cmdline_fixed_string_t cmd_what;
	cmdline_fixed_string_t cmd_all;
	uint16_t cmd_index;
};

static void
cmd_show_set_raw_parsed(void *parsed_result, struct cmdline *cl, void *data)
{
	struct cmd_show_set_raw_result *res = parsed_result;
	uint16_t index = res->cmd_index;
	uint8_t all = 0;
	uint8_t *raw_data = NULL;
	size_t raw_size = 0;
	char title[16] = {0};

	RTE_SET_USED(cl);
	RTE_SET_USED(data);
	if (!strcmp(res->cmd_all, "all")) {
		all = 1;
		index = 0;
	} else if (index >= RAW_ENCAP_CONFS_MAX_NUM) {
		fprintf(stderr, "index should be 0-%u\n",
			RAW_ENCAP_CONFS_MAX_NUM - 1);
		return;
	}
	do {
		if (!strcmp(res->cmd_what, "raw_encap")) {
			raw_data = (uint8_t *)&raw_encap_confs[index].data;
			raw_size = raw_encap_confs[index].size;
			snprintf(title, 16, "\nindex: %u", index);
			rte_hexdump(stdout, title, raw_data, raw_size);
		} else {
			raw_data = (uint8_t *)&raw_decap_confs[index].data;
			raw_size = raw_decap_confs[index].size;
			snprintf(title, 16, "\nindex: %u", index);
			rte_hexdump(stdout, title, raw_data, raw_size);
		}
	} while (all && ++index < RAW_ENCAP_CONFS_MAX_NUM);
}

static cmdline_parse_token_string_t cmd_show_set_raw_cmd_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_show, "show");
static cmdline_parse_token_string_t cmd_show_set_raw_cmd_what =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_what, "raw_encap#raw_decap");
static cmdline_parse_token_num_t cmd_show_set_raw_cmd_index =
	TOKEN_NUM_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_index, RTE_UINT16);
static cmdline_parse_token_string_t cmd_show_set_raw_cmd_all =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_all, "all");
cmdline_parse_inst_t cmd_show_set_raw = {
	.f = cmd_show_set_raw_parsed,
	.data = NULL,
	.help_str = "show <raw_encap|raw_decap> <index>",
	.tokens = {
		(void *)&cmd_show_set_raw_cmd_show,
		(void *)&cmd_show_set_raw_cmd_what,
		(void *)&cmd_show_set_raw_cmd_index,
		NULL,
	},
};
cmdline_parse_inst_t cmd_show_set_raw_all = {
	.f = cmd_show_set_raw_parsed,
	.data = NULL,
	.help_str = "show <raw_encap|raw_decap> all",
	.tokens = {
		(void *)&cmd_show_set_raw_cmd_show,
		(void *)&cmd_show_set_raw_cmd_what,
		(void *)&cmd_show_set_raw_cmd_all,
		NULL,
	},
};
