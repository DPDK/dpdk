/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>
#include <stdalign.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <rte_ip.h>
#include <rte_gre.h>
#include <rte_vxlan.h>
#include <rte_gtp.h>
#include <rte_eal_paging.h>
#include <rte_mpls.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"
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

static int
flow_dv_tbl_resource_release(struct rte_eth_dev *dev,
			     struct mlx5_flow_tbl_resource *tbl);

static int
flow_dv_default_miss_resource_release(struct rte_eth_dev *dev);

/**
 * Initialize flow attributes structure according to flow items' types.
 *
 * flow_dv_validate() avoids multiple L3/L4 layers cases other than tunnel
 * mode. For tunnel mode, the items to be modified are the outermost ones.
 *
 * @param[in] item
 *   Pointer to item specification.
 * @param[out] attr
 *   Pointer to flow attributes structure.
 * @param[in] dev_flow
 *   Pointer to the sub flow.
 * @param[in] tunnel_decap
 *   Whether action is after tunnel decapsulation.
 */
static void
flow_dv_attr_init(const struct rte_flow_item *item, union flow_dv_attr *attr,
		  struct mlx5_flow *dev_flow, bool tunnel_decap)
{
	uint64_t layers = dev_flow->handle->layers;

	/*
	 * If layers is already initialized, it means this dev_flow is the
	 * suffix flow, the layers flags is set by the prefix flow. Need to
	 * use the layer flags from prefix flow as the suffix flow may not
	 * have the user defined items as the flow is split.
	 */
	if (layers) {
		if (layers & MLX5_FLOW_LAYER_OUTER_L3_IPV4)
			attr->ipv4 = 1;
		else if (layers & MLX5_FLOW_LAYER_OUTER_L3_IPV6)
			attr->ipv6 = 1;
		if (layers & MLX5_FLOW_LAYER_OUTER_L4_TCP)
			attr->tcp = 1;
		else if (layers & MLX5_FLOW_LAYER_OUTER_L4_UDP)
			attr->udp = 1;
		attr->valid = 1;
		return;
	}
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		uint8_t next_protocol = 0xff;
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_GRE:
		case RTE_FLOW_ITEM_TYPE_NVGRE:
		case RTE_FLOW_ITEM_TYPE_VXLAN:
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		case RTE_FLOW_ITEM_TYPE_GENEVE:
		case RTE_FLOW_ITEM_TYPE_MPLS:
			if (tunnel_decap)
				attr->attr = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			if (!attr->ipv6)
				attr->ipv4 = 1;
			if (item->mask != NULL &&
			    ((const struct rte_flow_item_ipv4 *)
			    item->mask)->hdr.next_proto_id)
				next_protocol =
				    ((const struct rte_flow_item_ipv4 *)
				      (item->spec))->hdr.next_proto_id &
				    ((const struct rte_flow_item_ipv4 *)
				      (item->mask))->hdr.next_proto_id;
			if ((next_protocol == IPPROTO_IPIP ||
			    next_protocol == IPPROTO_IPV6) && tunnel_decap)
				attr->attr = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			if (!attr->ipv4)
				attr->ipv6 = 1;
			if (item->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			    item->mask)->hdr.proto)
				next_protocol =
				    ((const struct rte_flow_item_ipv6 *)
				      (item->spec))->hdr.proto &
				    ((const struct rte_flow_item_ipv6 *)
				      (item->mask))->hdr.proto;
			if ((next_protocol == IPPROTO_IPIP ||
			    next_protocol == IPPROTO_IPV6) && tunnel_decap)
				attr->attr = 0;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			if (!attr->tcp)
				attr->udp = 1;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			if (!attr->udp)
				attr->tcp = 1;
			break;
		default:
			break;
		}
	}
	attr->valid = 1;
}

/**
 * Convert rte_mtr_color to mlx5 color.
 *
 * @param[in] rcol
 *   rte_mtr_color.
 *
 * @return
 *   mlx5 color.
 */
static int
rte_col_2_mlx5_col(enum rte_color rcol)
{
	switch (rcol) {
	case RTE_COLOR_GREEN:
		return MLX5_FLOW_COLOR_GREEN;
	case RTE_COLOR_YELLOW:
		return MLX5_FLOW_COLOR_YELLOW;
	case RTE_COLOR_RED:
		return MLX5_FLOW_COLOR_RED;
	default:
		break;
	}
	return MLX5_FLOW_COLOR_UNDEFINED;
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
	{1,  1, MLX5_MODI_OUT_IP_DSCP},
	{1,  8, MLX5_MODI_OUT_IPV4_TTL},
	{4, 12, MLX5_MODI_OUT_SIPV4},
	{4, 16, MLX5_MODI_OUT_DIPV4},
	{0, 0, 0},
};

struct field_modify_info modify_ipv6[] = {
	{1,  0, MLX5_MODI_OUT_IP_DSCP},
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
	MLX5_ASSERT(item->type == RTE_FLOW_ITEM_TYPE_IPV4 ||
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
flow_dv_shared_lock(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;

	if (sh->dv_refcnt > 1) {
		int ret;

		ret = pthread_mutex_lock(&sh->dv_mutex);
		MLX5_ASSERT(!ret);
		(void)ret;
	}
}

static void
flow_dv_shared_unlock(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;

	if (sh->dv_refcnt > 1) {
		int ret;

		ret = pthread_mutex_unlock(&sh->dv_mutex);
		MLX5_ASSERT(!ret);
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
 * Fetch 1, 2, 3 or 4 byte field from the byte array
 * and return as unsigned integer in host-endian format.
 *
 * @param[in] data
 *   Pointer to data array.
 * @param[in] size
 *   Size of field to extract.
 *
 * @return
 *   converted field in host endian format.
 */
static inline uint32_t
flow_dv_fetch_field(const uint8_t *data, uint32_t size)
{
	uint32_t ret;

	switch (size) {
	case 1:
		ret = *data;
		break;
	case 2:
		ret = rte_be_to_cpu_16(*(const unaligned_uint16_t *)data);
		break;
	case 3:
		ret = rte_be_to_cpu_16(*(const unaligned_uint16_t *)data);
		ret = (ret << 8) | *(data + sizeof(uint16_t));
		break;
	case 4:
		ret = rte_be_to_cpu_32(*(const unaligned_uint32_t *)data);
		break;
	default:
		MLX5_ASSERT(false);
		ret = 0;
		break;
	}
	return ret;
}

/**
 * Convert modify-header action to DV specification.
 *
 * Data length of each action is determined by provided field description
 * and the item mask. Data bit offset and width of each action is determined
 * by provided item mask.
 *
 * @param[in] item
 *   Pointer to item specification.
 * @param[in] field
 *   Pointer to field modification information.
 *     For MLX5_MODIFICATION_TYPE_SET specifies destination field.
 *     For MLX5_MODIFICATION_TYPE_ADD specifies destination field.
 *     For MLX5_MODIFICATION_TYPE_COPY specifies source field.
 * @param[in] dcopy
 *   Destination field info for MLX5_MODIFICATION_TYPE_COPY in @type.
 *   Negative offset value sets the same offset as source offset.
 *   size field is ignored, value is taken from source field.
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
			      struct field_modify_info *dcopy,
			      struct mlx5_flow_dv_modify_hdr_resource *resource,
			      uint32_t type, struct rte_flow_error *error)
{
	uint32_t i = resource->actions_num;
	struct mlx5_modification_cmd *actions = resource->actions;

	/*
	 * The item and mask are provided in big-endian format.
	 * The fields should be presented as in big-endian format either.
	 * Mask must be always present, it defines the actual field width.
	 */
	MLX5_ASSERT(item->mask);
	MLX5_ASSERT(field->size);
	do {
		unsigned int size_b;
		unsigned int off_b;
		uint32_t mask;
		uint32_t data;

		if (i >= MLX5_MAX_MODIFY_NUM)
			return rte_flow_error_set(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				 "too many items to modify");
		/* Fetch variable byte size mask from the array. */
		mask = flow_dv_fetch_field((const uint8_t *)item->mask +
					   field->offset, field->size);
		if (!mask) {
			++field;
			continue;
		}
		/* Deduce actual data width in bits from mask value. */
		off_b = rte_bsf32(mask);
		size_b = sizeof(uint32_t) * CHAR_BIT -
			 off_b - __builtin_clz(mask);
		MLX5_ASSERT(size_b);
		size_b = size_b == sizeof(uint32_t) * CHAR_BIT ? 0 : size_b;
		actions[i] = (struct mlx5_modification_cmd) {
			.action_type = type,
			.field = field->id,
			.offset = off_b,
			.length = size_b,
		};
		/* Convert entire record to expected big-endian format. */
		actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
		if (type == MLX5_MODIFICATION_TYPE_COPY) {
			MLX5_ASSERT(dcopy);
			actions[i].dst_field = dcopy->id;
			actions[i].dst_offset =
				(int)dcopy->offset < 0 ? off_b : dcopy->offset;
			/* Convert entire record to big-endian format. */
			actions[i].data1 = rte_cpu_to_be_32(actions[i].data1);
		} else {
			MLX5_ASSERT(item->spec);
			data = flow_dv_fetch_field((const uint8_t *)item->spec +
						   field->offset, field->size);
			/* Shift out the trailing masked bits from data. */
			data = (data & mask) >> off_b;
			actions[i].data1 = rte_cpu_to_be_32(data);
		}
		++i;
		++field;
	} while (field->size);
	if (resource->actions_num == i)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "invalid modification flow item");
	resource->actions_num = i;
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
	return flow_dv_convert_modify_action(&item, modify_ipv4, NULL, resource,
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
	return flow_dv_convert_modify_action(&item, modify_ipv6, NULL, resource,
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
	return flow_dv_convert_modify_action(&item, modify_eth, NULL, resource,
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
	struct mlx5_modification_cmd *actions = resource->actions;
	struct field_modify_info *field = modify_vlan_out_first_vid;

	if (i >= MLX5_MAX_MODIFY_NUM)
		return rte_flow_error_set(error, EINVAL,
			 RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			 "too many items to modify");
	actions[i] = (struct mlx5_modification_cmd) {
		.action_type = MLX5_MODIFICATION_TYPE_SET,
		.field = field->id,
		.length = field->size,
		.offset = field->offset,
	};
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
 * @param[in] dev_flow
 *   Pointer to the sub flow.
 * @param[in] tunnel_decap
 *   Whether action is after tunnel decapsulation.
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
			 union flow_dv_attr *attr, struct mlx5_flow *dev_flow,
			 bool tunnel_decap, struct rte_flow_error *error)
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
		flow_dv_attr_init(items, attr, dev_flow, tunnel_decap);
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
	} else {
		MLX5_ASSERT(attr->tcp);
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
	return flow_dv_convert_modify_action(&item, field, NULL, resource,
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
 * @param[in] dev_flow
 *   Pointer to the sub flow.
 * @param[in] tunnel_decap
 *   Whether action is after tunnel decapsulation.
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
			 union flow_dv_attr *attr, struct mlx5_flow *dev_flow,
			 bool tunnel_decap, struct rte_flow_error *error)
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
		flow_dv_attr_init(items, attr, dev_flow, tunnel_decap);
	if (attr->ipv4) {
		memset(&ipv4, 0, sizeof(ipv4));
		memset(&ipv4_mask, 0, sizeof(ipv4_mask));
		ipv4.hdr.time_to_live = conf->ttl_value;
		ipv4_mask.hdr.time_to_live = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV4;
		item.spec = &ipv4;
		item.mask = &ipv4_mask;
		field = modify_ipv4;
	} else {
		MLX5_ASSERT(attr->ipv6);
		memset(&ipv6, 0, sizeof(ipv6));
		memset(&ipv6_mask, 0, sizeof(ipv6_mask));
		ipv6.hdr.hop_limits = conf->ttl_value;
		ipv6_mask.hdr.hop_limits = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV6;
		item.spec = &ipv6;
		item.mask = &ipv6_mask;
		field = modify_ipv6;
	}
	return flow_dv_convert_modify_action(&item, field, NULL, resource,
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
 * @param[in] dev_flow
 *   Pointer to the sub flow.
 * @param[in] tunnel_decap
 *   Whether action is after tunnel decapsulation.
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
			 union flow_dv_attr *attr, struct mlx5_flow *dev_flow,
			 bool tunnel_decap, struct rte_flow_error *error)
{
	struct rte_flow_item item;
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv4 ipv4_mask;
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_ipv6 ipv6_mask;
	struct field_modify_info *field;

	if (!attr->valid)
		flow_dv_attr_init(items, attr, dev_flow, tunnel_decap);
	if (attr->ipv4) {
		memset(&ipv4, 0, sizeof(ipv4));
		memset(&ipv4_mask, 0, sizeof(ipv4_mask));
		ipv4.hdr.time_to_live = 0xFF;
		ipv4_mask.hdr.time_to_live = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV4;
		item.spec = &ipv4;
		item.mask = &ipv4_mask;
		field = modify_ipv4;
	} else {
		MLX5_ASSERT(attr->ipv6);
		memset(&ipv6, 0, sizeof(ipv6));
		memset(&ipv6_mask, 0, sizeof(ipv6_mask));
		ipv6.hdr.hop_limits = 0xFF;
		ipv6_mask.hdr.hop_limits = 0xFF;
		item.type = RTE_FLOW_ITEM_TYPE_IPV6;
		item.spec = &ipv6;
		item.mask = &ipv6_mask;
		field = modify_ipv6;
	}
	return flow_dv_convert_modify_action(&item, field, NULL, resource,
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
	return flow_dv_convert_modify_action(&item, modify_tcp, NULL, resource,
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
	return flow_dv_convert_modify_action(&item, modify_tcp, NULL, resource,
					     MLX5_MODIFICATION_TYPE_ADD, error);
}

static enum mlx5_modification_field reg_to_field[] = {
	[REG_NONE] = MLX5_MODI_OUT_NONE,
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
	const struct mlx5_rte_flow_action_set_tag *conf = action->conf;
	struct mlx5_modification_cmd *actions = resource->actions;
	uint32_t i = resource->actions_num;

	if (i >= MLX5_MAX_MODIFY_NUM)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "too many items to modify");
	MLX5_ASSERT(conf->id != REG_NONE);
	MLX5_ASSERT(conf->id < RTE_DIM(reg_to_field));
	actions[i] = (struct mlx5_modification_cmd) {
		.action_type = MLX5_MODIFICATION_TYPE_SET,
		.field = reg_to_field[conf->id],
	};
	actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
	actions[i].data1 = rte_cpu_to_be_32(conf->data);
	++i;
	resource->actions_num = i;
	return 0;
}

/**
 * Convert SET_TAG action to DV specification.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] conf
 *   Pointer to action specification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_set_tag
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action_set_tag *conf,
			 struct rte_flow_error *error)
{
	rte_be32_t data = rte_cpu_to_be_32(conf->data);
	rte_be32_t mask = rte_cpu_to_be_32(conf->mask);
	struct rte_flow_item item = {
		.spec = &data,
		.mask = &mask,
	};
	struct field_modify_info reg_c_x[] = {
		[1] = {0, 0, 0},
	};
	enum mlx5_modification_field reg_type;
	int ret;

	ret = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, conf->index, error);
	if (ret < 0)
		return ret;
	MLX5_ASSERT(ret != REG_NONE);
	MLX5_ASSERT((unsigned int)ret < RTE_DIM(reg_to_field));
	reg_type = reg_to_field[ret];
	MLX5_ASSERT(reg_type > 0);
	reg_c_x[0] = (struct field_modify_info){4, 0, reg_type};
	return flow_dv_convert_modify_action(&item, reg_c_x, NULL, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert internal COPY_REG action to DV specification.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in,out] res
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
flow_dv_convert_action_copy_mreg(struct rte_eth_dev *dev,
				 struct mlx5_flow_dv_modify_hdr_resource *res,
				 const struct rte_flow_action *action,
				 struct rte_flow_error *error)
{
	const struct mlx5_flow_action_copy_mreg *conf = action->conf;
	rte_be32_t mask = RTE_BE32(UINT32_MAX);
	struct rte_flow_item item = {
		.spec = NULL,
		.mask = &mask,
	};
	struct field_modify_info reg_src[] = {
		{4, 0, reg_to_field[conf->src]},
		{0, 0, 0},
	};
	struct field_modify_info reg_dst = {
		.offset = 0,
		.id = reg_to_field[conf->dst],
	};
	/* Adjust reg_c[0] usage according to reported mask. */
	if (conf->dst == REG_C_0 || conf->src == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t reg_c0 = priv->sh->dv_regc0_mask;

		MLX5_ASSERT(reg_c0);
		MLX5_ASSERT(priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY);
		if (conf->dst == REG_C_0) {
			/* Copy to reg_c[0], within mask only. */
			reg_dst.offset = rte_bsf32(reg_c0);
			/*
			 * Mask is ignoring the enianness, because
			 * there is no conversion in datapath.
			 */
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			/* Copy from destination lower bits to reg_c[0]. */
			mask = reg_c0 >> reg_dst.offset;
#else
			/* Copy from destination upper bits to reg_c[0]. */
			mask = reg_c0 << (sizeof(reg_c0) * CHAR_BIT -
					  rte_fls_u32(reg_c0));
#endif
		} else {
			mask = rte_cpu_to_be_32(reg_c0);
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
			/* Copy from reg_c[0] to destination lower bits. */
			reg_dst.offset = 0;
#else
			/* Copy from reg_c[0] to destination upper bits. */
			reg_dst.offset = sizeof(reg_c0) * CHAR_BIT -
					 (rte_fls_u32(reg_c0) -
					  rte_bsf32(reg_c0));
#endif
		}
	}
	return flow_dv_convert_modify_action(&item,
					     reg_src, &reg_dst, res,
					     MLX5_MODIFICATION_TYPE_COPY,
					     error);
}

/**
 * Convert MARK action to DV specification. This routine is used
 * in extensive metadata only and requires metadata register to be
 * handled. In legacy mode hardware tag resource is engaged.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] conf
 *   Pointer to MARK action specification.
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_mark(struct rte_eth_dev *dev,
			    const struct rte_flow_action_mark *conf,
			    struct mlx5_flow_dv_modify_hdr_resource *resource,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	rte_be32_t mask = rte_cpu_to_be_32(MLX5_FLOW_MARK_MASK &
					   priv->sh->dv_mark_mask);
	rte_be32_t data = rte_cpu_to_be_32(conf->id) & mask;
	struct rte_flow_item item = {
		.spec = &data,
		.mask = &mask,
	};
	struct field_modify_info reg_c_x[] = {
		[1] = {0, 0, 0},
	};
	int reg;

	if (!mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "zero mark action mask");
	reg = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (reg < 0)
		return reg;
	MLX5_ASSERT(reg > 0);
	if (reg == REG_C_0) {
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0 = rte_bsf32(msk_c0);

		data = rte_cpu_to_be_32(rte_cpu_to_be_32(data) << shl_c0);
		mask = rte_cpu_to_be_32(mask) & msk_c0;
		mask = rte_cpu_to_be_32(mask << shl_c0);
	}
	reg_c_x[0] = (struct field_modify_info){4, 0, reg_to_field[reg]};
	return flow_dv_convert_modify_action(&item, reg_c_x, NULL, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Get metadata register index for specified steering domain.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Attributes of flow to determine steering domain.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   positive index on success, a negative errno value otherwise
 *   and rte_errno is set.
 */
static enum modify_reg
flow_dv_get_metadata_reg(struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 struct rte_flow_error *error)
{
	int reg =
		mlx5_flow_get_reg_id(dev, attr->transfer ?
					  MLX5_METADATA_FDB :
					    attr->egress ?
					    MLX5_METADATA_TX :
					    MLX5_METADATA_RX, 0, error);
	if (reg < 0)
		return rte_flow_error_set(error,
					  ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "unavailable "
					  "metadata register");
	return reg;
}

/**
 * Convert SET_META action to DV specification.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[in] conf
 *   Pointer to action specification.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_set_meta
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_action_set_meta *conf,
			 struct rte_flow_error *error)
{
	uint32_t data = conf->data;
	uint32_t mask = conf->mask;
	struct rte_flow_item item = {
		.spec = &data,
		.mask = &mask,
	};
	struct field_modify_info reg_c_x[] = {
		[1] = {0, 0, 0},
	};
	int reg = flow_dv_get_metadata_reg(dev, attr, error);

	if (reg < 0)
		return reg;
	/*
	 * In datapath code there is no endianness
	 * coversions for perfromance reasons, all
	 * pattern conversions are done in rte_flow.
	 */
	if (reg == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0;

		MLX5_ASSERT(msk_c0);
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		shl_c0 = rte_bsf32(msk_c0);
#else
		shl_c0 = sizeof(msk_c0) * CHAR_BIT - rte_fls_u32(msk_c0);
#endif
		mask <<= shl_c0;
		data <<= shl_c0;
		MLX5_ASSERT(!(~msk_c0 & rte_cpu_to_be_32(mask)));
	}
	reg_c_x[0] = (struct field_modify_info){4, 0, reg_to_field[reg]};
	/* The routine expects parameters in memory as big-endian ones. */
	return flow_dv_convert_modify_action(&item, reg_c_x, NULL, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set IPv4 DSCP action to DV specification.
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
flow_dv_convert_action_modify_ipv4_dscp
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_dscp *conf =
		(const struct rte_flow_action_set_dscp *)(action->conf);
	struct rte_flow_item item = { .type = RTE_FLOW_ITEM_TYPE_IPV4 };
	struct rte_flow_item_ipv4 ipv4;
	struct rte_flow_item_ipv4 ipv4_mask;

	memset(&ipv4, 0, sizeof(ipv4));
	memset(&ipv4_mask, 0, sizeof(ipv4_mask));
	ipv4.hdr.type_of_service = conf->dscp;
	ipv4_mask.hdr.type_of_service = RTE_IPV4_HDR_DSCP_MASK >> 2;
	item.spec = &ipv4;
	item.mask = &ipv4_mask;
	return flow_dv_convert_modify_action(&item, modify_ipv4, NULL, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Convert modify-header set IPv6 DSCP action to DV specification.
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
flow_dv_convert_action_modify_ipv6_dscp
			(struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_dscp *conf =
		(const struct rte_flow_action_set_dscp *)(action->conf);
	struct rte_flow_item item = { .type = RTE_FLOW_ITEM_TYPE_IPV6 };
	struct rte_flow_item_ipv6 ipv6;
	struct rte_flow_item_ipv6 ipv6_mask;

	memset(&ipv6, 0, sizeof(ipv6));
	memset(&ipv6_mask, 0, sizeof(ipv6_mask));
	/*
	 * Even though the DSCP bits offset of IPv6 is not byte aligned,
	 * rdma-core only accept the DSCP bits byte aligned start from
	 * bit 0 to 5 as to be compatible with IPv4. No need to shift the
	 * bits in IPv6 case as rdma-core requires byte aligned value.
	 */
	ipv6.hdr.vtc_flow = conf->dscp;
	ipv6_mask.hdr.vtc_flow = RTE_IPV6_HDR_DSCP_MASK >> 22;
	item.spec = &ipv6;
	item.mask = &ipv6_mask;
	return flow_dv_convert_modify_action(&item, modify_ipv6, NULL, resource,
					     MLX5_MODIFICATION_TYPE_SET, error);
}

/**
 * Validate MARK item.
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
flow_dv_validate_item_mark(struct rte_eth_dev *dev,
			   const struct rte_flow_item *item,
			   const struct rte_flow_attr *attr __rte_unused,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	const struct rte_flow_item_mark *spec = item->spec;
	const struct rte_flow_item_mark *mask = item->mask;
	const struct rte_flow_item_mark nic_mask = {
		.id = priv->sh->dv_mark_mask,
	};
	int ret;

	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "extended metadata feature"
					  " isn't enabled");
	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "extended metadata register"
					  " isn't supported");
	if (!nic_mask.id)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "extended metadata register"
					  " isn't available");
	ret = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (ret < 0)
		return ret;
	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (spec->id >= (MLX5_FLOW_MARK_MAX & nic_mask.id))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &spec->id,
					  "mark id exceeds the limit");
	if (!mask)
		mask = &nic_mask;
	if (!mask->id)
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					"mask cannot be zero");

	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_mark),
					error);
	if (ret < 0)
		return ret;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	const struct rte_flow_item_meta *spec = item->spec;
	const struct rte_flow_item_meta *mask = item->mask;
	struct rte_flow_item_meta nic_mask = {
		.data = UINT32_MAX
	};
	int reg;
	int ret;

	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (config->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
		if (!mlx5_flow_ext_mreg_supported(dev))
			return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "extended metadata register"
					  " isn't supported");
		reg = flow_dv_get_metadata_reg(dev, attr, error);
		if (reg < 0)
			return reg;
		if (reg == REG_B)
			return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "match on reg_b "
					  "isn't supported");
		if (reg != REG_A)
			nic_mask.data = priv->sh->dv_meta_mask;
	} else if (attr->transfer) {
		return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"extended metadata feature "
					"should be enabled when "
					"meta item is requested "
					"with e-switch mode ");
	}
	if (!mask)
		mask = &rte_flow_item_meta_mask;
	if (!mask->data)
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					"mask cannot be zero");

	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_meta),
					error);
	return ret;
}

/**
 * Validate TAG item.
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
flow_dv_validate_item_tag(struct rte_eth_dev *dev,
			  const struct rte_flow_item *item,
			  const struct rte_flow_attr *attr __rte_unused,
			  struct rte_flow_error *error)
{
	const struct rte_flow_item_tag *spec = item->spec;
	const struct rte_flow_item_tag *mask = item->mask;
	const struct rte_flow_item_tag nic_mask = {
		.data = RTE_BE32(UINT32_MAX),
		.index = 0xff,
	};
	int ret;

	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "extensive metadata register"
					  " isn't supported");
	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (!mask)
		mask = &rte_flow_item_tag_mask;
	if (!mask->data)
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					"mask cannot be zero");

	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_tag),
					error);
	if (ret < 0)
		return ret;
	if (mask->index != 0xff)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					  "partial mask for tag index"
					  " is not supported");
	ret = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, spec->index, error);
	if (ret < 0)
		return ret;
	MLX5_ASSERT(ret != REG_NONE);
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
	esw_priv = mlx5_port_to_eswitch_info(spec->id, false);
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
 * Validate VLAN item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] dev
 *   Ethernet device flow is being created on.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_vlan(const struct rte_flow_item *item,
			   uint64_t item_flags,
			   struct rte_eth_dev *dev,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *mask = item->mask;
	const struct rte_flow_item_vlan nic_mask = {
		.tci = RTE_BE16(UINT16_MAX),
		.inner_type = RTE_BE16(UINT16_MAX),
	};
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	int ret;
	const uint64_t l34m = tunnel ? (MLX5_FLOW_LAYER_INNER_L3 |
					MLX5_FLOW_LAYER_INNER_L4) :
				       (MLX5_FLOW_LAYER_OUTER_L3 |
					MLX5_FLOW_LAYER_OUTER_L4);
	const uint64_t vlanm = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					MLX5_FLOW_LAYER_OUTER_VLAN;

	if (item_flags & vlanm)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple VLAN layers not supported");
	else if ((item_flags & l34m) != 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VLAN cannot follow L3/L4 layer");
	if (!mask)
		mask = &rte_flow_item_vlan_mask;
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_vlan),
					error);
	if (ret)
		return ret;
	if (!tunnel && mask->tci != RTE_BE16(0x0fff)) {
		struct mlx5_priv *priv = dev->data->dev_private;

		if (priv->vmwa_context) {
			/*
			 * Non-NULL context means we have a virtual machine
			 * and SR-IOV enabled, we have to create VLAN interface
			 * to make hypervisor to setup E-Switch vport
			 * context correctly. We avoid creating the multiple
			 * VLAN interfaces, so we cannot support VLAN tag mask.
			 */
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "VLAN tag mask is not"
						  " supported in virtual"
						  " environment");
		}
	}
	return 0;
}

/*
 * GTP flags are contained in 1 byte of the format:
 * -------------------------------------------
 * | bit   | 0 - 2   | 3  | 4   | 5 | 6 | 7  |
 * |-----------------------------------------|
 * | value | Version | PT | Res | E | S | PN |
 * -------------------------------------------
 *
 * Matching is supported only for GTP flags E, S, PN.
 */
#define MLX5_GTP_FLAGS_MASK	0x07

/**
 * Validate GTP item.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_gtp(struct rte_eth_dev *dev,
			  const struct rte_flow_item *item,
			  uint64_t item_flags,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_gtp *spec = item->spec;
	const struct rte_flow_item_gtp *mask = item->mask;
	const struct rte_flow_item_gtp nic_mask = {
		.v_pt_rsv_flags = MLX5_GTP_FLAGS_MASK,
		.msg_type = 0xff,
		.teid = RTE_BE32(0xffffffff),
	};

	if (!priv->config.hca_attr.tunnel_stateless_gtp)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "GTP support is not enabled");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_gtp_mask;
	if (spec && spec->v_pt_rsv_flags & ~MLX5_GTP_FLAGS_MASK)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Match is supported for GTP"
					  " flags only");
	return mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_gtp),
		 error);
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
	const struct mlx5_priv *priv = dev->data->dev_private;

	(void)action;
	(void)attr;
	if (!priv->sh->pop_vlan_action)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "pop vlan action is not supported");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					  NULL,
					  "pop vlan action not supported for "
					  "egress");
	if (action_flags & MLX5_FLOW_VLAN_ACTIONS)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no support for multiple VLAN "
					  "actions");
	/* Pop VLAN with preceding Decap requires inner header with VLAN. */
	if ((action_flags & MLX5_FLOW_ACTION_DECAP) &&
	    !(item_flags & MLX5_FLOW_LAYER_INNER_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot pop vlan after decap without "
					  "match on inner vlan in the flow");
	/* Pop VLAN without preceding Decap requires outer header with VLAN. */
	if (!(action_flags & MLX5_FLOW_ACTION_DECAP) &&
	    !(item_flags & MLX5_FLOW_LAYER_OUTER_VLAN))
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
	if (!attr->transfer && priv->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "pop vlan action for VF representor "
					  "not supported on NIC table");
	return 0;
}

/**
 * Get VLAN default info from vlan match info.
 *
 * @param[in] items
 *   the list of item specifications.
 * @param[out] vlan
 *   pointer VLAN info to fill to.
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
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int type = items->type;

		if (type == RTE_FLOW_ITEM_TYPE_VLAN ||
		    type == MLX5_RTE_FLOW_ITEM_TYPE_VLAN)
			break;
	}
	if (items->type != RTE_FLOW_ITEM_TYPE_END) {
		const struct rte_flow_item_vlan *vlan_m = items->mask;
		const struct rte_flow_item_vlan *vlan_v = items->spec;

		/* If VLAN item in pattern doesn't contain data, return here. */
		if (!vlan_v)
			return;
		if (!vlan_m)
			vlan_m = &nic_mask;
		/* Only full match values are accepted */
		if ((vlan_m->tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) ==
		     MLX5DV_FLOW_VLAN_PCP_MASK_BE) {
			vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_PCP_MASK;
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
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] item_flags
 *   The items found in this flow rule.
 * @param[in] action
 *   Pointer to the action structure.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_push_vlan(struct rte_eth_dev *dev,
				  uint64_t action_flags,
				  const struct rte_flow_item_vlan *vlan_m,
				  const struct rte_flow_action *action,
				  const struct rte_flow_attr *attr,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_of_push_vlan *push_vlan = action->conf;
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (push_vlan->ethertype != RTE_BE16(RTE_ETHER_TYPE_VLAN) &&
	    push_vlan->ethertype != RTE_BE16(RTE_ETHER_TYPE_QINQ))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "invalid vlan ethertype");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after push VLAN");
	if (!attr->transfer && priv->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "push vlan action for VF representor "
					  "not supported on NIC table");
	if (vlan_m &&
	    (vlan_m->tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) &&
	    (vlan_m->tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) !=
		MLX5DV_FLOW_VLAN_PCP_MASK_BE &&
	    !(action_flags & MLX5_FLOW_ACTION_OF_SET_VLAN_PCP) &&
	    !(mlx5_flow_find_action
		(action + 1, RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "not full match mask on VLAN PCP and "
					  "there is no of_set_vlan_pcp action, "
					  "push VLAN action cannot figure out "
					  "PCP value");
	if (vlan_m &&
	    (vlan_m->tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) &&
	    (vlan_m->tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) !=
		MLX5DV_FLOW_VLAN_VID_MASK_BE &&
	    !(action_flags & MLX5_FLOW_ACTION_OF_SET_VLAN_VID) &&
	    !(mlx5_flow_find_action
		(action + 1, RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "not full match mask on VLAN VID and "
					  "there is no of_set_vlan_vid action, "
					  "push VLAN action cannot figure out "
					  "VID value");
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
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] actions
 *   Pointer to the list of actions remaining in the flow rule.
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

	if (rte_be_to_cpu_16(conf->vlan_vid) > 0xFFE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "VLAN VID value is too big");
	if (!(action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN) &&
	    !(item_flags & MLX5_FLOW_LAYER_OUTER_VLAN))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "set VLAN VID action must follow push"
					  " VLAN action or match on VLAN item");
	if (action_flags & MLX5_FLOW_ACTION_OF_SET_VLAN_VID)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Multiple VLAN VID modifications are "
					  "not supported");
	if (action_flags & MLX5_FLOW_ACTION_PORT_ID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, port_id should "
					  "be after set VLAN VID");
	return 0;
}

/*
 * Validate the FLAG action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
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
flow_dv_validate_action_flag(struct rte_eth_dev *dev,
			     uint64_t action_flags,
			     const struct rte_flow_attr *attr,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	int ret;

	/* Fall back if no extended metadata register support. */
	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY)
		return mlx5_flow_validate_action_flag(action_flags, attr,
						      error);
	/* Extensive metadata mode requires registers. */
	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "no metadata registers "
					  "to support flag action");
	if (!(priv->sh->dv_mark_mask & MLX5_FLOW_MARK_DEFAULT))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "extended metadata register"
					  " isn't available");
	ret = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (ret < 0)
		return ret;
	MLX5_ASSERT(ret > 0);
	if (action_flags & MLX5_FLOW_ACTION_MARK)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't mark and flag in same flow");
	if (action_flags & MLX5_FLOW_ACTION_FLAG)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 flag"
					  " actions in same flow");
	return 0;
}

/**
 * Validate MARK action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action
 *   Pointer to action.
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
flow_dv_validate_action_mark(struct rte_eth_dev *dev,
			     const struct rte_flow_action *action,
			     uint64_t action_flags,
			     const struct rte_flow_attr *attr,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	const struct rte_flow_action_mark *mark = action->conf;
	int ret;

	/* Fall back if no extended metadata register support. */
	if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY)
		return mlx5_flow_validate_action_mark(action, action_flags,
						      attr, error);
	/* Extensive metadata mode requires registers. */
	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "no metadata registers "
					  "to support mark action");
	if (!priv->sh->dv_mark_mask)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "extended metadata register"
					  " isn't available");
	ret = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (ret < 0)
		return ret;
	MLX5_ASSERT(ret > 0);
	if (!mark)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (mark->id >= (MLX5_FLOW_MARK_MAX & priv->sh->dv_mark_mask))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &mark->id,
					  "mark id exceeds the limit");
	if (action_flags & MLX5_FLOW_ACTION_FLAG)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't flag and mark in same flow");
	if (action_flags & MLX5_FLOW_ACTION_MARK)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 mark actions in same"
					  " flow");
	return 0;
}

/**
 * Validate SET_META action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the action structure.
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
flow_dv_validate_action_set_meta(struct rte_eth_dev *dev,
				 const struct rte_flow_action *action,
				 uint64_t action_flags __rte_unused,
				 const struct rte_flow_attr *attr,
				 struct rte_flow_error *error)
{
	const struct rte_flow_action_set_meta *conf;
	uint32_t nic_mask = UINT32_MAX;
	int reg;

	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "extended metadata register"
					  " isn't supported");
	reg = flow_dv_get_metadata_reg(dev, attr, error);
	if (reg < 0)
		return reg;
	if (reg != REG_A && reg != REG_B) {
		struct mlx5_priv *priv = dev->data->dev_private;

		nic_mask = priv->sh->dv_meta_mask;
	}
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	conf = (const struct rte_flow_action_set_meta *)action->conf;
	if (!conf->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "zero mask doesn't have any effect");
	if (conf->mask & ~nic_mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "meta data must be within reg C0");
	return 0;
}

/**
 * Validate SET_TAG action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the action structure.
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
flow_dv_validate_action_set_tag(struct rte_eth_dev *dev,
				const struct rte_flow_action *action,
				uint64_t action_flags,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	const struct rte_flow_action_set_tag *conf;
	const uint64_t terminal_action_flags =
		MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_QUEUE |
		MLX5_FLOW_ACTION_RSS;
	int ret;

	if (!mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "extensive metadata register"
					  " isn't supported");
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	conf = (const struct rte_flow_action_set_tag *)action->conf;
	if (!conf->mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "zero mask doesn't have any effect");
	ret = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, conf->index, error);
	if (ret < 0)
		return ret;
	if (!attr->transfer && attr->ingress &&
	    (action_flags & terminal_action_flags))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "set_tag has no effect"
					  " with terminal actions");
	return 0;
}

/**
 * Validate count action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
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
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the action structure.
 * @param[in] attr
 *   Pointer to flow attributes.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_l2_encap(struct rte_eth_dev *dev,
				 uint64_t action_flags,
				 const struct rte_flow_action *action,
				 const struct rte_flow_attr *attr,
				 struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (action_flags & MLX5_FLOW_ACTION_ENCAP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can only have a single encap action "
					  "in a flow");
	if (!attr->transfer && priv->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "encap action for VF representor "
					  "not supported on NIC table");
	return 0;
}

/**
 * Validate a decap action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
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
flow_dv_validate_action_decap(struct rte_eth_dev *dev,
			      uint64_t action_flags,
			      const struct rte_flow_attr *attr,
			      struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->config.hca_attr.scatter_fcs_w_decap_disable &&
	    !priv->config.decap_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "decap is not enabled");
	if (action_flags & MLX5_FLOW_XCAP_ACTIONS)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  action_flags &
					  MLX5_FLOW_ACTION_DECAP ? "can only "
					  "have a single decap action" : "decap "
					  "after encap is not supported");
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
	if (!attr->transfer && priv->representor)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "decap action for VF representor "
					  "not supported on NIC table");
	return 0;
}

const struct rte_flow_action_raw_decap empty_decap = {.data = NULL, .size = 0,};

/**
 * Validate the raw encap and decap actions.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] decap
 *   Pointer to the decap action.
 * @param[in] encap
 *   Pointer to the encap action.
 * @param[in] attr
 *   Pointer to flow attributes
 * @param[in/out] action_flags
 *   Holds the actions detected until now.
 * @param[out] actions_n
 *   pointer to the number of actions counter.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_raw_encap_decap
	(struct rte_eth_dev *dev,
	 const struct rte_flow_action_raw_decap *decap,
	 const struct rte_flow_action_raw_encap *encap,
	 const struct rte_flow_attr *attr, uint64_t *action_flags,
	 int *actions_n, struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	if (encap && (!encap->size || !encap->data))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "raw encap data cannot be empty");
	if (decap && encap) {
		if (decap->size <= MLX5_ENCAPSULATION_DECISION_SIZE &&
		    encap->size > MLX5_ENCAPSULATION_DECISION_SIZE)
			/* L3 encap. */
			decap = NULL;
		else if (encap->size <=
			   MLX5_ENCAPSULATION_DECISION_SIZE &&
			   decap->size >
			   MLX5_ENCAPSULATION_DECISION_SIZE)
			/* L3 decap. */
			encap = NULL;
		else if (encap->size >
			   MLX5_ENCAPSULATION_DECISION_SIZE &&
			   decap->size >
			   MLX5_ENCAPSULATION_DECISION_SIZE)
			/* 2 L2 actions: encap and decap. */
			;
		else
			return rte_flow_error_set(error,
				ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "unsupported too small "
				"raw decap and too small raw "
				"encap combination");
	}
	if (decap) {
		ret = flow_dv_validate_action_decap(dev, *action_flags, attr,
						    error);
		if (ret < 0)
			return ret;
		*action_flags |= MLX5_FLOW_ACTION_DECAP;
		++(*actions_n);
	}
	if (encap) {
		if (encap->size <= MLX5_ENCAPSULATION_DECISION_SIZE)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "small raw encap size");
		if (*action_flags & MLX5_FLOW_ACTION_ENCAP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "more than one encap action");
		if (!attr->transfer && priv->representor)
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "encap action for VF representor "
					 "not supported on NIC table");
		*action_flags |= MLX5_FLOW_ACTION_ENCAP;
		++(*actions_n);
	}
	return 0;
}

/**
 * Find existing encap/decap resource or create and register a new one.
 *
 * @param[in, out] dev
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_encap_decap_resource *cache_resource;
	struct mlx5dv_dr_domain *domain;
	uint32_t idx = 0;
	int ret;

	resource->flags = dev_flow->dv.group ? 0 : 1;
	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;
	/* Lookup a matching resource from cache. */
	ILIST_FOREACH(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], sh->encaps_decaps, idx,
		      cache_resource, next) {
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
			dev_flow->handle->dvh.rix_encap_decap = idx;
			dev_flow->dv.encap_decap = cache_resource;
			return 0;
		}
	}
	/* Register new encap/decap resource. */
	cache_resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_DECAP_ENCAP],
				       &dev_flow->handle->dvh.rix_encap_decap);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	ret = mlx5_flow_os_create_flow_action_packet_reformat
					(sh->ctx, domain, cache_resource,
					 &cache_resource->action);
	if (ret) {
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	ILIST_INSERT(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], &sh->encaps_decaps,
		     dev_flow->handle->dvh.rix_encap_decap, cache_resource,
		     next);
	dev_flow->dv.encap_decap = cache_resource;
	DRV_LOG(DEBUG, "new encap/decap resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Find existing table jump resource or create and register a new one.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] tbl
 *   Pointer to flow table resource.
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
			(struct rte_eth_dev *dev __rte_unused,
			 struct mlx5_flow_tbl_resource *tbl,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_flow_tbl_data_entry *tbl_data =
		container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);
	int cnt, ret;

	MLX5_ASSERT(tbl);
	cnt = rte_atomic32_read(&tbl_data->jump.refcnt);
	if (!cnt) {
		ret = mlx5_flow_os_create_flow_action_dest_flow_tbl
				(tbl->obj, &tbl_data->jump.action);
		if (ret)
			return rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "cannot create jump action");
		DRV_LOG(DEBUG, "new jump table resource %p: refcnt %d++",
			(void *)&tbl_data->jump, cnt);
	} else {
		/* old jump should not make the table ref++. */
		flow_dv_tbl_resource_release(dev, &tbl_data->tbl);
		MLX5_ASSERT(tbl_data->jump.action);
		DRV_LOG(DEBUG, "existed jump table resource %p: refcnt %d++",
			(void *)&tbl_data->jump, cnt);
	}
	rte_atomic32_inc(&tbl_data->jump.refcnt);
	dev_flow->handle->rix_jump = tbl_data->idx;
	dev_flow->dv.jump = &tbl_data->jump;
	return 0;
}

/**
 * Find existing default miss resource or create and register a new one.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_default_miss_resource_register(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_default_miss_resource *cache_resource =
			&sh->default_miss;
	int cnt = rte_atomic32_read(&cache_resource->refcnt);

	if (!cnt) {
		MLX5_ASSERT(cache_resource->action);
		cache_resource->action =
		mlx5_glue->dr_create_flow_action_default_miss();
		if (!cache_resource->action)
			return rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"cannot create default miss action");
		DRV_LOG(DEBUG, "new default miss resource %p: refcnt %d++",
				(void *)cache_resource->action, cnt);
	}
	rte_atomic32_inc(&cache_resource->refcnt);
	return 0;
}

/**
 * Find existing table port ID resource or create and register a new one.
 *
 * @param[in, out] dev
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_port_id_action_resource *cache_resource;
	uint32_t idx = 0;
	int ret;

	/* Lookup a matching resource from cache. */
	ILIST_FOREACH(sh->ipool[MLX5_IPOOL_PORT_ID], sh->port_id_action_list,
		      idx, cache_resource, next) {
		if (resource->port_id == cache_resource->port_id) {
			DRV_LOG(DEBUG, "port id action resource resource %p: "
				"refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->handle->rix_port_id_action = idx;
			dev_flow->dv.port_id_action = cache_resource;
			return 0;
		}
	}
	/* Register new port id action resource. */
	cache_resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PORT_ID],
				       &dev_flow->handle->rix_port_id_action);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	ret = mlx5_flow_os_create_flow_action_dest_port
				(priv->sh->fdb_domain, resource->port_id,
				 &cache_resource->action);
	if (ret) {
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	ILIST_INSERT(sh->ipool[MLX5_IPOOL_PORT_ID], &sh->port_id_action_list,
		     dev_flow->handle->rix_port_id_action, cache_resource,
		     next);
	dev_flow->dv.port_id_action = cache_resource;
	DRV_LOG(DEBUG, "new port id action resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Find existing push vlan resource or create and register a new one.
 *
 * @param [in, out] dev
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_push_vlan_action_resource *cache_resource;
	struct mlx5dv_dr_domain *domain;
	uint32_t idx = 0;
	int ret;

	/* Lookup a matching resource from cache. */
	ILIST_FOREACH(sh->ipool[MLX5_IPOOL_PUSH_VLAN],
		      sh->push_vlan_action_list, idx, cache_resource, next) {
		if (resource->vlan_tag == cache_resource->vlan_tag &&
		    resource->ft_type == cache_resource->ft_type) {
			DRV_LOG(DEBUG, "push-VLAN action resource resource %p: "
				"refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->handle->dvh.rix_push_vlan = idx;
			dev_flow->dv.push_vlan_res = cache_resource;
			return 0;
		}
	}
	/* Register new push_vlan action resource. */
	cache_resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PUSH_VLAN],
				       &dev_flow->handle->dvh.rix_push_vlan);
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
	ret = mlx5_flow_os_create_flow_action_push_vlan
					(domain, resource->vlan_tag,
					 &cache_resource->action);
	if (ret) {
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	ILIST_INSERT(sh->ipool[MLX5_IPOOL_PUSH_VLAN],
		     &sh->push_vlan_action_list,
		     dev_flow->handle->dvh.rix_push_vlan,
		     cache_resource, next);
	dev_flow->dv.push_vlan_res = cache_resource;
	DRV_LOG(DEBUG, "new push vlan action resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}
/**
 * Get the size of specific rte_flow_item_type hdr size
 *
 * @param[in] item_type
 *   Tested rte_flow_item_type.
 *
 * @return
 *   sizeof struct item_type, 0 if void or irrelevant.
 */
static size_t
flow_dv_get_item_hdr_len(const enum rte_flow_item_type item_type)
{
	size_t retval;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		retval = sizeof(struct rte_ether_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		retval = sizeof(struct rte_vlan_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		retval = sizeof(struct rte_ipv4_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		retval = sizeof(struct rte_ipv6_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		retval = sizeof(struct rte_udp_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		retval = sizeof(struct rte_tcp_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		retval = sizeof(struct rte_vxlan_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		retval = sizeof(struct rte_gre_hdr);
		break;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		retval = sizeof(struct rte_mpls_hdr);
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
		len = flow_dv_get_item_hdr_len(items->type);
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
	if (flow_dv_zero_encap_udp_csum(res.buf, error))
		return -rte_errno;
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

	memset(&res, 0, sizeof(res));
	encap_data = (const struct rte_flow_action_raw_encap *)action->conf;
	res.size = encap_data->size;
	memcpy(res.buf, encap_data->data, res.size);
	res.reformat_type = res.size < MLX5_ENCAPSULATION_DECISION_SIZE ?
		MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2 :
		MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL;
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
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] vlan
 *   Pointer to the vlan to push to the Ethernet header.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
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

	memset(&res, 0, sizeof(res));
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
	if (action_flags & MLX5_FLOW_ACTION_ENCAP)
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				 MLX5_FLOW_LAYER_OUTER_L3_IPV4;
		if (!(item_flags & layer))
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				 MLX5_FLOW_LAYER_OUTER_L3_IPV6;
		if (!(item_flags & layer))
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L4 :
				 MLX5_FLOW_LAYER_OUTER_L4;
		if (!(item_flags & layer))
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L4_TCP :
				 MLX5_FLOW_LAYER_OUTER_L4_TCP;
		if (!(item_flags & layer))
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L4_TCP :
				 MLX5_FLOW_LAYER_OUTER_L4_TCP;
		if (!(item_flags & layer))
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
	uint64_t layer;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (!ret) {
		layer = (action_flags & MLX5_FLOW_ACTION_DECAP) ?
				 MLX5_FLOW_LAYER_INNER_L3 :
				 MLX5_FLOW_LAYER_OUTER_L3;
		if (!(item_flags & layer))
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
	uint32_t target_group, table;
	int ret = 0;

	if (action_flags & (MLX5_FLOW_FATE_ACTIONS |
			    MLX5_FLOW_FATE_ESWITCH_ACTIONS))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions in"
					  " same flow");
	if (action_flags & MLX5_FLOW_ACTION_METER)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "jump with meter not support");
	if (!action->conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "action configuration not set");
	target_group =
		((const struct rte_flow_action_jump *)action->conf)->group;
	ret = mlx5_flow_group_to_table(attributes, external, target_group,
				       true, &table, error);
	if (ret)
		return ret;
	if (attributes->group == target_group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "target group must be other than"
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
	act_priv = mlx5_port_to_eswitch_info(port, false);
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
 * Get the maximum number of modify header actions.
 *
 * @param dev
 *   Pointer to rte_eth_dev structure.
 * @param flags
 *   Flags bits to check if root level.
 *
 * @return
 *   Max number of modify header actions device can support.
 */
static inline unsigned int
flow_dv_modify_hdr_action_max(struct rte_eth_dev *dev __rte_unused,
			      uint64_t flags)
{
	/*
	 * There's no way to directly query the max capacity from FW.
	 * The maximal value on root table should be assumed to be supported.
	 */
	if (!(flags & MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL))
		return MLX5_MAX_MODIFY_NUM;
	else
		return MLX5_ROOT_TBL_MODIFY_NUM;
}

/**
 * Validate the meter action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] action
 *   Pointer to the meter action.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_ernno is set.
 */
static int
mlx5_flow_validate_action_meter(struct rte_eth_dev *dev,
				uint64_t action_flags,
				const struct rte_flow_action *action,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_meter *am = action->conf;
	struct mlx5_flow_meter *fm;

	if (!am)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "meter action conf is NULL");

	if (action_flags & MLX5_FLOW_ACTION_METER)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "meter chaining not support");
	if (action_flags & MLX5_FLOW_ACTION_JUMP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "meter with jump not support");
	if (!priv->mtr_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "meter action not supported");
	fm = mlx5_flow_meter_find(priv, am->mtr_id);
	if (!fm)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Meter not found");
	if (fm->ref_cnt && (!(fm->transfer == attr->transfer ||
	      (!fm->ingress && !attr->ingress && attr->egress) ||
	      (!fm->egress && !attr->egress && attr->ingress))))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Flow attributes are either invalid "
					  "or have a conflict with current "
					  "meter attributes");
	return 0;
}

/**
 * Validate the age action.
 *
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the age action.
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_age(uint64_t action_flags,
			    const struct rte_flow_action *action,
			    struct rte_eth_dev *dev,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_age *age = action->conf;

	if (!priv->config.devx || priv->counter_fallback)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "age action not supported");
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (age->timeout >= UINT16_MAX / 2 / 10)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Max age time: 3275 seconds");
	if (action_flags & MLX5_FLOW_ACTION_AGE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Duplicate age ctions set");
	return 0;
}

/**
 * Validate the modify-header IPv4 DSCP actions.
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
flow_dv_validate_action_modify_ipv4_dscp(const uint64_t action_flags,
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
 * Validate the modify-header IPv6 DSCP actions.
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
flow_dv_validate_action_modify_ipv6_dscp(const uint64_t action_flags,
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_modify_hdr_resource *cache_resource;
	struct mlx5dv_dr_domain *ns;
	uint32_t actions_len;
	int ret;

	resource->flags = dev_flow->dv.group ? 0 :
			  MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL;
	if (resource->actions_num > flow_dv_modify_hdr_action_max(dev,
				    resource->flags))
		return rte_flow_error_set(error, EOVERFLOW,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "too many modify header items");
	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		ns = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_TX)
		ns = sh->tx_domain;
	else
		ns = sh->rx_domain;
	/* Lookup a matching resource from cache. */
	actions_len = resource->actions_num * sizeof(resource->actions[0]);
	LIST_FOREACH(cache_resource, &sh->modify_cmds, next) {
		if (resource->ft_type == cache_resource->ft_type &&
		    resource->actions_num == cache_resource->actions_num &&
		    resource->flags == cache_resource->flags &&
		    !memcmp((const void *)resource->actions,
			    (const void *)cache_resource->actions,
			    actions_len)) {
			DRV_LOG(DEBUG, "modify-header resource %p: refcnt %d++",
				(void *)cache_resource,
				rte_atomic32_read(&cache_resource->refcnt));
			rte_atomic32_inc(&cache_resource->refcnt);
			dev_flow->handle->dvh.modify_hdr = cache_resource;
			return 0;
		}
	}
	/* Register new modify-header resource. */
	cache_resource = mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(*cache_resource) + actions_len, 0,
				    SOCKET_ID_ANY);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	*cache_resource = *resource;
	rte_memcpy(cache_resource->actions, resource->actions, actions_len);
	ret = mlx5_flow_os_create_flow_action_modify_header
					(sh->ctx, ns, cache_resource,
					 actions_len, &cache_resource->action);
	if (ret) {
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	LIST_INSERT_HEAD(&sh->modify_cmds, cache_resource, next);
	dev_flow->handle->dvh.modify_hdr = cache_resource;
	DRV_LOG(DEBUG, "new modify-header resource %p: refcnt %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Get DV flow counter by index.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   mlx5 flow counter index in the container.
 * @param[out] ppool
 *   mlx5 flow counter pool in the container,
 *
 * @return
 *   Pointer to the counter, NULL otherwise.
 */
static struct mlx5_flow_counter *
flow_dv_counter_get_by_idx(struct rte_eth_dev *dev,
			   uint32_t idx,
			   struct mlx5_flow_counter_pool **ppool)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_pools_container *cont;
	struct mlx5_flow_counter_pool *pool;
	uint32_t batch = 0, age = 0;

	idx--;
	age = MLX_CNT_IS_AGE(idx);
	idx = age ? idx - MLX5_CNT_AGE_OFFSET : idx;
	if (idx >= MLX5_CNT_BATCH_OFFSET) {
		idx -= MLX5_CNT_BATCH_OFFSET;
		batch = 1;
	}
	cont = MLX5_CNT_CONTAINER(priv->sh, batch, age);
	MLX5_ASSERT(idx / MLX5_COUNTERS_PER_POOL < cont->n);
	pool = cont->pools[idx / MLX5_COUNTERS_PER_POOL];
	MLX5_ASSERT(pool);
	if (ppool)
		*ppool = pool;
	return MLX5_POOL_GET_CNT(pool, idx % MLX5_COUNTERS_PER_POOL);
}

/**
 * Check the devx counter belongs to the pool.
 *
 * @param[in] pool
 *   Pointer to the counter pool.
 * @param[in] id
 *   The counter devx ID.
 *
 * @return
 *   True if counter belongs to the pool, false otherwise.
 */
static bool
flow_dv_is_counter_in_pool(struct mlx5_flow_counter_pool *pool, int id)
{
	int base = (pool->min_dcs->id / MLX5_COUNTERS_PER_POOL) *
		   MLX5_COUNTERS_PER_POOL;

	if (id >= base && id < base + MLX5_COUNTERS_PER_POOL)
		return true;
	return false;
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
	uint32_t i;

	/* Check last used pool. */
	if (cont->last_pool_idx != POOL_IDX_INVALID &&
	    flow_dv_is_counter_in_pool(cont->pools[cont->last_pool_idx], id))
		return cont->pools[cont->last_pool_idx];
	/* ID out of range means no suitable pool in the container. */
	if (id > cont->max_id || id < cont->min_id)
		return NULL;
	/*
	 * Find the pool from the end of the container, since mostly counter
	 * ID is sequence increasing, and the last pool should be the needed
	 * one.
	 */
	i = rte_atomic16_read(&cont->n_valid);
	while (i--) {
		struct mlx5_flow_counter_pool *pool = cont->pools[i];

		if (flow_dv_is_counter_in_pool(pool, id))
			return pool;
	}
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_devx_mkey_attr mkey_attr;
	struct mlx5_counter_stats_mem_mng *mem_mng;
	volatile struct flow_counter_stats *raw_data;
	int size = (sizeof(struct flow_counter_stats) *
			MLX5_COUNTERS_PER_POOL +
			sizeof(struct mlx5_counter_stats_raw)) * raws_n +
			sizeof(struct mlx5_counter_stats_mem_mng);
	size_t pgsize = rte_mem_page_size();
	if (pgsize == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return NULL;
	}
	uint8_t *mem = mlx5_malloc(MLX5_MEM_ZERO, size, pgsize,
				  SOCKET_ID_ANY);
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
		mlx5_free(mem);
		return NULL;
	}
	mkey_attr.addr = (uintptr_t)mem;
	mkey_attr.size = size;
	mkey_attr.umem_id = mlx5_os_get_umem_id(mem_mng->umem);
	mkey_attr.pd = sh->pdn;
	mkey_attr.log_entity_size = 0;
	mkey_attr.pg_access = 0;
	mkey_attr.klm_array = NULL;
	mkey_attr.klm_num = 0;
	if (priv->config.hca_attr.relaxed_ordering_write &&
		priv->config.hca_attr.relaxed_ordering_read  &&
		!haswell_broadwell_cpu)
		mkey_attr.relaxed_ordering = 1;
	mem_mng->dm = mlx5_devx_cmd_mkey_create(sh->ctx, &mkey_attr);
	if (!mem_mng->dm) {
		mlx5_glue->devx_umem_dereg(mem_mng->umem);
		rte_errno = errno;
		mlx5_free(mem);
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
 * @param[in] age
 *   Whether the pool is for Aging counter.
 *
 * @return
 *   0 on success, otherwise negative errno value and rte_errno is set.
 */
static int
flow_dv_container_resize(struct rte_eth_dev *dev,
				uint32_t batch, uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(priv->sh, batch,
							       age);
	struct mlx5_counter_stats_mem_mng *mem_mng = NULL;
	void *old_pools = cont->pools;
	uint32_t resize = cont->n + MLX5_CNT_CONTAINER_RESIZE;
	uint32_t mem_size = sizeof(struct mlx5_flow_counter_pool *) * resize;
	void *pools = mlx5_malloc(MLX5_MEM_ZERO, mem_size, 0, SOCKET_ID_ANY);

	if (!pools) {
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	if (old_pools)
		memcpy(pools, old_pools, cont->n *
				       sizeof(struct mlx5_flow_counter_pool *));
	/*
	 * Fallback mode query the counter directly, no background query
	 * resources are needed.
	 */
	if (!priv->counter_fallback) {
		int i;

		mem_mng = flow_dv_create_counter_stat_mem_mng(dev,
			  MLX5_CNT_CONTAINER_RESIZE + MLX5_MAX_PENDING_QUERIES);
		if (!mem_mng) {
			mlx5_free(pools);
			return -ENOMEM;
		}
		for (i = 0; i < MLX5_MAX_PENDING_QUERIES; ++i)
			LIST_INSERT_HEAD(&priv->sh->cmng.free_stat_raws,
					 mem_mng->raws +
					 MLX5_CNT_CONTAINER_RESIZE +
					 i, next);
	}
	rte_spinlock_lock(&cont->resize_sl);
	cont->n = resize;
	cont->mem_mng = mem_mng;
	cont->pools = pools;
	rte_spinlock_unlock(&cont->resize_sl);
	if (old_pools)
		mlx5_free(old_pools);
	return 0;
}

/**
 * Query a devx flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] cnt
 *   Index to the flow counter.
 * @param[out] pkts
 *   The statistics value of packets.
 * @param[out] bytes
 *   The statistics value of bytes.
 *
 * @return
 *   0 on success, otherwise a negative errno value and rte_errno is set.
 */
static inline int
_flow_dv_query_count(struct rte_eth_dev *dev, uint32_t counter, uint64_t *pkts,
		     uint64_t *bytes)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt;
	struct mlx5_flow_counter_ext *cnt_ext = NULL;
	int offset;

	cnt = flow_dv_counter_get_by_idx(dev, counter, &pool);
	MLX5_ASSERT(pool);
	if (counter < MLX5_CNT_BATCH_OFFSET) {
		cnt_ext = MLX5_CNT_TO_CNT_EXT(pool, cnt);
		if (priv->counter_fallback)
			return mlx5_devx_cmd_flow_counter_query(cnt_ext->dcs, 0,
					0, pkts, bytes, 0, NULL, NULL, 0);
	}

	rte_spinlock_lock(&pool->sl);
	/*
	 * The single counters allocation may allocate smaller ID than the
	 * current allocated in parallel to the host reading.
	 * In this case the new counter values must be reported as 0.
	 */
	if (unlikely(cnt_ext && cnt_ext->dcs->id < pool->raw->min_dcs_id)) {
		*pkts = 0;
		*bytes = 0;
	} else {
		offset = MLX5_CNT_ARRAY_IDX(pool, cnt);
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
 * @param[in] age
 *   Whether the pool is for counter that was allocated for aging.
 * @param[in/out] cont_cur
 *   Pointer to the container pointer, it will be update in pool resize.
 *
 * @return
 *   The pool container pointer on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter_pool *
flow_dv_pool_create(struct rte_eth_dev *dev, struct mlx5_devx_obj *dcs,
		    uint32_t batch, uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(priv->sh, batch,
							       age);
	int16_t n_valid = rte_atomic16_read(&cont->n_valid);
	uint32_t size = sizeof(*pool);

	if (cont->n == n_valid && flow_dv_container_resize(dev, batch, age))
		return NULL;
	size += MLX5_COUNTERS_PER_POOL * CNT_SIZE;
	size += (batch ? 0 : MLX5_COUNTERS_PER_POOL * CNTEXT_SIZE);
	size += (!age ? 0 : MLX5_COUNTERS_PER_POOL * AGE_SIZE);
	pool = mlx5_malloc(MLX5_MEM_ZERO, size, 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		return NULL;
	}
	pool->min_dcs = dcs;
	if (!priv->counter_fallback)
		pool->raw = cont->mem_mng->raws + n_valid %
						      MLX5_CNT_CONTAINER_RESIZE;
	pool->raw_hw = NULL;
	pool->type = 0;
	pool->type |= (batch ? 0 :  CNT_POOL_TYPE_EXT);
	pool->type |= (!age ? 0 :  CNT_POOL_TYPE_AGE);
	pool->query_gen = 0;
	rte_spinlock_init(&pool->sl);
	TAILQ_INIT(&pool->counters[0]);
	TAILQ_INIT(&pool->counters[1]);
	TAILQ_INSERT_HEAD(&cont->pool_list, pool, next);
	pool->index = n_valid;
	cont->pools[n_valid] = pool;
	if (!batch) {
		int base = RTE_ALIGN_FLOOR(dcs->id, MLX5_COUNTERS_PER_POOL);

		if (base < cont->min_id)
			cont->min_id = base;
		if (base > cont->max_id)
			cont->max_id = base + MLX5_COUNTERS_PER_POOL - 1;
		cont->last_pool_idx = pool->index;
	}
	/* Pool initialization must be updated before host thread access. */
	rte_cio_wmb();
	rte_atomic16_add(&cont->n_valid, 1);
	return pool;
}

/**
 * Restore skipped counters in the pool.
 *
 * As counter pool query requires the first counter dcs
 * ID start with 4 alinged, if the pool counters with
 * min_dcs ID are not aligned with 4, the counters will
 * be skipped.
 * Once other min_dcs ID less than these skipped counter
 * dcs ID appears, the skipped counters will be safe to
 * use.
 * Should be called when min_dcs is updated.
 *
 * @param[in] pool
 *   Current counter pool.
 * @param[in] last_min_dcs
 *   Last min_dcs.
 */
static void
flow_dv_counter_restore(struct mlx5_flow_counter_pool *pool,
			struct mlx5_devx_obj *last_min_dcs)
{
	struct mlx5_flow_counter_ext *cnt_ext;
	uint32_t offset, new_offset;
	uint32_t skip_cnt = 0;
	uint32_t i;

	if (!pool->skip_cnt)
		return;
	/*
	 * If last min_dcs is not valid. The skipped counter may even after
	 * last min_dcs, set the offset to the whole pool.
	 */
	if (last_min_dcs->id & (MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT - 1))
		offset = MLX5_COUNTERS_PER_POOL;
	else
		offset = last_min_dcs->id % MLX5_COUNTERS_PER_POOL;
	new_offset = pool->min_dcs->id % MLX5_COUNTERS_PER_POOL;
	/*
	 * Check the counters from 1 to the last_min_dcs range. Counters
	 * before new min_dcs indicates pool still has skipped counters.
	 * Counters be skipped after new min_dcs will be ready to use.
	 * Offset 0 counter must be empty or min_dcs, start from 1.
	 */
	for (i = 1; i < offset; i++) {
		cnt_ext = MLX5_GET_POOL_CNT_EXT(pool, i);
		if (cnt_ext->skipped) {
			if (i > new_offset) {
				cnt_ext->skipped = 0;
				TAILQ_INSERT_TAIL
					(&pool->counters[pool->query_gen],
					 MLX5_POOL_GET_CNT(pool, i), next);
			} else {
				skip_cnt++;
			}
		}
	}
	if (!skip_cnt)
		pool->skip_cnt = 0;
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
 * @param[in] age
 *   Whether the pool is for counter that was allocated for aging.
 *
 * @return
 *   The counter pool pointer and @p cnt_free is set on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter_pool *
flow_dv_counter_pool_prepare(struct rte_eth_dev *dev,
			     struct mlx5_flow_counter **cnt_free,
			     uint32_t batch, uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_pools_container *cont;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_counters tmp_tq;
	struct mlx5_devx_obj *last_min_dcs;
	struct mlx5_devx_obj *dcs = NULL;
	struct mlx5_flow_counter *cnt;
	uint32_t add2other;
	uint32_t i;

	cont = MLX5_CNT_CONTAINER(priv->sh, batch, age);
	if (!batch) {
retry:
		add2other = 0;
		/* bulk_bitmap must be 0 for single counter allocation. */
		dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->ctx, 0);
		if (!dcs)
			return NULL;
		pool = flow_dv_find_pool_by_id(cont, dcs->id);
		/* Check if counter belongs to exist pool ID range. */
		if (!pool) {
			pool = flow_dv_find_pool_by_id
			       (MLX5_CNT_CONTAINER
			       (priv->sh, batch, (age ^ 0x1)), dcs->id);
			/*
			 * Pool eixsts, counter will be added to the other
			 * container, need to reallocate it later.
			 */
			if (pool) {
				add2other = 1;
			} else {
				pool = flow_dv_pool_create(dev, dcs, batch,
							   age);
				if (!pool) {
					mlx5_devx_cmd_destroy(dcs);
					return NULL;
				}
			}
		}
		if ((dcs->id < pool->min_dcs->id ||
		    pool->min_dcs->id &
		    (MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT - 1)) &&
		    !(dcs->id & (MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT - 1))) {
			/*
			 * Update the pool min_dcs only if current dcs is
			 * valid and exist min_dcs is not valid or greater
			 * than new dcs.
			 */
			last_min_dcs = pool->min_dcs;
			rte_atomic64_set(&pool->a64_dcs,
					 (int64_t)(uintptr_t)dcs);
			/*
			 * Restore any skipped counters if the new min_dcs
			 * ID is smaller or min_dcs is not valid.
			 */
			if (dcs->id < last_min_dcs->id ||
			    last_min_dcs->id &
			    (MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT - 1))
				flow_dv_counter_restore(pool, last_min_dcs);
		}
		i = dcs->id % MLX5_COUNTERS_PER_POOL;
		cnt = MLX5_POOL_GET_CNT(pool, i);
		cnt->pool = pool;
		MLX5_GET_POOL_CNT_EXT(pool, i)->dcs = dcs;
		/*
		 * If min_dcs is not valid, it means the new allocated dcs
		 * also fail to become the valid min_dcs, just skip it.
		 * Or if min_dcs is valid, and new dcs ID is smaller than
		 * min_dcs, but not become the min_dcs, also skip it.
		 */
		if (pool->min_dcs->id &
		    (MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT - 1) ||
		    dcs->id < pool->min_dcs->id) {
			MLX5_GET_POOL_CNT_EXT(pool, i)->skipped = 1;
			pool->skip_cnt = 1;
			goto retry;
		}
		if (add2other) {
			TAILQ_INSERT_TAIL(&pool->counters[pool->query_gen],
					  cnt, next);
			goto retry;
		}
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
	pool = flow_dv_pool_create(dev, dcs, batch, age);
	if (!pool) {
		mlx5_devx_cmd_destroy(dcs);
		return NULL;
	}
	TAILQ_INIT(&tmp_tq);
	for (i = 1; i < MLX5_COUNTERS_PER_POOL; ++i) {
		cnt = MLX5_POOL_GET_CNT(pool, i);
		cnt->pool = pool;
		TAILQ_INSERT_HEAD(&tmp_tq, cnt, next);
	}
	rte_spinlock_lock(&cont->csl);
	TAILQ_CONCAT(&cont->counters, &tmp_tq, next);
	rte_spinlock_unlock(&cont->csl);
	*cnt_free = MLX5_POOL_GET_CNT(pool, 0);
	(*cnt_free)->pool = pool;
	return pool;
}

/**
 * Search for existed shared counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] id
 *   The shared counter ID to search.
 * @param[out] ppool
 *   mlx5 flow counter pool in the container,
 *
 * @return
 *   NULL if not existed, otherwise pointer to the shared extend counter.
 */
static struct mlx5_flow_counter_ext *
flow_dv_counter_shared_search(struct rte_eth_dev *dev, uint32_t id,
			      struct mlx5_flow_counter_pool **ppool)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	union mlx5_l3t_data data;
	uint32_t cnt_idx;

	if (mlx5_l3t_get_entry(priv->sh->cnt_id_tbl, id, &data) || !data.dword)
		return NULL;
	cnt_idx = data.dword;
	/*
	 * Shared counters don't have age info. The counter extend is after
	 * the counter datat structure.
	 */
	return (struct mlx5_flow_counter_ext *)
	       ((flow_dv_counter_get_by_idx(dev, cnt_idx, ppool)) + 1);
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
 * @param[in] age
 *   Whether the counter was allocated for aging.
 *
 * @return
 *   Index to flow counter on success, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_dv_counter_alloc(struct rte_eth_dev *dev, uint32_t shared, uint32_t id,
		      uint16_t group, uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt_free = NULL;
	struct mlx5_flow_counter_ext *cnt_ext = NULL;
	/*
	 * Currently group 0 flow counter cannot be assigned to a flow if it is
	 * not the first one in the batch counter allocation, so it is better
	 * to allocate counters one by one for these flows in a separate
	 * container.
	 * A counter can be shared between different groups so need to take
	 * shared counters from the single container.
	 */
	uint32_t batch = (group && !shared && !priv->counter_fallback) ? 1 : 0;
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(priv->sh, batch,
							       age);
	uint32_t cnt_idx;

	if (!priv->config.devx) {
		rte_errno = ENOTSUP;
		return 0;
	}
	if (shared) {
		cnt_ext = flow_dv_counter_shared_search(dev, id, &pool);
		if (cnt_ext) {
			if (cnt_ext->ref_cnt + 1 == 0) {
				rte_errno = E2BIG;
				return 0;
			}
			cnt_ext->ref_cnt++;
			cnt_idx = pool->index * MLX5_COUNTERS_PER_POOL +
				  (cnt_ext->dcs->id % MLX5_COUNTERS_PER_POOL)
				  + 1;
			return cnt_idx;
		}
	}
	/* Get free counters from container. */
	rte_spinlock_lock(&cont->csl);
	cnt_free = TAILQ_FIRST(&cont->counters);
	if (cnt_free)
		TAILQ_REMOVE(&cont->counters, cnt_free, next);
	rte_spinlock_unlock(&cont->csl);
	if (!cnt_free && !flow_dv_counter_pool_prepare(dev, &cnt_free,
						       batch, age))
		goto err;
	pool = cnt_free->pool;
	if (!batch)
		cnt_ext = MLX5_CNT_TO_CNT_EXT(pool, cnt_free);
	/* Create a DV counter action only in the first time usage. */
	if (!cnt_free->action) {
		uint16_t offset;
		struct mlx5_devx_obj *dcs;
		int ret;

		if (batch) {
			offset = MLX5_CNT_ARRAY_IDX(pool, cnt_free);
			dcs = pool->min_dcs;
		} else {
			offset = 0;
			dcs = cnt_ext->dcs;
		}
		ret = mlx5_flow_os_create_flow_action_count(dcs->obj, offset,
							    &cnt_free->action);
		if (ret) {
			rte_errno = errno;
			goto err;
		}
	}
	cnt_idx = MLX5_MAKE_CNT_IDX(pool->index,
				MLX5_CNT_ARRAY_IDX(pool, cnt_free));
	cnt_idx += batch * MLX5_CNT_BATCH_OFFSET;
	cnt_idx += age * MLX5_CNT_AGE_OFFSET;
	/* Update the counter reset values. */
	if (_flow_dv_query_count(dev, cnt_idx, &cnt_free->hits,
				 &cnt_free->bytes))
		goto err;
	if (cnt_ext) {
		cnt_ext->shared = shared;
		cnt_ext->ref_cnt = 1;
		cnt_ext->id = id;
		if (shared) {
			union mlx5_l3t_data data;

			data.dword = cnt_idx;
			if (mlx5_l3t_set_entry(priv->sh->cnt_id_tbl, id, &data))
				return 0;
		}
	}
	if (!priv->counter_fallback && !priv->sh->cmng.query_thread_on)
		/* Start the asynchronous batch query by the host thread. */
		mlx5_set_query_alarm(priv->sh);
	return cnt_idx;
err:
	if (cnt_free) {
		cnt_free->pool = pool;
		rte_spinlock_lock(&cont->csl);
		TAILQ_INSERT_TAIL(&cont->counters, cnt_free, next);
		rte_spinlock_unlock(&cont->csl);
	}
	return 0;
}

/**
 * Get age param from counter index.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Index to the counter handler.
 *
 * @return
 *   The aging parameter specified for the counter index.
 */
static struct mlx5_age_param*
flow_dv_counter_idx_get_age(struct rte_eth_dev *dev,
				uint32_t counter)
{
	struct mlx5_flow_counter *cnt;
	struct mlx5_flow_counter_pool *pool = NULL;

	flow_dv_counter_get_by_idx(dev, counter, &pool);
	counter = (counter - 1) % MLX5_COUNTERS_PER_POOL;
	cnt = MLX5_POOL_GET_CNT(pool, counter);
	return MLX5_CNT_TO_AGE(cnt);
}

/**
 * Remove a flow counter from aged counter list.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Index to the counter handler.
 * @param[in] cnt
 *   Pointer to the counter handler.
 */
static void
flow_dv_counter_remove_from_age(struct rte_eth_dev *dev,
				uint32_t counter, struct mlx5_flow_counter *cnt)
{
	struct mlx5_age_info *age_info;
	struct mlx5_age_param *age_param;
	struct mlx5_priv *priv = dev->data->dev_private;

	age_info = GET_PORT_AGE_INFO(priv);
	age_param = flow_dv_counter_idx_get_age(dev, counter);
	if (rte_atomic16_cmpset((volatile uint16_t *)
			&age_param->state,
			AGE_CANDIDATE, AGE_FREE)
			!= AGE_CANDIDATE) {
		/**
		 * We need the lock even it is age timeout,
		 * since counter may still in process.
		 */
		rte_spinlock_lock(&age_info->aged_sl);
		TAILQ_REMOVE(&age_info->aged_counters, cnt, next);
		rte_spinlock_unlock(&age_info->aged_sl);
	}
	rte_atomic16_set(&age_param->state, AGE_FREE);
}
/**
 * Release a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
 *   Index to the counter handler.
 */
static void
flow_dv_counter_release(struct rte_eth_dev *dev, uint32_t counter)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt;
	struct mlx5_flow_counter_ext *cnt_ext = NULL;

	if (!counter)
		return;
	cnt = flow_dv_counter_get_by_idx(dev, counter, &pool);
	MLX5_ASSERT(pool);
	if (counter < MLX5_CNT_BATCH_OFFSET) {
		cnt_ext = MLX5_CNT_TO_CNT_EXT(pool, cnt);
		if (cnt_ext) {
			if (--cnt_ext->ref_cnt)
				return;
			if (cnt_ext->shared)
				mlx5_l3t_clear_entry(priv->sh->cnt_id_tbl,
						     cnt_ext->id);
		}
	}
	if (IS_AGE_POOL(pool))
		flow_dv_counter_remove_from_age(dev, counter, cnt);
	cnt->pool = pool;
	/*
	 * Put the counter back to list to be updated in none fallback mode.
	 * Currently, we are using two list alternately, while one is in query,
	 * add the freed counter to the other list based on the pool query_gen
	 * value. After query finishes, add counter the list to the global
	 * container counter list. The list changes while query starts. In
	 * this case, lock will not be needed as query callback and release
	 * function both operate with the different list.
	 *
	 */
	if (!priv->counter_fallback)
		TAILQ_INSERT_TAIL(&pool->counters[pool->query_gen], cnt, next);
	else
		TAILQ_INSERT_TAIL(&((MLX5_CNT_CONTAINER
				  (priv->sh, 0, 0))->counters),
				  cnt, next);
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
 *   - 0 on success and non root table.
 *   - 1 on success and root table.
 *   - a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_attributes(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attributes,
			    bool external __rte_unused,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->config.flow_prio - 1;
	int ret = 0;

#ifndef HAVE_MLX5DV_DR
	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups are not supported");
#else
	uint32_t table = 0;

	ret = mlx5_flow_group_to_table(attributes, external,
				       attributes->group, !!priv->fdb_def_rule,
				       &table, error);
	if (ret)
		return ret;
	if (!table)
		ret = MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL;
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
	return ret;
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
 * @param[in] hairpin
 *   Number of hairpin TX actions, 0 means classic flow.
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
		 bool external, int hairpin, struct rte_flow_error *error)
{
	int ret;
	uint64_t action_flags = 0;
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint8_t next_protocol = 0xff;
	uint16_t ether_type = 0;
	int actions_n = 0;
	uint8_t item_ipv6_proto = 0;
	const struct rte_flow_item *gre_item = NULL;
	const struct rte_flow_action_raw_decap *decap;
	const struct rte_flow_action_raw_encap *encap;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_item_tcp nic_tcp_mask = {
		.hdr = {
			.tcp_flags = 0xFF,
			.src_port = RTE_BE16(UINT16_MAX),
			.dst_port = RTE_BE16(UINT16_MAX),
		}
	};
	const struct rte_flow_item_ipv4 nic_ipv4_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.next_proto_id = 0xff,
			.time_to_live = 0xff,
		},
	};
	const struct rte_flow_item_ipv6 nic_ipv6_mask = {
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
	const struct rte_flow_item_ecpri nic_ecpri_mask = {
		.hdr = {
			.common = {
				.u32 =
				RTE_BE32(((const struct rte_ecpri_common_hdr) {
					.type = 0xFF,
					}).u32),
			},
			.dummy[0] = 0xffffffff,
		},
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *dev_conf = &priv->config;
	uint16_t queue_index = 0xFFFF;
	const struct rte_flow_item_vlan *vlan_m = NULL;
	int16_t rw_act_num = 0;
	uint64_t is_root;

	if (items == NULL)
		return -1;
	ret = flow_dv_validate_attributes(dev, attr, external, error);
	if (ret < 0)
		return ret;
	is_root = (uint64_t)ret;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int type = items->type;

		if (!mlx5_flow_os_item_supported(type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
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
			if (items->mask != NULL && items->spec != NULL) {
				ether_type =
					((const struct rte_flow_item_eth *)
					 items->spec)->type;
				ether_type &=
					((const struct rte_flow_item_eth *)
					 items->mask)->type;
				ether_type = rte_be_to_cpu_16(ether_type);
			} else {
				ether_type = 0;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = flow_dv_validate_item_vlan(items, item_flags,
							 dev, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
					     MLX5_FLOW_LAYER_OUTER_VLAN;
			if (items->mask != NULL && items->spec != NULL) {
				ether_type =
					((const struct rte_flow_item_vlan *)
					 items->spec)->inner_type;
				ether_type &=
					((const struct rte_flow_item_vlan *)
					 items->mask)->inner_type;
				ether_type = rte_be_to_cpu_16(ether_type);
			} else {
				ether_type = 0;
			}
			/* Store outer VLAN mask for of_push_vlan action. */
			if (!tunnel)
				vlan_m = items->mask;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
			ret = mlx5_flow_validate_item_ipv4(items, item_flags,
							   last_item,
							   ether_type,
							   &nic_ipv4_mask,
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
			mlx5_flow_tunnel_ip_check(items, next_protocol,
						  &item_flags, &tunnel);
			ret = mlx5_flow_validate_item_ipv6(items, item_flags,
							   last_item,
							   ether_type,
							   &nic_ipv6_mask,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6 *)
			     items->mask)->hdr.proto) {
				item_ipv6_proto =
					((const struct rte_flow_item_ipv6 *)
					 items->spec)->hdr.proto;
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
			last_item = MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_validate_item_mpls(dev, items,
							   item_flags,
							   last_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;

		case RTE_FLOW_ITEM_TYPE_MARK:
			ret = flow_dv_validate_item_mark(dev, items, attr,
							 error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_MARK;
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
			item_ipv6_proto = IPPROTO_ICMPV6;
			last_item = MLX5_FLOW_LAYER_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_TAG:
			ret = flow_dv_validate_item_tag(dev, items,
							attr, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_TAG;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		case MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			ret = flow_dv_validate_item_gtp(dev, items, item_flags,
							error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GTP;
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			/* Capacity will be checked in the translate stage. */
			ret = mlx5_flow_validate_item_ecpri(items, item_flags,
							    last_item,
							    ether_type,
							    &nic_ecpri_mask,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ECPRI;
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

		if (!mlx5_flow_os_action_supported(type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
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
			ret = flow_dv_validate_action_flag(dev, action_flags,
							   attr, error);
			if (ret < 0)
				return ret;
			if (dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
				/* Count all modify-header actions as one. */
				if (!(action_flags &
				      MLX5_FLOW_MODIFY_HDR_ACTIONS))
					++actions_n;
				action_flags |= MLX5_FLOW_ACTION_FLAG |
						MLX5_FLOW_ACTION_MARK_EXT;
			} else {
				action_flags |= MLX5_FLOW_ACTION_FLAG;
				++actions_n;
			}
			rw_act_num += MLX5_ACT_NUM_SET_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = flow_dv_validate_action_mark(dev, actions,
							   action_flags,
							   attr, error);
			if (ret < 0)
				return ret;
			if (dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
				/* Count all modify-header actions as one. */
				if (!(action_flags &
				      MLX5_FLOW_MODIFY_HDR_ACTIONS))
					++actions_n;
				action_flags |= MLX5_FLOW_ACTION_MARK |
						MLX5_FLOW_ACTION_MARK_EXT;
			} else {
				action_flags |= MLX5_FLOW_ACTION_MARK;
				++actions_n;
			}
			rw_act_num += MLX5_ACT_NUM_SET_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_META:
			ret = flow_dv_validate_action_set_meta(dev, actions,
							       action_flags,
							       attr, error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= MLX5_FLOW_ACTION_SET_META;
			rw_act_num += MLX5_ACT_NUM_SET_META;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TAG:
			ret = flow_dv_validate_action_set_tag(dev, actions,
							      action_flags,
							      attr, error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= MLX5_FLOW_ACTION_SET_TAG;
			rw_act_num += MLX5_ACT_NUM_SET_TAG;
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
			queue_index = ((const struct rte_flow_action_queue *)
							(actions->conf))->index;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			ret = mlx5_flow_validate_action_rss(actions,
							    action_flags, dev,
							    attr, item_flags,
							    error);
			if (ret < 0)
				return ret;
			if (rss != NULL && rss->queue_num)
				queue_index = rss->queue[0];
			action_flags |= MLX5_FLOW_ACTION_RSS;
			++actions_n;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			ret =
			mlx5_flow_validate_action_default_miss(action_flags,
					attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DEFAULT_MISS;
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
			ret = flow_dv_validate_action_push_vlan(dev,
								action_flags,
								vlan_m,
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
			rw_act_num += MLX5_ACT_NUM_MDF_VID;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			ret = flow_dv_validate_action_l2_encap(dev,
							       action_flags,
							       actions, attr,
							       error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			ret = flow_dv_validate_action_decap(dev, action_flags,
							    attr, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			ret = flow_dv_validate_action_raw_encap_decap
				(dev, NULL, actions->conf, attr, &action_flags,
				 &actions_n, error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			decap = actions->conf;
			while ((++actions)->type == RTE_FLOW_ACTION_TYPE_VOID)
				;
			if (actions->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
				encap = NULL;
				actions--;
			} else {
				encap = actions->conf;
			}
			ret = flow_dv_validate_action_raw_encap_decap
					   (dev,
					    decap ? decap : &empty_decap, encap,
					    attr, &action_flags, &actions_n,
					    error);
			if (ret < 0)
				return ret;
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
			/*
			 * Even if the source and destination MAC addresses have
			 * overlap in the header with 4B alignment, the convert
			 * function will handle them separately and 4 SW actions
			 * will be created. And 2 actions will be added each
			 * time no matter how many bytes of address will be set.
			 */
			rw_act_num += MLX5_ACT_NUM_MDF_MAC;
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
			rw_act_num += MLX5_ACT_NUM_MDF_IPV4;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			ret = flow_dv_validate_action_modify_ipv6(action_flags,
								  actions,
								  item_flags,
								  error);
			if (ret < 0)
				return ret;
			if (item_ipv6_proto == IPPROTO_ICMPV6)
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					actions,
					"Can't change header "
					"with ICMPv6 proto");
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
						MLX5_FLOW_ACTION_SET_IPV6_SRC :
						MLX5_FLOW_ACTION_SET_IPV6_DST;
			rw_act_num += MLX5_ACT_NUM_MDF_IPV6;
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
			rw_act_num += MLX5_ACT_NUM_MDF_PORT;
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
			rw_act_num += MLX5_ACT_NUM_MDF_TTL;
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
			rw_act_num += MLX5_ACT_NUM_MDF_TCPSEQ;
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
			rw_act_num += MLX5_ACT_NUM_MDF_TCPACK;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_MARK:
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_TAG:
		case MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG:
			rw_act_num += MLX5_ACT_NUM_SET_TAG;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			ret = mlx5_flow_validate_action_meter(dev,
							      action_flags,
							      actions, attr,
							      error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_METER;
			++actions_n;
			/* Meter action will add one more TAG action. */
			rw_act_num += MLX5_ACT_NUM_SET_TAG;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			ret = flow_dv_validate_action_age(action_flags,
							  actions, dev,
							  error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_AGE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
			ret = flow_dv_validate_action_modify_ipv4_dscp
							 (action_flags,
							  actions,
							  item_flags,
							  error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= MLX5_FLOW_ACTION_SET_IPV4_DSCP;
			rw_act_num += MLX5_ACT_NUM_SET_DSCP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
			ret = flow_dv_validate_action_modify_ipv6_dscp
								(action_flags,
								 actions,
								 item_flags,
								 error);
			if (ret < 0)
				return ret;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= MLX5_FLOW_ACTION_SET_IPV6_DSCP;
			rw_act_num += MLX5_ACT_NUM_SET_DSCP;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	/*
	 * Validate the drop action mutual exclusion with other actions.
	 * Drop action is mutually-exclusive with any other action, except for
	 * Count action.
	 */
	if ((action_flags & MLX5_FLOW_ACTION_DROP) &&
	    (action_flags & ~(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_COUNT)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Drop action is mutually-exclusive "
					  "with any other action, except for "
					  "Count action");
	/* Eswitch has few restrictions on using items and actions */
	if (attr->transfer) {
		if (!mlx5_flow_ext_mreg_supported(dev) &&
		    action_flags & MLX5_FLOW_ACTION_FLAG)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action FLAG");
		if (!mlx5_flow_ext_mreg_supported(dev) &&
		    action_flags & MLX5_FLOW_ACTION_MARK)
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
	/* Continue validation for Xcap and VLAN actions.*/
	if ((action_flags & (MLX5_FLOW_XCAP_ACTIONS |
			     MLX5_FLOW_VLAN_ACTIONS)) &&
	    (queue_index == 0xFFFF ||
	     mlx5_rxq_get_type(dev, queue_index) != MLX5_RXQ_TYPE_HAIRPIN)) {
		if ((action_flags & MLX5_FLOW_XCAP_ACTIONS) ==
		    MLX5_FLOW_XCAP_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "encap and decap "
						  "combination aren't supported");
		if (!attr->transfer && attr->ingress) {
			if (action_flags & MLX5_FLOW_ACTION_ENCAP)
				return rte_flow_error_set
						(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL, "encap is not supported"
						 " for ingress traffic");
			else if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN)
				return rte_flow_error_set
						(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL, "push VLAN action not "
						 "supported for ingress");
			else if ((action_flags & MLX5_FLOW_VLAN_ACTIONS) ==
					MLX5_FLOW_VLAN_ACTIONS)
				return rte_flow_error_set
						(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL, "no support for "
						 "multiple VLAN actions");
		}
	}
	/* Hairpin flow will add one more TAG action. */
	if (hairpin > 0)
		rw_act_num += MLX5_ACT_NUM_SET_TAG;
	/* extra metadata enabled: one more TAG action will be add. */
	if (dev_conf->dv_flow_en &&
	    dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    mlx5_flow_ext_mreg_supported(dev))
		rw_act_num += MLX5_ACT_NUM_SET_TAG;
	if ((uint32_t)rw_act_num >
			flow_dv_modify_hdr_action_max(dev, is_root)) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "too many header modify"
					  " actions to support");
	}
	return 0;
}

/**
 * Internal preparation function. Allocates the DV flow size,
 * this size is constant.
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
 *   Pointer to mlx5_flow object on success,
 *   otherwise NULL and rte_errno is set.
 */
static struct mlx5_flow *
flow_dv_prepare(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr __rte_unused,
		const struct rte_flow_item items[] __rte_unused,
		const struct rte_flow_action actions[] __rte_unused,
		struct rte_flow_error *error)
{
	uint32_t handle_idx = 0;
	struct mlx5_flow *dev_flow;
	struct mlx5_flow_handle *dev_handle;
	struct mlx5_priv *priv = dev->data->dev_private;

	/* In case of corrupting the memory. */
	if (priv->flow_idx >= MLX5_NUM_MAX_DEV_FLOWS) {
		rte_flow_error_set(error, ENOSPC,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not free temporary device flow");
		return NULL;
	}
	dev_handle = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
				   &handle_idx);
	if (!dev_handle) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "not enough memory to create flow handle");
		return NULL;
	}
	/* No multi-thread supporting. */
	dev_flow = &((struct mlx5_flow *)priv->inter_flows)[priv->flow_idx++];
	dev_flow->handle = dev_handle;
	dev_flow->handle_idx = handle_idx;
	/*
	 * In some old rdma-core releases, before continuing, a check of the
	 * length of matching parameter will be done at first. It needs to use
	 * the length without misc4 param. If the flow has misc4 support, then
	 * the length needs to be adjusted accordingly. Each param member is
	 * aligned with a 64B boundary naturally.
	 */
	dev_flow->dv.value.size = MLX5_ST_SZ_BYTES(fte_match_param) -
				  MLX5_ST_SZ_BYTES(fte_match_set_misc4);
	/*
	 * The matching value needs to be cleared to 0 before using. In the
	 * past, it will be automatically cleared when using rte_*alloc
	 * API. The time consumption will be almost the same as before.
	 */
	memset(dev_flow->dv.value.buf, 0, MLX5_ST_SZ_BYTES(fte_match_param));
	dev_flow->ingress = attr->ingress;
	dev_flow->dv.transfer = attr->transfer;
	return dev_flow;
}

#ifdef RTE_LIBRTE_MLX5_DEBUG
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
 * Add match of ip_version.
 *
 * @param[in] group
 *   Flow group.
 * @param[in] headers_v
 *   Values header pointer.
 * @param[in] headers_m
 *   Masks header pointer.
 * @param[in] ip_version
 *   The IP version to set.
 */
static inline void
flow_dv_set_match_ip_version(uint32_t group,
			     void *headers_v,
			     void *headers_m,
			     uint8_t ip_version)
{
	if (group == 0)
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version, 0xf);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_version,
			 ip_version);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version, ip_version);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ethertype, 0);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ethertype, 0);
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
			   const struct rte_flow_item *item, int inner,
			   uint32_t group)
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
	if (eth_v->type) {
		/* When ethertype is present set mask for tagged VLAN. */
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, cvlan_tag, 1);
		/* Set value for tagged VLAN if ethertype is 802.1Q. */
		if (eth_v->type == RTE_BE16(RTE_ETHER_TYPE_VLAN) ||
		    eth_v->type == RTE_BE16(RTE_ETHER_TYPE_QINQ)) {
			MLX5_SET(fte_match_set_lyr_2_4, headers_v, cvlan_tag,
				 1);
			/* Return here to avoid setting match on ethertype. */
			return;
		}
	}
	/*
	 * HW supports match on one Ethertype, the Ethertype following the last
	 * VLAN tag of the packet (see PRM).
	 * Set match on ethertype only if ETH header is not followed by VLAN.
	 * HW is optimized for IPv4/IPv6. In such cases, avoid setting
	 * ethertype, and use ip_version field instead.
	 * eCPRI over Ether layer will use type value 0xAEFE.
	 */
	if (eth_v->type == RTE_BE16(RTE_ETHER_TYPE_IPV4) &&
	    eth_m->type == 0xFFFF) {
		flow_dv_set_match_ip_version(group, headers_v, headers_m, 4);
	} else if (eth_v->type == RTE_BE16(RTE_ETHER_TYPE_IPV6) &&
		   eth_m->type == 0xFFFF) {
		flow_dv_set_match_ip_version(group, headers_v, headers_m, 6);
	} else {
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ethertype,
			 rte_be_to_cpu_16(eth_m->type));
		l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
				     ethertype);
		*(uint16_t *)(l24_v) = eth_m->type & eth_v->type;
	}
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
			    int inner, uint32_t group)
{
	const struct rte_flow_item_vlan *vlan_m = item->mask;
	const struct rte_flow_item_vlan *vlan_v = item->spec;
	void *headers_m;
	void *headers_v;
	uint16_t tci_m;
	uint16_t tci_v;

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
		if (vlan_v)
			dev_flow->handle->vf_vlan.tag =
					rte_be_to_cpu_16(vlan_v->tci) & 0x0fff;
	}
	/*
	 * When VLAN item exists in flow, mark packet as tagged,
	 * even if TCI is not specified.
	 */
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, cvlan_tag, 1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, cvlan_tag, 1);
	if (!vlan_v)
		return;
	if (!vlan_m)
		vlan_m = &rte_flow_item_vlan_mask;
	tci_m = rte_be_to_cpu_16(vlan_m->tci);
	tci_v = rte_be_to_cpu_16(vlan_m->tci & vlan_v->tci);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_vid, tci_m);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_vid, tci_v);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_cfi, tci_m >> 12);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_cfi, tci_v >> 12);
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, first_prio, tci_m >> 13);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, first_prio, tci_v >> 13);
	/*
	 * HW is optimized for IPv4/IPv6. In such cases, avoid setting
	 * ethertype, and use ip_version field instead.
	 */
	if (vlan_v->inner_type == RTE_BE16(RTE_ETHER_TYPE_IPV4) &&
	    vlan_m->inner_type == 0xFFFF) {
		flow_dv_set_match_ip_version(group, headers_v, headers_m, 4);
	} else if (vlan_v->inner_type == RTE_BE16(RTE_ETHER_TYPE_IPV6) &&
		   vlan_m->inner_type == 0xFFFF) {
		flow_dv_set_match_ip_version(group, headers_v, headers_m, 6);
	} else {
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, ethertype,
			 rte_be_to_cpu_16(vlan_m->inner_type));
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ethertype,
			 rte_be_to_cpu_16(vlan_m->inner_type &
					  vlan_v->inner_type));
	}
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
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] group
 *   The group to insert the rule.
 */
static void
flow_dv_translate_item_ipv4(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    const uint64_t item_flags,
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
			.time_to_live = 0xff,
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
	flow_dv_set_match_ip_version(group, headers_v, headers_m, 4);
	/*
	 * On outer header (which must contains L2), or inner header with L2,
	 * set cvlan_tag mask bit to mark this packet as untagged.
	 * This should be done even if item->spec is empty.
	 */
	if (!inner || item_flags & MLX5_FLOW_LAYER_INNER_L2)
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, cvlan_tag, 1);
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
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_ttl_hoplimit,
		 ipv4_m->hdr.time_to_live);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ttl_hoplimit,
		 ipv4_v->hdr.time_to_live & ipv4_m->hdr.time_to_live);
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
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] group
 *   The group to insert the rule.
 */
static void
flow_dv_translate_item_ipv6(void *matcher, void *key,
			    const struct rte_flow_item *item,
			    const uint64_t item_flags,
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
	flow_dv_set_match_ip_version(group, headers_v, headers_m, 6);
	/*
	 * On outer header (which must contains L2), or inner header with L2,
	 * set cvlan_tag mask bit to mark this packet as untagged.
	 * This should be done even if item->spec is empty.
	 */
	if (!inner || item_flags & MLX5_FLOW_LAYER_INNER_L2)
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, cvlan_tag, 1);
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
	/* Hop limit. */
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, ip_ttl_hoplimit,
		 ipv6_m->hdr.hop_limits);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ttl_hoplimit,
		 ipv6_v->hdr.hop_limits & ipv6_m->hdr.hop_limits);
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

	/* GRE K bit must be on and should already be validated */
	MLX5_SET(fte_match_set_misc, misc_m, gre_k_present, 1);
	MLX5_SET(fte_match_set_misc, misc_v, gre_k_present, 1);
	if (!key_v)
		return;
	if (!key_m)
		key_m = &gre_key_default_mask;
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
	const char *tni_flow_id_m;
	const char *tni_flow_id_v;
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
	tni_flow_id_m = (const char *)nvgre_m->tni;
	tni_flow_id_v = (const char *)nvgre_v->tni;
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
 * Add VXLAN-GPE item to matcher and to the value.
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
flow_dv_translate_item_vxlan_gpe(void *matcher, void *key,
				 const struct rte_flow_item *item, int inner)
{
	const struct rte_flow_item_vxlan_gpe *vxlan_m = item->mask;
	const struct rte_flow_item_vxlan_gpe *vxlan_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters_3);
	void *misc_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	char *vni_m;
	char *vni_v;
	uint16_t dport;
	int size;
	int i;
	uint8_t flags_m = 0xff;
	uint8_t flags_v = 0xc;

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
		vxlan_m = &rte_flow_item_vxlan_gpe_mask;
	size = sizeof(vxlan_m->vni);
	vni_m = MLX5_ADDR_OF(fte_match_set_misc3, misc_m, outer_vxlan_gpe_vni);
	vni_v = MLX5_ADDR_OF(fte_match_set_misc3, misc_v, outer_vxlan_gpe_vni);
	memcpy(vni_m, vxlan_m->vni, size);
	for (i = 0; i < size; ++i)
		vni_v[i] = vni_m[i] & vxlan_v->vni[i];
	if (vxlan_m->flags) {
		flags_m = vxlan_m->flags;
		flags_v = vxlan_v->flags;
	}
	MLX5_SET(fte_match_set_misc3, misc_m, outer_vxlan_gpe_flags, flags_m);
	MLX5_SET(fte_match_set_misc3, misc_v, outer_vxlan_gpe_flags, flags_v);
	MLX5_SET(fte_match_set_misc3, misc_m, outer_vxlan_gpe_next_protocol,
		 vxlan_m->protocol);
	MLX5_SET(fte_match_set_misc3, misc_v, outer_vxlan_gpe_next_protocol,
		 vxlan_v->protocol);
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
 * Add metadata register item to matcher
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] reg_type
 *   Type of device metadata register
 * @param[in] value
 *   Register value
 * @param[in] mask
 *   Register mask
 */
static void
flow_dv_match_meta_reg(void *matcher, void *key,
		       enum modify_reg reg_type,
		       uint32_t data, uint32_t mask)
{
	void *misc2_m =
		MLX5_ADDR_OF(fte_match_param, matcher, misc_parameters_2);
	void *misc2_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);
	uint32_t temp;

	data &= mask;
	switch (reg_type) {
	case REG_A:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_a, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_a, data);
		break;
	case REG_B:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_b, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_b, data);
		break;
	case REG_C_0:
		/*
		 * The metadata register C0 field might be divided into
		 * source vport index and META item value, we should set
		 * this field according to specified mask, not as whole one.
		 */
		temp = MLX5_GET(fte_match_set_misc2, misc2_m, metadata_reg_c_0);
		temp |= mask;
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_0, temp);
		temp = MLX5_GET(fte_match_set_misc2, misc2_v, metadata_reg_c_0);
		temp &= ~mask;
		temp |= data;
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_0, temp);
		break;
	case REG_C_1:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_1, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_1, data);
		break;
	case REG_C_2:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_2, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_2, data);
		break;
	case REG_C_3:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_3, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_3, data);
		break;
	case REG_C_4:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_4, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_4, data);
		break;
	case REG_C_5:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_5, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_5, data);
		break;
	case REG_C_6:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_6, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_6, data);
		break;
	case REG_C_7:
		MLX5_SET(fte_match_set_misc2, misc2_m, metadata_reg_c_7, mask);
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_7, data);
		break;
	default:
		MLX5_ASSERT(false);
		break;
	}
}

/**
 * Add MARK item to matcher
 *
 * @param[in] dev
 *   The device to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_item_mark(struct rte_eth_dev *dev,
			    void *matcher, void *key,
			    const struct rte_flow_item *item)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_mark *mark;
	uint32_t value;
	uint32_t mask;

	mark = item->mask ? (const void *)item->mask :
			    &rte_flow_item_mark_mask;
	mask = mark->id & priv->sh->dv_mark_mask;
	mark = (const void *)item->spec;
	MLX5_ASSERT(mark);
	value = mark->id & priv->sh->dv_mark_mask & mask;
	if (mask) {
		enum modify_reg reg;

		/* Get the metadata register index for the mark. */
		reg = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, NULL);
		MLX5_ASSERT(reg > 0);
		if (reg == REG_C_0) {
			struct mlx5_priv *priv = dev->data->dev_private;
			uint32_t msk_c0 = priv->sh->dv_regc0_mask;
			uint32_t shl_c0 = rte_bsf32(msk_c0);

			mask &= msk_c0;
			mask <<= shl_c0;
			value <<= shl_c0;
		}
		flow_dv_match_meta_reg(matcher, key, reg, value, mask);
	}
}

/**
 * Add META item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[in] item
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_item_meta(struct rte_eth_dev *dev,
			    void *matcher, void *key,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item *item)
{
	const struct rte_flow_item_meta *meta_m;
	const struct rte_flow_item_meta *meta_v;

	meta_m = (const void *)item->mask;
	if (!meta_m)
		meta_m = &rte_flow_item_meta_mask;
	meta_v = (const void *)item->spec;
	if (meta_v) {
		int reg;
		uint32_t value = meta_v->data;
		uint32_t mask = meta_m->data;

		reg = flow_dv_get_metadata_reg(dev, attr, NULL);
		if (reg < 0)
			return;
		/*
		 * In datapath code there is no endianness
		 * coversions for perfromance reasons, all
		 * pattern conversions are done in rte_flow.
		 */
		value = rte_cpu_to_be_32(value);
		mask = rte_cpu_to_be_32(mask);
		if (reg == REG_C_0) {
			struct mlx5_priv *priv = dev->data->dev_private;
			uint32_t msk_c0 = priv->sh->dv_regc0_mask;
			uint32_t shl_c0 = rte_bsf32(msk_c0);
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
			uint32_t shr_c0 = __builtin_clz(priv->sh->dv_meta_mask);

			value >>= shr_c0;
			mask >>= shr_c0;
#endif
			value <<= shl_c0;
			mask <<= shl_c0;
			MLX5_ASSERT(msk_c0);
			MLX5_ASSERT(!(~msk_c0 & mask));
		}
		flow_dv_match_meta_reg(matcher, key, reg, value, mask);
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
	flow_dv_match_meta_reg(matcher, key, REG_C_0, value, mask);
}

/**
 * Add tag item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_mlx5_item_tag(struct rte_eth_dev *dev,
				void *matcher, void *key,
				const struct rte_flow_item *item)
{
	const struct mlx5_rte_flow_item_tag *tag_v = item->spec;
	const struct mlx5_rte_flow_item_tag *tag_m = item->mask;
	uint32_t mask, value;

	MLX5_ASSERT(tag_v);
	value = tag_v->data;
	mask = tag_m ? tag_m->data : UINT32_MAX;
	if (tag_v->id == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0 = rte_bsf32(msk_c0);

		mask &= msk_c0;
		mask <<= shl_c0;
		value <<= shl_c0;
	}
	flow_dv_match_meta_reg(matcher, key, tag_v->id, value, mask);
}

/**
 * Add TAG item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 */
static void
flow_dv_translate_item_tag(struct rte_eth_dev *dev,
			   void *matcher, void *key,
			   const struct rte_flow_item *item)
{
	const struct rte_flow_item_tag *tag_v = item->spec;
	const struct rte_flow_item_tag *tag_m = item->mask;
	enum modify_reg reg;

	MLX5_ASSERT(tag_v);
	tag_m = tag_m ? tag_m : &rte_flow_item_tag_mask;
	/* Get the metadata register index for the tag. */
	reg = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, tag_v->index, NULL);
	MLX5_ASSERT(reg > 0);
	flow_dv_match_meta_reg(matcher, key, reg, tag_v->data, tag_m->data);
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
	priv = mlx5_port_to_eswitch_info(id, item == NULL);
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
	/*
	 * Force flow only to match the non-fragmented IPv6 ICMPv6 packets.
	 * If only the protocol is specified, no need to match the frag.
	 */
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, frag, 1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, frag, 0);
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
	/*
	 * Force flow only to match the non-fragmented IPv4 ICMP packets.
	 * If only the protocol is specified, no need to match the frag.
	 */
	MLX5_SET(fte_match_set_lyr_2_4, headers_m, frag, 1);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, frag, 0);
	MLX5_SET(fte_match_set_misc3, misc3_m, icmp_type,
		 icmp_m->hdr.icmp_type);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_type,
		 icmp_v->hdr.icmp_type & icmp_m->hdr.icmp_type);
	MLX5_SET(fte_match_set_misc3, misc3_m, icmp_code,
		 icmp_m->hdr.icmp_code);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_code,
		 icmp_v->hdr.icmp_code & icmp_m->hdr.icmp_code);
}

/**
 * Add GTP item to matcher and to the value.
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
flow_dv_translate_item_gtp(void *matcher, void *key,
			   const struct rte_flow_item *item, int inner)
{
	const struct rte_flow_item_gtp *gtp_m = item->mask;
	const struct rte_flow_item_gtp *gtp_v = item->spec;
	void *headers_m;
	void *headers_v;
	void *misc3_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_3);
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	uint16_t dport = RTE_GTPU_UDP_PORT;

	if (inner) {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 inner_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		headers_m = MLX5_ADDR_OF(fte_match_param, matcher,
					 outer_headers);
		headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	}
	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		MLX5_SET(fte_match_set_lyr_2_4, headers_m, udp_dport, 0xFFFF);
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport, dport);
	}
	if (!gtp_v)
		return;
	if (!gtp_m)
		gtp_m = &rte_flow_item_gtp_mask;
	MLX5_SET(fte_match_set_misc3, misc3_m, gtpu_msg_flags,
		 gtp_m->v_pt_rsv_flags);
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_msg_flags,
		 gtp_v->v_pt_rsv_flags & gtp_m->v_pt_rsv_flags);
	MLX5_SET(fte_match_set_misc3, misc3_m, gtpu_msg_type, gtp_m->msg_type);
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_msg_type,
		 gtp_v->msg_type & gtp_m->msg_type);
	MLX5_SET(fte_match_set_misc3, misc3_m, gtpu_teid,
		 rte_be_to_cpu_32(gtp_m->teid));
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_teid,
		 rte_be_to_cpu_32(gtp_v->teid & gtp_m->teid));
}

/**
 * Add eCPRI item to matcher and to the value.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] samples
 *   Sample IDs to be used in the matching.
 */
static void
flow_dv_translate_item_ecpri(struct rte_eth_dev *dev, void *matcher,
			     void *key, const struct rte_flow_item *item)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_ecpri *ecpri_m = item->mask;
	const struct rte_flow_item_ecpri *ecpri_v = item->spec;
	void *misc4_m = MLX5_ADDR_OF(fte_match_param, matcher,
				     misc_parameters_4);
	void *misc4_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_4);
	uint32_t *samples;
	void *dw_m;
	void *dw_v;

	if (!ecpri_v)
		return;
	if (!ecpri_m)
		ecpri_m = &rte_flow_item_ecpri_mask;
	/*
	 * Maximal four DW samples are supported in a single matching now.
	 * Two are used now for a eCPRI matching:
	 * 1. Type: one byte, mask should be 0x00ff0000 in network order
	 * 2. ID of a message: one or two bytes, mask 0xffff0000 or 0xff000000
	 *    if any.
	 */
	if (!ecpri_m->hdr.common.u32)
		return;
	samples = priv->sh->fp[MLX5_FLEX_PARSER_ECPRI_0].ids;
	/* Need to take the whole DW as the mask to fill the entry. */
	dw_m = MLX5_ADDR_OF(fte_match_set_misc4, misc4_m,
			    prog_sample_field_value_0);
	dw_v = MLX5_ADDR_OF(fte_match_set_misc4, misc4_v,
			    prog_sample_field_value_0);
	/* Already big endian (network order) in the header. */
	*(uint32_t *)dw_m = ecpri_m->hdr.common.u32;
	*(uint32_t *)dw_v = ecpri_v->hdr.common.u32;
	/* Sample#0, used for matching type, offset 0. */
	MLX5_SET(fte_match_set_misc4, misc4_m,
		 prog_sample_field_id_0, samples[0]);
	/* It makes no sense to set the sample ID in the mask field. */
	MLX5_SET(fte_match_set_misc4, misc4_v,
		 prog_sample_field_id_0, samples[0]);
	/*
	 * Checking if message body part needs to be matched.
	 * Some wildcard rules only matching type field should be supported.
	 */
	if (ecpri_m->hdr.dummy[0]) {
		switch (ecpri_v->hdr.common.type) {
		case RTE_ECPRI_MSG_TYPE_IQ_DATA:
		case RTE_ECPRI_MSG_TYPE_RTC_CTRL:
		case RTE_ECPRI_MSG_TYPE_DLY_MSR:
			dw_m = MLX5_ADDR_OF(fte_match_set_misc4, misc4_m,
					    prog_sample_field_value_1);
			dw_v = MLX5_ADDR_OF(fte_match_set_misc4, misc4_v,
					    prog_sample_field_value_1);
			*(uint32_t *)dw_m = ecpri_m->hdr.dummy[0];
			*(uint32_t *)dw_v = ecpri_v->hdr.dummy[0];
			/* Sample#1, to match message body, offset 4. */
			MLX5_SET(fte_match_set_misc4, misc4_m,
				 prog_sample_field_id_1, samples[1]);
			MLX5_SET(fte_match_set_misc4, misc4_v,
				 prog_sample_field_id_1, samples[1]);
			break;
		default:
			/* Others, do not match any sample ID. */
			break;
		}
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
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_3)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC3_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_4)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC4_BIT;
	return match_criteria_enable;
}


/**
 * Get a flow table.
 *
 * @param[in, out] dev
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_tbl_resource *tbl;
	union mlx5_flow_tbl_key table_key = {
		{
			.table_id = table_id,
			.reserved = 0,
			.domain = !!transfer,
			.direction = !!egress,
		}
	};
	struct mlx5_hlist_entry *pos = mlx5_hlist_lookup(sh->flow_tbls,
							 table_key.v64);
	struct mlx5_flow_tbl_data_entry *tbl_data;
	uint32_t idx = 0;
	int ret;
	void *domain;

	if (pos) {
		tbl_data = container_of(pos, struct mlx5_flow_tbl_data_entry,
					entry);
		tbl = &tbl_data->tbl;
		rte_atomic32_inc(&tbl->refcnt);
		return tbl;
	}
	tbl_data = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_JUMP], &idx);
	if (!tbl_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	tbl_data->idx = idx;
	tbl = &tbl_data->tbl;
	pos = &tbl_data->entry;
	if (transfer)
		domain = sh->fdb_domain;
	else if (egress)
		domain = sh->tx_domain;
	else
		domain = sh->rx_domain;
	ret = mlx5_flow_os_create_flow_tbl(domain, table_id, &tbl->obj);
	if (ret) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create flow table object");
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], idx);
		return NULL;
	}
	/*
	 * No multi-threads now, but still better to initialize the reference
	 * count before insert it into the hash list.
	 */
	rte_atomic32_init(&tbl->refcnt);
	/* Jump action reference count is initialized here. */
	rte_atomic32_init(&tbl_data->jump.refcnt);
	pos->key = table_key.v64;
	ret = mlx5_hlist_insert(sh->flow_tbls, pos);
	if (ret < 0) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot insert flow table data entry");
		mlx5_flow_os_destroy_flow_tbl(tbl->obj);
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], idx);
	}
	rte_atomic32_inc(&tbl->refcnt);
	return tbl;
}

/**
 * Release a flow table.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] tbl
 *   Table resource to be released.
 *
 * @return
 *   Returns 0 if table was released, else return 1;
 */
static int
flow_dv_tbl_resource_release(struct rte_eth_dev *dev,
			     struct mlx5_flow_tbl_resource *tbl)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_tbl_data_entry *tbl_data =
		container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);

	if (!tbl)
		return 0;
	if (rte_atomic32_dec_and_test(&tbl->refcnt)) {
		struct mlx5_hlist_entry *pos = &tbl_data->entry;

		mlx5_flow_os_destroy_flow_tbl(tbl->obj);
		tbl->obj = NULL;
		/* remove the entry from the hash list and free memory. */
		mlx5_hlist_remove(sh->flow_tbls, pos);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_JUMP],
				tbl_data->idx);
		return 0;
	}
	return 1;
}

/**
 * Register the flow matcher.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] matcher
 *   Pointer to flow matcher.
 * @param[in, out] key
 *   Pointer to flow table key.
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
			 union mlx5_flow_tbl_key *key,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_matcher *cache_matcher;
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.match_mask = (void *)&matcher->mask,
	};
	struct mlx5_flow_tbl_resource *tbl;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	int ret;

	tbl = flow_dv_tbl_resource_get(dev, key->table_id, key->direction,
				       key->domain, error);
	if (!tbl)
		return -rte_errno;	/* No need to refill the error info */
	tbl_data = container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);
	/* Lookup from cache. */
	LIST_FOREACH(cache_matcher, &tbl_data->matchers, next) {
		if (matcher->crc == cache_matcher->crc &&
		    matcher->priority == cache_matcher->priority &&
		    !memcmp((const void *)matcher->mask.buf,
			    (const void *)cache_matcher->mask.buf,
			    cache_matcher->mask.size)) {
			DRV_LOG(DEBUG,
				"%s group %u priority %hd use %s "
				"matcher %p: refcnt %d++",
				key->domain ? "FDB" : "NIC", key->table_id,
				cache_matcher->priority,
				key->direction ? "tx" : "rx",
				(void *)cache_matcher,
				rte_atomic32_read(&cache_matcher->refcnt));
			rte_atomic32_inc(&cache_matcher->refcnt);
			dev_flow->handle->dvh.matcher = cache_matcher;
			/* old matcher should not make the table ref++. */
			flow_dv_tbl_resource_release(dev, tbl);
			return 0;
		}
	}
	/* Register new matcher. */
	cache_matcher = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*cache_matcher), 0,
				    SOCKET_ID_ANY);
	if (!cache_matcher) {
		flow_dv_tbl_resource_release(dev, tbl);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate matcher memory");
	}
	*cache_matcher = *matcher;
	dv_attr.match_criteria_enable =
		flow_dv_matcher_enable(cache_matcher->mask.buf);
	dv_attr.priority = matcher->priority;
	if (key->direction)
		dv_attr.flags |= IBV_FLOW_ATTR_FLAGS_EGRESS;
	ret = mlx5_flow_os_create_flow_matcher(sh->ctx, &dv_attr, tbl->obj,
					       &cache_matcher->matcher_object);
	if (ret) {
		mlx5_free(cache_matcher);
#ifdef HAVE_MLX5DV_DR
		flow_dv_tbl_resource_release(dev, tbl);
#endif
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create matcher");
	}
	/* Save the table information */
	cache_matcher->tbl = tbl;
	rte_atomic32_init(&cache_matcher->refcnt);
	/* only matcher ref++, table ref++ already done above in get API. */
	rte_atomic32_inc(&cache_matcher->refcnt);
	LIST_INSERT_HEAD(&tbl_data->matchers, cache_matcher, next);
	dev_flow->handle->dvh.matcher = cache_matcher;
	DRV_LOG(DEBUG, "%s group %u priority %hd new %s matcher %p: refcnt %d",
		key->domain ? "FDB" : "NIC", key->table_id,
		cache_matcher->priority,
		key->direction ? "tx" : "rx", (void *)cache_matcher,
		rte_atomic32_read(&cache_matcher->refcnt));
	return 0;
}

/**
 * Find existing tag resource or create and register a new one.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] tag_be24
 *   Tag value in big endian then R-shift 8.
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
			 uint32_t tag_be24,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_tag_resource *cache_resource;
	struct mlx5_hlist_entry *entry;
	int ret;

	/* Lookup a matching resource from cache. */
	entry = mlx5_hlist_lookup(sh->tag_table, (uint64_t)tag_be24);
	if (entry) {
		cache_resource = container_of
			(entry, struct mlx5_flow_dv_tag_resource, entry);
		rte_atomic32_inc(&cache_resource->refcnt);
		dev_flow->handle->dvh.rix_tag = cache_resource->idx;
		dev_flow->dv.tag_resource = cache_resource;
		DRV_LOG(DEBUG, "cached tag resource %p: refcnt now %d++",
			(void *)cache_resource,
			rte_atomic32_read(&cache_resource->refcnt));
		return 0;
	}
	/* Register new resource. */
	cache_resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_TAG],
				       &dev_flow->handle->dvh.rix_tag);
	if (!cache_resource)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate resource memory");
	cache_resource->entry.key = (uint64_t)tag_be24;
	ret = mlx5_flow_os_create_flow_action_tag(tag_be24,
						  &cache_resource->action);
	if (ret) {
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot create action");
	}
	rte_atomic32_init(&cache_resource->refcnt);
	rte_atomic32_inc(&cache_resource->refcnt);
	if (mlx5_hlist_insert(sh->tag_table, &cache_resource->entry)) {
		mlx5_flow_os_destroy_flow_action(cache_resource->action);
		mlx5_free(cache_resource);
		return rte_flow_error_set(error, EEXIST,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "cannot insert tag");
	}
	dev_flow->dv.tag_resource = cache_resource;
	DRV_LOG(DEBUG, "new tag resource %p: refcnt now %d++",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	return 0;
}

/**
 * Release the tag.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param tag_idx
 *   Tag index.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_tag_release(struct rte_eth_dev *dev,
		    uint32_t tag_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_tag_resource *tag;

	tag = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_TAG], tag_idx);
	if (!tag)
		return 0;
	DRV_LOG(DEBUG, "port %u tag %p: refcnt %d--",
		dev->data->port_id, (void *)tag,
		rte_atomic32_read(&tag->refcnt));
	if (rte_atomic32_dec_and_test(&tag->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action(tag->action));
		mlx5_hlist_remove(sh->tag_table, &tag->entry);
		DRV_LOG(DEBUG, "port %u tag %p: removed",
			dev->data->port_id, (void *)tag);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_TAG], tag_idx);
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
	priv = mlx5_port_to_eswitch_info(port, false);
	if (!priv)
		return rte_flow_error_set(error, -rte_errno,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "No eswitch info was found for port");
#ifdef HAVE_MLX5DV_DR_DEVX_PORT
	/*
	 * This parameter is transferred to
	 * mlx5dv_dr_action_create_dest_ib_port().
	 */
	*dst_port_id = priv->dev_port;
#else
	/*
	 * Legacy mode, no LAG configurations is supported.
	 * This parameter is transferred to
	 * mlx5dv_dr_action_create_dest_vport().
	 */
	*dst_port_id = priv->vport_id;
#endif
	return 0;
}

/**
 * Create a counter with aging configuration.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[out] count
 *   Pointer to the counter action configuration.
 * @param[in] age
 *   Pointer to the aging action configuration.
 *
 * @return
 *   Index to flow counter on success, 0 otherwise.
 */
static uint32_t
flow_dv_translate_create_counter(struct rte_eth_dev *dev,
				struct mlx5_flow *dev_flow,
				const struct rte_flow_action_count *count,
				const struct rte_flow_action_age *age)
{
	uint32_t counter;
	struct mlx5_age_param *age_param;

	counter = flow_dv_counter_alloc(dev,
				count ? count->shared : 0,
				count ? count->id : 0,
				dev_flow->dv.group, !!age);
	if (!counter || age == NULL)
		return counter;
	age_param  = flow_dv_counter_idx_get_age(dev, counter);
	/*
	 * The counter age accuracy may have a bit delay. Have 3/4
	 * second bias on the timeount in order to let it age in time.
	 */
	age_param->context = age->context ? age->context :
		(void *)(uintptr_t)(dev_flow->flow_idx);
	/*
	 * The counter age accuracy may have a bit delay. Have 3/4
	 * second bias on the timeount in order to let it age in time.
	 */
	age_param->timeout = age->timeout * 10 - MLX5_AGING_TIME_DELAY;
	/* Set expire time in unit of 0.1 sec. */
	age_param->port_id = dev->data->port_id;
	age_param->expire = age_param->timeout +
			rte_rdtsc() / (rte_get_tsc_hz() / 10);
	rte_atomic16_set(&age_param->state, AGE_CANDIDATE);
	return counter;
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
 * Set the hash fields according to the @p flow information.
 *
 * @param[in] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] rss_desc
 *   Pointer to the mlx5_flow_rss_desc.
 */
static void
flow_dv_hashfields_set(struct mlx5_flow *dev_flow,
		       struct mlx5_flow_rss_desc *rss_desc)
{
	uint64_t items = dev_flow->handle->layers;
	int rss_inner = 0;
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_desc->types);

	dev_flow->hash_fields = 0;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (rss_desc->level >= 2) {
		dev_flow->hash_fields |= IBV_RX_HASH_INNER;
		rss_inner = 1;
	}
#endif
	if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L3_IPV4)) ||
	    (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L3_IPV4))) {
		if (rss_types & MLX5_IPV4_LAYER_TYPES) {
			if (rss_types & ETH_RSS_L3_SRC_ONLY)
				dev_flow->hash_fields |= IBV_RX_HASH_SRC_IPV4;
			else if (rss_types & ETH_RSS_L3_DST_ONLY)
				dev_flow->hash_fields |= IBV_RX_HASH_DST_IPV4;
			else
				dev_flow->hash_fields |= MLX5_IPV4_IBV_RX_HASH;
		}
	} else if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L3_IPV6)) ||
		   (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L3_IPV6))) {
		if (rss_types & MLX5_IPV6_LAYER_TYPES) {
			if (rss_types & ETH_RSS_L3_SRC_ONLY)
				dev_flow->hash_fields |= IBV_RX_HASH_SRC_IPV6;
			else if (rss_types & ETH_RSS_L3_DST_ONLY)
				dev_flow->hash_fields |= IBV_RX_HASH_DST_IPV6;
			else
				dev_flow->hash_fields |= MLX5_IPV6_IBV_RX_HASH;
		}
	}
	if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L4_UDP)) ||
	    (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L4_UDP))) {
		if (rss_types & ETH_RSS_UDP) {
			if (rss_types & ETH_RSS_L4_SRC_ONLY)
				dev_flow->hash_fields |=
						IBV_RX_HASH_SRC_PORT_UDP;
			else if (rss_types & ETH_RSS_L4_DST_ONLY)
				dev_flow->hash_fields |=
						IBV_RX_HASH_DST_PORT_UDP;
			else
				dev_flow->hash_fields |= MLX5_UDP_IBV_RX_HASH;
		}
	} else if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L4_TCP)) ||
		   (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L4_TCP))) {
		if (rss_types & ETH_RSS_TCP) {
			if (rss_types & ETH_RSS_L4_SRC_ONLY)
				dev_flow->hash_fields |=
						IBV_RX_HASH_SRC_PORT_TCP;
			else if (rss_types & ETH_RSS_L4_DST_ONLY)
				dev_flow->hash_fields |=
						IBV_RX_HASH_DST_PORT_TCP;
			else
				dev_flow->hash_fields |= MLX5_TCP_IBV_RX_HASH;
		}
	}
}

/**
 * Fill the flow with DV spec, lock free
 * (mutex should be acquired by caller).
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
__flow_dv_translate(struct rte_eth_dev *dev,
		    struct mlx5_flow *dev_flow,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item items[],
		    const struct rte_flow_action actions[],
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *dev_conf = &priv->config;
	struct rte_flow *flow = dev_flow->flow;
	struct mlx5_flow_handle *handle = dev_flow->handle;
	struct mlx5_flow_rss_desc *rss_desc = &((struct mlx5_flow_rss_desc *)
					      priv->rss_desc)
					      [!!priv->flow_nested_idx];
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint64_t action_flags = 0;
	uint64_t priority = attr->priority;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf) -
				MLX5_ST_SZ_BYTES(fte_match_set_misc4),
		},
	};
	int actions_n = 0;
	bool actions_end = false;
	union {
		struct mlx5_flow_dv_modify_hdr_resource res;
		uint8_t len[sizeof(struct mlx5_flow_dv_modify_hdr_resource) +
			    sizeof(struct mlx5_modification_cmd) *
			    (MLX5_MAX_MODIFY_NUM + 1)];
	} mhdr_dummy;
	struct mlx5_flow_dv_modify_hdr_resource *mhdr_res = &mhdr_dummy.res;
	const struct rte_flow_action_count *count = NULL;
	const struct rte_flow_action_age *age = NULL;
	union flow_dv_attr flow_attr = { .attr = 0 };
	uint32_t tag_be;
	union mlx5_flow_tbl_key tbl_key;
	uint32_t modify_action_position = UINT32_MAX;
	void *match_mask = matcher.mask.buf;
	void *match_value = dev_flow->dv.value.buf;
	uint8_t next_protocol = 0xff;
	struct rte_vlan_hdr vlan = { 0 };
	uint32_t table;
	int ret = 0;

	mhdr_res->ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					   MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	ret = mlx5_flow_group_to_table(attr, dev_flow->external, attr->group,
				       !!priv->fdb_def_rule, &table, error);
	if (ret)
		return ret;
	dev_flow->dv.group = table;
	if (attr->transfer)
		mhdr_res->ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	if (priority == MLX5_FLOW_PRIO_RSVD)
		priority = dev_conf->flow_prio - 1;
	/* number of actions must be set to 0 in case of dirty stack. */
	mhdr_res->actions_num = 0;
	for (; !actions_end ; actions++) {
		const struct rte_flow_action_queue *queue;
		const struct rte_flow_action_rss *rss;
		const struct rte_flow_action *action = actions;
		const uint8_t *rss_key;
		const struct rte_flow_action_jump *jump_data;
		const struct rte_flow_action_meter *mtr;
		struct mlx5_flow_tbl_resource *tbl;
		uint32_t port_id = 0;
		struct mlx5_flow_dv_port_id_action_resource port_id_resource;
		int action_type = actions->type;
		const struct rte_flow_action *found_action = NULL;
		struct mlx5_flow_meter *fm = NULL;

		if (!mlx5_flow_os_action_supported(action_type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		switch (action_type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			if (flow_dv_translate_action_port_id(dev, action,
							     &port_id, error))
				return -rte_errno;
			port_id_resource.port_id = port_id;
			MLX5_ASSERT(!handle->rix_port_id_action);
			if (flow_dv_port_id_action_resource_register
			    (dev, &port_id_resource, dev_flow, error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.port_id_action->action;
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			dev_flow->handle->mark = 1;
			if (dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
				struct rte_flow_action_mark mark = {
					.id = MLX5_FLOW_MARK_DEFAULT,
				};

				if (flow_dv_convert_action_mark(dev, &mark,
								mhdr_res,
								error))
					return -rte_errno;
				action_flags |= MLX5_FLOW_ACTION_MARK_EXT;
				break;
			}
			tag_be = mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT);
			/*
			 * Only one FLAG or MARK is supported per device flow
			 * right now. So the pointer to the tag resource must be
			 * zero before the register process.
			 */
			MLX5_ASSERT(!handle->dvh.rix_tag);
			if (flow_dv_tag_resource_register(dev, tag_be,
							  dev_flow, error))
				return -rte_errno;
			MLX5_ASSERT(dev_flow->dv.tag_resource);
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.tag_resource->action;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			action_flags |= MLX5_FLOW_ACTION_MARK;
			dev_flow->handle->mark = 1;
			if (dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY) {
				const struct rte_flow_action_mark *mark =
					(const struct rte_flow_action_mark *)
						actions->conf;

				if (flow_dv_convert_action_mark(dev, mark,
								mhdr_res,
								error))
					return -rte_errno;
				action_flags |= MLX5_FLOW_ACTION_MARK_EXT;
				break;
			}
			/* Fall-through */
		case MLX5_RTE_FLOW_ACTION_TYPE_MARK:
			/* Legacy (non-extensive) MARK action. */
			tag_be = mlx5_flow_mark_set
			      (((const struct rte_flow_action_mark *)
			       (actions->conf))->id);
			MLX5_ASSERT(!handle->dvh.rix_tag);
			if (flow_dv_tag_resource_register(dev, tag_be,
							  dev_flow, error))
				return -rte_errno;
			MLX5_ASSERT(dev_flow->dv.tag_resource);
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.tag_resource->action;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_META:
			if (flow_dv_convert_action_set_meta
				(dev, mhdr_res, attr,
				 (const struct rte_flow_action_set_meta *)
				  actions->conf, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_META;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TAG:
			if (flow_dv_convert_action_set_tag
				(dev, mhdr_res,
				 (const struct rte_flow_action_set_tag *)
				  actions->conf, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TAG;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			action_flags |= MLX5_FLOW_ACTION_DROP;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue = actions->conf;
			rss_desc->queue_num = 1;
			rss_desc->queue[0] = queue->index;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			memcpy(rss_desc->queue, rss->queue,
			       rss->queue_num * sizeof(uint16_t));
			rss_desc->queue_num = rss->queue_num;
			/* NULL RSS key indicates default RSS key. */
			rss_key = !rss->key ? rss_hash_default_key : rss->key;
			memcpy(rss_desc->key, rss_key, MLX5_RSS_HASH_KEY_LEN);
			/*
			 * rss->level and rss.types should be set in advance
			 * when expanding items for RSS.
			 */
			action_flags |= MLX5_FLOW_ACTION_RSS;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (!dev_conf->devx) {
				return rte_flow_error_set
					      (error, ENOTSUP,
					       RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					       NULL,
					       "count action not supported");
			}
			/* Save information first, will apply later. */
			if (actions->type == RTE_FLOW_ACTION_TYPE_COUNT)
				count = action->conf;
			else
				age = action->conf;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			dev_flow->dv.actions[actions_n++] =
						priv->sh->pop_vlan_action;
			action_flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			if (!(action_flags &
			      MLX5_FLOW_ACTION_OF_SET_VLAN_VID))
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
			MLX5_ASSERT(action_flags &
				    MLX5_FLOW_ACTION_OF_PUSH_VLAN);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN)
				break;
			flow_dev_get_vlan_info_from_items(items, &vlan);
			mlx5_update_vlan_vid_pcp(actions, &vlan);
			/* If no VLAN push - this is a modify header action */
			if (flow_dv_convert_action_modify_vlan_vid
						(mhdr_res, actions, error))
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
					dev_flow->dv.encap_decap->action;
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			if (flow_dv_create_action_l2_decap(dev, dev_flow,
							   attr->transfer,
							   error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->action;
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			/* Handle encap with preceding decap. */
			if (action_flags & MLX5_FLOW_ACTION_DECAP) {
				if (flow_dv_create_action_raw_encap
					(dev, actions, dev_flow, attr, error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->action;
			} else {
				/* Handle encap without preceding decap. */
				if (flow_dv_create_action_l2_encap
				    (dev, actions, dev_flow, attr->transfer,
				     error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->action;
			}
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			while ((++action)->type == RTE_FLOW_ACTION_TYPE_VOID)
				;
			if (action->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
				if (flow_dv_create_action_l2_decap
				    (dev, dev_flow, attr->transfer, error))
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->action;
			}
			/* If decap is followed by encap, handle it at encap. */
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			jump_data = action->conf;
			ret = mlx5_flow_group_to_table(attr, dev_flow->external,
						       jump_data->group,
						       !!priv->fdb_def_rule,
						       &table, error);
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
			if (flow_dv_jump_tbl_resource_register
			    (dev, tbl, dev_flow, error)) {
				flow_dv_tbl_resource_release(dev, tbl);
				return rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL,
						 "cannot create jump action.");
			}
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.jump->action;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			if (flow_dv_convert_action_modify_mac
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_MAC_SRC ?
					MLX5_FLOW_ACTION_SET_MAC_SRC :
					MLX5_FLOW_ACTION_SET_MAC_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			if (flow_dv_convert_action_modify_ipv4
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC ?
					MLX5_FLOW_ACTION_SET_IPV4_SRC :
					MLX5_FLOW_ACTION_SET_IPV4_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			if (flow_dv_convert_action_modify_ipv6
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC ?
					MLX5_FLOW_ACTION_SET_IPV6_SRC :
					MLX5_FLOW_ACTION_SET_IPV6_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			if (flow_dv_convert_action_modify_tp
					(mhdr_res, actions, items,
					 &flow_attr, dev_flow, !!(action_flags &
					 MLX5_FLOW_ACTION_DECAP), error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_TP_SRC ?
					MLX5_FLOW_ACTION_SET_TP_SRC :
					MLX5_FLOW_ACTION_SET_TP_DST;
			break;
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
			if (flow_dv_convert_action_modify_dec_ttl
					(mhdr_res, items, &flow_attr, dev_flow,
					 !!(action_flags &
					 MLX5_FLOW_ACTION_DECAP), error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_DEC_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			if (flow_dv_convert_action_modify_ttl
					(mhdr_res, actions, items, &flow_attr,
					 dev_flow, !!(action_flags &
					 MLX5_FLOW_ACTION_DECAP), error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ:
			if (flow_dv_convert_action_modify_tcp_seq
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ ?
					MLX5_FLOW_ACTION_INC_TCP_SEQ :
					MLX5_FLOW_ACTION_DEC_TCP_SEQ;
			break;

		case RTE_FLOW_ACTION_TYPE_INC_TCP_ACK:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK:
			if (flow_dv_convert_action_modify_tcp_ack
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_INC_TCP_ACK ?
					MLX5_FLOW_ACTION_INC_TCP_ACK :
					MLX5_FLOW_ACTION_DEC_TCP_ACK;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_TAG:
			if (flow_dv_convert_action_set_reg
					(mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TAG;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG:
			if (flow_dv_convert_action_copy_mreg
					(dev, mhdr_res, actions, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_TAG;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			action_flags |= MLX5_FLOW_ACTION_DEFAULT_MISS;
			dev_flow->handle->fate_action =
					MLX5_FLOW_FATE_DEFAULT_MISS;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			mtr = actions->conf;
			if (!flow->meter) {
				fm = mlx5_flow_meter_attach(priv, mtr->mtr_id,
							    attr, error);
				if (!fm)
					return rte_flow_error_set(error,
						rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"meter not found "
						"or invalid parameters");
				flow->meter = fm->idx;
			}
			/* Set the meter action. */
			if (!fm) {
				fm = mlx5_ipool_get(priv->sh->ipool
						[MLX5_IPOOL_MTR], flow->meter);
				if (!fm)
					return rte_flow_error_set(error,
						rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"meter not found "
						"or invalid parameters");
			}
			dev_flow->dv.actions[actions_n++] =
				fm->mfts->meter_action;
			action_flags |= MLX5_FLOW_ACTION_METER;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
			if (flow_dv_convert_action_modify_ipv4_dscp(mhdr_res,
							      actions, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_IPV4_DSCP;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
			if (flow_dv_convert_action_modify_ipv6_dscp(mhdr_res,
							      actions, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_SET_IPV6_DSCP;
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			if (mhdr_res->actions_num) {
				/* create modify action if needed. */
				if (flow_dv_modify_hdr_resource_register
					(dev, mhdr_res, dev_flow, error))
					return -rte_errno;
				dev_flow->dv.actions[modify_action_position] =
					handle->dvh.modify_hdr->action;
			}
			if (action_flags & MLX5_FLOW_ACTION_COUNT) {
				flow->counter =
					flow_dv_translate_create_counter(dev,
						dev_flow, count, age);

				if (!flow->counter)
					return rte_flow_error_set
						(error, rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"cannot create counter"
						" object.");
				dev_flow->dv.actions[actions_n++] =
					  (flow_dv_counter_get_by_idx(dev,
					  flow->counter, NULL))->action;
			}
			break;
		default:
			break;
		}
		if (mhdr_res->actions_num &&
		    modify_action_position == UINT32_MAX)
			modify_action_position = actions_n++;
	}
	dev_flow->dv.actions_n = actions_n;
	dev_flow->act_flags = action_flags;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int item_type = items->type;

		if (!mlx5_flow_os_item_supported(item_type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			flow_dv_translate_item_port_id(dev, match_mask,
						       match_value, items);
			last_item = MLX5_FLOW_ITEM_PORT_ID;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			flow_dv_translate_item_eth(match_mask, match_value,
						   items, tunnel,
						   dev_flow->dv.group);
			matcher.priority = action_flags &
					MLX5_FLOW_ACTION_DEFAULT_MISS &&
					!dev_flow->external ?
					MLX5_PRIORITY_MAP_L3 :
					MLX5_PRIORITY_MAP_L2;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					     MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			flow_dv_translate_item_vlan(dev_flow,
						    match_mask, match_value,
						    items, tunnel,
						    dev_flow->dv.group);
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
						    items, item_flags, tunnel,
						    dev_flow->dv.group);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
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
						    items, item_flags, tunnel,
						    dev_flow->dv.group);
			matcher.priority = MLX5_PRIORITY_MAP_L3;
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
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					     MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			flow_dv_translate_item_udp(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_PRIORITY_MAP_L4;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					     MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			flow_dv_translate_item_gre(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
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
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			flow_dv_translate_item_vxlan(match_mask, match_value,
						     items, tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			flow_dv_translate_item_vxlan_gpe(match_mask,
							 match_value, items,
							 tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			flow_dv_translate_item_geneve(match_mask, match_value,
						      items, tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			flow_dv_translate_item_mpls(match_mask, match_value,
						    items, last_item, tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case RTE_FLOW_ITEM_TYPE_MARK:
			flow_dv_translate_item_mark(dev, match_mask,
						    match_value, items);
			last_item = MLX5_FLOW_ITEM_MARK;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			flow_dv_translate_item_meta(dev, match_mask,
						    match_value, attr, items);
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
		case RTE_FLOW_ITEM_TYPE_TAG:
			flow_dv_translate_item_tag(dev, match_mask,
						   match_value, items);
			last_item = MLX5_FLOW_ITEM_TAG;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
			flow_dv_translate_mlx5_item_tag(dev, match_mask,
							match_value, items);
			last_item = MLX5_FLOW_ITEM_TAG;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			flow_dv_translate_item_tx_queue(dev, match_mask,
							match_value,
							items);
			last_item = MLX5_FLOW_ITEM_TX_QUEUE;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			flow_dv_translate_item_gtp(match_mask, match_value,
						   items, tunnel);
			matcher.priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
			last_item = MLX5_FLOW_LAYER_GTP;
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			if (!mlx5_flex_parser_ecpri_exist(dev)) {
				/* Create it only the first time to be used. */
				ret = mlx5_flex_parser_ecpri_alloc(dev);
				if (ret)
					return rte_flow_error_set
						(error, -ret,
						RTE_FLOW_ERROR_TYPE_ITEM,
						NULL,
						"cannot create eCPRI parser");
			}
			/* Adjust the length matcher and device flow value. */
			matcher.mask.size = MLX5_ST_SZ_BYTES(fte_match_param);
			dev_flow->dv.value.size =
					MLX5_ST_SZ_BYTES(fte_match_param);
			flow_dv_translate_item_ecpri(dev, match_mask,
						     match_value, items);
			/* No other protocol should follow eCPRI layer. */
			last_item = MLX5_FLOW_LAYER_ECPRI;
			break;
		default:
			break;
		}
		item_flags |= last_item;
	}
	/*
	 * When E-Switch mode is enabled, we have two cases where we need to
	 * set the source port manually.
	 * The first one, is in case of Nic steering rule, and the second is
	 * E-Switch rule where no port_id item was found. In both cases
	 * the source port is set according the current port in use.
	 */
	if (!(item_flags & MLX5_FLOW_ITEM_PORT_ID) &&
	    (priv->representor || priv->master)) {
		if (flow_dv_translate_item_port_id(dev, match_mask,
						   match_value, NULL))
			return -rte_errno;
	}
#ifdef RTE_LIBRTE_MLX5_DEBUG
	MLX5_ASSERT(!flow_dv_check_valid_spec(matcher.mask.buf,
					      dev_flow->dv.value.buf));
#endif
	/*
	 * Layers may be already initialized from prefix flow if this dev_flow
	 * is the suffix flow.
	 */
	handle->layers |= item_flags;
	if (action_flags & MLX5_FLOW_ACTION_RSS)
		flow_dv_hashfields_set(dev_flow, rss_desc);
	/* Register matcher. */
	matcher.crc = rte_raw_cksum((const void *)matcher.mask.buf,
				    matcher.mask.size);
	matcher.priority = mlx5_flow_adjust_priority(dev, priority,
						     matcher.priority);
	/* reserved field no needs to be set to 0 here. */
	tbl_key.domain = attr->transfer;
	tbl_key.direction = attr->egress;
	tbl_key.table_id = dev_flow->dv.group;
	if (flow_dv_matcher_register(dev, &matcher, &tbl_key, dev_flow, error))
		return -rte_errno;
	return 0;
}

/**
 * Apply the flow to the NIC, lock free,
 * (mutex should be acquired by caller).
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
__flow_dv_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct mlx5_flow_dv_workspace *dv;
	struct mlx5_flow_handle *dh;
	struct mlx5_flow_handle_dv *dv_h;
	struct mlx5_flow *dev_flow;
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t handle_idx;
	int n;
	int err;
	int idx;

	for (idx = priv->flow_idx - 1; idx >= priv->flow_nested_idx; idx--) {
		dev_flow = &((struct mlx5_flow *)priv->inter_flows)[idx];
		dv = &dev_flow->dv;
		dh = dev_flow->handle;
		dv_h = &dh->dvh;
		n = dv->actions_n;
		if (dh->fate_action == MLX5_FLOW_FATE_DROP) {
			if (dv->transfer) {
				dv->actions[n++] = priv->sh->esw_drop_action;
			} else {
				struct mlx5_hrxq *drop_hrxq;
				drop_hrxq = mlx5_hrxq_drop_new(dev);
				if (!drop_hrxq) {
					rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						 NULL,
						 "cannot get drop hash queue");
					goto error;
				}
				/*
				 * Drop queues will be released by the specify
				 * mlx5_hrxq_drop_release() function. Assign
				 * the special index to hrxq to mark the queue
				 * has been allocated.
				 */
				dh->rix_hrxq = UINT32_MAX;
				dv->actions[n++] = drop_hrxq->action;
			}
		} else if (dh->fate_action == MLX5_FLOW_FATE_QUEUE) {
			struct mlx5_hrxq *hrxq;
			uint32_t hrxq_idx;
			struct mlx5_flow_rss_desc *rss_desc =
				&((struct mlx5_flow_rss_desc *)priv->rss_desc)
				[!!priv->flow_nested_idx];

			MLX5_ASSERT(rss_desc->queue_num);
			hrxq_idx = mlx5_hrxq_get(dev, rss_desc->key,
						 MLX5_RSS_HASH_KEY_LEN,
						 dev_flow->hash_fields,
						 rss_desc->queue,
						 rss_desc->queue_num);
			if (!hrxq_idx) {
				hrxq_idx = mlx5_hrxq_new
						(dev, rss_desc->key,
						MLX5_RSS_HASH_KEY_LEN,
						dev_flow->hash_fields,
						rss_desc->queue,
						rss_desc->queue_num,
						!!(dh->layers &
						MLX5_FLOW_LAYER_TUNNEL));
			}
			hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ],
					      hrxq_idx);
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			dh->rix_hrxq = hrxq_idx;
			dv->actions[n++] = hrxq->action;
		} else if (dh->fate_action == MLX5_FLOW_FATE_DEFAULT_MISS) {
			if (flow_dv_default_miss_resource_register
					(dev, error)) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot create default miss resource");
				goto error_default_miss;
			}
			dh->rix_default_fate =  MLX5_FLOW_FATE_DEFAULT_MISS;
			dv->actions[n++] = priv->sh->default_miss.action;
		}
		err = mlx5_flow_os_create_flow(dv_h->matcher->matcher_object,
					       (void *)&dv->value, n,
					       dv->actions, &dh->drv_flow);
		if (err) {
			rte_flow_error_set(error, errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "hardware refuses to create flow");
			goto error;
		}
		if (priv->vmwa_context &&
		    dh->vf_vlan.tag && !dh->vf_vlan.created) {
			/*
			 * The rule contains the VLAN pattern.
			 * For VF we are going to create VLAN
			 * interface to make hypervisor set correct
			 * e-Switch vport context.
			 */
			mlx5_vlan_vmwa_acquire(dev, &dh->vf_vlan);
		}
	}
	return 0;
error:
	if (dh->fate_action == MLX5_FLOW_FATE_DEFAULT_MISS)
		flow_dv_default_miss_resource_release(dev);
error_default_miss:
	err = rte_errno; /* Save rte_errno before cleanup. */
	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       handle_idx, dh, next) {
		/* hrxq is union, don't clear it if the flag is not set. */
		if (dh->rix_hrxq) {
			if (dh->fate_action == MLX5_FLOW_FATE_DROP) {
				mlx5_hrxq_drop_release(dev);
				dh->rix_hrxq = 0;
			} else if (dh->fate_action == MLX5_FLOW_FATE_QUEUE) {
				mlx5_hrxq_release(dev, dh->rix_hrxq);
				dh->rix_hrxq = 0;
			}
		}
		if (dh->vf_vlan.tag && dh->vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &dh->vf_vlan);
	}
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Release the flow matcher.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_matcher_release(struct rte_eth_dev *dev,
			struct mlx5_flow_handle *handle)
{
	struct mlx5_flow_dv_matcher *matcher = handle->dvh.matcher;

	MLX5_ASSERT(matcher->matcher_object);
	DRV_LOG(DEBUG, "port %u matcher %p: refcnt %d--",
		dev->data->port_id, (void *)matcher,
		rte_atomic32_read(&matcher->refcnt));
	if (rte_atomic32_dec_and_test(&matcher->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (matcher->matcher_object));
		LIST_REMOVE(matcher, next);
		/* table ref-- in release interface. */
		flow_dv_tbl_resource_release(dev, matcher->tbl);
		mlx5_free(matcher);
		DRV_LOG(DEBUG, "port %u matcher %p: removed",
			dev->data->port_id, (void *)matcher);
		return 0;
	}
	return 1;
}

/**
 * Release an encap/decap resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_encap_decap_resource_release(struct rte_eth_dev *dev,
				     struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t idx = handle->dvh.rix_encap_decap;
	struct mlx5_flow_dv_encap_decap_resource *cache_resource;

	cache_resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_DECAP_ENCAP],
			 idx);
	if (!cache_resource)
		return 0;
	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "encap/decap resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action
						(cache_resource->action));
		ILIST_REMOVE(priv->sh->ipool[MLX5_IPOOL_DECAP_ENCAP],
			     &priv->sh->encaps_decaps, idx,
			     cache_resource, next);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_DECAP_ENCAP], idx);
		DRV_LOG(DEBUG, "encap/decap resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release an jump to table action resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_jump_tbl_resource_release(struct rte_eth_dev *dev,
				  struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_jump_tbl_resource *cache_resource;
	struct mlx5_flow_tbl_data_entry *tbl_data;

	tbl_data = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_JUMP],
			     handle->rix_jump);
	if (!tbl_data)
		return 0;
	cache_resource = &tbl_data->jump;
	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "jump table resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action
						(cache_resource->action));
		/* jump action memory free is inside the table release. */
		flow_dv_tbl_resource_release(dev, &tbl_data->tbl);
		DRV_LOG(DEBUG, "jump table resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release a default miss resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_default_miss_resource_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_default_miss_resource *cache_resource =
			&sh->default_miss;

	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "default miss resource %p: refcnt %d--",
			(void *)cache_resource->action,
			rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_glue->destroy_flow_action
				(cache_resource->action));
		DRV_LOG(DEBUG, "default miss resource %p: removed",
				(void *)cache_resource->action);
		return 0;
	}
	return 1;
}

/**
 * Release a modify-header resource.
 *
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_modify_hdr_resource_release(struct mlx5_flow_handle *handle)
{
	struct mlx5_flow_dv_modify_hdr_resource *cache_resource =
							handle->dvh.modify_hdr;

	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "modify-header resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action
						(cache_resource->action));
		LIST_REMOVE(cache_resource, next);
		mlx5_free(cache_resource);
		DRV_LOG(DEBUG, "modify-header resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release port ID action resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_port_id_action_resource_release(struct rte_eth_dev *dev,
					struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_port_id_action_resource *cache_resource;
	uint32_t idx = handle->rix_port_id_action;

	cache_resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_PORT_ID],
					idx);
	if (!cache_resource)
		return 0;
	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "port ID action resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action
						(cache_resource->action));
		ILIST_REMOVE(priv->sh->ipool[MLX5_IPOOL_PORT_ID],
			     &priv->sh->port_id_action_list, idx,
			     cache_resource, next);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_PORT_ID], idx);
		DRV_LOG(DEBUG, "port id action resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release push vlan action resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_push_vlan_action_resource_release(struct rte_eth_dev *dev,
					  struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t idx = handle->dvh.rix_push_vlan;
	struct mlx5_flow_dv_push_vlan_action_resource *cache_resource;

	cache_resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_PUSH_VLAN],
					idx);
	if (!cache_resource)
		return 0;
	MLX5_ASSERT(cache_resource->action);
	DRV_LOG(DEBUG, "push VLAN action resource %p: refcnt %d--",
		(void *)cache_resource,
		rte_atomic32_read(&cache_resource->refcnt));
	if (rte_atomic32_dec_and_test(&cache_resource->refcnt)) {
		claim_zero(mlx5_flow_os_destroy_flow_action
						(cache_resource->action));
		ILIST_REMOVE(priv->sh->ipool[MLX5_IPOOL_PUSH_VLAN],
			     &priv->sh->push_vlan_action_list, idx,
			     cache_resource, next);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_PUSH_VLAN], idx);
		DRV_LOG(DEBUG, "push vlan action resource %p: removed",
			(void *)cache_resource);
		return 0;
	}
	return 1;
}

/**
 * Release the fate resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param handle
 *   Pointer to mlx5_flow_handle.
 */
static void
flow_dv_fate_resource_release(struct rte_eth_dev *dev,
			       struct mlx5_flow_handle *handle)
{
	if (!handle->rix_fate)
		return;
	switch (handle->fate_action) {
	case MLX5_FLOW_FATE_DROP:
		mlx5_hrxq_drop_release(dev);
		break;
	case MLX5_FLOW_FATE_QUEUE:
		mlx5_hrxq_release(dev, handle->rix_hrxq);
		break;
	case MLX5_FLOW_FATE_JUMP:
		flow_dv_jump_tbl_resource_release(dev, handle);
		break;
	case MLX5_FLOW_FATE_PORT_ID:
		flow_dv_port_id_action_resource_release(dev, handle);
		break;
	case MLX5_FLOW_FATE_DEFAULT_MISS:
		flow_dv_default_miss_resource_release(dev);
		break;
	default:
		DRV_LOG(DEBUG, "Incorrect fate action:%d", handle->fate_action);
		break;
	}
	handle->rix_fate = 0;
}

/**
 * Remove the flow from the NIC but keeps it in memory.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
__flow_dv_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_handle *dh;
	uint32_t handle_idx;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!flow)
		return;
	handle_idx = flow->dev_handles;
	while (handle_idx) {
		dh = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
				    handle_idx);
		if (!dh)
			return;
		if (dh->drv_flow) {
			claim_zero(mlx5_flow_os_destroy_flow(dh->drv_flow));
			dh->drv_flow = NULL;
		}
		if (dh->fate_action == MLX5_FLOW_FATE_DROP ||
		    dh->fate_action == MLX5_FLOW_FATE_QUEUE ||
		    dh->fate_action == MLX5_FLOW_FATE_DEFAULT_MISS)
			flow_dv_fate_resource_release(dev, dh);
		if (dh->vf_vlan.tag && dh->vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &dh->vf_vlan);
		handle_idx = dh->next.next;
	}
}

/**
 * Remove the flow from the NIC and the memory.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
__flow_dv_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_handle *dev_handle;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!flow)
		return;
	__flow_dv_remove(dev, flow);
	if (flow->counter) {
		flow_dv_counter_release(dev, flow->counter);
		flow->counter = 0;
	}
	if (flow->meter) {
		struct mlx5_flow_meter *fm;

		fm = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MTR],
				    flow->meter);
		if (fm)
			mlx5_flow_meter_detach(fm);
		flow->meter = 0;
	}
	while (flow->dev_handles) {
		uint32_t tmp_idx = flow->dev_handles;

		dev_handle = mlx5_ipool_get(priv->sh->ipool
					    [MLX5_IPOOL_MLX5_FLOW], tmp_idx);
		if (!dev_handle)
			return;
		flow->dev_handles = dev_handle->next.next;
		if (dev_handle->dvh.matcher)
			flow_dv_matcher_release(dev, dev_handle);
		if (dev_handle->dvh.rix_encap_decap)
			flow_dv_encap_decap_resource_release(dev, dev_handle);
		if (dev_handle->dvh.modify_hdr)
			flow_dv_modify_hdr_resource_release(dev_handle);
		if (dev_handle->dvh.rix_push_vlan)
			flow_dv_push_vlan_action_resource_release(dev,
								  dev_handle);
		if (dev_handle->dvh.rix_tag)
			flow_dv_tag_release(dev,
					    dev_handle->dvh.rix_tag);
		flow_dv_fate_resource_release(dev, dev_handle);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
			   tmp_idx);
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
		struct mlx5_flow_counter *cnt;

		cnt = flow_dv_counter_get_by_idx(dev, flow->counter,
						 NULL);
		int err = _flow_dv_query_count(dev, flow->counter, &pkts,
					       &bytes);

		if (err)
			return rte_flow_error_set(error, -err,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "cannot read counters");
		qc->hits_set = 1;
		qc->bytes_set = 1;
		qc->hits = pkts - cnt->hits;
		qc->bytes = bytes - cnt->bytes;
		if (qc->reset) {
			cnt->hits = pkts;
			cnt->bytes = bytes;
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

/**
 * Destroy the meter table set.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] tbl
 *   Pointer to the meter table set.
 *
 * @return
 *   Always 0.
 */
static int
flow_dv_destroy_mtr_tbl(struct rte_eth_dev *dev,
			struct mlx5_meter_domains_infos *tbl)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_meter_domains_infos *mtd =
				(struct mlx5_meter_domains_infos *)tbl;

	if (!mtd || !priv->config.dv_flow_en)
		return 0;
	if (mtd->ingress.policer_rules[RTE_MTR_DROPPED])
		claim_zero(mlx5_flow_os_destroy_flow
			   (mtd->ingress.policer_rules[RTE_MTR_DROPPED]));
	if (mtd->egress.policer_rules[RTE_MTR_DROPPED])
		claim_zero(mlx5_flow_os_destroy_flow
			   (mtd->egress.policer_rules[RTE_MTR_DROPPED]));
	if (mtd->transfer.policer_rules[RTE_MTR_DROPPED])
		claim_zero(mlx5_flow_os_destroy_flow
			   (mtd->transfer.policer_rules[RTE_MTR_DROPPED]));
	if (mtd->egress.color_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->egress.color_matcher));
	if (mtd->egress.any_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->egress.any_matcher));
	if (mtd->egress.tbl)
		flow_dv_tbl_resource_release(dev, mtd->egress.tbl);
	if (mtd->egress.sfx_tbl)
		flow_dv_tbl_resource_release(dev, mtd->egress.sfx_tbl);
	if (mtd->ingress.color_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->ingress.color_matcher));
	if (mtd->ingress.any_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->ingress.any_matcher));
	if (mtd->ingress.tbl)
		flow_dv_tbl_resource_release(dev, mtd->ingress.tbl);
	if (mtd->ingress.sfx_tbl)
		flow_dv_tbl_resource_release(dev, mtd->ingress.sfx_tbl);
	if (mtd->transfer.color_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->transfer.color_matcher));
	if (mtd->transfer.any_matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher
			   (mtd->transfer.any_matcher));
	if (mtd->transfer.tbl)
		flow_dv_tbl_resource_release(dev, mtd->transfer.tbl);
	if (mtd->transfer.sfx_tbl)
		flow_dv_tbl_resource_release(dev, mtd->transfer.sfx_tbl);
	if (mtd->drop_actn)
		claim_zero(mlx5_flow_os_destroy_flow_action(mtd->drop_actn));
	mlx5_free(mtd);
	return 0;
}

/* Number of meter flow actions, count and jump or count and drop. */
#define METER_ACTIONS 2

/**
 * Create specify domain meter table and suffix table.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in,out] mtb
 *   Pointer to DV meter table set.
 * @param[in] egress
 *   Table attribute.
 * @param[in] transfer
 *   Table attribute.
 * @param[in] color_reg_c_idx
 *   Reg C index for color match.
 *
 * @return
 *   0 on success, -1 otherwise and rte_errno is set.
 */
static int
flow_dv_prepare_mtr_tables(struct rte_eth_dev *dev,
			   struct mlx5_meter_domains_infos *mtb,
			   uint8_t egress, uint8_t transfer,
			   uint32_t color_reg_c_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_dv_match_params mask = {
		.size = sizeof(mask.buf),
	};
	struct mlx5_flow_dv_match_params value = {
		.size = sizeof(value.buf),
	};
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.priority = 0,
		.match_criteria_enable = 0,
		.match_mask = (void *)&mask,
	};
	void *actions[METER_ACTIONS];
	struct mlx5_meter_domain_info *dtb;
	struct rte_flow_error error;
	int i = 0;
	int ret;

	if (transfer)
		dtb = &mtb->transfer;
	else if (egress)
		dtb = &mtb->egress;
	else
		dtb = &mtb->ingress;
	/* Create the meter table with METER level. */
	dtb->tbl = flow_dv_tbl_resource_get(dev, MLX5_FLOW_TABLE_LEVEL_METER,
					    egress, transfer, &error);
	if (!dtb->tbl) {
		DRV_LOG(ERR, "Failed to create meter policer table.");
		return -1;
	}
	/* Create the meter suffix table with SUFFIX level. */
	dtb->sfx_tbl = flow_dv_tbl_resource_get(dev,
					    MLX5_FLOW_TABLE_LEVEL_SUFFIX,
					    egress, transfer, &error);
	if (!dtb->sfx_tbl) {
		DRV_LOG(ERR, "Failed to create meter suffix table.");
		return -1;
	}
	/* Create matchers, Any and Color. */
	dv_attr.priority = 3;
	dv_attr.match_criteria_enable = 0;
	ret = mlx5_flow_os_create_flow_matcher(sh->ctx, &dv_attr, dtb->tbl->obj,
					       &dtb->any_matcher);
	if (ret) {
		DRV_LOG(ERR, "Failed to create meter"
			     " policer default matcher.");
		goto error_exit;
	}
	dv_attr.priority = 0;
	dv_attr.match_criteria_enable =
				1 << MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT;
	flow_dv_match_meta_reg(mask.buf, value.buf, color_reg_c_idx,
			       rte_col_2_mlx5_col(RTE_COLORS), UINT8_MAX);
	ret = mlx5_flow_os_create_flow_matcher(sh->ctx, &dv_attr, dtb->tbl->obj,
					       &dtb->color_matcher);
	if (ret) {
		DRV_LOG(ERR, "Failed to create meter policer color matcher.");
		goto error_exit;
	}
	if (mtb->count_actns[RTE_MTR_DROPPED])
		actions[i++] = mtb->count_actns[RTE_MTR_DROPPED];
	actions[i++] = mtb->drop_actn;
	/* Default rule: lowest priority, match any, actions: drop. */
	ret = mlx5_flow_os_create_flow(dtb->any_matcher, (void *)&value, i,
				       actions,
				       &dtb->policer_rules[RTE_MTR_DROPPED]);
	if (ret) {
		DRV_LOG(ERR, "Failed to create meter policer drop rule.");
		goto error_exit;
	}
	return 0;
error_exit:
	return -1;
}

/**
 * Create the needed meter and suffix tables.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Pointer to the flow meter.
 *
 * @return
 *   Pointer to table set on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_meter_domains_infos *
flow_dv_create_mtr_tbl(struct rte_eth_dev *dev,
		       const struct mlx5_flow_meter *fm)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_meter_domains_infos *mtb;
	int ret;
	int i;

	if (!priv->mtr_en) {
		rte_errno = ENOTSUP;
		return NULL;
	}
	mtb = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*mtb), 0, SOCKET_ID_ANY);
	if (!mtb) {
		DRV_LOG(ERR, "Failed to allocate memory for meter.");
		return NULL;
	}
	/* Create meter count actions */
	for (i = 0; i <= RTE_MTR_DROPPED; i++) {
		struct mlx5_flow_counter *cnt;
		if (!fm->policer_stats.cnt[i])
			continue;
		cnt = flow_dv_counter_get_by_idx(dev,
		      fm->policer_stats.cnt[i], NULL);
		mtb->count_actns[i] = cnt->action;
	}
	/* Create drop action. */
	ret = mlx5_flow_os_create_flow_action_drop(&mtb->drop_actn);
	if (ret) {
		DRV_LOG(ERR, "Failed to create drop action.");
		goto error_exit;
	}
	/* Egress meter table. */
	ret = flow_dv_prepare_mtr_tables(dev, mtb, 1, 0, priv->mtr_color_reg);
	if (ret) {
		DRV_LOG(ERR, "Failed to prepare egress meter table.");
		goto error_exit;
	}
	/* Ingress meter table. */
	ret = flow_dv_prepare_mtr_tables(dev, mtb, 0, 0, priv->mtr_color_reg);
	if (ret) {
		DRV_LOG(ERR, "Failed to prepare ingress meter table.");
		goto error_exit;
	}
	/* FDB meter table. */
	if (priv->config.dv_esw_en) {
		ret = flow_dv_prepare_mtr_tables(dev, mtb, 0, 1,
						 priv->mtr_color_reg);
		if (ret) {
			DRV_LOG(ERR, "Failed to prepare fdb meter table.");
			goto error_exit;
		}
	}
	return mtb;
error_exit:
	flow_dv_destroy_mtr_tbl(dev, mtb);
	return NULL;
}

/**
 * Destroy domain policer rule.
 *
 * @param[in] dt
 *   Pointer to domain table.
 */
static void
flow_dv_destroy_domain_policer_rule(struct mlx5_meter_domain_info *dt)
{
	int i;

	for (i = 0; i < RTE_MTR_DROPPED; i++) {
		if (dt->policer_rules[i]) {
			claim_zero(mlx5_flow_os_destroy_flow
				   (dt->policer_rules[i]));
			dt->policer_rules[i] = NULL;
		}
	}
	if (dt->jump_actn) {
		claim_zero(mlx5_flow_os_destroy_flow_action(dt->jump_actn));
		dt->jump_actn = NULL;
	}
}

/**
 * Destroy policer rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[in] attr
 *   Pointer to flow attributes.
 *
 * @return
 *   Always 0.
 */
static int
flow_dv_destroy_policer_rules(struct rte_eth_dev *dev __rte_unused,
			      const struct mlx5_flow_meter *fm,
			      const struct rte_flow_attr *attr)
{
	struct mlx5_meter_domains_infos *mtb = fm ? fm->mfts : NULL;

	if (!mtb)
		return 0;
	if (attr->egress)
		flow_dv_destroy_domain_policer_rule(&mtb->egress);
	if (attr->ingress)
		flow_dv_destroy_domain_policer_rule(&mtb->ingress);
	if (attr->transfer)
		flow_dv_destroy_domain_policer_rule(&mtb->transfer);
	return 0;
}

/**
 * Create specify domain meter policer rule.
 *
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[in] mtb
 *   Pointer to DV meter table set.
 * @param[in] mtr_reg_c
 *   Color match REG_C.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
static int
flow_dv_create_policer_forward_rule(struct mlx5_flow_meter *fm,
				    struct mlx5_meter_domain_info *dtb,
				    uint8_t mtr_reg_c)
{
	struct mlx5_flow_dv_match_params matcher = {
		.size = sizeof(matcher.buf),
	};
	struct mlx5_flow_dv_match_params value = {
		.size = sizeof(value.buf),
	};
	struct mlx5_meter_domains_infos *mtb = fm->mfts;
	void *actions[METER_ACTIONS];
	int i;
	int ret = 0;

	/* Create jump action. */
	if (!dtb->jump_actn)
		ret = mlx5_flow_os_create_flow_action_dest_flow_tbl
				(dtb->sfx_tbl->obj, &dtb->jump_actn);
	if (ret) {
		DRV_LOG(ERR, "Failed to create policer jump action.");
		goto error;
	}
	for (i = 0; i < RTE_MTR_DROPPED; i++) {
		int j = 0;

		flow_dv_match_meta_reg(matcher.buf, value.buf, mtr_reg_c,
				       rte_col_2_mlx5_col(i), UINT8_MAX);
		if (mtb->count_actns[i])
			actions[j++] = mtb->count_actns[i];
		if (fm->action[i] == MTR_POLICER_ACTION_DROP)
			actions[j++] = mtb->drop_actn;
		else
			actions[j++] = dtb->jump_actn;
		ret = mlx5_flow_os_create_flow(dtb->color_matcher,
					       (void *)&value, j, actions,
					       &dtb->policer_rules[i]);
		if (ret) {
			DRV_LOG(ERR, "Failed to create policer rule.");
			goto error;
		}
	}
	return 0;
error:
	rte_errno = errno;
	return -1;
}

/**
 * Create policer rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[in] attr
 *   Pointer to flow attributes.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
static int
flow_dv_create_policer_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter *fm,
			     const struct rte_flow_attr *attr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_meter_domains_infos *mtb = fm->mfts;
	int ret;

	if (attr->egress) {
		ret = flow_dv_create_policer_forward_rule(fm, &mtb->egress,
						priv->mtr_color_reg);
		if (ret) {
			DRV_LOG(ERR, "Failed to create egress policer.");
			goto error;
		}
	}
	if (attr->ingress) {
		ret = flow_dv_create_policer_forward_rule(fm, &mtb->ingress,
						priv->mtr_color_reg);
		if (ret) {
			DRV_LOG(ERR, "Failed to create ingress policer.");
			goto error;
		}
	}
	if (attr->transfer) {
		ret = flow_dv_create_policer_forward_rule(fm, &mtb->transfer,
						priv->mtr_color_reg);
		if (ret) {
			DRV_LOG(ERR, "Failed to create transfer policer.");
			goto error;
		}
	}
	return 0;
error:
	flow_dv_destroy_policer_rules(dev, fm, attr);
	return -1;
}

/**
 * Query a devx counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] cnt
 *   Index to the flow counter.
 * @param[in] clear
 *   Set to clear the counter statistics.
 * @param[out] pkts
 *   The statistics value of packets.
 * @param[out] bytes
 *   The statistics value of bytes.
 *
 * @return
 *   0 on success, otherwise return -1.
 */
static int
flow_dv_counter_query(struct rte_eth_dev *dev, uint32_t counter, bool clear,
		      uint64_t *pkts, uint64_t *bytes)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt;
	uint64_t inn_pkts, inn_bytes;
	int ret;

	if (!priv->config.devx)
		return -1;

	ret = _flow_dv_query_count(dev, counter, &inn_pkts, &inn_bytes);
	if (ret)
		return -1;
	cnt = flow_dv_counter_get_by_idx(dev, counter, NULL);
	*pkts = inn_pkts - cnt->hits;
	*bytes = inn_bytes - cnt->bytes;
	if (clear) {
		cnt->hits = inn_pkts;
		cnt->bytes = inn_bytes;
	}
	return 0;
}

/**
 * Get aged-out flows.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] context
 *   The address of an array of pointers to the aged-out flows contexts.
 * @param[in] nb_contexts
 *   The length of context array pointers.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   how many contexts get in success, otherwise negative errno value.
 *   if nb_contexts is 0, return the amount of all aged contexts.
 *   if nb_contexts is not 0 , return the amount of aged flows reported
 *   in the context array.
 * @note: only stub for now
 */
static int
flow_get_aged_flows(struct rte_eth_dev *dev,
		    void **context,
		    uint32_t nb_contexts,
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info;
	struct mlx5_age_param *age_param;
	struct mlx5_flow_counter *counter;
	int nb_flows = 0;

	if (nb_contexts && !context)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Should assign at least one flow or"
					  " context to get if nb_contexts != 0");
	age_info = GET_PORT_AGE_INFO(priv);
	rte_spinlock_lock(&age_info->aged_sl);
	TAILQ_FOREACH(counter, &age_info->aged_counters, next) {
		nb_flows++;
		if (nb_contexts) {
			age_param = MLX5_CNT_TO_AGE(counter);
			context[nb_flows - 1] = age_param->context;
			if (!(--nb_contexts))
				break;
		}
	}
	rte_spinlock_unlock(&age_info->aged_sl);
	MLX5_AGE_SET(age_info, MLX5_AGE_TRIGGER);
	return nb_flows;
}

/*
 * Mutex-protected thunk to lock-free  __flow_dv_translate().
 */
static int
flow_dv_translate(struct rte_eth_dev *dev,
		  struct mlx5_flow *dev_flow,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	int ret;

	flow_dv_shared_lock(dev);
	ret = __flow_dv_translate(dev, dev_flow, attr, items, actions, error);
	flow_dv_shared_unlock(dev);
	return ret;
}

/*
 * Mutex-protected thunk to lock-free  __flow_dv_apply().
 */
static int
flow_dv_apply(struct rte_eth_dev *dev,
	      struct rte_flow *flow,
	      struct rte_flow_error *error)
{
	int ret;

	flow_dv_shared_lock(dev);
	ret = __flow_dv_apply(dev, flow, error);
	flow_dv_shared_unlock(dev);
	return ret;
}

/*
 * Mutex-protected thunk to lock-free __flow_dv_remove().
 */
static void
flow_dv_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	flow_dv_shared_lock(dev);
	__flow_dv_remove(dev, flow);
	flow_dv_shared_unlock(dev);
}

/*
 * Mutex-protected thunk to lock-free __flow_dv_destroy().
 */
static void
flow_dv_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	flow_dv_shared_lock(dev);
	__flow_dv_destroy(dev, flow);
	flow_dv_shared_unlock(dev);
}

/*
 * Mutex-protected thunk to lock-free flow_dv_counter_alloc().
 */
static uint32_t
flow_dv_counter_allocate(struct rte_eth_dev *dev)
{
	uint32_t cnt;

	flow_dv_shared_lock(dev);
	cnt = flow_dv_counter_alloc(dev, 0, 0, 1, 0);
	flow_dv_shared_unlock(dev);
	return cnt;
}

/*
 * Mutex-protected thunk to lock-free flow_dv_counter_release().
 */
static void
flow_dv_counter_free(struct rte_eth_dev *dev, uint32_t cnt)
{
	flow_dv_shared_lock(dev);
	flow_dv_counter_release(dev, cnt);
	flow_dv_shared_unlock(dev);
}

const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops = {
	.validate = flow_dv_validate,
	.prepare = flow_dv_prepare,
	.translate = flow_dv_translate,
	.apply = flow_dv_apply,
	.remove = flow_dv_remove,
	.destroy = flow_dv_destroy,
	.query = flow_dv_query,
	.create_mtr_tbls = flow_dv_create_mtr_tbl,
	.destroy_mtr_tbls = flow_dv_destroy_mtr_tbl,
	.create_policer_rules = flow_dv_create_policer_rules,
	.destroy_policer_rules = flow_dv_destroy_policer_rules,
	.counter_alloc = flow_dv_counter_allocate,
	.counter_free = flow_dv_counter_free,
	.counter_query = flow_dv_counter_query,
	.get_aged_flows = flow_get_aged_flows,
};

#endif /* HAVE_IBV_FLOW_DV_SUPPORT */

