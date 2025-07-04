/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_net_flow.h"

#include <rte_flow_driver.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

#include "nfp_logs.h"
#include "nfp_net_cmsg.h"

/* Static initializer for a list of subsequent item types */
#define NEXT_ITEM(...) \
	((const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	})

/* Process structure associated with a flow item */
struct nfp_net_flow_item_proc {
	/* Bit-mask for fields supported by this PMD. */
	const void *mask_support;
	/* Bit-mask to use when @p item->mask is not provided. */
	const void *mask_default;
	/* Size in bytes for @p mask_support and @p mask_default. */
	const uint32_t mask_sz;
	/* Merge a pattern item into a flow rule handle. */
	int (*merge)(struct rte_flow *nfp_flow,
			const struct rte_flow_item *item,
			const struct nfp_net_flow_item_proc *proc);
	/* List of possible subsequent items. */
	const enum rte_flow_item_type *const next_item;
};

static int
nfp_net_flow_table_add(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int ret;

	ret = rte_hash_add_key_data(priv->flow_table, &nfp_flow->hash_key, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to flow table failed.");
		return ret;
	}

	return 0;
}

static int
nfp_net_flow_table_delete(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int ret;

	ret = rte_hash_del_key(priv->flow_table, &nfp_flow->hash_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Delete from flow table failed.");
		return ret;
	}

	return 0;
}

static struct rte_flow *
nfp_net_flow_table_search(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int index;
	struct rte_flow *flow_find;

	index = rte_hash_lookup_data(priv->flow_table, &nfp_flow->hash_key,
			(void **)&flow_find);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the flow table.");
		return NULL;
	}

	return flow_find;
}

static int
nfp_net_flow_position_acquire(struct nfp_net_priv *priv,
		uint32_t priority,
		struct rte_flow *nfp_flow)
{
	uint32_t i;
	uint32_t limit;

	limit = priv->flow_limit;

	if (priority != 0) {
		i = limit - priority - 1;

		if (priv->flow_position[i]) {
			PMD_DRV_LOG(ERR, "There is already a flow rule in this place.");
			return -EAGAIN;
		}

		priv->flow_position[i] = true;
		nfp_flow->position = priority;
		return 0;
	}

	for (i = 0; i < limit; i++) {
		if (!priv->flow_position[i]) {
			priv->flow_position[i] = true;
			break;
		}
	}

	if (i == limit) {
		PMD_DRV_LOG(ERR, "The limited flow number is reach.");
		return -ERANGE;
	}

	nfp_flow->position = limit - i - 1;

	return 0;
}

static void
nfp_net_flow_position_free(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	uint32_t index;

	index = priv->flow_limit - 1 - nfp_flow->position;

	priv->flow_position[index] = false;
}

static struct rte_flow *
nfp_net_flow_alloc(struct nfp_net_priv *priv,
		uint32_t priority,
		uint32_t match_len,
		uint32_t action_len,
		uint32_t port_id)
{
	int ret;
	char *data;
	struct rte_flow *nfp_flow;
	struct nfp_net_flow_payload *payload;

	nfp_flow = rte_zmalloc("nfp_flow", sizeof(struct rte_flow), 0);
	if (nfp_flow == NULL)
		return NULL;

	data = rte_zmalloc("nfp_flow_payload", match_len + action_len, 0);
	if (data == NULL)
		goto free_flow;

	ret = nfp_net_flow_position_acquire(priv, priority, nfp_flow);
	if (ret != 0)
		goto free_payload;

	nfp_flow->port_id      = port_id;
	payload                = &nfp_flow->payload;
	payload->match_len     = match_len;
	payload->action_len    = action_len;
	payload->match_data    = data;
	payload->action_data   = data + match_len;

	return nfp_flow;

free_payload:
	rte_free(data);
free_flow:
	rte_free(nfp_flow);

	return NULL;
}

static void
nfp_net_flow_free(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	nfp_net_flow_position_free(priv, nfp_flow);
	rte_free(nfp_flow->payload.match_data);
	rte_free(nfp_flow);
}

static int
nfp_net_flow_calculate_items(const struct rte_flow_item items[],
		uint32_t *match_len,
		uint32_t *item_type)
{
	int ret = -EINVAL;
	const struct rte_flow_item *item;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_ETH detected.");
			*match_len = sizeof(struct nfp_net_cmsg_match_eth);
			*item_type = RTE_FLOW_ITEM_TYPE_ETH;
			ret = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_IPV4 detected.");
			*match_len = sizeof(struct nfp_net_cmsg_match_v4);
			*item_type = RTE_FLOW_ITEM_TYPE_IPV4;
			return 0;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_IPV6 detected.");
			*match_len = sizeof(struct nfp_net_cmsg_match_v6);
			*item_type = RTE_FLOW_ITEM_TYPE_IPV6;
			return 0;
		default:
			PMD_DRV_LOG(ERR, "Can not calculate match length.");
			*match_len = 0;
			return -ENOTSUP;
		}
	}

	return ret;
}

static int
nfp_net_flow_merge_eth(__rte_unused struct rte_flow *nfp_flow,
		const struct rte_flow_item *item,
		__rte_unused const struct nfp_net_flow_item_proc *proc)
{
	struct nfp_net_cmsg_match_eth *eth;
	const struct rte_flow_item_eth *spec;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(ERR, "NFP flow merge eth: no item->spec!");
		return -EINVAL;
	}

	nfp_flow->payload.cmsg_type = NFP_NET_CFG_MBOX_CMD_FS_ADD_ETHTYPE;

	eth = (struct nfp_net_cmsg_match_eth *)nfp_flow->payload.match_data;
	eth->ether_type = rte_be_to_cpu_16(spec->type);

	return 0;
}

static int
nfp_net_flow_merge_ipv4(struct rte_flow *nfp_flow,
		const struct rte_flow_item *item,
		const struct nfp_net_flow_item_proc *proc)
{
	struct nfp_net_cmsg_match_v4 *ipv4;
	const struct rte_flow_item_ipv4 *mask;
	const struct rte_flow_item_ipv4 *spec;

	nfp_flow->payload.cmsg_type = NFP_NET_CFG_MBOX_CMD_FS_ADD_V4;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "NFP flow merge ipv4: no item->spec!");
		return 0;
	}

	mask = (item->mask != NULL) ? item->mask : proc->mask_default;

	ipv4 = (struct nfp_net_cmsg_match_v4 *)nfp_flow->payload.match_data;

	ipv4->l4_protocol_mask = mask->hdr.next_proto_id;
	ipv4->src_ipv4_mask    = rte_be_to_cpu_32(mask->hdr.src_addr);
	ipv4->dst_ipv4_mask    = rte_be_to_cpu_32(mask->hdr.dst_addr);

	ipv4->l4_protocol  = spec->hdr.next_proto_id;
	ipv4->src_ipv4     = rte_be_to_cpu_32(spec->hdr.src_addr);
	ipv4->dst_ipv4     = rte_be_to_cpu_32(spec->hdr.dst_addr);

	return 0;
}

static int
nfp_net_flow_merge_ipv6(struct rte_flow *nfp_flow,
		const struct rte_flow_item *item,
		const struct nfp_net_flow_item_proc *proc)
{
	uint32_t i;
	struct nfp_net_cmsg_match_v6 *ipv6;
	const struct rte_flow_item_ipv6 *mask;
	const struct rte_flow_item_ipv6 *spec;

	nfp_flow->payload.cmsg_type = NFP_NET_CFG_MBOX_CMD_FS_ADD_V6;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "NFP flow merge ipv6: no item->spec!");
		return 0;
	}

	mask = (item->mask != NULL) ? item->mask : proc->mask_default;

	ipv6 = (struct nfp_net_cmsg_match_v6 *)nfp_flow->payload.match_data;

	ipv6->l4_protocol_mask = mask->hdr.proto;
	for (i = 0; i < sizeof(ipv6->src_ipv6); i += 4) {
		ipv6->src_ipv6_mask[i] = mask->hdr.src_addr.a[i + 3];
		ipv6->src_ipv6_mask[i + 1] = mask->hdr.src_addr.a[i + 2];
		ipv6->src_ipv6_mask[i + 2] = mask->hdr.src_addr.a[i + 1];
		ipv6->src_ipv6_mask[i + 3] = mask->hdr.src_addr.a[i];

		ipv6->dst_ipv6_mask[i] = mask->hdr.dst_addr.a[i + 3];
		ipv6->dst_ipv6_mask[i + 1] = mask->hdr.dst_addr.a[i + 2];
		ipv6->dst_ipv6_mask[i + 2] = mask->hdr.dst_addr.a[i + 1];
		ipv6->dst_ipv6_mask[i + 3] = mask->hdr.dst_addr.a[i];
	}

	ipv6->l4_protocol = spec->hdr.proto;
	for (i = 0; i < sizeof(ipv6->src_ipv6); i += 4) {
		ipv6->src_ipv6[i] = spec->hdr.src_addr.a[i + 3];
		ipv6->src_ipv6[i + 1] = spec->hdr.src_addr.a[i + 2];
		ipv6->src_ipv6[i + 2] = spec->hdr.src_addr.a[i + 1];
		ipv6->src_ipv6[i + 3] = spec->hdr.src_addr.a[i];

		ipv6->dst_ipv6[i] = spec->hdr.dst_addr.a[i + 3];
		ipv6->dst_ipv6[i + 1] = spec->hdr.dst_addr.a[i + 2];
		ipv6->dst_ipv6[i + 2] = spec->hdr.dst_addr.a[i + 1];
		ipv6->dst_ipv6[i + 3] = spec->hdr.dst_addr.a[i];
	}

	return 0;
}

static int
nfp_flow_merge_l4(struct rte_flow *nfp_flow,
		const struct rte_flow_item *item,
		const struct nfp_net_flow_item_proc *proc)
{
	const struct rte_flow_item_tcp *mask;
	const struct rte_flow_item_tcp *spec;
	struct nfp_net_cmsg_match_v4 *ipv4 = NULL;
	struct nfp_net_cmsg_match_v6 *ipv6 = NULL;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(ERR, "NFP flow merge tcp: no item->spec!");
		return -EINVAL;
	}

	mask = (item->mask != NULL) ? item->mask : proc->mask_default;

	switch (nfp_flow->payload.cmsg_type) {
	case NFP_NET_CFG_MBOX_CMD_FS_ADD_V4:
		ipv4 = (struct nfp_net_cmsg_match_v4 *)nfp_flow->payload.match_data;
		break;
	case NFP_NET_CFG_MBOX_CMD_FS_ADD_V6:
		ipv6 = (struct nfp_net_cmsg_match_v6 *)nfp_flow->payload.match_data;
		break;
	default:
		PMD_DRV_LOG(ERR, "L3 layer neither IPv4 nor IPv6.");
		return -EINVAL;
	}

	if (ipv4 != NULL) {
		ipv4->src_port_mask = rte_be_to_cpu_16(mask->hdr.src_port);
		ipv4->dst_port_mask = rte_be_to_cpu_16(mask->hdr.dst_port);

		ipv4->src_port = rte_be_to_cpu_16(spec->hdr.src_port);
		ipv4->dst_port = rte_be_to_cpu_16(spec->hdr.dst_port);
	} else if (ipv6 != NULL) {
		ipv6->src_port_mask = rte_be_to_cpu_16(mask->hdr.src_port);
		ipv6->dst_port_mask = rte_be_to_cpu_16(mask->hdr.dst_port);

		ipv6->src_port = rte_be_to_cpu_16(spec->hdr.src_port);
		ipv6->dst_port = rte_be_to_cpu_16(spec->hdr.dst_port);
	} else {
		PMD_DRV_LOG(ERR, "No valid L3 layer pointer.");
		return -EINVAL;
	}

	return 0;
}

/* Graph of supported items and associated process function */
static const struct nfp_net_flow_item_proc nfp_net_flow_item_proc_list[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH,
				RTE_FLOW_ITEM_TYPE_IPV4,
				RTE_FLOW_ITEM_TYPE_IPV6),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.merge = nfp_net_flow_merge_eth,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_TCP,
				RTE_FLOW_ITEM_TYPE_UDP,
				RTE_FLOW_ITEM_TYPE_SCTP),
		.mask_support = &(const struct rte_flow_item_ipv4){
			.hdr = {
				.next_proto_id = 0xff,
				.src_addr      = RTE_BE32(0xffffffff),
				.dst_addr      = RTE_BE32(0xffffffff),
			},
		},
		.mask_default = &rte_flow_item_ipv4_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.merge = nfp_net_flow_merge_ipv4,
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_TCP,
				RTE_FLOW_ITEM_TYPE_UDP,
				RTE_FLOW_ITEM_TYPE_SCTP),
		.mask_support = &(const struct rte_flow_item_ipv6){
			.hdr = {
				.proto    = 0xff,
				.src_addr = RTE_IPV6_MASK_FULL,
				.dst_addr = RTE_IPV6_MASK_FULL,
			},
		},
		.mask_default = &rte_flow_item_ipv6_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv6),
		.merge = nfp_net_flow_merge_ipv6,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.mask_support = &(const struct rte_flow_item_tcp){
			.hdr = {
				.src_port  = RTE_BE16(0xffff),
				.dst_port  = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_tcp_mask,
		.mask_sz = sizeof(struct rte_flow_item_tcp),
		.merge = nfp_flow_merge_l4,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.mask_support = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = RTE_BE16(0xffff),
				.dst_port = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_udp_mask,
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.merge = nfp_flow_merge_l4,
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.mask_support = &(const struct rte_flow_item_sctp){
			.hdr = {
				.src_port  = RTE_BE16(0xffff),
				.dst_port  = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_sctp_mask,
		.mask_sz = sizeof(struct rte_flow_item_sctp),
		.merge = nfp_flow_merge_l4,
	},
};

static int
nfp_net_flow_item_check(const struct rte_flow_item *item,
		const struct nfp_net_flow_item_proc *proc)
{
	uint32_t i;
	int ret = 0;
	const uint8_t *mask;

	/* item->last and item->mask cannot exist without item->spec. */
	if (item->spec == NULL) {
		if (item->mask || item->last) {
			PMD_DRV_LOG(ERR, "The 'mask' or 'last' field provided"
					" without a corresponding 'spec'.");
			return -EINVAL;
		}

		/* No spec, no mask, no problem. */
		return 0;
	}

	mask = (item->mask != NULL) ? item->mask : proc->mask_default;

	/*
	 * Single-pass check to make sure that:
	 * - Mask is supported, no bits are set outside proc->mask_support.
	 * - Both item->spec and item->last are included in mask.
	 */
	for (i = 0; i != proc->mask_sz; ++i) {
		if (mask[i] == 0)
			continue;

		if ((mask[i] | ((const uint8_t *)proc->mask_support)[i]) !=
				((const uint8_t *)proc->mask_support)[i]) {
			PMD_DRV_LOG(ERR, "Unsupported field found in 'mask'.");
			ret = -EINVAL;
			break;
		}

		if (item->last != NULL &&
				(((const uint8_t *)item->spec)[i] & mask[i]) !=
				(((const uint8_t *)item->last)[i] & mask[i])) {
			PMD_DRV_LOG(ERR, "Range between 'spec' and 'last'"
					" is larger than 'mask'.");
			ret = -ERANGE;
			break;
		}
	}

	return ret;
}

static int
nfp_net_flow_compile_items(const struct rte_flow_item items[],
		struct rte_flow *nfp_flow)
{
	uint32_t i;
	int ret = 0;
	const struct rte_flow_item *item;
	const struct nfp_net_flow_item_proc *proc_list;

	proc_list = nfp_net_flow_item_proc_list;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		const struct nfp_net_flow_item_proc *proc = NULL;

		for (i = 0; (proc_list->next_item != NULL) &&
				(proc_list->next_item[i] != RTE_FLOW_ITEM_TYPE_END); ++i) {
			if (proc_list->next_item[i] == item->type) {
				proc = &nfp_net_flow_item_proc_list[item->type];
				break;
			}
		}

		if (proc == NULL) {
			PMD_DRV_LOG(ERR, "No next item provided for %d.", item->type);
			ret = -ENOTSUP;
			break;
		}

		/* Perform basic sanity checks */
		ret = nfp_net_flow_item_check(item, proc);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "NFP flow item %d check failed.", item->type);
			ret = -EINVAL;
			break;
		}

		if (proc->merge == NULL) {
			PMD_DRV_LOG(ERR, "NFP flow item %d no proc function.", item->type);
			ret = -ENOTSUP;
			break;
		}

		ret = proc->merge(nfp_flow, item, proc);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "NFP flow item %d exact merge failed.", item->type);
			break;
		}

		proc_list = proc;
	}

	return ret;
}

static void
nfp_net_flow_action_drop(struct rte_flow *nfp_flow)
{
	struct nfp_net_cmsg_action *action_data;

	action_data = (struct nfp_net_cmsg_action *)nfp_flow->payload.action_data;

	action_data->action = NFP_NET_CMSG_ACTION_DROP;
}

static void
nfp_net_flow_action_mark(struct rte_flow *nfp_flow,
		const struct rte_flow_action *action)
{
	struct nfp_net_cmsg_action *action_data;
	const struct rte_flow_action_mark *mark;

	action_data = (struct nfp_net_cmsg_action *)nfp_flow->payload.action_data;
	mark = action->conf;

	action_data->action |= NFP_NET_CMSG_ACTION_MARK;
	action_data->mark_id = mark->id;
}

static int
nfp_net_flow_action_queue(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		const struct rte_flow_action *action)
{
	struct nfp_net_cmsg_action *action_data;
	const struct rte_flow_action_queue *queue;

	action_data = (struct nfp_net_cmsg_action *)nfp_flow->payload.action_data;
	queue = action->conf;
	if (queue->index >= dev->data->nb_rx_queues ||
			dev->data->rx_queues[queue->index] == NULL) {
		PMD_DRV_LOG(ERR, "Queue index is illegal.");
		return -EINVAL;
	}

	action_data->action |= NFP_NET_CMSG_ACTION_QUEUE;
	action_data->queue = queue->index;

	return 0;
}

static int
nfp_net_flow_compile_actions(struct rte_eth_dev *dev,
		const struct rte_flow_action actions[],
		struct rte_flow *nfp_flow)
{
	int ret = 0;
	const struct rte_flow_action *action;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_DROP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_DROP.");
			nfp_net_flow_action_drop(nfp_flow);
			return 0;
		case RTE_FLOW_ACTION_TYPE_MARK:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_MARK.");
			nfp_net_flow_action_mark(nfp_flow, action);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_QUEUE.");
			ret = nfp_net_flow_action_queue(dev, nfp_flow, action);
			break;
		default:
			PMD_DRV_LOG(ERR, "Unsupported action type: %d.", action->type);
			return -ENOTSUP;
		}
	}

	return ret;
}

static void
nfp_net_flow_process_priority(struct rte_flow *nfp_flow,
		uint32_t match_len)
{
	struct nfp_net_cmsg_match_v4 *ipv4;
	struct nfp_net_cmsg_match_v6 *ipv6;

	switch (match_len) {
	case sizeof(struct nfp_net_cmsg_match_v4):
		ipv4 = (struct nfp_net_cmsg_match_v4 *)nfp_flow->payload.match_data;
		ipv4->position = nfp_flow->position;
		break;
	case sizeof(struct nfp_net_cmsg_match_v6):
		ipv6 = (struct nfp_net_cmsg_match_v6 *)nfp_flow->payload.match_data;
		ipv6->position = nfp_flow->position;
		break;
	default:
		break;
	}
}

static int
nfp_net_flow_check_count(struct nfp_net_flow_count *flow_count,
		uint32_t item_type)
{
	int ret = 0;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		if (flow_count->eth_count >= NFP_NET_ETH_FLOW_LIMIT)
			ret = -ENOSPC;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		if (flow_count->ipv4_count >= NFP_NET_IPV4_FLOW_LIMIT)
			ret = -ENOSPC;
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		if (flow_count->ipv6_count >= NFP_NET_IPV6_FLOW_LIMIT)
			ret = -ENOSPC;
		break;
	default:
		ret = -ENOTSUP;
		break;
	}

	return ret;
}

static int
nfp_net_flow_calculate_count(struct rte_flow *nfp_flow,
		struct nfp_net_flow_count *flow_count,
		bool delete_flag)
{
	uint16_t *count;

	switch (nfp_flow->payload.cmsg_type) {
	case NFP_NET_CFG_MBOX_CMD_FS_ADD_V4:
	case NFP_NET_CFG_MBOX_CMD_FS_DEL_V4:
		count = &flow_count->ipv4_count;
		break;
	case NFP_NET_CFG_MBOX_CMD_FS_ADD_V6:
	case NFP_NET_CFG_MBOX_CMD_FS_DEL_V6:
		count = &flow_count->ipv6_count;
		break;
	case NFP_NET_CFG_MBOX_CMD_FS_ADD_ETHTYPE:
	case NFP_NET_CFG_MBOX_CMD_FS_DEL_ETHTYPE:
		count = &flow_count->eth_count;
		break;
	default:
		PMD_DRV_LOG(ERR, "Flow count calculate failed.");
		return -EINVAL;
	}

	if (delete_flag)
		(*count)--;
	else
		(*count)++;

	return 0;
}

static struct rte_flow *
nfp_net_flow_setup(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[])
{
	int ret;
	char *hash_data;
	uint32_t port_id;
	uint32_t item_type;
	uint32_t action_len;
	struct nfp_net_hw *hw;
	uint32_t match_len = 0;
	struct nfp_net_priv *priv;
	struct rte_flow *nfp_flow;
	struct rte_flow *flow_find;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw_priv->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	ret = nfp_net_flow_calculate_items(items, &match_len, &item_type);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Key layers calculate failed.");
		return NULL;
	}

	ret = nfp_net_flow_check_count(&priv->flow_count, item_type);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Flow count check failed.");
		return NULL;
	}

	action_len = sizeof(struct nfp_net_cmsg_action);
	port_id = ((struct nfp_net_hw *)dev->data->dev_private)->nfp_idx;

	nfp_flow = nfp_net_flow_alloc(priv, attr->priority, match_len, action_len, port_id);
	if (nfp_flow == NULL) {
		PMD_DRV_LOG(ERR, "Alloc nfp flow failed.");
		return NULL;
	}

	ret = nfp_net_flow_compile_items(items, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "NFP flow item process failed.");
		goto free_flow;
	}

	ret = nfp_net_flow_compile_actions(dev, actions, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "NFP flow action process failed.");
		goto free_flow;
	}

	/* Calculate and store the hash_key for later use */
	hash_data = nfp_flow->payload.match_data;
	nfp_flow->hash_key = rte_jhash(hash_data, match_len + action_len,
			priv->hash_seed);

	/* Find the flow in hash table */
	flow_find = nfp_net_flow_table_search(priv, nfp_flow);
	if (flow_find != NULL) {
		PMD_DRV_LOG(ERR, "This flow is already exist.");
		goto free_flow;
	}

	ret = nfp_net_flow_calculate_count(nfp_flow, &priv->flow_count, false);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "NFP flow calculate count failed.");
		goto free_flow;
	}

	nfp_net_flow_process_priority(nfp_flow, match_len);

	return nfp_flow;

free_flow:
	nfp_net_flow_free(priv, nfp_flow);

	return NULL;
}

static int
nfp_net_flow_teardown(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	return nfp_net_flow_calculate_count(nfp_flow, &priv->flow_count, true);
}

static int
nfp_net_flow_offload(struct nfp_net_hw *hw,
		struct rte_flow *flow,
		bool delete_flag)
{
	int ret;
	char *tmp;
	uint32_t msg_size;
	struct nfp_net_cmsg *cmsg;

	msg_size = sizeof(uint32_t) + flow->payload.match_len +
			flow->payload.action_len;
	cmsg = nfp_net_cmsg_alloc(msg_size);
	if (cmsg == NULL) {
		PMD_DRV_LOG(ERR, "Alloc cmsg failed.");
		return -ENOMEM;
	}

	cmsg->cmd = flow->payload.cmsg_type;
	if (delete_flag)
		cmsg->cmd++;

	tmp = (char *)cmsg->data;
	rte_memcpy(tmp, flow->payload.match_data, flow->payload.match_len);
	tmp += flow->payload.match_len;
	rte_memcpy(tmp, flow->payload.action_data, flow->payload.action_len);

	ret = nfp_net_cmsg_xmit(hw, cmsg, msg_size);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Send cmsg failed.");
		ret = -EINVAL;
		goto free_cmsg;
	}

free_cmsg:
	nfp_net_cmsg_free(cmsg);

	return ret;
}

static int
nfp_net_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct nfp_net_priv *priv;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw_priv->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	nfp_flow = nfp_net_flow_setup(dev, attr, items, actions);
	if (nfp_flow == NULL) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
	}

	ret = nfp_net_flow_teardown(priv, nfp_flow);
	if (ret != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow resource free failed.");
	}

	nfp_net_flow_free(priv, nfp_flow);

	return 0;
}

static struct rte_flow *
nfp_net_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct nfp_net_priv *priv;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw_priv->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	nfp_flow = nfp_net_flow_setup(dev, attr, items, actions);
	if (nfp_flow == NULL) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
		return NULL;
	}

	/* Add the flow to flow hash table */
	ret = nfp_net_flow_table_add(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add flow to the flow table failed.");
		goto flow_teardown;
	}

	/* Add the flow to hardware */
	ret = nfp_net_flow_offload(hw, nfp_flow, false);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add flow to firmware failed.");
		goto table_delete;
	}

	return nfp_flow;

table_delete:
	nfp_net_flow_table_delete(priv, nfp_flow);
flow_teardown:
	nfp_net_flow_teardown(priv, nfp_flow);
	nfp_net_flow_free(priv, nfp_flow);

	return NULL;
}

static int
nfp_net_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct nfp_net_priv *priv;
	struct rte_flow *flow_find;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw_priv->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	/* Find the flow in flow hash table */
	flow_find = nfp_net_flow_table_search(priv, nfp_flow);
	if (flow_find == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow does not exist.");
		ret = -EINVAL;
		goto exit;
	}

	/* Delete the flow from hardware */
	ret = nfp_net_flow_offload(hw, nfp_flow, true);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete flow from firmware failed.");
		ret = -EINVAL;
		goto exit;
	}

	/* Delete the flow from flow hash table */
	ret = nfp_net_flow_table_delete(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete flow from the flow table failed.");
		ret = -EINVAL;
		goto exit;
	}

	ret = nfp_net_flow_teardown(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow teardown failed.");
		ret = -EINVAL;
		goto exit;
	}

exit:
	nfp_net_flow_free(priv, nfp_flow);

	return ret;
}

static int
nfp_net_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	int ret = 0;
	void *next_data;
	uint32_t iter = 0;
	const void *next_key;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct rte_hash *flow_table;
	struct nfp_net_hw_priv *hw_priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	hw_priv = dev->process_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw_priv->pf_dev->app_fw_priv);
	flow_table = app_fw_nic->ports[hw->idx]->priv->flow_table;

	while (rte_hash_iterate(flow_table, &next_key, &next_data, &iter) >= 0) {
		nfp_flow = next_data;
		ret = nfp_net_flow_destroy(dev, nfp_flow, error);
		if (ret != 0)
			break;
	}

	return ret;
}

static const struct rte_flow_ops nfp_net_flow_ops = {
	.validate                = nfp_net_flow_validate,
	.create                  = nfp_net_flow_create,
	.destroy                 = nfp_net_flow_destroy,
	.flush                   = nfp_net_flow_flush,
};

int
nfp_net_flow_ops_get(struct rte_eth_dev *dev,
		const struct rte_flow_ops **ops)
{
	struct nfp_net_hw *hw;

	if (rte_eth_dev_is_repr(dev)) {
		*ops = NULL;
		PMD_DRV_LOG(ERR, "Port is a representor.");
		return -EINVAL;
	}

	hw = dev->data->dev_private;
	if ((hw->super.ctrl_ext & NFP_NET_CFG_CTRL_FLOW_STEER) == 0) {
		*ops = NULL;
		return 0;
	}

	*ops = &nfp_net_flow_ops;

	return 0;
}

static uint32_t
nfp_net_fs_max_entry_get(struct nfp_hw *hw)
{
	uint32_t cnt;

	cnt = nn_cfg_readl(hw, NFP_NET_CFG_MAX_FS_CAP);
	if (cnt != 0)
		return cnt;

	return NFP_NET_FLOW_LIMIT;
}

int
nfp_net_flow_priv_init(struct nfp_pf_dev *pf_dev,
		uint16_t port)
{
	int ret = 0;
	struct nfp_hw *hw;
	struct nfp_net_priv *priv;
	char flow_name[RTE_HASH_NAMESIZE];
	struct nfp_app_fw_nic *app_fw_nic;
	const char *pci_name = strchr(pf_dev->pci_dev->name, ':') + 1;

	snprintf(flow_name, sizeof(flow_name), "%s_fl_%u", pci_name, port);

	struct rte_hash_parameters flow_hash_params = {
		.name       = flow_name,
		.hash_func  = rte_jhash,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(uint32_t),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY,
	};

	priv = rte_zmalloc("nfp_app_nic_priv", sizeof(struct nfp_net_priv), 0);
	if (priv == NULL) {
		PMD_INIT_LOG(ERR, "NFP app nic priv creation failed.");
		ret = -ENOMEM;
		goto exit;
	}

	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);
	app_fw_nic->ports[port]->priv = priv;
	priv->hash_seed = (uint32_t)rte_rand();

	/* Flow limit */
	hw = &app_fw_nic->ports[port]->super;
	priv->flow_limit = nfp_net_fs_max_entry_get(hw);
	if (priv->flow_limit == 0) {
		PMD_INIT_LOG(ERR, "NFP app nic flow limit not right.");
		ret = -EINVAL;
		goto free_priv;
	}

	/* Flow position array */
	priv->flow_position = rte_zmalloc(NULL, sizeof(bool) * priv->flow_limit, 0);
	if (priv->flow_position == NULL) {
		PMD_INIT_LOG(ERR, "NFP app nic flow position creation failed.");
		ret = -ENOMEM;
		goto free_priv;
	}

	/* Flow table */
	flow_hash_params.hash_func_init_val = priv->hash_seed;
	flow_hash_params.entries = priv->flow_limit * NFP_NET_HASH_REDUNDANCE;
	priv->flow_table = rte_hash_create(&flow_hash_params);
	if (priv->flow_table == NULL) {
		PMD_INIT_LOG(ERR, "Flow hash table creation failed.");
		ret = -ENOMEM;
		goto free_flow_position;
	}

	return 0;

free_flow_position:
	rte_free(priv->flow_position);
free_priv:
	rte_free(priv);
exit:
	return ret;
}

void
nfp_net_flow_priv_uninit(struct nfp_pf_dev *pf_dev,
		uint16_t port)
{
	struct nfp_net_priv *priv;
	struct nfp_app_fw_nic *app_fw_nic;

	if (pf_dev == NULL)
		return;

	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[port]->priv;
	if (priv != NULL) {
		rte_hash_free(priv->flow_table);
		rte_free(priv->flow_position);
	}

	rte_free(priv);
}
