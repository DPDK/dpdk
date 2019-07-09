/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <rte_flow_classify.h>
#include "rte_flow_classify_parse.h"
#include <rte_flow_driver.h>

struct classify_valid_pattern {
	enum rte_flow_item_type *items;
	parse_filter_t parse_filter;
};

static struct classify_action action;

/* Pattern for IPv4 5-tuple UDP filter */
static enum rte_flow_item_type pattern_ntuple_1[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern for IPv4 5-tuple TCP filter */
static enum rte_flow_item_type pattern_ntuple_2[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_END,
};

/* Pattern for IPv4 5-tuple SCTP filter */
static enum rte_flow_item_type pattern_ntuple_3[] = {
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_END,
};

static int
classify_parse_ntuple_filter(const struct rte_flow_attr *attr,
			 const struct rte_flow_item pattern[],
			 const struct rte_flow_action actions[],
			 struct rte_eth_ntuple_filter *filter,
			 struct rte_flow_error *error);

static struct classify_valid_pattern classify_supported_patterns[] = {
	/* ntuple */
	{ pattern_ntuple_1, classify_parse_ntuple_filter },
	{ pattern_ntuple_2, classify_parse_ntuple_filter },
	{ pattern_ntuple_3, classify_parse_ntuple_filter },
};

struct classify_action *
classify_get_flow_action(void)
{
	return &action;
}

/* Find the first VOID or non-VOID item pointer */
const struct rte_flow_item *
classify_find_first_item(const struct rte_flow_item *item, bool is_void)
{
	bool is_find;

	while (item->type != RTE_FLOW_ITEM_TYPE_END) {
		if (is_void)
			is_find = item->type == RTE_FLOW_ITEM_TYPE_VOID;
		else
			is_find = item->type != RTE_FLOW_ITEM_TYPE_VOID;
		if (is_find)
			break;
		item++;
	}
	return item;
}

/* Skip all VOID items of the pattern */
void
classify_pattern_skip_void_item(struct rte_flow_item *items,
			    const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = classify_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = classify_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END) {
			pb = pe;
			break;
		}
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
classify_match_pattern(enum rte_flow_item_type *item_array,
		   struct rte_flow_item *pattern)
{
	struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

/* Find if there's parse filter function matched */
parse_filter_t
classify_find_parse_filter_func(struct rte_flow_item *pattern)
{
	parse_filter_t parse_filter = NULL;
	uint8_t i = 0;

	for (; i < RTE_DIM(classify_supported_patterns); i++) {
		if (classify_match_pattern(classify_supported_patterns[i].items,
					pattern)) {
			parse_filter =
				classify_supported_patterns[i].parse_filter;
			break;
		}
	}

	return parse_filter;
}

#define FLOW_RULE_MIN_PRIORITY 8
#define FLOW_RULE_MAX_PRIORITY 0

#define NEXT_ITEM_OF_PATTERN(item, pattern, index)\
	do {\
		item = pattern + index;\
		while (item->type == RTE_FLOW_ITEM_TYPE_VOID) {\
			index++;\
			item = pattern + index;\
		} \
	} while (0)

#define NEXT_ITEM_OF_ACTION(act, actions, index)\
	do {\
		act = actions + index;\
		while (act->type == RTE_FLOW_ACTION_TYPE_VOID) {\
			index++;\
			act = actions + index;\
		} \
	} while (0)

/**
 * Please aware there's an assumption for all the parsers.
 * rte_flow_item is using big endian, rte_flow_attr and
 * rte_flow_action are using CPU order.
 * Because the pattern is used to describe the packets,
 * normally the packets should use network order.
 */

/**
 * Parse the rule to see if it is a n-tuple rule.
 * And get the n-tuple filter info BTW.
 * pattern:
 * The first not void item can be ETH or IPV4.
 * The second not void item must be IPV4 if the first one is ETH.
 * The third not void item must be UDP or TCP.
 * The next not void item must be END.
 * action:
 * The first not void action should be QUEUE.
 * The next not void action should be END.
 * pattern example:
 * ITEM		Spec			Mask
 * ETH		NULL			NULL
 * IPV4		src_addr 192.168.1.20	0xFFFFFFFF
 *			dst_addr 192.167.3.50	0xFFFFFFFF
 *			next_proto_id	17	0xFF
 * UDP/TCP/	src_port	80	0xFFFF
 * SCTP		dst_port	80	0xFFFF
 * END
 * other members in mask and spec should set to 0x00.
 * item->last should be NULL.
 */
static int
classify_parse_ntuple_filter(const struct rte_flow_attr *attr,
			 const struct rte_flow_item pattern[],
			 const struct rte_flow_action actions[],
			 struct rte_eth_ntuple_filter *filter,
			 struct rte_flow_error *error)
{
	const struct rte_flow_item *item;
	const struct rte_flow_action *act;
	const struct rte_flow_item_ipv4 *ipv4_spec;
	const struct rte_flow_item_ipv4 *ipv4_mask;
	const struct rte_flow_item_tcp *tcp_spec;
	const struct rte_flow_item_tcp *tcp_mask;
	const struct rte_flow_item_udp *udp_spec;
	const struct rte_flow_item_udp *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec;
	const struct rte_flow_item_sctp *sctp_mask;
	const struct rte_flow_action_count *count;
	const struct rte_flow_action_mark *mark_spec;
	uint32_t index;

	/* parse pattern */
	index = 0;

	/* the first not void item can be MAC or IPv4 */
	NEXT_ITEM_OF_PATTERN(item, pattern, index);

	if (item->type != RTE_FLOW_ITEM_TYPE_ETH &&
	    item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -EINVAL;
	}
	/* Skip Ethernet */
	if (item->type == RTE_FLOW_ITEM_TYPE_ETH) {
		/*Not supported last point for range*/
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					item,
					"Not supported last point for range");
			return -EINVAL;

		}
		/* if the first item is MAC, the content should be NULL */
		if (item->spec || item->mask) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not supported by ntuple filter");
			return -EINVAL;
		}
		/* check if the next not void item is IPv4 */
		index++;
		NEXT_ITEM_OF_PATTERN(item, pattern, index);
		if (item->type != RTE_FLOW_ITEM_TYPE_IPV4) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not supported by ntuple filter");
			return -EINVAL;
		}
	}

	/* get the IPv4 info */
	if (!item->spec || !item->mask) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Invalid ntuple mask");
		return -EINVAL;
	}
	/*Not supported last point for range*/
	if (item->last) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -EINVAL;

	}

	ipv4_mask = item->mask;
	/**
	 * Only support src & dst addresses, protocol,
	 * others should be masked.
	 */
	if (ipv4_mask->hdr.version_ihl ||
		ipv4_mask->hdr.type_of_service ||
		ipv4_mask->hdr.total_length ||
		ipv4_mask->hdr.packet_id ||
		ipv4_mask->hdr.fragment_offset ||
		ipv4_mask->hdr.time_to_live ||
		ipv4_mask->hdr.hdr_checksum) {
		rte_flow_error_set(error,
			EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -EINVAL;
	}

	filter->dst_ip_mask = ipv4_mask->hdr.dst_addr;
	filter->src_ip_mask = ipv4_mask->hdr.src_addr;
	filter->proto_mask  = ipv4_mask->hdr.next_proto_id;

	ipv4_spec = item->spec;
	filter->dst_ip = ipv4_spec->hdr.dst_addr;
	filter->src_ip = ipv4_spec->hdr.src_addr;
	filter->proto  = ipv4_spec->hdr.next_proto_id;

	/* check if the next not void item is TCP or UDP or SCTP */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_TCP &&
	    item->type != RTE_FLOW_ITEM_TYPE_UDP &&
	    item->type != RTE_FLOW_ITEM_TYPE_SCTP) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -EINVAL;
	}

	/* get the TCP/UDP info */
	if (!item->spec || !item->mask) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Invalid ntuple mask");
		return -EINVAL;
	}

	/*Not supported last point for range*/
	if (item->last) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			item, "Not supported last point for range");
		return -EINVAL;

	}

	if (item->type == RTE_FLOW_ITEM_TYPE_TCP) {
		tcp_mask = item->mask;

		/**
		 * Only support src & dst ports, tcp flags,
		 * others should be masked.
		 */
		if (tcp_mask->hdr.sent_seq ||
		    tcp_mask->hdr.recv_ack ||
		    tcp_mask->hdr.data_off ||
		    tcp_mask->hdr.rx_win ||
		    tcp_mask->hdr.cksum ||
		    tcp_mask->hdr.tcp_urp) {
			memset(filter, 0,
				sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -EINVAL;
		}

		filter->dst_port_mask  = tcp_mask->hdr.dst_port;
		filter->src_port_mask  = tcp_mask->hdr.src_port;
		if (tcp_mask->hdr.tcp_flags == 0xFF) {
			filter->flags |= RTE_NTUPLE_FLAGS_TCP_FLAG;
		} else if (!tcp_mask->hdr.tcp_flags) {
			filter->flags &= ~RTE_NTUPLE_FLAGS_TCP_FLAG;
		} else {
			memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -EINVAL;
		}

		tcp_spec = item->spec;
		filter->dst_port  = tcp_spec->hdr.dst_port;
		filter->src_port  = tcp_spec->hdr.src_port;
		filter->tcp_flags = tcp_spec->hdr.tcp_flags;
	} else if (item->type == RTE_FLOW_ITEM_TYPE_UDP) {
		udp_mask = item->mask;

		/**
		 * Only support src & dst ports,
		 * others should be masked.
		 */
		if (udp_mask->hdr.dgram_len ||
		    udp_mask->hdr.dgram_cksum) {
			memset(filter, 0,
				sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -EINVAL;
		}

		filter->dst_port_mask = udp_mask->hdr.dst_port;
		filter->src_port_mask = udp_mask->hdr.src_port;

		udp_spec = item->spec;
		filter->dst_port = udp_spec->hdr.dst_port;
		filter->src_port = udp_spec->hdr.src_port;
	} else {
		sctp_mask = item->mask;

		/**
		 * Only support src & dst ports,
		 * others should be masked.
		 */
		if (sctp_mask->hdr.tag ||
		    sctp_mask->hdr.cksum) {
			memset(filter, 0,
				sizeof(struct rte_eth_ntuple_filter));
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM,
				item, "Not supported by ntuple filter");
			return -EINVAL;
		}

		filter->dst_port_mask = sctp_mask->hdr.dst_port;
		filter->src_port_mask = sctp_mask->hdr.src_port;

		sctp_spec = item->spec;
		filter->dst_port = sctp_spec->hdr.dst_port;
		filter->src_port = sctp_spec->hdr.src_port;
	}

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_PATTERN(item, pattern, index);
	if (item->type != RTE_FLOW_ITEM_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ITEM,
			item, "Not supported by ntuple filter");
		return -EINVAL;
	}

	table_type = RTE_FLOW_CLASSIFY_TABLE_ACL_IP4_5TUPLE;

	/* parse attr */
	/* must be input direction */
	if (!attr->ingress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -EINVAL;
	}

	/* not supported */
	if (attr->egress) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -EINVAL;
	}

	if (attr->priority > 0xFFFF) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Error priority.");
		return -EINVAL;
	}
	filter->priority = (uint16_t)attr->priority;
	if (attr->priority >  FLOW_RULE_MIN_PRIORITY)
		filter->priority = FLOW_RULE_MAX_PRIORITY;

	/* parse action */
	index = 0;

	/**
	 * n-tuple only supports count and Mark,
	 * check if the first not void action is COUNT or MARK.
	 */
	memset(&action, 0, sizeof(action));
	NEXT_ITEM_OF_ACTION(act, actions, index);
	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		action.action_mask |= 1LLU << RTE_FLOW_ACTION_TYPE_COUNT;
		count = act->conf;
		memcpy(&action.act.counter, count, sizeof(action.act.counter));
		break;
	case RTE_FLOW_ACTION_TYPE_MARK:
		action.action_mask |= 1LLU << RTE_FLOW_ACTION_TYPE_MARK;
		mark_spec = act->conf;
		memcpy(&action.act.mark, mark_spec, sizeof(action.act.mark));
		break;
	default:
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
		   RTE_FLOW_ERROR_TYPE_ACTION, act,
		   "Invalid action.");
		return -EINVAL;
	}

	/* check if the next not void item is MARK or COUNT or END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_COUNT:
		action.action_mask |= 1LLU << RTE_FLOW_ACTION_TYPE_COUNT;
		count = act->conf;
		memcpy(&action.act.counter, count, sizeof(action.act.counter));
		break;
	case RTE_FLOW_ACTION_TYPE_MARK:
		action.action_mask |= 1LLU << RTE_FLOW_ACTION_TYPE_MARK;
		mark_spec = act->conf;
		memcpy(&action.act.mark, mark_spec, sizeof(action.act.mark));
		break;
	case RTE_FLOW_ACTION_TYPE_END:
		return 0;
	default:
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
		   RTE_FLOW_ERROR_TYPE_ACTION, act,
		   "Invalid action.");
		return -EINVAL;
	}

	/* check if the next not void item is END */
	index++;
	NEXT_ITEM_OF_ACTION(act, actions, index);
	if (act->type != RTE_FLOW_ACTION_TYPE_END) {
		memset(filter, 0, sizeof(struct rte_eth_ntuple_filter));
		rte_flow_error_set(error, EINVAL,
		   RTE_FLOW_ERROR_TYPE_ACTION, act,
		   "Invalid action.");
		return -EINVAL;
	}

	return 0;
}
