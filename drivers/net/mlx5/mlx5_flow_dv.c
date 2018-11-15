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
#include <rte_gre.h>

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
 * Validate the L2 encap action.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the encap action.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_l2_encap(uint64_t action_flags,
				 const struct rte_flow_action *action,
				 const struct rte_flow_attr *attr,
				 struct rte_flow_error *error)
{
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (action_flags & MLX5_FLOW_ACTION_DROP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't drop and encap in same flow");
	if (action_flags & (MLX5_FLOW_ENCAP_ACTIONS | MLX5_FLOW_DECAP_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can only have a single encap or"
					  " decap action in a flow");
	if (attr->ingress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "encap action not supported for "
					  "ingress");
	return 0;
}

/**
 * Validate the L2 decap action.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_l2_decap(uint64_t action_flags,
				 const struct rte_flow_attr *attr,
				 struct rte_flow_error *error)
{
	if (action_flags & MLX5_FLOW_ACTION_DROP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't drop and decap in same flow");
	if (action_flags & (MLX5_FLOW_ENCAP_ACTIONS | MLX5_FLOW_DECAP_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can only have a single encap or"
					  " decap action in a flow");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					  NULL,
					  "decap action not supported for "
					  "egress");
	return 0;
}

/**
 * Validate the raw encap action.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the encap action.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_raw_encap(uint64_t action_flags,
				  const struct rte_flow_action *action,
				  const struct rte_flow_attr *attr,
				  struct rte_flow_error *error)
{
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (action_flags & MLX5_FLOW_ACTION_DROP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't drop and encap in same flow");
	if (action_flags & MLX5_FLOW_ENCAP_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can only have a single encap"
					  " action in a flow");
	/* encap without preceding decap is not supported for ingress */
	if (attr->ingress && !(action_flags & MLX5_FLOW_ACTION_RAW_DECAP))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "encap action not supported for "
					  "ingress");
	return 0;
}

/**
 * Validate the raw decap action.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the encap action.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_raw_decap(uint64_t action_flags,
				  const struct rte_flow_action *action,
				  const struct rte_flow_attr *attr,
				  struct rte_flow_error *error)
{
	if (action_flags & MLX5_FLOW_ACTION_DROP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't drop and decap in same flow");
	if (action_flags & MLX5_FLOW_ENCAP_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have encap action before"
					  " decap action");
	if (action_flags & MLX5_FLOW_DECAP_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can only have a single decap"
					  " action in a flow");
	/* decap action is valid on egress only if it is followed by encap */
	if (attr->egress) {
		for (; action->type != RTE_FLOW_ACTION_TYPE_END &&
		       action->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP;
		       action++) {
		}
		if (action->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP)
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					 NULL, "decap action not supported"
					 " for egress");
	}
	return 0;
}


/**
 * Find existing encap/decap resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] resource
 *   Pointer to encap/decap resource.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_encap_decap_resource_register
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_encap_decap_resource *resource,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_encap_decap_resource *cache_resource;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &priv->encaps_decaps, next) {
		if (resource->reformat_type == cache_resource->reformat_type &&
		    resource->ft_type == cache_resource->ft_type &&
		    resource->size == cache_resource->size &&
		    !memcmp((const void *)resource->buf,
			    (const void *)cache_resource->buf,
			    resource->size)) {
			DRV_LOG(DEBUG, "encap/decap resource %p: refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->dv.encap_decap = cache_resource;
			return 0;
		}
	}
	/* Register new encap/decap resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	cache_resource->verbs_action =
		mlx5_glue->dv_create_flow_action_packet_reformat
			(priv->ctx, cache_resource->size,
			 (cache_resource->size ? cache_resource->buf : NULL),
			 cache_resource->reformat_type,
			 cache_resource->ft_type);
	if (!cache_resource->verbs_action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&priv->encaps_decaps, cache_resource, next);
	dev_flow->dv.encap_decap = cache_resource;
	DRV_LOG(DEBUG, "new encap/decap resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Get the size of specific rte_flow_item_type
 *
 * @param[in] item_type
 *   Tested rte_flow_item_type.
 *
 * @return
 *   sizeof struct item_type, 0 if void or irrelevant.
 */
static size_t
flow_dv_get_item_len(const enum rte_flow_item_type item_type)
{
	size_t retval;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		retval = sizeof(struct rte_flow_item_eth);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		retval = sizeof(struct rte_flow_item_vlan);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		retval = sizeof(struct rte_flow_item_ipv4);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		retval = sizeof(struct rte_flow_item_ipv6);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		retval = sizeof(struct rte_flow_item_udp);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		retval = sizeof(struct rte_flow_item_tcp);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		retval = sizeof(struct rte_flow_item_vxlan);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		retval = sizeof(struct rte_flow_item_gre);
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		retval = sizeof(struct rte_flow_item_nvgre);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		retval = sizeof(struct rte_flow_item_vxlan_gpe);
		break;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		retval = sizeof(struct rte_flow_item_mpls);
		break;
	case RTE_FLOW_ITEM_TYPE_VOID: /* Fall through. */
	default:
		retval = 0;
		break;
	}
	return retval;
}

#define MLX5_ENCAP_IPV4_VERSION		0x40
#define MLX5_ENCAP_IPV4_IHL_MIN		0x05
#define MLX5_ENCAP_IPV4_TTL_DEF		0x40
#define MLX5_ENCAP_IPV6_VTC_FLOW	0x60000000
#define MLX5_ENCAP_IPV6_HOP_LIMIT	0xff
#define MLX5_ENCAP_VXLAN_FLAGS		0x08000000
#define MLX5_ENCAP_VXLAN_GPE_FLAGS	0x04

/**
 * Convert the encap action data from list of rte_flow_item to raw buffer
 *
 * @param[in] items
 *   Pointer to rte_flow_item objects list.
 * @param[out] buf
 *   Pointer to the output buffer.
 * @param[out] size
 *   Pointer to the output buffer size.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_encap_data(const struct rte_flow_item *items, uint8_t *buf,
			   size_t *size, struct rte_flow_error *error)
{
	struct ether_hdr *eth = NULL;
	struct vlan_hdr *vlan = NULL;
	struct ipv4_hdr *ipv4 = NULL;
	struct ipv6_hdr *ipv6 = NULL;
	struct udp_hdr *udp = NULL;
	struct vxlan_hdr *vxlan = NULL;
	struct vxlan_gpe_hdr *vxlan_gpe = NULL;
	struct gre_hdr *gre = NULL;
	size_t len;
	size_t temp_size = 0;

	if (!items)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "invalid empty data");
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		len = flow_dv_get_item_len(items->type);
		if (len + temp_size > MLX5_ENCAP_MAX_LEN)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  (void *)items->type,
						  "items total size is too big"
						  " for encap action");
		rte_memcpy((void *)&buf[temp_size], items->spec, len);
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth = (struct ether_hdr *)&buf[temp_size];
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan = (struct vlan_hdr *)&buf[temp_size];
			if (!eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"eth header not found");
			if (!eth->ether_type)
				eth->ether_type = RTE_BE16(ETHER_TYPE_VLAN);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = (struct ipv4_hdr *)&buf[temp_size];
			if (!vlan && !eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"neither eth nor vlan"
						" header found");
			if (vlan && !vlan->eth_proto)
				vlan->eth_proto = RTE_BE16(ETHER_TYPE_IPv4);
			else if (eth && !eth->ether_type)
				eth->ether_type = RTE_BE16(ETHER_TYPE_IPv4);
			if (!ipv4->version_ihl)
				ipv4->version_ihl = MLX5_ENCAP_IPV4_VERSION |
						    MLX5_ENCAP_IPV4_IHL_MIN;
			if (!ipv4->time_to_live)
				ipv4->time_to_live = MLX5_ENCAP_IPV4_TTL_DEF;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6 = (struct ipv6_hdr *)&buf[temp_size];
			if (!vlan && !eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"neither eth nor vlan"
						" header found");
			if (vlan && !vlan->eth_proto)
				vlan->eth_proto = RTE_BE16(ETHER_TYPE_IPv6);
			else if (eth && !eth->ether_type)
				eth->ether_type = RTE_BE16(ETHER_TYPE_IPv6);
			if (!ipv6->vtc_flow)
				ipv6->vtc_flow =
					RTE_BE32(MLX5_ENCAP_IPV6_VTC_FLOW);
			if (!ipv6->hop_limits)
				ipv6->hop_limits = MLX5_ENCAP_IPV6_HOP_LIMIT;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp = (struct udp_hdr *)&buf[temp_size];
			if (!ipv4 && !ipv6)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"ip header not found");
			if (ipv4 && !ipv4->next_proto_id)
				ipv4->next_proto_id = IPPROTO_UDP;
			else if (ipv6 && !ipv6->proto)
				ipv6->proto = IPPROTO_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			vxlan = (struct vxlan_hdr *)&buf[temp_size];
			if (!udp)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"udp header not found");
			if (!udp->dst_port)
				udp->dst_port = RTE_BE16(MLX5_UDP_PORT_VXLAN);
			if (!vxlan->vx_flags)
				vxlan->vx_flags =
					RTE_BE32(MLX5_ENCAP_VXLAN_FLAGS);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			vxlan_gpe = (struct vxlan_gpe_hdr *)&buf[temp_size];
			if (!udp)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"udp header not found");
			if (!vxlan_gpe->proto)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"next protocol not found");
			if (!udp->dst_port)
				udp->dst_port =
					RTE_BE16(MLX5_UDP_PORT_VXLAN_GPE);
			if (!vxlan_gpe->vx_flags)
				vxlan_gpe->vx_flags =
						MLX5_ENCAP_VXLAN_GPE_FLAGS;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			gre = (struct gre_hdr *)&buf[temp_size];
			if (!gre->proto)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"next protocol not found");
			if (!ipv4 && !ipv6)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"ip header not found");
			if (ipv4 && !ipv4->next_proto_id)
				ipv4->next_proto_id = IPPROTO_GRE;
			else if (ipv6 && !ipv6->proto)
				ipv6->proto = IPPROTO_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  (void *)items->type,
						  "unsupported item type");
			break;
		}
		temp_size += len;
	}
	*size = temp_size;
	return 0;
}

/**
 * Convert L2 encap action to DV specification.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to action structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_create_action_l2_encap(struct rte_eth_dev *dev,
			       const struct rte_flow_action *action,
			       struct mlx5_flow *dev_flow,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *encap_data;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	struct mlx5_flow_dv_encap_decap_resource res = {
		.reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL,
		.ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX,
	};

	if (action->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
		raw_encap_data =
			(const struct rte_flow_action_raw_encap *)action->conf;
		res.size = raw_encap_data->size;
		memcpy(res.buf, raw_encap_data->data, res.size);
	} else {
		if (action->type == RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP)
			encap_data =
				((const struct rte_flow_action_vxlan_encap *)
						action->conf)->definition;
		else
			encap_data =
				((const struct rte_flow_action_nvgre_encap *)
						action->conf)->definition;
		if (flow_dv_convert_encap_data(encap_data, res.buf,
					       &res.size, error))
			return -rte_errno;
	}
	if (flow_dv_encap_decap_resource_register(dev, &res, dev_flow, error))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "can't create L2 encap action");
	return 0;
}

/**
 * Convert L2 decap action to DV specification.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_create_action_l2_decap(struct rte_eth_dev *dev,
			       struct mlx5_flow *dev_flow,
			       struct rte_flow_error *error)
{
	struct mlx5_flow_dv_encap_decap_resource res = {
		.size = 0,
		.reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2,
		.ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX,
	};

	if (flow_dv_encap_decap_resource_register(dev, &res, dev_flow, error))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "can't create L2 decap action");
	return 0;
}

/**
 * Convert raw decap/encap (L3 tunnel) action to DV specification.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to action structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_create_action_raw_encap(struct rte_eth_dev *dev,
				const struct rte_flow_action *action,
				struct mlx5_flow *dev_flow,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	const struct rte_flow_action_raw_encap *encap_data;
	struct mlx5_flow_dv_encap_decap_resource res;

	encap_data = (const struct rte_flow_action_raw_encap *)action->conf;
	res.size = encap_data->size;
	memcpy(res.buf, encap_data->data, res.size);
	res.reformat_type = attr->egress ?
		MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL :
		MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2;
	res.ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
				     MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	if (flow_dv_encap_decap_resource_register(dev, &res, dev_flow, error))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "can't create encap action");
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
	uint64_t last_item = 0;
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
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					     MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_validate_item_vlan(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					     MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_validate_item_ipv4(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv4 *)
			     items->mask)->hdr.next_proto_id) {
				next_protocol =
					((const struct rte_flow_item_ipv4 *)
					 (items->spec))->hdr.next_proto_id;
				next_protocol &=
					((const struct rte_flow_item_ipv4 *)
					 (items->mask))->hdr.next_proto_id;
			} else {
				/* Reset for inner layer. */
				next_protocol = 0xff;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_validate_item_ipv6(items, item_flags,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			     items->mask)->hdr.proto) {
				next_protocol =
					((const struct rte_flow_item_ipv6 *)
					 items->spec)->hdr.proto;
				next_protocol &=
					((const struct rte_flow_item_ipv6 *)
					 items->mask)->hdr.proto;
			} else {
				/* Reset for inner layer. */
				next_protocol = 0xff;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
						(items, item_flags,
						 next_protocol,
						 &rte_flow_item_tcp_mask,
						 error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					     MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(items, item_flags,
							  next_protocol,
							  error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					     MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			ret = mlx5_flow_validate_item_gre(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(items, item_flags,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_validate_item_vxlan_gpe(items,
								item_flags, dev,
								error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_validate_item_mpls(dev, items,
							   item_flags,
							   last_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			ret = flow_dv_validate_item_meta(dev, items, attr,
							 error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_METADATA;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
		item_flags |= last_item;
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
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			ret = flow_dv_validate_action_l2_encap(action_flags,
							       actions, attr,
							       error);
			if (ret < 0)
				return ret;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP ?
					MLX5_FLOW_ACTION_VXLAN_ENCAP :
					MLX5_FLOW_ACTION_NVGRE_ENCAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			ret = flow_dv_validate_action_l2_decap(action_flags,
							       attr, error);
			if (ret < 0)
				return ret;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_VXLAN_DECAP ?
					MLX5_FLOW_ACTION_VXLAN_DECAP :
					MLX5_FLOW_ACTION_NVGRE_DECAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			ret = flow_dv_validate_action_raw_encap(action_flags,
								actions, attr,
								error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RAW_ENCAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			ret = flow_dv_validate_action_raw_decap(action_flags,
								actions, attr,
								error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RAW_DECAP;
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

#ifndef NDEBUG
/**
 * Sanity check for match mask and value. Similar to check_valid_spec() in
 * kernel driver. If unmasked bit is present in value, it returns failure.
 *
 * @param match_mask
 *   pointer to match mask buffer.
 * @param match_value
 *   pointer to match value buffer.
 *
 * @return
 *   0 if valid, -EINVAL otherwise.
 */
static int
flow_dv_check_valid_spec(void *match_mask, void *match_value)
{
	uint8_t *m = match_mask;
	uint8_t *v = match_value;
	unsigned int i;

	for (i = 0; i < MLX5_ST_SZ_DB(fte_match_param); ++i) {
		if (v[i] & ~m[i]) {
			DRV_LOG(ERR,
				"match_value differs from match_criteria"
				" %p[%u] != %p[%u]",
				match_value, i, match_mask, i);
			return -EINVAL;
		}
	}
	return 0;
}
#endif

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
 * Add MPLS item to matcher and to the value.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] prev_layer
 *   The protocol layer indicated in previous item.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_mpls(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    uint64_t prev_layer,
			    int inner)
{
	const uint32_t *in_mpls_m = item->mask;
	const uint32_t *in_mpls_v = item->spec;
	uint32_t *out_mpls_m = 0;
	uint32_t *out_mpls_v = 0;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	void *misc2_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_2);
	void *misc2_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);
	void *headers_m = MLX5_ADDR_OF(fte_match_param, matcher, outer_headers);
	void *headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);

	switch (prev_layer) {
	case MLX5_FLOW_LAYER_OUTER_L4_UDP:
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport, 0xffff);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
			 MLX5_UDP_PORT_MPLS);
		break;
	case MLX5_FLOW_LAYER_GRE:
		MLX5_SET(fte_match_set_misc, misc_m, gre_protocol, 0xffff);
		MLX5_SET(fte_match_set_misc, misc_v, gre_protocol,
			 ETHER_TYPE_MPLS);
		break;
	default:
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xff);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
			 IPPROTO_MPLS);
		break;
	}
	if (!in_mpls_v)
		return;
	if (!in_mpls_m)
		in_mpls_m = (const uint32_t *)&rte_flow_item_mpls_mask;
	switch (prev_layer) {
	case MLX5_FLOW_LAYER_OUTER_L4_UDP:
		out_mpls_m =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_m,
						 outer_first_mpls_over_udp);
		out_mpls_v =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_v,
						 outer_first_mpls_over_udp);
		break;
	case MLX5_FLOW_LAYER_GRE:
		out_mpls_m =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_m,
						 outer_first_mpls_over_gre);
		out_mpls_v =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_v,
						 outer_first_mpls_over_gre);
		break;
	default:
		/* Inner MPLS not over GRE is not supported. */
		if (!inner) {
			out_mpls_m =
				(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2,
							 misc2_m,
							 outer_first_mpls);
			out_mpls_v =
				(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2,
							 misc2_v,
							 outer_first_mpls);
		}
		break;
	}
	if (out_mpls_m && out_mpls_v) {
		*out_mpls_m = *in_mpls_m;
		*out_mpls_v = *in_mpls_v & *in_mpls_m;
	}
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
	if (!cache_matcher->matcher_object) {
		rte_free(cache_matcher);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create matcher");
	}
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
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	struct rte_flow *flow = dev_flow->flow;
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint64_t action_flags = 0;
	uint64_t priority = attr->priority;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
	};
	int actions_n = 0;

	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = priv->config.flow_prio - 1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		const struct rte_flow_action_queue *queue;
		const struct rte_flow_action_rss *rss;
		const struct rte_flow_action *action = actions;
		const uint8_t *rss_key;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			dev_flow->dv.actions[actions_n].type =
				MLX5DV_FLOW_ACTION_TAG;
			dev_flow->dv.actions[actions_n].tag_value =
				mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT);
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			dev_flow->dv.actions[actions_n].type =
				MLX5DV_FLOW_ACTION_TAG;
			dev_flow->dv.actions[actions_n].tag_value =
				mlx5_flow_mark_set
				(((const struct rte_flow_action_mark *)
				  (actions->conf))->id);
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			dev_flow->dv.actions[actions_n].type =
				MLX5DV_FLOW_ACTION_DROP;
			action_flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue = actions->conf;
			flow->rss.queue_num = 1;
			(*flow->queue)[0] = queue->index;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			if (flow->queue)
				memcpy((*flow->queue), rss->queue,
				       rss->queue_num * sizeof(uint16_t));
			flow->rss.queue_num = rss->queue_num;
			/* NULL RSS key indicates default RSS key. */
			rss_key = !rss->key ? rss_hash_default_key : rss->key;
			memcpy(flow->key, rss_key, MLX5_RSS_HASH_KEY_LEN);
			/* RSS type 0 indicates default RSS type ETH_RSS_IP. */
			flow->rss.types = !rss->types ? ETH_RSS_IP : rss->types;
			flow->rss.level = rss->level;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			if (flow_dv_create_action_l2_encap(dev, actions,
							   dev_flow, error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n].type =
				MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
			dev_flow->dv.actions[actions_n].action =
				dev_flow->dv.encap_decap->verbs_action;
			actions_n++;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP ?
					MLX5_FLOW_ACTION_VXLAN_ENCAP :
					MLX5_FLOW_ACTION_NVGRE_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			if (flow_dv_create_action_l2_decap(dev, dev_flow,
							   error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n].type =
				MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
			dev_flow->dv.actions[actions_n].action =
				dev_flow->dv.encap_decap->verbs_action;
			actions_n++;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_VXLAN_DECAP ?
					MLX5_FLOW_ACTION_VXLAN_DECAP :
					MLX5_FLOW_ACTION_NVGRE_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			/* Handle encap with preceding decap. */
			if (action_flags & MLX5_FLOW_ACTION_RAW_DECAP) {
				if (flow_dv_create_action_raw_encap
					(dev, actions, dev_flow, attr, error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n].type =
					MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
				dev_flow->dv.actions[actions_n].action =
					dev_flow->dv.encap_decap->verbs_action;
			} else {
				/* Handle encap without preceding decap. */
				if (flow_dv_create_action_l2_encap(dev, actions,
								   dev_flow,
								   error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n].type =
					MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
				dev_flow->dv.actions[actions_n].action =
					dev_flow->dv.encap_decap->verbs_action;
			}
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_RAW_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			/* Check if this decap is followed by encap. */
			for (; action->type != RTE_FLOW_ACTION_TYPE_END &&
			       action->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP;
			       action++) {
			}
			/* Handle decap only if it isn't followed by encap. */
			if (action->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
				if (flow_dv_create_action_l2_decap(dev,
								   dev_flow,
								   error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n].type =
					MLX5DV_FLOW_ACTION_IBV_FLOW_ACTION;
				dev_flow->dv.actions[actions_n].action =
					dev_flow->dv.encap_decap->verbs_action;
				actions_n++;
			}
			/* If decap is followed by encap, handle it at encap. */
			action_flags |= MLX5_FLOW_ACTION_RAW_DECAP;
			break;
		default:
			break;
		}
	}
	dev_flow->dv.actions_n = actions_n;
	flow->actions = action_flags;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		void *match_mask = matcher.mask.buf;
		void *match_value = dev_flow->dv.value.buf;

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			flow_dv_translate_item_eth(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L2;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					     MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			flow_dv_translate_item_vlan(match_mask, match_value,
						    items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L2;
			last_item = tunnel ? (MLX5_FLOW_LAYER_INNER_L2 |
					      MLX5_FLOW_LAYER_INNER_VLAN) :
					     (MLX5_FLOW_LAYER_OUTER_L2 |
					      MLX5_FLOW_LAYER_OUTER_VLAN);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			flow_dv_translate_item_ipv4(match_mask, match_value,
						    items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel,
					 MLX5_IPV4_LAYER_TYPES,
					 MLX5_IPV4_IBV_RX_HASH);
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			flow_dv_translate_item_ipv6(match_mask, match_value,
						    items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel,
					 MLX5_IPV6_LAYER_TYPES,
					 MLX5_IPV6_IBV_RX_HASH);
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			flow_dv_translate_item_tcp(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L4;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel, ETH_RSS_TCP,
					 IBV_RX_HASH_SRC_PORT_TCP |
					 IBV_RX_HASH_DST_PORT_TCP);
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					     MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			flow_dv_translate_item_udp(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L4;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel, ETH_RSS_UDP,
					 IBV_RX_HASH_SRC_PORT_UDP |
					 IBV_RX_HASH_DST_PORT_UDP);
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					     MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			flow_dv_translate_item_gre(match_mask, match_value,
						   items, tunnel);
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			flow_dv_translate_item_nvgre(match_mask, match_value,
						     items, tunnel);
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			flow_dv_translate_item_vxlan(match_mask, match_value,
						     items, tunnel);
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			flow_dv_translate_item_vxlan(match_mask, match_value,
						     items, tunnel);
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			flow_dv_translate_item_mpls(match_mask, match_value,
						    items, last_item, tunnel);
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			flow_dv_translate_item_meta(match_mask, match_value,
						    items);
			last_item = MLX5_FLOW_ITEM_METADATA;
			break;
		default:
			break;
		}
		item_flags |= last_item;
	}
	assert(!flow_dv_check_valid_spec(matcher.mask.buf,
					 dev_flow->dv.value.buf));
	dev_flow->layers = item_flags;
	/* Register matcher. */
	matcher.crc = rte_raw_cksum((const void *)matcher.mask.buf,
				    matcher.mask.size);
	matcher.priority = mlx5_flow_adjust_priority(dev, priority,
						     matcher.priority);
	matcher.egress = attr->egress;
	if (flow_dv_matcher_register(dev, &matcher, dev_flow, error))
		return -rte_errno;
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
 * Release an encap/decap resource.
 *
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_encap_decap_resource_release(struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_encap_decap_resource *cache_resource =
						flow->dv.encap_decap;

	assert(cache_resource->verbs_action);
	DRV_LOG(DEBUG, "encap/decap resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->verbs_action));
		LIST_REMOVE(cache_resource, next);
		rte_free(cache_resource);
		DRV_LOG(DEBUG, "encap/decap resource %p: removed",
			(void *)cache_resource);
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
		if (dev_flow->dv.encap_decap)
			flow_dv_encap_decap_resource_release(dev_flow);
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
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "flow query with DV is not supported");
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
