/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#include <rte_flow_driver.h>

#include "base/hinic3_compat.h"
#include "base/hinic3_hwdev.h"
#include "base/hinic3_nic_cfg.h"
#include "hinic3_ethdev.h"

#define HINIC3_UINT8_MAX 0xff

typedef int (*hinic3_parse_filter_t)(struct rte_eth_dev *dev,
				     const struct rte_flow_attr *attr,
				     const struct rte_flow_item pattern[],
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error,
				     struct hinic3_filter_t *filter);

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

static int hinic3_flow_parse_fdir_vxlan_geneve_filter(struct rte_eth_dev *dev,
						      const struct rte_flow_attr *attr,
						      const struct rte_flow_item pattern[],
						      const struct rte_flow_action actions[],
						      struct rte_flow_error *error,
						      struct hinic3_filter_t *filter);

static inline void
net_addr_to_host(uint32_t *dst, const uint32_t *src, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		dst[i] = rte_be_to_cpu_32(src[i]);
}

/* IPINIP and GPE are split out due to chip support differences. */
enum hinic3_flow_filter_kind {
	HINIC3_FLOW_KIND_ETHERTYPE,	/* [ETH]. */
	HINIC3_FLOW_KIND_NON_TUNNEL,	/* single L3/L4 or ethertype L4. */
	HINIC3_FLOW_KIND_VXLAN_GENEVE,	/* vxlan/geneve. */
	HINIC3_FLOW_KIND_GPE,		/* vxlan-gpe. */
	HINIC3_FLOW_KIND_IPINIP,	/* ip-in-ip. */
	HINIC3_FLOW_KIND_INVALID,
};

/*
 * The pattern is parsed as a small protocol graph. Every supported pattern
 * starts with ETH and then follows one of the supported encapsulation paths
 * (plain L3/L4, tunneled, or ip-in-ip). VOID items are transparent.
 */

/* Peek at the next non-VOID item type. */
static enum rte_flow_item_type
hinic3_flow_next(const struct rte_flow_item *item)
{
	while (item->type == RTE_FLOW_ITEM_TYPE_VOID)
		item++;

	return item->type;
}

/* Consume the next non-VOID item when it matches @p type. */
static bool
hinic3_flow_take(const struct rte_flow_item **item,
		 enum rte_flow_item_type type)
{
	while ((*item)->type == RTE_FLOW_ITEM_TYPE_VOID)
		(*item)++;

	if ((*item)->type != type)
		return false;

	(*item)++;
	return true;
}

/* Return true when only the END item remains. */
static bool
hinic3_flow_at_end(const struct rte_flow_item *item)
{
	return hinic3_flow_next(item) == RTE_FLOW_ITEM_TYPE_END;
}

/* Finish a branch, which is valid only when END is reached. */
static enum hinic3_flow_filter_kind
hinic3_flow_finish(const struct rte_flow_item *item,
		   enum hinic3_flow_filter_kind kind)
{
	return hinic3_flow_at_end(item) ? kind : HINIC3_FLOW_KIND_INVALID;
}

/* Classify an inner L3 (IPv4/IPv6) with an optional L4, returning @p kind. */
static enum hinic3_flow_filter_kind
hinic3_flow_classify_inner(const struct rte_flow_item **item,
			   enum hinic3_flow_filter_kind kind,
			   enum rte_flow_item_type outer_ip)
{
	enum rte_flow_item_type next = hinic3_flow_next(*item);

	if (next != RTE_FLOW_ITEM_TYPE_IPV4 &&
	    next != RTE_FLOW_ITEM_TYPE_IPV6)
		return HINIC3_FLOW_KIND_INVALID;

	/*
	 * An outer IPv6 plus any inner L3 does not fit in the TCAM, so only
	 * an IPv4 outer (or no outer at all) may carry an inner L3.
	 */
	if (outer_ip == RTE_FLOW_ITEM_TYPE_IPV6)
		return HINIC3_FLOW_KIND_INVALID;

	hinic3_flow_take(item, next);

	switch (hinic3_flow_next(*item)) {
	case RTE_FLOW_ITEM_TYPE_END:
		return kind;
	case RTE_FLOW_ITEM_TYPE_TCP:
	case RTE_FLOW_ITEM_TYPE_UDP:
		hinic3_flow_take(item, hinic3_flow_next(*item));
		return hinic3_flow_finish(*item, kind);
	default:
		return HINIC3_FLOW_KIND_INVALID;
	}
}

/* Consume a tunnel header and classify the payload that follows it. */
static enum hinic3_flow_filter_kind
hinic3_flow_classify_tunnel(const struct rte_flow_item **item,
			    enum rte_flow_item_type tunnel_type,
			    enum rte_flow_item_type outer_ip)
{
	enum hinic3_flow_filter_kind kind =
		tunnel_type == RTE_FLOW_ITEM_TYPE_VXLAN_GPE ?
			HINIC3_FLOW_KIND_GPE : HINIC3_FLOW_KIND_VXLAN_GENEVE;
	enum rte_flow_item_type next;

	if (!hinic3_flow_take(item, tunnel_type))
		return HINIC3_FLOW_KIND_INVALID;

	next = hinic3_flow_next(*item);

	/* A bare tunnel with nothing after the header is valid. */
	if (next == RTE_FLOW_ITEM_TYPE_END)
		return kind;

	/* GPE: an optional inner ETH (which may be terminal), then an L3(+L4). */
	if (tunnel_type == RTE_FLOW_ITEM_TYPE_VXLAN_GPE) {
		if (next == RTE_FLOW_ITEM_TYPE_ETH) {
			hinic3_flow_take(item, RTE_FLOW_ITEM_TYPE_ETH);
			if (hinic3_flow_at_end(*item))
				return kind;
		}
		return hinic3_flow_classify_inner(item, kind, outer_ip);
	}

	/* vxlan/geneve: a bare L4/ANY, or an inner L3(+L4). */
	switch (next) {
	case RTE_FLOW_ITEM_TYPE_TCP:
	case RTE_FLOW_ITEM_TYPE_UDP:
	case RTE_FLOW_ITEM_TYPE_ANY:
		hinic3_flow_take(item, next);
		return hinic3_flow_finish(*item, kind);
	case RTE_FLOW_ITEM_TYPE_ETH:
	case RTE_FLOW_ITEM_TYPE_IPV4:
	case RTE_FLOW_ITEM_TYPE_IPV6:
		if (next == RTE_FLOW_ITEM_TYPE_ETH) {
			hinic3_flow_take(item, RTE_FLOW_ITEM_TYPE_ETH);
			break;
		}
		/* Geneve accepts a bare inner L3; vxlan requires the inner ETH. */
		if (tunnel_type != RTE_FLOW_ITEM_TYPE_GENEVE)
			return HINIC3_FLOW_KIND_INVALID;
		break;
	default:
		return HINIC3_FLOW_KIND_INVALID;
	}

	return hinic3_flow_classify_inner(item, kind, outer_ip);
}

/* Classify the payload that follows the outer L3. */
static enum hinic3_flow_filter_kind
hinic3_flow_classify_ip(const struct rte_flow_item **item,
			enum rte_flow_item_type ip_type)
{
	enum rte_flow_item_type next = hinic3_flow_next(*item);

	switch (next) {
	case RTE_FLOW_ITEM_TYPE_END:
		return HINIC3_FLOW_KIND_NON_TUNNEL;
	case RTE_FLOW_ITEM_TYPE_TCP:
		hinic3_flow_take(item, RTE_FLOW_ITEM_TYPE_TCP);
		return hinic3_flow_finish(*item, HINIC3_FLOW_KIND_NON_TUNNEL);
	case RTE_FLOW_ITEM_TYPE_UDP:
		hinic3_flow_take(item, RTE_FLOW_ITEM_TYPE_UDP);
		next = hinic3_flow_next(*item);
		switch (next) {
		case RTE_FLOW_ITEM_TYPE_END:
			return HINIC3_FLOW_KIND_NON_TUNNEL;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
		case RTE_FLOW_ITEM_TYPE_GENEVE:
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			return hinic3_flow_classify_tunnel(item, next, ip_type);
		default:
			return HINIC3_FLOW_KIND_INVALID;
		}
	case RTE_FLOW_ITEM_TYPE_ICMP:
	case RTE_FLOW_ITEM_TYPE_ANY:
		/* ICMP and ANY are valid only right after an IPv4 outer. */
		if (ip_type != RTE_FLOW_ITEM_TYPE_IPV4)
			return HINIC3_FLOW_KIND_INVALID;
		hinic3_flow_take(item, next);
		return hinic3_flow_finish(*item, HINIC3_FLOW_KIND_NON_TUNNEL);
	case RTE_FLOW_ITEM_TYPE_IPV4:
	case RTE_FLOW_ITEM_TYPE_IPV6:
		/*
		 * Ip-in-ip: a second L3 with an optional L4 (or ANY). An outer
		 * IPv6 plus an inner L3 does not fit in the TCAM.
		 */
		if (ip_type == RTE_FLOW_ITEM_TYPE_IPV6)
			return HINIC3_FLOW_KIND_INVALID;
		hinic3_flow_take(item, next);
		switch (hinic3_flow_next(*item)) {
		case RTE_FLOW_ITEM_TYPE_END:
			return HINIC3_FLOW_KIND_IPINIP;
		case RTE_FLOW_ITEM_TYPE_TCP:
		case RTE_FLOW_ITEM_TYPE_UDP:
		case RTE_FLOW_ITEM_TYPE_ANY:
			hinic3_flow_take(item, hinic3_flow_next(*item));
			return hinic3_flow_finish(*item, HINIC3_FLOW_KIND_IPINIP);
		default:
			return HINIC3_FLOW_KIND_INVALID;
		}
	default:
		return HINIC3_FLOW_KIND_INVALID;
	}
}

/* Classify a pattern into one of the supported filter kinds. */
static enum hinic3_flow_filter_kind
hinic3_flow_classify(const struct rte_flow_item *pattern)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type next;

	if (!hinic3_flow_take(&item, RTE_FLOW_ITEM_TYPE_ETH))
		return HINIC3_FLOW_KIND_INVALID;

	switch (hinic3_flow_next(item)) {
	case RTE_FLOW_ITEM_TYPE_END:
		return HINIC3_FLOW_KIND_ETHERTYPE;
	case RTE_FLOW_ITEM_TYPE_TCP:
		hinic3_flow_take(&item, RTE_FLOW_ITEM_TYPE_TCP);
		return hinic3_flow_finish(item, HINIC3_FLOW_KIND_NON_TUNNEL);
	case RTE_FLOW_ITEM_TYPE_UDP:
		hinic3_flow_take(&item, RTE_FLOW_ITEM_TYPE_UDP);
		next = hinic3_flow_next(item);
		switch (next) {
		case RTE_FLOW_ITEM_TYPE_END:
			return HINIC3_FLOW_KIND_NON_TUNNEL;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
		case RTE_FLOW_ITEM_TYPE_GENEVE:
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			/* No outer IP: pass END so classify_inner allows it. */
			return hinic3_flow_classify_tunnel(&item, next,
							    RTE_FLOW_ITEM_TYPE_END);
		default:
			return HINIC3_FLOW_KIND_INVALID;
		}
	case RTE_FLOW_ITEM_TYPE_IPV4:
		hinic3_flow_take(&item, RTE_FLOW_ITEM_TYPE_IPV4);
		return hinic3_flow_classify_ip(&item, RTE_FLOW_ITEM_TYPE_IPV4);
	case RTE_FLOW_ITEM_TYPE_IPV6:
		hinic3_flow_take(&item, RTE_FLOW_ITEM_TYPE_IPV6);
		return hinic3_flow_classify_ip(&item, RTE_FLOW_ITEM_TYPE_IPV6);
	default:
		return HINIC3_FLOW_KIND_INVALID;
	}
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
hinic3_find_parse_filter_func(struct rte_eth_dev *dev,
			      const struct rte_flow_item *pattern)
{
	struct hinic3_nic_dev *nic_dev = HINIC3_ETH_DEV_TO_PRIVATE_NIC_DEV(dev);

	switch (hinic3_flow_classify(pattern)) {
	case HINIC3_FLOW_KIND_ETHERTYPE:
		return hinic3_flow_parse_ethertype_filter;
	case HINIC3_FLOW_KIND_NON_TUNNEL:
		return hinic3_flow_parse_fdir_filter;
	case HINIC3_FLOW_KIND_VXLAN_GENEVE:
		return hinic3_flow_parse_fdir_vxlan_geneve_filter;
	case HINIC3_FLOW_KIND_GPE:
		return HINIC3_IS_SP620_NIC(nic_dev) ?
		hinic3_flow_parse_fdir_vxlan_geneve_filter : NULL;
	case HINIC3_FLOW_KIND_IPINIP:
		return HINIC3_IS_SP620_NIC(nic_dev) ? NULL :
		hinic3_flow_parse_fdir_vxlan_geneve_filter;
	default:
		return NULL;
	}
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

	/* Only one non-VOID action before END is supported. */
	const struct rte_flow_action *last_act = NULL;
	for (act = actions; act->type != RTE_FLOW_ACTION_TYPE_END; act++) {
		if (act->type == RTE_FLOW_ACTION_TYPE_VOID)
			continue;
		if (last_act != NULL) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Multiple actions not supported.");
			return -rte_errno;
		}
		last_act = act;
	}
	if (last_act == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "No valid action.");
		return -rte_errno;
	}

	act = last_act;

	switch (act->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		if (act->conf == NULL) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   act, "Invalid action queue config.");
			return -rte_errno;
		}
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

	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV4;
	filter->fdir_filter.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

	/* When both L3 mask and spec are empty, return 0, then proceed to evaluate L4. */
	if (!mask_ipv4 && !spec_ipv4)
		return 0;

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

	filter->fdir_filter.ip_type = HINIC3_FDIR_IP_TYPE_IPV6;
	filter->fdir_filter.tunnel_type = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

	/* When both L3 mask and spec are empty, return 0, then proceed to evaluate L4. */
	if (!mask_ipv6 && !spec_ipv6)
		return 0;

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

			case RTE_ETHER_TYPE_CNM:
				break;

			case RTE_ETHER_TYPE_ECP:
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
			     enum hinic3_fdir_tunnel_mode *tunnel_mode)
{
	const struct rte_flow_item_ipv4 *spec_ipv4, *mask_ipv4;
	mask_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->mask;
	spec_ipv4 = (const struct rte_flow_item_ipv4 *)flow_item->spec;

	if (*tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_MAX) {
		filter->fdir_filter.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV4;
		*tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

		if (!mask_ipv4 && !spec_ipv4)
			return 0;

		if (!mask_ipv4 || !spec_ipv4) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Invalid fdir filter, vxlan/geneve outer ipv4 mask or spec");
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
				"Not supported by fdir filter, vxlan/geneve outer ipv4 only support src ip,dst ip");
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
		if (*tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
			*tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_IPIP;
			filter->fdir_filter.tunnel_type = *tunnel_mode;
		}

		if (!mask_ipv4 && !spec_ipv4)
			return 0;

		if (!mask_ipv4 || !spec_ipv4) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   flow_item,
					   "Invalid fdir filter, vxlan/geneve inner ipv4 mask or spec");
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
					   "Not supported by fdir filter, vxlan/geneve inner ipv4 only support src ip,dst ip, proto");
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
			     enum hinic3_fdir_tunnel_mode *tunnel_mode)
{
	const struct rte_flow_item_ipv6 *spec_ipv6, *mask_ipv6;

	mask_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->mask;
	spec_ipv6 = (const struct rte_flow_item_ipv6 *)flow_item->spec;

	if (*tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_MAX) {
		filter->fdir_filter.outer_ip_type = HINIC3_FDIR_IP_TYPE_IPV6;
		*tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_NORMAL;

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
		if (*tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
			*tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_IPIP;
			filter->fdir_filter.tunnel_type = *tunnel_mode;
		}
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
				   "Not supported by fdir filter, vxlan/geneve only support inner tcp");
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
				   "Not supported by fdir filter, vxlan/geneve inner tcp only support src port,dst port");
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

	if (tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_MAX ||
	    tunnel_mode == HINIC3_FDIR_TUNNEL_MODE_NORMAL) {
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
					   "Invalid fdir filter vxlan/geneve inner udp mask or spec");
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

static inline enum hinic3_fdir_tunnel_mode
hinic3_flow_tunnel_mode(enum rte_flow_item_type type)
{
	switch (type) {
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		return HINIC3_FDIR_TUNNEL_MODE_GENEVE;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		return HINIC3_FDIR_TUNNEL_MODE_GPE;
	default:
		return HINIC3_FDIR_TUNNEL_MODE_VXLAN;
	}
}

static int
hinic3_flow_fdir_vxlan_geneve(struct rte_flow_error	  *error,
			      struct hinic3_filter_t	  *filter,
			      enum hinic3_fdir_tunnel_mode tunnel_mode,
			      const struct rte_flow_item  *flow_item)
{
	const uint8_t *vni;
	const uint8_t *vni_mask;
	uint32_t vxlan_vni_id = 0;
	uint32_t vxlan_vni_id_mask = 0;

	filter->fdir_filter.tunnel_type = tunnel_mode;

	if (!flow_item->spec && !flow_item->mask)
		return 0;

	if (filter->fdir_filter.outer_ip_type == HINIC3_FDIR_IP_TYPE_IPV6) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter vxlan/geneve mask or spec, ipv6 vxlan/geneve, don't support vni");
		return -rte_errno;
	}

	if (!flow_item->spec || !flow_item->mask) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
				   flow_item,
				   "Invalid fdir filter vxlan/geneve mask or spec");
		return -rte_errno;
	}

	switch (tunnel_mode) {
	case HINIC3_FDIR_TUNNEL_MODE_GENEVE: {
		const struct rte_flow_item_geneve *spec =
			(const struct rte_flow_item_geneve *)flow_item->spec;
		const struct rte_flow_item_geneve *mask =
			(const struct rte_flow_item_geneve *)flow_item->mask;

		/* Only VNI is matched; option length and protocol are not. */
		if (mask->ver_opt_len_o_c_rsvd0 || mask->protocol)
			goto unsupported;
		vni = spec->vni;
		vni_mask = mask->vni;
		break;
	}
	case HINIC3_FDIR_TUNNEL_MODE_GPE: {
		const struct rte_flow_item_vxlan_gpe *spec =
			(const struct rte_flow_item_vxlan_gpe *)flow_item->spec;
		const struct rte_flow_item_vxlan_gpe *mask =
			(const struct rte_flow_item_vxlan_gpe *)flow_item->mask;

		/* Only VNI is matched; flags, reserved bits and protocol are not. */
		if (mask->flags || mask->rsvd0[0] || mask->rsvd0[1] ||
		    mask->protocol || mask->rsvd1)
			goto unsupported;
		vni = spec->vni;
		vni_mask = mask->vni;
		break;
	}
	default:
		vni = ((const struct rte_flow_item_vxlan *)flow_item->spec)->vni;
		vni_mask =
			((const struct rte_flow_item_vxlan *)flow_item->mask)->vni;
		break;
	}

	memcpy(((uint8_t *)&vxlan_vni_id + 1), vni, 3);
	filter->fdir_filter.key_spec.tunnel.tunnel_id =
		rte_be_to_cpu_32(vxlan_vni_id);
	memcpy(((uint8_t *)&vxlan_vni_id_mask + 1), vni_mask, 3);
	filter->fdir_filter.key_mask.tunnel.tunnel_id =
		rte_be_to_cpu_32(vxlan_vni_id_mask);
	return 0;

unsupported:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, flow_item,
			   "Not supported by fdir filter, only vni is supported");
	return -rte_errno;
}

static int
hinic3_flow_parse_fdir_vxlan_geneve_pattern(__rte_unused struct rte_eth_dev *dev,
					    const struct rte_flow_item *pattern,
					    struct rte_flow_error *error,
					    struct hinic3_filter_t *filter)
{
	const struct rte_flow_item *flow_item = pattern;
	/* Pass tunnel_mode by pointer so the inner L3 parser can promote it
	 * to IPIP when a second IPv4/IPv6 header is recognized.
	 */
	enum hinic3_fdir_tunnel_mode tunnel_mode = HINIC3_FDIR_TUNNEL_MODE_MAX;
	enum rte_flow_item_type type;
	int err;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				   "Invalid pattern");
		return -rte_errno;
	}

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
				filter, flow_item, &tunnel_mode);
			if (err)
				return -rte_errno;
			break;

		case RTE_FLOW_ITEM_TYPE_IPV6:
			err = hinic3_flow_fdir_tunnel_ipv6(error,
				filter, flow_item, &tunnel_mode);
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
		case RTE_FLOW_ITEM_TYPE_GENEVE:
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			tunnel_mode = hinic3_flow_tunnel_mode(type);
			err = hinic3_flow_fdir_vxlan_geneve(error, filter, tunnel_mode, flow_item);
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
hinic3_flow_parse_fdir_vxlan_geneve_filter(struct rte_eth_dev *dev,
					   const struct rte_flow_attr *attr,
					   const struct rte_flow_item pattern[],
					   const struct rte_flow_action actions[],
					   struct rte_flow_error *error,
					   struct hinic3_filter_t *filter)
{
	int ret;

	ret = hinic3_flow_parse_fdir_vxlan_geneve_pattern(dev, pattern, error, filter);
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
	parse_filter = hinic3_find_parse_filter_func(dev, pattern);
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

const struct rte_flow_ops hinic3_flow_ops = {
	.validate = hinic3_flow_validate,
	.create = hinic3_flow_create,
	.destroy = hinic3_flow_destroy,
	.flush = hinic3_flow_flush,
};
