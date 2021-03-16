/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

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
	INTEGER,
	UNSIGNED,
	PREFIX,
	BOOLEAN,
	STRING,
	HEX,
	FILE_PATH,
	MAC_ADDR,
	IPV4_ADDR,
	IPV6_ADDR,
	RULE_ID,
	PORT_ID,
	GROUP_ID,
	PRIORITY_LEVEL,
	SHARED_ACTION_ID,

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
	SHARED_ACTION,
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
	GROUP,
	PRIORITY,
	INGRESS,
	EGRESS,
	TRANSFER,
	TUNNEL_SET,
	TUNNEL_MATCH,

	/* Shared action arguments */
	SHARED_ACTION_CREATE,
	SHARED_ACTION_UPDATE,
	SHARED_ACTION_DESTROY,
	SHARED_ACTION_QUERY,

	/* Shared action create arguments */
	SHARED_ACTION_CREATE_ID,
	SHARED_ACTION_INGRESS,
	SHARED_ACTION_EGRESS,
	SHARED_ACTION_TRANSFER,
	SHARED_ACTION_SPEC,

	/* Shared action destroy arguments */
	SHARED_ACTION_DESTROY_ID,

	/* Validate/create pattern. */
	PATTERN,
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
	ITEM_PF,
	ITEM_VF,
	ITEM_VF_ID,
	ITEM_PHY_PORT,
	ITEM_PHY_PORT_INDEX,
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
	ITEM_IPV4_TOS,
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
	ITEM_ICMP6,
	ITEM_ICMP6_TYPE,
	ITEM_ICMP6_CODE,
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

	/* Validate/create actions. */
	ACTIONS,
	ACTION_NEXT,
	ACTION_END,
	ACTION_VOID,
	ACTION_PASSTHRU,
	ACTION_JUMP,
	ACTION_JUMP_GROUP,
	ACTION_MARK,
	ACTION_MARK_ID,
	ACTION_FLAG,
	ACTION_QUEUE,
	ACTION_QUEUE_INDEX,
	ACTION_DROP,
	ACTION_COUNT,
	ACTION_COUNT_SHARED,
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
	ACTION_PHY_PORT,
	ACTION_PHY_PORT_ORIGINAL,
	ACTION_PHY_PORT_INDEX,
	ACTION_PORT_ID,
	ACTION_PORT_ID_ORIGINAL,
	ACTION_PORT_ID_ID,
	ACTION_METER,
	ACTION_METER_ID,
	ACTION_OF_SET_MPLS_TTL,
	ACTION_OF_SET_MPLS_TTL_MPLS_TTL,
	ACTION_OF_DEC_MPLS_TTL,
	ACTION_OF_SET_NW_TTL,
	ACTION_OF_SET_NW_TTL_NW_TTL,
	ACTION_OF_DEC_NW_TTL,
	ACTION_OF_COPY_TTL_OUT,
	ACTION_OF_COPY_TTL_IN,
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
	ACTION_SAMPLE,
	ACTION_SAMPLE_RATIO,
	ACTION_SAMPLE_INDEX,
	ACTION_SAMPLE_INDEX_VALUE,
	ACTION_SHARED,
	SHARED_ACTION_ID2PTR,
};

/** Maximum size for pattern in struct rte_flow_item_raw. */
#define ITEM_RAW_PATTERN_SIZE 40

/** Storage size for struct rte_flow_item_raw including pattern. */
#define ITEM_RAW_SIZE \
	(sizeof(struct rte_flow_item_raw) + ITEM_RAW_PATTERN_SIZE)

/** Maximum number of queue indices in struct rte_flow_action_rss. */
#define ACTION_RSS_QUEUE_NUM 128

/** Storage for struct rte_flow_action_rss including external data. */
struct action_rss_data {
	struct rte_flow_action_rss conf;
	uint8_t key[RSS_HASH_KEY_LENGTH];
	uint16_t queue[ACTION_RSS_QUEUE_NUM];
};

/** Maximum data size in struct rte_flow_action_raw_encap. */
#define ACTION_RAW_ENCAP_MAX_DATA 128
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
	union {
		struct {
			uint32_t *action_id;
			uint32_t action_id_n;
		} sa_destroy; /**< Shared action destroy arguments. */
		struct {
			uint32_t action_id;
		} sa; /* Shared action query arguments */
		struct {
			struct rte_flow_attr attr;
			struct tunnel_ops tunnel_ops;
			struct rte_flow_item *pattern;
			struct rte_flow_action *actions;
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

static const enum index next_sa_create_attr[] = {
	SHARED_ACTION_CREATE_ID,
	SHARED_ACTION_INGRESS,
	SHARED_ACTION_EGRESS,
	SHARED_ACTION_TRANSFER,
	SHARED_ACTION_SPEC,
	ZERO,
};

static const enum index next_sa_subcmd[] = {
	SHARED_ACTION_CREATE,
	SHARED_ACTION_UPDATE,
	SHARED_ACTION_DESTROY,
	SHARED_ACTION_QUERY,
	ZERO,
};

static const enum index next_vc_attr[] = {
	GROUP,
	PRIORITY,
	INGRESS,
	EGRESS,
	TRANSFER,
	TUNNEL_SET,
	TUNNEL_MATCH,
	PATTERN,
	ZERO,
};

static const enum index next_destroy_attr[] = {
	DESTROY_RULE,
	END,
	ZERO,
};

static const enum index next_dump_attr[] = {
	FILE_PATH,
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

static const enum index next_sa_destroy_attr[] = {
	SHARED_ACTION_DESTROY_ID,
	END,
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
	ITEM_PF,
	ITEM_VF,
	ITEM_PHY_PORT,
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
	ITEM_ICMP6,
	ITEM_ICMP6_ND_NS,
	ITEM_ICMP6_ND_NA,
	ITEM_ICMP6_ND_OPT,
	ITEM_ICMP6_ND_OPT_SLA_ETH,
	ITEM_ICMP6_ND_OPT_TLA_ETH,
	ITEM_META,
	ITEM_GRE_KEY,
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

static const enum index item_vf[] = {
	ITEM_VF_ID,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_phy_port[] = {
	ITEM_PHY_PORT_INDEX,
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
	ITEM_IPV4_TOS,
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
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp6[] = {
	ITEM_ICMP6_TYPE,
	ITEM_ICMP6_CODE,
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

static const enum index next_action[] = {
	ACTION_END,
	ACTION_VOID,
	ACTION_PASSTHRU,
	ACTION_JUMP,
	ACTION_MARK,
	ACTION_FLAG,
	ACTION_QUEUE,
	ACTION_DROP,
	ACTION_COUNT,
	ACTION_RSS,
	ACTION_PF,
	ACTION_VF,
	ACTION_PHY_PORT,
	ACTION_PORT_ID,
	ACTION_METER,
	ACTION_OF_SET_MPLS_TTL,
	ACTION_OF_DEC_MPLS_TTL,
	ACTION_OF_SET_NW_TTL,
	ACTION_OF_DEC_NW_TTL,
	ACTION_OF_COPY_TTL_OUT,
	ACTION_OF_COPY_TTL_IN,
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
	ACTION_SAMPLE,
	ACTION_SHARED,
	ZERO,
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
	ACTION_COUNT_SHARED,
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

static const enum index action_phy_port[] = {
	ACTION_PHY_PORT_ORIGINAL,
	ACTION_PHY_PORT_INDEX,
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

static const enum index action_of_set_mpls_ttl[] = {
	ACTION_OF_SET_MPLS_TTL_MPLS_TTL,
	ACTION_NEXT,
	ZERO,
};

static const enum index action_of_set_nw_ttl[] = {
	ACTION_OF_SET_NW_TTL_NW_TTL,
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

static const enum index action_sample[] = {
	ACTION_SAMPLE,
	ACTION_SAMPLE_RATIO,
	ACTION_SAMPLE_INDEX,
	ACTION_NEXT,
	ZERO,
};

static const enum index next_action_sample[] = {
	ACTION_QUEUE,
	ACTION_MARK,
	ACTION_COUNT,
	ACTION_PORT_ID,
	ACTION_RAW_ENCAP,
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
static int parse_vc_item_ecpri_type(struct context *, const struct token *,
				    const char *, unsigned int,
				    void *, unsigned int);
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
static int parse_tunnel(struct context *, const struct token *,
			const char *, unsigned int,
			void *, unsigned int);
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
static int parse_sa(struct context *, const struct token *,
		    const char *, unsigned int,
		    void *, unsigned int);
static int parse_sa_destroy(struct context *ctx, const struct token *token,
			    const char *str, unsigned int len,
			    void *buf, unsigned int size);
static int parse_sa_id2ptr(struct context *ctx, const struct token *token,
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

/** Token definitions. */
static const struct token token_list[] = {
	/* Special tokens. */
	[ZERO] = {
		.name = "ZERO",
		.help = "null entry, abused as the entry point",
		.next = NEXT(NEXT_ENTRY(FLOW)),
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
	[INTEGER] = {
		.name = "{int}",
		.type = "INTEGER",
		.help = "integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[UNSIGNED] = {
		.name = "{unsigned}",
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.call = parse_int,
		.comp = comp_none,
	},
	[PREFIX] = {
		.name = "{prefix}",
		.type = "PREFIX",
		.help = "prefix length for bit-mask",
		.call = parse_prefix,
		.comp = comp_none,
	},
	[BOOLEAN] = {
		.name = "{boolean}",
		.type = "BOOLEAN",
		.help = "any boolean value",
		.call = parse_boolean,
		.comp = comp_boolean,
	},
	[STRING] = {
		.name = "{string}",
		.type = "STRING",
		.help = "fixed string",
		.call = parse_string,
		.comp = comp_none,
	},
	[HEX] = {
		.name = "{hex}",
		.type = "HEX",
		.help = "fixed string",
		.call = parse_hex,
	},
	[FILE_PATH] = {
		.name = "{file path}",
		.type = "STRING",
		.help = "file path",
		.call = parse_string0,
		.comp = comp_none,
	},
	[MAC_ADDR] = {
		.name = "{MAC address}",
		.type = "MAC-48",
		.help = "standard MAC address notation",
		.call = parse_mac_addr,
		.comp = comp_none,
	},
	[IPV4_ADDR] = {
		.name = "{IPv4 address}",
		.type = "IPV4 ADDRESS",
		.help = "standard IPv4 address notation",
		.call = parse_ipv4_addr,
		.comp = comp_none,
	},
	[IPV6_ADDR] = {
		.name = "{IPv6 address}",
		.type = "IPV6 ADDRESS",
		.help = "standard IPv6 address notation",
		.call = parse_ipv6_addr,
		.comp = comp_none,
	},
	[RULE_ID] = {
		.name = "{rule id}",
		.type = "RULE ID",
		.help = "rule identifier",
		.call = parse_int,
		.comp = comp_rule_id,
	},
	[PORT_ID] = {
		.name = "{port_id}",
		.type = "PORT ID",
		.help = "port identifier",
		.call = parse_port,
		.comp = comp_port,
	},
	[GROUP_ID] = {
		.name = "{group_id}",
		.type = "GROUP ID",
		.help = "group identifier",
		.call = parse_int,
		.comp = comp_none,
	},
	[PRIORITY_LEVEL] = {
		.name = "{level}",
		.type = "PRIORITY",
		.help = "priority level",
		.call = parse_int,
		.comp = comp_none,
	},
	[SHARED_ACTION_ID] = {
		.name = "{shared_action_id}",
		.type = "SHARED_ACTION_ID",
		.help = "shared action id",
		.call = parse_int,
		.comp = comp_none,
	},
	/* Top-level command. */
	[FLOW] = {
		.name = "flow",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage ingress/egress flow rules",
		.next = NEXT(NEXT_ENTRY
			     (SHARED_ACTION,
			      VALIDATE,
			      CREATE,
			      DESTROY,
			      FLUSH,
			      DUMP,
			      LIST,
			      AGED,
			      QUERY,
			      ISOLATE,
			      TUNNEL)),
		.call = parse_init,
	},
	/* Top-level command. */
	[SHARED_ACTION] = {
		.name = "shared_action",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage shared actions",
		.next = NEXT(next_sa_subcmd, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_sa,
	},
	/* Sub-level commands. */
	[SHARED_ACTION_CREATE] = {
		.name = "create",
		.help = "create shared action",
		.next = NEXT(next_sa_create_attr),
		.call = parse_sa,
	},
	[SHARED_ACTION_UPDATE] = {
		.name = "update",
		.help = "update shared action",
		.next = NEXT(NEXT_ENTRY(SHARED_ACTION_SPEC),
			     NEXT_ENTRY(SHARED_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
		.call = parse_sa,
	},
	[SHARED_ACTION_DESTROY] = {
		.name = "destroy",
		.help = "destroy shared action",
		.next = NEXT(NEXT_ENTRY(SHARED_ACTION_DESTROY_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_sa_destroy,
	},
	[SHARED_ACTION_QUERY] = {
		.name = "query",
		.help = "query shared action",
		.next = NEXT(NEXT_ENTRY(END), NEXT_ENTRY(SHARED_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.sa.action_id)),
		.call = parse_sa,
	},
	[VALIDATE] = {
		.name = "validate",
		.help = "check whether a flow rule can be created",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_vc,
	},
	[CREATE] = {
		.name = "create",
		.help = "create a flow rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_vc,
	},
	[DESTROY] = {
		.name = "destroy",
		.help = "destroy specific flow rules",
		.next = NEXT(NEXT_ENTRY(DESTROY_RULE), NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_destroy,
	},
	[FLUSH] = {
		.name = "flush",
		.help = "destroy all flow rules",
		.next = NEXT(NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_flush,
	},
	[DUMP] = {
		.name = "dump",
		.help = "dump all flow rules to file",
		.next = NEXT(next_dump_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.dump.file),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_dump,
	},
	[QUERY] = {
		.name = "query",
		.help = "query an existing flow rule",
		.next = NEXT(NEXT_ENTRY(QUERY_ACTION),
			     NEXT_ENTRY(RULE_ID),
			     NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.query.action.type),
			     ARGS_ENTRY(struct buffer, args.query.rule),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_query,
	},
	[LIST] = {
		.name = "list",
		.help = "list existing flow rules",
		.next = NEXT(next_list_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_list,
	},
	[AGED] = {
		.name = "aged",
		.help = "list and destroy aged flows",
		.next = NEXT(next_aged_attr, NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_aged,
	},
	[ISOLATE] = {
		.name = "isolate",
		.help = "restrict ingress traffic to the defined flow rules",
		.next = NEXT(NEXT_ENTRY(BOOLEAN),
			     NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.isolate.set),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_isolate,
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
			     NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	[TUNNEL_CREATE_TYPE] = {
		.name = "type",
		.help = "create new tunnel",
		.next = NEXT(NEXT_ENTRY(FILE_PATH)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, type)),
		.call = parse_tunnel,
	},
	[TUNNEL_DESTROY] = {
		.name = "destroy",
		.help = "destroy tunel",
		.next = NEXT(NEXT_ENTRY(TUNNEL_DESTROY_ID),
			     NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	[TUNNEL_DESTROY_ID] = {
		.name = "id",
		.help = "tunnel identifier to testroy",
		.next = NEXT(NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_tunnel,
	},
	[TUNNEL_LIST] = {
		.name = "list",
		.help = "list existing tunnels",
		.next = NEXT(NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, port)),
		.call = parse_tunnel,
	},
	/* Destroy arguments. */
	[DESTROY_RULE] = {
		.name = "rule",
		.help = "specify a rule identifier",
		.next = NEXT(next_destroy_attr, NEXT_ENTRY(RULE_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer, args.destroy.rule)),
		.call = parse_destroy,
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
		.next = NEXT(next_list_attr, NEXT_ENTRY(GROUP_ID)),
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
	[GROUP] = {
		.name = "group",
		.help = "specify a group",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(GROUP_ID)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, group)),
		.call = parse_vc,
	},
	[PRIORITY] = {
		.name = "priority",
		.help = "specify a priority level",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(PRIORITY_LEVEL)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_attr, priority)),
		.call = parse_vc,
	},
	[INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[TRANSFER] = {
		.name = "transfer",
		.help = "apply rule directly to endpoints found in pattern",
		.next = NEXT(next_vc_attr),
		.call = parse_vc,
	},
	[TUNNEL_SET] = {
		.name = "tunnel_set",
		.help = "tunnel steer rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_vc,
	},
	[TUNNEL_MATCH] = {
		.name = "tunnel_match",
		.help = "tunnel match rule",
		.next = NEXT(next_vc_attr, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct tunnel_ops, id)),
		.call = parse_vc,
	},
	/* Validate/create pattern. */
	[PATTERN] = {
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
		.next = NEXT(NEXT_ENTRY(ACTIONS)),
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
		.next = NEXT(item_any, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_any, num)),
	},
	[ITEM_PF] = {
		.name = "pf",
		.help = "match traffic from/to the physical function",
		.priv = PRIV_ITEM(PF, 0),
		.next = NEXT(NEXT_ENTRY(ITEM_NEXT)),
		.call = parse_vc,
	},
	[ITEM_VF] = {
		.name = "vf",
		.help = "match traffic from/to a virtual function ID",
		.priv = PRIV_ITEM(VF, sizeof(struct rte_flow_item_vf)),
		.next = NEXT(item_vf),
		.call = parse_vc,
	},
	[ITEM_VF_ID] = {
		.name = "id",
		.help = "VF ID",
		.next = NEXT(item_vf, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_vf, id)),
	},
	[ITEM_PHY_PORT] = {
		.name = "phy_port",
		.help = "match traffic from/to a specific physical port",
		.priv = PRIV_ITEM(PHY_PORT,
				  sizeof(struct rte_flow_item_phy_port)),
		.next = NEXT(item_phy_port),
		.call = parse_vc,
	},
	[ITEM_PHY_PORT_INDEX] = {
		.name = "index",
		.help = "physical port index",
		.next = NEXT(item_phy_port, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_phy_port, index)),
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
		.next = NEXT(item_port_id, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_mark, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_raw, NEXT_ENTRY(BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   relative, 1)),
	},
	[ITEM_RAW_SEARCH] = {
		.name = "search",
		.help = "search pattern from offset (see also limit)",
		.next = NEXT(item_raw, NEXT_ENTRY(BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_raw,
					   search, 1)),
	},
	[ITEM_RAW_OFFSET] = {
		.name = "offset",
		.help = "absolute or relative offset for pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(INTEGER), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, offset)),
	},
	[ITEM_RAW_LIMIT] = {
		.name = "limit",
		.help = "search area limit for start of pattern",
		.next = NEXT(item_raw, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_raw, limit)),
	},
	[ITEM_RAW_PATTERN] = {
		.name = "pattern",
		.help = "byte string to look for",
		.next = NEXT(item_raw,
			     NEXT_ENTRY(STRING),
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
		.next = NEXT(item_eth, NEXT_ENTRY(MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, dst)),
	},
	[ITEM_ETH_SRC] = {
		.name = "src",
		.help = "source MAC",
		.next = NEXT(item_eth, NEXT_ENTRY(MAC_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, src)),
	},
	[ITEM_ETH_TYPE] = {
		.name = "type",
		.help = "EtherType",
		.next = NEXT(item_eth, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_eth, type)),
	},
	[ITEM_ETH_HAS_VLAN] = {
		.name = "has_vlan",
		.help = "packet header contains VLAN",
		.next = NEXT(item_eth, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan, tci)),
	},
	[ITEM_VLAN_PCP] = {
		.name = "pcp",
		.help = "priority code point",
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  tci, "\xe0\x00")),
	},
	[ITEM_VLAN_DEI] = {
		.name = "dei",
		.help = "drop eligible indicator",
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  tci, "\x10\x00")),
	},
	[ITEM_VLAN_VID] = {
		.name = "vid",
		.help = "VLAN identifier",
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_vlan,
						  tci, "\x0f\xff")),
	},
	[ITEM_VLAN_INNER_TYPE] = {
		.name = "inner_type",
		.help = "inner EtherType",
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vlan,
					     inner_type)),
	},
	[ITEM_VLAN_HAS_MORE_VLAN] = {
		.name = "has_more_vlan",
		.help = "packet header contains another VLAN",
		.next = NEXT(item_vlan, NEXT_ENTRY(UNSIGNED), item_param),
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
	[ITEM_IPV4_TOS] = {
		.name = "tos",
		.help = "type of service",
		.next = NEXT(item_ipv4, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.type_of_service)),
	},
	[ITEM_IPV4_FRAGMENT_OFFSET] = {
		.name = "fragment_offset",
		.help = "fragmentation flags and fragment offset",
		.next = NEXT(item_ipv4, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.fragment_offset)),
	},
	[ITEM_IPV4_TTL] = {
		.name = "ttl",
		.help = "time to live",
		.next = NEXT(item_ipv4, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.time_to_live)),
	},
	[ITEM_IPV4_PROTO] = {
		.name = "proto",
		.help = "next protocol ID",
		.next = NEXT(item_ipv4, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.next_proto_id)),
	},
	[ITEM_IPV4_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(IPV4_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv4,
					     hdr.src_addr)),
	},
	[ITEM_IPV4_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv4, NEXT_ENTRY(IPV4_ADDR), item_param),
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
		.next = NEXT(item_ipv6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x0f\xf0\x00\x00")),
	},
	[ITEM_IPV6_FLOW] = {
		.name = "flow",
		.help = "flow label",
		.next = NEXT(item_ipv6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_ipv6,
						  hdr.vtc_flow,
						  "\x00\x0f\xff\xff")),
	},
	[ITEM_IPV6_PROTO] = {
		.name = "proto",
		.help = "protocol (next header)",
		.next = NEXT(item_ipv6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.proto)),
	},
	[ITEM_IPV6_HOP] = {
		.name = "hop",
		.help = "hop limit",
		.next = NEXT(item_ipv6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.hop_limits)),
	},
	[ITEM_IPV6_SRC] = {
		.name = "src",
		.help = "source address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(IPV6_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.src_addr)),
	},
	[ITEM_IPV6_DST] = {
		.name = "dst",
		.help = "destination address",
		.next = NEXT(item_ipv6, NEXT_ENTRY(IPV6_ADDR), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6,
					     hdr.dst_addr)),
	},
	[ITEM_IPV6_HAS_FRAG_EXT] = {
		.name = "has_frag_ext",
		.help = "fragment packet attribute",
		.next = NEXT(item_ipv6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_item_ipv6,
					   has_frag_ext, 1)),
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
		.next = NEXT(item_icmp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_type)),
	},
	[ITEM_ICMP_CODE] = {
		.name = "code",
		.help = "ICMP packet code",
		.next = NEXT(item_icmp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_code)),
	},
	[ITEM_ICMP_IDENT] = {
		.name = "ident",
		.help = "ICMP packet identifier",
		.next = NEXT(item_icmp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp,
					     hdr.icmp_ident)),
	},
	[ITEM_ICMP_SEQ] = {
		.name = "seq",
		.help = "ICMP packet sequence number",
		.next = NEXT(item_icmp, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_udp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_udp,
					     hdr.src_port)),
	},
	[ITEM_UDP_DST] = {
		.name = "dst",
		.help = "UDP destination port",
		.next = NEXT(item_udp, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_tcp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.src_port)),
	},
	[ITEM_TCP_DST] = {
		.name = "dst",
		.help = "TCP destination port",
		.next = NEXT(item_tcp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_tcp,
					     hdr.dst_port)),
	},
	[ITEM_TCP_FLAGS] = {
		.name = "flags",
		.help = "TCP flags",
		.next = NEXT(item_tcp, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_sctp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.src_port)),
	},
	[ITEM_SCTP_DST] = {
		.name = "dst",
		.help = "SCTP destination port",
		.next = NEXT(item_sctp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.dst_port)),
	},
	[ITEM_SCTP_TAG] = {
		.name = "tag",
		.help = "validation tag",
		.next = NEXT(item_sctp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_sctp,
					     hdr.tag)),
	},
	[ITEM_SCTP_CKSUM] = {
		.name = "cksum",
		.help = "checksum",
		.next = NEXT(item_sctp, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_vxlan, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan, vni)),
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
		.next = NEXT(item_e_tag, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_nvgre, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_mpls, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\xff\xff\xf0")),
	},
	[ITEM_MPLS_TC] = {
		.name = "tc",
		.help = "MPLS Traffic Class",
		.next = NEXT(item_mpls, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_mpls,
						  label_tc_s,
						  "\x00\x00\x0e")),
	},
	[ITEM_MPLS_S] = {
		.name = "s",
		.help = "MPLS Bottom-of-Stack",
		.next = NEXT(item_mpls, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_gre, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     protocol)),
	},
	[ITEM_GRE_C_RSVD0_VER] = {
		.name = "c_rsvd0_ver",
		.help =
			"checksum (1b), undefined (1b), key bit (1b),"
			" sequence number (1b), reserved 0 (9b),"
			" version (3b)",
		.next = NEXT(item_gre, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gre,
					     c_rsvd0_ver)),
	},
	[ITEM_GRE_C_BIT] = {
		.name = "c_bit",
		.help = "checksum bit (C)",
		.next = NEXT(item_gre, NEXT_ENTRY(BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x80\x00\x00\x00")),
	},
	[ITEM_GRE_S_BIT] = {
		.name = "s_bit",
		.help = "sequence number bit (S)",
		.next = NEXT(item_gre, NEXT_ENTRY(BOOLEAN), item_param),
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_gre,
						  c_rsvd0_ver,
						  "\x10\x00\x00\x00")),
	},
	[ITEM_GRE_K_BIT] = {
		.name = "k_bit",
		.help = "key bit (K)",
		.next = NEXT(item_gre, NEXT_ENTRY(BOOLEAN), item_param),
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
		.next = NEXT(item_fuzzy, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_gtp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp,
					v_pt_rsv_flags)),
	},
	[ITEM_GTP_MSG_TYPE] = {
		.name = "msg_type",
		.help = "GTP message type",
		.next = NEXT(item_gtp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_gtp, msg_type)),
	},
	[ITEM_GTP_TEID] = {
		.name = "teid",
		.help = "tunnel endpoint identifier",
		.next = NEXT(item_gtp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gtp, teid)),
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
		.next = NEXT(item_geneve, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve, vni)),
	},
	[ITEM_GENEVE_PROTO] = {
		.name = "protocol",
		.help = "GENEVE protocol type",
		.next = NEXT(item_geneve, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_geneve,
					     protocol)),
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
		.next = NEXT(item_vxlan_gpe, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_vxlan_gpe,
					     vni)),
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
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     sha)),
	},
	[ITEM_ARP_ETH_IPV4_SPA] = {
		.name = "spa",
		.help = "sender IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     spa)),
	},
	[ITEM_ARP_ETH_IPV4_THA] = {
		.name = "tha",
		.help = "target hardware address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(MAC_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     tha)),
	},
	[ITEM_ARP_ETH_IPV4_TPA] = {
		.name = "tpa",
		.help = "target IPv4 address",
		.next = NEXT(item_arp_eth_ipv4, NEXT_ENTRY(IPV4_ADDR),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_arp_eth_ipv4,
					     tpa)),
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
		.next = NEXT(item_ipv6_ext, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_ipv6_frag_ext,
					hdr.next_header)),
	},
	[ITEM_IPV6_FRAG_EXT_FRAG_DATA] = {
		.name = "frag_data",
		.help = "Fragment flags and offset",
		.next = NEXT(item_ipv6_frag_ext, NEXT_ENTRY(UNSIGNED),
			     item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ipv6_frag_ext,
					     hdr.frag_data)),
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
		.next = NEXT(item_icmp6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     type)),
	},
	[ITEM_ICMP6_CODE] = {
		.name = "code",
		.help = "ICMPv6 code",
		.next = NEXT(item_icmp6, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_icmp6,
					     code)),
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
		.next = NEXT(item_icmp6_nd_ns, NEXT_ENTRY(IPV6_ADDR),
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
		.next = NEXT(item_icmp6_nd_na, NEXT_ENTRY(IPV6_ADDR),
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
		.next = NEXT(item_icmp6_nd_opt, NEXT_ENTRY(UNSIGNED),
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
		.next = NEXT(item_icmp6_nd_opt_sla_eth, NEXT_ENTRY(MAC_ADDR),
			     item_param),
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
		.next = NEXT(item_icmp6_nd_opt_tla_eth, NEXT_ENTRY(MAC_ADDR),
			     item_param),
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
		.next = NEXT(item_meta, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_gre_key, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARG_ENTRY_HTON(rte_be32_t)),
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
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gtp_psc,
					qfi)),
	},
	[ITEM_GTP_PSC_PDU_T] = {
		.name = "pdu_t",
		.help = "PDU type",
		.next = NEXT(item_gtp_psc, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_gtp_psc,
					pdu_type)),
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
		.next = NEXT(item_pppoes, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pppoe,
					session_id)),
	},
	[ITEM_PPPOE_PROTO_ID] = {
		.name = "pppoe_proto_id",
		.help = "match PPPoE session protocol identifier",
		.priv = PRIV_ITEM(PPPOE_PROTO_ID,
				sizeof(struct rte_flow_item_pppoe_proto_id)),
		.next = NEXT(item_pppoe_proto_id, NEXT_ENTRY(UNSIGNED),
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
		.next = NEXT(item_higig2, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_higig2_hdr,
					hdr.ppt1.classification)),
	},
	[ITEM_HIGIG2_VID] = {
		.name = "vid",
		.help = "matches vid of higig2 header",
		.next = NEXT(item_higig2, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_tag, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_item_tag, data)),
	},
	[ITEM_TAG_INDEX] = {
		.name = "index",
		.help = "index of tag array to match",
		.next = NEXT(item_tag, NEXT_ENTRY(UNSIGNED),
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
		.next = NEXT(item_l2tpv3oip, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_esp, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_ah, NEXT_ENTRY(UNSIGNED), item_param),
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
		.next = NEXT(item_pfcp, NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_pfcp,
				s_field)),
	},
	[ITEM_PFCP_SEID] = {
		.name = "seid",
		.help = "session endpoint identifier",
		.next = NEXT(item_pfcp, NEXT_ENTRY(UNSIGNED), item_param),
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
				NEXT_ENTRY(UNSIGNED), item_param),
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
				NEXT_ENTRY(UNSIGNED), item_param),
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
				NEXT_ENTRY(UNSIGNED), item_param),
		.args = ARGS(ARGS_ENTRY_HTON(struct rte_flow_item_ecpri,
				hdr.type5.msr_id)),
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
		.next = NEXT(action_jump, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_mark, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_queue, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_count, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_count, id)),
		.call = parse_vc_conf,
	},
	[ACTION_COUNT_SHARED] = {
		.name = "shared",
		.help = "shared counter",
		.next = NEXT(action_count, NEXT_ENTRY(BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_count,
					   shared, 1)),
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
		.next = NEXT(action_rss, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_rss, NEXT_ENTRY(HEX)),
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
		.next = NEXT(action_rss, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_vf, NEXT_ENTRY(BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_vf,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_VF_ID] = {
		.name = "id",
		.help = "VF ID",
		.next = NEXT(action_vf, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_vf, id)),
		.call = parse_vc_conf,
	},
	[ACTION_PHY_PORT] = {
		.name = "phy_port",
		.help = "direct packets to physical port index",
		.priv = PRIV_ACTION(PHY_PORT,
				    sizeof(struct rte_flow_action_phy_port)),
		.next = NEXT(action_phy_port),
		.call = parse_vc,
	},
	[ACTION_PHY_PORT_ORIGINAL] = {
		.name = "original",
		.help = "use original port index if possible",
		.next = NEXT(action_phy_port, NEXT_ENTRY(BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_phy_port,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_PHY_PORT_INDEX] = {
		.name = "index",
		.help = "physical port index",
		.next = NEXT(action_phy_port, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_phy_port,
					index)),
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
		.next = NEXT(action_port_id, NEXT_ENTRY(BOOLEAN)),
		.args = ARGS(ARGS_ENTRY_BF(struct rte_flow_action_port_id,
					   original, 1)),
		.call = parse_vc_conf,
	},
	[ACTION_PORT_ID_ID] = {
		.name = "id",
		.help = "DPDK port ID",
		.next = NEXT(action_port_id, NEXT_ENTRY(UNSIGNED)),
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
	[ACTION_METER_ID] = {
		.name = "mtr_id",
		.help = "meter id to use",
		.next = NEXT(action_meter, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_meter, mtr_id)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_SET_MPLS_TTL] = {
		.name = "of_set_mpls_ttl",
		.help = "OpenFlow's OFPAT_SET_MPLS_TTL",
		.priv = PRIV_ACTION
			(OF_SET_MPLS_TTL,
			 sizeof(struct rte_flow_action_of_set_mpls_ttl)),
		.next = NEXT(action_of_set_mpls_ttl),
		.call = parse_vc,
	},
	[ACTION_OF_SET_MPLS_TTL_MPLS_TTL] = {
		.name = "mpls_ttl",
		.help = "MPLS TTL",
		.next = NEXT(action_of_set_mpls_ttl, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_of_set_mpls_ttl,
					mpls_ttl)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_DEC_MPLS_TTL] = {
		.name = "of_dec_mpls_ttl",
		.help = "OpenFlow's OFPAT_DEC_MPLS_TTL",
		.priv = PRIV_ACTION(OF_DEC_MPLS_TTL, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_OF_SET_NW_TTL] = {
		.name = "of_set_nw_ttl",
		.help = "OpenFlow's OFPAT_SET_NW_TTL",
		.priv = PRIV_ACTION
			(OF_SET_NW_TTL,
			 sizeof(struct rte_flow_action_of_set_nw_ttl)),
		.next = NEXT(action_of_set_nw_ttl),
		.call = parse_vc,
	},
	[ACTION_OF_SET_NW_TTL_NW_TTL] = {
		.name = "nw_ttl",
		.help = "IP TTL",
		.next = NEXT(action_of_set_nw_ttl, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_of_set_nw_ttl,
					nw_ttl)),
		.call = parse_vc_conf,
	},
	[ACTION_OF_DEC_NW_TTL] = {
		.name = "of_dec_nw_ttl",
		.help = "OpenFlow's OFPAT_DEC_NW_TTL",
		.priv = PRIV_ACTION(OF_DEC_NW_TTL, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_OF_COPY_TTL_OUT] = {
		.name = "of_copy_ttl_out",
		.help = "OpenFlow's OFPAT_COPY_TTL_OUT",
		.priv = PRIV_ACTION(OF_COPY_TTL_OUT, 0),
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc,
	},
	[ACTION_OF_COPY_TTL_IN] = {
		.name = "of_copy_ttl_in",
		.help = "OpenFlow's OFPAT_COPY_TTL_IN",
		.priv = PRIV_ACTION(OF_COPY_TTL_IN, 0),
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
		.next = NEXT(action_of_push_vlan, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_of_set_vlan_vid, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_of_set_vlan_pcp, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_of_pop_mpls, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_of_push_mpls, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_ipv4_src, NEXT_ENTRY(IPV4_ADDR)),
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
		.next = NEXT(action_set_ipv4_dst, NEXT_ENTRY(IPV4_ADDR)),
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
		.next = NEXT(action_set_ipv6_src, NEXT_ENTRY(IPV6_ADDR)),
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
		.next = NEXT(action_set_ipv6_dst, NEXT_ENTRY(IPV6_ADDR)),
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
		.next = NEXT(action_set_tp_src, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_tp_dst, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_ttl, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_mac_src, NEXT_ENTRY(MAC_ADDR)),
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
		.next = NEXT(action_set_mac_dst, NEXT_ENTRY(MAC_ADDR)),
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
		.next = NEXT(action_inc_tcp_seq, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_dec_tcp_seq, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_inc_tcp_ack, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_dec_tcp_ack, NEXT_ENTRY(UNSIGNED)),
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
		.type = "UNSIGNED",
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
		.next = NEXT(action_set_tag, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY(struct rte_flow_action_set_tag, index)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TAG_DATA] = {
		.name = "data",
		.help = "tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_tag, data)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_TAG_MASK] = {
		.name = "mask",
		.help = "mask for tag value",
		.next = NEXT(action_set_tag, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_meta, NEXT_ENTRY(UNSIGNED)),
		.args = ARGS(ARGS_ENTRY
			     (struct rte_flow_action_set_meta, data)),
		.call = parse_vc_conf,
	},
	[ACTION_SET_META_MASK] = {
		.name = "mask",
		.help = "mask for metadata value",
		.next = NEXT(action_set_meta, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_ipv4_dscp, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_set_ipv6_dscp, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_age, NEXT_ENTRY(UNSIGNED)),
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
		.next = NEXT(action_sample, NEXT_ENTRY(UNSIGNED)),
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
		.type = "UNSIGNED",
		.help = "unsigned integer value",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_vc_action_sample_index,
		.comp = comp_set_sample_index,
	},
	/* Shared action destroy arguments. */
	[SHARED_ACTION_DESTROY_ID] = {
		.name = "action_id",
		.help = "specify a shared action id to destroy",
		.next = NEXT(next_sa_destroy_attr,
			     NEXT_ENTRY(SHARED_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY_PTR(struct buffer,
					    args.sa_destroy.action_id)),
		.call = parse_sa_destroy,
	},
	/* Shared action create arguments. */
	[SHARED_ACTION_CREATE_ID] = {
		.name = "action_id",
		.help = "specify a shared action id to create",
		.next = NEXT(next_sa_create_attr,
			     NEXT_ENTRY(SHARED_ACTION_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.vc.attr.group)),
	},
	[ACTION_SHARED] = {
		.name = "shared",
		.help = "apply shared action by id",
		.priv = PRIV_ACTION(SHARED, 0),
		.next = NEXT(NEXT_ENTRY(SHARED_ACTION_ID2PTR)),
		.args = ARGS(ARGS_ENTRY_ARB(0, sizeof(uint32_t))),
		.call = parse_vc,
	},
	[SHARED_ACTION_ID2PTR] = {
		.name = "{action_id}",
		.type = "SHARED_ACTION_ID",
		.help = "shared action id",
		.next = NEXT(NEXT_ENTRY(ACTION_NEXT)),
		.call = parse_sa_id2ptr,
		.comp = comp_none,
	},
	[SHARED_ACTION_INGRESS] = {
		.name = "ingress",
		.help = "affect rule to ingress",
		.next = NEXT(next_sa_create_attr),
		.call = parse_sa,
	},
	[SHARED_ACTION_EGRESS] = {
		.name = "egress",
		.help = "affect rule to egress",
		.next = NEXT(next_sa_create_attr),
		.call = parse_sa,
	},
	[SHARED_ACTION_TRANSFER] = {
		.name = "transfer",
		.help = "affect rule to transfer",
		.next = NEXT(next_sa_create_attr),
		.call = parse_sa,
	},
	[SHARED_ACTION_SPEC] = {
		.name = "action",
		.help = "specify action to share",
		.next = NEXT(next_action),
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

/** Parse tokens for shared action commands. */
static int
parse_sa(struct context *ctx, const struct token *token,
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
		if (ctx->curr != SHARED_ACTION)
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
	case SHARED_ACTION_CREATE:
	case SHARED_ACTION_UPDATE:
		out->args.vc.actions =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		out->args.vc.attr.group = UINT32_MAX;
		/* fallthrough */
	case SHARED_ACTION_QUERY:
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		return len;
	case SHARED_ACTION_EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case SHARED_ACTION_INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case SHARED_ACTION_TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	default:
		return -1;
	}
}


/** Parse tokens for shared action destroy command. */
static int
parse_sa_destroy(struct context *ctx, const struct token *token,
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
	if (!out->command || out->command == SHARED_ACTION) {
		if (ctx->curr != SHARED_ACTION_DESTROY)
			return -1;
		if (sizeof(*out) > size)
			return -1;
		out->command = ctx->curr;
		ctx->objdata = 0;
		ctx->object = out;
		ctx->objmask = NULL;
		out->args.sa_destroy.action_id =
			(void *)RTE_ALIGN_CEIL((uintptr_t)(out + 1),
					       sizeof(double));
		return len;
	}
	action_id = out->args.sa_destroy.action_id
		    + out->args.sa_destroy.action_id_n++;
	if ((uint8_t *)action_id > (uint8_t *)out + size)
		return -1;
	ctx->objdata = 0;
	ctx->object = action_id;
	ctx->objmask = NULL;
	return len;
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
		if (ctx->curr != VALIDATE && ctx->curr != CREATE)
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
	case TUNNEL_SET:
	case TUNNEL_MATCH:
		ctx->object = &out->args.vc.tunnel_ops;
		break;
	}
	ctx->objmask = NULL;
	switch (ctx->curr) {
	case GROUP:
	case PRIORITY:
		return len;
	case TUNNEL_SET:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.actions = 1;
		return len;
	case TUNNEL_MATCH:
		out->args.vc.tunnel_ops.enabled = 1;
		out->args.vc.tunnel_ops.items = 1;
		return len;
	case INGRESS:
		out->args.vc.attr.ingress = 1;
		return len;
	case EGRESS:
		out->args.vc.attr.egress = 1;
		return len;
	case TRANSFER:
		out->args.vc.attr.transfer = 1;
		return len;
	case PATTERN:
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
		static const enum index prefix[] = NEXT_ENTRY(PREFIX);

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
	/* Set up default configuration. */
	action_vxlan_encap_data = ctx->object;
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
		.item_eth.type = 0,
		.item_vlan = {
			.tci = vxlan_encap_conf.vlan_tci,
			.inner_type = 0,
		},
		.item_ipv4.hdr = {
			.src_addr = vxlan_encap_conf.ipv4_src,
			.dst_addr = vxlan_encap_conf.ipv4_dst,
		},
		.item_udp.hdr = {
			.src_port = vxlan_encap_conf.udp_src,
			.dst_port = vxlan_encap_conf.udp_dst,
		},
		.item_vxlan.flags = 0,
	};
	memcpy(action_vxlan_encap_data->item_eth.dst.addr_bytes,
	       vxlan_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(action_vxlan_encap_data->item_eth.src.addr_bytes,
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
	memcpy(action_vxlan_encap_data->item_vxlan.vni, vxlan_encap_conf.vni,
	       RTE_DIM(vxlan_encap_conf.vni));
	action->conf = &action_vxlan_encap_data->conf;
	return ret;
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
	/* Set up default configuration. */
	action_nvgre_encap_data = ctx->object;
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
		.item_eth.type = 0,
		.item_vlan = {
			.tci = nvgre_encap_conf.vlan_tci,
			.inner_type = 0,
		},
		.item_ipv4.hdr = {
		       .src_addr = nvgre_encap_conf.ipv4_src,
		       .dst_addr = nvgre_encap_conf.ipv4_dst,
		},
		.item_nvgre.c_k_s_rsvd0_ver = RTE_BE16(0x2000),
		.item_nvgre.protocol = RTE_BE16(RTE_ETHER_TYPE_TEB),
		.item_nvgre.flow_id = 0,
	};
	memcpy(action_nvgre_encap_data->item_eth.dst.addr_bytes,
	       nvgre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(action_nvgre_encap_data->item_eth.src.addr_bytes,
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {
		.tci = mplsoudp_encap_conf.vlan_tci,
		.inner_type = 0,
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (l2_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.dst.addr_bytes,
	       l2_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       l2_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (l2_encap_conf.select_vlan) {
		if (l2_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {
		.tci = mplsoudp_encap_conf.vlan_tci,
		.inner_type = 0,
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (l2_decap_conf.select_vlan) {
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {
		.tci = mplsogre_encap_conf.vlan_tci,
		.inner_type = 0,
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.dst.addr_bytes,
	       mplsogre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsogre_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {.tci = 0};
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.dst.addr_bytes,
	       mplsogre_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsogre_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {
		.tci = mplsoudp_encap_conf.vlan_tci,
		.inner_type = 0,
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.dst.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsoudp_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	struct rte_flow_item_eth eth = { .type = 0, };
	struct rte_flow_item_vlan vlan = {.tci = 0};
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
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	else
		eth.type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	memcpy(eth.dst.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, RTE_ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsoudp_encap_conf.eth_src, RTE_ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
		else
			vlan.inner_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
		memcpy(header, &vlan, sizeof(vlan));
		header += sizeof(vlan);
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
	}
	return len;
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
	if (!out->command) {
		if (ctx->curr != AGED)
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
	char *c = NULL;
	uint32_t i, len;
	char tmp[3];

	/* Check input parameters */
	if ((src == NULL) ||
		(dst == NULL) ||
		(size == NULL) ||
		(*size == 0))
		return -1;

	/* Convert chars to bytes */
	for (i = 0, len = 0; i < *size; i += 2) {
		snprintf(tmp, 3, "%s", src + i);
		dst[len++] = strtoul(tmp, &c, 16);
		if (*c != 0) {
			len--;
			dst[len] = 0;
			*size = len;
			return -1;
		}
	}
	dst[len] = 0;
	*size = len;

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
		return -1;
	ret = parse_hex_string(str, hex_tmp, &hexlen);
	if (ret < 0)
		goto error;
	/* Let parse_int() fill length information first. */
	ret = snprintf(tmp, sizeof(tmp), "%u", hexlen);
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

static int
parse_sa_id2ptr(struct context *ctx, const struct token *token,
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
	/* set shared action */
	if (action) {
		action->conf = port_shared_action_get_by_id(ctx->port, id);
		ret = (action->conf) ? ret : -1;
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
	case SHARED_ACTION_CREATE:
		port_shared_action_create(
				in->port, in->args.vc.attr.group,
				&((const struct rte_flow_shared_action_conf) {
					.ingress = in->args.vc.attr.ingress,
					.egress = in->args.vc.attr.egress,
					.transfer = in->args.vc.attr.transfer,
				}),
				in->args.vc.actions);
		break;
	case SHARED_ACTION_DESTROY:
		port_shared_action_destroy(in->port,
					   in->args.sa_destroy.action_id_n,
					   in->args.sa_destroy.action_id);
		break;
	case SHARED_ACTION_UPDATE:
		port_shared_action_update(in->port, in->args.vc.attr.group,
					  in->args.vc.actions);
		break;
	case SHARED_ACTION_QUERY:
		port_shared_action_query(in->port, in->args.sa.action_id);
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
	case DUMP:
		port_flow_dump(in->port, in->args.dump.file);
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
		ipv4->version_ihl = 0x45;
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

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ANY:
		mask = &rte_flow_item_any_mask;
		break;
	case RTE_FLOW_ITEM_TYPE_VF:
		mask = &rte_flow_item_vf_mask;
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
		default:
			printf("Error - Not supported action\n");
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
		item = in->args.vc.pattern + i;
		if (item->spec == NULL)
			item->spec = flow_item_default_mask(item);
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
			size = sizeof(struct rte_gtp_hdr);
			break;
		case RTE_FLOW_ITEM_TYPE_PFCP:
			size = sizeof(struct rte_flow_item_pfcp);
			break;
		default:
			printf("Error - Not supported item\n");
			*total_size = 0;
			memset(data, 0x00, ACTION_RAW_ENCAP_MAX_DATA);
			return;
		}
		*total_size += size;
		rte_memcpy(data_tail - (*total_size), item->spec, size);
		/* update some fields which cannot be set by cmdline */
		update_fields((data_tail - (*total_size)), item,
			      upper_layer);
		upper_layer = proto;
	}
	if (verbose_level & 0x1)
		printf("total data size is %zu\n", (*total_size));
	RTE_ASSERT((*total_size) <= ACTION_RAW_ENCAP_MAX_DATA);
	memmove(data, (data_tail - (*total_size)), *total_size);
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
		printf("index should be 0-%u\n", RAW_ENCAP_CONFS_MAX_NUM - 1);
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

cmdline_parse_token_string_t cmd_show_set_raw_cmd_show =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_show, "show");
cmdline_parse_token_string_t cmd_show_set_raw_cmd_what =
	TOKEN_STRING_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_what, "raw_encap#raw_decap");
cmdline_parse_token_num_t cmd_show_set_raw_cmd_index =
	TOKEN_NUM_INITIALIZER(struct cmd_show_set_raw_result,
			cmd_index, RTE_UINT16);
cmdline_parse_token_string_t cmd_show_set_raw_cmd_all =
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
