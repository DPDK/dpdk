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
#include <rte_flow.h>

#include "testpmd.h"

/** Parser token indices. */
enum index {
	/* Special tokens. */
	ZERO = 0,
	END,

	/* Common tokens. */
	INTEGER,
	UNSIGNED,
	PREFIX,
	BOOLEAN,
	STRING,
	HEX,
	MAC_ADDR,
	IPV4_ADDR,
	IPV6_ADDR,
	RULE_ID,
	PORT_ID,
	GROUP_ID,
	PRIORITY_LEVEL,

	/* Top-level command. */
	FLOW,

	/* Sub-level commands. */
	VALIDATE,
	CREATE,
	DESTROY,
	FLUSH,
	QUERY,
	LIST,
	ISOLATE,

	/* Destroy arguments. */
	DESTROY_RULE,

	/* Query arguments. */
	QUERY_ACTION,

	/* List arguments. */
	LIST_GROUP,

	/* Validate/create arguments. */
	GROUP,
	PRIORITY,
	INGRESS,
	EGRESS,
	TRANSFER,

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
	ITEM_VLAN,
	ITEM_VLAN_TCI,
	ITEM_VLAN_PCP,
	ITEM_VLAN_DEI,
	ITEM_VLAN_VID,
	ITEM_VLAN_INNER_TYPE,
	ITEM_IPV4,
	ITEM_IPV4_TOS,
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
	ITEM_ICMP,
	ITEM_ICMP_TYPE,
	ITEM_ICMP_CODE,
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
	ITEM_GRE,
	ITEM_GRE_PROTO,
	ITEM_FUZZY,
	ITEM_FUZZY_THRESH,
	ITEM_GTP,
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
};

/** Maximum size for pattern in struct rte_flow_item_raw. */
#define ITEM_RAW_PATTERN_SIZE 40

/** Storage size for struct rte_flow_item_raw including pattern. */
#define ITEM_RAW_SIZE \
	(sizeof(struct rte_flow_item_raw) + ITEM_RAW_PATTERN_SIZE)

/** Maximum number of queue indices in struct rte_flow_action_rss. */
#define ACTION_RSS_QUEUE_NUM 32

/** Storage for struct rte_flow_action_rss including external data. */
struct action_rss_data {
	struct rte_flow_action_rss conf;
	uint8_t key[RSS_HASH_KEY_LENGTH];
	uint16_t queue[ACTION_RSS_QUEUE_NUM];
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

/** Maximum data size in struct rte_flow_action_raw_encap. */
#define ACTION_RAW_ENCAP_MAX_DATA 128

/** Storage for struct rte_flow_action_raw_encap including external data. */
struct action_raw_encap_data {
	struct rte_flow_action_raw_encap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
	uint8_t preserve[ACTION_RAW_ENCAP_MAX_DATA];
};

/** Storage for struct rte_flow_action_raw_decap including external data. */
struct action_raw_decap_data {
	struct rte_flow_action_raw_decap conf;
	uint8_t data[ACTION_RAW_ENCAP_MAX_DATA];
};

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

/** Parser output buffer layout expected by cmd_flow_parsed(). */
struct buffer {
	enum index command; /**< Flow command. */
	portid_t port; /**< Affected port ID. */
	union {
		struct {
			struct rte_flow_attr attr;
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

static const enum index next_vc_attr[] = {
	GROUP,
	PRIORITY,
	INGRESS,
	EGRESS,
	TRANSFER,
	PATTERN,
	ZERO,
};

static const enum index next_destroy_attr[] = {
	DESTROY_RULE,
	END,
	ZERO,
};

static const enum index next_list_attr[] = {
	LIST_GROUP,
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
	ITEM_ICMP6,
	ITEM_ICMP6_ND_NS,
	ITEM_ICMP6_ND_NA,
	ITEM_ICMP6_ND_OPT,
	ITEM_ICMP6_ND_OPT_SLA_ETH,
	ITEM_ICMP6_ND_OPT_TLA_ETH,
	ITEM_META,
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
	ITEM_NEXT,
	ZERO,
};

static const enum index item_vlan[] = {
	ITEM_VLAN_TCI,
	ITEM_VLAN_PCP,
	ITEM_VLAN_DEI,
	ITEM_VLAN_VID,
	ITEM_VLAN_INNER_TYPE,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_ipv4[] = {
	ITEM_IPV4_TOS,
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
	ITEM_NEXT,
	ZERO,
};

static const enum index item_icmp[] = {
	ITEM_ICMP_TYPE,
	ITEM_ICMP_CODE,
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
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gre[] = {
	ITEM_GRE_PROTO,
	ITEM_NEXT,
	ZERO,
};

static const enum index item_gtp[] = {
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
static int parse_destroy(struct context *, const struct token *,
			 const char *, unsigned int,
			 void *, unsigned int);
static int parse_flush(struct context *, const struct token *,
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
static int parse_isolate(struct context *, const struct token *,
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
	/* Top-level command. */
	[FLOW] = {
		.name = "flow",
		.type = "{command} {port_id} [{arg} [...]]",
		.help = "manage ingress/egress flow rules",
		.next = NEXT(NEXT_ENTRY
			     (VALIDATE,
			      CREATE,
			      DESTROY,
			      FLUSH,
			      LIST,
			      QUERY,
			      ISOLATE)),
		.call = parse_init,
	},
	/* Sub-level commands. */
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
	[ISOLATE] = {
		.name = "isolate",
		.help = "restrict ingress traffic to the defined flow rules",
		.next = NEXT(NEXT_ENTRY(BOOLEAN),
			     NEXT_ENTRY(PORT_ID)),
		.args = ARGS(ARGS_ENTRY(struct buffer, args.isolate.set),
			     ARGS_ENTRY(struct buffer, port)),
		.call = parse_isolate,
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
		.args = ARGS(ARGS_ENTRY_MASK_HTON(struct rte_flow_item_meta,
						  data, "\xff\xff\xff\xff")),
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
					ACTION_RSS_FUNC_SIMPLE_XOR)),
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
		.args = ARGS(ARGS_ENTRY_ARB(0, 0),
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
	ctx->object = &out->args.vc.attr;
	ctx->objmask = NULL;
	switch (ctx->curr) {
	case GROUP:
	case PRIORITY:
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
			.key_len = sizeof(action_rss_data->key),
			.queue_num = RTE_MIN(nb_rxq, ACTION_RSS_QUEUE_NUM),
			.key = action_rss_data->key,
			.queue = action_rss_data->queue,
		},
		.key = "testpmd's default RSS hash key, "
			"override it for better balancing",
		.queue = { 0 },
	};
	for (i = 0; i < action_rss_data->conf.queue_num; ++i)
		action_rss_data->queue[i] = i;
	if (!port_id_is_invalid(ctx->port, DISABLED_WARN) &&
	    ctx->port != (portid_t)RTE_PORT_ALL) {
		struct rte_eth_dev_info info;

		rte_eth_dev_info_get(ctx->port, &info);
		action_rss_data->conf.key_len =
			RTE_MIN(sizeof(action_rss_data->key),
				info.hash_key_size);
	}
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
	if (push_args(ctx,
		      ARGS_ENTRY_ARB(offsetof(struct action_rss_data, queue) +
				     i * sizeof(action_rss_data->queue[i]),
				     sizeof(action_rss_data->queue[i]))))
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
	       vxlan_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(action_vxlan_encap_data->item_eth.src.addr_bytes,
	       vxlan_encap_conf.eth_src, ETHER_ADDR_LEN);
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
				RTE_BE32(0xfful << IPV6_HDR_TC_SHIFT);
			ipv6_mask_tos.hdr.hop_limits = 0xff;
			action_vxlan_encap_data->item_ipv6.hdr.vtc_flow |=
				rte_cpu_to_be_32
					((uint32_t)vxlan_encap_conf.ip_tos <<
					 IPV6_HDR_TC_SHIFT);
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
		.item_nvgre.flow_id = 0,
	};
	memcpy(action_nvgre_encap_data->item_eth.dst.addr_bytes,
	       nvgre_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(action_nvgre_encap_data->item_eth.src.addr_bytes,
	       nvgre_encap_conf.eth_src, ETHER_ADDR_LEN);
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else if (l2_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	else
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	memcpy(eth.dst.addr_bytes,
	       l2_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       l2_encap_conf.eth_src, ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (l2_encap_conf.select_vlan) {
		if (l2_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		else
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	else
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	memcpy(eth.dst.addr_bytes,
	       mplsogre_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsogre_encap_conf.eth_src, ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		else
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else if (mplsogre_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	else
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	memcpy(eth.dst.addr_bytes,
	       mplsogre_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsogre_encap_conf.eth_src, ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsogre_encap_conf.select_vlan) {
		if (mplsogre_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		else
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
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
		},
	};
	struct rte_flow_item_ipv6 ipv6 = {
		.hdr =  {
			.proto = IPPROTO_UDP,
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	else
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	memcpy(eth.dst.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsoudp_encap_conf.eth_src, ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		else
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
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
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_VLAN);
	else if (mplsoudp_encap_conf.select_ipv4)
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
	else
		eth.type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
	memcpy(eth.dst.addr_bytes,
	       mplsoudp_encap_conf.eth_dst, ETHER_ADDR_LEN);
	memcpy(eth.src.addr_bytes,
	       mplsoudp_encap_conf.eth_src, ETHER_ADDR_LEN);
	memcpy(header, &eth, sizeof(eth));
	header += sizeof(eth);
	if (mplsoudp_encap_conf.select_vlan) {
		if (mplsoudp_encap_conf.select_ipv4)
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		else
			vlan.inner_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
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
	struct ether_addr tmp;
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

/** Internal context. */
static struct context cmd_flow_context;

/** Global parser instance (cmdline API). */
cmdline_parse_inst_t cmd_flow;

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
	case VALIDATE:
		port_flow_validate(in->port, &in->args.vc.attr,
				   in->args.vc.pattern, in->args.vc.actions);
		break;
	case CREATE:
		port_flow_create(in->port, &in->args.vc.attr,
				 in->args.vc.pattern, in->args.vc.actions);
		break;
	case DESTROY:
		port_flow_destroy(in->port, in->args.destroy.rule_n,
				  in->args.destroy.rule);
		break;
	case FLUSH:
		port_flow_flush(in->port);
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
