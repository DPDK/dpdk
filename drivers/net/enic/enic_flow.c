/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 */

#include <errno.h>
#include <stdint.h>
#include <rte_log.h>
#include <ethdev_driver.h>
#include <rte_flow_driver.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_nic.h"

/*
 * Common arguments passed to copy_item functions. Use this structure
 * so we can easily add new arguments.
 * item: Item specification.
 * filter: Partially filled in NIC filter structure.
 * inner_ofst: If zero, this is an outer header. If non-zero, this is
 *   the offset into L5 where the header begins.
 * l2_proto_off: offset to EtherType eth or vlan header.
 * l3_proto_off: offset to next protocol field in IPv4 or 6 header.
 */
struct copy_item_args {
	const struct rte_flow_item *item;
	struct filter_v2 *filter;
	uint8_t *inner_ofst;
	uint8_t l2_proto_off;
	uint8_t l3_proto_off;
	struct enic *enic;
};

/* functions for copying items into enic filters */
typedef int (enic_copy_item_fn)(struct copy_item_args *arg);

/** Info about how to copy items into enic filters. */
struct enic_items {
	/** Function for copying and validating an item. */
	enic_copy_item_fn *copy_item;
	/** List of valid previous items. */
	const enum rte_flow_item_type * const prev_items;
	/** True if it's OK for this item to be the first item. For some NIC
	 * versions, it's invalid to start the stack above layer 3.
	 */
	const uint8_t valid_start_item;
	/* Inner packet version of copy_item. */
	enic_copy_item_fn *inner_copy_item;
};

/** Filtering capabilities for various NIC and firmware versions. */
struct enic_filter_cap {
	/** list of valid items and their handlers and attributes. */
	const struct enic_items *item_info;
	/* Max type in the above list, used to detect unsupported types */
	enum rte_flow_item_type max_item_type;
};

/* functions for copying flow actions into enic actions */
typedef int (copy_action_fn)(struct enic *enic,
			     const struct rte_flow_action actions[],
			     struct filter_action_v2 *enic_action);

/** Action capabilities for various NICs. */
struct enic_action_cap {
	/** list of valid actions */
	const enum rte_flow_action_type *actions;
	/** copy function for a particular NIC */
	copy_action_fn *copy_fn;
};

/* Forward declarations */
static enic_copy_item_fn enic_copy_item_ipv4_v1;
static enic_copy_item_fn enic_copy_item_udp_v1;
static enic_copy_item_fn enic_copy_item_tcp_v1;
static enic_copy_item_fn enic_copy_item_raw_v2;
static enic_copy_item_fn enic_copy_item_eth_v2;
static enic_copy_item_fn enic_copy_item_vlan_v2;
static enic_copy_item_fn enic_copy_item_ipv4_v2;
static enic_copy_item_fn enic_copy_item_ipv6_v2;
static enic_copy_item_fn enic_copy_item_udp_v2;
static enic_copy_item_fn enic_copy_item_tcp_v2;
static enic_copy_item_fn enic_copy_item_sctp_v2;
static enic_copy_item_fn enic_copy_item_vxlan_v2;
static enic_copy_item_fn enic_copy_item_inner_eth_v2;
static enic_copy_item_fn enic_copy_item_inner_vlan_v2;
static enic_copy_item_fn enic_copy_item_inner_ipv4_v2;
static enic_copy_item_fn enic_copy_item_inner_ipv6_v2;
static enic_copy_item_fn enic_copy_item_inner_udp_v2;
static enic_copy_item_fn enic_copy_item_inner_tcp_v2;
static copy_action_fn enic_copy_action_v1;
static copy_action_fn enic_copy_action_v2;

/**
 * Legacy NICs or NICs with outdated firmware. Only 5-tuple perfect match
 * is supported.
 */
static const struct enic_items enic_items_v1[] = {
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_copy_item_ipv4_v1,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v1,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v1,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
};

/**
 * NICs have Advanced Filters capability but they are disabled. This means
 * that layer 3 must be specified.
 */
static const struct enic_items enic_items_v2[] = {
	[RTE_FLOW_ITEM_TYPE_RAW] = {
		.copy_item = enic_copy_item_raw_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.copy_item = enic_copy_item_eth_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_eth_v2,
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.copy_item = enic_copy_item_vlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_vlan_v2,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_copy_item_ipv4_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_ipv4_v2,
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.copy_item = enic_copy_item_ipv6_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_ipv6_v2,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_udp_v2,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_tcp_v2,
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.copy_item = enic_copy_item_sctp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.copy_item = enic_copy_item_vxlan_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
};

/** NICs with Advanced filters enabled */
static const struct enic_items enic_items_v3[] = {
	[RTE_FLOW_ITEM_TYPE_RAW] = {
		.copy_item = enic_copy_item_raw_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.copy_item = enic_copy_item_eth_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_eth_v2,
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.copy_item = enic_copy_item_vlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_vlan_v2,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_copy_item_ipv4_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_ipv4_v2,
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.copy_item = enic_copy_item_ipv6_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_ipv6_v2,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_udp_v2,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = enic_copy_item_inner_tcp_v2,
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.copy_item = enic_copy_item_sctp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.copy_item = enic_copy_item_vxlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
		.inner_copy_item = NULL,
	},
};

/** Filtering capabilities indexed this NICs supported filter type. */
static const struct enic_filter_cap enic_filter_cap[] = {
	[FILTER_IPV4_5TUPLE] = {
		.item_info = enic_items_v1,
		.max_item_type = RTE_FLOW_ITEM_TYPE_TCP,
	},
	[FILTER_USNIC_IP] = {
		.item_info = enic_items_v2,
		.max_item_type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
	[FILTER_DPDK_1] = {
		.item_info = enic_items_v3,
		.max_item_type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
};

/** Supported actions for older NICs */
static const enum rte_flow_action_type enic_supported_actions_v1[] = {
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_END,
};

/** Supported actions for newer NICs */
static const enum rte_flow_action_type enic_supported_actions_v2_id[] = {
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_MARK,
	RTE_FLOW_ACTION_TYPE_FLAG,
	RTE_FLOW_ACTION_TYPE_RSS,
	RTE_FLOW_ACTION_TYPE_PASSTHRU,
	RTE_FLOW_ACTION_TYPE_END,
};

static const enum rte_flow_action_type enic_supported_actions_v2_drop[] = {
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_MARK,
	RTE_FLOW_ACTION_TYPE_FLAG,
	RTE_FLOW_ACTION_TYPE_DROP,
	RTE_FLOW_ACTION_TYPE_RSS,
	RTE_FLOW_ACTION_TYPE_PASSTHRU,
	RTE_FLOW_ACTION_TYPE_END,
};

/** Action capabilities indexed by NIC version information */
static const struct enic_action_cap enic_action_cap[] = {
	[FILTER_ACTION_RQ_STEERING_FLAG] = {
		.actions = enic_supported_actions_v1,
		.copy_fn = enic_copy_action_v1,
	},
	[FILTER_ACTION_FILTER_ID_FLAG] = {
		.actions = enic_supported_actions_v2_id,
		.copy_fn = enic_copy_action_v2,
	},
	[FILTER_ACTION_DROP_FLAG] = {
		.actions = enic_supported_actions_v2_drop,
		.copy_fn = enic_copy_action_v2,
	},
};

static int
mask_exact_match(const uint8_t *supported, const uint8_t *supplied,
		 unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		if (supported[i] != supplied[i])
			return 0;
	}
	return 1;
}

static int
enic_copy_item_ipv4_v1(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct rte_ipv4_hdr supported_mask = {
		.src_addr = 0xffffffff,
		.dst_addr = 0xffffffff,
	};

	ENICPMD_FUNC_TRACE();

	if (!mask)
		mask = &rte_flow_item_ipv4_mask;

	/* This is an exact match filter, both fields must be set */
	if (!spec || !spec->hdr.src_addr || !spec->hdr.dst_addr) {
		ENICPMD_LOG(ERR, "IPv4 exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the supplied mask exactly matches capability */
	if (!mask_exact_match((const uint8_t *)&supported_mask,
			      (const uint8_t *)item->mask, sizeof(*mask))) {
		ENICPMD_LOG(ERR, "IPv4 exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_addr = spec->hdr.src_addr;
	enic_5tup->dst_addr = spec->hdr.dst_addr;

	return 0;
}

static int
enic_copy_item_udp_v1(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct rte_udp_hdr supported_mask = {
		.src_port = 0xffff,
		.dst_port = 0xffff,
	};

	ENICPMD_FUNC_TRACE();

	if (!mask)
		mask = &rte_flow_item_udp_mask;

	/* This is an exact match filter, both ports must be set */
	if (!spec || !spec->hdr.src_port || !spec->hdr.dst_port) {
		ENICPMD_LOG(ERR, "UDP exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the supplied mask exactly matches capability */
	if (!mask_exact_match((const uint8_t *)&supported_mask,
			      (const uint8_t *)item->mask, sizeof(*mask))) {
		ENICPMD_LOG(ERR, "UDP exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_port = spec->hdr.src_port;
	enic_5tup->dst_port = spec->hdr.dst_port;
	enic_5tup->protocol = PROTO_UDP;

	return 0;
}

static int
enic_copy_item_tcp_v1(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct rte_tcp_hdr supported_mask = {
		.src_port = 0xffff,
		.dst_port = 0xffff,
	};

	ENICPMD_FUNC_TRACE();

	if (!mask)
		mask = &rte_flow_item_tcp_mask;

	/* This is an exact match filter, both ports must be set */
	if (!spec || !spec->hdr.src_port || !spec->hdr.dst_port) {
		ENICPMD_LOG(ERR, "TCPIPv4 exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the supplied mask exactly matches capability */
	if (!mask_exact_match((const uint8_t *)&supported_mask,
			     (const uint8_t *)item->mask, sizeof(*mask))) {
		ENICPMD_LOG(ERR, "TCP exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_port = spec->hdr.src_port;
	enic_5tup->dst_port = spec->hdr.dst_port;
	enic_5tup->protocol = PROTO_TCP;

	return 0;
}

/*
 * The common 'copy' function for all inner packet patterns. Patterns are
 * first appended to the L5 pattern buffer. Then, since the NIC filter
 * API has no special support for inner packet matching at the moment,
 * we set EtherType and IP proto as necessary.
 */
static int
copy_inner_common(struct filter_generic_1 *gp, uint8_t *inner_ofst,
		  const void *val, const void *mask, uint8_t val_size,
		  uint8_t proto_off, uint16_t proto_val, uint8_t proto_size)
{
	uint8_t *l5_mask, *l5_val;
	uint8_t start_off;

	/* No space left in the L5 pattern buffer. */
	start_off = *inner_ofst;
	if ((start_off + val_size) > FILTER_GENERIC_1_KEY_LEN)
		return ENOTSUP;
	l5_mask = gp->layer[FILTER_GENERIC_1_L5].mask;
	l5_val = gp->layer[FILTER_GENERIC_1_L5].val;
	/* Copy the pattern into the L5 buffer. */
	if (val) {
		memcpy(l5_mask + start_off, mask, val_size);
		memcpy(l5_val + start_off, val, val_size);
	}
	/* Set the protocol field in the previous header. */
	if (proto_off) {
		void *m, *v;

		m = l5_mask + proto_off;
		v = l5_val + proto_off;
		if (proto_size == 1) {
			*(uint8_t *)m = 0xff;
			*(uint8_t *)v = (uint8_t)proto_val;
		} else if (proto_size == 2) {
			*(uint16_t *)m = 0xffff;
			*(uint16_t *)v = proto_val;
		}
	}
	/* All inner headers land in L5 buffer even if their spec is null. */
	*inner_ofst += val_size;
	return 0;
}

static int
enic_copy_item_inner_eth_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_eth_mask;
	arg->l2_proto_off = *off + offsetof(struct rte_ether_hdr, ether_type);
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_ether_hdr),
		0 /* no previous protocol */, 0, 0);
}

static int
enic_copy_item_inner_vlan_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;
	uint8_t eth_type_off;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_vlan_mask;
	/* Append vlan header to L5 and set ether type = TPID */
	eth_type_off = arg->l2_proto_off;
	arg->l2_proto_off = *off + offsetof(struct rte_vlan_hdr, eth_proto);
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_vlan_hdr),
		eth_type_off, rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN), 2);
}

static int
enic_copy_item_inner_ipv4_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
	/* Append ipv4 header to L5 and set ether type = ipv4 */
	arg->l3_proto_off = *off + offsetof(struct rte_ipv4_hdr, next_proto_id);
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_ipv4_hdr),
		arg->l2_proto_off, rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4), 2);
}

static int
enic_copy_item_inner_ipv6_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	/* Append ipv6 header to L5 and set ether type = ipv6 */
	arg->l3_proto_off = *off + offsetof(struct rte_ipv6_hdr, proto);
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_ipv6_hdr),
		arg->l2_proto_off, rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6), 2);
}

static int
enic_copy_item_inner_udp_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	/* Append udp header to L5 and set ip proto = udp */
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_udp_hdr),
		arg->l3_proto_off, IPPROTO_UDP, 1);
}

static int
enic_copy_item_inner_tcp_v2(struct copy_item_args *arg)
{
	const void *mask = arg->item->mask;
	uint8_t *off = arg->inner_ofst;

	ENICPMD_FUNC_TRACE();
	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	/* Append tcp header to L5 and set ip proto = tcp */
	return copy_inner_common(&arg->filter->u.generic_1, off,
		arg->item->spec, mask, sizeof(struct rte_tcp_hdr),
		arg->l3_proto_off, IPPROTO_TCP, 1);
}

static int
enic_copy_item_eth_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	struct rte_ether_hdr enic_spec;
	struct rte_ether_hdr enic_mask;
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_eth_mask;

	memcpy(enic_spec.dst_addr.addr_bytes, spec->dst.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	memcpy(enic_spec.src_addr.addr_bytes, spec->src.addr_bytes,
	       RTE_ETHER_ADDR_LEN);

	memcpy(enic_mask.dst_addr.addr_bytes, mask->dst.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	memcpy(enic_mask.src_addr.addr_bytes, mask->src.addr_bytes,
	       RTE_ETHER_ADDR_LEN);
	enic_spec.ether_type = spec->type;
	enic_mask.ether_type = mask->type;

	/* outer header */
	memcpy(gp->layer[FILTER_GENERIC_1_L2].mask, &enic_mask,
	       sizeof(struct rte_ether_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L2].val, &enic_spec,
	       sizeof(struct rte_ether_hdr));
	return 0;
}

static int
enic_copy_item_vlan_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;
	struct rte_ether_hdr *eth_mask;
	struct rte_ether_hdr *eth_val;

	ENICPMD_FUNC_TRACE();

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_vlan_mask;

	eth_mask = (void *)gp->layer[FILTER_GENERIC_1_L2].mask;
	eth_val = (void *)gp->layer[FILTER_GENERIC_1_L2].val;
	/* Outer TPID cannot be matched */
	if (eth_mask->ether_type)
		return ENOTSUP;
	/*
	 * For recent models:
	 * When packet matching, the VIC always compares vlan-stripped
	 * L2, regardless of vlan stripping settings. So, the inner type
	 * from vlan becomes the ether type of the eth header.
	 *
	 * Older models w/o hardware vxlan parser have a different
	 * behavior when vlan stripping is disabled. In this case,
	 * vlan tag remains in the L2 buffer.
	 */
	if (!arg->enic->vxlan && !arg->enic->ig_vlan_strip_en) {
		struct rte_vlan_hdr *vlan;

		vlan = (struct rte_vlan_hdr *)(eth_mask + 1);
		vlan->eth_proto = mask->inner_type;
		vlan = (struct rte_vlan_hdr *)(eth_val + 1);
		vlan->eth_proto = spec->inner_type;
	} else {
		eth_mask->ether_type = mask->inner_type;
		eth_val->ether_type = spec->inner_type;
	}
	/* For TCI, use the vlan mask/val fields (little endian). */
	gp->mask_vlan = rte_be_to_cpu_16(mask->tci);
	gp->val_vlan = rte_be_to_cpu_16(spec->tci);
	return 0;
}

static int
enic_copy_item_ipv4_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Match IPv4 */
	gp->mask_flags |= FILTER_GENERIC_1_IPV4;
	gp->val_flags |= FILTER_GENERIC_1_IPV4;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_ipv4_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L3].mask, &mask->hdr,
	       sizeof(struct rte_ipv4_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L3].val, &spec->hdr,
	       sizeof(struct rte_ipv4_hdr));
	return 0;
}

static int
enic_copy_item_ipv6_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Match IPv6 */
	gp->mask_flags |= FILTER_GENERIC_1_IPV6;
	gp->val_flags |= FILTER_GENERIC_1_IPV6;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_ipv6_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L3].mask, &mask->hdr,
	       sizeof(struct rte_ipv6_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L3].val, &spec->hdr,
	       sizeof(struct rte_ipv6_hdr));
	return 0;
}

static int
enic_copy_item_udp_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Match UDP */
	gp->mask_flags |= FILTER_GENERIC_1_UDP;
	gp->val_flags |= FILTER_GENERIC_1_UDP;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_udp_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
	       sizeof(struct rte_udp_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
	       sizeof(struct rte_udp_hdr));
	return 0;
}

static int
enic_copy_item_tcp_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Match TCP */
	gp->mask_flags |= FILTER_GENERIC_1_TCP;
	gp->val_flags |= FILTER_GENERIC_1_TCP;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		return ENOTSUP;

	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
	       sizeof(struct rte_tcp_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
	       sizeof(struct rte_tcp_hdr));
	return 0;
}

static int
enic_copy_item_sctp_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	const struct rte_flow_item_sctp *spec = item->spec;
	const struct rte_flow_item_sctp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;
	uint8_t *ip_proto_mask = NULL;
	uint8_t *ip_proto = NULL;

	ENICPMD_FUNC_TRACE();

	/*
	 * The NIC filter API has no flags for "match sctp", so explicitly set
	 * the protocol number in the IP pattern.
	 */
	if (gp->val_flags & FILTER_GENERIC_1_IPV4) {
		struct rte_ipv4_hdr *ip;
		ip = (struct rte_ipv4_hdr *)gp->layer[FILTER_GENERIC_1_L3].mask;
		ip_proto_mask = &ip->next_proto_id;
		ip = (struct rte_ipv4_hdr *)gp->layer[FILTER_GENERIC_1_L3].val;
		ip_proto = &ip->next_proto_id;
	} else if (gp->val_flags & FILTER_GENERIC_1_IPV6) {
		struct rte_ipv6_hdr *ip;
		ip = (struct rte_ipv6_hdr *)gp->layer[FILTER_GENERIC_1_L3].mask;
		ip_proto_mask = &ip->proto;
		ip = (struct rte_ipv6_hdr *)gp->layer[FILTER_GENERIC_1_L3].val;
		ip_proto = &ip->proto;
	} else {
		/* Need IPv4/IPv6 pattern first */
		return EINVAL;
	}
	*ip_proto = IPPROTO_SCTP;
	*ip_proto_mask = 0xff;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_sctp_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
	       sizeof(struct rte_sctp_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
	       sizeof(struct rte_sctp_hdr));
	return 0;
}

static int
enic_copy_item_vxlan_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	uint8_t *inner_ofst = arg->inner_ofst;
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;
	struct rte_udp_hdr *udp;

	ENICPMD_FUNC_TRACE();

	/*
	 * The NIC filter API has no flags for "match vxlan". Set UDP port to
	 * avoid false positives.
	 */
	gp->mask_flags |= FILTER_GENERIC_1_UDP;
	gp->val_flags |= FILTER_GENERIC_1_UDP;
	udp = (struct rte_udp_hdr *)gp->layer[FILTER_GENERIC_1_L4].mask;
	udp->dst_port = 0xffff;
	udp = (struct rte_udp_hdr *)gp->layer[FILTER_GENERIC_1_L4].val;
	udp->dst_port = RTE_BE16(4789);
	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_vxlan_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L5].mask, mask,
	       sizeof(struct rte_vxlan_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L5].val, spec,
	       sizeof(struct rte_vxlan_hdr));

	*inner_ofst = sizeof(struct rte_vxlan_hdr);
	return 0;
}

/*
 * Copy raw item into version 2 NIC filter. Currently, raw pattern match is
 * very limited. It is intended for matching UDP tunnel header (e.g. vxlan
 * or geneve).
 */
static int
enic_copy_item_raw_v2(struct copy_item_args *arg)
{
	const struct rte_flow_item *item = arg->item;
	struct filter_v2 *enic_filter = arg->filter;
	uint8_t *inner_ofst = arg->inner_ofst;
	const struct rte_flow_item_raw *spec = item->spec;
	const struct rte_flow_item_raw *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	ENICPMD_FUNC_TRACE();

	/* Cannot be used for inner packet */
	if (*inner_ofst)
		return EINVAL;
	/* Need both spec and mask */
	if (!spec || !mask)
		return EINVAL;
	/* Only supports relative with offset 0 */
	if (!spec->relative || spec->offset != 0 || spec->search || spec->limit)
		return EINVAL;
	/* Need non-null pattern that fits within the NIC's filter pattern */
	if (spec->length == 0 ||
	    spec->length + sizeof(struct rte_udp_hdr) > FILTER_GENERIC_1_KEY_LEN ||
	    !spec->pattern || !mask->pattern)
		return EINVAL;
	/*
	 * Mask fields, including length, are often set to zero. Assume that
	 * means "same as spec" to avoid breaking existing apps. If length
	 * is not zero, then it should be >= spec length.
	 *
	 * No more pattern follows this, so append to the L4 layer instead of
	 * L5 to work with both recent and older VICs.
	 */
	if (mask->length != 0 && mask->length < spec->length)
		return EINVAL;
	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask + sizeof(struct rte_udp_hdr),
	       mask->pattern, spec->length);
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val + sizeof(struct rte_udp_hdr),
	       spec->pattern, spec->length);

	return 0;
}

/**
 * Return 1 if current item is valid on top of the previous one.
 *
 * @param prev_item[in]
 *   The item before this one in the pattern or RTE_FLOW_ITEM_TYPE_END if this
 *   is the first item.
 * @param item_info[in]
 *   Info about this item, like valid previous items.
 * @param is_first[in]
 *   True if this the first item in the pattern.
 */
static int
item_stacking_valid(enum rte_flow_item_type prev_item,
		    const struct enic_items *item_info, uint8_t is_first_item)
{
	enum rte_flow_item_type const *allowed_items = item_info->prev_items;

	ENICPMD_FUNC_TRACE();

	for (; *allowed_items != RTE_FLOW_ITEM_TYPE_END; allowed_items++) {
		if (prev_item == *allowed_items)
			return 1;
	}

	/* This is the first item in the stack. Check if that's cool */
	if (is_first_item && item_info->valid_start_item)
		return 1;

	return 0;
}

/*
 * Fix up the L5 layer.. HW vxlan parsing removes vxlan header from L5.
 * Instead it is in L4 following the UDP header. Append the vxlan
 * pattern to L4 (udp) and shift any inner packet pattern in L5.
 */
static void
fixup_l5_layer(struct enic *enic, struct filter_generic_1 *gp,
	       uint8_t inner_ofst)
{
	uint8_t layer[FILTER_GENERIC_1_KEY_LEN];
	uint8_t inner;
	uint8_t vxlan;

	if (!(inner_ofst > 0 && enic->vxlan))
		return;
	ENICPMD_FUNC_TRACE();
	vxlan = sizeof(struct rte_vxlan_hdr);
	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask + sizeof(struct rte_udp_hdr),
	       gp->layer[FILTER_GENERIC_1_L5].mask, vxlan);
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val + sizeof(struct rte_udp_hdr),
	       gp->layer[FILTER_GENERIC_1_L5].val, vxlan);
	inner = inner_ofst - vxlan;
	memset(layer, 0, sizeof(layer));
	memcpy(layer, gp->layer[FILTER_GENERIC_1_L5].mask + vxlan, inner);
	memcpy(gp->layer[FILTER_GENERIC_1_L5].mask, layer, sizeof(layer));
	memset(layer, 0, sizeof(layer));
	memcpy(layer, gp->layer[FILTER_GENERIC_1_L5].val + vxlan, inner);
	memcpy(gp->layer[FILTER_GENERIC_1_L5].val, layer, sizeof(layer));
}

/**
 * Build the internal enic filter structure from the provided pattern. The
 * pattern is validated as the items are copied.
 *
 * @param pattern[in]
 * @param items_info[in]
 *   Info about this NICs item support, like valid previous items.
 * @param enic_filter[out]
 *   NIC specific filters derived from the pattern.
 * @param error[out]
 */
static int
enic_copy_filter(const struct rte_flow_item pattern[],
		 const struct enic_filter_cap *cap,
		 struct enic *enic,
		 struct filter_v2 *enic_filter,
		 struct rte_flow_error *error)
{
	int ret;
	const struct rte_flow_item *item = pattern;
	uint8_t inner_ofst = 0; /* If encapsulated, ofst into L5 */
	enum rte_flow_item_type prev_item;
	const struct enic_items *item_info;
	struct copy_item_args args;
	enic_copy_item_fn *copy_fn;
	uint8_t is_first_item = 1;

	ENICPMD_FUNC_TRACE();

	prev_item = 0;

	args.filter = enic_filter;
	args.inner_ofst = &inner_ofst;
	args.enic = enic;
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		/* Get info about how to validate and copy the item. If NULL
		 * is returned the nic does not support the item.
		 */
		if (item->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		item_info = &cap->item_info[item->type];
		if (item->type > cap->max_item_type ||
		    item_info->copy_item == NULL ||
		    (inner_ofst > 0 && item_info->inner_copy_item == NULL)) {
			rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM,
				NULL, "Unsupported item.");
			return -rte_errno;
		}

		/* check to see if item stacking is valid */
		if (!item_stacking_valid(prev_item, item_info, is_first_item))
			goto stacking_error;

		args.item = item;
		copy_fn = inner_ofst > 0 ? item_info->inner_copy_item :
			item_info->copy_item;
		ret = copy_fn(&args);
		if (ret)
			goto item_not_supported;
		prev_item = item->type;
		is_first_item = 0;
	}
	fixup_l5_layer(enic, &enic_filter->u.generic_1, inner_ofst);

	return 0;

item_not_supported:
	rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_ITEM,
			   NULL, "enic type error");
	return -rte_errno;

stacking_error:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			   item, "stacking error");
	return -rte_errno;
}

/**
 * Build the internal version 1 NIC action structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param enic_action[out]
 *   NIC specific actions derived from the actions.
 * @param error[out]
 */
static int
enic_copy_action_v1(__rte_unused struct enic *enic,
		    const struct rte_flow_action actions[],
		    struct filter_action_v2 *enic_action)
{
	enum { FATE = 1, };
	uint32_t overlap = 0;

	ENICPMD_FUNC_TRACE();

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE: {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;

			if (overlap & FATE)
				return ENOTSUP;
			overlap |= FATE;
			enic_action->rq_idx =
				enic_rte_rq_idx_to_sop_idx(queue->index);
			break;
		}
		default:
			RTE_ASSERT(0);
			break;
		}
	}
	if (!(overlap & FATE))
		return ENOTSUP;
	enic_action->type = FILTER_ACTION_RQ_STEERING;
	return 0;
}

/**
 * Build the internal version 2 NIC action structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param enic_action[out]
 *   NIC specific actions derived from the actions.
 * @param error[out]
 */
static int
enic_copy_action_v2(struct enic *enic,
		    const struct rte_flow_action actions[],
		    struct filter_action_v2 *enic_action)
{
	enum { FATE = 1, MARK = 2, };
	uint32_t overlap = 0;
	bool passthru = false;

	ENICPMD_FUNC_TRACE();

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE: {
			const struct rte_flow_action_queue *queue =
				(const struct rte_flow_action_queue *)
				actions->conf;

			if (overlap & FATE)
				return ENOTSUP;
			overlap |= FATE;
			enic_action->rq_idx =
				enic_rte_rq_idx_to_sop_idx(queue->index);
			enic_action->flags |= FILTER_ACTION_RQ_STEERING_FLAG;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_MARK: {
			const struct rte_flow_action_mark *mark =
				(const struct rte_flow_action_mark *)
				actions->conf;
			if (enic->use_noscatter_vec_rx_handler)
				return ENOTSUP;
			if (overlap & MARK)
				return ENOTSUP;
			overlap |= MARK;
			/*
			 * Map mark ID (32-bit) to filter ID (16-bit):
			 * - Reject values > 16 bits
			 * - Filter ID 0 is reserved for filters that steer
			 *   but not mark. So add 1 to the mark ID to avoid
			 *   using 0.
			 * - Filter ID (ENIC_MAGIC_FILTER_ID = 0xffff) is
			 *   reserved for the "flag" action below.
			 */
			if (mark->id >= ENIC_MAGIC_FILTER_ID - 1)
				return EINVAL;
			enic_action->filter_id = mark->id + 1;
			enic_action->flags |= FILTER_ACTION_FILTER_ID_FLAG;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_FLAG: {
			if (enic->use_noscatter_vec_rx_handler)
				return ENOTSUP;
			if (overlap & MARK)
				return ENOTSUP;
			overlap |= MARK;
			/* ENIC_MAGIC_FILTER_ID is reserved for flagging */
			enic_action->filter_id = ENIC_MAGIC_FILTER_ID;
			enic_action->flags |= FILTER_ACTION_FILTER_ID_FLAG;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_DROP: {
			if (overlap & FATE)
				return ENOTSUP;
			overlap |= FATE;
			enic_action->flags |= FILTER_ACTION_DROP_FLAG;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_RSS: {
			const struct rte_flow_action_rss *rss =
				(const struct rte_flow_action_rss *)
				actions->conf;
			bool allow;
			uint16_t i;

			/*
			 * Hardware does not support general RSS actions, but
			 * we can still support the dummy one that is used to
			 * "receive normally".
			 */
			allow = rss->func == RTE_ETH_HASH_FUNCTION_DEFAULT &&
				rss->level == 0 &&
				(rss->types == 0 ||
				 rss->types == enic->rss_hf) &&
				rss->queue_num == enic->rq_count &&
				rss->key_len == 0;
			/* Identity queue map is ok */
			for (i = 0; i < rss->queue_num; i++)
				allow = allow && (i == rss->queue[i]);
			if (!allow)
				return ENOTSUP;
			if (overlap & FATE)
				return ENOTSUP;
			/* Need MARK or FLAG */
			if (!(overlap & MARK))
				return ENOTSUP;
			overlap |= FATE;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_PASSTHRU: {
			/*
			 * Like RSS above, PASSTHRU + MARK may be used to
			 * "mark and then receive normally". MARK usually comes
			 * after PASSTHRU, so remember we have seen passthru
			 * and check for mark later.
			 */
			if (overlap & FATE)
				return ENOTSUP;
			overlap |= FATE;
			passthru = true;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		default:
			RTE_ASSERT(0);
			break;
		}
	}
	/* Only PASSTHRU + MARK is allowed */
	if (passthru && !(overlap & MARK))
		return ENOTSUP;
	if (!(overlap & FATE))
		return ENOTSUP;
	enic_action->type = FILTER_ACTION_V2;
	return 0;
}

/** Check if the action is supported */
static int
enic_match_action(const struct rte_flow_action *action,
		  const enum rte_flow_action_type *supported_actions)
{
	for (; *supported_actions != RTE_FLOW_ACTION_TYPE_END;
	     supported_actions++) {
		if (action->type == *supported_actions)
			return 1;
	}
	return 0;
}

/** Get the NIC filter capabilties structure */
static const struct enic_filter_cap *
enic_get_filter_cap(struct enic *enic)
{
	if (enic->flow_filter_mode)
		return &enic_filter_cap[enic->flow_filter_mode];

	return NULL;
}

/** Get the actions for this NIC version. */
static const struct enic_action_cap *
enic_get_action_cap(struct enic *enic)
{
	const struct enic_action_cap *ea;
	uint8_t actions;

	actions = enic->filter_actions;
	if (actions & FILTER_ACTION_DROP_FLAG)
		ea = &enic_action_cap[FILTER_ACTION_DROP_FLAG];
	else if (actions & FILTER_ACTION_FILTER_ID_FLAG)
		ea = &enic_action_cap[FILTER_ACTION_FILTER_ID_FLAG];
	else
		ea = &enic_action_cap[FILTER_ACTION_RQ_STEERING_FLAG];
	return ea;
}

/* Debug function to dump internal NIC action structure. */
static void
enic_dump_actions(const struct filter_action_v2 *ea)
{
	if (ea->type == FILTER_ACTION_RQ_STEERING) {
		ENICPMD_LOG(INFO, "Action(V1), queue: %u\n", ea->rq_idx);
	} else if (ea->type == FILTER_ACTION_V2) {
		ENICPMD_LOG(INFO, "Actions(V2)\n");
		if (ea->flags & FILTER_ACTION_RQ_STEERING_FLAG)
			ENICPMD_LOG(INFO, "\tqueue: %u\n",
			       enic_sop_rq_idx_to_rte_idx(ea->rq_idx));
		if (ea->flags & FILTER_ACTION_FILTER_ID_FLAG)
			ENICPMD_LOG(INFO, "\tfilter_id: %u\n", ea->filter_id);
	}
}

/* Debug function to dump internal NIC filter structure. */
static void
enic_dump_filter(const struct filter_v2 *filt)
{
	const struct filter_generic_1 *gp;
	int i, j, mbyte;
	char buf[128], *bp;
	char ip4[16], ip6[16], udp[16], tcp[16], tcpudp[16], ip4csum[16];
	char l4csum[16], ipfrag[16];

	switch (filt->type) {
	case FILTER_IPV4_5TUPLE:
		ENICPMD_LOG(INFO, "FILTER_IPV4_5TUPLE\n");
		break;
	case FILTER_USNIC_IP:
	case FILTER_DPDK_1:
		/* FIXME: this should be a loop */
		gp = &filt->u.generic_1;
		ENICPMD_LOG(INFO, "Filter: vlan: 0x%04x, mask: 0x%04x\n",
		       gp->val_vlan, gp->mask_vlan);

		if (gp->mask_flags & FILTER_GENERIC_1_IPV4)
			sprintf(ip4, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IPV4)
				 ? "ip4(y)" : "ip4(n)");
		else
			sprintf(ip4, "%s ", "ip4(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_IPV6)
			sprintf(ip6, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IPV6)
				 ? "ip6(y)" : "ip6(n)");
		else
			sprintf(ip6, "%s ", "ip6(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_UDP)
			sprintf(udp, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_UDP)
				 ? "udp(y)" : "udp(n)");
		else
			sprintf(udp, "%s ", "udp(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_TCP)
			sprintf(tcp, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_TCP)
				 ? "tcp(y)" : "tcp(n)");
		else
			sprintf(tcp, "%s ", "tcp(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_TCP_OR_UDP)
			sprintf(tcpudp, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_TCP_OR_UDP)
				 ? "tcpudp(y)" : "tcpudp(n)");
		else
			sprintf(tcpudp, "%s ", "tcpudp(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_IP4SUM_OK)
			sprintf(ip4csum, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IP4SUM_OK)
				 ? "ip4csum(y)" : "ip4csum(n)");
		else
			sprintf(ip4csum, "%s ", "ip4csum(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_L4SUM_OK)
			sprintf(l4csum, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_L4SUM_OK)
				 ? "l4csum(y)" : "l4csum(n)");
		else
			sprintf(l4csum, "%s ", "l4csum(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_IPFRAG)
			sprintf(ipfrag, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IPFRAG)
				 ? "ipfrag(y)" : "ipfrag(n)");
		else
			sprintf(ipfrag, "%s ", "ipfrag(x)");
		ENICPMD_LOG(INFO, "\tFlags: %s%s%s%s%s%s%s%s\n", ip4, ip6, udp,
			 tcp, tcpudp, ip4csum, l4csum, ipfrag);

		for (i = 0; i < FILTER_GENERIC_1_NUM_LAYERS; i++) {
			mbyte = FILTER_GENERIC_1_KEY_LEN - 1;
			while (mbyte && !gp->layer[i].mask[mbyte])
				mbyte--;
			if (mbyte == 0)
				continue;

			bp = buf;
			for (j = 0; j <= mbyte; j++) {
				sprintf(bp, "%02x",
					gp->layer[i].mask[j]);
				bp += 2;
			}
			*bp = '\0';
			ENICPMD_LOG(INFO, "\tL%u mask: %s\n", i + 2, buf);
			bp = buf;
			for (j = 0; j <= mbyte; j++) {
				sprintf(bp, "%02x",
					gp->layer[i].val[j]);
				bp += 2;
			}
			*bp = '\0';
			ENICPMD_LOG(INFO, "\tL%u  val: %s\n", i + 2, buf);
		}
		break;
	default:
		ENICPMD_LOG(INFO, "FILTER UNKNOWN\n");
		break;
	}
}

/* Debug function to dump internal NIC flow structures. */
static void
enic_dump_flow(const struct filter_action_v2 *ea, const struct filter_v2 *filt)
{
	enic_dump_filter(filt);
	enic_dump_actions(ea);
}


/**
 * Internal flow parse/validate function.
 *
 * @param dev[in]
 *   This device pointer.
 * @param pattern[in]
 * @param actions[in]
 * @param error[out]
 * @param enic_filter[out]
 *   Internal NIC filter structure pointer.
 * @param enic_action[out]
 *   Internal NIC action structure pointer.
 */
static int
enic_flow_parse(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attrs,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error,
		struct filter_v2 *enic_filter,
		struct filter_action_v2 *enic_action)
{
	unsigned int ret = 0;
	struct enic *enic = pmd_priv(dev);
	const struct enic_filter_cap *enic_filter_cap;
	const struct enic_action_cap *enic_action_cap;
	const struct rte_flow_action *action;

	ENICPMD_FUNC_TRACE();

	memset(enic_filter, 0, sizeof(*enic_filter));
	memset(enic_action, 0, sizeof(*enic_action));

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "No pattern specified");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "No action specified");
		return -rte_errno;
	}

	if (attrs) {
		if (attrs->group) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					   NULL,
					   "priority groups are not supported");
			return -rte_errno;
		} else if (attrs->priority) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					   NULL,
					   "priorities are not supported");
			return -rte_errno;
		} else if (attrs->egress) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					   NULL,
					   "egress is not supported");
			return -rte_errno;
		} else if (attrs->transfer) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					   NULL,
					   "transfer is not supported");
			return -rte_errno;
		} else if (!attrs->ingress) {
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					   NULL,
					   "only ingress is supported");
			return -rte_errno;
		}

	} else {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "No attribute specified");
		return -rte_errno;
	}

	/* Verify Actions. */
	enic_action_cap =  enic_get_action_cap(enic);
	for (action = &actions[0]; action->type != RTE_FLOW_ACTION_TYPE_END;
	     action++) {
		if (action->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;
		else if (!enic_match_action(action, enic_action_cap->actions))
			break;
	}
	if (action->type != RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_ACTION,
				   action, "Invalid action.");
		return -rte_errno;
	}
	ret = enic_action_cap->copy_fn(enic, actions, enic_action);
	if (ret) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
			   NULL, "Unsupported action.");
		return -rte_errno;
	}

	/* Verify Flow items. If copying the filter from flow format to enic
	 * format fails, the flow is not supported
	 */
	enic_filter_cap =  enic_get_filter_cap(enic);
	if (enic_filter_cap == NULL) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_HANDLE,
			   NULL, "Flow API not available");
		return -rte_errno;
	}
	enic_filter->type = enic->flow_filter_mode;
	if (enic->adv_filters)
		enic_filter->type = FILTER_DPDK_1;
	ret = enic_copy_filter(pattern, enic_filter_cap, enic,
				       enic_filter, error);
	return ret;
}

/**
 * Push filter/action to the NIC.
 *
 * @param enic[in]
 *   Device structure pointer.
 * @param enic_filter[in]
 *   Internal NIC filter structure pointer.
 * @param enic_action[in]
 *   Internal NIC action structure pointer.
 * @param error[out]
 */
static struct rte_flow *
enic_flow_add_filter(struct enic *enic, struct filter_v2 *enic_filter,
		   struct filter_action_v2 *enic_action,
		   struct rte_flow_error *error)
{
	struct rte_flow *flow;
	int err;
	uint16_t entry;

	ENICPMD_FUNC_TRACE();

	flow = rte_calloc(__func__, 1, sizeof(*flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate flow memory");
		return NULL;
	}

	/* entry[in] is the queue id, entry[out] is the filter Id for delete */
	entry = enic_action->rq_idx;
	err = vnic_dev_classifier(enic->vdev, CLSF_ADD, &entry, enic_filter,
				  enic_action);
	if (err) {
		rte_flow_error_set(error, -err, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "vnic_dev_classifier error");
		rte_free(flow);
		return NULL;
	}

	flow->enic_filter_id = entry;
	flow->enic_filter = *enic_filter;
	return flow;
}

/**
 * Remove filter/action from the NIC.
 *
 * @param enic[in]
 *   Device structure pointer.
 * @param filter_id[in]
 *   Id of NIC filter.
 * @param enic_action[in]
 *   Internal NIC action structure pointer.
 * @param error[out]
 */
static int
enic_flow_del_filter(struct enic *enic, struct rte_flow *flow,
		   struct rte_flow_error *error)
{
	uint16_t filter_id;
	int err;

	ENICPMD_FUNC_TRACE();

	filter_id = flow->enic_filter_id;
	err = vnic_dev_classifier(enic->vdev, CLSF_DEL, &filter_id, NULL, NULL);
	if (err) {
		rte_flow_error_set(error, -err, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "vnic_dev_classifier failed");
		return -err;
	}
	return 0;
}

/*
 * The following functions are callbacks for Generic flow API.
 */

/**
 * Validate a flow supported by the NIC.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
static int
enic_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attrs,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct filter_v2 enic_filter;
	struct filter_action_v2 enic_action;
	int ret;

	ENICPMD_FUNC_TRACE();

	ret = enic_flow_parse(dev, attrs, pattern, actions, error,
			       &enic_filter, &enic_action);
	if (!ret)
		enic_dump_flow(&enic_action, &enic_filter);
	return ret;
}

/**
 * Create a flow supported by the NIC.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
static struct rte_flow *
enic_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attrs,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	int ret;
	struct filter_v2 enic_filter;
	struct filter_action_v2 enic_action;
	struct rte_flow *flow;
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();

	ret = enic_flow_parse(dev, attrs, pattern, actions, error, &enic_filter,
			      &enic_action);
	if (ret < 0)
		return NULL;

	flow = enic_flow_add_filter(enic, &enic_filter, &enic_action,
				    error);
	if (flow)
		LIST_INSERT_HEAD(&enic->flows, flow, next);

	return flow;
}

/**
 * Destroy a flow supported by the NIC.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
static int
enic_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		  __rte_unused struct rte_flow_error *error)
{
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();

	enic_flow_del_filter(enic, flow, error);
	LIST_REMOVE(flow, next);
	rte_free(flow);
	return 0;
}

/**
 * Flush all flows on the device.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
static int
enic_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct rte_flow *flow;
	struct enic *enic = pmd_priv(dev);

	ENICPMD_FUNC_TRACE();


	while (!LIST_EMPTY(&enic->flows)) {
		flow = LIST_FIRST(&enic->flows);
		enic_flow_del_filter(enic, flow, error);
		LIST_REMOVE(flow, next);
		rte_free(flow);
	}
	return 0;
}

/**
 * Flow callback registration.
 *
 * @see rte_flow_ops
 */
const struct rte_flow_ops enic_flow_ops = {
	.validate = enic_flow_validate,
	.create = enic_flow_create,
	.destroy = enic_flow_destroy,
	.flush = enic_flow_flush,
};
