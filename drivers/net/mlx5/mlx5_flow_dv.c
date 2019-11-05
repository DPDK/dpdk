/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

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
#include <rte_vxlan.h>

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

/* VLAN header definitions */
#define MLX5DV_FLOW_VLAN_PCP_SHIFT 13
#define MLX5DV_FLOW_VLAN_PCP_MASK (0x7 << MLX5DV_FLOW_VLAN_PCP_SHIFT)
#define MLX5DV_FLOW_VLAN_VID_MASK 0x0fff
#define MLX5DV_FLOW_VLAN_PCP_MASK_BE RTE_BE16(MLX5DV_FLOW_VLAN_PCP_MASK)
#define MLX5DV_FLOW_VLAN_VID_MASK_BE RTE_BE16(MLX5DV_FLOW_VLAN_VID_MASK)

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

struct field_modify_info modify_vlan_out_first_vid[] = {
	/* Size in bits !!! */
	{12, 0, MLX5_MODI_OUT_FIRST_VID},
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
	{4, 4, MLX5_MODI_OUT_TCP_SEQ_NUM},
	{4, 8, MLX5_MODI_OUT_TCP_ACK_NUM},
	{0, 0, 0},
};

static void
mlx5_flow_tunnel_ip_check(const struct rte_flow_item *item __rte_unused,
			  uint8_t next_protocol, uint64_t *item_flags,
			  int *tunnel)
{
	assert(item->type == RTE_FLOW_ITEM_TYPE_IPV4 ||
	       item->type == RTE_FLOW_ITEM_TYPE_IPV6);
	if (next_protocol == IPPROTO_IPIP) {
		*item_flags |= MLX5_FLOW_LAYER_IPIP;
		*tunnel = 1;
	}
	if (next_protocol == IPPROTO_IPV6) {
		*item_flags |= MLX5_FLOW_LAYER_IPV6_ENCAP;
		*tunnel = 1;
	}
}

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

/* Update VLAN's VID/PCP based on input rte_flow_action.
 *
 * @param[in] action
 *   Pointer to struct rte_flow_action.
 * @param[out] vlan
 *   Pointer to struct rte_vlan_hdr.
 */
static void
mlx5_update_vlan_vid_pcp(const struct rte_flow_action *action,
			 struct rte_vlan_hdr *vlan)
{
	uint16_t vlan_tci;
	if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) {
		vlan_tci =
		    ((const struct rte_flow_action_of_set_vlan_pcp *)
					       action->conf)->vlan_pcp;
		vlan_tci = vlan_tci << MLX5DV_FLOW_VLAN_PCP_SHIFT;
		vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_PCP_MASK;
		vlan->vlan_tci |= vlan_tci;
	} else if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) {
		vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_VID_MASK;
		vlan->vlan_tci |= rte_be_to_cpu_16
		    (((const struct rte_flow_action_of_set_vlan_vid *)
					     action->conf)->vlan_vid);
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
 * Convert modify-header set VLAN VID action to DV specification.
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
flow_dv_convert_action_modify_vlan_vid
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_of_set_vlan_vid *conf =
		(const struct rte_flow_action_of_set_vlan_vid *)(action->conf);
	int i = resource->actions_num;
	struct mlx5_modification_cmd *actions = &resource->actions[i];
	struct field_modify_info *field = modify_vlan_out_first_vid;

	if (i >= MLX5_MODIFY_NUM)
		return rte_flow_error_set(error, EINVAL,
			 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			 "too many items to modify");
	actions[i].action_type = MLX5_MODIFICATION_TYPE_SET;
	actions[i].field = field->id;
	actions[i].length = field->size;
	actions[i].offset = field->offset;
	actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
	actions[i].data1 = conf->vlan_vid;
	actions[i].data1 = actions[i].data1 << 16;
	resource->actions_num = ++i;
	return 0;
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
 * Convert modify-header increment/decrement TCP Sequence number
 * to DV specification.
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
flow_dv_convert_action_modify_tcp_seq
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const rte_be32_t *conf = (const rte_be32_t *)(action->conf);
	uint64_t value = rte_be_to_cpu_32(*conf);
	struct rte_flow_item item;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_tcp tcp_mask;

	memset(&tcp, 0, sizeof(tcp));
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	if (action->type == RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ)
		/*
		 * The HW has no decrement operation, only increment operation.
		 * To simulate decrement X from Y using increment operation
		 * we need to add UINT32_MAX X times to Y.
		 * Each adding of UINT32_MAX decrements Y by 1.
		 */
		value *= UINT32_MAX;
	tcp.hdr.sent_seq = rte_cpu_to_be_32((uint32_t)value);
	tcp_mask.hdr.sent_seq = RTE_BE32(UINT32_MAX);
	item.type = RTE_FLOW_ITEM_TYPE_TCP;
	item.spec = &tcp;
	item.mask = &tcp_mask;
	return flow_dv_convert_modify_action(&item, modify_tcp, resource,
					     MLX5_MODIFICATION_TYPE_ADD, error);
}

/**
 * Convert modify-header increment/decrement TCP Acknowledgment number
 * to DV specification.
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
flow_dv_convert_action_modify_tcp_ack
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const rte_be32_t *conf = (const rte_be32_t *)(action->conf);
	uint64_t value = rte_be_to_cpu_32(*conf);
	struct rte_flow_item item;
	struct rte_flow_item_tcp tcp;
	struct rte_flow_item_tcp tcp_mask;

	memset(&tcp, 0, sizeof(tcp));
	memset(&tcp_mask, 0, sizeof(tcp_mask));
	if (action->type == RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK)
		/*
		 * The HW has no decrement operation, only increment operation.
		 * To simulate decrement X from Y using increment operation
		 * we need to add UINT32_MAX X times to Y.
		 * Each adding of UINT32_MAX decrements Y by 1.
		 */
		value *= UINT32_MAX;
	tcp.hdr.recv_ack = rte_cpu_to_be_32((uint32_t)value);
	tcp_mask.hdr.recv_ack = RTE_BE32(UINT32_MAX);
	item.type = RTE_FLOW_ITEM_TYPE_TCP;
	item.spec = &tcp;
	item.mask = &tcp_mask;
	return flow_dv_convert_modify_action(&item, modify_tcp, resource,
					     MLX5_MODIFICATION_TYPE_ADD, error);
}

static enum mlx5_modification_field reg_to_field[] = {
	[REG_A] = MLX5_MODI_META_DATA_REG_A,
	[REG_B] = MLX5_MODI_META_DATA_REG_B,
	[REG_C_0] = MLX5_MODI_META_REG_C_0,
	[REG_C_1] = MLX5_MODI_META_REG_C_1,
	[REG_C_2] = MLX5_MODI_META_REG_C_2,
	[REG_C_3] = MLX5_MODI_META_REG_C_3,
	[REG_C_4] = MLX5_MODI_META_REG_C_4,
	[REG_C_5] = MLX5_MODI_META_REG_C_5,
	[REG_C_6] = MLX5_MODI_META_REG_C_6,
	[REG_C_7] = MLX5_MODI_META_REG_C_7,
};

/**
 * Convert register set to DV specification.
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
flow_dv_convert_action_set_reg
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct mlx5_rte_flow_action_set_tag *conf = (action->conf);
	struct mlx5_modification_cmd *actions = resource->actions;
	uint32_t i = resource->actions_num;

	if (i >= MLX5_MODIFY_NUM)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "too many items to modify");
	actions[i].action_type = MLX5_MODIFICATION_TYPE_SET;
	actions[i].field = reg_to_field[conf->id];
	actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
	actions[i].data1 = conf->data;
	++i;
	resource->actions_num = i;
	if (!resource->actions_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "invalid modification flow item");
	return 0;
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
flow_dv_validate_item_meta(struct rte_eth_dev *dev __rte_unused,
			   const struct rte_flow_item *item,
			   const struct rte_flow_attr *attr,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_meta *spec = item->spec;
	const struct rte_flow_item_meta *mask = item->mask;
	const struct rte_flow_item_meta nic_mask = {
		.data = UINT32_MAX
	};
	int ret;

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
	struct mlx5_priv *esw_priv;
	struct mlx5_priv *dev_priv;
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
	esw_priv = mlx5_port_to_eswitch_info(spec->id);
	if (!esw_priv)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "failed to obtain E-Switch info for"
					  " port");
	dev_priv = mlx5_dev_to_eswitch_info(dev);
	if (!dev_priv)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to obtain E-Switch info");
	if (esw_priv->domain_id != dev_priv->domain_id)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "cannot match on a port from a"
					  " different E-Switch");
	return 0;
}

/**
 * Validate the pop VLAN action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the pop vlan action.
 * @param[in] item_flags
 *   The items found in this flow rule.
 * @param[in] attr
 *   Pointer to flow attributes.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_pop_vlan(struct rte_eth_dev *dev,
				 uint64_t action_flags,
				 const struct rte_flow_action *action,
				 uint64_t item_flags,
				 const struct rte_flow_attr *attr,
				 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	(void)action;
	(void)attr;
	if (!priv->sh->pop_vlan_action)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "pop vlan action is not supported");
	/*
	 * Check for inconsistencies:
	 *  fail strip_vlan in a flow that matches packets without VLAN tags.
	 *  fail strip_vlan in a flow that matches packets without explicitly a
	 *  matching on VLAN tag ?
	 */
	if (action_flags & MLX5_FLOW_ACTION_OF_POP_VLAN)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "no support for multiple vlan pop "
					  "actions");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot pop vlan without a "
					  "match on (outer) vlan in the flow");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after pop VLAN action");
	return 0;
}

/**
 * Get VLAN default info from vlan match info.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   the list of item specifications.
 * @param[out] vlan
 *   pointer VLAN info to fill to.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static void
flow_dev_get_vlan_info_from_items(const struct rte_flow_item *items,
				  struct rte_vlan_hdr *vlan)
{
	const struct rte_flow_item_vlan nic_mask = {
		.tci = RTE_BE16(MLX5DV_FLOW_VLAN_PCP_MASK |
				MLX5DV_FLOW_VLAN_VID_MASK),
		.inner_type = RTE_BE16(0xffff),
	};

	if (items == NULL)
		return;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END &&
	       items->type != RTE_FLOW_ITEM_TYPE_VLAN; items++)
		;
	if (items->type == RTE_FLOW_ITEM_TYPE_VLAN) {
		const struct rte_flow_item_vlan *vlan_m = items->mask;
		const struct rte_flow_item_vlan *vlan_v = items->spec;

		if (!vlan_m)
			vlan_m = &nic_mask;
		/* Only full match values are accepted */
		if ((vlan_m->tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) ==
		     MLX5DV_FLOW_VLAN_PCP_MASK_BE) {
			vlan->vlan_tci &= MLX5DV_FLOW_VLAN_PCP_MASK;
			vlan->vlan_tci |=
				rte_be_to_cpu_16(vlan_v->tci &
						 MLX5DV_FLOW_VLAN_PCP_MASK_BE);
		}
		if ((vlan_m->tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) ==
		     MLX5DV_FLOW_VLAN_VID_MASK_BE) {
			vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_VID_MASK;
			vlan->vlan_tci |=
				rte_be_to_cpu_16(vlan_v->tci &
						 MLX5DV_FLOW_VLAN_VID_MASK_BE);
		}
		if (vlan_m->inner_type == nic_mask.inner_type)
			vlan->eth_proto = rte_be_to_cpu_16(vlan_v->inner_type &
							   vlan_m->inner_type);
	}
}

/**
 * Validate the push VLAN action.
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
flow_dv_validate_action_push_vlan(uint64_t action_flags,
				  uint64_t item_flags,
				  const struct rte_flow_action *action,
				  const struct rte_flow_attr *attr,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_of_push_vlan *push_vlan = action->conf;

	if (push_vlan->ethertype != RTE_BE16(RTE_ETHER_TYPE_VLAN) &&
	    push_vlan->ethertype != RTE_BE16(RTE_ETHER_TYPE_QINQ))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "invalid vlan ethertype");
	if (action_flags &
		(MLX5_FLOW_ACTION_OF_POP_VLAN | MLX5_FLOW_ACTION_OF_PUSH_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no support for multiple VLAN "
					  "actions");
	if (!mlx5_flow_find_action
			(action + 1, RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID) &&
	    !(item_flags & MLX5_FLOW_LAYER_OUTER_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"push VLAN needs to match on VLAN in order to "
				"get VLAN VID information because there is "
				"no followed set VLAN VID action");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after push VLAN");
	(void)attr;
	return 0;
}

/**
 * Validate the set VLAN PCP.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] actions
 *   Pointer to the list of actions remaining in the flow rule.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_set_vlan_pcp(uint64_t action_flags,
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error)
{
	const struct rte_flow_action *action = actions;
	const struct rte_flow_action_of_set_vlan_pcp *conf = action->conf;

	if (conf->vlan_pcp > 7)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "VLAN PCP value is too big");
	if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "set VLAN PCP action must follow "
					  "the push VLAN action");
	if (action_flags & MLX5_FLOW_ACTION_OF_SET_VLAN_PCP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Multiple VLAN PCP modification are "
					  "not supported");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after set VLAN PCP");
	return 0;
}

/**
 * Validate the set VLAN VID.
 *
 * @param[in] item_flags
 *   Holds the items detected in this rule.
 * @param[in] actions
 *   Pointer to the list of actions remaining in the flow rule.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_set_vlan_vid(uint64_t item_flags,
				     uint64_t action_flags,
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error)
{
	const struct rte_flow_action *action = actions;
	const struct rte_flow_action_of_set_vlan_vid *conf = action->conf;

	if (conf->vlan_vid > RTE_BE16(0xFFE))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "VLAN VID value is too big");
	/* there is an of_push_vlan action before us */
	if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN) {
		if (mlx5_flow_find_action(actions + 1,
					  RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID))
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"Multiple VLAN VID modifications are "
					"not supported");
		else
			return 0;
	}

	/*
	 * Action is on an existing VLAN header:
	 *    Need to verify this is a single modify CID action.
	 *   Rule mast include a match on outer VLAN.
	 */
	if (action_flags & MLX5_FLOW_ACTION_OF_SET_VLAN_VID)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Multiple VLAN VID modifications are "
					  "not supported");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_VLAN))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "match on VLAN is required in order "
					  "to set VLAN VID");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after set VLAN VID");
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
	const struct rte_flow_action_raw_encap *raw_encap =
		(const struct rte_flow_action_raw_encap *)action->conf;
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
	if (!raw_encap->size || !raw_encap->data)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "raw encap data cannot be empty");
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
 * Find existing push vlan resource or create and register a new one.
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
flow_dv_push_vlan_action_resource_register
		       (struct rte_eth_dev *dev,
			struct mlx5_flow_dv_push_vlan_action_resource *resource,
			struct mlx5_flow *dev_flow,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;
	struct mlx5_flow_dv_push_vlan_action_resource *cache_resource;
	struct mlx5dv_dr_domain *domain;

	/* Lookup a matching resource from cache. */
	LIST_FOREACH(cache_resource, &sh->push_vlan_action_list, next) {
		if (resource->vlan_tag == cache_resource->vlan_tag &&
		    resource->ft_type == cache_resource->ft_type) {
			DRV_LOG(DEBUG, "push-VLAN action resource resource %p: "
				"refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->dv.push_vlan_res = cache_resource;
			return 0;
		}
	}
	/* Register new push_vlan action resource. */
	cache_resource = rte_calloc(__func__, 1, sizeof(*cache_resource), 0);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;
	cache_resource->action =
		mlx5_glue->dr_create_flow_action_push_vlan(domain,
							   resource->vlan_tag);
	if (!cache_resource->action) {
		rte_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->push_vlan_action_list, cache_resource, next);
	dev_flow->dv.push_vlan_res = cache_resource;
	DRV_LOG(DEBUG, "new push vlan action resource %p: refcnt %d++",
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
	struct rte_ether_hdr *eth = NULL;
	struct rte_vlan_hdr *vlan = NULL;
	struct rte_ipv4_hdr *ipv4 = NULL;
	struct rte_ipv6_hdr *ipv6 = NULL;
	struct rte_udp_hdr *udp = NULL;
	struct rte_vxlan_hdr *vxlan = NULL;
	struct rte_vxlan_gpe_hdr *vxlan_gpe = NULL;
	struct rte_gre_hdr *gre = NULL;
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
			eth = (struct rte_ether_hdr *)&buf[temp_size];
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			vlan = (struct rte_vlan_hdr *)&buf[temp_size];
			if (!eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"eth header not found");
			if (!eth->ether_type)
				eth->ether_type = RTE_BE16(RTE_ETHER_TYPE_VLAN);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ipv4 = (struct rte_ipv4_hdr *)&buf[temp_size];
			if (!vlan && !eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"neither eth nor vlan"
						" header found");
			if (vlan && !vlan->eth_proto)
				vlan->eth_proto = RTE_BE16(RTE_ETHER_TYPE_IPV4);
			else if (eth && !eth->ether_type)
				eth->ether_type = RTE_BE16(RTE_ETHER_TYPE_IPV4);
			if (!ipv4->version_ihl)
				ipv4->version_ihl = MLX5_ENCAP_IPV4_VERSION |
						    MLX5_ENCAP_IPV4_IHL_MIN;
			if (!ipv4->time_to_live)
				ipv4->time_to_live = MLX5_ENCAP_IPV4_TTL_DEF;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ipv6 = (struct rte_ipv6_hdr *)&buf[temp_size];
			if (!vlan && !eth)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						(void *)items->type,
						"neither eth nor vlan"
						" header found");
			if (vlan && !vlan->eth_proto)
				vlan->eth_proto = RTE_BE16(RTE_ETHER_TYPE_IPV6);
			else if (eth && !eth->ether_type)
				eth->ether_type = RTE_BE16(RTE_ETHER_TYPE_IPV6);
			if (!ipv6->vtc_flow)
				ipv6->vtc_flow =
					RTE_BE32(MLX5_ENCAP_IPV6_VTC_FLOW);
			if (!ipv6->hop_limits)
				ipv6->hop_limits = MLX5_ENCAP_IPV6_HOP_LIMIT;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp = (struct rte_udp_hdr *)&buf[temp_size];
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
			vxlan = (struct rte_vxlan_hdr *)&buf[temp_size];
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
			vxlan_gpe = (struct rte_vxlan_gpe_hdr *)&buf[temp_size];
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
			gre = (struct rte_gre_hdr *)&buf[temp_size];
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

static int
flow_dv_zero_encap_udp_csum(void *data, struct rte_flow_error *error)
{
	struct rte_ether_hdr *eth = NULL;
	struct rte_vlan_hdr *vlan = NULL;
	struct rte_ipv6_hdr *ipv6 = NULL;
	struct rte_udp_hdr *udp = NULL;
	char *next_hdr;
	uint16_t proto;

	eth = (struct rte_ether_hdr *)data;
	next_hdr = (char *)(eth + 1);
	proto = RTE_BE16(eth->ether_type);

	/* VLAN skipping */
	while (proto == RTE_ETHER_TYPE_VLAN || proto == RTE_ETHER_TYPE_QINQ) {
		vlan = (struct rte_vlan_hdr *)next_hdr;
		proto = RTE_BE16(vlan->eth_proto);
		next_hdr += sizeof(struct rte_vlan_hdr);
	}

	/* HW calculates IPv4 csum. no need to proceed */
	if (proto == RTE_ETHER_TYPE_IPV4)
		return 0;

	/* non IPv4/IPv6 header. not supported */
	if (proto != RTE_ETHER_TYPE_IPV6) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "Cannot offload non IPv4/IPv6");
	}

	ipv6 = (struct rte_ipv6_hdr *)next_hdr;

	/* ignore non UDP */
	if (ipv6->proto != IPPROTO_UDP)
		return 0;

	udp = (struct rte_udp_hdr *)(ipv6 + 1);
	udp->dgram_cksum = 0;

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
		if (flow_dv_zero_encap_udp_csum(res.buf, error))
			return -rte_errno;
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
 * Create action push VLAN.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] vlan_tag
 *   the vlan tag to push to the Ethernet header.
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
flow_dv_create_action_push_vlan(struct rte_eth_dev *dev,
				const struct rte_flow_attr *attr,
				const struct rte_vlan_hdr *vlan,
				struct mlx5_flow *dev_flow,
				struct rte_flow_error *error)
{
	struct mlx5_flow_dv_push_vlan_action_resource res;

	res.vlan_tag =
		rte_cpu_to_be_32(((uint32_t)vlan->eth_proto) << 16 |
				 vlan->vlan_tci);
	if (attr->transfer)
		res.ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	else
		res.ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					     MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	return flow_dv_push_vlan_action_resource_register
					    (dev, &res, dev_flow, error);
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
 * Validate the modify-header actions of increment/decrement
 * TCP Sequence-number.
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
flow_dv_validate_action_modify_tcp_seq(const uint64_t action_flags,
				       const struct rte_flow_action *action,
				       const uint64_t item_flags,
				       struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no TCP item in"
						  " pattern");
		if ((action->type == RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ &&
			(action_flags & MLX5_FLOW_ACTION_DEC_TCP_SEQ)) ||
		    (action->type == RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ &&
			(action_flags & MLX5_FLOW_ACTION_INC_TCP_SEQ)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "cannot decrease and increase"
						  " TCP sequence number"
						  " at the same time");
	}
	return ret;
}

/**
 * Validate the modify-header actions of increment/decrement
 * TCP Acknowledgment number.
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
flow_dv_validate_action_modify_tcp_ack(const uint64_t action_flags,
				       const struct rte_flow_action *action,
				       const uint64_t item_flags,
				       struct rte_flow_error *error)
{
	int ret = 0;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no TCP item in"
						  " pattern");
		if ((action->type == RTE_FLOW_ACTION_TYPE_INC_TCP_ACK &&
			(action_flags & MLX5_FLOW_ACTION_DEC_TCP_ACK)) ||
		    (action->type == RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK &&
			(action_flags & MLX5_FLOW_ACTION_INC_TCP_ACK)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "cannot decrease and increase"
						  " TCP acknowledgment number"
						  " at the same time");
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
 *   Pointer to the jump action.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[in] external
 *   Action belongs to flow rule created by request external to PMD.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_jump(const struct rte_flow_action *action,
			     uint64_t action_flags,
			     const struct rte_flow_attr *attributes,
			     bool external, struct rte_flow_error *error)
{
	uint32_t max_group = attributes->transfer ? MLX5_MAX_TABLES_FDB :
						    MLX5_MAX_TABLES;
	uint32_t target_group, table;
	int ret = 0;

	if (action_flags & (MLX5_FLOW_FATE_ACTIONS |
			    MLX5_FLOW_FATE_ESWITCH_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions in"
					  " same flow");
	if (!action->conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "action configuration not set");
	target_group =
		((const struct rte_flow_action_jump *)action->conf)->group;
	ret = mlx5_flow_group_to_table(attributes, external, target_group,
				       &table, error);
	if (ret)
		return ret;
	if (table >= max_group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL,
					  "target group index out of range");
	if (attributes->group >= target_group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "target group must be higher than"
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
	struct mlx5_priv *act_priv;
	struct mlx5_priv *dev_priv;
	uint16_t port;

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
	dev_priv = mlx5_dev_to_eswitch_info(dev);
	if (!dev_priv)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to obtain E-Switch info");
	port_id = action->conf;
	port = port_id->original ? dev->data->port_id : port_id->id;
	act_priv = mlx5_port_to_eswitch_info(port);
	if (!act_priv)
		return rte_flow_error_set
				(error, rte_errno,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF, port_id,
				 "failed to obtain E-Switch port id for port");
	if (act_priv->domain_id != dev_priv->domain_id)
		return rte_flow_error_set
				(error, EINVAL,
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

#define MLX5_CNT_CONTAINER_RESIZE 64

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
flow_dv_counter_alloc_fallback(struct rte_eth_dev *dev, uint32_t shared,
			       uint32_t id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt = NULL;
	struct mlx5_devx_obj *dcs = NULL;

	if (!priv->config.devx) {
		rte_errno = ENOTSUP;
		return NULL;
	}
	if (shared) {
		TAILQ_FOREACH(cnt, &priv->sh->cmng.flow_counters, next) {
			if (cnt->shared && cnt->id == id) {
				cnt->ref_cnt++;
				return cnt;
			}
		}
	}
	dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->ctx, 0);
	if (!dcs)
		return NULL;
	cnt = rte_calloc(__func__, 1, sizeof(*cnt), 0);
	if (!cnt) {
		claim_zero(mlx5_devx_cmd_destroy(cnt->dcs));
		rte_errno = ENOMEM;
		return NULL;
	}
	struct mlx5_flow_counter tmpl = {
		.shared = shared,
		.ref_cnt = 1,
		.id = id,
		.dcs = dcs,
	};
	tmpl.action = mlx5_glue->dv_create_flow_action_counter(dcs->obj, 0);
	if (!tmpl.action) {
		claim_zero(mlx5_devx_cmd_destroy(cnt->dcs));
		rte_errno = errno;
		rte_free(cnt);
		return NULL;
	}
	*cnt = tmpl;
	TAILQ_INSERT_HEAD(&priv->sh->cmng.flow_counters, cnt, next);
	return cnt;
}

/**
 * Release a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Pointer to the counter handler.
 */
static void
flow_dv_counter_release_fallback(struct rte_eth_dev *dev,
				 struct mlx5_flow_counter *counter)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!counter)
		return;
	if (--counter->ref_cnt == 0) {
		TAILQ_REMOVE(&priv->sh->cmng.flow_counters, counter, next);
		claim_zero(mlx5_devx_cmd_destroy(counter->dcs));
		rte_free(counter);
	}
}

/**
 * Query a devx flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] cnt
 *   Pointer to the flow counter.
 * @param[out] pkts
 *   The statistics value of packets.
 * @param[out] bytes
 *   The statistics value of bytes.
 *
 * @return
 *   0 on success, otherwise a negative errno value and rte_errno is set.
 */
static inline int
_flow_dv_query_count_fallback(struct rte_eth_dev *dev __rte_unused,
		     struct mlx5_flow_counter *cnt, uint64_t *pkts,
		     uint64_t *bytes)
{
	return mlx5_devx_cmd_flow_counter_query(cnt->dcs, 0, 0, pkts, bytes,
						0, NULL, NULL, 0);
}

/**
 * Get a pool by a counter.
 *
 * @param[in] cnt
 *   Pointer to the counter.
 *
 * @return
 *   The counter pool.
 */
static struct mlx5_flow_counter_pool *
flow_dv_counter_pool_get(struct mlx5_flow_counter *cnt)
{
	if (!cnt->batch) {
		cnt -= cnt->dcs->id % MLX5_COUNTERS_PER_POOL;
		return (struct mlx5_flow_counter_pool *)cnt - 1;
	}
	return cnt->pool;
}

/**
 * Get a pool by devx counter ID.
 *
 * @param[in] cont
 *   Pointer to the counter container.
 * @param[in] id
 *   The counter devx ID.
 *
 * @return
 *   The counter pool pointer if exists, NULL otherwise,
 */
static struct mlx5_flow_counter_pool *
flow_dv_find_pool_by_id(struct mlx5_pools_container *cont, int id)
{
	struct mlx5_flow_counter_pool *pool;

	TAILQ_FOREACH(pool, &cont->pool_list, next) {
		int base = (pool->min_dcs->id / MLX5_COUNTERS_PER_POOL) *
				MLX5_COUNTERS_PER_POOL;

		if (id >= base && id < base + MLX5_COUNTERS_PER_POOL)
			return pool;
	};
	return NULL;
}

/**
 * Allocate a new memory for the counter values wrapped by all the needed
 * management.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] raws_n
 *   The raw memory areas - each one for MLX5_COUNTERS_PER_POOL counters.
 *
 * @return
 *   The new memory management pointer on success, otherwise NULL and rte_errno
 *   is set.
 */
static struct mlx5_counter_stats_mem_mng *
flow_dv_create_counter_stat_mem_mng(struct rte_eth_dev *dev, int raws_n)
{
	struct mlx5_ibv_shared *sh = ((struct mlx5_priv *)
					(dev->data->dev_private))->sh;
	struct mlx5_devx_mkey_attr mkey_attr;
	struct mlx5_counter_stats_mem_mng *mem_mng;
	volatile struct flow_counter_stats *raw_data;
	int size = (sizeof(struct flow_counter_stats) *
			MLX5_COUNTERS_PER_POOL +
			sizeof(struct mlx5_counter_stats_raw)) * raws_n +
			sizeof(struct mlx5_counter_stats_mem_mng);
	uint8_t *mem = rte_calloc(__func__, 1, size, sysconf(_SC_PAGESIZE));
	int i;

	if (!mem) {
		rte_errno = ENOMEM;
		return NULL;
	}
	mem_mng = (struct mlx5_counter_stats_mem_mng *)(mem + size) - 1;
	size = sizeof(*raw_data) * MLX5_COUNTERS_PER_POOL * raws_n;
	mem_mng->umem = mlx5_glue->devx_umem_reg(sh->ctx, mem, size,
						 IBV_ACCESS_LOCAL_WRITE);
	if (!mem_mng->umem) {
		rte_errno = errno;
		rte_free(mem);
		return NULL;
	}
	mkey_attr.addr = (uintptr_t)mem;
	mkey_attr.size = size;
	mkey_attr.umem_id = mem_mng->umem->umem_id;
	mkey_attr.pd = sh->pdn;
	mem_mng->dm = mlx5_devx_cmd_mkey_create(sh->ctx, &mkey_attr);
	if (!mem_mng->dm) {
		mlx5_glue->devx_umem_dereg(mem_mng->umem);
		rte_errno = errno;
		rte_free(mem);
		return NULL;
	}
	mem_mng->raws = (struct mlx5_counter_stats_raw *)(mem + size);
	raw_data = (volatile struct flow_counter_stats *)mem;
	for (i = 0; i < raws_n; ++i) {
		mem_mng->raws[i].mem_mng = mem_mng;
		mem_mng->raws[i].data = raw_data + i * MLX5_COUNTERS_PER_POOL;
	}
	LIST_INSERT_HEAD(&sh->cmng.mem_mngs, mem_mng, next);
	return mem_mng;
}

/**
 * Resize a counter container.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] batch
 *   Whether the pool is for counter that was allocated by batch command.
 *
 * @return
 *   The new container pointer on success, otherwise NULL and rte_errno is set.
 */
static struct mlx5_pools_container *
flow_dv_container_resize(struct rte_eth_dev *dev, uint32_t batch)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_pools_container *cont =
			MLX5_CNT_CONTAINER(priv->sh, batch, 0);
	struct mlx5_pools_container *new_cont =
			MLX5_CNT_CONTAINER_UNUSED(priv->sh, batch, 0);
	struct mlx5_counter_stats_mem_mng *mem_mng;
	uint32_t resize = cont->n + MLX5_CNT_CONTAINER_RESIZE;
	uint32_t mem_size = sizeof(struct mlx5_flow_counter_pool *) * resize;
	int i;

	if (cont != MLX5_CNT_CONTAINER(priv->sh, batch, 1)) {
		/* The last resize still hasn't detected by the host thread. */
		rte_errno = EAGAIN;
		return NULL;
	}
	new_cont->pools = rte_calloc(__func__, 1, mem_size, 0);
	if (!new_cont->pools) {
		rte_errno = ENOMEM;
		return NULL;
	}
	if (cont->n)
		memcpy(new_cont->pools, cont->pools, cont->n *
		       sizeof(struct mlx5_flow_counter_pool *));
	mem_mng = flow_dv_create_counter_stat_mem_mng(dev,
		MLX5_CNT_CONTAINER_RESIZE + MLX5_MAX_PENDING_QUERIES);
	if (!mem_mng) {
		rte_free(new_cont->pools);
		return NULL;
	}
	for (i = 0; i < MLX5_MAX_PENDING_QUERIES; ++i)
		LIST_INSERT_HEAD(&priv->sh->cmng.free_stat_raws,
				 mem_mng->raws + MLX5_CNT_CONTAINER_RESIZE +
				 i, next);
	new_cont->n = resize;
	rte_atomic16_set(&new_cont->n_valid, rte_atomic16_read(&cont->n_valid));
	TAILQ_INIT(&new_cont->pool_list);
	TAILQ_CONCAT(&new_cont->pool_list, &cont->pool_list, next);
	new_cont->init_mem_mng = mem_mng;
	rte_cio_wmb();
	 /* Flip the master container. */
	priv->sh->cmng.mhi[batch] ^= (uint8_t)1;
	return new_cont;
}

/**
 * Query a devx flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] cnt
 *   Pointer to the flow counter.
 * @param[out] pkts
 *   The statistics value of packets.
 * @param[out] bytes
 *   The statistics value of bytes.
 *
 * @return
 *   0 on success, otherwise a negative errno value and rte_errno is set.
 */
static inline int
_flow_dv_query_count(struct rte_eth_dev *dev,
		     struct mlx5_flow_counter *cnt, uint64_t *pkts,
		     uint64_t *bytes)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool =
			flow_dv_counter_pool_get(cnt);
	int offset = cnt - &pool->counters_raw[0];

	if (priv->counter_fallback)
		return _flow_dv_query_count_fallback(dev, cnt, pkts, bytes);

	rte_spinlock_lock(&pool->sl);
	/*
	 * The single counters allocation may allocate smaller ID than the
	 * current allocated in parallel to the host reading.
	 * In this case the new counter values must be reported as 0.
	 */
	if (unlikely(!cnt->batch && cnt->dcs->id < pool->raw->min_dcs_id)) {
		*pkts = 0;
		*bytes = 0;
	} else {
		*pkts = rte_be_to_cpu_64(pool->raw->data[offset].hits);
		*bytes = rte_be_to_cpu_64(pool->raw->data[offset].bytes);
	}
	rte_spinlock_unlock(&pool->sl);
	return 0;
}

/**
 * Create and initialize a new counter pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] dcs
 *   The devX counter handle.
 * @param[in] batch
 *   Whether the pool is for counter that was allocated by batch command.
 *
 * @return
 *   A new pool pointer on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter_pool *
flow_dv_pool_create(struct rte_eth_dev *dev, struct mlx5_devx_obj *dcs,
		    uint32_t batch)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(priv->sh, batch,
							       0);
	int16_t n_valid = rte_atomic16_read(&cont->n_valid);
	uint32_t size;

	if (cont->n == n_valid) {
		cont = flow_dv_container_resize(dev, batch);
		if (!cont)
			return NULL;
	}
	size = sizeof(*pool) + MLX5_COUNTERS_PER_POOL *
			sizeof(struct mlx5_flow_counter);
	pool = rte_calloc(__func__, 1, size, 0);
	if (!pool) {
		rte_errno = ENOMEM;
		return NULL;
	}
	pool->min_dcs = dcs;
	pool->raw = cont->init_mem_mng->raws + n_valid %
						     MLX5_CNT_CONTAINER_RESIZE;
	pool->raw_hw = NULL;
	rte_spinlock_init(&pool->sl);
	/*
	 * The generation of the new allocated counters in this pool is 0, 2 in
	 * the pool generation makes all the counters valid for allocation.
	 */
	rte_atomic64_set(&pool->query_gen, 0x2);
	TAILQ_INIT(&pool->counters);
	TAILQ_INSERT_TAIL(&cont->pool_list, pool, next);
	cont->pools[n_valid] = pool;
	/* Pool initialization must be updated before host thread access. */
	rte_cio_wmb();
	rte_atomic16_add(&cont->n_valid, 1);
	return pool;
}

/**
 * Prepare a new counter and/or a new counter pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] cnt_free
 *   Where to put the pointer of a new counter.
 * @param[in] batch
 *   Whether the pool is for counter that was allocated by batch command.
 *
 * @return
 *   The free counter pool pointer and @p cnt_free is set on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter_pool *
flow_dv_counter_pool_prepare(struct rte_eth_dev *dev,
			     struct mlx5_flow_counter **cnt_free,
			     uint32_t batch)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_devx_obj *dcs = NULL;
	struct mlx5_flow_counter *cnt;
	uint32_t i;

	if (!batch) {
		/* bulk_bitmap must be 0 for single counter allocation. */
		dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->ctx, 0);
		if (!dcs)
			return NULL;
		pool = flow_dv_find_pool_by_id
			(MLX5_CNT_CONTAINER(priv->sh, batch, 0), dcs->id);
		if (!pool) {
			pool = flow_dv_pool_create(dev, dcs, batch);
			if (!pool) {
				mlx5_devx_cmd_destroy(dcs);
				return NULL;
			}
		} else if (dcs->id < pool->min_dcs->id) {
			rte_atomic64_set(&pool->a64_dcs,
					 (int64_t)(uintptr_t)dcs);
		}
		cnt = &pool->counters_raw[dcs->id % MLX5_COUNTERS_PER_POOL];
		TAILQ_INSERT_HEAD(&pool->counters, cnt, next);
		cnt->dcs = dcs;
		*cnt_free = cnt;
		return pool;
	}
	/* bulk_bitmap is in 128 counters units. */
	if (priv->config.hca_attr.flow_counter_bulk_alloc_bitmap & 0x4)
		dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->ctx, 0x4);
	if (!dcs) {
		rte_errno = ENODATA;
		return NULL;
	}
	pool = flow_dv_pool_create(dev, dcs, batch);
	if (!pool) {
		mlx5_devx_cmd_destroy(dcs);
		return NULL;
	}
	for (i = 0; i < MLX5_COUNTERS_PER_POOL; ++i) {
		cnt = &pool->counters_raw[i];
		cnt->pool = pool;
		TAILQ_INSERT_HEAD(&pool->counters, cnt, next);
	}
	*cnt_free = &pool->counters_raw[0];
	return pool;
}

/**
 * Search for existed shared counter.
 *
 * @param[in] cont
 *   Pointer to the relevant counter pool container.
 * @param[in] id
 *   The shared counter ID to search.
 *
 * @return
 *   NULL if not existed, otherwise pointer to the shared counter.
 */
static struct mlx5_flow_counter *
flow_dv_counter_shared_search(struct mlx5_pools_container *cont,
			      uint32_t id)
{
	static struct mlx5_flow_counter *cnt;
	struct mlx5_flow_counter_pool *pool;
	int i;

	TAILQ_FOREACH(pool, &cont->pool_list, next) {
		for (i = 0; i < MLX5_COUNTERS_PER_POOL; ++i) {
			cnt = &pool->counters_raw[i];
			if (cnt->ref_cnt && cnt->shared && cnt->id == id)
				return cnt;
		}
	}
	return NULL;
}

/**
 * Allocate a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] shared
 *   Indicate if this counter is shared with other flows.
 * @param[in] id
 *   Counter identifier.
 * @param[in] group
 *   Counter flow group.
 *
 * @return
 *   pointer to flow counter on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter *
flow_dv_counter_alloc(struct rte_eth_dev *dev, uint32_t shared, uint32_t id,
		      uint16_t group)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt_free = NULL;
	/*
	 * Currently group 0 flow counter cannot be assigned to a flow if it is
	 * not the first one in the batch counter allocation, so it is better
	 * to allocate counters one by one for these flows in a separate
	 * container.
	 * A counter can be shared between different groups so need to take
	 * shared counters from the single container.
	 */
	uint32_t batch = (group && !shared) ? 1 : 0;
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(priv->sh, batch,
							       0);

	if (priv->counter_fallback)
		return flow_dv_counter_alloc_fallback(dev, shared, id);
	if (!priv->config.devx) {
		rte_errno = ENOTSUP;
		return NULL;
	}
	if (shared) {
		cnt_free = flow_dv_counter_shared_search(cont, id);
		if (cnt_free) {
			if (cnt_free->ref_cnt + 1 == 0) {
				rte_errno = E2BIG;
				return NULL;
			}
			cnt_free->ref_cnt++;
			return cnt_free;
		}
	}
	/* Pools which has a free counters are in the start. */
	TAILQ_FOREACH(pool, &cont->pool_list, next) {
		/*
		 * The free counter reset values must be updated between the
		 * counter release to the counter allocation, so, at least one
		 * query must be done in this time. ensure it by saving the
		 * query generation in the release time.
		 * The free list is sorted according to the generation - so if
		 * the first one is not updated, all the others are not
		 * updated too.
		 */
		cnt_free = TAILQ_FIRST(&pool->counters);
		if (cnt_free && cnt_free->query_gen + 1 <
		    rte_atomic64_read(&pool->query_gen))
			break;
		cnt_free = NULL;
	}
	if (!cnt_free) {
		pool = flow_dv_counter_pool_prepare(dev, &cnt_free, batch);
		if (!pool)
			return NULL;
	}
	cnt_free->batch = batch;
	/* Create a DV counter action only in the first time usage. */
	if (!cnt_free->action) {
		uint16_t offset;
		struct mlx5_devx_obj *dcs;

		if (batch) {
			offset = cnt_free - &pool->counters_raw[0];
			dcs = pool->min_dcs;
		} else {
			offset = 0;
			dcs = cnt_free->dcs;
		}
		cnt_free->action = mlx5_glue->dv_create_flow_action_counter
					(dcs->obj, offset);
		if (!cnt_free->action) {
			rte_errno = errno;
			return NULL;
		}
	}
	/* Update the counter reset values. */
	if (_flow_dv_query_count(dev, cnt_free, &cnt_free->hits,
				 &cnt_free->bytes))
		return NULL;
	cnt_free->shared = shared;
	cnt_free->ref_cnt = 1;
	cnt_free->id = id;
	if (!priv->sh->cmng.query_thread_on)
		/* Start the asynchronous batch query by the host thread. */
		mlx5_set_query_alarm(priv->sh);
	TAILQ_REMOVE(&pool->counters, cnt_free, next);
	if (TAILQ_EMPTY(&pool->counters)) {
		/* Move the pool to the end of the container pool list. */
		TAILQ_REMOVE(&cont->pool_list, pool, next);
		TAILQ_INSERT_TAIL(&cont->pool_list, pool, next);
	}
	return cnt_free;
}

/**
 * Release a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Pointer to the counter handler.
 */
static void
flow_dv_counter_release(struct rte_eth_dev *dev,
			struct mlx5_flow_counter *counter)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!counter)
		return;
	if (priv->counter_fallback) {
		flow_dv_counter_release_fallback(dev, counter);
		return;
	}
	if (--counter->ref_cnt == 0) {
		struct mlx5_flow_counter_pool *pool =
				flow_dv_counter_pool_get(counter);

		/* Put the counter in the end - the last updated one. */
		TAILQ_INSERT_TAIL(&pool->counters, counter, next);
		counter->query_gen = rte_atomic64_read(&pool->query_gen);
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
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_attributes(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attributes,
			    bool external __rte_unused,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->config.flow_prio - 1;

#ifndef HAVE_MLX5DV_DR
	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups are not supported");
#else
	uint32_t max_group = attributes->transfer ? MLX5_MAX_TABLES_FDB :
						    MLX5_MAX_TABLES;
	uint32_t table;
	int ret;

	ret = mlx5_flow_group_to_table(attributes, external,
				       attributes->group,
				       &table, error);
	if (ret)
		return ret;
	if (table >= max_group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL,
					  "group index out of range");
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
				 NULL, "E-Switch configuration can only be"
				 " done by a master or a representor device");
		if (attributes->egress)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, attributes,
				 "egress is not supported");
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
 * @param[in] external
 *   This flow rule is created by request external to PMD.
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
		 bool external, struct rte_flow_error *error)
{
	int ret;
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint8_t next_protocol = 0xff;
	int actions_n = 0;
	const struct rte_flow_item *gre_item = NULL;
	struct rte_flow_item_tcp nic_tcp_mask = {
		.hdr = {
			.tcp_flags = 0xFF,
			.src_port = RTE_BE16(UINT16_MAX),
			.dst_port = RTE_BE16(UINT16_MAX),
		}
	};

	if (items == NULL)
		return -1;
	ret = flow_dv_validate_attributes(dev, attr, external, error);
	if (ret < 0)
		return ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int type = items->type;

		switch (type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			ret = flow_dv_validate_item_port_id
					(dev, items, attr, item_flags, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_PORT_ID;
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
							   dev, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					     MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
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
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
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
						 &nic_tcp_mask,
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
			ret = mlx5_flow_validate_item_gre(items, item_flags,
							  next_protocol, error);
			if (ret < 0)
				return ret;
			gre_item = items;
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			ret = mlx5_flow_validate_item_nvgre(items, item_flags,
							    next_protocol,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_NVGRE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			ret = mlx5_flow_validate_item_gre_key
				(items, item_flags, gre_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GRE_KEY;
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
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			ret = mlx5_flow_validate_item_geneve(items,
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
		case RTE_FLOW_ITEM_TYPE_ICMP:
			ret = mlx5_flow_validate_item_icmp(items, item_flags,
							   next_protocol,
							   error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			ret = mlx5_flow_validate_item_icmp6(items, item_flags,
							    next_protocol,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ICMP6;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		case MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
		item_flags |= last_item;
	}
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		int type = actions->type;
		if (actions_n == MLX5_DV_MAX_NUMBER_OF_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions, "too many actions");
		switch (type) {
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
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			if (flow_dv_validate_action_pop_vlan(dev,
							     action_flags,
							     actions,
							     item_flags, attr,
							     error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			ret = flow_dv_validate_action_push_vlan(action_flags,
								item_flags,
								actions, attr,
								error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			ret = flow_dv_validate_action_set_vlan_pcp
						(action_flags, actions, error);
			if (ret < 0)
				return ret;
			/* Count PCP with push_vlan command. */
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			ret = flow_dv_validate_action_set_vlan_vid
						(item_flags, action_flags,
						 actions, error);
			if (ret < 0)
				return ret;
			/* Count VID with push_vlan command. */
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
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
							   action_flags,
							   attr, external,
							   error);
			if (ret)
				return ret;
			++actions_n;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ:
			ret = flow_dv_validate_action_modify_tcp_seq
								(action_flags,
								 actions,
								 item_flags,
								 error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ ?
						MLX5_FLOW_ACTION_INC_TCP_SEQ :
						MLX5_FLOW_ACTION_DEC_TCP_SEQ;
			break;
		case RTE_FLOW_ACTION_TYPE_INC_TCP_ACK:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK:
			ret = flow_dv_validate_action_modify_tcp_ack
								(action_flags,
								 actions,
								 item_flags,
								 error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_ACK ?
						MLX5_FLOW_ACTION_INC_TCP_ACK :
						MLX5_FLOW_ACTION_DEC_TCP_ACK;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_TAG:
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	if ((action_flags & MLX5_FLOW_LAYER_TUNNEL) &&
	    (action_flags & MLX5_FLOW_VLAN_ACTIONS))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  actions,
					  "can't have vxlan and vlan"
					  " actions in the same rule");
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
	flow->dv.value.size = MLX5_ST_SZ_BYTES(fte_match_param);
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

	for (i = 0; i < MLX5_ST_SZ_BYTES(fte_match_param); ++i) {
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
 * @param[in, out] dev_flow
 *   Flow descriptor.
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
flow_dv_translate_item_vlan(struct mlx5_flow *dev_flow,
			    void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner)
{
	const struct rte_flow_item_vlan *vlan_m = item->mask;
	const struct rte_flow_item_vlan *vlan_v = item->spec;
	void *headers_m;
	void *headers_v;
	uint16_t tci_m;
	uint16_t tci_v;

	if (!vlan_v)
		return;
	if (!vlan_m)
		vlan_m = &rte_flow_item_vlan_mask;
	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
		/*
		 * This is workaround, masks are not supported,
		 * and pre-validated.
		 */
		dev_flow->dv.vf_vlan.tag =
			rte_be_to_cpu_16(vlan_v->tci) & 0x0fff;
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
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ethertype,
		 rte_be_to_cpu_16(vlan_m->inner_type));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ethertype,
		 rte_be_to_cpu_16(vlan_m->inner_type & vlan_v->inner_type));
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
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, tcp_flags,
		 tcp_m->hdr.tcp_flags);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_flags,
		 (tcp_v->hdr.tcp_flags & tcp_m->hdr.tcp_flags));
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
 * Add GRE optional Key item to matcher and to the value.
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
flow_dv_translate_item_gre_key(void *matcher, void *key,
				   const struct rte_flow_item *item)
{
	const rte_be32_t *key_m = item->mask;
	const rte_be32_t *key_v = item->spec;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	rte_be32_t gre_key_default_mask = RTE_BE32(UINT32_MAX);

	if (!key_v)
		return;
	if (!key_m)
		key_m = &gre_key_default_mask;
	/* GRE K bit must be on and should already be validated */
	MLX5_SET(fte_match_set_misc, misc_m, gre_k_present, 1);
	MLX5_SET(fte_match_set_misc, misc_v, gre_k_present, 1);
	MLX5_SET(fte_match_set_misc, misc_m, gre_key_h,
		 rte_be_to_cpu_32(*key_m) >> 8);
	MLX5_SET(fte_match_set_misc, misc_v, gre_key_h,
		 rte_be_to_cpu_32((*key_v) & (*key_m)) >> 8);
	MLX5_SET(fte_match_set_misc, misc_m, gre_key_l,
		 rte_be_to_cpu_32(*key_m) & 0xFF);
	MLX5_SET(fte_match_set_misc, misc_v, gre_key_l,
		 rte_be_to_cpu_32((*key_v) & (*key_m)) & 0xFF);
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
	struct {
		union {
			__extension__
			struct {
				uint16_t version:3;
				uint16_t rsvd0:9;
				uint16_t s_present:1;
				uint16_t k_present:1;
				uint16_t rsvd_bit1:1;
				uint16_t c_present:1;
			};
			uint16_t value;
		};
	} gre_crks_rsvd0_ver_m, gre_crks_rsvd0_ver_v;

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
	gre_crks_rsvd0_ver_m.value = rte_be_to_cpu_16(gre_m->c_rsvd0_ver);
	gre_crks_rsvd0_ver_v.value = rte_be_to_cpu_16(gre_v->c_rsvd0_ver);
	MLX5_SET(fte_match_set_misc, misc_m, gre_c_present,
		 gre_crks_rsvd0_ver_m.c_present);
	MLX5_SET(fte_match_set_misc, misc_v, gre_c_present,
		 gre_crks_rsvd0_ver_v.c_present &
		 gre_crks_rsvd0_ver_m.c_present);
	MLX5_SET(fte_match_set_misc, misc_m, gre_k_present,
		 gre_crks_rsvd0_ver_m.k_present);
	MLX5_SET(fte_match_set_misc, misc_v, gre_k_present,
		 gre_crks_rsvd0_ver_v.k_present &
		 gre_crks_rsvd0_ver_m.k_present);
	MLX5_SET(fte_match_set_misc, misc_m, gre_s_present,
		 gre_crks_rsvd0_ver_m.s_present);
	MLX5_SET(fte_match_set_misc, misc_v, gre_s_present,
		 gre_crks_rsvd0_ver_v.s_present &
		 gre_crks_rsvd0_ver_m.s_present);
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

	/* For NVGRE, GRE header fields must be set with defined values. */
	const struct rte_flow_item_gre gre_spec = {
		.c_rsvd0_ver = RTE_BE16(0x2000),
		.protocol = RTE_BE16(RTE_ETHER_TYPE_TEB)
	};
	const struct rte_flow_item_gre gre_mask = {
		.c_rsvd0_ver = RTE_BE16(0xB000),
		.protocol = RTE_BE16(UINT16_MAX),
	};
	const struct rte_flow_item gre_item = {
		.spec = &gre_spec,
		.mask = &gre_mask,
		.last = NULL,
	};
	flow_dv_translate_item_gre(matcher, key, &gre_item, inner);
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
 * Add Geneve item to matcher and to the value.
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
flow_dv_translate_item_geneve(void *matcher, void *key,
			      const struct rte_flow_item *item, int inner)
{
	const struct rte_flow_item_geneve *geneve_m = item->mask;
	const struct rte_flow_item_geneve *geneve_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc_m = MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	uint16_t dport;
	uint16_t gbhdr_m;
	uint16_t gbhdr_v;
	char *vni_m;
	char *vni_v;
	size_t size, i;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	dport = MLX5_UDP_PORT_GENEVE;
	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport, 0xFFFF);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport, dport);
	}
	if (!geneve_v)
		return;
	if (!geneve_m)
		geneve_m = &rte_flow_item_geneve_mask;
	size = sizeof(geneve_m->vni);
	vni_m = MLX5_ADDR_OF(fte_match_set_misc, misc_m, geneve_vni);
	vni_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, geneve_vni);
	memcpy(vni_m, geneve_m->vni, size);
	for (i = 0; i < size; ++i)
		vni_v[i] = vni_m[i] & geneve_v->vni[i];
	MLX5_SET(fte_match_set_misc, misc_m, geneve_protocol_type,
		 rte_be_to_cpu_16(geneve_m->protocol));
	MLX5_SET(fte_match_set_misc, misc_v, geneve_protocol_type,
		 rte_be_to_cpu_16(geneve_v->protocol & geneve_m->protocol));
	gbhdr_m = rte_be_to_cpu_16(geneve_m->ver_opt_len_o_c_rsvd0);
	gbhdr_v = rte_be_to_cpu_16(geneve_v->ver_opt_len_o_c_rsvd0);
	MLX5_SET(fte_match_set_misc, misc_m, geneve_oam,
		 MLX5_GENEVE_OAMF_VAL(gbhdr_m));
	MLX5_SET(fte_match_set_misc, misc_v, geneve_oam,
		 MLX5_GENEVE_OAMF_VAL(gbhdr_v) & MLX5_GENEVE_OAMF_VAL(gbhdr_m));
	MLX5_SET(fte_match_set_misc, misc_m, geneve_opt_len,
		 MLX5_GENEVE_OPTLEN_VAL(gbhdr_m));
	MLX5_SET(fte_match_set_misc, misc_v, geneve_opt_len,
		 MLX5_GENEVE_OPTLEN_VAL(gbhdr_v) &
		 MLX5_GENEVE_OPTLEN_VAL(gbhdr_m));
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
			 RTE_ETHER_TYPE_MPLS);
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
		MLX5_SET(fte_match_set_misc2, misc2_m,
			 metadata_reg_a, meta_m->data);
		MLX5_SET(fte_match_set_misc2, misc2_v,
			 metadata_reg_a, meta_v->data & meta_m->data);
	}
}

/**
 * Add vport metadata Reg C0 item to matcher
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] reg
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_item_meta_vport(void *matcher, void *key,
				  uint32_t value, uint32_t mask)
{
	void *misc2_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters_2);
	void *misc2_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);

	MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_0, mask);
	MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_0, value);
}

/**
 * Add tag item to matcher
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_item_tag(void *matcher, void *key,
			   const struct rte_flow_item *item)
{
	void *misc2_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters_2);
	void *misc2_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);
	const struct mlx5_rte_flow_item_tag *tag_v = item->spec;
	const struct mlx5_rte_flow_item_tag *tag_m = item->mask;
	enum modify_reg reg = tag_v->id;
	rte_be32_t value = tag_v->data;
	rte_be32_t mask = tag_m->data;

	switch (reg) {
	case REG_A:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_a,
				rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_a,
				rte_be_to_cpu_32(value));
		break;
	case REG_B:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_b,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_b,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_0:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_0,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_0,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_1:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_1,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_1,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_2:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_2,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_2,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_3:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_3,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_3,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_4:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_4,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_4,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_5:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_5,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_5,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_6:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_6,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_6,
				rte_be_to_cpu_32(value));
		break;
	case REG_C_7:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_7,
				 rte_be_to_cpu_32(mask));
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_7,
				rte_be_to_cpu_32(value));
		break;
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
	struct mlx5_priv *priv;
	uint16_t mask, id;

	mask = pid_m ? pid_m->id : 0xffff;
	id = pid_v ? pid_v->id : dev->data->port_id;
	priv = mlx5_port_to_eswitch_info(id);
	if (!priv)
		return -rte_errno;
	/* Translate to vport field or to metadata, depending on mode. */
	if (priv->vport_meta_mask)
		flow_dv_translate_item_meta_vport(matcher, key,
						  priv->vport_meta_tag,
						  priv->vport_meta_mask);
	else
		flow_dv_translate_item_source_vport(matcher, key,
						    priv->vport_id, mask);
	return 0;
}

/**
 * Add ICMP6 item to matcher and to the value.
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
flow_dv_translate_item_icmp6(void *matcher, void *key,
			      const struct rte_flow_item *item,
			      int inner)
{
	const struct rte_flow_item_icmp6 *icmp6_m = item->mask;
	const struct rte_flow_item_icmp6 *icmp6_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc3_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_3);
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xFF);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, IPPROTO_ICMPV6);
	if (!icmp6_v)
		return;
	if (!icmp6_m)
		icmp6_m = &rte_flow_item_icmp6_mask;
	MLX5_SET(fte_match_set_misc3, misc3_m, icmpv6_type, icmp6_m->type);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_type,
		 icmp6_v->type & icmp6_m->type);
	MLX5_SET(fte_match_set_misc3, misc3_m, icmpv6_code, icmp6_m->code);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_code,
		 icmp6_v->code & icmp6_m->code);
}

/**
 * Add ICMP item to matcher and to the value.
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
flow_dv_translate_item_icmp(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    int inner)
{
	const struct rte_flow_item_icmp *icmp_m = item->mask;
	const struct rte_flow_item_icmp *icmp_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc3_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_3);
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_protocol, 0xFF);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, IPPROTO_ICMP);
	if (!icmp_v)
		return;
	if (!icmp_m)
		icmp_m = &rte_flow_item_icmp_mask;
	MLX5_SET(fte_match_set_misc3, misc3_m, icmp_type,
		 icmp_m->hdr.icmp_type);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_type,
		 icmp_v->hdr.icmp_type & icmp_m->hdr.icmp_type);
	MLX5_SET(fte_match_set_misc3, misc3_m, icmp_code,
		 icmp_m->hdr.icmp_code);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_code,
		 icmp_v->hdr.icmp_code & icmp_m->hdr.icmp_code);
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
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_3)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC3_BIT;
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
	tbl = flow_dv_tbl_resource_get(dev, matcher->group,
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
	struct mlx5_priv *priv;
	const struct rte_flow_action_port_id *conf =
			(const struct rte_flow_action_port_id *)action->conf;

	port = conf->original ? dev->data->port_id : conf->id;
	priv = mlx5_port_to_eswitch_info(port);
	if (!priv)
		return rte_flow_error_set(error, -rte_errno,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "No eswitch info was found for port");
	if (priv->vport_meta_mask)
		*dst_port_id = priv->vport_meta_tag;
	else
		*dst_port_id = priv->vport_id;
	return 0;
}

/**
 * Add Tx queue matcher
 *
 * @param[in] dev
 *   Pointer to the dev struct.
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
flow_dv_translate_item_tx_queue(struct rte_eth_dev *dev,
				void *matcher, void *key,
				const struct rte_flow_item *item)
{
	const struct mlx5_rte_flow_item_tx_queue *queue_m;
	const struct mlx5_rte_flow_item_tx_queue *queue_v;
	void *misc_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters);
	void *misc_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	struct mlx5_txq_ctrl *txq;
	uint32_t queue;


	queue_m = (const void *)item->mask;
	if (!queue_m)
		return;
	queue_v = (const void *)item->spec;
	if (!queue_v)
		return;
	txq = mlx5_txq_get(dev, queue_v->queue);
	if (!txq)
		return;
	queue = txq->obj->sq->id;
	MLX5_SET(fte_match_set_misc, misc_m, source_sqn, queue_m->queue);
	MLX5_SET(fte_match_set_misc, misc_v, source_sqn,
		 queue & queue_m->queue);
	mlx5_txq_release(dev, queue_v->queue);
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
	uint8_t next_protocol = 0xff;
	struct rte_vlan_hdr vlan = { 0 };
	uint32_t table;
	int ret = 0;

	ret = mlx5_flow_group_to_table(attr, dev_flow->external, attr->group,
				       &table, error);
	if (ret)
		return ret;
	flow->group = table;
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
		int action_type = actions->type;
		const struct rte_flow_action *found_action = NULL;

		switch (action_type) {
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
			flow->counter = flow_dv_counter_alloc(dev,
							      count->shared,
							      count->id,
							      flow->group);
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
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			dev_flow->dv.actions[actions_n++] =
						priv->sh->pop_vlan_action;
			action_flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			flow_dev_get_vlan_info_from_items(items, &vlan);
			vlan.eth_proto = rte_be_to_cpu_16
			     ((((const struct rte_flow_action_of_push_vlan *)
						   actions->conf)->ethertype));
			found_action = mlx5_flow_find_action
					(actions + 1,
					 RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID);
			if (found_action)
				mlx5_update_vlan_vid_pcp(found_action, &vlan);
			found_action = mlx5_flow_find_action
					(actions + 1,
					 RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP);
			if (found_action)
				mlx5_update_vlan_vid_pcp(found_action, &vlan);
			if (flow_dv_create_action_push_vlan
					    (dev, attr, &vlan, dev_flow, error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
					   dev_flow->dv.push_vlan_res->action;
			action_flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			/* of_vlan_push action handled this action */
			assert(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN)
				break;
			flow_dev_get_vlan_info_from_items(items, &vlan);
			mlx5_update_vlan_vid_pcp(actions, &vlan);
			/* If no VLAN push - this is a modify header action */
			if (flow_dv_convert_action_modify_vlan_vid
							(&res, actions, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			break;
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
			ret = mlx5_flow_group_to_table(attr, dev_flow->external,
						       jump_data->group, &table,
						       error);
			if (ret)
				return ret;
			tbl = flow_dv_tbl_resource_get(dev, table,
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
		case RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ:
			if (flow_dv_convert_action_modify_tcp_seq(&res, actions,
								  error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ ?
					MLX5_FLOW_ACTION_INC_TCP_SEQ :
					MLX5_FLOW_ACTION_DEC_TCP_SEQ;
			break;

		case RTE_FLOW_ACTION_TYPE_INC_TCP_ACK:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK:
			if (flow_dv_convert_action_modify_tcp_ack(&res, actions,
								  error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_ACK ?
					MLX5_FLOW_ACTION_INC_TCP_ACK :
					MLX5_FLOW_ACTION_DEC_TCP_ACK;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_TAG:
			if (flow_dv_convert_action_set_reg(&res, actions,
							   error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TAG;
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
	dev_flow->actions = action_flags;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int item_type = items->type;

		switch (item_type) {
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
			flow_dv_translate_item_vlan(dev_flow,
						    match_mask, match_value,
						    items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L2;
			last_item = tunnel ? (MLX5_FLOW_LAYER_INNER_L2 |
					      MLX5_FLOW_LAYER_INNER_VLAN) :
					     (MLX5_FLOW_LAYER_OUTER_L2 |
					      MLX5_FLOW_LAYER_OUTER_VLAN);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
			flow_dv_translate_item_ipv4(match_mask, match_value,
						    items, tunnel, flow->group);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel,
					 MLX5_IPV4_LAYER_TYPES,
					 MLX5_IPV4_IBV_RX_HASH);
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
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
			flow_dv_translate_item_ipv6(match_mask, match_value,
						    items, tunnel, flow->group);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
			dev_flow->dv.hash_fields |=
				mlx5_flow_hashfields_adjust
					(dev_flow, tunnel,
					 MLX5_IPV6_LAYER_TYPES,
					 MLX5_IPV6_IBV_RX_HASH);
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
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			flow_dv_translate_item_gre_key(match_mask,
						       match_value, items);
			last_item = MLX5_FLOW_LAYER_GRE_KEY;
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
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			flow_dv_translate_item_geneve(match_mask, match_value,
						      items, tunnel);
			last_item = MLX5_FLOW_LAYER_GENEVE;
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
		case RTE_FLOW_ITEM_TYPE_ICMP:
			flow_dv_translate_item_icmp(match_mask, match_value,
						    items, tunnel);
			last_item = MLX5_FLOW_LAYER_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			flow_dv_translate_item_icmp6(match_mask, match_value,
						      items, tunnel);
			last_item = MLX5_FLOW_LAYER_ICMP6;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
			flow_dv_translate_item_tag(match_mask, match_value,
						   items);
			last_item = MLX5_FLOW_ITEM_TAG;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			flow_dv_translate_item_tx_queue(dev, match_mask,
							match_value,
							items);
			last_item = MLX5_FLOW_ITEM_TX_QUEUE;
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
	matcher.group = flow->group;
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
		if (dev_flow->actions & MLX5_FLOW_ACTION_DROP) {
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
		} else if (dev_flow->actions &
			   (MLX5_FLOW_ACTION_QUEUE | MLX5_FLOW_ACTION_RSS)) {
			struct mlx5_hrxq *hrxq;

			hrxq = mlx5_hrxq_get(dev, flow->key,
					     MLX5_RSS_HASH_KEY_LEN,
					     dv->hash_fields,
					     (*flow->queue),
					     flow->rss.queue_num);
			if (!hrxq) {
				hrxq = mlx5_hrxq_new
					(dev, flow->key, MLX5_RSS_HASH_KEY_LEN,
					 dv->hash_fields, (*flow->queue),
					 flow->rss.queue_num,
					 !!(dev_flow->layers &
					    MLX5_FLOW_LAYER_TUNNEL));
			}
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
		if (priv->vmwa_context &&
		    dev_flow->dv.vf_vlan.tag &&
		    !dev_flow->dv.vf_vlan.created) {
			/*
			 * The rule contains the VLAN pattern.
			 * For VF we are going to create VLAN
			 * interface to make hypervisor set correct
			 * e-Switch vport context.
			 */
			mlx5_vlan_vmwa_acquire(dev, &dev_flow->dv.vf_vlan);
		}
	}
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	LIST_FOREACH(dev_flow, &flow->dev_flows, next) {
		struct mlx5_flow_dv *dv = &dev_flow->dv;
		if (dv->hrxq) {
			if (dev_flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, dv->hrxq);
			dv->hrxq = NULL;
		}
		if (dev_flow->dv.vf_vlan.tag &&
		    dev_flow->dv.vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &dev_flow->dv.vf_vlan);
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
 * Release push vlan action resource.
 *
 * @param flow
 *   Pointer to mlx5_flow.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_push_vlan_action_resource_release(struct mlx5_flow *flow)
{
	struct mlx5_flow_dv_push_vlan_action_resource *cache_resource =
		flow->dv.push_vlan_res;

	assert(cache_resource->action);
	DRV_LOG(DEBUG, "push VLAN action resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->action));
		LIST_REMOVE(cache_resource, next);
		rte_free(cache_resource);
		DRV_LOG(DEBUG, "push vlan action resource %p: removed",
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
			if (dev_flow->actions & MLX5_FLOW_ACTION_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, dv->hrxq);
			dv->hrxq = NULL;
		}
		if (dev_flow->dv.vf_vlan.tag &&
		    dev_flow->dv.vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &dev_flow->dv.vf_vlan);
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
		flow_dv_counter_release(dev, flow->counter);
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
		if (dev_flow->dv.push_vlan_res)
			flow_dv_push_vlan_action_resource_release(dev_flow);
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

	if (!priv->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "counters are not supported");
	if (flow->counter) {
		uint64_t pkts, bytes;
		int err = _flow_dv_query_count(dev, flow->counter, &pkts,
					       &bytes);

		if (err)
			return rte_flow_error_set(error, -err,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "cannot read counters");
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
