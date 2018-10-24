/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_eth_ctrl.h>
#include <rte_ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_ip.h>

#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_prm.h"
#include "mlx5_glue.h"
#include "mlx5_flow.h"

#ifdef HAVE_IBV_FLOW_DV_SUPPORT

/**
 * Validate META item.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   Item specification.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_meta(struct rte_eth_dev *dev,
			   const struct rte_flow_item *item,
			   const struct rte_flow_attr *attr,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_meta *spec = item->spec;
	const struct rte_flow_item_meta *mask = item->mask;
	const struct rte_flow_item_meta nic_mask = {
		.data = RTE_BE32(UINT32_MAX)
	};
	int ret;
	uint64_t offloads = dev->data->dev_conf.txmode.offloads;

	if (!(offloads & DEV_TX_OFFLOAD_MATCH_METADATA))
		return rte_flow_error_set(error, EPERM,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL,
					  "match on metadata offload "
					  "configuration is off for this port");
	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (!spec->data)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  NULL,
					  "data cannot be zero");
	if (!mask)
		mask = &rte_flow_item_meta_mask;
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_meta),
					error);
	if (ret < 0)
		return ret;
	if (attr->ingress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "pattern not supported for ingress");
	return 0;
}

/**
 * Verify the @p attributes will be correctly understood by the NIC and store
 * them in the @p flow if everything is correct.
 *
 * @param[in] dev
 *   Pointer to dev struct.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_attributes(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attributes,
			    struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->config.flow_prio - 1;

	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups is not supported");
	if (attributes->priority != MLX5_FLOW_PRIO_RSVD &&
	    attributes->priority >= priority_max)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL,
					  "priority out of range");
	if (attributes->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL,
					  "transfer is not supported");
	if (!(attributes->egress ^ attributes->ingress))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "must specify exactly one of "
					  "ingress or egress");
	return 0;
}

/**
 * Internal validation function. For validating both actions and items.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
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
flow_dv_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	int ret;
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	int tunnel = 0;
	uint8_t next_protocol = 0xff;
	int actions_n = 0;

	if (items == NULL)
		return -1;
	ret = flow_dv_validate_attributes(dev, attr, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					       MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					       MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv4 *)
			     items->mask)->hdr.next_proto_id)
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					       MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			     items->mask)->hdr.proto)
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 items->spec)->hdr.proto;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
						(items, item_flags,
						 next_protocol,
						 &rte_flow_item_tcp_mask,
						 error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					       MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							  next_protocol,
							  error);
			if (ret < 0)
				return ret;
			item_flags |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					       MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			ret = mlx5_flow_validate_item_gre(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(items, item_flags,
							    error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_validate_item_vxlan_gpe(items,
								item_flags, dev,
								error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			ret = flow_dv_validate_item_meta(dev, items, attr,
							 error);
			if (ret < 0)
				return ret;
			item_flags |= MLX5_FLOW_ITEM_METADATA;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions_n == MLX5_DV_MAX_NUMBER_OF_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions, "too many actions");
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			ret = mlx5_flow_validate_action_flag(action_flags,
							     attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = mlx5_flow_validate_action_mark(actions,
							     action_flags,
							     attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			ret = mlx5_flow_validate_action_drop(action_flags,
							     attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DROP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = mlx5_flow_validate_action_queue(actions,
							      action_flags, dev,
							      attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = mlx5_flow_validate_action_rss(actions,
							    action_flags, dev,
							    attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_flow_validate_action_count(dev, attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			++actions_n;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	if (!(action_flags & MLX5_FLOW_FATE_ACTIONS) && attr->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, actions,
					  "no fate action is found");
	return 0;
}

/**
 * Internal preparation function. Allocates the DV flow size,
 * this size is constant.
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
flow_dv_prepare(const struct rte_flow_attr *attr __rte_unused,
		const struct rte_flow_item items[] __rte_unused,
		const struct rte_flow_action actions[] __rte_unused,
		uint64_t *item_flags __rte_unused,
		uint64_t *action_flags __rte_unused,
		struct rte_flow_error *error)
{
	uint32_t size = sizeof(struct mlx5_flow);
	struct mlx5_flow *flow;

	flow = rte_calloc(__func__, 1, size, 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create flow");
		return NULL;
	}
	flow->dv.value.size = MLX5_ST_SZ_DB(fte_match_param);
	return flow;
}

/**
 * Add Ethernet item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_eth(void *matcher, void *key,
			   const struct rte_flow_item *item, int inner)
{
	const struct rte_flow_item_eth *eth_m = item->mask;
	const struct rte_flow_item_eth *eth_v = item->spec;
	const struct rte_flow_item_eth nic_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.type = RTE_BE16(0xffff),
	};
	void *headers_m;
	void *headers_v;
	char *l24_v;
	unsigned int i;

	if (!eth_v)
		return;
	if (!eth_m)
		eth_m = &nic_mask;
	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m, dmac_47_16),
	       &eth_m->dst, sizeof(eth_m->dst));
	/* The value must be in the range of the mask. */
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, dmac_47_16);
	for (i = 0; i < sizeof(eth_m->dst); ++i)
		l24_v[i] = eth_m->dst.addr_bytes[i] & eth_v->dst.addr_bytes[i];
	memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m, smac_47_16),
	       &eth_m->src, sizeof(eth_m->src));
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, smac_47_16);
	/* The value must be in the range of the mask. */
	for (i = 0; i < sizeof(eth_m->dst); ++i)
		l24_v[i] = eth_m->src.addr_bytes[i] & eth_v->src.addr_bytes[i];
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ethertype,
		 rte_be_to_cpu_16(eth_m->type));
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v, ethertype);
	*(uint16_t *)(l24_v) = eth_m->type & eth_v->type;
}

/**
 * Add VLAN item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_vlan(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner)
{
	const struct rte_flow_item_vlan *vlan_m = item->mask;
	const struct rte_flow_item_vlan *vlan_v = item->spec;
	const struct rte_flow_item_vlan nic_mask = {
		.tci = RTE_BE16(0x0fff),
		.inner_type = RTE_BE16(0xffff),
	};
	void *headers_m;
	void *headers_v;
	uint16_t tci_m;
	uint16_t tci_v;

	if (!vlan_v)
		return;
	if (!vlan_m)
		vlan_m = &nic_mask;
	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	tci_m = rte_be_to_cpu_16(vlan_m->tci);
	tci_v = rte_be_to_cpu_16(vlan_m->tci & vlan_v->tci);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, cvlan_tag, 1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, cvlan_tag, 1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_vid, tci_m);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_vid, tci_v);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_cfi, tci_m >> 12);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_cfi, tci_v >> 12);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_prio, tci_m >> 13);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_prio, tci_v >> 13);
}

/**
 * Add IPV4 item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_ipv4(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner)
{
	const struct rte_flow_item_ipv4 *ipv4_m = item->mask;
	const struct rte_flow_item_ipv4 *ipv4_v = item->spec;
	const struct rte_flow_item_ipv4 nic_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.next_proto_id = 0xff,
		},
	};
	void *headers_m;
	void *headers_v;
	char *l24_m;
	char *l24_v;
	uint8_t tos;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0xf);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version, 4);
	if (!ipv4_v)
		return;
	if (!ipv4_m)
		ipv4_m = &nic_mask;
	l24_m = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m,
			     dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
	*(uint32_t *)l24_m = ipv4_m->hdr.dst_addr;
	*(uint32_t *)l24_v = ipv4_m->hdr.dst_addr & ipv4_v->hdr.dst_addr;
	l24_m = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m,
			  src_ipv4_src_ipv6.ipv4_layout.ipv4);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			  src_ipv4_src_ipv6.ipv4_layout.ipv4);
	*(uint32_t *)l24_m = ipv4_m->hdr.src_addr;
	*(uint32_t *)l24_v = ipv4_m->hdr.src_addr & ipv4_v->hdr.src_addr;
	tos = ipv4_m->hdr.type_of_service & ipv4_v->hdr.type_of_service;
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_ecn,
		 ipv4_m->hdr.type_of_service);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ecn, tos);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_dscp,
		 ipv4_m->hdr.type_of_service >> 2);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_dscp, tos >> 2);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol,
		 ipv4_m->hdr.next_proto_id);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
		 ipv4_v->hdr.next_proto_id & ipv4_m->hdr.next_proto_id);
}

/**
 * Add IPV6 item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_ipv6(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner)
{
	const struct rte_flow_item_ipv6 *ipv6_m = item->mask;
	const struct rte_flow_item_ipv6 *ipv6_v = item->spec;
	const struct rte_flow_item_ipv6 nic_mask = {
		.hdr = {
			.src_addr =
				"\xff\xff\xff\xff\xff\xff\xff\xff"
				"\xff\xff\xff\xff\xff\xff\xff\xff",
			.dst_addr =
				"\xff\xff\xff\xff\xff\xff\xff\xff"
				"\xff\xff\xff\xff\xff\xff\xff\xff",
			.vtc_flow = RTE_BE32(0xffffffff),
			.proto = 0xff,
			.hop_limits = 0xff,
		},
	};
	void *headers_m;
	void *headers_v;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	char *l24_m;
	char *l24_v;
	uint32_t vtc_m;
	uint32_t vtc_v;
	int i;
	int size;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0xf);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version, 6);
	if (!ipv6_v)
		return;
	if (!ipv6_m)
		ipv6_m = &nic_mask;
	size = sizeof(ipv6_m->hdr.dst_addr);
	l24_m = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m,
			     dst_ipv4_dst_ipv6.ipv6_layout.ipv6);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     dst_ipv4_dst_ipv6.ipv6_layout.ipv6);
	memcpy(l24_m, ipv6_m->hdr.dst_addr, size);
	for (i = 0; i < size; ++i)
		l24_v[i] = l24_m[i] & ipv6_v->hdr.dst_addr[i];
	l24_m = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_m,
			     src_ipv4_src_ipv6.ipv6_layout.ipv6);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     src_ipv4_src_ipv6.ipv6_layout.ipv6);
	memcpy(l24_m, ipv6_m->hdr.src_addr, size);
	for (i = 0; i < size; ++i)
		l24_v[i] = l24_m[i] & ipv6_v->hdr.src_addr[i];
	/* TOS. */
	vtc_m = rte_be_to_cpu_32(ipv6_m->hdr.vtc_flow);
	vtc_v = rte_be_to_cpu_32(ipv6_m->hdr.vtc_flow & ipv6_v->hdr.vtc_flow);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_ecn, vtc_m >> 20);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ecn, vtc_v >> 20);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_dscp, vtc_m >> 22);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_dscp, vtc_v >> 22);
	/* Label. */
	if (inner) {
		MLX5_SET(fte_match_set_misc, misc_m, inner_ipv6_flow_label,
			 vtc_m);
		MLX5_SET(fte_match_set_misc, misc_v, inner_ipv6_flow_label,
			 vtc_v);
	} else {
		MLX5_SET(fte_match_set_misc, misc_m, outer_ipv6_flow_label,
			 vtc_m);
		MLX5_SET(fte_match_set_misc, misc_v, outer_ipv6_flow_label,
			 vtc_v);
	}
	/* Protocol. */
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol,
		 ipv6_m->hdr.proto);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
		 ipv6_v->hdr.proto & ipv6_m->hdr.proto);
}

/**
 * Add TCP item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_tcp(void *matcher, void *key,
			   const struct rte_flow_item *item,
			   int inner)
{
	const struct rte_flow_item_tcp *tcp_m = item->mask;
	const struct rte_flow_item_tcp *tcp_v = item->spec;
	void *headers_m;
	void *headers_v;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xff);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, IPPROTO_TCP);
	if (!tcp_v)
		return;
	if (!tcp_m)
		tcp_m = &rte_flow_item_tcp_mask;
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, tcp_sport,
		 rte_be_to_cpu_16(tcp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_sport,
		 rte_be_to_cpu_16(tcp_v->hdr.src_port & tcp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, tcp_dport,
		 rte_be_to_cpu_16(tcp_m->hdr.dst_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_dport,
		 rte_be_to_cpu_16(tcp_v->hdr.dst_port & tcp_m->hdr.dst_port));
}

/**
 * Add UDP item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_udp(void *matcher, void *key,
			   const struct rte_flow_item *item,
			   int inner)
{
	const struct rte_flow_item_udp *udp_m = item->mask;
	const struct rte_flow_item_udp *udp_v = item->spec;
	void *headers_m;
	void *headers_v;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xff);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, IPPROTO_UDP);
	if (!udp_v)
		return;
	if (!udp_m)
		udp_m = &rte_flow_item_udp_mask;
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_sport,
		 rte_be_to_cpu_16(udp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_sport,
		 rte_be_to_cpu_16(udp_v->hdr.src_port & udp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport,
		 rte_be_to_cpu_16(udp_m->hdr.dst_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
		 rte_be_to_cpu_16(udp_v->hdr.dst_port & udp_m->hdr.dst_port));
}

/**
 * Add GRE item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_gre(void *matcher, void *key,
			   const struct rte_flow_item *item,
			   int inner)
{
	const struct rte_flow_item_gre *gre_m = item->mask;
	const struct rte_flow_item_gre *gre_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xff);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, IPPROTO_GRE);
	if (!gre_v)
		return;
	if (!gre_m)
		gre_m = &rte_flow_item_gre_mask;
	MLX5_SET(fte_match_set_misc, misc_m, gre_protocol,
		 rte_be_to_cpu_16(gre_m->protocol));
	MLX5_SET(fte_match_set_misc, misc_v, gre_protocol,
		 rte_be_to_cpu_16(gre_v->protocol & gre_m->protocol));
}

/**
 * Add NVGRE item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_nvgre(void *matcher, void *key,
			     const struct rte_flow_item *item,
			     int inner)
{
	const struct rte_flow_item_nvgre *nvgre_m = item->mask;
	const struct rte_flow_item_nvgre *nvgre_v = item->spec;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	const char *tni_flow_id_m = (const char *)nvgre_m->tni;
	const char *tni_flow_id_v = (const char *)nvgre_v->tni;
	char *gre_key_m;
	char *gre_key_v;
	int size;
	int i;

	flow_dv_translate_item_gre(matcher, key, item, inner);
	if (!nvgre_v)
		return;
	if (!nvgre_m)
		nvgre_m = &rte_flow_item_nvgre_mask;
	size = sizeof(nvgre_m->tni) + sizeof(nvgre_m->flow_id);
	gre_key_m = MLX5_ADDR_OF(fte_match_set_misc, misc_m, gre_key_h);
	gre_key_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, gre_key_h);
	memcpy(gre_key_m, tni_flow_id_m, size);
	for (i = 0; i < size; ++i)
		gre_key_v[i] = gre_key_m[i] & tni_flow_id_v[i];
}

/**
 * Add VXLAN item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_vxlan(void *matcher, void *key,
			     const struct rte_flow_item *item,
			     int inner)
{
	const struct rte_flow_item_vxlan *vxlan_m = item->mask;
	const struct rte_flow_item_vxlan *vxlan_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	char *vni_m;
	char *vni_v;
	uint16_t dport;
	int size;
	int i;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	dport = item->type == RTE_FLOW_ITEM_TYPE_VXLAN ?
		MLX5_UDP_PORT_VXLAN : MLX5_UDP_PORT_VXLAN_GPE;
	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport, 0xFFFF);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport, dport);
	}
	if (!vxlan_v)
		return;
	if (!vxlan_m)
		vxlan_m = &rte_flow_item_vxlan_mask;
	size = sizeof(vxlan_m->vni);
	vni_m = MLX5_ADDR_OF(fte_match_set_misc, misc_m, vxlan_vni);
	vni_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, vxlan_vni);
	memcpy(vni_m, vxlan_m->vni, size);
	for (i = 0; i < size; ++i)
		vni_v[i] = vni_m[i] & vxlan_v->vni[i];
}

/**
 * Add META item to matcher
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_meta(void *matcher, void *key,
			    const struct rte_flow_item *item)
{
	const struct rte_flow_item_meta *meta_m;
	const struct rte_flow_item_meta *meta_v;
	void *misc2_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters_2);
	void *misc2_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);

	meta_m = (const void *)item->mask;
	if (!meta_m)
		meta_m = &rte_flow_item_meta_mask;
	meta_v = (const void *)item->spec;
	if (meta_v) {
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_a,
			 rte_be_to_cpu_32(meta_m->data));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_a,
			 rte_be_to_cpu_32(meta_v->data & meta_m->data));
	}
}

/**
 * Update the matcher and the value based the selected item.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_create_item(void *matcher, void *key,
		    const struct rte_flow_item *item,
		    struct mlx5_flow *dev_flow,
		    int inner)
{
	struct mlx5_flow_dv_matcher *tmatcher = matcher;

	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		flow_dv_translate_item_eth(tmatcher->mask.buf, key, item,
					   inner);
		tmatcher->priority = MLX5_PRIORITY_MAP_L2;
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		flow_dv_translate_item_vlan(tmatcher->mask.buf, key, item,
					    inner);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		flow_dv_translate_item_ipv4(tmatcher->mask.buf, key, item,
					    inner);
		tmatcher->priority = MLX5_PRIORITY_MAP_L3;
		dev_flow->dv.hash_fields |=
			mlx5_flow_hashfields_adjust(dev_flow, inner,
						    MLX5_IPV4_LAYER_TYPES,
						    MLX5_IPV4_IBV_RX_HASH);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		flow_dv_translate_item_ipv6(tmatcher->mask.buf, key, item,
					    inner);
		tmatcher->priority = MLX5_PRIORITY_MAP_L3;
		dev_flow->dv.hash_fields |=
			mlx5_flow_hashfields_adjust(dev_flow, inner,
						    MLX5_IPV6_LAYER_TYPES,
						    MLX5_IPV6_IBV_RX_HASH);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		flow_dv_translate_item_tcp(tmatcher->mask.buf, key, item,
					   inner);
		tmatcher->priority = MLX5_PRIORITY_MAP_L4;
		dev_flow->dv.hash_fields |=
			mlx5_flow_hashfields_adjust(dev_flow, inner,
						    ETH_RSS_TCP,
						    (IBV_RX_HASH_SRC_PORT_TCP |
						     IBV_RX_HASH_DST_PORT_TCP));
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		flow_dv_translate_item_udp(tmatcher->mask.buf, key, item,
					   inner);
		tmatcher->priority = MLX5_PRIORITY_MAP_L4;
		dev_flow->verbs.hash_fields |=
			mlx5_flow_hashfields_adjust(dev_flow, inner,
						    ETH_RSS_UDP,
						    (IBV_RX_HASH_SRC_PORT_UDP |
						     IBV_RX_HASH_DST_PORT_UDP));
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		flow_dv_translate_item_gre(tmatcher->mask.buf, key, item,
					   inner);
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		flow_dv_translate_item_nvgre(tmatcher->mask.buf, key, item,
					     inner);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		flow_dv_translate_item_vxlan(tmatcher->mask.buf, key, item,
					     inner);
		break;
	case RTE_FLOW_ITEM_TYPE_META:
		flow_dv_translate_item_meta(tmatcher->mask.buf, key, item);
		break;
	default:
		break;
	}
}

/**
 * Store the requested actions in an array.
 *
 * @param[in] action
 *   Flow action to translate.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 */
static void
flow_dv_create_action(const struct rte_flow_action *action,
		      struct mlx5_flow *dev_flow)
{
	const struct rte_flow_action_queue *queue;
	const struct rte_flow_action_rss *rss;
	int actions_n = dev_flow->dv.actions_n;
	struct rte_flow *flow = dev_flow->flow;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_VOID:
		break;
	case RTE_FLOW_ACTION_TYPE_FLAG:
		dev_flow->dv.actions[actions_n].type = MLX5DV_FLOW_ACTION_TAG;
		dev_flow->dv.actions[actions_n].tag_value =
			mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT);
		actions_n++;
		flow->actions |= MLX5_FLOW_ACTION_FLAG;
		break;
	case RTE_FLOW_ACTION_TYPE_MARK:
		dev_flow->dv.actions[actions_n].type = MLX5DV_FLOW_ACTION_TAG;
		dev_flow->dv.actions[actions_n].tag_value =
			mlx5_flow_mark_set
			(((const struct rte_flow_action_mark *)
			  (action->conf))->id);
		flow->actions |= MLX5_FLOW_ACTION_MARK;
		actions_n++;
		break;
	case RTE_FLOW_ACTION_TYPE_DROP:
		dev_flow->dv.actions[actions_n].type = MLX5DV_FLOW_ACTION_DROP;
		flow->actions |= MLX5_FLOW_ACTION_DROP;
		break;
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		queue = action->conf;
		flow->rss.queue_num = 1;
		(*flow->queue)[0] = queue->index;
		flow->actions |= MLX5_FLOW_ACTION_QUEUE;
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		rss = action->conf;
		if (flow->queue)
			memcpy((*flow->queue), rss->queue,
			       rss->queue_num * sizeof(uint16_t));
		flow->rss.queue_num = rss->queue_num;
		memcpy(flow->key, rss->key, MLX5_RSS_HASH_KEY_LEN);
		flow->rss.types = rss->types;
		flow->rss.level = rss->level;
		/* Added to array only in apply since we need the QP */
		flow->actions |= MLX5_FLOW_ACTION_RSS;
		break;
	default:
		break;
	}
	dev_flow->dv.actions_n = actions_n;
}

static uint32_t matcher_zero[MLX5_ST_SZ_DW(fte_match_param)] = { 0 };

#define HEADER_IS_ZERO(match_criteria, headers)				     \
	!(memcmp(MLX5_ADDR_OF(fte_match_param, match_criteria, headers),     \
		 matcher_zero, MLX5_FLD_SZ_BYTES(fte_match_param, headers))) \

/**
 * Calculate flow matcher enable bitmap.
 *
 * @param match_criteria
 *   Pointer to flow matcher criteria.
 *
 * @return
 *   Bitmap of enabled fields.
 */
static uint8_t
flow_dv_matcher_enable(uint32_t *match_criteria)
{
	uint8_t match_criteria_enable;

	match_criteria_enable =
		(!HEADER_IS_ZERO(match_criteria, outer_headers)) <<
		MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, inner_headers)) <<
		MLX5_MATCH_CRITERIA_ENABLE_INNER_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_2)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT;

	return match_criteria_enable;
}

/**
 * Register the flow matcher.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] matcher
 *   Pointer to flow matcher.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_matcher_register(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_matcher *matcher,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_matcher *cache_matcher;
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.match_mask = (void *)&matcher->mask,
	};

	/* Lookup from cache. */
	LIST_FOREACH(cache_matcher, &priv->matchers, next) {
		if (matcher->crc == cache_matcher->crc &&
		    matcher->priority == cache_matcher->priority &&
		    matcher->egress == cache_matcher->egress &&
		    !memcmp((const void *)matcher->mask.buf,
			    (const void *)cache_matcher->mask.buf,
			    cache_matcher->mask.size)) {
			DRV_LOG(DEBUG,
				"priority %hd use %s matcher %p: refcnt %d++",
				cache_matcher->priority,
				cache_matcher->egress ? "tx" : "rx",
				(void *)cache_matcher,
				rte_atomic32_read(&cache_matcher->refcnt));
			rte_atomic32_inc(&cache_matcher->refcnt);
			dev_flow->dv.matcher = cache_matcher;
			return 0;
		}
	}
	/* Register new matcher. */
	cache_matcher = rte_calloc(__func__, 1, sizeof(*cache_matcher), 0);
	if (!cache_matcher)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate matcher memory");
	*cache_matcher = *matcher;
	dv_attr.match_criteria_enable =
		flow_dv_matcher_enable(cache_matcher->mask.buf);
	dv_attr.priority = matcher->priority;
	if (matcher->egress)
		dv_attr.flags |= IBV_FLOW_ATTR_FLAGS_EGRESS;
	cache_matcher->matcher_object =
		mlx5_glue->dv_create_flow_matcher(priv->ctx, &dv_attr);
	if (!cache_matcher->matcher_object)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create matcher");
	rte_atomic32_inc(&cache_matcher->refcnt);
	LIST_INSERT_HEAD(&priv->matchers, cache_matcher, next);
	dev_flow->dv.matcher = cache_matcher;
	DRV_LOG(DEBUG, "priority %hd new %s matcher %p: refcnt %d",
		cache_matcher->priority,
		cache_matcher->egress ? "tx" : "rx", (void *)cache_matcher,
		rte_atomic32_read(&cache_matcher->refcnt));
	return 0;
}


/**
 * Fill the flow with DV spec.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] dev_flow
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
flow_dv_translate(struct rte_eth_dev *dev,
		  struct mlx5_flow *dev_flow,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[] __rte_unused,
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	uint64_t priority = attr->priority;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
	};
	void *match_value = dev_flow->dv.value.buf;
	int tunnel = 0;

	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = priv->config.flow_prio - 1;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		tunnel = !!(dev_flow->layers & MLX5_FLOW_LAYER_TUNNEL);
		flow_dv_create_item(&matcher, match_value, items, dev_flow,
				    tunnel);
	}
	matcher.crc = rte_raw_cksum((const void *)matcher.mask.buf,
				     matcher.mask.size);
	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = priv->config.flow_prio - 1;
	matcher.priority = mlx5_flow_adjust_priority(dev, priority,
						     matcher.priority);
	matcher.egress = attr->egress;
	if (flow_dv_matcher_register(dev, &matcher, dev_flow, error))
		return -rte_errno;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++)
		flow_dv_create_action(actions, dev_flow);
	return 0;
}

/**
 * Apply the flow to the NIC.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
	      struct rte_flow_error *error)
{
	struct mlx5_flow_dv *dv;
	struct mlx5_flow *dev_flow;
	int n;
	int err;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		dv = &dev_flow->dv;
		n = dv->actions_n;
		if (flow->actions & MLX5_FLOW_ACTION_DROP) {
			dv->hrxq = mlx5_hrxq_drop_new(dev);
			if (!dv->hrxq) {
				rte_flow_error_set
					(error, errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get drop hash queue");
				goto error;
			}
			dv->actions[n].type = MLX5DV_FLOW_ACTION_DEST_IBV_QP;
			dv->actions[n].qp = dv->hrxq->qp;
			n++;
		} else if (flow->actions &
			   (MLX5_FLOW_ACTION_QUEUE | MLX5_FLOW_ACTION_RSS)) {
			struct mlx5_hrxq *hrxq;
			hrxq = mlx5_hrxq_get(dev, flow->key,
					     MLX5_RSS_HASH_KEY_LEN,
					     dv->hash_fields,
					     (*flow->queue),
					     flow->rss.queue_num);
			if (!hrxq)
				hrxq = mlx5_hrxq_new
					(dev, flow->key, MLX5_RSS_HASH_KEY_LEN,
					 dv->hash_fields, (*flow->queue),
					 flow->rss.queue_num,
					 !!(dev_flow->layers &
					    MLX5_FLOW_LAYER_TUNNEL));
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			dv->hrxq = hrxq;
			dv->actions[n].type = MLX5DV_FLOW_ACTION_DEST_IBV_QP;
			dv->actions[n].qp = hrxq->qp;
			n++;
		}
		dv->flow =
			mlx5_glue->dv_create_flow(dv->matcher->matcher_object,
						  (void *)&dv->value, n,
						  dv->actions);
		if (!dv->flow) {
			rte_flow_error_set(error, errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "hardware refuses to create flow");
			goto error;
		}
	}
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		struct mlx5_flow_dv *dv = &dev_flow->dv;
		if (dv->hrxq) {
			if (flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, dv->hrxq);
			dv->hrxq = NULL;
		}
	}
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Release the flow matcher.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_matcher_release(struct rte_eth_dev *dev,
			struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_matcher *matcher = flow->dv.matcher;

	assert(matcher->matcher_object);
	DRV_LOG(DEBUG, "port %u matcher %p: refcnt %d--",
		dev->data->port_id, (void *)matcher,
		rte_atomic32_read(&matcher->refcnt));
	if (rte_atomic32_dec_and_test(&matcher->refcnt)) {
		claim_zero(mlx5_glue->dv_destroy_flow_matcher
			   (matcher->matcher_object));
		LIST_REMOVE(matcher, next);
		rte_free(matcher);
		DRV_LOG(DEBUG, "port %u matcher %p: removed",
			dev->data->port_id, (void *)matcher);
		return 0;
	}
	return 1;
}

/**
 * Remove the flow from the NIC but keeps it in memory.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
flow_dv_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_dv *dv;
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		dv = &dev_flow->dv;
		if (dv->flow) {
			claim_zero(mlx5_glue->destroy_flow(dv->flow));
			dv->flow = NULL;
		}
		if (dv->hrxq) {
			if (flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, dv->hrxq);
			dv->hrxq = NULL;
		}
	}
	if (flow->counter)
		flow->counter = NULL;
}

/**
 * Remove the flow from the NIC and the memory.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
flow_dv_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	if (!flow)
		return;
	flow_dv_remove(dev, flow);
	while (!LIST_EMPTY(&flow->dev_flows)) {
		dev_flow = LIST_FIRST(&flow->dev_flows);
		LIST_REMOVE(dev_flow, next);
		if (dev_flow->dv.matcher)
			flow_dv_matcher_release(dev, dev_flow);
		rte_free(dev_flow);
	}
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_dv_query(struct rte_eth_dev *dev __rte_unused,
	      struct rte_flow *flow __rte_unused,
	      const struct rte_flow_action *actions __rte_unused,
	      void *data __rte_unused,
	      struct rte_flow_error *error __rte_unused)
{
	rte_errno = ENOTSUP;
	return -rte_errno;
}


const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops = {
	.validate = flow_dv_validate,
	.prepare = flow_dv_prepare,
	.translate = flow_dv_translate,
	.apply = flow_dv_apply,
	.remove = flow_dv_remove,
	.destroy = flow_dv_destroy,
	.query = flow_dv_query,
};

#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
