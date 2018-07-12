/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <sys/queue.h>
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

/* Dev ops structure defined in mlx5.c */
extern const struct eth_dev_ops mlx5_dev_ops;
extern const struct eth_dev_ops mlx5_dev_ops_isolate;

/* Pattern Layer bits. */
#define MLX5_FLOW_LAYER_OUTER_L2 (1u << 0)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV4 (1u << 1)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV6 (1u << 2)
#define MLX5_FLOW_LAYER_OUTER_L4_UDP (1u << 3)
#define MLX5_FLOW_LAYER_OUTER_L4_TCP (1u << 4)
#define MLX5_FLOW_LAYER_OUTER_VLAN (1u << 5)
/* Masks. */
#define MLX5_FLOW_LAYER_OUTER_L3 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV4 | MLX5_FLOW_LAYER_OUTER_L3_IPV6)
#define MLX5_FLOW_LAYER_OUTER_L4 \
	(MLX5_FLOW_LAYER_OUTER_L4_UDP | MLX5_FLOW_LAYER_OUTER_L4_TCP)

/* Actions that modify the fate of matching traffic. */
#define MLX5_FLOW_FATE_DROP (1u << 0)
#define MLX5_FLOW_FATE_QUEUE (1u << 1)
#define MLX5_FLOW_FATE_RSS (1u << 2)

/* Modify a packet. */
#define MLX5_FLOW_MOD_FLAG (1u << 0)
#define MLX5_FLOW_MOD_MARK (1u << 1)

/* possible L3 layers protocols filtering. */
#define MLX5_IP_PROTOCOL_TCP 6
#define MLX5_IP_PROTOCOL_UDP 17

/* Priority reserved for default flows. */
#define MLX5_FLOW_PRIO_RSVD ((uint32_t)-1)

enum mlx5_expansion {
	MLX5_EXPANSION_ROOT,
	MLX5_EXPANSION_ETH,
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
	[MLX5_EXPANSION_ETH] = {
		.next = RTE_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
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

/** Handles information leading to a drop fate. */
struct mlx5_flow_verbs {
	LIST_ENTRY(mlx5_flow_verbs) next;
	unsigned int size; /**< Size of the attribute. */
	struct {
		struct ibv_flow_attr *attr;
		/**< Pointer to the Specification buffer. */
		uint8_t *specs; /**< Pointer to the specifications. */
	};
	struct ibv_flow *flow; /**< Verbs flow pointer. */
	struct mlx5_hrxq *hrxq; /**< Hash Rx queue object. */
	uint64_t hash_fields; /**< Verbs hash Rx queue hash fields. */
};

/* Flow structure. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	struct rte_flow_attr attributes; /**< User flow attribute. */
	uint32_t l3_protocol_en:1; /**< Protocol filtering requested. */
	uint32_t layers;
	/**< Bit-fields of present layers see MLX5_FLOW_LAYER_*. */
	uint32_t modifier;
	/**< Bit-fields of present modifier see MLX5_FLOW_MOD_*. */
	uint32_t fate;
	/**< Bit-fields of present fate see MLX5_FLOW_FATE_*. */
	uint8_t l3_protocol; /**< valid when l3_protocol_en is set. */
	LIST_HEAD(verbs, mlx5_flow_verbs) verbs; /**< Verbs flows list. */
	struct mlx5_flow_verbs *cur_verbs;
	/**< Current Verbs flow structure being filled. */
	struct rte_flow_action_rss rss;/**< RSS context. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	uint16_t (*queue)[]; /**< Destination queues to redirect traffic to. */
};

static const struct rte_flow_ops mlx5_flow_ops = {
	.validate = mlx5_flow_validate,
	.create = mlx5_flow_create,
	.destroy = mlx5_flow_destroy,
	.flush = mlx5_flow_flush,
	.isolate = mlx5_flow_isolate,
};

/* Convert FDIR request to Generic flow. */
struct mlx5_fdir {
	struct rte_flow_attr attr;
	struct rte_flow_action actions[2];
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
	struct rte_flow_action_queue queue;
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
};

/*
 * Number of sub priorities.
 * For each kind of pattern matching i.e. L2, L3, L4 to have a correct
 * matching on the NIC (firmware dependent) L4 most have the higher priority
 * followed by L3 and ending with L2.
 */
#define MLX5_PRIORITY_MAP_L2 2
#define MLX5_PRIORITY_MAP_L3 1
#define MLX5_PRIORITY_MAP_L4 0
#define MLX5_PRIORITY_MAP_MAX 3

/* Map of Verbs to Flow priority with 8 Verbs priorities. */
static const uint32_t priority_map_3[][MLX5_PRIORITY_MAP_MAX] = {
	{ 0, 1, 2 }, { 2, 3, 4 }, { 5, 6, 7 },
};

/* Map of Verbs to Flow priority with 16 Verbs priorities. */
static const uint32_t priority_map_5[][MLX5_PRIORITY_MAP_MAX] = {
	{ 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },
	{ 9, 10, 11 }, { 12, 13, 14 },
};

/**
 * Discover the maximum number of priority available.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   number of supported flow priority on success, a negative errno
 *   value otherwise and rte_errno is set.
 */
int
mlx5_flow_discover_priorities(struct rte_eth_dev *dev)
{
	struct {
		struct ibv_flow_attr attr;
		struct ibv_flow_spec_eth eth;
		struct ibv_flow_spec_action_drop drop;
	} flow_attr = {
		.attr = {
			.num_of_specs = 2,
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
			dev->data->port_id, vprio[i]);
		return -rte_errno;
	}
	mlx5_hrxq_drop_release(dev);
	DRV_LOG(INFO, "port %u flow maximum priority: %d",
		dev->data->port_id, priority);
	return priority;
}

/**
 * Adjust flow priority.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to an rte flow.
 */
static void
mlx5_flow_adjust_priority(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	uint32_t priority = flow->attributes.priority;
	uint32_t subpriority = flow->cur_verbs->attr->priority;

	switch (priv->config.flow_prio) {
	case RTE_DIM(priority_map_3):
		priority = priority_map_3[priority][subpriority];
		break;
	case RTE_DIM(priority_map_5):
		priority = priority_map_5[priority][subpriority];
		break;
	}
	flow->cur_verbs->attr->priority = priority;
}

/**
 * Verify the @p attributes will be correctly understood by the NIC and store
 * them in the @p flow if everything is correct.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] attributes
 *   Pointer to flow attributes
 * @param[in, out] flow
 *   Pointer to the rte_flow structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_attributes(struct rte_eth_dev *dev,
		     const struct rte_flow_attr *attributes,
		     struct rte_flow *flow,
		     struct rte_flow_error *error)
{
	uint32_t priority_max =
		((struct priv *)dev->data->dev_private)->config.flow_prio - 1;

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
	if (attributes->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS,
					  NULL,
					  "egress is not supported");
	if (attributes->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL,
					  "transfer is not supported");
	if (!attributes->ingress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_INGRESS,
					  NULL,
					  "ingress attribute is mandatory");
	flow->attributes = *attributes;
	if (attributes->priority == MLX5_FLOW_PRIO_RSVD)
		flow->attributes.priority = priority_max;
	return 0;
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
static int
mlx5_flow_item_acceptable(const struct rte_flow_item *item,
			  const uint8_t *mask,
			  const uint8_t *nic_mask,
			  unsigned int size,
			  struct rte_flow_error *error)
{
	unsigned int i;

	assert(nic_mask);
	for (i = 0; i < size; ++i)
		if ((nic_mask[i] | mask[i]) != nic_mask[i])
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "mask enables non supported"
						  " bits");
	if (!item->spec && (item->mask || item->last))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
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
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "range is not supported");
	}
	return 0;
}

/**
 * Add a verbs item specification into @p flow.
 *
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] src
 *   Create specification.
 * @param[in] size
 *   Size in bytes of the specification to copy.
 */
static void
mlx5_flow_spec_verbs_add(struct rte_flow *flow, void *src, unsigned int size)
{
	struct mlx5_flow_verbs *verbs = flow->cur_verbs;

	if (verbs->specs) {
		void *dst;

		dst = (void *)(verbs->specs + verbs->size);
		memcpy(dst, src, size);
		++verbs->attr->num_of_specs;
	}
	verbs->size += size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_eth(const struct rte_flow_item *item, struct rte_flow *flow,
		   const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *spec = item->spec;
	const struct rte_flow_item_eth *mask = item->mask;
	const struct rte_flow_item_eth nic_mask = {
		.dst.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.src.addr_bytes = "\xff\xff\xff\xff\xff\xff",
		.type = RTE_BE16(0xffff),
	};
	const unsigned int size = sizeof(struct ibv_flow_spec_eth);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH,
		.size = size,
	};
	int ret;

	if (flow->layers & MLX5_FLOW_LAYER_OUTER_L2)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L2 layers already configured");
	if (!mask)
		mask = &rte_flow_item_eth_mask;
	ret = mlx5_flow_item_acceptable(item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_eth),
					error);
	if (ret)
		return ret;
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L2;
	if (size > flow_size)
		return size;
	if (spec) {
		unsigned int i;

		memcpy(&eth.val.dst_mac, spec->dst.addr_bytes, ETHER_ADDR_LEN);
		memcpy(&eth.val.src_mac, spec->src.addr_bytes, ETHER_ADDR_LEN);
		eth.val.ether_type = spec->type;
		memcpy(&eth.mask.dst_mac, mask->dst.addr_bytes, ETHER_ADDR_LEN);
		memcpy(&eth.mask.src_mac, mask->src.addr_bytes, ETHER_ADDR_LEN);
		eth.mask.ether_type = mask->type;
		/* Remove unwanted bits from values. */
		for (i = 0; i < ETHER_ADDR_LEN; ++i) {
			eth.val.dst_mac[i] &= eth.mask.dst_mac[i];
			eth.val.src_mac[i] &= eth.mask.src_mac[i];
		}
		eth.val.ether_type &= eth.mask.ether_type;
	}
	flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	mlx5_flow_spec_verbs_add(flow, &eth, size);
	return size;
}

/**
 * Update the VLAN tag in the Verbs Ethernet specification.
 *
 * @param[in, out] attr
 *   Pointer to Verbs attributes structure.
 * @param[in] eth
 *   Verbs structure containing the VLAN information to copy.
 */
static void
mlx5_flow_item_vlan_update(struct ibv_flow_attr *attr,
			   struct ibv_flow_spec_eth *eth)
{
	unsigned int i;
	enum ibv_flow_spec_type search = IBV_FLOW_SPEC_ETH;
	struct ibv_spec_header *hdr = (struct ibv_spec_header *)
		((uint8_t *)attr + sizeof(struct ibv_flow_attr));

	for (i = 0; i != attr->num_of_specs; ++i) {
		if (hdr->type == search) {
			struct ibv_flow_spec_eth *e =
				(struct ibv_flow_spec_eth *)hdr;

			e->val.vlan_tag = eth->val.vlan_tag;
			e->mask.vlan_tag = eth->mask.vlan_tag;
			e->val.ether_type = eth->val.ether_type;
			e->mask.ether_type = eth->mask.ether_type;
			break;
		}
		hdr = (struct ibv_spec_header *)((uint8_t *)hdr + hdr->size);
	}
}

/**
 * Convert the @p item into @p flow (or by updating the already present
 * Ethernet Verbs) specification after ensuring the NIC will understand and
 * process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_vlan(const struct rte_flow_item *item, struct rte_flow *flow,
		    const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_vlan *spec = item->spec;
	const struct rte_flow_item_vlan *mask = item->mask;
	const struct rte_flow_item_vlan nic_mask = {
		.tci = RTE_BE16(0x0fff),
		.inner_type = RTE_BE16(0xffff),
	};
	unsigned int size = sizeof(struct ibv_flow_spec_eth);
	struct mlx5_flow_verbs *verbs = flow->cur_verbs;
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH,
		.size = size,
	};
	int ret;
	const uint32_t l34m = MLX5_FLOW_LAYER_OUTER_L3 |
			MLX5_FLOW_LAYER_OUTER_L4;
	const uint32_t vlanm = MLX5_FLOW_LAYER_OUTER_VLAN;
	const uint32_t l2m = MLX5_FLOW_LAYER_OUTER_L2;

	if (flow->layers & vlanm)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VLAN layer already configured");
	else if ((flow->layers & l34m) != 0)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L2 layer cannot follow L3/L4 layer");
	if (!mask)
		mask = &rte_flow_item_vlan_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_vlan), error);
	if (ret)
		return ret;
	if (spec) {
		eth.val.vlan_tag = spec->tci;
		eth.mask.vlan_tag = mask->tci;
		eth.val.vlan_tag &= eth.mask.vlan_tag;
		eth.val.ether_type = spec->inner_type;
		eth.mask.ether_type = mask->inner_type;
		eth.val.ether_type &= eth.mask.ether_type;
	}
	/*
	 * From verbs perspective an empty VLAN is equivalent
	 * to a packet without VLAN layer.
	 */
	if (!eth.mask.vlan_tag)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  item->spec,
					  "VLAN cannot be empty");
	if (!(flow->layers & l2m)) {
		if (size <= flow_size) {
			flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
			mlx5_flow_spec_verbs_add(flow, &eth, size);
		}
	} else {
		if (verbs->attr)
			mlx5_flow_item_vlan_update(verbs->attr, &eth);
		size = 0; /* Only an update is done in eth specification. */
	}
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_OUTER_VLAN;
	return size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_ipv4(const struct rte_flow_item *item, struct rte_flow *flow,
		    const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv4 *spec = item->spec;
	const struct rte_flow_item_ipv4 *mask = item->mask;
	const struct rte_flow_item_ipv4 nic_mask = {
		.hdr = {
			.src_addr = RTE_BE32(0xffffffff),
			.dst_addr = RTE_BE32(0xffffffff),
			.type_of_service = 0xff,
			.next_proto_id = 0xff,
		},
	};
	unsigned int size = sizeof(struct ibv_flow_spec_ipv4_ext);
	struct ibv_flow_spec_ipv4_ext ipv4 = {
		.type = IBV_FLOW_SPEC_IPV4_EXT,
		.size = size,
	};
	int ret;

	if (flow->layers & MLX5_FLOW_LAYER_OUTER_L3)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "multiple L3 layers not supported");
	else if (flow->layers & MLX5_FLOW_LAYER_OUTER_L4)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 cannot follow an L4 layer.");
	if (!mask)
		mask = &rte_flow_item_ipv4_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_ipv4), error);
	if (ret < 0)
		return ret;
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
	if (spec) {
		ipv4.val = (struct ibv_flow_ipv4_ext_filter){
			.src_ip = spec->hdr.src_addr,
			.dst_ip = spec->hdr.dst_addr,
			.proto = spec->hdr.next_proto_id,
			.tos = spec->hdr.type_of_service,
		};
		ipv4.mask = (struct ibv_flow_ipv4_ext_filter){
			.src_ip = mask->hdr.src_addr,
			.dst_ip = mask->hdr.dst_addr,
			.proto = mask->hdr.next_proto_id,
			.tos = mask->hdr.type_of_service,
		};
		/* Remove unwanted bits from values. */
		ipv4.val.src_ip &= ipv4.mask.src_ip;
		ipv4.val.dst_ip &= ipv4.mask.dst_ip;
		ipv4.val.proto &= ipv4.mask.proto;
		ipv4.val.tos &= ipv4.mask.tos;
	}
	flow->l3_protocol_en = !!ipv4.mask.proto;
	flow->l3_protocol = ipv4.val.proto;
	if (size <= flow_size) {
		uint64_t hash_fields = IBV_RX_HASH_SRC_IPV4 |
			IBV_RX_HASH_DST_IPV4;

		if (!(flow->rss.types &
		      (ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 |
		       ETH_RSS_NONFRAG_IPV4_OTHER)))
			hash_fields = 0;
		flow->cur_verbs->hash_fields |= hash_fields;
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L3;
		mlx5_flow_spec_verbs_add(flow, &ipv4, size);
	}
	return size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_ipv6(const struct rte_flow_item *item, struct rte_flow *flow,
		    const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_ipv6 *spec = item->spec;
	const struct rte_flow_item_ipv6 *mask = item->mask;
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
	unsigned int size = sizeof(struct ibv_flow_spec_ipv6);
	struct ibv_flow_spec_ipv6 ipv6 = {
		.type = IBV_FLOW_SPEC_IPV6,
		.size = size,
	};
	int ret;

	if (flow->layers & MLX5_FLOW_LAYER_OUTER_L3)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "multiple L3 layers not supported");
	else if (flow->layers & MLX5_FLOW_LAYER_OUTER_L4)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 cannot follow an L4 layer.");
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_ipv6), error);
	if (ret < 0)
		return ret;
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
	if (spec) {
		unsigned int i;
		uint32_t vtc_flow_val;
		uint32_t vtc_flow_mask;

		memcpy(&ipv6.val.src_ip, spec->hdr.src_addr,
		       RTE_DIM(ipv6.val.src_ip));
		memcpy(&ipv6.val.dst_ip, spec->hdr.dst_addr,
		       RTE_DIM(ipv6.val.dst_ip));
		memcpy(&ipv6.mask.src_ip, mask->hdr.src_addr,
		       RTE_DIM(ipv6.mask.src_ip));
		memcpy(&ipv6.mask.dst_ip, mask->hdr.dst_addr,
		       RTE_DIM(ipv6.mask.dst_ip));
		vtc_flow_val = rte_be_to_cpu_32(spec->hdr.vtc_flow);
		vtc_flow_mask = rte_be_to_cpu_32(mask->hdr.vtc_flow);
		ipv6.val.flow_label =
			rte_cpu_to_be_32((vtc_flow_val & IPV6_HDR_FL_MASK) >>
					 IPV6_HDR_FL_SHIFT);
		ipv6.val.traffic_class = (vtc_flow_val & IPV6_HDR_TC_MASK) >>
					 IPV6_HDR_TC_SHIFT;
		ipv6.val.next_hdr = spec->hdr.proto;
		ipv6.val.hop_limit = spec->hdr.hop_limits;
		ipv6.mask.flow_label =
			rte_cpu_to_be_32((vtc_flow_mask & IPV6_HDR_FL_MASK) >>
					 IPV6_HDR_FL_SHIFT);
		ipv6.mask.traffic_class = (vtc_flow_mask & IPV6_HDR_TC_MASK) >>
					  IPV6_HDR_TC_SHIFT;
		ipv6.mask.next_hdr = mask->hdr.proto;
		ipv6.mask.hop_limit = mask->hdr.hop_limits;
		/* Remove unwanted bits from values. */
		for (i = 0; i < RTE_DIM(ipv6.val.src_ip); ++i) {
			ipv6.val.src_ip[i] &= ipv6.mask.src_ip[i];
			ipv6.val.dst_ip[i] &= ipv6.mask.dst_ip[i];
		}
		ipv6.val.flow_label &= ipv6.mask.flow_label;
		ipv6.val.traffic_class &= ipv6.mask.traffic_class;
		ipv6.val.next_hdr &= ipv6.mask.next_hdr;
		ipv6.val.hop_limit &= ipv6.mask.hop_limit;
	}
	flow->l3_protocol_en = !!ipv6.mask.next_hdr;
	flow->l3_protocol = ipv6.val.next_hdr;
	if (size <= flow_size) {
		uint64_t hash_fields = IBV_RX_HASH_SRC_IPV6 |
			IBV_RX_HASH_DST_IPV6;

		if (!(flow->rss.types &
		      (ETH_RSS_IPV6 | ETH_RSS_NONFRAG_IPV6_OTHER)))
			hash_fields = 0;
		flow->cur_verbs->hash_fields |= hash_fields;
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L3;
		mlx5_flow_spec_verbs_add(flow, &ipv6, size);
	}
	return size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_udp(const struct rte_flow_item *item, struct rte_flow *flow,
		   const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_udp *spec = item->spec;
	const struct rte_flow_item_udp *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp udp = {
		.type = IBV_FLOW_SPEC_UDP,
		.size = size,
	};
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_UDP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with UDP layer");
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 is mandatory to filter"
					  " on L4");
	if (flow->layers & MLX5_FLOW_LAYER_OUTER_L4)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L4 layer is already"
					  " present");
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_udp_mask,
		 sizeof(struct rte_flow_item_udp), error);
	if (ret < 0)
		return ret;
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
	if (spec) {
		udp.val.dst_port = spec->hdr.dst_port;
		udp.val.src_port = spec->hdr.src_port;
		udp.mask.dst_port = mask->hdr.dst_port;
		udp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		udp.val.src_port &= udp.mask.src_port;
		udp.val.dst_port &= udp.mask.dst_port;
	}
	if (size <= flow_size) {
		uint64_t hash_fields = IBV_RX_HASH_SRC_PORT_UDP |
			IBV_RX_HASH_DST_PORT_UDP;

		if (!(flow->rss.types & ETH_RSS_UDP))
			hash_fields = 0;
		flow->cur_verbs->hash_fields |= hash_fields;
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L4;
		mlx5_flow_spec_verbs_add(flow, &udp, size);
	}
	return size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] item
 *   Item specification.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p item has fully been converted,
 *   otherwise another call with this returned memory size should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_item_tcp(const struct rte_flow_item *item, struct rte_flow *flow,
		   const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_tcp *spec = item->spec;
	const struct rte_flow_item_tcp *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp tcp = {
		.type = IBV_FLOW_SPEC_TCP,
		.size = size,
	};
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_TCP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with TCP layer");
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 is mandatory to filter on L4");
	if (flow->layers & MLX5_FLOW_LAYER_OUTER_L4)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L4 layer is already present");
	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_tcp_mask,
		 sizeof(struct rte_flow_item_tcp), error);
	if (ret < 0)
		return ret;
	flow->layers |= MLX5_FLOW_LAYER_OUTER_L4_TCP;
	if (spec) {
		tcp.val.dst_port = spec->hdr.dst_port;
		tcp.val.src_port = spec->hdr.src_port;
		tcp.mask.dst_port = mask->hdr.dst_port;
		tcp.mask.src_port = mask->hdr.src_port;
		/* Remove unwanted bits from values. */
		tcp.val.src_port &= tcp.mask.src_port;
		tcp.val.dst_port &= tcp.mask.dst_port;
	}
	if (size <= flow_size) {
		uint64_t hash_fields = IBV_RX_HASH_SRC_PORT_TCP |
			IBV_RX_HASH_DST_PORT_TCP;

		if (!(flow->rss.types & ETH_RSS_TCP))
			hash_fields = 0;
		flow->cur_verbs->hash_fields |= hash_fields;
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L4;
		mlx5_flow_spec_verbs_add(flow, &tcp, size);
	}
	return size;
}

/**
 * Convert the @p pattern into a Verbs specifications after ensuring the NIC
 * will understand and process it correctly.
 * The conversion is performed item per item, each of them is written into
 * the @p flow if its size is lesser or equal to @p flow_size.
 * Validation and memory consumption computation are still performed until the
 * end of @p pattern, unless an error is encountered.
 *
 * @param[in] pattern
 *   Flow pattern.
 * @param[in, out] flow
 *   Pointer to the rte_flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small some
 *   garbage may be present.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @pattern  has fully been
 *   converted, otherwise another call with this returned memory size should
 *   be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_items(const struct rte_flow_item pattern[],
		struct rte_flow *flow, const size_t flow_size,
		struct rte_flow_error *error)
{
	int remain = flow_size;
	size_t size = 0;

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		int ret = 0;

		switch (pattern->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_item_eth(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_item_vlan(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5_flow_item_ipv4(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5_flow_item_ipv6(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_item_udp(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_item_tcp(pattern, flow, remain, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  pattern,
						  "item not supported");
		}
		if (ret < 0)
			return ret;
		if (remain > ret)
			remain -= ret;
		else
			remain = 0;
		size += ret;
	}
	if (!flow->layers) {
		const struct rte_flow_item item = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
		};

		return mlx5_flow_item_eth(&item, flow, flow_size, error);
	}
	return size;
}

/**
 * Convert the @p action into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p action has fully been
 *   converted, otherwise another call with this returned memory size should
 *   be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_action_drop(const struct rte_flow_action *action,
		      struct rte_flow *flow, const size_t flow_size,
		      struct rte_flow_error *error)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_drop);
	struct ibv_flow_spec_action_drop drop = {
			.type = IBV_FLOW_SPEC_ACTION_DROP,
			.size = size,
	};

	if (flow->fate)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "multiple fate actions are not"
					  " supported");
	if (flow->modifier & (MLX5_FLOW_MOD_FLAG | MLX5_FLOW_MOD_MARK))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "drop is not compatible with"
					  " flag/mark action");
	if (size < flow_size)
		mlx5_flow_spec_verbs_add(flow, &drop, size);
	flow->fate |= MLX5_FLOW_FATE_DROP;
	return size;
}

/**
 * Convert the @p action into @p flow after ensuring the NIC will understand
 * and process it correctly.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] action
 *   Action configuration.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_action_queue(struct rte_eth_dev *dev,
		       const struct rte_flow_action *action,
		       struct rte_flow *flow,
		       struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_flow_action_queue *queue = action->conf;

	if (flow->fate)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "multiple fate actions are not"
					  " supported");
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
	if (flow->queue)
		(*flow->queue)[0] = queue->index;
	flow->rss.queue_num = 1;
	flow->fate |= MLX5_FLOW_FATE_QUEUE;
	return 0;
}

/**
 * Ensure the @p action will be understood and used correctly by the  NIC.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param action[in]
 *   Pointer to flow actions array.
 * @param flow[in, out]
 *   Pointer to the rte_flow structure.
 * @param error[in, out]
 *   Pointer to error structure.
 *
 * @return
 *   On success @p flow->queue array and @p flow->rss are filled and valid.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_action_rss(struct rte_eth_dev *dev,
		     const struct rte_flow_action *action,
		     struct rte_flow *flow,
		     struct rte_flow_error *error)
{
	struct priv *priv = dev->data->dev_private;
	const struct rte_flow_action_rss *rss = action->conf;
	unsigned int i;

	if (flow->fate)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "multiple fate actions are not"
					  " supported");
	if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
	    rss->func != RTE_ETH_HASH_FUNCTION_TOEPLITZ)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->func,
					  "RSS hash function not supported");
	if (rss->level > 1)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->level,
					  "tunnel RSS is not supported");
	if (rss->key_len < MLX5_RSS_HASH_KEY_LEN)
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
	for (i = 0; i != rss->queue_num; ++i) {
		if (!(*priv->rxqs)[rss->queue[i]])
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 &rss->queue[i],
				 "queue is not configured");
	}
	if (flow->queue)
		memcpy((*flow->queue), rss->queue,
		       rss->queue_num * sizeof(uint16_t));
	flow->rss.queue_num = rss->queue_num;
	memcpy(flow->key, rss->key, MLX5_RSS_HASH_KEY_LEN);
	flow->rss.types = rss->types;
	flow->fate |= MLX5_FLOW_FATE_RSS;
	return 0;
}

/**
 * Convert the @p action into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p action has fully been
 *   converted, otherwise another call with this returned memory size should
 *   be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_action_flag(const struct rte_flow_action *action,
		      struct rte_flow *flow, const size_t flow_size,
		      struct rte_flow_error *error)
{
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
		.tag_id = mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT),
	};
	struct mlx5_flow_verbs *verbs = flow->cur_verbs;

	if (flow->modifier & MLX5_FLOW_MOD_FLAG)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "flag action already present");
	if (flow->fate & MLX5_FLOW_FATE_DROP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "flag is not compatible with drop"
					  " action");
	if (flow->modifier & MLX5_FLOW_MOD_MARK)
		size = 0;
	else if (size <= flow_size && verbs)
		mlx5_flow_spec_verbs_add(flow, &tag, size);
	flow->modifier |= MLX5_FLOW_MOD_FLAG;
	return size;
}

/**
 * Update verbs specification to modify the flag to mark.
 *
 * @param[in, out] verbs
 *   Pointer to the mlx5_flow_verbs structure.
 * @param[in] mark_id
 *   Mark identifier to replace the flag.
 */
static void
mlx5_flow_verbs_mark_update(struct mlx5_flow_verbs *verbs, uint32_t mark_id)
{
	struct ibv_spec_header *hdr;
	int i;

	if (!verbs)
		return;
	/* Update Verbs specification. */
	hdr = (struct ibv_spec_header *)verbs->specs;
	if (!hdr)
		return;
	for (i = 0; i != verbs->attr->num_of_specs; ++i) {
		if (hdr->type == IBV_FLOW_SPEC_ACTION_TAG) {
			struct ibv_flow_spec_action_tag *t =
				(struct ibv_flow_spec_action_tag *)hdr;

			t->tag_id = mlx5_flow_mark_set(mark_id);
		}
		hdr = (struct ibv_spec_header *)((uintptr_t)hdr + hdr->size);
	}
}

/**
 * Convert the @p action into @p flow (or by updating the already present
 * Flag Verbs specification) after ensuring the NIC will understand and
 * process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param[in] action
 *   Action configuration.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p action has fully been
 *   converted, otherwise another call with this returned memory size should
 *   be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_action_mark(const struct rte_flow_action *action,
		      struct rte_flow *flow, const size_t flow_size,
		      struct rte_flow_error *error)
{
	const struct rte_flow_action_mark *mark = action->conf;
	unsigned int size = sizeof(struct ibv_flow_spec_action_tag);
	struct ibv_flow_spec_action_tag tag = {
		.type = IBV_FLOW_SPEC_ACTION_TAG,
		.size = size,
	};
	struct mlx5_flow_verbs *verbs = flow->cur_verbs;

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
	if (flow->modifier & MLX5_FLOW_MOD_MARK)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "mark action already present");
	if (flow->fate & MLX5_FLOW_FATE_DROP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "mark is not compatible with drop"
					  " action");
	if (flow->modifier & MLX5_FLOW_MOD_FLAG) {
		mlx5_flow_verbs_mark_update(verbs, mark->id);
		size = 0;
	} else if (size <= flow_size) {
		tag.tag_id = mlx5_flow_mark_set(mark->id);
		mlx5_flow_spec_verbs_add(flow, &tag, size);
	}
	flow->modifier |= MLX5_FLOW_MOD_MARK;
	return size;
}

/**
 * Convert the @p action into @p flow after ensuring the NIC will understand
 * and process it correctly.
 * The conversion is performed action per action, each of them is written into
 * the @p flow if its size is lesser or equal to @p flow_size.
 * Validation and memory consumption computation are still performed until the
 * end of @p action, unless an error is encountered.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] actions
 *   Pointer to flow actions array.
 * @param[in, out] flow
 *   Pointer to the rte_flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small some
 *   garbage may be present.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the @p actions has fully been
 *   converted, otherwise another call with this returned memory size should
 *   be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_actions(struct rte_eth_dev *dev,
		  const struct rte_flow_action actions[],
		  struct rte_flow *flow, const size_t flow_size,
		  struct rte_flow_error *error)
{
	size_t size = 0;
	int remain = flow_size;
	int ret = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			ret = mlx5_flow_action_flag(actions, flow, remain,
						    error);
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = mlx5_flow_action_mark(actions, flow, remain,
						    error);
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			ret = mlx5_flow_action_drop(actions, flow, remain,
						    error);
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = mlx5_flow_action_queue(dev, actions, flow, error);
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = mlx5_flow_action_rss(dev, actions, flow, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
		if (ret < 0)
			return ret;
		if (remain > ret)
			remain -= ret;
		else
			remain = 0;
		size += ret;
	}
	if (!flow->fate)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "no fate action found");
	return size;
}

/**
 * Convert the @p attributes, @p pattern, @p action, into an flow for the NIC
 * after ensuring the NIC will understand and process it correctly.
 * The conversion is only performed item/action per item/action, each of
 * them is written into the @p flow if its size is lesser or equal to @p
 * flow_size.
 * Validation and memory consumption computation are still performed until the
 * end, unless an error is encountered.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] flow_size
 *   Size in bytes of the available space in @p flow, if too small some
 *   garbage may be present.
 * @param[in] attributes
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   On success the number of bytes consumed/necessary, if the returned value
 *   is lesser or equal to @p flow_size, the flow has fully been converted and
 *   can be applied, otherwise another call with this returned memory size
 *   should be done.
 *   On error, a negative errno value is returned and rte_errno is set.
 */
static int
mlx5_flow_merge(struct rte_eth_dev *dev, struct rte_flow *flow,
		const size_t flow_size,
		const struct rte_flow_attr *attributes,
		const struct rte_flow_item pattern[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	struct rte_flow local_flow = { .layers = 0, };
	size_t size = sizeof(*flow);
	union {
		struct rte_flow_expand_rss buf;
		uint8_t buffer[2048];
	} expand_buffer;
	struct rte_flow_expand_rss *buf = &expand_buffer.buf;
	struct mlx5_flow_verbs *original_verbs = NULL;
	size_t original_verbs_size = 0;
	uint32_t original_layers = 0;
	int expanded_pattern_idx = 0;
	int ret;
	uint32_t i;

	if (size > flow_size)
		flow = &local_flow;
	ret = mlx5_flow_attributes(dev, attributes, flow, error);
	if (ret < 0)
		return ret;
	ret = mlx5_flow_actions(dev, actions, &local_flow, 0, error);
	if (ret < 0)
		return ret;
	if (local_flow.rss.types) {
		ret = rte_flow_expand_rss(buf, sizeof(expand_buffer.buffer),
					  pattern, local_flow.rss.types,
					  mlx5_support_expansion,
					  MLX5_EXPANSION_ROOT);
		assert(ret > 0 &&
		       (unsigned int)ret < sizeof(expand_buffer.buffer));
	} else {
		buf->entries = 1;
		buf->entry[0].pattern = (void *)(uintptr_t)pattern;
	}
	size += RTE_ALIGN_CEIL(local_flow.rss.queue_num * sizeof(uint16_t),
			       sizeof(void *));
	if (size <= flow_size)
		flow->queue = (void *)(flow + 1);
	LIST_INIT(&flow->verbs);
	flow->layers = 0;
	flow->modifier = 0;
	flow->fate = 0;
	for (i = 0; i != buf->entries; ++i) {
		size_t off = size;
		size_t off2;

		flow->layers = original_layers;
		size += sizeof(struct ibv_flow_attr) +
			sizeof(struct mlx5_flow_verbs);
		off2 = size;
		if (size < flow_size) {
			flow->cur_verbs = (void *)((uintptr_t)flow + off);
			flow->cur_verbs->attr = (void *)(flow->cur_verbs + 1);
			flow->cur_verbs->specs =
				(void *)(flow->cur_verbs->attr + 1);
		}
		/* First iteration convert the pattern into Verbs. */
		if (i == 0) {
			/* Actions don't need to be converted several time. */
			ret = mlx5_flow_actions(dev, actions, flow,
						(size < flow_size) ?
						flow_size - size : 0,
						error);
			if (ret < 0)
				return ret;
			size += ret;
		} else {
			/*
			 * Next iteration means the pattern has already been
			 * converted and an expansion is necessary to match
			 * the user RSS request.  For that only the expanded
			 * items will be converted, the common part with the
			 * user pattern are just copied into the next buffer
			 * zone.
			 */
			size += original_verbs_size;
			if (size < flow_size) {
				rte_memcpy(flow->cur_verbs->attr,
					   original_verbs->attr,
					   original_verbs_size +
					   sizeof(struct ibv_flow_attr));
				flow->cur_verbs->size = original_verbs_size;
			}
		}
		ret = mlx5_flow_items
			((const struct rte_flow_item *)
			 &buf->entry[i].pattern[expanded_pattern_idx],
			 flow,
			 (size < flow_size) ? flow_size - size : 0, error);
		if (ret < 0)
			return ret;
		size += ret;
		if (size <= flow_size) {
			mlx5_flow_adjust_priority(dev, flow);
			LIST_INSERT_HEAD(&flow->verbs, flow->cur_verbs, next);
		}
		/*
		 * Keep a pointer of the first verbs conversion and the layers
		 * it has encountered.
		 */
		if (i == 0) {
			original_verbs = flow->cur_verbs;
			original_verbs_size = size - off2;
			original_layers = flow->layers;
			/*
			 * move the index of the expanded pattern to the
			 * first item not addressed yet.
			 */
			if (pattern->type == RTE_FLOW_ITEM_TYPE_END) {
				expanded_pattern_idx++;
			} else {
				const struct rte_flow_item *item = pattern;

				for (item = pattern;
				     item->type != RTE_FLOW_ITEM_TYPE_END;
				     ++item)
					expanded_pattern_idx++;
			}
		}
	}
	/* Restore the origin layers in the flow. */
	flow->layers = original_layers;
	return size;
}

/**
 * Mark the Rx queues mark flag if the flow has a mark or flag modifier.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to flow structure.
 */
static void
mlx5_flow_rxq_mark_set(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;

	if (flow->modifier & (MLX5_FLOW_MOD_FLAG | MLX5_FLOW_MOD_MARK)) {
		unsigned int i;

		for (i = 0; i != flow->rss.queue_num; ++i) {
			int idx = (*flow->queue)[i];
			struct mlx5_rxq_ctrl *rxq_ctrl =
				container_of((*priv->rxqs)[idx],
					     struct mlx5_rxq_ctrl, rxq);

			rxq_ctrl->rxq.mark = 1;
			rxq_ctrl->flow_mark_n++;
		}
	}
}

/**
 * Clear the Rx queue mark associated with the @p flow if no other flow uses
 * it with a mark request.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to the flow.
 */
static void
mlx5_flow_rxq_mark_trim(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;

	if (flow->modifier & (MLX5_FLOW_MOD_FLAG | MLX5_FLOW_MOD_MARK)) {
		unsigned int i;

		for (i = 0; i != flow->rss.queue_num; ++i) {
			int idx = (*flow->queue)[i];
			struct mlx5_rxq_ctrl *rxq_ctrl =
				container_of((*priv->rxqs)[idx],
					     struct mlx5_rxq_ctrl, rxq);

			rxq_ctrl->flow_mark_n--;
			rxq_ctrl->rxq.mark = !!rxq_ctrl->flow_mark_n;
		}
	}
}

/**
 * Clear the mark bit in all Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_flow_rxq_mark_clear(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int idx;

	for (idx = 0, i = 0; idx != priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (!(*priv->rxqs)[idx])
			continue;
		rxq_ctrl = container_of((*priv->rxqs)[idx],
					struct mlx5_rxq_ctrl, rxq);
		rxq_ctrl->flow_mark_n = 0;
		rxq_ctrl->rxq.mark = 0;
		++idx;
	}
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
	int ret = mlx5_flow_merge(dev, NULL, 0, attr, items, actions, error);

	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Remove the flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 */
static void
mlx5_flow_remove(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_flow_verbs *verbs;

	LIST_FOREACH(verbs, &flow->verbs, next) {
		if (verbs->flow) {
			claim_zero(mlx5_glue->destroy_flow(verbs->flow));
			verbs->flow = NULL;
		}
		if (verbs->hrxq) {
			if (flow->fate & MLX5_FLOW_FATE_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, verbs->hrxq);
			verbs->hrxq = NULL;
		}
	}
}

/**
 * Apply the flow.
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
static int
mlx5_flow_apply(struct rte_eth_dev *dev, struct rte_flow *flow,
		struct rte_flow_error *error)
{
	struct mlx5_flow_verbs *verbs;
	int err;

	LIST_FOREACH(verbs, &flow->verbs, next) {
		if (flow->fate & MLX5_FLOW_FATE_DROP) {
			verbs->hrxq = mlx5_hrxq_drop_new(dev);
			if (!verbs->hrxq) {
				rte_flow_error_set
					(error, errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					 NULL,
					 "cannot get drop hash queue");
				goto error;
			}
		} else {
			struct mlx5_hrxq *hrxq;

			hrxq = mlx5_hrxq_get(dev, flow->key,
					     MLX5_RSS_HASH_KEY_LEN,
					     verbs->hash_fields,
					     (*flow->queue),
					     flow->rss.queue_num, 0, 0);
			if (!hrxq)
				hrxq = mlx5_hrxq_new(dev, flow->key,
						     MLX5_RSS_HASH_KEY_LEN,
						     verbs->hash_fields,
						     (*flow->queue),
						     flow->rss.queue_num, 0, 0);
			if (!hrxq) {
				rte_flow_error_set
					(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					 NULL,
					 "cannot get hash queue");
				goto error;
			}
			verbs->hrxq = hrxq;
		}
		verbs->flow =
			mlx5_glue->create_flow(verbs->hrxq->qp, verbs->attr);
		if (!verbs->flow) {
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
	LIST_FOREACH(verbs, &flow->verbs, next) {
		if (verbs->hrxq) {
			if (flow->fate & MLX5_FLOW_FATE_DROP)
				mlx5_hrxq_drop_release(dev);
			else
				mlx5_hrxq_release(dev, verbs->hrxq);
			verbs->hrxq = NULL;
		}
	}
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Create a flow and add it to @p list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A flow on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow *
mlx5_flow_list_create(struct rte_eth_dev *dev,
		      struct mlx5_flows *list,
		      const struct rte_flow_attr *attr,
		      const struct rte_flow_item items[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	struct rte_flow *flow = NULL;
	size_t size = 0;
	int ret;

	ret = mlx5_flow_merge(dev, flow, size, attr, items, actions, error);
	if (ret < 0)
		return NULL;
	size = ret;
	flow = rte_calloc(__func__, 1, size, 0);
	if (!flow) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "not enough memory to create flow");
		return NULL;
	}
	ret = mlx5_flow_merge(dev, flow, size, attr, items, actions, error);
	if (ret < 0) {
		rte_free(flow);
		return NULL;
	}
	assert((size_t)ret == size);
	if (dev->data->dev_started) {
		ret = mlx5_flow_apply(dev, flow, error);
		if (ret < 0) {
			ret = rte_errno; /* Save rte_errno before cleanup. */
			if (flow) {
				mlx5_flow_remove(dev, flow);
				rte_free(flow);
			}
			rte_errno = ret; /* Restore rte_errno. */
			return NULL;
		}
	}
	mlx5_flow_rxq_mark_set(dev, flow);
	TAILQ_INSERT_TAIL(list, flow, next);
	return flow;
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
	return mlx5_flow_list_create
		(dev, &((struct priv *)dev->data->dev_private)->flows,
		 attr, items, actions, error);
}

/**
 * Destroy a flow in a list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param list
 *   Pointer to a TAILQ flow list.
 * @param[in] flow
 *   Flow to destroy.
 */
static void
mlx5_flow_list_destroy(struct rte_eth_dev *dev, struct mlx5_flows *list,
		       struct rte_flow *flow)
{
	mlx5_flow_remove(dev, flow);
	TAILQ_REMOVE(list, flow, next);
	mlx5_flow_rxq_mark_trim(dev, flow);
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
		mlx5_flow_list_destroy(dev, list, flow);
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

	TAILQ_FOREACH_REVERSE(flow, list, mlx5_flows, next)
		mlx5_flow_remove(dev, flow);
	mlx5_flow_rxq_mark_clear(dev);
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

	TAILQ_FOREACH(flow, list, next) {
		ret = mlx5_flow_apply(dev, flow, &error);
		if (ret < 0)
			goto error;
		mlx5_flow_rxq_mark_set(dev, flow);
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
	struct priv *priv = dev->data->dev_private;
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
	struct priv *priv = dev->data->dev_private;
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

	if (!priv->reta_idx_n) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	for (i = 0; i != priv->reta_idx_n; ++i)
		queue[i] = (*priv->reta_idx)[i];
	flow = mlx5_flow_list_create(dev, &priv->ctrl_flows, &attr, items,
				     actions, &error);
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
	struct priv *priv = dev->data->dev_private;

	mlx5_flow_list_destroy(dev, &priv->flows, flow);
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
	struct priv *priv = dev->data->dev_private;

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
	struct priv *priv = dev->data->dev_private;

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
mlx5_fdir_filter_convert(struct rte_eth_dev *dev,
			 const struct rte_eth_fdir_filter *fdir_filter,
			 struct mlx5_fdir *attributes)
{
	struct priv *priv = dev->data->dev_private;
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
		attributes->l3.ipv4.hdr = (struct ipv4_hdr){
			.src_addr = input->flow.ip4_flow.src_ip,
			.dst_addr = input->flow.ip4_flow.dst_ip,
			.time_to_live = input->flow.ip4_flow.ttl,
			.type_of_service = input->flow.ip4_flow.tos,
			.next_proto_id = input->flow.ip4_flow.proto,
		};
		attributes->l3_mask.ipv4.hdr = (struct ipv4_hdr){
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
		attributes->l3.ipv6.hdr = (struct ipv6_hdr){
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
		attributes->l4.udp.hdr = (struct udp_hdr){
			.src_port = input->flow.udp4_flow.src_port,
			.dst_port = input->flow.udp4_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct udp_hdr){
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
		attributes->l4.tcp.hdr = (struct tcp_hdr){
			.src_port = input->flow.tcp4_flow.src_port,
			.dst_port = input->flow.tcp4_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct tcp_hdr){
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
		attributes->l4.udp.hdr = (struct udp_hdr){
			.src_port = input->flow.udp6_flow.src_port,
			.dst_port = input->flow.udp6_flow.dst_port,
		};
		attributes->l4_mask.udp.hdr = (struct udp_hdr){
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
		attributes->l4.tcp.hdr = (struct tcp_hdr){
			.src_port = input->flow.tcp6_flow.src_port,
			.dst_port = input->flow.tcp6_flow.dst_port,
		};
		attributes->l4_mask.tcp.hdr = (struct tcp_hdr){
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
mlx5_fdir_filter_add(struct rte_eth_dev *dev,
		     const struct rte_eth_fdir_filter *fdir_filter)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_fdir attributes = {
		.attr.group = 0,
		.l2_mask = {
			.dst.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.src.addr_bytes = "\x00\x00\x00\x00\x00\x00",
			.type = 0,
		},
	};
	struct rte_flow_error error;
	struct rte_flow *flow;
	int ret;

	ret = mlx5_fdir_filter_convert(dev, fdir_filter, &attributes);
	if (ret)
		return ret;
	flow = mlx5_flow_list_create(dev, &priv->flows, &attributes.attr,
				     attributes.items, attributes.actions,
				     &error);
	if (flow) {
		DRV_LOG(DEBUG, "port %u FDIR created %p", dev->data->port_id,
			(void *)flow);
		return 0;
	}
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
mlx5_fdir_filter_delete(struct rte_eth_dev *dev __rte_unused,
			const struct rte_eth_fdir_filter *fdir_filter
			__rte_unused)
{
	rte_errno = ENOTSUP;
	return -rte_errno;
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
mlx5_fdir_filter_update(struct rte_eth_dev *dev,
			const struct rte_eth_fdir_filter *fdir_filter)
{
	int ret;

	ret = mlx5_fdir_filter_delete(dev, fdir_filter);
	if (ret)
		return ret;
	return mlx5_fdir_filter_add(dev, fdir_filter);
}

/**
 * Flush all filters.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_fdir_filter_flush(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;

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
mlx5_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir_info)
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
mlx5_fdir_ctrl_func(struct rte_eth_dev *dev, enum rte_filter_op filter_op,
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
		return mlx5_fdir_filter_add(dev, arg);
	case RTE_ETH_FILTER_UPDATE:
		return mlx5_fdir_filter_update(dev, arg);
	case RTE_ETH_FILTER_DELETE:
		return mlx5_fdir_filter_delete(dev, arg);
	case RTE_ETH_FILTER_FLUSH:
		mlx5_fdir_filter_flush(dev);
		break;
	case RTE_ETH_FILTER_INFO:
		mlx5_fdir_info_get(dev, arg);
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
		return mlx5_fdir_ctrl_func(dev, filter_op, arg);
	default:
		DRV_LOG(ERR, "port %u filter type (%d) not supported",
			dev->data->port_id, filter_type);
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	return 0;
}
