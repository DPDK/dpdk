/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_malloc.h>

#include "ice_ethdev.h"
#include "ice_generic_flow.h"
#include "ice_switch_filter.h"

static int ice_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static struct rte_flow *ice_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error);
static int ice_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct rte_flow_error *error);
static int ice_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error);

const struct rte_flow_ops ice_flow_ops = {
	.validate = ice_flow_validate,
	.create = ice_flow_create,
	.destroy = ice_flow_destroy,
	.flush = ice_flow_flush,
};

static int
ice_flow_valid_attr(const struct rte_flow_attr *attr,
		     struct rte_flow_error *error)
{
	/* Must be input direction */
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
				   attr, "Only support ingress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->egress) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
				   attr, "Not support egress.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->priority) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
				   attr, "Not support priority.");
		return -rte_errno;
	}

	/* Not supported */
	if (attr->group) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   attr, "Not support group.");
		return -rte_errno;
	}

	return 0;
}

/* Find the first VOID or non-VOID item pointer */
static const struct rte_flow_item *
ice_find_first_item(const struct rte_flow_item *item, bool is_void)
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
static void
ice_pattern_skip_void_item(struct rte_flow_item *items,
			    const struct rte_flow_item *pattern)
{
	uint32_t cpy_count = 0;
	const struct rte_flow_item *pb = pattern, *pe = pattern;

	for (;;) {
		/* Find a non-void item first */
		pb = ice_find_first_item(pb, false);
		if (pb->type == RTE_FLOW_ITEM_TYPE_END) {
			pe = pb;
			break;
		}

		/* Find a void item */
		pe = ice_find_first_item(pb + 1, true);

		cpy_count = pe - pb;
		rte_memcpy(items, pb, sizeof(struct rte_flow_item) * cpy_count);

		items += cpy_count;

		if (pe->type == RTE_FLOW_ITEM_TYPE_END) {
			pb = pe;
			break;
		}

		pb = pe + 1;
	}
	/* Copy the END item. */
	rte_memcpy(items, pe, sizeof(struct rte_flow_item));
}

/* Check if the pattern matches a supported item type array */
static bool
ice_match_pattern(enum rte_flow_item_type *item_array,
		const struct rte_flow_item *pattern)
{
	const struct rte_flow_item *item = pattern;

	while ((*item_array == item->type) &&
	       (*item_array != RTE_FLOW_ITEM_TYPE_END)) {
		item_array++;
		item++;
	}

	return (*item_array == RTE_FLOW_ITEM_TYPE_END &&
		item->type == RTE_FLOW_ITEM_TYPE_END);
}

static uint64_t ice_flow_valid_pattern(const struct rte_flow_item pattern[],
		struct rte_flow_error *error)
{
	uint16_t i = 0;
	uint64_t inset;
	struct rte_flow_item *items; /* used for pattern without VOID items */
	uint32_t item_num = 0; /* non-void item number */

	/* Get the non-void item number of pattern */
	while ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_END) {
		if ((pattern + i)->type != RTE_FLOW_ITEM_TYPE_VOID)
			item_num++;
		i++;
	}
	item_num++;

	items = rte_zmalloc("ice_pattern",
			    item_num * sizeof(struct rte_flow_item), 0);
	if (!items) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "No memory for PMD internal items.");
		return -ENOMEM;
	}

	ice_pattern_skip_void_item(items, pattern);

	for (i = 0; i < RTE_DIM(ice_supported_patterns); i++)
		if (ice_match_pattern(ice_supported_patterns[i].items,
				      items)) {
			inset = ice_supported_patterns[i].sw_fields;
			rte_free(items);
			return inset;
		}
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			   pattern, "Unsupported pattern");

	rte_free(items);
	return 0;
}

static uint64_t ice_get_flow_field(const struct rte_flow_item pattern[],
			struct rte_flow_error *error)
{
	const struct rte_flow_item *item = pattern;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_icmp *icmp_mask;
	const struct rte_flow_item_icmp6 *icmp6_mask;
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	const struct rte_flow_item_nvgre *nvgre_spec, *nvgre_mask;
	enum rte_flow_item_type item_type;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	uint64_t input_set = ICE_INSET_NONE;
	bool is_tunnel = false;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Not support range");
			return 0;
		}
		item_type = item->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			if (eth_spec && eth_mask) {
				if (rte_is_broadcast_ether_addr(&eth_mask->src))
					input_set |= ICE_INSET_SMAC;
				if (rte_is_broadcast_ether_addr(&eth_mask->dst))
					input_set |= ICE_INSET_DMAC;
				if (eth_mask->type == RTE_BE16(0xffff))
					input_set |= ICE_INSET_ETHERTYPE;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			if (!(ipv4_spec && ipv4_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid IPv4 spec or mask.");
				return 0;
			}

			/* Check IPv4 mask and update input set */
			if (ipv4_mask->hdr.version_ihl ||
			    ipv4_mask->hdr.total_length ||
			    ipv4_mask->hdr.packet_id ||
			    ipv4_mask->hdr.hdr_checksum) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid IPv4 mask.");
				return 0;
			}

			if (is_tunnel) {
				if (ipv4_mask->hdr.src_addr == UINT32_MAX)
					input_set |= ICE_INSET_TUN_IPV4_SRC;
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
					input_set |= ICE_INSET_TUN_IPV4_DST;
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
					input_set |= ICE_INSET_TUN_IPV4_TTL;
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
					input_set |= ICE_INSET_TUN_IPV4_PROTO;
			} else {
				if (ipv4_mask->hdr.src_addr == UINT32_MAX)
					input_set |= ICE_INSET_IPV4_SRC;
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
					input_set |= ICE_INSET_IPV4_DST;
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TTL;
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_PROTO;
				if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TOS;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			if (!(ipv6_spec && ipv6_mask)) {
				rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item, "Invalid IPv6 spec or mask");
				return 0;
			}

			if (ipv6_mask->hdr.payload_len ||
			    ipv6_mask->hdr.vtc_flow) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid IPv6 mask");
				return 0;
			}

			if (is_tunnel) {
				if (!memcmp(ipv6_mask->hdr.src_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
					input_set |= ICE_INSET_TUN_IPV6_SRC;
				if (!memcmp(ipv6_mask->hdr.dst_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
					input_set |= ICE_INSET_TUN_IPV6_DST;
				if (ipv6_mask->hdr.proto == UINT8_MAX)
					input_set |= ICE_INSET_TUN_IPV6_PROTO;
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
					input_set |= ICE_INSET_TUN_IPV6_TTL;
			} else {
				if (!memcmp(ipv6_mask->hdr.src_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
					input_set |= ICE_INSET_IPV6_SRC;
				if (!memcmp(ipv6_mask->hdr.dst_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
					input_set |= ICE_INSET_IPV6_DST;
				if (ipv6_mask->hdr.proto == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_PROTO;
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_HOP_LIMIT;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (!(udp_spec && udp_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid UDP mask");
				return 0;
			}

			/* Check UDP mask and update input set*/
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return 0;
			}

			if (is_tunnel) {
				if (udp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_SRC_PORT;
				if (udp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_DST_PORT;
			} else {
				if (udp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_SRC_PORT;
				if (udp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_DST_PORT;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (!(tcp_spec && tcp_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid TCP mask");
				return 0;
			}

			/* Check TCP mask and update input set */
			if (tcp_mask->hdr.sent_seq ||
			    tcp_mask->hdr.recv_ack ||
			    tcp_mask->hdr.data_off ||
			    tcp_mask->hdr.tcp_flags ||
			    tcp_mask->hdr.rx_win ||
			    tcp_mask->hdr.cksum ||
			    tcp_mask->hdr.tcp_urp) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
				return 0;
			}

			if (is_tunnel) {
				if (tcp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_SRC_PORT;
				if (tcp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_DST_PORT;
			} else {
				if (tcp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_SRC_PORT;
				if (tcp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_DST_PORT;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (!(sctp_spec && sctp_mask)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid SCTP mask");
				return 0;
			}

			/* Check SCTP mask and update input set */
			if (sctp_mask->hdr.cksum) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid SCTP mask");
				return 0;
			}

			if (is_tunnel) {
				if (sctp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_SRC_PORT;
				if (sctp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_TUN_DST_PORT;
			} else {
				if (sctp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_SRC_PORT;
				if (sctp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_DST_PORT;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			icmp_mask = item->mask;
			if (icmp_mask->hdr.icmp_code ||
			    icmp_mask->hdr.icmp_cksum ||
			    icmp_mask->hdr.icmp_ident ||
			    icmp_mask->hdr.icmp_seq_nb) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ICMP mask");
				return 0;
			}

			if (icmp_mask->hdr.icmp_type == UINT8_MAX)
				input_set |= ICE_INSET_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			icmp6_mask = item->mask;
			if (icmp6_mask->code ||
			    icmp6_mask->checksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid ICMP6 mask");
				return 0;
			}

			if (icmp6_mask->type == UINT8_MAX)
				input_set |= ICE_INSET_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			/* Check if VXLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!vxlan_spec && vxlan_mask) ||
			    (vxlan_spec && !vxlan_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid VXLAN item");
				return 0;
			}
			is_tunnel = 1;

			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			nvgre_spec = item->spec;
			nvgre_mask = item->mask;
			/* Check if VXLAN item is used to describe protocol.
			 * If yes, both spec and mask should be NULL.
			 * If no, both spec and mask shouldn't be NULL.
			 */
			if ((!nvgre_spec && nvgre_mask) ||
			    (nvgre_spec && !nvgre_mask)) {
				rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid NVGRE item");
				return 0;
			}
			is_tunnel = 1;

			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid mask no exist");
			break;
		}
	}
	return input_set;
}

static int ice_flow_valid_inset(const struct rte_flow_item pattern[],
			uint64_t inset, struct rte_flow_error *error)
{
	uint64_t fields;

	/* get valid field */
	fields = ice_get_flow_field(pattern, error);
	if (!fields || fields & (~inset)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		return -rte_errno;
	}

	return 0;
}

static int ice_flow_valid_action(struct rte_eth_dev *dev,
				const struct rte_flow_action *actions,
				struct rte_flow_error *error)
{
	const struct rte_flow_action_queue *act_q;
	uint16_t queue;

	switch (actions->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		act_q = actions->conf;
		queue = act_q->index;
		if (queue >= dev->data->nb_rx_queues) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   actions, "Invalid queue ID for"
					   " switch filter.");
			return -rte_errno;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_DROP:
		break;
	default:
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action.");
		return -rte_errno;
	}

	return 0;
}

static int
ice_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item pattern[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	uint64_t inset = 0;
	int ret = ICE_ERR_NOT_SUPPORTED;

	if (!pattern) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_NUM,
				   NULL, "NULL pattern.");
		return -rte_errno;
	}

	if (!actions) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "NULL action.");
		return -rte_errno;
	}

	if (!attr) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "NULL attribute.");
		return -rte_errno;
	}

	ret = ice_flow_valid_attr(attr, error);
	if (ret)
		return ret;

	inset = ice_flow_valid_pattern(pattern, error);
	if (!inset)
		return -rte_errno;

	ret = ice_flow_valid_inset(pattern, inset, error);
	if (ret)
		return ret;

	ret = ice_flow_valid_action(dev, actions, error);
	if (ret)
		return ret;

	return 0;
}

static struct rte_flow *
ice_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *flow = NULL;
	int ret;

	flow = rte_zmalloc("ice_flow", sizeof(struct rte_flow), 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return flow;
	}

	ret = ice_flow_validate(dev, attr, pattern, actions, error);
	if (ret < 0)
		goto free_flow;

	ret = ice_create_switch_filter(pf, pattern, actions, flow, error);
	if (ret)
		goto free_flow;

	TAILQ_INSERT_TAIL(&pf->flow_list, flow, node);
	return flow;

free_flow:
	rte_flow_error_set(error, -ret,
			   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			   "Failed to create flow.");
	rte_free(flow);
	return NULL;
}

static int
ice_flow_destroy(struct rte_eth_dev *dev,
		 struct rte_flow *flow,
		 struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = 0;

	ret = ice_destroy_switch_filter(pf, flow, error);

	if (!ret) {
		TAILQ_REMOVE(&pf->flow_list, flow, node);
		rte_free(flow);
	} else {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to destroy flow.");
	}

	return ret;
}

static int
ice_flow_flush(struct rte_eth_dev *dev,
	       struct rte_flow_error *error)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_flow *p_flow;
	int ret = 0;

	TAILQ_FOREACH(p_flow, &pf->flow_list, node) {
		ret = ice_flow_destroy(dev, p_flow, error);
		if (ret) {
			rte_flow_error_set(error, -ret,
					   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					   "Failed to flush SW flows.");
			return -rte_errno;
		}
	}

	return ret;
}
