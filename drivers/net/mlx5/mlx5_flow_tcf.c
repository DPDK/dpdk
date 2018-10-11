/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 6WIND S.A.
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <assert.h>
#include <errno.h>
#include <libmnl/libmnl.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/pkt_cls.h>
#include <linux/pkt_sched.h>
#include <linux/rtnetlink.h>
#include <linux/tc_act/tc_gact.h>
#include <linux/tc_act/tc_mirred.h>
#include <netinet/in.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

#include <rte_byteorder.h>
#include <rte_errno.h>
#include <rte_ether.h>
#include <rte_flow.h>
#include <rte_malloc.h>

#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_autoconf.h"

#ifdef HAVE_TC_ACT_VLAN

#include <linux/tc_act/tc_vlan.h>

#else /* HAVE_TC_ACT_VLAN */

#define TCA_VLAN_ACT_POP 1
#define TCA_VLAN_ACT_PUSH 2
#define TCA_VLAN_ACT_MODIFY 3
#define TCA_VLAN_PARMS 2
#define TCA_VLAN_PUSH_VLAN_ID 3
#define TCA_VLAN_PUSH_VLAN_PROTOCOL 4
#define TCA_VLAN_PAD 5
#define TCA_VLAN_PUSH_VLAN_PRIORITY 6

struct tc_vlan {
	tc_gen;
	int v_action;
};

#endif /* HAVE_TC_ACT_VLAN */

#ifdef HAVE_TC_ACT_PEDIT

#include <linux/tc_act/tc_pedit.h>

#else /* HAVE_TC_ACT_VLAN */

enum {
	TCA_PEDIT_UNSPEC,
	TCA_PEDIT_TM,
	TCA_PEDIT_PARMS,
	TCA_PEDIT_PAD,
	TCA_PEDIT_PARMS_EX,
	TCA_PEDIT_KEYS_EX,
	TCA_PEDIT_KEY_EX,
	__TCA_PEDIT_MAX
};

enum {
	TCA_PEDIT_KEY_EX_HTYPE = 1,
	TCA_PEDIT_KEY_EX_CMD = 2,
	__TCA_PEDIT_KEY_EX_MAX
};

enum pedit_header_type {
	TCA_PEDIT_KEY_EX_HDR_TYPE_NETWORK = 0,
	TCA_PEDIT_KEY_EX_HDR_TYPE_ETH = 1,
	TCA_PEDIT_KEY_EX_HDR_TYPE_IP4 = 2,
	TCA_PEDIT_KEY_EX_HDR_TYPE_IP6 = 3,
	TCA_PEDIT_KEY_EX_HDR_TYPE_TCP = 4,
	TCA_PEDIT_KEY_EX_HDR_TYPE_UDP = 5,
	__PEDIT_HDR_TYPE_MAX,
};

enum pedit_cmd {
	TCA_PEDIT_KEY_EX_CMD_SET = 0,
	TCA_PEDIT_KEY_EX_CMD_ADD = 1,
	__PEDIT_CMD_MAX,
};

struct tc_pedit_key {
	__u32           mask;  /* AND */
	__u32           val;   /*XOR */
	__u32           off;  /*offset */
	__u32           at;
	__u32           offmask;
	__u32           shift;
};

struct tc_pedit_sel {
	tc_gen;
	unsigned char           nkeys;
	unsigned char           flags;
	struct tc_pedit_key     keys[0];
};

#endif /* HAVE_TC_ACT_VLAN */

/* Normally found in linux/netlink.h. */
#ifndef NETLINK_CAP_ACK
#define NETLINK_CAP_ACK 10
#endif

/* Normally found in linux/pkt_sched.h. */
#ifndef TC_H_MIN_INGRESS
#define TC_H_MIN_INGRESS 0xfff2u
#endif

/* Normally found in linux/pkt_cls.h. */
#ifndef TCA_CLS_FLAGS_SKIP_SW
#define TCA_CLS_FLAGS_SKIP_SW (1 << 1)
#endif
#ifndef HAVE_TCA_FLOWER_ACT
#define TCA_FLOWER_ACT 3
#endif
#ifndef HAVE_TCA_FLOWER_FLAGS
#define TCA_FLOWER_FLAGS 22
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_TYPE
#define TCA_FLOWER_KEY_ETH_TYPE 8
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_DST
#define TCA_FLOWER_KEY_ETH_DST 4
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_DST_MASK
#define TCA_FLOWER_KEY_ETH_DST_MASK 5
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_SRC
#define TCA_FLOWER_KEY_ETH_SRC 6
#endif
#ifndef HAVE_TCA_FLOWER_KEY_ETH_SRC_MASK
#define TCA_FLOWER_KEY_ETH_SRC_MASK 7
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IP_PROTO
#define TCA_FLOWER_KEY_IP_PROTO 9
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_SRC
#define TCA_FLOWER_KEY_IPV4_SRC 10
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_SRC_MASK
#define TCA_FLOWER_KEY_IPV4_SRC_MASK 11
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_DST
#define TCA_FLOWER_KEY_IPV4_DST 12
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV4_DST_MASK
#define TCA_FLOWER_KEY_IPV4_DST_MASK 13
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_SRC
#define TCA_FLOWER_KEY_IPV6_SRC 14
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_SRC_MASK
#define TCA_FLOWER_KEY_IPV6_SRC_MASK 15
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_DST
#define TCA_FLOWER_KEY_IPV6_DST 16
#endif
#ifndef HAVE_TCA_FLOWER_KEY_IPV6_DST_MASK
#define TCA_FLOWER_KEY_IPV6_DST_MASK 17
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_SRC
#define TCA_FLOWER_KEY_TCP_SRC 18
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_SRC_MASK
#define TCA_FLOWER_KEY_TCP_SRC_MASK 35
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_DST
#define TCA_FLOWER_KEY_TCP_DST 19
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_DST_MASK
#define TCA_FLOWER_KEY_TCP_DST_MASK 36
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_SRC
#define TCA_FLOWER_KEY_UDP_SRC 20
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_SRC_MASK
#define TCA_FLOWER_KEY_UDP_SRC_MASK 37
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_DST
#define TCA_FLOWER_KEY_UDP_DST 21
#endif
#ifndef HAVE_TCA_FLOWER_KEY_UDP_DST_MASK
#define TCA_FLOWER_KEY_UDP_DST_MASK 38
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_ID
#define TCA_FLOWER_KEY_VLAN_ID 23
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_PRIO
#define TCA_FLOWER_KEY_VLAN_PRIO 24
#endif
#ifndef HAVE_TCA_FLOWER_KEY_VLAN_ETH_TYPE
#define TCA_FLOWER_KEY_VLAN_ETH_TYPE 25
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_FLAGS
#define TCA_FLOWER_KEY_TCP_FLAGS 71
#endif
#ifndef HAVE_TCA_FLOWER_KEY_TCP_FLAGS_MASK
#define TCA_FLOWER_KEY_TCP_FLAGS_MASK 72
#endif

#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN 16
#endif

#ifndef IPV4_ADDR_LEN
#define IPV4_ADDR_LEN 4
#endif

#ifndef TP_PORT_LEN
#define TP_PORT_LEN 2 /* Transport Port (UDP/TCP) Length */
#endif

/** Empty masks for known item types. */
static const union {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
} flow_tcf_mask_empty;

/** Supported masks for known item types. */
static const struct {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
} flow_tcf_mask_supported = {
	.port_id = {
		.id = 0xffffffff,
	},
	.eth = {
		.type = RTE_BE16(0xffff),
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	},
	.vlan = {
		/* PCP and VID only, no DEI. */
		.tci = RTE_BE16(0xefff),
		.inner_type = RTE_BE16(0xffff),
	},
	.ipv4.hdr = {
		.next_proto_id = 0xff,
		.src_addr = RTE_BE32(0xffffffff),
		.dst_addr = RTE_BE32(0xffffffff),
	},
	.ipv6.hdr = {
		.proto = 0xff,
		.src_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
		.dst_addr =
			"\xff\xff\xff\xff\xff\xff\xff\xff"
			"\xff\xff\xff\xff\xff\xff\xff\xff",
	},
	.tcp.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
		.tcp_flags = 0xff,
	},
	.udp.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};

#define SZ_NLATTR_HDR MNL_ALIGN(sizeof(struct nlattr))
#define SZ_NLATTR_NEST SZ_NLATTR_HDR
#define SZ_NLATTR_DATA_OF(len) MNL_ALIGN(SZ_NLATTR_HDR + (len))
#define SZ_NLATTR_TYPE_OF(typ) SZ_NLATTR_DATA_OF(sizeof(typ))
#define SZ_NLATTR_STRZ_OF(str) SZ_NLATTR_DATA_OF(strlen(str) + 1)

#define PTOI_TABLE_SZ_MAX(dev) (mlx5_dev_to_port_id((dev)->device, NULL, 0) + 2)

/** DPDK port to network interface index (ifindex) conversion. */
struct flow_tcf_ptoi {
	uint16_t port_id; /**< DPDK port ID. */
	unsigned int ifindex; /**< Network interface index. */
};

#define MLX5_TCF_FATE_ACTIONS (MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_PORT_ID)
#define MLX5_TCF_VLAN_ACTIONS \
	(MLX5_FLOW_ACTION_OF_POP_VLAN | MLX5_FLOW_ACTION_OF_PUSH_VLAN | \
	 MLX5_FLOW_ACTION_OF_SET_VLAN_VID | MLX5_FLOW_ACTION_OF_SET_VLAN_PCP)

#define MLX5_TCF_PEDIT_ACTIONS \
	(MLX5_FLOW_ACTION_SET_IPV4_SRC | MLX5_FLOW_ACTION_SET_IPV4_DST | \
	 MLX5_FLOW_ACTION_SET_IPV6_SRC | MLX5_FLOW_ACTION_SET_IPV6_DST | \
	 MLX5_FLOW_ACTION_SET_TP_SRC | MLX5_FLOW_ACTION_SET_TP_DST)

#define MLX5_TCF_CONFIG_ACTIONS \
	(MLX5_FLOW_ACTION_PORT_ID | MLX5_FLOW_ACTION_OF_PUSH_VLAN | \
	 MLX5_FLOW_ACTION_OF_SET_VLAN_VID | MLX5_FLOW_ACTION_OF_SET_VLAN_PCP | \
	 MLX5_TCF_PEDIT_ACTIONS)

#define MAX_PEDIT_KEYS 128
#define SZ_PEDIT_KEY_VAL 4

#define NUM_OF_PEDIT_KEYS(sz) \
	(((sz) / SZ_PEDIT_KEY_VAL) + (((sz) % SZ_PEDIT_KEY_VAL) ? 1 : 0))

struct pedit_key_ex {
	enum pedit_header_type htype;
	enum pedit_cmd cmd;
};

struct pedit_parser {
	struct tc_pedit_sel sel;
	struct tc_pedit_key keys[MAX_PEDIT_KEYS];
	struct pedit_key_ex keys_ex[MAX_PEDIT_KEYS];
};


/**
 * Set pedit key of transport (TCP/UDP) port value
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 * @param[in] item_flags
 *   flags of all items presented
 */
static void
flow_tcf_pedit_key_set_tp_port(const struct rte_flow_action *actions,
				struct pedit_parser *p_parser,
				uint64_t item_flags)
{
	int idx = p_parser->sel.nkeys;

	if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP)
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_UDP;
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP)
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_TCP;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	/* offset of src/dst port is same for TCP and UDP */
	p_parser->keys[idx].off =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_TP_SRC ?
		offsetof(struct tcp_hdr, src_port) :
		offsetof(struct tcp_hdr, dst_port);
	p_parser->keys[idx].mask = 0xFFFF0000;
	p_parser->keys[idx].val =
		(__u32)((const struct rte_flow_action_set_tp *)
				actions->conf)->port;
	p_parser->sel.nkeys = (++idx);
}

/**
 * Set pedit key of ipv6 address
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 */
static void
flow_tcf_pedit_key_set_ipv6_addr(const struct rte_flow_action *actions,
				 struct pedit_parser *p_parser)
{
	int idx = p_parser->sel.nkeys;
	int keys = NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
	int off_base =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
		offsetof(struct ipv6_hdr, src_addr) :
		offsetof(struct ipv6_hdr, dst_addr);
	const struct rte_flow_action_set_ipv6 *conf =
		(const struct rte_flow_action_set_ipv6 *)actions->conf;

	for (int i = 0; i < keys; i++, idx++) {
		p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP6;
		p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
		p_parser->keys[idx].off = off_base + i * SZ_PEDIT_KEY_VAL;
		p_parser->keys[idx].mask = ~UINT32_MAX;
		memcpy(&p_parser->keys[idx].val,
			conf->ipv6_addr + i *  SZ_PEDIT_KEY_VAL,
			SZ_PEDIT_KEY_VAL);
	}
	p_parser->sel.nkeys += keys;
}

/**
 * Set pedit key of ipv4 address
 *
 * @param[in] actions
 *   pointer to action specification
 * @param[in,out] p_parser
 *   pointer to pedit_parser
 */
static void
flow_tcf_pedit_key_set_ipv4_addr(const struct rte_flow_action *actions,
				 struct pedit_parser *p_parser)
{
	int idx = p_parser->sel.nkeys;

	p_parser->keys_ex[idx].htype = TCA_PEDIT_KEY_EX_HDR_TYPE_IP4;
	p_parser->keys_ex[idx].cmd = TCA_PEDIT_KEY_EX_CMD_SET;
	p_parser->keys[idx].off =
		actions->type == RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC ?
		offsetof(struct ipv4_hdr, src_addr) :
		offsetof(struct ipv4_hdr, dst_addr);
	p_parser->keys[idx].mask = ~UINT32_MAX;
	p_parser->keys[idx].val =
		((const struct rte_flow_action_set_ipv4 *)
		 actions->conf)->ipv4_addr;
	p_parser->sel.nkeys = (++idx);
}

/**
 * Create the pedit's na attribute in netlink message
 * on pre-allocate message buffer
 *
 * @param[in,out] nl
 *   pointer to pre-allocated netlink message buffer
 * @param[in,out] actions
 *   pointer to pointer of actions specification.
 * @param[in,out] action_flags
 *   pointer to actions flags
 * @param[in] item_flags
 *   flags of all item presented
 */
static void
flow_tcf_create_pedit_mnl_msg(struct nlmsghdr *nl,
			      const struct rte_flow_action **actions,
			      uint64_t item_flags)
{
	struct pedit_parser p_parser;
	struct nlattr *na_act_options;
	struct nlattr *na_pedit_keys;

	memset(&p_parser, 0, sizeof(p_parser));
	mnl_attr_put_strz(nl, TCA_ACT_KIND, "pedit");
	na_act_options = mnl_attr_nest_start(nl, TCA_ACT_OPTIONS);
	/* all modify header actions should be in one tc-pedit action */
	for (; (*actions)->type != RTE_FLOW_ACTION_TYPE_END; (*actions)++) {
		switch ((*actions)->type) {
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			flow_tcf_pedit_key_set_ipv4_addr(*actions, &p_parser);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			flow_tcf_pedit_key_set_ipv6_addr(*actions, &p_parser);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			flow_tcf_pedit_key_set_tp_port(*actions,
							&p_parser, item_flags);
			break;
		default:
			goto pedit_mnl_msg_done;
		}
	}
pedit_mnl_msg_done:
	p_parser.sel.action = TC_ACT_PIPE;
	mnl_attr_put(nl, TCA_PEDIT_PARMS_EX,
		     sizeof(p_parser.sel) +
		     p_parser.sel.nkeys * sizeof(struct tc_pedit_key),
		     &p_parser);
	na_pedit_keys =
		mnl_attr_nest_start(nl, TCA_PEDIT_KEYS_EX | NLA_F_NESTED);
	for (int i = 0; i < p_parser.sel.nkeys; i++) {
		struct nlattr *na_pedit_key =
			mnl_attr_nest_start(nl,
					    TCA_PEDIT_KEY_EX | NLA_F_NESTED);
		mnl_attr_put_u16(nl, TCA_PEDIT_KEY_EX_HTYPE,
				 p_parser.keys_ex[i].htype);
		mnl_attr_put_u16(nl, TCA_PEDIT_KEY_EX_CMD,
				 p_parser.keys_ex[i].cmd);
		mnl_attr_nest_end(nl, na_pedit_key);
	}
	mnl_attr_nest_end(nl, na_pedit_keys);
	mnl_attr_nest_end(nl, na_act_options);
	(*actions)--;
}

/**
 * Calculate max memory size of one TC-pedit actions.
 * One TC-pedit action can contain set of keys each defining
 * a rewrite element (rte_flow action)
 *
 * @param[in,out] actions
 *   actions specification.
 * @param[in,out] action_flags
 *   actions flags
 * @param[in,out] size
 *   accumulated size
 * @return
 *   Max memory size of one TC-pedit action
 */
static int
flow_tcf_get_pedit_actions_size(const struct rte_flow_action **actions,
				uint64_t *action_flags)
{
	int pedit_size = 0;
	int keys = 0;
	uint64_t flags = 0;

	pedit_size += SZ_NLATTR_NEST + /* na_act_index. */
		      SZ_NLATTR_STRZ_OF("pedit") +
		      SZ_NLATTR_NEST; /* TCA_ACT_OPTIONS. */
	for (; (*actions)->type != RTE_FLOW_ACTION_TYPE_END; (*actions)++) {
		switch ((*actions)->type) {
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			keys += NUM_OF_PEDIT_KEYS(IPV4_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV4_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			keys += NUM_OF_PEDIT_KEYS(IPV4_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			keys += NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV6_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			keys += NUM_OF_PEDIT_KEYS(IPV6_ADDR_LEN);
			flags |= MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			/* TCP is as same as UDP */
			keys += NUM_OF_PEDIT_KEYS(TP_PORT_LEN);
			flags |= MLX5_FLOW_ACTION_SET_TP_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			/* TCP is as same as UDP */
			keys += NUM_OF_PEDIT_KEYS(TP_PORT_LEN);
			flags |= MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		default:
			goto get_pedit_action_size_done;
		}
	}
get_pedit_action_size_done:
	/* TCA_PEDIT_PARAMS_EX */
	pedit_size +=
		SZ_NLATTR_DATA_OF(sizeof(struct tc_pedit_sel) +
				  keys * sizeof(struct tc_pedit_key));
	pedit_size += SZ_NLATTR_NEST; /* TCA_PEDIT_KEYS */
	pedit_size += keys *
		      /* TCA_PEDIT_KEY_EX + HTYPE + CMD */
		      (SZ_NLATTR_NEST + SZ_NLATTR_DATA_OF(2) +
		       SZ_NLATTR_DATA_OF(2));
	(*action_flags) |= flags;
	(*actions)--;
	return pedit_size;
}

/**
 * Retrieve mask for pattern item.
 *
 * This function does basic sanity checks on a pattern item in order to
 * return the most appropriate mask for it.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] mask_default
 *   Default mask for pattern item as specified by the flow API.
 * @param[in] mask_supported
 *   Mask fields supported by the implementation.
 * @param[in] mask_empty
 *   Empty mask to return when there is no specification.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   Either @p item->mask or one of the mask parameters on success, NULL
 *   otherwise and rte_errno is set.
 */
static const void *
flow_tcf_item_mask(const struct rte_flow_item *item, const void *mask_default,
		   const void *mask_supported, const void *mask_empty,
		   size_t mask_size, struct rte_flow_error *error)
{
	const uint8_t *mask;
	size_t i;

	/* item->last and item->mask cannot exist without item->spec. */
	if (!item->spec && (item->mask || item->last)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "\"mask\" or \"last\" field provided without"
				   " a corresponding \"spec\"");
		return NULL;
	}
	/* No spec, no mask, no problem. */
	if (!item->spec)
		return mask_empty;
	mask = item->mask ? item->mask : mask_default;
	assert(mask);
	/*
	 * Single-pass check to make sure that:
	 * - Mask is supported, no bits are set outside mask_supported.
	 * - Both item->spec and item->last are included in mask.
	 */
	for (i = 0; i != mask_size; ++i) {
		if (!mask[i])
			continue;
		if ((mask[i] | ((const uint8_t *)mask_supported)[i]) !=
		    ((const uint8_t *)mask_supported)[i]) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					   "unsupported field found"
					   " in \"mask\"");
			return NULL;
		}
		if (item->last &&
		    (((const uint8_t *)item->spec)[i] & mask[i]) !=
		    (((const uint8_t *)item->last)[i] & mask[i])) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					   item->last,
					   "range between \"spec\" and \"last\""
					   " not comprised in \"mask\"");
			return NULL;
		}
	}
	return mask;
}

/**
 * Build a conversion table between port ID and ifindex.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[out] ptoi
 *   Pointer to ptoi table.
 * @param[in] len
 *   Size of ptoi table provided.
 *
 * @return
 *   Size of ptoi table filled.
 */
static unsigned int
flow_tcf_build_ptoi_table(struct rte_eth_dev *dev, struct flow_tcf_ptoi *ptoi,
			  unsigned int len)
{
	unsigned int n = mlx5_dev_to_port_id(dev->device, NULL, 0);
	uint16_t port_id[n + 1];
	unsigned int i;
	unsigned int own = 0;

	/* At least one port is needed when no switch domain is present. */
	if (!n) {
		n = 1;
		port_id[0] = dev->data->port_id;
	} else {
		n = RTE_MIN(mlx5_dev_to_port_id(dev->device, port_id, n), n);
	}
	if (n > len)
		return 0;
	for (i = 0; i != n; ++i) {
		struct rte_eth_dev_info dev_info;

		rte_eth_dev_info_get(port_id[i], &dev_info);
		if (port_id[i] == dev->data->port_id)
			own = i;
		ptoi[i].port_id = port_id[i];
		ptoi[i].ifindex = dev_info.if_index;
	}
	/* Ensure first entry of ptoi[] is the current device. */
	if (own) {
		ptoi[n] = ptoi[0];
		ptoi[0] = ptoi[own];
		ptoi[own] = ptoi[n];
	}
	/* An entry with zero ifindex terminates ptoi[]. */
	ptoi[n].port_id = 0;
	ptoi[n].ifindex = 0;
	return n;
}

/**
 * Verify the @p attr will be correctly understood by the E-switch.
 *
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_validate_attributes(const struct rte_flow_attr *attr,
			     struct rte_flow_error *error)
{
	/*
	 * Supported attributes: no groups, some priorities and ingress only.
	 * Don't care about transfer as it is the caller's problem.
	 */
	if (attr->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP, attr,
					  "groups are not supported");
	if (attr->priority > 0xfffe)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  attr,
					  "lowest priority level is 0xfffe");
	if (!attr->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  attr, "only ingress is supported");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  attr, "egress is not supported");
	return 0;
}

/**
 * Validate flow for E-Switch.
 *
 * @param[in] priv
 *   Pointer to the priv structure.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_ernno is set.
 */
static int
flow_tcf_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	union {
		const struct rte_flow_item_port_id *port_id;
		const struct rte_flow_item_eth *eth;
		const struct rte_flow_item_vlan *vlan;
		const struct rte_flow_item_ipv4 *ipv4;
		const struct rte_flow_item_ipv6 *ipv6;
		const struct rte_flow_item_tcp *tcp;
		const struct rte_flow_item_udp *udp;
	} spec, mask;
	union {
		const struct rte_flow_action_port_id *port_id;
		const struct rte_flow_action_of_push_vlan *of_push_vlan;
		const struct rte_flow_action_of_set_vlan_vid *
			of_set_vlan_vid;
		const struct rte_flow_action_of_set_vlan_pcp *
			of_set_vlan_pcp;
		const struct rte_flow_action_set_ipv4 *set_ipv4;
		const struct rte_flow_action_set_ipv6 *set_ipv6;
	} conf;
	uint32_t item_flags = 0;
	uint32_t action_flags = 0;
	uint8_t next_protocol = -1;
	unsigned int tcm_ifindex = 0;
	uint8_t pedit_validated = 0;
	struct flow_tcf_ptoi ptoi[PTOI_TABLE_SZ_MAX(dev)];
	struct rte_eth_dev *port_id_dev = NULL;
	bool in_port_id_set;
	int ret;

	claim_nonzero(flow_tcf_build_ptoi_table(dev, ptoi,
						PTOI_TABLE_SZ_MAX(dev)));
	ret = flow_tcf_validate_attributes(attr, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		unsigned int i;

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			mask.port_id = flow_tcf_item_mask
				(items, &rte_flow_item_port_id_mask,
				 &flow_tcf_mask_supported.port_id,
				 &flow_tcf_mask_empty.port_id,
				 sizeof(flow_tcf_mask_supported.port_id),
				 error);
			if (!mask.port_id)
				return -rte_errno;
			if (mask.port_id == &flow_tcf_mask_empty.port_id) {
				in_port_id_set = 1;
				break;
			}
			spec.port_id = items->spec;
			if (mask.port_id->id && mask.port_id->id != 0xffffffff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.port_id,
					 "no support for partial mask on"
					 " \"id\" field");
			if (!mask.port_id->id)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == spec.port_id->id)
						break;
			if (!ptoi[i].ifindex)
				return rte_flow_error_set
					(error, ENODEV,
					 RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					 spec.port_id,
					 "missing data to convert port ID to"
					 " ifindex");
			if (in_port_id_set && ptoi[i].ifindex != tcm_ifindex)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					 spec.port_id,
					 "cannot match traffic for"
					 " several port IDs through"
					 " a single flow rule");
			tcm_ifindex = ptoi[i].ifindex;
			in_port_id_set = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L2;
			/* TODO:
			 * Redundant check due to different supported mask.
			 * Same for the rest of items.
			 */
			mask.eth = flow_tcf_item_mask
				(items, &rte_flow_item_eth_mask,
				 &flow_tcf_mask_supported.eth,
				 &flow_tcf_mask_empty.eth,
				 sizeof(flow_tcf_mask_supported.eth),
				 error);
			if (!mask.eth)
				return -rte_errno;
			if (mask.eth->type && mask.eth->type !=
			    RTE_BE16(0xffff))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.eth,
					 "no support for partial mask on"
					 " \"type\" field");
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_VLAN;
			mask.vlan = flow_tcf_item_mask
				(items, &rte_flow_item_vlan_mask,
				 &flow_tcf_mask_supported.vlan,
				 &flow_tcf_mask_empty.vlan,
				 sizeof(flow_tcf_mask_supported.vlan),
				 error);
			if (!mask.vlan)
				return -rte_errno;
			if ((mask.vlan->tci & RTE_BE16(0xe000) &&
			     (mask.vlan->tci & RTE_BE16(0xe000)) !=
			      RTE_BE16(0xe000)) ||
			    (mask.vlan->tci & RTE_BE16(0x0fff) &&
			     (mask.vlan->tci & RTE_BE16(0x0fff)) !=
			      RTE_BE16(0x0fff)) ||
			    (mask.vlan->inner_type &&
			     mask.vlan->inner_type != RTE_BE16(0xffff)))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.vlan,
					 "no support for partial masks on"
					 " \"tci\" (PCP and VID parts) and"
					 " \"inner_type\" fields");
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			mask.ipv4 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv4_mask,
				 &flow_tcf_mask_supported.ipv4,
				 &flow_tcf_mask_empty.ipv4,
				 sizeof(flow_tcf_mask_supported.ipv4),
				 error);
			if (!mask.ipv4)
				return -rte_errno;
			if (mask.ipv4->hdr.next_proto_id &&
			    mask.ipv4->hdr.next_proto_id != 0xff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.ipv4,
					 "no support for partial mask on"
					 " \"hdr.next_proto_id\" field");
			else if (mask.ipv4->hdr.next_proto_id)
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			mask.ipv6 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv6_mask,
				 &flow_tcf_mask_supported.ipv6,
				 &flow_tcf_mask_empty.ipv6,
				 sizeof(flow_tcf_mask_supported.ipv6),
				 error);
			if (!mask.ipv6)
				return -rte_errno;
			if (mask.ipv6->hdr.proto &&
			    mask.ipv6->hdr.proto != 0xff)
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					 mask.ipv6,
					 "no support for partial mask on"
					 " \"hdr.proto\" field");
			else if (mask.ipv6->hdr.proto)
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 (items->spec))->hdr.proto;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
			mask.udp = flow_tcf_item_mask
				(items, &rte_flow_item_udp_mask,
				 &flow_tcf_mask_supported.udp,
				 &flow_tcf_mask_empty.udp,
				 sizeof(flow_tcf_mask_supported.udp),
				 error);
			if (!mask.udp)
				return -rte_errno;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
					     (items, item_flags,
					      next_protocol,
					      &flow_tcf_mask_supported.tcp,
					      error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_OUTER_L4_TCP;
			mask.tcp = flow_tcf_item_mask
				(items, &rte_flow_item_tcp_mask,
				 &flow_tcf_mask_supported.tcp,
				 &flow_tcf_mask_empty.tcp,
				 sizeof(flow_tcf_mask_supported.tcp),
				 error);
			if (!mask.tcp)
				return -rte_errno;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		unsigned int i;
		uint32_t current_action_flag = 0;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			current_action_flag = MLX5_FLOW_ACTION_PORT_ID;
			if (action_flags & MLX5_TCF_FATE_ACTIONS)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "can't have multiple fate actions");
			if (!actions->conf)
				break;
			conf.port_id = actions->conf;
			if (conf.port_id->original)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == conf.port_id->id)
						break;
			if (!ptoi[i].ifindex)
				return rte_flow_error_set
					(error, ENODEV,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					 conf.port_id,
					 "missing data to convert port ID to"
					 " ifindex");
			port_id_dev = &rte_eth_devices[conf.port_id->id];
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			if (action_flags & MLX5_TCF_FATE_ACTIONS)
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "can't have multiple fate actions");
			current_action_flag = MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			current_action_flag = MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			current_action_flag = MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "vlan modify is not supported,"
					 " set action must follow push action");
			current_action_flag = MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN))
				return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION, actions,
					 "vlan modify is not supported,"
					 " set action must follow push action");
			current_action_flag = MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV4_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV6_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			current_action_flag = MLX5_FLOW_ACTION_SET_TP_SRC;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			current_action_flag = MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
		if (current_action_flag & MLX5_TCF_CONFIG_ACTIONS) {
			if (!actions->conf)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						actions,
						"action configuration not set");
		}
		if ((current_action_flag & MLX5_TCF_PEDIT_ACTIONS) &&
				pedit_validated)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "set actions should be "
						  "listed successively");
		if ((current_action_flag & ~MLX5_TCF_PEDIT_ACTIONS) &&
		    (action_flags & MLX5_TCF_PEDIT_ACTIONS))
			pedit_validated = 1;
		action_flags |= current_action_flag;
	}
	if ((action_flags & MLX5_TCF_PEDIT_ACTIONS) &&
	    (action_flags & MLX5_FLOW_ACTION_DROP))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  actions,
					  "set action is not compatible with "
					  "drop action");
	if ((action_flags & MLX5_TCF_PEDIT_ACTIONS) &&
	    !(action_flags & MLX5_FLOW_ACTION_PORT_ID))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  actions,
					  "set action must be followed by "
					  "port_id action");
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_IPV4_SRC | MLX5_FLOW_ACTION_SET_IPV4_DST)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no ipv4 item found in"
						  " pattern");
	}
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_IPV6_SRC | MLX5_FLOW_ACTION_SET_IPV6_DST)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV6))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no ipv6 item found in"
						  " pattern");
	}
	if (action_flags &
	   (MLX5_FLOW_ACTION_SET_TP_SRC | MLX5_FLOW_ACTION_SET_TP_DST)) {
		if (!(item_flags &
		     (MLX5_FLOW_LAYER_OUTER_L4_UDP |
		      MLX5_FLOW_LAYER_OUTER_L4_TCP)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no TCP/UDP item found in"
						  " pattern");
	}
	/*
	 * FW syndrome (0xA9C090):
	 *     set_flow_table_entry: push vlan action fte in fdb can ONLY be
	 *     forward to the uplink.
	 */
	if ((action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN) &&
	    (action_flags & MLX5_FLOW_ACTION_PORT_ID) &&
	    ((struct priv *)port_id_dev->data->dev_private)->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "vlan push can only be applied"
					  " when forwarding to uplink port");
	/*
	 * FW syndrome (0x294609):
	 *     set_flow_table_entry: modify/pop/push actions in fdb flow table
	 *     are supported only while forwarding to vport.
	 */
	if ((action_flags & MLX5_TCF_VLAN_ACTIONS) &&
	    !(action_flags & MLX5_FLOW_ACTION_PORT_ID))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "vlan actions are supported"
					  " only with port_id action");
	if (!(action_flags & MLX5_TCF_FATE_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "no fate action is found");
	return 0;
}

/**
 * Calculate maximum size of memory for flow items of Linux TC flower and
 * extract specified items.
 *
 * @param[in] items
 *   Pointer to the list of items.
 * @param[out] item_flags
 *   Pointer to the detected items.
 *
 * @return
 *   Maximum size of memory for items.
 */
static int
flow_tcf_get_items_and_size(const struct rte_flow_item items[],
			    uint64_t *item_flags)
{
	int size = 0;
	uint64_t flags = 0;

	size += SZ_NLATTR_STRZ_OF("flower") +
		SZ_NLATTR_NEST + /* TCA_OPTIONS. */
		SZ_NLATTR_TYPE_OF(uint32_t); /* TCA_CLS_FLAGS_SKIP_SW. */
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			size += SZ_NLATTR_TYPE_OF(uint16_t) + /* Ether type. */
				SZ_NLATTR_DATA_OF(ETHER_ADDR_LEN) * 4;
				/* dst/src MAC addr and mask. */
			flags |= MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			size += SZ_NLATTR_TYPE_OF(uint16_t) + /* Ether type. */
				SZ_NLATTR_TYPE_OF(uint16_t) +
				/* VLAN Ether type. */
				SZ_NLATTR_TYPE_OF(uint8_t) + /* VLAN prio. */
				SZ_NLATTR_TYPE_OF(uint16_t); /* VLAN ID. */
			flags |= MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			size += SZ_NLATTR_TYPE_OF(uint16_t) + /* Ether type. */
				SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint32_t) * 4;
				/* dst/src IP addr and mask. */
			flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			size += SZ_NLATTR_TYPE_OF(uint16_t) + /* Ether type. */
				SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(IPV6_ADDR_LEN) * 4;
				/* dst/src IP addr and mask. */
			flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			size += SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint16_t) * 4;
				/* dst/src port and mask. */
			flags |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			size += SZ_NLATTR_TYPE_OF(uint8_t) + /* IP proto. */
				SZ_NLATTR_TYPE_OF(uint16_t) * 4;
				/* dst/src port and mask. */
			flags |= MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		default:
			DRV_LOG(WARNING,
				"unsupported item %p type %d,"
				" items must be validated before flow creation",
				(const void *)items, items->type);
			break;
		}
	}
	*item_flags = flags;
	return size;
}

/**
 * Calculate maximum size of memory for flow actions of Linux TC flower and
 * extract specified actions.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] action_flags
 *   Pointer to the detected actions.
 *
 * @return
 *   Maximum size of memory for actions.
 */
static int
flow_tcf_get_actions_and_size(const struct rte_flow_action actions[],
			      uint64_t *action_flags)
{
	int size = 0;
	uint64_t flags = 0;

	size += SZ_NLATTR_NEST; /* TCA_FLOWER_ACT. */
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("mirred") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_mirred);
			flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("gact") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_gact);
			flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			goto action_of_vlan;
action_of_vlan:
			size += SZ_NLATTR_NEST + /* na_act_index. */
				SZ_NLATTR_STRZ_OF("vlan") +
				SZ_NLATTR_NEST + /* TCA_ACT_OPTIONS. */
				SZ_NLATTR_TYPE_OF(struct tc_vlan) +
				SZ_NLATTR_TYPE_OF(uint16_t) +
				/* VLAN protocol. */
				SZ_NLATTR_TYPE_OF(uint16_t) + /* VLAN ID. */
				SZ_NLATTR_TYPE_OF(uint8_t); /* VLAN prio. */
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			size += flow_tcf_get_pedit_actions_size(&actions,
								&flags);
			break;
		default:
			DRV_LOG(WARNING,
				"unsupported action %p type %d,"
				" items must be validated before flow creation",
				(const void *)actions, actions->type);
			break;
		}
	}
	*action_flags = flags;
	return size;
}

/**
 * Brand rtnetlink buffer with unique handle.
 *
 * This handle should be unique for a given network interface to avoid
 * collisions.
 *
 * @param nlh
 *   Pointer to Netlink message.
 * @param handle
 *   Unique 32-bit handle to use.
 */
static void
flow_tcf_nl_brand(struct nlmsghdr *nlh, uint32_t handle)
{
	struct tcmsg *tcm = mnl_nlmsg_get_payload(nlh);

	tcm->tcm_handle = handle;
	DRV_LOG(DEBUG, "Netlink msg %p is branded with handle %x",
		(void *)nlh, handle);
}

/**
 * Prepare a flow object for Linux TC flower. It calculates the maximum size of
 * memory required, allocates the memory, initializes Netlink message headers
 * and set unique TC message handle.
 *
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] item_flags
 *   Pointer to bit mask of all items detected.
 * @param[out] action_flags
 *   Pointer to bit mask of all actions detected.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Pointer to mlx5_flow object on success,
 *   otherwise NULL and rte_ernno is set.
 */
static struct mlx5_flow *
flow_tcf_prepare(const struct rte_flow_attr *attr __rte_unused,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 uint64_t *item_flags, uint64_t *action_flags,
		 struct rte_flow_error *error)
{
	size_t size = sizeof(struct mlx5_flow) +
		      MNL_ALIGN(sizeof(struct nlmsghdr)) +
		      MNL_ALIGN(sizeof(struct tcmsg));
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;

	size += flow_tcf_get_items_and_size(items, item_flags);
	size += flow_tcf_get_actions_and_size(actions, action_flags);
	dev_flow = rte_zmalloc(__func__, size, MNL_ALIGNTO);
	if (!dev_flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create E-Switch flow");
		return NULL;
	}
	nlh = mnl_nlmsg_put_header((void *)(dev_flow + 1));
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	*dev_flow = (struct mlx5_flow){
		.tcf = (struct mlx5_flow_tcf){
			.nlh = nlh,
			.tcm = tcm,
		},
	};
	/*
	 * Generate a reasonably unique handle based on the address of the
	 * target buffer.
	 *
	 * This is straightforward on 32-bit systems where the flow pointer can
	 * be used directly. Otherwise, its least significant part is taken
	 * after shifting it by the previous power of two of the pointed buffer
	 * size.
	 */
	if (sizeof(dev_flow) <= 4)
		flow_tcf_nl_brand(nlh, (uintptr_t)dev_flow);
	else
		flow_tcf_nl_brand(nlh, (uintptr_t)dev_flow >>
				       rte_log2_u32(rte_align32prevpow2(size)));
	return dev_flow;
}

/**
 * Translate flow for Linux TC flower and construct Netlink message.
 *
 * @param[in] priv
 *   Pointer to the priv structure.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_ernno is set.
 */
static int
flow_tcf_translate(struct rte_eth_dev *dev, struct mlx5_flow *dev_flow,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	union {
		const struct rte_flow_item_port_id *port_id;
		const struct rte_flow_item_eth *eth;
		const struct rte_flow_item_vlan *vlan;
		const struct rte_flow_item_ipv4 *ipv4;
		const struct rte_flow_item_ipv6 *ipv6;
		const struct rte_flow_item_tcp *tcp;
		const struct rte_flow_item_udp *udp;
	} spec, mask;
	union {
		const struct rte_flow_action_port_id *port_id;
		const struct rte_flow_action_of_push_vlan *of_push_vlan;
		const struct rte_flow_action_of_set_vlan_vid *
			of_set_vlan_vid;
		const struct rte_flow_action_of_set_vlan_pcp *
			of_set_vlan_pcp;
	} conf;
	struct flow_tcf_ptoi ptoi[PTOI_TABLE_SZ_MAX(dev)];
	struct nlmsghdr *nlh = dev_flow->tcf.nlh;
	struct tcmsg *tcm = dev_flow->tcf.tcm;
	uint32_t na_act_index_cur;
	bool eth_type_set = 0;
	bool vlan_present = 0;
	bool vlan_eth_type_set = 0;
	bool ip_proto_set = 0;
	struct nlattr *na_flower;
	struct nlattr *na_flower_act;
	struct nlattr *na_vlan_id = NULL;
	struct nlattr *na_vlan_priority = NULL;
	uint64_t item_flags = 0;

	claim_nonzero(flow_tcf_build_ptoi_table(dev, ptoi,
						PTOI_TABLE_SZ_MAX(dev)));
	nlh = dev_flow->tcf.nlh;
	tcm = dev_flow->tcf.tcm;
	/* Prepare API must have been called beforehand. */
	assert(nlh != NULL && tcm != NULL);
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ptoi[0].ifindex;
	tcm->tcm_parent = TC_H_MAKE(TC_H_INGRESS, TC_H_MIN_INGRESS);
	/*
	 * Priority cannot be zero to prevent the kernel from picking one
	 * automatically.
	 */
	tcm->tcm_info = TC_H_MAKE((attr->priority + 1) << 16,
				  RTE_BE16(ETH_P_ALL));
	mnl_attr_put_strz(nlh, TCA_KIND, "flower");
	na_flower = mnl_attr_nest_start(nlh, TCA_OPTIONS);
	mnl_attr_put_u32(nlh, TCA_FLOWER_FLAGS, TCA_CLS_FLAGS_SKIP_SW);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		unsigned int i;

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			mask.port_id = flow_tcf_item_mask
				(items, &rte_flow_item_port_id_mask,
				 &flow_tcf_mask_supported.port_id,
				 &flow_tcf_mask_empty.port_id,
				 sizeof(flow_tcf_mask_supported.port_id),
				 error);
			assert(mask.port_id);
			if (mask.port_id == &flow_tcf_mask_empty.port_id)
				break;
			spec.port_id = items->spec;
			if (!mask.port_id->id)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == spec.port_id->id)
						break;
			assert(ptoi[i].ifindex);
			tcm->tcm_ifindex = ptoi[i].ifindex;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			item_flags |= MLX5_FLOW_LAYER_OUTER_L2;
			mask.eth = flow_tcf_item_mask
				(items, &rte_flow_item_eth_mask,
				 &flow_tcf_mask_supported.eth,
				 &flow_tcf_mask_empty.eth,
				 sizeof(flow_tcf_mask_supported.eth),
				 error);
			assert(mask.eth);
			if (mask.eth == &flow_tcf_mask_empty.eth)
				break;
			spec.eth = items->spec;
			if (mask.eth->type) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_ETH_TYPE,
						 spec.eth->type);
				eth_type_set = 1;
			}
			if (!is_zero_ether_addr(&mask.eth->dst)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_DST,
					     ETHER_ADDR_LEN,
					     spec.eth->dst.addr_bytes);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_DST_MASK,
					     ETHER_ADDR_LEN,
					     mask.eth->dst.addr_bytes);
			}
			if (!is_zero_ether_addr(&mask.eth->src)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_SRC,
					     ETHER_ADDR_LEN,
					     spec.eth->src.addr_bytes);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_ETH_SRC_MASK,
					     ETHER_ADDR_LEN,
					     mask.eth->src.addr_bytes);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			item_flags |= MLX5_FLOW_LAYER_OUTER_VLAN;
			mask.vlan = flow_tcf_item_mask
				(items, &rte_flow_item_vlan_mask,
				 &flow_tcf_mask_supported.vlan,
				 &flow_tcf_mask_empty.vlan,
				 sizeof(flow_tcf_mask_supported.vlan),
				 error);
			assert(mask.vlan);
			if (!eth_type_set)
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_ETH_TYPE,
						 RTE_BE16(ETH_P_8021Q));
			eth_type_set = 1;
			vlan_present = 1;
			if (mask.vlan == &flow_tcf_mask_empty.vlan)
				break;
			spec.vlan = items->spec;
			if (mask.vlan->inner_type) {
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_VLAN_ETH_TYPE,
						 spec.vlan->inner_type);
				vlan_eth_type_set = 1;
			}
			if (mask.vlan->tci & RTE_BE16(0xe000))
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_VLAN_PRIO,
						(rte_be_to_cpu_16
						 (spec.vlan->tci) >> 13) & 0x7);
			if (mask.vlan->tci & RTE_BE16(0x0fff))
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_VLAN_ID,
						 rte_be_to_cpu_16
						 (spec.vlan->tci &
						  RTE_BE16(0x0fff)));
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			mask.ipv4 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv4_mask,
				 &flow_tcf_mask_supported.ipv4,
				 &flow_tcf_mask_empty.ipv4,
				 sizeof(flow_tcf_mask_supported.ipv4),
				 error);
			assert(mask.ipv4);
			if (!eth_type_set || !vlan_eth_type_set)
				mnl_attr_put_u16(nlh,
						 vlan_present ?
						 TCA_FLOWER_KEY_VLAN_ETH_TYPE :
						 TCA_FLOWER_KEY_ETH_TYPE,
						 RTE_BE16(ETH_P_IP));
			eth_type_set = 1;
			vlan_eth_type_set = 1;
			if (mask.ipv4 == &flow_tcf_mask_empty.ipv4)
				break;
			spec.ipv4 = items->spec;
			if (mask.ipv4->hdr.next_proto_id) {
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						spec.ipv4->hdr.next_proto_id);
				ip_proto_set = 1;
			}
			if (mask.ipv4->hdr.src_addr) {
				mnl_attr_put_u32(nlh, TCA_FLOWER_KEY_IPV4_SRC,
						 spec.ipv4->hdr.src_addr);
				mnl_attr_put_u32(nlh,
						 TCA_FLOWER_KEY_IPV4_SRC_MASK,
						 mask.ipv4->hdr.src_addr);
			}
			if (mask.ipv4->hdr.dst_addr) {
				mnl_attr_put_u32(nlh, TCA_FLOWER_KEY_IPV4_DST,
						 spec.ipv4->hdr.dst_addr);
				mnl_attr_put_u32(nlh,
						 TCA_FLOWER_KEY_IPV4_DST_MASK,
						 mask.ipv4->hdr.dst_addr);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			item_flags |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			mask.ipv6 = flow_tcf_item_mask
				(items, &rte_flow_item_ipv6_mask,
				 &flow_tcf_mask_supported.ipv6,
				 &flow_tcf_mask_empty.ipv6,
				 sizeof(flow_tcf_mask_supported.ipv6),
				 error);
			assert(mask.ipv6);
			if (!eth_type_set || !vlan_eth_type_set)
				mnl_attr_put_u16(nlh,
						 vlan_present ?
						 TCA_FLOWER_KEY_VLAN_ETH_TYPE :
						 TCA_FLOWER_KEY_ETH_TYPE,
						 RTE_BE16(ETH_P_IPV6));
			eth_type_set = 1;
			vlan_eth_type_set = 1;
			if (mask.ipv6 == &flow_tcf_mask_empty.ipv6)
				break;
			spec.ipv6 = items->spec;
			if (mask.ipv6->hdr.proto) {
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						spec.ipv6->hdr.proto);
				ip_proto_set = 1;
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(mask.ipv6->hdr.src_addr)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_SRC,
					     sizeof(spec.ipv6->hdr.src_addr),
					     spec.ipv6->hdr.src_addr);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_SRC_MASK,
					     sizeof(mask.ipv6->hdr.src_addr),
					     mask.ipv6->hdr.src_addr);
			}
			if (!IN6_IS_ADDR_UNSPECIFIED(mask.ipv6->hdr.dst_addr)) {
				mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_DST,
					     sizeof(spec.ipv6->hdr.dst_addr),
					     spec.ipv6->hdr.dst_addr);
				mnl_attr_put(nlh, TCA_FLOWER_KEY_IPV6_DST_MASK,
					     sizeof(mask.ipv6->hdr.dst_addr),
					     mask.ipv6->hdr.dst_addr);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			item_flags |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
			mask.udp = flow_tcf_item_mask
				(items, &rte_flow_item_udp_mask,
				 &flow_tcf_mask_supported.udp,
				 &flow_tcf_mask_empty.udp,
				 sizeof(flow_tcf_mask_supported.udp),
				 error);
			assert(mask.udp);
			if (!ip_proto_set)
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						IPPROTO_UDP);
			if (mask.udp == &flow_tcf_mask_empty.udp)
				break;
			spec.udp = items->spec;
			if (mask.udp->hdr.src_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_UDP_SRC,
						 spec.udp->hdr.src_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_UDP_SRC_MASK,
						 mask.udp->hdr.src_port);
			}
			if (mask.udp->hdr.dst_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_UDP_DST,
						 spec.udp->hdr.dst_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_UDP_DST_MASK,
						 mask.udp->hdr.dst_port);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			item_flags |= MLX5_FLOW_LAYER_OUTER_L4_TCP;
			mask.tcp = flow_tcf_item_mask
				(items, &rte_flow_item_tcp_mask,
				 &flow_tcf_mask_supported.tcp,
				 &flow_tcf_mask_empty.tcp,
				 sizeof(flow_tcf_mask_supported.tcp),
				 error);
			assert(mask.tcp);
			if (!ip_proto_set)
				mnl_attr_put_u8(nlh, TCA_FLOWER_KEY_IP_PROTO,
						IPPROTO_TCP);
			if (mask.tcp == &flow_tcf_mask_empty.tcp)
				break;
			spec.tcp = items->spec;
			if (mask.tcp->hdr.src_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_TCP_SRC,
						 spec.tcp->hdr.src_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_TCP_SRC_MASK,
						 mask.tcp->hdr.src_port);
			}
			if (mask.tcp->hdr.dst_port) {
				mnl_attr_put_u16(nlh, TCA_FLOWER_KEY_TCP_DST,
						 spec.tcp->hdr.dst_port);
				mnl_attr_put_u16(nlh,
						 TCA_FLOWER_KEY_TCP_DST_MASK,
						 mask.tcp->hdr.dst_port);
			}
			if (mask.tcp->hdr.tcp_flags) {
				mnl_attr_put_u16
					(nlh,
					 TCA_FLOWER_KEY_TCP_FLAGS,
					 rte_cpu_to_be_16
						(spec.tcp->hdr.tcp_flags));
				mnl_attr_put_u16
					(nlh,
					 TCA_FLOWER_KEY_TCP_FLAGS_MASK,
					 rte_cpu_to_be_16
						(mask.tcp->hdr.tcp_flags));
			}
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	na_flower_act = mnl_attr_nest_start(nlh, TCA_FLOWER_ACT);
	na_act_index_cur = 1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		struct nlattr *na_act_index;
		struct nlattr *na_act;
		unsigned int vlan_act;
		unsigned int i;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			conf.port_id = actions->conf;
			if (conf.port_id->original)
				i = 0;
			else
				for (i = 0; ptoi[i].ifindex; ++i)
					if (ptoi[i].port_id == conf.port_id->id)
						break;
			assert(ptoi[i].ifindex);
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "mirred");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_MIRRED_PARMS,
				     sizeof(struct tc_mirred),
				     &(struct tc_mirred){
					.action = TC_ACT_STOLEN,
					.eaction = TCA_EGRESS_REDIR,
					.ifindex = ptoi[i].ifindex,
				     });
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "gact");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_GACT_PARMS,
				     sizeof(struct tc_gact),
				     &(struct tc_gact){
					.action = TC_ACT_SHOT,
				     });
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			conf.of_push_vlan = NULL;
			vlan_act = TCA_VLAN_ACT_POP;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			conf.of_push_vlan = actions->conf;
			vlan_act = TCA_VLAN_ACT_PUSH;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			conf.of_set_vlan_vid = actions->conf;
			if (na_vlan_id)
				goto override_na_vlan_id;
			vlan_act = TCA_VLAN_ACT_MODIFY;
			goto action_of_vlan;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			conf.of_set_vlan_pcp = actions->conf;
			if (na_vlan_priority)
				goto override_na_vlan_priority;
			vlan_act = TCA_VLAN_ACT_MODIFY;
			goto action_of_vlan;
action_of_vlan:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			assert(na_act_index);
			mnl_attr_put_strz(nlh, TCA_ACT_KIND, "vlan");
			na_act = mnl_attr_nest_start(nlh, TCA_ACT_OPTIONS);
			assert(na_act);
			mnl_attr_put(nlh, TCA_VLAN_PARMS,
				     sizeof(struct tc_vlan),
				     &(struct tc_vlan){
					.action = TC_ACT_PIPE,
					.v_action = vlan_act,
				     });
			if (vlan_act == TCA_VLAN_ACT_POP) {
				mnl_attr_nest_end(nlh, na_act);
				mnl_attr_nest_end(nlh, na_act_index);
				break;
			}
			if (vlan_act == TCA_VLAN_ACT_PUSH)
				mnl_attr_put_u16(nlh,
						 TCA_VLAN_PUSH_VLAN_PROTOCOL,
						 conf.of_push_vlan->ethertype);
			na_vlan_id = mnl_nlmsg_get_payload_tail(nlh);
			mnl_attr_put_u16(nlh, TCA_VLAN_PAD, 0);
			na_vlan_priority = mnl_nlmsg_get_payload_tail(nlh);
			mnl_attr_put_u8(nlh, TCA_VLAN_PAD, 0);
			mnl_attr_nest_end(nlh, na_act);
			mnl_attr_nest_end(nlh, na_act_index);
			if (actions->type ==
			    RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) {
override_na_vlan_id:
				na_vlan_id->nla_type = TCA_VLAN_PUSH_VLAN_ID;
				*(uint16_t *)mnl_attr_get_payload(na_vlan_id) =
					rte_be_to_cpu_16
					(conf.of_set_vlan_vid->vlan_vid);
			} else if (actions->type ==
				   RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) {
override_na_vlan_priority:
				na_vlan_priority->nla_type =
					TCA_VLAN_PUSH_VLAN_PRIORITY;
				*(uint8_t *)mnl_attr_get_payload
					(na_vlan_priority) =
					conf.of_set_vlan_pcp->vlan_pcp;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			na_act_index =
				mnl_attr_nest_start(nlh, na_act_index_cur++);
			flow_tcf_create_pedit_mnl_msg(nlh,
						      &actions, item_flags);
			mnl_attr_nest_end(nlh, na_act_index);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	assert(na_flower);
	assert(na_flower_act);
	mnl_attr_nest_end(nlh, na_flower_act);
	mnl_attr_nest_end(nlh, na_flower);
	return 0;
}

/**
 * Send Netlink message with acknowledgment.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param nlh
 *   Message to send. This function always raises the NLM_F_ACK flag before
 *   sending.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_tcf_nl_ack(struct mnl_socket *nl, struct nlmsghdr *nlh)
{
	alignas(struct nlmsghdr)
	uint8_t ans[mnl_nlmsg_size(sizeof(struct nlmsgerr)) +
		    nlh->nlmsg_len - sizeof(*nlh)];
	uint32_t seq = random();
	int ret;

	nlh->nlmsg_flags |= NLM_F_ACK;
	nlh->nlmsg_seq = seq;
	ret = mnl_socket_sendto(nl, nlh, nlh->nlmsg_len);
	if (ret != -1)
		ret = mnl_socket_recvfrom(nl, ans, sizeof(ans));
	if (ret != -1)
		ret = mnl_cb_run
			(ans, ret, seq, mnl_socket_get_portid(nl), NULL, NULL);
	if (ret > 0)
		return 0;
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Apply flow to E-Switch by sending Netlink message.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_ernno is set.
 */
static int
flow_tcf_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct mnl_socket *nl = priv->mnl_socket;
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;

	dev_flow = LIST_FIRST(&flow->dev_flows);
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	nlh = dev_flow->tcf.nlh;
	nlh->nlmsg_type = RTM_NEWTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	if (!flow_tcf_nl_ack(nl, nlh))
		return 0;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "netlink: failed to create TC flow rule");
}

/**
 * Remove flow from E-Switch by sending Netlink message.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 */
static void
flow_tcf_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	struct mnl_socket *nl = priv->mnl_socket;
	struct mlx5_flow *dev_flow;
	struct nlmsghdr *nlh;

	if (!flow)
		return;
	dev_flow = LIST_FIRST(&flow->dev_flows);
	if (!dev_flow)
		return;
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	nlh = dev_flow->tcf.nlh;
	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	flow_tcf_nl_ack(nl, nlh);
}

/**
 * Remove flow from E-Switch and release resources of the device flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 */
static void
flow_tcf_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	flow_tcf_remove(dev, flow);
	dev_flow = LIST_FIRST(&flow->dev_flows);
	if (!dev_flow)
		return;
	/* E-Switch flow can't be expanded. */
	assert(!LIST_NEXT(dev_flow, next));
	LIST_REMOVE(dev_flow, next);
	rte_free(dev_flow);
}

const struct mlx5_flow_driver_ops mlx5_flow_tcf_drv_ops = {
	.validate = flow_tcf_validate,
	.prepare = flow_tcf_prepare,
	.translate = flow_tcf_translate,
	.apply = flow_tcf_apply,
	.remove = flow_tcf_remove,
	.destroy = flow_tcf_destroy,
};

/**
 * Initialize ingress qdisc of a given network interface.
 *
 * @param nl
 *   Libmnl socket of the @p NETLINK_ROUTE kind.
 * @param ifindex
 *   Index of network interface to initialize.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_tcf_init(struct mnl_socket *nl, unsigned int ifindex,
		   struct rte_flow_error *error)
{
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	alignas(struct nlmsghdr)
	uint8_t buf[mnl_nlmsg_size(sizeof(*tcm) + 128)];

	/* Destroy existing ingress qdisc and everything attached to it. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_DELQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	/* Ignore errors when qdisc is already absent. */
	if (flow_tcf_nl_ack(nl, nlh) &&
	    rte_errno != EINVAL && rte_errno != ENOENT)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "netlink: failed to remove ingress"
					  " qdisc");
	/* Create fresh ingress qdisc. */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_NEWQDISC;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
	tcm->tcm_family = AF_UNSPEC;
	tcm->tcm_ifindex = ifindex;
	tcm->tcm_handle = TC_H_MAKE(TC_H_INGRESS, 0);
	tcm->tcm_parent = TC_H_INGRESS;
	mnl_attr_put_strz_check(nlh, sizeof(buf), TCA_KIND, "ingress");
	if (flow_tcf_nl_ack(nl, nlh))
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "netlink: failed to create ingress"
					  " qdisc");
	return 0;
}

/**
 * Create and configure a libmnl socket for Netlink flow rules.
 *
 * @return
 *   A valid libmnl socket object pointer on success, NULL otherwise and
 *   rte_errno is set.
 */
struct mnl_socket *
mlx5_flow_tcf_socket_create(void)
{
	struct mnl_socket *nl = mnl_socket_open(NETLINK_ROUTE);

	if (nl) {
		mnl_socket_setsockopt(nl, NETLINK_CAP_ACK, &(int){ 1 },
				      sizeof(int));
		if (!mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID))
			return nl;
	}
	rte_errno = errno;
	if (nl)
		mnl_socket_close(nl);
	return NULL;
}

/**
 * Destroy a libmnl socket.
 *
 * @param nl
 *   Libmnl socket of the @p NETLINK_ROUTE kind.
 */
void
mlx5_flow_tcf_socket_destroy(struct mnl_socket *nl)
{
	mnl_socket_close(nl);
}
