/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_flow_driver.h>

#include "base/hinic3_compat.h"
#include "base/hinic3_hwdev.h"
#include "base/hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"
#include "hinic3_flow.h"

#define HINIC3_UINT8_MAX 0xff

typedef int (*hinic3_parse_filter_t)(struct rte_eth_dev *dev,
				     const struct rte_flow_attr *attr,
				     const struct rte_flow_item pattern[],
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error,
				     struct hinic3_filter_t *filter);

/* Indicate valid filter mode . */
struct hinic3_valid_pattern {
	enum rte_flow_item_type *items;
	hinic3_parse_filter_t parse_filter;
};

static int hinic3_flow_parse_fdir_filter(struct rte_eth_dev *dev,
					 const struct rte_flow_attr *attr,
					 const struct rte_flow_item pattern[],
					 const struct rte_flow_action actions[],
					 struct rte_flow_error *error,
					 struct hinic3_filter_t *filter);

static int hinic3_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
					      const struct rte_flow_attr *attr,
					      const struct rte_flow_item pattern[],
					      const struct rte_flow_action actions[],
					      struct rte_flow_error *error,
					      struct hinic3_filter_t *filter);

static int hinic3_flow_parse_fdir_vxlan_filter(struct rte_eth_dev *dev,
					       const struct rte_flow_attr *attr,
					       const struct rte_flow_item pattern[],
					       const struct rte_flow_action actions[],
					       struct rte_flow_error *error,
					       struct hinic3_filter_t *filter);

/*
 * Define a supported pattern array, including the matching patterns of
 * various network protocols and corresponding parsing functions.
 */
static const struct hinic3_valid_pattern hinic3_supported_patterns[] = {
	/* Support ethertype. */
	{pattern_ethertype, hinic3_flow_parse_ethertype_filter},
	/* Support ipv4 but not tunnel, and any field can be masked. */
	{pattern_ipv4, hinic3_flow_parse_fdir_filter},
	{pattern_ipv4_any, hinic3_flow_parse_fdir_filter},
	/* Support ipv4 + l4 but not tunnel, and any field can be masked. */
	{pattern_ipv4_udp, hinic3_flow_parse_fdir_filter},
	{pattern_ipv4_tcp, hinic3_flow_parse_fdir_filter},
	/* Support ipv4 + icmp not tunnel, and any field can be masked. */
	{pattern_ipv4_icmp, hinic3_flow_parse_fdir_filter},

	/* Support ipv4 + l4 but not tunnel, and any field can be masked. */
	{pattern_ethertype_udp, hinic3_flow_parse_fdir_filter},
	{pattern_ethertype_tcp, hinic3_flow_parse_fdir_filter},

	/* Support ipv4 + vxlan + any, and any field can be masked. */
	{pattern_ipv4_vxlan, hinic3_flow_parse_fdir_vxlan_filter},
	/* Support ipv4 + vxlan + ipv4, and any field can be masked. */
	{pattern_ipv4_vxlan_ipv4, hinic3_flow_parse_fdir_vxlan_filter},
	/* Support ipv4 + vxlan + ipv4 + l4, and any field can be masked. */
	{pattern_ipv4_vxlan_ipv4_tcp, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv4_vxlan_ipv4_udp, hinic3_flow_parse_fdir_vxlan_filter},
	/* Support ipv4 + vxlan + ipv6, and any field can be masked. */
	{pattern_ipv4_vxlan_ipv6, hinic3_flow_parse_fdir_vxlan_filter},
	/* Support ipv4 + vxlan + ipv6 + l4, and any field can be masked. */
	{pattern_ipv4_vxlan_ipv6_tcp, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv4_vxlan_ipv6_udp, hinic3_flow_parse_fdir_vxlan_filter},
	/* Support ipv4 + vxlan + l4, and any field can be masked. */
	{pattern_ipv4_vxlan_tcp, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv4_vxlan_udp, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv4_vxlan_any, hinic3_flow_parse_fdir_vxlan_filter},

	/* Support ipv6 but not tunnel, and any field can be masked. */
	{pattern_ipv6, hinic3_flow_parse_fdir_filter},
	/* Support ipv6 + l4 but not tunnel, and any field can be masked. */
	{pattern_ipv6_udp, hinic3_flow_parse_fdir_filter},
	{pattern_ipv6_tcp, hinic3_flow_parse_fdir_filter},

	/* Support ipv6 + vxlan + any, and any field can be masked. */
	{pattern_ipv6_vxlan, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv6_vxlan_any, hinic3_flow_parse_fdir_vxlan_filter},

	/* Support ipv6 + vxlan + l4, and any field can be masked. */
	{pattern_ipv6_vxlan_tcp, hinic3_flow_parse_fdir_vxlan_filter},
	{pattern_ipv6_vxlan_udp, hinic3_flow_parse_fdir_vxlan_filter},

};

static inline void
net_addr_to_host(uint32_t *dst, const uint32_t *src, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		dst[i] = rte_be_to_cpu_32(src[i]);
}

static bool
hinic3_match_pattern(enum rte_flow_item_type *item_array,
		     const struct rte_flow_item *pattern)
{
	const struct rte_flow_item *item = pattern;

	/* skip the first void item. */
	while (item->type == RTE_FLOW_ITEM_TYPE_VOID)
		item++;

	/* Find no void item. */
	while (((*item_array == item->type) &&
		(*item_array != RTE_FLOW_ITEM_TYPE_END)) ||
	       (item->type == RTE_FLOW_ITEM_TYPE_VOID)) {
		if (item->type == RTE_FLOW_ITEM_TYPE_VOID) {
			item++;
		} else {
			item_array++;
			item++;
		}
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

/**
 * Find matching parsing filter functions.
 *
 * @param[in] pattern
 * Pattern to match.
 * @return
 * Matched resolution filter. If no resolution filter is found, return NULL.
 */
static hinic3_parse_filter_t
hinic3_find_parse_filter_func(const struct rte_flow_item *pattern)
{
	hinic3_parse_filter_t parse_filter = NULL;
	uint8_t i;
	/* Traverse all supported patterns. */
	for (i = 0; i < RTE_DIM(hinic3_supported_patterns); i++) {
		if (hinic3_match_pattern(hinic3_supported_patterns[i].items, pattern)) {
			parse_filter =
				hinic3_supported_patterns[i].parse_filter;
			break;
		}
	}

	return parse_filter;
}

/**
 * Action for parsing and processing Ethernet types.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse_action(struct rte_eth_dev *dev,
			 const struct rte_flow_action *actions,
			 struct rte_flow_error *error,
			 struct hinic3_filter_t *filter)
{
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action *act = actions;

	/* skip the first void item. */
	while (act->type == RTE_FLOW_ACTION_TYPE_VOID)
		act++;

	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		act_q = (const struct rte_flow_action_queue *)act->conf;
		filter->fdir_filter.rq_index = act_q->index;
		if (filter->fdir_filter.rq_index >= dev->data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Invalid action param.");
			return -rte_errno;
		}
		break;
	default:
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action type.");
		return -rte_errno;
	}

	return 0;
}

int
hinic3_flow_parse_attr(const struct rte_flow_attr *attr,
		       struct rte_flow_error *error)
{
	/* Not supported. */
	if (!attr->ingress || attr->egress || attr->priority || attr->group) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, attr,
				   "Only support ingress.");
		return -rte_errno;
	}

	return 0;
}

static int
hinic3_flow_fdir_ipv4(const struct rte_flow_item *flow_item,
		      struct hinic3_filter_t *filter,
		      struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec_ipv4, *mask_ipv4;

	mask_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->mask;
	spec_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->spec;
	if (!mask_ipv4 || !spec_ipv4) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter ipv4 mask or spec");
		return -rte_errno;
	}

	/*
	 * Only support src address , dst addresses, proto,
	 * others should be masked.
	 */
	if (mask_ipv4->hdr.version_ihl || mask_ipv4->hdr.type_of_service ||
	    mask_ipv4->hdr.total_length || mask_ipv4->hdr.packet_id ||
	    mask_ipv4->hdr.fragment_offset || mask_ipv4->hdr.time_to_live ||
	    mask_ipv4->hdr.hdr_checksum) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Not supported by fdir filter, ipv4 only support src ip, dst ip, proto");
		return -rte_errno;
	}

	/* Set the filter information. */
	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;
	filter->fdir_filter.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;
	filter->fdir_filter.key_mask.ipv4.src_ip =
		rte_be_to_cpu_32(mask_ipv4->hdr.src_addr);
	filter->fdir_filter.key_spec.ipv4.src_ip =
		rte_be_to_cpu_32(spec_ipv4->hdr.src_addr);
	filter->fdir_filter.key_mask.ipv4.dst_ip =
		rte_be_to_cpu_32(mask_ipv4->hdr.dst_addr);
	filter->fdir_filter.key_spec.ipv4.dst_ip =
		rte_be_to_cpu_32(spec_ipv4->hdr.dst_addr);
	filter->fdir_filter.key_mask.proto = mask_ipv4->hdr.next_proto_id;
	filter->fdir_filter.key_spec.proto = spec_ipv4->hdr.next_proto_id;

	return 0;
}

static int
hinic3_flow_fdir_ipv6(const struct rte_flow_item *flow_item,
		      struct hinic3_filter_t *filter,
		      struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec_ipv6, *mask_ipv6;

	mask_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->mask;
	spec_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->spec;
	if (!mask_ipv6 || !spec_ipv6) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter ipv6 mask or spec");
		return -rte_errno;
	}

	/* Only support dst addresses, src addresses, proto. */
	if (mask_ipv6->hdr.vtc_flow || mask_ipv6->hdr.payload_len ||
	    mask_ipv6->hdr.hop_limits) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Not supported by fdir filter, ipv6 only support src ip, dst ip, proto");
		return -rte_errno;
	}

	/* Set the filter information. */
	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;
	filter->fdir_filter.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;
	net_addr_to_host(filter->fdir_filter.key_mask.ipv6.src_ip,
			 (const uint32_t *)mask_ipv6->hdr.src_addr.a, 4);
	net_addr_to_host(filter->fdir_filter.key_spec.ipv6.src_ip,
			 (const uint32_t *)spec_ipv6->hdr.src_addr.a, 4);
	net_addr_to_host(filter->fdir_filter.key_mask.ipv6.dst_ip,
			 (const uint32_t *)mask_ipv6->hdr.dst_addr.a, 4);
	net_addr_to_host(filter->fdir_filter.key_spec.ipv6.dst_ip,
			 (const uint32_t *)spec_ipv6->hdr.dst_addr.a, 4);
	filter->fdir_filter.key_mask.proto = mask_ipv6->hdr.proto;
	filter->fdir_filter.key_spec.proto = spec_ipv6->hdr.proto;

	return 0;
}

static int
hinic3_flow_fdir_tcp(const struct rte_flow_item *flow_item,
		     struct hinic3_filter_t *filter,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *spec_tcp, *mask_tcp;

	mask_tcp = (const struct rte_flow_item_tcp *)flow_item->mask;
	spec_tcp = (const struct rte_flow_item_tcp *)flow_item->spec;

	filter->fdir_filter.key_mask.proto = HINIC3_UINT8_MAX;
	filter->fdir_filter.key_spec.proto = IPPROTO_TCP;

	if (!mask_tcp && !spec_tcp)
		return 0;

	if (!mask_tcp || !spec_tcp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter tcp mask or spec");
		return -rte_errno;
	}

	/* Only support src, dst ports, others should be masked. */
	if (mask_tcp->hdr.sent_seq || mask_tcp->hdr.recv_ack ||
	    mask_tcp->hdr.data_off || mask_tcp->hdr.rx_win ||
	    mask_tcp->hdr.tcp_flags || mask_tcp->hdr.cksum ||
	    mask_tcp->hdr.tcp_urp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Not supported by fdir filter, tcp only support src port, dst port");
		return -rte_errno;
	}

	/* Set the filter information. */
	filter->fdir_filter.key_mask.src_port =
		(uint16_t)rte_be_to_cpu_16(mask_tcp->hdr.src_port);
	filter->fdir_filter.key_spec.src_port =
		(uint16_t)rte_be_to_cpu_16(spec_tcp->hdr.src_port);
	filter->fdir_filter.key_mask.dst_port =
		(uint16_t)rte_be_to_cpu_16(mask_tcp->hdr.dst_port);
	filter->fdir_filter.key_spec.dst_port =
		(uint16_t)rte_be_to_cpu_16(spec_tcp->hdr.dst_port);

	return 0;
}

static int
hinic3_flow_fdir_udp(const struct rte_flow_item *flow_item,
		     struct hinic3_filter_t *filter,
		     struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec_udp, *mask_udp;

	mask_udp = (const struct rte_flow_item_udp *)flow_item->mask;
	spec_udp = (const struct rte_flow_item_udp *)flow_item->spec;

	filter->fdir_filter.key_mask.proto = HINIC3_UINT8_MAX;
	filter->fdir_filter.key_spec.proto = IPPROTO_UDP;

	if (!mask_udp && !spec_udp)
		return 0;

	if (!mask_udp || !spec_udp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter udp mask or spec");
		return -rte_errno;
	}

	/* Set the filter information. */
	filter->fdir_filter.key_mask.src_port =
		(uint16_t)rte_be_to_cpu_16(mask_udp->hdr.src_port);
	filter->fdir_filter.key_spec.src_port =
		(uint16_t)rte_be_to_cpu_16(spec_udp->hdr.src_port);
	filter->fdir_filter.key_mask.dst_port =
		(uint16_t)rte_be_to_cpu_16(mask_udp->hdr.dst_port);
	filter->fdir_filter.key_spec.dst_port =
		(uint16_t)rte_be_to_cpu_16(spec_udp->hdr.dst_port);

	return 0;
}

/**
 * Parse the pattern of network traffic and apply the parsing result to the
 * traffic filter.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, Its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse_fdir_pattern(__rte_unused struct rte_eth_dev *dev,
			       const struct rte_flow_item *pattern,
			       struct rte_flow_error *error,
			       struct hinic3_filter_t *filter)
{
	const struct rte_flow_item *flow_item = pattern;
	enum rte_flow_item_type type;
	int err;

	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_ANY;
	/* Traverse all modes until RTE_FLOW_ITEM_TYPE_END is reached. */
	for (; flow_item->type != RTE_FLOW_ITEM_TYPE_END; flow_item++) {
		if (flow_item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item, "Not support range");
			return -rte_errno;
		}
		type = flow_item->type;
		switch (type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			if (flow_item->spec || flow_item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "Not supported by fdir filter, not support mac");
				return -rte_errno;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			err = hinic3_flow_fdir_ipv4(flow_item, filter, error);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			err = hinic3_flow_fdir_ipv6(flow_item, filter, error);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			err = hinic3_flow_fdir_tcp(flow_item, filter, error);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			err = hinic3_flow_fdir_udp(flow_item, filter, error);
			if (err)
				return -rte_errno;
			break;

		default:
			break;
		}
	}

	return 0;
}

/**
 * Resolve rules for network traffic filters.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] attr
 * Indicates the attribute of a flow rule.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, Its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse_fdir_filter(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      const struct rte_flow_item pattern[],
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error,
			      struct hinic3_filter_t *filter)
{
	int ret;

	ret = hinic3_flow_parse_fdir_pattern(dev, pattern, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_action(dev, actions, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	filter->filter_type = RTE_ETH_FILTER_FDIR;

	return 0;
}

/**
 * Parse and process the actions of the Ethernet type.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, Its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse_ethertype_action(struct rte_eth_dev *dev,
				   const struct rte_flow_action *actions,
				   struct rte_flow_error *error,
				   struct hinic3_filter_t *filter)
{
	const struct rte_flow_action *act = actions;
	const struct rte_flow_action_queue *act_q;

	/* Skip the firset void item. */
	while (act->type == RTE_FLOW_ACTION_TYPE_VOID)
		act++;

	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		act_q = (const struct rte_flow_action_queue *)act->conf;
		filter->ethertype_filter.queue = act_q->index;
		if (filter->ethertype_filter.queue >= dev->data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Invalid action param.");
			return -rte_errno;
		}
		break;

	default:
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   act, "Invalid action type.");
		return -rte_errno;
	}

	return 0;
}

static int
hinic3_flow_parse_ethertype_pattern(__rte_unused struct rte_eth_dev *dev,
				    const struct rte_flow_item *pattern,
				    struct rte_flow_error *error,
				    struct hinic3_filter_t *filter)
{
	const struct rte_flow_item_eth *ether_spec, *ether_mask;
	const struct rte_flow_item *flow_item = pattern;
	enum rte_flow_item_type type;

	/* Traverse all modes until RTE_FLOW_ITEM_TYPE_END is reached. */
	for (; flow_item->type != RTE_FLOW_ITEM_TYPE_END; flow_item++) {
		if (flow_item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item, "Not support range");
			return -rte_errno;
		}
		type = flow_item->type;
		switch (type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			/* Obtaining Ethernet Specifications and Masks. */
			ether_spec = (const struct rte_flow_item_eth *)
					     flow_item->spec;
			ether_mask = (const struct rte_flow_item_eth *)
					     flow_item->mask;
			if (!ether_spec || !ether_mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "NULL ETH spec/mask");
				return -rte_errno;
			}

			/*
			 * Mask bits of source MAC address must be full of 0.
			 * Mask bits of destination MAC address must be full 0.
			 * Filters traffic based on the type of Ethernet.
			 */
			if (!rte_is_zero_ether_addr(&ether_mask->src) ||
			    (!rte_is_zero_ether_addr(&ether_mask->dst))) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "Invalid ether address mask");
				return -rte_errno;
			}

			if ((ether_mask->type & UINT16_MAX) != UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "Invalid ethertype mask");
				return -rte_errno;
			}

			filter->ethertype_filter.ether_type =
				(uint16_t)rte_be_to_cpu_16(ether_spec->type);

			switch (filter->ethertype_filter.ether_type) {
			case RTE_ETHER_TYPE_SLOW:
				break;

			case RTE_ETHER_TYPE_ARP:
				break;

			case RTE_ETHER_TYPE_RARP:
				break;

			case RTE_ETHER_TYPE_LLDP:
				break;

			default:
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "Unsupported ether_type in control packet filter.");
				return -rte_errno;
			}
			break;

		default:
			break;
		}
	}

	return 0;
}

static int
hinic3_flow_parse_ethertype_filter(struct rte_eth_dev *dev,
				   const struct rte_flow_attr *attr,
				   const struct rte_flow_item pattern[],
				   const struct rte_flow_action actions[],
				   struct rte_flow_error *error,
				   struct hinic3_filter_t *filter)
{
	int ret;

	ret = hinic3_flow_parse_ethertype_pattern(dev, pattern, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_ethertype_action(dev, actions, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	filter->filter_type = RTE_ETH_FILTER_ETHERTYPE;
	return 0;
}

static int
hinic3_flow_fdir_tunnel_ipv4(struct rte_flow_error *error,
			     struct hinic3_filter_t *filter,
			     const struct rte_flow_item *flow_item,
			     enum hinic3_fdir_tunnel_mode tunnel_mode)
{
	const struct rte_flow_item_ipv4 *spec_ipv4, *mask_ipv4;
	mask_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->mask;
	spec_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->spec;

	if (tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
		filter->fdir_filter.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV4;

		if (!mask_ipv4 && !spec_ipv4)
			return 0;

		if (!mask_ipv4 || !spec_ipv4) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Invalid fdir filter, vxlan outer ipv4 mask or spec");
			return -rte_errno;
		}

		/*
		 * Only support src address , dst addresses, others should be
		 * masked.
		 */
		if (mask_ipv4->hdr.version_ihl ||
		    mask_ipv4->hdr.type_of_service ||
		    mask_ipv4->hdr.total_length || mask_ipv4->hdr.packet_id ||
		    mask_ipv4->hdr.fragment_offset ||
		    mask_ipv4->hdr.time_to_live ||
		    mask_ipv4->hdr.next_proto_id ||
		    mask_ipv4->hdr.hdr_checksum) {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
				"Not supported by fdir filter, vxlan outer ipv4 only support src ip, dst ip");
			return -rte_errno;
		}

		/* Set the filter information. */
		filter->fdir_filter.key_mask.ipv4.src_ip =
			rte_be_to_cpu_32(mask_ipv4->hdr.src_addr);
		filter->fdir_filter.key_spec.ipv4.src_ip =
			rte_be_to_cpu_32(spec_ipv4->hdr.src_addr);
		filter->fdir_filter.key_mask.ipv4.dst_ip =
			rte_be_to_cpu_32(mask_ipv4->hdr.dst_addr);
		filter->fdir_filter.key_spec.ipv4.dst_ip =
			rte_be_to_cpu_32(spec_ipv4->hdr.dst_addr);
	} else {
		filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;

		if (!mask_ipv4 && !spec_ipv4)
			return 0;

		if (!mask_ipv4 || !spec_ipv4) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Invalid fdir filter, vxlan inner ipv4 mask or spec");
			return -rte_errno;
		}

		/*
		 * Only support src addr , dst addr, ip proto, others should be
		 * masked.
		 */
		if (mask_ipv4->hdr.version_ihl ||
		    mask_ipv4->hdr.type_of_service ||
		    mask_ipv4->hdr.total_length || mask_ipv4->hdr.packet_id ||
		    mask_ipv4->hdr.fragment_offset ||
		    mask_ipv4->hdr.time_to_live ||
		    mask_ipv4->hdr.hdr_checksum) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Not supported by fdir filter, vxlan inner ipv4 only support src ip, dst ip, proto");
			return -rte_errno;
		}

		/* Set the filter information. */
		filter->fdir_filter.key_mask.inner_ipv4.src_ip =
			rte_be_to_cpu_32(mask_ipv4->hdr.src_addr);
		filter->fdir_filter.key_spec.inner_ipv4.src_ip =
			rte_be_to_cpu_32(spec_ipv4->hdr.src_addr);
		filter->fdir_filter.key_mask.inner_ipv4.dst_ip =
			rte_be_to_cpu_32(mask_ipv4->hdr.dst_addr);
		filter->fdir_filter.key_spec.inner_ipv4.dst_ip =
			rte_be_to_cpu_32(spec_ipv4->hdr.dst_addr);
		filter->fdir_filter.key_mask.proto =
			mask_ipv4->hdr.next_proto_id;
		filter->fdir_filter.key_spec.proto =
			spec_ipv4->hdr.next_proto_id;
	}
	return 0;
}

static int
hinic3_flow_fdir_tunnel_ipv6(struct rte_flow_error *error,
			     struct hinic3_filter_t *filter,
			     const struct rte_flow_item *flow_item,
			     enum hinic3_fdir_tunnel_mode tunnel_mode)
{
	const struct rte_flow_item_ipv6 *spec_ipv6, *mask_ipv6;

	mask_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->mask;
	spec_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->spec;

	if (tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
		filter->fdir_filter.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV6;

		if (!mask_ipv6 && !spec_ipv6)
			return 0;

		if (!mask_ipv6 || !spec_ipv6) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
				"Invalid fdir filter ipv6 mask or spec");
			return -rte_errno;
		}

		/* Only support dst addresses, src addresses. */
		if (mask_ipv6->hdr.vtc_flow || mask_ipv6->hdr.payload_len ||
		    mask_ipv6->hdr.hop_limits || mask_ipv6->hdr.proto) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
				"Not supported by fdir filter, ipv6 only support src ip, dst ip, proto");
			return -rte_errno;
		}

		net_addr_to_host(filter->fdir_filter.key_mask.ipv6.src_ip,
				 (const uint32_t *)mask_ipv6->hdr.src_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_spec.ipv6.src_ip,
				 (const uint32_t *)spec_ipv6->hdr.src_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_mask.ipv6.dst_ip,
				 (const uint32_t *)mask_ipv6->hdr.dst_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_spec.ipv6.dst_ip,
				 (const uint32_t *)spec_ipv6->hdr.dst_addr.a, 4);
	} else {
		filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;

		if (!mask_ipv6 && !spec_ipv6)
			return 0;

		if (!mask_ipv6 || !spec_ipv6) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
				"Invalid fdir filter ipv6 mask or spec");
			return -rte_errno;
		}

		/* Only support dst addresses, src addresses, proto. */
		if (mask_ipv6->hdr.vtc_flow || mask_ipv6->hdr.payload_len ||
		    mask_ipv6->hdr.hop_limits) {
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
				"Not supported by fdir filter, ipv6 only support src ip, dst ip, proto");
			return -rte_errno;
		}

		net_addr_to_host(filter->fdir_filter.key_mask.inner_ipv6.src_ip,
				 (const uint32_t *)mask_ipv6->hdr.src_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_spec.inner_ipv6.src_ip,
				 (const uint32_t *)spec_ipv6->hdr.src_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_mask.inner_ipv6.dst_ip,
				 (const uint32_t *)mask_ipv6->hdr.dst_addr.a, 4);
		net_addr_to_host(filter->fdir_filter.key_spec.inner_ipv6.dst_ip,
				 (const uint32_t *)spec_ipv6->hdr.dst_addr.a, 4);

		filter->fdir_filter.key_mask.proto = mask_ipv6->hdr.proto;
		filter->fdir_filter.key_spec.proto = spec_ipv6->hdr.proto;
	}

	return 0;
}

static int
hinic3_flow_fdir_tunnel_tcp(struct rte_flow_error *error,
			    struct hinic3_filter_t *filter,
			    enum hinic3_fdir_tunnel_mode tunnel_mode,
			    const struct rte_flow_item *flow_item)
{
	const struct rte_flow_item_tcp *spec_tcp, *mask_tcp;

	if (tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Not supported by fdir filter, vxlan only support inner tcp");
		return -rte_errno;
	}

	filter->fdir_filter.key_mask.proto = HINIC3_UINT8_MAX;
	filter->fdir_filter.key_spec.proto = IPPROTO_TCP;

	mask_tcp = (const struct rte_flow_item_tcp *)flow_item->mask;
	spec_tcp = (const struct rte_flow_item_tcp *)flow_item->spec;
	if (!mask_tcp && !spec_tcp)
		return 0;
	if (!mask_tcp || !spec_tcp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter tcp mask or spec");
		return -rte_errno;
	}

	/* Only support src, dst ports, others should be masked. */
	if (mask_tcp->hdr.sent_seq || mask_tcp->hdr.recv_ack ||
	    mask_tcp->hdr.data_off || mask_tcp->hdr.rx_win ||
	    mask_tcp->hdr.tcp_flags || mask_tcp->hdr.cksum ||
	    mask_tcp->hdr.tcp_urp) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Not supported by fdir filter, vxlan inner tcp only support src port,dst port");
		return -rte_errno;
	}

	/* Set the filter information. */
	filter->fdir_filter.key_mask.src_port =
		(uint16_t)rte_be_to_cpu_16(mask_tcp->hdr.src_port);
	filter->fdir_filter.key_spec.src_port =
		(uint16_t)rte_be_to_cpu_16(spec_tcp->hdr.src_port);
	filter->fdir_filter.key_mask.dst_port =
		(uint16_t)rte_be_to_cpu_16(mask_tcp->hdr.dst_port);
	filter->fdir_filter.key_spec.dst_port =
		(uint16_t)rte_be_to_cpu_16(spec_tcp->hdr.dst_port);
	return 0;
}

static int
hinic3_flow_fdir_tunnel_udp(struct rte_flow_error *error,
			    struct hinic3_filter_t *filter,
			    enum hinic3_fdir_tunnel_mode tunnel_mode,
			    const struct rte_flow_item *flow_item)
{
	const struct rte_flow_item_udp *spec_udp, *mask_udp;

	mask_udp = (const struct rte_flow_item_udp *)flow_item->mask;
	spec_udp = (const struct rte_flow_item_udp *)flow_item->spec;

	if (tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
		/*
		 * UDP is used to describe protocol,
		 * spec and mask should be NULL.
		 */
		if (flow_item->spec || flow_item->mask) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item, "Invalid UDP item");
			return -rte_errno;
		}
	} else {
		filter->fdir_filter.key_mask.proto = HINIC3_UINT8_MAX;
		filter->fdir_filter.key_spec.proto = IPPROTO_UDP;
		if (!mask_udp && !spec_udp)
			return 0;

		if (!mask_udp || !spec_udp) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Invalid fdir filter vxlan inner udp mask or spec");
			return -rte_errno;
		}

		/* Set the filter information. */
		filter->fdir_filter.key_mask.src_port =
			(uint16_t)rte_be_to_cpu_16(mask_udp->hdr.src_port);
		filter->fdir_filter.key_spec.src_port =
			(uint16_t)rte_be_to_cpu_16(spec_udp->hdr.src_port);
		filter->fdir_filter.key_mask.dst_port =
			(uint16_t)rte_be_to_cpu_16(mask_udp->hdr.dst_port);
		filter->fdir_filter.key_spec.dst_port =
			(uint16_t)rte_be_to_cpu_16(spec_udp->hdr.dst_port);
	}

	return 0;
}

static int
hinic3_flow_fdir_vxlan(struct rte_flow_error *error,
		       struct hinic3_filter_t *filter,
		       const struct rte_flow_item *flow_item)
{
	const struct rte_flow_item_vxlan *spec_vxlan, *mask_vxlan;
	uint32_t vxlan_vni_id = 0;

	spec_vxlan = (const struct rte_flow_item_vxlan *)flow_item->spec;
	mask_vxlan = (const struct rte_flow_item_vxlan *)flow_item->mask;

	filter->fdir_filter.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_VXLAN;

	if (!spec_vxlan && !mask_vxlan) {
		return 0;
	} else if (filter->fdir_filter.outer_ip_type == HINIC3_FDIR_IP_TYPE_IPV6) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter vxlan mask or spec, ipv6 vxlan, don't support vni");
		return -rte_errno;
	}

	if (!spec_vxlan || !mask_vxlan) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter vxlan mask or spec");
		return -rte_errno;
	}

	memcpy(((uint8_t *)&vxlan_vni_id + 1), spec_vxlan->vni, 3);
	filter->fdir_filter.key_mask.tunnel.tunnel_id =
		rte_be_to_cpu_32(vxlan_vni_id);
	return 0;
}

static int
hinic3_flow_parse_fdir_vxlan_pattern(__rte_unused struct rte_eth_dev *dev,
				     const struct rte_flow_item *pattern,
				     struct rte_flow_error *error,
				     struct hinic3_filter_t *filter)
{
	const struct rte_flow_item *flow_item = pattern;
	enum hinic3_fdir_tunnel_mode tunnel_mode =
		HINIC3_FDIR_TUNNEL_MODE_NORMAL;
	enum rte_flow_item_type type;
	int err;

	/* Inner and outer ip type, set it to any by default */
	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_ANY;
	filter->fdir_filter.outer_ip_type = HINIC3_FDIR_IP_TYPE_ANY;

	for (; flow_item->type != RTE_FLOW_ITEM_TYPE_END; flow_item++) {
		if (flow_item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item, "Not support range");
			return -rte_errno;
		}

		type = flow_item->type;
		switch (type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			/* All should be masked. */
			if (flow_item->spec || flow_item->mask) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   flow_item,
						   "Not supported by fdir filter, not support mac");
				return -rte_errno;
			}
			break;

		case RTE_FLOW_ITEM_TYPE_IPV4:
			err = hinic3_flow_fdir_tunnel_ipv4(error,
				filter, flow_item, tunnel_mode);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			err = hinic3_flow_fdir_tunnel_ipv6(error,
				filter, flow_item, tunnel_mode);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			err = hinic3_flow_fdir_tunnel_tcp(error,
				filter, tunnel_mode, flow_item);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_UDP:
			err = hinic3_flow_fdir_tunnel_udp(error,
				filter, tunnel_mode, flow_item);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_VXLAN:
			err = hinic3_flow_fdir_vxlan(error, filter, flow_item);
			if (err)
				return -rte_errno;
			tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_VXLAN;
			break;

		default:
			break;
		}
	}

	return 0;
}

/**
 * Resolve VXLAN Filters in Flow Filters.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] attr
 * Indicates the attribute of a flow rule.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse_fdir_vxlan_filter(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item pattern[],
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error,
				    struct hinic3_filter_t *filter)
{
	int ret;

	ret = hinic3_flow_parse_fdir_vxlan_pattern(dev, pattern, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_action(dev, actions, error, filter);
	if (ret)
		return ret;

	ret = hinic3_flow_parse_attr(attr, error);
	if (ret)
		return ret;

	filter->filter_type = RTE_ETH_FILTER_FDIR;

	return 0;
}

/**
 * Parse patterns and actions of network traffic.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] attr
 * Indicates the attribute of a flow rule.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @param[out] filter
 * Filter information, its used to store and manipulate packet filtering rules.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_parse(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error, struct hinic3_filter_t *filter)
{
	hinic3_parse_filter_t parse_filter;
	uint32_t pattern_num = 0;
	int ret = 0;
	/* Check whether the parameter is valid. */
	if (!pattern || !actions || !attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "NULL param.");
		return -rte_errno;
	}

	while ((pattern + pattern_num)->type != RTE_FLOW_ITEM_TYPE_END) {
		pattern_num++;
		if (pattern_num > HINIC3_FLOW_MAX_PATTERN_NUM) {
			rte_flow_error_set(error, EINVAL,
					   HINIC3_FLOW_MAX_PATTERN_NUM, NULL,
					   "Too many patterns.");
			return -rte_errno;
		}
	}
	/*
	 * The corresponding filter is returned. If the filter is not found,
	 * NULL is returned.
	 */
	parse_filter = hinic3_find_parse_filter_func(pattern);
	if (!parse_filter) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   pattern, "Unsupported pattern");
		return -rte_errno;
	}
	/* Parsing with filters. */
	ret = parse_filter(dev, attr, pattern, actions, error, filter);

	return ret;
}

/**
 * Check whether the traffic rule provided by the user is valid.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] attr
 * Indicates the attribute of a flow rule.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	struct hinic3_filter_t filter_rules = {0};

	return hinic3_flow_parse(dev, attr, pattern, actions, error, &filter_rules);
}

/**
 * Create a flow item.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[in] attr
 * Indicates the attribute of a flow rule.
 * @param[in] pattern
 * Indicates the pattern or matching condition of a traffic rule.
 * @param[in] actions
 * Indicates the action to be taken on the matched traffic.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @return
 * If the operation is successful, the created flow is returned. Otherwise, NULL
 * is returned.
 *
 */
static struct rte_flow *
hinic3_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct hinic3_filter_t *filter_rules = NULL;
	struct rte_flow *flow = NULL;
	int ret;

	filter_rules =
		rte_zmalloc("filter_rules", sizeof(struct hinic3_filter_t), 0);
	if (!filter_rules) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL,
				   "Failed to allocate filter rules memory.");
		return NULL;
	}

	flow = rte_zmalloc("hinic3_rte_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to allocate flow memory.");
		rte_free(filter_rules);
		return NULL;
	}
	/* Parses the flow rule to be created and generates a filter. */
	ret = hinic3_flow_parse(dev, attr, pattern, actions, error,
				filter_rules);
	if (ret < 0)
		goto free_flow;

	switch (filter_rules->filter_type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = hinic3_flow_add_del_ethertype_filter(dev,
			&filter_rules->ethertype_filter, true);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Create ethertype filter failed.");
			goto free_flow;
		}

		flow->rule = filter_rules;
		flow->filter_type = filter_rules->filter_type;
		TAILQ_INSERT_TAIL(&nic_dev->filter_ethertype_list, flow, node);
		break;

	case RTE_ETH_FILTER_FDIR:
		ret = hinic3_flow_add_del_fdir_filter(dev,
			&filter_rules->fdir_filter, true);
		if (ret) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Create fdir filter failed.");
			goto free_flow;
		}

		flow->rule = filter_rules;
		flow->filter_type = filter_rules->filter_type;
		TAILQ_INSERT_TAIL(&nic_dev->filter_fdir_rule_list, flow, node);
		break;
	default:
		PMD_DRV_LOG(ERR, "Filter type %d not supported",
			    filter_rules->filter_type);
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Unsupported filter type.");
		goto free_flow;
	}

	return flow;

free_flow:
	rte_free(flow);
	rte_free(filter_rules);

	return NULL;
}

static int
hinic3_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		    struct rte_flow_error *error)
{
	int ret = -EINVAL;
	enum rte_filter_type type;
	struct hinic3_filter_t *rules = NULL;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	if (!flow) {
		PMD_DRV_LOG(ERR, "Invalid flow parameter!");
		return -EPERM;
	}

	type = flow->filter_type;
	rules = (struct hinic3_filter_t *)flow->rule;
	/* Perform operations based on the type. */
	switch (type) {
	case RTE_ETH_FILTER_ETHERTYPE:
		ret = hinic3_flow_add_del_ethertype_filter(dev,
			&rules->ethertype_filter, false);
		if (!ret)
			TAILQ_REMOVE(&nic_dev->filter_ethertype_list, flow, node);

		flow->rule = rules;
		flow->filter_type = rules->filter_type;
		TAILQ_REMOVE(&nic_dev->filter_ethertype_list, flow, node);
		break;

	case RTE_ETH_FILTER_FDIR:
		ret = hinic3_flow_add_del_fdir_filter(dev, &rules->fdir_filter, false);
		if (!ret)
			TAILQ_REMOVE(&nic_dev->filter_fdir_rule_list, flow, node);
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type %d not supported", type);
		ret = -EINVAL;
		break;
	}

	/* Deleted successfully. Resources are released. */
	if (!ret) {
		rte_free(rules);
		rte_free(flow);
	} else {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to destroy flow.");
	}

	return ret;
}

/**
 * Clear all fdir type flow rules on the network device.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_flush_fdir_filter(struct rte_eth_dev *dev)
{
	int ret = 0;
	struct hinic3_filter_t *filter_rules = NULL;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_flow *flow;

	while (true) {
		flow = TAILQ_FIRST(&nic_dev->filter_fdir_rule_list);
		if (flow == NULL)
			break;
		filter_rules = (struct hinic3_filter_t *)flow->rule;

		/* Delete flow rules. */
		ret = hinic3_flow_add_del_fdir_filter(dev,
			&filter_rules->fdir_filter, false);

		if (ret)
			return ret;

		TAILQ_REMOVE(&nic_dev->filter_fdir_rule_list, flow, node);
		rte_free(filter_rules);
		rte_free(flow);
	}

	return ret;
}

/**
 * Clear all ether type flow rules on the network device.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_flush_ethertype_filter(struct rte_eth_dev *dev)
{
	struct hinic3_filter_t *filter_rules = NULL;
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);
	struct rte_flow *flow;
	int ret = 0;

	while (true) {
		flow = TAILQ_FIRST(&nic_dev->filter_ethertype_list);
		if (flow == NULL)
			break;
		filter_rules = (struct hinic3_filter_t *)flow->rule;

		/* Delete flow rules. */
		ret = hinic3_flow_add_del_ethertype_filter(dev,
			&filter_rules->ethertype_filter, false);

		if (ret)
			return ret;

		TAILQ_REMOVE(&nic_dev->filter_ethertype_list, flow, node);
		rte_free(filter_rules);
		rte_free(flow);
	}

	return ret;
}

/**
 * Clear all flow rules on the network device.
 *
 * @param[in] dev
 * Pointer to ethernet device structure.
 * @param[out] error
 * Structure that contains error information, such as error code and error
 * description.
 * @return
 * 0 on success, non-zero on failure.
 */
static int
hinic3_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	int ret;

	ret = hinic3_flow_flush_fdir_filter(dev);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to flush fdir flows.");
		return -rte_errno;
	}

	ret = hinic3_flow_flush_ethertype_filter(dev);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_HANDLE,
				   NULL, "Failed to flush ethertype flows.");
		return -rte_errno;
	}
	return ret;
}

/* Structure for managing flow table operations. */
const struct rte_flow_ops hinic3_flow_ops = {
	.validate = hinic3_flow_validate,
	.create = hinic3_flow_create,
	.destroy = hinic3_flow_destroy,
	.flush = hinic3_flow_flush,
};
