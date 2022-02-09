/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#include "rte_malloc.h"
#include "igc_logs.h"
#include "igc_txrx.h"
#include "igc_filter.h"
#include "igc_flow.h"

/*******************************************************************************
 * All Supported Rule Type
 *
 * Notes:
 * `para` or `(para)`, the para must been set
 * `[para]`, the para is optional
 * `([para1][para2]...)`, all paras is optional, but must one of them been set
 * `para1 | para2 | ...`, only one of the paras can be set
 *
 * ether-type filter
 * pattern: ETH(type)/END
 * action: QUEUE/END
 * attribute:
 *
 * n-tuple filter
 * pattern: [ETH/]([IPv4(protocol)|IPv6(protocol)/][UDP(dst_port)|
 *          TCP([dst_port],[flags])|SCTP(dst_port)/])END
 * action: QUEUE/END
 * attribute: [priority(0-7)]
 *
 * SYN filter
 * pattern: [ETH/][IPv4|IPv6/]TCP(flags=SYN)/END
 * action: QUEUE/END
 * attribute: [priority(0,1)]
 *
 * RSS filter
 * pattern:
 * action: RSS/END
 * attribute:
 ******************************************************************************/

/* Structure to store all filters */
struct igc_all_filter {
	struct igc_ethertype_filter ethertype;
	struct igc_ntuple_filter ntuple;
	struct igc_syn_filter syn;
	struct igc_rss_filter rss;
	uint32_t	mask;	/* see IGC_FILTER_MASK_* definition */
};

#define IGC_FILTER_MASK_ETHER		(1u << IGC_FILTER_TYPE_ETHERTYPE)
#define IGC_FILTER_MASK_NTUPLE		(1u << IGC_FILTER_TYPE_NTUPLE)
#define IGC_FILTER_MASK_TCP_SYN		(1u << IGC_FILTER_TYPE_SYN)
#define IGC_FILTER_MASK_RSS		(1u << IGC_FILTER_TYPE_HASH)
#define IGC_FILTER_MASK_ALL		(IGC_FILTER_MASK_ETHER |	\
					IGC_FILTER_MASK_NTUPLE |	\
					IGC_FILTER_MASK_TCP_SYN |	\
					IGC_FILTER_MASK_RSS)

#define IGC_SET_FILTER_MASK(_filter, _mask_bits)			\
					((_filter)->mask &= (_mask_bits))

#define IGC_IS_ALL_BITS_SET(_val)	((_val) == (typeof(_val))~0)
#define IGC_NOT_ALL_BITS_SET(_val)	((_val) != (typeof(_val))~0)

/* Parse rule attribute */
static int
igc_parse_attribute(const struct rte_flow_attr *attr,
	struct igc_all_filter *filter, struct rte_flow_error *error)
{
	if (!attr)
		return 0;

	if (attr->group)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_GROUP, attr,
				"Not support");

	if (attr->egress)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attr,
				"Not support");

	if (attr->transfer)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, attr,
				"Not support");

	if (!attr->ingress)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, attr,
				"A rule must apply to ingress traffic");

	if (attr->priority == 0)
		return 0;

	/* only n-tuple and SYN filter have priority level */
	IGC_SET_FILTER_MASK(filter,
		IGC_FILTER_MASK_NTUPLE | IGC_FILTER_MASK_TCP_SYN);

	if (IGC_IS_ALL_BITS_SET(attr->priority)) {
		/* only SYN filter match this value */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_TCP_SYN);
		filter->syn.hig_pri = 1;
		return 0;
	}

	if (attr->priority > IGC_NTUPLE_MAX_PRI)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, attr,
				"Priority value is invalid.");

	if (attr->priority > 1) {
		/* only n-tuple filter match this value */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

		/* get priority */
		filter->ntuple.tuple_info.priority = (uint8_t)attr->priority;
		return 0;
	}

	/* get priority */
	filter->ntuple.tuple_info.priority = (uint8_t)attr->priority;
	filter->syn.hig_pri = (uint8_t)attr->priority;

	return 0;
}

/* function type of parse pattern */
typedef int (*igc_pattern_parse)(const struct rte_flow_item *,
		struct igc_all_filter *, struct rte_flow_error *);

static int igc_parse_pattern_void(__rte_unused const struct rte_flow_item *item,
		__rte_unused struct igc_all_filter *filter,
		__rte_unused struct rte_flow_error *error);
static int igc_parse_pattern_ether(const struct rte_flow_item *item,
		struct igc_all_filter *filter, struct rte_flow_error *error);
static int igc_parse_pattern_ip(const struct rte_flow_item *item,
		struct igc_all_filter *filter, struct rte_flow_error *error);
static int igc_parse_pattern_ipv6(const struct rte_flow_item *item,
		struct igc_all_filter *filter, struct rte_flow_error *error);
static int igc_parse_pattern_udp(const struct rte_flow_item *item,
		struct igc_all_filter *filter, struct rte_flow_error *error);
static int igc_parse_pattern_tcp(const struct rte_flow_item *item,
		struct igc_all_filter *filter, struct rte_flow_error *error);

static igc_pattern_parse pattern_parse_list[] = {
		[RTE_FLOW_ITEM_TYPE_VOID] = igc_parse_pattern_void,
		[RTE_FLOW_ITEM_TYPE_ETH] = igc_parse_pattern_ether,
		[RTE_FLOW_ITEM_TYPE_IPV4] = igc_parse_pattern_ip,
		[RTE_FLOW_ITEM_TYPE_IPV6] = igc_parse_pattern_ipv6,
		[RTE_FLOW_ITEM_TYPE_UDP] = igc_parse_pattern_udp,
		[RTE_FLOW_ITEM_TYPE_TCP] = igc_parse_pattern_tcp,
};

/* Parse rule patterns */
static int
igc_parse_patterns(const struct rte_flow_item patterns[],
	struct igc_all_filter *filter, struct rte_flow_error *error)
{
	const struct rte_flow_item *item = patterns;

	if (item == NULL) {
		/* only RSS filter match this pattern */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_RSS);
		return 0;
	}

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		int ret;

		if (item->type >= RTE_DIM(pattern_parse_list))
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not been supported");

		if (item->last)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_LAST, item,
					"Range not been supported");

		/* check pattern format is valid */
		if (!!item->spec ^ !!item->mask)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Format error");

		/* get the pattern type callback */
		igc_pattern_parse parse_func =
				pattern_parse_list[item->type];
		if (!parse_func)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not been supported");

		/* call the pattern type function */
		ret = parse_func(item, filter, error);
		if (ret)
			return ret;

		/* if no filter match the pattern */
		if (filter->mask == 0)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Not been supported");
	}

	return 0;
}

static int igc_parse_action_queue(struct rte_eth_dev *dev,
		const struct rte_flow_action *act,
		struct igc_all_filter *filter, struct rte_flow_error *error);
static int igc_parse_action_rss(struct rte_eth_dev *dev,
		const struct rte_flow_action *act,
		struct igc_all_filter *filter, struct rte_flow_error *error);

/* Parse flow actions */
static int
igc_parse_actions(struct rte_eth_dev *dev,
		const struct rte_flow_action actions[],
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_action *act = actions;
	int ret;

	if (act == NULL)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_NUM, act,
				"Action is needed");

	for (; act->type != RTE_FLOW_ACTION_TYPE_END; act++) {
		switch (act->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = igc_parse_action_queue(dev, act, filter, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = igc_parse_action_rss(dev, act, filter, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, act,
					"Not been supported");
		}

		/* if no filter match the action */
		if (filter->mask == 0)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, act,
					"Not been supported");
	}

	return 0;
}

/* Parse a flow rule */
static int
igc_parse_flow(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item patterns[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error,
		struct igc_all_filter *filter)
{
	int ret;

	/* clear all filters */
	memset(filter, 0, sizeof(*filter));

	/* set default filter mask */
	filter->mask = IGC_FILTER_MASK_ALL;

	ret = igc_parse_attribute(attr, filter, error);
	if (ret)
		return ret;

	ret = igc_parse_patterns(patterns, filter, error);
	if (ret)
		return ret;

	ret = igc_parse_actions(dev, actions, filter, error);
	if (ret)
		return ret;

	/* if no or more than one filter matched this flow */
	if (filter->mask == 0 || (filter->mask & (filter->mask - 1)))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"Flow can't be recognized");
	return 0;
}

/* Parse pattern type of void */
static int
igc_parse_pattern_void(__rte_unused const struct rte_flow_item *item,
		__rte_unused struct igc_all_filter *filter,
		__rte_unused struct rte_flow_error *error)
{
	return 0;
}

/* Parse pattern type of ethernet header */
static int
igc_parse_pattern_ether(const struct rte_flow_item *item,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	struct igc_ethertype_filter *ether;

	if (mask == NULL) {
		/* only n-tuple and SYN filter match the pattern */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE |
				IGC_FILTER_MASK_TCP_SYN);
		return 0;
	}

	/* only ether-type filter match the pattern*/
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_ETHER);

	/* destination and source MAC address are not supported */
	if (!rte_is_zero_ether_addr(&mask->src) ||
		!rte_is_zero_ether_addr(&mask->dst))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Only support ether-type");

	/* ether-type mask bits must be all 1 */
	if (IGC_NOT_ALL_BITS_SET(mask->type))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"Ethernet type mask bits must be all 1");

	ether = &filter->ethertype;

	/* get ether-type */
	ether->ether_type = rte_be_to_cpu_16(spec->type);

	/* ether-type should not be IPv4 and IPv6 */
	if (ether->ether_type == RTE_ETHER_TYPE_IPV4 ||
		ether->ether_type == RTE_ETHER_TYPE_IPV6 ||
		ether->ether_type == 0)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM, NULL,
			"IPv4/IPv6/0 not supported by ethertype filter");
	return 0;
}

/* Parse pattern type of IP */
static int
igc_parse_pattern_ip(const struct rte_flow_item *item,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;

	if (mask == NULL) {
		/* only n-tuple and SYN filter match this pattern */
		IGC_SET_FILTER_MASK(filter,
			IGC_FILTER_MASK_NTUPLE | IGC_FILTER_MASK_TCP_SYN);
		return 0;
	}

	/* only n-tuple filter match this pattern */
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

	/* only protocol is used */
	if (mask->hdr.version_ihl ||
		mask->hdr.type_of_service ||
		mask->hdr.total_length ||
		mask->hdr.packet_id ||
		mask->hdr.fragment_offset ||
		mask->hdr.time_to_live ||
		mask->hdr.hdr_checksum ||
		mask->hdr.dst_addr ||
		mask->hdr.src_addr)
		return rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
			"IPv4 only support protocol");

	if (mask->hdr.next_proto_id == 0)
		return 0;

	if (IGC_NOT_ALL_BITS_SET(mask->hdr.next_proto_id))
		return rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"IPv4 protocol mask bits must be all 0 or 1");

	/* get protocol type */
	filter->ntuple.tuple_info.proto_mask = 1;
	filter->ntuple.tuple_info.proto = spec->hdr.next_proto_id;
	return 0;
}

/*
 * Check ipv6 address is 0
 * Return 1 if true, 0 for false.
 */
static inline bool
igc_is_zero_ipv6_addr(const void *ipv6_addr)
{
	const uint64_t *ddw = ipv6_addr;
	return ddw[0] == 0 && ddw[1] == 0;
}

/* Parse pattern type of IPv6 */
static int
igc_parse_pattern_ipv6(const struct rte_flow_item *item,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;

	if (mask == NULL) {
		/* only n-tuple and syn filter match this pattern */
		IGC_SET_FILTER_MASK(filter,
			IGC_FILTER_MASK_NTUPLE | IGC_FILTER_MASK_TCP_SYN);
		return 0;
	}

	/* only n-tuple filter match this pattern */
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

	/* only protocol is used */
	if (mask->hdr.vtc_flow ||
		mask->hdr.payload_len ||
		mask->hdr.hop_limits ||
		!igc_is_zero_ipv6_addr(mask->hdr.src_addr) ||
		!igc_is_zero_ipv6_addr(mask->hdr.dst_addr))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"IPv6 only support protocol");

	if (mask->hdr.proto == 0)
		return 0;

	if (IGC_NOT_ALL_BITS_SET(mask->hdr.proto))
		return rte_flow_error_set(error,
				EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"IPv6 protocol mask bits must be all 0 or 1");

	/* get protocol type */
	filter->ntuple.tuple_info.proto_mask = 1;
	filter->ntuple.tuple_info.proto = spec->hdr.proto;

	return 0;
}

/* Parse pattern type of UDP */
static int
igc_parse_pattern_udp(const struct rte_flow_item *item,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;

	/* only n-tuple filter match this pattern */
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

	if (mask == NULL)
		return 0;

	/* only destination port is used */
	if (mask->hdr.dgram_len || mask->hdr.dgram_cksum || mask->hdr.src_port)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
			"UDP only support destination port");

	if (mask->hdr.dst_port == 0)
		return 0;

	if (IGC_NOT_ALL_BITS_SET(mask->hdr.dst_port))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"UDP port mask bits must be all 0 or 1");

	/* get destination port info. */
	filter->ntuple.tuple_info.dst_port_mask = 1;
	filter->ntuple.tuple_info.dst_port = spec->hdr.dst_port;

	return 0;
}

/* Parse pattern type of TCP */
static int
igc_parse_pattern_tcp(const struct rte_flow_item *item,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	struct igc_ntuple_info *tuple_info = &filter->ntuple.tuple_info;

	if (mask == NULL) {
		/* only n-tuple filter match this pattern */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);
		return 0;
	}

	/* only n-tuple and SYN filter match this pattern */
	IGC_SET_FILTER_MASK(filter,
			IGC_FILTER_MASK_NTUPLE | IGC_FILTER_MASK_TCP_SYN);

	/* only destination port and TCP flags are used */
	if (mask->hdr.sent_seq ||
		mask->hdr.recv_ack ||
		mask->hdr.data_off ||
		mask->hdr.rx_win ||
		mask->hdr.cksum ||
		mask->hdr.tcp_urp ||
		mask->hdr.src_port)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
			"TCP only support destination port and flags");

	/* if destination port is used */
	if (mask->hdr.dst_port) {
		/* only n-tuple match this pattern */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

		if (IGC_NOT_ALL_BITS_SET(mask->hdr.dst_port))
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
				"TCP port mask bits must be all 1");

		/* get destination port info. */
		tuple_info->dst_port = spec->hdr.dst_port;
		tuple_info->dst_port_mask = 1;
	}

	/* if TCP flags are used */
	if (mask->hdr.tcp_flags) {
		if (IGC_IS_ALL_BITS_SET(mask->hdr.tcp_flags)) {
			/* only n-tuple match this pattern */
			IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);

			/* get TCP flags */
			tuple_info->tcp_flags = spec->hdr.tcp_flags;
		} else if (mask->hdr.tcp_flags == RTE_TCP_SYN_FLAG) {
			/* only TCP SYN filter match this pattern */
			IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_TCP_SYN);
		} else {
			/* no filter match this pattern */
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_MASK, item,
					"TCP flags can't match");
		}
	} else {
		/* only n-tuple match this pattern */
		IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_NTUPLE);
	}

	return 0;
}

static int
igc_parse_action_queue(struct rte_eth_dev *dev,
		const struct rte_flow_action *act,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	uint16_t queue_idx;

	if (act->conf == NULL)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"NULL pointer");

	/* only ether-type, n-tuple, SYN filter match the action */
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_ETHER |
			IGC_FILTER_MASK_NTUPLE | IGC_FILTER_MASK_TCP_SYN);

	/* get queue index */
	queue_idx = ((const struct rte_flow_action_queue *)act->conf)->index;

	/* check the queue index is valid */
	if (queue_idx >= dev->data->nb_rx_queues)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"Queue id is invalid");

	/* get queue info. */
	filter->ethertype.queue = queue_idx;
	filter->ntuple.queue = queue_idx;
	filter->syn.queue = queue_idx;
	return 0;
}

/* Parse action of RSS */
static int
igc_parse_action_rss(struct rte_eth_dev *dev,
		const struct rte_flow_action *act,
		struct igc_all_filter *filter,
		struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss = act->conf;
	uint32_t i;

	if (act->conf == NULL)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"NULL pointer");

	/* only RSS match the action */
	IGC_SET_FILTER_MASK(filter, IGC_FILTER_MASK_RSS);

	/* RSS redirect table can't be zero and can't exceed 128 */
	if (!rss || !rss->queue_num || rss->queue_num > IGC_RSS_RDT_SIZD)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"No valid queues");

	/* queue index can't exceed max queue index */
	for (i = 0; i < rss->queue_num; i++) {
		if (rss->queue[i] >= dev->data->nb_rx_queues)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
					"Queue id is invalid");
	}

	/* only default RSS hash function is supported */
	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"Only default RSS hash functions is supported");

	if (rss->level)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"Only 0 RSS encapsulation level is supported");

	/* check key length is valid */
	if (rss->key_len && rss->key_len != sizeof(filter->rss.key))
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF, act,
				"RSS hash key must be exactly 40 bytes");

	/* get RSS info. */
	igc_rss_conf_set(&filter->rss, rss);
	return 0;
}

/**
 * Allocate a rte_flow from the heap
 * Return the pointer of the flow, or NULL for failed
 **/
static inline struct rte_flow *
igc_alloc_flow(const void *filter, enum igc_filter_type type, size_t inbytes)
{
	/* allocate memory, 8 bytes boundary aligned */
	struct rte_flow *flow = rte_malloc("igc flow filter",
			sizeof(struct rte_flow) + inbytes, 8);
	if (flow == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		return NULL;
	}

	flow->filter_type = type;

	/* copy filter data */
	memcpy(flow->filter, filter, inbytes);
	return flow;
}

/* Append a rte_flow to the list */
static inline void
igc_append_flow(struct igc_flow_list *list, struct rte_flow *flow)
{
	TAILQ_INSERT_TAIL(list, flow, node);
}

/**
 * Remove the flow and free the flow buffer
 * The caller should make sure the flow is really exist in the list
 **/
static inline void
igc_remove_flow(struct igc_flow_list *list, struct rte_flow *flow)
{
	TAILQ_REMOVE(list, flow, node);
	rte_free(flow);
}

/* Check whether the flow is really in the list or not */
static inline bool
igc_is_flow_in_list(struct igc_flow_list *list, struct rte_flow *flow)
{
	struct rte_flow *it;

	TAILQ_FOREACH(it, list, node) {
		if (it == flow)
			return true;
	}

	return false;
}

/**
 * Create a flow rule.
 * Theoretically one rule can match more than one filters.
 * We will let it use the filter which it hit first.
 * So, the sequence matters.
 **/
static struct rte_flow *
igc_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item patterns[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	struct igc_all_filter filter;
	int ret;

	ret = igc_parse_flow(dev, attr, patterns, actions, error, &filter);
	if (ret)
		return NULL;
	ret = -ENOMEM;

	switch (filter.mask) {
	case IGC_FILTER_MASK_ETHER:
		flow = igc_alloc_flow(&filter.ethertype,
				IGC_FILTER_TYPE_ETHERTYPE,
				sizeof(filter.ethertype));
		if (flow)
			ret = igc_add_ethertype_filter(dev, &filter.ethertype);
		break;
	case IGC_FILTER_MASK_NTUPLE:
		/* Check n-tuple filter is valid */
		if (filter.ntuple.tuple_info.dst_port_mask == 0 &&
			filter.ntuple.tuple_info.proto_mask == 0) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_NONE, NULL,
					"Flow can't be recognized");
			return NULL;
		}

		flow = igc_alloc_flow(&filter.ntuple, IGC_FILTER_TYPE_NTUPLE,
				sizeof(filter.ntuple));
		if (flow)
			ret = igc_add_ntuple_filter(dev, &filter.ntuple);
		break;
	case IGC_FILTER_MASK_TCP_SYN:
		flow = igc_alloc_flow(&filter.syn, IGC_FILTER_TYPE_SYN,
				sizeof(filter.syn));
		if (flow)
			ret = igc_set_syn_filter(dev, &filter.syn);
		break;
	case IGC_FILTER_MASK_RSS:
		flow = igc_alloc_flow(&filter.rss, IGC_FILTER_TYPE_HASH,
				sizeof(filter.rss));
		if (flow) {
			struct igc_rss_filter *rss =
					(struct igc_rss_filter *)flow->filter;
			rss->conf.key = rss->key;
			rss->conf.queue = rss->queue;
			ret = igc_add_rss_filter(dev, &filter.rss);
		}
		break;
	default:
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_NONE, NULL,
				"Flow can't be recognized");
		return NULL;
	}

	if (ret) {
		rte_free(flow);
		rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to create flow.");
		return NULL;
	}

	/* append the flow to the tail of the list */
	igc_append_flow(IGC_DEV_PRIVATE_FLOW_LIST(dev), flow);
	return flow;
}

/**
 * Check if the flow rule is supported by the device.
 * It only checks the format. Don't guarantee the rule can be programmed into
 * the HW. Because there can be no enough room for the rule.
 **/
static int
igc_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item patterns[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct igc_all_filter filter;
	int ret;

	ret = igc_parse_flow(dev, attr, patterns, actions, error, &filter);
	if (ret)
		return ret;

	switch (filter.mask) {
	case IGC_FILTER_MASK_NTUPLE:
		/* Check n-tuple filter is valid */
		if (filter.ntuple.tuple_info.dst_port_mask == 0 &&
			filter.ntuple.tuple_info.proto_mask == 0)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_NONE, NULL,
					"Flow can't be recognized");
		break;
	}

	return 0;
}

/**
 * Disable a valid flow, the flow must be not NULL and
 * chained in the device flow list.
 **/
static int
igc_disable_flow(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	int ret = 0;

	switch (flow->filter_type) {
	case IGC_FILTER_TYPE_ETHERTYPE:
		ret = igc_del_ethertype_filter(dev,
			(struct igc_ethertype_filter *)&flow->filter);
		break;
	case IGC_FILTER_TYPE_NTUPLE:
		ret = igc_del_ntuple_filter(dev,
				(struct igc_ntuple_filter *)&flow->filter);
		break;
	case IGC_FILTER_TYPE_SYN:
		igc_clear_syn_filter(dev);
		break;
	case IGC_FILTER_TYPE_HASH:
		ret = igc_del_rss_filter(dev);
		break;
	default:
		PMD_DRV_LOG(ERR, "Filter type (%d) not supported",
				flow->filter_type);
		ret = -EINVAL;
	}

	return ret;
}

/* Destroy a flow rule */
static int
igc_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct igc_flow_list *list = IGC_DEV_PRIVATE_FLOW_LIST(dev);
	int ret;

	if (!flow) {
		PMD_DRV_LOG(ERR, "NULL flow!");
		return -EINVAL;
	}

	/* check the flow is create by IGC PMD */
	if (!igc_is_flow_in_list(list, flow)) {
		PMD_DRV_LOG(ERR, "Flow(%p) not been found!", flow);
		return -ENOENT;
	}

	ret = igc_disable_flow(dev, flow);
	if (ret)
		rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_HANDLE,
				NULL, "Failed to destroy flow");

	igc_remove_flow(list, flow);
	return ret;
}

/* Initiate device flow list header */
void
igc_flow_init(struct rte_eth_dev *dev)
{
	TAILQ_INIT(IGC_DEV_PRIVATE_FLOW_LIST(dev));
}

/* Destroy all flow in the list and free memory */
int
igc_flow_flush(struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_error *error)
{
	struct igc_flow_list *list = IGC_DEV_PRIVATE_FLOW_LIST(dev);
	struct rte_flow *flow;

	while ((flow = TAILQ_FIRST(list)) != NULL) {
		igc_disable_flow(dev, flow);
		igc_remove_flow(list, flow);
	}

	return 0;
}

const struct rte_flow_ops igc_flow_ops = {
	.validate = igc_flow_validate,
	.create = igc_flow_create,
	.destroy = igc_flow_destroy,
	.flush = igc_flow_flush,
};
