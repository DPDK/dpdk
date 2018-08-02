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

#include "mlx5.h"
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

/** Parser state definitions for mlx5_nl_flow_trans[]. */
enum mlx5_nl_flow_trans {
	INVALID,
	BACK,
	ATTR,
	PATTERN,
	ITEM_VOID,
	ITEM_PORT_ID,
	ITEM_ETH,
	ITEM_VLAN,
	ITEM_IPV4,
	ITEM_IPV6,
	ITEM_TCP,
	ITEM_UDP,
	ACTIONS,
	ACTION_VOID,
	ACTION_PORT_ID,
	ACTION_DROP,
	ACTION_OF_POP_VLAN,
	ACTION_OF_PUSH_VLAN,
	ACTION_OF_SET_VLAN_VID,
	ACTION_OF_SET_VLAN_PCP,
	END,
};

#define TRANS(...) (const enum mlx5_nl_flow_trans []){ __VA_ARGS__, INVALID, }

#define PATTERN_COMMON \
	ITEM_VOID, ITEM_PORT_ID, ACTIONS
#define ACTIONS_COMMON \
	ACTION_VOID, ACTION_OF_POP_VLAN, ACTION_OF_PUSH_VLAN, \
	ACTION_OF_SET_VLAN_VID, ACTION_OF_SET_VLAN_PCP
#define ACTIONS_FATE \
	ACTION_PORT_ID, ACTION_DROP

/** Parser state transitions used by mlx5_nl_flow_transpose(). */
static const enum mlx5_nl_flow_trans *const mlx5_nl_flow_trans[] = {
	[INVALID] = NULL,
	[BACK] = NULL,
	[ATTR] = TRANS(PATTERN),
	[PATTERN] = TRANS(ITEM_ETH, PATTERN_COMMON),
	[ITEM_VOID] = TRANS(BACK),
	[ITEM_PORT_ID] = TRANS(BACK),
	[ITEM_ETH] = TRANS(ITEM_IPV4, ITEM_IPV6, ITEM_VLAN, PATTERN_COMMON),
	[ITEM_VLAN] = TRANS(ITEM_IPV4, ITEM_IPV6, PATTERN_COMMON),
	[ITEM_IPV4] = TRANS(ITEM_TCP, ITEM_UDP, PATTERN_COMMON),
	[ITEM_IPV6] = TRANS(ITEM_TCP, ITEM_UDP, PATTERN_COMMON),
	[ITEM_TCP] = TRANS(PATTERN_COMMON),
	[ITEM_UDP] = TRANS(PATTERN_COMMON),
	[ACTIONS] = TRANS(ACTIONS_FATE, ACTIONS_COMMON),
	[ACTION_VOID] = TRANS(BACK),
	[ACTION_PORT_ID] = TRANS(ACTION_VOID, END),
	[ACTION_DROP] = TRANS(ACTION_VOID, END),
	[ACTION_OF_POP_VLAN] = TRANS(ACTIONS_FATE, ACTIONS_COMMON),
	[ACTION_OF_PUSH_VLAN] = TRANS(ACTIONS_FATE, ACTIONS_COMMON),
	[ACTION_OF_SET_VLAN_VID] = TRANS(ACTIONS_FATE, ACTIONS_COMMON),
	[ACTION_OF_SET_VLAN_PCP] = TRANS(ACTIONS_FATE, ACTIONS_COMMON),
	[END] = NULL,
};

/** Empty masks for known item types. */
static const union {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
} mlx5_nl_flow_mask_empty;

/** Supported masks for known item types. */
static const struct {
	struct rte_flow_item_port_id port_id;
	struct rte_flow_item_eth eth;
	struct rte_flow_item_vlan vlan;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_udp udp;
} mlx5_nl_flow_mask_supported = {
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
	},
	.udp.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};

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
mlx5_nl_flow_item_mask(const struct rte_flow_item *item,
		       const void *mask_default,
		       const void *mask_supported,
		       const void *mask_empty,
		       size_t mask_size,
		       struct rte_flow_error *error)
{
	const uint8_t *mask;
	size_t i;

	/* item->last and item->mask cannot exist without item->spec. */
	if (!item->spec && (item->mask || item->last)) {
		rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "\"mask\" or \"last\" field provided without a"
			 " corresponding \"spec\"");
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
			rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask, "unsupported field found in \"mask\"");
			return NULL;
		}
		if (item->last &&
		    (((const uint8_t *)item->spec)[i] & mask[i]) !=
		    (((const uint8_t *)item->last)[i] & mask[i])) {
			rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_LAST,
				 item->last,
				 "range between \"spec\" and \"last\" not"
				 " comprised in \"mask\"");
			return NULL;
		}
	}
	return mask;
}

/**
 * Transpose flow rule description to rtnetlink message.
 *
 * This function transposes a flow rule description to a traffic control
 * (TC) filter creation message ready to be sent over Netlink.
 *
 * Target interface is specified as the first entry of the @p ptoi table.
 * Subsequent entries enable this function to resolve other DPDK port IDs
 * found in the flow rule.
 *
 * @param[out] buf
 *   Output message buffer. May be NULL when @p size is 0.
 * @param size
 *   Size of @p buf. Message may be truncated if not large enough.
 * @param[in] ptoi
 *   DPDK port ID to network interface index translation table. This table
 *   is terminated by an entry with a zero ifindex value.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification.
 * @param[in] actions
 *   Associated actions.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the exact size of the message in bytes
 *   regardless of the @p size parameter on success, a negative errno value
 *   otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_transpose(void *buf,
		       size_t size,
		       const struct mlx5_nl_flow_ptoi *ptoi,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item *pattern,
		       const struct rte_flow_action *actions,
		       struct rte_flow_error *error)
{
	alignas(struct nlmsghdr)
	uint8_t buf_tmp[mnl_nlmsg_size(sizeof(struct tcmsg) + 1024)];
	const struct rte_flow_item *item;
	const struct rte_flow_action *action;
	unsigned int n;
	uint32_t act_index_cur;
	bool in_port_id_set;
	bool eth_type_set;
	bool vlan_present;
	bool vlan_eth_type_set;
	bool ip_proto_set;
	struct nlattr *na_flower;
	struct nlattr *na_flower_act;
	struct nlattr *na_vlan_id;
	struct nlattr *na_vlan_priority;
	const enum mlx5_nl_flow_trans *trans;
	const enum mlx5_nl_flow_trans *back;

	if (!size)
		goto error_nobufs;
init:
	item = pattern;
	action = actions;
	n = 0;
	act_index_cur = 0;
	in_port_id_set = false;
	eth_type_set = false;
	vlan_present = false;
	vlan_eth_type_set = false;
	ip_proto_set = false;
	na_flower = NULL;
	na_flower_act = NULL;
	na_vlan_id = NULL;
	na_vlan_priority = NULL;
	trans = TRANS(ATTR);
	back = trans;
trans:
	switch (trans[n++]) {
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
		struct nlmsghdr *nlh;
		struct tcmsg *tcm;
		struct nlattr *act_index;
		struct nlattr *act;
		unsigned int i;

	case INVALID:
		if (item->type)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
				 item, "unsupported pattern item combination");
		else if (action->type)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				 action, "unsupported action combination");
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "flow rule lacks some kind of fate action");
	case BACK:
		trans = back;
		n = 0;
		goto trans;
	case ATTR:
		/*
		 * Supported attributes: no groups, some priorities and
		 * ingress only. Don't care about transfer as it is the
		 * caller's problem.
		 */
		if (attr->group)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				 attr, "groups are not supported");
		if (attr->priority > 0xfffe)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				 attr, "lowest priority level is 0xfffe");
		if (!attr->ingress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				 attr, "only ingress is supported");
		if (attr->egress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				 attr, "egress is not supported");
		if (size < mnl_nlmsg_size(sizeof(*tcm)))
			goto error_nobufs;
		nlh = mnl_nlmsg_put_header(buf);
		nlh->nlmsg_type = 0;
		nlh->nlmsg_flags = 0;
		nlh->nlmsg_seq = 0;
		tcm = mnl_nlmsg_put_extra_header(nlh, sizeof(*tcm));
		tcm->tcm_family = AF_UNSPEC;
		tcm->tcm_ifindex = ptoi[0].ifindex;
		/*
		 * Let kernel pick a handle by default. A predictable handle
		 * can be set by the caller on the resulting buffer through
		 * mlx5_nl_flow_brand().
		 */
		tcm->tcm_handle = 0;
		tcm->tcm_parent = TC_H_MAKE(TC_H_INGRESS, TC_H_MIN_INGRESS);
		/*
		 * Priority cannot be zero to prevent the kernel from
		 * picking one automatically.
		 */
		tcm->tcm_info = TC_H_MAKE((attr->priority + 1) << 16,
					  RTE_BE16(ETH_P_ALL));
		break;
	case PATTERN:
		if (!mnl_attr_put_strz_check(buf, size, TCA_KIND, "flower"))
			goto error_nobufs;
		na_flower = mnl_attr_nest_start_check(buf, size, TCA_OPTIONS);
		if (!na_flower)
			goto error_nobufs;
		if (!mnl_attr_put_u32_check(buf, size, TCA_FLOWER_FLAGS,
					    TCA_CLS_FLAGS_SKIP_SW))
			goto error_nobufs;
		break;
	case ITEM_VOID:
		if (item->type != RTE_FLOW_ITEM_TYPE_VOID)
			goto trans;
		++item;
		break;
	case ITEM_PORT_ID:
		if (item->type != RTE_FLOW_ITEM_TYPE_PORT_ID)
			goto trans;
		mask.port_id = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_port_id_mask,
			 &mlx5_nl_flow_mask_supported.port_id,
			 &mlx5_nl_flow_mask_empty.port_id,
			 sizeof(mlx5_nl_flow_mask_supported.port_id), error);
		if (!mask.port_id)
			return -rte_errno;
		if (mask.port_id == &mlx5_nl_flow_mask_empty.port_id) {
			in_port_id_set = 1;
			++item;
			break;
		}
		spec.port_id = item->spec;
		if (mask.port_id->id && mask.port_id->id != 0xffffffff)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
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
				(error, ENODEV, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				 spec.port_id,
				 "missing data to convert port ID to ifindex");
		tcm = mnl_nlmsg_get_payload(buf);
		if (in_port_id_set &&
		    ptoi[i].ifindex != (unsigned int)tcm->tcm_ifindex)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				 spec.port_id,
				 "cannot match traffic for several port IDs"
				 " through a single flow rule");
		tcm->tcm_ifindex = ptoi[i].ifindex;
		in_port_id_set = 1;
		++item;
		break;
	case ITEM_ETH:
		if (item->type != RTE_FLOW_ITEM_TYPE_ETH)
			goto trans;
		mask.eth = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_eth_mask,
			 &mlx5_nl_flow_mask_supported.eth,
			 &mlx5_nl_flow_mask_empty.eth,
			 sizeof(mlx5_nl_flow_mask_supported.eth), error);
		if (!mask.eth)
			return -rte_errno;
		if (mask.eth == &mlx5_nl_flow_mask_empty.eth) {
			++item;
			break;
		}
		spec.eth = item->spec;
		if (mask.eth->type && mask.eth->type != RTE_BE16(0xffff))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.eth,
				 "no support for partial mask on"
				 " \"type\" field");
		if (mask.eth->type) {
			if (!mnl_attr_put_u16_check(buf, size,
						    TCA_FLOWER_KEY_ETH_TYPE,
						    spec.eth->type))
				goto error_nobufs;
			eth_type_set = 1;
		}
		if ((!is_zero_ether_addr(&mask.eth->dst) &&
		     (!mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_ETH_DST,
					  ETHER_ADDR_LEN,
					  spec.eth->dst.addr_bytes) ||
		      !mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_ETH_DST_MASK,
					  ETHER_ADDR_LEN,
					  mask.eth->dst.addr_bytes))) ||
		    (!is_zero_ether_addr(&mask.eth->src) &&
		     (!mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_ETH_SRC,
					  ETHER_ADDR_LEN,
					  spec.eth->src.addr_bytes) ||
		      !mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_ETH_SRC_MASK,
					  ETHER_ADDR_LEN,
					  mask.eth->src.addr_bytes))))
			goto error_nobufs;
		++item;
		break;
	case ITEM_VLAN:
		if (item->type != RTE_FLOW_ITEM_TYPE_VLAN)
			goto trans;
		mask.vlan = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_vlan_mask,
			 &mlx5_nl_flow_mask_supported.vlan,
			 &mlx5_nl_flow_mask_empty.vlan,
			 sizeof(mlx5_nl_flow_mask_supported.vlan), error);
		if (!mask.vlan)
			return -rte_errno;
		if (!eth_type_set &&
		    !mnl_attr_put_u16_check(buf, size,
					    TCA_FLOWER_KEY_ETH_TYPE,
					    RTE_BE16(ETH_P_8021Q)))
			goto error_nobufs;
		eth_type_set = 1;
		vlan_present = 1;
		if (mask.vlan == &mlx5_nl_flow_mask_empty.vlan) {
			++item;
			break;
		}
		spec.vlan = item->spec;
		if ((mask.vlan->tci & RTE_BE16(0xe000) &&
		     (mask.vlan->tci & RTE_BE16(0xe000)) != RTE_BE16(0xe000)) ||
		    (mask.vlan->tci & RTE_BE16(0x0fff) &&
		     (mask.vlan->tci & RTE_BE16(0x0fff)) != RTE_BE16(0x0fff)) ||
		    (mask.vlan->inner_type &&
		     mask.vlan->inner_type != RTE_BE16(0xffff)))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.vlan,
				 "no support for partial masks on"
				 " \"tci\" (PCP and VID parts) and"
				 " \"inner_type\" fields");
		if (mask.vlan->inner_type) {
			if (!mnl_attr_put_u16_check
			    (buf, size, TCA_FLOWER_KEY_VLAN_ETH_TYPE,
			     spec.vlan->inner_type))
				goto error_nobufs;
			vlan_eth_type_set = 1;
		}
		if ((mask.vlan->tci & RTE_BE16(0xe000) &&
		     !mnl_attr_put_u8_check
		     (buf, size, TCA_FLOWER_KEY_VLAN_PRIO,
		      (rte_be_to_cpu_16(spec.vlan->tci) >> 13) & 0x7)) ||
		    (mask.vlan->tci & RTE_BE16(0x0fff) &&
		     !mnl_attr_put_u16_check
		     (buf, size, TCA_FLOWER_KEY_VLAN_ID,
		      rte_be_to_cpu_16(spec.vlan->tci & RTE_BE16(0x0fff)))))
			goto error_nobufs;
		++item;
		break;
	case ITEM_IPV4:
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4)
			goto trans;
		mask.ipv4 = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_ipv4_mask,
			 &mlx5_nl_flow_mask_supported.ipv4,
			 &mlx5_nl_flow_mask_empty.ipv4,
			 sizeof(mlx5_nl_flow_mask_supported.ipv4), error);
		if (!mask.ipv4)
			return -rte_errno;
		if ((!eth_type_set || !vlan_eth_type_set) &&
		    !mnl_attr_put_u16_check(buf, size,
					    vlan_present ?
					    TCA_FLOWER_KEY_VLAN_ETH_TYPE :
					    TCA_FLOWER_KEY_ETH_TYPE,
					    RTE_BE16(ETH_P_IP)))
			goto error_nobufs;
		eth_type_set = 1;
		vlan_eth_type_set = 1;
		if (mask.ipv4 == &mlx5_nl_flow_mask_empty.ipv4) {
			++item;
			break;
		}
		spec.ipv4 = item->spec;
		if (mask.ipv4->hdr.next_proto_id &&
		    mask.ipv4->hdr.next_proto_id != 0xff)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.ipv4,
				 "no support for partial mask on"
				 " \"hdr.next_proto_id\" field");
		if (mask.ipv4->hdr.next_proto_id) {
			if (!mnl_attr_put_u8_check
			    (buf, size, TCA_FLOWER_KEY_IP_PROTO,
			     spec.ipv4->hdr.next_proto_id))
				goto error_nobufs;
			ip_proto_set = 1;
		}
		if ((mask.ipv4->hdr.src_addr &&
		     (!mnl_attr_put_u32_check(buf, size,
					      TCA_FLOWER_KEY_IPV4_SRC,
					      spec.ipv4->hdr.src_addr) ||
		      !mnl_attr_put_u32_check(buf, size,
					      TCA_FLOWER_KEY_IPV4_SRC_MASK,
					      mask.ipv4->hdr.src_addr))) ||
		    (mask.ipv4->hdr.dst_addr &&
		     (!mnl_attr_put_u32_check(buf, size,
					      TCA_FLOWER_KEY_IPV4_DST,
					      spec.ipv4->hdr.dst_addr) ||
		      !mnl_attr_put_u32_check(buf, size,
					      TCA_FLOWER_KEY_IPV4_DST_MASK,
					      mask.ipv4->hdr.dst_addr))))
			goto error_nobufs;
		++item;
		break;
	case ITEM_IPV6:
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV6)
			goto trans;
		mask.ipv6 = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_ipv6_mask,
			 &mlx5_nl_flow_mask_supported.ipv6,
			 &mlx5_nl_flow_mask_empty.ipv6,
			 sizeof(mlx5_nl_flow_mask_supported.ipv6), error);
		if (!mask.ipv6)
			return -rte_errno;
		if ((!eth_type_set || !vlan_eth_type_set) &&
		    !mnl_attr_put_u16_check(buf, size,
					    vlan_present ?
					    TCA_FLOWER_KEY_VLAN_ETH_TYPE :
					    TCA_FLOWER_KEY_ETH_TYPE,
					    RTE_BE16(ETH_P_IPV6)))
			goto error_nobufs;
		eth_type_set = 1;
		vlan_eth_type_set = 1;
		if (mask.ipv6 == &mlx5_nl_flow_mask_empty.ipv6) {
			++item;
			break;
		}
		spec.ipv6 = item->spec;
		if (mask.ipv6->hdr.proto && mask.ipv6->hdr.proto != 0xff)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.ipv6,
				 "no support for partial mask on"
				 " \"hdr.proto\" field");
		if (mask.ipv6->hdr.proto) {
			if (!mnl_attr_put_u8_check
			    (buf, size, TCA_FLOWER_KEY_IP_PROTO,
			     spec.ipv6->hdr.proto))
				goto error_nobufs;
			ip_proto_set = 1;
		}
		if ((!IN6_IS_ADDR_UNSPECIFIED(mask.ipv6->hdr.src_addr) &&
		     (!mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_IPV6_SRC,
					  sizeof(spec.ipv6->hdr.src_addr),
					  spec.ipv6->hdr.src_addr) ||
		      !mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_IPV6_SRC_MASK,
					  sizeof(mask.ipv6->hdr.src_addr),
					  mask.ipv6->hdr.src_addr))) ||
		    (!IN6_IS_ADDR_UNSPECIFIED(mask.ipv6->hdr.dst_addr) &&
		     (!mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_IPV6_DST,
					  sizeof(spec.ipv6->hdr.dst_addr),
					  spec.ipv6->hdr.dst_addr) ||
		      !mnl_attr_put_check(buf, size,
					  TCA_FLOWER_KEY_IPV6_DST_MASK,
					  sizeof(mask.ipv6->hdr.dst_addr),
					  mask.ipv6->hdr.dst_addr))))
			goto error_nobufs;
		++item;
		break;
	case ITEM_TCP:
		if (item->type != RTE_FLOW_ITEM_TYPE_TCP)
			goto trans;
		mask.tcp = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_tcp_mask,
			 &mlx5_nl_flow_mask_supported.tcp,
			 &mlx5_nl_flow_mask_empty.tcp,
			 sizeof(mlx5_nl_flow_mask_supported.tcp), error);
		if (!mask.tcp)
			return -rte_errno;
		if (!ip_proto_set &&
		    !mnl_attr_put_u8_check(buf, size,
					   TCA_FLOWER_KEY_IP_PROTO,
					   IPPROTO_TCP))
			goto error_nobufs;
		if (mask.tcp == &mlx5_nl_flow_mask_empty.tcp) {
			++item;
			break;
		}
		spec.tcp = item->spec;
		if ((mask.tcp->hdr.src_port &&
		     mask.tcp->hdr.src_port != RTE_BE16(0xffff)) ||
		    (mask.tcp->hdr.dst_port &&
		     mask.tcp->hdr.dst_port != RTE_BE16(0xffff)))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.tcp,
				 "no support for partial masks on"
				 " \"hdr.src_port\" and \"hdr.dst_port\""
				 " fields");
		if ((mask.tcp->hdr.src_port &&
		     (!mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_TCP_SRC,
					      spec.tcp->hdr.src_port) ||
		      !mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_TCP_SRC_MASK,
					      mask.tcp->hdr.src_port))) ||
		    (mask.tcp->hdr.dst_port &&
		     (!mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_TCP_DST,
					      spec.tcp->hdr.dst_port) ||
		      !mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_TCP_DST_MASK,
					      mask.tcp->hdr.dst_port))))
			goto error_nobufs;
		++item;
		break;
	case ITEM_UDP:
		if (item->type != RTE_FLOW_ITEM_TYPE_UDP)
			goto trans;
		mask.udp = mlx5_nl_flow_item_mask
			(item, &rte_flow_item_udp_mask,
			 &mlx5_nl_flow_mask_supported.udp,
			 &mlx5_nl_flow_mask_empty.udp,
			 sizeof(mlx5_nl_flow_mask_supported.udp), error);
		if (!mask.udp)
			return -rte_errno;
		if (!ip_proto_set &&
		    !mnl_attr_put_u8_check(buf, size,
					   TCA_FLOWER_KEY_IP_PROTO,
					   IPPROTO_UDP))
			goto error_nobufs;
		if (mask.udp == &mlx5_nl_flow_mask_empty.udp) {
			++item;
			break;
		}
		spec.udp = item->spec;
		if ((mask.udp->hdr.src_port &&
		     mask.udp->hdr.src_port != RTE_BE16(0xffff)) ||
		    (mask.udp->hdr.dst_port &&
		     mask.udp->hdr.dst_port != RTE_BE16(0xffff)))
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM_MASK,
				 mask.udp,
				 "no support for partial masks on"
				 " \"hdr.src_port\" and \"hdr.dst_port\""
				 " fields");
		if ((mask.udp->hdr.src_port &&
		     (!mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_UDP_SRC,
					      spec.udp->hdr.src_port) ||
		      !mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_UDP_SRC_MASK,
					      mask.udp->hdr.src_port))) ||
		    (mask.udp->hdr.dst_port &&
		     (!mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_UDP_DST,
					      spec.udp->hdr.dst_port) ||
		      !mnl_attr_put_u16_check(buf, size,
					      TCA_FLOWER_KEY_UDP_DST_MASK,
					      mask.udp->hdr.dst_port))))
			goto error_nobufs;
		++item;
		break;
	case ACTIONS:
		if (item->type != RTE_FLOW_ITEM_TYPE_END)
			goto trans;
		assert(na_flower);
		assert(!na_flower_act);
		na_flower_act =
			mnl_attr_nest_start_check(buf, size, TCA_FLOWER_ACT);
		if (!na_flower_act)
			goto error_nobufs;
		act_index_cur = 1;
		break;
	case ACTION_VOID:
		if (action->type != RTE_FLOW_ACTION_TYPE_VOID)
			goto trans;
		++action;
		break;
	case ACTION_PORT_ID:
		if (action->type != RTE_FLOW_ACTION_TYPE_PORT_ID)
			goto trans;
		conf.port_id = action->conf;
		if (conf.port_id->original)
			i = 0;
		else
			for (i = 0; ptoi[i].ifindex; ++i)
				if (ptoi[i].port_id == conf.port_id->id)
					break;
		if (!ptoi[i].ifindex)
			return rte_flow_error_set
				(error, ENODEV, RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 conf.port_id,
				 "missing data to convert port ID to ifindex");
		act_index =
			mnl_attr_nest_start_check(buf, size, act_index_cur++);
		if (!act_index ||
		    !mnl_attr_put_strz_check(buf, size, TCA_ACT_KIND, "mirred"))
			goto error_nobufs;
		act = mnl_attr_nest_start_check(buf, size, TCA_ACT_OPTIONS);
		if (!act)
			goto error_nobufs;
		if (!mnl_attr_put_check(buf, size, TCA_MIRRED_PARMS,
					sizeof(struct tc_mirred),
					&(struct tc_mirred){
						.action = TC_ACT_STOLEN,
						.eaction = TCA_EGRESS_REDIR,
						.ifindex = ptoi[i].ifindex,
					}))
			goto error_nobufs;
		mnl_attr_nest_end(buf, act);
		mnl_attr_nest_end(buf, act_index);
		++action;
		break;
	case ACTION_DROP:
		if (action->type != RTE_FLOW_ACTION_TYPE_DROP)
			goto trans;
		act_index =
			mnl_attr_nest_start_check(buf, size, act_index_cur++);
		if (!act_index ||
		    !mnl_attr_put_strz_check(buf, size, TCA_ACT_KIND, "gact"))
			goto error_nobufs;
		act = mnl_attr_nest_start_check(buf, size, TCA_ACT_OPTIONS);
		if (!act)
			goto error_nobufs;
		if (!mnl_attr_put_check(buf, size, TCA_GACT_PARMS,
					sizeof(struct tc_gact),
					&(struct tc_gact){
						.action = TC_ACT_SHOT,
					}))
			goto error_nobufs;
		mnl_attr_nest_end(buf, act);
		mnl_attr_nest_end(buf, act_index);
		++action;
		break;
	case ACTION_OF_POP_VLAN:
		if (action->type != RTE_FLOW_ACTION_TYPE_OF_POP_VLAN)
			goto trans;
		conf.of_push_vlan = NULL;
		i = TCA_VLAN_ACT_POP;
		goto action_of_vlan;
	case ACTION_OF_PUSH_VLAN:
		if (action->type != RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN)
			goto trans;
		conf.of_push_vlan = action->conf;
		i = TCA_VLAN_ACT_PUSH;
		goto action_of_vlan;
	case ACTION_OF_SET_VLAN_VID:
		if (action->type != RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)
			goto trans;
		conf.of_set_vlan_vid = action->conf;
		if (na_vlan_id)
			goto override_na_vlan_id;
		i = TCA_VLAN_ACT_MODIFY;
		goto action_of_vlan;
	case ACTION_OF_SET_VLAN_PCP:
		if (action->type != RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP)
			goto trans;
		conf.of_set_vlan_pcp = action->conf;
		if (na_vlan_priority)
			goto override_na_vlan_priority;
		i = TCA_VLAN_ACT_MODIFY;
		goto action_of_vlan;
action_of_vlan:
		act_index =
			mnl_attr_nest_start_check(buf, size, act_index_cur++);
		if (!act_index ||
		    !mnl_attr_put_strz_check(buf, size, TCA_ACT_KIND, "vlan"))
			goto error_nobufs;
		act = mnl_attr_nest_start_check(buf, size, TCA_ACT_OPTIONS);
		if (!act)
			goto error_nobufs;
		if (!mnl_attr_put_check(buf, size, TCA_VLAN_PARMS,
					sizeof(struct tc_vlan),
					&(struct tc_vlan){
						.action = TC_ACT_PIPE,
						.v_action = i,
					}))
			goto error_nobufs;
		if (i == TCA_VLAN_ACT_POP) {
			mnl_attr_nest_end(buf, act);
			mnl_attr_nest_end(buf, act_index);
			++action;
			break;
		}
		if (i == TCA_VLAN_ACT_PUSH &&
		    !mnl_attr_put_u16_check(buf, size,
					    TCA_VLAN_PUSH_VLAN_PROTOCOL,
					    conf.of_push_vlan->ethertype))
			goto error_nobufs;
		na_vlan_id = mnl_nlmsg_get_payload_tail(buf);
		if (!mnl_attr_put_u16_check(buf, size, TCA_VLAN_PAD, 0))
			goto error_nobufs;
		na_vlan_priority = mnl_nlmsg_get_payload_tail(buf);
		if (!mnl_attr_put_u8_check(buf, size, TCA_VLAN_PAD, 0))
			goto error_nobufs;
		mnl_attr_nest_end(buf, act);
		mnl_attr_nest_end(buf, act_index);
		if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) {
override_na_vlan_id:
			na_vlan_id->nla_type = TCA_VLAN_PUSH_VLAN_ID;
			*(uint16_t *)mnl_attr_get_payload(na_vlan_id) =
				rte_be_to_cpu_16
				(conf.of_set_vlan_vid->vlan_vid);
		} else if (action->type ==
			   RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) {
override_na_vlan_priority:
			na_vlan_priority->nla_type =
				TCA_VLAN_PUSH_VLAN_PRIORITY;
			*(uint8_t *)mnl_attr_get_payload(na_vlan_priority) =
				conf.of_set_vlan_pcp->vlan_pcp;
		}
		++action;
		break;
	case END:
		if (item->type != RTE_FLOW_ITEM_TYPE_END ||
		    action->type != RTE_FLOW_ACTION_TYPE_END)
			goto trans;
		if (na_flower_act)
			mnl_attr_nest_end(buf, na_flower_act);
		if (na_flower)
			mnl_attr_nest_end(buf, na_flower);
		nlh = buf;
		return nlh->nlmsg_len;
	}
	back = trans;
	trans = mlx5_nl_flow_trans[trans[n - 1]];
	n = 0;
	goto trans;
error_nobufs:
	if (buf != buf_tmp) {
		buf = buf_tmp;
		size = sizeof(buf_tmp);
		goto init;
	}
	return rte_flow_error_set
		(error, ENOBUFS, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "generated TC message is too large");
}

/**
 * Brand rtnetlink buffer with unique handle.
 *
 * This handle should be unique for a given network interface to avoid
 * collisions.
 *
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param handle
 *   Unique 32-bit handle to use.
 */
void
mlx5_nl_flow_brand(void *buf, uint32_t handle)
{
	struct tcmsg *tcm = mnl_nlmsg_get_payload(buf);

	tcm->tcm_handle = handle;
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
mlx5_nl_flow_nl_ack(struct mnl_socket *nl, struct nlmsghdr *nlh)
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
	if (!ret)
		return 0;
	rte_errno = errno;
	return -rte_errno;
}

/**
 * Create a Netlink flow rule.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_create(struct mnl_socket *nl, void *buf,
		    struct rte_flow_error *error)
{
	struct nlmsghdr *nlh = buf;

	nlh->nlmsg_type = RTM_NEWTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
	if (!mlx5_nl_flow_nl_ack(nl, nlh))
		return 0;
	return rte_flow_error_set
		(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "netlink: failed to create TC flow rule");
}

/**
 * Destroy a Netlink flow rule.
 *
 * @param nl
 *   Libmnl socket to use.
 * @param buf
 *   Flow rule buffer previously initialized by mlx5_nl_flow_transpose().
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_nl_flow_destroy(struct mnl_socket *nl, void *buf,
		     struct rte_flow_error *error)
{
	struct nlmsghdr *nlh = buf;

	nlh->nlmsg_type = RTM_DELTFILTER;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	if (!mlx5_nl_flow_nl_ack(nl, nlh))
		return 0;
	return rte_flow_error_set
		(error, errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
		 "netlink: failed to destroy TC flow rule");
}

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
mlx5_nl_flow_init(struct mnl_socket *nl, unsigned int ifindex,
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
	if (mlx5_nl_flow_nl_ack(nl, nlh) &&
	    rte_errno != EINVAL && rte_errno != ENOENT)
		return rte_flow_error_set
			(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "netlink: failed to remove ingress qdisc");
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
	if (mlx5_nl_flow_nl_ack(nl, nlh))
		return rte_flow_error_set
			(error, rte_errno, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			 NULL, "netlink: failed to create ingress qdisc");
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
mlx5_nl_flow_socket_create(void)
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
 */
void
mlx5_nl_flow_socket_destroy(struct mnl_socket *nl)
{
	mnl_socket_close(nl);
}
