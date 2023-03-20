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
#include <ethdev_driver.h>
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_cycles.h>
#include <bus_pci_driver.h>
#include <rte_ip.h>
#include <rte_gre.h>
#include <rte_vxlan.h>
#include <rte_gtp.h>
#include <rte_eal_paging.h>
#include <rte_mpls.h>
#include <rte_mtr.h>
#include <rte_mtr_driver.h>
#include <rte_tailq.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "rte_pmd_mlx5.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

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

#define MLX5_ITEM_VALID(item, key_type) \
	(((MLX5_SET_MATCHER_SW & (key_type)) && !((item)->spec)) || \
	 ((MLX5_SET_MATCHER_HS_V == (key_type)) && !((item)->spec)) || \
	 ((MLX5_SET_MATCHER_HS_M == (key_type)) && !((item)->mask)))

#define MLX5_ITEM_UPDATE(item, key_type, v, m, gm) \
	do { \
		if ((key_type) == MLX5_SET_MATCHER_SW_V) { \
			v = (item)->spec; \
			m = (item)->mask ? (item)->mask : (gm); \
		} else if ((key_type) == MLX5_SET_MATCHER_HS_V) { \
			v = (item)->spec; \
			m = (v); \
		} else { \
			v = (item)->mask ? (item)->mask : (gm); \
			m = (v); \
		} \
	} while (0)

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
flow_dv_encap_decap_resource_release(struct rte_eth_dev *dev,
				     uint32_t encap_decap_idx);

static int
flow_dv_port_id_action_resource_release(struct rte_eth_dev *dev,
					uint32_t port_id);
static void
flow_dv_shared_rss_action_release(struct rte_eth_dev *dev, uint32_t srss);

static int
flow_dv_jump_tbl_resource_release(struct rte_eth_dev *dev,
				  uint32_t rix_jump);

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
	bool tunnel_match = false;

	/*
	 * If layers is already initialized, it means this dev_flow is the
	 * suffix flow, the layers flags is set by the prefix flow. Need to
	 * use the layer flags from prefix flow as the suffix flow may not
	 * have the user defined items as the flow is split.
	 */
	if (layers) {
		if (tunnel_decap) {
			/*
			 * If decap action before modify, it means the driver
			 * should take the inner as outer for the modify actions.
			 */
			layers = ((layers >> 6) & MLX5_FLOW_LAYER_OUTER);
		}
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
		case RTE_FLOW_ITEM_TYPE_GTP:
			if (tunnel_decap) {
				attr->attr = 0;
				tunnel_match = true;
			}
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
			    next_protocol == IPPROTO_IPV6) && tunnel_decap &&
			    !tunnel_match)
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
			    next_protocol == IPPROTO_IPV6) && tunnel_decap &&
			    !tunnel_match)
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

static inline struct mlx5_hlist *
flow_dv_hlist_prepare(struct mlx5_dev_ctx_shared *sh, struct mlx5_hlist **phl,
		     const char *name, uint32_t size, bool direct_key,
		     bool lcores_share, void *ctx,
		     mlx5_list_create_cb cb_create,
		     mlx5_list_match_cb cb_match,
		     mlx5_list_remove_cb cb_remove,
		     mlx5_list_clone_cb cb_clone,
		     mlx5_list_clone_free_cb cb_clone_free,
		     struct rte_flow_error *error)
{
	struct mlx5_hlist *hl;
	struct mlx5_hlist *expected = NULL;
	char s[MLX5_NAME_SIZE];

	hl = __atomic_load_n(phl, __ATOMIC_SEQ_CST);
	if (likely(hl))
		return hl;
	snprintf(s, sizeof(s), "%s_%s", sh->ibdev_name, name);
	hl = mlx5_hlist_create(s, size, direct_key, lcores_share,
			ctx, cb_create, cb_match, cb_remove, cb_clone,
			cb_clone_free);
	if (!hl) {
		DRV_LOG(ERR, "%s hash creation failed", name);
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	if (!__atomic_compare_exchange_n(phl, &expected, hl, false,
					 __ATOMIC_SEQ_CST,
					 __ATOMIC_SEQ_CST)) {
		mlx5_hlist_destroy(hl);
		hl = __atomic_load_n(phl, __ATOMIC_SEQ_CST);
	}
	return hl;
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
int
flow_dv_convert_modify_action(struct rte_flow_item *item,
			      struct field_modify_info *field,
			      struct field_modify_info *dcopy,
			      struct mlx5_flow_dv_modify_hdr_resource *resource,
			      uint32_t type, struct rte_flow_error *error)
{
	uint32_t i = resource->actions_num;
	struct mlx5_modification_cmd *actions = resource->actions;
	uint32_t carry_b = 0;

	/*
	 * The item and mask are provided in big-endian format.
	 * The fields should be presented as in big-endian format either.
	 * Mask must be always present, it defines the actual field width.
	 */
	MLX5_ASSERT(item->mask);
	MLX5_ASSERT(field->size);
	do {
		uint32_t size_b;
		uint32_t off_b;
		uint32_t mask;
		uint32_t data;
		bool next_field = true;
		bool next_dcopy = true;

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
		if (type == MLX5_MODIFICATION_TYPE_COPY && field->is_flex) {
			off_b = 32 - field->shift + carry_b - field->size * CHAR_BIT;
			size_b = field->size * CHAR_BIT - carry_b;
		} else {
			/* Deduce actual data width in bits from mask value. */
			off_b = rte_bsf32(mask) + carry_b;
			size_b = sizeof(uint32_t) * CHAR_BIT -
				 off_b - __builtin_clz(mask);
		}
		MLX5_ASSERT(size_b);
		actions[i] = (struct mlx5_modification_cmd) {
			.action_type = type,
			.field = field->id,
			.offset = off_b,
			.length = (size_b == sizeof(uint32_t) * CHAR_BIT) ?
				0 : size_b,
		};
		if (type == MLX5_MODIFICATION_TYPE_COPY) {
			MLX5_ASSERT(dcopy);
			actions[i].dst_field = dcopy->id;
			actions[i].dst_offset =
				(int)dcopy->offset < 0 ? off_b : dcopy->offset;
			/* Convert entire record to big-endian format. */
			actions[i].data1 = rte_cpu_to_be_32(actions[i].data1);
			/*
			 * Destination field overflow. Copy leftovers of
			 * a source field to the next destination field.
			 */
			if ((size_b > dcopy->size * CHAR_BIT - dcopy->offset) &&
			    dcopy->size != 0) {
				actions[i].length =
					dcopy->size * CHAR_BIT - dcopy->offset;
				carry_b += actions[i].length;
				next_field = false;
			} else {
				carry_b = 0;
			}
			/*
			 * Not enough bits in a source filed to fill a
			 * destination field. Switch to the next source.
			 */
			if ((size_b < dcopy->size * CHAR_BIT - dcopy->offset) &&
			    ((size_b == field->size * CHAR_BIT - off_b) ||
			     field->is_flex)) {
				actions[i].length = size_b;
				dcopy->offset += actions[i].length;
				next_dcopy = false;
			}
		} else {
			MLX5_ASSERT(item->spec);
			data = flow_dv_fetch_field((const uint8_t *)item->spec +
						   field->offset, field->size);
			/* Shift out the trailing masked bits from data. */
			data = (data & mask) >> off_b;
			if (field->is_flex)
				actions[i].offset = 32 - field->shift - field->size * CHAR_BIT;
			actions[i].data1 = rte_cpu_to_be_32(data);
		}
		/* Convert entire record to expected big-endian format. */
		actions[i].data0 = rte_cpu_to_be_32(actions[i].data0);
		if ((type != MLX5_MODIFICATION_TYPE_COPY ||
		     dcopy->id != (enum mlx5_modification_field)UINT32_MAX) &&
		    field->id != (enum mlx5_modification_field)UINT32_MAX)
			++i;
		if (next_dcopy && type == MLX5_MODIFICATION_TYPE_COPY)
			++dcopy;
		if (next_field)
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
		memcpy(&eth.hdr.src_addr.addr_bytes, &conf->mac_addr,
		       sizeof(eth.hdr.src_addr.addr_bytes));
		memcpy(&eth_mask.hdr.src_addr.addr_bytes,
		       &rte_flow_item_eth_mask.hdr.src_addr.addr_bytes,
		       sizeof(eth_mask.hdr.src_addr.addr_bytes));
	} else {
		memcpy(&eth.hdr.dst_addr.addr_bytes, &conf->mac_addr,
		       sizeof(eth.hdr.dst_addr.addr_bytes));
		memcpy(&eth_mask.hdr.dst_addr.addr_bytes,
		       &rte_flow_item_eth_mask.hdr.dst_addr.addr_bytes,
		       sizeof(eth_mask.hdr.dst_addr.addr_bytes));
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
	[REG_NON] = MLX5_MODI_OUT_NONE,
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
	MLX5_ASSERT(conf->id != REG_NON);
	MLX5_ASSERT(conf->id < (enum modify_reg)RTE_DIM(reg_to_field));
	actions[i] = (struct mlx5_modification_cmd) {
		.action_type = MLX5_MODIFICATION_TYPE_SET,
		.field = reg_to_field[conf->id],
		.offset = conf->offset,
		.length = conf->length,
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
	MLX5_ASSERT(ret != REG_NON);
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
		MLX5_ASSERT(priv->sh->config.dv_xmeta_en !=
			    MLX5_XMETA_MODE_LEGACY);
		if (conf->dst == REG_C_0) {
			/* Copy to reg_c[0], within mask only. */
			reg_dst.offset = rte_bsf32(reg_c0);
			mask = rte_cpu_to_be_32(reg_c0 >> reg_dst.offset);
		} else {
			reg_dst.offset = 0;
			mask = rte_cpu_to_be_32(reg_c0);
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
	uint32_t mask = rte_cpu_to_be_32(conf->mask);
	uint32_t data = rte_cpu_to_be_32(conf->data) & mask;
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
	MLX5_ASSERT(reg != REG_NON);
	if (reg == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0 = rte_bsf32(msk_c0);

		data = rte_cpu_to_be_32(rte_cpu_to_be_32(data) << shl_c0);
		mask = rte_cpu_to_be_32(mask) & msk_c0;
		mask = rte_cpu_to_be_32(mask << shl_c0);
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

int
mlx5_flow_item_field_width(struct rte_eth_dev *dev,
			   enum rte_flow_field_id field, int inherit,
			   const struct rte_flow_attr *attr,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	switch (field) {
	case RTE_FLOW_FIELD_START:
		return 32;
	case RTE_FLOW_FIELD_MAC_DST:
	case RTE_FLOW_FIELD_MAC_SRC:
		return 48;
	case RTE_FLOW_FIELD_VLAN_TYPE:
		return 16;
	case RTE_FLOW_FIELD_VLAN_ID:
		return 12;
	case RTE_FLOW_FIELD_MAC_TYPE:
		return 16;
	case RTE_FLOW_FIELD_IPV4_DSCP:
		return 6;
	case RTE_FLOW_FIELD_IPV4_TTL:
		return 8;
	case RTE_FLOW_FIELD_IPV4_SRC:
	case RTE_FLOW_FIELD_IPV4_DST:
		return 32;
	case RTE_FLOW_FIELD_IPV6_DSCP:
		return 6;
	case RTE_FLOW_FIELD_IPV6_HOPLIMIT:
	case RTE_FLOW_FIELD_IPV6_PROTO:
		return 8;
	case RTE_FLOW_FIELD_IPV6_SRC:
	case RTE_FLOW_FIELD_IPV6_DST:
		return 128;
	case RTE_FLOW_FIELD_TCP_PORT_SRC:
	case RTE_FLOW_FIELD_TCP_PORT_DST:
		return 16;
	case RTE_FLOW_FIELD_TCP_SEQ_NUM:
	case RTE_FLOW_FIELD_TCP_ACK_NUM:
		return 32;
	case RTE_FLOW_FIELD_TCP_FLAGS:
		return 9;
	case RTE_FLOW_FIELD_UDP_PORT_SRC:
	case RTE_FLOW_FIELD_UDP_PORT_DST:
		return 16;
	case RTE_FLOW_FIELD_VXLAN_VNI:
	case RTE_FLOW_FIELD_GENEVE_VNI:
		return 24;
	case RTE_FLOW_FIELD_GTP_TEID:
	case RTE_FLOW_FIELD_TAG:
		return 32;
	case RTE_FLOW_FIELD_MARK:
		return __builtin_popcount(priv->sh->dv_mark_mask);
	case RTE_FLOW_FIELD_META:
		return (flow_dv_get_metadata_reg(dev, attr, error) == REG_C_0) ?
			__builtin_popcount(priv->sh->dv_meta_mask) : 32;
	case RTE_FLOW_FIELD_POINTER:
	case RTE_FLOW_FIELD_VALUE:
		return inherit < 0 ? 0 : inherit;
	case RTE_FLOW_FIELD_IPV4_ECN:
	case RTE_FLOW_FIELD_IPV6_ECN:
	case RTE_FLOW_FIELD_METER_COLOR:
		return 2;
	case RTE_FLOW_FIELD_HASH_RESULT:
		return 32;
	default:
		MLX5_ASSERT(false);
	}
	return 0;
}

static __rte_always_inline uint8_t
flow_modify_info_mask_8(uint32_t length, uint32_t off)
{
	return (0xffu >> (8 - length)) << off;
}

static __rte_always_inline uint16_t
flow_modify_info_mask_16(uint32_t length, uint32_t off)
{
	return rte_cpu_to_be_16((0xffffu >> (16 - length)) << off);
}

static __rte_always_inline uint32_t
flow_modify_info_mask_32(uint32_t length, uint32_t off)
{
	return rte_cpu_to_be_32((0xffffffffu >> (32 - length)) << off);
}

static __rte_always_inline uint32_t
flow_modify_info_mask_32_masked(uint32_t length, uint32_t off, uint32_t post_mask)
{
	uint32_t mask = (0xffffffffu >> (32 - length)) << off;
	return rte_cpu_to_be_32(mask & post_mask);
}

static void
mlx5_modify_flex_item(const struct rte_eth_dev *dev,
		      const struct mlx5_flex_item *flex,
		      const struct rte_flow_action_modify_data *data,
		      struct field_modify_info *info,
		      uint32_t *mask, uint32_t width)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_flex_attr *attr = &priv->sh->cdev->config.hca_attr.flex;
	uint32_t i, j;
	int id = 0;
	uint32_t pos = 0;
	const struct mlx5_flex_pattern_field *map;
	uint32_t offset = data->offset;
	uint32_t width_left = width;
	uint32_t def;
	uint32_t cur_width = 0;
	uint32_t tmp_ofs;
	uint32_t idx = 0;
	struct field_modify_info tmp;
	int tmp_id;

	if (!attr->query_match_sample_info) {
		DRV_LOG(ERR, "FW doesn't support modify field with flex item.");
		return;
	}
	/*
	 * search for the mapping instance until Accumulated width is no
	 * less than data->offset.
	 */
	for (i = 0; i < flex->mapnum; i++) {
		if (flex->map[i].width + pos > data->offset)
			break;
		pos += flex->map[i].width;
	}
	if (i >= flex->mapnum)
		return;
	tmp_ofs = pos < data->offset ? data->offset - pos : 0;
	for (j = i; i < flex->mapnum && width_left > 0; ) {
		map = flex->map + i;
		id = mlx5_flex_get_sample_id(flex, i, &pos, false, &def);
		if (id == -1) {
			i++;
			/* All left length is dummy */
			if (pos >= data->offset + width)
				return;
			cur_width = map->width;
		/* One mapping instance covers the whole width. */
		} else if (pos + map->width >= (data->offset + width)) {
			cur_width = width_left;
		} else {
			cur_width = cur_width + map->width - tmp_ofs;
			pos += map->width;
			/*
			 * Continue to search next until:
			 * 1. Another flex parser ID.
			 * 2. Width has been covered.
			 */
			for (j = i + 1; j < flex->mapnum; j++) {
				tmp_id = mlx5_flex_get_sample_id(flex, j, &pos, false, &def);
				if (tmp_id == -1) {
					i = j;
					pos -= flex->map[j].width;
					break;
				}
				if (id >= (int)flex->devx_fp->num_samples ||
				    id >= MLX5_GRAPH_NODE_SAMPLE_NUM ||
				    tmp_id >= (int)flex->devx_fp->num_samples ||
				    tmp_id >= MLX5_GRAPH_NODE_SAMPLE_NUM)
					return;
				if (flex->devx_fp->sample_info[id].modify_field_id !=
				    flex->devx_fp->sample_info[tmp_id].modify_field_id ||
				    flex->map[j].shift != flex->map[j - 1].width +
							  flex->map[j - 1].shift) {
					i = j;
					break;
				}
				if ((pos + flex->map[j].width) >= (data->offset + width)) {
					cur_width = width_left;
					break;
				}
				pos += flex->map[j].width;
				cur_width += flex->map[j].width;
			}
		}
		if (cur_width > width_left)
			cur_width = width_left;
		else if (cur_width < width_left && (j == flex->mapnum || i == flex->mapnum))
			return;

		MLX5_ASSERT(id < (int)flex->devx_fp->num_samples);
		if (id >= (int)flex->devx_fp->num_samples || id >= MLX5_GRAPH_NODE_SAMPLE_NUM)
			return;
		/* Use invalid entry as placeholder for DUMMY mapping. */
		info[idx] = (struct field_modify_info){cur_width / CHAR_BIT, offset / CHAR_BIT,
			     id == -1 ? MLX5_MODI_INVALID :
			     (enum mlx5_modification_field)
			     flex->devx_fp->sample_info[id].modify_field_id,
			     map->shift + tmp_ofs, 1};
		offset += cur_width;
		width_left -= cur_width;
		if (!mask) {
			info[idx].offset = (32 - cur_width - map->shift - tmp_ofs);
			info[idx].size = cur_width / CHAR_BIT + info[idx].offset / CHAR_BIT;
		}
		cur_width = 0;
		tmp_ofs = 0;
		idx++;
	}
	if (unlikely(width_left > 0)) {
		MLX5_ASSERT(false);
		return;
	}
	if (mask)
		memset(mask, 0xff, data->offset / CHAR_BIT + width / CHAR_BIT);
	/* Re-order the info to follow IPv6 address. */
	for (i = 0; i < idx / 2; i++) {
		tmp = info[i];
		MLX5_ASSERT(info[i].id);
		MLX5_ASSERT(info[idx - 1 - i].id);
		info[i] = info[idx - 1 - i];
		info[idx - 1 - i] = tmp;
	}
}

void
mlx5_flow_field_id_to_modify_info
		(const struct rte_flow_action_modify_data *data,
		 struct field_modify_info *info, uint32_t *mask,
		 uint32_t width, struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t idx = 0;
	uint32_t off_be = 0;
	uint32_t length = 0;
	switch ((int)data->field) {
	case RTE_FLOW_FIELD_START:
		/* not supported yet */
		MLX5_ASSERT(false);
		break;
	case RTE_FLOW_FIELD_MAC_DST:
		MLX5_ASSERT(data->offset + width <= 48);
		off_be = 48 - (data->offset + width);
		if (off_be < 16) {
			info[idx] = (struct field_modify_info){2, 4,
					MLX5_MODI_OUT_DMAC_15_0};
			length = off_be + width <= 16 ? width : 16 - off_be;
			if (mask)
				mask[1] = flow_modify_info_mask_16(length,
								   off_be);
			else
				info[idx].offset = off_be;
			width -= length;
			if (!width)
				break;
			off_be = 0;
			idx++;
		} else {
			off_be -= 16;
		}
		info[idx] = (struct field_modify_info){4, 0,
				MLX5_MODI_OUT_DMAC_47_16};
		if (mask)
			mask[0] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_MAC_SRC:
		MLX5_ASSERT(data->offset + width <= 48);
		off_be = 48 - (data->offset + width);
		if (off_be < 16) {
			info[idx] = (struct field_modify_info){2, 4,
					MLX5_MODI_OUT_SMAC_15_0};
			length = off_be + width <= 16 ? width : 16 - off_be;
			if (mask)
				mask[1] = flow_modify_info_mask_16(length,
								   off_be);
			else
				info[idx].offset = off_be;
			width -= length;
			if (!width)
				break;
			off_be = 0;
			idx++;
		} else {
			off_be -= 16;
		}
		info[idx] = (struct field_modify_info){4, 0,
				MLX5_MODI_OUT_SMAC_47_16};
		if (mask)
			mask[0] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_VLAN_TYPE:
		/* not supported yet */
		break;
	case RTE_FLOW_FIELD_VLAN_ID:
		MLX5_ASSERT(data->offset + width <= 12);
		off_be = 12 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_FIRST_VID};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_MAC_TYPE:
		MLX5_ASSERT(data->offset + width <= 16);
		off_be = 16 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_ETHERTYPE};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV4_DSCP:
		MLX5_ASSERT(data->offset + width <= 6);
		off_be = 6 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0,
					MLX5_MODI_OUT_IP_DSCP};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV4_TTL:
		MLX5_ASSERT(data->offset + width <= 8);
		off_be = 8 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0,
					MLX5_MODI_OUT_IPV4_TTL};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV4_SRC:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_OUT_SIPV4};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV4_DST:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_OUT_DIPV4};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV6_DSCP:
		MLX5_ASSERT(data->offset + width <= 6);
		off_be = 6 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0,
					MLX5_MODI_OUT_IP_DSCP};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV6_HOPLIMIT:
		MLX5_ASSERT(data->offset + width <= 8);
		off_be = 8 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0,
					MLX5_MODI_OUT_IPV6_HOPLIMIT};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_IPV6_SRC: {
		/*
		 * Fields corresponding to IPv6 source address bytes
		 * arranged according to network byte ordering.
		 */
		struct field_modify_info fields[] = {
			{ 4, 0, MLX5_MODI_OUT_SIPV6_127_96 },
			{ 4, 4, MLX5_MODI_OUT_SIPV6_95_64 },
			{ 4, 8, MLX5_MODI_OUT_SIPV6_63_32 },
			{ 4, 12, MLX5_MODI_OUT_SIPV6_31_0 },
		};
		/* First mask to be modified is the mask of 4th address byte. */
		uint32_t midx = 3;

		MLX5_ASSERT(data->offset + width <= 128);
		off_be = 128 - (data->offset + width);
		while (width > 0 && midx > 0) {
			if (off_be < 32) {
				info[idx] = fields[midx];
				length = off_be + width <= 32 ?
					 width : 32 - off_be;
				if (mask)
					mask[midx] = flow_modify_info_mask_32
						(length, off_be);
				else
					info[idx].offset = off_be;
				width -= length;
				off_be = 0;
				idx++;
			} else {
				off_be -= 32;
			}
			midx--;
		}
		if (!width)
			break;
		info[idx] = fields[midx];
		if (mask)
			mask[midx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	}
	case RTE_FLOW_FIELD_IPV6_DST: {
		/*
		 * Fields corresponding to IPv6 destination address bytes
		 * arranged according to network byte ordering.
		 */
		struct field_modify_info fields[] = {
			{ 4, 0, MLX5_MODI_OUT_DIPV6_127_96 },
			{ 4, 4, MLX5_MODI_OUT_DIPV6_95_64 },
			{ 4, 8, MLX5_MODI_OUT_DIPV6_63_32 },
			{ 4, 12, MLX5_MODI_OUT_DIPV6_31_0 },
		};
		/* First mask to be modified is the mask of 4th address byte. */
		uint32_t midx = 3;

		MLX5_ASSERT(data->offset + width <= 128);
		off_be = 128 - (data->offset + width);
		while (width > 0 && midx > 0) {
			if (off_be < 32) {
				info[idx] = fields[midx];
				length = off_be + width <= 32 ?
					 width : 32 - off_be;
				if (mask)
					mask[midx] = flow_modify_info_mask_32
						(length, off_be);
				else
					info[idx].offset = off_be;
				width -= length;
				off_be = 0;
				idx++;
			} else {
				off_be -= 32;
			}
			midx--;
		}
		if (!width)
			break;
		info[idx] = fields[midx];
		if (mask)
			mask[midx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	}
	case RTE_FLOW_FIELD_TCP_PORT_SRC:
		MLX5_ASSERT(data->offset + width <= 16);
		off_be = 16 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_TCP_SPORT};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_TCP_PORT_DST:
		MLX5_ASSERT(data->offset + width <= 16);
		off_be = 16 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_TCP_DPORT};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_TCP_SEQ_NUM:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_OUT_TCP_SEQ_NUM};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_TCP_ACK_NUM:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_OUT_TCP_ACK_NUM};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_TCP_FLAGS:
		MLX5_ASSERT(data->offset + width <= 9);
		off_be = 9 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_TCP_FLAGS};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_UDP_PORT_SRC:
		MLX5_ASSERT(data->offset + width <= 16);
		off_be = 16 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_UDP_SPORT};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_UDP_PORT_DST:
		MLX5_ASSERT(data->offset + width <= 16);
		off_be = 16 - (data->offset + width);
		info[idx] = (struct field_modify_info){2, 0,
					MLX5_MODI_OUT_UDP_DPORT};
		if (mask)
			mask[idx] = flow_modify_info_mask_16(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_VXLAN_VNI:
		MLX5_ASSERT(data->offset + width <= 24);
		/* VNI is on bits 31-8 of TUNNEL_HDR_DW_1. */
		off_be = 24 - (data->offset + width) + 8;
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_TUNNEL_HDR_DW_1};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_GENEVE_VNI:
		/* not supported yet*/
		break;
	case RTE_FLOW_FIELD_GTP_TEID:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_GTP_TEID};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_TAG:
		{
			MLX5_ASSERT(data->offset + width <= 32);
			int reg;

			off_be = (data->level == MLX5_LINEAR_HASH_TAG_INDEX) ?
				 16 - (data->offset + width) + 16 : data->offset;
			if (priv->sh->config.dv_flow_en == 2)
				reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_TAG,
							 data->level);
			else
				reg = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG,
							   data->level, error);
			if (reg < 0)
				return;
			MLX5_ASSERT(reg != REG_NON);
			MLX5_ASSERT((unsigned int)reg < RTE_DIM(reg_to_field));
			info[idx] = (struct field_modify_info){4, 0,
						reg_to_field[reg]};
			if (mask)
				mask[idx] = flow_modify_info_mask_32
					(width, off_be);
			else
				info[idx].offset = off_be;
		}
		break;
	case RTE_FLOW_FIELD_MARK:
		{
			uint32_t mark_mask = priv->sh->dv_mark_mask;
			uint32_t mark_count = __builtin_popcount(mark_mask);
			RTE_SET_USED(mark_count);
			MLX5_ASSERT(data->offset + width <= mark_count);
			int reg = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK,
						       0, error);
			if (reg < 0)
				return;
			MLX5_ASSERT(reg != REG_NON);
			MLX5_ASSERT((unsigned int)reg < RTE_DIM(reg_to_field));
			info[idx] = (struct field_modify_info){4, 0,
						reg_to_field[reg]};
			if (mask)
				mask[idx] = flow_modify_info_mask_32_masked
					(width, data->offset, mark_mask);
			else
				info[idx].offset = data->offset;
		}
		break;
	case RTE_FLOW_FIELD_META:
		{
			uint32_t meta_mask = priv->sh->dv_meta_mask;
			uint32_t meta_count = __builtin_popcount(meta_mask);
			RTE_SET_USED(meta_count);
			MLX5_ASSERT(data->offset + width <= meta_count);
			int reg = flow_dv_get_metadata_reg(dev, attr, error);
			if (reg < 0)
				return;
			MLX5_ASSERT(reg != REG_NON);
			MLX5_ASSERT((unsigned int)reg < RTE_DIM(reg_to_field));
			info[idx] = (struct field_modify_info){4, 0,
						reg_to_field[reg]};
			if (mask)
				mask[idx] = flow_modify_info_mask_32_masked
					(width, data->offset, meta_mask);
			else
				info[idx].offset = data->offset;
		}
		break;
	case RTE_FLOW_FIELD_IPV4_ECN:
	case RTE_FLOW_FIELD_IPV6_ECN:
		MLX5_ASSERT(data->offset + width <= 2);
		off_be = 2 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0,
					MLX5_MODI_OUT_IP_ECN};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_GTP_PSC_QFI:
		MLX5_ASSERT(data->offset + width <= 8);
		off_be = data->offset + 8;
		info[idx] = (struct field_modify_info){4, 0,
					MLX5_MODI_GTPU_FIRST_EXT_DW_0};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case MLX5_RTE_FLOW_FIELD_META_REG:
		{
			uint32_t meta_mask = priv->sh->dv_meta_mask;
			uint32_t meta_count = __builtin_popcount(meta_mask);
			uint32_t reg = data->level;

			RTE_SET_USED(meta_count);
			MLX5_ASSERT(data->offset + width <= meta_count);
			MLX5_ASSERT(reg != REG_NON);
			MLX5_ASSERT(reg < RTE_DIM(reg_to_field));
			info[idx] = (struct field_modify_info){4, 0, reg_to_field[reg]};
			if (mask)
				mask[idx] = flow_modify_info_mask_32_masked
					(width, data->offset, meta_mask);
			else
				info[idx].offset = data->offset;
		}
		break;
	case RTE_FLOW_FIELD_METER_COLOR:
		{
			const uint32_t color_mask =
				(UINT32_C(1) << MLX5_MTR_COLOR_BITS) - 1;
			int reg;

			if (priv->sh->config.dv_flow_en == 2)
				reg = flow_hw_get_reg_id
					(RTE_FLOW_ITEM_TYPE_METER_COLOR, 0);
			else
				reg = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR,
						       0, error);
			if (reg < 0)
				return;
			MLX5_ASSERT(reg != REG_NON);
			MLX5_ASSERT((unsigned int)reg < RTE_DIM(reg_to_field));
			info[idx] = (struct field_modify_info){4, 0,
						reg_to_field[reg]};
			if (mask)
				mask[idx] = flow_modify_info_mask_32_masked
					(width, data->offset, color_mask);
			else
				info[idx].offset = data->offset;
		}
		break;
	case RTE_FLOW_FIELD_IPV6_PROTO:
		MLX5_ASSERT(data->offset + width <= 8);
		off_be = 8 - (data->offset + width);
		info[idx] = (struct field_modify_info){1, 0, MLX5_MODI_OUT_IPV6_NEXT_HDR};
		if (mask)
			mask[idx] = flow_modify_info_mask_8(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_FLEX_ITEM:
		MLX5_ASSERT(data->flex_handle != NULL && !(data->offset & 0x7));
		mlx5_modify_flex_item(dev, (const struct mlx5_flex_item *)data->flex_handle,
				      data, info, mask, width);
		break;
	case RTE_FLOW_FIELD_HASH_RESULT:
		MLX5_ASSERT(data->offset + width <= 32);
		off_be = 32 - (data->offset + width);
		info[idx] = (struct field_modify_info){4, 0,
						       MLX5_MODI_HASH_RESULT};
		if (mask)
			mask[idx] = flow_modify_info_mask_32(width, off_be);
		else
			info[idx].offset = off_be;
		break;
	case RTE_FLOW_FIELD_POINTER:
	case RTE_FLOW_FIELD_VALUE:
	default:
		MLX5_ASSERT(false);
		break;
	}
}

/**
 * Convert modify_field action to DV specification.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in,out] resource
 *   Pointer to the modify-header resource.
 * @param[in] action
 *   Pointer to action specification.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_convert_action_modify_field
			(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_modify_hdr_resource *resource,
			 const struct rte_flow_action *action,
			 const struct rte_flow_attr *attr,
			 struct rte_flow_error *error)
{
	const struct rte_flow_action_modify_field *conf =
		(const struct rte_flow_action_modify_field *)(action->conf);
	struct rte_flow_item item = {
		.spec = NULL,
		.mask = NULL
	};
	struct field_modify_info field[MLX5_ACT_MAX_MOD_FIELDS] = {
								{0, 0, 0} };
	struct field_modify_info dcopy[MLX5_ACT_MAX_MOD_FIELDS] = {
								{0, 0, 0} };
	uint32_t mask[MLX5_ACT_MAX_MOD_FIELDS] = {0, 0, 0, 0, 0};
	uint32_t type, meta = 0;

	if (conf->src.field == RTE_FLOW_FIELD_POINTER ||
	    conf->src.field == RTE_FLOW_FIELD_VALUE) {
		type = conf->operation == RTE_FLOW_MODIFY_SET ?
			MLX5_MODIFICATION_TYPE_SET : MLX5_MODIFICATION_TYPE_ADD;
		/** For SET fill the destination field (field) first. */
		mlx5_flow_field_id_to_modify_info(&conf->dst, field, mask,
						  conf->width, dev,
						  attr, error);
		item.spec = conf->src.field == RTE_FLOW_FIELD_POINTER ?
					(void *)(uintptr_t)conf->src.pvalue :
					(void *)(uintptr_t)&conf->src.value;
		if (conf->dst.field == RTE_FLOW_FIELD_META ||
		    conf->dst.field == RTE_FLOW_FIELD_TAG ||
		    conf->dst.field == RTE_FLOW_FIELD_METER_COLOR) {
			meta = *(const unaligned_uint32_t *)item.spec;
			meta = rte_cpu_to_be_32(meta);
			item.spec = &meta;
		}
	} else {
		type = MLX5_MODIFICATION_TYPE_COPY;
		/** For COPY fill the destination field (dcopy) without mask. */
		mlx5_flow_field_id_to_modify_info(&conf->dst, dcopy, NULL,
						  conf->width, dev,
						  attr, error);
		/** Then construct the source field (field) with mask. */
		mlx5_flow_field_id_to_modify_info(&conf->src, field, mask,
						  conf->width, dev,
						  attr, error);
	}
	item.mask = &mask;
	return flow_dv_convert_modify_action(&item,
			field, dcopy, resource, type, error);
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
	struct mlx5_sh_config *config = &priv->sh->config;
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
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
	struct mlx5_sh_config *config = &priv->sh->config;
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
		if (reg == REG_NON)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"unavailable extended metadata register");
		if (reg == REG_B)
			return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "match on reg_b "
					  "isn't supported");
		if (reg != REG_A)
			nic_mask.data = priv->sh->dv_meta_mask;
	} else {
		if (attr->transfer)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"extended metadata feature "
					"should be enabled when "
					"meta item is requested "
					"with e-switch mode ");
		if (attr->ingress)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"match on metadata for ingress "
					"is not supported in legacy "
					"metadata mode");
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
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
	MLX5_ASSERT(ret != REG_NON);
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
			      struct mlx5_priv **act_priv,
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
				 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret)
		return ret;
	if (!spec)
		return 0;
	if (spec->id == MLX5_PORT_ESW_MGR)
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
	*act_priv = esw_priv;
	return 0;
}

/**
 * Validate represented port item.
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
flow_dv_validate_item_represented_port(struct rte_eth_dev *dev,
				       const struct rte_flow_item *item,
				       const struct rte_flow_attr *attr,
				       uint64_t item_flags,
				       struct mlx5_priv **act_priv,
				       struct rte_flow_error *error)
{
	const struct rte_flow_item_ethdev *spec = item->spec;
	const struct rte_flow_item_ethdev *mask = item->mask;
	const struct rte_flow_item_ethdev switch_mask = {
			.port_id = UINT16_MAX,
	};
	struct mlx5_priv *esw_priv;
	struct mlx5_priv *dev_priv;
	int ret;

	if (!attr->transfer)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "match on port id is valid only when transfer flag is enabled");
	if (item_flags & MLX5_FLOW_ITEM_REPRESENTED_PORT)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple source ports are not supported");
	if (!mask)
		mask = &switch_mask;
	if (mask->port_id != UINT16_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					   "no support for partial mask on \"id\" field");
	ret = mlx5_flow_item_acceptable
				(item, (const uint8_t *)mask,
				 (const uint8_t *)&rte_flow_item_ethdev_mask,
				 sizeof(struct rte_flow_item_ethdev),
				 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret)
		return ret;
	if (!spec || spec->port_id == UINT16_MAX)
		return 0;
	esw_priv = mlx5_port_to_eswitch_info(spec->port_id, false);
	if (!esw_priv)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "failed to obtain E-Switch info for port");
	dev_priv = mlx5_dev_to_eswitch_info(dev);
	if (!dev_priv)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to obtain E-Switch info");
	if (esw_priv->domain_id != dev_priv->domain_id)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, spec,
					  "cannot match on a port from a different E-Switch");
	*act_priv = esw_priv;
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
		.hdr.vlan_tci = RTE_BE16(UINT16_MAX),
		.hdr.eth_proto = RTE_BE16(UINT16_MAX),
		.has_more_vlan = 1,
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
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret)
		return ret;
	if (!tunnel && mask->hdr.vlan_tci != RTE_BE16(0x0fff)) {
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
		.hdr.gtp_hdr_info = MLX5_GTP_FLAGS_MASK,
		.hdr.msg_type = 0xff,
		.hdr.teid = RTE_BE32(0xffffffff),
	};

	if (!priv->sh->cdev->config.hca_attr.tunnel_stateless_gtp)
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
	if (spec && spec->hdr.gtp_hdr_info & ~MLX5_GTP_FLAGS_MASK)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Match is supported for GTP"
					  " flags only");
	return mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					 (const uint8_t *)&nic_mask,
					 sizeof(struct rte_flow_item_gtp),
					 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
}

/**
 * Validate GTP PSC item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] last_item
 *   Previous validated item in the pattern items.
 * @param[in] gtp_item
 *   Previous GTP item specification.
 * @param root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_gtp_psc(const struct rte_flow_item *item,
			      uint64_t last_item,
			      const struct rte_flow_item *gtp_item,
			      bool root,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_gtp *gtp_spec;
	const struct rte_flow_item_gtp *gtp_mask;
	const struct rte_flow_item_gtp_psc *mask;
	const struct rte_flow_item_gtp_psc nic_mask = {
		.hdr.type = 0xF,
		.hdr.qfi = 0x3F,
	};

	if (!gtp_item || !(last_item & MLX5_FLOW_LAYER_GTP))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "GTP PSC item must be preceded with GTP item");
	gtp_spec = gtp_item->spec;
	gtp_mask = gtp_item->mask ? gtp_item->mask : &rte_flow_item_gtp_mask;
	/* GTP spec and E flag is requested to match zero. */
	if (gtp_spec &&
		(gtp_mask->hdr.gtp_hdr_info &
		~gtp_spec->hdr.gtp_hdr_info & MLX5_GTP_EXT_HEADER_FLAG))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			 "GTP E flag must be 1 to match GTP PSC");
	/* Check the flow is not created in group zero. */
	if (root)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			 "GTP PSC is not supported for group 0");
	/* GTP spec is here and E flag is requested to match zero. */
	if (!item->spec)
		return 0;
	mask = item->mask ? item->mask : &rte_flow_item_gtp_psc_mask;
	return mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					 (const uint8_t *)&nic_mask,
					 sizeof(struct rte_flow_item_gtp_psc),
					 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
}

/**
 * Validate IPV4 item.
 * Use existing validation function mlx5_flow_validate_item_ipv4(), and
 * add specific validation of fragment_offset field,
 *
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
flow_dv_validate_item_ipv4(struct rte_eth_dev *dev,
			   const struct rte_flow_item *item,
			   uint64_t item_flags, uint64_t last_item,
			   uint16_t ether_type, struct rte_flow_error *error)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *attr = &priv->sh->cdev->config.hca_attr;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *last = item->last;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	rte_be16_t fragment_offset_spec = 0;
	rte_be16_t fragment_offset_last = 0;
	struct rte_flow_item_ipv4 nic_ipv4_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.fragment_offset = RTE_BE16(0xffff),
			.next_proto_id = 0xff,
			.time_to_live = 0xff,
		},
	};

	if (mask && (mask->hdr.version_ihl & RTE_IPV4_HDR_IHL_MASK)) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		bool ihl_cap = !tunnel ?
			       attr->outer_ipv4_ihl : attr->inner_ipv4_ihl;
		if (!ihl_cap)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "IPV4 ihl offload not supported");
		nic_ipv4_mask.hdr.version_ihl = mask->hdr.version_ihl;
	}
	ret = mlx5_flow_validate_item_ipv4(item, item_flags, last_item,
					   ether_type, &nic_ipv4_mask,
					   MLX5_ITEM_RANGE_ACCEPTED, error);
	if (ret < 0)
		return ret;
	if (spec && mask)
		fragment_offset_spec = spec->hdr.fragment_offset &
				       mask->hdr.fragment_offset;
	if (!fragment_offset_spec)
		return 0;
	/*
	 * spec and mask are valid, enforce using full mask to make sure the
	 * complete value is used correctly.
	 */
	if ((mask->hdr.fragment_offset & RTE_BE16(MLX5_IPV4_FRAG_OFFSET_MASK))
			!= RTE_BE16(MLX5_IPV4_FRAG_OFFSET_MASK))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					  item, "must use full mask for"
					  " fragment_offset");
	/*
	 * Match on fragment_offset 0x2000 means MF is 1 and frag-offset is 0,
	 * indicating this is 1st fragment of fragmented packet.
	 * This is not yet supported in MLX5, return appropriate error message.
	 */
	if (fragment_offset_spec == RTE_BE16(RTE_IPV4_HDR_MF_FLAG))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "match on first fragment not "
					  "supported");
	if (fragment_offset_spec && !last)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "specified value not supported");
	/* spec and last are valid, validate the specified range. */
	fragment_offset_last = last->hdr.fragment_offset &
			       mask->hdr.fragment_offset;
	/*
	 * Match on fragment_offset spec 0x2001 and last 0x3fff
	 * means MF is 1 and frag-offset is > 0.
	 * This packet is fragment 2nd and onward, excluding last.
	 * This is not yet supported in MLX5, return appropriate
	 * error message.
	 */
	if (fragment_offset_spec == RTE_BE16(RTE_IPV4_HDR_MF_FLAG + 1) &&
	    fragment_offset_last == RTE_BE16(MLX5_IPV4_FRAG_OFFSET_MASK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					  last, "match on following "
					  "fragments not supported");
	/*
	 * Match on fragment_offset spec 0x0001 and last 0x1fff
	 * means MF is 0 and frag-offset is > 0.
	 * This packet is last fragment of fragmented packet.
	 * This is not yet supported in MLX5, return appropriate
	 * error message.
	 */
	if (fragment_offset_spec == RTE_BE16(1) &&
	    fragment_offset_last == RTE_BE16(RTE_IPV4_HDR_OFFSET_MASK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					  last, "match on last "
					  "fragment not supported");
	/*
	 * Match on fragment_offset spec 0x0001 and last 0x3fff
	 * means MF and/or frag-offset is not 0.
	 * This is a fragmented packet.
	 * Other range values are invalid and rejected.
	 */
	if (!(fragment_offset_spec == RTE_BE16(1) &&
	      fragment_offset_last == RTE_BE16(MLX5_IPV4_FRAG_OFFSET_MASK)))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST, last,
					  "specified range not supported");
	return 0;
}

/**
 * Validate IPV6 fragment extension item.
 *
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
flow_dv_validate_item_ipv6_frag_ext(const struct rte_flow_item *item,
				    uint64_t item_flags,
				    struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6_frag_ext *spec = item->spec;
	const struct rte_flow_item_ipv6_frag_ext *last = item->last;
	const struct rte_flow_item_ipv6_frag_ext *mask = item->mask;
	rte_be16_t frag_data_spec = 0;
	rte_be16_t frag_data_last = 0;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret = 0;
	struct rte_flow_item_ipv6_frag_ext nic_mask = {
		.hdr = {
			.next_header = 0xff,
			.frag_data = RTE_BE16(0xffff),
		},
	};

	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "ipv6 fragment extension item cannot "
					  "follow L4 item.");
	if ((tunnel && !(item_flags & MLX5_FLOW_LAYER_INNER_L3_IPV6)) ||
	    (!tunnel && !(item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV6)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "ipv6 fragment extension item must "
					  "follow ipv6 item");
	if (spec && mask)
		frag_data_spec = spec->hdr.frag_data & mask->hdr.frag_data;
	if (!frag_data_spec)
		return 0;
	/*
	 * spec and mask are valid, enforce using full mask to make sure the
	 * complete value is used correctly.
	 */
	if ((mask->hdr.frag_data & RTE_BE16(RTE_IPV6_FRAG_USED_MASK)) !=
				RTE_BE16(RTE_IPV6_FRAG_USED_MASK))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK,
					  item, "must use full mask for"
					  " frag_data");
	/*
	 * Match on frag_data 0x00001 means M is 1 and frag-offset is 0.
	 * This is 1st fragment of fragmented packet.
	 */
	if (frag_data_spec == RTE_BE16(RTE_IPV6_EHDR_MF_MASK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "match on first fragment not "
					  "supported");
	if (frag_data_spec && !last)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "specified value not supported");
	ret = mlx5_flow_item_acceptable
				(item, (const uint8_t *)mask,
				 (const uint8_t *)&nic_mask,
				 sizeof(struct rte_flow_item_ipv6_frag_ext),
				 MLX5_ITEM_RANGE_ACCEPTED, error);
	if (ret)
		return ret;
	/* spec and last are valid, validate the specified range. */
	frag_data_last = last->hdr.frag_data & mask->hdr.frag_data;
	/*
	 * Match on frag_data spec 0x0009 and last 0xfff9
	 * means M is 1 and frag-offset is > 0.
	 * This packet is fragment 2nd and onward, excluding last.
	 * This is not yet supported in MLX5, return appropriate
	 * error message.
	 */
	if (frag_data_spec == RTE_BE16(RTE_IPV6_EHDR_FO_ALIGN |
				       RTE_IPV6_EHDR_MF_MASK) &&
	    frag_data_last == RTE_BE16(RTE_IPV6_FRAG_USED_MASK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					  last, "match on following "
					  "fragments not supported");
	/*
	 * Match on frag_data spec 0x0008 and last 0xfff8
	 * means M is 0 and frag-offset is > 0.
	 * This packet is last fragment of fragmented packet.
	 * This is not yet supported in MLX5, return appropriate
	 * error message.
	 */
	if (frag_data_spec == RTE_BE16(RTE_IPV6_EHDR_FO_ALIGN) &&
	    frag_data_last == RTE_BE16(RTE_IPV6_EHDR_FO_MASK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_LAST,
					  last, "match on last "
					  "fragment not supported");
	/* Other range values are invalid and rejected. */
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_ITEM_LAST, last,
				  "specified range not supported");
}

/*
 * Validate ASO CT item.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Pointer to bit-fields that holds the items detected until now.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_item_aso_ct(struct rte_eth_dev *dev,
			     const struct rte_flow_item *item,
			     uint64_t *item_flags,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_conntrack *spec = item->spec;
	const struct rte_flow_item_conntrack *mask = item->mask;
	RTE_SET_USED(dev);
	uint32_t flags;

	if (*item_flags & MLX5_FLOW_LAYER_ASO_CT)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "Only one CT is supported");
	if (!mask)
		mask = &rte_flow_item_conntrack_mask;
	flags = spec->flags & mask->flags;
	if ((flags & RTE_FLOW_CONNTRACK_PKT_STATE_VALID) &&
	    ((flags & RTE_FLOW_CONNTRACK_PKT_STATE_INVALID) ||
	     (flags & RTE_FLOW_CONNTRACK_PKT_STATE_BAD) ||
	     (flags & RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "Conflict status bits");
	/* State change also needs to be considered. */
	*item_flags |= MLX5_FLOW_LAYER_ASO_CT;
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
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->sh->pop_vlan_action)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "pop vlan action is not supported");
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
		.hdr.vlan_tci = RTE_BE16(MLX5DV_FLOW_VLAN_PCP_MASK |
				MLX5DV_FLOW_VLAN_VID_MASK),
		.hdr.eth_proto = RTE_BE16(0xffff),
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
		if ((vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) ==
		     MLX5DV_FLOW_VLAN_PCP_MASK_BE) {
			vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_PCP_MASK;
			vlan->vlan_tci |=
				rte_be_to_cpu_16(vlan_v->hdr.vlan_tci &
						 MLX5DV_FLOW_VLAN_PCP_MASK_BE);
		}
		if ((vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) ==
		     MLX5DV_FLOW_VLAN_VID_MASK_BE) {
			vlan->vlan_tci &= ~MLX5DV_FLOW_VLAN_VID_MASK;
			vlan->vlan_tci |=
				rte_be_to_cpu_16(vlan_v->hdr.vlan_tci &
						 MLX5DV_FLOW_VLAN_VID_MASK_BE);
		}
		if (vlan_m->hdr.eth_proto == nic_mask.hdr.eth_proto)
			vlan->eth_proto = rte_be_to_cpu_16(vlan_v->hdr.eth_proto &
							   vlan_m->hdr.eth_proto);
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
	    (vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) &&
	    (vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_PCP_MASK_BE) !=
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
	    (vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) &&
	    (vlan_m->hdr.vlan_tci & MLX5DV_FLOW_VLAN_VID_MASK_BE) !=
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
	struct mlx5_sh_config *config = &priv->sh->config;
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
	struct mlx5_sh_config *config = &priv->sh->config;
	const struct rte_flow_action_mark *mark = action->conf;
	int ret;

	if (is_tunnel_offload_active(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "no mark action "
					  "if tunnel offload active");
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
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	const struct rte_flow_action_set_meta *conf;
	uint32_t nic_mask = UINT32_MAX;
	int reg;

	if (config->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    !mlx5_flow_ext_mreg_supported(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "extended metadata register"
					  " isn't supported");
	reg = flow_dv_get_metadata_reg(dev, attr, error);
	if (reg < 0)
		return reg;
	if (reg == REG_NON)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "unavailable extended metadata register");
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
	if (attr->ingress && (action_flags & terminal_action_flags))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "set_tag has no effect"
					  " with terminal actions");
	return 0;
}

/**
 * Indicates whether ASO aging is supported.
 *
 * @param[in] priv
 *   Pointer to device private context structure.
 * @param[in] root
 *   Whether action is on root table.
 *
 * @return
 *   True when ASO aging is supported, false otherwise.
 */
static inline bool
flow_hit_aso_supported(const struct mlx5_priv *priv, bool root)
{
	MLX5_ASSERT(priv);
	return (priv->sh->flow_hit_aso_en && !root);
}

/**
 * Validate count action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] shared
 *   Indicator if action is shared.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_count(struct rte_eth_dev *dev, bool shared,
			      uint64_t action_flags,
			      bool root,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->sh->cdev->config.devx)
		goto notsup_err;
	if (action_flags & MLX5_FLOW_ACTION_COUNT)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "duplicate count actions set");
	if (shared && (action_flags & MLX5_FLOW_ACTION_AGE) &&
	    !flow_hit_aso_supported(priv, root))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "old age and indirect count combination is not supported");
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
 * @param[in] action
 *   Pointer to the action structure.
 * @param[in] item_flags
 *   Holds the items detected.
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
			      const struct rte_flow_action *action,
			      const uint64_t item_flags,
			      const struct rte_flow_attr *attr,
			      struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->cdev->config.hca_attr.scatter_fcs_w_decap_disable &&
	    !priv->sh->config.decap_en)
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
	if (action->type == RTE_FLOW_ACTION_TYPE_VXLAN_DECAP &&
	    !(item_flags & MLX5_FLOW_LAYER_VXLAN))
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"VXLAN item should be present for VXLAN decap");
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
 * @param[in] action
 *   Pointer to the action structure.
 * @param[in] item_flags
 *   Holds the items detected.
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
	 int *actions_n, const struct rte_flow_action *action,
	 uint64_t item_flags, struct rte_flow_error *error)
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
		ret = flow_dv_validate_action_decap(dev, *action_flags, action,
						    item_flags, attr, error);
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

/*
 * Validate the ASO CT action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] item_flags
 *   The items found in this flow rule.
 * @param root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_aso_ct(struct rte_eth_dev *dev,
			       uint64_t action_flags,
			       uint64_t item_flags,
			       bool root,
			       struct rte_flow_error *error)
{
	RTE_SET_USED(dev);

	if (root)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Only support non-root table");
	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "CT cannot follow a fate action");
	if ((action_flags & MLX5_FLOW_ACTION_METER) ||
	    (action_flags & MLX5_FLOW_ACTION_AGE))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Only one ASO action is supported");
	if (action_flags & MLX5_FLOW_ACTION_ENCAP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Encap cannot exist before CT");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Not a outer TCP packet");
	return 0;
}

/**
 * Validate METER_COLOR item.
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
flow_dv_validate_item_meter_color(struct rte_eth_dev *dev,
			   const struct rte_flow_item *item,
			   const struct rte_flow_attr *attr __rte_unused,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_meter_color *spec = item->spec;
	const struct rte_flow_item_meter_color *mask = item->mask;
	struct rte_flow_item_meter_color nic_mask = {
		.color = RTE_COLORS
	};
	int ret;

	if (priv->mtr_color_reg == REG_NON)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "meter color register"
					  " isn't available");
	ret = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, error);
	if (ret < 0)
		return ret;
	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (spec->color > RTE_COLORS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &spec->color,
					  "meter color is invalid");
	if (!mask)
		mask = &rte_flow_item_meter_color_mask;
	if (!mask->color)
		return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					"mask cannot be zero");

	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
				(const uint8_t *)&nic_mask,
				sizeof(struct rte_flow_item_meter_color),
				MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate aggregated affinity item.
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
flow_dv_validate_item_aggr_affinity(struct rte_eth_dev *dev,
				   const struct rte_flow_item *item,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_aggr_affinity *spec = item->spec;
	const struct rte_flow_item_aggr_affinity *mask = item->mask;
	struct rte_flow_item_aggr_affinity nic_mask = {
		.affinity = UINT8_MAX
	};
	int ret;

	if (!priv->sh->lag_rx_port_affinity_en)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "Unsupported aggregated affinity with Older FW");
	if ((attr->transfer && priv->fdb_def_rule) ||
	    attr->egress || attr->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "aggregated affinity is not supported with egress or FDB on non root table");
	if (!spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "data cannot be empty");
	if (spec->affinity == 0)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "zero affinity number not supported");
	if (spec->affinity > priv->num_lag_ports)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "exceed max affinity number in lag ports");
	if (!mask)
		mask = &rte_flow_item_aggr_affinity_mask;
	if (!mask->affinity)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
					  "mask cannot be zero");
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
				(const uint8_t *)&nic_mask,
				sizeof(struct rte_flow_item_aggr_affinity),
				MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	return 0;
}

int
flow_dv_encap_decap_match_cb(void *tool_ctx __rte_unused,
			     struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_encap_decap_resource *ctx_resource = ctx->data;
	struct mlx5_flow_dv_encap_decap_resource *resource;

	resource = container_of(entry, struct mlx5_flow_dv_encap_decap_resource,
				entry);
	if (resource->reformat_type == ctx_resource->reformat_type &&
	    resource->ft_type == ctx_resource->ft_type &&
	    resource->flags == ctx_resource->flags &&
	    resource->size == ctx_resource->size &&
	    !memcmp((const void *)resource->buf,
		    (const void *)ctx_resource->buf,
		    resource->size))
		return 0;
	return -1;
}

struct mlx5_list_entry *
flow_dv_encap_decap_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5dv_dr_domain *domain;
	struct mlx5_flow_dv_encap_decap_resource *ctx_resource = ctx->data;
	struct mlx5_flow_dv_encap_decap_resource *resource;
	uint32_t idx;
	int ret;

	if (ctx_resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (ctx_resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;
	/* Register new encap/decap resource. */
	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	*resource = *ctx_resource;
	resource->idx = idx;
	ret = mlx5_flow_os_create_flow_action_packet_reformat(sh->cdev->ctx,
							      domain, resource,
							     &resource->action);
	if (ret) {
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], idx);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create action");
		return NULL;
	}

	return &resource->entry;
}

struct mlx5_list_entry *
flow_dv_encap_decap_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
			     void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_encap_decap_resource *cache_resource;
	uint32_t idx;

	cache_resource = mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_DECAP_ENCAP],
					   &idx);
	if (!cache_resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	memcpy(cache_resource, oentry, sizeof(*cache_resource));
	cache_resource->idx = idx;
	return &cache_resource->entry;
}

void
flow_dv_encap_decap_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_encap_decap_resource *res =
				       container_of(entry, typeof(*res), entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], res->idx);
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
	struct mlx5_list_entry *entry;
	union {
		struct {
			uint32_t ft_type:8;
			uint32_t refmt_type:8;
			/*
			 * Header reformat actions can be shared between
			 * non-root tables. One bit to indicate non-root
			 * table or not.
			 */
			uint32_t is_root:1;
			uint32_t reserve:15;
		};
		uint32_t v32;
	} encap_decap_key = {
		{
			.ft_type = resource->ft_type,
			.refmt_type = resource->reformat_type,
			.is_root = !!dev_flow->dv.group,
			.reserve = 0,
		}
	};
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = resource,
	};
	struct mlx5_hlist *encaps_decaps;
	uint64_t key64;

	encaps_decaps = flow_dv_hlist_prepare(sh, &sh->encaps_decaps,
				"encaps_decaps",
				MLX5_FLOW_ENCAP_DECAP_HTABLE_SZ,
				true, true, sh,
				flow_dv_encap_decap_create_cb,
				flow_dv_encap_decap_match_cb,
				flow_dv_encap_decap_remove_cb,
				flow_dv_encap_decap_clone_cb,
				flow_dv_encap_decap_clone_free_cb,
				error);
	if (unlikely(!encaps_decaps))
		return -rte_errno;
	resource->flags = dev_flow->dv.group ? 0 : 1;
	key64 =  __rte_raw_cksum(&encap_decap_key.v32,
				 sizeof(encap_decap_key.v32), 0);
	if (resource->reformat_type !=
	    MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2 &&
	    resource->size)
		key64 = __rte_raw_cksum(resource->buf, resource->size, key64);
	entry = mlx5_hlist_register(encaps_decaps, key64, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->dv.encap_decap = resource;
	dev_flow->handle->dvh.rix_encap_decap = resource->idx;
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
			 struct rte_flow_error *error __rte_unused)
{
	struct mlx5_flow_tbl_data_entry *tbl_data =
		container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);

	MLX5_ASSERT(tbl);
	MLX5_ASSERT(tbl_data->jump.action);
	dev_flow->handle->rix_jump = tbl_data->idx;
	dev_flow->dv.jump = &tbl_data->jump;
	return 0;
}

int
flow_dv_port_id_match_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_port_id_action_resource *ref = ctx->data;
	struct mlx5_flow_dv_port_id_action_resource *res =
				       container_of(entry, typeof(*res), entry);

	return ref->port_id != res->port_id;
}

struct mlx5_list_entry *
flow_dv_port_id_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_port_id_action_resource *ref = ctx->data;
	struct mlx5_flow_dv_port_id_action_resource *resource;
	uint32_t idx;
	int ret;

	/* Register new port id action resource. */
	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PORT_ID], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate port_id action memory");
		return NULL;
	}
	*resource = *ref;
	ret = mlx5_flow_os_create_flow_action_dest_port(sh->fdb_domain,
							ref->port_id,
							&resource->action);
	if (ret) {
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PORT_ID], idx);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create action");
		return NULL;
	}
	resource->idx = idx;
	return &resource->entry;
}

struct mlx5_list_entry *
flow_dv_port_id_clone_cb(void *tool_ctx,
			 struct mlx5_list_entry *entry __rte_unused,
			 void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_port_id_action_resource *resource;
	uint32_t idx;

	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PORT_ID], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate port_id action memory");
		return NULL;
	}
	memcpy(resource, entry, sizeof(*resource));
	resource->idx = idx;
	return &resource->entry;
}

void
flow_dv_port_id_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_port_id_action_resource *resource =
				  container_of(entry, typeof(*resource), entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PORT_ID], resource->idx);
}

/**
 * Find existing table port ID resource or create and register a new one.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] ref
 *   Pointer to port ID action resource reference.
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
			 struct mlx5_flow_dv_port_id_action_resource *ref,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_list_entry *entry;
	struct mlx5_flow_dv_port_id_action_resource *resource;
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = ref,
	};

	entry = mlx5_list_register(priv->sh->port_id_action_list, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->dv.port_id_action = resource;
	dev_flow->handle->rix_port_id_action = resource->idx;
	return 0;
}

int
flow_dv_push_vlan_match_cb(void *tool_ctx __rte_unused,
			   struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_push_vlan_action_resource *ref = ctx->data;
	struct mlx5_flow_dv_push_vlan_action_resource *res =
				       container_of(entry, typeof(*res), entry);

	return ref->vlan_tag != res->vlan_tag || ref->ft_type != res->ft_type;
}

struct mlx5_list_entry *
flow_dv_push_vlan_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_push_vlan_action_resource *ref = ctx->data;
	struct mlx5_flow_dv_push_vlan_action_resource *resource;
	struct mlx5dv_dr_domain *domain;
	uint32_t idx;
	int ret;

	/* Register new port id action resource. */
	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PUSH_VLAN], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate push_vlan action memory");
		return NULL;
	}
	*resource = *ref;
	if (ref->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (ref->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;
	ret = mlx5_flow_os_create_flow_action_push_vlan(domain, ref->vlan_tag,
							&resource->action);
	if (ret) {
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PUSH_VLAN], idx);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create push vlan action");
		return NULL;
	}
	resource->idx = idx;
	return &resource->entry;
}

struct mlx5_list_entry *
flow_dv_push_vlan_clone_cb(void *tool_ctx,
			   struct mlx5_list_entry *entry __rte_unused,
			   void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_push_vlan_action_resource *resource;
	uint32_t idx;

	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_PUSH_VLAN], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate push_vlan action memory");
		return NULL;
	}
	memcpy(resource, entry, sizeof(*resource));
	resource->idx = idx;
	return &resource->entry;
}

void
flow_dv_push_vlan_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_push_vlan_action_resource *resource =
				  container_of(entry, typeof(*resource), entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PUSH_VLAN], resource->idx);
}

/**
 * Find existing push vlan resource or create and register a new one.
 *
 * @param [in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] ref
 *   Pointer to port ID action resource reference.
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
			struct mlx5_flow_dv_push_vlan_action_resource *ref,
			struct mlx5_flow *dev_flow,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_push_vlan_action_resource *resource;
	struct mlx5_list_entry *entry;
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = ref,
	};

	entry = mlx5_list_register(priv->sh->push_vlan_action_list, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);

	dev_flow->handle->dvh.rix_push_vlan = resource->idx;
	dev_flow->dv.push_vlan_res = resource;
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
size_t
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
int
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
 * Validate the generic modify field actions.
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the modify action.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Number of header fields to modify (0 or more) on success,
 *   a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_modify_field(struct rte_eth_dev *dev,
				   const uint64_t action_flags,
				   const struct rte_flow_action *action,
				   const struct rte_flow_attr *attr,
				   bool root,
				   struct rte_flow_error *error)
{
	int ret = 0;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	struct mlx5_hca_attr *hca_attr = &priv->sh->cdev->config.hca_attr;
	const struct rte_flow_action_modify_field *action_modify_field =
		action->conf;
	uint32_t dst_width, src_width;

	ret = flow_dv_validate_action_modify_hdr(action_flags, action, error);
	if (ret)
		return ret;
	if (action_modify_field->src.field == RTE_FLOW_FIELD_FLEX_ITEM ||
	    action_modify_field->dst.field == RTE_FLOW_FIELD_FLEX_ITEM)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"flex item fields modification"
				" is not supported");
	dst_width = mlx5_flow_item_field_width(dev, action_modify_field->dst.field,
					       -1, attr, error);
	src_width = mlx5_flow_item_field_width(dev, action_modify_field->src.field,
					       dst_width, attr, error);
	if (action_modify_field->width == 0)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"no bits are requested to be modified");
	else if (action_modify_field->width > dst_width ||
		 action_modify_field->width > src_width)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"cannot modify more bits than"
				" the width of a field");
	if (action_modify_field->dst.field != RTE_FLOW_FIELD_VALUE &&
	    action_modify_field->dst.field != RTE_FLOW_FIELD_POINTER) {
		if (action_modify_field->dst.offset +
		    action_modify_field->width > dst_width)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"destination offset is too big");
		if (action_modify_field->dst.level &&
		    action_modify_field->dst.field != RTE_FLOW_FIELD_TAG)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"inner header fields modification"
					" is not supported");
	}
	if (action_modify_field->src.field != RTE_FLOW_FIELD_VALUE &&
	    action_modify_field->src.field != RTE_FLOW_FIELD_POINTER) {
		if (root)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"modify field action is not"
					" supported for group 0");
		if (action_modify_field->src.offset +
		    action_modify_field->width > src_width)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"source offset is too big");
		if (action_modify_field->src.level &&
		    action_modify_field->src.field != RTE_FLOW_FIELD_TAG)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"inner header fields modification"
					" is not supported");
	}
	if ((action_modify_field->dst.field ==
	     action_modify_field->src.field) &&
	    (action_modify_field->dst.level ==
	     action_modify_field->src.level))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source and destination fields"
				" cannot be the same");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_VALUE ||
	    action_modify_field->dst.field == RTE_FLOW_FIELD_POINTER ||
	    action_modify_field->dst.field == RTE_FLOW_FIELD_MARK)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"mark, immediate value or a pointer to it"
				" cannot be used as a destination");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_START ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_START)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifications of an arbitrary"
				" place in a packet is not supported");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_VLAN_TYPE ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_VLAN_TYPE)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifications of the 802.1Q Tag"
				" Identifier is not supported");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_VXLAN_VNI ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_VXLAN_VNI)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifications of the VXLAN Network"
				" Identifier is not supported");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_GENEVE_VNI ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_GENEVE_VNI)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifications of the GENEVE Network"
				" Identifier is not supported");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_MARK ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_MARK)
		if (config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
		    !mlx5_flow_ext_mreg_supported(dev))
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"cannot modify mark in legacy mode"
					" or without extensive registers");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_META ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_META) {
		if (config->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
		    !mlx5_flow_ext_mreg_supported(dev))
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"cannot modify meta without"
					" extensive registers support");
		ret = flow_dv_get_metadata_reg(dev, attr, error);
		if (ret < 0 || ret == REG_NON)
			return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"cannot modify meta without"
					" extensive registers available");
	}
	if (action_modify_field->operation == RTE_FLOW_MODIFY_SUB)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"sub operations are not supported");
	if (action_modify_field->dst.field == RTE_FLOW_FIELD_IPV4_ECN ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_IPV4_ECN ||
	    action_modify_field->dst.field == RTE_FLOW_FIELD_IPV6_ECN ||
	    action_modify_field->src.field == RTE_FLOW_FIELD_IPV6_ECN)
		if (!hca_attr->modify_outer_ip_ecn && root)
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifications of the ECN for current firmware is not supported");
	return (action_modify_field->width / 32) +
	       !!(action_modify_field->width % 32);
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
flow_dv_validate_action_jump(struct rte_eth_dev *dev,
			     const struct mlx5_flow_tunnel *tunnel,
			     const struct rte_flow_action *action,
			     uint64_t action_flags,
			     const struct rte_flow_attr *attributes,
			     bool external, struct rte_flow_error *error)
{
	uint32_t target_group, table = 0;
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct flow_grp_info grp_info = {
		.external = !!external,
		.transfer = !!attributes->transfer,
		.fdb_def_rule = !!priv->fdb_def_rule,
		.std_tbl_fix = 0
	};
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
	ret = mlx5_flow_group_to_table(dev, tunnel, target_group, &table,
				       &grp_info, error);
	if (ret)
		return ret;
	if (attributes->group == target_group &&
	    !(action_flags & (MLX5_FLOW_ACTION_TUNNEL_SET |
			      MLX5_FLOW_ACTION_TUNNEL_MATCH)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "target group must be other than"
					  " the current flow group");
	if (table == 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "root table shouldn't be destination");
	return 0;
}

/*
 * Validate action PORT_ID / REPRESENTED_PORT.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] action
 *   PORT_ID / REPRESENTED_PORT action structure.
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
	const struct rte_flow_action_ethdev *ethdev;
	struct mlx5_priv *act_priv;
	struct mlx5_priv *dev_priv;
	uint16_t port;

	if (!attr->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "port action is valid in transfer"
					  " mode only");
	if (!action || !action->conf)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL,
					  "port action parameters must be"
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
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_PORT_ID:
		port_id = action->conf;
		port = port_id->original ? dev->data->port_id : port_id->id;
		break;
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		ethdev = action->conf;
		port = ethdev->port_id;
		break;
	default:
		MLX5_ASSERT(false);
		return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION, action,
				 "unknown E-Switch action");
	}
	act_priv = mlx5_port_to_eswitch_info(port, false);
	if (!act_priv)
		return rte_flow_error_set
				(error, rte_errno,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF, action->conf,
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
 * @param root
 *   Whether action is on root table.
 *
 * @return
 *   Max number of modify header actions device can support.
 */
static inline unsigned int
flow_dv_modify_hdr_action_max(struct rte_eth_dev *dev __rte_unused,
			      bool root)
{
	/*
	 * There's no way to directly query the max capacity from FW.
	 * The maximal value on root table should be assumed to be supported.
	 */
	if (!root)
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
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[in] action
 *   Pointer to the meter action.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[in] port_id_item
 *   Pointer to item indicating port id.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_validate_action_meter(struct rte_eth_dev *dev,
				uint64_t action_flags, uint64_t item_flags,
				const struct rte_flow_action *action,
				const struct rte_flow_attr *attr,
				const struct rte_flow_item *port_id_item,
				bool *def_policy,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_meter *am = action->conf;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *mtr_policy;
	struct mlx5_flow_mtr_mng *mtrmng = priv->sh->mtrmng;
	uint16_t flow_src_port = priv->representor_id;
	bool all_ports = false;

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
	fm = mlx5_flow_meter_find(priv, am->mtr_id, NULL);
	if (!fm)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Meter not found");
	/* aso meter can always be shared by different domains */
	if (fm->ref_cnt && !priv->sh->meter_aso_en &&
	    !(fm->transfer == attr->transfer ||
	      (!fm->ingress && !attr->ingress && attr->egress) ||
	      (!fm->egress && !attr->egress && attr->ingress)))
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, NULL,
			"Flow attributes domain are either invalid "
			"or have a domain conflict with current "
			"meter attributes");
	if (fm->def_policy) {
		if (!((attr->transfer &&
			mtrmng->def_policy[MLX5_MTR_DOMAIN_TRANSFER]) ||
			(attr->egress &&
			mtrmng->def_policy[MLX5_MTR_DOMAIN_EGRESS]) ||
			(attr->ingress &&
			mtrmng->def_policy[MLX5_MTR_DOMAIN_INGRESS])))
			return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Flow attributes domain "
					  "have a conflict with current "
					  "meter domain attributes");
		*def_policy = true;
	} else {
		mtr_policy = mlx5_flow_meter_policy_find(dev,
						fm->policy_id, NULL);
		if (!mtr_policy)
			return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Invalid policy id for meter ");
		if (!((attr->transfer && mtr_policy->transfer) ||
			(attr->egress && mtr_policy->egress) ||
			(attr->ingress && mtr_policy->ingress)))
			return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Flow attributes domain "
					  "have a conflict with current "
					  "meter domain attributes");
		if (port_id_item) {
			if (mlx5_flow_get_item_vport_id(dev, port_id_item, &flow_src_port,
							&all_ports, error))
				return -rte_errno;
		}
		if (attr->transfer) {
			/* When flow matching all src ports, meter should not have drop count. */
			if (all_ports && (fm->drop_cnt || mtr_policy->hierarchy_match_port))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, NULL,
							  "Meter drop count or "
							  "modify_field/set_tag in meter hierarchy "
							  "not supported when matching all ports.");
		} else if (mtr_policy->is_rss) {
			struct mlx5_flow_meter_policy *fp;
			struct mlx5_meter_policy_action_container *acg;
			struct mlx5_meter_policy_action_container *acy;
			const struct rte_flow_action *rss_act;
			int ret;

			fp = mlx5_flow_meter_hierarchy_get_final_policy(dev,
								mtr_policy);
			if (fp == NULL)
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						  "Unable to get the final "
						  "policy in the hierarchy");
			acg = &fp->act_cnt[RTE_COLOR_GREEN];
			acy = &fp->act_cnt[RTE_COLOR_YELLOW];
			MLX5_ASSERT(acg->fate_action ==
				    MLX5_FLOW_FATE_SHARED_RSS ||
				    acy->fate_action ==
				    MLX5_FLOW_FATE_SHARED_RSS);
			if (acg->fate_action == MLX5_FLOW_FATE_SHARED_RSS)
				rss_act = acg->rss;
			else
				rss_act = acy->rss;
			ret = mlx5_flow_validate_action_rss(rss_act,
					action_flags, dev, attr,
					item_flags, error);
			if (ret)
				return ret;
		}
		*def_policy = false;
	}
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

	if (!priv->sh->cdev->config.devx ||
	    (priv->sh->sws_cmng.counter_fallback && !priv->sh->aso_age_mng))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "age action not supported");
	if (!(action->conf))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be null");
	if (!(age->timeout))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "invalid timeout value 0");
	if (action_flags & MLX5_FLOW_ACTION_AGE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "duplicate age actions set");
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

int
flow_dv_modify_match_cb(void *tool_ctx __rte_unused,
			struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_modify_hdr_resource *ref = ctx->data;
	struct mlx5_flow_dv_modify_hdr_resource *resource =
				  container_of(entry, typeof(*resource), entry);
	uint32_t key_len = sizeof(*ref) - offsetof(typeof(*ref), ft_type);

	key_len += ref->actions_num * sizeof(ref->actions[0]);
	return ref->actions_num != resource->actions_num ||
	       memcmp(&ref->ft_type, &resource->ft_type, key_len);
}

static struct mlx5_indexed_pool *
flow_dv_modify_ipool_get(struct mlx5_dev_ctx_shared *sh, uint8_t index)
{
	struct mlx5_indexed_pool *ipool = __atomic_load_n
				     (&sh->mdh_ipools[index], __ATOMIC_SEQ_CST);

	if (!ipool) {
		struct mlx5_indexed_pool *expected = NULL;
		struct mlx5_indexed_pool_config cfg =
		    (struct mlx5_indexed_pool_config) {
		       .size = sizeof(struct mlx5_flow_dv_modify_hdr_resource) +
								   (index + 1) *
					   sizeof(struct mlx5_modification_cmd),
		       .trunk_size = 64,
		       .grow_trunk = 3,
		       .grow_shift = 2,
		       .need_lock = 1,
		       .release_mem_en = !!sh->config.reclaim_mode,
		       .per_core_cache =
				       sh->config.reclaim_mode ? 0 : (1 << 16),
		       .malloc = mlx5_malloc,
		       .free = mlx5_free,
		       .type = "mlx5_modify_action_resource",
		};

		cfg.size = RTE_ALIGN(cfg.size, sizeof(ipool));
		ipool = mlx5_ipool_create(&cfg);
		if (!ipool)
			return NULL;
		if (!__atomic_compare_exchange_n(&sh->mdh_ipools[index],
						 &expected, ipool, false,
						 __ATOMIC_SEQ_CST,
						 __ATOMIC_SEQ_CST)) {
			mlx5_ipool_destroy(ipool);
			ipool = __atomic_load_n(&sh->mdh_ipools[index],
						__ATOMIC_SEQ_CST);
		}
	}
	return ipool;
}

struct mlx5_list_entry *
flow_dv_modify_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5dv_dr_domain *ns;
	struct mlx5_flow_dv_modify_hdr_resource *entry;
	struct mlx5_flow_dv_modify_hdr_resource *ref = ctx->data;
	struct mlx5_indexed_pool *ipool = flow_dv_modify_ipool_get(sh,
							  ref->actions_num - 1);
	int ret;
	uint32_t data_len = ref->actions_num * sizeof(ref->actions[0]);
	uint32_t key_len = sizeof(*ref) - offsetof(typeof(*ref), ft_type);
	uint32_t idx;

	if (unlikely(!ipool)) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot allocate modify ipool");
		return NULL;
	}
	entry = mlx5_ipool_zmalloc(ipool, &idx);
	if (!entry) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	rte_memcpy(&entry->ft_type,
		   RTE_PTR_ADD(ref, offsetof(typeof(*ref), ft_type)),
		   key_len + data_len);
	if (entry->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		ns = sh->fdb_domain;
	else if (entry->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_TX)
		ns = sh->tx_domain;
	else
		ns = sh->rx_domain;
	ret = mlx5_flow_os_create_flow_action_modify_header
					(sh->cdev->ctx, ns, entry,
					 data_len, &entry->action);
	if (ret) {
		mlx5_ipool_free(sh->mdh_ipools[ref->actions_num - 1], idx);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create modification action");
		return NULL;
	}
	entry->idx = idx;
	return &entry->entry;
}

struct mlx5_list_entry *
flow_dv_modify_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
			void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_modify_hdr_resource *entry;
	struct mlx5_flow_dv_modify_hdr_resource *ref = ctx->data;
	uint32_t data_len = ref->actions_num * sizeof(ref->actions[0]);
	uint32_t idx;

	entry = mlx5_ipool_malloc(sh->mdh_ipools[ref->actions_num - 1],
				  &idx);
	if (!entry) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	memcpy(entry, oentry, sizeof(*entry) + data_len);
	entry->idx = idx;
	return &entry->entry;
}

void
flow_dv_modify_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_modify_hdr_resource *res =
		container_of(entry, typeof(*res), entry);

	mlx5_ipool_free(sh->mdh_ipools[res->actions_num - 1], res->idx);
}

/**
 * Validate the sample action.
 *
 * @param[in, out] action_flags
 *   Holds the actions detected until now.
 * @param[in] action
 *   Pointer to the sample action.
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[in] item_flags
 *   Holds the items detected.
 * @param[in] rss
 *   Pointer to the RSS action.
 * @param[out] sample_rss
 *   Pointer to the RSS action in sample action list.
 * @param[out] count
 *   Pointer to the COUNT action in sample action list.
 * @param[out] fdb_mirror
 *   Pointer to the FDB mirror flag.
 * @param root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_validate_action_sample(uint64_t *action_flags,
			       const struct rte_flow_action *action,
			       struct rte_eth_dev *dev,
			       const struct rte_flow_attr *attr,
			       uint64_t item_flags,
			       const struct rte_flow_action_rss *rss,
			       const struct rte_flow_action_rss **sample_rss,
			       const struct rte_flow_action_count **count,
			       int *fdb_mirror,
			       bool root,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *dev_conf = &priv->sh->config;
	const struct rte_flow_action_sample *sample = action->conf;
	const struct rte_flow_action *act;
	uint64_t sub_action_flags = 0;
	uint16_t queue_index = 0xFFFF;
	int actions_n = 0;
	int ret;

	if (!sample)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "configuration cannot be NULL");
	if (sample->ratio == 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "ratio value starts from 1");
	if (!priv->sh->cdev->config.devx ||
	    (sample->ratio > 0 && !priv->sampler_en))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "sample action not supported");
	if (*action_flags & MLX5_FLOW_ACTION_SAMPLE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Multiple sample actions not "
					  "supported");
	if (*action_flags & MLX5_FLOW_ACTION_METER)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, meter should "
					  "be after sample action");
	if (*action_flags & MLX5_FLOW_ACTION_JUMP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "wrong action order, jump should "
					  "be after sample action");
	if (*action_flags & MLX5_FLOW_ACTION_CT)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Sample after CT not supported");
	act = sample->actions;
	for (; act->type != RTE_FLOW_ACTION_TYPE_END; act++) {
		if (actions_n == MLX5_DV_MAX_NUMBER_OF_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  act, "too many actions");
		switch (act->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = mlx5_flow_validate_action_queue(act,
							      sub_action_flags,
							      dev,
							      attr, error);
			if (ret < 0)
				return ret;
			queue_index = ((const struct rte_flow_action_queue *)
							(act->conf))->index;
			sub_action_flags |= MLX5_FLOW_ACTION_QUEUE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			*sample_rss = act->conf;
			ret = mlx5_flow_validate_action_rss(act,
							    sub_action_flags,
							    dev, attr,
							    item_flags,
							    error);
			if (ret < 0)
				return ret;
			if (rss && *sample_rss &&
			    ((*sample_rss)->level != rss->level ||
			    (*sample_rss)->types != rss->types))
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"Can't use the different RSS types "
					"or level in the same flow");
			if (*sample_rss != NULL && (*sample_rss)->queue_num)
				queue_index = (*sample_rss)->queue[0];
			sub_action_flags |= MLX5_FLOW_ACTION_RSS;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = flow_dv_validate_action_mark(dev, act,
							   sub_action_flags,
							   attr, error);
			if (ret < 0)
				return ret;
			if (dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY)
				sub_action_flags |= MLX5_FLOW_ACTION_MARK |
						MLX5_FLOW_ACTION_MARK_EXT;
			else
				sub_action_flags |= MLX5_FLOW_ACTION_MARK;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_dv_validate_action_count
				(dev, false, *action_flags | sub_action_flags,
				 root, error);
			if (ret < 0)
				return ret;
			*count = act->conf;
			sub_action_flags |= MLX5_FLOW_ACTION_COUNT;
			*action_flags |= MLX5_FLOW_ACTION_COUNT;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			ret = flow_dv_validate_action_port_id(dev,
							      sub_action_flags,
							      act,
							      attr,
							      error);
			if (ret)
				return ret;
			sub_action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			ret = flow_dv_validate_action_raw_encap_decap
				(dev, NULL, act->conf, attr, &sub_action_flags,
				 &actions_n, action, item_flags, error);
			if (ret < 0)
				return ret;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			ret = flow_dv_validate_action_l2_encap(dev,
							       sub_action_flags,
							       act, attr,
							       error);
			if (ret < 0)
				return ret;
			sub_action_flags |= MLX5_FLOW_ACTION_ENCAP;
			++actions_n;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "Doesn't support optional "
						  "action");
		}
	}
	if (attr->ingress) {
		if (!(sub_action_flags & (MLX5_FLOW_ACTION_QUEUE |
					  MLX5_FLOW_ACTION_RSS)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "Ingress must has a dest "
						  "QUEUE for Sample");
	} else if (attr->egress) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "Sample Only support Ingress "
					  "or E-Switch");
	} else if (sample->actions->type != RTE_FLOW_ACTION_TYPE_END) {
		MLX5_ASSERT(attr->transfer);
		if (sample->ratio > 1)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "E-Switch doesn't support "
						  "any optional action "
						  "for sampling");
		if (sub_action_flags & MLX5_FLOW_ACTION_QUEUE)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action QUEUE");
		if (sub_action_flags & MLX5_FLOW_ACTION_RSS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "unsupported action QUEUE");
		if (!(sub_action_flags & MLX5_FLOW_ACTION_PORT_ID))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "E-Switch must has a dest "
						  "port for mirroring");
		*fdb_mirror = 1;
	}
	/* Continue validation for Xcap actions.*/
	if ((sub_action_flags & MLX5_FLOW_XCAP_ACTIONS) &&
	    (queue_index == 0xFFFF || !mlx5_rxq_is_hairpin(dev, queue_index))) {
		if ((sub_action_flags & MLX5_FLOW_XCAP_ACTIONS) ==
		     MLX5_FLOW_XCAP_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "encap and decap "
						  "combination aren't "
						  "supported");
		if (attr->ingress && (sub_action_flags & MLX5_FLOW_ACTION_ENCAP))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "encap is not supported"
						  " for ingress traffic");
	}
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
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	uint32_t key_len = sizeof(*resource) -
			   offsetof(typeof(*resource), ft_type) +
			   resource->actions_num * sizeof(resource->actions[0]);
	struct mlx5_list_entry *entry;
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = resource,
	};
	struct mlx5_hlist *modify_cmds;
	uint64_t key64;

	modify_cmds = flow_dv_hlist_prepare(sh, &sh->modify_cmds,
				"hdr_modify",
				MLX5_FLOW_HDR_MODIFY_HTABLE_SZ,
				true, false, sh,
				flow_dv_modify_create_cb,
				flow_dv_modify_match_cb,
				flow_dv_modify_remove_cb,
				flow_dv_modify_clone_cb,
				flow_dv_modify_clone_free_cb,
				error);
	if (unlikely(!modify_cmds))
		return -rte_errno;
	resource->root = !dev_flow->dv.group;
	if (resource->actions_num > flow_dv_modify_hdr_action_max(dev,
								resource->root))
		return rte_flow_error_set(error, EOVERFLOW,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "too many modify header items");
	key64 = __rte_raw_cksum(&resource->ft_type, key_len, 0);
	entry = mlx5_hlist_register(modify_cmds, key64, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->handle->dvh.modify_hdr = resource;
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
 *   mlx5 flow counter pool in the container.
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
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	struct mlx5_flow_counter_pool *pool;

	/* Decrease to original index and clear shared bit. */
	idx = (idx - 1) & (MLX5_CNT_SHARED_OFFSET - 1);
	MLX5_ASSERT(idx / MLX5_COUNTERS_PER_POOL < MLX5_COUNTER_POOLS_MAX_NUM);
	pool = cmng->pools[idx / MLX5_COUNTERS_PER_POOL];
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
 * @param[in] cmng
 *   Pointer to the counter management.
 * @param[in] id
 *   The counter devx ID.
 *
 * @return
 *   The counter pool pointer if exists, NULL otherwise,
 */
static struct mlx5_flow_counter_pool *
flow_dv_find_pool_by_id(struct mlx5_flow_counter_mng *cmng, int id)
{
	uint32_t i;
	struct mlx5_flow_counter_pool *pool = NULL;

	rte_spinlock_lock(&cmng->pool_update_sl);
	/* Check last used pool. */
	if (cmng->last_pool_idx != POOL_IDX_INVALID &&
	    flow_dv_is_counter_in_pool(cmng->pools[cmng->last_pool_idx], id)) {
		pool = cmng->pools[cmng->last_pool_idx];
		goto out;
	}
	/* ID out of range means no suitable pool in the container. */
	if (id > cmng->max_id || id < cmng->min_id)
		goto out;
	/*
	 * Find the pool from the end of the container, since mostly counter
	 * ID is sequence increasing, and the last pool should be the needed
	 * one.
	 */
	i = cmng->n_valid;
	while (i--) {
		struct mlx5_flow_counter_pool *pool_tmp = cmng->pools[i];

		if (flow_dv_is_counter_in_pool(pool_tmp, id)) {
			pool = pool_tmp;
			break;
		}
	}
out:
	rte_spinlock_unlock(&cmng->pool_update_sl);
	return pool;
}

/**
 * Query a devx flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] counter
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
	int offset;

	cnt = flow_dv_counter_get_by_idx(dev, counter, &pool);
	MLX5_ASSERT(pool);
	if (priv->sh->sws_cmng.counter_fallback)
		return mlx5_devx_cmd_flow_counter_query(cnt->dcs_when_active, 0,
					0, pkts, bytes, 0, NULL, NULL, 0);
	rte_spinlock_lock(&pool->sl);
	if (!pool->raw) {
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
 * @param[in] age
 *   Whether the pool is for counter that was allocated for aging.
 *
 * @return
 *   The pool container pointer on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter_pool *
flow_dv_pool_create(struct rte_eth_dev *dev, struct mlx5_devx_obj *dcs,
		    uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	bool fallback = cmng->counter_fallback;
	uint32_t size = sizeof(*pool);

	if (cmng->n_valid == MLX5_COUNTER_POOLS_MAX_NUM) {
		DRV_LOG(ERR, "All counter is in used, try again later.");
		rte_errno = EAGAIN;
		return NULL;
	}
	size += MLX5_COUNTERS_PER_POOL * MLX5_CNT_SIZE;
	size += (!age ? 0 : MLX5_COUNTERS_PER_POOL * MLX5_AGE_SIZE);
	pool = mlx5_malloc(MLX5_MEM_ZERO, size, 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		return NULL;
	}
	pool->raw = NULL;
	pool->is_aged = !!age;
	pool->query_gen = 0;
	pool->min_dcs = dcs;
	rte_spinlock_init(&pool->sl);
	rte_spinlock_init(&pool->csl);
	TAILQ_INIT(&pool->counters[0]);
	TAILQ_INIT(&pool->counters[1]);
	pool->time_of_last_age_check = MLX5_CURR_TIME_SEC;
	rte_spinlock_lock(&cmng->pool_update_sl);
	pool->index = cmng->n_valid;
	cmng->pools[pool->index] = pool;
	cmng->n_valid++;
	if (unlikely(fallback)) {
		int base = RTE_ALIGN_FLOOR(dcs->id, MLX5_COUNTERS_PER_POOL);

		if (base < cmng->min_id)
			cmng->min_id = base;
		if (base > cmng->max_id)
			cmng->max_id = base + MLX5_COUNTERS_PER_POOL - 1;
		cmng->last_pool_idx = pool->index;
	}
	rte_spinlock_unlock(&cmng->pool_update_sl);
	return pool;
}

/**
 * Prepare a new counter and/or a new counter pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] cnt_free
 *   Where to put the pointer of a new counter.
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
			     uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	struct mlx5_flow_counter_pool *pool;
	struct mlx5_counters tmp_tq;
	struct mlx5_devx_obj *dcs = NULL;
	struct mlx5_flow_counter *cnt;
	enum mlx5_counter_type cnt_type =
			age ? MLX5_COUNTER_TYPE_AGE : MLX5_COUNTER_TYPE_ORIGIN;
	bool fallback = priv->sh->sws_cmng.counter_fallback;
	uint32_t i;

	if (fallback) {
		/* bulk_bitmap must be 0 for single counter allocation. */
		dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->cdev->ctx, 0);
		if (!dcs)
			return NULL;
		pool = flow_dv_find_pool_by_id(cmng, dcs->id);
		if (!pool) {
			pool = flow_dv_pool_create(dev, dcs, age);
			if (!pool) {
				mlx5_devx_cmd_destroy(dcs);
				return NULL;
			}
		}
		i = dcs->id % MLX5_COUNTERS_PER_POOL;
		cnt = MLX5_POOL_GET_CNT(pool, i);
		cnt->pool = pool;
		cnt->dcs_when_free = dcs;
		*cnt_free = cnt;
		return pool;
	}
	dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->cdev->ctx, 0x4);
	if (!dcs) {
		rte_errno = ENODATA;
		return NULL;
	}
	pool = flow_dv_pool_create(dev, dcs, age);
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
	rte_spinlock_lock(&cmng->csl[cnt_type]);
	TAILQ_CONCAT(&cmng->counters[cnt_type], &tmp_tq, next);
	rte_spinlock_unlock(&cmng->csl[cnt_type]);
	*cnt_free = MLX5_POOL_GET_CNT(pool, 0);
	(*cnt_free)->pool = pool;
	return pool;
}

/**
 * Allocate a flow counter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] age
 *   Whether the counter was allocated for aging.
 *
 * @return
 *   Index to flow counter on success, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_dv_counter_alloc(struct rte_eth_dev *dev, uint32_t age)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt_free = NULL;
	bool fallback = priv->sh->sws_cmng.counter_fallback;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	enum mlx5_counter_type cnt_type =
			age ? MLX5_COUNTER_TYPE_AGE : MLX5_COUNTER_TYPE_ORIGIN;
	uint32_t cnt_idx;

	if (!priv->sh->cdev->config.devx) {
		rte_errno = ENOTSUP;
		return 0;
	}
	/* Get free counters from container. */
	rte_spinlock_lock(&cmng->csl[cnt_type]);
	cnt_free = TAILQ_FIRST(&cmng->counters[cnt_type]);
	if (cnt_free)
		TAILQ_REMOVE(&cmng->counters[cnt_type], cnt_free, next);
	rte_spinlock_unlock(&cmng->csl[cnt_type]);
	if (!cnt_free && !flow_dv_counter_pool_prepare(dev, &cnt_free, age))
		goto err;
	pool = cnt_free->pool;
	if (fallback)
		cnt_free->dcs_when_active = cnt_free->dcs_when_free;
	/* Create a DV counter action only in the first time usage. */
	if (!cnt_free->action) {
		uint16_t offset;
		struct mlx5_devx_obj *dcs;
		int ret;

		if (!fallback) {
			offset = MLX5_CNT_ARRAY_IDX(pool, cnt_free);
			dcs = pool->min_dcs;
		} else {
			offset = 0;
			dcs = cnt_free->dcs_when_free;
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
	/* Update the counter reset values. */
	if (_flow_dv_query_count(dev, cnt_idx, &cnt_free->hits,
				 &cnt_free->bytes))
		goto err;
	if (!fallback && !priv->sh->sws_cmng.query_thread_on)
		/* Start the asynchronous batch query by the host thread. */
		mlx5_set_query_alarm(priv->sh);
	/*
	 * When the count action isn't shared (by ID), shared_info field is
	 * used for indirect action API's refcnt.
	 * When the counter action is not shared neither by ID nor by indirect
	 * action API, shared info must be 1.
	 */
	cnt_free->shared_info.refcnt = 1;
	return cnt_idx;
err:
	if (cnt_free) {
		cnt_free->pool = pool;
		if (fallback)
			cnt_free->dcs_when_free = cnt_free->dcs_when_active;
		rte_spinlock_lock(&cmng->csl[cnt_type]);
		TAILQ_INSERT_TAIL(&cmng->counters[cnt_type], cnt_free, next);
		rte_spinlock_unlock(&cmng->csl[cnt_type]);
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
	uint16_t expected = AGE_CANDIDATE;

	age_info = GET_PORT_AGE_INFO(priv);
	age_param = flow_dv_counter_idx_get_age(dev, counter);
	if (!__atomic_compare_exchange_n(&age_param->state, &expected,
					 AGE_FREE, false, __ATOMIC_RELAXED,
					 __ATOMIC_RELAXED)) {
		/**
		 * We need the lock even it is age timeout,
		 * since counter may still in process.
		 */
		rte_spinlock_lock(&age_info->aged_sl);
		TAILQ_REMOVE(&age_info->aged_counters, cnt, next);
		rte_spinlock_unlock(&age_info->aged_sl);
		__atomic_store_n(&age_param->state, AGE_FREE, __ATOMIC_RELAXED);
	}
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
flow_dv_counter_free(struct rte_eth_dev *dev, uint32_t counter)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter_pool *pool = NULL;
	struct mlx5_flow_counter *cnt;
	enum mlx5_counter_type cnt_type;

	if (!counter)
		return;
	cnt = flow_dv_counter_get_by_idx(dev, counter, &pool);
	MLX5_ASSERT(pool);
	if (pool->is_aged) {
		flow_dv_counter_remove_from_age(dev, counter, cnt);
	} else {
		/*
		 * If the counter action is shared by indirect action API,
		 * the atomic function reduces its references counter.
		 * If after the reduction the action is still referenced, the
		 * function returns here and does not release it.
		 * When the counter action is not shared by
		 * indirect action API, shared info is 1 before the reduction,
		 * so this condition is failed and function doesn't return here.
		 */
		if (__atomic_fetch_sub(&cnt->shared_info.refcnt, 1,
				       __ATOMIC_RELAXED) - 1)
			return;
	}
	cnt->pool = pool;
	/*
	 * Put the counter back to list to be updated in none fallback mode.
	 * Currently, we are using two list alternately, while one is in query,
	 * add the freed counter to the other list based on the pool query_gen
	 * value. After query finishes, add counter the list to the global
	 * container counter list. The list changes while query starts. In
	 * this case, lock will not be needed as query callback and release
	 * function both operate with the different list.
	 */
	if (!priv->sh->sws_cmng.counter_fallback) {
		rte_spinlock_lock(&pool->csl);
		TAILQ_INSERT_TAIL(&pool->counters[pool->query_gen], cnt, next);
		rte_spinlock_unlock(&pool->csl);
	} else {
		cnt->dcs_when_free = cnt->dcs_when_active;
		cnt_type = pool->is_aged ? MLX5_COUNTER_TYPE_AGE :
					   MLX5_COUNTER_TYPE_ORIGIN;
		rte_spinlock_lock(&priv->sh->sws_cmng.csl[cnt_type]);
		TAILQ_INSERT_TAIL(&priv->sh->sws_cmng.counters[cnt_type],
				  cnt, next);
		rte_spinlock_unlock(&priv->sh->sws_cmng.csl[cnt_type]);
	}
}

/**
 * Resize a meter id container.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   0 on success, otherwise negative errno value and rte_errno is set.
 */
static int
flow_dv_mtr_container_resize(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pools_mng *pools_mng =
				&priv->sh->mtrmng->pools_mng;
	void *old_pools = pools_mng->pools;
	uint32_t resize = pools_mng->n + MLX5_MTRS_CONTAINER_RESIZE;
	uint32_t mem_size = sizeof(struct mlx5_aso_mtr_pool *) * resize;
	void *pools = mlx5_malloc(MLX5_MEM_ZERO, mem_size, 0, SOCKET_ID_ANY);

	if (!pools) {
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	if (!pools_mng->n)
		if (mlx5_aso_queue_init(priv->sh, ASO_OPC_MOD_POLICER, 1)) {
			mlx5_free(pools);
			return -ENOMEM;
		}
	if (old_pools)
		memcpy(pools, old_pools, pools_mng->n *
				       sizeof(struct mlx5_aso_mtr_pool *));
	pools_mng->n = resize;
	pools_mng->pools = pools;
	if (old_pools)
		mlx5_free(old_pools);
	return 0;
}

/**
 * Prepare a new meter and/or a new meter pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] mtr_free
 *   Where to put the pointer of a new meter.g.
 *
 * @return
 *   The meter pool pointer and @mtr_free is set on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_aso_mtr_pool *
flow_dv_mtr_pool_create(struct rte_eth_dev *dev, struct mlx5_aso_mtr **mtr_free)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pools_mng *pools_mng = &priv->sh->mtrmng->pools_mng;
	struct mlx5_aso_mtr_pool *pool = NULL;
	struct mlx5_devx_obj *dcs = NULL;
	uint32_t i;
	uint32_t log_obj_size;

	log_obj_size = rte_log2_u32(MLX5_ASO_MTRS_PER_POOL >> 1);
	dcs = mlx5_devx_cmd_create_flow_meter_aso_obj(priv->sh->cdev->ctx,
						      priv->sh->cdev->pdn,
						      log_obj_size);
	if (!dcs) {
		rte_errno = ENODATA;
		return NULL;
	}
	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool), 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		claim_zero(mlx5_devx_cmd_destroy(dcs));
		return NULL;
	}
	pool->devx_obj = dcs;
	rte_rwlock_write_lock(&pools_mng->resize_mtrwl);
	pool->index = pools_mng->n_valid;
	if (pool->index == pools_mng->n && flow_dv_mtr_container_resize(dev)) {
		mlx5_free(pool);
		claim_zero(mlx5_devx_cmd_destroy(dcs));
		rte_rwlock_write_unlock(&pools_mng->resize_mtrwl);
		return NULL;
	}
	pools_mng->pools[pool->index] = pool;
	pools_mng->n_valid++;
	rte_rwlock_write_unlock(&pools_mng->resize_mtrwl);
	for (i = 1; i < MLX5_ASO_MTRS_PER_POOL; ++i) {
		pool->mtrs[i].offset = i;
		LIST_INSERT_HEAD(&pools_mng->meters, &pool->mtrs[i], next);
	}
	pool->mtrs[0].offset = 0;
	*mtr_free = &pool->mtrs[0];
	return pool;
}

/**
 * Release a flow meter into pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_idx
 *   Index to aso flow meter.
 */
static void
flow_dv_aso_mtr_release_to_pool(struct rte_eth_dev *dev, uint32_t mtr_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pools_mng *pools_mng =
				&priv->sh->mtrmng->pools_mng;
	struct mlx5_aso_mtr *aso_mtr = mlx5_aso_meter_by_idx(priv, mtr_idx);

	MLX5_ASSERT(aso_mtr);
	rte_spinlock_lock(&pools_mng->mtrsl);
	memset(&aso_mtr->fm, 0, sizeof(struct mlx5_flow_meter_info));
	aso_mtr->state = ASO_METER_FREE;
	LIST_INSERT_HEAD(&pools_mng->meters, aso_mtr, next);
	rte_spinlock_unlock(&pools_mng->mtrsl);
}

/**
 * Allocate a aso flow meter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   Index to aso flow meter on success, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_dv_mtr_alloc(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr *mtr_free = NULL;
	struct mlx5_aso_mtr_pools_mng *pools_mng =
				&priv->sh->mtrmng->pools_mng;
	struct mlx5_aso_mtr_pool *pool;
	uint32_t mtr_idx = 0;

	if (!priv->sh->cdev->config.devx) {
		rte_errno = ENOTSUP;
		return 0;
	}
	/* Allocate the flow meter memory. */
	/* Get free meters from management. */
	rte_spinlock_lock(&pools_mng->mtrsl);
	mtr_free = LIST_FIRST(&pools_mng->meters);
	if (mtr_free)
		LIST_REMOVE(mtr_free, next);
	if (!mtr_free && !flow_dv_mtr_pool_create(dev, &mtr_free)) {
		rte_spinlock_unlock(&pools_mng->mtrsl);
		return 0;
	}
	mtr_free->state = ASO_METER_WAIT;
	rte_spinlock_unlock(&pools_mng->mtrsl);
	pool = container_of(mtr_free,
			struct mlx5_aso_mtr_pool,
			mtrs[mtr_free->offset]);
	mtr_idx = MLX5_MAKE_MTR_IDX(pool->index, mtr_free->offset);
	if (!mtr_free->fm.meter_action_g) {
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
		struct rte_flow_error error;
		uint8_t reg_id;

		reg_id = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, &error);
		mtr_free->fm.meter_action_g =
			mlx5_glue->dv_create_flow_action_aso
						(priv->sh->rx_domain,
						 pool->devx_obj->obj,
						 mtr_free->offset,
						 (1 << MLX5_FLOW_COLOR_GREEN),
						 reg_id - REG_C_0);
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */
		if (!mtr_free->fm.meter_action_g) {
			flow_dv_aso_mtr_release_to_pool(dev, mtr_idx);
			return 0;
		}
	}
	return mtr_idx;
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
			    const struct mlx5_flow_tunnel *tunnel,
			    const struct rte_flow_attr *attributes,
			    const struct flow_grp_info *grp_info,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t lowest_priority = mlx5_get_lowest_priority(dev, attributes);
	int ret = 0;

#ifndef HAVE_MLX5DV_DR
	RTE_SET_USED(tunnel);
	RTE_SET_USED(grp_info);
	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "groups are not supported");
#else
	uint32_t table = 0;

	ret = mlx5_flow_group_to_table(dev, tunnel, attributes->group, &table,
				       grp_info, error);
	if (ret)
		return ret;
	if (!table)
		ret = MLX5DV_DR_ACTION_FLAGS_ROOT_LEVEL;
#endif
	if (attributes->priority != MLX5_FLOW_LOWEST_PRIO_INDICATOR &&
	    attributes->priority > lowest_priority)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL,
					  "priority out of range");
	if (attributes->transfer && !priv->sh->config.dv_esw_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "E-Switch dr is not supported");
	if (attributes->ingress + attributes->egress + attributes->transfer != 1) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "must specify exactly one of "
					  "ingress, egress or transfer");
	}
	return ret;
}

static int
validate_integrity_bits(const struct rte_flow_item_integrity *mask,
			int64_t pattern_flags, uint64_t l3_flags,
			uint64_t l4_flags, uint64_t ip4_flag,
			struct rte_flow_error *error)
{
	if (mask->l3_ok && !(pattern_flags & l3_flags))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "missing L3 protocol");

	if (mask->ipv4_csum_ok && !(pattern_flags & ip4_flag))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "missing IPv4 protocol");

	if ((mask->l4_ok || mask->l4_csum_ok) && !(pattern_flags & l4_flags))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "missing L4 protocol");

	return 0;
}

static int
flow_dv_validate_item_integrity_post(const struct
				     rte_flow_item *integrity_items[2],
				     int64_t pattern_flags,
				     struct rte_flow_error *error)
{
	const struct rte_flow_item_integrity *mask;
	int ret;

	if (pattern_flags & MLX5_FLOW_ITEM_OUTER_INTEGRITY) {
		mask = (typeof(mask))integrity_items[0]->mask;
		ret = validate_integrity_bits(mask, pattern_flags,
					      MLX5_FLOW_LAYER_OUTER_L3,
					      MLX5_FLOW_LAYER_OUTER_L4,
					      MLX5_FLOW_LAYER_OUTER_L3_IPV4,
					      error);
		if (ret)
			return ret;
	}
	if (pattern_flags & MLX5_FLOW_ITEM_INNER_INTEGRITY) {
		mask = (typeof(mask))integrity_items[1]->mask;
		ret = validate_integrity_bits(mask, pattern_flags,
					      MLX5_FLOW_LAYER_INNER_L3,
					      MLX5_FLOW_LAYER_INNER_L4,
					      MLX5_FLOW_LAYER_INNER_L3_IPV4,
					      error);
		if (ret)
			return ret;
	}
	return 0;
}

static int
flow_dv_validate_item_integrity(struct rte_eth_dev *dev,
				const struct rte_flow_item *integrity_item,
				uint64_t pattern_flags, uint64_t *last_item,
				const struct rte_flow_item *integrity_items[2],
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_integrity *mask = (typeof(mask))
						     integrity_item->mask;
	const struct rte_flow_item_integrity *spec = (typeof(spec))
						     integrity_item->spec;

	if (!priv->sh->cdev->config.hca_attr.pkt_integrity_match)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  integrity_item,
					  "packet integrity integrity_item not supported");
	if (!spec)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  integrity_item,
					  "no spec for integrity item");
	if (!mask)
		mask = &rte_flow_item_integrity_mask;
	if (!mlx5_validate_integrity_item(mask))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  integrity_item,
					  "unsupported integrity filter");
	if ((mask->l3_ok & !spec->l3_ok) || (mask->l4_ok & !spec->l4_ok) ||
		(mask->ipv4_csum_ok & !spec->ipv4_csum_ok) ||
		(mask->l4_csum_ok & !spec->l4_csum_ok))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL, "negative integrity flow is not supported");
	if (spec->level > 1) {
		if (pattern_flags & MLX5_FLOW_ITEM_INNER_INTEGRITY)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM,
				 NULL, "multiple inner integrity items not supported");
		integrity_items[1] = integrity_item;
		*last_item |= MLX5_FLOW_ITEM_INNER_INTEGRITY;
	} else {
		if (pattern_flags & MLX5_FLOW_ITEM_OUTER_INTEGRITY)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_ITEM,
				 NULL, "multiple outer integrity items not supported");
		integrity_items[0] = integrity_item;
		*last_item |= MLX5_FLOW_ITEM_OUTER_INTEGRITY;
	}
	return 0;
}

static int
flow_dv_validate_item_flex(struct rte_eth_dev *dev,
			   const struct rte_flow_item *item,
			   uint64_t item_flags,
			   uint64_t *last_item,
			   bool is_inner,
			   struct rte_flow_error *error)
{
	const struct rte_flow_item_flex *flow_spec = item->spec;
	const struct rte_flow_item_flex *flow_mask = item->mask;
	struct mlx5_flex_item *flex;

	if (!flow_spec)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex flow item spec cannot be NULL");
	if (!flow_mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex flow item mask cannot be NULL");
	if (item->last)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "flex flow item last not supported");
	if (mlx5_flex_acquire_index(dev, flow_spec->handle, false) < 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "invalid flex flow item handle");
	flex = (struct mlx5_flex_item *)flow_spec->handle;
	switch (flex->tunnel_mode) {
	case FLEX_TUNNEL_MODE_SINGLE:
		if (item_flags &
		    (MLX5_FLOW_ITEM_OUTER_FLEX | MLX5_FLOW_ITEM_INNER_FLEX))
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "multiple flex items not supported");
		break;
	case FLEX_TUNNEL_MODE_OUTER:
		if (is_inner)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "inner flex item was not configured");
		if (item_flags & MLX5_FLOW_ITEM_OUTER_FLEX)
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "multiple flex items not supported");
		break;
	case FLEX_TUNNEL_MODE_INNER:
		if (!is_inner)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "outer flex item was not configured");
		if (item_flags & MLX5_FLOW_ITEM_INNER_FLEX)
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "multiple flex items not supported");
		break;
	case FLEX_TUNNEL_MODE_MULTI:
		if ((is_inner && (item_flags & MLX5_FLOW_ITEM_INNER_FLEX)) ||
		    (!is_inner && (item_flags & MLX5_FLOW_ITEM_OUTER_FLEX))) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "multiple flex items not supported");
		}
		break;
	case FLEX_TUNNEL_MODE_TUNNEL:
		if (is_inner || (item_flags & MLX5_FLOW_ITEM_FLEX_TUNNEL))
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   NULL, "multiple flex tunnel items not supported");
		break;
	default:
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   NULL, "invalid flex item configuration");
	}
	*last_item = flex->tunnel_mode == FLEX_TUNNEL_MODE_TUNNEL ?
		     MLX5_FLOW_ITEM_FLEX_TUNNEL : is_inner ?
		     MLX5_FLOW_ITEM_INNER_FLEX : MLX5_FLOW_ITEM_OUTER_FLEX;
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
	uint64_t aso_mask, action_flags = 0;
	uint64_t item_flags = 0;
	uint64_t last_item = 0;
	uint8_t next_protocol = 0xff;
	uint16_t ether_type = 0;
	int actions_n = 0;
	uint8_t item_ipv6_proto = 0;
	int fdb_mirror = 0;
	int modify_after_mirror = 0;
	const struct rte_flow_item *geneve_item = NULL;
	const struct rte_flow_item *gre_item = NULL;
	const struct rte_flow_item *gtp_item = NULL;
	const struct rte_flow_action_raw_decap *decap;
	const struct rte_flow_action_raw_encap *encap;
	const struct rte_flow_action_rss *rss = NULL;
	const struct rte_flow_action_rss *sample_rss = NULL;
	const struct rte_flow_action_count *sample_count = NULL;
	const struct rte_flow_item_tcp nic_tcp_mask = {
		.hdr = {
			.tcp_flags = 0xFF,
			.src_port = RTE_BE16(UINT16_MAX),
			.dst_port = RTE_BE16(UINT16_MAX),
		}
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
		.has_frag_ext = 1,
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
	struct mlx5_sh_config *dev_conf = &priv->sh->config;
	uint16_t queue_index = 0xFFFF;
	const struct rte_flow_item_vlan *vlan_m = NULL;
	uint32_t rw_act_num = 0;
	uint64_t is_root;
	const struct mlx5_flow_tunnel *tunnel;
	enum mlx5_tof_rule_type tof_rule_type;
	struct flow_grp_info grp_info = {
		.external = !!external,
		.transfer = !!attr->transfer,
		.fdb_def_rule = !!priv->fdb_def_rule,
		.std_tbl_fix = true,
	};
	const struct rte_eth_hairpin_conf *conf;
	const struct rte_flow_item *integrity_items[2] = {NULL, NULL};
	const struct rte_flow_item *port_id_item = NULL;
	bool def_policy = false;
	bool shared_count = false;
	uint16_t udp_dport = 0;
	uint32_t tag_id = 0;
	const struct rte_flow_action_age *non_shared_age = NULL;
	const struct rte_flow_action_count *count = NULL;
	struct mlx5_priv *act_priv = NULL;
	int aso_after_sample = 0;

	if (items == NULL)
		return -1;
	tunnel = is_tunnel_offload_active(dev) ?
		 mlx5_get_tof(items, actions, &tof_rule_type) : NULL;
	if (tunnel) {
		if (!dev_conf->dv_flow_en)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL, "tunnel offload requires DV flow interface");
		if (priv->representor)
			return rte_flow_error_set
				(error, ENOTSUP,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL, "decap not supported for VF representor");
		if (tof_rule_type == MLX5_TUNNEL_OFFLOAD_SET_RULE)
			action_flags |= MLX5_FLOW_ACTION_TUNNEL_SET;
		else if (tof_rule_type == MLX5_TUNNEL_OFFLOAD_MATCH_RULE)
			action_flags |= MLX5_FLOW_ACTION_TUNNEL_MATCH |
					MLX5_FLOW_ACTION_DECAP;
		grp_info.std_tbl_fix = tunnel_use_standard_attr_group_translate
					(dev, attr, tunnel, tof_rule_type);
	}
	ret = flow_dv_validate_attributes(dev, tunnel, attr, &grp_info, error);
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
		case RTE_FLOW_ITEM_TYPE_ESP:
			ret = mlx5_flow_os_validate_item_esp(items, item_flags,
							  next_protocol,
							  error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_ESP;
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			ret = flow_dv_validate_item_port_id
					(dev, items, attr, item_flags, &act_priv, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_PORT_ID;
			port_id_item = items;
			break;
		case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
			ret = flow_dv_validate_item_represented_port
					(dev, items, attr, item_flags, &act_priv, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_REPRESENTED_PORT;
			port_id_item = items;
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(items, item_flags,
							  true, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
					     MLX5_FLOW_LAYER_OUTER_L2;
			if (items->mask != NULL && items->spec != NULL) {
				ether_type =
					((const struct rte_flow_item_eth *)
					 items->spec)->hdr.ether_type;
				ether_type &=
					((const struct rte_flow_item_eth *)
					 items->mask)->hdr.ether_type;
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
					 items->spec)->hdr.eth_proto;
				ether_type &=
					((const struct rte_flow_item_vlan *)
					 items->mask)->hdr.eth_proto;
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
			ret = flow_dv_validate_item_ipv4(dev, items, item_flags,
							 last_item, ether_type,
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
		case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
			ret = flow_dv_validate_item_ipv6_frag_ext(items,
								  item_flags,
								  error);
			if (ret < 0)
				return ret;
			last_item = tunnel ?
					MLX5_FLOW_LAYER_INNER_L3_IPV6_FRAG_EXT :
					MLX5_FLOW_LAYER_OUTER_L3_IPV6_FRAG_EXT;
			if (items->mask != NULL &&
			    ((const struct rte_flow_item_ipv6_frag_ext *)
			     items->mask)->hdr.next_header) {
				next_protocol =
				((const struct rte_flow_item_ipv6_frag_ext *)
				 items->spec)->hdr.next_header;
				next_protocol &=
				((const struct rte_flow_item_ipv6_frag_ext *)
				 items->mask)->hdr.next_header;
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
			const struct rte_flow_item_udp *spec = items->spec;
			const struct rte_flow_item_udp *mask = items->mask;
			if (!mask)
				mask = &rte_flow_item_udp_mask;
			if (spec != NULL)
				udp_dport = rte_be_to_cpu_16
						(spec->hdr.dst_port &
						 mask->hdr.dst_port);
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
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
			ret = mlx5_flow_validate_item_gre_option(dev, items, item_flags,
							  attr, gre_item, error);
			if (ret < 0)
				return ret;
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
			ret = mlx5_flow_validate_item_vxlan(dev, udp_dport,
							    items, item_flags,
							    is_root, error);
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
			geneve_item = items;
			last_item = MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
			ret = mlx5_flow_validate_item_geneve_opt(items,
								 last_item,
								 geneve_item,
								 dev,
								 error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GENEVE_OPT;
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
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST:
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY:
			ret = mlx5_flow_validate_item_icmp6_echo(items,
								 item_flags,
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
		case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
			last_item = MLX5_FLOW_ITEM_SQ;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			ret = flow_dv_validate_item_gtp(dev, items, item_flags,
							error);
			if (ret < 0)
				return ret;
			gtp_item = items;
			last_item = MLX5_FLOW_LAYER_GTP;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			ret = flow_dv_validate_item_gtp_psc(items, last_item,
							    gtp_item, is_root,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GTP_PSC;
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
		case RTE_FLOW_ITEM_TYPE_INTEGRITY:
			ret = flow_dv_validate_item_integrity(dev, items,
							      item_flags,
							      &last_item,
							      integrity_items,
							      error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ITEM_TYPE_CONNTRACK:
			ret = flow_dv_validate_item_aso_ct(dev, items,
							   &item_flags, error);
			if (ret < 0)
				return ret;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_TUNNEL:
			/* tunnel offload item was processed before
			 * list it here as a supported type
			 */
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX:
			ret = flow_dv_validate_item_flex(dev, items, item_flags,
							 &last_item,
							 tunnel != 0, error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ITEM_TYPE_METER_COLOR:
			ret = flow_dv_validate_item_meter_color(dev, items,
								attr, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_METER_COLOR;
			break;
		case RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY:
			ret = flow_dv_validate_item_aggr_affinity(dev, items,
								  attr, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_AGGR_AFFINITY;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		}
		item_flags |= last_item;
	}
	if (item_flags & MLX5_FLOW_ITEM_INTEGRITY) {
		ret = flow_dv_validate_item_integrity_post(integrity_items,
							   item_flags, error);
		if (ret)
			return ret;
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
		if (action_flags &
			MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY)
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "meter action with policy "
				"must be the last action");
		switch (type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
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
				if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
					modify_after_mirror = 1;

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
				if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
					modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			tag_id = ((const struct rte_flow_action_set_tag *)
				  actions->conf)->index;
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
			if (rss && sample_rss &&
			    (sample_rss->level != rss->level ||
			    sample_rss->types != rss->types))
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"Can't use the different RSS types "
					"or level in the same flow");
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
		case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
			shared_count = true;
			/* fall-through. */
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_dv_validate_action_count(dev, shared_count,
							    action_flags,
							    is_root, error);
			if (ret < 0)
				return ret;
			count = actions->conf;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			action_flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			ret = flow_dv_validate_action_set_vlan_pcp
						(action_flags, actions, error);
			if (ret < 0)
				return ret;
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			/* Count PCP with push_vlan command. */
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_PCP;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			ret = flow_dv_validate_action_set_vlan_vid
						(item_flags, action_flags,
						 actions, error);
			if (ret < 0)
				return ret;
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
							    actions, item_flags,
							    attr, error);
			if (ret < 0)
				return ret;
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			ret = flow_dv_validate_action_raw_encap_decap
				(dev, NULL, actions->conf, attr, &action_flags,
				 &actions_n, actions, item_flags, error);
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
					    actions, item_flags, error);
			if (ret < 0)
				return ret;
			if ((action_flags & MLX5_FLOW_ACTION_SAMPLE) &&
			    (action_flags & MLX5_FLOW_ACTION_DECAP))
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			action_flags |= actions->type ==
					RTE_FLOW_ACTION_TYPE_SET_TTL ?
						MLX5_FLOW_ACTION_SET_TTL :
						MLX5_FLOW_ACTION_DEC_TTL;
			rw_act_num += MLX5_ACT_NUM_MDF_TTL;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			ret = flow_dv_validate_action_jump(dev, tunnel, actions,
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
							      item_flags,
							      actions, attr,
							      port_id_item,
							      &def_policy,
							      error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_METER;
			if (!def_policy)
				action_flags |=
				MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY;
			++actions_n;
			/* Meter action will add one more TAG action. */
			rw_act_num += MLX5_ACT_NUM_SET_TAG;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_AGE:
			if (is_root)
				return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
									   NULL,
			  "Shared ASO age action is not supported for group 0");
			if (action_flags & MLX5_FLOW_ACTION_AGE)
				return rte_flow_error_set
						  (error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   NULL,
						   "duplicate age actions set");
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				aso_after_sample = 1;
			action_flags |= MLX5_FLOW_ACTION_AGE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			non_shared_age = actions->conf;
			ret = flow_dv_validate_action_age(action_flags,
							  actions, dev,
							  error);
			if (ret < 0)
				return ret;
			/*
			 * Validate the regular AGE action (using counter)
			 * mutual exclusion with indirect counter actions.
			 */
			if (!flow_hit_aso_supported(priv, is_root)) {
				if (shared_count)
					return rte_flow_error_set
						(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"old age and indirect count combination is not supported");
				if (sample_count)
					return rte_flow_error_set
						(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"old age action and count must be in the same sub flow");
			} else {
				if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
					aso_after_sample = 1;
			}
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			action_flags |= MLX5_FLOW_ACTION_SET_IPV6_DSCP;
			rw_act_num += MLX5_ACT_NUM_SET_DSCP;
			break;
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
			ret = flow_dv_validate_action_sample(&action_flags,
							     actions, dev,
							     attr, item_flags,
							     rss, &sample_rss,
							     &sample_count,
							     &fdb_mirror,
							     is_root,
							     error);
			if (ret < 0)
				return ret;
			if ((action_flags & MLX5_FLOW_ACTION_SET_TAG) &&
			    tag_id == 0 && priv->mtr_color_reg == REG_NON)
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"sample after tag action causes metadata tag index 0 corruption");
			action_flags |= MLX5_FLOW_ACTION_SAMPLE;
			++actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			ret = flow_dv_validate_action_modify_field(dev,
								   action_flags,
								   actions,
								   attr,
								   is_root,
								   error);
			if (ret < 0)
				return ret;
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				modify_after_mirror = 1;
			/* Count all modify-header actions as one action. */
			if (!(action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS))
				++actions_n;
			action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
			rw_act_num += ret;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			ret = flow_dv_validate_action_aso_ct(dev, action_flags,
							     item_flags,
							     is_root, error);
			if (ret < 0)
				return ret;
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				aso_after_sample = 1;
			action_flags |= MLX5_FLOW_ACTION_CT;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_TUNNEL_SET:
			/* tunnel offload action was processed before
			 * list it here as a supported type
			 */
			break;
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			action_flags |= MLX5_FLOW_ACTION_SEND_TO_KERNEL;
			++actions_n;
			break;
#endif
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
	}
	/*
	 * Validate actions in flow rules
	 * - Explicit decap action is prohibited by the tunnel offload API.
	 * - Drop action in tunnel steer rule is prohibited by the API.
	 * - Application cannot use MARK action because it's value can mask
	 *   tunnel default miss notification.
	 * - JUMP in tunnel match rule has no support in current PMD
	 *   implementation.
	 * - TAG & META are reserved for future uses.
	 */
	if (action_flags & MLX5_FLOW_ACTION_TUNNEL_SET) {
		uint64_t bad_actions_mask = MLX5_FLOW_ACTION_DECAP    |
					    MLX5_FLOW_ACTION_MARK     |
					    MLX5_FLOW_ACTION_SET_TAG  |
					    MLX5_FLOW_ACTION_SET_META |
					    MLX5_FLOW_ACTION_DROP;

		if (action_flags & bad_actions_mask)
			return rte_flow_error_set
					(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Invalid RTE action in tunnel "
					"set decap rule");
		if (!(action_flags & MLX5_FLOW_ACTION_JUMP))
			return rte_flow_error_set
					(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"tunnel set decap rule must terminate "
					"with JUMP");
		if (attr->egress)
			return rte_flow_error_set
					(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"tunnel flows for ingress and transfer traffic only");
	}
	if (action_flags & MLX5_FLOW_ACTION_TUNNEL_MATCH) {
		uint64_t bad_actions_mask = MLX5_FLOW_ACTION_JUMP    |
					    MLX5_FLOW_ACTION_MARK    |
					    MLX5_FLOW_ACTION_SET_TAG |
					    MLX5_FLOW_ACTION_SET_META;

		if (action_flags & bad_actions_mask)
			return rte_flow_error_set
					(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Invalid RTE action in tunnel "
					"set match rule");
	}
	/*
	 * Validate the drop action mutual exclusion with other actions.
	 * Drop action is mutually-exclusive with any other action, except for
	 * Count/Sample/Age actions.
	 * Drop action compatibility with tunnel offload was already validated.
	 */
	if (action_flags & (MLX5_FLOW_ACTION_TUNNEL_MATCH |
			    MLX5_FLOW_ACTION_TUNNEL_MATCH));
	else if ((action_flags & MLX5_FLOW_ACTION_DROP) &&
	    (action_flags & ~(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_DROP_INCLUSIVE_ACTIONS)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Drop action is mutually-exclusive "
					  "with any other action, except for "
					  "Count/Sample/Age action");
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
	/*
	 * Continue validation for Xcap and VLAN actions.
	 * If hairpin is working in explicit TX rule mode, there is no actions
	 * splitting and the validation of hairpin ingress flow should be the
	 * same as other standard flows.
	 */
	if ((action_flags & (MLX5_FLOW_XCAP_ACTIONS |
			     MLX5_FLOW_VLAN_ACTIONS)) &&
	    (queue_index == 0xFFFF || !mlx5_rxq_is_hairpin(dev, queue_index) ||
	     ((conf = mlx5_rxq_get_hairpin_conf(dev, queue_index)) != NULL &&
	     conf->tx_explicit != 0))) {
		if ((action_flags & MLX5_FLOW_XCAP_ACTIONS) ==
		    MLX5_FLOW_XCAP_ACTIONS)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "encap and decap "
						  "combination aren't supported");
		/* Push VLAN is not supported in ingress except for NICs newer than CX5. */
		if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN) {
			struct mlx5_dev_ctx_shared *sh = priv->sh;
			bool direction_error = false;

			if (attr->transfer) {
				bool fdb_tx = flow_source_vport_representor(priv, act_priv);
				bool is_cx5 = sh->steering_format_version ==
				    MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5;

				if (!fdb_tx && is_cx5)
					direction_error = true;
			} else if (attr->ingress) {
				direction_error = true;
			}
			if (direction_error)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
							  NULL,
							  "push VLAN action not supported "
							  "for ingress");
		}
		if (attr->ingress) {
			if (action_flags & MLX5_FLOW_ACTION_ENCAP)
				return rte_flow_error_set
						(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL, "encap is not supported"
						 " for ingress traffic");
			else if ((action_flags & MLX5_FLOW_VLAN_ACTIONS) ==
					MLX5_FLOW_VLAN_ACTIONS)
				return rte_flow_error_set
						(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL, "no support for "
						 "multiple VLAN actions");
		}
	}
	/* Pop VLAN is not supported in egress except for NICs newer than CX5. */
	if (action_flags & MLX5_FLOW_ACTION_OF_POP_VLAN) {
		struct mlx5_dev_ctx_shared *sh = priv->sh;
		bool direction_error = false;

		if (attr->transfer) {
			bool fdb_tx = flow_source_vport_representor(priv, act_priv);
			bool is_cx5 = sh->steering_format_version ==
					MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5;

			if (fdb_tx && is_cx5)
				direction_error = true;
		} else if (attr->egress) {
			direction_error = true;
		}
		if (direction_error)
			return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
						NULL,
						"pop vlan action not supported for egress");
	}
	if (action_flags & MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY) {
		if ((action_flags & (MLX5_FLOW_FATE_ACTIONS &
			~MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY)) &&
			attr->ingress)
			return rte_flow_error_set
				(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ACTION,
				NULL, "fate action not supported for "
				"meter with policy");
		if (attr->egress) {
			if (action_flags & MLX5_FLOW_MODIFY_HDR_ACTIONS)
				return rte_flow_error_set
					(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "modify header action in egress "
					"cannot be done before meter action");
			if (action_flags & MLX5_FLOW_ACTION_ENCAP)
				return rte_flow_error_set
					(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "encap action in egress "
					"cannot be done before meter action");
			if (action_flags & MLX5_FLOW_ACTION_OF_PUSH_VLAN)
				return rte_flow_error_set
					(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "push vlan action in egress "
					"cannot be done before meter action");
		}
	}
	/*
	 * Only support one ASO action in a single flow rule.
	 * non-shared AGE + counter will fallback to use HW counter, no ASO hit object.
	 * Group 0 uses HW counter for AGE too even if no counter action.
	 */
	aso_mask = (action_flags & MLX5_FLOW_ACTION_METER && priv->sh->meter_aso_en) << 2 |
		   (action_flags & MLX5_FLOW_ACTION_CT && priv->sh->ct_aso_en) << 1 |
		   (action_flags & MLX5_FLOW_ACTION_AGE &&
		    !(non_shared_age && count) &&
		    (attr->group || (attr->transfer && priv->fdb_def_rule)) &&
		    priv->sh->flow_hit_aso_en);
	if (__builtin_popcountl(aso_mask) > 1)
		return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "unsupported combining AGE, METER, CT ASO actions in a single rule");
	/*
	 * Hairpin flow will add one more TAG action in TX implicit mode.
	 * In TX explicit mode, there will be no hairpin flow ID.
	 */
	if (hairpin > 0)
		rw_act_num += MLX5_ACT_NUM_SET_TAG;
	/* extra metadata enabled: one more TAG action will be add. */
	if (dev_conf->dv_flow_en &&
	    dev_conf->dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
	    mlx5_flow_ext_mreg_supported(dev))
		rw_act_num += MLX5_ACT_NUM_SET_TAG;
	if (rw_act_num >
			flow_dv_modify_hdr_action_max(dev, is_root)) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "too many header modify"
					  " actions to support");
	}
	if (fdb_mirror) {
		if (!priv->sh->cdev->config.hca_attr.reg_c_preserve &&
		    flow_source_vport_representor(priv, act_priv)) {
			/* Eswitch egress mirror and modify flow has limitation on CX5 */
			if (modify_after_mirror)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						"sample before modify action is not supported");
			if (action_flags & MLX5_FLOW_ACTION_JUMP)
				return rte_flow_error_set(error, EINVAL,
							RTE_FLOW_ERROR_TYPE_ACTION, NULL,
							"sample and jump action combination is not supported");
		}
		if (aso_mask > 0 && aso_after_sample && fdb_mirror)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						  "sample before ASO action is not supported");
	}
	/*
	 * Validation the NIC Egress flow on representor, except implicit
	 * hairpin default egress flow with TX_QUEUE item, other flows not
	 * work due to metadata regC0 mismatch.
	 */
	if (attr->egress && priv->representor && !(item_flags & MLX5_FLOW_ITEM_SQ))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  NULL,
					  "NIC egress rules on representors"
					  " is not supported");
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
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	MLX5_ASSERT(wks);
	wks->skip_matcher_reg = 0;
	wks->policy = NULL;
	wks->final_policy = NULL;
	wks->vport_meta_tag = 0;
	/* In case of corrupting the memory. */
	if (wks->flow_idx >= MLX5_NUM_MAX_DEV_FLOWS) {
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
	MLX5_ASSERT(wks->flow_idx < RTE_DIM(wks->flows));
	dev_flow = &wks->flows[wks->flow_idx++];
	memset(dev_flow, 0, sizeof(*dev_flow));
	dev_flow->handle = dev_handle;
	dev_flow->handle_idx = handle_idx;
	dev_flow->dv.value.size = MLX5_ST_SZ_BYTES(fte_match_param);
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
			     uint32_t key_type,
			     uint8_t ip_version)
{
	if (group == 0 && (key_type & MLX5_SET_MATCHER_M))
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version, 0xf);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_version,
			 ip_version);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ethertype, 0);
}

/**
 * Add Ethernet item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] grpup
 *   Flow matcher group.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_eth(void *key, const struct rte_flow_item *item,
			   int inner, uint32_t group, uint32_t key_type)
{
	const struct rte_flow_item_eth *eth_vv = item->spec;
	const struct rte_flow_item_eth *eth_m;
	const struct rte_flow_item_eth *eth_v;
	const struct rte_flow_item_eth nic_mask = {
		.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.hdr.src_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.hdr.ether_type = RTE_BE16(0xffff),
		.has_vlan = 0,
	};
	void *hdrs_v;
	char *l24_v;
	unsigned int i;

	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, eth_v, eth_m, &nic_mask);
	if (!eth_vv)
		eth_vv = eth_v;
	if (inner)
		hdrs_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	else
		hdrs_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	/* The value must be in the range of the mask. */
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, hdrs_v, dmac_47_16);
	for (i = 0; i < sizeof(eth_m->hdr.dst_addr); ++i)
		l24_v[i] = eth_m->hdr.dst_addr.addr_bytes[i] & eth_v->hdr.dst_addr.addr_bytes[i];
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, hdrs_v, smac_47_16);
	/* The value must be in the range of the mask. */
	for (i = 0; i < sizeof(eth_m->hdr.dst_addr); ++i)
		l24_v[i] = eth_m->hdr.src_addr.addr_bytes[i] & eth_v->hdr.src_addr.addr_bytes[i];
	/*
	 * HW supports match on one Ethertype, the Ethertype following the last
	 * VLAN tag of the packet (see PRM).
	 * Set match on ethertype only if ETH header is not followed by VLAN.
	 * HW is optimized for IPv4/IPv6. In such cases, avoid setting
	 * ethertype, and use ip_version field instead.
	 * eCPRI over Ether layer will use type value 0xAEFE.
	 */
	if (eth_m->hdr.ether_type == 0xFFFF) {
		rte_be16_t type = eth_v->hdr.ether_type;

		/*
		 * When set the matcher mask, refer to the original spec
		 * value.
		 */
		if (key_type == MLX5_SET_MATCHER_SW_M) {
			MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, cvlan_tag, 1);
			type = eth_vv->hdr.ether_type;
		}
		/* Set cvlan_tag mask for any single\multi\un-tagged case. */
		switch (type) {
		case RTE_BE16(RTE_ETHER_TYPE_VLAN):
			MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, cvlan_tag, 1);
			return;
		case RTE_BE16(RTE_ETHER_TYPE_QINQ):
			MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, svlan_tag, 1);
			return;
		case RTE_BE16(RTE_ETHER_TYPE_IPV4):
			flow_dv_set_match_ip_version(group, hdrs_v, key_type,
						     4);
			return;
		case RTE_BE16(RTE_ETHER_TYPE_IPV6):
			flow_dv_set_match_ip_version(group, hdrs_v, key_type,
						     6);
			return;
		default:
			break;
		}
	}
	/*
	 * Only SW steering value should refer to the mask value.
	 * Other cases are using the fake masks, just ignore the mask.
	 */
	if (eth_v->has_vlan && eth_m->has_vlan) {
		/*
		 * Here, when also has_more_vlan field in VLAN item is
		 * not set, only single-tagged packets will be matched.
		 */
		MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, cvlan_tag, 1);
		if (key_type != MLX5_SET_MATCHER_HS_M && eth_vv->has_vlan)
			return;
	}
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, hdrs_v, ethertype);
	*(uint16_t *)(l24_v) = eth_m->hdr.ether_type & eth_v->hdr.ether_type;
}

/**
 * Add VLAN item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] wks
 *   Item workspace.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_vlan(void *key, const struct rte_flow_item *item,
			    int inner, struct mlx5_dv_matcher_workspace *wks,
			    uint32_t key_type)
{
	const struct rte_flow_item_vlan *vlan_m;
	const struct rte_flow_item_vlan *vlan_v;
	const struct rte_flow_item_vlan *vlan_vv = item->spec;
	void *hdrs_v;
	uint16_t tci_v;

	if (inner) {
		hdrs_v = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
	} else {
		hdrs_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
		/*
		 * This is workaround, masks are not supported,
		 * and pre-validated.
		 */
		if (vlan_vv)
			wks->vlan_tag = rte_be_to_cpu_16(vlan_vv->hdr.vlan_tci) & 0x0fff;
	}
	/*
	 * When VLAN item exists in flow, mark packet as tagged,
	 * even if TCI is not specified.
	 */
	if (!MLX5_GET(fte_match_set_lyr_2_4, hdrs_v, svlan_tag))
		MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, cvlan_tag, 1);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, vlan_v, vlan_m,
			 &rte_flow_item_vlan_mask);
	tci_v = rte_be_to_cpu_16(vlan_m->hdr.vlan_tci & vlan_v->hdr.vlan_tci);
	MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, first_vid, tci_v);
	MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, first_cfi, tci_v >> 12);
	MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, first_prio, tci_v >> 13);
	/*
	 * HW is optimized for IPv4/IPv6. In such cases, avoid setting
	 * ethertype, and use ip_version field instead.
	 */
	if (vlan_m->hdr.eth_proto == 0xFFFF) {
		rte_be16_t inner_type = vlan_v->hdr.eth_proto;

		/*
		 * When set the matcher mask, refer to the original spec
		 * value.
		 */
		if (key_type == MLX5_SET_MATCHER_SW_M)
			inner_type = vlan_vv->hdr.eth_proto;
		switch (inner_type) {
		case RTE_BE16(RTE_ETHER_TYPE_VLAN):
			MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, svlan_tag, 1);
			if (key_type & MLX5_SET_MATCHER_V)
				MLX5_SET(fte_match_set_lyr_2_4, hdrs_v,
					 cvlan_tag, 0);
			return;
		case RTE_BE16(RTE_ETHER_TYPE_IPV4):
			flow_dv_set_match_ip_version
				(wks->group, hdrs_v, key_type, 4);
			return;
		case RTE_BE16(RTE_ETHER_TYPE_IPV6):
			flow_dv_set_match_ip_version
				(wks->group, hdrs_v, key_type, 6);
			return;
		default:
			break;
		}
	}
	if (vlan_m->has_more_vlan && vlan_v->has_more_vlan) {
		MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, svlan_tag, 1);
		/* Only one vlan_tag bit can be set. */
		if (key_type & MLX5_SET_MATCHER_V)
			MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, cvlan_tag, 0);
		return;
	}
	MLX5_SET(fte_match_set_lyr_2_4, hdrs_v, ethertype,
		 rte_be_to_cpu_16(vlan_m->hdr.eth_proto & vlan_v->hdr.eth_proto));
}

/**
 * Add IPV4 item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] group
 *   The group to insert the rule.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_ipv4(void *key, const struct rte_flow_item *item,
			    int inner, uint32_t group, uint32_t key_type)
{
	const struct rte_flow_item_ipv4 *ipv4_m;
	const struct rte_flow_item_ipv4 *ipv4_v;
	const struct rte_flow_item_ipv4 nic_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.next_proto_id = 0xff,
			.time_to_live = 0xff,
		},
	};
	void *headers_v;
	char *l24_v;
	uint8_t tos;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
			    MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	flow_dv_set_match_ip_version(group, headers_v, key_type, 4);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, ipv4_v, ipv4_m, &nic_mask);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
	*(uint32_t *)l24_v = ipv4_m->hdr.dst_addr & ipv4_v->hdr.dst_addr;
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			  src_ipv4_src_ipv6.ipv4_layout.ipv4);
	*(uint32_t *)l24_v = ipv4_m->hdr.src_addr & ipv4_v->hdr.src_addr;
	tos = ipv4_m->hdr.type_of_service & ipv4_v->hdr.type_of_service;
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ipv4_ihl,
		 ipv4_v->hdr.ihl & ipv4_m->hdr.ihl);
	if (key_type == MLX5_SET_MATCHER_SW_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ecn,
			 ipv4_v->hdr.type_of_service);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ecn, tos);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_dscp, tos >> 2);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
		 ipv4_v->hdr.next_proto_id & ipv4_m->hdr.next_proto_id);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ttl_hoplimit,
		 ipv4_v->hdr.time_to_live & ipv4_m->hdr.time_to_live);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, frag,
		 !!(ipv4_v->hdr.fragment_offset & ipv4_m->hdr.fragment_offset));
}

/**
 * Add IPV6 item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] group
 *   The group to insert the rule.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_ipv6(void *key, const struct rte_flow_item *item,
			    int inner, uint32_t group, uint32_t key_type)
{
	const struct rte_flow_item_ipv6 *ipv6_m;
	const struct rte_flow_item_ipv6 *ipv6_v;
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
	void *headers_v;
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	char *l24_v;
	uint32_t vtc_v;
	int i;
	int size;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
			    MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	flow_dv_set_match_ip_version(group, headers_v, key_type, 6);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, ipv6_v, ipv6_m, &nic_mask);
	size = sizeof(ipv6_m->hdr.dst_addr);
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     dst_ipv4_dst_ipv6.ipv6_layout.ipv6);
	for (i = 0; i < size; ++i)
		l24_v[i] = ipv6_m->hdr.dst_addr[i] & ipv6_v->hdr.dst_addr[i];
	l24_v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, headers_v,
			     src_ipv4_src_ipv6.ipv6_layout.ipv6);
	for (i = 0; i < size; ++i)
		l24_v[i] = ipv6_m->hdr.src_addr[i] & ipv6_v->hdr.src_addr[i];
	/* TOS. */
	vtc_v = rte_be_to_cpu_32(ipv6_m->hdr.vtc_flow & ipv6_v->hdr.vtc_flow);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ecn, vtc_v >> 20);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_dscp, vtc_v >> 22);
	/* Label. */
	if (inner)
		MLX5_SET(fte_match_set_misc, misc_v, inner_ipv6_flow_label,
			 vtc_v);
	else
		MLX5_SET(fte_match_set_misc, misc_v, outer_ipv6_flow_label,
			 vtc_v);
	/* Protocol. */
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
		 ipv6_v->hdr.proto & ipv6_m->hdr.proto);
	/* Hop limit. */
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_ttl_hoplimit,
		 ipv6_v->hdr.hop_limits & ipv6_m->hdr.hop_limits);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, frag,
		 !!(ipv6_v->has_frag_ext & ipv6_m->has_frag_ext));
}

/**
 * Add IPV6 fragment extension item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_ipv6_frag_ext(void *key,
				     const struct rte_flow_item *item,
				     int inner, uint32_t key_type)
{
	const struct rte_flow_item_ipv6_frag_ext *ipv6_frag_ext_m;
	const struct rte_flow_item_ipv6_frag_ext *ipv6_frag_ext_v;
	const struct rte_flow_item_ipv6_frag_ext nic_mask = {
		.hdr = {
			.next_header = 0xff,
			.frag_data = RTE_BE16(0xffff),
		},
	};
	void *headers_v;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
			    MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	/* IPv6 fragment extension item exists, so packet is IP fragment. */
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, frag, 1);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, ipv6_frag_ext_v,
			 ipv6_frag_ext_m, &nic_mask);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
		 ipv6_frag_ext_v->hdr.next_header &
		 ipv6_frag_ext_m->hdr.next_header);
}

/**
 * Add TCP item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_tcp(void *key, const struct rte_flow_item *item,
			   int inner, uint32_t key_type)
{
	const struct rte_flow_item_tcp *tcp_m;
	const struct rte_flow_item_tcp *tcp_v;
	void *headers_v;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, 0xff);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, IPPROTO_TCP);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, tcp_v, tcp_m,
			 &rte_flow_item_tcp_mask);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_sport,
		 rte_be_to_cpu_16(tcp_v->hdr.src_port & tcp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_dport,
		 rte_be_to_cpu_16(tcp_v->hdr.dst_port & tcp_m->hdr.dst_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, tcp_flags,
		 tcp_v->hdr.tcp_flags & tcp_m->hdr.tcp_flags);
}

/**
 * Add ESP item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_esp(void *key, const struct rte_flow_item *item,
			   int inner, uint32_t key_type)
{
	const struct rte_flow_item_esp *esp_m;
	const struct rte_flow_item_esp *esp_v;
	void *headers_v;
	char *spi_v;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, 0xff);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, IPPROTO_ESP);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, esp_v, esp_m,
			 &rte_flow_item_esp_mask);
	headers_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	spi_v = inner ? MLX5_ADDR_OF(fte_match_set_misc, headers_v,
				inner_esp_spi) : MLX5_ADDR_OF(fte_match_set_misc
				, headers_v, outer_esp_spi);
	*(uint32_t *)spi_v = esp_m->hdr.spi & esp_v->hdr.spi;
}

/**
 * Add UDP item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_udp(void *key, const struct rte_flow_item *item,
			   int inner, struct mlx5_dv_matcher_workspace *wks,
			   uint32_t key_type)
{
	const struct rte_flow_item_udp *udp_m;
	const struct rte_flow_item_udp *udp_v;
	void *headers_v;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, 0xff);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, IPPROTO_UDP);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, udp_v, udp_m,
			 &rte_flow_item_udp_mask);
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_sport,
		 rte_be_to_cpu_16(udp_v->hdr.src_port & udp_m->hdr.src_port));
	MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
		 rte_be_to_cpu_16(udp_v->hdr.dst_port & udp_m->hdr.dst_port));
	/* Force get UDP dport in case to be used in VXLAN translate. */
	if (key_type & MLX5_SET_MATCHER_SW) {
		udp_v = item->spec;
		wks->udp_dport = rte_be_to_cpu_16(udp_v->hdr.dst_port &
						  udp_m->hdr.dst_port);
	}
}

/**
 * Add GRE optional Key item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 */
static void
flow_dv_translate_item_gre_key(void *key, const struct rte_flow_item *item,
			       uint32_t key_type)
{
	const rte_be32_t *key_m;
	const rte_be32_t *key_v;
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	rte_be32_t gre_key_default_mask = RTE_BE32(UINT32_MAX);

	/* GRE K bit must be on and should already be validated */
	MLX5_SET(fte_match_set_misc, misc_v, gre_k_present, 1);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, key_v, key_m,
			 &gre_key_default_mask);
	MLX5_SET(fte_match_set_misc, misc_v, gre_key_h,
		 rte_be_to_cpu_32((*key_v) & (*key_m)) >> 8);
	MLX5_SET(fte_match_set_misc, misc_v, gre_key_l,
		 rte_be_to_cpu_32((*key_v) & (*key_m)) & 0xFF);
}

/**
 * Add GRE item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] pattern_flags
 *   Accumulated pattern flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_gre(void *key, const struct rte_flow_item *item,
			   uint64_t pattern_flags, uint32_t key_type)
{
	static const struct rte_flow_item_gre empty_gre = {0,};
	const struct rte_flow_item_gre *gre_m = item->mask;
	const struct rte_flow_item_gre *gre_v = item->spec;
	void *headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
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
	uint16_t protocol_m, protocol_v;

	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, 0xff);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
			 IPPROTO_GRE);
	if (!gre_v) {
		gre_v = &empty_gre;
		gre_m = &empty_gre;
	} else {
		if (!gre_m)
			gre_m = &rte_flow_item_gre_mask;
	}
	if (key_type & MLX5_SET_MATCHER_M)
		gre_v = gre_m;
	else if (key_type == MLX5_SET_MATCHER_HS_V)
		gre_m = gre_v;
	gre_crks_rsvd0_ver_m.value = rte_be_to_cpu_16(gre_m->c_rsvd0_ver);
	gre_crks_rsvd0_ver_v.value = rte_be_to_cpu_16(gre_v->c_rsvd0_ver);
	MLX5_SET(fte_match_set_misc, misc_v, gre_c_present,
		 gre_crks_rsvd0_ver_v.c_present &
		 gre_crks_rsvd0_ver_m.c_present);
	MLX5_SET(fte_match_set_misc, misc_v, gre_k_present,
		 gre_crks_rsvd0_ver_v.k_present &
		 gre_crks_rsvd0_ver_m.k_present);
	MLX5_SET(fte_match_set_misc, misc_v, gre_s_present,
		 gre_crks_rsvd0_ver_v.s_present &
		 gre_crks_rsvd0_ver_m.s_present);
	protocol_m = rte_be_to_cpu_16(gre_m->protocol);
	protocol_v = rte_be_to_cpu_16(gre_v->protocol);
	if (!protocol_m) {
		/* Force next protocol to prevent matchers duplication */
		protocol_v = mlx5_translate_tunnel_etypes(pattern_flags);
		if (protocol_v)
			protocol_m = 0xFFFF;
		/* Restore the value to mask in mask case. */
		if (key_type & MLX5_SET_MATCHER_M)
			protocol_v = protocol_m;
	}
	MLX5_SET(fte_match_set_misc, misc_v, gre_protocol,
		 protocol_m & protocol_v);
}

/**
 * Add GRE optional items to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] gre_item
 *   Pointer to gre_item.
 * @param[in] pattern_flags
 *   Accumulated pattern flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_gre_option(void *key,
				  const struct rte_flow_item *item,
				  const struct rte_flow_item *gre_item,
				  uint64_t pattern_flags, uint32_t key_type)
{
	void *misc5_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_5);
	const struct rte_flow_item_gre_opt *option_m = item->mask;
	const struct rte_flow_item_gre_opt *option_v = item->spec;
	const struct rte_flow_item_gre *gre_m = gre_item->mask;
	const struct rte_flow_item_gre *gre_v = gre_item->spec;
	static const struct rte_flow_item_gre empty_gre = {0};
	struct rte_flow_item gre_key_item;
	uint16_t c_rsvd0_ver_m, c_rsvd0_ver_v;
	uint16_t protocol_m, protocol_v;

	/*
	 * If only match key field, keep using misc for matching.
	 * If need to match checksum or sequence, using misc5 and do
	 * not need using misc.
	 */
	if (!(option_m->sequence.sequence ||
	      option_m->checksum_rsvd.checksum)) {
		flow_dv_translate_item_gre(key, gre_item, pattern_flags, key_type);
		gre_key_item.spec = &option_v->key.key;
		gre_key_item.mask = &option_m->key.key;
		flow_dv_translate_item_gre_key(key, &gre_key_item, key_type);
		return;
	}
	if (!gre_v) {
		gre_v = &empty_gre;
		gre_m = &empty_gre;
	} else {
		if (!gre_m)
			gre_m = &rte_flow_item_gre_mask;
	}
	protocol_v = gre_v->protocol;
	protocol_m = gre_m->protocol;
	if (!protocol_m) {
		/* Force next protocol to prevent matchers duplication */
		uint16_t ether_type =
			mlx5_translate_tunnel_etypes(pattern_flags);
		if (ether_type) {
			protocol_v = rte_be_to_cpu_16(ether_type);
			protocol_m = UINT16_MAX;
		}
	}
	c_rsvd0_ver_v = gre_v->c_rsvd0_ver;
	c_rsvd0_ver_m = gre_m->c_rsvd0_ver;
	if (option_m->sequence.sequence) {
		c_rsvd0_ver_v |= RTE_BE16(0x1000);
		c_rsvd0_ver_m |= RTE_BE16(0x1000);
	}
	if (option_m->key.key) {
		c_rsvd0_ver_v |= RTE_BE16(0x2000);
		c_rsvd0_ver_m |= RTE_BE16(0x2000);
	}
	if (option_m->checksum_rsvd.checksum) {
		c_rsvd0_ver_v |= RTE_BE16(0x8000);
		c_rsvd0_ver_m |= RTE_BE16(0x8000);
	}
	if (key_type & MLX5_SET_MATCHER_M) {
		c_rsvd0_ver_v = c_rsvd0_ver_m;
		protocol_v = protocol_m;
		option_v = option_m;
	}
	/*
	 * Hardware parses GRE optional field into the fixed location,
	 * do not need to adjust the tunnel dword indices.
	 */
	MLX5_SET(fte_match_set_misc5, misc5_v, tunnel_header_0,
		 rte_be_to_cpu_32((c_rsvd0_ver_v | protocol_v << 16) &
				  (c_rsvd0_ver_m | protocol_m << 16)));
	MLX5_SET(fte_match_set_misc5, misc5_v, tunnel_header_1,
		 rte_be_to_cpu_32(option_v->checksum_rsvd.checksum &
				  option_m->checksum_rsvd.checksum));
	MLX5_SET(fte_match_set_misc5, misc5_v, tunnel_header_2,
		 rte_be_to_cpu_32(option_v->key.key & option_m->key.key));
	MLX5_SET(fte_match_set_misc5, misc5_v, tunnel_header_3,
		 rte_be_to_cpu_32(option_v->sequence.sequence &
				  option_m->sequence.sequence));
}

/**
 * Add NVGRE item to matcher and to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] pattern_flags
 *   Accumulated pattern flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_nvgre(void *key, const struct rte_flow_item *item,
			     unsigned long pattern_flags, uint32_t key_type)
{
	const struct rte_flow_item_nvgre *nvgre_m;
	const struct rte_flow_item_nvgre *nvgre_v;
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	const char *tni_flow_id_m;
	const char *tni_flow_id_v;
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
	flow_dv_translate_item_gre(key, &gre_item, pattern_flags, key_type);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, nvgre_v, nvgre_m,
		    &rte_flow_item_nvgre_mask);
	tni_flow_id_m = (const char *)nvgre_m->tni;
	tni_flow_id_v = (const char *)nvgre_v->tni;
	size = sizeof(nvgre_m->tni) + sizeof(nvgre_m->flow_id);
	gre_key_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, gre_key_h);
	for (i = 0; i < size; ++i)
		gre_key_v[i] = tni_flow_id_m[i] & tni_flow_id_v[i];
}

/**
 * Add VXLAN item to the value.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] wks
 *   Matcher workspace.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_vxlan(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     void *key, const struct rte_flow_item *item,
			     int inner, struct mlx5_dv_matcher_workspace *wks,
			     uint32_t key_type)
{
	const struct rte_flow_item_vxlan *vxlan_m;
	const struct rte_flow_item_vxlan *vxlan_v;
	const struct rte_flow_item_vxlan *vxlan_vv = item->spec;
	void *headers_v;
	void *misc_v;
	void *misc5_v;
	uint32_t tunnel_v;
	uint32_t *tunnel_header_v;
	char *vni_v;
	uint16_t dport;
	int size;
	int i;
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_vxlan nic_mask = {
		.hdr.vni = "\xff\xff\xff",
		.hdr.rsvd1 = 0xff,
	};

	misc5_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_5);
	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	dport = item->type == RTE_FLOW_ITEM_TYPE_VXLAN ?
		MLX5_UDP_PORT_VXLAN : MLX5_UDP_PORT_VXLAN_GPE;
	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		if (key_type & MLX5_SET_MATCHER_M)
			MLX5_SET(fte_match_set_lyr_2_4, headers_v,
				 udp_dport, 0xFFFF);
		else
			MLX5_SET(fte_match_set_lyr_2_4, headers_v,
				 udp_dport, dport);
	}
	/*
	 * Read the UDP dport to check if the value satisfies the VXLAN
	 * matching with MISC5 for CX5.
	 */
	if (wks->udp_dport)
		dport = wks->udp_dport;
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, vxlan_v, vxlan_m, &nic_mask);
	if ((item->mask == &nic_mask) &&
	    ((!attr->group && !(attr->transfer && priv->fdb_def_rule) &&
	    !priv->sh->tunnel_header_0_1) ||
	    ((attr->group || (attr->transfer && priv->fdb_def_rule)) &&
	    !priv->sh->misc5_cap)))
		vxlan_m = &rte_flow_item_vxlan_mask;
	if ((priv->sh->steering_format_version ==
	     MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5 &&
	     dport != MLX5_UDP_PORT_VXLAN) ||
	    (!attr->group && !(attr->transfer && priv->fdb_def_rule)) ||
	    ((attr->group || (attr->transfer && priv->fdb_def_rule)) &&
	    !priv->sh->misc5_cap)) {
		misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
		size = sizeof(vxlan_m->hdr.vni);
		vni_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, vxlan_vni);
		for (i = 0; i < size; ++i)
			vni_v[i] = vxlan_m->hdr.vni[i] & vxlan_v->hdr.vni[i];
		return;
	}
	tunnel_header_v = (uint32_t *)MLX5_ADDR_OF(fte_match_set_misc5,
						   misc5_v,
						   tunnel_header_1);
	tunnel_v = (vxlan_v->hdr.vni[0] & vxlan_m->hdr.vni[0]) |
		   (vxlan_v->hdr.vni[1] & vxlan_m->hdr.vni[1]) << 8 |
		   (vxlan_v->hdr.vni[2] & vxlan_m->hdr.vni[2]) << 16;
	*tunnel_header_v = tunnel_v;
	if (key_type == MLX5_SET_MATCHER_SW_M) {
		tunnel_v = (vxlan_vv->hdr.vni[0] & vxlan_m->hdr.vni[0]) |
			   (vxlan_vv->hdr.vni[1] & vxlan_m->hdr.vni[1]) << 8 |
			   (vxlan_vv->hdr.vni[2] & vxlan_m->hdr.vni[2]) << 16;
		if (!tunnel_v)
			*tunnel_header_v = 0x0;
		if (vxlan_vv->hdr.rsvd1 & vxlan_m->hdr.rsvd1)
			*tunnel_header_v |= vxlan_v->hdr.rsvd1 << 24;
	} else {
		*tunnel_header_v |= (vxlan_v->hdr.rsvd1 & vxlan_m->hdr.rsvd1) << 24;
	}
}

/**
 * Add VXLAN-GPE item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] pattern_flags
 *   Item pattern flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */

static void
flow_dv_translate_item_vxlan_gpe(void *key, const struct rte_flow_item *item,
				 const uint64_t pattern_flags,
				 uint32_t key_type)
{
	static const struct rte_flow_item_vxlan_gpe dummy_vxlan_gpe_hdr = {{{0}}};
	const struct rte_flow_item_vxlan_gpe *vxlan_m = item->mask;
	const struct rte_flow_item_vxlan_gpe *vxlan_v = item->spec;
	/* The item was validated to be on the outer side */
	void *headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	void *misc_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	char *vni_v =
		MLX5_ADDR_OF(fte_match_set_misc3, misc_v, outer_vxlan_gpe_vni);
	int i, size = sizeof(vxlan_m->hdr.vni);
	uint8_t flags_m = 0xff;
	uint8_t flags_v = 0xc;
	uint8_t m_protocol, v_protocol;

	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		if (key_type & MLX5_SET_MATCHER_M)
			MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
				 0xFFFF);
		else
			MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
				 MLX5_UDP_PORT_VXLAN_GPE);
	}
	if (!vxlan_v) {
		vxlan_v = &dummy_vxlan_gpe_hdr;
		vxlan_m = &dummy_vxlan_gpe_hdr;
	} else {
		if (!vxlan_m)
			vxlan_m = &rte_flow_item_vxlan_gpe_mask;
	}
	if (key_type & MLX5_SET_MATCHER_M)
		vxlan_v = vxlan_m;
	else if (key_type == MLX5_SET_MATCHER_HS_V)
		vxlan_m = vxlan_v;
	for (i = 0; i < size; ++i)
		vni_v[i] = vxlan_m->hdr.vni[i] & vxlan_v->hdr.vni[i];
	if (vxlan_m->hdr.flags) {
		flags_m = vxlan_m->hdr.flags;
		flags_v = vxlan_v->hdr.flags;
	}
	MLX5_SET(fte_match_set_misc3, misc_v, outer_vxlan_gpe_flags,
		 flags_m & flags_v);
	m_protocol = vxlan_m->hdr.protocol;
	v_protocol = vxlan_v->hdr.protocol;
	if (!m_protocol) {
		/* Force next protocol to ensure next headers parsing. */
		if (pattern_flags & MLX5_FLOW_LAYER_INNER_L2)
			v_protocol = RTE_VXLAN_GPE_TYPE_ETH;
		else if (pattern_flags & MLX5_FLOW_LAYER_INNER_L3_IPV4)
			v_protocol = RTE_VXLAN_GPE_TYPE_IPV4;
		else if (pattern_flags & MLX5_FLOW_LAYER_INNER_L3_IPV6)
			v_protocol = RTE_VXLAN_GPE_TYPE_IPV6;
		if (v_protocol)
			m_protocol = 0xFF;
		/* Restore the value to mask in mask case. */
		if (key_type & MLX5_SET_MATCHER_M)
			v_protocol = m_protocol;
	}
	MLX5_SET(fte_match_set_misc3, misc_v,
		 outer_vxlan_gpe_next_protocol, m_protocol & v_protocol);
}

/**
 * Add Geneve item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] pattern_flags
 *   Item pattern flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */

static void
flow_dv_translate_item_geneve(void *key, const struct rte_flow_item *item,
			      uint64_t pattern_flags, uint32_t key_type)
{
	static const struct rte_flow_item_geneve empty_geneve = {0,};
	const struct rte_flow_item_geneve *geneve_m = item->mask;
	const struct rte_flow_item_geneve *geneve_v = item->spec;
	/* GENEVE flow item validation allows single tunnel item */
	void *headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	uint16_t gbhdr_m;
	uint16_t gbhdr_v;
	char *vni_v = MLX5_ADDR_OF(fte_match_set_misc, misc_v, geneve_vni);
	size_t size = sizeof(geneve_m->vni), i;
	uint16_t protocol_m, protocol_v;

	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		if (key_type & MLX5_SET_MATCHER_M)
			MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
				 0xFFFF);
		else
			MLX5_SET(fte_match_set_lyr_2_4, headers_v, udp_dport,
				 MLX5_UDP_PORT_GENEVE);
	}
	if (!geneve_v) {
		geneve_v = &empty_geneve;
		geneve_m = &empty_geneve;
	} else {
		if (!geneve_m)
			geneve_m = &rte_flow_item_geneve_mask;
	}
	if (key_type & MLX5_SET_MATCHER_M)
		geneve_v = geneve_m;
	else if (key_type == MLX5_SET_MATCHER_HS_V)
		geneve_m = geneve_v;
	for (i = 0; i < size; ++i)
		vni_v[i] = geneve_m->vni[i] & geneve_v->vni[i];
	gbhdr_m = rte_be_to_cpu_16(geneve_m->ver_opt_len_o_c_rsvd0);
	gbhdr_v = rte_be_to_cpu_16(geneve_v->ver_opt_len_o_c_rsvd0);
	MLX5_SET(fte_match_set_misc, misc_v, geneve_oam,
		 MLX5_GENEVE_OAMF_VAL(gbhdr_v) & MLX5_GENEVE_OAMF_VAL(gbhdr_m));
	MLX5_SET(fte_match_set_misc, misc_v, geneve_opt_len,
		 MLX5_GENEVE_OPTLEN_VAL(gbhdr_v) &
		 MLX5_GENEVE_OPTLEN_VAL(gbhdr_m));
	protocol_m = rte_be_to_cpu_16(geneve_m->protocol);
	protocol_v = rte_be_to_cpu_16(geneve_v->protocol);
	if (!protocol_m) {
		/* Force next protocol to prevent matchers duplication */
		protocol_v = mlx5_translate_tunnel_etypes(pattern_flags);
		if (protocol_v)
			protocol_m = 0xFFFF;
		/* Restore the value to mask in mask case. */
		if (key_type & MLX5_SET_MATCHER_M)
			protocol_v = protocol_m;
	}
	MLX5_SET(fte_match_set_misc, misc_v, geneve_protocol_type,
		 protocol_m & protocol_v);
}

/**
 * Create Geneve TLV option resource.
 *
 * @param dev[in, out]
 *   Pointer to rte_eth_dev structure.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */

int
flow_dev_geneve_tlv_option_resource_register(struct rte_eth_dev *dev,
					     const struct rte_flow_item *item,
					     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_geneve_tlv_option_resource *geneve_opt_resource =
			sh->geneve_tlv_option_resource;
	struct mlx5_devx_obj *obj;
	const struct rte_flow_item_geneve_opt *geneve_opt_v = item->spec;
	int ret = 0;

	if (!geneve_opt_v)
		return -1;
	rte_spinlock_lock(&sh->geneve_tlv_opt_sl);
	if (geneve_opt_resource != NULL) {
		if (geneve_opt_resource->option_class ==
			geneve_opt_v->option_class &&
			geneve_opt_resource->option_type ==
			geneve_opt_v->option_type &&
			geneve_opt_resource->length ==
			geneve_opt_v->option_len) {
			/*
			 * We already have GENEVE TLV option obj allocated.
			 * Increasing refcnt only in SWS. HWS uses it as global.
			 */
			if (priv->sh->config.dv_flow_en == 1)
				__atomic_fetch_add(&geneve_opt_resource->refcnt, 1,
						   __ATOMIC_RELAXED);
		} else {
			ret = rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Only one GENEVE TLV option supported");
			goto exit;
		}
	} else {
		/* Create a GENEVE TLV object and resource. */
		obj = mlx5_devx_cmd_create_geneve_tlv_option(sh->cdev->ctx,
				geneve_opt_v->option_class,
				geneve_opt_v->option_type,
				geneve_opt_v->option_len);
		if (!obj) {
			ret = rte_flow_error_set(error, ENODATA,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to create GENEVE TLV Devx object");
			goto exit;
		}
		sh->geneve_tlv_option_resource =
				mlx5_malloc(MLX5_MEM_ZERO,
						sizeof(*geneve_opt_resource),
						0, SOCKET_ID_ANY);
		if (!sh->geneve_tlv_option_resource) {
			claim_zero(mlx5_devx_cmd_destroy(obj));
			ret = rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"GENEVE TLV object memory allocation failed");
			goto exit;
		}
		geneve_opt_resource = sh->geneve_tlv_option_resource;
		geneve_opt_resource->obj = obj;
		geneve_opt_resource->option_class = geneve_opt_v->option_class;
		geneve_opt_resource->option_type = geneve_opt_v->option_type;
		geneve_opt_resource->length = geneve_opt_v->option_len;
		__atomic_store_n(&geneve_opt_resource->refcnt, 1,
				__ATOMIC_RELAXED);
	}
exit:
	rte_spinlock_unlock(&sh->geneve_tlv_opt_sl);
	return ret;
}

/**
 * Add Geneve TLV option item to value.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 * @param[out] error
 *   Pointer to error structure.
 */
static int
flow_dv_translate_item_geneve_opt(struct rte_eth_dev *dev, void *key,
				  const struct rte_flow_item *item,
				  uint32_t key_type,
				  struct rte_flow_error *error)
{
	const struct rte_flow_item_geneve_opt *geneve_opt_m;
	const struct rte_flow_item_geneve_opt *geneve_opt_v;
	const struct rte_flow_item_geneve_opt *geneve_opt_vv = item->spec;
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	rte_be32_t opt_data_key = 0, opt_data_mask = 0;
	uint32_t *data;
	int ret = 0;

	if (MLX5_ITEM_VALID(item, key_type))
		return -1;
	MLX5_ITEM_UPDATE(item, key_type, geneve_opt_v, geneve_opt_m,
			 &rte_flow_item_geneve_opt_mask);
	/* Register resource requires item spec. */
	if (key_type & MLX5_SET_MATCHER_V) {
		ret = flow_dev_geneve_tlv_option_resource_register(dev, item,
								   error);
		if (ret) {
			DRV_LOG(ERR, "Failed to create geneve_tlv_obj");
			return ret;
		}
	}
	/*
	 * Set the option length in GENEVE header if not requested.
	 * The GENEVE TLV option length is expressed by the option length field
	 * in the GENEVE header.
	 * If the option length was not requested but the GENEVE TLV option item
	 * is present we set the option length field implicitly.
	 */
	if (!MLX5_GET16(fte_match_set_misc, misc_v, geneve_opt_len)) {
		if (key_type & MLX5_SET_MATCHER_M)
			MLX5_SET(fte_match_set_misc, misc_v, geneve_opt_len,
				 MLX5_GENEVE_OPTLEN_MASK);
		else
			MLX5_SET(fte_match_set_misc, misc_v, geneve_opt_len,
				 geneve_opt_v->option_len + 1);
	}
	/* Set the data. */
	if (key_type == MLX5_SET_MATCHER_SW_V)
		data = geneve_opt_vv->data;
	else
		data = geneve_opt_v->data;
	if (data) {
		memcpy(&opt_data_key, data,
			RTE_MIN((uint32_t)(geneve_opt_v->option_len * 4),
				sizeof(opt_data_key)));
		memcpy(&opt_data_mask, geneve_opt_m->data,
			RTE_MIN((uint32_t)(geneve_opt_v->option_len * 4),
				sizeof(opt_data_mask)));
		MLX5_SET(fte_match_set_misc3, misc3_v,
				geneve_tlv_option_0_data,
			rte_be_to_cpu_32(opt_data_key & opt_data_mask));
	}
	return ret;
}

/**
 * Add MPLS item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] prev_layer
 *   The protocol layer indicated in previous item.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_mpls(void *key, const struct rte_flow_item *item,
			    uint64_t prev_layer, int inner,
			    uint32_t key_type)
{
	const uint32_t *in_mpls_m;
	const uint32_t *in_mpls_v;
	uint32_t *out_mpls_v = 0;
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	void *misc2_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);
	void *headers_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);

	switch (prev_layer) {
	case MLX5_FLOW_LAYER_OUTER_L4_UDP:
		if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
			if (key_type & MLX5_SET_MATCHER_M)
				MLX5_SET(fte_match_set_lyr_2_4, headers_v,
					 udp_dport, 0xffff);
			else
				MLX5_SET(fte_match_set_lyr_2_4, headers_v,
					 udp_dport, MLX5_UDP_PORT_MPLS);
		}
		break;
	case MLX5_FLOW_LAYER_GRE:
		/* Fall-through. */
	case MLX5_FLOW_LAYER_GRE_KEY:
		if (!MLX5_GET16(fte_match_set_misc, misc_v, gre_protocol)) {
			if (key_type & MLX5_SET_MATCHER_M)
				MLX5_SET(fte_match_set_misc, misc_v,
					 gre_protocol, 0xffff);
			else
				MLX5_SET(fte_match_set_misc, misc_v,
					 gre_protocol, RTE_ETHER_TYPE_MPLS);
		}
		break;
	default:
		break;
	}
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, in_mpls_v, in_mpls_m,
			 &rte_flow_item_mpls_mask);
	switch (prev_layer) {
	case MLX5_FLOW_LAYER_OUTER_L4_UDP:
		out_mpls_v =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_v,
						 outer_first_mpls_over_udp);
		break;
	case MLX5_FLOW_LAYER_GRE:
		out_mpls_v =
			(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2, misc2_v,
						 outer_first_mpls_over_gre);
		break;
	default:
		/* Inner MPLS not over GRE is not supported. */
		if (!inner)
			out_mpls_v =
				(uint32_t *)MLX5_ADDR_OF(fte_match_set_misc2,
							 misc2_v,
							 outer_first_mpls);
		break;
	}
	if (out_mpls_v)
		*out_mpls_v = *in_mpls_v & *in_mpls_m;
}

/**
 * Add metadata register item to matcher
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] reg_type
 *   Type of device metadata register
 * @param[in] data
 *   Register data
 * @param[in] mask
 *   Register mask
 */
static void
flow_dv_match_meta_reg(void *key, enum modify_reg reg_type,
		       uint32_t data, uint32_t mask)
{
	void *misc2_v =
		MLX5_ADDR_OF(fte_match_param, key, misc_parameters_2);
	uint32_t temp;

	data &= mask;
	switch (reg_type) {
	case REG_A:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_a, data);
		break;
	case REG_B:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_b, data);
		break;
	case REG_C_0:
		/*
		 * The metadata register C0 field might be divided into
		 * source vport index and META item value, we should set
		 * this field according to specified mask, not as whole one.
		 */
		temp = MLX5_GET(fte_match_set_misc2, misc2_v, metadata_reg_c_0);
		if (mask)
			temp &= ~mask;
		temp |= data;
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_0, temp);
		break;
	case REG_C_1:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_1, data);
		break;
	case REG_C_2:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_2, data);
		break;
	case REG_C_3:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_3, data);
		break;
	case REG_C_4:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_4, data);
		break;
	case REG_C_5:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_5, data);
		break;
	case REG_C_6:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_6, data);
		break;
	case REG_C_7:
		MLX5_SET(fte_match_set_misc2, misc2_v, metadata_reg_c_7, data);
		break;
	default:
		MLX5_ASSERT(false);
		break;
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
flow_dv_match_meta_reg_all(void *matcher, void *key, enum modify_reg reg_type,
			   uint32_t data, uint32_t mask)
{
	flow_dv_match_meta_reg(key, reg_type, data, mask);
	flow_dv_match_meta_reg(matcher, reg_type, mask, mask);
}

/**
 * Add MARK item to matcher
 *
 * @param[in] dev
 *   The device to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_mark(struct rte_eth_dev *dev, void *key,
			    const struct rte_flow_item *item,
			    uint32_t key_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_mark *mark;
	uint32_t value;
	uint32_t mask = 0;

	if (key_type & MLX5_SET_MATCHER_SW) {
		mark = item->mask ? (const void *)item->mask :
				    &rte_flow_item_mark_mask;
		mask = mark->id;
		if (key_type == MLX5_SET_MATCHER_SW_M) {
			value = mask;
		} else {
			mark = (const void *)item->spec;
			MLX5_ASSERT(mark);
			value = mark->id;
		}
	} else {
		mark = (key_type == MLX5_SET_MATCHER_HS_V) ?
			(const void *)item->spec : (const void *)item->mask;
		MLX5_ASSERT(mark);
		value = mark->id;
		if (key_type == MLX5_SET_MATCHER_HS_M)
			mask = value;
	}
	mask &= priv->sh->dv_mark_mask;
	value &= mask;
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
		flow_dv_match_meta_reg(key, reg, value, mask);
	}
}

/**
 * Add META item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] attr
 *   Attributes of flow that includes this item.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_meta(struct rte_eth_dev *dev,
			    void *key,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item *item,
			    uint32_t key_type)
{
	const struct rte_flow_item_meta *meta_m;
	const struct rte_flow_item_meta *meta_v;
	uint32_t value;
	uint32_t mask = 0;
	int reg;

	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, meta_v, meta_m,
			 &rte_flow_item_meta_mask);
	value = meta_v->data;
	mask = meta_m->data;
	if (key_type == MLX5_SET_MATCHER_HS_M)
		mask = value;
	/*
	 * In the current implementation, REG_B cannot be used to match.
	 * Force to use REG_C_1 in HWS root table as other tables.
	 * This map may change.
	 * NIC: modify - REG_B to be present in SW
	 *      match - REG_C_1 when copied from FDB, different from SWS
	 * FDB: modify - REG_C_1 in Xmeta mode, REG_NON in legacy mode
	 *      match - REG_C_1 in FDB
	 */
	if (!!(key_type & MLX5_SET_MATCHER_SW))
		reg = flow_dv_get_metadata_reg(dev, attr, NULL);
	else
		reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_META, 0);
	if (reg < 0)
		return;
	MLX5_ASSERT(reg != REG_NON);
	if (reg == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0 = rte_bsf32(msk_c0);

		mask &= msk_c0;
		mask <<= shl_c0;
		value <<= shl_c0;
	}
	flow_dv_match_meta_reg(key, reg, value, mask);
}

/**
 * Add vport metadata Reg C0 item to matcher
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] value
 *   Register value
 * @param[in] mask
 *   Register mask
 */
static void
flow_dv_translate_item_meta_vport(void *key, uint32_t value, uint32_t mask)
{
	flow_dv_match_meta_reg(key, REG_C_0, value, mask);
}

/**
 * Add tag item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_mlx5_item_tag(struct rte_eth_dev *dev, void *key,
				const struct rte_flow_item *item,
				uint32_t key_type)
{
	const struct mlx5_rte_flow_item_tag *tag_v = item->spec;
	const struct mlx5_rte_flow_item_tag *tag_m = item->mask;
	uint32_t mask, value;

	MLX5_ASSERT(tag_v);
	value = tag_v->data;
	mask = tag_m ? tag_m->data : UINT32_MAX;
	if (key_type & MLX5_SET_MATCHER_M)
		value = mask;
	if (tag_v->id == REG_C_0) {
		struct mlx5_priv *priv = dev->data->dev_private;
		uint32_t msk_c0 = priv->sh->dv_regc0_mask;
		uint32_t shl_c0 = rte_bsf32(msk_c0);

		mask &= msk_c0;
		mask <<= shl_c0;
		value <<= shl_c0;
	}
	flow_dv_match_meta_reg(key, tag_v->id, value, mask);
}

/**
 * Add TAG item to matcher
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_tag(struct rte_eth_dev *dev, void *key,
			   const struct rte_flow_item *item,
			   uint32_t key_type)
{
	const struct rte_flow_item_tag *tag_vv = item->spec;
	const struct rte_flow_item_tag *tag_v;
	const struct rte_flow_item_tag *tag_m;
	int reg;
	uint32_t index;

	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, tag_v, tag_m,
		&rte_flow_item_tag_mask);
	/* When set mask, the index should be from spec. */
	index = tag_vv ? tag_vv->index : tag_v->index;
	/* Get the metadata register index for the tag. */
	if (!!(key_type & MLX5_SET_MATCHER_SW))
		reg = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, index, NULL);
	else
		reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_TAG, index);
	MLX5_ASSERT(reg > 0);
	flow_dv_match_meta_reg(key, (enum modify_reg)reg, tag_v->data, tag_m->data);
}

/**
 * Add source vport match to the specified matcher.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] port
 *   Source vport value to match
 */
static void
flow_dv_translate_item_source_vport(void *key,
				    int16_t port)
{
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);

	MLX5_SET(fte_match_set_misc, misc_v, source_port, port);
}

/**
 * Translate port-id item to eswitch match on  port-id.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] attr
 *   Flow attributes.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_dv_translate_item_port_id(struct rte_eth_dev *dev, void *key,
			       const struct rte_flow_item *item,
			       const struct rte_flow_attr *attr,
			       uint32_t key_type)
{
	const struct rte_flow_item_port_id *pid_m = item ? item->mask : NULL;
	const struct rte_flow_item_port_id *pid_v = item ? item->spec : NULL;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_priv *priv;
	uint16_t mask, id;
	uint32_t vport_meta;

	MLX5_ASSERT(wks);
	if (pid_v && pid_v->id == MLX5_PORT_ESW_MGR) {
		flow_dv_translate_item_source_vport(key,
				key_type & MLX5_SET_MATCHER_V ?
				mlx5_flow_get_esw_manager_vport_id(dev) : 0xffff);
		return 0;
	}
	mask = pid_m ? pid_m->id : 0xffff;
	id = pid_v ? pid_v->id : dev->data->port_id;
	priv = mlx5_port_to_eswitch_info(id, item == NULL);
	if (!priv)
		return -rte_errno;
	if (key_type & MLX5_SET_MATCHER_M) {
		id = mask;
		vport_meta = priv->vport_meta_mask;
	} else {
		id = priv->vport_id;
		vport_meta = priv->vport_meta_tag;
		wks->vport_meta_tag = vport_meta;
	}
	/*
	 * Translate to vport field or to metadata, depending on mode.
	 * Kernel can use either misc.source_port or half of C0 metadata
	 * register.
	 */
	if (priv->vport_meta_mask) {
		/*
		 * Provide the hint for SW steering library
		 * to insert the flow into ingress domain and
		 * save the extra vport match.
		 */
		if (mask == 0xffff && priv->vport_id == 0xffff &&
		    priv->pf_bond < 0 && attr->transfer)
			flow_dv_translate_item_source_vport(key, id);
		/*
		 * We should always set the vport metadata register,
		 * otherwise the SW steering library can drop
		 * the rule if wire vport metadata value is not zero,
		 * it depends on kernel configuration.
		 */
		flow_dv_translate_item_meta_vport
				(key, vport_meta, priv->vport_meta_mask);
	} else {
		flow_dv_translate_item_source_vport(key, id);
	}
	return 0;
}

/**
 * Translate port representor item to eswitch match on port id.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_dv_translate_item_port_representor(struct rte_eth_dev *dev, void *key,
					uint32_t key_type)
{
	flow_dv_translate_item_source_vport(key,
			key_type & MLX5_SET_MATCHER_V ?
			mlx5_flow_get_esw_manager_vport_id(dev) : 0xffff);
	return 0;
}

/**
 * Translate represented port item to eswitch match on port id.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in]
 *   Flow attributes.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_dv_translate_item_represented_port(struct rte_eth_dev *dev, void *key,
					const struct rte_flow_item *item,
					const struct rte_flow_attr *attr,
					uint32_t key_type)
{
	const struct rte_flow_item_ethdev *pid_m = item ? item->mask : NULL;
	const struct rte_flow_item_ethdev *pid_v = item ? item->spec : NULL;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_priv *priv;
	uint16_t mask, id;
	uint32_t vport_meta;

	MLX5_ASSERT(wks);
	if (!pid_m && !pid_v)
		return 0;
	if (pid_v && pid_v->port_id == UINT16_MAX) {
		flow_dv_translate_item_source_vport(key,
			key_type & MLX5_SET_MATCHER_V ?
			mlx5_flow_get_esw_manager_vport_id(dev) : 0xffff);
		return 0;
	}
	mask = pid_m ? pid_m->port_id : UINT16_MAX;
	id = pid_v ? pid_v->port_id : dev->data->port_id;
	priv = mlx5_port_to_eswitch_info(id, item == NULL);
	if (!priv)
		return -rte_errno;
	if (key_type & MLX5_SET_MATCHER_M) {
		id = mask;
		vport_meta = priv->vport_meta_mask;
	} else {
		id = priv->vport_id;
		vport_meta = priv->vport_meta_tag;
		wks->vport_meta_tag = vport_meta;
	}
	/*
	 * Translate to vport field or to metadata, depending on mode.
	 * Kernel can use either misc.source_port or half of C0 metadata
	 * register.
	 */
	if (priv->vport_meta_mask) {
		/*
		 * Provide the hint for SW steering library
		 * to insert the flow into ingress domain and
		 * save the extra vport match.
		 */
		if (mask == UINT16_MAX && priv->vport_id == UINT16_MAX &&
		    priv->pf_bond < 0 && attr->transfer &&
		    priv->sh->config.dv_flow_en != 2)
			flow_dv_translate_item_source_vport(key, id);
		/*
		 * We should always set the vport metadata register,
		 * otherwise the SW steering library can drop
		 * the rule if wire vport metadata value is not zero,
		 * it depends on kernel configuration.
		 */
		flow_dv_translate_item_meta_vport(key, vport_meta,
						  priv->vport_meta_mask);
	} else {
		flow_dv_translate_item_source_vport(key, id);
	}
	return 0;
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
 * @param[in] attr
 *   Flow attributes.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
flow_dv_translate_item_port_id_all(struct rte_eth_dev *dev,
			       void *matcher, void *key,
			       const struct rte_flow_item *item,
			       const struct rte_flow_attr *attr)
{
	int ret;

	ret = flow_dv_translate_item_port_id
			(dev, matcher, item, attr, MLX5_SET_MATCHER_SW_M);
	if (ret)
		return ret;
	ret = flow_dv_translate_item_port_id
			(dev, key, item, attr, MLX5_SET_MATCHER_SW_V);
	return ret;
}


/**
 * Add ICMP6 item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_icmp6(void *key, const struct rte_flow_item *item,
			     int inner, uint32_t key_type)
{
	const struct rte_flow_item_icmp6 *icmp6_m;
	const struct rte_flow_item_icmp6 *icmp6_v;
	void *headers_v;
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, 0xFF);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
			 IPPROTO_ICMPV6);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, icmp6_v, icmp6_m,
		&rte_flow_item_icmp6_mask);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_type,
		 icmp6_v->type & icmp6_m->type);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_code,
		 icmp6_v->code & icmp6_m->code);
}

/**
 * Add ICMP6 echo request/reply item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_icmp6_echo(void *key, const struct rte_flow_item *item,
				  int inner, uint32_t key_type)
{
	const struct rte_flow_item_icmp6_echo *icmp6_m;
	const struct rte_flow_item_icmp6_echo *icmp6_v;
	uint32_t icmp6_header_data_m = 0;
	uint32_t icmp6_header_data_v = 0;
	void *headers_v;
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	uint8_t icmp6_type = 0;
	struct rte_flow_item_icmp6_echo zero_mask;

	memset(&zero_mask, 0, sizeof(zero_mask));
	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
		MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol, 0xFF);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v, ip_protocol,
			 IPPROTO_ICMPV6);
	MLX5_ITEM_UPDATE(item, key_type, icmp6_v, icmp6_m, &zero_mask);
	/* Set fixed type and code for icmpv6 echo request or reply */
	icmp6_type = (item->type == RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST ?
		      RTE_ICMP6_ECHO_REQUEST : RTE_ICMP6_ECHO_REPLY);
	if (key_type & MLX5_SET_MATCHER_M) {
		MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_type, 0xFF);
		MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_code, 0xFF);
	} else {
		MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_type, icmp6_type);
		MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_code, 0);
	}
	if (icmp6_v == NULL)
		return;
	/* Set icmp6 header data (identifier & sequence) accordingly */
	icmp6_header_data_m =
		(rte_be_to_cpu_16(icmp6_m->hdr.identifier) << 16) |
		rte_be_to_cpu_16(icmp6_m->hdr.sequence);
	if (icmp6_header_data_m) {
		icmp6_header_data_v =
			(rte_be_to_cpu_16(icmp6_v->hdr.identifier) << 16) |
			rte_be_to_cpu_16(icmp6_v->hdr.sequence);
		MLX5_SET(fte_match_set_misc3, misc3_v, icmpv6_header_data,
			 icmp6_header_data_v & icmp6_header_data_m);
	}
}

/**
 * Add ICMP item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_icmp(void *key, const struct rte_flow_item *item,
			    int inner, uint32_t key_type)
{
	const struct rte_flow_item_icmp *icmp_m;
	const struct rte_flow_item_icmp *icmp_v;
	uint32_t icmp_header_data_m = 0;
	uint32_t icmp_header_data_v = 0;
	void *headers_v;
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
			MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (key_type & MLX5_SET_MATCHER_M)
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, 0xFF);
	else
		MLX5_SET(fte_match_set_lyr_2_4, headers_v,
			 ip_protocol, IPPROTO_ICMP);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, icmp_v, icmp_m,
		&rte_flow_item_icmp_mask);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_type,
		 icmp_v->hdr.icmp_type & icmp_m->hdr.icmp_type);
	MLX5_SET(fte_match_set_misc3, misc3_v, icmp_code,
		 icmp_v->hdr.icmp_code & icmp_m->hdr.icmp_code);
	icmp_header_data_m = rte_be_to_cpu_16(icmp_m->hdr.icmp_seq_nb);
	icmp_header_data_m |= rte_be_to_cpu_16(icmp_m->hdr.icmp_ident) << 16;
	if (icmp_header_data_m) {
		icmp_header_data_v = rte_be_to_cpu_16(icmp_v->hdr.icmp_seq_nb);
		icmp_header_data_v |=
			 rte_be_to_cpu_16(icmp_v->hdr.icmp_ident) << 16;
		MLX5_SET(fte_match_set_misc3, misc3_v, icmp_header_data,
			 icmp_header_data_v & icmp_header_data_m);
	}
}

/**
 * Add GTP item to the value.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] inner
 *   Item is inner pattern.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_gtp(void *key, const struct rte_flow_item *item,
			   int inner, uint32_t key_type)
{
	const struct rte_flow_item_gtp *gtp_m;
	const struct rte_flow_item_gtp *gtp_v;
	void *headers_v;
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	uint16_t dport = RTE_GTPU_UDP_PORT;

	headers_v = inner ? MLX5_ADDR_OF(fte_match_param, key, inner_headers) :
			MLX5_ADDR_OF(fte_match_param, key, outer_headers);
	if (!MLX5_GET16(fte_match_set_lyr_2_4, headers_v, udp_dport)) {
		if (key_type & MLX5_SET_MATCHER_M)
			MLX5_SET(fte_match_set_lyr_2_4, headers_v,
				 udp_dport, 0xFFFF);
		else
			MLX5_SET(fte_match_set_lyr_2_4, headers_v,
				 udp_dport, dport);
	}
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, gtp_v, gtp_m,
		&rte_flow_item_gtp_mask);
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_msg_flags,
		 gtp_v->hdr.gtp_hdr_info & gtp_m->hdr.gtp_hdr_info);
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_msg_type,
		 gtp_v->hdr.msg_type & gtp_m->hdr.msg_type);
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_teid,
		 rte_be_to_cpu_32(gtp_v->hdr.teid & gtp_m->hdr.teid));
}

/**
 * Add GTP PSC item to matcher.
 *
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static int
flow_dv_translate_item_gtp_psc(void *key, const struct rte_flow_item *item,
			       uint32_t key_type)
{
	const struct rte_flow_item_gtp_psc *gtp_psc_m;
	const struct rte_flow_item_gtp_psc *gtp_psc_v;
	void *misc3_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_3);
	union {
		uint32_t w32;
		struct {
			uint16_t seq_num;
			uint8_t npdu_num;
			uint8_t next_ext_header_type;
		};
	} dw_2;
	union {
		uint32_t w32;
		struct {
			uint8_t len;
			uint8_t type_flags;
			uint8_t qfi;
			uint8_t reserved;
		};
	} dw_0;
	uint8_t gtp_flags;

	/* Always set E-flag match on one, regardless of GTP item settings. */
	gtp_flags = MLX5_GET(fte_match_set_misc3, misc3_v, gtpu_msg_flags);
	gtp_flags |= MLX5_GTP_EXT_HEADER_FLAG;
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_msg_flags, gtp_flags);
	/*Set next extension header type. */
	dw_2.seq_num = 0;
	dw_2.npdu_num = 0;
	if (key_type & MLX5_SET_MATCHER_M)
		dw_2.next_ext_header_type = 0xff;
	else
		dw_2.next_ext_header_type = 0x85;
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_dw_2,
		 rte_cpu_to_be_32(dw_2.w32));
	if (MLX5_ITEM_VALID(item, key_type))
		return 0;
	MLX5_ITEM_UPDATE(item, key_type, gtp_psc_v,
		gtp_psc_m, &rte_flow_item_gtp_psc_mask);
	dw_0.w32 = 0;
	dw_0.type_flags = MLX5_GTP_PDU_TYPE_SHIFT(gtp_psc_v->hdr.type &
						  gtp_psc_m->hdr.type);
	dw_0.qfi = gtp_psc_v->hdr.qfi & gtp_psc_m->hdr.qfi;
	MLX5_SET(fte_match_set_misc3, misc3_v, gtpu_first_ext_dw_0,
		 rte_cpu_to_be_32(dw_0.w32));
	return 0;
}

/**
 * Add eCPRI item to matcher and to the value.
 *
 * @param[in] dev
 *   The devich to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] last_item
 *   Last item flags.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_ecpri(struct rte_eth_dev *dev, void *key,
			     const struct rte_flow_item *item,
			     uint64_t last_item, uint32_t key_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_ecpri *ecpri_m;
	const struct rte_flow_item_ecpri *ecpri_v;
	const struct rte_flow_item_ecpri *ecpri_vv = item->spec;
	struct rte_ecpri_common_hdr common;
	void *misc4_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters_4);
	uint32_t *samples;
	void *dw_v;

	/*
	 * In case of eCPRI over Ethernet, if EtherType is not specified,
	 * match on eCPRI EtherType implicitly.
	 */
	if (last_item & MLX5_FLOW_LAYER_OUTER_L2) {
		void *hdrs_v, *l2v;

		hdrs_v = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
		l2v = MLX5_ADDR_OF(fte_match_set_lyr_2_4, hdrs_v, ethertype);
		if (*(uint16_t *)l2v == 0) {
			if (key_type & MLX5_SET_MATCHER_M)
				*(uint16_t *)l2v = UINT16_MAX;
			else
				*(uint16_t *)l2v =
					RTE_BE16(RTE_ETHER_TYPE_ECPRI);
		}
	}
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, ecpri_v, ecpri_m,
		&rte_flow_item_ecpri_mask);
	/*
	 * Maximal four DW samples are supported in a single matching now.
	 * Two are used now for a eCPRI matching:
	 * 1. Type: one byte, mask should be 0x00ff0000 in network order
	 * 2. ID of a message: one or two bytes, mask 0xffff0000 or 0xff000000
	 *    if any.
	 */
	if (!ecpri_m->hdr.common.u32)
		return;
	samples = priv->sh->ecpri_parser.ids;
	/* Need to take the whole DW as the mask to fill the entry. */
	dw_v = MLX5_ADDR_OF(fte_match_set_misc4, misc4_v,
			    prog_sample_field_value_0);
	/* Already big endian (network order) in the header. */
	*(uint32_t *)dw_v = ecpri_v->hdr.common.u32 & ecpri_m->hdr.common.u32;
	/* Sample#0, used for matching type, offset 0. */
	/* It makes no sense to set the sample ID in the mask field. */
	MLX5_SET(fte_match_set_misc4, misc4_v,
		 prog_sample_field_id_0, samples[0]);
	/*
	 * Checking if message body part needs to be matched.
	 * Some wildcard rules only matching type field should be supported.
	 */
	if (ecpri_m->hdr.dummy[0]) {
		if (key_type == MLX5_SET_MATCHER_SW_M)
			common.u32 = rte_be_to_cpu_32(ecpri_vv->hdr.common.u32);
		else
			common.u32 = rte_be_to_cpu_32(ecpri_v->hdr.common.u32);
		switch (common.type) {
		case RTE_ECPRI_MSG_TYPE_IQ_DATA:
		case RTE_ECPRI_MSG_TYPE_RTC_CTRL:
		case RTE_ECPRI_MSG_TYPE_DLY_MSR:
			dw_v = MLX5_ADDR_OF(fte_match_set_misc4, misc4_v,
					    prog_sample_field_value_1);
			*(uint32_t *)dw_v = ecpri_v->hdr.dummy[0] &
					    ecpri_m->hdr.dummy[0];
			/* Sample#1, to match message body, offset 4. */
			MLX5_SET(fte_match_set_misc4, misc4_v,
				 prog_sample_field_id_1, samples[1]);
			break;
		default:
			/* Others, do not match any sample ID. */
			break;
		}
	}
}

/*
 * Add connection tracking status item to matcher
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
flow_dv_translate_item_aso_ct(struct rte_eth_dev *dev,
			      void *matcher, void *key,
			      const struct rte_flow_item *item)
{
	uint32_t reg_value = 0;
	int reg_id;
	/* 8LSB 0b 11/0000/11, middle 4 bits are reserved. */
	uint32_t reg_mask = 0;
	const struct rte_flow_item_conntrack *spec = item->spec;
	const struct rte_flow_item_conntrack *mask = item->mask;
	uint32_t flags;
	struct rte_flow_error error;

	if (!mask)
		mask = &rte_flow_item_conntrack_mask;
	if (!spec || !mask->flags)
		return;
	flags = spec->flags & mask->flags;
	/* The conflict should be checked in the validation. */
	if (flags & RTE_FLOW_CONNTRACK_PKT_STATE_VALID)
		reg_value |= MLX5_CT_SYNDROME_VALID;
	if (flags & RTE_FLOW_CONNTRACK_PKT_STATE_CHANGED)
		reg_value |= MLX5_CT_SYNDROME_STATE_CHANGE;
	if (flags & RTE_FLOW_CONNTRACK_PKT_STATE_INVALID)
		reg_value |= MLX5_CT_SYNDROME_INVALID;
	if (flags & RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED)
		reg_value |= MLX5_CT_SYNDROME_TRAP;
	if (flags & RTE_FLOW_CONNTRACK_PKT_STATE_BAD)
		reg_value |= MLX5_CT_SYNDROME_BAD_PACKET;
	if (mask->flags & (RTE_FLOW_CONNTRACK_PKT_STATE_VALID |
			   RTE_FLOW_CONNTRACK_PKT_STATE_INVALID |
			   RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED))
		reg_mask |= 0xc0;
	if (mask->flags & RTE_FLOW_CONNTRACK_PKT_STATE_CHANGED)
		reg_mask |= MLX5_CT_SYNDROME_STATE_CHANGE;
	if (mask->flags & RTE_FLOW_CONNTRACK_PKT_STATE_BAD)
		reg_mask |= MLX5_CT_SYNDROME_BAD_PACKET;
	/* The REG_C_x value could be saved during startup. */
	reg_id = mlx5_flow_get_reg_id(dev, MLX5_ASO_CONNTRACK, 0, &error);
	if (reg_id == REG_NON)
		return;
	flow_dv_match_meta_reg_all(matcher, key, (enum modify_reg)reg_id,
			       reg_value, reg_mask);
}

static void
flow_dv_translate_item_flex(struct rte_eth_dev *dev, void *matcher, void *key,
			    const struct rte_flow_item *item,
			    struct mlx5_flow *dev_flow, bool is_inner)
{
	const struct rte_flow_item_flex *spec =
		(const struct rte_flow_item_flex *)item->spec;
	int index = mlx5_flex_acquire_index(dev, spec->handle, false);

	MLX5_ASSERT(index >= 0 && index < (int)(sizeof(uint32_t) * CHAR_BIT));
	if (index < 0)
		return;
	if (!(dev_flow->handle->flex_item & RTE_BIT32(index))) {
		/* Don't count both inner and outer flex items in one rule. */
		if (mlx5_flex_acquire_index(dev, spec->handle, true) != index)
			MLX5_ASSERT(false);
		dev_flow->handle->flex_item |= (uint8_t)RTE_BIT32(index);
	}
	mlx5_flex_flow_translate_item(dev, matcher, key, item, is_inner);
}

/**
 * Add METER_COLOR item to matcher
 *
 * @param[in] dev
 *   The device to configure through.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_meter_color(struct rte_eth_dev *dev, void *key,
			    const struct rte_flow_item *item,
			    uint32_t key_type)
{
	const struct rte_flow_item_meter_color *color_m = item->mask;
	const struct rte_flow_item_meter_color *color_v = item->spec;
	uint32_t value, mask;
	int reg = REG_NON;

	MLX5_ASSERT(color_v);
	if (MLX5_ITEM_VALID(item, key_type))
		return;
	MLX5_ITEM_UPDATE(item, key_type, color_v, color_m,
		&rte_flow_item_meter_color_mask);
	value = rte_col_2_mlx5_col(color_v->color);
	mask = color_m ?
		color_m->color : (UINT32_C(1) << MLX5_MTR_COLOR_BITS) - 1;
	if (!!(key_type & MLX5_SET_MATCHER_SW))
		reg = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, NULL);
	else
		reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_METER_COLOR, 0);
	if (reg == REG_NON)
		return;
	flow_dv_match_meta_reg(key, (enum modify_reg)reg, value, mask);
}

static void
flow_dv_translate_item_aggr_affinity(void *key,
				    const struct rte_flow_item *item,
				    uint32_t key_type)
{
	const struct rte_flow_item_aggr_affinity *affinity_v;
	const struct rte_flow_item_aggr_affinity *affinity_m;
	void *misc_v;

	MLX5_ITEM_UPDATE(item, key_type, affinity_v, affinity_m,
			 &rte_flow_item_aggr_affinity_mask);
	misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	MLX5_SET(fte_match_set_misc, misc_v, lag_rx_port_affinity,
		 affinity_v->affinity & affinity_m->affinity);
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
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters_5)) <<
		MLX5_MATCH_CRITERIA_ENABLE_MISC5_BIT;
	return match_criteria_enable;
}

static void
__flow_dv_adjust_buf_size(size_t *size, uint8_t match_criteria)
{
	/*
	 * Check flow matching criteria first, subtract misc5/4 length if flow
	 * doesn't own misc5/4 parameters. In some old rdma-core releases,
	 * misc5/4 are not supported, and matcher creation failure is expected
	 * w/o subtraction. If misc5 is provided, misc4 must be counted in since
	 * misc5 is right after misc4.
	 */
	if (!(match_criteria & (1 << MLX5_MATCH_CRITERIA_ENABLE_MISC5_BIT))) {
		*size = MLX5_ST_SZ_BYTES(fte_match_param) -
			MLX5_ST_SZ_BYTES(fte_match_set_misc5);
		if (!(match_criteria & (1 <<
			MLX5_MATCH_CRITERIA_ENABLE_MISC4_BIT))) {
			*size -= MLX5_ST_SZ_BYTES(fte_match_set_misc4);
		}
	}
}

static struct mlx5_list_entry *
flow_dv_matcher_clone_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_matcher *ref = ctx->data;
	struct mlx5_flow_tbl_data_entry *tbl = container_of(ref->tbl,
							    typeof(*tbl), tbl);
	struct mlx5_flow_dv_matcher *resource = mlx5_malloc(MLX5_MEM_ANY,
							    sizeof(*resource),
							    0, SOCKET_ID_ANY);

	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create matcher");
		return NULL;
	}
	memcpy(resource, entry, sizeof(*resource));
	resource->tbl = &tbl->tbl;
	return &resource->entry;
}

static void
flow_dv_matcher_clone_free_cb(void *tool_ctx __rte_unused,
			     struct mlx5_list_entry *entry)
{
	mlx5_free(entry);
}

struct mlx5_list_entry *
flow_dv_tbl_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct mlx5_flow_tbl_tunnel_prm *tt_prm = ctx->data2;
	struct rte_flow_error *error = ctx->error;
	union mlx5_flow_tbl_key key = { .v64 = *(uint64_t *)(ctx->data) };
	struct mlx5_flow_tbl_resource *tbl;
	void *domain;
	uint32_t idx = 0;
	int ret;

	tbl_data = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_JUMP], &idx);
	if (!tbl_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	tbl_data->idx = idx;
	tbl_data->tunnel = tt_prm->tunnel;
	tbl_data->group_id = tt_prm->group_id;
	tbl_data->external = !!tt_prm->external;
	tbl_data->tunnel_offload = is_tunnel_offload_active(dev);
	tbl_data->is_egress = !!key.is_egress;
	tbl_data->is_transfer = !!key.is_fdb;
	tbl_data->dummy = !!key.dummy;
	tbl_data->level = key.level;
	tbl_data->id = key.id;
	tbl = &tbl_data->tbl;
	if (key.dummy)
		return &tbl_data->entry;
	if (key.is_fdb)
		domain = sh->fdb_domain;
	else if (key.is_egress)
		domain = sh->tx_domain;
	else
		domain = sh->rx_domain;
	ret = mlx5_flow_os_create_flow_tbl(domain, key.level, &tbl->obj);
	if (ret) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create flow table object");
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], idx);
		return NULL;
	}
	if (key.level != 0) {
		ret = mlx5_flow_os_create_flow_action_dest_flow_tbl
					(tbl->obj, &tbl_data->jump.action);
		if (ret) {
			rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "cannot create flow jump action");
			mlx5_flow_os_destroy_flow_tbl(tbl->obj);
			mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], idx);
			return NULL;
		}
	}
	MKSTR(matcher_name, "%s_%s_%u_%u_matcher_list",
	      key.is_fdb ? "FDB" : "NIC", key.is_egress ? "egress" : "ingress",
	      key.level, key.id);
	tbl_data->matchers = mlx5_list_create(matcher_name, sh, true,
					      flow_dv_matcher_create_cb,
					      flow_dv_matcher_match_cb,
					      flow_dv_matcher_remove_cb,
					      flow_dv_matcher_clone_cb,
					      flow_dv_matcher_clone_free_cb);
	if (!tbl_data->matchers) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot create tbl matcher list");
		mlx5_flow_os_destroy_flow_action(tbl_data->jump.action);
		mlx5_flow_os_destroy_flow_tbl(tbl->obj);
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], idx);
		return NULL;
	}
	return &tbl_data->entry;
}

int
flow_dv_tbl_match_cb(void *tool_ctx __rte_unused, struct mlx5_list_entry *entry,
		     void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_tbl_data_entry *tbl_data =
		container_of(entry, struct mlx5_flow_tbl_data_entry, entry);
	union mlx5_flow_tbl_key key = { .v64 =  *(uint64_t *)(ctx->data) };

	return tbl_data->level != key.level ||
	       tbl_data->id != key.id ||
	       tbl_data->dummy != key.dummy ||
	       tbl_data->is_transfer != !!key.is_fdb ||
	       tbl_data->is_egress != !!key.is_egress;
}

struct mlx5_list_entry *
flow_dv_tbl_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
		      void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct rte_flow_error *error = ctx->error;
	uint32_t idx = 0;

	tbl_data = mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_JUMP], &idx);
	if (!tbl_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	memcpy(tbl_data, oentry, sizeof(*tbl_data));
	tbl_data->idx = idx;
	return &tbl_data->entry;
}

void
flow_dv_tbl_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_tbl_data_entry *tbl_data =
		    container_of(entry, struct mlx5_flow_tbl_data_entry, entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], tbl_data->idx);
}

/**
 * Get a flow table.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] table_level
 *   Table level to use.
 * @param[in] egress
 *   Direction of the table.
 * @param[in] transfer
 *   E-Switch or NIC flow.
 * @param[in] dummy
 *   Dummy entry for dv API.
 * @param[in] table_id
 *   Table id to use.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   Returns tables resource based on the index, NULL in case of failed.
 */
struct mlx5_flow_tbl_resource *
flow_dv_tbl_resource_get(struct rte_eth_dev *dev,
			 uint32_t table_level, uint8_t egress,
			 uint8_t transfer,
			 bool external,
			 const struct mlx5_flow_tunnel *tunnel,
			 uint32_t group_id, uint8_t dummy,
			 uint32_t table_id,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	union mlx5_flow_tbl_key table_key = {
		{
			.level = table_level,
			.id = table_id,
			.reserved = 0,
			.dummy = !!dummy,
			.is_fdb = !!transfer,
			.is_egress = !!egress,
		}
	};
	struct mlx5_flow_tbl_tunnel_prm tt_prm = {
		.tunnel = tunnel,
		.group_id = group_id,
		.external = external,
	};
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &table_key.v64,
		.data2 = &tt_prm,
	};
	struct mlx5_list_entry *entry;
	struct mlx5_flow_tbl_data_entry *tbl_data;

	entry = mlx5_hlist_register(priv->sh->flow_tbls, table_key.v64, &ctx);
	if (!entry) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot get table");
		return NULL;
	}
	DRV_LOG(DEBUG, "table_level %u table_id %u "
		"tunnel %u group %u registered.",
		table_level, table_id,
		tunnel ? tunnel->tunnel_id : 0, group_id);
	tbl_data = container_of(entry, struct mlx5_flow_tbl_data_entry, entry);
	return &tbl_data->tbl;
}

void
flow_dv_tbl_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_tbl_data_entry *tbl_data =
		    container_of(entry, struct mlx5_flow_tbl_data_entry, entry);

	MLX5_ASSERT(entry && sh);
	if (tbl_data->jump.action)
		mlx5_flow_os_destroy_flow_action(tbl_data->jump.action);
	if (tbl_data->tbl.obj)
		mlx5_flow_os_destroy_flow_tbl(tbl_data->tbl.obj);
	if (tbl_data->tunnel_offload && tbl_data->external) {
		struct mlx5_list_entry *he;
		struct mlx5_hlist *tunnel_grp_hash;
		struct mlx5_flow_tunnel_hub *thub = sh->tunnel_hub;
		union tunnel_tbl_key tunnel_key = {
			.tunnel_id = tbl_data->tunnel ?
					tbl_data->tunnel->tunnel_id : 0,
			.group = tbl_data->group_id
		};
		uint32_t table_level = tbl_data->level;
		struct mlx5_flow_cb_ctx ctx = {
			.data = (void *)&tunnel_key.val,
		};

		tunnel_grp_hash = tbl_data->tunnel ?
					tbl_data->tunnel->groups :
					thub->groups;
		he = mlx5_hlist_lookup(tunnel_grp_hash, tunnel_key.val, &ctx);
		if (he)
			mlx5_hlist_unregister(tunnel_grp_hash, he);
		DRV_LOG(DEBUG,
			"table_level %u id %u tunnel %u group %u released.",
			table_level,
			tbl_data->id,
			tbl_data->tunnel ?
			tbl_data->tunnel->tunnel_id : 0,
			tbl_data->group_id);
	}
	if (tbl_data->matchers)
		mlx5_list_destroy(tbl_data->matchers);
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_JUMP], tbl_data->idx);
}

/**
 * Release a flow table.
 *
 * @param[in] sh
 *   Pointer to device shared structure.
 * @param[in] tbl
 *   Table resource to be released.
 *
 * @return
 *   Returns 0 if table was released, else return 1;
 */
int
flow_dv_tbl_resource_release(struct mlx5_dev_ctx_shared *sh,
			     struct mlx5_flow_tbl_resource *tbl)
{
	struct mlx5_flow_tbl_data_entry *tbl_data =
		container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);

	if (!tbl)
		return 0;
	return mlx5_hlist_unregister(sh->flow_tbls, &tbl_data->entry);
}

int
flow_dv_matcher_match_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_matcher *ref = ctx->data;
	struct mlx5_flow_dv_matcher *cur = container_of(entry, typeof(*cur),
							entry);

	return cur->crc != ref->crc ||
	       cur->priority != ref->priority ||
	       memcmp((const void *)cur->mask.buf,
		      (const void *)ref->mask.buf, ref->mask.size);
}

struct mlx5_list_entry *
flow_dv_matcher_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_matcher *ref = ctx->data;
	struct mlx5_flow_dv_matcher *resource;
	struct mlx5dv_flow_matcher_attr dv_attr = {
		.type = IBV_FLOW_ATTR_NORMAL,
		.match_mask = (void *)&ref->mask,
	};
	struct mlx5_flow_tbl_data_entry *tbl = container_of(ref->tbl,
							    typeof(*tbl), tbl);
	int ret;

	resource = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*resource), 0,
			       SOCKET_ID_ANY);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create matcher");
		return NULL;
	}
	*resource = *ref;
	dv_attr.match_criteria_enable =
		flow_dv_matcher_enable(resource->mask.buf);
	__flow_dv_adjust_buf_size(&ref->mask.size,
				  dv_attr.match_criteria_enable);
	dv_attr.priority = ref->priority;
	if (tbl->is_egress)
		dv_attr.flags |= IBV_FLOW_ATTR_FLAGS_EGRESS;
	ret = mlx5_flow_os_create_flow_matcher(sh->cdev->ctx, &dv_attr,
					       tbl->tbl.obj,
					       &resource->matcher_object);
	if (ret) {
		mlx5_free(resource);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create matcher");
		return NULL;
	}
	return &resource->entry;
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
			 struct mlx5_flow_dv_matcher *ref,
			 union mlx5_flow_tbl_key *key,
			 struct mlx5_flow *dev_flow,
			 const struct mlx5_flow_tunnel *tunnel,
			 uint32_t group_id,
			 struct rte_flow_error *error)
{
	struct mlx5_list_entry *entry;
	struct mlx5_flow_dv_matcher *resource;
	struct mlx5_flow_tbl_resource *tbl;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = ref,
	};
	/**
	 * tunnel offload API requires this registration for cases when
	 * tunnel match rule was inserted before tunnel set rule.
	 */
	tbl = flow_dv_tbl_resource_get(dev, key->level,
				       key->is_egress, key->is_fdb,
				       dev_flow->external, tunnel,
				       group_id, 0, key->id, error);
	if (!tbl)
		return -rte_errno;	/* No need to refill the error info */
	tbl_data = container_of(tbl, struct mlx5_flow_tbl_data_entry, tbl);
	ref->tbl = tbl;
	entry = mlx5_list_register(tbl_data->matchers, &ctx);
	if (!entry) {
		flow_dv_tbl_resource_release(MLX5_SH(dev), tbl);
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot allocate ref memory");
	}
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->handle->dvh.matcher = resource;
	return 0;
}

struct mlx5_list_entry *
flow_dv_tag_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_tag_resource *entry;
	uint32_t idx = 0;
	int ret;

	entry = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_TAG], &idx);
	if (!entry) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		return NULL;
	}
	entry->idx = idx;
	entry->tag_id = *(uint32_t *)(ctx->data);
	ret = mlx5_flow_os_create_flow_action_tag(entry->tag_id,
						  &entry->action);
	if (ret) {
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_TAG], idx);
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "cannot create action");
		return NULL;
	}
	return &entry->entry;
}

int
flow_dv_tag_match_cb(void *tool_ctx __rte_unused, struct mlx5_list_entry *entry,
		     void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_tag_resource *tag =
		   container_of(entry, struct mlx5_flow_dv_tag_resource, entry);

	return *(uint32_t *)(ctx->data) != tag->tag_id;
}

struct mlx5_list_entry *
flow_dv_tag_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
		     void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_tag_resource *entry;
	uint32_t idx = 0;

	entry = mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_TAG], &idx);
	if (!entry) {
		rte_flow_error_set(ctx->error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate tag resource memory");
		return NULL;
	}
	memcpy(entry, oentry, sizeof(*entry));
	entry->idx = idx;
	return &entry->entry;
}

void
flow_dv_tag_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_tag_resource *tag =
		   container_of(entry, struct mlx5_flow_dv_tag_resource, entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_TAG], tag->idx);
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
	struct mlx5_flow_dv_tag_resource *resource;
	struct mlx5_list_entry *entry;
	struct mlx5_flow_cb_ctx ctx = {
					.error = error,
					.data = &tag_be24,
					};
	struct mlx5_hlist *tag_table;

	tag_table = flow_dv_hlist_prepare(priv->sh, &priv->sh->tag_table,
				      "tags",
				      MLX5_TAGS_HLIST_ARRAY_SIZE,
				      false, false, priv->sh,
				      flow_dv_tag_create_cb,
				      flow_dv_tag_match_cb,
				      flow_dv_tag_remove_cb,
				      flow_dv_tag_clone_cb,
				      flow_dv_tag_clone_free_cb,
				      error);
	if (unlikely(!tag_table))
		return -rte_errno;
	entry = mlx5_hlist_register(tag_table, tag_be24, &ctx);
	if (entry) {
		resource = container_of(entry, struct mlx5_flow_dv_tag_resource,
					entry);
		dev_flow->handle->dvh.rix_tag = resource->idx;
		dev_flow->dv.tag_resource = resource;
		return 0;
	}
	return -rte_errno;
}

void
flow_dv_tag_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_tag_resource *tag =
		   container_of(entry, struct mlx5_flow_dv_tag_resource, entry);

	MLX5_ASSERT(tag && sh && tag->action);
	claim_zero(mlx5_flow_os_destroy_flow_action(tag->action));
	DRV_LOG(DEBUG, "Tag %p: removed.", (void *)tag);
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_TAG], tag->idx);
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
	struct mlx5_flow_dv_tag_resource *tag;

	tag = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_TAG], tag_idx);
	if (!tag)
		return 0;
	DRV_LOG(DEBUG, "port %u tag %p: refcnt %d--",
		dev->data->port_id, (void *)tag, tag->entry.ref_cnt);
	return mlx5_hlist_unregister(priv->sh->tag_table, &tag->entry);
}

/**
 * Translate action PORT_ID / REPRESENTED_PORT to vport.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to action PORT_ID / REPRESENTED_PORT.
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

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_PORT_ID: {
		const struct rte_flow_action_port_id *conf;

		conf = (const struct rte_flow_action_port_id *)action->conf;
		port = conf->original ? dev->data->port_id : conf->id;
		break;
	}
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT: {
		const struct rte_flow_action_ethdev *ethdev;

		ethdev = (const struct rte_flow_action_ethdev *)action->conf;
		port = ethdev->port_id;
		break;
	}
	default:
		MLX5_ASSERT(false);
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "unknown E-Switch action");
	}

	priv = mlx5_port_to_eswitch_info(port, false);
	if (!priv)
		return rte_flow_error_set(error, -rte_errno,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "No eswitch info was found for port");
#ifdef HAVE_MLX5DV_DR_CREATE_DEST_IB_PORT
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
 * @param[in] dev_flow
 *   Pointer to the mlx5_flow.
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
				const struct rte_flow_action_count *count
					__rte_unused,
				const struct rte_flow_action_age *age)
{
	uint32_t counter;
	struct mlx5_age_param *age_param;

	counter = flow_dv_counter_alloc(dev, !!age);
	if (!counter || age == NULL)
		return counter;
	age_param = flow_dv_counter_idx_get_age(dev, counter);
	age_param->context = age->context ? age->context :
		(void *)(uintptr_t)(dev_flow->flow_idx);
	age_param->timeout = age->timeout;
	age_param->port_id = dev->data->port_id;
	__atomic_store_n(&age_param->sec_since_last_hit, 0, __ATOMIC_RELAXED);
	__atomic_store_n(&age_param->state, AGE_CANDIDATE, __ATOMIC_RELAXED);
	return counter;
}

/**
 * Add SQ matcher
 *
 * @param[in, out] matcher
 *   Flow matcher.
 * @param[in, out] key
 *   Flow matcher value.
 * @param[in] item
 *   Flow pattern to translate.
 * @param[in] key_type
 *   Set flow matcher mask or value.
 */
static void
flow_dv_translate_item_sq(void *key,
			  const struct rte_flow_item *item,
			  uint32_t key_type)
{
	const struct mlx5_rte_flow_item_sq *queue_m;
	const struct mlx5_rte_flow_item_sq *queue_v;
	const struct mlx5_rte_flow_item_sq queue_mask = {
		.queue = UINT32_MAX,
	};
	void *misc_v = MLX5_ADDR_OF(fte_match_param, key, misc_parameters);
	uint32_t queue;

	MLX5_ITEM_UPDATE(item, key_type, queue_v, queue_m, &queue_mask);
	if (!queue_m || !queue_v)
		return;
	if (key_type & MLX5_SET_MATCHER_V) {
		queue = queue_v->queue;
		if (key_type == MLX5_SET_MATCHER_SW_V)
			queue &= queue_m->queue;
	} else {
		queue = queue_m->queue;
	}
	MLX5_SET(fte_match_set_misc, misc_v, source_sqn, queue);
}

/**
 * Set the hash fields according to the @p flow information.
 *
 * @param[in] item_flags
 *   The match pattern item flags.
 * @param[in] rss_desc
 *   Pointer to the mlx5_flow_rss_desc.
 * @param[out] hash_fields
 *   Pointer to the RSS hash fields.
 */
void
flow_dv_hashfields_set(uint64_t item_flags,
		       struct mlx5_flow_rss_desc *rss_desc,
		       uint64_t *hash_fields)
{
	uint64_t items = item_flags;
	uint64_t fields = 0;
	int rss_inner = 0;
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_desc->types);

	*hash_fields = 0;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (rss_desc->level >= 2)
		rss_inner = 1;
#endif
	if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L3_IPV4)) ||
	    (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L3_IPV4))) {
		if (rss_types & MLX5_IPV4_LAYER_TYPES) {
			if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
				fields |= IBV_RX_HASH_SRC_IPV4;
			else if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
				fields |= IBV_RX_HASH_DST_IPV4;
			else
				fields |= MLX5_IPV4_IBV_RX_HASH;
		}
	} else if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L3_IPV6)) ||
		   (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L3_IPV6))) {
		if (rss_types & MLX5_IPV6_LAYER_TYPES) {
			if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
				fields |= IBV_RX_HASH_SRC_IPV6;
			else if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
				fields |= IBV_RX_HASH_DST_IPV6;
			else
				fields |= MLX5_IPV6_IBV_RX_HASH;
		}
	}
	if (items & MLX5_FLOW_ITEM_ESP) {
		if (rss_types & RTE_ETH_RSS_ESP)
			fields |= IBV_RX_HASH_IPSEC_SPI;
	}
	if ((fields & ~IBV_RX_HASH_IPSEC_SPI) == 0) {
		*hash_fields = fields;
		/*
		 * There is no match between the RSS types and the
		 * L3 protocol (IPv4/IPv6) defined in the flow rule.
		 */
		return;
	}
	if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L4_UDP)) ||
	    (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L4_UDP))) {
		if (rss_types & RTE_ETH_RSS_UDP) {
			if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
				fields |= IBV_RX_HASH_SRC_PORT_UDP;
			else if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
				fields |= IBV_RX_HASH_DST_PORT_UDP;
			else
				fields |= MLX5_UDP_IBV_RX_HASH;
		}
	} else if ((rss_inner && (items & MLX5_FLOW_LAYER_INNER_L4_TCP)) ||
		   (!rss_inner && (items & MLX5_FLOW_LAYER_OUTER_L4_TCP))) {
		if (rss_types & RTE_ETH_RSS_TCP) {
			if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
				fields |= IBV_RX_HASH_SRC_PORT_TCP;
			else if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
				fields |= IBV_RX_HASH_DST_PORT_TCP;
			else
				fields |= MLX5_TCP_IBV_RX_HASH;
		}
	}
	if (rss_inner)
		fields |= IBV_RX_HASH_INNER;
	*hash_fields = fields;
}

/**
 * Prepare an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] rss_desc
 *   Pointer to the mlx5_flow_rss_desc.
 * @param[out] hrxq_idx
 *   Hash Rx queue index.
 *
 * @return
 *   The Verbs/DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_hrxq *
flow_dv_hrxq_prepare(struct rte_eth_dev *dev,
		     struct mlx5_flow *dev_flow,
		     struct mlx5_flow_rss_desc *rss_desc,
		     uint32_t *hrxq_idx)
{
	struct mlx5_flow_handle *dh = dev_flow->handle;
	uint32_t shared_rss = rss_desc->shared_rss;
	struct mlx5_hrxq *hrxq;

	MLX5_ASSERT(rss_desc->queue_num);
	rss_desc->key_len = MLX5_RSS_HASH_KEY_LEN;
	rss_desc->hash_fields = dev_flow->hash_fields;
	rss_desc->tunnel = !!(dh->layers & MLX5_FLOW_LAYER_TUNNEL);
	rss_desc->shared_rss = 0;
	if (rss_desc->hash_fields == 0)
		rss_desc->queue_num = 1;
	hrxq = mlx5_hrxq_get(dev, rss_desc);
	*hrxq_idx = hrxq ? hrxq->idx : 0;
	rss_desc->shared_rss = shared_rss;
	return hrxq;
}

/**
 * Release sample sub action resource.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] act_res
 *   Pointer to sample sub action resource.
 */
static void
flow_dv_sample_sub_actions_release(struct rte_eth_dev *dev,
				   struct mlx5_flow_sub_actions_idx *act_res)
{
	if (act_res->rix_hrxq) {
		mlx5_hrxq_release(dev, act_res->rix_hrxq);
		act_res->rix_hrxq = 0;
	}
	if (act_res->rix_encap_decap) {
		flow_dv_encap_decap_resource_release(dev,
						     act_res->rix_encap_decap);
		act_res->rix_encap_decap = 0;
	}
	if (act_res->rix_port_id_action) {
		flow_dv_port_id_action_resource_release(dev,
						act_res->rix_port_id_action);
		act_res->rix_port_id_action = 0;
	}
	if (act_res->rix_tag) {
		flow_dv_tag_release(dev, act_res->rix_tag);
		act_res->rix_tag = 0;
	}
	if (act_res->rix_jump) {
		flow_dv_jump_tbl_resource_release(dev, act_res->rix_jump);
		act_res->rix_jump = 0;
	}
}

int
flow_dv_sample_match_cb(void *tool_ctx __rte_unused,
			struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_sample_resource *ctx_resource = ctx->data;
	struct mlx5_flow_dv_sample_resource *resource = container_of(entry,
							      typeof(*resource),
							      entry);

	if (ctx_resource->ratio == resource->ratio &&
	    ctx_resource->ft_type == resource->ft_type &&
	    ctx_resource->ft_id == resource->ft_id &&
	    ctx_resource->set_action == resource->set_action &&
	    !memcmp((void *)&ctx_resource->sample_act,
		    (void *)&resource->sample_act,
		    sizeof(struct mlx5_flow_sub_actions_list))) {
		/*
		 * Existing sample action should release the prepared
		 * sub-actions reference counter.
		 */
		flow_dv_sample_sub_actions_release(dev,
						   &ctx_resource->sample_idx);
		return 0;
	}
	return 1;
}

struct mlx5_list_entry *
flow_dv_sample_create_cb(void *tool_ctx __rte_unused, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_sample_resource *ctx_resource = ctx->data;
	void **sample_dv_actions = ctx_resource->sub_actions;
	struct mlx5_flow_dv_sample_resource *resource;
	struct mlx5dv_dr_flow_sampler_attr sampler_attr;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_tbl_resource *tbl;
	uint32_t idx = 0;
	const uint32_t next_ft_step = 1;
	uint32_t next_ft_id = ctx_resource->ft_id + next_ft_step;
	uint8_t is_egress = 0;
	uint8_t is_transfer = 0;
	struct rte_flow_error *error = ctx->error;

	/* Register new sample resource. */
	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_SAMPLE], &idx);
	if (!resource) {
		rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot allocate resource memory");
		return NULL;
	}
	*resource = *ctx_resource;
	/* Create normal path table level */
	if (ctx_resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		is_transfer = 1;
	else if (ctx_resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_TX)
		is_egress = 1;
	tbl = flow_dv_tbl_resource_get(dev, next_ft_id,
					is_egress, is_transfer,
					true, NULL, 0, 0, 0, error);
	if (!tbl) {
		rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "fail to create normal path table "
					  "for sample");
		goto error;
	}
	resource->normal_path_tbl = tbl;
	if (ctx_resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB) {
		if (!sh->default_miss_action) {
			rte_flow_error_set(error, ENOMEM,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL,
						"default miss action was not "
						"created");
			goto error;
		}
		sample_dv_actions[ctx_resource->sample_act.actions_num++] =
						sh->default_miss_action;
	}
	/* Create a DR sample action */
	sampler_attr.sample_ratio = resource->ratio;
	sampler_attr.default_next_table = tbl->obj;
	sampler_attr.num_sample_actions = ctx_resource->sample_act.actions_num;
	sampler_attr.sample_actions = (struct mlx5dv_dr_action **)
							&sample_dv_actions[0];
	sampler_attr.action = resource->set_action;
	if (mlx5_os_flow_dr_create_flow_action_sampler
			(&sampler_attr, &resource->verbs_action)) {
		rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "cannot create sample action");
		goto error;
	}
	resource->idx = idx;
	resource->dev = dev;
	return &resource->entry;
error:
	if (resource->ft_type != MLX5DV_FLOW_TABLE_TYPE_FDB)
		flow_dv_sample_sub_actions_release(dev,
						   &resource->sample_idx);
	if (resource->normal_path_tbl)
		flow_dv_tbl_resource_release(MLX5_SH(dev),
				resource->normal_path_tbl);
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_SAMPLE], idx);
	return NULL;

}

struct mlx5_list_entry *
flow_dv_sample_clone_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry __rte_unused,
			 void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_sample_resource *resource;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	uint32_t idx = 0;

	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_SAMPLE], &idx);
	if (!resource) {
		rte_flow_error_set(ctx->error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot allocate resource memory");
		return NULL;
	}
	memcpy(resource, entry, sizeof(*resource));
	resource->idx = idx;
	resource->dev = dev;
	return &resource->entry;
}

void
flow_dv_sample_clone_free_cb(void *tool_ctx __rte_unused,
			     struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_sample_resource *resource =
				  container_of(entry, typeof(*resource), entry);
	struct rte_eth_dev *dev = resource->dev;
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_SAMPLE], resource->idx);
}

/**
 * Find existing sample resource or create and register a new one.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] ref
 *   Pointer to sample resource reference.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_sample_resource_register(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_sample_resource *ref,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_flow_dv_sample_resource *resource;
	struct mlx5_list_entry *entry;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = ref,
	};

	entry = mlx5_list_register(priv->sh->sample_action_list, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->handle->dvh.rix_sample = resource->idx;
	dev_flow->dv.sample_res = resource;
	return 0;
}

int
flow_dv_dest_array_match_cb(void *tool_ctx __rte_unused,
			    struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_dv_dest_array_resource *ctx_resource = ctx->data;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_dest_array_resource *resource =
				  container_of(entry, typeof(*resource), entry);
	uint32_t idx = 0;

	if (ctx_resource->num_of_dest == resource->num_of_dest &&
	    ctx_resource->ft_type == resource->ft_type &&
	    !memcmp((void *)resource->sample_act,
		    (void *)ctx_resource->sample_act,
		   (ctx_resource->num_of_dest *
		   sizeof(struct mlx5_flow_sub_actions_list)))) {
		/*
		 * Existing sample action should release the prepared
		 * sub-actions reference counter.
		 */
		for (idx = 0; idx < ctx_resource->num_of_dest; idx++)
			flow_dv_sample_sub_actions_release(dev,
					&ctx_resource->sample_idx[idx]);
		return 0;
	}
	return 1;
}

struct mlx5_list_entry *
flow_dv_dest_array_create_cb(void *tool_ctx __rte_unused, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_dest_array_resource *resource;
	struct mlx5_flow_dv_dest_array_resource *ctx_resource = ctx->data;
	struct mlx5dv_dr_action_dest_attr *dest_attr[MLX5_MAX_DEST_NUM] = { 0 };
	struct mlx5dv_dr_action_dest_reformat dest_reformat[MLX5_MAX_DEST_NUM];
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_flow_sub_actions_list *sample_act;
	struct mlx5dv_dr_domain *domain;
	uint32_t idx = 0, res_idx = 0;
	struct rte_flow_error *error = ctx->error;
	uint64_t action_flags;
	int ret;

	/* Register new destination array resource. */
	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_DEST_ARRAY],
					    &res_idx);
	if (!resource) {
		rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot allocate resource memory");
		return NULL;
	}
	*resource = *ctx_resource;
	if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_FDB)
		domain = sh->fdb_domain;
	else if (resource->ft_type == MLX5DV_FLOW_TABLE_TYPE_NIC_RX)
		domain = sh->rx_domain;
	else
		domain = sh->tx_domain;
	for (idx = 0; idx < ctx_resource->num_of_dest; idx++) {
		dest_attr[idx] = (struct mlx5dv_dr_action_dest_attr *)
				 mlx5_malloc(MLX5_MEM_ZERO,
				 sizeof(struct mlx5dv_dr_action_dest_attr),
				 0, SOCKET_ID_ANY);
		if (!dest_attr[idx]) {
			rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "cannot allocate resource memory");
			goto error;
		}
		dest_attr[idx]->type = MLX5DV_DR_ACTION_DEST;
		sample_act = &ctx_resource->sample_act[idx];
		action_flags = sample_act->action_flags;
		switch (action_flags) {
		case MLX5_FLOW_ACTION_QUEUE:
			dest_attr[idx]->dest = sample_act->dr_queue_action;
			break;
		case (MLX5_FLOW_ACTION_PORT_ID | MLX5_FLOW_ACTION_ENCAP):
			dest_attr[idx]->type = MLX5DV_DR_ACTION_DEST_REFORMAT;
			dest_attr[idx]->dest_reformat = &dest_reformat[idx];
			dest_attr[idx]->dest_reformat->reformat =
					sample_act->dr_encap_action;
			dest_attr[idx]->dest_reformat->dest =
					sample_act->dr_port_id_action;
			break;
		case MLX5_FLOW_ACTION_PORT_ID:
			dest_attr[idx]->dest = sample_act->dr_port_id_action;
			break;
		case MLX5_FLOW_ACTION_JUMP:
			dest_attr[idx]->dest = sample_act->dr_jump_action;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   NULL,
					   "unsupported actions type");
			goto error;
		}
	}
	/* create a dest array action */
	ret = mlx5_os_flow_dr_create_flow_action_dest_array
						(domain,
						 resource->num_of_dest,
						 dest_attr,
						 &resource->action);
	if (ret) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot create destination array action");
		goto error;
	}
	resource->idx = res_idx;
	resource->dev = dev;
	for (idx = 0; idx < ctx_resource->num_of_dest; idx++)
		mlx5_free(dest_attr[idx]);
	return &resource->entry;
error:
	for (idx = 0; idx < ctx_resource->num_of_dest; idx++) {
		flow_dv_sample_sub_actions_release(dev,
						   &resource->sample_idx[idx]);
		if (dest_attr[idx])
			mlx5_free(dest_attr[idx]);
	}
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_DEST_ARRAY], res_idx);
	return NULL;
}

struct mlx5_list_entry *
flow_dv_dest_array_clone_cb(void *tool_ctx __rte_unused,
			    struct mlx5_list_entry *entry __rte_unused,
			    void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct mlx5_flow_dv_dest_array_resource *resource;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	uint32_t res_idx = 0;
	struct rte_flow_error *error = ctx->error;

	resource = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_DEST_ARRAY],
				      &res_idx);
	if (!resource) {
		rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot allocate dest-array memory");
		return NULL;
	}
	memcpy(resource, entry, sizeof(*resource));
	resource->idx = res_idx;
	resource->dev = dev;
	return &resource->entry;
}

void
flow_dv_dest_array_clone_free_cb(void *tool_ctx __rte_unused,
				 struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_dest_array_resource *resource =
			container_of(entry, typeof(*resource), entry);
	struct rte_eth_dev *dev = resource->dev;
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_DEST_ARRAY], resource->idx);
}

/**
 * Find existing destination array resource or create and register a new one.
 *
 * @param[in, out] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] ref
 *   Pointer to destination array resource reference.
 * @parm[in, out] dev_flow
 *   Pointer to the dev_flow.
 * @param[out] error
 *   pointer to error structure.
 *
 * @return
 *   0 on success otherwise -errno and errno is set.
 */
static int
flow_dv_dest_array_resource_register(struct rte_eth_dev *dev,
			 struct mlx5_flow_dv_dest_array_resource *ref,
			 struct mlx5_flow *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_flow_dv_dest_array_resource *resource;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_list_entry *entry;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = ref,
	};

	entry = mlx5_list_register(priv->sh->dest_array_list, &ctx);
	if (!entry)
		return -rte_errno;
	resource = container_of(entry, typeof(*resource), entry);
	dev_flow->handle->dvh.rix_dest_array = resource->idx;
	dev_flow->dv.dest_array_res = resource;
	return 0;
}

/**
 * Convert Sample action to DV specification.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to sample action structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in, out] num_of_dest
 *   Pointer to the num of destination.
 * @param[in, out] sample_actions
 *   Pointer to sample actions list.
 * @param[in, out] res
 *   Pointer to sample resource.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_translate_action_sample(struct rte_eth_dev *dev,
				const struct rte_flow_action_sample *action,
				struct mlx5_flow *dev_flow,
				const struct rte_flow_attr *attr,
				uint32_t *num_of_dest,
				void **sample_actions,
				struct mlx5_flow_dv_sample_resource *res,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action *sub_actions;
	struct mlx5_flow_sub_actions_list *sample_act;
	struct mlx5_flow_sub_actions_idx *sample_idx;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct rte_flow *flow = dev_flow->flow;
	struct mlx5_flow_rss_desc *rss_desc;
	uint64_t action_flags = 0;

	MLX5_ASSERT(wks);
	rss_desc = &wks->rss_desc;
	sample_act = &res->sample_act;
	sample_idx = &res->sample_idx;
	res->ratio = action->ratio;
	sub_actions = action->actions;
	for (; sub_actions->type != RTE_FLOW_ACTION_TYPE_END; sub_actions++) {
		int type = sub_actions->type;
		uint32_t pre_rix = 0;
		void *pre_r;
		switch (type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		{
			const struct rte_flow_action_queue *queue;
			struct mlx5_hrxq *hrxq;
			uint32_t hrxq_idx;

			queue = sub_actions->conf;
			rss_desc->queue_num = 1;
			rss_desc->queue[0] = queue->index;
			hrxq = flow_dv_hrxq_prepare(dev, dev_flow,
						    rss_desc, &hrxq_idx);
			if (!hrxq)
				return rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL,
					 "cannot create fate queue");
			sample_act->dr_queue_action = hrxq->action;
			sample_idx->rix_hrxq = hrxq_idx;
			sample_actions[sample_act->actions_num++] =
						hrxq->action;
			(*num_of_dest)++;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			if (action_flags & MLX5_FLOW_ACTION_MARK)
				dev_flow->handle->rix_hrxq = hrxq_idx;
			dev_flow->handle->fate_action =
					MLX5_FLOW_FATE_QUEUE;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_RSS:
		{
			struct mlx5_hrxq *hrxq;
			uint32_t hrxq_idx;
			const struct rte_flow_action_rss *rss;
			const uint8_t *rss_key;

			rss = sub_actions->conf;
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
			flow_dv_hashfields_set(dev_flow->handle->layers,
					       rss_desc,
					       &dev_flow->hash_fields);
			hrxq = flow_dv_hrxq_prepare(dev, dev_flow,
						    rss_desc, &hrxq_idx);
			if (!hrxq)
				return rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL,
					 "cannot create fate queue");
			sample_act->dr_queue_action = hrxq->action;
			sample_idx->rix_hrxq = hrxq_idx;
			sample_actions[sample_act->actions_num++] =
						hrxq->action;
			(*num_of_dest)++;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			if (action_flags & MLX5_FLOW_ACTION_MARK)
				dev_flow->handle->rix_hrxq = hrxq_idx;
			dev_flow->handle->fate_action =
					MLX5_FLOW_FATE_QUEUE;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_MARK:
		{
			uint32_t tag_be = mlx5_flow_mark_set
				(((const struct rte_flow_action_mark *)
				(sub_actions->conf))->id);

			wks->mark = 1;
			pre_rix = dev_flow->handle->dvh.rix_tag;
			/* Save the mark resource before sample */
			pre_r = dev_flow->dv.tag_resource;
			if (flow_dv_tag_resource_register(dev, tag_be,
						  dev_flow, error))
				return -rte_errno;
			MLX5_ASSERT(dev_flow->dv.tag_resource);
			sample_act->dr_tag_action =
				dev_flow->dv.tag_resource->action;
			sample_idx->rix_tag =
				dev_flow->handle->dvh.rix_tag;
			sample_actions[sample_act->actions_num++] =
						sample_act->dr_tag_action;
			/* Recover the mark resource after sample */
			dev_flow->dv.tag_resource = pre_r;
			dev_flow->handle->dvh.rix_tag = pre_rix;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_COUNT:
		{
			if (!flow->counter) {
				flow->counter =
					flow_dv_translate_create_counter(dev,
						dev_flow, sub_actions->conf,
						0);
				if (!flow->counter)
					return rte_flow_error_set
						(error, rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"cannot create counter"
						" object.");
			}
			sample_act->dr_cnt_action =
				  (flow_dv_counter_get_by_idx(dev,
				  flow->counter, NULL))->action;
			sample_actions[sample_act->actions_num++] =
						sample_act->dr_cnt_action;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		{
			struct mlx5_flow_dv_port_id_action_resource
					port_id_resource;
			uint32_t port_id = 0;

			memset(&port_id_resource, 0, sizeof(port_id_resource));
			/* Save the port id resource before sample */
			pre_rix = dev_flow->handle->rix_port_id_action;
			pre_r = dev_flow->dv.port_id_action;
			if (flow_dv_translate_action_port_id(dev, sub_actions,
							     &port_id, error))
				return -rte_errno;
			port_id_resource.port_id = port_id;
			if (flow_dv_port_id_action_resource_register
			    (dev, &port_id_resource, dev_flow, error))
				return -rte_errno;
			sample_act->dr_port_id_action =
				dev_flow->dv.port_id_action->action;
			sample_idx->rix_port_id_action =
				dev_flow->handle->rix_port_id_action;
			sample_actions[sample_act->actions_num++] =
						sample_act->dr_port_id_action;
			/* Recover the port id resource after sample */
			dev_flow->dv.port_id_action = pre_r;
			dev_flow->handle->rix_port_id_action = pre_rix;
			(*num_of_dest)++;
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			/* Save the encap resource before sample */
			pre_rix = dev_flow->handle->dvh.rix_encap_decap;
			pre_r = dev_flow->dv.encap_decap;
			if (flow_dv_create_action_l2_encap(dev, sub_actions,
							   dev_flow,
							   attr->transfer,
							   error))
				return -rte_errno;
			sample_act->dr_encap_action =
				dev_flow->dv.encap_decap->action;
			sample_idx->rix_encap_decap =
				dev_flow->handle->dvh.rix_encap_decap;
			sample_actions[sample_act->actions_num++] =
						sample_act->dr_encap_action;
			/* Recover the encap resource after sample */
			dev_flow->dv.encap_decap = pre_r;
			dev_flow->handle->dvh.rix_encap_decap = pre_rix;
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Not support for sampler action");
		}
	}
	sample_act->action_flags = action_flags;
	res->ft_id = dev_flow->dv.group;
	if (attr->transfer) {
		union {
			uint32_t action_in[MLX5_ST_SZ_DW(set_action_in)];
			uint64_t set_action;
		} action_ctx = { .set_action = 0 };
		uint32_t vport_meta_tag = wks->vport_meta_tag ?
					  wks->vport_meta_tag :
					  priv->vport_meta_tag;

		res->ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
		MLX5_SET(set_action_in, action_ctx.action_in, action_type,
			 MLX5_MODIFICATION_TYPE_SET);
		MLX5_SET(set_action_in, action_ctx.action_in, field,
			 MLX5_MODI_META_REG_C_0);
		MLX5_SET(set_action_in, action_ctx.action_in, data,
			 vport_meta_tag);
		res->set_action = action_ctx.set_action;
	} else if (attr->ingress) {
		res->ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	} else {
		res->ft_type = MLX5DV_FLOW_TABLE_TYPE_NIC_TX;
	}
	return 0;
}

static void *
flow_dv_translate_action_send_to_kernel(struct rte_eth_dev *dev,
					struct rte_flow_error *error)
{
	struct mlx5_flow_tbl_resource *tbl;
	struct mlx5_dev_ctx_shared *sh;
	uint32_t priority;
	void *action;
	int ret;

	sh = MLX5_SH(dev);
	if (sh->send_to_kernel_action.action)
		return sh->send_to_kernel_action.action;
	priority = mlx5_get_send_to_kernel_priority(dev);
	if (priority == (uint32_t)-1) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "required priority is not available");
		return NULL;
	}
	tbl = flow_dv_tbl_resource_get(dev, 0, 0, 0, false, NULL, 0, 0, 0,
				       error);
	if (!tbl) {
		rte_flow_error_set(error, ENODATA,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot find destination root table");
		return NULL;
	}
	ret = mlx5_flow_os_create_flow_action_send_to_kernel(tbl->obj,
				priority, &action);
	if (ret) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot create action");
		goto err;
	}
	MLX5_ASSERT(action);
	sh->send_to_kernel_action.action = action;
	sh->send_to_kernel_action.tbl = tbl;
	return action;
err:
	flow_dv_tbl_resource_release(sh, tbl);
	return NULL;
}

/**
 * Convert Sample action to DV specification.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in] num_of_dest
 *   The num of destination.
 * @param[in, out] res
 *   Pointer to sample resource.
 * @param[in, out] mdest_res
 *   Pointer to destination array resource.
 * @param[in] sample_actions
 *   Pointer to sample path actions list.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_create_action_sample(struct rte_eth_dev *dev,
			     struct mlx5_flow *dev_flow,
			     uint32_t num_of_dest,
			     struct mlx5_flow_dv_sample_resource *res,
			     struct mlx5_flow_dv_dest_array_resource *mdest_res,
			     void **sample_actions,
			     uint64_t action_flags,
			     struct rte_flow_error *error)
{
	/* update normal path action resource into last index of array */
	uint32_t dest_index = MLX5_MAX_DEST_NUM - 1;
	struct mlx5_flow_sub_actions_list *sample_act =
					&mdest_res->sample_act[dest_index];
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_flow_rss_desc *rss_desc;
	uint32_t normal_idx = 0;
	struct mlx5_hrxq *hrxq;
	uint32_t hrxq_idx;

	MLX5_ASSERT(wks);
	rss_desc = &wks->rss_desc;
	if (num_of_dest > 1) {
		if (sample_act->action_flags & MLX5_FLOW_ACTION_QUEUE) {
			/* Handle QP action for mirroring */
			hrxq = flow_dv_hrxq_prepare(dev, dev_flow,
						    rss_desc, &hrxq_idx);
			if (!hrxq)
				return rte_flow_error_set
				     (error, rte_errno,
				      RTE_FLOW_ERROR_TYPE_ACTION,
				      NULL,
				      "cannot create rx queue");
			normal_idx++;
			mdest_res->sample_idx[dest_index].rix_hrxq = hrxq_idx;
			sample_act->dr_queue_action = hrxq->action;
			if (action_flags & MLX5_FLOW_ACTION_MARK)
				dev_flow->handle->rix_hrxq = hrxq_idx;
			dev_flow->handle->fate_action = MLX5_FLOW_FATE_QUEUE;
		}
		if (sample_act->action_flags & MLX5_FLOW_ACTION_ENCAP) {
			normal_idx++;
			mdest_res->sample_idx[dest_index].rix_encap_decap =
				dev_flow->handle->dvh.rix_encap_decap;
			sample_act->dr_encap_action =
				dev_flow->dv.encap_decap->action;
			dev_flow->handle->dvh.rix_encap_decap = 0;
		}
		if (sample_act->action_flags & MLX5_FLOW_ACTION_PORT_ID) {
			normal_idx++;
			mdest_res->sample_idx[dest_index].rix_port_id_action =
				dev_flow->handle->rix_port_id_action;
			sample_act->dr_port_id_action =
				dev_flow->dv.port_id_action->action;
			dev_flow->handle->rix_port_id_action = 0;
		}
		if (sample_act->action_flags & MLX5_FLOW_ACTION_JUMP) {
			normal_idx++;
			mdest_res->sample_idx[dest_index].rix_jump =
				dev_flow->handle->rix_jump;
			sample_act->dr_jump_action =
				dev_flow->dv.jump->action;
			dev_flow->handle->rix_jump = 0;
		}
		sample_act->actions_num = normal_idx;
		/* update sample action resource into first index of array */
		mdest_res->ft_type = res->ft_type;
		memcpy(&mdest_res->sample_idx[0], &res->sample_idx,
				sizeof(struct mlx5_flow_sub_actions_idx));
		memcpy(&mdest_res->sample_act[0], &res->sample_act,
				sizeof(struct mlx5_flow_sub_actions_list));
		mdest_res->num_of_dest = num_of_dest;
		if (flow_dv_dest_array_resource_register(dev, mdest_res,
							 dev_flow, error))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "can't create sample "
						  "action");
	} else {
		res->sub_actions = sample_actions;
		if (flow_dv_sample_resource_register(dev, res, dev_flow, error))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "can't create sample action");
	}
	return 0;
}

/**
 * Remove an ASO age action from age actions list.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] age
 *   Pointer to the aso age action handler.
 */
static void
flow_dv_aso_age_remove_from_age(struct rte_eth_dev *dev,
				struct mlx5_aso_age_action *age)
{
	struct mlx5_age_info *age_info;
	struct mlx5_age_param *age_param = &age->age_params;
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t expected = AGE_CANDIDATE;

	age_info = GET_PORT_AGE_INFO(priv);
	if (!__atomic_compare_exchange_n(&age_param->state, &expected,
					 AGE_FREE, false, __ATOMIC_RELAXED,
					 __ATOMIC_RELAXED)) {
		/**
		 * We need the lock even it is age timeout,
		 * since age action may still in process.
		 */
		rte_spinlock_lock(&age_info->aged_sl);
		LIST_REMOVE(age, next);
		rte_spinlock_unlock(&age_info->aged_sl);
		__atomic_store_n(&age_param->state, AGE_FREE, __ATOMIC_RELAXED);
	}
}

/**
 * Release an ASO age action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] age_idx
 *   Index of ASO age action to release.
 * @param[in] flow
 *   True if the release operation is during flow destroy operation.
 *   False if the release operation is during action destroy operation.
 *
 * @return
 *   0 when age action was removed, otherwise the number of references.
 */
static int
flow_dv_aso_age_release(struct rte_eth_dev *dev, uint32_t age_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_age_mng *mng = priv->sh->aso_age_mng;
	struct mlx5_aso_age_action *age = flow_aso_age_get_by_idx(dev, age_idx);
	uint32_t ret = __atomic_fetch_sub(&age->refcnt, 1, __ATOMIC_RELAXED) - 1;

	if (!ret) {
		flow_dv_aso_age_remove_from_age(dev, age);
		rte_spinlock_lock(&mng->free_sl);
		LIST_INSERT_HEAD(&mng->free, age, next);
		rte_spinlock_unlock(&mng->free_sl);
	}
	return ret;
}

/**
 * Resize the ASO age pools array by MLX5_ASO_AGE_CONTAINER_RESIZE pools.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   0 on success, otherwise negative errno value and rte_errno is set.
 */
static int
flow_dv_aso_age_pools_resize(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_age_mng *mng = priv->sh->aso_age_mng;
	void *old_pools = mng->pools;
	uint32_t resize = mng->n + MLX5_ASO_AGE_CONTAINER_RESIZE;
	uint32_t mem_size = sizeof(struct mlx5_aso_age_pool *) * resize;
	void *pools = mlx5_malloc(MLX5_MEM_ZERO, mem_size, 0, SOCKET_ID_ANY);

	if (!pools) {
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	if (old_pools) {
		memcpy(pools, old_pools,
		       mng->n * sizeof(struct mlx5_flow_counter_pool *));
		mlx5_free(old_pools);
	} else {
		/* First ASO flow hit allocation - starting ASO data-path. */
		int ret = mlx5_aso_flow_hit_queue_poll_start(priv->sh);

		if (ret) {
			mlx5_free(pools);
			return ret;
		}
	}
	mng->n = resize;
	mng->pools = pools;
	return 0;
}

/**
 * Create and initialize a new ASO aging pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] age_free
 *   Where to put the pointer of a new age action.
 *
 * @return
 *   The age actions pool pointer and @p age_free is set on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_aso_age_pool *
flow_dv_age_pool_create(struct rte_eth_dev *dev,
			struct mlx5_aso_age_action **age_free)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_age_mng *mng = priv->sh->aso_age_mng;
	struct mlx5_aso_age_pool *pool = NULL;
	struct mlx5_devx_obj *obj = NULL;
	uint32_t i;

	obj = mlx5_devx_cmd_create_flow_hit_aso_obj(priv->sh->cdev->ctx,
						    priv->sh->cdev->pdn);
	if (!obj) {
		rte_errno = ENODATA;
		DRV_LOG(ERR, "Failed to create flow_hit_aso_obj using DevX.");
		return NULL;
	}
	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool), 0, SOCKET_ID_ANY);
	if (!pool) {
		claim_zero(mlx5_devx_cmd_destroy(obj));
		rte_errno = ENOMEM;
		return NULL;
	}
	pool->flow_hit_aso_obj = obj;
	pool->time_of_last_age_check = MLX5_CURR_TIME_SEC;
	rte_rwlock_write_lock(&mng->resize_rwl);
	pool->index = mng->next;
	/* Resize pools array if there is no room for the new pool in it. */
	if (pool->index == mng->n && flow_dv_aso_age_pools_resize(dev)) {
		claim_zero(mlx5_devx_cmd_destroy(obj));
		mlx5_free(pool);
		rte_rwlock_write_unlock(&mng->resize_rwl);
		return NULL;
	}
	mng->pools[pool->index] = pool;
	mng->next++;
	rte_rwlock_write_unlock(&mng->resize_rwl);
	/* Assign the first action in the new pool, the rest go to free list. */
	*age_free = &pool->actions[0];
	for (i = 1; i < MLX5_ASO_AGE_ACTIONS_PER_POOL; i++) {
		pool->actions[i].offset = i;
		LIST_INSERT_HEAD(&mng->free, &pool->actions[i], next);
	}
	return pool;
}

/**
 * Allocate a ASO aging bit.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Index to ASO age action on success, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_dv_aso_age_alloc(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct mlx5_aso_age_pool *pool;
	struct mlx5_aso_age_action *age_free = NULL;
	struct mlx5_aso_age_mng *mng = priv->sh->aso_age_mng;

	MLX5_ASSERT(mng);
	/* Try to get the next free age action bit. */
	rte_spinlock_lock(&mng->free_sl);
	age_free = LIST_FIRST(&mng->free);
	if (age_free) {
		LIST_REMOVE(age_free, next);
	} else if (!flow_dv_age_pool_create(dev, &age_free)) {
		rte_spinlock_unlock(&mng->free_sl);
		rte_flow_error_set(error, rte_errno, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "failed to create ASO age pool");
		return 0; /* 0 is an error. */
	}
	rte_spinlock_unlock(&mng->free_sl);
	pool = container_of
	  ((const struct mlx5_aso_age_action (*)[MLX5_ASO_AGE_ACTIONS_PER_POOL])
		  (age_free - age_free->offset), const struct mlx5_aso_age_pool,
								       actions);
	if (!age_free->dr_action) {
		int reg_c = mlx5_flow_get_reg_id(dev, MLX5_ASO_FLOW_HIT, 0,
						 error);

		if (reg_c < 0) {
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   NULL, "failed to get reg_c "
					   "for ASO flow hit");
			return 0; /* 0 is an error. */
		}
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
		age_free->dr_action = mlx5_glue->dv_create_flow_action_aso
				(priv->sh->rx_domain,
				 pool->flow_hit_aso_obj->obj, age_free->offset,
				 MLX5DV_DR_ACTION_FLAGS_ASO_FIRST_HIT_SET,
				 (reg_c - REG_C_0));
#endif /* HAVE_MLX5_DR_CREATE_ACTION_ASO */
		if (!age_free->dr_action) {
			rte_errno = errno;
			rte_spinlock_lock(&mng->free_sl);
			LIST_INSERT_HEAD(&mng->free, age_free, next);
			rte_spinlock_unlock(&mng->free_sl);
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   NULL, "failed to create ASO "
					   "flow hit action");
			return 0; /* 0 is an error. */
		}
	}
	__atomic_store_n(&age_free->refcnt, 1, __ATOMIC_RELAXED);
	return pool->index | ((age_free->offset + 1) << 16);
}

/**
 * Initialize flow ASO age parameters.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] age_idx
 *   Index of ASO age action.
 * @param[in] context
 *   Pointer to flow counter age context.
 * @param[in] timeout
 *   Aging timeout in seconds.
 *
 */
static void
flow_dv_aso_age_params_init(struct rte_eth_dev *dev,
			    uint32_t age_idx,
			    void *context,
			    uint32_t timeout)
{
	struct mlx5_aso_age_action *aso_age;

	aso_age = flow_aso_age_get_by_idx(dev, age_idx);
	MLX5_ASSERT(aso_age);
	aso_age->age_params.context = context;
	aso_age->age_params.timeout = timeout;
	aso_age->age_params.port_id = dev->data->port_id;
	__atomic_store_n(&aso_age->age_params.sec_since_last_hit, 0,
			 __ATOMIC_RELAXED);
	__atomic_store_n(&aso_age->age_params.state, AGE_CANDIDATE,
			 __ATOMIC_RELAXED);
}

static void
flow_dv_translate_integrity_l4(const struct rte_flow_item_integrity *mask,
			       void *headers)
{
	/*
	 * In HWS mode MLX5_ITEM_UPDATE() macro assigns the same pointer to
	 * both mask and value, therefore ether can be used.
	 * In SWS SW_V mode mask points to item mask and value points to item
	 * spec. Integrity item value is used only if matching mask is set.
	 * Use mask reference here to keep SWS functionality.
	 */
	if (mask->l4_ok) {
		/* RTE l4_ok filter aggregates hardware l4_ok and
		 * l4_checksum_ok filters.
		 * Positive RTE l4_ok match requires hardware match on both L4
		 * hardware integrity bits.
		 * PMD supports positive integrity item semantics only.
		 */
		MLX5_SET(fte_match_set_lyr_2_4, headers, l4_ok, 1);
		MLX5_SET(fte_match_set_lyr_2_4, headers, l4_checksum_ok, 1);
	} else if (mask->l4_csum_ok) {
		MLX5_SET(fte_match_set_lyr_2_4, headers, l4_checksum_ok, 1);
	}
}

static void
flow_dv_translate_integrity_l3(const struct rte_flow_item_integrity *mask,
			       void *headers, bool is_ipv4)
{
	/*
	 * In HWS mode MLX5_ITEM_UPDATE() macro assigns the same pointer to
	 * both mask and value, therefore ether can be used.
	 * In SWS SW_V mode mask points to item mask and value points to item
	 * spec. Integrity item value used only if matching mask is set.
	 * Use mask reference here to keep SWS functionality.
	 */
	if (mask->l3_ok) {
		/* RTE l3_ok filter aggregates for IPv4 hardware l3_ok and
		 * ipv4_csum_ok filters.
		 * Positive RTE l3_ok match requires hardware match on both L3
		 * hardware integrity bits.
		 * PMD supports positive integrity item semantics only.
		 */
		MLX5_SET(fte_match_set_lyr_2_4, headers, l3_ok, 1);
		if (is_ipv4) {
			MLX5_SET(fte_match_set_lyr_2_4, headers,
				 ipv4_checksum_ok, 1);
		}
	} else if (is_ipv4 && mask->ipv4_csum_ok) {
		MLX5_SET(fte_match_set_lyr_2_4, headers, ipv4_checksum_ok, 1);
	}
}

static void
set_integrity_bits(void *headers, const struct rte_flow_item *integrity_item,
		   bool is_l3_ip4, uint32_t key_type)
{
	const struct rte_flow_item_integrity *spec;
	const struct rte_flow_item_integrity *mask;

	/* Integrity bits validation cleared spec pointer */
	if (MLX5_ITEM_VALID(integrity_item, key_type))
		return;
	MLX5_ITEM_UPDATE(integrity_item, key_type, spec, mask,
			 &rte_flow_item_integrity_mask);
	flow_dv_translate_integrity_l3(mask, headers, is_l3_ip4);
	flow_dv_translate_integrity_l4(mask, headers);
}

static void
flow_dv_translate_item_integrity_post(void *key,
				      const
				      struct rte_flow_item *integrity_items[2],
				      uint64_t pattern_flags, uint32_t key_type)
{
	void *headers;
	bool is_l3_ip4;

	if (pattern_flags & MLX5_FLOW_ITEM_INNER_INTEGRITY) {
		headers = MLX5_ADDR_OF(fte_match_param, key, inner_headers);
		is_l3_ip4 = (pattern_flags & MLX5_FLOW_LAYER_INNER_L3_IPV4) !=
			    0;
		set_integrity_bits(headers, integrity_items[1], is_l3_ip4,
				   key_type);
	}
	if (pattern_flags & MLX5_FLOW_ITEM_OUTER_INTEGRITY) {
		headers = MLX5_ADDR_OF(fte_match_param, key, outer_headers);
		is_l3_ip4 = (pattern_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4) !=
			    0;
		set_integrity_bits(headers, integrity_items[0], is_l3_ip4,
				   key_type);
	}
}

static uint64_t
flow_dv_translate_item_integrity(const struct rte_flow_item *item,
				 struct mlx5_dv_matcher_workspace *wks,
				 uint64_t key_type)
{
	if ((key_type & MLX5_SET_MATCHER_SW) != 0) {
		const struct rte_flow_item_integrity
			*spec = (typeof(spec))item->spec;

		/* SWS integrity bits validation cleared spec pointer */
		if (spec->level > 1) {
			wks->integrity_items[1] = item;
			wks->last_item |= MLX5_FLOW_ITEM_INNER_INTEGRITY;
		} else {
			wks->integrity_items[0] = item;
			wks->last_item |= MLX5_FLOW_ITEM_OUTER_INTEGRITY;
		}
	} else {
		/* HWS supports outer integrity only */
		wks->integrity_items[0] = item;
		wks->last_item |= MLX5_FLOW_ITEM_OUTER_INTEGRITY;
	}
	return wks->last_item;
}

/**
 * Prepares DV flow counter with aging configuration.
 * Gets it by index when exists, creates a new one when doesn't.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] dev_flow
 *   Pointer to the mlx5_flow.
 * @param[in, out] flow
 *   Pointer to the sub flow.
 * @param[in] count
 *   Pointer to the counter action configuration.
 * @param[in] age
 *   Pointer to the aging action configuration.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Pointer to the counter, NULL otherwise.
 */
static struct mlx5_flow_counter *
flow_dv_prepare_counter(struct rte_eth_dev *dev,
			struct mlx5_flow *dev_flow,
			struct rte_flow *flow,
			const struct rte_flow_action_count *count,
			const struct rte_flow_action_age *age,
			struct rte_flow_error *error)
{
	if (!flow->counter) {
		flow->counter = flow_dv_translate_create_counter(dev, dev_flow,
								 count, age);
		if (!flow->counter) {
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "cannot create counter object.");
			return NULL;
		}
	}
	return flow_dv_counter_get_by_idx(dev, flow->counter, NULL);
}

/*
 * Release an ASO CT action by its own device.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   Index of ASO CT action to release.
 *
 * @return
 *   0 when CT action was removed, otherwise the number of references.
 */
static inline int
flow_dv_aso_ct_dev_release(struct rte_eth_dev *dev, uint32_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pools_mng *mng = priv->sh->ct_mng;
	uint32_t ret;
	struct mlx5_aso_ct_action *ct = flow_aso_ct_get_by_dev_idx(dev, idx);
	enum mlx5_aso_ct_state state =
			__atomic_load_n(&ct->state, __ATOMIC_RELAXED);

	/* Cannot release when CT is in the ASO SQ. */
	if (state == ASO_CONNTRACK_WAIT || state == ASO_CONNTRACK_QUERY)
		return -1;
	ret = __atomic_fetch_sub(&ct->refcnt, 1, __ATOMIC_RELAXED) - 1;
	if (!ret) {
		if (ct->dr_action_orig) {
#ifdef HAVE_MLX5_DR_ACTION_ASO_CT
			claim_zero(mlx5_glue->destroy_flow_action
					(ct->dr_action_orig));
#endif
			ct->dr_action_orig = NULL;
		}
		if (ct->dr_action_rply) {
#ifdef HAVE_MLX5_DR_ACTION_ASO_CT
			claim_zero(mlx5_glue->destroy_flow_action
					(ct->dr_action_rply));
#endif
			ct->dr_action_rply = NULL;
		}
		/* Clear the state to free, no need in 1st allocation. */
		MLX5_ASO_CT_UPDATE_STATE(ct, ASO_CONNTRACK_FREE);
		rte_spinlock_lock(&mng->ct_sl);
		LIST_INSERT_HEAD(&mng->free_cts, ct, next);
		rte_spinlock_unlock(&mng->ct_sl);
	}
	return (int)ret;
}

static inline int
flow_dv_aso_ct_release(struct rte_eth_dev *dev, uint32_t own_idx,
		       struct rte_flow_error *error)
{
	uint16_t owner = (uint16_t)MLX5_INDIRECT_ACT_CT_GET_OWNER(own_idx);
	uint32_t idx = MLX5_INDIRECT_ACT_CT_GET_IDX(own_idx);
	struct rte_eth_dev *owndev = &rte_eth_devices[owner];
	int ret;

	MLX5_ASSERT(owner < RTE_MAX_ETHPORTS);
	if (dev->data->dev_started != 1)
		return rte_flow_error_set(error, EAGAIN,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "Indirect CT action cannot be destroyed when the port is stopped");
	ret = flow_dv_aso_ct_dev_release(owndev, idx);
	if (ret < 0)
		return rte_flow_error_set(error, EAGAIN,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "Current state prevents indirect CT action from being destroyed");
	return ret;
}

/*
 * Resize the ASO CT pools array by 64 pools.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   0 on success, otherwise negative errno value and rte_errno is set.
 */
static int
flow_dv_aso_ct_pools_resize(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pools_mng *mng = priv->sh->ct_mng;
	void *old_pools = mng->pools;
	/* Magic number now, need a macro. */
	uint32_t resize = mng->n + 64;
	uint32_t mem_size = sizeof(struct mlx5_aso_ct_pool *) * resize;
	void *pools = mlx5_malloc(MLX5_MEM_ZERO, mem_size, 0, SOCKET_ID_ANY);

	if (!pools) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	rte_rwlock_write_lock(&mng->resize_rwl);
	/* ASO SQ/QP was already initialized in the startup. */
	if (old_pools) {
		/* Realloc could be an alternative choice. */
		rte_memcpy(pools, old_pools,
			   mng->n * sizeof(struct mlx5_aso_ct_pool *));
		mlx5_free(old_pools);
	}
	mng->n = resize;
	mng->pools = pools;
	rte_rwlock_write_unlock(&mng->resize_rwl);
	return 0;
}

/*
 * Create and initialize a new ASO CT pool.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] ct_free
 *   Where to put the pointer of a new CT action.
 *
 * @return
 *   The CT actions pool pointer and @p ct_free is set on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_aso_ct_pool *
flow_dv_ct_pool_create(struct rte_eth_dev *dev,
		       struct mlx5_aso_ct_action **ct_free)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pools_mng *mng = priv->sh->ct_mng;
	struct mlx5_aso_ct_pool *pool = NULL;
	struct mlx5_devx_obj *obj = NULL;
	uint32_t i;
	uint32_t log_obj_size = rte_log2_u32(MLX5_ASO_CT_ACTIONS_PER_POOL);
	size_t mem_size;

	obj = mlx5_devx_cmd_create_conn_track_offload_obj(priv->sh->cdev->ctx,
							  priv->sh->cdev->pdn,
							  log_obj_size);
	if (!obj) {
		rte_errno = ENODATA;
		DRV_LOG(ERR, "Failed to create conn_track_offload_obj using DevX.");
		return NULL;
	}
	mem_size = sizeof(struct mlx5_aso_ct_action) *
		   MLX5_ASO_CT_ACTIONS_PER_POOL +
		   sizeof(*pool);
	pool = mlx5_malloc(MLX5_MEM_ZERO, mem_size, 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		claim_zero(mlx5_devx_cmd_destroy(obj));
		return NULL;
	}
	pool->devx_obj = obj;
	pool->index = mng->next;
	/* Resize pools array if there is no room for the new pool in it. */
	if (pool->index == mng->n && flow_dv_aso_ct_pools_resize(dev)) {
		claim_zero(mlx5_devx_cmd_destroy(obj));
		mlx5_free(pool);
		return NULL;
	}
	mng->pools[pool->index] = pool;
	mng->next++;
	/* Assign the first action in the new pool, the rest go to free list. */
	*ct_free = &pool->actions[0];
	/* Lock outside, the list operation is safe here. */
	for (i = 1; i < MLX5_ASO_CT_ACTIONS_PER_POOL; i++) {
		/* refcnt is 0 when allocating the memory. */
		pool->actions[i].offset = i;
		LIST_INSERT_HEAD(&mng->free_cts, &pool->actions[i], next);
	}
	return pool;
}

/*
 * Allocate a ASO CT action from free list.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Index to ASO CT action on success, 0 otherwise and rte_errno is set.
 */
static uint32_t
flow_dv_aso_ct_alloc(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pools_mng *mng = priv->sh->ct_mng;
	struct mlx5_aso_ct_action *ct = NULL;
	struct mlx5_aso_ct_pool *pool;
	uint8_t reg_c;
	uint32_t ct_idx;

	MLX5_ASSERT(mng);
	if (!priv->sh->cdev->config.devx) {
		rte_errno = ENOTSUP;
		return 0;
	}
	/* Get a free CT action, if no, a new pool will be created. */
	rte_spinlock_lock(&mng->ct_sl);
	ct = LIST_FIRST(&mng->free_cts);
	if (ct) {
		LIST_REMOVE(ct, next);
	} else if (!flow_dv_ct_pool_create(dev, &ct)) {
		rte_spinlock_unlock(&mng->ct_sl);
		rte_flow_error_set(error, rte_errno, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "failed to create ASO CT pool");
		return 0;
	}
	rte_spinlock_unlock(&mng->ct_sl);
	pool = container_of(ct, struct mlx5_aso_ct_pool, actions[ct->offset]);
	ct_idx = MLX5_MAKE_CT_IDX(pool->index, ct->offset);
	/* 0: inactive, 1: created, 2+: used by flows. */
	__atomic_store_n(&ct->refcnt, 1, __ATOMIC_RELAXED);
	reg_c = mlx5_flow_get_reg_id(dev, MLX5_ASO_CONNTRACK, 0, error);
	if (!ct->dr_action_orig) {
#ifdef HAVE_MLX5_DR_ACTION_ASO_CT
		ct->dr_action_orig = mlx5_glue->dv_create_flow_action_aso
			(priv->sh->rx_domain, pool->devx_obj->obj,
			 ct->offset,
			 MLX5DV_DR_ACTION_FLAGS_ASO_CT_DIRECTION_INITIATOR,
			 reg_c - REG_C_0);
#else
		RTE_SET_USED(reg_c);
#endif
		if (!ct->dr_action_orig) {
			flow_dv_aso_ct_dev_release(dev, ct_idx);
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "failed to create ASO CT action");
			return 0;
		}
	}
	if (!ct->dr_action_rply) {
#ifdef HAVE_MLX5_DR_ACTION_ASO_CT
		ct->dr_action_rply = mlx5_glue->dv_create_flow_action_aso
			(priv->sh->rx_domain, pool->devx_obj->obj,
			 ct->offset,
			 MLX5DV_DR_ACTION_FLAGS_ASO_CT_DIRECTION_RESPONDER,
			 reg_c - REG_C_0);
#endif
		if (!ct->dr_action_rply) {
			flow_dv_aso_ct_dev_release(dev, ct_idx);
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "failed to create ASO CT action");
			return 0;
		}
	}
	return ct_idx;
}

/*
 * Create a conntrack object with context and actions by using ASO mechanism.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] pro
 *   Pointer to conntrack information profile.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Index to conntrack object on success, 0 otherwise.
 */
static uint32_t
flow_dv_translate_create_conntrack(struct rte_eth_dev *dev,
				   const struct rte_flow_action_conntrack *pro,
				   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_aso_ct_action *ct;
	uint32_t idx;

	if (!sh->ct_aso_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Connection is not supported");
	idx = flow_dv_aso_ct_alloc(dev, error);
	if (!idx)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Failed to allocate CT object");
	ct = flow_aso_ct_get_by_dev_idx(dev, idx);
	if (mlx5_aso_ct_update_by_wqe(sh, MLX5_HW_INV_QUEUE, ct, pro, NULL, true)) {
		flow_dv_aso_ct_dev_release(dev, idx);
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				   "Failed to update CT");
		return 0;
	}
	ct->is_original = !!pro->is_original_dir;
	ct->peer = pro->peer_port;
	return idx;
}

/**
 * Fill the flow matcher with DV spec.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] wks
 *   Pointer to the matcher workspace.
 * @param[in] key
 *   Pointer to the flow matcher key.
 * @param[in] key_type
 *   Key type.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_translate_items(struct rte_eth_dev *dev,
			const struct rte_flow_item *items,
			struct mlx5_dv_matcher_workspace *wks,
			void *key, uint32_t key_type,
			struct rte_flow_error *error)
{
	struct mlx5_flow_rss_desc *rss_desc = wks->rss_desc;
	uint8_t next_protocol = wks->next_protocol;
	int tunnel = !!(wks->item_flags & MLX5_FLOW_LAYER_TUNNEL);
	int item_type = items->type;
	uint64_t last_item = wks->last_item;
	int ret;

	switch (item_type) {
	case RTE_FLOW_ITEM_TYPE_ESP:
		flow_dv_translate_item_esp(key, items, tunnel, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = MLX5_FLOW_ITEM_ESP;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_ID:
		flow_dv_translate_item_port_id
			(dev, key, items, wks->attr, key_type);
		last_item = MLX5_FLOW_ITEM_PORT_ID;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
		flow_dv_translate_item_port_representor
			(dev, key, key_type);
		last_item = MLX5_FLOW_ITEM_PORT_REPRESENTOR;
		break;
	case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		flow_dv_translate_item_represented_port
			(dev, key, items, wks->attr, key_type);
		last_item = MLX5_FLOW_ITEM_REPRESENTED_PORT;
		break;
	case RTE_FLOW_ITEM_TYPE_ETH:
		flow_dv_translate_item_eth(key, items, tunnel,
					   wks->group, key_type);
		wks->priority = wks->action_flags &
				MLX5_FLOW_ACTION_DEFAULT_MISS &&
				!wks->external ?
				MLX5_PRIORITY_MAP_L3 :
				MLX5_PRIORITY_MAP_L2;
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
				     MLX5_FLOW_LAYER_OUTER_L2;
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		flow_dv_translate_item_vlan(key, items, tunnel, wks, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L2;
		last_item = tunnel ? (MLX5_FLOW_LAYER_INNER_L2 |
					  MLX5_FLOW_LAYER_INNER_VLAN) :
					 (MLX5_FLOW_LAYER_OUTER_L2 |
					  MLX5_FLOW_LAYER_OUTER_VLAN);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		mlx5_flow_tunnel_ip_check(items, next_protocol,
					  &wks->item_flags, &tunnel);
		flow_dv_translate_item_ipv4(key, items, tunnel,
					    wks->group, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L3;
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				     MLX5_FLOW_LAYER_OUTER_L3_IPV4;
		if (items->mask != NULL &&
		    items->spec != NULL &&
			((const struct rte_flow_item_ipv4 *)
			 items->mask)->hdr.next_proto_id) {
			next_protocol =
				((const struct rte_flow_item_ipv4 *)
				 (items->spec))->hdr.next_proto_id;
			next_protocol &=
				((const struct rte_flow_item_ipv4 *)
				 (items->mask))->hdr.next_proto_id;
		} else if (key_type == MLX5_SET_MATCHER_HS_M &&
			   items->mask != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv4 *)
					(items->mask))->hdr.next_proto_id;
		} else if (key_type == MLX5_SET_MATCHER_HS_V &&
			   items->spec != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv4 *)
					(items->spec))->hdr.next_proto_id;
		} else {
			/* Reset for inner layer. */
			next_protocol = 0xff;
		}
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		mlx5_flow_tunnel_ip_check(items, next_protocol,
					  &wks->item_flags, &tunnel);
		flow_dv_translate_item_ipv6(key, items, tunnel,
					    wks->group, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L3;
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
		if (items->mask != NULL &&
		    items->spec != NULL &&
			((const struct rte_flow_item_ipv6 *)
			 items->mask)->hdr.proto) {
			next_protocol =
				((const struct rte_flow_item_ipv6 *)
				 items->spec)->hdr.proto;
			next_protocol &=
				((const struct rte_flow_item_ipv6 *)
				 items->mask)->hdr.proto;
		} else if (key_type == MLX5_SET_MATCHER_HS_M &&
			   items->mask != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv6 *)
					(items->mask))->hdr.proto;
		} else if (key_type == MLX5_SET_MATCHER_HS_V &&
			   items->spec != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv6 *)
					(items->spec))->hdr.proto;
		} else {
			/* Reset for inner layer. */
			next_protocol = 0xff;
		}
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
		flow_dv_translate_item_ipv6_frag_ext
					(key, items, tunnel, key_type);
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6_FRAG_EXT :
				     MLX5_FLOW_LAYER_OUTER_L3_IPV6_FRAG_EXT;
		if (items->mask != NULL &&
		    items->spec != NULL &&
			((const struct rte_flow_item_ipv6_frag_ext *)
			 items->mask)->hdr.next_header) {
			next_protocol =
			((const struct rte_flow_item_ipv6_frag_ext *)
			 items->spec)->hdr.next_header;
			next_protocol &=
			((const struct rte_flow_item_ipv6_frag_ext *)
			 items->mask)->hdr.next_header;
		} else if (key_type == MLX5_SET_MATCHER_HS_M &&
			   items->mask != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv6_frag_ext *)
					(items->mask))->hdr.next_header;
		} else if (key_type == MLX5_SET_MATCHER_HS_V &&
			   items->spec != NULL) {
			next_protocol =  ((const struct rte_flow_item_ipv6_frag_ext *)
					(items->spec))->hdr.next_header;
		} else {
			/* Reset for inner layer. */
			next_protocol = 0xff;
		}
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		flow_dv_translate_item_tcp(key, items, tunnel, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
				     MLX5_FLOW_LAYER_OUTER_L4_TCP;
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		flow_dv_translate_item_udp(key, items, tunnel, wks, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
				     MLX5_FLOW_LAYER_OUTER_L4_UDP;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		wks->tunnel_item = items;
		wks->gre_item = items;
		last_item = MLX5_FLOW_LAYER_GRE;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		flow_dv_translate_item_gre_key(key, items, key_type);
		last_item = MLX5_FLOW_LAYER_GRE_KEY;
		break;
	case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		wks->tunnel_item = items;
		last_item = MLX5_FLOW_LAYER_GRE;
		break;
	case RTE_FLOW_ITEM_TYPE_NVGRE:
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		wks->tunnel_item = items;
		last_item = MLX5_FLOW_LAYER_GRE;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		flow_dv_translate_item_vxlan(dev, wks->attr, key,
					     items, tunnel, wks, key_type);
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		last_item = MLX5_FLOW_LAYER_VXLAN;
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		wks->tunnel_item = items;
		last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		wks->tunnel_item = items;
		last_item = MLX5_FLOW_LAYER_GENEVE;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
		ret = flow_dv_translate_item_geneve_opt
				(dev, key, items, key_type, error);
		if (ret)
			return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"cannot create GENEVE TLV option");
		wks->geneve_tlv_option = 1;
		last_item = MLX5_FLOW_LAYER_GENEVE_OPT;
		break;
	case RTE_FLOW_ITEM_TYPE_MPLS:
		flow_dv_translate_item_mpls(key, items, last_item,
					    tunnel, key_type);
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		last_item = MLX5_FLOW_LAYER_MPLS;
		break;
	case RTE_FLOW_ITEM_TYPE_MARK:
		flow_dv_translate_item_mark(dev, key, items, key_type);
		last_item = MLX5_FLOW_ITEM_MARK;
		break;
	case RTE_FLOW_ITEM_TYPE_META:
		flow_dv_translate_item_meta
				(dev, key, wks->attr, items, key_type);
		last_item = MLX5_FLOW_ITEM_METADATA;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP:
		flow_dv_translate_item_icmp(key, items, tunnel, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = MLX5_FLOW_LAYER_ICMP;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP6:
		flow_dv_translate_item_icmp6(key, items, tunnel, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = MLX5_FLOW_LAYER_ICMP6;
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST:
	case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY:
		flow_dv_translate_item_icmp6_echo(key, items, tunnel, key_type);
		wks->priority = MLX5_PRIORITY_MAP_L4;
		last_item = MLX5_FLOW_LAYER_ICMP6;
		break;
	case RTE_FLOW_ITEM_TYPE_TAG:
		flow_dv_translate_item_tag(dev, key, items, key_type);
		last_item = MLX5_FLOW_ITEM_TAG;
		break;
	case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		flow_dv_translate_mlx5_item_tag(dev, key, items, key_type);
		last_item = MLX5_FLOW_ITEM_TAG;
		break;
	case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
		flow_dv_translate_item_sq(key, items, key_type);
		last_item = MLX5_FLOW_ITEM_SQ;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP:
		flow_dv_translate_item_gtp(key, items, tunnel, key_type);
		wks->priority = MLX5_TUNNEL_PRIO_GET(rss_desc);
		last_item = MLX5_FLOW_LAYER_GTP;
		break;
	case RTE_FLOW_ITEM_TYPE_GTP_PSC:
		ret = flow_dv_translate_item_gtp_psc(key, items, key_type);
		if (ret)
			return rte_flow_error_set(error, -ret,
				RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"cannot create GTP PSC item");
		last_item = MLX5_FLOW_LAYER_GTP_PSC;
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
		flow_dv_translate_item_ecpri
				(dev, key, items, last_item, key_type);
		/* No other protocol should follow eCPRI layer. */
		last_item = MLX5_FLOW_LAYER_ECPRI;
		break;
	case RTE_FLOW_ITEM_TYPE_METER_COLOR:
		flow_dv_translate_item_meter_color(dev, key, items, key_type);
		last_item = MLX5_FLOW_ITEM_METER_COLOR;
		break;
	case RTE_FLOW_ITEM_TYPE_INTEGRITY:
		last_item = flow_dv_translate_item_integrity(items,
							     wks, key_type);
		break;
	case RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY:
		flow_dv_translate_item_aggr_affinity(key, items, key_type);
		last_item = MLX5_FLOW_ITEM_AGGR_AFFINITY;
		break;
	default:
		break;
	}
	wks->item_flags |= last_item;
	wks->last_item = last_item;
	wks->next_protocol = next_protocol;
	return 0;
}

/**
 * Fill the HW steering flow with DV spec.
 *
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] key
 *   Pointer to the flow matcher key.
 * @param[in] key_type
 *   Key type.
 * @param[in, out] item_flags
 *   Pointer to the flow item flags.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
flow_dv_translate_items_hws(const struct rte_flow_item *items,
			    struct mlx5_flow_attr *attr, void *key,
			    uint32_t key_type, uint64_t *item_flags,
			    uint8_t *match_criteria,
			    struct rte_flow_error *error)
{
	struct mlx5_flow_workspace *flow_wks = mlx5_flow_push_thread_workspace();
	struct mlx5_flow_rss_desc rss_desc = { .level = attr->rss_level };
	struct rte_flow_attr rattr = {
		.group = attr->group,
		.priority = attr->priority,
		.ingress = !!(attr->tbl_type == MLX5DR_TABLE_TYPE_NIC_RX),
		.egress = !!(attr->tbl_type == MLX5DR_TABLE_TYPE_NIC_TX),
		.transfer = !!(attr->tbl_type == MLX5DR_TABLE_TYPE_FDB),
	};
	struct mlx5_dv_matcher_workspace wks = {
		.action_flags = attr->act_flags,
		.item_flags = item_flags ? *item_flags : 0,
		.external = 0,
		.next_protocol = 0xff,
		.attr = &rattr,
		.rss_desc = &rss_desc,
	};
	int ret = 0;

	RTE_SET_USED(flow_wks);
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		if (!mlx5_flow_os_item_supported(items->type)) {
			ret = rte_flow_error_set(error, ENOTSUP,
						 RTE_FLOW_ERROR_TYPE_ITEM,
						 NULL, "item not supported");
			goto exit;
		}
		ret = flow_dv_translate_items(&rte_eth_devices[attr->port_id],
			items, &wks, key, key_type,  NULL);
		if (ret)
			goto exit;
	}
	if (wks.item_flags & MLX5_FLOW_ITEM_INTEGRITY) {
		flow_dv_translate_item_integrity_post(key,
						      wks.integrity_items,
						      wks.item_flags,
						      key_type);
	}
	if (wks.item_flags & MLX5_FLOW_LAYER_VXLAN_GPE) {
		flow_dv_translate_item_vxlan_gpe(key,
						 wks.tunnel_item,
						 wks.item_flags,
						 key_type);
	} else if (wks.item_flags & MLX5_FLOW_LAYER_GENEVE) {
		flow_dv_translate_item_geneve(key,
					      wks.tunnel_item,
					      wks.item_flags,
					      key_type);
	} else if (wks.item_flags & MLX5_FLOW_LAYER_GRE) {
		if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_GRE) {
			flow_dv_translate_item_gre(key,
						   wks.tunnel_item,
						   wks.item_flags,
						   key_type);
		} else if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_GRE_OPTION) {
			flow_dv_translate_item_gre_option(key,
							  wks.tunnel_item,
							  wks.gre_item,
							  wks.item_flags,
							  key_type);
		} else if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_NVGRE) {
			flow_dv_translate_item_nvgre(key,
						     wks.tunnel_item,
						     wks.item_flags,
						     key_type);
		} else {
			MLX5_ASSERT(false);
		}
	}

	if (match_criteria)
		*match_criteria = flow_dv_matcher_enable(key);
	if (item_flags)
		*item_flags = wks.item_flags;
exit:
	mlx5_flow_pop_thread_workspace();
	return ret;
}

/**
 * Fill the SW steering flow with DV spec.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in, out] dev_flow
 *   Pointer to the sub flow.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in, out] matcher
 *   Pointer to the flow matcher.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_translate_items_sws(struct rte_eth_dev *dev,
			    struct mlx5_flow *dev_flow,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item *items,
			    struct mlx5_flow_dv_matcher *matcher,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	void *match_mask = matcher->mask.buf;
	void *match_value = dev_flow->dv.value.buf;
	struct mlx5_dv_matcher_workspace wks = {
		.action_flags = dev_flow->act_flags,
		.item_flags = 0,
		.external = dev_flow->external,
		.next_protocol = 0xff,
		.group = dev_flow->dv.group,
		.attr = attr,
		.rss_desc = &((struct mlx5_flow_workspace *)
			     mlx5_flow_get_thread_workspace())->rss_desc,
	};
	struct mlx5_dv_matcher_workspace wks_m = wks;
	int item_type;
	int ret = 0;
	int tunnel;

	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		if (!mlx5_flow_os_item_supported(items->type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "item not supported");
		tunnel = !!(wks.item_flags & MLX5_FLOW_LAYER_TUNNEL);
		item_type = items->type;
		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_CONNTRACK:
			flow_dv_translate_item_aso_ct(dev, match_mask,
						      match_value, items);
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX:
			flow_dv_translate_item_flex(dev, match_mask,
						    match_value, items,
						    dev_flow, tunnel != 0);
			wks.last_item = tunnel ? MLX5_FLOW_ITEM_INNER_FLEX :
						 MLX5_FLOW_ITEM_OUTER_FLEX;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
			flow_dv_translate_item_sq(match_value, items,
						  MLX5_SET_MATCHER_SW_V);
			flow_dv_translate_item_sq(match_mask, items,
						  MLX5_SET_MATCHER_SW_M);
			break;
		default:
			ret = flow_dv_translate_items(dev, items, &wks_m,
				match_mask, MLX5_SET_MATCHER_SW_M, error);
			if (ret)
				return ret;
			ret = flow_dv_translate_items(dev, items, &wks,
				match_value, MLX5_SET_MATCHER_SW_V, error);
			if (ret)
				return ret;
			break;
		}
		wks.item_flags |= wks.last_item;
	}
	/*
	 * When E-Switch mode is enabled, we have two cases where we need to
	 * set the source port manually.
	 * The first one, is in case of NIC ingress steering rule, and the
	 * second is E-Switch rule where no port_id item was found.
	 * In both cases the source port is set according the current port
	 * in use.
	 */
	if (!(wks.item_flags & MLX5_FLOW_ITEM_PORT_ID) &&
	    !(wks.item_flags & MLX5_FLOW_ITEM_REPRESENTED_PORT) &&
	    !(wks.item_flags & MLX5_FLOW_ITEM_PORT_REPRESENTOR) &&
	    priv->sh->esw_mode &&
	    !attr->egress &&
	    attr->group != MLX5_FLOW_MREG_CP_TABLE_GROUP) {
		if (flow_dv_translate_item_port_id_all(dev, match_mask,
						   match_value, NULL, attr))
			return -rte_errno;
	}
	if (wks.item_flags & MLX5_FLOW_ITEM_INTEGRITY) {
		flow_dv_translate_item_integrity_post(match_mask,
						      wks_m.integrity_items,
						      wks_m.item_flags,
						      MLX5_SET_MATCHER_SW_M);
		flow_dv_translate_item_integrity_post(match_value,
						      wks.integrity_items,
						      wks.item_flags,
						      MLX5_SET_MATCHER_SW_V);
	}
	if (wks.item_flags & MLX5_FLOW_LAYER_VXLAN_GPE) {
		flow_dv_translate_item_vxlan_gpe(match_mask,
						 wks.tunnel_item,
						 wks.item_flags,
						 MLX5_SET_MATCHER_SW_M);
		flow_dv_translate_item_vxlan_gpe(match_value,
						 wks.tunnel_item,
						 wks.item_flags,
						 MLX5_SET_MATCHER_SW_V);
	} else if (wks.item_flags & MLX5_FLOW_LAYER_GENEVE) {
		flow_dv_translate_item_geneve(match_mask,
					      wks.tunnel_item,
					      wks.item_flags,
					      MLX5_SET_MATCHER_SW_M);
		flow_dv_translate_item_geneve(match_value,
					      wks.tunnel_item,
					      wks.item_flags,
					      MLX5_SET_MATCHER_SW_V);
	} else if (wks.item_flags & MLX5_FLOW_LAYER_GRE) {
		if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_GRE) {
			flow_dv_translate_item_gre(match_mask,
						   wks.tunnel_item,
						   wks.item_flags,
						   MLX5_SET_MATCHER_SW_M);
			flow_dv_translate_item_gre(match_value,
						   wks.tunnel_item,
						   wks.item_flags,
						   MLX5_SET_MATCHER_SW_V);
		} else if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_NVGRE) {
			flow_dv_translate_item_nvgre(match_mask,
						     wks.tunnel_item,
						     wks.item_flags,
						     MLX5_SET_MATCHER_SW_M);
			flow_dv_translate_item_nvgre(match_value,
						     wks.tunnel_item,
						     wks.item_flags,
						     MLX5_SET_MATCHER_SW_V);
		} else if (wks.tunnel_item->type == RTE_FLOW_ITEM_TYPE_GRE_OPTION) {
			flow_dv_translate_item_gre_option(match_mask,
							  wks.tunnel_item,
							  wks.gre_item,
							  wks.item_flags,
							  MLX5_SET_MATCHER_SW_M);
			flow_dv_translate_item_gre_option(match_value,
							  wks.tunnel_item,
							  wks.gre_item,
							  wks.item_flags,
							  MLX5_SET_MATCHER_SW_V);
		} else {
			MLX5_ASSERT(false);
		}
	}
	dev_flow->handle->vf_vlan.tag = wks.vlan_tag;
	matcher->priority = wks.priority;
#ifdef RTE_LIBRTE_MLX5_DEBUG
	MLX5_ASSERT(!flow_dv_check_valid_spec(match_mask, match_value));
#endif
	/*
	 * Layers may be already initialized from prefix flow if this dev_flow
	 * is the suffix flow.
	 */
	dev_flow->handle->layers |= wks.item_flags;
	/*
	 * Update geneve_tlv_option flag only it is set in workspace.
	 * Avoid be overwritten by other sub mlx5_flows.
	 */
	if (wks.geneve_tlv_option)
		dev_flow->flow->geneve_tlv_option = wks.geneve_tlv_option;
	return 0;
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
flow_dv_translate(struct rte_eth_dev *dev,
		  struct mlx5_flow *dev_flow,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *dev_conf = &priv->sh->config;
	struct rte_flow *flow = dev_flow->flow;
	struct mlx5_flow_handle *handle = dev_flow->handle;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_flow_rss_desc *rss_desc;
	uint64_t action_flags = 0;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
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
	const struct rte_flow_action_age *non_shared_age = NULL;
	union flow_dv_attr flow_attr = { .attr = 0 };
	uint32_t tag_be;
	union mlx5_flow_tbl_key tbl_key;
	uint32_t modify_action_position = UINT32_MAX;
	struct rte_vlan_hdr vlan = { 0 };
	struct mlx5_flow_dv_dest_array_resource mdest_res;
	struct mlx5_flow_dv_sample_resource sample_res;
	void *sample_actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS] = {0};
	const struct rte_flow_action_sample *sample = NULL;
	struct mlx5_flow_sub_actions_list *sample_act;
	uint32_t sample_act_pos = UINT32_MAX;
	uint32_t age_act_pos = UINT32_MAX;
	uint32_t num_of_dest = 0;
	int tmp_actions_n = 0;
	uint32_t table;
	int ret = 0;
	const struct mlx5_flow_tunnel *tunnel = NULL;
	struct flow_grp_info grp_info = {
		.external = !!dev_flow->external,
		.transfer = !!attr->transfer,
		.fdb_def_rule = !!priv->fdb_def_rule,
		.skip_scale = dev_flow->skip_scale &
			(1 << MLX5_SCALE_FLOW_GROUP_BIT),
		.std_tbl_fix = true,
	};

	if (!wks)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to push flow workspace");
	rss_desc = &wks->rss_desc;
	memset(&mdest_res, 0, sizeof(struct mlx5_flow_dv_dest_array_resource));
	memset(&sample_res, 0, sizeof(struct mlx5_flow_dv_sample_resource));
	mhdr_res->ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					   MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	/* update normal path action resource into last index of array */
	sample_act = &mdest_res.sample_act[MLX5_MAX_DEST_NUM - 1];
	if (is_tunnel_offload_active(dev)) {
		if (dev_flow->tunnel) {
			RTE_VERIFY(dev_flow->tof_type ==
				   MLX5_TUNNEL_OFFLOAD_MISS_RULE);
			tunnel = dev_flow->tunnel;
		} else {
			tunnel = mlx5_get_tof(items, actions,
					      &dev_flow->tof_type);
			dev_flow->tunnel = tunnel;
		}
		grp_info.std_tbl_fix = tunnel_use_standard_attr_group_translate
					(dev, attr, tunnel, dev_flow->tof_type);
	}
	mhdr_res->ft_type = attr->egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					   MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	ret = mlx5_flow_group_to_table(dev, tunnel, attr->group, &table,
				       &grp_info, error);
	if (ret)
		return ret;
	dev_flow->dv.group = table;
	if (attr->transfer)
		mhdr_res->ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	/* number of actions must be set to 0 in case of dirty stack. */
	mhdr_res->actions_num = 0;
	if (is_flow_tunnel_match_rule(dev_flow->tof_type)) {
		/*
		 * do not add decap action if match rule drops packet
		 * HW rejects rules with decap & drop
		 *
		 * if tunnel match rule was inserted before matching tunnel set
		 * rule flow table used in the match rule must be registered.
		 * current implementation handles that in the
		 * flow_dv_match_register() at the function end.
		 */
		bool add_decap = true;
		const struct rte_flow_action *ptr = actions;

		for (; ptr->type != RTE_FLOW_ACTION_TYPE_END; ptr++) {
			if (ptr->type == RTE_FLOW_ACTION_TYPE_DROP) {
				add_decap = false;
				break;
			}
		}
		if (add_decap) {
			if (flow_dv_create_action_l2_decap(dev, dev_flow,
							   attr->transfer,
							   error))
				return -rte_errno;
			dev_flow->dv.actions[actions_n++] =
					dev_flow->dv.encap_decap->action;
			action_flags |= MLX5_FLOW_ACTION_DECAP;
		}
	}
	for (; !actions_end ; actions++) {
		const struct rte_flow_action_queue *queue;
		const struct rte_flow_action_rss *rss;
		const struct rte_flow_action *action = actions;
		const uint8_t *rss_key;
		struct mlx5_flow_tbl_resource *tbl;
		struct mlx5_aso_age_action *age_act;
		struct mlx5_flow_counter *cnt_act;
		uint32_t port_id = 0;
		struct mlx5_flow_dv_port_id_action_resource port_id_resource;
		int action_type = actions->type;
		const struct rte_flow_action *found_action = NULL;
		uint32_t jump_group = 0;
		uint32_t owner_idx;
		struct mlx5_aso_ct_action *ct;

		if (!mlx5_flow_os_action_supported(action_type))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		switch (action_type) {
		case MLX5_RTE_FLOW_ACTION_TYPE_TUNNEL_SET:
			action_flags |= MLX5_FLOW_ACTION_TUNNEL_SET;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
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
			sample_act->action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			num_of_dest++;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			wks->mark = 1;
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
			wks->mark = 1;
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
			sample_act->action_flags |= MLX5_FLOW_ACTION_QUEUE;
			num_of_dest++;
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
			dev_flow->handle->fate_action = rss_desc->shared_rss ?
				MLX5_FLOW_FATE_SHARED_RSS :
				MLX5_FLOW_FATE_QUEUE;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_AGE:
			owner_idx = (uint32_t)(uintptr_t)action->conf;
			age_act = flow_aso_age_get_by_idx(dev, owner_idx);
			if (flow->age == 0) {
				flow->age = owner_idx;
				__atomic_fetch_add(&age_act->refcnt, 1,
						   __ATOMIC_RELAXED);
			}
			age_act_pos = actions_n++;
			action_flags |= MLX5_FLOW_ACTION_AGE;
			break;
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			dev_flow->dv.actions[actions_n] =
				flow_dv_translate_action_send_to_kernel(dev,
							error);
			if (!dev_flow->dv.actions[actions_n])
				return -rte_errno;
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_SEND_TO_KERNEL;
			dev_flow->handle->fate_action =
					MLX5_FLOW_FATE_SEND_TO_KERNEL;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			non_shared_age = action->conf;
			age_act_pos = actions_n++;
			action_flags |= MLX5_FLOW_ACTION_AGE;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
			owner_idx = (uint32_t)(uintptr_t)action->conf;
			cnt_act = flow_dv_counter_get_by_idx(dev, owner_idx,
							     NULL);
			MLX5_ASSERT(cnt_act != NULL);
			/**
			 * When creating meter drop flow in drop table, the
			 * counter should not overwrite the rte flow counter.
			 */
			if (attr->group == MLX5_FLOW_TABLE_LEVEL_METER &&
			    dev_flow->dv.table_id == MLX5_MTR_TABLE_ID_DROP) {
				dev_flow->dv.actions[actions_n++] =
							cnt_act->action;
			} else {
				if (flow->counter == 0) {
					flow->counter = owner_idx;
					__atomic_fetch_add
						(&cnt_act->shared_info.refcnt,
						 1, __ATOMIC_RELAXED);
				}
				/* Save information first, will apply later. */
				action_flags |= MLX5_FLOW_ACTION_COUNT;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (!priv->sh->cdev->config.devx) {
				return rte_flow_error_set
					      (error, ENOTSUP,
					       RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					       NULL,
					       "count action not supported");
			}
			/* Save information first, will apply later. */
			count = action->conf;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				sample_act->action_flags |=
							MLX5_FLOW_ACTION_ENCAP;
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
			if (action_flags & MLX5_FLOW_ACTION_SAMPLE)
				sample_act->action_flags |=
							MLX5_FLOW_ACTION_ENCAP;
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
		case MLX5_RTE_FLOW_ACTION_TYPE_JUMP:
			dev_flow->dv.actions[actions_n++] =
				(void *)(uintptr_t)action->conf;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			jump_group = ((const struct rte_flow_action_jump *)
							action->conf)->group;
			grp_info.std_tbl_fix = 0;
			if (dev_flow->skip_scale &
				(1 << MLX5_SCALE_JUMP_FLOW_GROUP_BIT))
				grp_info.skip_scale = 1;
			else
				grp_info.skip_scale = 0;
			ret = mlx5_flow_group_to_table(dev, tunnel,
						       jump_group,
						       &table,
						       &grp_info, error);
			if (ret)
				return ret;
			tbl = flow_dv_tbl_resource_get(dev, table, attr->egress,
						       attr->transfer,
						       !!dev_flow->external,
						       tunnel, jump_group, 0,
						       0, error);
			if (!tbl)
				return rte_flow_error_set
						(error, errno,
						 RTE_FLOW_ERROR_TYPE_ACTION,
						 NULL,
						 "cannot create jump action.");
			if (flow_dv_jump_tbl_resource_register
			    (dev, tbl, dev_flow, error)) {
				flow_dv_tbl_resource_release(MLX5_SH(dev), tbl);
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
			sample_act->action_flags |= MLX5_FLOW_ACTION_JUMP;
			num_of_dest++;
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
			if (!wks->fm)
				return rte_flow_error_set(error, rte_errno,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "Failed to get meter in flow.");
			/* Set the meter action. */
			dev_flow->dv.actions[actions_n++] =
				wks->fm->meter_action_g;
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
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
			sample_act_pos = actions_n;
			sample = (const struct rte_flow_action_sample *)
				 action->conf;
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_SAMPLE;
			/* put encap action into group if work with port id */
			if ((action_flags & MLX5_FLOW_ACTION_ENCAP) &&
			    (action_flags & MLX5_FLOW_ACTION_PORT_ID))
				sample_act->action_flags |=
							MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			if (flow_dv_convert_action_modify_field
					(dev, mhdr_res, actions, attr, error))
				return -rte_errno;
			action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			owner_idx = (uint32_t)(uintptr_t)action->conf;
			ct = flow_aso_ct_get_by_idx(dev, owner_idx);
			if (!ct)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Failed to get CT object.");
			if (mlx5_aso_ct_available(priv->sh, MLX5_HW_INV_QUEUE, ct))
				return rte_flow_error_set(error, rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"CT is unavailable.");
			if (ct->is_original)
				dev_flow->dv.actions[actions_n] =
							ct->dr_action_orig;
			else
				dev_flow->dv.actions[actions_n] =
							ct->dr_action_rply;
			if (flow->ct == 0) {
				flow->indirect_type =
						MLX5_INDIRECT_ACTION_TYPE_CT;
				flow->ct = owner_idx;
				__atomic_fetch_add(&ct->refcnt, 1,
						   __ATOMIC_RELAXED);
			}
			actions_n++;
			action_flags |= MLX5_FLOW_ACTION_CT;
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
			/*
			 * Handle AGE and COUNT action by single HW counter
			 * when they are not shared.
			 */
			if (action_flags & MLX5_FLOW_ACTION_AGE) {
				if ((non_shared_age && count) ||
				    !flow_hit_aso_supported(priv, !dev_flow->dv.group)) {
					/* Creates age by counters. */
					cnt_act = flow_dv_prepare_counter
								(dev, dev_flow,
								 flow, count,
								 non_shared_age,
								 error);
					if (!cnt_act)
						return -rte_errno;
					dev_flow->dv.actions[age_act_pos] =
								cnt_act->action;
					break;
				}
				if (!flow->age && non_shared_age) {
					flow->age = flow_dv_aso_age_alloc
								(dev, error);
					if (!flow->age)
						return -rte_errno;
					flow_dv_aso_age_params_init
						    (dev, flow->age,
						     non_shared_age->context ?
						     non_shared_age->context :
						     (void *)(uintptr_t)
						     (dev_flow->flow_idx),
						     non_shared_age->timeout);
				}
				age_act = flow_aso_age_get_by_idx(dev,
								  flow->age);
				dev_flow->dv.actions[age_act_pos] =
							     age_act->dr_action;
			}
			if (action_flags & MLX5_FLOW_ACTION_COUNT) {
				/*
				 * Create one count action, to be used
				 * by all sub-flows.
				 */
				cnt_act = flow_dv_prepare_counter(dev, dev_flow,
								  flow, count,
								  NULL, error);
				if (!cnt_act)
					return -rte_errno;
				dev_flow->dv.actions[actions_n++] =
								cnt_act->action;
			}
		default:
			break;
		}
		if (mhdr_res->actions_num &&
		    modify_action_position == UINT32_MAX)
			modify_action_position = actions_n++;
	}
	dev_flow->act_flags = action_flags;
	ret = flow_dv_translate_items_sws(dev, dev_flow, attr, items, &matcher,
				      error);
	if (ret)
		return -rte_errno;
	if (action_flags & MLX5_FLOW_ACTION_RSS)
		flow_dv_hashfields_set(dev_flow->handle->layers,
				       rss_desc,
				       &dev_flow->hash_fields);
	/* If has RSS action in the sample action, the Sample/Mirror resource
	 * should be registered after the hash filed be update.
	 */
	if (action_flags & MLX5_FLOW_ACTION_SAMPLE) {
		ret = flow_dv_translate_action_sample(dev,
						      sample,
						      dev_flow, attr,
						      &num_of_dest,
						      sample_actions,
						      &sample_res,
						      error);
		if (ret < 0)
			return ret;
		ret = flow_dv_create_action_sample(dev,
						   dev_flow,
						   num_of_dest,
						   &sample_res,
						   &mdest_res,
						   sample_actions,
						   action_flags,
						   error);
		if (ret < 0)
			return rte_flow_error_set
						(error, rte_errno,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"cannot create sample action");
		if (num_of_dest > 1) {
			dev_flow->dv.actions[sample_act_pos] =
			dev_flow->dv.dest_array_res->action;
		} else {
			dev_flow->dv.actions[sample_act_pos] =
			dev_flow->dv.sample_res->verbs_action;
		}
	}
	/*
	 * For multiple destination (sample action with ratio=1), the encap
	 * action and port id action will be combined into group action.
	 * So need remove the original these actions in the flow and only
	 * use the sample action instead of.
	 */
	if (num_of_dest > 1 &&
	    (sample_act->dr_port_id_action || sample_act->dr_jump_action)) {
		int i;
		void *temp_actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS] = {0};

		for (i = 0; i < actions_n; i++) {
			if ((sample_act->dr_encap_action &&
				sample_act->dr_encap_action ==
				dev_flow->dv.actions[i]) ||
				(sample_act->dr_port_id_action &&
				sample_act->dr_port_id_action ==
				dev_flow->dv.actions[i]) ||
				(sample_act->dr_jump_action &&
				sample_act->dr_jump_action ==
				dev_flow->dv.actions[i]))
				continue;
			temp_actions[tmp_actions_n++] = dev_flow->dv.actions[i];
		}
		memcpy((void *)dev_flow->dv.actions,
				(void *)temp_actions,
				tmp_actions_n * sizeof(void *));
		actions_n = tmp_actions_n;
	}
	dev_flow->dv.actions_n = actions_n;
	if (wks->skip_matcher_reg)
		return 0;
	/* Register matcher. */
	matcher.crc = rte_raw_cksum((const void *)matcher.mask.buf,
				    matcher.mask.size);
	matcher.priority = mlx5_get_matcher_priority(dev, attr,
						     matcher.priority,
						     dev_flow->external);
	/**
	 * When creating meter drop flow in drop table, using original
	 * 5-tuple match, the matcher priority should be lower than
	 * mtr_id matcher.
	 */
	if (attr->group == MLX5_FLOW_TABLE_LEVEL_METER &&
	    dev_flow->dv.table_id == MLX5_MTR_TABLE_ID_DROP &&
	    matcher.priority <= MLX5_REG_BITS)
		matcher.priority += MLX5_REG_BITS;
	/* reserved field no needs to be set to 0 here. */
	tbl_key.is_fdb = attr->transfer;
	tbl_key.is_egress = attr->egress;
	tbl_key.level = dev_flow->dv.group;
	tbl_key.id = dev_flow->dv.table_id;
	if (flow_dv_matcher_register(dev, &matcher, &tbl_key, dev_flow,
				     tunnel, attr->group, error))
		return -rte_errno;
	return 0;
}

/**
 * Set hash RX queue by hash fields (see enum ibv_rx_hash_fields)
 * and tunnel.
 *
 * @param[in, out] action
 *   Shred RSS action holding hash RX queue objects.
 * @param[in] hash_fields
 *   Defines combination of packet fields to participate in RX hash.
 * @param[in] tunnel
 *   Tunnel type
 * @param[in] hrxq_idx
 *   Hash RX queue index to set.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
__flow_dv_action_rss_hrxq_set(struct mlx5_shared_action_rss *action,
			      const uint64_t hash_fields,
			      uint32_t hrxq_idx)
{
	uint32_t *hrxqs = action->hrxq;

	switch (hash_fields & ~IBV_RX_HASH_INNER) {
	case MLX5_RSS_HASH_IPV4:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_SRC_ONLY:
		hrxqs[0] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV4_TCP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_TCP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_TCP_SRC_ONLY:
		hrxqs[1] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV4_UDP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_UDP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_UDP_SRC_ONLY:
		hrxqs[2] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV6:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_SRC_ONLY:
		hrxqs[3] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV6_TCP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_TCP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_TCP_SRC_ONLY:
		hrxqs[4] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV6_UDP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_UDP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_UDP_SRC_ONLY:
		hrxqs[5] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_NONE:
		hrxqs[6] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV4_ESP:
		hrxqs[7] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_IPV6_ESP:
		hrxqs[8] = hrxq_idx;
		return 0;
	case MLX5_RSS_HASH_ESP_SPI:
		hrxqs[9] = hrxq_idx;
		return 0;
	default:
		return -1;
	}
}

/**
 * Look up for hash RX queue by hash fields (see enum ibv_rx_hash_fields)
 * and tunnel.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   Shared RSS action ID holding hash RX queue objects.
 * @param[in] hash_fields
 *   Defines combination of packet fields to participate in RX hash.
 * @param[in] tunnel
 *   Tunnel type
 *
 * @return
 *   Valid hash RX queue index, otherwise 0.
 */
uint32_t
flow_dv_action_rss_hrxq_lookup(struct rte_eth_dev *dev, uint32_t idx,
			       const uint64_t hash_fields)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss =
	    mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
	const uint32_t *hrxqs = shared_rss->hrxq;

	switch (hash_fields & ~IBV_RX_HASH_INNER) {
	case MLX5_RSS_HASH_IPV4:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_SRC_ONLY:
		return hrxqs[0];
	case MLX5_RSS_HASH_IPV4_TCP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_TCP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_TCP_SRC_ONLY:
		return hrxqs[1];
	case MLX5_RSS_HASH_IPV4_UDP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_UDP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV4_UDP_SRC_ONLY:
		return hrxqs[2];
	case MLX5_RSS_HASH_IPV6:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_SRC_ONLY:
		return hrxqs[3];
	case MLX5_RSS_HASH_IPV6_TCP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_TCP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_TCP_SRC_ONLY:
		return hrxqs[4];
	case MLX5_RSS_HASH_IPV6_UDP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_UDP_DST_ONLY:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_UDP_SRC_ONLY:
		return hrxqs[5];
	case MLX5_RSS_HASH_NONE:
		return hrxqs[6];
	case MLX5_RSS_HASH_IPV4_ESP:
		return hrxqs[7];
	case MLX5_RSS_HASH_IPV6_ESP:
		return hrxqs[8];
	case MLX5_RSS_HASH_ESP_SPI:
		return hrxqs[9];
	default:
		return 0;
	}

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
flow_dv_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
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
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct mlx5_flow_rss_desc *rss_desc = &wks->rss_desc;
	uint8_t misc_mask;

	MLX5_ASSERT(wks);
	for (idx = wks->flow_idx - 1; idx >= 0; idx--) {
		dev_flow = &wks->flows[idx];
		dv = &dev_flow->dv;
		dh = dev_flow->handle;
		dv_h = &dh->dvh;
		n = dv->actions_n;
		if (dh->fate_action == MLX5_FLOW_FATE_DROP) {
			if (dv->transfer) {
				MLX5_ASSERT(priv->sh->dr_drop_action);
				dv->actions[n++] = priv->sh->dr_drop_action;
			} else {
#ifdef HAVE_MLX5DV_DR
				/* DR supports drop action placeholder. */
				MLX5_ASSERT(priv->sh->dr_drop_action);
				dv->actions[n++] = dv->group ?
					priv->sh->dr_drop_action :
					priv->root_drop_action;
#else
				/* For DV we use the explicit drop queue. */
				MLX5_ASSERT(priv->drop_queue.hrxq);
				dv->actions[n++] =
						priv->drop_queue.hrxq->action;
#endif
			}
		} else if ((dh->fate_action == MLX5_FLOW_FATE_QUEUE &&
			   !dv_h->rix_sample && !dv_h->rix_dest_array)) {
			struct mlx5_hrxq *hrxq;
			uint32_t hrxq_idx;

			hrxq = flow_dv_hrxq_prepare(dev, dev_flow, rss_desc,
						    &hrxq_idx);
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			dh->rix_hrxq = hrxq_idx;
			dv->actions[n++] = hrxq->action;
		} else if (dh->fate_action == MLX5_FLOW_FATE_SHARED_RSS) {
			struct mlx5_hrxq *hrxq = NULL;
			uint32_t hrxq_idx;

			hrxq_idx = flow_dv_action_rss_hrxq_lookup(dev,
						rss_desc->shared_rss,
						dev_flow->hash_fields);
			if (hrxq_idx)
				hrxq = mlx5_ipool_get
					(priv->sh->ipool[MLX5_IPOOL_HRXQ],
					 hrxq_idx);
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot get hash queue");
				goto error;
			}
			dh->rix_srss = rss_desc->shared_rss;
			dv->actions[n++] = hrxq->action;
		} else if (dh->fate_action == MLX5_FLOW_FATE_DEFAULT_MISS) {
			if (!priv->sh->default_miss_action) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "default miss action not be created.");
				goto error;
			}
			dv->actions[n++] = priv->sh->default_miss_action;
		}
		misc_mask = flow_dv_matcher_enable(dv->value.buf);
		__flow_dv_adjust_buf_size(&dv->value.size, misc_mask);
		err = mlx5_flow_os_create_flow(dv_h->matcher->matcher_object,
					       (void *)&dv->value, n,
					       dv->actions, &dh->drv_flow);
		if (err) {
			rte_flow_error_set
				(error, errno,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				(!priv->sh->config.allow_duplicate_pattern &&
				errno == EEXIST) ?
				"duplicating pattern is not allowed" :
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
	err = rte_errno; /* Save rte_errno before cleanup. */
	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       handle_idx, dh, next) {
		/* hrxq is union, don't clear it if the flag is not set. */
		if (dh->fate_action == MLX5_FLOW_FATE_QUEUE && dh->rix_hrxq) {
			mlx5_hrxq_release(dev, dh->rix_hrxq);
			dh->rix_hrxq = 0;
		} else if (dh->fate_action == MLX5_FLOW_FATE_SHARED_RSS) {
			dh->rix_srss = 0;
		}
		if (dh->vf_vlan.tag && dh->vf_vlan.created)
			mlx5_vlan_vmwa_release(dev, &dh->vf_vlan);
	}
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

void
flow_dv_matcher_remove_cb(void *tool_ctx __rte_unused,
			  struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_matcher *resource = container_of(entry,
							     typeof(*resource),
							     entry);

	claim_zero(mlx5_flow_os_destroy_flow_matcher(resource->matcher_object));
	mlx5_free(resource);
}

/**
 * Release the flow matcher.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param port_id
 *   Index to port ID action resource.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_matcher_release(struct rte_eth_dev *dev,
			struct mlx5_flow_handle *handle)
{
	struct mlx5_flow_dv_matcher *matcher = handle->dvh.matcher;
	struct mlx5_flow_tbl_data_entry *tbl = container_of(matcher->tbl,
							    typeof(*tbl), tbl);
	int ret;

	MLX5_ASSERT(matcher->matcher_object);
	ret = mlx5_list_unregister(tbl->matchers, &matcher->entry);
	flow_dv_tbl_resource_release(MLX5_SH(dev), &tbl->tbl);
	return ret;
}

void
flow_dv_encap_decap_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_encap_decap_resource *res =
				       container_of(entry, typeof(*res), entry);

	claim_zero(mlx5_flow_os_destroy_flow_action(res->action));
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_DECAP_ENCAP], res->idx);
}

/**
 * Release an encap/decap resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param encap_decap_idx
 *   Index of encap decap resource.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_encap_decap_resource_release(struct rte_eth_dev *dev,
				     uint32_t encap_decap_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_encap_decap_resource *resource;

	resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_DECAP_ENCAP],
				  encap_decap_idx);
	if (!resource)
		return 0;
	MLX5_ASSERT(resource->action);
	return mlx5_hlist_unregister(priv->sh->encaps_decaps, &resource->entry);
}

/**
 * Release an jump to table action resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param rix_jump
 *   Index to the jump action resource.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
static int
flow_dv_jump_tbl_resource_release(struct rte_eth_dev *dev,
				  uint32_t rix_jump)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_tbl_data_entry *tbl_data;

	tbl_data = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_JUMP],
				  rix_jump);
	if (!tbl_data)
		return 0;
	return flow_dv_tbl_resource_release(MLX5_SH(dev), &tbl_data->tbl);
}

void
flow_dv_modify_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_modify_hdr_resource *res =
		container_of(entry, typeof(*res), entry);
	struct mlx5_dev_ctx_shared *sh = tool_ctx;

	claim_zero(mlx5_flow_os_destroy_flow_action(res->action));
	mlx5_ipool_free(sh->mdh_ipools[res->actions_num - 1], res->idx);
}

/**
 * Release a modify-header resource.
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
flow_dv_modify_hdr_resource_release(struct rte_eth_dev *dev,
				    struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_modify_hdr_resource *entry = handle->dvh.modify_hdr;

	MLX5_ASSERT(entry->action);
	return mlx5_hlist_unregister(priv->sh->modify_cmds, &entry->entry);
}

void
flow_dv_port_id_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_port_id_action_resource *resource =
				  container_of(entry, typeof(*resource), entry);

	claim_zero(mlx5_flow_os_destroy_flow_action(resource->action));
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PORT_ID], resource->idx);
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
					uint32_t port_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_port_id_action_resource *resource;

	resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_PORT_ID], port_id);
	if (!resource)
		return 0;
	MLX5_ASSERT(resource->action);
	return mlx5_list_unregister(priv->sh->port_id_action_list,
				    &resource->entry);
}

/**
 * Release shared RSS action resource.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param srss
 *   Shared RSS action index.
 */
static void
flow_dv_shared_rss_action_release(struct rte_eth_dev *dev, uint32_t srss)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss;

	shared_rss = mlx5_ipool_get
			(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], srss);
	__atomic_fetch_sub(&shared_rss->refcnt, 1, __ATOMIC_RELAXED);
}

void
flow_dv_push_vlan_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_dv_push_vlan_action_resource *resource =
			container_of(entry, typeof(*resource), entry);

	claim_zero(mlx5_flow_os_destroy_flow_action(resource->action));
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_PUSH_VLAN], resource->idx);
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
	struct mlx5_flow_dv_push_vlan_action_resource *resource;
	uint32_t idx = handle->dvh.rix_push_vlan;

	resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_PUSH_VLAN], idx);
	if (!resource)
		return 0;
	MLX5_ASSERT(resource->action);
	return mlx5_list_unregister(priv->sh->push_vlan_action_list,
				    &resource->entry);
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
	case MLX5_FLOW_FATE_QUEUE:
		if (!handle->dvh.rix_sample && !handle->dvh.rix_dest_array)
			mlx5_hrxq_release(dev, handle->rix_hrxq);
		break;
	case MLX5_FLOW_FATE_JUMP:
		flow_dv_jump_tbl_resource_release(dev, handle->rix_jump);
		break;
	case MLX5_FLOW_FATE_PORT_ID:
		flow_dv_port_id_action_resource_release(dev,
				handle->rix_port_id_action);
		break;
	case MLX5_FLOW_FATE_SEND_TO_KERNEL:
		/* In case of send_to_kernel action the actual release of
		 * resource is done when all shared DR resources are released
		 * since this resource is created once and always reused.
		 */
		break;
	default:
		DRV_LOG(DEBUG, "Incorrect fate action:%d", handle->fate_action);
		break;
	}
	handle->rix_fate = 0;
}

void
flow_dv_sample_remove_cb(void *tool_ctx __rte_unused,
			 struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_sample_resource *resource = container_of(entry,
							      typeof(*resource),
							      entry);
	struct rte_eth_dev *dev = resource->dev;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (resource->verbs_action)
		claim_zero(mlx5_flow_os_destroy_flow_action
						      (resource->verbs_action));
	if (resource->normal_path_tbl)
		flow_dv_tbl_resource_release(MLX5_SH(dev),
					     resource->normal_path_tbl);
	flow_dv_sample_sub_actions_release(dev, &resource->sample_idx);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_SAMPLE], resource->idx);
	DRV_LOG(DEBUG, "sample resource %p: removed", (void *)resource);
}

/**
 * Release an sample resource.
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
flow_dv_sample_resource_release(struct rte_eth_dev *dev,
				     struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_sample_resource *resource;

	resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_SAMPLE],
				  handle->dvh.rix_sample);
	if (!resource)
		return 0;
	MLX5_ASSERT(resource->verbs_action);
	return mlx5_list_unregister(priv->sh->sample_action_list,
				    &resource->entry);
}

void
flow_dv_dest_array_remove_cb(void *tool_ctx __rte_unused,
			     struct mlx5_list_entry *entry)
{
	struct mlx5_flow_dv_dest_array_resource *resource =
			container_of(entry, typeof(*resource), entry);
	struct rte_eth_dev *dev = resource->dev;
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i = 0;

	MLX5_ASSERT(resource->action);
	if (resource->action)
		claim_zero(mlx5_flow_os_destroy_flow_action(resource->action));
	for (; i < resource->num_of_dest; i++)
		flow_dv_sample_sub_actions_release(dev,
						   &resource->sample_idx[i]);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_DEST_ARRAY], resource->idx);
	DRV_LOG(DEBUG, "destination array resource %p: removed",
		(void *)resource);
}

/**
 * Release an destination array resource.
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
flow_dv_dest_array_resource_release(struct rte_eth_dev *dev,
				    struct mlx5_flow_handle *handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_dest_array_resource *resource;

	resource = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_DEST_ARRAY],
				  handle->dvh.rix_dest_array);
	if (!resource)
		return 0;
	MLX5_ASSERT(resource->action);
	return mlx5_list_unregister(priv->sh->dest_array_list,
				    &resource->entry);
}

void
flow_dev_geneve_tlv_option_resource_release(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_geneve_tlv_option_resource *geneve_opt_resource =
				sh->geneve_tlv_option_resource;
	rte_spinlock_lock(&sh->geneve_tlv_opt_sl);
	if (geneve_opt_resource) {
		if (!(__atomic_fetch_sub(&geneve_opt_resource->refcnt, 1,
					 __ATOMIC_RELAXED) - 1)) {
			claim_zero(mlx5_devx_cmd_destroy
					(geneve_opt_resource->obj));
			mlx5_free(sh->geneve_tlv_option_resource);
			sh->geneve_tlv_option_resource = NULL;
		}
	}
	rte_spinlock_unlock(&sh->geneve_tlv_opt_sl);
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
flow_dv_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
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
		if (dh->fate_action == MLX5_FLOW_FATE_QUEUE)
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
flow_dv_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_handle *dev_handle;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *fm = NULL;
	uint32_t srss = 0;

	if (!flow)
		return;
	flow_dv_remove(dev, flow);
	if (flow->counter) {
		flow_dv_counter_free(dev, flow->counter);
		flow->counter = 0;
	}
	if (flow->meter) {
		fm = flow_dv_meter_find_by_idx(priv, flow->meter);
		if (fm)
			mlx5_flow_meter_detach(priv, fm);
		flow->meter = 0;
	}
	/* Keep the current age handling by default. */
	if (flow->indirect_type == MLX5_INDIRECT_ACTION_TYPE_CT && flow->ct)
		flow_dv_aso_ct_release(dev, flow->ct, NULL);
	else if (flow->age)
		flow_dv_aso_age_release(dev, flow->age);
	if (flow->geneve_tlv_option) {
		flow_dev_geneve_tlv_option_resource_release(priv->sh);
		flow->geneve_tlv_option = 0;
	}
	while (flow->dev_handles) {
		uint32_t tmp_idx = flow->dev_handles;

		dev_handle = mlx5_ipool_get(priv->sh->ipool
					    [MLX5_IPOOL_MLX5_FLOW], tmp_idx);
		if (!dev_handle)
			return;
		flow->dev_handles = dev_handle->next.next;
		while (dev_handle->flex_item) {
			int index = rte_bsf32(dev_handle->flex_item);

			mlx5_flex_release_index(dev, index);
			dev_handle->flex_item &= ~(uint8_t)RTE_BIT32(index);
		}
		if (dev_handle->dvh.matcher)
			flow_dv_matcher_release(dev, dev_handle);
		if (dev_handle->dvh.rix_sample)
			flow_dv_sample_resource_release(dev, dev_handle);
		if (dev_handle->dvh.rix_dest_array)
			flow_dv_dest_array_resource_release(dev, dev_handle);
		if (dev_handle->dvh.rix_encap_decap)
			flow_dv_encap_decap_resource_release(dev,
				dev_handle->dvh.rix_encap_decap);
		if (dev_handle->dvh.modify_hdr)
			flow_dv_modify_hdr_resource_release(dev, dev_handle);
		if (dev_handle->dvh.rix_push_vlan)
			flow_dv_push_vlan_action_resource_release(dev,
								  dev_handle);
		if (dev_handle->dvh.rix_tag)
			flow_dv_tag_release(dev,
					    dev_handle->dvh.rix_tag);
		if (dev_handle->fate_action != MLX5_FLOW_FATE_SHARED_RSS)
			flow_dv_fate_resource_release(dev, dev_handle);
		else if (!srss)
			srss = dev_handle->rix_srss;
		if (fm && dev_handle->is_meter_flow_id &&
		    dev_handle->split_flow_id)
			mlx5_ipool_free(fm->flow_ipool,
					dev_handle->split_flow_id);
		else if (dev_handle->split_flow_id &&
		    !dev_handle->is_meter_flow_id)
			mlx5_ipool_free(priv->sh->ipool
					[MLX5_IPOOL_RSS_EXPANTION_FLOW_ID],
					dev_handle->split_flow_id);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
			   tmp_idx);
	}
	if (srss)
		flow_dv_shared_rss_action_release(dev, srss);
}

/**
 * Release array of hash RX queue objects.
 * Helper function.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] hrxqs
 *   Array of hash RX queue objects.
 *
 * @return
 *   Total number of references to hash RX queue objects in *hrxqs* array
 *   after this operation.
 */
static int
__flow_dv_hrxqs_release(struct rte_eth_dev *dev,
			uint32_t (*hrxqs)[MLX5_RSS_HASH_FIELDS_LEN])
{
	size_t i;
	int remaining = 0;

	for (i = 0; i < RTE_DIM(*hrxqs); i++) {
		int ret = mlx5_hrxq_release(dev, (*hrxqs)[i]);

		if (!ret)
			(*hrxqs)[i] = 0;
		remaining += ret;
	}
	return remaining;
}

/**
 * Release all hash RX queue objects representing shared RSS action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in, out] action
 *   Shared RSS action to remove hash RX queue objects from.
 *
 * @return
 *   Total number of references to hash RX queue objects stored in *action*
 *   after this operation.
 *   Expected to be 0 if no external references held.
 */
static int
__flow_dv_action_rss_hrxqs_release(struct rte_eth_dev *dev,
				 struct mlx5_shared_action_rss *shared_rss)
{
	return __flow_dv_hrxqs_release(dev, &shared_rss->hrxq);
}

/**
 * Adjust L3/L4 hash value of pre-created shared RSS hrxq according to
 * user input.
 *
 * Only one hash value is available for one L3+L4 combination:
 * for example:
 * MLX5_RSS_HASH_IPV4, MLX5_RSS_HASH_IPV4_SRC_ONLY, and
 * MLX5_RSS_HASH_IPV4_DST_ONLY are mutually exclusive so they can share
 * same slot in mlx5_rss_hash_fields.
 *
 * @param[in] orig_rss_types
 *   RSS type as provided in shared RSS action.
 * @param[in, out] hash_field
 *   hash_field variable needed to be adjusted.
 *
 * @return
 *   void
 */
void
flow_dv_action_rss_l34_hash_adjust(uint64_t orig_rss_types,
				   uint64_t *hash_field)
{
	uint64_t rss_types = rte_eth_rss_hf_refine(orig_rss_types);

	switch (*hash_field & ~IBV_RX_HASH_INNER) {
	case MLX5_RSS_HASH_IPV4:
		if (rss_types & MLX5_IPV4_LAYER_TYPES) {
			*hash_field &= ~MLX5_RSS_HASH_IPV4;
			if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
				*hash_field |= IBV_RX_HASH_DST_IPV4;
			else if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
				*hash_field |= IBV_RX_HASH_SRC_IPV4;
			else
				*hash_field |= MLX5_RSS_HASH_IPV4;
		}
		return;
	case MLX5_RSS_HASH_IPV6:
		if (rss_types & MLX5_IPV6_LAYER_TYPES) {
			*hash_field &= ~MLX5_RSS_HASH_IPV6;
			if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
				*hash_field |= IBV_RX_HASH_DST_IPV6;
			else if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
				*hash_field |= IBV_RX_HASH_SRC_IPV6;
			else
				*hash_field |= MLX5_RSS_HASH_IPV6;
		}
		return;
	case MLX5_RSS_HASH_IPV4_UDP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_UDP:
		if (rss_types & RTE_ETH_RSS_UDP) {
			*hash_field &= ~MLX5_UDP_IBV_RX_HASH;
			if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
				*hash_field |= IBV_RX_HASH_DST_PORT_UDP;
			else if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
				*hash_field |= IBV_RX_HASH_SRC_PORT_UDP;
			else
				*hash_field |= MLX5_UDP_IBV_RX_HASH;
		}
		return;
	case MLX5_RSS_HASH_IPV4_TCP:
		/* fall-through. */
	case MLX5_RSS_HASH_IPV6_TCP:
		if (rss_types & RTE_ETH_RSS_TCP) {
			*hash_field &= ~MLX5_TCP_IBV_RX_HASH;
			if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
				*hash_field |= IBV_RX_HASH_DST_PORT_TCP;
			else if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
				*hash_field |= IBV_RX_HASH_SRC_PORT_TCP;
			else
				*hash_field |= MLX5_TCP_IBV_RX_HASH;
		}
		return;
	default:
		return;
	}
}

/**
 * Setup shared RSS action.
 * Prepare set of hash RX queue objects sufficient to handle all valid
 * hash_fields combinations (see enum ibv_rx_hash_fields).
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action_idx
 *   Shared RSS action ipool index.
 * @param[in, out] action
 *   Partially initialized shared RSS action.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
__flow_dv_action_rss_setup(struct rte_eth_dev *dev,
			   uint32_t action_idx,
			   struct mlx5_shared_action_rss *shared_rss,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_rss_desc rss_desc = { 0 };
	size_t i;
	int err;

	shared_rss->ind_tbl = mlx5_ind_table_obj_new
			      (dev, shared_rss->origin.queue,
			       shared_rss->origin.queue_num,
			       true,
			       !!dev->data->dev_started);
	if (!shared_rss->ind_tbl)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot setup indirection table");
	memcpy(rss_desc.key, shared_rss->origin.key, MLX5_RSS_HASH_KEY_LEN);
	rss_desc.key_len = MLX5_RSS_HASH_KEY_LEN;
	rss_desc.const_q = shared_rss->origin.queue;
	rss_desc.queue_num = shared_rss->origin.queue_num;
	/* Set non-zero value to indicate a shared RSS. */
	rss_desc.shared_rss = action_idx;
	rss_desc.ind_tbl = shared_rss->ind_tbl;
	if (priv->sh->config.dv_flow_en == 2)
		rss_desc.hws_flags = MLX5DR_ACTION_FLAG_HWS_RX;
	for (i = 0; i < MLX5_RSS_HASH_FIELDS_LEN; i++) {
		struct mlx5_hrxq *hrxq;
		uint64_t hash_fields = mlx5_rss_hash_fields[i];
		int tunnel = 0;

		flow_dv_action_rss_l34_hash_adjust(shared_rss->origin.types,
						   &hash_fields);
		if (shared_rss->origin.level > 1) {
			hash_fields |= IBV_RX_HASH_INNER;
			tunnel = 1;
		}
		rss_desc.tunnel = tunnel;
		rss_desc.hash_fields = hash_fields;
		hrxq = mlx5_hrxq_get(dev, &rss_desc);
		if (!hrxq) {
			rte_flow_error_set
				(error, rte_errno,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				 "cannot get hash queue");
			goto error_hrxq_new;
		}
		err = __flow_dv_action_rss_hrxq_set
			(shared_rss, hash_fields, hrxq->idx);
		MLX5_ASSERT(!err);
	}
	return 0;
error_hrxq_new:
	err = rte_errno;
	__flow_dv_action_rss_hrxqs_release(dev, shared_rss);
	if (!mlx5_ind_table_obj_release(dev, shared_rss->ind_tbl, true))
		shared_rss->ind_tbl = NULL;
	rte_errno = err;
	return -rte_errno;
}

/**
 * Create shared RSS action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] conf
 *   Shared action configuration.
 * @param[in] rss
 *   RSS action specification used to create shared action.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   A valid shared action ID in case of success, 0 otherwise and
 *   rte_errno is set.
 */
static uint32_t
__flow_dv_action_rss_create(struct rte_eth_dev *dev,
			    const struct rte_flow_indir_action_conf *conf,
			    const struct rte_flow_action_rss *rss,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss = NULL;
	struct rte_flow_action_rss *origin;
	const uint8_t *rss_key;
	uint32_t idx;

	RTE_SET_USED(conf);
	shared_rss = mlx5_ipool_zmalloc
			 (priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], &idx);
	if (!shared_rss) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "cannot allocate resource memory");
		goto error_rss_init;
	}
	if (idx > (1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET)) {
		rte_flow_error_set(error, E2BIG,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "rss action number out of range");
		goto error_rss_init;
	}
	origin = &shared_rss->origin;
	origin->func = rss->func;
	origin->level = rss->level;
	/* RSS type 0 indicates default RSS type (RTE_ETH_RSS_IP). */
	origin->types = !rss->types ? RTE_ETH_RSS_IP : rss->types;
	/* NULL RSS key indicates default RSS key. */
	rss_key = !rss->key ? rss_hash_default_key : rss->key;
	memcpy(shared_rss->key, rss_key, MLX5_RSS_HASH_KEY_LEN);
	origin->key = &shared_rss->key[0];
	origin->key_len = MLX5_RSS_HASH_KEY_LEN;
	origin->queue = rss->queue;
	origin->queue_num = rss->queue_num;
	if (__flow_dv_action_rss_setup(dev, idx, shared_rss, error))
		goto error_rss_init;
	/* Update queue with indirect table queue memoyr. */
	origin->queue = shared_rss->ind_tbl->queues;
	rte_spinlock_init(&shared_rss->action_rss_sl);
	__atomic_fetch_add(&shared_rss->refcnt, 1, __ATOMIC_RELAXED);
	rte_spinlock_lock(&priv->shared_act_sl);
	ILIST_INSERT(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
		     &priv->rss_shared_actions, idx, shared_rss, next);
	rte_spinlock_unlock(&priv->shared_act_sl);
	return idx;
error_rss_init:
	if (shared_rss) {
		if (shared_rss->ind_tbl)
			mlx5_ind_table_obj_release(dev, shared_rss->ind_tbl,
						   !!dev->data->dev_started);
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
				idx);
	}
	return 0;
}

/**
 * Destroy the shared RSS action.
 * Release related hash RX queue objects.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   The shared RSS action object ID to be removed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
__flow_dv_action_rss_release(struct rte_eth_dev *dev, uint32_t idx,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss =
	    mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
	uint32_t old_refcnt = 1;
	int remaining;

	if (!shared_rss)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "invalid shared action");
	if (!__atomic_compare_exchange_n(&shared_rss->refcnt, &old_refcnt,
					 0, 0, __ATOMIC_ACQUIRE,
					 __ATOMIC_RELAXED))
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "shared rss has references");
	remaining = __flow_dv_action_rss_hrxqs_release(dev, shared_rss);
	if (remaining)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "shared rss hrxq has references");
	remaining = mlx5_ind_table_obj_release(dev, shared_rss->ind_tbl,
					       !!dev->data->dev_started);
	if (remaining)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "shared rss indirection table has"
					  " references");
	rte_spinlock_lock(&priv->shared_act_sl);
	ILIST_REMOVE(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
		     &priv->rss_shared_actions, idx, shared_rss, next);
	rte_spinlock_unlock(&priv->shared_act_sl);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
			idx);
	return 0;
}

/**
 * Create indirect action, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] conf
 *   Shared action configuration.
 * @param[in] action
 *   Action specification used to create indirect action.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   A valid shared action handle in case of success, NULL otherwise and
 *   rte_errno is set.
 */
struct rte_flow_action_handle *
flow_dv_action_create(struct rte_eth_dev *dev,
		      const struct rte_flow_indir_action_conf *conf,
		      const struct rte_flow_action *action,
		      struct rte_flow_error *err)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t age_idx = 0;
	uint32_t idx = 0;
	uint32_t ret = 0;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
		ret = __flow_dv_action_rss_create(dev, conf, action->conf, err);
		idx = (MLX5_INDIRECT_ACTION_TYPE_RSS <<
		       MLX5_INDIRECT_ACTION_TYPE_OFFSET) | ret;
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
		age_idx = flow_dv_aso_age_alloc(dev, err);
		if (!age_idx) {
			ret = -rte_errno;
			break;
		}
		idx = (MLX5_INDIRECT_ACTION_TYPE_AGE <<
		       MLX5_INDIRECT_ACTION_TYPE_OFFSET) | age_idx;
		flow_dv_aso_age_params_init(dev, age_idx,
					((const struct rte_flow_action_age *)
						action->conf)->context ?
					((const struct rte_flow_action_age *)
						action->conf)->context :
					(void *)(uintptr_t)idx,
					((const struct rte_flow_action_age *)
						action->conf)->timeout);
		ret = age_idx;
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		ret = flow_dv_translate_create_counter(dev, NULL, NULL, NULL);
		idx = (MLX5_INDIRECT_ACTION_TYPE_COUNT <<
		       MLX5_INDIRECT_ACTION_TYPE_OFFSET) | ret;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		ret = flow_dv_translate_create_conntrack(dev, action->conf,
							 err);
		idx = MLX5_INDIRECT_ACT_CT_GEN_IDX(PORT_ID(priv), ret);
		break;
	default:
		rte_flow_error_set(err, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "action type not supported");
		break;
	}
	return ret ? (struct rte_flow_action_handle *)(uintptr_t)idx : NULL;
}

/**
 * Destroy the indirect action.
 * Release action related resources on the NIC and the memory.
 * Lock free, (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] handle
 *   The indirect action object handle to be removed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
flow_dv_action_destroy(struct rte_eth_dev *dev,
		       struct rte_flow_action_handle *handle,
		       struct rte_flow_error *error)
{
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	struct mlx5_flow_counter *cnt;
	uint32_t no_flow_refcnt = 1;
	int ret;

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		return __flow_dv_action_rss_release(dev, idx, error);
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		cnt = flow_dv_counter_get_by_idx(dev, idx, NULL);
		if (!__atomic_compare_exchange_n(&cnt->shared_info.refcnt,
						 &no_flow_refcnt, 1, false,
						 __ATOMIC_ACQUIRE,
						 __ATOMIC_RELAXED))
			return rte_flow_error_set(error, EBUSY,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "Indirect count action has references");
		flow_dv_counter_free(dev, idx);
		return 0;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		ret = flow_dv_aso_age_release(dev, idx);
		if (ret)
			/*
			 * In this case, the last flow has a reference will
			 * actually release the age action.
			 */
			DRV_LOG(DEBUG, "Indirect age action %" PRIu32 " was"
				" released with references %d.", idx, ret);
		return 0;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		ret = flow_dv_aso_ct_release(dev, idx, error);
		if (ret < 0)
			return ret;
		if (ret > 0)
			DRV_LOG(DEBUG, "Connection tracking object %u still "
				"has references %d.", idx, ret);
		return 0;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "action type not supported");
	}
}

/**
 * Updates in place shared RSS action configuration.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   The shared RSS action object ID to be updated.
 * @param[in] action_conf
 *   RSS action specification used to modify *shared_rss*.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 * @note: currently only support update of RSS queues.
 */
static int
__flow_dv_action_rss_update(struct rte_eth_dev *dev, uint32_t idx,
			    const struct rte_flow_action_rss *action_conf,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss =
	    mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
	int ret = 0;
	void *queue = NULL;
	void *queue_i = NULL;
	uint32_t queue_size = action_conf->queue_num * sizeof(uint16_t);
	bool dev_started = !!dev->data->dev_started;

	if (!shared_rss)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "invalid shared action to update");
	if (priv->obj_ops.ind_table_modify == NULL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "cannot modify indirection table");
	queue = mlx5_malloc(MLX5_MEM_ZERO,
			    RTE_ALIGN_CEIL(queue_size, sizeof(void *)),
			    0, SOCKET_ID_ANY);
	if (!queue)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "cannot allocate resource memory");
	memcpy(queue, action_conf->queue, queue_size);
	MLX5_ASSERT(shared_rss->ind_tbl);
	rte_spinlock_lock(&shared_rss->action_rss_sl);
	queue_i = shared_rss->ind_tbl->queues;
	ret = mlx5_ind_table_obj_modify(dev, shared_rss->ind_tbl,
					queue, action_conf->queue_num,
					true /* standalone */,
					dev_started /* ref_new_qs */,
					dev_started /* deref_old_qs */);
	if (ret) {
		ret = rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "cannot update indirection table");
	} else {
		/* Restore the queue to indirect table internal queue. */
		memcpy(queue_i, queue, queue_size);
		shared_rss->ind_tbl->queues = queue_i;
		shared_rss->origin.queue_num = action_conf->queue_num;
	}
	mlx5_free(queue);
	rte_spinlock_unlock(&shared_rss->action_rss_sl);
	return ret;
}

/*
 * Updates in place conntrack context or direction.
 * Context update should be synchronized.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] idx
 *   The conntrack object ID to be updated.
 * @param[in] update
 *   Pointer to the structure of information to update.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
__flow_dv_action_ct_update(struct rte_eth_dev *dev, uint32_t idx,
			   const struct rte_flow_modify_conntrack *update,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_action *ct;
	const struct rte_flow_action_conntrack *new_prf;
	int ret = 0;
	uint16_t owner = (uint16_t)MLX5_INDIRECT_ACT_CT_GET_OWNER(idx);
	uint32_t dev_idx;

	if (PORT_ID(priv) != owner)
		return rte_flow_error_set(error, EACCES,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "CT object owned by another port");
	dev_idx = MLX5_INDIRECT_ACT_CT_GET_IDX(idx);
	ct = flow_aso_ct_get_by_dev_idx(dev, dev_idx);
	if (!ct->refcnt)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "CT object is inactive");
	new_prf = &update->new_ct;
	if (update->direction)
		ct->is_original = !!new_prf->is_original_dir;
	if (update->state) {
		/* Only validate the profile when it needs to be updated. */
		ret = mlx5_validate_action_ct(dev, new_prf, error);
		if (ret)
			return ret;
		ret = mlx5_aso_ct_update_by_wqe(priv->sh, MLX5_HW_INV_QUEUE,
						ct, new_prf, NULL, true);
		if (ret)
			return rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"Failed to send CT context update WQE");
		/* Block until ready or a failure, default is asynchronous. */
		ret = mlx5_aso_ct_available(priv->sh, MLX5_HW_INV_QUEUE, ct);
		if (ret)
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "Timeout to get the CT update");
	}
	return ret;
}

/**
 * Updates in place shared action configuration, lock free,
 * (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] handle
 *   The indirect action object handle to be updated.
 * @param[in] update
 *   Action specification used to modify the action pointed by *handle*.
 *   *update* could be of same type with the action pointed by the *handle*
 *   handle argument, or some other structures like a wrapper, depending on
 *   the indirect action type.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
flow_dv_action_update(struct rte_eth_dev *dev,
			struct rte_flow_action_handle *handle,
			const void *update,
			struct rte_flow_error *err)
{
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	const void *action_conf;

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		action_conf = ((const struct rte_flow_action *)update)->conf;
		return __flow_dv_action_rss_update(dev, idx, action_conf, err);
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		return __flow_dv_action_ct_update(dev, idx, update, err);
	default:
		return rte_flow_error_set(err, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "action type update not supported");
	}
}

/**
 * Destroy the meter sub policy table rules.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] sub_policy
 *   Pointer to meter sub policy table.
 */
static void
__flow_dv_destroy_sub_policy_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter_sub_policy *sub_policy)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_tbl_data_entry *tbl;
	struct mlx5_flow_meter_policy *policy = sub_policy->main_policy;
	struct mlx5_flow_meter_info *next_fm;
	struct mlx5_sub_policy_color_rule *color_rule;
	void *tmp;
	uint32_t i;

	for (i = 0; i < RTE_COLORS; i++) {
		next_fm = NULL;
		if (i <= RTE_COLOR_YELLOW && policy &&
		    policy->act_cnt[i].fate_action == MLX5_FLOW_FATE_MTR)
			next_fm = mlx5_flow_meter_find(priv,
					policy->act_cnt[i].next_mtr_id, NULL);
		RTE_TAILQ_FOREACH_SAFE(color_rule, &sub_policy->color_rules[i],
				   next_port, tmp) {
			claim_zero(mlx5_flow_os_destroy_flow(color_rule->rule));
			tbl = container_of(color_rule->matcher->tbl,
					   typeof(*tbl), tbl);
			mlx5_list_unregister(tbl->matchers,
					     &color_rule->matcher->entry);
			TAILQ_REMOVE(&sub_policy->color_rules[i],
				     color_rule, next_port);
			mlx5_free(color_rule);
			if (next_fm)
				mlx5_flow_meter_detach(priv, next_fm);
		}
	}
	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (sub_policy->rix_hrxq[i]) {
			if (policy && !policy->is_hierarchy)
				mlx5_hrxq_release(dev, sub_policy->rix_hrxq[i]);
			sub_policy->rix_hrxq[i] = 0;
		}
		if (sub_policy->jump_tbl[i]) {
			flow_dv_tbl_resource_release(MLX5_SH(dev),
						     sub_policy->jump_tbl[i]);
			sub_policy->jump_tbl[i] = NULL;
		}
	}
	if (sub_policy->tbl_rsc) {
		flow_dv_tbl_resource_release(MLX5_SH(dev),
					     sub_policy->tbl_rsc);
		sub_policy->tbl_rsc = NULL;
	}
}

/**
 * Destroy policy rules, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_policy
 *   Meter policy struct.
 */
static void
flow_dv_destroy_policy_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter_policy *mtr_policy)
{
	uint32_t i, j;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	uint16_t sub_policy_num;

	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		for (j = 0; j < sub_policy_num; j++) {
			sub_policy = mtr_policy->sub_policys[i][j];
			if (sub_policy)
				__flow_dv_destroy_sub_policy_rules(dev,
								   sub_policy);
		}
	}
}

/**
 * Destroy policy action, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_policy
 *   Meter policy struct.
 */
static void
flow_dv_destroy_mtr_policy_acts(struct rte_eth_dev *dev,
		      struct mlx5_flow_meter_policy *mtr_policy)
{
	struct rte_flow_action *rss_action;
	struct mlx5_flow_handle dev_handle;
	uint32_t i, j;

	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (mtr_policy->act_cnt[i].rix_mark) {
			flow_dv_tag_release(dev,
				mtr_policy->act_cnt[i].rix_mark);
			mtr_policy->act_cnt[i].rix_mark = 0;
		}
		if (mtr_policy->act_cnt[i].modify_hdr) {
			dev_handle.dvh.modify_hdr =
				mtr_policy->act_cnt[i].modify_hdr;
			flow_dv_modify_hdr_resource_release(dev, &dev_handle);
		}
		switch (mtr_policy->act_cnt[i].fate_action) {
		case MLX5_FLOW_FATE_SHARED_RSS:
			rss_action = mtr_policy->act_cnt[i].rss;
			mlx5_free(rss_action);
			break;
		case MLX5_FLOW_FATE_PORT_ID:
			if (mtr_policy->act_cnt[i].rix_port_id_action) {
				flow_dv_port_id_action_resource_release(dev,
				mtr_policy->act_cnt[i].rix_port_id_action);
				mtr_policy->act_cnt[i].rix_port_id_action = 0;
			}
			break;
		case MLX5_FLOW_FATE_DROP:
		case MLX5_FLOW_FATE_JUMP:
			for (j = 0; j < MLX5_MTR_DOMAIN_MAX; j++)
				mtr_policy->act_cnt[i].dr_jump_action[j] =
						NULL;
			break;
		default:
			/*Queue action do nothing*/
			break;
		}
	}
	for (j = 0; j < MLX5_MTR_DOMAIN_MAX; j++)
		mtr_policy->dr_drop_action[j] = NULL;
}

/**
 * Create yellow action for color aware meter.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] fm
 *   Meter information table.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
__flow_dv_create_mtr_yellow_action(struct rte_eth_dev *dev,
				   struct mlx5_flow_meter_info *fm,
				   struct rte_mtr_error *error)
{
#ifdef HAVE_MLX5_DR_CREATE_ACTION_ASO
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_error flow_err;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_aso_mtr_pool *pool;
	uint8_t reg_id;

	aso_mtr = container_of(fm, struct mlx5_aso_mtr, fm);
	pool = container_of(aso_mtr, struct mlx5_aso_mtr_pool, mtrs[aso_mtr->offset]);
	reg_id = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, &flow_err);
	fm->meter_action_y =
		mlx5_glue->dv_create_flow_action_aso(priv->sh->rx_domain,
						     pool->devx_obj->obj,
						     aso_mtr->offset,
						     (1 << MLX5_FLOW_COLOR_YELLOW),
						     reg_id - REG_C_0);
#else
	RTE_SET_USED(dev);
#endif
	if (!fm->meter_action_y) {
		return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					  "Fail to create yellow meter action.");
	}
	return 0;
}

/**
 * Create policy action per domain, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_policy
 *   Meter policy struct.
 * @param[in] action
 *   Action specification used to create meter actions.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
__flow_dv_create_domain_policy_acts(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_policy *mtr_policy,
			const struct rte_flow_action *actions[RTE_COLORS],
			struct rte_flow_attr *attr,
			enum mlx5_meter_domain domain,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_error flow_err;
	const struct rte_flow_action *act;
	uint64_t action_flags;
	struct mlx5_flow_handle dh;
	struct mlx5_flow dev_flow;
	struct mlx5_flow_dv_port_id_action_resource port_id_action;
	int i, ret;
	uint8_t egress, transfer;
	struct mlx5_meter_policy_action_container *act_cnt = NULL;
	union {
		struct mlx5_flow_dv_modify_hdr_resource res;
		uint8_t len[sizeof(struct mlx5_flow_dv_modify_hdr_resource) +
			    sizeof(struct mlx5_modification_cmd) *
			    (MLX5_MAX_MODIFY_NUM + 1)];
	} mhdr_dummy;
	struct mlx5_flow_dv_modify_hdr_resource *mhdr_res = &mhdr_dummy.res;

	egress = (domain == MLX5_MTR_DOMAIN_EGRESS) ? 1 : 0;
	transfer = (domain == MLX5_MTR_DOMAIN_TRANSFER) ? 1 : 0;
	memset(&dh, 0, sizeof(struct mlx5_flow_handle));
	memset(&dev_flow, 0, sizeof(struct mlx5_flow));
	memset(&port_id_action, 0,
	       sizeof(struct mlx5_flow_dv_port_id_action_resource));
	memset(mhdr_res, 0, sizeof(*mhdr_res));
	mhdr_res->ft_type = transfer ? MLX5DV_FLOW_TABLE_TYPE_FDB :
				       (egress ? MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					MLX5DV_FLOW_TABLE_TYPE_NIC_RX);
	dev_flow.handle = &dh;
	dev_flow.dv.port_id_action = &port_id_action;
	dev_flow.external = true;
	for (i = 0; i < RTE_COLORS; i++) {
		if (i < MLX5_MTR_RTE_COLORS)
			act_cnt = &mtr_policy->act_cnt[i];
		/* Skip the color policy actions creation. */
		if ((i == RTE_COLOR_YELLOW && mtr_policy->skip_y) ||
		    (i == RTE_COLOR_GREEN && mtr_policy->skip_g))
			continue;
		action_flags = 0;
		for (act = actions[i];
		     act && act->type != RTE_FLOW_ACTION_TYPE_END; act++) {
			switch (act->type) {
			case RTE_FLOW_ACTION_TYPE_MARK:
			{
				uint32_t tag_be = mlx5_flow_mark_set
					(((const struct rte_flow_action_mark *)
					(act->conf))->id);

				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "cannot create policy "
					  "mark action for this color");
				if (flow_dv_tag_resource_register(dev, tag_be,
						  &dev_flow, &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL,
					"cannot setup policy mark action");
				MLX5_ASSERT(dev_flow.dv.tag_resource);
				act_cnt->rix_mark =
					dev_flow.handle->dvh.rix_tag;
				action_flags |= MLX5_FLOW_ACTION_MARK;
				mtr_policy->mark = 1;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_SET_TAG:
				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "cannot create policy "
					  "set tag action for this color");
				if (flow_dv_convert_action_set_tag
				(dev, mhdr_res,
				(const struct rte_flow_action_set_tag *)
				act->conf,  &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot convert policy "
					"set tag action");
				if (!mhdr_res->actions_num)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot find policy "
					"set tag action");
				action_flags |= MLX5_FLOW_ACTION_SET_TAG;
				break;
			case RTE_FLOW_ACTION_TYPE_DROP:
			{
				struct mlx5_flow_mtr_mng *mtrmng =
						priv->sh->mtrmng;
				struct mlx5_flow_tbl_data_entry *tbl_data;

				/*
				 * Create the drop table with
				 * METER DROP level.
				 */
				if (!mtrmng->drop_tbl[domain]) {
					mtrmng->drop_tbl[domain] =
					flow_dv_tbl_resource_get(dev,
					MLX5_FLOW_TABLE_LEVEL_METER,
					egress, transfer, false, NULL, 0,
					0, MLX5_MTR_TABLE_ID_DROP, &flow_err);
					if (!mtrmng->drop_tbl[domain])
						return -rte_mtr_error_set
					(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL,
					"Failed to create meter drop table");
				}
				tbl_data = container_of
				(mtrmng->drop_tbl[domain],
				struct mlx5_flow_tbl_data_entry, tbl);
				if (i < MLX5_MTR_RTE_COLORS) {
					act_cnt->dr_jump_action[domain] =
						tbl_data->jump.action;
					act_cnt->fate_action =
						MLX5_FLOW_FATE_DROP;
				}
				if (i == RTE_COLOR_RED)
					mtr_policy->dr_drop_action[domain] =
						tbl_data->jump.action;
				action_flags |= MLX5_FLOW_ACTION_DROP;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_QUEUE:
			{
				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot create policy "
					"fate queue for this color");
				act_cnt->queue =
				((const struct rte_flow_action_queue *)
					(act->conf))->index;
				act_cnt->fate_action =
					MLX5_FLOW_FATE_QUEUE;
				dev_flow.handle->fate_action =
					MLX5_FLOW_FATE_QUEUE;
				mtr_policy->is_queue = 1;
				action_flags |= MLX5_FLOW_ACTION_QUEUE;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_RSS:
			{
				int rss_size;

				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "cannot create policy "
					  "rss action for this color");
				/*
				 * Save RSS conf into policy struct
				 * for translate stage.
				 */
				rss_size = (int)rte_flow_conv
					(RTE_FLOW_CONV_OP_ACTION,
					NULL, 0, act, &flow_err);
				if (rss_size <= 0)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "Get the wrong "
					  "rss action struct size");
				act_cnt->rss = mlx5_malloc(MLX5_MEM_ZERO,
						rss_size, 0, SOCKET_ID_ANY);
				if (!act_cnt->rss)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "Fail to malloc rss action memory");
				ret = rte_flow_conv(RTE_FLOW_CONV_OP_ACTION,
					act_cnt->rss, rss_size,
					act, &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "Fail to save "
					  "rss action into policy struct");
				act_cnt->fate_action =
					MLX5_FLOW_FATE_SHARED_RSS;
				action_flags |= MLX5_FLOW_ACTION_RSS;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_PORT_ID:
			case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			{
				struct mlx5_flow_dv_port_id_action_resource
					port_id_resource;
				uint32_t port_id = 0;

				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot create policy "
					"port action for this color");
				memset(&port_id_resource, 0,
					sizeof(port_id_resource));
				if (flow_dv_translate_action_port_id(dev, act,
						&port_id, &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot translate "
					"policy port action");
				port_id_resource.port_id = port_id;
				if (flow_dv_port_id_action_resource_register
					(dev, &port_id_resource,
					&dev_flow, &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot setup "
					"policy port action");
				act_cnt->rix_port_id_action =
					dev_flow.handle->rix_port_id_action;
				act_cnt->fate_action =
					MLX5_FLOW_FATE_PORT_ID;
				action_flags |= MLX5_FLOW_ACTION_PORT_ID;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_JUMP:
			{
				uint32_t jump_group = 0;
				uint32_t table = 0;
				struct mlx5_flow_tbl_data_entry *tbl_data;
				struct flow_grp_info grp_info = {
					.external = !!dev_flow.external,
					.transfer = !!transfer,
					.fdb_def_rule = !!priv->fdb_def_rule,
					.std_tbl_fix = 0,
					.skip_scale = dev_flow.skip_scale &
					(1 << MLX5_SCALE_FLOW_GROUP_BIT),
				};
				struct mlx5_flow_meter_sub_policy *sub_policy =
					mtr_policy->sub_policys[domain][0];

				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "cannot create policy "
					  "jump action for this color");
				jump_group =
				((const struct rte_flow_action_jump *)
							act->conf)->group;
				if (mlx5_flow_group_to_table(dev, NULL,
						       jump_group,
						       &table,
						       &grp_info, &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot setup "
					"policy jump action");
				sub_policy->jump_tbl[i] =
				flow_dv_tbl_resource_get(dev,
					table, egress,
					transfer,
					!!dev_flow.external,
					NULL, jump_group, 0,
					0, &flow_err);
				if
				(!sub_policy->jump_tbl[i])
					return  -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot create jump action.");
				tbl_data = container_of
				(sub_policy->jump_tbl[i],
				struct mlx5_flow_tbl_data_entry, tbl);
				act_cnt->dr_jump_action[domain] =
					tbl_data->jump.action;
				act_cnt->fate_action =
					MLX5_FLOW_FATE_JUMP;
				action_flags |= MLX5_FLOW_ACTION_JUMP;
				break;
			}
			case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			{
				if (i >= MLX5_MTR_RTE_COLORS)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL,
					  "cannot create policy modify field for this color");
				if (flow_dv_convert_action_modify_field
					(dev, mhdr_res, act, attr, &flow_err))
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot setup policy modify field action");
				if (!mhdr_res->actions_num)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "cannot find policy modify field action");
				action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
				break;
			}
			/*
			 * No need to check meter hierarchy for R colors
			 * here since it is done in the validation stage.
			 */
			case RTE_FLOW_ACTION_TYPE_METER:
			{
				const struct rte_flow_action_meter *mtr;
				struct mlx5_flow_meter_info *next_fm;
				struct mlx5_flow_meter_policy *next_policy;
				struct rte_flow_action tag_action;
				struct mlx5_rte_flow_action_set_tag set_tag;
				uint32_t next_mtr_idx = 0;

				mtr = act->conf;
				next_fm = mlx5_flow_meter_find(priv,
							mtr->mtr_id,
							&next_mtr_idx);
				if (!next_fm)
					return -rte_mtr_error_set(error, EINVAL,
						RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
						"Fail to find next meter.");
				if (next_fm->def_policy)
					return -rte_mtr_error_set(error, EINVAL,
						RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
				"Hierarchy only supports termination meter.");
				next_policy = mlx5_flow_meter_policy_find(dev,
						next_fm->policy_id, NULL);
				MLX5_ASSERT(next_policy);
				if (next_fm->drop_cnt) {
					set_tag.id =
						(enum modify_reg)
						mlx5_flow_get_reg_id(dev,
						MLX5_MTR_ID,
						0,
						(struct rte_flow_error *)error);
					set_tag.offset = (priv->mtr_reg_share ?
						MLX5_MTR_COLOR_BITS : 0);
					set_tag.length = (priv->mtr_reg_share ?
					       MLX5_MTR_IDLE_BITS_IN_COLOR_REG :
					       MLX5_REG_BITS);
					set_tag.data = next_mtr_idx;
					tag_action.type =
						(enum rte_flow_action_type)
						MLX5_RTE_FLOW_ACTION_TYPE_TAG;
					tag_action.conf = &set_tag;
					if (flow_dv_convert_action_set_reg
						(mhdr_res, &tag_action,
						(struct rte_flow_error *)error))
						return -rte_errno;
					action_flags |=
						MLX5_FLOW_ACTION_SET_TAG;
				}
				if (i == RTE_COLOR_YELLOW && next_fm->color_aware &&
				    !next_fm->meter_action_y)
					if (__flow_dv_create_mtr_yellow_action(dev, next_fm, error))
						return -rte_errno;
				act_cnt->fate_action = MLX5_FLOW_FATE_MTR;
				act_cnt->next_mtr_id = next_fm->meter_id;
				act_cnt->next_sub_policy = NULL;
				mtr_policy->is_hierarchy = 1;
				if (next_policy->mark)
					mtr_policy->mark = 1;
				mtr_policy->hierarchy_match_port =
							next_policy->hierarchy_match_port;
				action_flags |=
				MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY;
				break;
			}
			default:
				return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "action type not supported");
			}
			if ((action_flags & MLX5_FLOW_ACTION_SET_TAG) ||
			    (action_flags & MLX5_FLOW_ACTION_MODIFY_FIELD)) {
				/* create modify action if needed. */
				dev_flow.dv.group = 1;
				if (flow_dv_modify_hdr_resource_register
					(dev, mhdr_res, &dev_flow, &flow_err))
					return -rte_mtr_error_set(error,
						ENOTSUP,
						RTE_MTR_ERROR_TYPE_METER_POLICY,
						NULL, "cannot register policy set tag/modify field action");
				act_cnt->modify_hdr =
					dev_flow.handle->dvh.modify_hdr;
			}
		}
	}
	return 0;
}

/**
 * Create policy action per domain, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_policy
 *   Meter policy struct.
 * @param[in] action
 *   Action specification used to create meter actions.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
flow_dv_create_mtr_policy_acts(struct rte_eth_dev *dev,
		      struct mlx5_flow_meter_policy *mtr_policy,
		      const struct rte_flow_action *actions[RTE_COLORS],
		      struct rte_flow_attr *attr,
		      struct rte_mtr_error *error)
{
	int ret, i;
	uint16_t sub_policy_num;

	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		if (sub_policy_num) {
			ret = __flow_dv_create_domain_policy_acts(dev,
				mtr_policy, actions, attr,
				(enum mlx5_meter_domain)i, error);
			/* Cleaning resource is done in the caller level. */
			if (ret)
				return ret;
		}
	}
	return 0;
}

/**
 * Query a DV flow rule for its statistics via DevX.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] cnt_idx
 *   Index to the flow counter.
 * @param[out] data
 *   Data retrieved by the query.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_query_count(struct rte_eth_dev *dev, uint32_t cnt_idx, void *data,
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_query_count *qc = data;

	if (!priv->sh->cdev->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "counters are not supported");
	if (cnt_idx) {
		uint64_t pkts, bytes;
		struct mlx5_flow_counter *cnt;
		int err = _flow_dv_query_count(dev, cnt_idx, &pkts, &bytes);

		if (err)
			return rte_flow_error_set(error, -err,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "cannot read counters");
		cnt = flow_dv_counter_get_by_idx(dev, cnt_idx, NULL);
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

int
flow_dv_action_query(struct rte_eth_dev *dev,
		     const struct rte_flow_action_handle *handle, void *data,
		     struct rte_flow_error *error)
{
	struct mlx5_age_param *age_param;
	struct rte_flow_query_age *resp;
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_action *ct;
	uint16_t owner;
	uint32_t dev_idx;

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		age_param = &flow_aso_age_get_by_idx(dev, idx)->age_params;
		resp = data;
		resp->aged = __atomic_load_n(&age_param->state,
					      __ATOMIC_RELAXED) == AGE_TMOUT ?
									  1 : 0;
		resp->sec_since_last_hit_valid = !resp->aged;
		if (resp->sec_since_last_hit_valid)
			resp->sec_since_last_hit = __atomic_load_n
			     (&age_param->sec_since_last_hit, __ATOMIC_RELAXED);
		return 0;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		return flow_dv_query_count(dev, idx, data, error);
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		owner = (uint16_t)MLX5_INDIRECT_ACT_CT_GET_OWNER(idx);
		if (owner != PORT_ID(priv))
			return rte_flow_error_set(error, EACCES,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"CT object owned by another port");
		dev_idx = MLX5_INDIRECT_ACT_CT_GET_IDX(idx);
		ct = flow_aso_ct_get_by_dev_idx(dev, dev_idx);
		MLX5_ASSERT(ct);
		if (!ct->refcnt)
			return rte_flow_error_set(error, EFAULT,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"CT object is inactive");
		((struct rte_flow_action_conntrack *)data)->peer_port =
							ct->peer;
		((struct rte_flow_action_conntrack *)data)->is_original_dir =
							ct->is_original;
		if (mlx5_aso_ct_query_by_wqe(priv->sh, MLX5_HW_INV_QUEUE, ct,
					data, NULL, true))
			return rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"Failed to query CT context");
		return 0;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "action type query not supported");
	}
}

/**
 * Query a flow rule AGE action for aging information.
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
flow_dv_query_age(struct rte_eth_dev *dev, struct rte_flow *flow,
		  void *data, struct rte_flow_error *error)
{
	struct rte_flow_query_age *resp = data;
	struct mlx5_age_param *age_param;

	if (flow->age) {
		struct mlx5_aso_age_action *act =
				     flow_aso_age_get_by_idx(dev, flow->age);

		age_param = &act->age_params;
	} else if (flow->counter) {
		age_param = flow_dv_counter_idx_get_age(dev, flow->counter);

		if (!age_param || !age_param->timeout)
			return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					 NULL, "cannot read age data");
	} else {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "age data not available");
	}
	resp->aged = __atomic_load_n(&age_param->state, __ATOMIC_RELAXED) ==
				     AGE_TMOUT ? 1 : 0;
	resp->sec_since_last_hit_valid = !resp->aged;
	if (resp->sec_since_last_hit_valid)
		resp->sec_since_last_hit = __atomic_load_n
			     (&age_param->sec_since_last_hit, __ATOMIC_RELAXED);
	return 0;
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
			ret = flow_dv_query_count(dev, flow->counter, data,
						  error);
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			ret = flow_dv_query_age(dev, flow, data, error);
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
 * @param[in] fm
 *   Meter information table.
 */
static void
flow_dv_destroy_mtr_tbls(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_info *fm)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;

	if (!fm || !priv->sh->config.dv_flow_en)
		return;
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (fm->drop_rule[i]) {
			claim_zero(mlx5_flow_os_destroy_flow(fm->drop_rule[i]));
			fm->drop_rule[i] = NULL;
		}
	}
}

static void
flow_dv_destroy_mtr_drop_tbls(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_mtr_mng *mtrmng = priv->sh->mtrmng;
	struct mlx5_flow_tbl_data_entry *tbl;
	int i, j;

	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (mtrmng->def_rule[i]) {
			claim_zero(mlx5_flow_os_destroy_flow
					(mtrmng->def_rule[i]));
			mtrmng->def_rule[i] = NULL;
		}
		if (mtrmng->def_matcher[i]) {
			tbl = container_of(mtrmng->def_matcher[i]->tbl,
				struct mlx5_flow_tbl_data_entry, tbl);
			mlx5_list_unregister(tbl->matchers,
					     &mtrmng->def_matcher[i]->entry);
			mtrmng->def_matcher[i] = NULL;
		}
		for (j = 0; j < MLX5_REG_BITS; j++) {
			if (mtrmng->drop_matcher[i][j]) {
				tbl =
				container_of(mtrmng->drop_matcher[i][j]->tbl,
					     struct mlx5_flow_tbl_data_entry,
					     tbl);
				mlx5_list_unregister(tbl->matchers,
					    &mtrmng->drop_matcher[i][j]->entry);
				mtrmng->drop_matcher[i][j] = NULL;
			}
		}
		if (mtrmng->drop_tbl[i]) {
			flow_dv_tbl_resource_release(MLX5_SH(dev),
				mtrmng->drop_tbl[i]);
			mtrmng->drop_tbl[i] = NULL;
		}
	}
}

/* Number of meter flow actions, count and jump or count and drop. */
#define METER_ACTIONS 2

static void
__flow_dv_destroy_domain_def_policy(struct rte_eth_dev *dev,
				    enum mlx5_meter_domain domain)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_def_policy *def_policy =
			priv->sh->mtrmng->def_policy[domain];

	__flow_dv_destroy_sub_policy_rules(dev, &def_policy->sub_policy);
	mlx5_free(def_policy);
	priv->sh->mtrmng->def_policy[domain] = NULL;
}

/**
 * Destroy the default policy table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 */
static void
flow_dv_destroy_def_policy(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;

	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++)
		if (priv->sh->mtrmng->def_policy[i])
			__flow_dv_destroy_domain_def_policy(dev,
					(enum mlx5_meter_domain)i);
	priv->sh->mtrmng->def_policy_id = MLX5_INVALID_POLICY_ID;
}

static int
__flow_dv_create_policy_flow(struct rte_eth_dev *dev,
			uint32_t color_reg_c_idx,
			enum rte_color color, void *matcher_object,
			int actions_n, void *actions,
			bool match_src_port, const struct rte_flow_item *item,
			void **rule, const struct rte_flow_attr *attr)
{
	int ret;
	struct mlx5_flow_dv_match_params value = {
		.size = sizeof(value.buf),
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	uint8_t misc_mask;

	if (match_src_port && priv->sh->esw_mode) {
		if (item && item->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT)
			ret = flow_dv_translate_item_represented_port(dev, value.buf,
						item, attr, MLX5_SET_MATCHER_SW_V);
		else if (item && item->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR)
			ret = flow_dv_translate_item_port_representor(dev, value.buf,
								      MLX5_SET_MATCHER_SW_V);
		else
			ret = flow_dv_translate_item_port_id(dev, value.buf,
						item, attr, MLX5_SET_MATCHER_SW_V);
		if (ret) {
			DRV_LOG(ERR, "Failed to create meter policy%d flow's"
				" value with port.", color);
			return -1;
		}
	}
	flow_dv_match_meta_reg(value.buf, (enum modify_reg)color_reg_c_idx,
			       rte_col_2_mlx5_col(color), UINT32_MAX);
	misc_mask = flow_dv_matcher_enable(value.buf);
	__flow_dv_adjust_buf_size(&value.size, misc_mask);
	ret = mlx5_flow_os_create_flow(matcher_object, (void *)&value,
				       actions_n, actions, rule);
	if (ret) {
		DRV_LOG(ERR, "Failed to create meter policy%d flow.", color);
		return -1;
	}
	return 0;
}

static int
__flow_dv_create_policy_matcher(struct rte_eth_dev *dev,
			uint32_t color_reg_c_idx,
			uint16_t priority,
			struct mlx5_flow_meter_sub_policy *sub_policy,
			const struct rte_flow_attr *attr,
			bool match_src_port,
			const struct rte_flow_item *item,
			struct mlx5_flow_dv_matcher **policy_matcher,
			struct rte_flow_error *error)
{
	struct mlx5_list_entry *entry;
	struct mlx5_flow_tbl_resource *tbl_rsc = sub_policy->tbl_rsc;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
		.tbl = tbl_rsc,
	};
	struct mlx5_flow_cb_ctx ctx = {
		.error = error,
		.data = &matcher,
	};
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct mlx5_priv *priv = dev->data->dev_private;
	const uint32_t color_mask = (UINT32_C(1) << MLX5_MTR_COLOR_BITS) - 1;
	int ret;

	if (match_src_port && priv->sh->esw_mode) {
		if (item && item->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT)
			ret = flow_dv_translate_item_represented_port(dev, matcher.mask.buf,
						item, attr, MLX5_SET_MATCHER_SW_M);
		else if (item && item->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR)
			ret = flow_dv_translate_item_port_representor(dev, matcher.mask.buf,
								      MLX5_SET_MATCHER_SW_M);
		else
			ret = flow_dv_translate_item_port_id(dev, matcher.mask.buf,
						item, attr, MLX5_SET_MATCHER_SW_M);
		if (ret) {
			DRV_LOG(ERR, "Failed to register meter policy%d matcher"
				" with port.", priority);
			return -1;
		}
	}
	tbl_data = container_of(tbl_rsc, struct mlx5_flow_tbl_data_entry, tbl);
	if (priority < RTE_COLOR_RED)
		flow_dv_match_meta_reg(matcher.mask.buf,
			(enum modify_reg)color_reg_c_idx, color_mask, color_mask);
	matcher.priority = priority;
	matcher.crc = rte_raw_cksum((const void *)matcher.mask.buf,
				    matcher.mask.size);
	entry = mlx5_list_register(tbl_data->matchers, &ctx);
	if (!entry) {
		DRV_LOG(ERR, "Failed to register meter drop matcher.");
		return -1;
	}
	*policy_matcher =
		container_of(entry, struct mlx5_flow_dv_matcher, entry);
	return 0;
}

/**
 * Create the policy rules per domain.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] sub_policy
 *    Pointer to sub policy table..
 * @param[in] egress
 *   Direction of the table.
 * @param[in] transfer
 *   E-Switch or NIC flow.
 * @param[in] acts
 *   Pointer to policy action list per color.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
static int
__flow_dv_create_domain_policy_rules(struct rte_eth_dev *dev,
		struct mlx5_flow_meter_sub_policy *sub_policy,
		uint8_t egress, uint8_t transfer, bool match_src_port,
		struct mlx5_meter_policy_acts acts[RTE_COLORS])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_error flow_err;
	uint32_t color_reg_c_idx;
	struct rte_flow_attr attr = {
		.group = MLX5_FLOW_TABLE_LEVEL_POLICY,
		.priority = 0,
		.ingress = 0,
		.egress = !!egress,
		.transfer = !!transfer,
		.reserved = 0,
	};
	int i;
	int ret = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, &flow_err);
	struct mlx5_sub_policy_color_rule *color_rule;
	bool svport_match;
	struct mlx5_sub_policy_color_rule *tmp_rules[RTE_COLORS] = {NULL};

	if (ret < 0)
		return -1;
	/* Create policy table with POLICY level. */
	if (!sub_policy->tbl_rsc)
		sub_policy->tbl_rsc = flow_dv_tbl_resource_get(dev,
				MLX5_FLOW_TABLE_LEVEL_POLICY,
				egress, transfer, false, NULL, 0, 0,
				sub_policy->idx, &flow_err);
	if (!sub_policy->tbl_rsc) {
		DRV_LOG(ERR,
			"Failed to create meter sub policy table.");
		return -1;
	}
	/* Prepare matchers. */
	color_reg_c_idx = ret;
	for (i = 0; i < RTE_COLORS; i++) {
		TAILQ_INIT(&sub_policy->color_rules[i]);
		if (!acts[i].actions_n)
			continue;
		color_rule = mlx5_malloc(MLX5_MEM_ZERO,
				sizeof(struct mlx5_sub_policy_color_rule),
				0, SOCKET_ID_ANY);
		if (!color_rule) {
			DRV_LOG(ERR, "No memory to create color rule.");
			goto err_exit;
		}
		tmp_rules[i] = color_rule;
		TAILQ_INSERT_TAIL(&sub_policy->color_rules[i],
				  color_rule, next_port);
		color_rule->src_port = priv->representor_id;
		/* No use. */
		attr.priority = i;
		/* Create matchers for colors. */
		svport_match = (i != RTE_COLOR_RED) ? match_src_port : false;
		if (__flow_dv_create_policy_matcher(dev, color_reg_c_idx,
				MLX5_MTR_POLICY_MATCHER_PRIO, sub_policy,
				&attr, svport_match, NULL,
				&color_rule->matcher, &flow_err)) {
			DRV_LOG(ERR, "Failed to create color%u matcher.", i);
			goto err_exit;
		}
		/* Create flow, matching color. */
		if (__flow_dv_create_policy_flow(dev,
				color_reg_c_idx, (enum rte_color)i,
				color_rule->matcher->matcher_object,
				acts[i].actions_n, acts[i].dv_actions,
				svport_match, NULL, &color_rule->rule,
				&attr)) {
			DRV_LOG(ERR, "Failed to create color%u rule.", i);
			goto err_exit;
		}
	}
	return 0;
err_exit:
	/* All the policy rules will be cleared. */
	do {
		color_rule = tmp_rules[i];
		if (color_rule) {
			if (color_rule->rule)
				mlx5_flow_os_destroy_flow(color_rule->rule);
			if (color_rule->matcher) {
				struct mlx5_flow_tbl_data_entry *tbl =
					container_of(color_rule->matcher->tbl,
						     typeof(*tbl), tbl);
				mlx5_list_unregister(tbl->matchers,
						&color_rule->matcher->entry);
			}
			TAILQ_REMOVE(&sub_policy->color_rules[i],
				     color_rule, next_port);
			mlx5_free(color_rule);
		}
	} while (i--);
	return -1;
}

static int
__flow_dv_create_policy_acts_rules(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_policy *mtr_policy,
			struct mlx5_flow_meter_sub_policy *sub_policy,
			uint32_t domain)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_meter_policy_acts acts[RTE_COLORS];
	struct mlx5_flow_dv_tag_resource *tag;
	struct mlx5_flow_dv_port_id_action_resource *port_action;
	struct mlx5_hrxq *hrxq;
	struct mlx5_flow_meter_info *next_fm[RTE_COLORS] = {NULL};
	struct mlx5_flow_meter_policy *next_policy;
	struct mlx5_flow_meter_sub_policy *next_sub_policy;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct rte_flow_error error;
	uint8_t egress = (domain == MLX5_MTR_DOMAIN_EGRESS) ? 1 : 0;
	uint8_t transfer = (domain == MLX5_MTR_DOMAIN_TRANSFER) ? 1 : 0;
	bool mtr_first = egress || (transfer && priv->representor_id != UINT16_MAX);
	bool match_src_port = false;
	int i;

	/* If RSS or Queue, no previous actions / rules is created. */
	for (i = 0; i < RTE_COLORS; i++) {
		acts[i].actions_n = 0;
		if (i == RTE_COLOR_RED) {
			/* Only support drop on red. */
			acts[i].dv_actions[0] =
				mtr_policy->dr_drop_action[domain];
			acts[i].actions_n = 1;
			continue;
		}
		if (mtr_policy->act_cnt[i].fate_action == MLX5_FLOW_FATE_MTR) {
			struct rte_flow_attr attr = {
				.transfer = transfer
			};

			next_fm[i] = mlx5_flow_meter_find(priv,
					mtr_policy->act_cnt[i].next_mtr_id,
					NULL);
			if (!next_fm[i]) {
				DRV_LOG(ERR,
					"Failed to get next hierarchy meter.");
				goto err_exit;
			}
			if (mlx5_flow_meter_attach(priv, next_fm[i],
						   &attr, &error)) {
				DRV_LOG(ERR, "%s", error.message);
				next_fm[i] = NULL;
				goto err_exit;
			}
			/* Meter action must be the first for TX. */
			if (mtr_first) {
				acts[i].dv_actions[acts[i].actions_n] =
					(next_fm[i]->color_aware && i == RTE_COLOR_YELLOW) ?
						next_fm[i]->meter_action_y :
						next_fm[i]->meter_action_g;
				acts[i].actions_n++;
			}
		}
		if (mtr_policy->act_cnt[i].rix_mark) {
			tag = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_TAG],
					mtr_policy->act_cnt[i].rix_mark);
			if (!tag) {
				DRV_LOG(ERR, "Failed to find "
				"mark action for policy.");
				goto err_exit;
			}
			acts[i].dv_actions[acts[i].actions_n] = tag->action;
			acts[i].actions_n++;
		}
		if (mtr_policy->act_cnt[i].modify_hdr) {
			acts[i].dv_actions[acts[i].actions_n] =
				mtr_policy->act_cnt[i].modify_hdr->action;
			acts[i].actions_n++;
		}
		if (mtr_policy->act_cnt[i].fate_action) {
			switch (mtr_policy->act_cnt[i].fate_action) {
			case MLX5_FLOW_FATE_PORT_ID:
				port_action = mlx5_ipool_get
					(priv->sh->ipool[MLX5_IPOOL_PORT_ID],
				mtr_policy->act_cnt[i].rix_port_id_action);
				if (!port_action) {
					DRV_LOG(ERR, "Failed to find "
						"port action for policy.");
					goto err_exit;
				}
				acts[i].dv_actions[acts[i].actions_n] =
					port_action->action;
				acts[i].actions_n++;
				match_src_port = true;
				break;
			case MLX5_FLOW_FATE_DROP:
			case MLX5_FLOW_FATE_JUMP:
				acts[i].dv_actions[acts[i].actions_n] =
				mtr_policy->act_cnt[i].dr_jump_action[domain];
				acts[i].actions_n++;
				break;
			case MLX5_FLOW_FATE_SHARED_RSS:
			case MLX5_FLOW_FATE_QUEUE:
				hrxq = mlx5_ipool_get
					(priv->sh->ipool[MLX5_IPOOL_HRXQ],
					 sub_policy->rix_hrxq[i]);
				if (!hrxq) {
					DRV_LOG(ERR, "Failed to find "
						"queue action for policy.");
					goto err_exit;
				}
				acts[i].dv_actions[acts[i].actions_n] =
					hrxq->action;
				acts[i].actions_n++;
				break;
			case MLX5_FLOW_FATE_MTR:
				if (!next_fm[i]) {
					DRV_LOG(ERR,
						"No next hierarchy meter.");
					goto err_exit;
				}
				if (!mtr_first) {
					acts[i].dv_actions[acts[i].actions_n] =
						(next_fm[i]->color_aware && i == RTE_COLOR_YELLOW) ?
							next_fm[i]->meter_action_y :
							next_fm[i]->meter_action_g;
					acts[i].actions_n++;
				}
				if (mtr_policy->act_cnt[i].next_sub_policy) {
					next_sub_policy =
					mtr_policy->act_cnt[i].next_sub_policy;
				} else {
					next_policy =
						mlx5_flow_meter_policy_find(dev,
								next_fm[i]->policy_id, NULL);
					MLX5_ASSERT(next_policy);
					next_sub_policy =
					next_policy->sub_policys[domain][0];
				}
				tbl_data =
					container_of(next_sub_policy->tbl_rsc,
					struct mlx5_flow_tbl_data_entry, tbl);
				acts[i].dv_actions[acts[i].actions_n++] =
							tbl_data->jump.action;
				if (mtr_policy->act_cnt[i].modify_hdr)
					match_src_port = !!transfer;
				break;
			default:
				/*Queue action do nothing*/
				break;
			}
		}
	}
	if (__flow_dv_create_domain_policy_rules(dev, sub_policy,
				egress, transfer, match_src_port, acts)) {
		DRV_LOG(ERR,
			"Failed to create policy rules per domain.");
		goto err_exit;
	}
	if (match_src_port) {
		mtr_policy->match_port = match_src_port;
		mtr_policy->hierarchy_match_port = match_src_port;
	}
	return 0;
err_exit:
	for (i = 0; i < RTE_COLORS; i++)
		if (next_fm[i])
			mlx5_flow_meter_detach(priv, next_fm[i]);
	return -1;
}

/**
 * Create the policy rules.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in,out] mtr_policy
 *   Pointer to meter policy table.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
static int
flow_dv_create_policy_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter_policy *mtr_policy)
{
	int i;
	int ret = 0;
	uint16_t sub_policy_num;
	struct mlx5_flow_workspace *wks = mlx5_flow_push_thread_workspace();

	RTE_SET_USED(wks);
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		if (!sub_policy_num)
			continue;
		/* Prepare actions list and create policy rules. */
		if (__flow_dv_create_policy_acts_rules(dev, mtr_policy,
			mtr_policy->sub_policys[i][0], i)) {
			DRV_LOG(ERR, "Failed to create policy action "
				"list per domain.");
			ret = -1;
			goto exit;
		}
	}
exit:
	mlx5_flow_pop_thread_workspace();
	return ret;
}

static int
__flow_dv_create_domain_def_policy(struct rte_eth_dev *dev, uint32_t domain)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_mtr_mng *mtrmng = priv->sh->mtrmng;
	struct mlx5_flow_meter_def_policy *def_policy;
	struct mlx5_flow_tbl_resource *jump_tbl;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	uint8_t egress, transfer;
	struct rte_flow_error error;
	struct mlx5_meter_policy_acts acts[RTE_COLORS];
	int ret;

	egress = (domain == MLX5_MTR_DOMAIN_EGRESS) ? 1 : 0;
	transfer = (domain == MLX5_MTR_DOMAIN_TRANSFER) ? 1 : 0;
	def_policy = mtrmng->def_policy[domain];
	if (!def_policy) {
		def_policy = mlx5_malloc(MLX5_MEM_ZERO,
			sizeof(struct mlx5_flow_meter_def_policy),
			RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!def_policy) {
			DRV_LOG(ERR, "Failed to alloc default policy table.");
			goto def_policy_error;
		}
		mtrmng->def_policy[domain] = def_policy;
		/* Create the meter suffix table with SUFFIX level. */
		jump_tbl = flow_dv_tbl_resource_get(dev,
				MLX5_FLOW_TABLE_LEVEL_METER,
				egress, transfer, false, NULL, 0,
				0, MLX5_MTR_TABLE_ID_SUFFIX, &error);
		if (!jump_tbl) {
			DRV_LOG(ERR,
				"Failed to create meter suffix table.");
			goto def_policy_error;
		}
		def_policy->sub_policy.jump_tbl[RTE_COLOR_GREEN] = jump_tbl;
		tbl_data = container_of(jump_tbl,
					struct mlx5_flow_tbl_data_entry, tbl);
		def_policy->dr_jump_action[RTE_COLOR_GREEN] =
						tbl_data->jump.action;
		acts[RTE_COLOR_GREEN].dv_actions[0] = tbl_data->jump.action;
		acts[RTE_COLOR_GREEN].actions_n = 1;
		/*
		 * YELLOW has the same default policy as GREEN does.
		 * G & Y share the same table and action. The 2nd time of table
		 * resource getting is just to update the reference count for
		 * the releasing stage.
		 */
		jump_tbl = flow_dv_tbl_resource_get(dev,
				MLX5_FLOW_TABLE_LEVEL_METER,
				egress, transfer, false, NULL, 0,
				0, MLX5_MTR_TABLE_ID_SUFFIX, &error);
		if (!jump_tbl) {
			DRV_LOG(ERR,
				"Failed to get meter suffix table.");
			goto def_policy_error;
		}
		def_policy->sub_policy.jump_tbl[RTE_COLOR_YELLOW] = jump_tbl;
		tbl_data = container_of(jump_tbl,
					struct mlx5_flow_tbl_data_entry, tbl);
		def_policy->dr_jump_action[RTE_COLOR_YELLOW] =
						tbl_data->jump.action;
		acts[RTE_COLOR_YELLOW].dv_actions[0] = tbl_data->jump.action;
		acts[RTE_COLOR_YELLOW].actions_n = 1;
		/* Create jump action to the drop table. */
		if (!mtrmng->drop_tbl[domain]) {
			mtrmng->drop_tbl[domain] = flow_dv_tbl_resource_get
				(dev, MLX5_FLOW_TABLE_LEVEL_METER,
				 egress, transfer, false, NULL, 0,
				 0, MLX5_MTR_TABLE_ID_DROP, &error);
			if (!mtrmng->drop_tbl[domain]) {
				DRV_LOG(ERR, "Failed to create meter "
					"drop table for default policy.");
				goto def_policy_error;
			}
		}
		/* all RED: unique Drop table for jump action. */
		tbl_data = container_of(mtrmng->drop_tbl[domain],
					struct mlx5_flow_tbl_data_entry, tbl);
		def_policy->dr_jump_action[RTE_COLOR_RED] =
						tbl_data->jump.action;
		acts[RTE_COLOR_RED].dv_actions[0] = tbl_data->jump.action;
		acts[RTE_COLOR_RED].actions_n = 1;
		/* Create default policy rules. */
		ret = __flow_dv_create_domain_policy_rules(dev,
					&def_policy->sub_policy,
					egress, transfer, false, acts);
		if (ret) {
			DRV_LOG(ERR, "Failed to create default policy rules.");
			goto def_policy_error;
		}
	}
	return 0;
def_policy_error:
	__flow_dv_destroy_domain_def_policy(dev,
					    (enum mlx5_meter_domain)domain);
	return -1;
}

/**
 * Create the default policy table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @return
 *   0 on success, -1 otherwise.
 */
static int
flow_dv_create_def_policy(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;

	/* Non-termination policy table. */
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (!priv->sh->config.dv_esw_en &&
		    i == MLX5_MTR_DOMAIN_TRANSFER)
			continue;
		if (__flow_dv_create_domain_def_policy(dev, i)) {
			DRV_LOG(ERR, "Failed to create default policy");
			/* Rollback the created default policies for others. */
			flow_dv_destroy_def_policy(dev);
			return -1;
		}
	}
	return 0;
}

/**
 * Create the needed meter tables.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Meter information table.
 * @param[in] mtr_idx
 *   Meter index.
 * @param[in] domain_bitmap
 *   Domain bitmap.
 * @return
 *   0 on success, -1 otherwise.
 */
static int
flow_dv_create_mtr_tbls(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_info *fm,
			uint32_t mtr_idx,
			uint8_t domain_bitmap)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_mtr_mng *mtrmng = priv->sh->mtrmng;
	struct rte_flow_error error;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	uint8_t egress, transfer;
	void *actions[METER_ACTIONS];
	int domain, ret, i;
	struct mlx5_flow_counter *cnt;
	struct mlx5_flow_dv_match_params value = {
		.size = sizeof(value.buf),
	};
	struct mlx5_flow_dv_match_params matcher_para = {
		.size = sizeof(matcher_para.buf),
	};
	int mtr_id_reg_c = mlx5_flow_get_reg_id(dev, MLX5_MTR_ID,
						     0, &error);
	uint32_t mtr_id_mask = (UINT32_C(1) << mtrmng->max_mtr_bits) - 1;
	uint8_t mtr_id_offset = priv->mtr_reg_share ? MLX5_MTR_COLOR_BITS : 0;
	struct mlx5_list_entry *entry;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
	};
	struct mlx5_flow_dv_matcher *drop_matcher;
	struct mlx5_flow_cb_ctx ctx = {
		.error = &error,
		.data = &matcher,
	};
	uint8_t misc_mask;

	if (!priv->mtr_en || mtr_id_reg_c < 0) {
		rte_errno = ENOTSUP;
		return -1;
	}
	for (domain = 0; domain < MLX5_MTR_DOMAIN_MAX; domain++) {
		if (!(domain_bitmap & (1 << domain)) ||
			(mtrmng->def_rule[domain] && !fm->drop_cnt))
			continue;
		egress = (domain == MLX5_MTR_DOMAIN_EGRESS) ? 1 : 0;
		transfer = (domain == MLX5_MTR_DOMAIN_TRANSFER) ? 1 : 0;
		/* Create the drop table with METER DROP level. */
		if (!mtrmng->drop_tbl[domain]) {
			mtrmng->drop_tbl[domain] = flow_dv_tbl_resource_get(dev,
					MLX5_FLOW_TABLE_LEVEL_METER,
					egress, transfer, false, NULL, 0,
					0, MLX5_MTR_TABLE_ID_DROP, &error);
			if (!mtrmng->drop_tbl[domain]) {
				DRV_LOG(ERR, "Failed to create meter drop table.");
				goto policy_error;
			}
		}
		/* Create default matcher in drop table. */
		matcher.tbl = mtrmng->drop_tbl[domain],
		tbl_data = container_of(mtrmng->drop_tbl[domain],
				struct mlx5_flow_tbl_data_entry, tbl);
		if (!mtrmng->def_matcher[domain]) {
			flow_dv_match_meta_reg_all(matcher.mask.buf, value.buf,
				       (enum modify_reg)mtr_id_reg_c,
				       0, 0);
			matcher.priority = MLX5_MTRS_DEFAULT_RULE_PRIORITY;
			matcher.crc = rte_raw_cksum
					((const void *)matcher.mask.buf,
					matcher.mask.size);
			entry = mlx5_list_register(tbl_data->matchers, &ctx);
			if (!entry) {
				DRV_LOG(ERR, "Failed to register meter "
				"drop default matcher.");
				goto policy_error;
			}
			mtrmng->def_matcher[domain] = container_of(entry,
			struct mlx5_flow_dv_matcher, entry);
		}
		/* Create default rule in drop table. */
		if (!mtrmng->def_rule[domain]) {
			i = 0;
			actions[i++] = priv->sh->dr_drop_action;
			flow_dv_match_meta_reg_all(matcher_para.buf, value.buf,
				(enum modify_reg)mtr_id_reg_c, 0, 0);
			misc_mask = flow_dv_matcher_enable(value.buf);
			__flow_dv_adjust_buf_size(&value.size, misc_mask);
			ret = mlx5_flow_os_create_flow
				(mtrmng->def_matcher[domain]->matcher_object,
				(void *)&value, i, actions,
				&mtrmng->def_rule[domain]);
			if (ret) {
				DRV_LOG(ERR, "Failed to create meter "
				"default drop rule for drop table.");
				goto policy_error;
			}
		}
		if (!fm->drop_cnt)
			continue;
		MLX5_ASSERT(mtrmng->max_mtr_bits);
		if (!mtrmng->drop_matcher[domain][mtrmng->max_mtr_bits - 1]) {
			/* Create matchers for Drop. */
			flow_dv_match_meta_reg_all(matcher.mask.buf, value.buf,
					(enum modify_reg)mtr_id_reg_c, 0,
					(mtr_id_mask << mtr_id_offset));
			matcher.priority = MLX5_REG_BITS - mtrmng->max_mtr_bits;
			matcher.crc = rte_raw_cksum
					((const void *)matcher.mask.buf,
					matcher.mask.size);
			entry = mlx5_list_register(tbl_data->matchers, &ctx);
			if (!entry) {
				DRV_LOG(ERR,
				"Failed to register meter drop matcher.");
				goto policy_error;
			}
			mtrmng->drop_matcher[domain][mtrmng->max_mtr_bits - 1] =
				container_of(entry, struct mlx5_flow_dv_matcher,
					     entry);
		}
		drop_matcher =
			mtrmng->drop_matcher[domain][mtrmng->max_mtr_bits - 1];
		/* Create drop rule, matching meter_id only. */
		flow_dv_match_meta_reg_all(matcher_para.buf, value.buf,
				(enum modify_reg)mtr_id_reg_c,
				(mtr_idx << mtr_id_offset), UINT32_MAX);
		i = 0;
		cnt = flow_dv_counter_get_by_idx(dev,
					fm->drop_cnt, NULL);
		actions[i++] = cnt->action;
		actions[i++] = priv->sh->dr_drop_action;
		misc_mask = flow_dv_matcher_enable(value.buf);
		__flow_dv_adjust_buf_size(&value.size, misc_mask);
		ret = mlx5_flow_os_create_flow(drop_matcher->matcher_object,
					       (void *)&value, i, actions,
					       &fm->drop_rule[domain]);
		if (ret) {
			DRV_LOG(ERR, "Failed to create meter "
				"drop rule for drop table.");
				goto policy_error;
		}
	}
	return 0;
policy_error:
	for (i = 0; i < MLX5_MTR_DOMAIN_MAX; i++) {
		if (fm->drop_rule[i]) {
			claim_zero(mlx5_flow_os_destroy_flow
				(fm->drop_rule[i]));
			fm->drop_rule[i] = NULL;
		}
	}
	return -1;
}

static struct mlx5_flow_meter_sub_policy *
__flow_dv_meter_get_rss_sub_policy(struct rte_eth_dev *dev,
		struct mlx5_flow_meter_policy *mtr_policy,
		struct mlx5_flow_rss_desc *rss_desc[MLX5_MTR_RTE_COLORS],
		struct mlx5_flow_meter_sub_policy *next_sub_policy,
		bool *is_reuse)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy = NULL;
	uint32_t sub_policy_idx = 0;
	uint32_t hrxq_idx[MLX5_MTR_RTE_COLORS] = {0};
	uint32_t i, j;
	struct mlx5_hrxq *hrxq;
	struct mlx5_flow_handle dh;
	struct mlx5_meter_policy_action_container *act_cnt;
	uint32_t domain = MLX5_MTR_DOMAIN_INGRESS;
	uint16_t sub_policy_num;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	MLX5_ASSERT(wks);
	rte_spinlock_lock(&mtr_policy->sl);
	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (!rss_desc[i])
			continue;
		hrxq = mlx5_hrxq_get(dev, rss_desc[i]);
		if (!hrxq) {
			rte_spinlock_unlock(&mtr_policy->sl);
			return NULL;
		}
		hrxq_idx[i] = hrxq->idx;
	}
	sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
	for (j = 0; j < sub_policy_num; j++) {
		for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
			if (rss_desc[i] &&
			    hrxq_idx[i] !=
			    mtr_policy->sub_policys[domain][j]->rix_hrxq[i])
				break;
		}
		if (i >= MLX5_MTR_RTE_COLORS) {
			/*
			 * Found the sub policy table with
			 * the same queue per color.
			 */
			rte_spinlock_unlock(&mtr_policy->sl);
			for (i = 0; i < MLX5_MTR_RTE_COLORS; i++)
				mlx5_hrxq_release(dev, hrxq_idx[i]);
			*is_reuse = true;
			return mtr_policy->sub_policys[domain][j];
		}
	}
	/* Create sub policy. */
	if (!mtr_policy->sub_policys[domain][0]->rix_hrxq[RTE_COLOR_GREEN] &&
	    !mtr_policy->sub_policys[domain][0]->rix_hrxq[RTE_COLOR_YELLOW]) {
		/* Reuse the first pre-allocated sub_policy. */
		sub_policy = mtr_policy->sub_policys[domain][0];
		sub_policy_idx = sub_policy->idx;
	} else {
		sub_policy = mlx5_ipool_zmalloc
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
				 &sub_policy_idx);
		if (!sub_policy ||
		    sub_policy_idx > MLX5_MAX_SUB_POLICY_TBL_NUM) {
			for (i = 0; i < MLX5_MTR_RTE_COLORS; i++)
				mlx5_hrxq_release(dev, hrxq_idx[i]);
			goto rss_sub_policy_error;
		}
		sub_policy->idx = sub_policy_idx;
		sub_policy->main_policy = mtr_policy;
	}
	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (!rss_desc[i])
			continue;
		sub_policy->rix_hrxq[i] = hrxq_idx[i];
		if (mtr_policy->is_hierarchy) {
			act_cnt = &mtr_policy->act_cnt[i];
			act_cnt->next_sub_policy = next_sub_policy;
			mlx5_hrxq_release(dev, hrxq_idx[i]);
		} else {
			/*
			 * Overwrite the last action from
			 * RSS action to Queue action.
			 */
			hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ],
					      hrxq_idx[i]);
			if (!hrxq) {
				DRV_LOG(ERR, "Failed to get policy hrxq");
				goto rss_sub_policy_error;
			}
			act_cnt = &mtr_policy->act_cnt[i];
			if (act_cnt->rix_mark || act_cnt->modify_hdr) {
				memset(&dh, 0, sizeof(struct mlx5_flow_handle));
				if (act_cnt->rix_mark)
					wks->mark = 1;
				dh.fate_action = MLX5_FLOW_FATE_QUEUE;
				dh.rix_hrxq = hrxq_idx[i];
				flow_drv_rxq_flags_set(dev, &dh);
			}
		}
	}
	if (__flow_dv_create_policy_acts_rules(dev, mtr_policy,
					       sub_policy, domain)) {
		DRV_LOG(ERR, "Failed to create policy "
			"rules for ingress domain.");
		goto rss_sub_policy_error;
	}
	if (sub_policy != mtr_policy->sub_policys[domain][0]) {
		i = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
		if (i >= MLX5_MTR_RSS_MAX_SUB_POLICY) {
			DRV_LOG(ERR, "No free sub-policy slot.");
			goto rss_sub_policy_error;
		}
		mtr_policy->sub_policys[domain][i] = sub_policy;
		i++;
		mtr_policy->sub_policy_num &= ~(MLX5_MTR_SUB_POLICY_NUM_MASK <<
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain));
		mtr_policy->sub_policy_num |=
			(i & MLX5_MTR_SUB_POLICY_NUM_MASK) <<
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain);
	}
	rte_spinlock_unlock(&mtr_policy->sl);
	*is_reuse = false;
	return sub_policy;
rss_sub_policy_error:
	if (sub_policy) {
		__flow_dv_destroy_sub_policy_rules(dev, sub_policy);
		if (sub_policy != mtr_policy->sub_policys[domain][0]) {
			i = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
			mtr_policy->sub_policys[domain][i] = NULL;
			mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
					sub_policy->idx);
		}
	}
	rte_spinlock_unlock(&mtr_policy->sl);
	return NULL;
}

/**
 * Find the policy table for prefix table with RSS.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy
 *   Pointer to meter policy table.
 * @param[in] rss_desc
 *   Pointer to rss_desc
 * @return
 *   Pointer to table set on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_meter_sub_policy *
flow_dv_meter_sub_policy_rss_prepare(struct rte_eth_dev *dev,
		struct mlx5_flow_meter_policy *mtr_policy,
		struct mlx5_flow_rss_desc *rss_desc[MLX5_MTR_RTE_COLORS])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy = NULL;
	struct mlx5_flow_meter_info *next_fm;
	struct mlx5_flow_meter_policy *next_policy;
	struct mlx5_flow_meter_sub_policy *next_sub_policy = NULL;
	struct mlx5_flow_meter_policy *policies[MLX5_MTR_CHAIN_MAX_NUM];
	struct mlx5_flow_meter_sub_policy *sub_policies[MLX5_MTR_CHAIN_MAX_NUM];
	uint32_t domain = MLX5_MTR_DOMAIN_INGRESS;
	bool reuse_sub_policy;
	uint32_t i = 0;
	uint32_t j = 0;

	while (true) {
		/* Iterate hierarchy to get all policies in this hierarchy. */
		policies[i++] = mtr_policy;
		if (!mtr_policy->is_hierarchy)
			break;
		if (i >= MLX5_MTR_CHAIN_MAX_NUM) {
			DRV_LOG(ERR, "Exceed max meter number in hierarchy.");
			return NULL;
		}
		rte_spinlock_lock(&mtr_policy->sl);
		next_fm = mlx5_flow_meter_hierarchy_next_meter(priv, mtr_policy, NULL);
		rte_spinlock_unlock(&mtr_policy->sl);
		if (!next_fm) {
			DRV_LOG(ERR, "Failed to get next meter in hierarchy.");
			return NULL;
		}
		next_policy =
			mlx5_flow_meter_policy_find(dev, next_fm->policy_id,
						    NULL);
		MLX5_ASSERT(next_policy);
		mtr_policy = next_policy;
	}
	while (i) {
		/**
		 * From last policy to the first one in hierarchy,
		 * create / get the sub policy for each of them.
		 */
		sub_policy = __flow_dv_meter_get_rss_sub_policy(dev,
							policies[--i],
							rss_desc,
							next_sub_policy,
							&reuse_sub_policy);
		if (!sub_policy) {
			DRV_LOG(ERR, "Failed to get the sub policy.");
			goto err_exit;
		}
		if (!reuse_sub_policy)
			sub_policies[j++] = sub_policy;
		next_sub_policy = sub_policy;
	}
	return sub_policy;
err_exit:
	while (j) {
		uint16_t sub_policy_num;

		sub_policy = sub_policies[--j];
		mtr_policy = sub_policy->main_policy;
		__flow_dv_destroy_sub_policy_rules(dev, sub_policy);
		if (sub_policy != mtr_policy->sub_policys[domain][0]) {
			sub_policy_num = (mtr_policy->sub_policy_num >>
				(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain)) &
				MLX5_MTR_SUB_POLICY_NUM_MASK;
			mtr_policy->sub_policys[domain][sub_policy_num - 1] =
									NULL;
			sub_policy_num--;
			mtr_policy->sub_policy_num &=
				~(MLX5_MTR_SUB_POLICY_NUM_MASK <<
				  (MLX5_MTR_SUB_POLICY_NUM_SHIFT * i));
			mtr_policy->sub_policy_num |=
			(sub_policy_num & MLX5_MTR_SUB_POLICY_NUM_MASK) <<
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * i);
			mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
					sub_policy->idx);
		}
	}
	return NULL;
}

/**
 * Check if need to create hierarchy tag rule.
 *
 * @param[in] priv
 *   Pointer to mlx5_priv.
 * @param[in] mtr_policy
 *   Pointer to current meter policy.
 * @param[in] src_port
 *   The src port this extra rule should use.
 * @param[out] next_fm
 *   Pointer to next meter in hierarchy.
 * @param[out] skip
 *   Indicate if skip the tag rule creation.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_meter_hierarchy_skip_tag_rule(struct mlx5_priv *priv,
				   struct mlx5_flow_meter_policy *mtr_policy,
				   int32_t src_port,
				   struct mlx5_flow_meter_info **next_fm,
				   bool *skip,
				   struct rte_flow_error *error)
{
	struct mlx5_flow_meter_sub_policy *sub_policy;
	struct mlx5_sub_policy_color_rule *color_rule;
	uint32_t domain = MLX5_MTR_DOMAIN_TRANSFER;
	int ret = 0;
	int i;

	*next_fm = NULL;
	*skip = false;
	rte_spinlock_lock(&mtr_policy->sl);
	if (mtr_policy->is_hierarchy) {
		*next_fm = mlx5_flow_meter_hierarchy_next_meter(priv, mtr_policy, NULL);
		if (!*next_fm) {
			ret = rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						NULL, "Failed to find next meter in hierarchy.");
			goto exit;
		}
	}
	if (!mtr_policy->match_port) {
		*skip = true;
		goto exit;
	}
	sub_policy = mtr_policy->sub_policys[domain][0];
	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		if (mtr_policy->act_cnt[i].fate_action != MLX5_FLOW_FATE_MTR &&
		    mtr_policy->act_cnt[i].fate_action != MLX5_FLOW_FATE_PORT_ID)
			continue;
		TAILQ_FOREACH(color_rule, &sub_policy->color_rules[i], next_port)
			if (color_rule->src_port == src_port) {
				*skip = true;
				goto exit;
			}
	}
exit:
	rte_spinlock_unlock(&mtr_policy->sl);
	return ret;
}

/**
 * Create the sub policy tag rule for all meters in hierarchy.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Meter information table.
 * @param[in] src_port
 *   The src port this extra rule should use.
 * @param[in] item
 *   The src port match item.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_dv_meter_hierarchy_rule_create(struct rte_eth_dev *dev,
				struct mlx5_flow_meter_info *fm,
				int32_t src_port,
				const struct rte_flow_item *item,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_policy *mtr_policy;
	struct mlx5_flow_meter_sub_policy *sub_policy;
	struct mlx5_flow_meter_info *next_fm = NULL;
	struct mlx5_flow_meter_policy *next_policy;
	struct mlx5_flow_meter_sub_policy *next_sub_policy;
	struct mlx5_flow_tbl_data_entry *tbl_data;
	struct mlx5_sub_policy_color_rule *color_rule;
	struct mlx5_meter_policy_acts acts;
	uint32_t color_reg_c_idx;
	bool mtr_first = (src_port != UINT16_MAX) ? true : false;
	struct rte_flow_attr attr = {
		.group = MLX5_FLOW_TABLE_LEVEL_POLICY,
		.priority = 0,
		.ingress = 0,
		.egress = 0,
		.transfer = 1,
		.reserved = 0,
	};
	uint32_t domain = MLX5_MTR_DOMAIN_TRANSFER;
	struct {
		struct mlx5_flow_meter_policy *fm_policy;
		struct mlx5_flow_meter_info *next_fm;
		struct mlx5_sub_policy_color_rule *tag_rule[MLX5_MTR_RTE_COLORS];
	} fm_info[MLX5_MTR_CHAIN_MAX_NUM] = { {0} };
	uint32_t fm_cnt = 0;
	uint32_t i, j;

	color_reg_c_idx = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, error);
	/* Get all fms who need to create the tag color rule. */
	do {
		bool skip = false;

		mtr_policy = mlx5_flow_meter_policy_find(dev, fm->policy_id, NULL);
		MLX5_ASSERT(mtr_policy);
		if (mlx5_meter_hierarchy_skip_tag_rule(priv, mtr_policy, src_port,
						       &next_fm, &skip, error))
			goto err_exit;
		if (!skip) {
			fm_info[fm_cnt].fm_policy = mtr_policy;
			fm_info[fm_cnt].next_fm = next_fm;
			if (++fm_cnt >= MLX5_MTR_CHAIN_MAX_NUM) {
				rte_flow_error_set(error, errno,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"Exceed max meter number in hierarchy.");
				goto err_exit;
			}
		}
		fm = next_fm;
	} while (fm);
	/* Create tag color rules for all needed fms. */
	for (i = 0; i < fm_cnt; i++) {
		void *mtr_action;

		mtr_policy = fm_info[i].fm_policy;
		rte_spinlock_lock(&mtr_policy->sl);
		sub_policy = mtr_policy->sub_policys[domain][0];
		for (j = 0; j < MLX5_MTR_RTE_COLORS; j++) {
			uint8_t act_n = 0;
			struct mlx5_flow_dv_modify_hdr_resource *modify_hdr;
			struct mlx5_flow_dv_port_id_action_resource *port_action;

			if (mtr_policy->act_cnt[j].fate_action != MLX5_FLOW_FATE_MTR &&
			    mtr_policy->act_cnt[j].fate_action != MLX5_FLOW_FATE_PORT_ID)
				continue;
			color_rule = mlx5_malloc(MLX5_MEM_ZERO,
						 sizeof(struct mlx5_sub_policy_color_rule),
						 0, SOCKET_ID_ANY);
			if (!color_rule) {
				rte_spinlock_unlock(&mtr_policy->sl);
				rte_flow_error_set(error, ENOMEM,
						   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
						   "No memory to create tag color rule.");
				goto err_exit;
			}
			color_rule->src_port = src_port;
			modify_hdr = mtr_policy->act_cnt[j].modify_hdr;
			/* Prepare to create color rule. */
			if (mtr_policy->act_cnt[j].fate_action == MLX5_FLOW_FATE_MTR) {
				next_fm = fm_info[i].next_fm;
				if (mlx5_flow_meter_attach(priv, next_fm, &attr, error)) {
					mlx5_free(color_rule);
					rte_spinlock_unlock(&mtr_policy->sl);
					goto err_exit;
				}
				mtr_action = (next_fm->color_aware && j == RTE_COLOR_YELLOW) ?
									next_fm->meter_action_y :
									next_fm->meter_action_g;
				next_policy = mlx5_flow_meter_policy_find(dev, next_fm->policy_id,
									  NULL);
				MLX5_ASSERT(next_policy);
				next_sub_policy = next_policy->sub_policys[domain][0];
				tbl_data = container_of(next_sub_policy->tbl_rsc,
							struct mlx5_flow_tbl_data_entry, tbl);
				if (mtr_first) {
					acts.dv_actions[act_n++] = mtr_action;
					if (modify_hdr)
						acts.dv_actions[act_n++] = modify_hdr->action;
				} else {
					if (modify_hdr)
						acts.dv_actions[act_n++] = modify_hdr->action;
					acts.dv_actions[act_n++] = mtr_action;
				}
				acts.dv_actions[act_n++] = tbl_data->jump.action;
				acts.actions_n = act_n;
			} else {
				port_action =
					mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_PORT_ID],
						       mtr_policy->act_cnt[j].rix_port_id_action);
				if (!port_action) {
					mlx5_free(color_rule);
					rte_spinlock_unlock(&mtr_policy->sl);
					goto err_exit;
				}
				if (modify_hdr)
					acts.dv_actions[act_n++] = modify_hdr->action;
				acts.dv_actions[act_n++] = port_action->action;
				acts.actions_n = act_n;
			}
			fm_info[i].tag_rule[j] = color_rule;
			TAILQ_INSERT_TAIL(&sub_policy->color_rules[j], color_rule, next_port);
			if (__flow_dv_create_policy_matcher(dev, color_reg_c_idx,
						MLX5_MTR_POLICY_MATCHER_PRIO, sub_policy,
						&attr, true, item, &color_rule->matcher, error)) {
				rte_spinlock_unlock(&mtr_policy->sl);
				rte_flow_error_set(error, errno,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						   "Failed to create hierarchy meter matcher.");
				goto err_exit;
			}
			if (__flow_dv_create_policy_flow(dev, color_reg_c_idx, (enum rte_color)j,
						color_rule->matcher->matcher_object,
						acts.actions_n, acts.dv_actions,
						true, item, &color_rule->rule, &attr)) {
				rte_spinlock_unlock(&mtr_policy->sl);
				rte_flow_error_set(error, errno,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						   "Failed to create hierarchy meter rule.");
				goto err_exit;
			}
		}
		rte_spinlock_unlock(&mtr_policy->sl);
	}
	return 0;
err_exit:
	for (i = 0; i < fm_cnt; i++) {
		mtr_policy = fm_info[i].fm_policy;
		rte_spinlock_lock(&mtr_policy->sl);
		sub_policy = mtr_policy->sub_policys[domain][0];
		for (j = 0; j < MLX5_MTR_RTE_COLORS; j++) {
			color_rule = fm_info[i].tag_rule[j];
			if (!color_rule)
				continue;
			if (color_rule->rule)
				mlx5_flow_os_destroy_flow(color_rule->rule);
			if (color_rule->matcher) {
				struct mlx5_flow_tbl_data_entry *tbl =
					container_of(color_rule->matcher->tbl, typeof(*tbl), tbl);
				mlx5_list_unregister(tbl->matchers, &color_rule->matcher->entry);
			}
			if (fm_info[i].next_fm)
				mlx5_flow_meter_detach(priv, fm_info[i].next_fm);
			TAILQ_REMOVE(&sub_policy->color_rules[j], color_rule, next_port);
			mlx5_free(color_rule);
		}
		rte_spinlock_unlock(&mtr_policy->sl);
	}
	return -rte_errno;
}

/**
 * Destroy the sub policy table with RX queue.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy
 *   Pointer to meter policy table.
 */
static void
flow_dv_destroy_sub_policy_with_rxq(struct rte_eth_dev *dev,
				    struct mlx5_flow_meter_policy *mtr_policy)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_sub_policy *sub_policy = NULL;
	uint32_t domain = MLX5_MTR_DOMAIN_INGRESS;
	uint32_t i, j;
	uint16_t sub_policy_num, new_policy_num;

	rte_spinlock_lock(&mtr_policy->sl);
	for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
		switch (mtr_policy->act_cnt[i].fate_action) {
		case MLX5_FLOW_FATE_SHARED_RSS:
			sub_policy_num = (mtr_policy->sub_policy_num >>
			(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain)) &
			MLX5_MTR_SUB_POLICY_NUM_MASK;
			new_policy_num = sub_policy_num;
			for (j = 0; j < sub_policy_num; j++) {
				sub_policy =
					mtr_policy->sub_policys[domain][j];
				if (sub_policy) {
					__flow_dv_destroy_sub_policy_rules(dev,
						sub_policy);
				if (sub_policy !=
					mtr_policy->sub_policys[domain][0]) {
					mtr_policy->sub_policys[domain][j] =
								NULL;
					mlx5_ipool_free
				(priv->sh->ipool[MLX5_IPOOL_MTR_POLICY],
						sub_policy->idx);
						new_policy_num--;
					}
				}
			}
			if (new_policy_num != sub_policy_num) {
				mtr_policy->sub_policy_num &=
				~(MLX5_MTR_SUB_POLICY_NUM_MASK <<
				(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain));
				mtr_policy->sub_policy_num |=
				(new_policy_num &
					MLX5_MTR_SUB_POLICY_NUM_MASK) <<
				(MLX5_MTR_SUB_POLICY_NUM_SHIFT * domain);
			}
			break;
		case MLX5_FLOW_FATE_QUEUE:
			sub_policy = mtr_policy->sub_policys[domain][0];
			__flow_dv_destroy_sub_policy_rules(dev,
							   sub_policy);
			break;
		default:
			/*Other actions without queue and do nothing*/
			break;
		}
	}
	rte_spinlock_unlock(&mtr_policy->sl);
}
/**
 * Check whether the DR drop action is supported on the root table or not.
 *
 * Create a simple flow with DR drop action on root table to validate
 * if DR drop action on root table is supported or not.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_discover_dr_action_support(struct rte_eth_dev *dev)
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
	struct mlx5_flow_tbl_resource *tbl = NULL;
	void *matcher = NULL;
	void *flow = NULL;
	int ret = -1;

	tbl = flow_dv_tbl_resource_get(dev, 0, 0, 0, false, NULL,
					0, 0, 0, NULL);
	if (!tbl)
		goto err;
	dv_attr.match_criteria_enable = flow_dv_matcher_enable(mask.buf);
	__flow_dv_adjust_buf_size(&mask.size, dv_attr.match_criteria_enable);
	ret = mlx5_flow_os_create_flow_matcher(sh->cdev->ctx, &dv_attr,
					       tbl->obj, &matcher);
	if (ret)
		goto err;
	__flow_dv_adjust_buf_size(&value.size, dv_attr.match_criteria_enable);
	ret = mlx5_flow_os_create_flow(matcher, (void *)&value, 1,
				       &sh->dr_drop_action, &flow);
err:
	/*
	 * If DR drop action is not supported on root table, flow create will
	 * be failed with EOPNOTSUPP or EPROTONOSUPPORT.
	 */
	if (!flow) {
		if (matcher &&
		    (errno == EPROTONOSUPPORT || errno == EOPNOTSUPP))
			DRV_LOG(INFO, "DR drop action is not supported in root table.");
		else
			DRV_LOG(ERR, "Unexpected error in DR drop action support detection");
		ret = -1;
	} else {
		claim_zero(mlx5_flow_os_destroy_flow(flow));
	}
	if (matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher(matcher));
	if (tbl)
		flow_dv_tbl_resource_release(MLX5_SH(dev), tbl);
	return ret;
}

/**
 * Validate the batch counter support in root table.
 *
 * Create a simple flow with invalid counter and drop action on root table to
 * validate if batch counter with offset on root table is supported or not.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_dv_discover_counter_offset_support(struct rte_eth_dev *dev)
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
		.type = IBV_FLOW_ATTR_NORMAL | IBV_FLOW_ATTR_FLAGS_EGRESS,
		.priority = 0,
		.match_criteria_enable = 0,
		.match_mask = (void *)&mask,
	};
	void *actions[2] = { 0 };
	struct mlx5_flow_tbl_resource *tbl = NULL;
	struct mlx5_devx_obj *dcs = NULL;
	void *matcher = NULL;
	void *flow = NULL;
	int ret = -1;

	tbl = flow_dv_tbl_resource_get(dev, 0, 1, 0, false, NULL,
					0, 0, 0, NULL);
	if (!tbl)
		goto err;
	dcs = mlx5_devx_cmd_flow_counter_alloc(priv->sh->cdev->ctx, 0x4);
	if (!dcs)
		goto err;
	ret = mlx5_flow_os_create_flow_action_count(dcs->obj, UINT16_MAX,
						    &actions[0]);
	if (ret)
		goto err;
	dv_attr.match_criteria_enable = flow_dv_matcher_enable(mask.buf);
	__flow_dv_adjust_buf_size(&mask.size, dv_attr.match_criteria_enable);
	ret = mlx5_flow_os_create_flow_matcher(sh->cdev->ctx, &dv_attr,
					       tbl->obj, &matcher);
	if (ret)
		goto err;
	__flow_dv_adjust_buf_size(&value.size, dv_attr.match_criteria_enable);
	ret = mlx5_flow_os_create_flow(matcher, (void *)&value, 1,
				       actions, &flow);
err:
	/*
	 * If batch counter with offset is not supported, the driver will not
	 * validate the invalid offset value, flow create should success.
	 * In this case, it means batch counter is not supported in root table.
	 *
	 * Otherwise, if flow create is failed, counter offset is supported.
	 */
	if (flow) {
		DRV_LOG(INFO, "Batch counter is not supported in root "
			      "table. Switch to fallback mode.");
		rte_errno = ENOTSUP;
		ret = -rte_errno;
		claim_zero(mlx5_flow_os_destroy_flow(flow));
	} else {
		/* Check matcher to make sure validate fail at flow create. */
		if (!matcher || (matcher && errno != EINVAL))
			DRV_LOG(ERR, "Unexpected error in counter offset "
				     "support detection");
		ret = 0;
	}
	if (actions[0])
		claim_zero(mlx5_flow_os_destroy_flow_action(actions[0]));
	if (matcher)
		claim_zero(mlx5_flow_os_destroy_flow_matcher(matcher));
	if (tbl)
		flow_dv_tbl_resource_release(MLX5_SH(dev), tbl);
	if (dcs)
		claim_zero(mlx5_devx_cmd_destroy(dcs));
	return ret;
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
		      uint64_t *pkts, uint64_t *bytes, void **action)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt;
	uint64_t inn_pkts, inn_bytes;
	int ret;

	if (!priv->sh->cdev->config.devx)
		return -1;

	ret = _flow_dv_query_count(dev, counter, &inn_pkts, &inn_bytes);
	if (ret)
		return -1;
	cnt = flow_dv_counter_get_by_idx(dev, counter, NULL);
	if (cnt && action)
		*action = cnt->action;

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
flow_dv_get_aged_flows(struct rte_eth_dev *dev,
		    void **context,
		    uint32_t nb_contexts,
		    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info;
	struct mlx5_age_param *age_param;
	struct mlx5_flow_counter *counter;
	struct mlx5_aso_age_action *act;
	int nb_flows = 0;

	if (nb_contexts && !context)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "empty context");
	age_info = GET_PORT_AGE_INFO(priv);
	rte_spinlock_lock(&age_info->aged_sl);
	LIST_FOREACH(act, &age_info->aged_aso, next) {
		nb_flows++;
		if (nb_contexts) {
			context[nb_flows - 1] =
						act->age_params.context;
			if (!(--nb_contexts))
				break;
		}
	}
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
 * Mutex-protected thunk to lock-free flow_dv_counter_alloc().
 */
static uint32_t
flow_dv_counter_allocate(struct rte_eth_dev *dev)
{
	return flow_dv_counter_alloc(dev, 0);
}

/**
 * Validate indirect action.
 * Dispatcher for action type specific validation.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] conf
 *   Indirect action configuration.
 * @param[in] action
 *   The indirect action object to validate.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
flow_dv_action_validate(struct rte_eth_dev *dev,
			const struct rte_flow_indir_action_conf *conf,
			const struct rte_flow_action *action,
			struct rte_flow_error *err)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	/* called from RTE API */

	RTE_SET_USED(conf);
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
		/*
		 * priv->obj_ops is set according to driver capabilities.
		 * When DevX capabilities are
		 * sufficient, it is set to devx_obj_ops.
		 * Otherwise, it is set to ibv_obj_ops.
		 * ibv_obj_ops doesn't support ind_table_modify operation.
		 * In this case the indirect RSS action can't be used.
		 */
		if (priv->obj_ops.ind_table_modify == NULL)
			return rte_flow_error_set
					(err, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL,
					 "Indirect RSS action not supported");
		return mlx5_validate_action_rss(dev, action, err);
	case RTE_FLOW_ACTION_TYPE_AGE:
		if (!priv->sh->aso_age_mng)
			return rte_flow_error_set(err, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL,
						"Indirect age action not supported");
		return flow_dv_validate_action_age(0, action, dev, err);
	case RTE_FLOW_ACTION_TYPE_COUNT:
		return flow_dv_validate_action_count(dev, true, 0, false, err);
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		if (!priv->sh->ct_aso_en)
			return rte_flow_error_set(err, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"ASO CT is not supported");
		return mlx5_validate_action_ct(dev, action->conf, err);
	default:
		return rte_flow_error_set(err, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL,
					  "action type not supported");
	}
}

/*
 * Check if the RSS configurations for colors of a meter policy match
 * each other, except the queues.
 *
 * @param[in] r1
 *   Pointer to the first RSS flow action.
 * @param[in] r2
 *   Pointer to the second RSS flow action.
 *
 * @return
 *   0 on match, 1 on conflict.
 */
static inline int
flow_dv_mtr_policy_rss_compare(const struct rte_flow_action_rss *r1,
			       const struct rte_flow_action_rss *r2)
{
	if (r1 == NULL || r2 == NULL)
		return 0;
	if (!(r1->level <= 1 && r2->level <= 1) &&
	    !(r1->level > 1 && r2->level > 1))
		return 1;
	if (r1->types != r2->types &&
	    !((r1->types == 0 || r1->types == RTE_ETH_RSS_IP) &&
	      (r2->types == 0 || r2->types == RTE_ETH_RSS_IP)))
		return 1;
	if (r1->key || r2->key) {
		const void *key1 = r1->key ? r1->key : rss_hash_default_key;
		const void *key2 = r2->key ? r2->key : rss_hash_default_key;

		if (memcmp(key1, key2, MLX5_RSS_HASH_KEY_LEN))
			return 1;
	}
	return 0;
}

/**
 * Validate the meter hierarchy chain for meter policy.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] meter_id
 *   Meter id.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[out] is_rss
 *   Is RSS or not.
 * @param[out] hierarchy_domain
 *   The domain bitmap for hierarchy policy.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value with error set.
 */
static int
flow_dv_validate_policy_mtr_hierarchy(struct rte_eth_dev *dev,
				  uint32_t meter_id,
				  uint64_t action_flags,
				  bool *is_rss,
				  uint8_t *hierarchy_domain,
				  struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *fm;
	struct mlx5_flow_meter_policy *policy;
	uint8_t cnt = 1;

	if (action_flags & (MLX5_FLOW_FATE_ACTIONS |
			    MLX5_FLOW_FATE_ESWITCH_ACTIONS))
		return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_POLICER_ACTION_GREEN,
					NULL,
					"Multiple fate actions not supported.");
	*hierarchy_domain = 0;
	fm = mlx5_flow_meter_find(priv, meter_id, NULL);
	while (true) {
		if (!fm)
			return -rte_mtr_error_set(error, EINVAL,
						RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					"Meter not found in meter hierarchy.");
		if (fm->def_policy)
			return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
			"Non termination meter not supported in hierarchy.");
		if (!fm->shared)
			return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_MTR_ID, NULL,
					"Only shared meter supported in hierarchy.");
		policy = mlx5_flow_meter_policy_find(dev, fm->policy_id, NULL);
		MLX5_ASSERT(policy);
		/**
		 * Only inherit the supported domains of the first meter in
		 * hierarchy.
		 * One meter supports at least one domain.
		 */
		if (!*hierarchy_domain) {
			if (policy->transfer)
				*hierarchy_domain |=
						MLX5_MTR_DOMAIN_TRANSFER_BIT;
			if (policy->ingress)
				*hierarchy_domain |=
						MLX5_MTR_DOMAIN_INGRESS_BIT;
			if (policy->egress)
				*hierarchy_domain |= MLX5_MTR_DOMAIN_EGRESS_BIT;
		}
		if (!policy->is_hierarchy) {
			*is_rss = policy->is_rss;
			break;
		}
		rte_spinlock_lock(&policy->sl);
		fm = mlx5_flow_meter_hierarchy_next_meter(priv, policy, NULL);
		rte_spinlock_unlock(&policy->sl);
		if (++cnt >= MLX5_MTR_CHAIN_MAX_NUM)
			return -rte_mtr_error_set(error, EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
					"Exceed max hierarchy meter number.");
	}
	return 0;
}

/**
 * Validate meter policy actions.
 * Dispatcher for action type specific validation.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action
 *   The meter policy action object to validate.
 * @param[in] attr
 *   Attributes of flow to determine steering domain.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
flow_dv_validate_mtr_policy_acts(struct rte_eth_dev *dev,
			const struct rte_flow_action *actions[RTE_COLORS],
			struct rte_flow_attr *attr,
			bool *is_rss,
			uint8_t *domain_bitmap,
			uint8_t *policy_mode,
			struct rte_mtr_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *dev_conf = &priv->sh->config;
	const struct rte_flow_action *act;
	uint64_t action_flags[RTE_COLORS] = {0};
	int actions_n;
	int i, ret;
	struct rte_flow_error flow_err;
	uint8_t domain_color[RTE_COLORS] = {0};
	uint8_t def_domain = MLX5_MTR_ALL_DOMAIN_BIT;
	uint8_t hierarchy_domain = 0;
	const struct rte_flow_action_meter *mtr;
	const struct rte_flow_action_meter *next_mtr = NULL;
	bool def_green = false;
	bool def_yellow = false;
	const struct rte_flow_action_rss *rss_color[RTE_COLORS] = {NULL};
	/* Called from RTE API */
	bool is_root = !(attr->group || (attr->transfer && priv->fdb_def_rule));

	if (!dev_conf->dv_esw_en)
		def_domain &= ~MLX5_MTR_DOMAIN_TRANSFER_BIT;
	*domain_bitmap = def_domain;
	/* Red color could only support DROP action. */
	if (!actions[RTE_COLOR_RED] ||
	    actions[RTE_COLOR_RED]->type != RTE_FLOW_ACTION_TYPE_DROP)
		return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Red color only supports drop action.");
	/*
	 * Check default policy actions:
	 * Green / Yellow: no action, Red: drop action
	 * Either G or Y will trigger default policy actions to be created.
	 */
	if (!actions[RTE_COLOR_GREEN] ||
	    actions[RTE_COLOR_GREEN]->type == RTE_FLOW_ACTION_TYPE_END)
		def_green = true;
	if (!actions[RTE_COLOR_YELLOW] ||
	    actions[RTE_COLOR_YELLOW]->type == RTE_FLOW_ACTION_TYPE_END)
		def_yellow = true;
	if (def_green && def_yellow) {
		*policy_mode = MLX5_MTR_POLICY_MODE_DEF;
		return 0;
	} else if (!def_green && def_yellow) {
		*policy_mode = MLX5_MTR_POLICY_MODE_OG;
	} else if (def_green && !def_yellow) {
		*policy_mode = MLX5_MTR_POLICY_MODE_OY;
	} else {
		*policy_mode = MLX5_MTR_POLICY_MODE_ALL;
	}
	/* Set to empty string in case of NULL pointer access by user. */
	flow_err.message = "";
	for (i = 0; i < RTE_COLORS; i++) {
		act = actions[i];
		for (action_flags[i] = 0, actions_n = 0;
		     act && act->type != RTE_FLOW_ACTION_TYPE_END;
		     act++) {
			if (actions_n == MLX5_DV_MAX_NUMBER_OF_ACTIONS)
				return -rte_mtr_error_set(error, ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "too many actions");
			switch (act->type) {
			case RTE_FLOW_ACTION_TYPE_PORT_ID:
			case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
				if (!dev_conf->dv_esw_en)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "PORT action validate check"
					" fail for ESW disable");
				ret = flow_dv_validate_action_port_id(dev,
						action_flags[i],
						act, attr, &flow_err);
				if (ret)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, flow_err.message ?
					flow_err.message :
					"PORT action validate check fail");
				++actions_n;
				action_flags[i] |= MLX5_FLOW_ACTION_PORT_ID;
				break;
			case RTE_FLOW_ACTION_TYPE_MARK:
				ret = flow_dv_validate_action_mark(dev, act,
							   action_flags[i],
							   attr, &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, flow_err.message ?
					flow_err.message :
					"Mark action validate check fail");
				if (dev_conf->dv_xmeta_en !=
					MLX5_XMETA_MODE_LEGACY)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "Extend MARK action is "
					"not supported. Please try use "
					"default policy for meter.");
				action_flags[i] |= MLX5_FLOW_ACTION_MARK;
				++actions_n;
				break;
			case RTE_FLOW_ACTION_TYPE_SET_TAG:
				ret = flow_dv_validate_action_set_tag(dev,
							act, action_flags[i],
							attr, &flow_err);
				if (ret)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, flow_err.message ?
					flow_err.message :
					"Set tag action validate check fail");
				action_flags[i] |= MLX5_FLOW_ACTION_SET_TAG;
				++actions_n;
				break;
			case RTE_FLOW_ACTION_TYPE_DROP:
				ret = mlx5_flow_validate_action_drop
					(action_flags[i], attr, &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, flow_err.message ?
					flow_err.message :
					"Drop action validate check fail");
				action_flags[i] |= MLX5_FLOW_ACTION_DROP;
				++actions_n;
				break;
			case RTE_FLOW_ACTION_TYPE_QUEUE:
				/*
				 * Check whether extensive
				 * metadata feature is engaged.
				 */
				if (dev_conf->dv_flow_en &&
				    (dev_conf->dv_xmeta_en !=
				     MLX5_XMETA_MODE_LEGACY) &&
				    mlx5_flow_ext_mreg_supported(dev))
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "Queue action with meta "
					  "is not supported. Please try use "
					  "default policy for meter.");
				ret = mlx5_flow_validate_action_queue(act,
							action_flags[i], dev,
							attr, &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, flow_err.message ?
					  flow_err.message :
					  "Queue action validate check fail");
				action_flags[i] |= MLX5_FLOW_ACTION_QUEUE;
				++actions_n;
				break;
			case RTE_FLOW_ACTION_TYPE_RSS:
				if (dev_conf->dv_flow_en &&
				    (dev_conf->dv_xmeta_en !=
				     MLX5_XMETA_MODE_LEGACY) &&
				    mlx5_flow_ext_mreg_supported(dev))
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "RSS action with meta "
					  "is not supported. Please try use "
					  "default policy for meter.");
				ret = mlx5_validate_action_rss(dev, act,
							       &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, flow_err.message ?
					  flow_err.message :
					  "RSS action validate check fail");
				action_flags[i] |= MLX5_FLOW_ACTION_RSS;
				++actions_n;
				/* Either G or Y will set the RSS. */
				rss_color[i] = act->conf;
				break;
			case RTE_FLOW_ACTION_TYPE_JUMP:
				ret = flow_dv_validate_action_jump(dev,
					NULL, act, action_flags[i],
					attr, true, &flow_err);
				if (ret)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, flow_err.message ?
					  flow_err.message :
					  "Jump action validate check fail");
				++actions_n;
				action_flags[i] |= MLX5_FLOW_ACTION_JUMP;
				break;
			case RTE_FLOW_ACTION_TYPE_METER:
				mtr = act->conf;
				if (next_mtr && next_mtr->mtr_id != mtr->mtr_id)
					return -rte_mtr_error_set(error, ENOTSUP,
						RTE_MTR_ERROR_TYPE_METER_POLICY, NULL,
						"Green and Yellow must use the same meter.");
				ret = flow_dv_validate_policy_mtr_hierarchy(dev,
							mtr->mtr_id,
							action_flags[i],
							is_rss,
							&hierarchy_domain,
							error);
				if (ret)
					return ret;
				++actions_n;
				action_flags[i] |=
				MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY;
				next_mtr = mtr;
				break;
			case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
				ret = flow_dv_validate_action_modify_field(dev,
					action_flags[i], act, attr, is_root, &flow_err);
				if (ret < 0)
					return -rte_mtr_error_set(error,
					  ENOTSUP,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, flow_err.message ?
					  flow_err.message :
					  "Modify field action validate check fail");
				++actions_n;
				action_flags[i] |= MLX5_FLOW_ACTION_MODIFY_FIELD;
				break;
			default:
				return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL,
					"Doesn't support optional action");
			}
		}
		if (action_flags[i] & MLX5_FLOW_ACTION_PORT_ID) {
			domain_color[i] = MLX5_MTR_DOMAIN_TRANSFER_BIT;
		} else if ((action_flags[i] &
			  (MLX5_FLOW_ACTION_RSS | MLX5_FLOW_ACTION_QUEUE)) ||
			  (action_flags[i] & MLX5_FLOW_ACTION_MARK)) {
			/*
			 * Only support MLX5_XMETA_MODE_LEGACY
			 * so MARK action is only in ingress domain.
			 */
			domain_color[i] = MLX5_MTR_DOMAIN_INGRESS_BIT;
		} else {
			domain_color[i] = def_domain;
			if (action_flags[i] &&
			    !(action_flags[i] & MLX5_FLOW_FATE_ESWITCH_ACTIONS))
				domain_color[i] &=
				~MLX5_MTR_DOMAIN_TRANSFER_BIT;
		}
		if (action_flags[i] &
		    MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY)
			domain_color[i] &= hierarchy_domain;
		/*
		 * Non-termination actions only support NIC Tx domain.
		 * The adjustion should be skipped when there is no
		 * action or only END is provided. The default domains
		 * bit-mask is set to find the MIN intersection.
		 * The action flags checking should also be skipped.
		 */
		if ((def_green && i == RTE_COLOR_GREEN) ||
		    (def_yellow && i == RTE_COLOR_YELLOW))
			continue;
		/*
		 * Validate the drop action mutual exclusion
		 * with other actions. Drop action is mutually-exclusive
		 * with any other action, except for Count action.
		 */
		if ((action_flags[i] & MLX5_FLOW_ACTION_DROP) &&
		    (action_flags[i] & ~MLX5_FLOW_ACTION_DROP)) {
			return -rte_mtr_error_set(error, ENOTSUP,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL, "Drop action is mutually-exclusive "
				"with any other action");
		}
		/* Eswitch has few restrictions on using items and actions */
		if (domain_color[i] & MLX5_MTR_DOMAIN_TRANSFER_BIT) {
			if (!mlx5_flow_ext_mreg_supported(dev) &&
			    action_flags[i] & MLX5_FLOW_ACTION_MARK)
				return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "unsupported action MARK");
			if (action_flags[i] & MLX5_FLOW_ACTION_QUEUE)
				return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "unsupported action QUEUE");
			if (action_flags[i] & MLX5_FLOW_ACTION_RSS)
				return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "unsupported action RSS");
			if (!(action_flags[i] & MLX5_FLOW_FATE_ESWITCH_ACTIONS))
				return -rte_mtr_error_set(error, ENOTSUP,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL, "no fate action is found");
		} else {
			if (!(action_flags[i] & MLX5_FLOW_FATE_ACTIONS) &&
			    (domain_color[i] & MLX5_MTR_DOMAIN_INGRESS_BIT)) {
				if ((domain_color[i] &
				     MLX5_MTR_DOMAIN_EGRESS_BIT))
					domain_color[i] =
						MLX5_MTR_DOMAIN_EGRESS_BIT;
				else
					return -rte_mtr_error_set(error,
						ENOTSUP,
						RTE_MTR_ERROR_TYPE_METER_POLICY,
						NULL,
						"no fate action is found");
			}
		}
	}
	if (next_mtr && *policy_mode == MLX5_MTR_POLICY_MODE_ALL) {
		if (!(action_flags[RTE_COLOR_GREEN] & action_flags[RTE_COLOR_YELLOW] &
		      MLX5_FLOW_ACTION_METER_WITH_TERMINATED_POLICY))
			return -rte_mtr_error_set(error, EINVAL, RTE_MTR_ERROR_TYPE_METER_POLICY,
						  NULL,
						  "Meter hierarchy supports meter action only.");
	}
	/* If both colors have RSS, the attributes should be the same. */
	if (flow_dv_mtr_policy_rss_compare(rss_color[RTE_COLOR_GREEN],
					   rss_color[RTE_COLOR_YELLOW]))
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "policy RSS attr conflict");
	if (rss_color[RTE_COLOR_GREEN] || rss_color[RTE_COLOR_YELLOW])
		*is_rss = true;
	/* "domain_color[C]" is non-zero for each color, default is ALL. */
	if (!def_green && !def_yellow &&
	    domain_color[RTE_COLOR_GREEN] != domain_color[RTE_COLOR_YELLOW] &&
	    !(action_flags[RTE_COLOR_GREEN] & MLX5_FLOW_ACTION_DROP) &&
	    !(action_flags[RTE_COLOR_YELLOW] & MLX5_FLOW_ACTION_DROP))
		return -rte_mtr_error_set(error, EINVAL,
					  RTE_MTR_ERROR_TYPE_METER_POLICY,
					  NULL, "policy domains conflict");
	/*
	 * At least one color policy is listed in the actions, the domains
	 * to be supported should be the intersection.
	 */
	*domain_bitmap = domain_color[RTE_COLOR_GREEN] &
			 domain_color[RTE_COLOR_YELLOW];
	return 0;
}

static int
flow_dv_sync_domain(struct rte_eth_dev *dev, uint32_t domains, uint32_t flags)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;

	if ((domains & MLX5_DOMAIN_BIT_NIC_RX) && priv->sh->rx_domain != NULL) {
		ret = mlx5_os_flow_dr_sync_domain(priv->sh->rx_domain,
						flags);
		if (ret != 0)
			return ret;
	}
	if ((domains & MLX5_DOMAIN_BIT_NIC_TX) && priv->sh->tx_domain != NULL) {
		ret = mlx5_os_flow_dr_sync_domain(priv->sh->tx_domain, flags);
		if (ret != 0)
			return ret;
	}
	if ((domains & MLX5_DOMAIN_BIT_FDB) && priv->sh->fdb_domain != NULL) {
		ret = mlx5_os_flow_dr_sync_domain(priv->sh->fdb_domain, flags);
		if (ret != 0)
			return ret;
	}
	return 0;
}

/**
 * Discover the number of available flow priorities
 * by trying to create a flow with the highest priority value
 * for each possible number.
 *
 * @param[in] dev
 *   Ethernet device.
 * @param[in] vprio
 *   List of possible number of available priorities.
 * @param[in] vprio_n
 *   Size of @p vprio array.
 * @return
 *   On success, number of available flow priorities.
 *   On failure, a negative errno-style code and rte_errno is set.
 */
static int
flow_dv_discover_priorities(struct rte_eth_dev *dev,
			    const uint16_t *vprio, int vprio_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_indexed_pool *pool = priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW];
	struct rte_flow_item_eth eth;
	struct rte_flow_item item = {
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &eth,
		.mask = &eth,
	};
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
	};
	union mlx5_flow_tbl_key tbl_key;
	struct mlx5_flow flow;
	void *action;
	struct rte_flow_error error;
	uint8_t misc_mask;
	int i, err, ret = -ENOTSUP;

	/*
	 * Prepare a flow with a catch-all pattern and a drop action.
	 * Use drop queue, because shared drop action may be unavailable.
	 */
	action = priv->drop_queue.hrxq->action;
	if (action == NULL) {
		DRV_LOG(ERR, "Priority discovery requires a drop action");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	memset(&flow, 0, sizeof(flow));
	flow.handle = mlx5_ipool_zmalloc(pool, &flow.handle_idx);
	if (flow.handle == NULL) {
		DRV_LOG(ERR, "Cannot create flow handle");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	flow.ingress = true;
	flow.dv.value.size = MLX5_ST_SZ_BYTES(fte_match_param);
	flow.dv.actions[0] = action;
	flow.dv.actions_n = 1;
	memset(&eth, 0, sizeof(eth));
	flow_dv_translate_item_eth(matcher.mask.buf, &item,
				   /* inner */ false, /* group */ 0,
				   MLX5_SET_MATCHER_SW_M);
	flow_dv_translate_item_eth(flow.dv.value.buf, &item,
				   /* inner */ false, /* group */ 0,
				   MLX5_SET_MATCHER_SW_V);
	matcher.crc = rte_raw_cksum(matcher.mask.buf, matcher.mask.size);
	for (i = 0; i < vprio_n; i++) {
		/* Configure the next proposed maximum priority. */
		matcher.priority = vprio[i] - 1;
		memset(&tbl_key, 0, sizeof(tbl_key));
		err = flow_dv_matcher_register(dev, &matcher, &tbl_key, &flow,
					       /* tunnel */ NULL,
					       /* group */ 0,
					       &error);
		if (err != 0) {
			/* This action is pure SW and must always succeed. */
			DRV_LOG(ERR, "Cannot register matcher");
			ret = -rte_errno;
			break;
		}
		/* Try to apply the flow to HW. */
		misc_mask = flow_dv_matcher_enable(flow.dv.value.buf);
		__flow_dv_adjust_buf_size(&flow.dv.value.size, misc_mask);
		err = mlx5_flow_os_create_flow
				(flow.handle->dvh.matcher->matcher_object,
				 (void *)&flow.dv.value, flow.dv.actions_n,
				 flow.dv.actions, &flow.handle->drv_flow);
		if (err == 0) {
			claim_zero(mlx5_flow_os_destroy_flow
						(flow.handle->drv_flow));
			flow.handle->drv_flow = NULL;
		}
		claim_zero(flow_dv_matcher_release(dev, flow.handle));
		if (err != 0)
			break;
		ret = vprio[i];
	}
	mlx5_ipool_free(pool, flow.handle_idx);
	/* Set rte_errno if no expected priority value matched. */
	if (ret < 0)
		rte_errno = -ret;
	return ret;
}

const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops = {
	.validate = flow_dv_validate,
	.prepare = flow_dv_prepare,
	.translate = flow_dv_translate,
	.apply = flow_dv_apply,
	.remove = flow_dv_remove,
	.destroy = flow_dv_destroy,
	.query = flow_dv_query,
	.create_mtr_tbls = flow_dv_create_mtr_tbls,
	.destroy_mtr_tbls = flow_dv_destroy_mtr_tbls,
	.destroy_mtr_drop_tbls = flow_dv_destroy_mtr_drop_tbls,
	.create_meter = flow_dv_mtr_alloc,
	.free_meter = flow_dv_aso_mtr_release_to_pool,
	.validate_mtr_acts = flow_dv_validate_mtr_policy_acts,
	.create_mtr_acts = flow_dv_create_mtr_policy_acts,
	.destroy_mtr_acts = flow_dv_destroy_mtr_policy_acts,
	.create_policy_rules = flow_dv_create_policy_rules,
	.destroy_policy_rules = flow_dv_destroy_policy_rules,
	.create_def_policy = flow_dv_create_def_policy,
	.destroy_def_policy = flow_dv_destroy_def_policy,
	.meter_sub_policy_rss_prepare = flow_dv_meter_sub_policy_rss_prepare,
	.meter_hierarchy_rule_create = flow_dv_meter_hierarchy_rule_create,
	.destroy_sub_policy_with_rxq = flow_dv_destroy_sub_policy_with_rxq,
	.counter_alloc = flow_dv_counter_allocate,
	.counter_free = flow_dv_counter_free,
	.counter_query = flow_dv_counter_query,
	.get_aged_flows = flow_dv_get_aged_flows,
	.action_validate = flow_dv_action_validate,
	.action_create = flow_dv_action_create,
	.action_destroy = flow_dv_action_destroy,
	.action_update = flow_dv_action_update,
	.action_query = flow_dv_action_query,
	.sync_domain = flow_dv_sync_domain,
	.discover_priorities = flow_dv_discover_priorities,
	.item_create = flow_dv_item_create,
	.item_release = flow_dv_item_release,
};

#endif /* HAVE_IBV_FLOW_DV_SUPPORT */
