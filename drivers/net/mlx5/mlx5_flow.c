/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
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

/* Dev ops structure defined in mlx5.c */
extern const struct eth_dev_ops mlx5_dev_ops;
extern const struct eth_dev_ops mlx5_dev_ops_isolate;

/* Pattern outer Layer bits. */
#define MLX5_FLOW_LAYER_OUTER_L2 (1u << 0)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV4 (1u << 1)
#define MLX5_FLOW_LAYER_OUTER_L3_IPV6 (1u << 2)
#define MLX5_FLOW_LAYER_OUTER_L4_UDP (1u << 3)
#define MLX5_FLOW_LAYER_OUTER_L4_TCP (1u << 4)
#define MLX5_FLOW_LAYER_OUTER_VLAN (1u << 5)

/* Pattern inner Layer bits. */
#define MLX5_FLOW_LAYER_INNER_L2 (1u << 6)
#define MLX5_FLOW_LAYER_INNER_L3_IPV4 (1u << 7)
#define MLX5_FLOW_LAYER_INNER_L3_IPV6 (1u << 8)
#define MLX5_FLOW_LAYER_INNER_L4_UDP (1u << 9)
#define MLX5_FLOW_LAYER_INNER_L4_TCP (1u << 10)
#define MLX5_FLOW_LAYER_INNER_VLAN (1u << 11)

/* Pattern tunnel Layer bits. */
#define MLX5_FLOW_LAYER_VXLAN (1u << 12)
#define MLX5_FLOW_LAYER_VXLAN_GPE (1u << 13)
#define MLX5_FLOW_LAYER_GRE (1u << 14)
#define MLX5_FLOW_LAYER_MPLS (1u << 15)

/* Outer Masks. */
#define MLX5_FLOW_LAYER_OUTER_L3 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV4 | MLX5_FLOW_LAYER_OUTER_L3_IPV6)
#define MLX5_FLOW_LAYER_OUTER_L4 \
	(MLX5_FLOW_LAYER_OUTER_L4_UDP | MLX5_FLOW_LAYER_OUTER_L4_TCP)
#define MLX5_FLOW_LAYER_OUTER \
	(MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_OUTER_L3 | \
	 MLX5_FLOW_LAYER_OUTER_L4)

/* Tunnel Masks. */
#define MLX5_FLOW_LAYER_TUNNEL \
	(MLX5_FLOW_LAYER_VXLAN | MLX5_FLOW_LAYER_VXLAN_GPE | \
	 MLX5_FLOW_LAYER_GRE | MLX5_FLOW_LAYER_MPLS)

/* Inner Masks. */
#define MLX5_FLOW_LAYER_INNER_L3 \
	(MLX5_FLOW_LAYER_INNER_L3_IPV4 | MLX5_FLOW_LAYER_INNER_L3_IPV6)
#define MLX5_FLOW_LAYER_INNER_L4 \
	(MLX5_FLOW_LAYER_INNER_L4_UDP | MLX5_FLOW_LAYER_INNER_L4_TCP)
#define MLX5_FLOW_LAYER_INNER \
	(MLX5_FLOW_LAYER_INNER_L2 | MLX5_FLOW_LAYER_INNER_L3 | \
	 MLX5_FLOW_LAYER_INNER_L4)

/* Actions that modify the fate of matching traffic. */
#define MLX5_FLOW_FATE_DROP (1u << 0)
#define MLX5_FLOW_FATE_QUEUE (1u << 1)
#define MLX5_FLOW_FATE_RSS (1u << 2)

/* Modify a packet. */
#define MLX5_FLOW_MOD_FLAG (1u << 0)
#define MLX5_FLOW_MOD_MARK (1u << 1)
#define MLX5_FLOW_MOD_COUNT (1u << 2)

/* possible L3 layers protocols filtering. */
#define MLX5_IP_PROTOCOL_TCP 6
#define MLX5_IP_PROTOCOL_UDP 17
#define MLX5_IP_PROTOCOL_GRE 47
#define MLX5_IP_PROTOCOL_MPLS 147

/* Priority reserved for default flows. */
#define MLX5_FLOW_PRIO_RSVD ((uint32_t)-1)

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
			 MLX5_EXPANSION_GRE),
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
			 MLX5_EXPANSION_OUTER_IPV6_TCP),
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

/* Counters information. */
struct mlx5_flow_counter {
	LIST_ENTRY(mlx5_flow_counter) next; /**< Pointer to the next counter. */
	uint32_t shared:1; /**< Share counter ID with other flow rules. */
	uint32_t ref_cnt:31; /**< Reference counter. */
	uint32_t id; /**< Counter ID. */
	struct ibv_counter_set *cs; /**< Holds the counters for the rule. */
	uint64_t hits; /**< Number of packets matched by the rule. */
	uint64_t bytes; /**< Number of bytes matched by the rule. */
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
	struct mlx5_flow_counter *counter; /**< Holds Verbs flow counter. */
	struct rte_flow_action_rss rss;/**< RSS context. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	uint16_t (*queue)[]; /**< Destination queues to redirect traffic to. */
	void *nl_flow; /**< Netlink flow buffer if relevant. */
};

static const struct rte_flow_ops mlx5_flow_ops = {
	.validate = mlx5_flow_validate,
	.create = mlx5_flow_create,
	.destroy = mlx5_flow_destroy,
	.flush = mlx5_flow_flush,
	.isolate = mlx5_flow_isolate,
	.query = mlx5_flow_query,
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

/* Tunnel information. */
struct mlx5_flow_tunnel_info {
	uint32_t tunnel; /**< Tunnel bit (see MLX5_FLOW_*). */
	uint32_t ptype; /**< Tunnel Ptype (see RTE_PTYPE_*). */
};

static struct mlx5_flow_tunnel_info tunnels_info[] = {
	{
		.tunnel = MLX5_FLOW_LAYER_VXLAN,
		.ptype = RTE_PTYPE_TUNNEL_VXLAN | RTE_PTYPE_L4_UDP,
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
		.ptype = RTE_PTYPE_TUNNEL_MPLS_IN_GRE | RTE_PTYPE_L4_UDP,
	},
	{
		.tunnel = MLX5_FLOW_LAYER_MPLS,
		.ptype = RTE_PTYPE_TUNNEL_MPLS_IN_GRE,
	},
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
 * Get a flow counter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] shared
 *   Indicate if this counter is shared with other flows.
 * @param[in] id
 *   Counter identifier.
 *
 * @return
 *   A pointer to the counter, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_counter *
mlx5_flow_counter_new(struct rte_eth_dev *dev, uint32_t shared, uint32_t id)
{
	struct priv *priv = dev->data->dev_private;
	struct mlx5_flow_counter *cnt;

	LIST_FOREACH(cnt, &priv->flow_counters, next) {
		if (!cnt->shared || cnt->shared != shared)
			continue;
		if (cnt->id != id)
			continue;
		cnt->ref_cnt++;
		return cnt;
	}
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT

	struct mlx5_flow_counter tmpl = {
		.shared = shared,
		.id = id,
		.cs = mlx5_glue->create_counter_set
			(priv->ctx,
			 &(struct ibv_counter_set_init_attr){
				 .counter_set_id = id,
			 }),
		.hits = 0,
		.bytes = 0,
	};

	if (!tmpl.cs) {
		rte_errno = errno;
		return NULL;
	}
	cnt = rte_calloc(__func__, 1, sizeof(*cnt), 0);
	if (!cnt) {
		rte_errno = ENOMEM;
		return NULL;
	}
	*cnt = tmpl;
	LIST_INSERT_HEAD(&priv->flow_counters, cnt, next);
	return cnt;
#endif
	rte_errno = ENOTSUP;
	return NULL;
}

/**
 * Release a flow counter.
 *
 * @param[in] counter
 *   Pointer to the counter handler.
 */
static void
mlx5_flow_counter_release(struct mlx5_flow_counter *counter)
{
	if (--counter->ref_cnt == 0) {
		claim_zero(mlx5_glue->destroy_counter_set(counter->cs));
		LIST_REMOVE(counter, next);
		rte_free(counter);
	}
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
 * Adjust verbs hash fields according to the @p flow information.
 *
 * @param[in, out] flow.
 *   Pointer to flow structure.
 * @param[in] tunnel
 *   1 when the hash field is for a tunnel item.
 * @param[in] layer_types
 *   ETH_RSS_* types.
 * @param[in] hash_fields
 *   Item hash fields.
 */
static void
mlx5_flow_verbs_hashfields_adjust(struct rte_flow *flow,
				  int tunnel __rte_unused,
				  uint32_t layer_types, uint64_t hash_fields)
{
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	hash_fields |= (tunnel ? IBV_RX_HASH_INNER : 0);
	if (flow->rss.level == 2 && !tunnel)
		hash_fields = 0;
	else if (flow->rss.level < 2 && tunnel)
		hash_fields = 0;
#endif
	if (!(flow->rss.types & layer_types))
		hash_fields = 0;
	flow->cur_verbs->hash_fields |= hash_fields;
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	const unsigned int size = sizeof(struct ibv_flow_spec_eth);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;

	if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
			    MLX5_FLOW_LAYER_OUTER_L2))
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
	flow->layers |= tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
		MLX5_FLOW_LAYER_OUTER_L2;
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
	const enum ibv_flow_spec_type search = eth->type;
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	struct ibv_flow_spec_eth eth = {
		.type = IBV_FLOW_SPEC_ETH | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;
	const uint32_t l34m = tunnel ? (MLX5_FLOW_LAYER_INNER_L3 |
					MLX5_FLOW_LAYER_INNER_L4) :
		(MLX5_FLOW_LAYER_OUTER_L3 | MLX5_FLOW_LAYER_OUTER_L4);
	const uint32_t vlanm = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
		MLX5_FLOW_LAYER_OUTER_VLAN;
	const uint32_t l2m = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
		MLX5_FLOW_LAYER_OUTER_L2;

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
		if (flow->cur_verbs)
			mlx5_flow_item_vlan_update(flow->cur_verbs->attr,
						   &eth);
		size = 0; /* Only an update is done in eth specification. */
	}
	flow->layers |= tunnel ?
		(MLX5_FLOW_LAYER_INNER_L2 | MLX5_FLOW_LAYER_INNER_VLAN) :
		(MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_OUTER_VLAN);
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_ipv4_ext);
	struct ibv_flow_spec_ipv4_ext ipv4 = {
		.type = IBV_FLOW_SPEC_IPV4_EXT |
			(tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;

	if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
			    MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "multiple L3 layers not supported");
	else if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				 MLX5_FLOW_LAYER_OUTER_L4))
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
	flow->layers |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
		MLX5_FLOW_LAYER_OUTER_L3_IPV4;
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
		mlx5_flow_verbs_hashfields_adjust
			(flow, tunnel,
			 (ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 |
			  ETH_RSS_NONFRAG_IPV4_TCP |
			  ETH_RSS_NONFRAG_IPV4_UDP |
			  ETH_RSS_NONFRAG_IPV4_OTHER),
			 (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4));
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_ipv6);
	struct ibv_flow_spec_ipv6 ipv6 = {
		.type = IBV_FLOW_SPEC_IPV6 | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;

	if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
			    MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "multiple L3 layers not supported");
	else if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
				 MLX5_FLOW_LAYER_OUTER_L4))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 cannot follow an L4 layer.");
	/*
	 * IPv6 is not recognised by the NIC inside a GRE tunnel.
	 * Such support has to be disabled as the rule will be
	 * accepted.  Issue reproduced with Mellanox OFED 4.3-3.0.2.1 and
	 * Mellanox OFED 4.4-1.0.0.0.
	 */
	if (tunnel && flow->layers & MLX5_FLOW_LAYER_GRE)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "IPv6 inside a GRE tunnel is"
					  " not recognised.");
	if (!mask)
		mask = &rte_flow_item_ipv6_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_ipv6), error);
	if (ret < 0)
		return ret;
	flow->layers |= tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
		MLX5_FLOW_LAYER_OUTER_L3_IPV6;
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
		mlx5_flow_verbs_hashfields_adjust
			(flow, tunnel,
			 (ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 |
			  ETH_RSS_NONFRAG_IPV6_TCP | ETH_RSS_NONFRAG_IPV6_UDP |
			  ETH_RSS_NONFRAG_IPV6_OTHER | ETH_RSS_IPV6_EX |
			  ETH_RSS_IPV6_TCP_EX | ETH_RSS_IPV6_UDP_EX),
			 (IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6));
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp udp = {
		.type = IBV_FLOW_SPEC_UDP | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_UDP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with UDP layer");
	if (!(flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
			      MLX5_FLOW_LAYER_OUTER_L3)))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 is mandatory to filter"
					  " on L4");
	if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
			    MLX5_FLOW_LAYER_OUTER_L4))
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
	flow->layers |= tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
		MLX5_FLOW_LAYER_OUTER_L4_UDP;
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
		mlx5_flow_verbs_hashfields_adjust(flow, tunnel, ETH_RSS_UDP,
						  (IBV_RX_HASH_SRC_PORT_UDP |
						   IBV_RX_HASH_DST_PORT_UDP));
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
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int size = sizeof(struct ibv_flow_spec_tcp_udp);
	struct ibv_flow_spec_tcp_udp tcp = {
		.type = IBV_FLOW_SPEC_TCP | (tunnel ? IBV_FLOW_SPEC_INNER : 0),
		.size = size,
	};
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_TCP)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with TCP layer");
	if (!(flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L3 :
			      MLX5_FLOW_LAYER_OUTER_L3)))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 is mandatory to filter on L4");
	if (flow->layers & (tunnel ? MLX5_FLOW_LAYER_INNER_L4 :
			    MLX5_FLOW_LAYER_OUTER_L4))
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
	flow->layers |=  tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
		MLX5_FLOW_LAYER_OUTER_L4_TCP;
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
		mlx5_flow_verbs_hashfields_adjust(flow, tunnel, ETH_RSS_TCP,
						  (IBV_RX_HASH_SRC_PORT_TCP |
						   IBV_RX_HASH_DST_PORT_TCP));
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L4;
		mlx5_flow_spec_verbs_add(flow, &tcp, size);
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
mlx5_flow_item_vxlan(const struct rte_flow_item *item, struct rte_flow *flow,
		     const size_t flow_size, struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	int ret;
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };

	if (flow->layers & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "a tunnel is already present");
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_vxlan_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_vxlan_mask,
		 sizeof(struct rte_flow_item_vxlan), error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan.mask.tunnel_id = id.vlan_id;
		/* Remove unwanted bits from values. */
		vxlan.val.tunnel_id &= vxlan.mask.tunnel_id;
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
	if (!vxlan.val.tunnel_id)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VXLAN vni cannot be 0");
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VXLAN tunnel must be fully defined");
	if (size <= flow_size) {
		mlx5_flow_spec_verbs_add(flow, &vxlan, size);
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	}
	flow->layers |= MLX5_FLOW_LAYER_VXLAN;
	return size;
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_flow_item_vxlan_gpe(struct rte_eth_dev *dev,
			 const struct rte_flow_item *item,
			 struct rte_flow *flow, const size_t flow_size,
			 struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan_gpe *spec = item->spec;
	const struct rte_flow_item_vxlan_gpe *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel vxlan_gpe = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
	int ret;
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };

	if (!((struct priv *)dev->data->dev_private)->config.l3_vxlan_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 VXLAN is not enabled by device"
					  " parameter and/or not configured in"
					  " firmware");
	if (flow->layers & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "a tunnel is already present");
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "no outer UDP layer found");
	if (!mask)
		mask = &rte_flow_item_vxlan_gpe_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_vxlan_gpe_mask,
		 sizeof(struct rte_flow_item_vxlan_gpe), error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&id.vni[1], spec->vni, 3);
		vxlan_gpe.val.tunnel_id = id.vlan_id;
		memcpy(&id.vni[1], mask->vni, 3);
		vxlan_gpe.mask.tunnel_id = id.vlan_id;
		if (spec->protocol)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ITEM,
				 item,
				 "VxLAN-GPE protocol not supported");
		/* Remove unwanted bits from values. */
		vxlan_gpe.val.tunnel_id &= vxlan_gpe.mask.tunnel_id;
	}
	/*
	 * Tunnel id 0 is equivalent as not adding a VXLAN layer, if only this
	 * layer is defined in the Verbs specification it is interpreted as
	 * wildcard and all packets will match this rule, if it follows a full
	 * stack layer (ex: eth / ipv4 / udp), all packets matching the layers
	 * before will also match this rule.  To avoid such situation, VNI 0
	 * is currently refused.
	 */
	if (!vxlan_gpe.val.tunnel_id)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VXLAN-GPE vni cannot be 0");
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "VXLAN-GPE tunnel must be fully"
					  " defined");
	if (size <= flow_size) {
		mlx5_flow_spec_verbs_add(flow, &vxlan_gpe, size);
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	}
	flow->layers |= MLX5_FLOW_LAYER_VXLAN_GPE;
	return size;
}

/**
 * Update the protocol in Verbs IPv4/IPv6 spec.
 *
 * @param[in, out] attr
 *   Pointer to Verbs attributes structure.
 * @param[in] search
 *   Specification type to search in order to update the IP protocol.
 * @param[in] protocol
 *   Protocol value to set if none is present in the specification.
 */
static void
mlx5_flow_item_gre_ip_protocol_update(struct ibv_flow_attr *attr,
				      enum ibv_flow_spec_type search,
				      uint8_t protocol)
{
	unsigned int i;
	struct ibv_spec_header *hdr = (struct ibv_spec_header *)
		((uint8_t *)attr + sizeof(struct ibv_flow_attr));

	if (!attr)
		return;
	for (i = 0; i != attr->num_of_specs; ++i) {
		if (hdr->type == search) {
			union {
				struct ibv_flow_spec_ipv4_ext *ipv4;
				struct ibv_flow_spec_ipv6 *ipv6;
			} ip;

			switch (search) {
			case IBV_FLOW_SPEC_IPV4_EXT:
				ip.ipv4 = (struct ibv_flow_spec_ipv4_ext *)hdr;
				if (!ip.ipv4->val.proto) {
					ip.ipv4->val.proto = protocol;
					ip.ipv4->mask.proto = 0xff;
				}
				break;
			case IBV_FLOW_SPEC_IPV6:
				ip.ipv6 = (struct ibv_flow_spec_ipv6 *)hdr;
				if (!ip.ipv6->val.next_hdr) {
					ip.ipv6->val.next_hdr = protocol;
					ip.ipv6->mask.next_hdr = 0xff;
				}
				break;
			default:
				break;
			}
			break;
		}
		hdr = (struct ibv_spec_header *)((uint8_t *)hdr + hdr->size);
	}
}

/**
 * Convert the @p item into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * It will also update the previous L3 layer with the protocol value matching
 * the GRE.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param dev
 *   Pointer to Ethernet device.
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
mlx5_flow_item_gre(const struct rte_flow_item *item,
		   struct rte_flow *flow, const size_t flow_size,
		   struct rte_flow_error *error)
{
	struct mlx5_flow_verbs *verbs = flow->cur_verbs;
	const struct rte_flow_item_gre *spec = item->spec;
	const struct rte_flow_item_gre *mask = item->mask;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	unsigned int size = sizeof(struct ibv_flow_spec_gre);
	struct ibv_flow_spec_gre tunnel = {
		.type = IBV_FLOW_SPEC_GRE,
		.size = size,
	};
#else
	unsigned int size = sizeof(struct ibv_flow_spec_tunnel);
	struct ibv_flow_spec_tunnel tunnel = {
		.type = IBV_FLOW_SPEC_VXLAN_TUNNEL,
		.size = size,
	};
#endif
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_GRE)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with this GRE layer");
	if (flow->layers & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "a tunnel is already present");
	if (!(flow->layers & MLX5_FLOW_LAYER_OUTER_L3))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "L3 Layer is missing");
	if (!mask)
		mask = &rte_flow_item_gre_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_gre_mask,
		 sizeof(struct rte_flow_item_gre), error);
	if (ret < 0)
		return ret;
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	if (spec) {
		tunnel.val.c_ks_res0_ver = spec->c_rsvd0_ver;
		tunnel.val.protocol = spec->protocol;
		tunnel.mask.c_ks_res0_ver = mask->c_rsvd0_ver;
		tunnel.mask.protocol = mask->protocol;
		/* Remove unwanted bits from values. */
		tunnel.val.c_ks_res0_ver &= tunnel.mask.c_ks_res0_ver;
		tunnel.val.protocol &= tunnel.mask.protocol;
		tunnel.val.key &= tunnel.mask.key;
	}
#else
	if (spec && (spec->protocol & mask->protocol))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "without MPLS support the"
					  " specification cannot be used for"
					  " filtering");
#endif /* !HAVE_IBV_DEVICE_MPLS_SUPPORT */
	if (size <= flow_size) {
		if (flow->layers & MLX5_FLOW_LAYER_OUTER_L3_IPV4)
			mlx5_flow_item_gre_ip_protocol_update
				(verbs->attr, IBV_FLOW_SPEC_IPV4_EXT,
				 MLX5_IP_PROTOCOL_GRE);
		else
			mlx5_flow_item_gre_ip_protocol_update
				(verbs->attr, IBV_FLOW_SPEC_IPV6,
				 MLX5_IP_PROTOCOL_GRE);
		mlx5_flow_spec_verbs_add(flow, &tunnel, size);
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	}
	flow->layers |= MLX5_FLOW_LAYER_GRE;
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
mlx5_flow_item_mpls(const struct rte_flow_item *item __rte_unused,
		    struct rte_flow *flow __rte_unused,
		    const size_t flow_size __rte_unused,
		    struct rte_flow_error *error)
{
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
	const struct rte_flow_item_mpls *spec = item->spec;
	const struct rte_flow_item_mpls *mask = item->mask;
	unsigned int size = sizeof(struct ibv_flow_spec_mpls);
	struct ibv_flow_spec_mpls mpls = {
		.type = IBV_FLOW_SPEC_MPLS,
		.size = size,
	};
	int ret;

	if (flow->l3_protocol_en && flow->l3_protocol != MLX5_IP_PROTOCOL_MPLS)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "protocol filtering not compatible"
					  " with MPLS layer");
	/* Multi-tunnel isn't allowed but MPLS over GRE is an exception. */
	if (flow->layers & MLX5_FLOW_LAYER_TUNNEL &&
	    (flow->layers & MLX5_FLOW_LAYER_GRE) != MLX5_FLOW_LAYER_GRE)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item,
					  "a tunnel is already"
					  " present");
	if (!mask)
		mask = &rte_flow_item_mpls_mask;
	ret = mlx5_flow_item_acceptable
		(item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_mpls_mask,
		 sizeof(struct rte_flow_item_mpls), error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&mpls.val.label, spec, sizeof(mpls.val.label));
		memcpy(&mpls.mask.label, mask, sizeof(mpls.mask.label));
		/* Remove unwanted bits from values.  */
		mpls.val.label &= mpls.mask.label;
	}
	if (size <= flow_size) {
		mlx5_flow_spec_verbs_add(flow, &mpls, size);
		flow->cur_verbs->attr->priority = MLX5_PRIORITY_MAP_L2;
	}
	flow->layers |= MLX5_FLOW_LAYER_MPLS;
	return size;
#endif /* !HAVE_IBV_DEVICE_MPLS_SUPPORT */
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_ITEM,
				  item,
				  "MPLS is not supported by Verbs, please"
				  " update.");
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
mlx5_flow_items(struct rte_eth_dev *dev,
		const struct rte_flow_item pattern[],
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
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_item_vxlan(pattern, flow, remain,
						   error);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_item_vxlan_gpe(dev, pattern, flow,
						       remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = mlx5_flow_item_gre(pattern, flow, remain, error);
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_item_mpls(pattern, flow, remain, error);
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
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (rss->level > 2)
#else
	if (rss->level > 1)
#endif
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
	if (!rss->queue_num)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  rss,
					  "no queues were provided for RSS");
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
		if (rss->queue[i] >= priv->rxqs_n)
			return rte_flow_error_set
				(error, EINVAL,
				 RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 rss,
				 "queue index out of range");
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
	flow->rss.level = rss->level;
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
 * Convert the @p action into a Verbs specification after ensuring the NIC
 * will understand and process it correctly.
 * If the necessary size for the conversion is greater than the @p flow_size,
 * nothing is written in @p flow, the validation is still performed.
 *
 * @param action[in]
 *   Action configuration.
 * @param flow[in, out]
 *   Pointer to flow structure.
 * @param flow_size[in]
 *   Size in bytes of the available space in @p flow, if too small, nothing is
 *   written.
 * @param error[int, out]
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
mlx5_flow_action_count(struct rte_eth_dev *dev,
		       const struct rte_flow_action *action,
		       struct rte_flow *flow,
		       const size_t flow_size __rte_unused,
		       struct rte_flow_error *error)
{
	const struct rte_flow_action_count *count = action->conf;
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	unsigned int size = sizeof(struct ibv_flow_spec_counter_action);
	struct ibv_flow_spec_counter_action counter = {
		.type = IBV_FLOW_SPEC_ACTION_COUNT,
		.size = size,
	};
#endif

	if (!flow->counter) {
		flow->counter = mlx5_flow_counter_new(dev, count->shared,
						      count->id);
		if (!flow->counter)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "cannot get counter"
						  " context.");
	}
	if (!((struct priv *)dev->data->dev_private)->config.flow_counter_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action,
					  "flow counters are not supported.");
	flow->modifier |= MLX5_FLOW_MOD_COUNT;
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	counter.counter_set_handle = flow->counter->cs->handle;
	if (size <= flow_size)
		mlx5_flow_spec_verbs_add(flow, &counter, size);
	return size;
#endif
	return 0;
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
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_flow_action_count(dev, actions, flow, remain,
						     error);
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
 * Validate flow rule and fill flow structure accordingly.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] flow
 *   Pointer to flow structure.
 * @param flow_size
 *   Size of allocated space for @p flow.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] pattern
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   A positive value representing the size of the flow object in bytes
 *   regardless of @p flow_size on success, a negative errno value otherwise
 *   and rte_errno is set.
 */
static int
mlx5_flow_merge_switch(struct rte_eth_dev *dev,
		       struct rte_flow *flow,
		       size_t flow_size,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item pattern[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_error *error)
{
	unsigned int n = mlx5_dev_to_port_id(dev->device, NULL, 0);
	uint16_t port_id[!n + n];
	struct mlx5_nl_flow_ptoi ptoi[!n + n + 1];
	size_t off = RTE_ALIGN_CEIL(sizeof(*flow), alignof(max_align_t));
	unsigned int i;
	unsigned int own = 0;
	int ret;

	/* At least one port is needed when no switch domain is present. */
	if (!n) {
		n = 1;
		port_id[0] = dev->data->port_id;
	} else {
		n = RTE_MIN(mlx5_dev_to_port_id(dev->device, port_id, n), n);
	}
	for (i = 0; i != n; ++i) {
		struct rte_eth_dev_info dev_info;

		rte_eth_dev_info_get(port_id[i], &dev_info);
		if (port_id[i] == dev->data->port_id)
			own = i;
		ptoi[i].port_id = port_id[i];
		ptoi[i].ifindex = dev_info.if_index;
	}
	/* Ensure first entry of ptoi[] is the current device. */
	if (own) {
		ptoi[n] = ptoi[0];
		ptoi[0] = ptoi[own];
		ptoi[own] = ptoi[n];
	}
	/* An entry with zero ifindex terminates ptoi[]. */
	ptoi[n].port_id = 0;
	ptoi[n].ifindex = 0;
	if (flow_size < off)
		flow_size = 0;
	ret = mlx5_nl_flow_transpose((uint8_t *)flow + off,
				     flow_size ? flow_size - off : 0,
				     ptoi, attr, pattern, actions, error);
	if (ret < 0)
		return ret;
	if (flow_size) {
		*flow = (struct rte_flow){
			.attributes = *attr,
			.nl_flow = (uint8_t *)flow + off,
		};
		/*
		 * Generate a reasonably unique handle based on the address
		 * of the target buffer.
		 *
		 * This is straightforward on 32-bit systems where the flow
		 * pointer can be used directly. Otherwise, its least
		 * significant part is taken after shifting it by the
		 * previous power of two of the pointed buffer size.
		 */
		if (sizeof(flow) <= 4)
			mlx5_nl_flow_brand(flow->nl_flow, (uintptr_t)flow);
		else
			mlx5_nl_flow_brand
				(flow->nl_flow,
				 (uintptr_t)flow >>
				 rte_log2_u32(rte_align32prevpow2(flow_size)));
	}
	return off + ret;
}

static unsigned int
mlx5_find_graph_root(const struct rte_flow_item pattern[], uint32_t rss_level)
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

	if (attributes->transfer)
		return mlx5_flow_merge_switch(dev, flow, flow_size,
					      attributes, pattern,
					      actions, error);
	if (size > flow_size)
		flow = &local_flow;
	ret = mlx5_flow_attributes(dev, attributes, flow, error);
	if (ret < 0)
		return ret;
	ret = mlx5_flow_actions(dev, actions, &local_flow, 0, error);
	if (ret < 0)
		return ret;
	if (local_flow.rss.types) {
		unsigned int graph_root;

		graph_root = mlx5_find_graph_root(pattern,
						  local_flow.rss.level);
		ret = rte_flow_expand_rss(buf, sizeof(expand_buffer.buffer),
					  pattern, local_flow.rss.types,
					  mlx5_support_expansion,
					  graph_root);
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
			(dev,
			 (const struct rte_flow_item *)
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
 * Lookup and set the ptype in the data Rx part.  A single Ptype can be used,
 * if several tunnel rules are used on this queue, the tunnel ptype will be
 * cleared.
 *
 * @param rxq_ctrl
 *   Rx queue to update.
 */
static void
mlx5_flow_rxq_tunnel_ptype_update(struct mlx5_rxq_ctrl *rxq_ctrl)
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
 * Set the Rx queue flags (Mark/Flag and Tunnel Ptypes) according to the flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Pointer to flow structure.
 */
static void
mlx5_flow_rxq_flags_set(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	const int mark = !!(flow->modifier &
			    (MLX5_FLOW_MOD_FLAG | MLX5_FLOW_MOD_MARK));
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int i;

	for (i = 0; i != flow->rss.queue_num; ++i) {
		int idx = (*flow->queue)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of((*priv->rxqs)[idx],
				     struct mlx5_rxq_ctrl, rxq);

		if (mark) {
			rxq_ctrl->rxq.mark = 1;
			rxq_ctrl->flow_mark_n++;
		}
		if (tunnel) {
			unsigned int j;

			/* Increase the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel & flow->layers) ==
				    tunnels_info[j].tunnel) {
					rxq_ctrl->flow_tunnels_n[j]++;
					break;
				}
			}
			mlx5_flow_rxq_tunnel_ptype_update(rxq_ctrl);
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
mlx5_flow_rxq_flags_trim(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct priv *priv = dev->data->dev_private;
	const int mark = !!(flow->modifier &
			    (MLX5_FLOW_MOD_FLAG | MLX5_FLOW_MOD_MARK));
	const int tunnel = !!(flow->layers & MLX5_FLOW_LAYER_TUNNEL);
	unsigned int i;

	assert(dev->data->dev_started);
	for (i = 0; i != flow->rss.queue_num; ++i) {
		int idx = (*flow->queue)[i];
		struct mlx5_rxq_ctrl *rxq_ctrl =
			container_of((*priv->rxqs)[idx],
				     struct mlx5_rxq_ctrl, rxq);

		if (mark) {
			rxq_ctrl->flow_mark_n--;
			rxq_ctrl->rxq.mark = !!rxq_ctrl->flow_mark_n;
		}
		if (tunnel) {
			unsigned int j;

			/* Decrease the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel & flow->layers) ==
				    tunnels_info[j].tunnel) {
					rxq_ctrl->flow_tunnels_n[j]--;
					break;
				}
			}
			mlx5_flow_rxq_tunnel_ptype_update(rxq_ctrl);
		}
	}
}

/**
 * Clear the Mark/Flag and Tunnel ptype information in all Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_flow_rxq_flags_clear(struct rte_eth_dev *dev)
{
	struct priv *priv = dev->data->dev_private;
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
	struct priv *priv = dev->data->dev_private;
	struct mlx5_flow_verbs *verbs;

	if (flow->nl_flow && priv->mnl_socket)
		mlx5_nl_flow_destroy(priv->mnl_socket, flow->nl_flow, NULL);
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
	if (flow->counter) {
		mlx5_flow_counter_release(flow->counter);
		flow->counter = NULL;
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
	struct priv *priv = dev->data->dev_private;
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
					     flow->rss.queue_num);
			if (!hrxq)
				hrxq = mlx5_hrxq_new(dev, flow->key,
						     MLX5_RSS_HASH_KEY_LEN,
						     verbs->hash_fields,
						     (*flow->queue),
						     flow->rss.queue_num,
						     !!(flow->layers &
						      MLX5_FLOW_LAYER_TUNNEL));
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
	if (flow->nl_flow &&
	    priv->mnl_socket &&
	    mlx5_nl_flow_create(priv->mnl_socket, flow->nl_flow, error))
		goto error;
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
	TAILQ_INSERT_TAIL(list, flow, next);
	mlx5_flow_rxq_flags_set(dev, flow);
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
	/*
	 * Update RX queue flags only if port is started, otherwise it is
	 * already clean.
	 */
	if (dev->data->dev_started)
		mlx5_flow_rxq_flags_trim(dev, flow);
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
	mlx5_flow_rxq_flags_clear(dev);
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
		mlx5_flow_rxq_flags_set(dev, flow);
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
 * Query flow counter.
 *
 * @param flow
 *   Pointer to the flow.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_query_count(struct rte_flow *flow __rte_unused,
		      void *data __rte_unused,
		      struct rte_flow_error *error)
{
#ifdef HAVE_IBV_DEVICE_COUNTERS_SET_SUPPORT
	if (flow->modifier & MLX5_FLOW_MOD_COUNT) {
		struct rte_flow_query_count *qc = data;
		uint64_t counters[2] = {0, 0};
		struct ibv_query_counter_set_attr query_cs_attr = {
			.cs = flow->counter->cs,
			.query_flags = IBV_COUNTER_SET_FORCE_UPDATE,
		};
		struct ibv_counter_set_data query_out = {
			.out = counters,
			.outlen = 2 * sizeof(uint64_t),
		};
		int err = mlx5_glue->query_counter_set(&query_cs_attr,
						       &query_out);

		if (err)
			return rte_flow_error_set
				(error, err,
				 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				 NULL,
				 "cannot read counter");
		qc->hits_set = 1;
		qc->bytes_set = 1;
		qc->hits = counters[0] - flow->counter->hits;
		qc->bytes = counters[1] - flow->counter->bytes;
		if (qc->reset) {
			flow->counter->hits = counters[0];
			flow->counter->bytes = counters[1];
		}
		return 0;
	}
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "flow does not have counter");
#endif
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL,
				  "counters are not available");
}

/**
 * Query a flows.
 *
 * @see rte_flow_query()
 * @see rte_flow_ops
 */
int
mlx5_flow_query(struct rte_eth_dev *dev __rte_unused,
		struct rte_flow *flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	int ret = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_flow_query_count(flow, data, error);
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  actions,
						  "action not supported");
		}
		if (ret < 0)
			return ret;
	}
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
