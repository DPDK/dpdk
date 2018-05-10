/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 */

#include <errno.h>
#include <stdint.h>
#include <rte_log.h>
#include <rte_ethdev_driver.h>
#include <rte_flow_driver.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_nic.h"

#define FLOW_TRACE() \
	rte_log(RTE_LOG_DEBUG, enicpmd_logtype_flow, \
		"%s()\n", __func__)
#define FLOW_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, enicpmd_logtype_flow, \
		fmt "\n", ##args)

/** Info about how to copy items into enic filters. */
struct enic_items {
	/** Function for copying and validating an item. */
	int (*copy_item)(const struct rte_flow_item *item,
			 struct filter_v2 *enic_filter, u8 *inner_ofst);
	/** List of valid previous items. */
	const enum rte_flow_item_type * const prev_items;
	/** True if it's OK for this item to be the first item. For some NIC
	 * versions, it's invalid to start the stack above layer 3.
	 */
	const u8 valid_start_item;
};

/** Filtering capabilities for various NIC and firmware versions. */
struct enic_filter_cap {
	/** list of valid items and their handlers and attributes. */
	const struct enic_items *item_info;
};

/* functions for copying flow actions into enic actions */
typedef int (copy_action_fn)(const struct rte_flow_action actions[],
			     struct filter_action_v2 *enic_action);

/* functions for copying items into enic filters */
typedef int(enic_copy_item_fn)(const struct rte_flow_item *item,
			  struct filter_v2 *enic_filter, u8 *inner_ofst);

/** Action capabilities for various NICs. */
struct enic_action_cap {
	/** list of valid actions */
	const enum rte_flow_action_type *actions;
	/** copy function for a particular NIC */
	int (*copy_fn)(const struct rte_flow_action actions[],
		       struct filter_action_v2 *enic_action);
};

/* Forward declarations */
static enic_copy_item_fn enic_copy_item_ipv4_v1;
static enic_copy_item_fn enic_copy_item_udp_v1;
static enic_copy_item_fn enic_copy_item_tcp_v1;
static enic_copy_item_fn enic_copy_item_eth_v2;
static enic_copy_item_fn enic_copy_item_vlan_v2;
static enic_copy_item_fn enic_copy_item_ipv4_v2;
static enic_copy_item_fn enic_copy_item_ipv6_v2;
static enic_copy_item_fn enic_copy_item_udp_v2;
static enic_copy_item_fn enic_copy_item_tcp_v2;
static enic_copy_item_fn enic_copy_item_sctp_v2;
static enic_copy_item_fn enic_copy_item_sctp_v2;
static enic_copy_item_fn enic_copy_item_vxlan_v2;
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
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v1,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v1,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
};

/**
 * NICs have Advanced Filters capability but they are disabled. This means
 * that layer 3 must be specified.
 */
static const struct enic_items enic_items_v2[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.copy_item = enic_copy_item_eth_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.copy_item = enic_copy_item_vlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_copy_item_ipv4_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.copy_item = enic_copy_item_ipv6_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.copy_item = enic_copy_item_sctp_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.copy_item = enic_copy_item_vxlan_v2,
		.valid_start_item = 0,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
};

/** NICs with Advanced filters enabled */
static const struct enic_items enic_items_v3[] = {
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.copy_item = enic_copy_item_eth_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_VXLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.copy_item = enic_copy_item_vlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.copy_item = enic_copy_item_ipv4_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.copy_item = enic_copy_item_ipv6_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_ETH,
			       RTE_FLOW_ITEM_TYPE_VLAN,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.copy_item = enic_copy_item_udp_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.copy_item = enic_copy_item_tcp_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.copy_item = enic_copy_item_sctp_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_IPV4,
			       RTE_FLOW_ITEM_TYPE_IPV6,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.copy_item = enic_copy_item_vxlan_v2,
		.valid_start_item = 1,
		.prev_items = (const enum rte_flow_item_type[]) {
			       RTE_FLOW_ITEM_TYPE_UDP,
			       RTE_FLOW_ITEM_TYPE_END,
		},
	},
};

/** Filtering capabilities indexed this NICs supported filter type. */
static const struct enic_filter_cap enic_filter_cap[] = {
	[FILTER_IPV4_5TUPLE] = {
		.item_info = enic_items_v1,
	},
	[FILTER_USNIC_IP] = {
		.item_info = enic_items_v2,
	},
	[FILTER_DPDK_1] = {
		.item_info = enic_items_v3,
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
	RTE_FLOW_ACTION_TYPE_END,
};

static const enum rte_flow_action_type enic_supported_actions_v2_drop[] = {
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_MARK,
	RTE_FLOW_ACTION_TYPE_FLAG,
	RTE_FLOW_ACTION_TYPE_DROP,
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
mask_exact_match(const u8 *supported, const u8 *supplied,
		 unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++) {
		if (supported[i] != supplied[i])
			return 0;
	}
	return 1;
}

/**
 * Copy IPv4 item into version 1 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Should always be 0 for version 1.
 */
static int
enic_copy_item_ipv4_v1(const struct rte_flow_item *item,
		       struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct ipv4_hdr supported_mask = {
		.src_addr = 0xffffffff,
		.dst_addr = 0xffffffff,
	};

	FLOW_TRACE();

	if (*inner_ofst)
		return ENOTSUP;

	if (!mask)
		mask = &rte_flow_item_ipv4_mask;

	/* This is an exact match filter, both fields must be set */
	if (!spec || !spec->hdr.src_addr || !spec->hdr.dst_addr) {
		FLOW_LOG(ERR, "IPv4 exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the suppied mask exactly matches capabilty */
	if (!mask_exact_match((const u8 *)&supported_mask,
			      (const u8 *)item->mask, sizeof(*mask))) {
		FLOW_LOG(ERR, "IPv4 exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_addr = spec->hdr.src_addr;
	enic_5tup->dst_addr = spec->hdr.dst_addr;

	return 0;
}

/**
 * Copy UDP item into version 1 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Should always be 0 for version 1.
 */
static int
enic_copy_item_udp_v1(const struct rte_flow_item *item,
		      struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct udp_hdr supported_mask = {
		.src_port = 0xffff,
		.dst_port = 0xffff,
	};

	FLOW_TRACE();

	if (*inner_ofst)
		return ENOTSUP;

	if (!mask)
		mask = &rte_flow_item_udp_mask;

	/* This is an exact match filter, both ports must be set */
	if (!spec || !spec->hdr.src_port || !spec->hdr.dst_port) {
		FLOW_LOG(ERR, "UDP exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the suppied mask exactly matches capabilty */
	if (!mask_exact_match((const u8 *)&supported_mask,
			      (const u8 *)item->mask, sizeof(*mask))) {
		FLOW_LOG(ERR, "UDP exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_port = spec->hdr.src_port;
	enic_5tup->dst_port = spec->hdr.dst_port;
	enic_5tup->protocol = PROTO_UDP;

	return 0;
}

/**
 * Copy TCP item into version 1 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Should always be 0 for version 1.
 */
static int
enic_copy_item_tcp_v1(const struct rte_flow_item *item,
		      struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct filter_ipv4_5tuple *enic_5tup = &enic_filter->u.ipv4;
	struct tcp_hdr supported_mask = {
		.src_port = 0xffff,
		.dst_port = 0xffff,
	};

	FLOW_TRACE();

	if (*inner_ofst)
		return ENOTSUP;

	if (!mask)
		mask = &rte_flow_item_tcp_mask;

	/* This is an exact match filter, both ports must be set */
	if (!spec || !spec->hdr.src_port || !spec->hdr.dst_port) {
		FLOW_LOG(ERR, "TCPIPv4 exact match src/dst addr");
		return ENOTSUP;
	}

	/* check that the suppied mask exactly matches capabilty */
	if (!mask_exact_match((const u8 *)&supported_mask,
			     (const u8 *)item->mask, sizeof(*mask))) {
		FLOW_LOG(ERR, "TCP exact match mask");
		return ENOTSUP;
	}

	enic_filter->u.ipv4.flags = FILTER_FIELDS_IPV4_5TUPLE;
	enic_5tup->src_port = spec->hdr.src_port;
	enic_5tup->dst_port = spec->hdr.dst_port;
	enic_5tup->protocol = PROTO_TCP;

	return 0;
}

/**
 * Copy ETH item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   If zero, this is an outer header. If non-zero, this is the offset into L5
 *   where the header begins.
 */
static int
enic_copy_item_eth_v2(const struct rte_flow_item *item,
		      struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	struct ether_hdr enic_spec;
	struct ether_hdr enic_mask;
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_eth_mask;

	memcpy(enic_spec.d_addr.addr_bytes, spec->dst.addr_bytes,
	       ETHER_ADDR_LEN);
	memcpy(enic_spec.s_addr.addr_bytes, spec->src.addr_bytes,
	       ETHER_ADDR_LEN);

	memcpy(enic_mask.d_addr.addr_bytes, mask->dst.addr_bytes,
	       ETHER_ADDR_LEN);
	memcpy(enic_mask.s_addr.addr_bytes, mask->src.addr_bytes,
	       ETHER_ADDR_LEN);
	enic_spec.ether_type = spec->type;
	enic_mask.ether_type = mask->type;

	if (*inner_ofst == 0) {
		/* outer header */
		memcpy(gp->layer[FILTER_GENERIC_1_L2].mask, &enic_mask,
		       sizeof(struct ether_hdr));
		memcpy(gp->layer[FILTER_GENERIC_1_L2].val, &enic_spec,
		       sizeof(struct ether_hdr));
	} else {
		/* inner header */
		if ((*inner_ofst + sizeof(struct ether_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		/* Offset into L5 where inner Ethernet header goes */
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       &enic_mask, sizeof(struct ether_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       &enic_spec, sizeof(struct ether_hdr));
		*inner_ofst += sizeof(struct ether_hdr);
	}
	return 0;
}

/**
 * Copy VLAN item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   If zero, this is an outer header. If non-zero, this is the offset into L5
 *   where the header begins.
 */
static int
enic_copy_item_vlan_v2(const struct rte_flow_item *item,
		       struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_vlan_mask;

	if (*inner_ofst == 0) {
		struct ether_hdr *eth_mask =
			(void *)gp->layer[FILTER_GENERIC_1_L2].mask;
		struct ether_hdr *eth_val =
			(void *)gp->layer[FILTER_GENERIC_1_L2].val;

		/* Outer TPID cannot be matched */
		if (eth_mask->ether_type)
			return ENOTSUP;
		eth_mask->ether_type = mask->inner_type;
		eth_val->ether_type = spec->inner_type;

		/* Outer header. Use the vlan mask/val fields */
		gp->mask_vlan = mask->tci;
		gp->val_vlan = spec->tci;
	} else {
		/* Inner header. Mask/Val start at *inner_ofst into L5 */
		if ((*inner_ofst + sizeof(struct vlan_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       mask, sizeof(struct vlan_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       spec, sizeof(struct vlan_hdr));
		*inner_ofst += sizeof(struct vlan_hdr);
	}
	return 0;
}

/**
 * Copy IPv4 item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. Don't support inner IPv4 filtering.
 */
static int
enic_copy_item_ipv4_v2(const struct rte_flow_item *item,
		       struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	if (*inner_ofst == 0) {
		/* Match IPv4 */
		gp->mask_flags |= FILTER_GENERIC_1_IPV4;
		gp->val_flags |= FILTER_GENERIC_1_IPV4;

		/* Match all if no spec */
		if (!spec)
			return 0;

		if (!mask)
			mask = &rte_flow_item_ipv4_mask;

		memcpy(gp->layer[FILTER_GENERIC_1_L3].mask, &mask->hdr,
		       sizeof(struct ipv4_hdr));
		memcpy(gp->layer[FILTER_GENERIC_1_L3].val, &spec->hdr,
		       sizeof(struct ipv4_hdr));
	} else {
		/* Inner IPv4 header. Mask/Val start at *inner_ofst into L5 */
		if ((*inner_ofst + sizeof(struct ipv4_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       mask, sizeof(struct ipv4_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       spec, sizeof(struct ipv4_hdr));
		*inner_ofst += sizeof(struct ipv4_hdr);
	}
	return 0;
}

/**
 * Copy IPv6 item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. Don't support inner IPv6 filtering.
 */
static int
enic_copy_item_ipv6_v2(const struct rte_flow_item *item,
		       struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	/* Match IPv6 */
	gp->mask_flags |= FILTER_GENERIC_1_IPV6;
	gp->val_flags |= FILTER_GENERIC_1_IPV6;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_ipv6_mask;

	if (*inner_ofst == 0) {
		memcpy(gp->layer[FILTER_GENERIC_1_L3].mask, &mask->hdr,
		       sizeof(struct ipv6_hdr));
		memcpy(gp->layer[FILTER_GENERIC_1_L3].val, &spec->hdr,
		       sizeof(struct ipv6_hdr));
	} else {
		/* Inner IPv6 header. Mask/Val start at *inner_ofst into L5 */
		if ((*inner_ofst + sizeof(struct ipv6_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       mask, sizeof(struct ipv6_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       spec, sizeof(struct ipv6_hdr));
		*inner_ofst += sizeof(struct ipv6_hdr);
	}
	return 0;
}

/**
 * Copy UDP item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. Don't support inner UDP filtering.
 */
static int
enic_copy_item_udp_v2(const struct rte_flow_item *item,
		      struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	/* Match UDP */
	gp->mask_flags |= FILTER_GENERIC_1_UDP;
	gp->val_flags |= FILTER_GENERIC_1_UDP;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_udp_mask;

	if (*inner_ofst == 0) {
		memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
		       sizeof(struct udp_hdr));
		memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
		       sizeof(struct udp_hdr));
	} else {
		/* Inner IPv6 header. Mask/Val start at *inner_ofst into L5 */
		if ((*inner_ofst + sizeof(struct udp_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       mask, sizeof(struct udp_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       spec, sizeof(struct udp_hdr));
		*inner_ofst += sizeof(struct udp_hdr);
	}
	return 0;
}

/**
 * Copy TCP item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. Don't support inner TCP filtering.
 */
static int
enic_copy_item_tcp_v2(const struct rte_flow_item *item,
		      struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	/* Match TCP */
	gp->mask_flags |= FILTER_GENERIC_1_TCP;
	gp->val_flags |= FILTER_GENERIC_1_TCP;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		return ENOTSUP;

	if (*inner_ofst == 0) {
		memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
		       sizeof(struct tcp_hdr));
		memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
		       sizeof(struct tcp_hdr));
	} else {
		/* Inner IPv6 header. Mask/Val start at *inner_ofst into L5 */
		if ((*inner_ofst + sizeof(struct tcp_hdr)) >
		     FILTER_GENERIC_1_KEY_LEN)
			return ENOTSUP;
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].mask[*inner_ofst],
		       mask, sizeof(struct tcp_hdr));
		memcpy(&gp->layer[FILTER_GENERIC_1_L5].val[*inner_ofst],
		       spec, sizeof(struct tcp_hdr));
		*inner_ofst += sizeof(struct tcp_hdr);
	}
	return 0;
}

/**
 * Copy SCTP item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. Don't support inner SCTP filtering.
 */
static int
enic_copy_item_sctp_v2(const struct rte_flow_item *item,
		       struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_sctp *spec = item->spec;
	const struct rte_flow_item_sctp *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	if (*inner_ofst)
		return ENOTSUP;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_sctp_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L4].mask, &mask->hdr,
	       sizeof(struct sctp_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L4].val, &spec->hdr,
	       sizeof(struct sctp_hdr));
	return 0;
}

/**
 * Copy UDP item into version 2 NIC filter.
 *
 * @param item[in]
 *   Item specification.
 * @param enic_filter[out]
 *   Partially filled in NIC filter structure.
 * @param inner_ofst[in]
 *   Must be 0. VxLAN headers always start at the beginning of L5.
 */
static int
enic_copy_item_vxlan_v2(const struct rte_flow_item *item,
			struct filter_v2 *enic_filter, u8 *inner_ofst)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	struct filter_generic_1 *gp = &enic_filter->u.generic_1;

	FLOW_TRACE();

	if (*inner_ofst)
		return EINVAL;

	/* Match all if no spec */
	if (!spec)
		return 0;

	if (!mask)
		mask = &rte_flow_item_vxlan_mask;

	memcpy(gp->layer[FILTER_GENERIC_1_L5].mask, mask,
	       sizeof(struct vxlan_hdr));
	memcpy(gp->layer[FILTER_GENERIC_1_L5].val, spec,
	       sizeof(struct vxlan_hdr));

	*inner_ofst = sizeof(struct vxlan_hdr);
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
		    const struct enic_items *item_info, u8 is_first_item)
{
	enum rte_flow_item_type const *allowed_items = item_info->prev_items;

	FLOW_TRACE();

	for (; *allowed_items != RTE_FLOW_ITEM_TYPE_END; allowed_items++) {
		if (prev_item == *allowed_items)
			return 1;
	}

	/* This is the first item in the stack. Check if that's cool */
	if (is_first_item && item_info->valid_start_item)
		return 1;

	return 0;
}

/**
 * Build the intenal enic filter structure from the provided pattern. The
 * pattern is validated as the items are copied.
 *
 * @param pattern[in]
 * @param items_info[in]
 *   Info about this NICs item support, like valid previous items.
 * @param enic_filter[out]
 *   NIC specfilc filters derived from the pattern.
 * @param error[out]
 */
static int
enic_copy_filter(const struct rte_flow_item pattern[],
		 const struct enic_items *items_info,
		 struct filter_v2 *enic_filter,
		 struct rte_flow_error *error)
{
	int ret;
	const struct rte_flow_item *item = pattern;
	u8 inner_ofst = 0; /* If encapsulated, ofst into L5 */
	enum rte_flow_item_type prev_item;
	const struct enic_items *item_info;

	u8 is_first_item = 1;

	FLOW_TRACE();

	prev_item = 0;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		/* Get info about how to validate and copy the item. If NULL
		 * is returned the nic does not support the item.
		 */
		if (item->type == RTE_FLOW_ITEM_TYPE_VOID)
			continue;

		item_info = &items_info[item->type];

		/* check to see if item stacking is valid */
		if (!item_stacking_valid(prev_item, item_info, is_first_item))
			goto stacking_error;

		ret = item_info->copy_item(item, enic_filter, &inner_ofst);
		if (ret)
			goto item_not_supported;
		prev_item = item->type;
		is_first_item = 0;
	}
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
 * Build the intenal version 1 NIC action structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param enic_action[out]
 *   NIC specfilc actions derived from the actions.
 * @param error[out]
 */
static int
enic_copy_action_v1(const struct rte_flow_action actions[],
		    struct filter_action_v2 *enic_action)
{
	enum { FATE = 1, };
	uint32_t overlap = 0;

	FLOW_TRACE();

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
 * Build the intenal version 2 NIC action structure from the provided pattern.
 * The pattern is validated as the items are copied.
 *
 * @param actions[in]
 * @param enic_action[out]
 *   NIC specfilc actions derived from the actions.
 * @param error[out]
 */
static int
enic_copy_action_v2(const struct rte_flow_action actions[],
		    struct filter_action_v2 *enic_action)
{
	enum { FATE = 1, MARK = 2, };
	uint32_t overlap = 0;

	FLOW_TRACE();

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

			if (overlap & MARK)
				return ENOTSUP;
			overlap |= MARK;
			/* ENIC_MAGIC_FILTER_ID is reserved and is the highest
			 * in the range of allows mark ids.
			 */
			if (mark->id >= ENIC_MAGIC_FILTER_ID)
				return EINVAL;
			enic_action->filter_id = mark->id;
			enic_action->flags |= FILTER_ACTION_FILTER_ID_FLAG;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_FLAG: {
			if (overlap & MARK)
				return ENOTSUP;
			overlap |= MARK;
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
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		default:
			RTE_ASSERT(0);
			break;
		}
	}
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
		FLOW_LOG(INFO, "Action(V1), queue: %u\n", ea->rq_idx);
	} else if (ea->type == FILTER_ACTION_V2) {
		FLOW_LOG(INFO, "Actions(V2)\n");
		if (ea->flags & FILTER_ACTION_RQ_STEERING_FLAG)
			FLOW_LOG(INFO, "\tqueue: %u\n",
			       enic_sop_rq_idx_to_rte_idx(ea->rq_idx));
		if (ea->flags & FILTER_ACTION_FILTER_ID_FLAG)
			FLOW_LOG(INFO, "\tfilter_id: %u\n", ea->filter_id);
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
		FLOW_LOG(INFO, "FILTER_IPV4_5TUPLE\n");
		break;
	case FILTER_USNIC_IP:
	case FILTER_DPDK_1:
		/* FIXME: this should be a loop */
		gp = &filt->u.generic_1;
		FLOW_LOG(INFO, "Filter: vlan: 0x%04x, mask: 0x%04x\n",
		       gp->val_vlan, gp->mask_vlan);

		if (gp->mask_flags & FILTER_GENERIC_1_IPV4)
			sprintf(ip4, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IPV4)
				 ? "ip4(y)" : "ip4(n)");
		else
			sprintf(ip4, "%s ", "ip4(x)");

		if (gp->mask_flags & FILTER_GENERIC_1_IPV6)
			sprintf(ip6, "%s ",
				(gp->val_flags & FILTER_GENERIC_1_IPV4)
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
		FLOW_LOG(INFO, "\tFlags: %s%s%s%s%s%s%s%s\n", ip4, ip6, udp,
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
			FLOW_LOG(INFO, "\tL%u mask: %s\n", i + 2, buf);
			bp = buf;
			for (j = 0; j <= mbyte; j++) {
				sprintf(bp, "%02x",
					gp->layer[i].val[j]);
				bp += 2;
			}
			*bp = '\0';
			FLOW_LOG(INFO, "\tL%u  val: %s\n", i + 2, buf);
		}
		break;
	default:
		FLOW_LOG(INFO, "FILTER UNKNOWN\n");
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

	FLOW_TRACE();

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
	ret = enic_action_cap->copy_fn(actions, enic_action);
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
	ret = enic_copy_filter(pattern, enic_filter_cap->item_info,
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
	int ret;
	u16 entry;

	FLOW_TRACE();

	flow = rte_calloc(__func__, 1, sizeof(*flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "cannot allocate flow memory");
		return NULL;
	}

	/* entry[in] is the queue id, entry[out] is the filter Id for delete */
	entry = enic_action->rq_idx;
	ret = vnic_dev_classifier(enic->vdev, CLSF_ADD, &entry, enic_filter,
				  enic_action);
	if (!ret) {
		flow->enic_filter_id = entry;
		flow->enic_filter = *enic_filter;
	} else {
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "vnic_dev_classifier error");
		rte_free(flow);
		return NULL;
	}
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
enic_flow_del_filter(struct enic *enic, u16 filter_id,
		   struct rte_flow_error *error)
{
	int ret;

	FLOW_TRACE();

	ret = vnic_dev_classifier(enic->vdev, CLSF_DEL, &filter_id, NULL, NULL);
	if (!ret)
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "vnic_dev_classifier failed");
	return ret;
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

	FLOW_TRACE();

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

	FLOW_TRACE();

	ret = enic_flow_parse(dev, attrs, pattern, actions, error, &enic_filter,
			      &enic_action);
	if (ret < 0)
		return NULL;

	rte_spinlock_lock(&enic->flows_lock);
	flow = enic_flow_add_filter(enic, &enic_filter, &enic_action,
				    error);
	if (flow)
		LIST_INSERT_HEAD(&enic->flows, flow, next);
	rte_spinlock_unlock(&enic->flows_lock);

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

	FLOW_TRACE();

	rte_spinlock_lock(&enic->flows_lock);
	enic_flow_del_filter(enic, flow->enic_filter_id, error);
	LIST_REMOVE(flow, next);
	rte_spinlock_unlock(&enic->flows_lock);
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

	FLOW_TRACE();

	rte_spinlock_lock(&enic->flows_lock);

	while (!LIST_EMPTY(&enic->flows)) {
		flow = LIST_FIRST(&enic->flows);
		enic_flow_del_filter(enic, flow->enic_filter_id, error);
		LIST_REMOVE(flow, next);
	}
	rte_spinlock_unlock(&enic->flows_lock);
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
