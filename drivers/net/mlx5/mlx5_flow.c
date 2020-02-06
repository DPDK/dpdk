/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <netinet/in.h>
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

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_rxtx.h"

/* Dev ops structure defined in mlx5.c */
extern const struct eth_dev_ops mlx5_dev_ops;
extern const struct eth_dev_ops mlx5_dev_ops_isolate;

/** Device flow drivers. */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
extern const struct mlx5_flow_driver_ops mlx5_flow_dv_drv_ops;
#endif
extern const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops;

const struct mlx5_flow_driver_ops mlx5_flow_null_drv_ops;

const struct mlx5_flow_driver_ops *flow_drv_ops[] = {
	[MLX5_FLOW_TYPE_MIN] = &mlx5_flow_null_drv_ops,
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	[MLX5_FLOW_TYPE_DV] = &mlx5_flow_dv_drv_ops,
#endif
	[MLX5_FLOW_TYPE_VERBS] = &mlx5_flow_verbs_drv_ops,
	[MLX5_FLOW_TYPE_MAX] = &mlx5_flow_null_drv_ops
};

enum mlx5_expansion {
	MLX5_EXPANSION_ROOT,
	MLX5_EXPANSION_ROOT_OUTER,
	MLX5_EXPANSION_ROOT_ETH_VLAN,
	MLX5_EXPANSION_ROOT_OUTER_ETH_VLAN,
	MLX5_EXPANSION_OUTER_ETH,
	MLX5_EXPANSION_OUTER_ETH_VLAN,
	MLX5_EXPANSION_OUTER_VLAN,
	MLX5_EXPANSION_OUTER_IPV4,
	MLX5_EXPANSION_OUTER_IPV4_UDP,
	MLX5_EXPANSION_OUTER_IPV4_TCP,
	MLX5_EXPANSION_OUTER_IPV6,
	MLX5_EXPANSION_OUTER_IPV6_UDP,
	MLX5_EXPANSION_OUTER_IPV6_TCP,
	MLX5_EXPANSION_VXLAN,
	MLX5_EXPANSION_VXLAN_GPE,
	MLX5_EXPANSION_GRE,
	MLX5_EXPANSION_MPLS,
	MLX5_EXPANSION_ETH,
	MLX5_EXPANSION_ETH_VLAN,
	MLX5_EXPANSION_VLAN,
	MLX5_EXPANSION_IPV4,
	MLX5_EXPANSION_IPV4_UDP,
	MLX5_EXPANSION_IPV4_TCP,
	MLX5_EXPANSION_IPV6,
	MLX5_EXPANSION_IPV6_UDP,
	MLX5_EXPANSION_IPV6_TCP,
};

/** Supported expansion of items. */
static const struct rte_flow_expand_node mlx5_support_expansion[] = {
	[MLX5_EXPANSION_ROOT] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						 MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_ROOT_OUTER] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_ETH,
						 MLX5_EXPANSION_OUTER_IPV4,
						 MLX5_EXPANSION_OUTER_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_ROOT_ETH_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_ROOT_OUTER_ETH_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_ETH_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_OUTER_ETH] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_IPV4,
						 MLX5_EXPANSION_OUTER_IPV6,
						 MLX5_EXPANSION_MPLS),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.rss_types = 0,
	},
	[MLX5_EXPANSION_OUTER_ETH_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.rss_types = 0,
	},
	[MLX5_EXPANSION_OUTER_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_IPV4,
						 MLX5_EXPANSION_OUTER_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
	},
	[MLX5_EXPANSION_OUTER_IPV4] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT
			(MLX5_EXPANSION_OUTER_IPV4_UDP,
			 MLX5_EXPANSION_OUTER_IPV4_TCP,
			 MLX5_EXPANSION_GRE,
			 MLX5_EXPANSION_IPV4,
			 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.rss_types = ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 |
			ETH_RSS_NONFRAG_IPV4_OTHER,
	},
	[MLX5_EXPANSION_OUTER_IPV4_UDP] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VXLAN,
						 MLX5_EXPANSION_VXLAN_GPE),
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = ETH_RSS_NONFRAG_IPV4_UDP,
	},
	[MLX5_EXPANSION_OUTER_IPV4_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = ETH_RSS_NONFRAG_IPV4_TCP,
	},
	[MLX5_EXPANSION_OUTER_IPV6] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT
			(MLX5_EXPANSION_OUTER_IPV6_UDP,
			 MLX5_EXPANSION_OUTER_IPV6_TCP,
			 MLX5_EXPANSION_IPV4,
			 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.rss_types = ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 |
			ETH_RSS_NONFRAG_IPV6_OTHER,
	},
	[MLX5_EXPANSION_OUTER_IPV6_UDP] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VXLAN,
						 MLX5_EXPANSION_VXLAN_GPE),
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = ETH_RSS_NONFRAG_IPV6_UDP,
	},
	[MLX5_EXPANSION_OUTER_IPV6_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = ETH_RSS_NONFRAG_IPV6_TCP,
	},
	[MLX5_EXPANSION_VXLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH),
		.type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
	[MLX5_EXPANSION_VXLAN_GPE] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						 MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
	},
	[MLX5_EXPANSION_GRE] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4),
		.type = RTE_FLOW_ITEM_TYPE_GRE,
	},
	[MLX5_EXPANSION_MPLS] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_MPLS,
	},
	[MLX5_EXPANSION_ETH] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
	},
	[MLX5_EXPANSION_ETH_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
	},
	[MLX5_EXPANSION_VLAN] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
	},
	[MLX5_EXPANSION_IPV4] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4_UDP,
						 MLX5_EXPANSION_IPV4_TCP),
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.rss_types = ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 |
			ETH_RSS_NONFRAG_IPV4_OTHER,
	},
	[MLX5_EXPANSION_IPV4_UDP] = {
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = ETH_RSS_NONFRAG_IPV4_UDP,
	},
	[MLX5_EXPANSION_IPV4_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = ETH_RSS_NONFRAG_IPV4_TCP,
	},
	[MLX5_EXPANSION_IPV6] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV6_UDP,
						 MLX5_EXPANSION_IPV6_TCP),
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.rss_types = ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 |
			ETH_RSS_NONFRAG_IPV6_OTHER,
	},
	[MLX5_EXPANSION_IPV6_UDP] = {
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = ETH_RSS_NONFRAG_IPV6_UDP,
	},
	[MLX5_EXPANSION_IPV6_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = ETH_RSS_NONFRAG_IPV6_TCP,
	},
};

static const struct rte_flow_ops mlx5_flow_ops = {
	.validate = mlx5_flow_validate,
	.create = mlx5_flow_create,
	.destroy = mlx5_flow_destroy,
	.flush = mlx5_flow_flush,
	.isolate = mlx5_flow_isolate,
	.query = mlx5_flow_query,
	.dev_dump = mlx5_flow_dev_dump,
};

/* Convert FDIR request to Generic flow. */
struct mlx5_fdir {
	struct rte_flow_attr attr;
	struct rte_flow_item items[4];
	struct rte_flow_item_eth l2;
	struct rte_flow_item_eth l2_mask;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} l3;
	union {
		struct rte_flow_item_ipv4 ipv4;
		struct rte_flow_item_ipv6 ipv6;
	} l3_mask;
	union {
		struct rte_flow_item_udp udp;
		struct rte_flow_item_tcp tcp;
	} l4;
	union {
		struct rte_flow_item_udp udp;
		struct rte_flow_item_tcp tcp;
	} l4_mask;
	struct rte_flow_action actions[2];
	struct rte_flow_action_queue queue;
};

/* Map of Verbs to Flow priority with 8 Verbs priorities. */
static const uint32_t priority_map_3[][MLX5_PRIORITY_MAP_MAX] = {
	{ 0, 1, 2 }, { 2, 3, 4 }, { 5, 6, 7 },
};

/* Map of Verbs to Flow priority with 16 Verbs priorities. */
static const uint32_t priority_map_5[][MLX5_PRIORITY_MAP_MAX] = {
	{ 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },
	{ 9, 10, 11 }, { 12, 13, 14 },
};

/* Tunnel information. */
struct mlx5_flow_tunnel_info {
	uint64_t tunnel; /**< Tunnel bit (see MLX5_FLOW_*). */
	uint32_t ptype; /**< Tunnel Ptype (see RTE_PTYPE_*). */
};

static struct mlx5_flow_tunnel_info tunnels_info[] = {
	{
		.tunnel = MLX5_FLOW_LAYER_VXLAN,
		.ptype = RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L4_UDP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_GENEVE,
		.ptype = RTE_PTYPE_TUNNEL_GENEVE | RTE_PTYPE_L4_UDP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_VXLAN_GPE,
		.ptype = RTE_PTYPE_TUNNEL_VXLAN_GPE | RTE_PTYPE_L4_UDP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_GRE,
		.ptype = RTE_PTYPE_TUNNEL_GRE,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_MPLS | MLX5_FLOW_LAYER_OUTER_L4_UDP,
		.ptype = RTE_PTYPE_TUNNEL_MPLS_IN_UDP | RTE_PTYPE_L4_UDP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_MPLS,
		.ptype = RTE_PTYPE_TUNNEL_MPLS_IN_GRE,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_NVGRE,
		.ptype = RTE_PTYPE_TUNNEL_NVGRE,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_IPIP,
		.ptype = RTE_PTYPE_TUNNEL_IP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_IPV6_ENCAP,
		.ptype = RTE_PTYPE_TUNNEL_IP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_GTP,
		.ptype = RTE_PTYPE_TUNNEL_GTPU,
	},
};

/**
 * Translate tag ID to register.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] feature
 *   The feature that request the register.
 * @param[in] id
 *   The request register ID.
 * @param[out] error
 *   Error description in case of any.
 *
 * @return
 *   The request register on success, a negative errno
 *   value otherwise and rte_errno is set.
 */
int
mlx5_flow_get_reg_id(struct rte_eth_dev *dev,
		     enum mlx5_feature_name feature,
		     uint32_t id,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	enum modify_reg start_reg;
	bool skip_mtr_reg = false;

	switch (feature) {
	case MLX5_HAIRPIN_RX:
		return REG_B;
	case MLX5_HAIRPIN_TX:
		return REG_A;
	case MLX5_METADATA_RX:
		switch (config->dv_xmeta_en) {
		case MLX5_XMETA_MODE_LEGACY:
			return REG_B;
		case MLX5_XMETA_MODE_META16:
			return REG_C_0;
		case MLX5_XMETA_MODE_META32:
			return REG_C_1;
		}
		break;
	case MLX5_METADATA_TX:
		return REG_A;
	case MLX5_METADATA_FDB:
		switch (config->dv_xmeta_en) {
		case MLX5_XMETA_MODE_LEGACY:
			return REG_NONE;
		case MLX5_XMETA_MODE_META16:
			return REG_C_0;
		case MLX5_XMETA_MODE_META32:
			return REG_C_1;
		}
		break;
	case MLX5_FLOW_MARK:
		switch (config->dv_xmeta_en) {
		case MLX5_XMETA_MODE_LEGACY:
			return REG_NONE;
		case MLX5_XMETA_MODE_META16:
			return REG_C_1;
		case MLX5_XMETA_MODE_META32:
			return REG_C_0;
		}
		break;
	case MLX5_MTR_SFX:
		/*
		 * If meter color and flow match share one register, flow match
		 * should use the meter color register for match.
		 */
		if (priv->mtr_reg_share)
			return priv->mtr_color_reg;
		else
			return priv->mtr_color_reg != REG_C_2 ? REG_C_2 :
			       REG_C_3;
	case MLX5_MTR_COLOR:
		MLX5_ASSERT(priv->mtr_color_reg != REG_NONE);
		return priv->mtr_color_reg;
	case MLX5_COPY_MARK:
		/*
		 * Metadata COPY_MARK register using is in meter suffix sub
		 * flow while with meter. It's safe to share the same register.
		 */
		return priv->mtr_color_reg != REG_C_2 ? REG_C_2 : REG_C_3;
	case MLX5_APP_TAG:
		/*
		 * If meter is enable, it will engage the register for color
		 * match and flow match. If meter color match is not using the
		 * REG_C_2, need to skip the REG_C_x be used by meter color
		 * match.
		 * If meter is disable, free to use all available registers.
		 */
		start_reg = priv->mtr_color_reg != REG_C_2 ? REG_C_2 :
			    (priv->mtr_reg_share ? REG_C_3 : REG_C_4);
		skip_mtr_reg = !!(priv->mtr_en && start_reg == REG_C_2);
		if (id > (REG_C_7 - start_reg))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "invalid tag id");
		if (config->flow_mreg_c[id + start_reg - REG_C_0] == REG_NONE)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "unsupported tag id");
		/*
		 * This case means meter is using the REG_C_x great than 2.
		 * Take care not to conflict with meter color REG_C_x.
		 * If the available index REG_C_y >= REG_C_x, skip the
		 * color register.
		 */
		if (skip_mtr_reg && config->flow_mreg_c
		    [id + start_reg - REG_C_0] >= priv->mtr_color_reg) {
			if (config->flow_mreg_c
			    [id + 1 + start_reg - REG_C_0] != REG_NONE)
				return config->flow_mreg_c
					       [id + 1 + start_reg - REG_C_0];
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "unsupported tag id");
		}
		return config->flow_mreg_c[id + start_reg - REG_C_0];
	}
	MLX5_ASSERT(false);
	return rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "invalid feature name");
}

/**
 * Check extensive flow metadata register support.
 *
 * @param dev
 *   Pointer to rte_eth_dev structure.
 *
 * @return
 *   True if device supports extensive flow metadata register, otherwise false.
 */
bool
mlx5_flow_ext_mreg_supported(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;

	/*
	 * Having available reg_c can be regarded inclusively as supporting
	 * extensive flow metadata register, which could mean,
	 * - metadata register copy action by modify header.
	 * - 16 modify header actions is supported.
	 * - reg_c's are preserved across different domain (FDB and NIC) on
	 *   packet loopback by flow lookup miss.
	 */
	return config->flow_mreg_c[2] != REG_NONE;
}

/**
 * Discover the maximum number of priority available.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   number of supported flow priority on success, a negative errno
 *   value otherwise and rte_errno is set.
 */
int
mlx5_flow_discover_priorities(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth eth;
		struct ibv_flow_spec_action_drop drop;
	} flow_attr = {
		.attr = {
			.num_of_specs = 2,
			.port = (uint8_t)priv->ibv_port,
		},
		.eth = {
			.type = IBV_FLOW_SPEC_ETH,
			.size = sizeof(struct ibv_flow_spec_eth),
		},
		.drop = {
			.size = sizeof(struct ibv_flow_spec_action_drop),
			.type = IBV_FLOW_SPEC_ACTION_DROP,
		},
	};
	struct ibv_flow *flow;
	struct mlx5_hrxq *drop = mlx5_hrxq_drop_new(dev);
	uint16_t vprio[] = { 8, 16 };
	int i;
	int priority = 0;

	if (!drop) {
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	for (i = 0; i != RTE_DIM(vprio); i++) {
		flow_attr.attr.priority = vprio[i] - 1;
		flow = mlx5_glue->create_flow(drop->qp, &flow_attr.attr);
		if (!flow)
			break;
		claim_zero(mlx5_glue->destroy_flow(flow));
		priority = vprio[i];
	}
	mlx5_hrxq_drop_release(dev);
	switch (priority) {
	case 8:
		priority = RTE_DIM(priority_map_3);
		break;
	case 16:
		priority = RTE_DIM(priority_map_5);
		break;
	default:
		rte_errno = ENOTSUP;
		DRV_LOG(ERR,
			"port %u verbs maximum priority: %d expected 8/16",
			dev->data->port_id, priority);
		return -rte_errno;
	}
	DRV_LOG(INFO, "port %u flow maximum priority: %d",
		dev->data->port_id, priority);
	return priority;
}

/**
 * Adjust flow priority based on the highest layer and the request priority.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] priority
 *   The rule base priority.
 * @param[in] subpriority
 *   The priority based on the items.
 *
 * @return
 *   The new priority.
 */
uint32_t mlx5_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
				   uint32_t subpriority)
{
	uint32_t res = 0;
	struct mlx5_priv *priv = dev->data->dev_private;

	switch (priv->config.flow_prio) {
	case RTE_DIM(priority_map_3):
		res = priority_map_3[priority][subpriority];
		break;
	case RTE_DIM(priority_map_5):
		res = priority_map_5[priority][subpriority];
		break;
	}
	return  res;
}

/**
 * Verify the @p item specifications (spec, last, mask) are compatible with the
 * NIC capabilities.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] mask
 *   @p item->mask or flow default bit-masks.
 * @param[in] nic_mask
 *   Bit-masks covering supported fields by the NIC to compare with user mask.
 * @param[in] size
 *   Bit-masks size in bytes.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_item_acceptable(const struct rte_flow_item *item,
			  const uint8_t *mask,
			  const uint8_t *nic_mask,
			  unsigned int size,
			  struct rte_flow_error *error)
{
	unsigned int i;

	MLX5_ASSERT(nic_mask);
	for (i = 0; i < size; ++i)
		if ((nic_mask[i] | mask[i]) != nic_mask[i])
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "mask enables non supported"
						  " bits");
	if (!item->spec && (item->mask || item->last))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "mask/last without a spec is not"
					  " supported");
	if (item->spec && item->last) {
		uint8_t spec[size];
		uint8_t last[size];
		unsigned int i;
		int ret;

		for (i = 0; i < size; ++i) {
			spec[i] = ((const uint8_t *)item->spec)[i] & mask[i];
			last[i] = ((const uint8_t *)item->last)[i] & mask[i];
		}
		ret = memcmp(spec, last, size);
		if (ret != 0)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "range is not valid");
	}
	return 0;
}

/**
 * Adjust the hash fields according to the @p flow information.
 *
 * @param[in] dev_flow.
 *   Pointer to the mlx5_flow.
 * @param[in] tunnel
 *   1 when the hash field is for a tunnel item.
 * @param[in] layer_types
 *   ETH_RSS_* types.
 * @param[in] hash_fields
 *   Item hash fields.
 *
 * @return
 *   The hash fields that should be used.
 */
uint64_t
mlx5_flow_hashfields_adjust(struct mlx5_flow *dev_flow,
			    int tunnel __rte_unused, uint64_t layer_types,
			    uint64_t hash_fields)
{
	struct rte_flow *flow = dev_flow->flow;
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	int rss_request_inner = flow->rss.level >= 2;

	/* Check RSS hash level for tunnel. */
	if (tunnel && rss_request_inner)
		hash_fields |= IBV_RX_HASH_INNER;
	else if (tunnel || rss_request_inner)
		return 0;
#endif
	/* Check if requested layer matches RSS hash fields. */
	if (!(flow->rss.types & layer_types))
		return 0;
	return hash_fields;
}

/**
 * Lookup and set the ptype in the data Rx part.  A single Ptype can be used,
 * if several tunnel rules are used on this queue, the tunnel ptype will be
 * cleared.
 *
 * @param rxq_ctrl
 *   Rx queue to update.
 */
static void
flow_rxq_tunnel_ptype_update(struct mlx5_rxq_ctrl *rxq_ctrl)
{
	unsigned int i;
	uint32_t tunnel_ptype = 0;

	/* Look up for the ptype to use. */
	for (i = 0; i != MLX5_FLOW_TUNNEL; ++i) {
		if (!rxq_ctrl->flow_tunnels_n[i])
			continue;
		if (!tunnel_ptype) {
			tunnel_ptype = tunnels_info[i].ptype;
		} else {
			tunnel_ptype = 0;
			break;
		}
	}
	rxq_ctrl->rxq.tunnel = tunnel_ptype;
}

/**
 * Set the Rx queue flags (Mark/Flag and Tunnel Ptypes) according to the devive
 * flow.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] dev_flow
 *   Pointer to device flow structure.
 */
static void
flow_drv_rxq_flags_set(struct rte_eth_dev *dev, struct mlx5_flow *dev_flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = dev_flow->flow;
	const int mark = !!(dev_flow->actions &
			    (MLX5_FLOW_ACTION_FLAG | MLX5_FLOW_ACTION_MARK));
	const int tunnel = !!(dev_flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int i;

	for (i = 0; i != flow->rss.queue_num; ++i) {
		int idx = (*flow->rss.queue)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of((*priv->rxqs)[idx],
				     struct mlx5_rxq_ctrl, rxq);

		/*
		 * To support metadata register copy on Tx loopback,
		 * this must be always enabled (metadata may arive
		 * from other port - not from local flows only.
		 */
		if (priv->config.dv_flow_en &&
		    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
		    mlx5_flow_ext_mreg_supported(dev)) {
			rxq_ctrl->rxq.mark = 1;
			rxq_ctrl->flow_mark_n = 1;
		} else if (mark) {
			rxq_ctrl->rxq.mark = 1;
			rxq_ctrl->flow_mark_n++;
		}
		if (tunnel) {
			unsigned int j;

			/* Increase the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel &
				     dev_flow->layers) ==
				    tunnels_info[j].tunnel) {
					rxq_ctrl->flow_tunnels_n[j]++;
					break;
				}
			}
			flow_rxq_tunnel_ptype_update(rxq_ctrl);
		}
	}
}

/**
 * Set the Rx queue flags (Mark/Flag and Tunnel Ptypes) for a flow
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] flow
 *   Pointer to flow structure.
 */
static void
flow_rxq_flags_set(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next)
		flow_drv_rxq_flags_set(dev, dev_flow);
}

/**
 * Clear the Rx queue flags (Mark/Flag and Tunnel Ptype) associated with the
 * device flow if no other flow uses it with the same kind of request.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] dev_flow
 *   Pointer to the device flow.
 */
static void
flow_drv_rxq_flags_trim(struct rte_eth_dev *dev, struct mlx5_flow *dev_flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = dev_flow->flow;
	const int mark = !!(dev_flow->actions &
			    (MLX5_FLOW_ACTION_FLAG | MLX5_FLOW_ACTION_MARK));
	const int tunnel = !!(dev_flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int i;

	MLX5_ASSERT(dev->data->dev_started);
	for (i = 0; i != flow->rss.queue_num; ++i) {
		int idx = (*flow->rss.queue)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of((*priv->rxqs)[idx],
				     struct mlx5_rxq_ctrl, rxq);

		if (priv->config.dv_flow_en &&
		    priv->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
		    mlx5_flow_ext_mreg_supported(dev)) {
			rxq_ctrl->rxq.mark = 1;
			rxq_ctrl->flow_mark_n = 1;
		} else if (mark) {
			rxq_ctrl->flow_mark_n--;
			rxq_ctrl->rxq.mark = !!rxq_ctrl->flow_mark_n;
		}
		if (tunnel) {
			unsigned int j;

			/* Decrease the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel &
				     dev_flow->layers) ==
				    tunnels_info[j].tunnel) {
					rxq_ctrl->flow_tunnels_n[j]--;
					break;
				}
			}
			flow_rxq_tunnel_ptype_update(rxq_ctrl);
		}
	}
}

/**
 * Clear the Rx queue flags (Mark/Flag and Tunnel Ptype) associated with the
 * @p flow if no other flow uses it with the same kind of request.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to the flow.
 */
static void
flow_rxq_flags_trim(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next)
		flow_drv_rxq_flags_trim(dev, dev_flow);
}

/**
 * Clear the Mark/Flag and Tunnel ptype information in all Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
flow_rxq_flags_clear(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;
		unsigned int j;

		if (!(*priv->rxqs)[i])
			continue;
		rxq_ctrl = container_of((*priv->rxqs)[i],
					struct mlx5_rxq_ctrl, rxq);
		rxq_ctrl->flow_mark_n = 0;
		rxq_ctrl->rxq.mark = 0;
		for (j = 0; j != MLX5_FLOW_TUNNEL; ++j)
			rxq_ctrl->flow_tunnels_n[j] = 0;
		rxq_ctrl->rxq.tunnel = 0;
	}
}

/*
 * return a pointer to the desired action in the list of actions.
 *
 * @param[in] actions
 *   The list of actions to search the action in.
 * @param[in] action
 *   The action to find.
 *
 * @return
 *   Pointer to the action in the list, if found. NULL otherwise.
 */
const struct rte_flow_action *
mlx5_flow_find_action(const struct rte_flow_action *actions,
		      enum rte_flow_action_type action)
{
	if (actions == NULL)
		return NULL;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++)
		if (actions->type == action)
			return actions;
	return NULL;
}

/*
 * Validate the flag action.
 *
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_flag(uint64_t action_flags,
			       const struct rte_flow_attr *attr,
			       struct rte_flow_error *error)
{
	if (action_flags & MLX5_FLOW_ACTION_MARK)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't mark and flag in same flow");
	if (action_flags & MLX5_FLOW_ACTION_FLAG)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 flag"
					  " actions in same flow");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "flag action not supported for "
					  "egress");
	return 0;
}

/*
 * Validate the mark action.
 *
 * @param[in] action
 *   Pointer to the queue action.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_mark(const struct rte_flow_action *action,
			       uint64_t action_flags,
			       const struct rte_flow_attr *attr,
			       struct rte_flow_error *error)
{
	const struct rte_flow_action_mark *mark = action->conf;

	if (!mark)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "configuration cannot be null");
	if (mark->id >= MLX5_FLOW_MARK_MAX)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &mark->id,
					  "mark id must in 0 <= id < "
					  RTE_STR(MLX5_FLOW_MARK_MAX));
	if (action_flags & MLX5_FLOW_ACTION_FLAG)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't flag and mark in same flow");
	if (action_flags & MLX5_FLOW_ACTION_MARK)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 mark actions in same"
					  " flow");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "mark action not supported for "
					  "egress");
	return 0;
}

/*
 * Validate the drop action.
 *
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_drop(uint64_t action_flags __rte_unused,
			       const struct rte_flow_attr *attr,
			       struct rte_flow_error *error)
{
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "drop action not supported for "
					  "egress");
	return 0;
}

/*
 * Validate the queue action.
 *
 * @param[in] action
 *   Pointer to the queue action.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_queue(const struct rte_flow_action *action,
				uint64_t action_flags,
				struct rte_eth_dev *dev,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_queue *queue = action->conf;

	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions in"
					  " same flow");
	if (!priv->rxqs_n)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No Rx queues configured");
	if (queue->index >= priv->rxqs_n)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &queue->index,
					  "queue index out of range");
	if (!(*priv->rxqs)[queue->index])
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &queue->index,
					  "queue is not configured");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "queue action not supported for "
					  "egress");
	return 0;
}

/*
 * Validate the rss action.
 *
 * @param[in] action
 *   Pointer to the queue action.
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[in] item_flags
 *   Items that were detected.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_rss(const struct rte_flow_action *action,
			      uint64_t action_flags,
			      struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attr,
			      uint64_t item_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_rss *rss = action->conf;
	int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int i;

	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions"
					  " in same flow");
	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
	    rss->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->func,
					  "RSS hash function not supported");
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (rss->level > 2)
#else
	if (rss->level > 1)
#endif
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->level,
					  "tunnel RSS is not supported");
	/* allow RSS key_len 0 in case of NULL (default) RSS key. */
	if (rss->key_len == 0 && rss->key != NULL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->key_len,
					  "RSS hash key length 0");
	if (rss->key_len > 0 && rss->key_len < MLX5_RSS_HASH_KEY_LEN)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->key_len,
					  "RSS hash key too small");
	if (rss->key_len > MLX5_RSS_HASH_KEY_LEN)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->key_len,
					  "RSS hash key too large");
	if (rss->queue_num > priv->config.ind_table_max_size)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->queue_num,
					  "number of queues too large");
	if (rss->types & MLX5_RSS_HF_MASK)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->types,
					  "some RSS protocols are not"
					  " supported");
	if ((rss->types & (ETH_RSS_L3_SRC_ONLY | ETH_RSS_L3_DST_ONLY)) &&
	    !(rss->types & ETH_RSS_IP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "L3 partial RSS requested but L3 RSS"
					  " type not specified");
	if ((rss->types & (ETH_RSS_L4_SRC_ONLY | ETH_RSS_L4_DST_ONLY)) &&
	    !(rss->types & (ETH_RSS_UDP | ETH_RSS_TCP)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "L4 partial RSS requested but L4 RSS"
					  " type not specified");
	if (!priv->rxqs_n)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No Rx queues configured");
	if (!rss->queue_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No queues configured");
	for (i = 0; i != rss->queue_num; ++i) {
		if (rss->queue[i] >= priv->rxqs_n)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 &rss->queue[i], "queue index out of range");
		if (!(*priv->rxqs)[rss->queue[i]])
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 &rss->queue[i], "queue is not configured");
	}
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "rss action not supported for "
					  "egress");
	if (rss->level > 1 &&  !tunnel)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "inner RSS is not supported for "
					  "non-tunnel flows");
	return 0;
}

/*
 * Validate the count action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_count(struct rte_eth_dev *dev __rte_unused,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "count action not supported for "
					  "egress");
	return 0;
}

/**
 * Verify the @p attributes will be correctly understood by the NIC and store
 * them in the @p flow if everything is correct.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_attributes(struct rte_eth_dev *dev,
			      const struct rte_flow_attr *attributes,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t priority_max = priv->config.flow_prio - 1;

	if (attributes->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL, "groups is not supported");
	if (attributes->priority != MLX5_FLOW_PRIO_RSVD &&
	    attributes->priority >= priority_max)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY,
					  NULL, "priority out of range");
	if (attributes->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "egress is not supported");
	if (attributes->transfer && !priv->config.dv_esw_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL, "transfer is not supported");
	if (!attributes->ingress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "ingress attribute is mandatory");
	return 0;
}

/**
 * Validate ICMP6 item.
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
int
mlx5_flow_validate_item_icmp6(const struct rte_flow_item *item,
			       uint64_t item_flags,
			       uint8_t target_protocol,
			       struct rte_flow_error *error)
{
	const struct rte_flow_item_icmp6 *mask = item->mask;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV6;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;

	if (target_protocol != 0xFF && target_protocol != IPPROTO_ICMPV6)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with ICMP6 layer");
	if (!(item_flags & l3m))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "IPv6 is mandatory to filter on"
					  " ICMP6");
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_icmp6_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_icmp6_mask,
		 sizeof(struct rte_flow_item_icmp6), error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate ICMP item.
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
int
mlx5_flow_validate_item_icmp(const struct rte_flow_item *item,
			     uint64_t item_flags,
			     uint8_t target_protocol,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_icmp *mask = item->mask;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				      MLX5_FLOW_LAYER_OUTER_L3_IPV4;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;

	if (target_protocol != 0xFF && target_protocol != IPPROTO_ICMP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with ICMP layer");
	if (!(item_flags & l3m))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "IPv4 is mandatory to filter"
					  " on ICMP");
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_icmp_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_icmp_mask,
		 sizeof(struct rte_flow_item_icmp), error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate Ethernet item.
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
int
mlx5_flow_validate_item_eth(const struct rte_flow_item *item,
			    uint64_t item_flags,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *mask = item->mask;
	const struct rte_flow_item_eth nic_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.type = RTE_BE16(0xffff),
	};
	int ret;
	int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t ethm = tunnel ? MLX5_FLOW_LAYER_INNER_L2	:
				       MLX5_FLOW_LAYER_OUTER_L2;

	if (item_flags & ethm)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L2 layers not supported");
	if ((!tunnel && (item_flags & MLX5_FLOW_LAYER_OUTER_L3)) ||
	    (tunnel && (item_flags & MLX5_FLOW_LAYER_INNER_L3)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L2 layer should not follow "
					  "L3 layers");
	if ((!tunnel && (item_flags & MLX5_FLOW_LAYER_OUTER_VLAN)) ||
	    (tunnel && (item_flags & MLX5_FLOW_LAYER_INNER_VLAN)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L2 layer should not follow VLAN");
	if (!mask)
		mask = &rte_flow_item_eth_mask;
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_eth),
					error);
	return ret;
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
int
mlx5_flow_validate_item_vlan(const struct rte_flow_item *item,
			     uint64_t item_flags,
			     struct rte_eth_dev *dev,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	const struct rte_flow_item_vlan nic_mask = {
		.tci = RTE_BE16(UINT16_MAX),
		.inner_type = RTE_BE16(UINT16_MAX),
	};
	uint16_t vlan_tag = 0;
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
	if (spec) {
		vlan_tag = spec->tci;
		vlan_tag &= mask->tci;
	}
	/*
	 * From verbs perspective an empty VLAN is equivalent
	 * to a packet without VLAN layer.
	 */
	if (!vlan_tag)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "VLAN cannot be empty");
	return 0;
}

/**
 * Validate IPV4 item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] acc_mask
 *   Acceptable mask, if NULL default internal default mask
 *   will be used to check whether item fields are supported.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_ipv4(const struct rte_flow_item *item,
			     uint64_t item_flags,
			     uint64_t last_item,
			     uint16_t ether_type,
			     const struct rte_flow_item_ipv4 *acc_mask,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *mask = item->mask;
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 nic_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.next_proto_id = 0xff,
		},
	};
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;
	uint8_t next_proto = 0xFF;
	const uint64_t l2_vlan = (MLX5_FLOW_LAYER_L2 |
				  MLX5_FLOW_LAYER_OUTER_VLAN |
				  MLX5_FLOW_LAYER_INNER_VLAN);

	if ((last_item & l2_vlan) && ether_type &&
	    ether_type != RTE_ETHER_TYPE_IPV4)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "IPv4 cannot follow L2/VLAN layer "
					  "which ether type is not IPv4");
	if (item_flags & MLX5_FLOW_LAYER_IPIP) {
		if (mask && spec)
			next_proto = mask->hdr.next_proto_id &
				     spec->hdr.next_proto_id;
		if (next_proto == IPPROTO_IPIP || next_proto == IPPROTO_IPV6)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "multiple tunnel "
						  "not supported");
	}
	if (item_flags & MLX5_FLOW_LAYER_IPV6_ENCAP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "wrong tunnel type - IPv6 specified "
					  "but IPv4 item provided");
	if (item_flags & l3m)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L3 layers not supported");
	else if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 cannot follow an L4 layer.");
	else if ((item_flags & MLX5_FLOW_LAYER_NVGRE) &&
		  !(item_flags & MLX5_FLOW_LAYER_INNER_L2))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 cannot follow an NVGRE layer.");
	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
	else if (mask->hdr.next_proto_id != 0 &&
		 mask->hdr.next_proto_id != 0xff)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "partial mask is not supported"
					  " for protocol");
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					acc_mask ? (const uint8_t *)acc_mask
						 : (const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_ipv4),
					error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate IPV6 item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] acc_mask
 *   Acceptable mask, if NULL default internal default mask
 *   will be used to check whether item fields are supported.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_ipv6(const struct rte_flow_item *item,
			     uint64_t item_flags,
			     uint64_t last_item,
			     uint16_t ether_type,
			     const struct rte_flow_item_ipv6 *acc_mask,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *mask = item->mask;
	const struct rte_flow_item_ipv6 *spec = item->spec;
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
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;
	uint8_t next_proto = 0xFF;
	const uint64_t l2_vlan = (MLX5_FLOW_LAYER_L2 |
				  MLX5_FLOW_LAYER_OUTER_VLAN |
				  MLX5_FLOW_LAYER_INNER_VLAN);

	if ((last_item & l2_vlan) && ether_type &&
	    ether_type != RTE_ETHER_TYPE_IPV6)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "IPv6 cannot follow L2/VLAN layer "
					  "which ether type is not IPv6");
	if (item_flags & MLX5_FLOW_LAYER_IPV6_ENCAP) {
		if (mask && spec)
			next_proto = mask->hdr.proto & spec->hdr.proto;
		if (next_proto == IPPROTO_IPIP || next_proto == IPPROTO_IPV6)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "multiple tunnel "
						  "not supported");
	}
	if (item_flags & MLX5_FLOW_LAYER_IPIP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "wrong tunnel type - IPv4 specified "
					  "but IPv6 item provided");
	if (item_flags & l3m)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L3 layers not supported");
	else if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 cannot follow an L4 layer.");
	else if ((item_flags & MLX5_FLOW_LAYER_NVGRE) &&
		  !(item_flags & MLX5_FLOW_LAYER_INNER_L2))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 cannot follow an NVGRE layer.");
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					acc_mask ? (const uint8_t *)acc_mask
						 : (const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_ipv6),
					error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate UDP item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[in] flow_mask
 *   mlx5 flow-specific (DV, verbs, etc.) supported header fields mask.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_udp(const struct rte_flow_item *item,
			    uint64_t item_flags,
			    uint8_t target_protocol,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *mask = item->mask;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;

	if (target_protocol != 0xff && target_protocol != IPPROTO_UDP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with UDP layer");
	if (!(item_flags & l3m))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 is mandatory to filter on L4");
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_udp_mask,
		 sizeof(struct rte_flow_item_udp), error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate TCP item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_tcp(const struct rte_flow_item *item,
			    uint64_t item_flags,
			    uint8_t target_protocol,
			    const struct rte_flow_item_tcp *flow_mask,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *mask = item->mask;
	const int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	const uint64_t l3m = tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
				      MLX5_FLOW_LAYER_OUTER_L3;
	const uint64_t l4m = tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				      MLX5_FLOW_LAYER_OUTER_L4;
	int ret;

	MLX5_ASSERT(flow_mask);
	if (target_protocol != 0xff && target_protocol != IPPROTO_TCP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with TCP layer");
	if (!(item_flags & l3m))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 is mandatory to filter on L4");
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)flow_mask,
		 sizeof(struct rte_flow_item_tcp), error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate VXLAN item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_vxlan(const struct rte_flow_item *item,
			      uint64_t item_flags,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	int ret;
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };
	uint32_t vlan_id = 0;


	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_vxlan_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_vxlan_mask,
		 sizeof(struct rte_flow_item_vxlan),
		 error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&id.vni[1], spec->vni, 3);
		vlan_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vlan_id &= id.vlan_id;
	}
	/*
	 * Tunnel id 0 is equivalent as not adding a VXLAN layer, if
	 * only this layer is defined in the Verbs specification it is
	 * interpreted as wildcard and all packets will match this
	 * rule, if it follows a full stack layer (ex: eth / ipv4 /
	 * udp), all packets matching the layers before will also
	 * match this rule.  To avoid such situation, VNI 0 is
	 * currently refused.
	 */
	if (!vlan_id)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VXLAN vni cannot be 0");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VXLAN tunnel must be fully defined");
	return 0;
}

/**
 * Validate VXLAN_GPE item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] priv
 *   Pointer to the private data structure.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_vxlan_gpe(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  struct rte_eth_dev *dev,
				  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_vxlan_gpe *spec = item->spec;
	const struct rte_flow_item_vxlan_gpe *mask = item->mask;
	int ret;
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };
	uint32_t vlan_id = 0;

	if (!priv->config.l3_vxlan_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 VXLAN is not enabled by device"
					  " parameter and/or not configured in"
					  " firmware");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_vxlan_gpe_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_vxlan_gpe_mask,
		 sizeof(struct rte_flow_item_vxlan_gpe),
		 error);
	if (ret < 0)
		return ret;
	if (spec) {
		if (spec->protocol)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "VxLAN-GPE protocol"
						  " not supported");
		memcpy(&id.vni[1], spec->vni, 3);
		vlan_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vlan_id &= id.vlan_id;
	}
	/*
	 * Tunnel id 0 is equivalent as not adding a VXLAN layer, if only this
	 * layer is defined in the Verbs specification it is interpreted as
	 * wildcard and all packets will match this rule, if it follows a full
	 * stack layer (ex: eth / ipv4 / udp), all packets matching the layers
	 * before will also match this rule.  To avoid such situation, VNI 0
	 * is currently refused.
	 */
	if (!vlan_id)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VXLAN-GPE vni cannot be 0");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VXLAN-GPE tunnel must be fully"
					  " defined");
	return 0;
}
/**
 * Validate GRE Key item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit flags to mark detected items.
 * @param[in] gre_item
 *   Pointer to gre_item
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_gre_key(const struct rte_flow_item *item,
				uint64_t item_flags,
				const struct rte_flow_item *gre_item,
				struct rte_flow_error *error)
{
	const rte_be32_t *mask = item->mask;
	int ret = 0;
	rte_be32_t gre_key_default_mask = RTE_BE32(UINT32_MAX);
	const struct rte_flow_item_gre *gre_spec;
	const struct rte_flow_item_gre *gre_mask;

	if (item_flags & MLX5_FLOW_LAYER_GRE_KEY)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Multiple GRE key not support");
	if (!(item_flags & MLX5_FLOW_LAYER_GRE))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "No preceding GRE header");
	if (item_flags & MLX5_FLOW_LAYER_INNER)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "GRE key following a wrong item");
	gre_mask = gre_item->mask;
	if (!gre_mask)
		gre_mask = &rte_flow_item_gre_mask;
	gre_spec = gre_item->spec;
	if (gre_spec && (gre_mask->c_rsvd0_ver & RTE_BE16(0x2000)) &&
			 !(gre_spec->c_rsvd0_ver & RTE_BE16(0x2000)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Key bit must be on");

	if (!mask)
		mask = &gre_key_default_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&gre_key_default_mask,
		 sizeof(rte_be32_t), error);
	return ret;
}

/**
 * Validate GRE item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit flags to mark detected items.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_gre(const struct rte_flow_item *item,
			    uint64_t item_flags,
			    uint8_t target_protocol,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_gre *spec __rte_unused = item->spec;
	const struct rte_flow_item_gre *mask = item->mask;
	int ret;
	const struct rte_flow_item_gre nic_mask = {
		.c_rsvd0_ver = RTE_BE16(0xB000),
		.protocol = RTE_BE16(UINT16_MAX),
	};

	if (target_protocol != 0xff && target_protocol != IPPROTO_GRE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with this GRE layer");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 Layer is missing");
	if (!mask)
		mask = &rte_flow_item_gre_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_gre), error);
	if (ret < 0)
		return ret;
#ifndef HAVE_MLX5DV_DR
#ifndef HAVE_IBV_DEVICE_MPLS_SUPPORT
	if (spec && (spec->protocol & mask->protocol))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "without MPLS support the"
					  " specification cannot be used for"
					  " filtering");
#endif
#endif
	return 0;
}

/**
 * Validate Geneve item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] itemFlags
 *   Bit-fields that holds the items detected until now.
 * @param[in] enPriv
 *   Pointer to the private data structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

int
mlx5_flow_validate_item_geneve(const struct rte_flow_item *item,
			       uint64_t item_flags,
			       struct rte_eth_dev *dev,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_geneve *spec = item->spec;
	const struct rte_flow_item_geneve *mask = item->mask;
	int ret;
	uint16_t gbhdr;
	uint8_t opt_len = priv->config.hca_attr.geneve_max_opt_len ?
			  MLX5_GENEVE_OPT_LEN_1 : MLX5_GENEVE_OPT_LEN_0;
	const struct rte_flow_item_geneve nic_mask = {
		.ver_opt_len_o_c_rsvd0 = RTE_BE16(0x3f80),
		.vni = "\xff\xff\xff",
		.protocol = RTE_BE16(UINT16_MAX),
	};

	if (!(priv->config.hca_attr.flex_parser_protocols &
	      MLX5_HCA_FLEX_GENEVE_ENABLED) ||
	    !priv->config.hca_attr.tunnel_stateless_geneve_rx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 Geneve is not enabled by device"
					  " parameter and/or not configured in"
					  " firmware");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_geneve_mask;
	ret = mlx5_flow_item_acceptable
				  (item, (const uint8_t *)mask,
				   (const uint8_t *)&nic_mask,
				   sizeof(struct rte_flow_item_geneve), error);
	if (ret)
		return ret;
	if (spec) {
		gbhdr = rte_be_to_cpu_16(spec->ver_opt_len_o_c_rsvd0);
		if (MLX5_GENEVE_VER_VAL(gbhdr) ||
		     MLX5_GENEVE_CRITO_VAL(gbhdr) ||
		     MLX5_GENEVE_RSVD_VAL(gbhdr) || spec->rsvd1)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "Geneve protocol unsupported"
						  " fields are being used");
		if (MLX5_GENEVE_OPTLEN_VAL(gbhdr) > opt_len)
			return rte_flow_error_set
					(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ITEM,
					 item,
					 "Unsupported Geneve options length");
	}
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set
				    (error, ENOTSUP,
				     RTE_FLOW_ERROR_TYPE_ITEM, item,
				     "Geneve tunnel must be fully defined");
	return 0;
}

/**
 * Validate MPLS item.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] prev_layer
 *   The protocol layer indicated in previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_mpls(struct rte_eth_dev *dev __rte_unused,
			     const struct rte_flow_item *item __rte_unused,
			     uint64_t item_flags __rte_unused,
			     uint64_t prev_layer __rte_unused,
			     struct rte_flow_error *error)
{
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	const struct rte_flow_item_mpls *mask = item->mask;
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	if (!priv->config.mpls_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "MPLS not supported or"
					  " disabled in firmware"
					  " configuration.");
	/* MPLS over IP, UDP, GRE is allowed */
	if (!(prev_layer & (MLX5_FLOW_LAYER_OUTER_L3 |
			    MLX5_FLOW_LAYER_OUTER_L4_UDP |
			    MLX5_FLOW_LAYER_GRE)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with MPLS layer");
	/* Multi-tunnel isn't allowed but MPLS over GRE is an exception. */
	if ((item_flags & MLX5_FLOW_LAYER_TUNNEL) &&
	    !(item_flags & MLX5_FLOW_LAYER_GRE))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	if (!mask)
		mask = &rte_flow_item_mpls_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_mpls_mask,
		 sizeof(struct rte_flow_item_mpls), error);
	if (ret < 0)
		return ret;
	return 0;
#endif
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_ITEM, item,
				  "MPLS is not supported by Verbs, please"
				  " update.");
}

/**
 * Validate NVGRE item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit flags to mark detected items.
 * @param[in] target_protocol
 *   The next protocol in the previous item.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_nvgre(const struct rte_flow_item *item,
			      uint64_t item_flags,
			      uint8_t target_protocol,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *mask = item->mask;
	int ret;

	if (target_protocol != 0xff && target_protocol != IPPROTO_GRE)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "protocol filtering not compatible"
					  " with this GRE layer");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L3 Layer is missing");
	if (!mask)
		mask = &rte_flow_item_nvgre_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_nvgre_mask,
		 sizeof(struct rte_flow_item_nvgre), error);
	if (ret < 0)
		return ret;
	return 0;
}

/* Allocate unique ID for the split Q/RSS subflows. */
static uint32_t
flow_qrss_get_id(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t qrss_id, ret;

	ret = mlx5_flow_id_get(priv->qrss_id_pool, &qrss_id);
	if (ret)
		return 0;
	MLX5_ASSERT(qrss_id);
	return qrss_id;
}

/* Free unique ID for the split Q/RSS subflows. */
static void
flow_qrss_free_id(struct rte_eth_dev *dev,  uint32_t qrss_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (qrss_id)
		mlx5_flow_id_release(priv->qrss_id_pool, qrss_id);
}

/**
 * Release resource related QUEUE/RSS action split.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Flow to release id's from.
 */
static void
flow_mreg_split_qrss_release(struct rte_eth_dev *dev,
			     struct rte_flow *flow)
{
	struct mlx5_flow *dev_flow;

	LIST_FOREACH(dev_flow, &flow->dev_flows, next)
		if (dev_flow->qrss_id)
			flow_qrss_free_id(dev, dev_flow->qrss_id);
}

static int
flow_null_validate(struct rte_eth_dev *dev __rte_unused,
		   const struct rte_flow_attr *attr __rte_unused,
		   const struct rte_flow_item items[] __rte_unused,
		   const struct rte_flow_action actions[] __rte_unused,
		   bool external __rte_unused,
		   struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
}

static struct mlx5_flow *
flow_null_prepare(const struct rte_flow_attr *attr __rte_unused,
		  const struct rte_flow_item items[] __rte_unused,
		  const struct rte_flow_action actions[] __rte_unused,
		  struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
	return NULL;
}

static int
flow_null_translate(struct rte_eth_dev *dev __rte_unused,
		    struct mlx5_flow *dev_flow __rte_unused,
		    const struct rte_flow_attr *attr __rte_unused,
		    const struct rte_flow_item items[] __rte_unused,
		    const struct rte_flow_action actions[] __rte_unused,
		    struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
}

static int
flow_null_apply(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow __rte_unused,
		struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
}

static void
flow_null_remove(struct rte_eth_dev *dev __rte_unused,
		 struct rte_flow *flow __rte_unused)
{
}

static void
flow_null_destroy(struct rte_eth_dev *dev __rte_unused,
		  struct rte_flow *flow __rte_unused)
{
}

static int
flow_null_query(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow __rte_unused,
		const struct rte_flow_action *actions __rte_unused,
		void *data __rte_unused,
		struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
}

/* Void driver to protect from null pointer reference. */
const struct mlx5_flow_driver_ops mlx5_flow_null_drv_ops = {
	.validate = flow_null_validate,
	.prepare = flow_null_prepare,
	.translate = flow_null_translate,
	.apply = flow_null_apply,
	.remove = flow_null_remove,
	.destroy = flow_null_destroy,
	.query = flow_null_query,
};

/**
 * Select flow driver type according to flow attributes and device
 * configuration.
 *
 * @param[in] dev
 *   Pointer to the dev structure.
 * @param[in] attr
 *   Pointer to the flow attributes.
 *
 * @return
 *   flow driver type, MLX5_FLOW_TYPE_MAX otherwise.
 */
static enum mlx5_flow_drv_type
flow_get_drv_type(struct rte_eth_dev *dev, const struct rte_flow_attr *attr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	enum mlx5_flow_drv_type type = MLX5_FLOW_TYPE_MAX;

	if (attr->transfer && priv->config.dv_esw_en)
		type = MLX5_FLOW_TYPE_DV;
	if (!attr->transfer)
		type = priv->config.dv_flow_en ? MLX5_FLOW_TYPE_DV :
						 MLX5_FLOW_TYPE_VERBS;
	return type;
}

#define flow_get_drv_ops(type) flow_drv_ops[type]

/**
 * Flow driver validation API. This abstracts calling driver specific functions.
 * The type of flow driver is determined according to flow attributes.
 *
 * @param[in] dev
 *   Pointer to the dev structure.
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
static inline int
flow_drv_validate(struct rte_eth_dev *dev,
		  const struct rte_flow_attr *attr,
		  const struct rte_flow_item items[],
		  const struct rte_flow_action actions[],
		  bool external, struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow_get_drv_type(dev, attr);

	fops = flow_get_drv_ops(type);
	return fops->validate(dev, attr, items, actions, external, error);
}

/**
 * Flow driver preparation API. This abstracts calling driver specific
 * functions. Parent flow (rte_flow) should have driver type (drv_type). It
 * calculates the size of memory required for device flow, allocates the memory,
 * initializes the device flow and returns the pointer.
 *
 * @note
 *   This function initializes device flow structure such as dv or verbs in
 *   struct mlx5_flow. However, it is caller's responsibility to initialize the
 *   rest. For example, adding returning device flow to flow->dev_flow list and
 *   setting backward reference to the flow should be done out of this function.
 *   layers field is not filled either.
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
 *   Pointer to device flow on success, otherwise NULL and rte_errno is set.
 */
static inline struct mlx5_flow *
flow_drv_prepare(const struct rte_flow *flow,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	return fops->prepare(attr, items, actions, error);
}

/**
 * Flow driver translation API. This abstracts calling driver specific
 * functions. Parent flow (rte_flow) should have driver type (drv_type). It
 * translates a generic flow into a driver flow. flow_drv_prepare() must
 * precede.
 *
 * @note
 *   dev_flow->layers could be filled as a result of parsing during translation
 *   if needed by flow_drv_apply(). dev_flow->flow->actions can also be filled
 *   if necessary. As a flow can have multiple dev_flows by RSS flow expansion,
 *   flow->actions could be overwritten even though all the expanded dev_flows
 *   have the same actions.
 *
 * @param[in] dev
 *   Pointer to the rte dev structure.
 * @param[in, out] dev_flow
 *   Pointer to the mlx5 flow.
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
static inline int
flow_drv_translate(struct rte_eth_dev *dev, struct mlx5_flow *dev_flow,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = dev_flow->flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	return fops->translate(dev, dev_flow, attr, items, actions, error);
}

/**
 * Flow driver apply API. This abstracts calling driver specific functions.
 * Parent flow (rte_flow) should have driver type (drv_type). It applies
 * translated driver flows on to device. flow_drv_translate() must precede.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static inline int
flow_drv_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
	       struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	return fops->apply(dev, flow, error);
}

/**
 * Flow driver remove API. This abstracts calling driver specific functions.
 * Parent flow (rte_flow) should have driver type (drv_type). It removes a flow
 * on device. All the resources of the flow should be freed by calling
 * flow_drv_destroy().
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static inline void
flow_drv_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	fops->remove(dev, flow);
}

/**
 * Flow driver destroy API. This abstracts calling driver specific functions.
 * Parent flow (rte_flow) should have driver type (drv_type). It removes a flow
 * on device and releases resources of the flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static inline void
flow_drv_destroy(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	flow_mreg_split_qrss_release(dev, flow);
	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	fops->destroy(dev, flow);
}

/**
 * Validate a flow supported by the NIC.
 *
 * @see rte_flow_validate()
 * @see rte_flow_ops
 */
int
mlx5_flow_validate(struct rte_eth_dev *dev,
		   const struct rte_flow_attr *attr,
		   const struct rte_flow_item items[],
		   const struct rte_flow_action actions[],
		   struct rte_flow_error *error)
{
	int ret;

	ret = flow_drv_validate(dev, attr, items, actions, true, error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Get port id item from the item list.
 *
 * @param[in] item
 *   Pointer to the list of items.
 *
 * @return
 *   Pointer to the port id item if exist, else return NULL.
 */
static const struct rte_flow_item *
find_port_id_item(const struct rte_flow_item *item)
{
	MLX5_ASSERT(item);
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_PORT_ID)
			return item;
	}
	return NULL;
}

/**
 * Get RSS action from the action list.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 *
 * @return
 *   Pointer to the RSS action if exist, else return NULL.
 */
static const struct rte_flow_action_rss*
flow_get_rss_action(const struct rte_flow_action actions[])
{
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			return (const struct rte_flow_action_rss *)
			       actions->conf;
		default:
			break;
		}
	}
	return NULL;
}

static unsigned int
find_graph_root(const struct rte_flow_item pattern[], uint32_t rss_level)
{
	const struct rte_flow_item *item;
	unsigned int has_vlan = 0;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_VLAN) {
			has_vlan = 1;
			break;
		}
	}
	if (has_vlan)
		return rss_level < 2 ? MLX5_EXPANSION_ROOT_ETH_VLAN :
				       MLX5_EXPANSION_ROOT_OUTER_ETH_VLAN;
	return rss_level < 2 ? MLX5_EXPANSION_ROOT :
			       MLX5_EXPANSION_ROOT_OUTER;
}

/**
 * Get QUEUE/RSS action from the action list.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] qrss
 *   Pointer to the return pointer.
 * @param[out] qrss_type
 *   Pointer to the action type to return. RTE_FLOW_ACTION_TYPE_END is returned
 *   if no QUEUE/RSS is found.
 *
 * @return
 *   Total number of actions.
 */
static int
flow_parse_qrss_action(const struct rte_flow_action actions[],
		       const struct rte_flow_action **qrss)
{
	int actions_n = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
			*qrss = actions;
			break;
		default:
			break;
		}
		actions_n++;
	}
	/* Count RTE_FLOW_ACTION_TYPE_END. */
	return actions_n + 1;
}

/**
 * Check meter action from the action list.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] mtr
 *   Pointer to the meter exist flag.
 *
 * @return
 *   Total number of actions.
 */
static int
flow_check_meter_action(const struct rte_flow_action actions[], uint32_t *mtr)
{
	int actions_n = 0;

	MLX5_ASSERT(mtr);
	*mtr = 0;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_METER:
			*mtr = 1;
			break;
		default:
			break;
		}
		actions_n++;
	}
	/* Count RTE_FLOW_ACTION_TYPE_END. */
	return actions_n + 1;
}

/**
 * Check if the flow should be splited due to hairpin.
 * The reason for the split is that in current HW we can't
 * support encap on Rx, so if a flow have encap we move it
 * to Tx.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 *
 * @return
 *   > 0 the number of actions and the flow should be split,
 *   0 when no split required.
 */
static int
flow_check_hairpin_split(struct rte_eth_dev *dev,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_action actions[])
{
	int queue_action = 0;
	int action_n = 0;
	int encap = 0;
	const struct rte_flow_action_queue *queue;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_action_raw_encap *raw_encap;

	if (!attr->ingress)
		return 0;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue = actions->conf;
			if (queue == NULL)
				return 0;
			if (mlx5_rxq_get_type(dev, queue->index) !=
			    MLX5_RXQ_TYPE_HAIRPIN)
				return 0;
			queue_action = 1;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			if (rss == NULL || rss->queue_num == 0)
				return 0;
			if (mlx5_rxq_get_type(dev, rss->queue[0]) !=
			    MLX5_RXQ_TYPE_HAIRPIN)
				return 0;
			queue_action = 1;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			encap = 1;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size >
			    (sizeof(struct rte_flow_item_eth) +
			     sizeof(struct rte_flow_item_ipv4)))
				encap = 1;
			action_n++;
			break;
		default:
			action_n++;
			break;
		}
	}
	if (encap == 1 && queue_action)
		return action_n;
	return 0;
}

/* Declare flow create/destroy prototype in advance. */
static struct rte_flow *
flow_list_create(struct rte_eth_dev *dev, struct mlx5_flows *list,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 bool external, struct rte_flow_error *error);

static void
flow_list_destroy(struct rte_eth_dev *dev, struct mlx5_flows *list,
		  struct rte_flow *flow);

/**
 * Add a flow of copying flow metadata registers in RX_CP_TBL.
 *
 * As mark_id is unique, if there's already a registered flow for the mark_id,
 * return by increasing the reference counter of the resource. Otherwise, create
 * the resource (mcp_res) and flow.
 *
 * Flow looks like,
 *   - If ingress port is ANY and reg_c[1] is mark_id,
 *     flow_tag := mark_id, reg_b := reg_c[0] and jump to RX_ACT_TBL.
 *
 * For default flow (zero mark_id), flow is like,
 *   - If ingress port is ANY,
 *     reg_b := reg_c[0] and jump to RX_ACT_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param mark_id
 *   ID of MARK action, zero means default flow for META.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   Associated resource on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_mreg_copy_resource *
flow_mreg_add_copy_action(struct rte_eth_dev *dev, uint32_t mark_id,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_attr attr = {
		.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
		.ingress = 1,
	};
	struct mlx5_rte_flow_item_tag tag_spec = {
		.data = mark_id,
	};
	struct rte_flow_item items[] = {
		[1] = { .type = RTE_FLOW_ITEM_TYPE_END, },
	};
	struct rte_flow_action_mark ftag = {
		.id = mark_id,
	};
	struct mlx5_flow_action_copy_mreg cp_mreg = {
		.dst = REG_B,
		.src = 0,
	};
	struct rte_flow_action_jump jump = {
		.group = MLX5_FLOW_MREG_ACT_TABLE_GROUP,
	};
	struct rte_flow_action actions[] = {
		[3] = { .type = RTE_FLOW_ACTION_TYPE_END, },
	};
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	int ret;

	/* Fill the register fileds in the flow. */
	ret = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (ret < 0)
		return NULL;
	tag_spec.id = ret;
	ret = mlx5_flow_get_reg_id(dev, MLX5_METADATA_RX, 0, error);
	if (ret < 0)
		return NULL;
	cp_mreg.src = ret;
	/* Check if already registered. */
	MLX5_ASSERT(priv->mreg_cp_tbl);
	mcp_res = (void *)mlx5_hlist_lookup(priv->mreg_cp_tbl, mark_id);
	if (mcp_res) {
		/* For non-default rule. */
		if (mark_id != MLX5_DEFAULT_COPY_ID)
			mcp_res->refcnt++;
		MLX5_ASSERT(mark_id != MLX5_DEFAULT_COPY_ID ||
			    mcp_res->refcnt == 1);
		return mcp_res;
	}
	/* Provide the full width of FLAG specific value. */
	if (mark_id == (priv->sh->dv_regc0_mask & MLX5_FLOW_MARK_DEFAULT))
		tag_spec.data = MLX5_FLOW_MARK_DEFAULT;
	/* Build a new flow. */
	if (mark_id != MLX5_DEFAULT_COPY_ID) {
		items[0] = (struct rte_flow_item){
			.type = MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &tag_spec,
		};
		items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_END,
		};
		actions[0] = (struct rte_flow_action){
			.type = MLX5_RTE_FLOW_ACTION_TYPE_MARK,
			.conf = &ftag,
		};
		actions[1] = (struct rte_flow_action){
			.type = MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
			.conf = &cp_mreg,
		};
		actions[2] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump,
		};
		actions[3] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_END,
		};
	} else {
		/* Default rule, wildcard match. */
		attr.priority = MLX5_FLOW_PRIO_RSVD;
		items[0] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_END,
		};
		actions[0] = (struct rte_flow_action){
			.type = MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
			.conf = &cp_mreg,
		};
		actions[1] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump,
		};
		actions[2] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_END,
		};
	}
	/* Build a new entry. */
	mcp_res = rte_zmalloc(__func__, sizeof(*mcp_res), 0);
	if (!mcp_res) {
		rte_errno = ENOMEM;
		return NULL;
	}
	/*
	 * The copy Flows are not included in any list. There
	 * ones are referenced from other Flows and can not
	 * be applied, removed, deleted in ardbitrary order
	 * by list traversing.
	 */
	mcp_res->flow = flow_list_create(dev, NULL, &attr, items,
					 actions, false, error);
	if (!mcp_res->flow)
		goto error;
	mcp_res->refcnt++;
	mcp_res->hlist_ent.key = mark_id;
	ret = mlx5_hlist_insert(priv->mreg_cp_tbl,
				&mcp_res->hlist_ent);
	MLX5_ASSERT(!ret);
	if (ret)
		goto error;
	return mcp_res;
error:
	if (mcp_res->flow)
		flow_list_destroy(dev, NULL, mcp_res->flow);
	rte_free(mcp_res);
	return NULL;
}

/**
 * Release flow in RX_CP_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @flow
 *   Parent flow for wich copying is provided.
 */
static void
flow_mreg_del_copy_action(struct rte_eth_dev *dev,
			  struct rte_flow *flow)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res = flow->mreg_copy;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!mcp_res || !priv->mreg_cp_tbl)
		return;
	if (flow->copy_applied) {
		MLX5_ASSERT(mcp_res->appcnt);
		flow->copy_applied = 0;
		--mcp_res->appcnt;
		if (!mcp_res->appcnt)
			flow_drv_remove(dev, mcp_res->flow);
	}
	/*
	 * We do not check availability of metadata registers here,
	 * because copy resources are not allocated in this case.
	 */
	if (--mcp_res->refcnt)
		return;
	MLX5_ASSERT(mcp_res->flow);
	flow_list_destroy(dev, NULL, mcp_res->flow);
	mlx5_hlist_remove(priv->mreg_cp_tbl, &mcp_res->hlist_ent);
	rte_free(mcp_res);
	flow->mreg_copy = NULL;
}

/**
 * Start flow in RX_CP_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @flow
 *   Parent flow for wich copying is provided.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_mreg_start_copy_action(struct rte_eth_dev *dev,
			    struct rte_flow *flow)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res = flow->mreg_copy;
	int ret;

	if (!mcp_res || flow->copy_applied)
		return 0;
	if (!mcp_res->appcnt) {
		ret = flow_drv_apply(dev, mcp_res->flow, NULL);
		if (ret)
			return ret;
	}
	++mcp_res->appcnt;
	flow->copy_applied = 1;
	return 0;
}

/**
 * Stop flow in RX_CP_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @flow
 *   Parent flow for wich copying is provided.
 */
static void
flow_mreg_stop_copy_action(struct rte_eth_dev *dev,
			   struct rte_flow *flow)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res = flow->mreg_copy;

	if (!mcp_res || !flow->copy_applied)
		return;
	MLX5_ASSERT(mcp_res->appcnt);
	--mcp_res->appcnt;
	flow->copy_applied = 0;
	if (!mcp_res->appcnt)
		flow_drv_remove(dev, mcp_res->flow);
}

/**
 * Remove the default copy action from RX_CP_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
flow_mreg_del_default_copy_action(struct rte_eth_dev *dev)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct mlx5_priv *priv = dev->data->dev_private;

	/* Check if default flow is registered. */
	if (!priv->mreg_cp_tbl)
		return;
	mcp_res = (void *)mlx5_hlist_lookup(priv->mreg_cp_tbl,
					    MLX5_DEFAULT_COPY_ID);
	if (!mcp_res)
		return;
	MLX5_ASSERT(mcp_res->flow);
	flow_list_destroy(dev, NULL, mcp_res->flow);
	mlx5_hlist_remove(priv->mreg_cp_tbl, &mcp_res->hlist_ent);
	rte_free(mcp_res);
}

/**
 * Add the default copy action in in RX_CP_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 for success, negative value otherwise and rte_errno is set.
 */
static int
flow_mreg_add_default_copy_action(struct rte_eth_dev *dev,
				  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_mreg_copy_resource *mcp_res;

	/* Check whether extensive metadata feature is engaged. */
	if (!priv->config.dv_flow_en ||
	    priv->config.dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !mlx5_flow_ext_mreg_supported(dev) ||
	    !priv->sh->dv_regc0_mask)
		return 0;
	mcp_res = flow_mreg_add_copy_action(dev, MLX5_DEFAULT_COPY_ID, error);
	if (!mcp_res)
		return -rte_errno;
	return 0;
}

/**
 * Add a flow of copying flow metadata registers in RX_CP_TBL.
 *
 * All the flow having Q/RSS action should be split by
 * flow_mreg_split_qrss_prep() to pass by RX_CP_TBL. A flow in the RX_CP_TBL
 * performs the following,
 *   - CQE->flow_tag := reg_c[1] (MARK)
 *   - CQE->flow_table_metadata (reg_b) := reg_c[0] (META)
 * As CQE's flow_tag is not a register, it can't be simply copied from reg_c[1]
 * but there should be a flow per each MARK ID set by MARK action.
 *
 * For the aforementioned reason, if there's a MARK action in flow's action
 * list, a corresponding flow should be added to the RX_CP_TBL in order to copy
 * the MARK ID to CQE's flow_tag like,
 *   - If reg_c[1] is mark_id,
 *     flow_tag := mark_id, reg_b := reg_c[0] and jump to RX_ACT_TBL.
 *
 * For SET_META action which stores value in reg_c[0], as the destination is
 * also a flow metadata register (reg_b), adding a default flow is enough. Zero
 * MARK ID means the default flow. The default flow looks like,
 *   - For all flow, reg_b := reg_c[0] and jump to RX_ACT_TBL.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to flow structure.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_mreg_update_copy_table(struct rte_eth_dev *dev,
			    struct rte_flow *flow,
			    const struct rte_flow_action *actions,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	const struct rte_flow_action_mark *mark;

	/* Check whether extensive metadata feature is engaged. */
	if (!config->dv_flow_en ||
	    config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !mlx5_flow_ext_mreg_supported(dev) ||
	    !priv->sh->dv_regc0_mask)
		return 0;
	/* Find MARK action. */
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_FLAG:
			mcp_res = flow_mreg_add_copy_action
				(dev, MLX5_FLOW_MARK_DEFAULT, error);
			if (!mcp_res)
				return -rte_errno;
			flow->mreg_copy = mcp_res;
			if (dev->data->dev_started) {
				mcp_res->appcnt++;
				flow->copy_applied = 1;
			}
			return 0;
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark = (const struct rte_flow_action_mark *)
				actions->conf;
			mcp_res =
				flow_mreg_add_copy_action(dev, mark->id, error);
			if (!mcp_res)
				return -rte_errno;
			flow->mreg_copy = mcp_res;
			if (dev->data->dev_started) {
				mcp_res->appcnt++;
				flow->copy_applied = 1;
			}
			return 0;
		default:
			break;
		}
	}
	return 0;
}

#define MLX5_MAX_SPLIT_ACTIONS 24
#define MLX5_MAX_SPLIT_ITEMS 24

/**
 * Split the hairpin flow.
 * Since HW can't support encap on Rx we move the encap to Tx.
 * If the count action is after the encap then we also
 * move the count action. in this case the count will also measure
 * the outer bytes.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] actions_rx
 *   Rx flow actions.
 * @param[out] actions_tx
 *   Tx flow actions..
 * @param[out] pattern_tx
 *   The pattern items for the Tx flow.
 * @param[out] flow_id
 *   The flow ID connected to this flow.
 *
 * @return
 *   0 on success.
 */
static int
flow_hairpin_split(struct rte_eth_dev *dev,
		   const struct rte_flow_action actions[],
		   struct rte_flow_action actions_rx[],
		   struct rte_flow_action actions_tx[],
		   struct rte_flow_item pattern_tx[],
		   uint32_t *flow_id)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_raw_encap *raw_encap;
	const struct rte_flow_action_raw_decap *raw_decap;
	struct mlx5_rte_flow_action_set_tag *set_tag;
	struct rte_flow_action *tag_action;
	struct mlx5_rte_flow_item_tag *tag_item;
	struct rte_flow_item *item;
	char *addr;
	int encap = 0;

	mlx5_flow_id_get(priv->sh->flow_id_pool, flow_id);
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			rte_memcpy(actions_tx, actions,
			       sizeof(struct rte_flow_action));
			actions_tx++;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (encap) {
				rte_memcpy(actions_tx, actions,
					   sizeof(struct rte_flow_action));
				actions_tx++;
			} else {
				rte_memcpy(actions_rx, actions,
					   sizeof(struct rte_flow_action));
				actions_rx++;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size >
			    (sizeof(struct rte_flow_item_eth) +
			     sizeof(struct rte_flow_item_ipv4))) {
				memcpy(actions_tx, actions,
				       sizeof(struct rte_flow_action));
				actions_tx++;
				encap = 1;
			} else {
				rte_memcpy(actions_rx, actions,
					   sizeof(struct rte_flow_action));
				actions_rx++;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			raw_decap = actions->conf;
			if (raw_decap->size <
			    (sizeof(struct rte_flow_item_eth) +
			     sizeof(struct rte_flow_item_ipv4))) {
				memcpy(actions_tx, actions,
				       sizeof(struct rte_flow_action));
				actions_tx++;
			} else {
				rte_memcpy(actions_rx, actions,
					   sizeof(struct rte_flow_action));
				actions_rx++;
			}
			break;
		default:
			rte_memcpy(actions_rx, actions,
				   sizeof(struct rte_flow_action));
			actions_rx++;
			break;
		}
	}
	/* Add set meta action and end action for the Rx flow. */
	tag_action = actions_rx;
	tag_action->type = MLX5_RTE_FLOW_ACTION_TYPE_TAG;
	actions_rx++;
	rte_memcpy(actions_rx, actions, sizeof(struct rte_flow_action));
	actions_rx++;
	set_tag = (void *)actions_rx;
	set_tag->id = mlx5_flow_get_reg_id(dev, MLX5_HAIRPIN_RX, 0, NULL);
	MLX5_ASSERT(set_tag->id > REG_NONE);
	set_tag->data = *flow_id;
	tag_action->conf = set_tag;
	/* Create Tx item list. */
	rte_memcpy(actions_tx, actions, sizeof(struct rte_flow_action));
	addr = (void *)&pattern_tx[2];
	item = pattern_tx;
	item->type = MLX5_RTE_FLOW_ITEM_TYPE_TAG;
	tag_item = (void *)addr;
	tag_item->data = *flow_id;
	tag_item->id = mlx5_flow_get_reg_id(dev, MLX5_HAIRPIN_TX, 0, NULL);
	MLX5_ASSERT(set_tag->id > REG_NONE);
	item->spec = tag_item;
	addr += sizeof(struct mlx5_rte_flow_item_tag);
	tag_item = (void *)addr;
	tag_item->data = UINT32_MAX;
	tag_item->id = UINT16_MAX;
	item->mask = tag_item;
	addr += sizeof(struct mlx5_rte_flow_item_tag);
	item->last = NULL;
	item++;
	item->type = RTE_FLOW_ITEM_TYPE_END;
	return 0;
}

/**
 * The last stage of splitting chain, just creates the subflow
 * without any modification.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param[in, out] sub_flow
 *   Pointer to return the created subflow, may be NULL.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_create_split_inner(struct rte_eth_dev *dev,
			struct rte_flow *flow,
			struct mlx5_flow **sub_flow,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item items[],
			const struct rte_flow_action actions[],
			bool external, struct rte_flow_error *error)
{
	struct mlx5_flow *dev_flow;

	dev_flow = flow_drv_prepare(flow, attr, items, actions, error);
	if (!dev_flow)
		return -rte_errno;
	dev_flow->flow = flow;
	dev_flow->external = external;
	/* Subflow object was created, we must include one in the list. */
	LIST_INSERT_HEAD(&flow->dev_flows, dev_flow, next);
	if (sub_flow)
		*sub_flow = dev_flow;
	return flow_drv_translate(dev, dev_flow, attr, items, actions, error);
}

/**
 * Split the meter flow.
 *
 * As meter flow will split to three sub flow, other than meter
 * action, the other actions make sense to only meter accepts
 * the packet. If it need to be dropped, no other additional
 * actions should be take.
 *
 * One kind of special action which decapsulates the L3 tunnel
 * header will be in the prefix sub flow, as not to take the
 * L3 tunnel header into account.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] actions_sfx
 *   Suffix flow actions.
 * @param[out] actions_pre
 *   Prefix flow actions.
 * @param[out] pattern_sfx
 *   The pattern items for the suffix flow.
 * @param[out] tag_sfx
 *   Pointer to suffix flow tag.
 *
 * @return
 *   0 on success.
 */
static int
flow_meter_split_prep(struct rte_eth_dev *dev,
		 const struct rte_flow_action actions[],
		 struct rte_flow_action actions_sfx[],
		 struct rte_flow_action actions_pre[])
{
	struct rte_flow_action *tag_action = NULL;
	struct mlx5_rte_flow_action_set_tag *set_tag;
	struct rte_flow_error error;
	const struct rte_flow_action_raw_encap *raw_encap;
	const struct rte_flow_action_raw_decap *raw_decap;
	uint32_t tag_id;

	/* Prepare the actions for prefix and suffix flow. */
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_METER:
			/* Add the extra tag action first. */
			tag_action = actions_pre;
			tag_action->type = MLX5_RTE_FLOW_ACTION_TYPE_TAG;
			actions_pre++;
			memcpy(actions_pre, actions,
			       sizeof(struct rte_flow_action));
			actions_pre++;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			memcpy(actions_pre, actions,
			       sizeof(struct rte_flow_action));
			actions_pre++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size >
			    (sizeof(struct rte_flow_item_eth) +
			     sizeof(struct rte_flow_item_ipv4))) {
				memcpy(actions_sfx, actions,
				       sizeof(struct rte_flow_action));
				actions_sfx++;
			} else {
				rte_memcpy(actions_pre, actions,
					   sizeof(struct rte_flow_action));
				actions_pre++;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			raw_decap = actions->conf;
			/* Size 0 decap means 50 bytes as vxlan decap. */
			if (raw_decap->size && (raw_decap->size <
			    (sizeof(struct rte_flow_item_eth) +
			     sizeof(struct rte_flow_item_ipv4)))) {
				memcpy(actions_sfx, actions,
				       sizeof(struct rte_flow_action));
				actions_sfx++;
			} else {
				rte_memcpy(actions_pre, actions,
					   sizeof(struct rte_flow_action));
				actions_pre++;
			}
			break;
		default:
			memcpy(actions_sfx, actions,
				sizeof(struct rte_flow_action));
			actions_sfx++;
			break;
		}
	}
	/* Add end action to the actions. */
	actions_sfx->type = RTE_FLOW_ACTION_TYPE_END;
	actions_pre->type = RTE_FLOW_ACTION_TYPE_END;
	actions_pre++;
	/* Set the tag. */
	set_tag = (void *)actions_pre;
	set_tag->id = mlx5_flow_get_reg_id(dev, MLX5_MTR_SFX, 0, &error);
	/*
	 * Get the id from the qrss_pool to make qrss share the id with meter.
	 */
	tag_id = flow_qrss_get_id(dev);
	set_tag->data = tag_id << MLX5_MTR_COLOR_BITS;
	assert(tag_action);
	tag_action->conf = set_tag;
	return tag_id;
}

/**
 * Split action list having QUEUE/RSS for metadata register copy.
 *
 * Once Q/RSS action is detected in user's action list, the flow action
 * should be split in order to copy metadata registers, which will happen in
 * RX_CP_TBL like,
 *   - CQE->flow_tag := reg_c[1] (MARK)
 *   - CQE->flow_table_metadata (reg_b) := reg_c[0] (META)
 * The Q/RSS action will be performed on RX_ACT_TBL after passing by RX_CP_TBL.
 * This is because the last action of each flow must be a terminal action
 * (QUEUE, RSS or DROP).
 *
 * Flow ID must be allocated to identify actions in the RX_ACT_TBL and it is
 * stored and kept in the mlx5_flow structure per each sub_flow.
 *
 * The Q/RSS action is replaced with,
 *   - SET_TAG, setting the allocated flow ID to reg_c[2].
 * And the following JUMP action is added at the end,
 *   - JUMP, to RX_CP_TBL.
 *
 * A flow to perform remained Q/RSS action will be created in RX_ACT_TBL by
 * flow_create_split_metadata() routine. The flow will look like,
 *   - If flow ID matches (reg_c[2]), perform Q/RSS.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] split_actions
 *   Pointer to store split actions to jump to CP_TBL.
 * @param[in] actions
 *   Pointer to the list of original flow actions.
 * @param[in] qrss
 *   Pointer to the Q/RSS action.
 * @param[in] actions_n
 *   Number of original actions.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   non-zero unique flow_id on success, otherwise 0 and
 *   error/rte_error are set.
 */
static uint32_t
flow_mreg_split_qrss_prep(struct rte_eth_dev *dev,
			  struct rte_flow_action *split_actions,
			  const struct rte_flow_action *actions,
			  const struct rte_flow_action *qrss,
			  int actions_n, struct rte_flow_error *error)
{
	struct mlx5_rte_flow_action_set_tag *set_tag;
	struct rte_flow_action_jump *jump;
	const int qrss_idx = qrss - actions;
	uint32_t flow_id = 0;
	int ret = 0;

	/*
	 * Given actions will be split
	 * - Replace QUEUE/RSS action with SET_TAG to set flow ID.
	 * - Add jump to mreg CP_TBL.
	 * As a result, there will be one more action.
	 */
	++actions_n;
	memcpy(split_actions, actions, sizeof(*split_actions) * actions_n);
	set_tag = (void *)(split_actions + actions_n);
	/*
	 * If tag action is not set to void(it means we are not the meter
	 * suffix flow), add the tag action. Since meter suffix flow already
	 * has the tag added.
	 */
	if (split_actions[qrss_idx].type != RTE_FLOW_ACTION_TYPE_VOID) {
		/*
		 * Allocate the new subflow ID. This one is unique within
		 * device and not shared with representors. Otherwise,
		 * we would have to resolve multi-thread access synch
		 * issue. Each flow on the shared device is appended
		 * with source vport identifier, so the resulting
		 * flows will be unique in the shared (by master and
		 * representors) domain even if they have coinciding
		 * IDs.
		 */
		flow_id = flow_qrss_get_id(dev);
		if (!flow_id)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "can't allocate id "
						  "for split Q/RSS subflow");
		/* Internal SET_TAG action to set flow ID. */
		*set_tag = (struct mlx5_rte_flow_action_set_tag){
			.data = flow_id,
		};
		ret = mlx5_flow_get_reg_id(dev, MLX5_COPY_MARK, 0, error);
		if (ret < 0)
			return ret;
		set_tag->id = ret;
		/* Construct new actions array. */
		/* Replace QUEUE/RSS action. */
		split_actions[qrss_idx] = (struct rte_flow_action){
			.type = MLX5_RTE_FLOW_ACTION_TYPE_TAG,
			.conf = set_tag,
		};
	}
	/* JUMP action to jump to mreg copy table (CP_TBL). */
	jump = (void *)(set_tag + 1);
	*jump = (struct rte_flow_action_jump){
		.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
	};
	split_actions[actions_n - 2] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_JUMP,
		.conf = jump,
	};
	split_actions[actions_n - 1] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	return flow_id;
}

/**
 * Extend the given action list for Tx metadata copy.
 *
 * Copy the given action list to the ext_actions and add flow metadata register
 * copy action in order to copy reg_a set by WQE to reg_c[0].
 *
 * @param[out] ext_actions
 *   Pointer to the extended action list.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[in] actions_n
 *   Number of actions in the list.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_mreg_tx_copy_prep(struct rte_eth_dev *dev,
		       struct rte_flow_action *ext_actions,
		       const struct rte_flow_action *actions,
		       int actions_n, struct rte_flow_error *error)
{
	struct mlx5_flow_action_copy_mreg *cp_mreg =
		(struct mlx5_flow_action_copy_mreg *)
			(ext_actions + actions_n + 1);
	int ret;

	ret = mlx5_flow_get_reg_id(dev, MLX5_METADATA_RX, 0, error);
	if (ret < 0)
		return ret;
	cp_mreg->dst = ret;
	ret = mlx5_flow_get_reg_id(dev, MLX5_METADATA_TX, 0, error);
	if (ret < 0)
		return ret;
	cp_mreg->src = ret;
	memcpy(ext_actions, actions,
			sizeof(*ext_actions) * actions_n);
	ext_actions[actions_n - 1] = (struct rte_flow_action){
		.type = MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
		.conf = cp_mreg,
	};
	ext_actions[actions_n] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	return 0;
}

/**
 * The splitting for metadata feature.
 *
 * - Q/RSS action on NIC Rx should be split in order to pass by
 *   the mreg copy table (RX_CP_TBL) and then it jumps to the
 *   action table (RX_ACT_TBL) which has the split Q/RSS action.
 *
 * - All the actions on NIC Tx should have a mreg copy action to
 *   copy reg_a from WQE to reg_c[0].
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_create_split_metadata(struct rte_eth_dev *dev,
			   struct rte_flow *flow,
			   const struct rte_flow_attr *attr,
			   const struct rte_flow_item items[],
			   const struct rte_flow_action actions[],
			   bool external, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	const struct rte_flow_action *qrss = NULL;
	struct rte_flow_action *ext_actions = NULL;
	struct mlx5_flow *dev_flow = NULL;
	uint32_t qrss_id = 0;
	int mtr_sfx = 0;
	size_t act_size;
	int actions_n;
	int ret;

	/* Check whether extensive metadata feature is engaged. */
	if (!config->dv_flow_en ||
	    config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !mlx5_flow_ext_mreg_supported(dev))
		return flow_create_split_inner(dev, flow, NULL, attr, items,
					       actions, external, error);
	actions_n = flow_parse_qrss_action(actions, &qrss);
	if (qrss) {
		/* Exclude hairpin flows from splitting. */
		if (qrss->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue;

			queue = qrss->conf;
			if (mlx5_rxq_get_type(dev, queue->index) ==
			    MLX5_RXQ_TYPE_HAIRPIN)
				qrss = NULL;
		} else if (qrss->type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss;

			rss = qrss->conf;
			if (mlx5_rxq_get_type(dev, rss->queue[0]) ==
			    MLX5_RXQ_TYPE_HAIRPIN)
				qrss = NULL;
		}
	}
	if (qrss) {
		/* Check if it is in meter suffix table. */
		mtr_sfx = attr->group == (attr->transfer ?
			  (MLX5_FLOW_TABLE_LEVEL_SUFFIX - 1) :
			  MLX5_FLOW_TABLE_LEVEL_SUFFIX);
		/*
		 * Q/RSS action on NIC Rx should be split in order to pass by
		 * the mreg copy table (RX_CP_TBL) and then it jumps to the
		 * action table (RX_ACT_TBL) which has the split Q/RSS action.
		 */
		act_size = sizeof(struct rte_flow_action) * (actions_n + 1) +
			   sizeof(struct rte_flow_action_set_tag) +
			   sizeof(struct rte_flow_action_jump);
		ext_actions = rte_zmalloc(__func__, act_size, 0);
		if (!ext_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "metadata flow");
		/*
		 * If we are the suffix flow of meter, tag already exist.
		 * Set the tag action to void.
		 */
		if (mtr_sfx)
			ext_actions[qrss - actions].type =
						RTE_FLOW_ACTION_TYPE_VOID;
		else
			ext_actions[qrss - actions].type =
						MLX5_RTE_FLOW_ACTION_TYPE_TAG;
		/*
		 * Create the new actions list with removed Q/RSS action
		 * and appended set tag and jump to register copy table
		 * (RX_CP_TBL). We should preallocate unique tag ID here
		 * in advance, because it is needed for set tag action.
		 */
		qrss_id = flow_mreg_split_qrss_prep(dev, ext_actions, actions,
						    qrss, actions_n, error);
		if (!mtr_sfx && !qrss_id) {
			ret = -rte_errno;
			goto exit;
		}
	} else if (attr->egress && !attr->transfer) {
		/*
		 * All the actions on NIC Tx should have a metadata register
		 * copy action to copy reg_a from WQE to reg_c[meta]
		 */
		act_size = sizeof(struct rte_flow_action) * (actions_n + 1) +
			   sizeof(struct mlx5_flow_action_copy_mreg);
		ext_actions = rte_zmalloc(__func__, act_size, 0);
		if (!ext_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "metadata flow");
		/* Create the action list appended with copy register. */
		ret = flow_mreg_tx_copy_prep(dev, ext_actions, actions,
					     actions_n, error);
		if (ret < 0)
			goto exit;
	}
	/* Add the unmodified original or prefix subflow. */
	ret = flow_create_split_inner(dev, flow, &dev_flow, attr, items,
				      ext_actions ? ext_actions : actions,
				      external, error);
	if (ret < 0)
		goto exit;
	MLX5_ASSERT(dev_flow);
	if (qrss) {
		const struct rte_flow_attr q_attr = {
			.group = MLX5_FLOW_MREG_ACT_TABLE_GROUP,
			.ingress = 1,
		};
		/* Internal PMD action to set register. */
		struct mlx5_rte_flow_item_tag q_tag_spec = {
			.data = qrss_id,
			.id = 0,
		};
		struct rte_flow_item q_items[] = {
			{
				.type = MLX5_RTE_FLOW_ITEM_TYPE_TAG,
				.spec = &q_tag_spec,
				.last = NULL,
				.mask = NULL,
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		};
		struct rte_flow_action q_actions[] = {
			{
				.type = qrss->type,
				.conf = qrss->conf,
			},
			{
				.type = RTE_FLOW_ACTION_TYPE_END,
			},
		};
		uint64_t hash_fields = dev_flow->hash_fields;

		/*
		 * Configure the tag item only if there is no meter subflow.
		 * Since tag is already marked in the meter suffix subflow
		 * we can just use the meter suffix items as is.
		 */
		if (qrss_id) {
			/* Not meter subflow. */
			MLX5_ASSERT(!mtr_sfx);
			/*
			 * Put unique id in prefix flow due to it is destroyed
			 * after suffix flow and id will be freed after there
			 * is no actual flows with this id and identifier
			 * reallocation becomes possible (for example, for
			 * other flows in other threads).
			 */
			dev_flow->qrss_id = qrss_id;
			qrss_id = 0;
			ret = mlx5_flow_get_reg_id(dev, MLX5_COPY_MARK, 0,
						   error);
			if (ret < 0)
				goto exit;
			q_tag_spec.id = ret;
		}
		dev_flow = NULL;
		/* Add suffix subflow to execute Q/RSS. */
		ret = flow_create_split_inner(dev, flow, &dev_flow,
					      &q_attr, mtr_sfx ? items :
					      q_items, q_actions,
					      external, error);
		if (ret < 0)
			goto exit;
		MLX5_ASSERT(dev_flow);
		dev_flow->hash_fields = hash_fields;
	}

exit:
	/*
	 * We do not destroy the partially created sub_flows in case of error.
	 * These ones are included into parent flow list and will be destroyed
	 * by flow_drv_destroy.
	 */
	flow_qrss_free_id(dev, qrss_id);
	rte_free(ext_actions);
	return ret;
}

/**
 * The splitting for meter feature.
 *
 * - The meter flow will be split to two flows as prefix and
 *   suffix flow. The packets make sense only it pass the prefix
 *   meter action.
 *
 * - Reg_C_5 is used for the packet to match betweend prefix and
 *   suffix flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_create_split_meter(struct rte_eth_dev *dev,
			   struct rte_flow *flow,
			   const struct rte_flow_attr *attr,
			   const struct rte_flow_item items[],
			   const struct rte_flow_action actions[],
			   bool external, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_action *sfx_actions = NULL;
	struct rte_flow_action *pre_actions = NULL;
	struct rte_flow_item *sfx_items = NULL;
	const  struct rte_flow_item *sfx_port_id_item;
	struct mlx5_flow *dev_flow = NULL;
	struct rte_flow_attr sfx_attr = *attr;
	uint32_t mtr = 0;
	uint32_t mtr_tag_id = 0;
	size_t act_size;
	size_t item_size;
	int actions_n = 0;
	int ret;

	if (priv->mtr_en)
		actions_n = flow_check_meter_action(actions, &mtr);
	if (mtr) {
		struct mlx5_rte_flow_item_tag *tag_spec;
		struct mlx5_rte_flow_item_tag *tag_mask;
		/* The five prefix actions: meter, decap, encap, tag, end. */
		act_size = sizeof(struct rte_flow_action) * (actions_n + 5) +
			   sizeof(struct rte_flow_action_set_tag);
		/* tag, end. */
#define METER_SUFFIX_ITEM 3
		item_size = sizeof(struct rte_flow_item) * METER_SUFFIX_ITEM +
			    sizeof(struct mlx5_rte_flow_item_tag) * 2;
		sfx_actions = rte_zmalloc(__func__, (act_size + item_size), 0);
		if (!sfx_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "meter flow");
		pre_actions = sfx_actions + actions_n;
		mtr_tag_id = flow_meter_split_prep(dev, actions, sfx_actions,
						     pre_actions);
		if (!mtr_tag_id) {
			ret = -rte_errno;
			goto exit;
		}
		/* Add the prefix subflow. */
		ret = flow_create_split_inner(dev, flow, &dev_flow, attr, items,
						  pre_actions, external, error);
		if (ret) {
			ret = -rte_errno;
			goto exit;
		}
		dev_flow->mtr_flow_id = mtr_tag_id;
		/* Prepare the suffix flow match pattern. */
		sfx_items = (struct rte_flow_item *)((char *)sfx_actions +
			     act_size);
		tag_spec = (struct mlx5_rte_flow_item_tag *)(sfx_items +
			    METER_SUFFIX_ITEM);
		tag_spec->data = dev_flow->mtr_flow_id << MLX5_MTR_COLOR_BITS;
		tag_spec->id = mlx5_flow_get_reg_id(dev, MLX5_MTR_SFX, 0,
						    error);
		tag_mask = tag_spec + 1;
		tag_mask->data = 0xffffff00;
		sfx_items->type = MLX5_RTE_FLOW_ITEM_TYPE_TAG;
		sfx_items->spec = tag_spec;
		sfx_items->last = NULL;
		sfx_items->mask = tag_mask;
		sfx_items++;
		sfx_port_id_item = find_port_id_item(items);
		if (sfx_port_id_item) {
			memcpy(sfx_items, sfx_port_id_item,
			       sizeof(*sfx_items));
			sfx_items++;
		}
		sfx_items->type = RTE_FLOW_ITEM_TYPE_END;
		sfx_items -= sfx_port_id_item ? 2 : 1;
		/* Setting the sfx group atrr. */
		sfx_attr.group = sfx_attr.transfer ?
				(MLX5_FLOW_TABLE_LEVEL_SUFFIX - 1) :
				 MLX5_FLOW_TABLE_LEVEL_SUFFIX;
	}
	/* Add the prefix subflow. */
	ret = flow_create_split_metadata(dev, flow, &sfx_attr,
					 sfx_items ? sfx_items : items,
					 sfx_actions ? sfx_actions : actions,
					 external, error);
exit:
	if (sfx_actions)
		rte_free(sfx_actions);
	return ret;
}

/**
 * Split the flow to subflow set. The splitters might be linked
 * in the chain, like this:
 * flow_create_split_outer() calls:
 *   flow_create_split_meter() calls:
 *     flow_create_split_metadata(meter_subflow_0) calls:
 *       flow_create_split_inner(metadata_subflow_0)
 *       flow_create_split_inner(metadata_subflow_1)
 *       flow_create_split_inner(metadata_subflow_2)
 *     flow_create_split_metadata(meter_subflow_1) calls:
 *       flow_create_split_inner(metadata_subflow_0)
 *       flow_create_split_inner(metadata_subflow_1)
 *       flow_create_split_inner(metadata_subflow_2)
 *
 * This provide flexible way to add new levels of flow splitting.
 * The all of successfully created subflows are included to the
 * parent flow dev_flow list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_create_split_outer(struct rte_eth_dev *dev,
			struct rte_flow *flow,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item items[],
			const struct rte_flow_action actions[],
			bool external, struct rte_flow_error *error)
{
	int ret;

	ret = flow_create_split_meter(dev, flow, attr, items,
					 actions, external, error);
	MLX5_ASSERT(ret <= 0);
	return ret;
}

/**
 * Create a flow and add it to @p list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list. If this parameter NULL,
 *   no list insertion occurred, flow is just created,
 *   this is caller's responsibility to track the
 *   created flow.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow *
flow_list_create(struct rte_eth_dev *dev, struct mlx5_flows *list,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 bool external, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = NULL;
	struct mlx5_flow *dev_flow;
	const struct rte_flow_action_rss *rss;
	union {
		struct rte_flow_expand_rss buf;
		uint8_t buffer[2048];
	} expand_buffer;
	union {
		struct rte_flow_action actions[MLX5_MAX_SPLIT_ACTIONS];
		uint8_t buffer[2048];
	} actions_rx;
	union {
		struct rte_flow_action actions[MLX5_MAX_SPLIT_ACTIONS];
		uint8_t buffer[2048];
	} actions_hairpin_tx;
	union {
		struct rte_flow_item items[MLX5_MAX_SPLIT_ITEMS];
		uint8_t buffer[2048];
	} items_tx;
	struct rte_flow_expand_rss *buf = &expand_buffer.buf;
	const struct rte_flow_action *p_actions_rx = actions;
	uint32_t i;
	uint32_t flow_size;
	int hairpin_flow = 0;
	uint32_t hairpin_id = 0;
	struct rte_flow_attr attr_tx = { .priority = 0 };
	int ret = flow_drv_validate(dev, attr, items, p_actions_rx, external,
				    error);

	if (ret < 0)
		return NULL;
	hairpin_flow = flow_check_hairpin_split(dev, attr, actions);
	if (hairpin_flow > 0) {
		if (hairpin_flow > MLX5_MAX_SPLIT_ACTIONS) {
			rte_errno = EINVAL;
			return NULL;
		}
		flow_hairpin_split(dev, actions, actions_rx.actions,
				   actions_hairpin_tx.actions, items_tx.items,
				   &hairpin_id);
		p_actions_rx = actions_rx.actions;
	}
	flow_size = sizeof(struct rte_flow);
	rss = flow_get_rss_action(p_actions_rx);
	if (rss)
		flow_size += RTE_ALIGN_CEIL(rss->queue_num * sizeof(uint16_t),
					    sizeof(void *));
	else
		flow_size += RTE_ALIGN_CEIL(sizeof(uint16_t), sizeof(void *));
	flow = rte_calloc(__func__, 1, flow_size, 0);
	if (!flow) {
		rte_errno = ENOMEM;
		goto error_before_flow;
	}
	flow->drv_type = flow_get_drv_type(dev, attr);
	if (hairpin_id != 0)
		flow->hairpin_flow_id = hairpin_id;
	MLX5_ASSERT(flow->drv_type > MLX5_FLOW_TYPE_MIN &&
		    flow->drv_type < MLX5_FLOW_TYPE_MAX);
	flow->rss.queue = (void *)(flow + 1);
	if (rss) {
		/*
		 * The following information is required by
		 * mlx5_flow_hashfields_adjust() in advance.
		 */
		flow->rss.level = rss->level;
		/* RSS type 0 indicates default RSS type (ETH_RSS_IP). */
		flow->rss.types = !rss->types ? ETH_RSS_IP : rss->types;
	}
	LIST_INIT(&flow->dev_flows);
	if (rss && rss->types) {
		unsigned int graph_root;

		graph_root = find_graph_root(items, rss->level);
		ret = rte_flow_expand_rss(buf, sizeof(expand_buffer.buffer),
					  items, rss->types,
					  mlx5_support_expansion,
					  graph_root);
		MLX5_ASSERT(ret > 0 &&
		       (unsigned int)ret < sizeof(expand_buffer.buffer));
	} else {
		buf->entries = 1;
		buf->entry[0].pattern = (void *)(uintptr_t)items;
	}
	for (i = 0; i < buf->entries; ++i) {
		/*
		 * The splitter may create multiple dev_flows,
		 * depending on configuration. In the simplest
		 * case it just creates unmodified original flow.
		 */
		ret = flow_create_split_outer(dev, flow, attr,
					      buf->entry[i].pattern,
					      p_actions_rx, external,
					      error);
		if (ret < 0)
			goto error;
	}
	/* Create the tx flow. */
	if (hairpin_flow) {
		attr_tx.group = MLX5_HAIRPIN_TX_TABLE;
		attr_tx.ingress = 0;
		attr_tx.egress = 1;
		dev_flow = flow_drv_prepare(flow, &attr_tx, items_tx.items,
					    actions_hairpin_tx.actions, error);
		if (!dev_flow)
			goto error;
		dev_flow->flow = flow;
		dev_flow->external = 0;
		LIST_INSERT_HEAD(&flow->dev_flows, dev_flow, next);
		ret = flow_drv_translate(dev, dev_flow, &attr_tx,
					 items_tx.items,
					 actions_hairpin_tx.actions, error);
		if (ret < 0)
			goto error;
	}
	/*
	 * Update the metadata register copy table. If extensive
	 * metadata feature is enabled and registers are supported
	 * we might create the extra rte_flow for each unique
	 * MARK/FLAG action ID.
	 *
	 * The table is updated for ingress Flows only, because
	 * the egress Flows belong to the different device and
	 * copy table should be updated in peer NIC Rx domain.
	 */
	if (attr->ingress &&
	    (external || attr->group != MLX5_FLOW_MREG_CP_TABLE_GROUP)) {
		ret = flow_mreg_update_copy_table(dev, flow, actions, error);
		if (ret)
			goto error;
	}
	if (dev->data->dev_started) {
		ret = flow_drv_apply(dev, flow, error);
		if (ret < 0)
			goto error;
	}
	if (list)
		TAILQ_INSERT_TAIL(list, flow, next);
	flow_rxq_flags_set(dev, flow);
	return flow;
error_before_flow:
	if (hairpin_id)
		mlx5_flow_id_release(priv->sh->flow_id_pool,
				     hairpin_id);
	return NULL;
error:
	MLX5_ASSERT(flow);
	flow_mreg_del_copy_action(dev, flow);
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (flow->hairpin_flow_id)
		mlx5_flow_id_release(priv->sh->flow_id_pool,
				     flow->hairpin_flow_id);
	MLX5_ASSERT(flow);
	flow_drv_destroy(dev, flow);
	rte_free(flow);
	rte_errno = ret; /* Restore rte_errno. */
	return NULL;
}

/**
 * Create a dedicated flow rule on e-switch table 0 (root table), to direct all
 * incoming packets to table 1.
 *
 * Other flow rules, requested for group n, will be created in
 * e-switch table n+1.
 * Jump action to e-switch group n will be created to group n+1.
 *
 * Used when working in switchdev mode, to utilise advantages of table 1
 * and above.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow on success, NULL otherwise and rte_errno is set.
 */
struct rte_flow *
mlx5_flow_create_esw_table_zero_flow(struct rte_eth_dev *dev)
{
	const struct rte_flow_attr attr = {
		.group = 0,
		.priority = 0,
		.ingress = 1,
		.egress = 0,
		.transfer = 1,
	};
	const struct rte_flow_item pattern = {
		.type = RTE_FLOW_ITEM_TYPE_END,
	};
	struct rte_flow_action_jump jump = {
		.group = 1,
	};
	const struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_error error;

	return flow_list_create(dev, &priv->ctrl_flows, &attr, &pattern,
				actions, false, &error);
}

/**
 * Create a flow.
 *
 * @see rte_flow_create()
 * @see rte_flow_ops
 */
struct rte_flow *
mlx5_flow_create(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	return flow_list_create(dev, &priv->flows,
				attr, items, actions, true, error);
}

/**
 * Destroy a flow in a list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list. If this parameter NULL,
 *   there is no flow removal from the list.
 * @param[in] flow
 *   Flow to destroy.
 */
static void
flow_list_destroy(struct rte_eth_dev *dev, struct mlx5_flows *list,
		  struct rte_flow *flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	/*
	 * Update RX queue flags only if port is started, otherwise it is
	 * already clean.
	 */
	if (dev->data->dev_started)
		flow_rxq_flags_trim(dev, flow);
	if (flow->hairpin_flow_id)
		mlx5_flow_id_release(priv->sh->flow_id_pool,
				     flow->hairpin_flow_id);
	flow_drv_destroy(dev, flow);
	if (list)
		TAILQ_REMOVE(list, flow, next);
	flow_mreg_del_copy_action(dev, flow);
	rte_free(flow->fdir);
	rte_free(flow);
}

/**
 * Destroy all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 */
void
mlx5_flow_list_flush(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	while (!TAILQ_EMPTY(list)) {
		struct rte_flow *flow;

		flow = TAILQ_FIRST(list);
		flow_list_destroy(dev, list, flow);
	}
}

/**
 * Remove all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 */
void
mlx5_flow_stop(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	struct rte_flow *flow;

	TAILQ_FOREACH_REVERSE(flow, list, mlx5_flows, next) {
		flow_drv_remove(dev, flow);
		flow_mreg_stop_copy_action(dev, flow);
	}
	flow_mreg_del_default_copy_action(dev);
	flow_rxq_flags_clear(dev);
}

/**
 * Add all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_start(struct rte_eth_dev *dev, struct mlx5_flows *list)
{
	struct rte_flow *flow;
	struct rte_flow_error error;
	int ret = 0;

	/* Make sure default copy action (reg_c[0] -> reg_b) is created. */
	ret = flow_mreg_add_default_copy_action(dev, &error);
	if (ret < 0)
		return -rte_errno;
	/* Apply Flows created by application. */
	TAILQ_FOREACH(flow, list, next) {
		ret = flow_mreg_start_copy_action(dev, flow);
		if (ret < 0)
			goto error;
		ret = flow_drv_apply(dev, flow, &error);
		if (ret < 0)
			goto error;
		flow_rxq_flags_set(dev, flow);
	}
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_flow_stop(dev, list);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Verify the flow list is empty
 *
 * @param dev
 *  Pointer to Ethernet device.
 *
 * @return the number of flows not released.
 */
int
mlx5_flow_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	int ret = 0;

	TAILQ_FOREACH(flow, &priv->flows, next) {
		DRV_LOG(DEBUG, "port %u flow %p still referenced",
			dev->data->port_id, (void *)flow);
		++ret;
	}
	return ret;
}

/**
 * Enable default hairpin egress flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param queue
 *   The queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow_source_queue(struct rte_eth_dev *dev,
			    uint32_t queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_attr attr = {
		.egress = 1,
		.priority = 0,
	};
	struct mlx5_rte_flow_item_tx_queue queue_spec = {
		.queue = queue,
	};
	struct mlx5_rte_flow_item_tx_queue queue_mask = {
		.queue = UINT32_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE,
			.spec = &queue_spec,
			.last = NULL,
			.mask = &queue_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action_jump jump = {
		.group = MLX5_HAIRPIN_TX_TABLE,
	};
	struct rte_flow_action actions[2];
	struct rte_flow *flow;
	struct rte_flow_error error;

	actions[0].type = RTE_FLOW_ACTION_TYPE_JUMP;
	actions[0].conf = &jump;
	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	flow = flow_list_create(dev, &priv->ctrl_flows,
				&attr, items, actions, false, &error);
	if (!flow) {
		DRV_LOG(DEBUG,
			"Failed to create ctrl flow: rte_errno(%d),"
			" type(%d), message(%s)",
			rte_errno, error.type,
			error.message ? error.message : " (no stated reason)");
		return -rte_errno;
	}
	return 0;
}

/**
 * Enable a control flow configured from the control plane.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param eth_spec
 *   An Ethernet flow spec to apply.
 * @param eth_mask
 *   An Ethernet flow mask to apply.
 * @param vlan_spec
 *   A VLAN flow spec to apply.
 * @param vlan_mask
 *   A VLAN flow mask to apply.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow_vlan(struct rte_eth_dev *dev,
		    struct rte_flow_item_eth *eth_spec,
		    struct rte_flow_item_eth *eth_mask,
		    struct rte_flow_item_vlan *vlan_spec,
		    struct rte_flow_item_vlan *vlan_mask)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_attr attr = {
		.ingress = 1,
		.priority = MLX5_FLOW_PRIO_RSVD,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = eth_spec,
			.last = NULL,
			.mask = eth_mask,
		},
		{
			.type = (vlan_spec) ? RTE_FLOW_ITEM_TYPE_VLAN :
					      RTE_FLOW_ITEM_TYPE_END,
			.spec = vlan_spec,
			.last = NULL,
			.mask = vlan_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	uint16_t queue[priv->reta_idx_n];
	struct rte_flow_action_rss action_rss = {
		.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
		.level = 0,
		.types = priv->rss_conf.rss_hf,
		.key_len = priv->rss_conf.rss_key_len,
		.queue_num = priv->reta_idx_n,
		.key = priv->rss_conf.rss_key,
		.queue = queue,
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = &action_rss,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow *flow;
	struct rte_flow_error error;
	unsigned int i;

	if (!priv->reta_idx_n || !priv->rxqs_n) {
		return 0;
	}
	for (i = 0; i != priv->reta_idx_n; ++i)
		queue[i] = (*priv->reta_idx)[i];
	flow = flow_list_create(dev, &priv->ctrl_flows,
				&attr, items, actions, false, &error);
	if (!flow)
		return -rte_errno;
	return 0;
}

/**
 * Enable a flow control configured from the control plane.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param eth_spec
 *   An Ethernet flow spec to apply.
 * @param eth_mask
 *   An Ethernet flow mask to apply.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow(struct rte_eth_dev *dev,
	       struct rte_flow_item_eth *eth_spec,
	       struct rte_flow_item_eth *eth_mask)
{
	return mlx5_ctrl_flow_vlan(dev, eth_spec, eth_mask, NULL, NULL);
}

/**
 * Destroy a flow.
 *
 * @see rte_flow_destroy()
 * @see rte_flow_ops
 */
int
mlx5_flow_destroy(struct rte_eth_dev *dev,
		  struct rte_flow *flow,
		  struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	flow_list_destroy(dev, &priv->flows, flow);
	return 0;
}

/**
 * Destroy all flows.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
int
mlx5_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->flows);
	return 0;
}

/**
 * Isolated mode.
 *
 * @see rte_flow_isolate()
 * @see rte_flow_ops
 */
int
mlx5_flow_isolate(struct rte_eth_dev *dev,
		  int enable,
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (dev->data->dev_started) {
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "port must be stopped first");
		return -rte_errno;
	}
	priv->isolated = !!enable;
	if (enable)
		dev->dev_ops = &mlx5_dev_ops_isolate;
	else
		dev->dev_ops = &mlx5_dev_ops;
	return 0;
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
static int
flow_drv_query(struct rte_eth_dev *dev,
	       struct rte_flow *flow,
	       const struct rte_flow_action *actions,
	       void *data,
	       struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type ftype = flow->drv_type;

	MLX5_ASSERT(ftype > MLX5_FLOW_TYPE_MIN && ftype < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(ftype);

	return fops->query(dev, flow, actions, data, error);
}

/**
 * Query a flow.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
int
mlx5_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	int ret;

	ret = flow_drv_query(dev, flow, actions, data, error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Convert a flow director filter to a generic flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Flow director filter to add.
 * @param attributes
 *   Generic flow parameters structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_fdir_filter_convert(struct rte_eth_dev *dev,
			 const struct rte_eth_fdir_filter *fdir_filter,
			 struct mlx5_fdir *attributes)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_eth_fdir_input *input = &fdir_filter->input;
	const struct rte_eth_fdir_masks *mask =
		&dev->data->dev_conf.fdir_conf.mask;

	/* Validate queue number. */
	if (fdir_filter->action.rx_queue >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u invalid queue number %d",
			dev->data->port_id, fdir_filter->action.rx_queue);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	attributes->attr.ingress = 1;
	attributes->items[0] = (struct rte_flow_item) {
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &attributes->l2,
		.mask = &attributes->l2_mask,
	};
	switch (fdir_filter->action.behavior) {
	case RTE_ETH_FDIR_ACCEPT:
		attributes->actions[0] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_QUEUE,
			.conf = &attributes->queue,
		};
		break;
	case RTE_ETH_FDIR_REJECT:
		attributes->actions[0] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_DROP,
		};
		break;
	default:
		DRV_LOG(ERR, "port %u invalid behavior %d",
			dev->data->port_id,
			fdir_filter->action.behavior);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	attributes->queue.index = fdir_filter->action.rx_queue;
	/* Handle L3. */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
		attributes->l3.ipv4.hdr = (struct rte_ipv4_hdr){
			.src_addr = input->flow.ip4_flow.src_ip,
			.dst_addr = input->flow.ip4_flow.dst_ip,
			.time_to_live = input->flow.ip4_flow.ttl,
			.type_of_service = input->flow.ip4_flow.tos,
		};
		attributes->l3_mask.ipv4.hdr = (struct rte_ipv4_hdr){
			.src_addr = mask->ipv4_mask.src_ip,
			.dst_addr = mask->ipv4_mask.dst_ip,
			.time_to_live = mask->ipv4_mask.ttl,
			.type_of_service = mask->ipv4_mask.tos,
			.next_proto_id = mask->ipv4_mask.proto,
		};
		attributes->items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV4,
			.spec = &attributes->l3,
			.mask = &attributes->l3_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		attributes->l3.ipv6.hdr = (struct rte_ipv6_hdr){
			.hop_limits = input->flow.ipv6_flow.hop_limits,
			.proto = input->flow.ipv6_flow.proto,
		};

		memcpy(attributes->l3.ipv6.hdr.src_addr,
		       input->flow.ipv6_flow.src_ip,
		       RTE_DIM(attributes->l3.ipv6.hdr.src_addr));
		memcpy(attributes->l3.ipv6.hdr.dst_addr,
		       input->flow.ipv6_flow.dst_ip,
		       RTE_DIM(attributes->l3.ipv6.hdr.src_addr));
		memcpy(attributes->l3_mask.ipv6.hdr.src_addr,
		       mask->ipv6_mask.src_ip,
		       RTE_DIM(attributes->l3_mask.ipv6.hdr.src_addr));
		memcpy(attributes->l3_mask.ipv6.hdr.dst_addr,
		       mask->ipv6_mask.dst_ip,
		       RTE_DIM(attributes->l3_mask.ipv6.hdr.src_addr));
		attributes->items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_IPV6,
			.spec = &attributes->l3,
			.mask = &attributes->l3_mask,
		};
		break;
	default:
		DRV_LOG(ERR, "port %u invalid flow type%d",
			dev->data->port_id, fdir_filter->input.flow_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	/* Handle L4. */
	switch (fdir_filter->input.flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		attributes->l4.udp.hdr = (struct rte_udp_hdr){
			.src_port = input->flow.udp4_flow.src_port,
			.dst_port = input->flow.udp4_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct rte_udp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		attributes->l4.tcp.hdr = (struct rte_tcp_hdr){
			.src_port = input->flow.tcp4_flow.src_port,
			.dst_port = input->flow.tcp4_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct rte_tcp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		attributes->l4.udp.hdr = (struct rte_udp_hdr){
			.src_port = input->flow.udp6_flow.src_port,
			.dst_port = input->flow.udp6_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct rte_udp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_UDP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		attributes->l4.tcp.hdr = (struct rte_tcp_hdr){
			.src_port = input->flow.tcp6_flow.src_port,
			.dst_port = input->flow.tcp6_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct rte_tcp_hdr){
			.src_port = mask->src_port_mask,
			.dst_port = mask->dst_port_mask,
		};
		attributes->items[2] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_TCP,
			.spec = &attributes->l4,
			.mask = &attributes->l4_mask,
		};
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
		break;
	default:
		DRV_LOG(ERR, "port %u invalid flow type%d",
			dev->data->port_id, fdir_filter->input.flow_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}

#define FLOW_FDIR_CMP(f1, f2, fld) \
	memcmp(&(f1)->fld, &(f2)->fld, sizeof(f1->fld))

/**
 * Compare two FDIR flows. If items and actions are identical, the two flows are
 * regarded as same.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param f1
 *   FDIR flow to compare.
 * @param f2
 *   FDIR flow to compare.
 *
 * @return
 *   Zero on match, 1 otherwise.
 */
static int
flow_fdir_cmp(const struct mlx5_fdir *f1, const struct mlx5_fdir *f2)
{
	if (FLOW_FDIR_CMP(f1, f2, attr) ||
	    FLOW_FDIR_CMP(f1, f2, l2) ||
	    FLOW_FDIR_CMP(f1, f2, l2_mask) ||
	    FLOW_FDIR_CMP(f1, f2, l3) ||
	    FLOW_FDIR_CMP(f1, f2, l3_mask) ||
	    FLOW_FDIR_CMP(f1, f2, l4) ||
	    FLOW_FDIR_CMP(f1, f2, l4_mask) ||
	    FLOW_FDIR_CMP(f1, f2, actions[0].type))
		return 1;
	if (f1->actions[0].type == RTE_FLOW_ACTION_TYPE_QUEUE &&
	    FLOW_FDIR_CMP(f1, f2, queue))
		return 1;
	return 0;
}

/**
 * Search device flow list to find out a matched FDIR flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_flow
 *   FDIR flow to lookup.
 *
 * @return
 *   Pointer of flow if found, NULL otherwise.
 */
static struct rte_flow *
flow_fdir_filter_lookup(struct rte_eth_dev *dev, struct mlx5_fdir *fdir_flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = NULL;

	MLX5_ASSERT(fdir_flow);
	TAILQ_FOREACH(flow, &priv->flows, next) {
		if (flow->fdir && !flow_fdir_cmp(flow->fdir, fdir_flow)) {
			DRV_LOG(DEBUG, "port %u found FDIR flow %p",
				dev->data->port_id, (void *)flow);
			break;
		}
	}
	return flow;
}

/**
 * Add new flow director filter and store it in list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Flow director filter to add.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_fdir_filter_add(struct rte_eth_dev *dev,
		     const struct rte_eth_fdir_filter *fdir_filter)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_fdir *fdir_flow;
	struct rte_flow *flow;
	int ret;

	fdir_flow = rte_zmalloc(__func__, sizeof(*fdir_flow), 0);
	if (!fdir_flow) {
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	ret = flow_fdir_filter_convert(dev, fdir_filter, fdir_flow);
	if (ret)
		goto error;
	flow = flow_fdir_filter_lookup(dev, fdir_flow);
	if (flow) {
		rte_errno = EEXIST;
		goto error;
	}
	flow = flow_list_create(dev, &priv->flows, &fdir_flow->attr,
				fdir_flow->items, fdir_flow->actions, true,
				NULL);
	if (!flow)
		goto error;
	MLX5_ASSERT(!flow->fdir);
	flow->fdir = fdir_flow;
	DRV_LOG(DEBUG, "port %u created FDIR flow %p",
		dev->data->port_id, (void *)flow);
	return 0;
error:
	rte_free(fdir_flow);
	return -rte_errno;
}

/**
 * Delete specific filter.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Filter to be deleted.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_fdir_filter_delete(struct rte_eth_dev *dev,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	struct mlx5_fdir fdir_flow = {
		.attr.group = 0,
	};
	int ret;

	ret = flow_fdir_filter_convert(dev, fdir_filter, &fdir_flow);
	if (ret)
		return -rte_errno;
	flow = flow_fdir_filter_lookup(dev, &fdir_flow);
	if (!flow) {
		rte_errno = ENOENT;
		return -rte_errno;
	}
	flow_list_destroy(dev, &priv->flows, flow);
	DRV_LOG(DEBUG, "port %u deleted FDIR flow %p",
		dev->data->port_id, (void *)flow);
	return 0;
}

/**
 * Update queue for specific filter.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fdir_filter
 *   Filter to be updated.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_fdir_filter_update(struct rte_eth_dev *dev,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	int ret;

	ret = flow_fdir_filter_delete(dev, fdir_filter);
	if (ret)
		return ret;
	return flow_fdir_filter_add(dev, fdir_filter);
}

/**
 * Flush all filters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
flow_fdir_filter_flush(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_flow_list_flush(dev, &priv->flows);
}

/**
 * Get flow director information.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] fdir_info
 *   Resulting flow director information.
 */
static void
flow_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir_info)
{
	struct rte_eth_fdir_masks *mask =
		&dev->data->dev_conf.fdir_conf.mask;

	fdir_info->mode = dev->data->dev_conf.fdir_conf.mode;
	fdir_info->guarant_spc = 0;
	rte_memcpy(&fdir_info->mask, mask, sizeof(fdir_info->mask));
	fdir_info->max_flexpayload = 0;
	fdir_info->flow_types_mask[0] = 0;
	fdir_info->flex_payload_unit = 0;
	fdir_info->max_flex_payload_segment_num = 0;
	fdir_info->flex_payload_limit = 0;
	memset(&fdir_info->flex_conf, 0, sizeof(fdir_info->flex_conf));
}

/**
 * Deal with flow director operations.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_fdir_ctrl_func(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
		    void *arg)
{
	enum rte_fdir_mode fdir_mode =
		dev->data->dev_conf.fdir_conf.mode;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;
	if (fdir_mode != RTE_FDIR_MODE_PERFECT &&
	    fdir_mode != RTE_FDIR_MODE_PERFECT_MAC_VLAN) {
		DRV_LOG(ERR, "port %u flow director mode %d not supported",
			dev->data->port_id, fdir_mode);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		return flow_fdir_filter_add(dev, arg);
	case RTE_ETH_FILTER_UPDATE:
		return flow_fdir_filter_update(dev, arg);
	case RTE_ETH_FILTER_DELETE:
		return flow_fdir_filter_delete(dev, arg);
	case RTE_ETH_FILTER_FLUSH:
		flow_fdir_filter_flush(dev);
		break;
	case RTE_ETH_FILTER_INFO:
		flow_fdir_info_get(dev, arg);
		break;
	default:
		DRV_LOG(DEBUG, "port %u unknown operation %u",
			dev->data->port_id, filter_op);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
}

/**
 * Manage filter operations.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param filter_type
 *   Filter type.
 * @param filter_op
 *   Operation to perform.
 * @param arg
 *   Pointer to operation-specific structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_dev_filter_ctrl(struct rte_eth_dev *dev,
		     enum rte_filter_type filter_type,
		     enum rte_filter_op filter_op,
		     void *arg)
{
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET) {
			rte_errno = EINVAL;
			return -rte_errno;
		}
		*(const void **)arg = &mlx5_flow_ops;
		return 0;
	case RTE_ETH_FILTER_FDIR:
		return flow_fdir_ctrl_func(dev, filter_op, arg);
	default:
		DRV_LOG(ERR, "port %u filter type (%d) not supported",
			dev->data->port_id, filter_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}

/**
 * Create the needed meter and suffix tables.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] fm
 *   Pointer to the flow meter.
 *
 * @return
 *   Pointer to table set on success, NULL otherwise.
 */
struct mlx5_meter_domains_infos *
mlx5_flow_create_mtr_tbls(struct rte_eth_dev *dev,
			  const struct mlx5_flow_meter *fm)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_mtr_tbls(dev, fm);
}

/**
 * Destroy the meter table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] tbl
 *   Pointer to the meter table set.
 *
 * @return
 *   0 on success.
 */
int
mlx5_flow_destroy_mtr_tbls(struct rte_eth_dev *dev,
			   struct mlx5_meter_domains_infos *tbls)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->destroy_mtr_tbls(dev, tbls);
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
int
mlx5_flow_create_policer_rules(struct rte_eth_dev *dev,
			       struct mlx5_flow_meter *fm,
			       const struct rte_flow_attr *attr)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_policer_rules(dev, fm, attr);
}

/**
 * Destroy policer rules.
 *
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[in] attr
 *   Pointer to flow attributes.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
int
mlx5_flow_destroy_policer_rules(struct rte_eth_dev *dev,
				struct mlx5_flow_meter *fm,
				const struct rte_flow_attr *attr)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->destroy_policer_rules(dev, fm, attr);
}

/**
 * Allocate a counter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Pointer to allocated counter  on success, NULL otherwise.
 */
struct mlx5_flow_counter *
mlx5_counter_alloc(struct rte_eth_dev *dev)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = { .transfer = 0 };

	if (flow_get_drv_type(dev, &attr) == MLX5_FLOW_TYPE_DV) {
		fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
		return fops->counter_alloc(dev);
	}
	DRV_LOG(ERR,
		"port %u counter allocate is not supported.",
		 dev->data->port_id);
	return NULL;
}

/**
 * Free a counter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] cnt
 *   Pointer to counter to be free.
 */
void
mlx5_counter_free(struct rte_eth_dev *dev, struct mlx5_flow_counter *cnt)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = { .transfer = 0 };

	if (flow_get_drv_type(dev, &attr) == MLX5_FLOW_TYPE_DV) {
		fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
		fops->counter_free(dev, cnt);
		return;
	}
	DRV_LOG(ERR,
		"port %u counter free is not supported.",
		 dev->data->port_id);
}

/**
 * Query counter statistics.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] cnt
 *   Pointer to counter to query.
 * @param[in] clear
 *   Set to clear counter statistics.
 * @param[out] pkts
 *   The counter hits packets number to save.
 * @param[out] bytes
 *   The counter hits bytes number to save.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
int
mlx5_counter_query(struct rte_eth_dev *dev, struct mlx5_flow_counter *cnt,
		   bool clear, uint64_t *pkts, uint64_t *bytes)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = { .transfer = 0 };

	if (flow_get_drv_type(dev, &attr) == MLX5_FLOW_TYPE_DV) {
		fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
		return fops->counter_query(dev, cnt, clear, pkts, bytes);
	}
	DRV_LOG(ERR,
		"port %u counter query is not supported.",
		 dev->data->port_id);
	return -ENOTSUP;
}

#define MLX5_POOL_QUERY_FREQ_US 1000000

/**
 * Set the periodic procedure for triggering asynchronous batch queries for all
 * the counter pools.
 *
 * @param[in] sh
 *   Pointer to mlx5_ibv_shared object.
 */
void
mlx5_set_query_alarm(struct mlx5_ibv_shared *sh)
{
	struct mlx5_pools_container *cont = MLX5_CNT_CONTAINER(sh, 0, 0);
	uint32_t pools_n = rte_atomic16_read(&cont->n_valid);
	uint32_t us;

	cont = MLX5_CNT_CONTAINER(sh, 1, 0);
	pools_n += rte_atomic16_read(&cont->n_valid);
	us = MLX5_POOL_QUERY_FREQ_US / pools_n;
	DRV_LOG(DEBUG, "Set alarm for %u pools each %u us", pools_n, us);
	if (rte_eal_alarm_set(us, mlx5_flow_query_alarm, sh)) {
		sh->cmng.query_thread_on = 0;
		DRV_LOG(ERR, "Cannot reinitialize query alarm");
	} else {
		sh->cmng.query_thread_on = 1;
	}
}

/**
 * The periodic procedure for triggering asynchronous batch queries for all the
 * counter pools. This function is probably called by the host thread.
 *
 * @param[in] arg
 *   The parameter for the alarm process.
 */
void
mlx5_flow_query_alarm(void *arg)
{
	struct mlx5_ibv_shared *sh = arg;
	struct mlx5_devx_obj *dcs;
	uint16_t offset;
	int ret;
	uint8_t batch = sh->cmng.batch;
	uint16_t pool_index = sh->cmng.pool_index;
	struct mlx5_pools_container *cont;
	struct mlx5_pools_container *mcont;
	struct mlx5_flow_counter_pool *pool;

	if (sh->cmng.pending_queries >= MLX5_MAX_PENDING_QUERIES)
		goto set_alarm;
next_container:
	cont = MLX5_CNT_CONTAINER(sh, batch, 1);
	mcont = MLX5_CNT_CONTAINER(sh, batch, 0);
	/* Check if resize was done and need to flip a container. */
	if (cont != mcont) {
		if (cont->pools) {
			/* Clean the old container. */
			rte_free(cont->pools);
			memset(cont, 0, sizeof(*cont));
		}
		rte_cio_wmb();
		 /* Flip the host container. */
		sh->cmng.mhi[batch] ^= (uint8_t)2;
		cont = mcont;
	}
	if (!cont->pools) {
		/* 2 empty containers case is unexpected. */
		if (unlikely(batch != sh->cmng.batch))
			goto set_alarm;
		batch ^= 0x1;
		pool_index = 0;
		goto next_container;
	}
	pool = cont->pools[pool_index];
	if (pool->raw_hw)
		/* There is a pool query in progress. */
		goto set_alarm;
	pool->raw_hw =
		LIST_FIRST(&sh->cmng.free_stat_raws);
	if (!pool->raw_hw)
		/* No free counter statistics raw memory. */
		goto set_alarm;
	dcs = (struct mlx5_devx_obj *)(uintptr_t)rte_atomic64_read
							      (&pool->a64_dcs);
	offset = batch ? 0 : dcs->id % MLX5_COUNTERS_PER_POOL;
	ret = mlx5_devx_cmd_flow_counter_query(dcs, 0, MLX5_COUNTERS_PER_POOL -
					       offset, NULL, NULL,
					       pool->raw_hw->mem_mng->dm->id,
					       (void *)(uintptr_t)
					       (pool->raw_hw->data + offset),
					       sh->devx_comp,
					       (uint64_t)(uintptr_t)pool);
	if (ret) {
		DRV_LOG(ERR, "Failed to trigger asynchronous query for dcs ID"
			" %d", pool->min_dcs->id);
		pool->raw_hw = NULL;
		goto set_alarm;
	}
	pool->raw_hw->min_dcs_id = dcs->id;
	LIST_REMOVE(pool->raw_hw, next);
	sh->cmng.pending_queries++;
	pool_index++;
	if (pool_index >= rte_atomic16_read(&cont->n_valid)) {
		batch ^= 0x1;
		pool_index = 0;
	}
set_alarm:
	sh->cmng.batch = batch;
	sh->cmng.pool_index = pool_index;
	mlx5_set_query_alarm(sh);
}

/**
 * Handler for the HW respond about ready values from an asynchronous batch
 * query. This function is probably called by the host thread.
 *
 * @param[in] sh
 *   The pointer to the shared IB device context.
 * @param[in] async_id
 *   The Devx async ID.
 * @param[in] status
 *   The status of the completion.
 */
void
mlx5_flow_async_pool_query_handle(struct mlx5_ibv_shared *sh,
				  uint64_t async_id, int status)
{
	struct mlx5_flow_counter_pool *pool =
		(struct mlx5_flow_counter_pool *)(uintptr_t)async_id;
	struct mlx5_counter_stats_raw *raw_to_free;

	if (unlikely(status)) {
		raw_to_free = pool->raw_hw;
	} else {
		raw_to_free = pool->raw;
		rte_spinlock_lock(&pool->sl);
		pool->raw = pool->raw_hw;
		rte_spinlock_unlock(&pool->sl);
		rte_atomic64_add(&pool->query_gen, 1);
		/* Be sure the new raw counters data is updated in memory. */
		rte_cio_wmb();
	}
	LIST_INSERT_HEAD(&sh->cmng.free_stat_raws, raw_to_free, next);
	pool->raw_hw = NULL;
	sh->cmng.pending_queries--;
}

/**
 * Translate the rte_flow group index to HW table value.
 *
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[in] external
 *   Value is part of flow rule created by request external to PMD.
 * @param[in] group
 *   rte_flow group index value.
 * @param[out] fdb_def_rule
 *   Whether fdb jump to table 1 is configured.
 * @param[out] table
 *   HW table value.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_group_to_table(const struct rte_flow_attr *attributes, bool external,
			 uint32_t group, bool fdb_def_rule, uint32_t *table,
			 struct rte_flow_error *error)
{
	if (attributes->transfer && external && fdb_def_rule) {
		if (group == UINT32_MAX)
			return rte_flow_error_set
						(error, EINVAL,
						 RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
						 NULL,
						 "group index not supported");
		*table = group + 1;
	} else {
		*table = group;
	}
	return 0;
}

/**
 * Discover availability of metadata reg_c's.
 *
 * Iteratively use test flows to check availability.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_discover_mreg_c(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_config *config = &priv->config;
	enum modify_reg idx;
	int n = 0;

	/* reg_c[0] and reg_c[1] are reserved. */
	config->flow_mreg_c[n++] = REG_C_0;
	config->flow_mreg_c[n++] = REG_C_1;
	/* Discover availability of other reg_c's. */
	for (idx = REG_C_2; idx <= REG_C_7; ++idx) {
		struct rte_flow_attr attr = {
			.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
			.priority = MLX5_FLOW_PRIO_RSVD,
			.ingress = 1,
		};
		struct rte_flow_item items[] = {
			[0] = {
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		};
		struct rte_flow_action actions[] = {
			[0] = {
				.type = MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
				.conf = &(struct mlx5_flow_action_copy_mreg){
					.src = REG_C_1,
					.dst = idx,
				},
			},
			[1] = {
				.type = RTE_FLOW_ACTION_TYPE_JUMP,
				.conf = &(struct rte_flow_action_jump){
					.group = MLX5_FLOW_MREG_ACT_TABLE_GROUP,
				},
			},
			[2] = {
				.type = RTE_FLOW_ACTION_TYPE_END,
			},
		};
		struct rte_flow *flow;
		struct rte_flow_error error;

		if (!config->dv_flow_en)
			break;
		/* Create internal flow, validation skips copy action. */
		flow = flow_list_create(dev, NULL, &attr, items,
					actions, false, &error);
		if (!flow)
			continue;
		if (dev->data->dev_started || !flow_drv_apply(dev, flow, NULL))
			config->flow_mreg_c[n++] = idx;
		flow_list_destroy(dev, NULL, flow);
	}
	for (; n < MLX5_MREG_C_NUM; ++n)
		config->flow_mreg_c[n] = REG_NONE;
	return 0;
}

/**
 * Dump flow raw hw data to file
 *
 * @param[in] dev
 *    The pointer to Ethernet device.
 * @param[in] file
 *   A pointer to a file for output.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   0 on success, a nagative value otherwise.
 */
int
mlx5_flow_dev_dump(struct rte_eth_dev *dev,
		   FILE *file,
		   struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ibv_shared *sh = priv->sh;

	return mlx5_devx_cmd_flow_dump(sh->fdb_domain, sh->rx_domain,
				       sh->tx_domain, file);
}
