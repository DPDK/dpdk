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
#include <rte_ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_gre.h>

#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_glue.h"
#include "mlx5_flow.h"
#include "mlx5_prm.h"
#include "mlx5_rxtx.h"

#ifdef HAVE_IBV_FLOW_DV_SUPPORT

#ifndef HAVE_IBV_FLOW_DEVX_COUNTERS
#define MLX5DV_FLOW_ACTION_COUNTERS_DEVX 0
#endif

#ifndef HAVE_MLX5DV_DR_ESWITCH
#ifndef MLX5DV_FLOW_TABLE_TYPE_FDB
#define MLX5DV_FLOW_TABLE_TYPE_FDB 0
#endif
#endif

#ifndef HAVE_MLX5DV_DR
#define MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL 1
#endif

union flow_dv_attr {
	struct {
		uint32_t valid:1;
		uint32_t ipv4:1;
		uint32_t ipv6:1;
		uint32_t tcp:1;
		uint32_t udp:1;
		uint32_t reserved:27;
	};
	uint32_t attr;
};

/**
 * Initialize flow attributes structure according to flow items' types.
 *
 * @param[in] item
 *   Pointer to item specification.
 * @param[out] attr
 *   Pointer to flow attributes structure.
 */
static void
flow_dv_attr_init(const struct rte_flow_item *item, union flow_dv_attr *attr)
{
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			attr->ipv4 = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			attr->ipv6 = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			attr->udp = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			attr->tcp = 1;
			break;
		default:
			break;
		}
	}
	attr->valid = 1;
}

struct field_modify_info {
	uint32_t size; /* Size of field in protocol header, in bytes. */
	uint32_t offset; /* Offset of field in protocol header, in bytes. */
	enum mlx5_modification_field id;
};

struct field_modify_info modify_eth[] = {
	{4,  0, MLX5_MODI_OUT_DMAC_47_16},
	{2,  4, MLX5_MODI_OUT_DMAC_15_0},
	{4,  6, MLX5_MODI_OUT_SMAC_47_16},
	{2, 10, MLX5_MODI_OUT_SMAC_15_0},
	{0, 0, 0},
};

struct field_modify_info modify_ipv4[] = {
	{1,  8, MLX5_MODI_OUT_IPV4_TTL},
	{4, 12, MLX5_MODI_OUT_SIPV4},
	{4, 16, MLX5_MODI_OUT_DIPV4},
	{0, 0, 0},
};

struct field_modify_info modify_ipv6[] = {
	{1,  7, MLX5_MODI_OUT_IPV6_HOPLIMIT},
	{4,  8, MLX5_MODI_OUT_SIPV6_127_96},
	{4, 12, MLX5_MODI_OUT_SIPV6_95_64},
	{4, 16, MLX5_MODI_OUT_SIPV6_63_32},
	{4, 20, MLX5_MODI_OUT_SIPV6_31_0},
	{4, 24, MLX5_MODI_OUT_DIPV6_127_96},
	{4, 28, MLX5_MODI_OUT_DIPV6_95_64},
	{4, 32, MLX5_MODI_OUT_DIPV6_63_32},
	{4, 36, MLX5_MODI_OUT_DIPV6_31_0},
	{0, 0, 0},
};

struct field_modify_info modify_udp[] = {
	{2, 0, MLX5_MODI_OUT_UDP_SPORT},
	{2, 2, MLX5_MODI_OUT_UDP_DPORT},
	{0, 0, 0},
};

struct field_modify_info modify_tcp[] = {
	{2, 0, MLX5_MODI_OUT_TCP_SPORT},
	{2, 2, MLX5_MODI_OUT_TCP_DPORT},
	{0, 0, 0},
};

/**
 * Acquire the synchronizing object to protect multithreaded access
 * to shared dv context. Lock occurs only if context is actually
 * shared, i.e. we have multiport IB device and representors are
 * created.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 */
static void
flow_d_shared_lock(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	if (sh->dv_refcnt > 1) {
		int ret;

		ret = pthread_mutex_lock(&sh->dv_mutex);
		assert(!ret);
		(void)ret;
	}
}

static void
flow_d_shared_unlock(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	if (sh->dv_refcnt > 1) {
		int ret;

		ret = pthread_mutex_unlock(&sh->dv_mutex);
		assert(!ret);
		(void)ret;
	}
}

/**
 * Convert modify-header action to DV specification.
 *
 * @param[in] item
 *   Pointer to item specification.
 * @param[in] field
 *   Pointer to field modification information.
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] type
 *   Type of modification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_modify_action(struct rte_flow_item *item,
			      struct field_modify_info *field,
			      struct mlx5_flow_dv_modify_hdr_resource *resource,
			      uint32_t type,
			      struct rte_flow_error *error)
{
	uint32_t i = resource->actions_num;
	struct mlx5_modification_cmd *actions = resource->actions;
	const uint8_t *spec = item->spec;
	const uint8_t *mask = item->mask;
	uint32_t set;

	while (field->size) {
		set = 0;
		/* Generate modify command for each mask segment. */
		memcpy(&set, &mask[field->offset], field->size);
		if (set) {
			if (i >= MLX5_MODIFY_NUM)
				return rte_flow_error_set(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					 "too many items to modify");
			actions[i].action_type = type;
			actions[i].field = field->id;
			actions[i].length = field->size ==
					4 ? 0 : field->size * 8;
			rte_memcpy(&actions[i].data[4 - field->size],
				   &spec[field->offset], field->size);
			actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
			++i;
		}
		if (resource->actions_num != i)
			resource->actions_num = i;
		field++;
	}
	if (!resource->actions_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "invalid modification flow item");
	return 0;
}

/**
 * Convert modify-header set IPv4 address action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_ipv4
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_ipv4 *conf =
		(const struct rte_flow_action_set_ipv4 *)(action->conf);
	struct rte_flow_item item = { .type = RTE_FLOW_ITEM_TYPE_IPV4 };
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv4 ipv4_mask;

	memset(&ipv4, 0, sizeof(ipv4));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	if (action->type == RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC) {
		ipv4.hdr.src_addr = conf->ipv4_addr;
		ipv4_mask.hdr.src_addr = rte_flow_item_ipv4_mask.hdr.src_addr;
	} else {
		ipv4.hdr.dst_addr = conf->ipv4_addr;
		ipv4_mask.hdr.dst_addr = rte_flow_item_ipv4_mask.hdr.dst_addr;
	}
	item.spec = &ipv4;
	item.mask = &ipv4_mask;
	return flow_dv_convert_modify_action(&item, modify_ipv4, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set IPv6 address action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_ipv6
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_ipv6 *conf =
		(const struct rte_flow_action_set_ipv6 *)(action->conf);
	struct rte_flow_item item = { .type = RTE_FLOW_ITEM_TYPE_IPV6 };
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_ipv6 ipv6_mask;

	memset(&ipv6, 0, sizeof(ipv6));
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	if (action->type == RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC) {
		memcpy(&ipv6.hdr.src_addr, &conf->ipv6_addr,
		       sizeof(ipv6.hdr.src_addr));
		memcpy(&ipv6_mask.hdr.src_addr,
		       &rte_flow_item_ipv6_mask.hdr.src_addr,
		       sizeof(ipv6.hdr.src_addr));
	} else {
		memcpy(&ipv6.hdr.dst_addr, &conf->ipv6_addr,
		       sizeof(ipv6.hdr.dst_addr));
		memcpy(&ipv6_mask.hdr.dst_addr,
		       &rte_flow_item_ipv6_mask.hdr.dst_addr,
		       sizeof(ipv6.hdr.dst_addr));
	}
	item.spec = &ipv6;
	item.mask = &ipv6_mask;
	return flow_dv_convert_modify_action(&item, modify_ipv6, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set MAC address action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_mac
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_mac *conf =
		(const struct rte_flow_action_set_mac *)(action->conf);
	struct rte_flow_item item = { .type = RTE_FLOW_ITEM_TYPE_ETH };
	struct rte_flow_item_eth eth;
	struct rte_flow_item_eth eth_mask;

	memset(&eth, 0, sizeof(eth));
	memset(&eth_mask, 0, sizeof(eth_mask));
	if (action->type == RTE_FLOW_ACTION_TYPE_SET_MAC_SRC) {
		memcpy(&eth.src.addr_bytes, &conf->mac_addr,
		       sizeof(eth.src.addr_bytes));
		memcpy(&eth_mask.src.addr_bytes,
		       &rte_flow_item_eth_mask.src.addr_bytes,
		       sizeof(eth_mask.src.addr_bytes));
	} else {
		memcpy(&eth.dst.addr_bytes, &conf->mac_addr,
		       sizeof(eth.dst.addr_bytes));
		memcpy(&eth_mask.dst.addr_bytes,
		       &rte_flow_item_eth_mask.dst.addr_bytes,
		       sizeof(eth_mask.dst.addr_bytes));
	}
	item.spec = &eth;
	item.mask = &eth_mask;
	return flow_dv_convert_modify_action(&item, modify_eth, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set TP action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[in] items
 *   Pointer to rte_flow_item objects list.
 * @param[in] attr
 *   Pointer to flow attributes structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_tp
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 const struct rte_flow_item *items,
			 union flow_dv_attr *attr,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_tp *conf =
		(const struct rte_flow_action_set_tp *)(action->conf);
	struct rte_flow_item item;
	struct rte_flow_item_udp udp;
	struct rte_flow_item_udp udp_mask;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_tcp tcp_mask;
	struct field_modify_info *field;

	if (!attr->valid)
		flow_dv_attr_init(items, attr);
	if (attr->udp) {
		memset(&udp, 0, sizeof(udp));
		memset(&udp_mask, 0, sizeof(udp_mask));
		if (action->type == RTE_FLOW_ACTION_TYPE_SET_TP_SRC) {
			udp.hdr.src_port = conf->port;
			udp_mask.hdr.src_port =
					rte_flow_item_udp_mask.hdr.src_port;
		} else {
			udp.hdr.dst_port = conf->port;
			udp_mask.hdr.dst_port =
					rte_flow_item_udp_mask.hdr.dst_port;
		}
		item.type = RTE_FLOW_ITEM_TYPE_UDP;
		item.spec = &udp;
		item.mask = &udp_mask;
		field = modify_udp;
	}
	if (attr->tcp) {
		memset(&tcp, 0, sizeof(tcp));
		memset(&tcp_mask, 0, sizeof(tcp_mask));
		if (action->type == RTE_FLOW_ACTION_TYPE_SET_TP_SRC) {
			tcp.hdr.src_port = conf->port;
			tcp_mask.hdr.src_port =
					rte_flow_item_tcp_mask.hdr.src_port;
		} else {
			tcp.hdr.dst_port = conf->port;
			tcp_mask.hdr.dst_port =
					rte_flow_item_tcp_mask.hdr.dst_port;
		}
		item.type = RTE_FLOW_ITEM_TYPE_TCP;
		item.spec = &tcp;
		item.mask = &tcp_mask;
		field = modify_tcp;
	}
	return flow_dv_convert_modify_action(&item, field, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set TTL action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[in] items
 *   Pointer to rte_flow_item objects list.
 * @param[in] attr
 *   Pointer to flow attributes structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_ttl
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 const struct rte_flow_item *items,
			 union flow_dv_attr *attr,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_ttl *conf =
		(const struct rte_flow_action_set_ttl *)(action->conf);
	struct rte_flow_item item;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv4 ipv4_mask;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_ipv6 ipv6_mask;
	struct field_modify_info *field;

	if (!attr->valid)
		flow_dv_attr_init(items, attr);
	if (attr->ipv4) {
		memset(&ipv4, 0, sizeof(ipv4));
		memset(&ipv4_mask, 0, sizeof(ipv4_mask));
		ipv4.hdr.time_to_live = conf->ttl_value;
		ipv4_mask.hdr.time_to_live = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV4;
		item.spec = &ipv4;
		item.mask = &ipv4_mask;
		field = modify_ipv4;
	}
	if (attr->ipv6) {
		memset(&ipv6, 0, sizeof(ipv6));
		memset(&ipv6_mask, 0, sizeof(ipv6_mask));
		ipv6.hdr.hop_limits = conf->ttl_value;
		ipv6_mask.hdr.hop_limits = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV6;
		item.spec = &ipv6;
		item.mask = &ipv6_mask;
		field = modify_ipv6;
	}
	return flow_dv_convert_modify_action(&item, field, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header decrement TTL action to DV specification.
 *
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[in] items
 *   Pointer to rte_flow_item objects list.
 * @param[in] attr
 *   Pointer to flow attributes structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_dec_ttl
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_item *items,
			 union flow_dv_attr *attr,
			 struct rte_flow_error *error)
{
	struct rte_flow_item item;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv4 ipv4_mask;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_ipv6 ipv6_mask;
	struct field_modify_info *field;

	if (!attr->valid)
		flow_dv_attr_init(items, attr);
	if (attr->ipv4) {
		memset(&ipv4, 0, sizeof(ipv4));
		memset(&ipv4_mask, 0, sizeof(ipv4_mask));
		ipv4.hdr.time_to_live = 0xFF;
		ipv4_mask.hdr.time_to_live = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV4;
		item.spec = &ipv4;
		item.mask = &ipv4_mask;
		field = modify_ipv4;
	}
	if (attr->ipv6) {
		memset(&ipv6, 0, sizeof(ipv6));
		memset(&ipv6_mask, 0, sizeof(ipv6_mask));
		ipv6.hdr.hop_limits = 0xFF;
		ipv6_mask.hdr.hop_limits = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV6;
		item.spec = &ipv6;
		item.mask = &ipv6_mask;
		field = modify_ipv6;
	}
	return flow_dv_convert_modify_action(&item, field, resource,
					     MLX5_MODIFICATION_TYPE_ADD, error);
}

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
 * Validate vport item.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   Item specification.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_port_id(struct rte_eth_dev *dev,
			      const struct rte_flow_item *item,
			      const struct rte_flow_attr *attr,
			      uint64_t item_flags,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_port_id *spec = item->spec;
	const struct rte_flow_item_port_id *mask = item->mask;
	const struct rte_flow_item_port_id switch_mask = {
			.id = 0xffffffff,
	};
	uint16_t esw_domain_id;
	uint16_t item_port_esw_domain_id;
	int ret;

	if (!attr->transfer)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL,
					  "match on port id is valid only"
					  " when transfer flag is enabled");
	if (item_flags & MLX5_FLOW_ITEM_PORT_ID)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple source ports are not"
					  " supported");
	if (!mask)
		mask = &switch_mask;
	if (mask->id != 0xffffffff)
		return rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					   mask,
					   "no support for partial mask on"
					   " \"id\" field");
	ret = mlx5_flow_item_acceptable
				(item, (const uint8_t *)mask,
				 (const uint8_t *)&rte_flow_item_port_id_mask,
				 sizeof(struct rte_flow_item_port_id),
				 error);
	if (ret)
		return ret;
	if (!spec)
		return 0;
	ret = mlx5_port_to_eswitch_info(spec->id, &item_port_esw_domain_id,
					NULL);
	if (ret)
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "failed to obtain E-Switch info for"
					  " port");
	ret = mlx5_port_to_eswitch_info(dev->data->port_id,
					&esw_domain_id, NULL);
	if (ret < 0)
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to obtain E-Switch info");
	if (item_port_esw_domain_id != esw_domain_id)
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "cannot match on a port from a"
					  " different E-Switch");
	return 0;
}

/**
 * Validate count action.
 *
 * @param[in] dev
 *   device otr.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_count(struct rte_eth_dev *dev,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->config.devx)
		goto notsup_err;
#ifdef HAVE_IBV_FLOW_DEVX_COUNTERS
	return 0;
#endif
notsup_err:
	return rte_flow_error_set
		      (error, ENOTSUP,
		       RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
		       NULL,
		       "count action not supported");
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
	if (!attr->transfer && attr->ingress)
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
	if (action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have decap action after"
					  " modify action");
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
	if (!attr->transfer &&  attr->ingress &&
	    !(action_flags & MLX5_FLOW_ACTION_RAW_DECAP))
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
	if (action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have decap action after"
					  " modify action");
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_encap_decap_resource *cache_resource;
	struct rte_flow *flow = dev_flow->flow;
	struct mlx5dv_dr_domain *domain;

	resource->flags = flow->group ? 0 : 1;
	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->encaps_decaps, next) {
		if (resource->reformat_type == cache_resource->reformat_type &&
		    resource->ft_type == cache_resource->ft_type &&
		    resource->flags == cache_resource->flags &&
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
			(sh->ctx, cache_resource->reformat_type,
			 cache_resource->ft_type, domain, cache_resource->flags,
			 cache_resource->size,
			 (cache_resource->size ? cache_resource->buf : NULL));
	if (!cache_resource->verbs_action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->encaps_decaps, cache_resource, next);
	dev_flow->dv.encap_decap = cache_resource;
	DRV_LOG(DEBUG, "new encap/decap resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Find existing table jump resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] resource
 *   Pointer to jump table resource.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_jump_tbl_resource_register
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_jump_tbl_resource *resource,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_jump_tbl_resource *cache_resource;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->jump_tbl, next) {
		if (resource->tbl == cache_resource->tbl) {
			DRV_LOG(DEBUG, "jump table resource resource %p: refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->dv.jump = cache_resource;
			return 0;
		}
	}
	/* Register new jump table resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	cache_resource->action =
		mlx5_glue->dr_create_flow_action_dest_flow_tbl
		(resource->tbl->obj);
	if (!cache_resource->action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->jump_tbl, cache_resource, next);
	dev_flow->dv.jump = cache_resource;
	DRV_LOG(DEBUG, "new jump table  resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Find existing table port ID resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] resource
 *   Pointer to port ID action resource.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_port_id_action_resource_register
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_port_id_action_resource *resource,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_port_id_action_resource *cache_resource;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->port_id_action_list, next) {
		if (resource->port_id == cache_resource->port_id) {
			DRV_LOG(DEBUG, "port id action resource resource %p: "
				"refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->dv.port_id_action = cache_resource;
			return 0;
		}
	}
	/* Register new port id action resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	cache_resource->action =
		mlx5_glue->dr_create_flow_action_dest_vport
			(priv->sh->fdb_domain, resource->port_id);
	if (!cache_resource->action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->port_id_action_list, cache_resource, next);
	dev_flow->dv.port_id_action = cache_resource;
	DRV_LOG(DEBUG, "new port id action resource %p: refcnt %d++",
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
 * @param[in] transfer
 *   Mark if the flow is E-Switch flow.
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
			       uint8_t transfer,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item *encap_data;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	struct mlx5_flow_dv_encap_decap_resource res = {
		.reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL,
		.ft_type = transfer ? MLX5DV_FLOW_TABLE_TYPE_FDB :
				      MLX5DV_FLOW_TABLE_TYPE_NIC_TX,
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
 * @param[in] transfer
 *   Mark if the flow is E-Switch flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_create_action_l2_decap(struct rte_eth_dev *dev,
			       struct mlx5_flow *dev_flow,
			       uint8_t transfer,
			       struct rte_flow_error *error)
{
	struct mlx5_flow_dv_encap_decap_resource res = {
		.size = 0,
		.reformat_type =
			MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2,
		.ft_type = transfer ? MLX5DV_FLOW_TABLE_TYPE_FDB :
				      MLX5DV_FLOW_TABLE_TYPE_NIC_RX,
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
	if (attr->transfer)
		res.ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	else
		res.ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					     MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	if (flow_dv_encap_decap_resource_register(dev, &res, dev_flow, error))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "can't create encap action");
	return 0;
}

/**
 * Validate the modify-header actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_hdr(const uint64_t action_flags,
				   const struct rte_flow_action *action,
				   struct rte_flow_error *error)
{
	if (action->type != RTE_FLOW_ACTION_TYPE_DEC_TTL && !action->conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "action configuration not set");
	if (action_flags & MLX5_FLOW_ENCAP_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have encap action before"
					  " modify action");
	return 0;
}

/**
 * Validate the modify-header MAC address actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_mac(const uint64_t action_flags,
				   const struct rte_flow_action *action,
				   const uint64_t item_flags,
				   struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_L2))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no L2 item in pattern");
	}
	return ret;
}

/**
 * Validate the modify-header IPv4 address actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_ipv4(const uint64_t action_flags,
				    const struct rte_flow_action *action,
				    const uint64_t item_flags,
				    struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_L3_IPV4))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no ipv4 item in pattern");
	}
	return ret;
}

/**
 * Validate the modify-header IPv6 address actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_ipv6(const uint64_t action_flags,
				    const struct rte_flow_action *action,
				    const uint64_t item_flags,
				    struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_L3_IPV6))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no ipv6 item in pattern");
	}
	return ret;
}

/**
 * Validate the modify-header TP actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_tp(const uint64_t action_flags,
				  const struct rte_flow_action *action,
				  const uint64_t item_flags,
				  struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_L4))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no transport layer "
						  "in pattern");
	}
	return ret;
}

/**
 * Validate the modify-header TTL actions.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_ttl(const uint64_t action_flags,
				   const struct rte_flow_action *action,
				   const uint64_t item_flags,
				   struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_L3))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "no IP protocol in pattern");
	}
	return ret;
}

/**
 * Validate jump action.
 *
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] group
 *   The group of the current flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_jump(const struct rte_flow_action *action,
			     uint32_t group,
			     struct rte_flow_error *error)
{
	if (action->type != RTE_FLOW_ACTION_TYPE_JUMP && !action->conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "action configuration not set");
	if (group >= ((const struct rte_flow_action_jump *)action->conf)->group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "target group must be higher then"
					  " the current flow group");
	return 0;
}

/*
 * Validate the port_id action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] action
 *   Port_id RTE action structure.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_port_id(struct rte_eth_dev *dev,
				uint64_t action_flags,
				const struct rte_flow_action *action,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	const struct rte_flow_action_port_id *port_id;
	uint16_t port;
	uint16_t esw_domain_id;
	uint16_t act_port_domain_id;
	int ret;

	if (!attr->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "port id action is valid in transfer"
					  " mode only");
	if (!action || !action->conf)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "port id action parameters must be"
					  " specified");
	if (action_flags & (MLX5_FLOW_FATE_ACTIONS |
			    MLX5_FLOW_FATE_ESWITCH_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can have only one fate actions in"
					  " a flow");
	ret = mlx5_port_to_eswitch_info(dev->data->port_id,
					&esw_domain_id, NULL);
	if (ret < 0)
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to obtain E-Switch info");
	port_id = action->conf;
	port = port_id->original ? dev->data->port_id : port_id->id;
	ret = mlx5_port_to_eswitch_info(port, &act_port_domain_id, NULL);
	if (ret)
		return rte_flow_error_set
				(error, -ret,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF, port_id,
				 "failed to obtain E-Switch port id for port");
	if (act_port_domain_id != esw_domain_id)
		return rte_flow_error_set
				(error, -ret,
				 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				 "port does not belong to"
				 " E-Switch being configured");
	return 0;
}

/**
 * Find existing modify-header resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] resource
 *   Pointer to modify-header resource.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_modify_hdr_resource_register
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_modify_hdr_resource *resource,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_modify_hdr_resource *cache_resource;
	struct mlx5dv_dr_domain *ns;

	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		ns = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_TX)
		ns = sh->tx_domain;
	else
		ns = sh->rx_domain;
	resource->flags =
		dev_flow->flow->group ? 0 : MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL;
	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->modify_cmds, next) {
		if (resource->ft_type == cache_resource->ft_type &&
		    resource->actions_num == cache_resource->actions_num &&
		    resource->flags == cache_resource->flags &&
		    !memcmp((const void *)resource->actions,
			    (const void *)cache_resource->actions,
			    (resource->actions_num *
					    sizeof(resource->actions[0])))) {
			DRV_LOG(DEBUG, "modify-header resource %p: refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->dv.modify_hdr = cache_resource;
			return 0;
		}
	}
	/* Register new modify-header resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	cache_resource->verbs_action =
		mlx5_glue->dv_create_flow_action_modify_header
					(sh->ctx, cache_resource->ft_type,
					 ns, cache_resource->flags,
					 cache_resource->actions_num *
					 sizeof(cache_resource->actions[0]),
					 (uint64_t *)cache_resource->actions);
	if (!cache_resource->verbs_action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->modify_cmds, cache_resource, next);
	dev_flow->dv.modify_hdr = cache_resource;
	DRV_LOG(DEBUG, "new modify-header resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Get or create a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] shared
 *   Indicate if this counter is shared with other flows.
 * @param[in] id
 *   Counter identifier.
 *
 * @return
 *   pointer to flow counter on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter *
flow_dv_counter_new(struct rte_eth_dev *dev, uint32_t shared, uint32_t id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt = NULL;
	struct mlx5_devx_counter_set *dcs = NULL;
	int ret;

	if (!priv->config.devx) {
		ret = -ENOTSUP;
		goto error_exit;
	}
	if (shared) {
		LIST_FOREACH(cnt, &priv->flow_counters, next) {
			if (cnt->shared && cnt->id == id) {
				cnt->ref_cnt++;
				return cnt;
			}
		}
	}
	cnt = rte_calloc(__func__, 1, sizeof(*cnt), 0);
	dcs = rte_calloc(__func__, 1, sizeof(*dcs), 0);
	if (!dcs || !cnt) {
		ret = -ENOMEM;
		goto error_exit;
	}
	ret = mlx5_devx_cmd_flow_counter_alloc(priv->sh->ctx, dcs);
	if (ret)
		goto error_exit;
	struct mlx5_flow_counter tmpl = {
		.shared = shared,
		.ref_cnt = 1,
		.id = id,
		.dcs = dcs,
	};
	tmpl.action = mlx5_glue->dv_create_flow_action_counter(dcs->obj, 0);
	if (!tmpl.action) {
		ret = errno;
		goto error_exit;
	}
	*cnt = tmpl;
	LIST_INSERT_HEAD(&priv->flow_counters, cnt, next);
	return cnt;
error_exit:
	rte_free(cnt);
	rte_free(dcs);
	rte_errno = -ret;
	return NULL;
}

/**
 * Release a flow counter.
 *
 * @param[in] counter
 *   Pointer to the counter handler.
 */
static void
flow_dv_counter_release(struct mlx5_flow_counter *counter)
{
	int ret;

	if (!counter)
		return;
	if (--counter->ref_cnt == 0) {
		ret = mlx5_devx_cmd_flow_counter_free(counter->dcs->obj);
		if (ret)
			DRV_LOG(ERR, "Failed to free devx counters, %d", ret);
		LIST_REMOVE(counter, next);
		rte_free(counter->dcs);
		rte_free(counter);
	}
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
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->config.flow_prio - 1;

#ifndef HAVE_MLX5DV_DR
	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups is not supported");
#endif
	if (attributes->priority != MLX5_FLOW_PRIO_RSVD &&
	    attributes->priority >= priority_max)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL,
					  "priority out of range");
	if (attributes->transfer) {
		if (!priv->config.dv_esw_en)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				 "E-Switch dr is not supported");
		if (!(priv->representor || priv->master))
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL, "E-Switch configurationd can only be"
				 " done by a master or a representor device");
		if (attributes->egress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attributes,
				 "egress is not supported");
		if (attributes->group >= MLX5_MAX_TABLES_FDB)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
				 NULL, "group must be smaller than "
				 RTE_STR(MLX5_MAX_FDB_TABLES));
	}
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
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
	uint8_t next_protocol = 0xff;
	int actions_n = 0;

	if (items == NULL)
		return -1;
	ret = flow_dv_validate_attributes(dev, attr, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			ret = flow_dv_validate_item_port_id
					(dev, items, attr, item_flags, error);
			if (ret < 0)
				return ret;
			last_item |= MLX5_FLOW_ITEM_PORT_ID;
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
							   NULL, error);
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
							   NULL, error);
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
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			ret = flow_dv_validate_action_port_id(dev,
							      action_flags,
							      actions,
							      attr,
							      error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			++actions_n;
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
							    attr, item_flags,
							    error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_dv_validate_action_count(dev, error);
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
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			ret = flow_dv_validate_action_modify_mac(action_flags,
								 actions,
								 item_flags,
								 error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_MAC_SRC ?
						MLX5_FLOW_ACTION_SET_MAC_SRC :
						MLX5_FLOW_ACTION_SET_MAC_DST;
			break;

		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			ret = flow_dv_validate_action_modify_ipv4(action_flags,
								  actions,
								  item_flags,
								  error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC ?
						MLX5_FLOW_ACTION_SET_IPV4_SRC :
						MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			ret = flow_dv_validate_action_modify_ipv6(action_flags,
								  actions,
								  item_flags,
								  error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
						MLX5_FLOW_ACTION_SET_IPV6_SRC :
						MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			ret = flow_dv_validate_action_modify_tp(action_flags,
								actions,
								item_flags,
								error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_TP_SRC ?
						MLX5_FLOW_ACTION_SET_TP_SRC :
						MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			ret = flow_dv_validate_action_modify_ttl(action_flags,
								 actions,
								 item_flags,
								 error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_TTL ?
						MLX5_FLOW_ACTION_SET_TTL :
						MLX5_FLOW_ACTION_DEC_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			ret = flow_dv_validate_action_jump(actions,
							   attr->group, error);
			if (ret)
				return ret;
			++actions_n;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	/* Eswitch has few restrictions on using items and actions */
	if (attr->transfer) {
		if (action_flags & MLX5_FLOW_ACTION_FLAG)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action FLAG");
		if (action_flags & MLX5_FLOW_ACTION_MARK)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action MARK");
		if (action_flags & MLX5_FLOW_ACTION_QUEUE)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action QUEUE");
		if (action_flags & MLX5_FLOW_ACTION_RSS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action RSS");
		if (!(action_flags & MLX5_FLOW_FATE_ESWITCH_ACTIONS))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no fate action is found");
	} else {
		if (!(action_flags & MLX5_FLOW_FATE_ACTIONS) && attr->ingress)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "no fate action is found");
	}
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
 *   otherwise NULL and rte_errno is set.
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
 * @param[in] group
 *   The group to insert the rule.
 */
static void
flow_dv_translate_item_ipv4(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner, uint32_t group)
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
	if (group == 0)
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0xf);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0x4);
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
 * @param[in] group
 *   The group to insert the rule.
 */
static void
flow_dv_translate_item_ipv6(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner, uint32_t group)
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
	if (group == 0)
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0xf);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0x6);
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

/**
 * Add source vport match to the specified matcher.
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] port
 *   Source vport value to match
 * @param[in] mask
 *   Mask
 */
static void
flow_dv_translate_item_source_vport(void *matcher, void *key,
				    int16_t port, uint16_t mask)
{
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);

	MLX5_SET(fte_match_set_misc, misc_m, source_port, mask);
	MLX5_SET(fte_match_set_misc, misc_v, source_port, port);
}

/**
 * Translate port-id item to eswitch match on  port-id.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_dv_translate_item_port_id(struct rte_eth_dev *dev, void *matcher,
			       void *key, const struct rte_flow_item *item)
{
	const struct rte_flow_item_port_id *pid_m = item ? item->mask : NULL;
	const struct rte_flow_item_port_id *pid_v = item ? item->spec : NULL;
	uint16_t mask, val, id;
	int ret;

	mask = pid_m ? pid_m->id : 0xffff;
	id = pid_v ? pid_v->id : dev->data->port_id;
	ret = mlx5_port_to_eswitch_info(id, NULL, &val);
	if (ret)
		return ret;
	flow_dv_translate_item_source_vport(matcher, key, val, mask);
	return 0;
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
#ifdef HAVE_MLX5DV_DR
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_3)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC3_BIT;
#endif
	return match_criteria_enable;
}


/**
 * Get a flow table.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in] table_id
 *   Table id to use.
 * @param[in] egress
 *   Direction of the table.
 * @param[in] transfer
 *   E-Switch or NIC flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   Returns tables resource based on the index, NULL in case of failed.
 */
static struct mlx5_flow_tbl_resource *
flow_dv_tbl_resource_get(struct rte_eth_dev *dev,
			 uint32_t table_id, uint8_t egress,
			 uint8_t transfer,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_tbl_resource *tbl;

#ifdef HAVE_MLX5DV_DR
	if (transfer) {
		tbl = &sh->fdb_tbl[table_id];
		if (!tbl->obj)
			tbl->obj = mlx5_glue->dr_create_flow_tbl
				(sh->fdb_domain, table_id);
	} else if (egress) {
		tbl = &sh->tx_tbl[table_id];
		if (!tbl->obj)
			tbl->obj = mlx5_glue->dr_create_flow_tbl
				(sh->tx_domain, table_id);
	} else {
		tbl = &sh->rx_tbl[table_id];
		if (!tbl->obj)
			tbl->obj = mlx5_glue->dr_create_flow_tbl
				(sh->rx_domain, table_id);
	}
	if (!tbl->obj) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create table");
		return NULL;
	}
	rte_atomic32_inc(&tbl->refcnt);
	return tbl;
#else
	(void)error;
	(void)tbl;
	if (transfer)
		return &sh->fdb_tbl[table_id];
	else if (egress)
		return &sh->tx_tbl[table_id];
	else
		return &sh->rx_tbl[table_id];
#endif
}

/**
 * Release a flow table.
 *
 * @param[in] tbl
 *   Table resource to be released.
 *
 * @return
 *   Returns 0 if table was released, else return 1;
 */
static int
flow_dv_tbl_resource_release(struct mlx5_flow_tbl_resource *tbl)
{
	if (!tbl)
		return 0;
	if (rte_atomic32_dec_and_test(&tbl->refcnt)) {
		mlx5_glue->dr_destroy_flow_tbl(tbl->obj);
		tbl->obj = NULL;
		return 0;
	}
	return 1;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_matcher *cache_matcher;
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.match_mask = (void *)&matcher->mask,
	};
	struct mlx5_flow_tbl_resource *tbl = NULL;

	/* Lookup from cache. */
	LIST_FOREACH(cache_matcher, &sh->matchers, next) {
		if (matcher->crc == cache_matcher->crc &&
		    matcher->priority == cache_matcher->priority &&
		    matcher->egress == cache_matcher->egress &&
		    matcher->group == cache_matcher->group &&
		    matcher->transfer == cache_matcher->transfer &&
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
	tbl = flow_dv_tbl_resource_get(dev, matcher->group * MLX5_GROUP_FACTOR,
				       matcher->egress, matcher->transfer,
				       error);
	if (!tbl) {
		rte_free(cache_matcher);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create table");
	}
	*cache_matcher = *matcher;
	dv_attr.match_criteria_enable =
		flow_dv_matcher_enable(cache_matcher->mask.buf);
	dv_attr.priority = matcher->priority;
	if (matcher->egress)
		dv_attr.flags |= IBV_FLOW_ATTR_FLAGS_EGRESS;
	cache_matcher->matcher_object =
		mlx5_glue->dv_create_flow_matcher(sh->ctx, &dv_attr, tbl->obj);
	if (!cache_matcher->matcher_object) {
		rte_free(cache_matcher);
#ifdef HAVE_MLX5DV_DR
		flow_dv_tbl_resource_release(tbl);
#endif
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create matcher");
	}
	rte_atomic32_inc(&cache_matcher->refcnt);
	LIST_INSERT_HEAD(&sh->matchers, cache_matcher, next);
	dev_flow->dv.matcher = cache_matcher;
	DRV_LOG(DEBUG, "priority %hd new %s matcher %p: refcnt %d",
		cache_matcher->priority,
		cache_matcher->egress ? "tx" : "rx", (void *)cache_matcher,
		rte_atomic32_read(&cache_matcher->refcnt));
	rte_atomic32_inc(&tbl->refcnt);
	return 0;
}

/**
 * Find existing tag resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] resource
 *   Pointer to tag resource.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_tag_resource_register
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_tag_resource *resource,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_tag_resource *cache_resource;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->tags, next) {
		if (resource->tag == cache_resource->tag) {
			DRV_LOG(DEBUG, "tag resource %p: refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->flow->tag_resource = cache_resource;
			return 0;
		}
	}
	/* Register new  resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	cache_resource->action = mlx5_glue->dv_create_flow_action_tag
		(resource->tag);
	if (!cache_resource->action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->tags, cache_resource, next);
	dev_flow->flow->tag_resource = cache_resource;
	DRV_LOG(DEBUG, "new tag resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Release the tag.
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
flow_dv_tag_release(struct rte_eth_dev *dev,
		    struct mlx5_flow_dv_tag_resource *tag)
{
	assert(tag);
	DRV_LOG(DEBUG, "port %u tag %p: refcnt %d--",
		dev->data->port_id, (void *)tag,
		rte_atomic32_read(&tag->refcnt));
	if (rte_atomic32_dec_and_test(&tag->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action(tag->action));
		LIST_REMOVE(tag, next);
		DRV_LOG(DEBUG, "port %u tag %p: removed",
			dev->data->port_id, (void *)tag);
		rte_free(tag);
		return 0;
	}
	return 1;
}

/**
 * Translate port ID action to vport.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the port ID action.
 * @param[out] dst_port_id
 *   The target port ID.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_translate_action_port_id(struct rte_eth_dev *dev,
				 const struct rte_flow_action *action,
				 uint32_t *dst_port_id,
				 struct rte_flow_error *error)
{
	uint32_t port;
	uint16_t port_id;
	int ret;
	const struct rte_flow_action_port_id *conf =
			(const struct rte_flow_action_port_id *)action->conf;

	port = conf->original ? dev->data->port_id : conf->id;
	ret = mlx5_port_to_eswitch_info(port, NULL, &port_id);
	if (ret)
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "No eswitch info was found for port");
	*dst_port_id = port_id;
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
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_translate(struct rte_eth_dev *dev,
		  struct mlx5_flow *dev_flow,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
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
	bool actions_end = false;
	struct mlx5_flow_dv_modify_hdr_resource res = {
		.ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					  MLX5DV_FLOW_TABLE_TYPE_NIC_RX
	};
	union flow_dv_attr flow_attr = { .attr = 0 };
	struct mlx5_flow_dv_tag_resource tag_resource;
	uint32_t modify_action_position = UINT32_MAX;
	void *match_mask = matcher.mask.buf;
	void *match_value = dev_flow->dv.value.buf;

	flow->group = attr->group;
	if (attr->transfer)
		res.ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = priv->config.flow_prio - 1;
	for (; !actions_end ; actions++) {
		const struct rte_flow_action_queue *queue;
		const struct rte_flow_action_rss *rss;
		const struct rte_flow_action *action = actions;
		const struct rte_flow_action_count *count = action->conf;
		const uint8_t *rss_key;
		const struct rte_flow_action_jump *jump_data;
		struct mlx5_flow_dv_jump_tbl_resource jump_tbl_resource;
		struct mlx5_flow_tbl_resource *tbl;
		uint32_t port_id = 0;
		struct mlx5_flow_dv_port_id_action_resource port_id_resource;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			if (flow_dv_translate_action_port_id(dev, action,
							     &port_id, error))
				return -rte_errno;
			port_id_resource.port_id = port_id;
			if (flow_dv_port_id_action_resource_register
			    (dev, &port_id_resource, dev_flow, error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
				dev_flow->dv.port_id_action->action;
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			tag_resource.tag =
				mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT);
			if (!flow->tag_resource)
				if (flow_dv_tag_resource_register
				    (dev, &tag_resource, dev_flow, error))
					return errno;
			dev_flow->dv.actions[actions_n++] =
				flow->tag_resource->action;
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			tag_resource.tag = mlx5_flow_mark_set
			      (((const struct rte_flow_action_mark *)
			       (actions->conf))->id);
			if (!flow->tag_resource)
				if (flow_dv_tag_resource_register
				    (dev, &tag_resource, dev_flow, error))
					return errno;
			dev_flow->dv.actions[actions_n++] =
				flow->tag_resource->action;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
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
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (!priv->config.devx) {
				rte_errno = ENOTSUP;
				goto cnt_err;
			}
			flow->counter = flow_dv_counter_new(dev, count->shared,
							    count->id);
			if (flow->counter == NULL)
				goto cnt_err;
			dev_flow->dv.actions[actions_n++] =
				flow->counter->action;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
cnt_err:
			if (rte_errno == ENOTSUP)
				return rte_flow_error_set
					      (error, ENOTSUP,
					       RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					       NULL,
					       "count action not supported");
			else
				return rte_flow_error_set
						(error, rte_errno,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 action,
						 "cannot create counter"
						  " object.");
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			if (flow_dv_create_action_l2_encap(dev, actions,
							   dev_flow,
							   attr->transfer,
							   error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
				dev_flow->dv.encap_decap->verbs_action;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP ?
					MLX5_FLOW_ACTION_VXLAN_ENCAP :
					MLX5_FLOW_ACTION_NVGRE_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			if (flow_dv_create_action_l2_decap(dev, dev_flow,
							   attr->transfer,
							   error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
				dev_flow->dv.encap_decap->verbs_action;
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
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->verbs_action;
			} else {
				/* Handle encap without preceding decap. */
				if (flow_dv_create_action_l2_encap
				    (dev, actions, dev_flow, attr->transfer,
				     error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->verbs_action;
			}
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
				if (flow_dv_create_action_l2_decap
				    (dev, dev_flow, attr->transfer, error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->verbs_action;
			}
			/* If decap is followed by encap, handle it at encap. */
			action_flags |= MLX5_FLOW_ACTION_RAW_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			jump_data = action->conf;
			tbl = flow_dv_tbl_resource_get(dev, jump_data->group *
						       MLX5_GROUP_FACTOR,
						       attr->egress,
						       attr->transfer, error);
			if (!tbl)
				return rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL,
						 "cannot create jump action.");
			jump_tbl_resource.tbl = tbl;
			if (flow_dv_jump_tbl_resource_register
			    (dev, &jump_tbl_resource, dev_flow, error)) {
				flow_dv_tbl_resource_release(tbl);
				return rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL,
						 "cannot create jump action.");
			}
			dev_flow->dv.actions[actions_n++] =
				dev_flow->dv.jump->action;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			if (flow_dv_convert_action_modify_mac(&res, actions,
							      error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_MAC_SRC ?
					MLX5_FLOW_ACTION_SET_MAC_SRC :
					MLX5_FLOW_ACTION_SET_MAC_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			if (flow_dv_convert_action_modify_ipv4(&res, actions,
							       error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC ?
					MLX5_FLOW_ACTION_SET_IPV4_SRC :
					MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			if (flow_dv_convert_action_modify_ipv6(&res, actions,
							       error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
					MLX5_FLOW_ACTION_SET_IPV6_SRC :
					MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			if (flow_dv_convert_action_modify_tp(&res, actions,
							     items, &flow_attr,
							     error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_TP_SRC ?
					MLX5_FLOW_ACTION_SET_TP_SRC :
					MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
			if (flow_dv_convert_action_modify_dec_ttl(&res, items,
								  &flow_attr,
								  error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_DEC_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			if (flow_dv_convert_action_modify_ttl(&res, actions,
							     items, &flow_attr,
							     error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			if (action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS) {
				/* create modify action if needed. */
				if (flow_dv_modify_hdr_resource_register
								(dev, &res,
								 dev_flow,
								 error))
					return -rte_errno;
				dev_flow->dv.actions[modify_action_position] =
					dev_flow->dv.modify_hdr->verbs_action;
			}
			break;
		default:
			break;
		}
		if ((action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS) &&
		    modify_action_position == UINT32_MAX)
			modify_action_position = actions_n++;
	}
	dev_flow->dv.actions_n = actions_n;
	flow->actions = action_flags;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);

		switch (items->type) {
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			flow_dv_translate_item_port_id(dev, match_mask,
						       match_value, items);
			last_item = MLX5_FLOW_ITEM_PORT_ID;
			break;
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
						    items, tunnel, attr->group);
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
						    items, tunnel, attr->group);
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
	/*
	 * In case of ingress traffic when E-Switch mode is enabled,
	 * we have two cases where we need to set the source port manually.
	 * The first one, is in case of Nic steering rule, and the second is
	 * E-Switch rule where no port_id item was found. In both cases
	 * the source port is set according the current port in use.
	 */
	if ((attr->ingress && !(item_flags & MLX5_FLOW_ITEM_PORT_ID)) &&
	    (priv->representor || priv->master)) {
		if (flow_dv_translate_item_port_id(dev, match_mask,
						   match_value, NULL))
			return -rte_errno;
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
	matcher.group = attr->group;
	matcher.transfer = attr->transfer;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	int n;
	int err;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		dv = &dev_flow->dv;
		n = dv->actions_n;
		if (flow->actions & MLX5_FLOW_ACTION_DROP) {
			if (flow->transfer) {
				dv->actions[n++] = priv->sh->esw_drop_action;
			} else {
				dv->hrxq = mlx5_hrxq_drop_new(dev);
				if (!dv->hrxq) {
					rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						 NULL,
						 "cannot get drop hash queue");
					goto error;
				}
				dv->actions[n++] = dv->hrxq->action;
			}
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
			dv->actions[n++] = dv->hrxq->action;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_tbl_resource *tbl;

	assert(matcher->matcher_object);
	DRV_LOG(DEBUG, "port %u matcher %p: refcnt %d--",
		dev->data->port_id, (void *)matcher,
		rte_atomic32_read(&matcher->refcnt));
	if (rte_atomic32_dec_and_test(&matcher->refcnt)) {
		claim_zero(mlx5_glue->dv_destroy_flow_matcher
			   (matcher->matcher_object));
		LIST_REMOVE(matcher, next);
		if (matcher->egress)
			tbl = &sh->tx_tbl[matcher->group];
		else
			tbl = &sh->rx_tbl[matcher->group];
		flow_dv_tbl_resource_release(tbl);
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
 * Release an jump to table action resource.
 *
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_jump_tbl_resource_release(struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_jump_tbl_resource *cache_resource =
						flow->dv.jump;

	assert(cache_resource->action);
	DRV_LOG(DEBUG, "jump table resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->action));
		LIST_REMOVE(cache_resource, next);
		flow_dv_tbl_resource_release(cache_resource->tbl);
		rte_free(cache_resource);
		DRV_LOG(DEBUG, "jump table resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release a modify-header resource.
 *
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_modify_hdr_resource_release(struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_modify_hdr_resource *cache_resource =
						flow->dv.modify_hdr;

	assert(cache_resource->verbs_action);
	DRV_LOG(DEBUG, "modify-header resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->verbs_action));
		LIST_REMOVE(cache_resource, next);
		rte_free(cache_resource);
		DRV_LOG(DEBUG, "modify-header resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release port ID action resource.
 *
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_port_id_action_resource_release(struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_port_id_action_resource *cache_resource =
		flow->dv.port_id_action;

	assert(cache_resource->action);
	DRV_LOG(DEBUG, "port ID action resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->action));
		LIST_REMOVE(cache_resource, next);
		rte_free(cache_resource);
		DRV_LOG(DEBUG, "port id action resource %p: removed",
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
			claim_zero(mlx5_glue->dv_destroy_flow(dv->flow));
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
	if (flow->counter) {
		flow_dv_counter_release(flow->counter);
		flow->counter = NULL;
	}
	if (flow->tag_resource) {
		flow_dv_tag_release(dev, flow->tag_resource);
		flow->tag_resource = NULL;
	}
	while (!LIST_EMPTY(&flow->dev_flows)) {
		dev_flow = LIST_FIRST(&flow->dev_flows);
		LIST_REMOVE(dev_flow, next);
		if (dev_flow->dv.matcher)
			flow_dv_matcher_release(dev, dev_flow);
		if (dev_flow->dv.encap_decap)
			flow_dv_encap_decap_resource_release(dev_flow);
		if (dev_flow->dv.modify_hdr)
			flow_dv_modify_hdr_resource_release(dev_flow);
		if (dev_flow->dv.jump)
			flow_dv_jump_tbl_resource_release(dev_flow);
		if (dev_flow->dv.port_id_action)
			flow_dv_port_id_action_resource_release(dev_flow);
		rte_free(dev_flow);
	}
}

/**
 * Query a dv flow  rule for its statistics via devx.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to the sub flow.
 * @param[out] data
 *   data retrieved by the query.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_query_count(struct rte_eth_dev *dev, struct rte_flow *flow,
		    void *data, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_query_count *qc = data;
	uint64_t pkts = 0;
	uint64_t bytes = 0;
	int err;

	if (!priv->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "counters are not supported");
	if (flow->counter) {
		err = mlx5_devx_cmd_flow_counter_query
						(flow->counter->dcs,
						 qc->reset, &pkts, &bytes);
		if (err)
			return rte_flow_error_set
				(error, err,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL,
				 "cannot read counters");
		qc->hits_set = 1;
		qc->bytes_set = 1;
		qc->hits = pkts - flow->counter->hits;
		qc->bytes = bytes - flow->counter->bytes;
		if (qc->reset) {
			flow->counter->hits = pkts;
			flow->counter->bytes = bytes;
		}
		return 0;
	}
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "counters are not available");
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_dv_query(struct rte_eth_dev *dev,
	      struct rte_flow *flow __rte_unused,
	      const struct rte_flow_action *actions __rte_unused,
	      void *data __rte_unused,
	      struct rte_flow_error *error __rte_unused)
{
	int ret = -EINVAL;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_dv_query_count(dev, flow, data, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	return ret;
}

/*
 * Mutex-protected thunk to flow_dv_translate().
 */
static int
flow_d_translate(struct rte_eth_dev *dev,
		 struct mlx5_flow *dev_flow,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	int ret;

	flow_d_shared_lock(dev);
	ret = flow_dv_translate(dev, dev_flow, attr, items, actions, error);
	flow_d_shared_unlock(dev);
	return ret;
}

/*
 * Mutex-protected thunk to flow_dv_apply().
 */
static int
flow_d_apply(struct rte_eth_dev *dev,
	     struct rte_flow *flow,
	     struct rte_flow_error *error)
{
	int ret;

	flow_d_shared_lock(dev);
	ret = flow_dv_apply(dev, flow, error);
	flow_d_shared_unlock(dev);
	return ret;
}

/*
 * Mutex-protected thunk to flow_dv_remove().
 */
static void
flow_d_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	flow_d_shared_lock(dev);
	flow_dv_remove(dev, flow);
	flow_d_shared_unlock(dev);
}

/*
 * Mutex-protected thunk to flow_dv_destroy().
 */
static void
flow_d_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	flow_d_shared_lock(dev);
	flow_dv_destroy(dev, flow);
	flow_d_shared_unlock(dev);
}

const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops = {
	.validate = flow_dv_validate,
	.prepare = flow_dv_prepare,
	.translate = flow_d_translate,
	.apply = flow_d_apply,
	.remove = flow_d_remove,
	.destroy = flow_d_destroy,
	.query = flow_dv_query,
};

#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
