/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#include <stdalign.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/queue.h>

#include <eal_export.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_eal_paging.h>
#include <rte_flow.h>
#include <rte_cycles.h>
#include <rte_flow_driver.h>
#include <rte_malloc.h>
#include <rte_ip.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"
#include "mlx5_rx.h"
#include "mlx5_tx.h"
#include "mlx5_common_os.h"
#include "rte_pmd_mlx5.h"

/*
 * Shared array for quick translation between port_id and vport mask/values
 * used for HWS rules.
 */
struct flow_hw_port_info mlx5_flow_hw_port_infos[RTE_MAX_ETHPORTS];

struct tunnel_default_miss_ctx {
	uint16_t *queue;
	__extension__
	union {
		struct rte_flow_action_rss action_rss;
		struct rte_flow_action_queue miss_queue;
		struct rte_flow_action_jump miss_jump;
		uint8_t raw[0];
	};
};

void
mlx5_indirect_list_handles_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
#ifdef HAVE_MLX5_HWS_SUPPORT
	struct rte_flow_error error;
#endif

	while (!LIST_EMPTY(&priv->indirect_list_head)) {
		struct mlx5_indirect_list *e =
			LIST_FIRST(&priv->indirect_list_head);

		LIST_REMOVE(e, entry);
		switch (e->type) {
#ifdef HAVE_MLX5_HWS_SUPPORT
		case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
			mlx5_hw_mirror_destroy(dev, (struct mlx5_mirror *)e);
		break;
		case MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY:
			mlx5_destroy_legacy_indirect(dev, e);
			break;
		case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
			mlx5_reformat_action_destroy(dev,
				(struct rte_flow_action_list_handle *)e, &error);
			break;
#endif
		default:
			DRV_LOG(ERR, "invalid indirect list type");
			MLX5_ASSERT(false);
			break;
		}
	}
}

static int
flow_tunnel_add_default_miss(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_action *app_actions,
			     uint32_t flow_idx,
			     const struct mlx5_flow_tunnel *tunnel,
			     struct tunnel_default_miss_ctx *ctx,
			     struct rte_flow_error *error);
static struct mlx5_flow_tunnel *
mlx5_find_tunnel_id(struct rte_eth_dev *dev, uint32_t id);
static void
mlx5_flow_tunnel_free(struct rte_eth_dev *dev, struct mlx5_flow_tunnel *tunnel);
static uint32_t
tunnel_flow_group_to_flow_table(struct rte_eth_dev *dev,
				const struct mlx5_flow_tunnel *tunnel,
				uint32_t group, uint32_t *table,
				struct rte_flow_error *error);

/** Device flow drivers. */
extern const struct mlx5_flow_driver_ops mlx5_flow_verbs_drv_ops;

const struct mlx5_flow_driver_ops mlx5_flow_null_drv_ops;

const struct mlx5_flow_driver_ops *flow_drv_ops[] = {
	[MLX5_FLOW_TYPE_MIN] = &mlx5_flow_null_drv_ops,
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	[MLX5_FLOW_TYPE_DV] = &mlx5_flow_dv_drv_ops,
#endif
#ifdef HAVE_MLX5_HWS_SUPPORT
	[MLX5_FLOW_TYPE_HW] = &mlx5_flow_hw_drv_ops,
#endif
	[MLX5_FLOW_TYPE_VERBS] = &mlx5_flow_verbs_drv_ops,
	[MLX5_FLOW_TYPE_MAX] = &mlx5_flow_null_drv_ops
};

/** Helper macro to build input graph for mlx5_flow_expand_rss(). */
#define MLX5_FLOW_EXPAND_RSS_NEXT(...) \
	(const int []){ \
		__VA_ARGS__, 0, \
	}

/** Node object of input graph for mlx5_flow_expand_rss(). */
struct mlx5_flow_expand_node {
	const int *const next;
	/**<
	 * List of next node indexes. Index 0 is interpreted as a terminator.
	 */
	const enum rte_flow_item_type type;
	/**< Pattern item type of current node. */
	uint64_t rss_types;
	/**<
	 * RSS types bit-field associated with this node
	 * (see RTE_ETH_RSS_* definitions).
	 */
	uint64_t node_flags;
	/**<
	 *  Bit-fields that define how the node is used in the expansion.
	 * (see MLX5_EXPANSION_NODE_* definitions).
	 */
};

/** Keep same format with mlx5_flow_expand_rss to share the buffer for expansion. */
struct mlx5_flow_expand_sqn {
	uint32_t entries; /** Number of entries */
	struct {
		struct rte_flow_item *pattern; /**< Expanded pattern array. */
		uint32_t priority; /**< Priority offset for each expansion. */
	} entry[];
};

/* Optional expand field. The expansion alg will not go deeper. */
#define MLX5_EXPANSION_NODE_OPTIONAL (UINT64_C(1) << 0)

/* The node is not added implicitly as expansion to the flow pattern.
 * If the node type does not match the flow pattern item type, the
 * expansion alg will go deeper to its next items.
 * In the current implementation, the list of next nodes indexes can
 * have up to one node with this flag set and it has to be the last
 * node index (before the list terminator).
 */
#define MLX5_EXPANSION_NODE_EXPLICIT (UINT64_C(1) << 1)

/** Object returned by mlx5_flow_expand_rss(). */
struct mlx5_flow_expand_rss {
	uint32_t entries;
	/**< Number of entries @p patterns and @p priorities. */
	struct {
		struct rte_flow_item *pattern; /**< Expanded pattern array. */
		uint32_t priority; /**< Priority offset for each expansion. */
	} entry[];
};

static void
mlx5_dbg__print_pattern(const struct rte_flow_item *item);

static const struct mlx5_flow_expand_node *
mlx5_flow_expand_rss_adjust_node(const struct rte_flow_item *pattern,
		unsigned int item_idx,
		const struct mlx5_flow_expand_node graph[],
		const struct mlx5_flow_expand_node *node);

static __rte_always_inline int
mlx5_need_cache_flow(const struct mlx5_priv *priv,
		     const struct rte_flow_attr *attr)
{
	return priv->isolated && priv->sh->config.dv_flow_en == 1 &&
		(attr ? !attr->group : true) &&
		priv->mode_info.mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_STANDBY &&
		(!priv->sh->config.dv_esw_en || !priv->sh->config.fdb_def_rule);
}

static bool
mlx5_flow_is_rss_expandable_item(const struct rte_flow_item *item)
{
	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
	case RTE_FLOW_ITEM_TYPE_VLAN:
	case RTE_FLOW_ITEM_TYPE_IPV4:
	case RTE_FLOW_ITEM_TYPE_IPV6:
	case RTE_FLOW_ITEM_TYPE_UDP:
	case RTE_FLOW_ITEM_TYPE_TCP:
	case RTE_FLOW_ITEM_TYPE_ESP:
	case RTE_FLOW_ITEM_TYPE_ICMP:
	case RTE_FLOW_ITEM_TYPE_ICMP6:
	case RTE_FLOW_ITEM_TYPE_VXLAN:
	case RTE_FLOW_ITEM_TYPE_NVGRE:
	case RTE_FLOW_ITEM_TYPE_GRE:
	case RTE_FLOW_ITEM_TYPE_GENEVE:
	case RTE_FLOW_ITEM_TYPE_MPLS:
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
	case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
	case RTE_FLOW_ITEM_TYPE_GTP:
		return true;
	default:
		break;
	}
	return false;
}

/**
 * Network Service Header (NSH) and its next protocol values
 * are described in RFC-8393.
 */
static enum rte_flow_item_type
mlx5_nsh_proto_to_item_type(uint8_t proto_spec, uint8_t proto_mask)
{
	enum rte_flow_item_type type;

	switch (proto_mask & proto_spec) {
	case 0:
		type = RTE_FLOW_ITEM_TYPE_VOID;
		break;
	case RTE_VXLAN_GPE_TYPE_IPV4:
		type = RTE_FLOW_ITEM_TYPE_IPV4;
		break;
	case RTE_VXLAN_GPE_TYPE_IPV6:
		type = RTE_VXLAN_GPE_TYPE_IPV6;
		break;
	case RTE_VXLAN_GPE_TYPE_ETH:
		type = RTE_FLOW_ITEM_TYPE_ETH;
		break;
	default:
		type = RTE_FLOW_ITEM_TYPE_END;
	}
	return type;
}

static enum rte_flow_item_type
mlx5_inet_proto_to_item_type(uint8_t proto_spec, uint8_t proto_mask)
{
	enum rte_flow_item_type type;

	switch (proto_mask & proto_spec) {
	case 0:
		type = RTE_FLOW_ITEM_TYPE_VOID;
		break;
	case IPPROTO_UDP:
		type = RTE_FLOW_ITEM_TYPE_UDP;
		break;
	case IPPROTO_TCP:
		type = RTE_FLOW_ITEM_TYPE_TCP;
		break;
	case IPPROTO_IPIP:
		type = RTE_FLOW_ITEM_TYPE_IPV4;
		break;
	case IPPROTO_IPV6:
		type = RTE_FLOW_ITEM_TYPE_IPV6;
		break;
	case IPPROTO_ESP:
		type = RTE_FLOW_ITEM_TYPE_ESP;
		break;
	default:
		type = RTE_FLOW_ITEM_TYPE_END;
	}
	return type;
}

static enum rte_flow_item_type
mlx5_ethertype_to_item_type(rte_be16_t type_spec,
			    rte_be16_t type_mask, bool is_tunnel)
{
	enum rte_flow_item_type type;

	switch (rte_be_to_cpu_16(type_spec & type_mask)) {
	case 0:
		type = RTE_FLOW_ITEM_TYPE_VOID;
		break;
	case RTE_ETHER_TYPE_TEB:
		type = is_tunnel ?
		       RTE_FLOW_ITEM_TYPE_ETH : RTE_FLOW_ITEM_TYPE_END;
		break;
	case RTE_ETHER_TYPE_VLAN:
		type = !is_tunnel ?
		       RTE_FLOW_ITEM_TYPE_VLAN : RTE_FLOW_ITEM_TYPE_END;
		break;
	case RTE_ETHER_TYPE_IPV4:
		type = RTE_FLOW_ITEM_TYPE_IPV4;
		break;
	case RTE_ETHER_TYPE_IPV6:
		type = RTE_FLOW_ITEM_TYPE_IPV6;
		break;
	default:
		type = RTE_FLOW_ITEM_TYPE_END;
	}
	return type;
}

static enum rte_flow_item_type
mlx5_flow_expand_rss_item_complete(const struct rte_flow_item *item)
{
#define MLX5_XSET_ITEM_MASK_SPEC(type, fld)                              \
	do {                                                             \
		const void *m = item->mask;                              \
		const void *s = item->spec;                              \
		mask = m ?                                               \
			((const struct rte_flow_item_##type *)m)->fld :  \
			rte_flow_item_##type##_mask.fld;                 \
		spec = ((const struct rte_flow_item_##type *)s)->fld;    \
	} while (0)

	enum rte_flow_item_type ret;
	uint16_t spec, mask;

	if (item == NULL || item->spec == NULL)
		return RTE_FLOW_ITEM_TYPE_VOID;
	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		MLX5_XSET_ITEM_MASK_SPEC(eth, hdr.ether_type);
		if (!mask)
			return RTE_FLOW_ITEM_TYPE_VOID;
		ret = mlx5_ethertype_to_item_type(spec, mask, false);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		MLX5_XSET_ITEM_MASK_SPEC(vlan, hdr.eth_proto);
		if (!mask)
			return RTE_FLOW_ITEM_TYPE_VOID;
		ret = mlx5_ethertype_to_item_type(spec, mask, false);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		MLX5_XSET_ITEM_MASK_SPEC(ipv4, hdr.next_proto_id);
		if (!mask)
			return RTE_FLOW_ITEM_TYPE_VOID;
		ret = mlx5_inet_proto_to_item_type(spec, mask);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		MLX5_XSET_ITEM_MASK_SPEC(ipv6, hdr.proto);
		if (!mask)
			return RTE_FLOW_ITEM_TYPE_VOID;
		ret = mlx5_inet_proto_to_item_type(spec, mask);
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		MLX5_XSET_ITEM_MASK_SPEC(geneve, protocol);
		ret = mlx5_ethertype_to_item_type(spec, mask, true);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		MLX5_XSET_ITEM_MASK_SPEC(gre, protocol);
		ret = mlx5_ethertype_to_item_type(spec, mask, true);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
		MLX5_XSET_ITEM_MASK_SPEC(vxlan_gpe, hdr.proto);
		ret = mlx5_nsh_proto_to_item_type(spec, mask);
		break;
	default:
		ret = RTE_FLOW_ITEM_TYPE_VOID;
		break;
	}
	return ret;
#undef MLX5_XSET_ITEM_MASK_SPEC
}

static const int *
mlx5_flow_expand_rss_skip_explicit(const struct mlx5_flow_expand_node graph[],
		const int *next_node)
{
	const struct mlx5_flow_expand_node *node = NULL;
	const int *next = next_node;

	while (next && *next) {
		/*
		 * Skip the nodes with the MLX5_EXPANSION_NODE_EXPLICIT
		 * flag set, because they were not found in the flow pattern.
		 */
		node = &graph[*next];
		if (!(node->node_flags & MLX5_EXPANSION_NODE_EXPLICIT))
			break;
		next = node->next;
	}
	return next;
}

#define MLX5_RSS_EXP_ELT_N 32

/**
 * Expand RSS flows into several possible flows according to the RSS hash
 * fields requested and the driver capabilities.
 *
 * @param[out] buf
 *   Buffer to store the result expansion.
 * @param[in] size
 *   Buffer size in bytes. If 0, @p buf can be NULL.
 * @param[in] pattern
 *   User flow pattern.
 * @param[in] types
 *   RSS types to expand (see RTE_ETH_RSS_* definitions).
 * @param[in] graph
 *   Input graph to expand @p pattern according to @p types.
 * @param[in] graph_root_index
 *   Index of root node in @p graph, typically 0.
 *
 * @return
 *   A positive value representing the size of @p buf in bytes regardless of
 *   @p size on success, a negative errno value otherwise and rte_errno is
 *   set, the following errors are defined:
 *
 *   -E2BIG: graph-depth @p graph is too deep.
 *   -EINVAL: @p size has not enough space for expanded pattern.
 */
static int
mlx5_flow_expand_rss(struct mlx5_flow_expand_rss *buf, size_t size,
		     const struct rte_flow_item *pattern, uint64_t types,
		     const struct mlx5_flow_expand_node graph[],
		     int graph_root_index)
{
	const struct rte_flow_item *item;
	const struct mlx5_flow_expand_node *node = &graph[graph_root_index];
	const int *next_node;
	const int *stack[MLX5_RSS_EXP_ELT_N];
	int stack_pos = 0;
	struct rte_flow_item flow_items[MLX5_RSS_EXP_ELT_N];
	unsigned int i, item_idx, last_expand_item_idx = 0;
	size_t lsize;
	size_t user_pattern_size = 0;
	void *addr = NULL;
	const struct mlx5_flow_expand_node *next = NULL;
	struct rte_flow_item missed_item;
	int missed = 0;
	int elt = 0;
	const struct rte_flow_item *last_expand_item = NULL;

	memset(&missed_item, 0, sizeof(missed_item));
	lsize = offsetof(struct mlx5_flow_expand_rss, entry) +
		MLX5_RSS_EXP_ELT_N * sizeof(buf->entry[0]);
	if (lsize > size)
		return -EINVAL;
	buf->entry[0].priority = 0;
	buf->entry[0].pattern = (void *)&buf->entry[MLX5_RSS_EXP_ELT_N];
	buf->entries = 0;
	addr = buf->entry[0].pattern;
	for (item = pattern, item_idx = 0;
			item->type != RTE_FLOW_ITEM_TYPE_END;
			item++, item_idx++) {
		if (!mlx5_flow_is_rss_expandable_item(item)) {
			user_pattern_size += sizeof(*item);
			continue;
		}
		last_expand_item = item;
		last_expand_item_idx = item_idx;
		i = 0;
		while (node->next && node->next[i]) {
			next = &graph[node->next[i]];
			if (next->type == item->type)
				break;
			if (next->node_flags & MLX5_EXPANSION_NODE_EXPLICIT) {
				node = next;
				i = 0;
			} else {
				++i;
			}
		}
		if (next)
			node = next;
		user_pattern_size += sizeof(*item);
	}
	user_pattern_size += sizeof(*item); /* Handle END item. */
	lsize += user_pattern_size;
	if (lsize > size)
		return -EINVAL;
	/* Copy the user pattern in the first entry of the buffer. */
	rte_memcpy(addr, pattern, user_pattern_size);
	addr = (void *)(((uintptr_t)addr) + user_pattern_size);
	buf->entries = 1;
	/* Start expanding. */
	memset(flow_items, 0, sizeof(flow_items));
	user_pattern_size -= sizeof(*item);
	/*
	 * Check if the last valid item has spec set, need complete pattern,
	 * and the pattern can be used for expansion.
	 */
	missed_item.type = mlx5_flow_expand_rss_item_complete(last_expand_item);
	if (missed_item.type == RTE_FLOW_ITEM_TYPE_END) {
		/* Item type END indicates expansion is not required. */
		return lsize;
	}
	if (missed_item.type != RTE_FLOW_ITEM_TYPE_VOID) {
		next = NULL;
		missed = 1;
		i = 0;
		while (node->next && node->next[i]) {
			next = &graph[node->next[i]];
			if (next->type == missed_item.type) {
				flow_items[0].type = missed_item.type;
				flow_items[1].type = RTE_FLOW_ITEM_TYPE_END;
				break;
			}
			if (next->node_flags & MLX5_EXPANSION_NODE_EXPLICIT) {
				node = next;
				i = 0;
			} else {
				++i;
			}
			next = NULL;
		}
	}
	if (next && missed) {
		elt = 2; /* missed item + item end. */
		node = next;
		lsize += elt * sizeof(*item) + user_pattern_size;
		if (lsize > size)
			return -EINVAL;
		if (node->rss_types & types) {
			buf->entry[buf->entries].priority = 1;
			buf->entry[buf->entries].pattern = addr;
			buf->entries++;
			rte_memcpy(addr, buf->entry[0].pattern,
				   user_pattern_size);
			addr = (void *)(((uintptr_t)addr) + user_pattern_size);
			rte_memcpy(addr, flow_items, elt * sizeof(*item));
			addr = (void *)(((uintptr_t)addr) +
					elt * sizeof(*item));
		}
	} else if (last_expand_item != NULL) {
		node = mlx5_flow_expand_rss_adjust_node(pattern,
				last_expand_item_idx, graph, node);
	}
	memset(flow_items, 0, sizeof(flow_items));
	next_node = mlx5_flow_expand_rss_skip_explicit(graph,
			node->next);
	stack[stack_pos] = next_node;
	node = next_node ? &graph[*next_node] : NULL;
	while (node) {
		flow_items[stack_pos].type = node->type;
		if (node->rss_types & types) {
			size_t n;
			/*
			 * compute the number of items to copy from the
			 * expansion and copy it.
			 * When the stack_pos is 0, there are 1 element in it,
			 * plus the addition END item.
			 */
			elt = stack_pos + 2;
			flow_items[stack_pos + 1].type = RTE_FLOW_ITEM_TYPE_END;
			lsize += elt * sizeof(*item) + user_pattern_size;
			if (lsize > size)
				return -EINVAL;
			n = elt * sizeof(*item);
			MLX5_ASSERT((buf->entries) < MLX5_RSS_EXP_ELT_N);
			buf->entry[buf->entries].priority =
				stack_pos + 1 + missed;
			buf->entry[buf->entries].pattern = addr;
			buf->entries++;
			rte_memcpy(addr, buf->entry[0].pattern,
				   user_pattern_size);
			addr = (void *)(((uintptr_t)addr) +
					user_pattern_size);
			rte_memcpy(addr, &missed_item,
				   missed * sizeof(*item));
			addr = (void *)(((uintptr_t)addr) +
				missed * sizeof(*item));
			rte_memcpy(addr, flow_items, n);
			addr = (void *)(((uintptr_t)addr) + n);
		}
		/* Go deeper. */
		if (!(node->node_flags & MLX5_EXPANSION_NODE_OPTIONAL) &&
				node->next) {
			next_node = mlx5_flow_expand_rss_skip_explicit(graph,
					node->next);
			if (stack_pos++ == MLX5_RSS_EXP_ELT_N) {
				rte_errno = E2BIG;
				return -rte_errno;
			}
			stack[stack_pos] = next_node;
		} else if (*(next_node + 1)) {
			/* Follow up with the next possibility. */
			next_node = mlx5_flow_expand_rss_skip_explicit(graph,
					++next_node);
		} else if (!stack_pos) {
			/*
			 * Completing the traverse over the different paths.
			 * The next_node is advanced to the terminator.
			 */
			++next_node;
		} else {
			/* Move to the next path. */
			while (stack_pos) {
				next_node = stack[--stack_pos];
				next_node++;
				if (*next_node)
					break;
			}
			next_node = mlx5_flow_expand_rss_skip_explicit(graph,
					next_node);
			stack[stack_pos] = next_node;
		}
		node = next_node && *next_node ? &graph[*next_node] : NULL;
	};
	return lsize;
}

/**
 * Expand SQN flows into several possible flows according to the Tx queue
 * number
 *
 * @param[in] buf
 *   Buffer to store the result expansion.
 * @param[in] size
 *   Buffer size in bytes. If 0, @p buf can be NULL.
 * @param[in] pattern
 *   User flow pattern.
 * @param[in] sq_specs
 *   Buffer to store sq spec.
 *
 * @return
 *   0 for success and negative value for failure
 *
 */
static int
mlx5_flow_expand_sqn(struct mlx5_flow_expand_sqn *buf, size_t size,
		     const struct rte_flow_item *pattern,
		     struct mlx5_rte_flow_item_sq *sq_specs)
{
	const struct rte_flow_item *item;
	bool port_representor = false;
	size_t user_pattern_size = 0;
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	void *addr = NULL;
	uint16_t port_id;
	size_t lsize;
	int elt = 2;
	uint16_t i;

	buf->entries = 0;
	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR) {
			const struct rte_flow_item_ethdev *pid_v = item->spec;

			if (!pid_v)
				return 0;
			port_id = pid_v->port_id;
			port_representor = true;
		}
		user_pattern_size += sizeof(*item);
	}
	if (!port_representor)
		return 0;
	dev = &rte_eth_devices[port_id];
	priv = dev->data->dev_private;
	buf->entry[0].pattern = (void *)&buf->entry[priv->txqs_n];
	lsize = offsetof(struct mlx5_flow_expand_sqn, entry) +
		sizeof(buf->entry[0]) * priv->txqs_n;
	if (lsize + (user_pattern_size + sizeof(struct rte_flow_item) * elt) * priv->txqs_n > size)
		return -EINVAL;
	addr = buf->entry[0].pattern;
	for (i = 0; i != priv->txqs_n; ++i) {
		struct rte_flow_item pattern_add[] = {
			{
				.type = (enum rte_flow_item_type)
					MLX5_RTE_FLOW_ITEM_TYPE_SQ,
				.spec = &sq_specs[i],
			},
			{
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		};
		struct mlx5_txq_ctrl *txq = mlx5_txq_get(dev, i);

		if (txq == NULL)
			return -EINVAL;
		buf->entry[i].pattern = addr;
		sq_specs[i].queue = mlx5_txq_get_sqn(txq);
		mlx5_txq_release(dev, i);
		rte_memcpy(addr, pattern, user_pattern_size);
		addr = (void *)(((uintptr_t)addr) + user_pattern_size);
		rte_memcpy(addr, pattern_add, sizeof(struct rte_flow_item) * elt);
		addr = (void *)(((uintptr_t)addr) + sizeof(struct rte_flow_item) * elt);
		buf->entries++;
	}
	return 0;
}

enum mlx5_expansion {
	MLX5_EXPANSION_ROOT,
	MLX5_EXPANSION_ROOT_OUTER,
	MLX5_EXPANSION_OUTER_ETH,
	MLX5_EXPANSION_OUTER_VLAN,
	MLX5_EXPANSION_OUTER_IPV4,
	MLX5_EXPANSION_OUTER_IPV4_UDP,
	MLX5_EXPANSION_OUTER_IPV4_TCP,
	MLX5_EXPANSION_OUTER_IPV4_ESP,
	MLX5_EXPANSION_OUTER_IPV4_ICMP,
	MLX5_EXPANSION_OUTER_IPV6,
	MLX5_EXPANSION_OUTER_IPV6_UDP,
	MLX5_EXPANSION_OUTER_IPV6_TCP,
	MLX5_EXPANSION_OUTER_IPV6_ESP,
	MLX5_EXPANSION_OUTER_IPV6_ICMP6,
	MLX5_EXPANSION_VXLAN,
	MLX5_EXPANSION_STD_VXLAN,
	MLX5_EXPANSION_L3_VXLAN,
	MLX5_EXPANSION_VXLAN_GPE,
	MLX5_EXPANSION_GRE,
	MLX5_EXPANSION_NVGRE,
	MLX5_EXPANSION_GRE_KEY,
	MLX5_EXPANSION_MPLS,
	MLX5_EXPANSION_ETH,
	MLX5_EXPANSION_VLAN,
	MLX5_EXPANSION_IPV4,
	MLX5_EXPANSION_IPV4_UDP,
	MLX5_EXPANSION_IPV4_TCP,
	MLX5_EXPANSION_IPV4_ESP,
	MLX5_EXPANSION_IPV4_ICMP,
	MLX5_EXPANSION_IPV6,
	MLX5_EXPANSION_IPV6_UDP,
	MLX5_EXPANSION_IPV6_TCP,
	MLX5_EXPANSION_IPV6_ESP,
	MLX5_EXPANSION_IPV6_ICMP6,
	MLX5_EXPANSION_IPV6_FRAG_EXT,
	MLX5_EXPANSION_GTP,
	MLX5_EXPANSION_GENEVE,
};

/** Supported expansion of items. */
static const struct mlx5_flow_expand_node mlx5_support_expansion[] = {
	[MLX5_EXPANSION_ROOT] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						  MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_ROOT_OUTER] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_ETH,
						  MLX5_EXPANSION_OUTER_IPV4,
						  MLX5_EXPANSION_OUTER_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_END,
	},
	[MLX5_EXPANSION_OUTER_ETH] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.rss_types = 0,
	},
	[MLX5_EXPANSION_OUTER_VLAN] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_OUTER_IPV4,
						  MLX5_EXPANSION_OUTER_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.node_flags = MLX5_EXPANSION_NODE_EXPLICIT,
	},
	[MLX5_EXPANSION_OUTER_IPV4] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT
			(MLX5_EXPANSION_OUTER_IPV4_UDP,
			 MLX5_EXPANSION_OUTER_IPV4_TCP,
			 MLX5_EXPANSION_OUTER_IPV4_ESP,
			 MLX5_EXPANSION_OUTER_IPV4_ICMP,
			 MLX5_EXPANSION_GRE,
			 MLX5_EXPANSION_NVGRE,
			 MLX5_EXPANSION_IPV4,
			 MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.rss_types = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	},
	[MLX5_EXPANSION_OUTER_IPV4_UDP] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VXLAN,
						  MLX5_EXPANSION_VXLAN_GPE,
						  MLX5_EXPANSION_MPLS,
						  MLX5_EXPANSION_GENEVE,
						  MLX5_EXPANSION_GTP),
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	},
	[MLX5_EXPANSION_OUTER_IPV4_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	},
	[MLX5_EXPANSION_OUTER_IPV4_ESP] = {
		.type = RTE_FLOW_ITEM_TYPE_ESP,
		.rss_types = RTE_ETH_RSS_ESP,
	},
	[MLX5_EXPANSION_OUTER_IPV4_ICMP] = {
		.type = RTE_FLOW_ITEM_TYPE_ICMP,
	},
	[MLX5_EXPANSION_OUTER_IPV6] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT
			(MLX5_EXPANSION_OUTER_IPV6_UDP,
			 MLX5_EXPANSION_OUTER_IPV6_TCP,
			 MLX5_EXPANSION_OUTER_IPV6_ESP,
			 MLX5_EXPANSION_OUTER_IPV6_ICMP6,
			 MLX5_EXPANSION_IPV4,
			 MLX5_EXPANSION_IPV6,
			 MLX5_EXPANSION_GRE,
			 MLX5_EXPANSION_NVGRE),
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.rss_types = RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
			RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	},
	[MLX5_EXPANSION_OUTER_IPV6_UDP] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VXLAN,
						  MLX5_EXPANSION_VXLAN_GPE,
						  MLX5_EXPANSION_MPLS,
						  MLX5_EXPANSION_GENEVE,
						  MLX5_EXPANSION_GTP),
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	},
	[MLX5_EXPANSION_OUTER_IPV6_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	},
	[MLX5_EXPANSION_OUTER_IPV6_ESP] = {
		.type = RTE_FLOW_ITEM_TYPE_ESP,
		.rss_types = RTE_ETH_RSS_ESP,
	},
	[MLX5_EXPANSION_OUTER_IPV6_ICMP6] = {
		.type = RTE_FLOW_ITEM_TYPE_ICMP6,
	},
	[MLX5_EXPANSION_VXLAN] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						  MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
	[MLX5_EXPANSION_STD_VXLAN] = {
			.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH),
					.type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
	[MLX5_EXPANSION_L3_VXLAN] = {
			.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
					MLX5_EXPANSION_IPV6),
					.type = RTE_FLOW_ITEM_TYPE_VXLAN,
	},
	[MLX5_EXPANSION_VXLAN_GPE] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						  MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VXLAN_GPE,
	},
	[MLX5_EXPANSION_GRE] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						  MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6,
						  MLX5_EXPANSION_GRE_KEY,
						  MLX5_EXPANSION_MPLS),
		.type = RTE_FLOW_ITEM_TYPE_GRE,
	},
	[MLX5_EXPANSION_GRE_KEY] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6,
						  MLX5_EXPANSION_MPLS),
		.type = RTE_FLOW_ITEM_TYPE_GRE_KEY,
		.node_flags = MLX5_EXPANSION_NODE_OPTIONAL,
	},
	[MLX5_EXPANSION_NVGRE] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH),
		.type = RTE_FLOW_ITEM_TYPE_NVGRE,
	},
	[MLX5_EXPANSION_MPLS] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6,
						  MLX5_EXPANSION_ETH),
		.type = RTE_FLOW_ITEM_TYPE_MPLS,
		.node_flags = MLX5_EXPANSION_NODE_OPTIONAL,
	},
	[MLX5_EXPANSION_ETH] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_VLAN),
		.type = RTE_FLOW_ITEM_TYPE_ETH,
	},
	[MLX5_EXPANSION_VLAN] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.node_flags = MLX5_EXPANSION_NODE_EXPLICIT,
	},
	[MLX5_EXPANSION_IPV4] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4_UDP,
						  MLX5_EXPANSION_IPV4_TCP,
						  MLX5_EXPANSION_IPV4_ESP,
						  MLX5_EXPANSION_IPV4_ICMP),
		.type = RTE_FLOW_ITEM_TYPE_IPV4,
		.rss_types = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 |
			RTE_ETH_RSS_NONFRAG_IPV4_OTHER,
	},
	[MLX5_EXPANSION_IPV4_UDP] = {
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV4_UDP,
	},
	[MLX5_EXPANSION_IPV4_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV4_TCP,
	},
	[MLX5_EXPANSION_IPV4_ESP] = {
		.type = RTE_FLOW_ITEM_TYPE_ESP,
		.rss_types = RTE_ETH_RSS_ESP,
	},
	[MLX5_EXPANSION_IPV4_ICMP] = {
		.type = RTE_FLOW_ITEM_TYPE_ICMP,
	},
	[MLX5_EXPANSION_IPV6] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV6_UDP,
						  MLX5_EXPANSION_IPV6_TCP,
						  MLX5_EXPANSION_IPV6_ESP,
						  MLX5_EXPANSION_IPV6_ICMP6,
						  MLX5_EXPANSION_IPV6_FRAG_EXT),
		.type = RTE_FLOW_ITEM_TYPE_IPV6,
		.rss_types = RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 |
			RTE_ETH_RSS_NONFRAG_IPV6_OTHER,
	},
	[MLX5_EXPANSION_IPV6_UDP] = {
		.type = RTE_FLOW_ITEM_TYPE_UDP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV6_UDP,
	},
	[MLX5_EXPANSION_IPV6_TCP] = {
		.type = RTE_FLOW_ITEM_TYPE_TCP,
		.rss_types = RTE_ETH_RSS_NONFRAG_IPV6_TCP,
	},
	[MLX5_EXPANSION_IPV6_ESP] = {
		.type = RTE_FLOW_ITEM_TYPE_ESP,
		.rss_types = RTE_ETH_RSS_ESP,
	},
	[MLX5_EXPANSION_IPV6_FRAG_EXT] = {
		.type = RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT,
	},
	[MLX5_EXPANSION_IPV6_ICMP6] = {
		.type = RTE_FLOW_ITEM_TYPE_ICMP6,
	},
	[MLX5_EXPANSION_GTP] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_GTP,
	},
	[MLX5_EXPANSION_GENEVE] = {
		.next = MLX5_FLOW_EXPAND_RSS_NEXT(MLX5_EXPANSION_ETH,
						  MLX5_EXPANSION_IPV4,
						  MLX5_EXPANSION_IPV6),
		.type = RTE_FLOW_ITEM_TYPE_GENEVE,
	},
};

static struct rte_flow_action_handle *
mlx5_action_handle_create(struct rte_eth_dev *dev,
			  const struct rte_flow_indir_action_conf *conf,
			  const struct rte_flow_action *action,
			  struct rte_flow_error *error);
static int mlx5_action_handle_destroy
				(struct rte_eth_dev *dev,
				 struct rte_flow_action_handle *handle,
				 struct rte_flow_error *error);
static int mlx5_action_handle_update
				(struct rte_eth_dev *dev,
				 struct rte_flow_action_handle *handle,
				 const void *update,
				 struct rte_flow_error *error);
static int mlx5_action_handle_query
				(struct rte_eth_dev *dev,
				 const struct rte_flow_action_handle *handle,
				 void *data,
				 struct rte_flow_error *error);
static int
mlx5_flow_tunnel_decap_set(struct rte_eth_dev *dev,
		    struct rte_flow_tunnel *app_tunnel,
		    struct rte_flow_action **actions,
		    uint32_t *num_of_actions,
		    struct rte_flow_error *error);
static int
mlx5_flow_tunnel_match(struct rte_eth_dev *dev,
		       struct rte_flow_tunnel *app_tunnel,
		       struct rte_flow_item **items,
		       uint32_t *num_of_items,
		       struct rte_flow_error *error);
static int
mlx5_flow_tunnel_item_release(struct rte_eth_dev *dev,
			      struct rte_flow_item *pmd_items,
			      uint32_t num_items, struct rte_flow_error *err);
static int
mlx5_flow_tunnel_action_release(struct rte_eth_dev *dev,
				struct rte_flow_action *pmd_actions,
				uint32_t num_actions,
				struct rte_flow_error *err);
static int
mlx5_flow_tunnel_get_restore_info(struct rte_eth_dev *dev,
				  struct rte_mbuf *m,
				  struct rte_flow_restore_info *info,
				  struct rte_flow_error *err);
static struct rte_flow_item_flex_handle *
mlx5_flow_flex_item_create(struct rte_eth_dev *dev,
			   const struct rte_flow_item_flex_conf *conf,
			   struct rte_flow_error *error);
static int
mlx5_flow_flex_item_release(struct rte_eth_dev *dev,
			    const struct rte_flow_item_flex_handle *handle,
			    struct rte_flow_error *error);
static int
mlx5_flow_info_get(struct rte_eth_dev *dev,
		   struct rte_flow_port_info *port_info,
		   struct rte_flow_queue_info *queue_info,
		   struct rte_flow_error *error);
static int
mlx5_flow_port_configure(struct rte_eth_dev *dev,
			 const struct rte_flow_port_attr *port_attr,
			 uint16_t nb_queue,
			 const struct rte_flow_queue_attr *queue_attr[],
			 struct rte_flow_error *err);

static struct rte_flow_pattern_template *
mlx5_flow_pattern_template_create(struct rte_eth_dev *dev,
		const struct rte_flow_pattern_template_attr *attr,
		const struct rte_flow_item items[],
		struct rte_flow_error *error);

static int
mlx5_flow_pattern_template_destroy(struct rte_eth_dev *dev,
				   struct rte_flow_pattern_template *template,
				   struct rte_flow_error *error);
static struct rte_flow_actions_template *
mlx5_flow_actions_template_create(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error);
static int
mlx5_flow_actions_template_destroy(struct rte_eth_dev *dev,
				   struct rte_flow_actions_template *template,
				   struct rte_flow_error *error);

static struct rte_flow_template_table *
mlx5_flow_table_create(struct rte_eth_dev *dev,
		       const struct rte_flow_template_table_attr *attr,
		       struct rte_flow_pattern_template *item_templates[],
		       uint8_t nb_item_templates,
		       struct rte_flow_actions_template *action_templates[],
		       uint8_t nb_action_templates,
		       struct rte_flow_error *error);
static int
mlx5_flow_table_destroy(struct rte_eth_dev *dev,
			struct rte_flow_template_table *table,
			struct rte_flow_error *error);
static int
mlx5_flow_group_set_miss_actions(struct rte_eth_dev *dev,
				 uint32_t group_id,
				 const struct rte_flow_group_attr *attr,
				 const struct rte_flow_action actions[],
				 struct rte_flow_error *error);

static int
mlx5_action_handle_query_update(struct rte_eth_dev *dev,
				struct rte_flow_action_handle *handle,
				const void *update, void *query,
				enum rte_flow_query_update_mode qu_mode,
				struct rte_flow_error *error);

static struct rte_flow_action_list_handle *
mlx5_action_list_handle_create(struct rte_eth_dev *dev,
			       const struct rte_flow_indir_action_conf *conf,
			       const struct rte_flow_action *actions,
			       struct rte_flow_error *error);

static int
mlx5_action_list_handle_destroy(struct rte_eth_dev *dev,
				struct rte_flow_action_list_handle *handle,
				struct rte_flow_error *error);

static int
mlx5_flow_action_list_handle_query_update(struct rte_eth_dev *dev,
					  const
					  struct rte_flow_action_list_handle *handle,
					  const void **update, void **query,
					  enum rte_flow_query_update_mode mode,
					  struct rte_flow_error *error);

static int
mlx5_flow_calc_table_hash(struct rte_eth_dev *dev,
			  const struct rte_flow_template_table *table,
			  const struct rte_flow_item pattern[],
			  uint8_t pattern_template_index,
			  uint32_t *hash, struct rte_flow_error *error);
static int
mlx5_flow_calc_encap_hash(struct rte_eth_dev *dev,
			  const struct rte_flow_item pattern[],
			  enum rte_flow_encap_hash_field dest_field,
			  uint8_t *hash,
			  struct rte_flow_error *error);

static int
mlx5_template_table_resize(struct rte_eth_dev *dev,
			   struct rte_flow_template_table *table,
			   uint32_t nb_rules, struct rte_flow_error *error);
static int
mlx5_flow_async_update_resized(struct rte_eth_dev *dev, uint32_t queue,
			       const struct rte_flow_op_attr *attr,
			       struct rte_flow *rule, void *user_data,
			       struct rte_flow_error *error);
static int
mlx5_table_resize_complete(struct rte_eth_dev *dev,
			   struct rte_flow_template_table *table,
			   struct rte_flow_error *error);

static const struct rte_flow_ops mlx5_flow_ops = {
	.validate = mlx5_flow_validate,
	.create = mlx5_flow_create,
	.destroy = mlx5_flow_destroy,
	.flush = mlx5_flow_flush,
	.isolate = mlx5_flow_isolate,
	.query = mlx5_flow_query,
	.dev_dump = mlx5_flow_dev_dump,
	.get_q_aged_flows = mlx5_flow_get_q_aged_flows,
	.get_aged_flows = mlx5_flow_get_aged_flows,
	.action_handle_create = mlx5_action_handle_create,
	.action_handle_destroy = mlx5_action_handle_destroy,
	.action_handle_update = mlx5_action_handle_update,
	.action_handle_query = mlx5_action_handle_query,
	.action_handle_query_update = mlx5_action_handle_query_update,
	.action_list_handle_create = mlx5_action_list_handle_create,
	.action_list_handle_destroy = mlx5_action_list_handle_destroy,
	.tunnel_decap_set = mlx5_flow_tunnel_decap_set,
	.tunnel_match = mlx5_flow_tunnel_match,
	.tunnel_action_decap_release = mlx5_flow_tunnel_action_release,
	.tunnel_item_release = mlx5_flow_tunnel_item_release,
	.get_restore_info = mlx5_flow_tunnel_get_restore_info,
	.flex_item_create = mlx5_flow_flex_item_create,
	.flex_item_release = mlx5_flow_flex_item_release,
	.info_get = mlx5_flow_info_get,
	.pick_transfer_proxy = mlx5_flow_pick_transfer_proxy,
	.configure = mlx5_flow_port_configure,
	.pattern_template_create = mlx5_flow_pattern_template_create,
	.pattern_template_destroy = mlx5_flow_pattern_template_destroy,
	.actions_template_create = mlx5_flow_actions_template_create,
	.actions_template_destroy = mlx5_flow_actions_template_destroy,
	.template_table_create = mlx5_flow_table_create,
	.template_table_destroy = mlx5_flow_table_destroy,
	.group_set_miss_actions = mlx5_flow_group_set_miss_actions,
	.action_list_handle_query_update =
		mlx5_flow_action_list_handle_query_update,
	.flow_calc_table_hash = mlx5_flow_calc_table_hash,
	.flow_calc_encap_hash = mlx5_flow_calc_encap_hash,
	.flow_template_table_resize = mlx5_template_table_resize,
	.flow_update_resized = mlx5_flow_async_update_resized,
	.flow_template_table_resize_complete = mlx5_table_resize_complete,
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
	struct mlx5_sh_config *config = &priv->sh->config;
	struct mlx5_dev_registers *reg = &priv->sh->registers;
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
		case MLX5_XMETA_MODE_META32_HWS:
			return REG_C_1;
		}
		break;
	case MLX5_METADATA_TX:
		if (config->dv_flow_en == 2 && config->dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS) {
			return REG_C_1;
		} else {
			return REG_A;
		}
	case MLX5_METADATA_FDB:
		switch (config->dv_xmeta_en) {
		case MLX5_XMETA_MODE_LEGACY:
			return REG_NON;
		case MLX5_XMETA_MODE_META16:
			return REG_C_0;
		case MLX5_XMETA_MODE_META32:
			return REG_C_1;
		case MLX5_XMETA_MODE_META32_HWS:
			return REG_C_1;
		}
		break;
	case MLX5_FLOW_MARK:
		switch (config->dv_xmeta_en) {
		case MLX5_XMETA_MODE_LEGACY:
		case MLX5_XMETA_MODE_META32_HWS:
			return REG_NON;
		case MLX5_XMETA_MODE_META16:
			return REG_C_1;
		case MLX5_XMETA_MODE_META32:
			return REG_C_0;
		}
		break;
	case MLX5_MTR_ID:
		/*
		 * If meter color and meter id share one register, flow match
		 * should use the meter color register for match.
		 */
		if (priv->mtr_reg_share)
			return reg->aso_reg;
		else
			return reg->aso_reg != REG_C_2 ? REG_C_2 :
			       REG_C_3;
	case MLX5_MTR_COLOR:
	case MLX5_ASO_FLOW_HIT:
	case MLX5_ASO_CONNTRACK:
	case MLX5_SAMPLE_ID:
		/* All features use the same REG_C. */
		MLX5_ASSERT(reg->aso_reg != REG_NON);
		return reg->aso_reg;
	case MLX5_COPY_MARK:
		/*
		 * Metadata COPY_MARK register using is in meter suffix sub
		 * flow while with meter. It's safe to share the same register.
		 */
		return reg->aso_reg != REG_C_2 ? REG_C_2 : REG_C_3;
	case MLX5_APP_TAG:
		/*
		 * If meter is enable, it will engage the register for color
		 * match and flow match. If meter color match is not using the
		 * REG_C_2, need to skip the REG_C_x be used by meter color
		 * match.
		 * If meter is disable, free to use all available registers.
		 */
		start_reg = reg->aso_reg != REG_C_2 ? REG_C_2 :
			    (priv->mtr_reg_share ? REG_C_3 : REG_C_4);
		skip_mtr_reg = !!(priv->mtr_en && start_reg == REG_C_2);
		if (id > (uint32_t)(REG_C_7 - start_reg))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "invalid tag id");
		if (priv->sh->flow_mreg_c[id + start_reg - REG_C_0] == REG_NON)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "unsupported tag id");
		/*
		 * This case means meter is using the REG_C_x great than 2.
		 * Take care not to conflict with meter color REG_C_x.
		 * If the available index REG_C_y >= REG_C_x, skip the
		 * color register.
		 */
		if (skip_mtr_reg && priv->sh->flow_mreg_c
		    [id + start_reg - REG_C_0] >= reg->aso_reg) {
			if (id >= (uint32_t)(REG_C_7 - start_reg))
				return rte_flow_error_set(error, EINVAL,
						       RTE_FLOW_ERROR_TYPE_ITEM,
							NULL, "invalid tag id");
			if (priv->sh->flow_mreg_c
			    [id + 1 + start_reg - REG_C_0] != REG_NON)
				return priv->sh->flow_mreg_c
					       [id + 1 + start_reg - REG_C_0];
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  NULL, "unsupported tag id");
		}
		return priv->sh->flow_mreg_c[id + start_reg - REG_C_0];
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

	/*
	 * Having available reg_c can be regarded inclusively as supporting
	 * extensive flow metadata register, which could mean,
	 * - metadata register copy action by modify header.
	 * - 16 modify header actions is supported.
	 * - reg_c's are preserved across different domain (FDB and NIC) on
	 *   packet loopback by flow lookup miss.
	 */
	return priv->sh->flow_mreg_c[2] != REG_NON;
}

/**
 * Get the lowest priority.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attributes
 *   Pointer to device flow rule attributes.
 *
 * @return
 *   The value of lowest priority of flow.
 */
uint32_t
mlx5_get_lowest_priority(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!attr->group && !(attr->transfer && priv->fdb_def_rule))
		return priv->sh->flow_max_priority - 2;
	return MLX5_NON_ROOT_FLOW_MAX_PRIO - 1;
}

/**
 * Calculate matcher priority of the flow.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] attr
 *   Pointer to device flow rule attributes.
 * @param[in] subpriority
 *   The priority based on the items.
 * @param[in] external
 *   Flow is user flow.
 * @return
 *   The matcher priority of the flow.
 */
uint16_t
mlx5_get_matcher_priority(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  uint32_t subpriority, bool external)
{
	uint16_t priority = (uint16_t)attr->priority;
	struct mlx5_priv *priv = dev->data->dev_private;

	/* NIC root rules */
	if (!attr->group && !attr->transfer) {
		if (attr->priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR)
			priority = priv->sh->flow_max_priority - 1;
		return mlx5_os_flow_adjust_priority(dev, priority, subpriority);
	/* FDB root rules */
	} else if (attr->transfer && (!external || !priv->fdb_def_rule) &&
		   attr->group == 0 &&
		   attr->priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR) {
		return (priv->sh->flow_max_priority - 1) * 3;
	}
	if (attr->priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR)
		priority = MLX5_NON_ROOT_FLOW_MAX_PRIO;
	return priority * 3 + subpriority;
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
 * @param[in] range_accepted
 *   True if range of values is accepted for specific fields, false otherwise.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_item_acceptable(const struct rte_eth_dev *dev,
			  const struct rte_flow_item *item,
			  const uint8_t *mask,
			  const uint8_t *nic_mask,
			  unsigned int size,
			  bool range_accepted,
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
	if (mlx5_hws_active(dev))
		return 0;
	if (!item->spec && (item->mask || item->last))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "mask/last without a spec is not"
					  " supported");
	if (item->spec && item->last && !range_accepted) {
		uint8_t *spec = alloca(size);
		uint8_t *last = alloca(size);
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
 *   RTE_ETH_RSS_* types.
 * @param[in] hash_fields
 *   Item hash fields.
 *
 * @return
 *   The hash fields that should be used.
 */
uint64_t
mlx5_flow_hashfields_adjust(struct mlx5_flow_rss_desc *rss_desc,
			    int tunnel __rte_unused, uint64_t layer_types,
			    uint64_t hash_fields)
{
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	int rss_request_inner = rss_desc->level >= 2;

	/* Check RSS hash level for tunnel. */
	if (tunnel && rss_request_inner)
		hash_fields |= IBV_RX_HASH_INNER;
	else if (tunnel || rss_request_inner)
		return 0;
#endif
	/* Check if requested layer matches RSS hash fields. */
	if (!(rss_desc->types & layer_types))
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
 * Set the Rx queue flags (Mark/Flag and Tunnel Ptypes) according to the device
 * flow.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] dev_handle
 *   Pointer to device flow handle structure.
 */
void
flow_drv_rxq_flags_set(struct rte_eth_dev *dev,
		       struct mlx5_flow_handle *dev_handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const int tunnel = !!(dev_handle->layers & MLX5_FLOW_LAYER_TUNNEL);
	struct mlx5_ind_table_obj *ind_tbl = NULL;
	unsigned int i;

	if (dev_handle->fate_action == MLX5_FLOW_FATE_QUEUE) {
		struct mlx5_hrxq *hrxq;

		hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ],
			      dev_handle->rix_hrxq);
		if (hrxq)
			ind_tbl = hrxq->ind_table;
	} else if (dev_handle->fate_action == MLX5_FLOW_FATE_SHARED_RSS) {
		struct mlx5_shared_action_rss *shared_rss;

		shared_rss = mlx5_ipool_get
			(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
			 dev_handle->rix_srss);
		if (shared_rss)
			ind_tbl = shared_rss->ind_tbl;
	}
	if (!ind_tbl)
		return;
	for (i = 0; i != ind_tbl->queues_n; ++i) {
		int idx = ind_tbl->queues[i];
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (mlx5_is_external_rxq(dev, idx))
			continue;
		rxq_ctrl = mlx5_rxq_ctrl_get(dev, idx);
		MLX5_ASSERT(rxq_ctrl != NULL);
		if (rxq_ctrl == NULL)
			continue;
		/*
		 * To support metadata register copy on Tx loopback,
		 * this must be always enabled (metadata may arive
		 * from other port - not from local flows only.
		 */
		if (tunnel) {
			unsigned int j;

			/* Increase the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel &
				     dev_handle->layers) ==
				    tunnels_info[j].tunnel) {
					rxq_ctrl->flow_tunnels_n[j]++;
					break;
				}
			}
			flow_rxq_tunnel_ptype_update(rxq_ctrl);
		}
	}
}

void
mlx5_flow_rxq_mark_flag_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_ctrl *rxq_ctrl;
	uint16_t port_id;

	if (priv->sh->shared_mark_enabled)
		return;
	if (priv->master || priv->representor) {
		MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
			struct mlx5_priv *opriv =
				rte_eth_devices[port_id].data->dev_private;

			if (!opriv ||
			    opriv->sh != priv->sh ||
			    opriv->domain_id != priv->domain_id ||
			    opriv->mark_enabled)
				continue;
			LIST_FOREACH(rxq_ctrl, &opriv->rxqsctrl, next) {
				rxq_ctrl->rxq.mark = 1;
			}
			LIST_FOREACH(rxq_ctrl, &opriv->sh->shared_rxqs, next) {
				rxq_ctrl->rxq.mark = 1;
			}
			opriv->mark_enabled = 1;
		}
	} else {
		LIST_FOREACH(rxq_ctrl, &priv->rxqsctrl, next) {
			rxq_ctrl->rxq.mark = 1;
		}
		LIST_FOREACH(rxq_ctrl, &priv->sh->shared_rxqs, next) {
			rxq_ctrl->rxq.mark = 1;
		}
		priv->mark_enabled = 1;
	}
	priv->sh->shared_mark_enabled = 1;
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
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t handle_idx;
	struct mlx5_flow_handle *dev_handle;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	MLX5_ASSERT(wks);
	if (wks->mark)
		mlx5_flow_rxq_mark_flag_set(dev);
	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       handle_idx, dev_handle, next)
		flow_drv_rxq_flags_set(dev, dev_handle);
}

/**
 * Clear the Rx queue flags (Mark/Flag and Tunnel Ptype) associated with the
 * device flow if no other flow uses it with the same kind of request.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] dev_handle
 *   Pointer to the device flow handle structure.
 */
static void
flow_drv_rxq_flags_trim(struct rte_eth_dev *dev,
			struct mlx5_flow_handle *dev_handle)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const int tunnel = !!(dev_handle->layers & MLX5_FLOW_LAYER_TUNNEL);
	struct mlx5_ind_table_obj *ind_tbl = NULL;
	unsigned int i;

	if (dev_handle->fate_action == MLX5_FLOW_FATE_QUEUE) {
		struct mlx5_hrxq *hrxq;

		hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ],
			      dev_handle->rix_hrxq);
		if (hrxq)
			ind_tbl = hrxq->ind_table;
	} else if (dev_handle->fate_action == MLX5_FLOW_FATE_SHARED_RSS) {
		struct mlx5_shared_action_rss *shared_rss;

		shared_rss = mlx5_ipool_get
			(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
			 dev_handle->rix_srss);
		if (shared_rss)
			ind_tbl = shared_rss->ind_tbl;
	}
	if (!ind_tbl)
		return;
	MLX5_ASSERT(dev->data->dev_started);
	for (i = 0; i != ind_tbl->queues_n; ++i) {
		int idx = ind_tbl->queues[i];
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (mlx5_is_external_rxq(dev, idx))
			continue;
		rxq_ctrl = mlx5_rxq_ctrl_get(dev, idx);
		MLX5_ASSERT(rxq_ctrl != NULL);
		if (rxq_ctrl == NULL)
			continue;
		if (tunnel) {
			unsigned int j;

			/* Decrease the counter matching the flow. */
			for (j = 0; j != MLX5_FLOW_TUNNEL; ++j) {
				if ((tunnels_info[j].tunnel &
				     dev_handle->layers) ==
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
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t handle_idx;
	struct mlx5_flow_handle *dev_handle;

	SILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW], flow->dev_handles,
		       handle_idx, dev_handle, next)
		flow_drv_rxq_flags_trim(dev, dev_handle);
}

/**
 * Clear the Mark/Flag and Tunnel ptype information in all Rx queues.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_rxq_flags_clear(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, i);
		unsigned int j;

		if (rxq == NULL || rxq->ctrl == NULL)
			continue;
		rxq->ctrl->rxq.mark = 0;
		for (j = 0; j != MLX5_FLOW_TUNNEL; ++j)
			rxq->ctrl->flow_tunnels_n[j] = 0;
		rxq->ctrl->rxq.tunnel = 0;
	}
	priv->mark_enabled = 0;
	priv->sh->shared_mark_enabled = 0;
}

static uint64_t mlx5_restore_info_dynflag;

int
mlx5_flow_rx_metadata_negotiate(struct rte_eth_dev *dev, uint64_t *features)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint64_t supported = 0;

	if (!is_tunnel_offload_active(dev)) {
		supported |= RTE_ETH_RX_METADATA_USER_FLAG;
		supported |= RTE_ETH_RX_METADATA_USER_MARK;
		if ((*features & RTE_ETH_RX_METADATA_TUNNEL_ID) != 0) {
			DRV_LOG(DEBUG,
				"tunnel offload was not activated, consider setting dv_xmeta_en=%d",
				MLX5_XMETA_MODE_MISS_INFO);
		}
	} else {
		supported |= RTE_ETH_RX_METADATA_TUNNEL_ID;
		if ((*features & RTE_ETH_RX_METADATA_TUNNEL_ID) != 0 &&
				mlx5_restore_info_dynflag == 0)
			mlx5_restore_info_dynflag = rte_flow_restore_info_dynflag();
	}

	if (((*features & supported) & RTE_ETH_RX_METADATA_TUNNEL_ID) != 0)
		priv->tunnel_enabled = 1;
	else
		priv->tunnel_enabled = 0;

	*features &= supported;
	return 0;
}

/**
 * Set the Rx queue dynamic metadata (mask and offset) for a flow
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 */
void
mlx5_flow_rxq_dynf_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint64_t mark_flag = RTE_MBUF_F_RX_FDIR_ID;
	unsigned int i;

	if (priv->tunnel_enabled)
		mark_flag |= mlx5_restore_info_dynflag;

	for (i = 0; i != priv->rxqs_n; ++i) {
		struct mlx5_rxq_priv *rxq = mlx5_rxq_get(dev, i);
		struct mlx5_rxq_data *data;

		if (rxq == NULL || rxq->ctrl == NULL)
			continue;
		data = &rxq->ctrl->rxq;
		if (!data->shared || !rxq->ctrl->started) {
			if (!rte_flow_dynf_metadata_avail()) {
				data->dynf_meta = 0;
				data->flow_meta_mask = 0;
				data->flow_meta_offset = -1;
				data->flow_meta_port_mask = 0;
			} else {
				data->dynf_meta = 1;
				data->flow_meta_mask = rte_flow_dynf_metadata_mask;
				data->flow_meta_offset = rte_flow_dynf_metadata_offs;
				data->flow_meta_port_mask = priv->sh->dv_meta_mask;
			}
		}
		data->mark_flag = mark_flag;
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
mlx5_flow_validate_action_mark(__rte_unused struct rte_eth_dev *dev,
			       const struct rte_flow_action *action,
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
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] is_root
 *   True if flow is validated for root table. False otherwise.
 * @param[in] attr
 *   Attributes of flow that includes this action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_drop(struct rte_eth_dev *dev,
			       bool is_root,
			       const struct rte_flow_attr *attr,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->config.dv_flow_en == 0 && attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "drop action not supported for "
					  "egress");
	if (priv->sh->config.dv_flow_en == 1 && is_root && (attr->egress || attr->transfer) &&
	    !priv->sh->dr_root_drop_action_en) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "drop action not supported for "
					  "egress and transfer on group 0");
	}
	return 0;
}

/*
 * Check if a queue specified in the queue action is valid.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action
 *   Pointer to the queue action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_target_queue(struct rte_eth_dev *dev,
				const struct rte_flow_action *action,
				struct rte_flow_error *error)
{
	const struct rte_flow_action_queue *queue = action->conf;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (mlx5_is_external_rxq(dev, queue->index))
		return 0;
	if (!priv->rxqs_n)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No Rx queues configured");
	if (queue->index >= priv->rxqs_n)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &queue->index,
					  "queue index out of range");
	if (mlx5_rxq_get(dev, queue->index) == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &queue->index,
					  "queue is not configured");
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
	const struct rte_flow_action_queue *queue = action->conf;

	if (!queue)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "no QUEUE action configuration");
	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions in"
					  " same flow");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "queue action not supported for egress.");
	return mlx5_flow_validate_target_queue(dev, action, error);
}

/**
 * Validate queue numbers for device RSS.
 *
 * @param[in] dev
 *   Configured device.
 * @param[in] queues
 *   Array of queue numbers.
 * @param[in] queues_n
 *   Size of the @p queues array.
 * @param[out] error
 *   On error, filled with a textual error description.
 * @param[out] queue_idx
 *   On error, filled with an offending queue index in @p queues array.
 *
 * @return
 *   0 on success, a negative errno code on error.
 */
static int
mlx5_validate_rss_queues(struct rte_eth_dev *dev,
			 const uint16_t *queues, uint32_t queues_n,
			 const char **error, uint32_t *queue_idx)
{
	const struct mlx5_priv *priv = dev->data->dev_private;
	bool is_hairpin = false;
	bool is_ext_rss = false;
	uint32_t i;

	for (i = 0; i != queues_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl;

		if (mlx5_is_external_rxq(dev, queues[0])) {
			is_ext_rss = true;
			continue;
		}
		if (is_ext_rss) {
			*error = "Combining external and regular RSS queues is not supported";
			*queue_idx = i;
			return -ENOTSUP;
		}
		if (queues[i] >= priv->rxqs_n) {
			*error = "queue index out of range";
			*queue_idx = i;
			return -EINVAL;
		}
		rxq_ctrl = mlx5_rxq_ctrl_get(dev, queues[i]);
		if (rxq_ctrl == NULL) {
			*error =  "queue is not configured";
			*queue_idx = i;
			return -EINVAL;
		}
		if (i == 0 && rxq_ctrl->is_hairpin)
			is_hairpin = true;
		if (is_hairpin != rxq_ctrl->is_hairpin) {
			*error = "combining hairpin and regular RSS queues is not supported";
			*queue_idx = i;
			return -ENOTSUP;
		}
	}
	return 0;
}

/*
 * Validate the rss action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] action
 *   Pointer to the queue action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_validate_action_rss(struct rte_eth_dev *dev,
			 const struct rte_flow_action *action,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_rss *rss = action->conf;
	int ret;
	const char *message;
	uint32_t queue_idx;

	if (!rss)
		return rte_flow_error_set
			(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
			 action, "no RSS action configuration");
	if (rss->func == RTE_ETH_HASH_FUNCTION_SYMMETRIC_TOEPLITZ) {
		DRV_LOG(WARNING, "port %u symmetric RSS supported with SORT",
			dev->data->port_id);
	} else if (rss->func != RTE_ETH_HASH_FUNCTION_DEFAULT &&
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
	if (rss->queue_num > priv->sh->dev_cap.ind_table_max_size)
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
	if ((rss->types & (RTE_ETH_RSS_L3_SRC_ONLY | RTE_ETH_RSS_L3_DST_ONLY)) &&
	    !(rss->types & RTE_ETH_RSS_IP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "L3 partial RSS requested but L3 RSS"
					  " type not specified");
	if ((rss->types & (RTE_ETH_RSS_L4_SRC_ONLY | RTE_ETH_RSS_L4_DST_ONLY)) &&
	    !(rss->types & (RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "L4 partial RSS requested but L4 RSS"
					  " type not specified");
	if (!priv->rxqs_n && priv->ext_rxqs == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No Rx queues configured");
	if (!rss->queue_num)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "No queues configured");
	ret = mlx5_validate_rss_queues(dev, rss->queue, rss->queue_num,
				       &message, &queue_idx);
	if (ret != 0) {
		return rte_flow_error_set(error, -ret,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  &rss->queue[queue_idx], message);
	}
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
	const struct rte_flow_action_rss *rss = action->conf;
	int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	int ret;

	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions"
					  " in same flow");
	ret = mlx5_validate_action_rss(dev, action, error);
	if (ret)
		return ret;
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "rss action not supported for "
					  "egress");
	if (rss->level > 1 && !tunnel)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "inner RSS is not supported for "
					  "non-tunnel flows");
	if ((item_flags & MLX5_FLOW_LAYER_ECPRI) &&
	    !(item_flags & MLX5_FLOW_LAYER_INNER_L4_UDP)) {
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "RSS on eCPRI is not supported now");
	}
	if ((item_flags & MLX5_FLOW_LAYER_MPLS) &&
	    !(item_flags &
	      (MLX5_FLOW_LAYER_INNER_L2 | MLX5_FLOW_LAYER_INNER_L3)) &&
	    rss->level > 1)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
					  "MPLS inner RSS needs to specify inner L2/L3 items after MPLS in pattern");
	return 0;
}

/*
 * Validate the default miss action.
 *
 * @param[in] action_flags
 *   Bit-fields that holds the actions detected until now.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_action_default_miss(uint64_t action_flags,
				const struct rte_flow_attr *attr,
				struct rte_flow_error *error)
{
	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "can't have 2 fate actions in"
					  " same flow");
	if (attr->egress)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
					  "default miss action not supported "
					  "for egress");
	if (attr->group)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP, NULL,
					  "only group 0 is supported");
	if (attr->transfer)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL, "transfer is not supported");
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

/*
 * Validate the ASO CT action.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] conntrack
 *   Pointer to the CT action profile.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_validate_action_ct(struct rte_eth_dev *dev,
			const struct rte_flow_action_conntrack *conntrack,
			struct rte_flow_error *error)
{
	RTE_SET_USED(dev);

	if (conntrack->state > RTE_FLOW_CONNTRACK_STATE_TIME_WAIT)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Invalid CT state");
	if (conntrack->last_index > RTE_FLOW_CONNTRACK_FLAG_RST)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Invalid last TCP packet flag");
	return 0;
}

/**
 * Validate the level value for modify field action.
 *
 * @param[in] data
 *   Pointer to the rte_flow_field_data structure either src or dst.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
flow_validate_modify_field_level(const struct rte_flow_field_data *data,
				 struct rte_flow_error *error)
{
	if (data->level == 0 || data->field == RTE_FLOW_FIELD_FLEX_ITEM)
		return 0;
	if (data->field != RTE_FLOW_FIELD_TAG &&
	    data->field != (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG) {
		if (data->level > 1)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "inner header fields modification is not supported");
		return 0;
	}
	if (data->tag_index != 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "tag array can be provided using 'level' or 'tag_index' fields, not both");
	/*
	 * The tag array for RTE_FLOW_FIELD_TAG type is provided using
	 * 'tag_index' field. In old API, it was provided using 'level' field
	 * and it is still supported for backwards compatibility.
	 */
	DRV_LOG(DEBUG, "tag array provided in 'level' field instead of 'tag_index' field.");
	return 0;
}

/**
 * Validate ICMP6 item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] ext_vlan_sup
 *   Whether extended VLAN features are supported or not.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_icmp6(const struct rte_eth_dev *dev,
			      const struct rte_flow_item *item,
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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "IPv6 is mandatory to filter on ICMP6");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_icmp6_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_icmp6_mask,
		 sizeof(struct rte_flow_item_icmp6),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate ICMP6 echo request/reply item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] ext_vlan_sup
 *   Whether extended VLAN features are supported or not.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_icmp6_echo(const struct rte_eth_dev *dev,
				   const struct rte_flow_item *item,
				   uint64_t item_flags,
				   uint8_t target_protocol,
				   struct rte_flow_error *error)
{
	const struct rte_flow_item_icmp6_echo *mask = item->mask;
	const struct rte_flow_item_icmp6_echo nic_mask = {
		.hdr.base.type = 0xff,
		.hdr.base.code = 0xff,
		.hdr.identifier = RTE_BE16(0xffff),
		.hdr.sequence = RTE_BE16(0xffff),
	};
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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "IPv6 is mandatory to filter on ICMP6");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &nic_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_icmp6_echo),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
mlx5_flow_validate_item_icmp(const struct rte_eth_dev *dev,
			     const struct rte_flow_item *item,
			     uint64_t item_flags,
			     uint8_t target_protocol,
			     struct rte_flow_error *error)
{
	const struct rte_flow_item_icmp *mask = item->mask;
	const struct rte_flow_item_icmp nic_mask = {
		.hdr.icmp_type = 0xff,
		.hdr.icmp_code = 0xff,
		.hdr.icmp_ident = RTE_BE16(0xffff),
		.hdr.icmp_seq_nb = RTE_BE16(0xffff),
	};
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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "IPv4 is mandatory to filter on ICMP");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &nic_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_icmp),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
mlx5_flow_validate_item_eth(const struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
			    uint64_t item_flags, bool ext_vlan_sup,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_eth *mask = item->mask;
	const struct rte_flow_item_eth nic_mask = {
		.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.hdr.src_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.hdr.ether_type = RTE_BE16(0xffff),
		.has_vlan = ext_vlan_sup ? 1 : 0,
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
	if (item_flags & MLX5_FLOW_LAYER_GTP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "L2 layer should not follow GTP");
	if (!mask)
		mask = &rte_flow_item_eth_mask;
	ret = mlx5_flow_item_acceptable(dev, item, (const uint8_t *)mask,
					(const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_eth),
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
		.hdr.vlan_tci = RTE_BE16(UINT16_MAX),
		.hdr.eth_proto = RTE_BE16(UINT16_MAX),
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
	ret = mlx5_flow_item_acceptable(dev, item, (const uint8_t *)mask,
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
	if (spec) {
		vlan_tag = spec->hdr.vlan_tci;
		vlan_tag &= mask->hdr.vlan_tci;
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
 * @param[in] last_item
 *   Previous validated item in the pattern items.
 * @param[in] ether_type
 *   Type in the ethernet layer header (including dot1q).
 * @param[in] acc_mask
 *   Acceptable mask, if NULL default internal default mask
 *   will be used to check whether item fields are supported.
 * @param[in] range_accepted
 *   True if range of values is accepted for specific fields, false otherwise.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_ipv4(const struct rte_eth_dev *dev,
			     const struct rte_flow_item *item,
			     uint64_t item_flags,
			     uint64_t last_item,
			     uint16_t ether_type,
			     const struct rte_flow_item_ipv4 *acc_mask,
			     bool range_accepted,
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
	ret = mlx5_flow_item_acceptable(dev, item, (const uint8_t *)mask,
					acc_mask ? (const uint8_t *)acc_mask
						 : (const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_ipv4),
					range_accepted, error);
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
 * @param[in] last_item
 *   Previous validated item in the pattern items.
 * @param[in] ether_type
 *   Type in the ethernet layer header (including dot1q).
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
mlx5_flow_validate_item_ipv6(const struct rte_eth_dev *dev,
			     const struct rte_flow_item *item,
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
			.src_addr = RTE_IPV6_MASK_FULL,
			.dst_addr = RTE_IPV6_MASK_FULL,
			.vtc_flow = RTE_BE32(0xffffffff),
			.proto = 0xff,
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
	if (mask && mask->hdr.proto == UINT8_MAX && spec)
		next_proto = spec->hdr.proto;
	if (item_flags & MLX5_FLOW_LAYER_IPIP) {
		if (next_proto == IPPROTO_IPIP || next_proto == IPPROTO_IPV6)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "multiple tunnel "
						  "not supported");
	}
	if (next_proto == IPPROTO_HOPOPTS  ||
	    next_proto == IPPROTO_ROUTING  ||
	    next_proto == IPPROTO_FRAGMENT ||
	    next_proto == IPPROTO_AH	   ||
	    next_proto == IPPROTO_DSTOPTS  ||
	    (!mlx5_hws_active(dev) && next_proto == IPPROTO_ESP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "IPv6 proto (next header) should "
					  "not be set as extension header");
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
	ret = mlx5_flow_item_acceptable(dev, item, (const uint8_t *)mask,
					acc_mask ? (const uint8_t *)acc_mask
						 : (const uint8_t *)&nic_mask,
					sizeof(struct rte_flow_item_ipv6),
					MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
mlx5_flow_validate_item_udp(const struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
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

	if (!mlx5_hws_active(dev)) {
		if (target_protocol != 0xff && target_protocol != IPPROTO_UDP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "protocol filtering not compatible with UDP layer");
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "L3 is mandatory to filter on L4");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_udp_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&rte_flow_item_udp_mask,
		 sizeof(struct rte_flow_item_udp), MLX5_ITEM_RANGE_NOT_ACCEPTED,
		 error);
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
mlx5_flow_validate_item_tcp(const struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
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
	if (!mlx5_hws_active(dev)) {
		if (target_protocol != 0xff && target_protocol != IPPROTO_TCP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "protocol filtering not compatible with TCP layer");
		if (!(item_flags & l3m))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "L3 is mandatory to filter on L4");
	}
	if (item_flags & l4m)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L4 layers not supported");
	if (!mask)
		mask = &rte_flow_item_tcp_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)flow_mask,
		 sizeof(struct rte_flow_item_tcp), MLX5_ITEM_RANGE_NOT_ACCEPTED,
		 error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate VXLAN item.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] udp_dport
 *   UDP destination port
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param root
 *   Whether action is on root table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_vxlan(struct rte_eth_dev *dev,
			      uint16_t udp_dport,
			      const struct rte_flow_item *item,
			      uint64_t item_flags,
			      bool root,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_vxlan *spec = item->spec;
	const struct rte_flow_item_vxlan *mask = item->mask;
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	union vni {
		uint32_t vlan_id;
		uint8_t vni[4];
	} id = { .vlan_id = 0, };
	const struct rte_flow_item_vxlan nic_mask = {
		.hdr.vni =  { 0xff, 0xff, 0xff },
		.hdr.rsvd1 = 0xff,
	};
	const struct rte_flow_item_vxlan *valid_mask;

	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple tunnel layers not"
					  " supported");
	/* HWS can match entire VXLAN, VXLAN-GBP and VXLAN-GPE headers */
	if (mlx5_hws_active(dev))
		return 0;
	valid_mask = &rte_flow_item_vxlan_mask;
	/*
	 * Verify only UDPv4 is present as defined in
	 * https://tools.ietf.org/html/rfc7348
	 */
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM,
					  item, "no outer UDP layer found");
	if (!(item_flags & MLX5_FLOW_LAYER_OUTER))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "VXLAN tunnel must be fully defined");
	if (!mask)
		mask = &rte_flow_item_vxlan_mask;

	if (priv->sh->steering_format_version !=
	    MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5 ||
	    !udp_dport || udp_dport == MLX5_UDP_PORT_VXLAN) {
		/* non-root table */
		if (!root && priv->sh->misc5_cap)
			valid_mask = &nic_mask;
		/* Group zero in NIC domain */
		if (!root && priv->sh->tunnel_header_0_1)
			valid_mask = &nic_mask;
	}
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)valid_mask,
		 sizeof(struct rte_flow_item_vxlan),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&id.vni[1], spec->hdr.vni, 3);
		memcpy(&id.vni[1], mask->hdr.vni, 3);
	}
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

	struct rte_flow_item_vxlan_gpe nic_mask = {
		.vni =  { 0xff, 0xff, 0xff },
		.protocol = 0xff,
		.flags = 0xff,
	};

	if (!priv->sh->config.l3_vxlan_en)
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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "no outer UDP layer found");
	}
	if (!mask)
		mask = &rte_flow_item_vxlan_gpe_mask;
	if (mlx5_hws_active(dev) ||
	    (priv->sh->misc5_cap && priv->sh->tunnel_header_0_1)) {
		nic_mask.rsvd0[0] = 0xff;
		nic_mask.rsvd0[1] = 0xff;
		nic_mask.rsvd1 = 0xff;
	}
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_vxlan_gpe),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	if (spec) {
		memcpy(&id.vni[1], spec->hdr.vni, 3);
		memcpy(&id.vni[1], mask->hdr.vni, 3);
	}
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
mlx5_flow_validate_item_gre_key(const struct rte_eth_dev *dev,
				const struct rte_flow_item *item,
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
	gre_mask = gre_item ? gre_item->mask : NULL;
	if (!gre_mask)
		gre_mask = &rte_flow_item_gre_mask;
	gre_spec = gre_item ? gre_item->spec : NULL;
	if (gre_spec && (gre_mask->c_rsvd0_ver & RTE_BE16(0x2000)) &&
			 !(gre_spec->c_rsvd0_ver & RTE_BE16(0x2000)))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Key bit must be on");

	if (!mask)
		mask = &gre_key_default_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&gre_key_default_mask,
		 sizeof(rte_be32_t), MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	return ret;
}

/**
 * Validate GRE optional item.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit flags to mark detected items.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] gre_item
 *   Pointer to gre_item
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_gre_option(struct rte_eth_dev *dev,
				   const struct rte_flow_item *item,
				   uint64_t item_flags,
				   const struct rte_flow_attr *attr,
				   const struct rte_flow_item *gre_item,
				   struct rte_flow_error *error)
{
	const struct rte_flow_item_gre *gre_spec = gre_item ? gre_item->spec : NULL;
	const struct rte_flow_item_gre *gre_mask = gre_item ? gre_item->mask : NULL;
	const struct rte_flow_item_gre_opt *spec = item->spec;
	const struct rte_flow_item_gre_opt *mask = item->mask;
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct rte_flow_item_gre_opt nic_mask = {
		.checksum_rsvd = {
			.checksum = RTE_BE16(UINT16_MAX),
			.reserved1 = 0x0,
		},
		.key = {
			.key = RTE_BE32(UINT32_MAX),
		},
		.sequence = {
			.sequence = RTE_BE32(UINT32_MAX),
		},
	};

	if (!(item_flags & MLX5_FLOW_LAYER_GRE))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "No preceding GRE header");
	if (item_flags & MLX5_FLOW_LAYER_INNER)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "GRE option following a wrong item");
	if ((!spec && !mlx5_hws_active(dev)) || !mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "At least one field gre_option(checksum/key/sequence) must be specified");
	if (!gre_mask)
		gre_mask = &rte_flow_item_gre_mask;
	if (mask->checksum_rsvd.checksum)
		if (gre_spec && (gre_mask->c_rsvd0_ver & RTE_BE16(0x8000)) &&
				 !(gre_spec->c_rsvd0_ver & RTE_BE16(0x8000)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "Checksum bit must be on");
	if (mask->key.key)
		if (gre_spec && (gre_mask->c_rsvd0_ver & RTE_BE16(0x2000)) &&
				 !(gre_spec->c_rsvd0_ver & RTE_BE16(0x2000)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "Key bit must be on");
	if (mask->sequence.sequence)
		if (gre_spec && (gre_mask->c_rsvd0_ver & RTE_BE16(0x1000)) &&
				 !(gre_spec->c_rsvd0_ver & RTE_BE16(0x1000)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item,
						  "Sequence bit must be on");
	if (!mlx5_hws_active(dev)) {
		if (mask->checksum_rsvd.checksum || mask->sequence.sequence) {
			if (priv->sh->steering_format_version ==
			    MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5 ||
			    ((attr->group ||
			      (attr->transfer && priv->fdb_def_rule)) &&
			     !priv->sh->misc5_cap) ||
			    (!(priv->sh->tunnel_header_0_1 &&
			       priv->sh->tunnel_header_2_3) &&
			     !attr->group &&
			     (!attr->transfer || !priv->fdb_def_rule)))
				return rte_flow_error_set
					(error, EINVAL,	RTE_FLOW_ERROR_TYPE_ITEM,
					 item, "Checksum/Sequence not supported");
		}
	}
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_gre_opt),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
mlx5_flow_validate_item_gre(const struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "L3 Layer is missing");
	}
	if (!mask)
		mask = &rte_flow_item_gre_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)&nic_mask,
		 sizeof(struct rte_flow_item_gre), MLX5_ITEM_RANGE_NOT_ACCEPTED,
		 error);
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
	uint8_t opt_len = priv->sh->cdev->config.hca_attr.geneve_max_opt_len ?
			  MLX5_GENEVE_OPT_LEN_1 : MLX5_GENEVE_OPT_LEN_0;
	const struct rte_flow_item_geneve nic_mask = {
		.ver_opt_len_o_c_rsvd0 = RTE_BE16(0x3f80),
		.vni =  { 0xff, 0xff, 0xff },
		.protocol = RTE_BE16(UINT16_MAX),
	};

	if (!priv->sh->cdev->config.hca_attr.tunnel_stateless_geneve_rx)
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
				  (dev, item, (const uint8_t *)mask,
				   (const uint8_t *)&nic_mask,
				   sizeof(struct rte_flow_item_geneve),
				   MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
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
 * Validate Geneve TLV option item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] last_item
 *   Previous validated item in the pattern items.
 * @param[in] geneve_item
 *   Previous GENEVE item specification.
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_geneve_opt(const struct rte_flow_item *item,
				   uint64_t last_item,
				   const struct rte_flow_item *geneve_item,
				   struct rte_eth_dev *dev,
				   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_geneve_tlv_option_resource *geneve_opt_resource;
	struct mlx5_hca_attr *hca_attr = &sh->cdev->config.hca_attr;
	uint8_t data_max_supported =
			hca_attr->max_geneve_tlv_option_data_len * 4;
	const struct rte_flow_item_geneve *geneve_spec;
	const struct rte_flow_item_geneve *geneve_mask;
	const struct rte_flow_item_geneve_opt *spec = item->spec;
	const struct rte_flow_item_geneve_opt *mask = item->mask;
	unsigned int i;
	unsigned int data_len;
	uint8_t tlv_option_len;
	uint16_t optlen_m, optlen_v;
	const struct rte_flow_item_geneve_opt full_mask = {
		.option_class = RTE_BE16(0xffff),
		.option_type = 0xff,
		.option_len = 0x1f,
	};

	if (!mask)
		mask = &rte_flow_item_geneve_opt_mask;
	if (!spec)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt class/type/length must be specified");
	if ((uint32_t)spec->option_len > MLX5_GENEVE_OPTLEN_MASK)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt length exceeds the limit (31)");
	/* Check if class type and length masks are full. */
	if (full_mask.option_class != mask->option_class ||
	    full_mask.option_type != mask->option_type ||
	    full_mask.option_len != (mask->option_len & full_mask.option_len))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt class/type/length masks must be full");
	/* Check if length is supported */
	if ((uint32_t)spec->option_len >
			hca_attr->max_geneve_tlv_option_data_len)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt length not supported");
	if (hca_attr->max_geneve_tlv_options > 1)
		DRV_LOG(DEBUG,
			"max_geneve_tlv_options supports more than 1 option");
	/* Check GENEVE item preceding. */
	if (!geneve_item || !(last_item & MLX5_FLOW_LAYER_GENEVE))
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve opt item must be preceded with Geneve item");
	geneve_spec = geneve_item->spec;
	geneve_mask = geneve_item->mask ? geneve_item->mask :
					  &rte_flow_item_geneve_mask;
	/* Check if GENEVE TLV option size doesn't exceed option length */
	if (geneve_spec && (geneve_mask->ver_opt_len_o_c_rsvd0 ||
			    geneve_spec->ver_opt_len_o_c_rsvd0)) {
		tlv_option_len = spec->option_len & mask->option_len;
		optlen_v = rte_be_to_cpu_16(geneve_spec->ver_opt_len_o_c_rsvd0);
		optlen_v = MLX5_GENEVE_OPTLEN_VAL(optlen_v);
		optlen_m = rte_be_to_cpu_16(geneve_mask->ver_opt_len_o_c_rsvd0);
		optlen_m = MLX5_GENEVE_OPTLEN_VAL(optlen_m);
		if ((optlen_v & optlen_m) <= tlv_option_len)
			return rte_flow_error_set
				(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
				 "GENEVE TLV option length exceeds optlen");
	}
	/* Check if length is 0 or data is 0. */
	if (spec->data == NULL || spec->option_len == 0)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt with zero data/length not supported");
	/* Check not all data & mask are 0. */
	data_len = spec->option_len * 4;
	if (mask->data == NULL) {
		for (i = 0; i < data_len; i++)
			if (spec->data[i])
				break;
		if (i == data_len)
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Can't match on Geneve option data 0");
	} else {
		for (i = 0; i < data_len; i++)
			if (spec->data[i] & mask->data[i])
				break;
		if (i == data_len)
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Can't match on Geneve option data and mask 0");
		/* Check data mask supported. */
		for (i = data_max_supported; i < data_len ; i++)
			if (mask->data[i])
				return rte_flow_error_set(error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ITEM, item,
					"Data mask is of unsupported size");
	}
	/* Check GENEVE option is supported in NIC. */
	if (!hca_attr->geneve_tlv_opt)
		return rte_flow_error_set
			(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ITEM, item,
			"Geneve TLV opt not supported");
	/* Check if we already have geneve option with different type/class. */
	rte_spinlock_lock(&sh->geneve_tlv_opt_sl);
	geneve_opt_resource = sh->geneve_tlv_option_resource;
	if (geneve_opt_resource != NULL)
		if (geneve_opt_resource->option_class != spec->option_class ||
		    geneve_opt_resource->option_type != spec->option_type ||
		    geneve_opt_resource->length != spec->option_len) {
			rte_spinlock_unlock(&sh->geneve_tlv_opt_sl);
			return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_ITEM, item,
				"Only one Geneve TLV option supported");
		}
	rte_spinlock_unlock(&sh->geneve_tlv_opt_sl);
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
	const struct rte_flow_item_mpls hws_nic_mask = {
		.label_tc_s = {0xff, 0xff, 0xff},
		.ttl = 0xff
	};
	const struct rte_flow_item_mpls *nic_mask = !mlx5_hws_active(dev) ?
		&rte_flow_item_mpls_mask : &hws_nic_mask;
	const struct rte_flow_item_mpls *mask = item->mask;
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	if (!mlx5_hws_active(dev)) {
		/* MPLS has HW support in HWS */
		if (!priv->sh->dev_cap.mpls_en)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "MPLS not supported or disabled in firmware configuration.");
		/* MPLS over UDP, GRE is allowed */
		if (!(prev_layer & (MLX5_FLOW_LAYER_OUTER_L4_UDP |
				    MLX5_FLOW_LAYER_GRE |
				    MLX5_FLOW_LAYER_GRE_KEY)))
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "protocol filtering not compatible with MPLS layer");
		/* Multi-tunnel isn't allowed but MPLS over GRE is an exception. */
		if ((item_flags & MLX5_FLOW_LAYER_TUNNEL) &&
		    !(item_flags & MLX5_FLOW_LAYER_GRE))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM, item,
						  "multiple tunnel layers not supported");
	}
	if (!mask)
		mask = nic_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)nic_mask,
		 sizeof(struct rte_flow_item_mpls),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	return 0;
#else
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_ITEM, item,
				  "MPLS is not supported by Verbs, please"
				  " update.");
#endif
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
mlx5_flow_validate_item_nvgre(const struct rte_eth_dev *dev,
			      const struct rte_flow_item *item,
			      uint64_t item_flags,
			      uint8_t target_protocol,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_nvgre *mask = item->mask;
	int ret;

	const struct rte_flow_item_nvgre hws_nic_mask = {
		.c_k_s_rsvd0_ver = RTE_BE16(0xB000),
		.protocol = RTE_BE16(UINT16_MAX),
		.tni = {0xff, 0xff, 0xff},
		.flow_id = 0xff
	};
	const struct rte_flow_item_nvgre *nic_mask = !mlx5_hws_active(dev) ?
		&rte_flow_item_nvgre_mask : &hws_nic_mask;

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
	if (!mlx5_hws_active(dev)) {
		if (!(item_flags & MLX5_FLOW_LAYER_OUTER_L3))
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ITEM,
						  item, "L3 Layer is missing");
	}
	if (!mask)
		mask = nic_mask;
	ret = mlx5_flow_item_acceptable
		(dev, item, (const uint8_t *)mask,
		 (const uint8_t *)nic_mask,
		 sizeof(struct rte_flow_item_nvgre),
		 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Validate eCPRI item.
 *
 * @param[in] item
 *   Item specification.
 * @param[in] item_flags
 *   Bit-fields that holds the items detected until now.
 * @param[in] last_item
 *   Previous validated item in the pattern items.
 * @param[in] ether_type
 *   Type in the ethernet layer header (including dot1q).
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
mlx5_flow_validate_item_ecpri(const struct rte_eth_dev *dev,
			      const struct rte_flow_item *item,
			      uint64_t item_flags,
			      uint64_t last_item,
			      uint16_t ether_type,
			      const struct rte_flow_item_ecpri *acc_mask,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_ecpri *mask = item->mask;
	const struct rte_flow_item_ecpri nic_mask = {
		.hdr = {
			.common = {
				.u32 =
				RTE_BE32(((const struct rte_ecpri_common_hdr) {
					.type = 0xFF,
					}).u32),
			},
			.dummy[0] = 0xFFFFFFFF,
		},
	};
	const uint64_t outer_l2_vlan = (MLX5_FLOW_LAYER_OUTER_L2 |
					MLX5_FLOW_LAYER_OUTER_VLAN);
	struct rte_flow_item_ecpri mask_lo;

	if (!(last_item & outer_l2_vlan) &&
	    last_item != MLX5_FLOW_LAYER_OUTER_L4_UDP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "eCPRI can only follow L2/VLAN layer or UDP layer");
	if ((last_item & outer_l2_vlan) && ether_type &&
	    ether_type != RTE_ETHER_TYPE_ECPRI)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "eCPRI cannot follow L2/VLAN layer which ether type is not 0xAEFE");
	if (item_flags & MLX5_FLOW_LAYER_TUNNEL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "eCPRI with tunnel is not supported right now");
	if (item_flags & MLX5_FLOW_LAYER_OUTER_L3)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "multiple L3 layers not supported");
	else if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_TCP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "eCPRI cannot coexist with a TCP layer");
	/* In specification, eCPRI could be over UDP layer. */
	else if (item_flags & MLX5_FLOW_LAYER_OUTER_L4_UDP)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "eCPRI over UDP layer is not yet supported right now");
	/* Mask for type field in common header could be zero. */
	if (!mask)
		mask = &rte_flow_item_ecpri_mask;
	mask_lo.hdr.common.u32 = rte_be_to_cpu_32(mask->hdr.common.u32);
	/* Input mask is in big-endian format. */
	if (mask_lo.hdr.common.type != 0 && mask_lo.hdr.common.type != 0xff)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "partial mask is not supported for protocol");
	else if (mask_lo.hdr.common.type == 0 && mask->hdr.dummy[0] != 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ITEM_MASK, mask,
					  "message header mask must be after a type mask");
	return mlx5_flow_item_acceptable(dev, item, (const uint8_t *)mask,
					 acc_mask ? (const uint8_t *)acc_mask
						  : (const uint8_t *)&nic_mask,
					 sizeof(struct rte_flow_item_ecpri),
					 MLX5_ITEM_RANGE_NOT_ACCEPTED, error);
}

/**
 * Validate the NSH item.
 *
 * @param[in] dev
 *   Pointer to Ethernet device on which flow rule is being created on.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_validate_item_nsh(struct rte_eth_dev *dev,
			    const struct rte_flow_item *item,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (item->mask) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "NSH fields matching is not supported");
	}

	if (!priv->sh->config.dv_flow_en) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "NSH support requires DV flow interface");
	}

	if (!priv->sh->cdev->config.hca_attr.tunnel_stateless_vxlan_gpe_nsh) {
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ITEM, item,
					  "Current FW does not support matching on NSH");
	}

	return 0;
}

static uintptr_t
flow_null_list_create(struct rte_eth_dev *dev __rte_unused,
		      enum mlx5_flow_type type __rte_unused,
		      const struct rte_flow_attr *attr __rte_unused,
		      const struct rte_flow_item items[] __rte_unused,
		      const struct rte_flow_action actions[] __rte_unused,
		      bool external __rte_unused,
		      struct rte_flow_error *error)
{
	rte_flow_error_set(error, ENOTSUP,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
	return 0;
}

static int
flow_null_validate(struct rte_eth_dev *dev __rte_unused,
		   const struct rte_flow_attr *attr __rte_unused,
		   const struct rte_flow_item items[] __rte_unused,
		   const struct rte_flow_action actions[] __rte_unused,
		   bool external __rte_unused,
		   int hairpin __rte_unused,
		   struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL, NULL);
}

static struct mlx5_flow *
flow_null_prepare(struct rte_eth_dev *dev __rte_unused,
		  const struct rte_flow_attr *attr __rte_unused,
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

static int
flow_null_sync_domain(struct rte_eth_dev *dev __rte_unused,
		      uint32_t domains __rte_unused,
		      uint32_t flags __rte_unused)
{
	return 0;
}

int
flow_null_get_aged_flows(struct rte_eth_dev *dev,
		    void **context __rte_unused,
		    uint32_t nb_contexts __rte_unused,
		    struct rte_flow_error *error __rte_unused)
{
	DRV_LOG(ERR, "port %u get aged flows is not supported.",
		dev->data->port_id);
	return -ENOTSUP;
}

uint32_t
flow_null_counter_allocate(struct rte_eth_dev *dev)
{
	DRV_LOG(ERR, "port %u counter allocate is not supported.",
		dev->data->port_id);
	return 0;
}

void
flow_null_counter_free(struct rte_eth_dev *dev,
			uint32_t counter __rte_unused)
{
	DRV_LOG(ERR, "port %u counter free is not supported.",
		 dev->data->port_id);
}

int
flow_null_counter_query(struct rte_eth_dev *dev,
			uint32_t counter __rte_unused,
			bool clear __rte_unused,
			uint64_t *pkts __rte_unused,
			uint64_t *bytes __rte_unused,
			void **action __rte_unused)
{
	DRV_LOG(ERR, "port %u counter query is not supported.",
		 dev->data->port_id);
	return -ENOTSUP;
}

/* Void driver to protect from null pointer reference. */
const struct mlx5_flow_driver_ops mlx5_flow_null_drv_ops = {
	.list_create = flow_null_list_create,
	.validate = flow_null_validate,
	.prepare = flow_null_prepare,
	.translate = flow_null_translate,
	.apply = flow_null_apply,
	.remove = flow_null_remove,
	.destroy = flow_null_destroy,
	.query = flow_null_query,
	.sync_domain = flow_null_sync_domain,
	.get_aged_flows = flow_null_get_aged_flows,
	.counter_alloc = flow_null_counter_allocate,
	.counter_free = flow_null_counter_free,
	.counter_query = flow_null_counter_query
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
	/* The OS can determine first a specific flow type (DV, VERBS) */
	enum mlx5_flow_drv_type type = mlx5_flow_os_get_type();

	if (type != MLX5_FLOW_TYPE_MAX)
		return type;
	/*
	 * Currently when dv_flow_en == 2, only HW steering engine is
	 * supported. New engines can also be chosen here if ready.
	 */
	if (priv->sh->config.dv_flow_en == 2)
		return MLX5_FLOW_TYPE_HW;
	if (!attr)
		return MLX5_FLOW_TYPE_MIN;
	/* If no OS specific type - continue with DV/VERBS selection */
	if (attr->transfer && priv->sh->config.dv_esw_en)
		type = MLX5_FLOW_TYPE_DV;
	if (!attr->transfer)
		type = priv->sh->config.dv_flow_en ? MLX5_FLOW_TYPE_DV :
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
 * @param[in] hairpin
 *   Number of hairpin TX actions, 0 means classic flow.
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
		  bool external, int hairpin, struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow_get_drv_type(dev, attr);

	fops = flow_get_drv_ops(type);
	return fops->validate(dev, attr, items, actions, external,
			      hairpin, error);
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
 * @param[in] dev
 *   Pointer to the dev structure.
 * @param[in] attr
 *   Pointer to the flow attributes.
 * @param[in] items
 *   Pointer to the list of items.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[in] flow_idx
 *   This memory pool index to the flow.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   Pointer to device flow on success, otherwise NULL and rte_errno is set.
 */
static inline struct mlx5_flow *
flow_drv_prepare(struct rte_eth_dev *dev,
		 const struct rte_flow *flow,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 uint32_t flow_idx,
		 struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;
	struct mlx5_flow *mlx5_flow = NULL;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	mlx5_flow = fops->prepare(dev, attr, items, actions, error);
	if (mlx5_flow)
		mlx5_flow->flow_idx = flow_idx;
	return mlx5_flow;
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

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	fops->destroy(dev, flow);
}

/**
 * Flow driver find RSS policy tbl API. This abstracts calling driver
 * specific functions. Parent flow (rte_flow) should have driver
 * type (drv_type). It will find the RSS policy table that has the rss_desc.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] policy
 *   Pointer to meter policy table.
 * @param[in] rss_desc
 *   Pointer to rss_desc
 */
static struct mlx5_flow_meter_sub_policy *
flow_drv_meter_sub_policy_rss_prepare(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct mlx5_flow_meter_policy *policy,
		struct mlx5_flow_rss_desc *rss_desc[MLX5_MTR_RTE_COLORS])
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	return fops->meter_sub_policy_rss_prepare(dev, policy, rss_desc);
}

/**
 * Flow driver color tag rule API. This abstracts calling driver
 * specific functions. Parent flow (rte_flow) should have driver
 * type (drv_type). It will create the color tag rules in hierarchy meter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in, out] flow
 *   Pointer to flow structure.
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[in] src_port
 *   The src port this extra rule should use.
 * @param[in] item
 *   The src port id match item.
 * @param[out] error
 *   Pointer to error structure.
 */
static int
flow_drv_mtr_hierarchy_rule_create(struct rte_eth_dev *dev,
		struct rte_flow *flow,
		struct mlx5_flow_meter_info *fm,
		int32_t src_port,
		const struct rte_flow_item *item,
		struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type = flow->drv_type;

	MLX5_ASSERT(type > MLX5_FLOW_TYPE_MIN && type < MLX5_FLOW_TYPE_MAX);
	fops = flow_get_drv_ops(type);
	return fops->meter_hierarchy_rule_create(dev, fm,
						src_port, item, error);
}

/**
 * Get RSS action from the action list.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[in] flow
 *   Parent flow structure pointer.
 *
 * @return
 *   Pointer to the RSS action if exist, else return NULL.
 */
static const struct rte_flow_action_rss*
flow_get_rss_action(struct rte_eth_dev *dev,
		    const struct rte_flow_action actions[])
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_rss *rss = NULL;
	struct mlx5_meter_policy_action_container *acg;
	struct mlx5_meter_policy_action_container *acy;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			break;
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
		{
			const struct rte_flow_action_sample *sample =
								actions->conf;
			const struct rte_flow_action *act = sample->actions;
			for (; act->type != RTE_FLOW_ACTION_TYPE_END; act++)
				if (act->type == RTE_FLOW_ACTION_TYPE_RSS)
					rss = act->conf;
			break;
		}
		case RTE_FLOW_ACTION_TYPE_METER:
		{
			uint32_t mtr_idx;
			struct mlx5_flow_meter_info *fm;
			struct mlx5_flow_meter_policy *policy;
			const struct rte_flow_action_meter *mtr = actions->conf;

			fm = mlx5_flow_meter_find(priv, mtr->mtr_id, &mtr_idx);
			if (fm && !fm->def_policy) {
				policy = mlx5_flow_meter_policy_find(dev,
						fm->policy_id, NULL);
				MLX5_ASSERT(policy);
				if (policy->is_hierarchy) {
					policy =
				mlx5_flow_meter_hierarchy_get_final_policy(dev,
									policy);
					if (!policy)
						return NULL;
				}
				if (policy->is_rss) {
					acg =
					&policy->act_cnt[RTE_COLOR_GREEN];
					acy =
					&policy->act_cnt[RTE_COLOR_YELLOW];
					if (acg->fate_action ==
					    MLX5_FLOW_FATE_SHARED_RSS)
						rss = acg->rss->conf;
					else if (acy->fate_action ==
						 MLX5_FLOW_FATE_SHARED_RSS)
						rss = acy->rss->conf;
				}
			}
			break;
		}
		default:
			break;
		}
	}
	return rss;
}

/**
 * Get ASO age action by index.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] age_idx
 *   Index to the ASO age action.
 *
 * @return
 *   The specified ASO age action.
 */
struct mlx5_aso_age_action*
flow_aso_age_get_by_idx(struct rte_eth_dev *dev, uint32_t age_idx)
{
	uint16_t pool_idx = age_idx & UINT16_MAX;
	uint16_t offset = (age_idx >> 16) & UINT16_MAX;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_age_mng *mng = priv->sh->aso_age_mng;
	struct mlx5_aso_age_pool *pool;

	rte_rwlock_read_lock(&mng->resize_rwl);
	pool = mng->pools[pool_idx];
	rte_rwlock_read_unlock(&mng->resize_rwl);
	return &pool->actions[offset - 1];
}

/* maps indirect action to translated direct in some actions array */
struct mlx5_translated_action_handle {
	struct rte_flow_action_handle *action; /**< Indirect action handle. */
	int index; /**< Index in related array of rte_flow_action. */
};

/**
 * Translates actions of type RTE_FLOW_ACTION_TYPE_INDIRECT to related
 * direct action if translation possible.
 * This functionality used to run same execution path for both direct and
 * indirect actions on flow create. All necessary preparations for indirect
 * action handling should be performed on *handle* actions list returned
 * from this call.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   List of actions to translate.
 * @param[out] handle
 *   List to store translated indirect action object handles.
 * @param[in, out] indir_n
 *   Size of *handle* array. On return should be updated with number of
 *   indirect actions retrieved from the *actions* list.
 * @param[out] translated_actions
 *   List of actions where all indirect actions were translated to direct
 *   if possible. NULL if no translation took place.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_action_handles_translate(struct rte_eth_dev *dev,
			      const struct rte_flow_action actions[],
			      struct mlx5_translated_action_handle *handle,
			      int *indir_n,
			      struct rte_flow_action **translated_actions,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_action *translated = NULL;
	size_t actions_size;
	int n;
	int copied_n = 0;
	struct mlx5_translated_action_handle *handle_end = NULL;

	for (n = 0; actions[n].type != RTE_FLOW_ACTION_TYPE_END; n++) {
		if (actions[n].type != RTE_FLOW_ACTION_TYPE_INDIRECT)
			continue;
		if (copied_n == *indir_n) {
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				 NULL, "too many shared actions");
		}
		rte_memcpy(&handle[copied_n].action, &actions[n].conf,
			   sizeof(actions[n].conf));
		handle[copied_n].index = n;
		copied_n++;
	}
	n++;
	*indir_n = copied_n;
	if (!copied_n)
		return 0;
	actions_size = sizeof(struct rte_flow_action) * n;
	translated = mlx5_malloc(MLX5_MEM_ZERO, actions_size, 0, SOCKET_ID_ANY);
	if (!translated) {
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	memcpy(translated, actions, actions_size);
	for (handle_end = handle + copied_n; handle < handle_end; handle++) {
		struct mlx5_shared_action_rss *shared_rss;
		uint32_t act_idx = (uint32_t)(uintptr_t)handle->action;
		uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
		uint32_t idx = act_idx &
			       ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);

		switch (type) {
		case MLX5_INDIRECT_ACTION_TYPE_RSS:
			shared_rss = mlx5_ipool_get
			  (priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
			translated[handle->index].type =
				RTE_FLOW_ACTION_TYPE_RSS;
			translated[handle->index].conf =
				&shared_rss->origin;
			break;
		case MLX5_INDIRECT_ACTION_TYPE_COUNT:
			translated[handle->index].type =
						(enum rte_flow_action_type)
						MLX5_RTE_FLOW_ACTION_TYPE_COUNT;
			translated[handle->index].conf = (void *)(uintptr_t)idx;
			break;
		case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
			translated[handle->index].type =
						(enum rte_flow_action_type)
						MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK;
			translated[handle->index].conf = (void *)(uintptr_t)idx;
			break;
		case MLX5_INDIRECT_ACTION_TYPE_AGE:
			if (priv->sh->flow_hit_aso_en) {
				translated[handle->index].type =
					(enum rte_flow_action_type)
					MLX5_RTE_FLOW_ACTION_TYPE_AGE;
				translated[handle->index].conf =
							 (void *)(uintptr_t)idx;
				break;
			}
			/* Fall-through */
		case MLX5_INDIRECT_ACTION_TYPE_CT:
			if (priv->sh->ct_aso_en) {
				translated[handle->index].type =
					RTE_FLOW_ACTION_TYPE_CONNTRACK;
				translated[handle->index].conf =
							 (void *)(uintptr_t)idx;
				break;
			}
			/* Fall-through */
		default:
			mlx5_free(translated);
			return rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				 NULL, "invalid indirect action type");
		}
	}
	*translated_actions = translated;
	return 0;
}

/**
 * Get Shared RSS action from the action list.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] shared
 *   Pointer to the list of actions.
 * @param[in] shared_n
 *   Actions list length.
 *
 * @return
 *   The MLX5 RSS action ID if exists, otherwise return 0.
 */
static uint32_t
flow_get_shared_rss_action(struct rte_eth_dev *dev,
			   struct mlx5_translated_action_handle *handle,
			   int shared_n)
{
	struct mlx5_translated_action_handle *handle_end;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss;


	for (handle_end = handle + shared_n; handle < handle_end; handle++) {
		uint32_t act_idx = (uint32_t)(uintptr_t)handle->action;
		uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
		uint32_t idx = act_idx &
			       ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
		switch (type) {
		case MLX5_INDIRECT_ACTION_TYPE_RSS:
			shared_rss = mlx5_ipool_get
				(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
									   idx);
			rte_atomic_fetch_add_explicit(&shared_rss->refcnt, 1,
					   rte_memory_order_relaxed);
			return idx;
		default:
			break;
		}
	}
	return 0;
}

static unsigned int
find_graph_root(uint32_t rss_level)
{
	return rss_level < 2 ? MLX5_EXPANSION_ROOT :
			       MLX5_EXPANSION_ROOT_OUTER;
}

/**
 *  Get layer flags from the prefix flow.
 *
 *  Some flows may be split to several subflows, the prefix subflow gets the
 *  match items and the suffix sub flow gets the actions.
 *  Some actions need the user defined match item flags to get the detail for
 *  the action.
 *  This function helps the suffix flow to get the item layer flags from prefix
 *  subflow.
 *
 * @param[in] dev_flow
 *   Pointer the created prefix subflow.
 *
 * @return
 *   The layers get from prefix subflow.
 */
static inline uint64_t
flow_get_prefix_layer_flags(struct mlx5_flow *dev_flow)
{
	uint64_t layers = 0;

	/*
	 * Layers bits could be localization, but usually the compiler will
	 * help to do the optimization work for source code.
	 * If no decap actions, use the layers directly.
	 */
	if (!(dev_flow->act_flags & MLX5_FLOW_ACTION_DECAP))
		return dev_flow->handle->layers;
	/* Convert L3 layers with decap action. */
	if (dev_flow->handle->layers & MLX5_FLOW_LAYER_INNER_L3_IPV4)
		layers |= MLX5_FLOW_LAYER_OUTER_L3_IPV4;
	else if (dev_flow->handle->layers & MLX5_FLOW_LAYER_INNER_L3_IPV6)
		layers |= MLX5_FLOW_LAYER_OUTER_L3_IPV6;
	/* Convert L4 layers with decap action.  */
	if (dev_flow->handle->layers & MLX5_FLOW_LAYER_INNER_L4_TCP)
		layers |= MLX5_FLOW_LAYER_OUTER_L4_TCP;
	else if (dev_flow->handle->layers & MLX5_FLOW_LAYER_INNER_L4_UDP)
		layers |= MLX5_FLOW_LAYER_OUTER_L4_UDP;
	return layers;
}

/**
 * Get metadata split action information.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] qrss
 *   Pointer to the return pointer.
 * @param[out] qrss_type
 *   Pointer to the action type to return. RTE_FLOW_ACTION_TYPE_END is returned
 *   if no QUEUE/RSS is found.
 * @param[out] encap_idx
 *   Pointer to the index of the encap action if exists, otherwise the last
 *   action index.
 *
 * @return
 *   Total number of actions.
 */
static int
flow_parse_metadata_split_actions_info(const struct rte_flow_action actions[],
				       const struct rte_flow_action **qrss,
				       int *encap_idx)
{
	const struct rte_flow_action_raw_encap *raw_encap;
	int actions_n = 0;
	int raw_decap_idx = -1;

	*encap_idx = -1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			*encap_idx = actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			raw_decap_idx = actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size > MLX5_ENCAPSULATION_DECISION_SIZE)
				*encap_idx = raw_decap_idx != -1 ?
						      raw_decap_idx : actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
			*qrss = actions;
			break;
		default:
			break;
		}
		actions_n++;
	}
	if (*encap_idx == -1)
		*encap_idx = actions_n;
	/* Count RTE_FLOW_ACTION_TYPE_END. */
	return actions_n + 1;
}

/**
 * Check if the action will change packet.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] type
 *   action type.
 *
 * @return
 *   true if action will change packet, false otherwise.
 */
static bool flow_check_modify_action_type(struct rte_eth_dev *dev,
					  enum rte_flow_action_type type)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	switch (type) {
	case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
	case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
	case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
	case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
	case RTE_FLOW_ACTION_TYPE_DEC_TTL:
	case RTE_FLOW_ACTION_TYPE_SET_TTL:
	case RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ:
	case RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ:
	case RTE_FLOW_ACTION_TYPE_INC_TCP_ACK:
	case RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK:
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
	case RTE_FLOW_ACTION_TYPE_SET_META:
	case RTE_FLOW_ACTION_TYPE_SET_TAG:
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
	case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
	case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
	case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
	case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
	case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
		return true;
	case RTE_FLOW_ACTION_TYPE_FLAG:
	case RTE_FLOW_ACTION_TYPE_MARK:
		if (priv->sh->config.dv_xmeta_en != MLX5_XMETA_MODE_LEGACY &&
		    priv->sh->config.dv_xmeta_en != MLX5_XMETA_MODE_META32_HWS)
			return true;
		else
			return false;
	default:
		return false;
	}
}

/**
 * Check meter action from the action list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[out] has_mtr
 *   Pointer to the meter exist flag.
 * @param[out] has_modify
 *   Pointer to the flag showing there's packet change action.
 * @param[out] meter_id
 *   Pointer to the meter id.
 *
 * @return
 *   Total number of actions.
 */
static int
flow_check_meter_action(struct rte_eth_dev *dev,
			const struct rte_flow_action actions[],
			bool *has_mtr, bool *has_modify, uint32_t *meter_id)
{
	const struct rte_flow_action_meter *mtr = NULL;
	int actions_n = 0;

	MLX5_ASSERT(has_mtr);
	*has_mtr = false;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_METER:
			mtr = actions->conf;
			*meter_id = mtr->mtr_id;
			*has_mtr = true;
			break;
		default:
			break;
		}
		if (!*has_mtr)
			*has_modify |= flow_check_modify_action_type(dev,
								actions->type);
		actions_n++;
	}
	/* Count RTE_FLOW_ACTION_TYPE_END. */
	return actions_n + 1;
}

/**
 * Check if the flow should be split due to hairpin.
 * The reason for the split is that in current HW we can't
 * support encap and push-vlan on Rx, so if a flow contains
 * these actions we move it to Tx.
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
	int split = 0;
	int push_vlan = 0;
	const struct rte_flow_action_queue *queue;
	const struct rte_flow_action_rss *rss;
	const struct rte_flow_action_raw_encap *raw_encap;
	const struct rte_eth_hairpin_conf *conf;

	if (!attr->ingress)
		return 0;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN)
			push_vlan = 1;
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			queue = actions->conf;
			if (queue == NULL)
				return 0;
			conf = mlx5_rxq_get_hairpin_conf(dev, queue->index);
			if (conf == NULL || conf->tx_explicit != 0)
				return 0;
			queue_action = 1;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss = actions->conf;
			if (rss == NULL || rss->queue_num == 0)
				return 0;
			conf = mlx5_rxq_get_hairpin_conf(dev, rss->queue[0]);
			if (conf == NULL || conf->tx_explicit != 0)
				return 0;
			queue_action = 1;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			split++;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (push_vlan)
				split++;
			action_n++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size > MLX5_ENCAPSULATION_DECISION_SIZE)
				split++;
			action_n++;
			break;
		default:
			action_n++;
			break;
		}
	}
	if (split && queue_action)
		return action_n;
	return 0;
}

int
flow_dv_mreg_match_cb(void *tool_ctx __rte_unused,
		      struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_mreg_copy_resource *mcp_res =
			       container_of(entry, typeof(*mcp_res), hlist_ent);

	return mcp_res->mark_id != *(uint32_t *)(ctx->data);
}

struct mlx5_list_entry *
flow_dv_mreg_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct rte_flow_error *error = ctx->error;
	uint32_t idx = 0;
	int ret;
	uint32_t mark_id = *(uint32_t *)(ctx->data);
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
		.src = REG_NON,
	};
	struct rte_flow_action_jump jump = {
		.group = MLX5_FLOW_MREG_ACT_TABLE_GROUP,
	};
	struct rte_flow_action actions[] = {
		[3] = { .type = RTE_FLOW_ACTION_TYPE_END, },
	};

	/* Fill the register fields in the flow. */
	ret = mlx5_flow_get_reg_id(dev, MLX5_FLOW_MARK, 0, error);
	if (ret < 0)
		return NULL;
	tag_spec.id = ret;
	ret = mlx5_flow_get_reg_id(dev, MLX5_METADATA_RX, 0, error);
	if (ret < 0)
		return NULL;
	cp_mreg.src = ret;
	/* Provide the full width of FLAG specific value. */
	if (mark_id == (priv->sh->dv_regc0_mask & MLX5_FLOW_MARK_DEFAULT))
		tag_spec.data = MLX5_FLOW_MARK_DEFAULT;
	/* Build a new flow. */
	if (mark_id != MLX5_DEFAULT_COPY_ID) {
		items[0] = (struct rte_flow_item){
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &tag_spec,
		};
		items[1] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_END,
		};
		actions[0] = (struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_MARK,
			.conf = &ftag,
		};
		actions[1] = (struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
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
		attr.priority = MLX5_FLOW_LOWEST_PRIO_INDICATOR;
		items[0] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_END,
		};
		actions[0] = (struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
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
	mcp_res = mlx5_ipool_zmalloc(priv->sh->ipool[MLX5_IPOOL_MCP], &idx);
	if (!mcp_res) {
		rte_errno = ENOMEM;
		return NULL;
	}
	mcp_res->idx = idx;
	mcp_res->mark_id = mark_id;
	/*
	 * The copy Flows are not included in any list. There
	 * ones are referenced from other Flows and can not
	 * be applied, removed, deleted in arbitrary order
	 * by list traversing.
	 */
	mcp_res->rix_flow = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_MCP,
					&attr, items, actions, false, error);
	if (!mcp_res->rix_flow) {
		mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MCP], idx);
		return NULL;
	}
	return &mcp_res->hlist_ent;
}

struct mlx5_list_entry *
flow_dv_mreg_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
		      void *cb_ctx __rte_unused)
{
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	uint32_t idx = 0;

	mcp_res = mlx5_ipool_malloc(priv->sh->ipool[MLX5_IPOOL_MCP], &idx);
	if (!mcp_res) {
		rte_errno = ENOMEM;
		return NULL;
	}
	memcpy(mcp_res, oentry, sizeof(*mcp_res));
	mcp_res->idx = idx;
	return &mcp_res->hlist_ent;
}

void
flow_dv_mreg_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res =
			       container_of(entry, typeof(*mcp_res), hlist_ent);
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MCP], mcp_res->idx);
}

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
	struct mlx5_list_entry *entry;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &mark_id,
	};

	/* Check if already registered. */
	MLX5_ASSERT(priv->sh->mreg_cp_tbl);
	entry = mlx5_hlist_register(priv->sh->mreg_cp_tbl, mark_id, &ctx);
	if (!entry)
		return NULL;
	return container_of(entry, struct mlx5_flow_mreg_copy_resource,
			    hlist_ent);
}

void
flow_dv_mreg_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_flow_mreg_copy_resource *mcp_res =
			       container_of(entry, typeof(*mcp_res), hlist_ent);
	struct rte_eth_dev *dev = tool_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;

	MLX5_ASSERT(mcp_res->rix_flow);
	mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_MCP, mcp_res->rix_flow);
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_MCP], mcp_res->idx);
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
	struct mlx5_flow_mreg_copy_resource *mcp_res;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!flow->rix_mreg_copy)
		return;
	mcp_res = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MCP],
				 flow->rix_mreg_copy);
	if (!mcp_res || !priv->sh->mreg_cp_tbl)
		return;
	MLX5_ASSERT(mcp_res->rix_flow);
	mlx5_hlist_unregister(priv->sh->mreg_cp_tbl, &mcp_res->hlist_ent);
	flow->rix_mreg_copy = 0;
}

/**
 * Remove the default copy action from RX_CP_TBL.
 *
 * This functions is called in the mlx5_dev_start(). No thread safe
 * is guaranteed.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
flow_mreg_del_default_copy_action(struct rte_eth_dev *dev)
{
	struct mlx5_list_entry *entry;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_cb_ctx ctx;
	uint32_t mark_id;

	/* Check if default flow is registered. */
	if (!priv->sh->mreg_cp_tbl)
		return;
	mark_id = MLX5_DEFAULT_COPY_ID;
	ctx.data = &mark_id;
	entry = mlx5_hlist_lookup(priv->sh->mreg_cp_tbl, mark_id, &ctx);
	if (!entry)
		return;
	mlx5_hlist_unregister(priv->sh->mreg_cp_tbl, entry);
}

/**
 * Add the default copy action in RX_CP_TBL.
 *
 * This functions is called in the mlx5_dev_start(). No thread safe
 * is guaranteed.
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
	struct mlx5_flow_cb_ctx ctx;
	uint32_t mark_id;

	/* Check whether extensive metadata feature is engaged. */
	if (!priv->sh->config.dv_flow_en ||
	    priv->sh->config.dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !mlx5_flow_ext_mreg_supported(dev) ||
	    !priv->sh->dv_regc0_mask)
		return 0;
	/*
	 * Add default mreg copy flow may be called multiple time, but
	 * only be called once in stop. Avoid register it twice.
	 */
	mark_id = MLX5_DEFAULT_COPY_ID;
	ctx.data = &mark_id;
	if (mlx5_hlist_lookup(priv->sh->mreg_cp_tbl, mark_id, &ctx))
		return 0;
	mcp_res = flow_mreg_add_copy_action(dev, mark_id, error);
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
	struct mlx5_sh_config *config = &priv->sh->config;
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
			flow->rix_mreg_copy = mcp_res->idx;
			return 0;
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark = (const struct rte_flow_action_mark *)
				actions->conf;
			mcp_res =
				flow_mreg_add_copy_action(dev, mark->id, error);
			if (!mcp_res)
				return -rte_errno;
			flow->rix_mreg_copy = mcp_res->idx;
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
 * Since HW can't support encap and push-vlan on Rx, we move these
 * actions to Tx.
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
		   uint32_t flow_id)
{
	const struct rte_flow_action_raw_encap *raw_encap;
	const struct rte_flow_action_raw_decap *raw_decap;
	struct mlx5_rte_flow_action_set_tag *set_tag;
	struct rte_flow_action *tag_action;
	struct mlx5_rte_flow_item_tag *tag_item;
	struct rte_flow_item *item;
	char *addr;
	int push_vlan = 0;
	int encap = 0;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN)
			push_vlan = 1;
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			rte_memcpy(actions_tx, actions,
			       sizeof(struct rte_flow_action));
			actions_tx++;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			if (push_vlan) {
				rte_memcpy(actions_tx, actions,
					   sizeof(struct rte_flow_action));
				actions_tx++;
			} else {
				rte_memcpy(actions_rx, actions,
					   sizeof(struct rte_flow_action));
				actions_rx++;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
		case RTE_FLOW_ACTION_TYPE_AGE:
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
			if (raw_encap->size > MLX5_ENCAPSULATION_DECISION_SIZE) {
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
			if (raw_decap->size < MLX5_ENCAPSULATION_DECISION_SIZE) {
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
	tag_action->type = (enum rte_flow_action_type)
			   MLX5_RTE_FLOW_ACTION_TYPE_TAG;
	actions_rx++;
	rte_memcpy(actions_rx, actions, sizeof(struct rte_flow_action));
	actions_rx++;
	set_tag = (void *)actions_rx;
	*set_tag = (struct mlx5_rte_flow_action_set_tag) {
		.id = mlx5_flow_get_reg_id(dev, MLX5_HAIRPIN_RX, 0, NULL),
		.data = flow_id,
	};
	MLX5_ASSERT(set_tag->id > REG_NON);
	tag_action->conf = set_tag;
	/* Create Tx item list. */
	rte_memcpy(actions_tx, actions, sizeof(struct rte_flow_action));
	addr = (void *)&pattern_tx[2];
	item = pattern_tx;
	item->type = (enum rte_flow_item_type)
		     MLX5_RTE_FLOW_ITEM_TYPE_TAG;
	tag_item = (void *)addr;
	tag_item->data = flow_id;
	tag_item->id = mlx5_flow_get_reg_id(dev, MLX5_HAIRPIN_TX, 0, NULL);
	MLX5_ASSERT(set_tag->id > REG_NON);
	item->spec = tag_item;
	addr += sizeof(struct mlx5_rte_flow_item_tag);
	tag_item = (void *)addr;
	tag_item->data = UINT32_MAX;
	tag_item->id = UINT16_MAX;
	item->mask = tag_item;
	item->last = NULL;
	item++;
	item->type = RTE_FLOW_ITEM_TYPE_END;
	return 0;
}

/**
 * The last stage of splitting chain, just creates the subflow
 * without any modification.
 *
 * @param[in] dev
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
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
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
			struct mlx5_flow_split_info *flow_split_info,
			struct rte_flow_error *error)
{
	struct mlx5_flow *dev_flow;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();

	dev_flow = flow_drv_prepare(dev, flow, attr, items, actions,
				    flow_split_info->flow_idx, error);
	if (!dev_flow)
		return -rte_errno;
	dev_flow->flow = flow;
	dev_flow->external = flow_split_info->external;
	dev_flow->skip_scale = flow_split_info->skip_scale;
	/* Subflow object was created, we must include one in the list. */
	SILIST_INSERT(&flow->dev_handles, dev_flow->handle_idx,
		      dev_flow->handle, next);
	/*
	 * If dev_flow is as one of the suffix flow, some actions in suffix
	 * flow may need some user defined item layer flags, and pass the
	 * Metadata rxq mark flag to suffix flow as well.
	 */
	if (flow_split_info->prefix_layers)
		dev_flow->handle->layers = flow_split_info->prefix_layers;
	if (flow_split_info->prefix_mark) {
		MLX5_ASSERT(wks);
		wks->mark = 1;
	}
	if (sub_flow)
		*sub_flow = dev_flow;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	dev_flow->dv.table_id = flow_split_info->table_id;
#endif
	return flow_drv_translate(dev, dev_flow, attr, items, actions, error);
}

/**
 * Get the sub policy of a meter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param wks
 *   Pointer to thread flow work space.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   Pointer to the meter sub policy, NULL otherwise and rte_errno is set.
 */
static struct mlx5_flow_meter_sub_policy *
get_meter_sub_policy(struct rte_eth_dev *dev,
		     struct rte_flow *flow,
		     struct mlx5_flow_workspace *wks,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item items[],
		     struct rte_flow_error *error)
{
	struct mlx5_flow_meter_policy *policy;
	struct mlx5_flow_meter_policy *final_policy;
	struct mlx5_flow_meter_sub_policy *sub_policy = NULL;

	policy = wks->policy;
	final_policy = policy->is_hierarchy ? wks->final_policy : policy;
	if (final_policy->is_rss || final_policy->is_queue) {
		struct mlx5_flow_rss_desc rss_desc_v[MLX5_MTR_RTE_COLORS];
		struct mlx5_flow_rss_desc *rss_desc[MLX5_MTR_RTE_COLORS] = {0};
		uint32_t i;

		/*
		 * This is a tmp dev_flow,
		 * no need to register any matcher for it in translate.
		 */
		wks->skip_matcher_reg = 1;
		for (i = 0; i < MLX5_MTR_RTE_COLORS; i++) {
			struct mlx5_flow dev_flow = {0};
			struct mlx5_flow_handle dev_handle = { {0} };
			uint8_t fate = final_policy->act_cnt[i].fate_action;

			if (fate == MLX5_FLOW_FATE_SHARED_RSS) {
				const struct rte_flow_action_rss *rss_act =
					final_policy->act_cnt[i].rss->conf;
				struct rte_flow_action rss_actions[2] = {
					[0] = {
					.type = RTE_FLOW_ACTION_TYPE_RSS,
					.conf = rss_act,
					},
					[1] = {
					.type = RTE_FLOW_ACTION_TYPE_END,
					.conf = NULL,
					}
				};

				dev_flow.handle = &dev_handle;
				dev_flow.ingress = attr->ingress;
				dev_flow.flow = flow;
				dev_flow.external = 0;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
				dev_flow.dv.transfer = attr->transfer;
#endif
				/**
				 * Translate RSS action to get rss hash fields.
				 */
				if (flow_drv_translate(dev, &dev_flow, attr,
						items, rss_actions, error))
					goto exit;
				rss_desc_v[i] = wks->rss_desc;
				rss_desc_v[i].symmetric_hash_function =
						dev_flow.symmetric_hash_function;
				rss_desc_v[i].key_len = MLX5_RSS_HASH_KEY_LEN;
				rss_desc_v[i].hash_fields =
						dev_flow.hash_fields;
				rss_desc_v[i].queue_num =
						rss_desc_v[i].hash_fields ?
						rss_desc_v[i].queue_num : 1;
				rss_desc_v[i].tunnel =
						!!(dev_flow.handle->layers &
						   MLX5_FLOW_LAYER_TUNNEL);
				/* Use the RSS queues in the containers. */
				rss_desc_v[i].queue =
					(uint16_t *)(uintptr_t)rss_act->queue;
				rss_desc[i] = &rss_desc_v[i];
			} else if (fate == MLX5_FLOW_FATE_QUEUE) {
				/* This is queue action. */
				rss_desc_v[i] = wks->rss_desc;
				rss_desc_v[i].key_len = 0;
				rss_desc_v[i].hash_fields = 0;
				rss_desc_v[i].queue =
					&final_policy->act_cnt[i].queue;
				rss_desc_v[i].queue_num = 1;
				rss_desc[i] = &rss_desc_v[i];
			} else {
				rss_desc[i] = NULL;
			}
		}
		sub_policy = flow_drv_meter_sub_policy_rss_prepare(dev,
						flow, policy, rss_desc);
	} else {
		enum mlx5_meter_domain mtr_domain =
			attr->transfer ? MLX5_MTR_DOMAIN_TRANSFER :
				(attr->egress ? MLX5_MTR_DOMAIN_EGRESS :
						MLX5_MTR_DOMAIN_INGRESS);
		sub_policy = policy->sub_policys[mtr_domain][0];
	}
	if (!sub_policy)
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to get meter sub-policy.");
exit:
	return sub_policy;
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
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param wks
 *   Pointer to thread flow work space.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[out] sfx_items
 *   Suffix flow match items (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] actions_sfx
 *   Suffix flow actions.
 * @param[out] actions_pre
 *   Prefix flow actions.
 * @param[out] mtr_flow_id
 *   Pointer to meter flow id.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_meter_split_prep(struct rte_eth_dev *dev,
		      struct rte_flow *flow,
		      struct mlx5_flow_workspace *wks,
		      const struct rte_flow_attr *attr,
		      const struct rte_flow_item items[],
		      struct rte_flow_item sfx_items[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_action actions_sfx[],
		      struct rte_flow_action actions_pre[],
		      uint32_t *mtr_flow_id,
		      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_meter_info *fm = wks->fm;
	struct rte_flow_action *tag_action = NULL;
	struct rte_flow_item *tag_item;
	struct mlx5_rte_flow_action_set_tag *set_tag;
	const struct rte_flow_action_raw_encap *raw_encap;
	const struct rte_flow_action_raw_decap *raw_decap;
	struct mlx5_rte_flow_item_tag *tag_item_spec;
	struct mlx5_rte_flow_item_tag *tag_item_mask;
	uint32_t tag_id = 0;
	bool vlan_actions;
	struct rte_flow_item *orig_sfx_items = sfx_items;
	const struct rte_flow_item *orig_items = items;
	struct rte_flow_action *hw_mtr_action;
	struct rte_flow_action *action_pre_head = NULL;
	uint16_t flow_src_port = priv->representor_id;
	bool mtr_first;
	uint8_t mtr_id_offset = priv->mtr_reg_share ? MLX5_MTR_COLOR_BITS : 0;
	uint8_t mtr_reg_bits = priv->mtr_reg_share ?
				MLX5_MTR_IDLE_BITS_IN_COLOR_REG : MLX5_REG_BITS;
	uint32_t flow_id = 0;
	uint32_t flow_id_reversed = 0;
	uint8_t flow_id_bits = 0;
	bool after_meter = false;
	int shift;

	/* Prepare the suffix subflow items. */
	tag_item = sfx_items++;
	tag_item->type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG;
	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int item_type = items->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
		case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
			if (mlx5_flow_get_item_vport_id(dev, items, &flow_src_port, NULL, error))
				return -rte_errno;
			if (!fm->def_policy && wks->policy->hierarchy_match_port &&
			    flow_src_port != priv->representor_id) {
				if (flow_drv_mtr_hierarchy_rule_create(dev,
								flow, fm,
								flow_src_port,
								items,
								error))
					return -rte_errno;
			}
			memcpy(sfx_items, items, sizeof(*sfx_items));
			sfx_items++;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			/*
			 * Copy VLAN items in case VLAN actions are performed.
			 * If there are no VLAN actions, these items will be VOID.
			 */
			memcpy(sfx_items, items, sizeof(*sfx_items));
			sfx_items->type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_VLAN;
			sfx_items++;
			break;
		default:
			break;
		}
	}
	sfx_items->type = RTE_FLOW_ITEM_TYPE_END;
	sfx_items++;
	mtr_first = priv->sh->meter_aso_en &&
		(attr->egress || (attr->transfer && flow_src_port != UINT16_MAX));
	/* For ASO meter, meter must be before tag in TX direction. */
	if (mtr_first) {
		action_pre_head = actions_pre++;
		/* Leave space for tag action. */
		tag_action = actions_pre++;
	}
	/* Prepare the actions for prefix and suffix flow. */
	vlan_actions = false;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		struct rte_flow_action *action_cur = NULL;

		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_METER:
			if (mtr_first) {
				action_cur = action_pre_head;
			} else {
				/* Leave space for tag action. */
				tag_action = actions_pre++;
				action_cur = actions_pre++;
			}
			after_meter = true;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			action_cur = actions_pre++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap = actions->conf;
			if (raw_encap->size < MLX5_ENCAPSULATION_DECISION_SIZE)
				action_cur = actions_pre++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			raw_decap = actions->conf;
			if (raw_decap->size > MLX5_ENCAPSULATION_DECISION_SIZE)
				action_cur = actions_pre++;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			vlan_actions = true;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (fm->def_policy)
				action_cur = after_meter ?
						actions_sfx++ : actions_pre++;
			break;
		default:
			break;
		}
		if (!action_cur)
			action_cur = (fm->def_policy) ?
					actions_sfx++ : actions_pre++;
		memcpy(action_cur, actions, sizeof(struct rte_flow_action));
	}
	/* If there are no VLAN actions, convert VLAN items to VOID in suffix flow items. */
	if (!vlan_actions) {
		struct rte_flow_item *it = orig_sfx_items;

		for (; it->type != RTE_FLOW_ITEM_TYPE_END; it++)
			if (it->type == (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_VLAN)
				it->type = RTE_FLOW_ITEM_TYPE_VOID;
	}
	/* Add end action to the actions. */
	actions_sfx->type = RTE_FLOW_ACTION_TYPE_END;
	if (priv->sh->meter_aso_en) {
		/**
		 * For ASO meter, need to add an extra jump action explicitly,
		 * to jump from meter to policer table.
		 */
		struct mlx5_flow_meter_sub_policy *sub_policy;
		struct mlx5_flow_tbl_data_entry *tbl_data;

		if (!fm->def_policy) {
			sub_policy = get_meter_sub_policy(dev, flow, wks,
							  attr, orig_items,
							  error);
			if (!sub_policy)
				return -rte_errno;
		} else {
			enum mlx5_meter_domain mtr_domain =
			attr->transfer ? MLX5_MTR_DOMAIN_TRANSFER :
				(attr->egress ? MLX5_MTR_DOMAIN_EGRESS :
						MLX5_MTR_DOMAIN_INGRESS);

			sub_policy =
			&priv->sh->mtrmng->def_policy[mtr_domain]->sub_policy;
		}
		tbl_data = container_of(sub_policy->tbl_rsc,
					struct mlx5_flow_tbl_data_entry, tbl);
		hw_mtr_action = actions_pre++;
		hw_mtr_action->type = (enum rte_flow_action_type)
				      MLX5_RTE_FLOW_ACTION_TYPE_JUMP;
		hw_mtr_action->conf = tbl_data->jump.action;
	}
	actions_pre->type = RTE_FLOW_ACTION_TYPE_END;
	actions_pre++;
	if (!tag_action)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "No tag action space.");
	if (!mtr_flow_id) {
		tag_action->type = RTE_FLOW_ACTION_TYPE_VOID;
		goto exit;
	}
	/* Only default-policy Meter creates mtr flow id. */
	if (fm->def_policy) {
		mlx5_ipool_malloc(fm->flow_ipool, &tag_id);
		if (!tag_id)
			return rte_flow_error_set(error, ENOMEM,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"Failed to allocate meter flow id.");
		flow_id = tag_id - 1;
		flow_id_bits = (!flow_id) ? 1 :
				(MLX5_REG_BITS - rte_clz32(flow_id));
		if ((flow_id_bits + priv->sh->mtrmng->max_mtr_bits) >
		    mtr_reg_bits) {
			mlx5_ipool_free(fm->flow_ipool, tag_id);
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"Meter flow id exceeds max limit.");
		}
		if (flow_id_bits > priv->sh->mtrmng->max_mtr_flow_bits)
			priv->sh->mtrmng->max_mtr_flow_bits = flow_id_bits;
	}
	/* Build tag actions and items for meter_id/meter flow_id. */
	set_tag = (struct mlx5_rte_flow_action_set_tag *)actions_pre;
	tag_item_spec = (struct mlx5_rte_flow_item_tag *)sfx_items;
	tag_item_mask = tag_item_spec + 1;
	/* Both flow_id and meter_id share the same register. */
	*set_tag = (struct mlx5_rte_flow_action_set_tag) {
		.id = (enum modify_reg)mlx5_flow_get_reg_id(dev, MLX5_MTR_ID,
							    0, error),
		.offset = mtr_id_offset,
		.length = mtr_reg_bits,
		.data = flow->meter,
	};
	/*
	 * The color Reg bits used by flow_id are growing from
	 * msb to lsb, so must do bit reverse for flow_id val in RegC.
	 */
	for (shift = 0; shift < flow_id_bits; shift++)
		flow_id_reversed = (flow_id_reversed << 1) |
				((flow_id >> shift) & 0x1);
	set_tag->data |=
		flow_id_reversed << (mtr_reg_bits - flow_id_bits);
	tag_item_spec->id = set_tag->id;
	tag_item_spec->data = set_tag->data << mtr_id_offset;
	tag_item_mask->data = UINT32_MAX << mtr_id_offset;
	tag_action->type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_TAG;
	tag_action->conf = set_tag;
	tag_item->spec = tag_item_spec;
	tag_item->last = NULL;
	tag_item->mask = tag_item_mask;
exit:
	if (mtr_flow_id)
		*mtr_flow_id = tag_id;
	return 0;
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
 * @param[in] mtr_sfx
 *   Check if it is in meter suffix table.
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
			  int actions_n, int mtr_sfx,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
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
	memcpy(split_actions, actions, sizeof(*split_actions) * actions_n);
	/* Count MLX5_RTE_FLOW_ACTION_TYPE_TAG. */
	++actions_n;
	set_tag = (void *)(split_actions + actions_n);
	/*
	 * If we are not the meter suffix flow, add the tag action.
	 * Since meter suffix flow already has the tag added.
	 */
	if (!mtr_sfx) {
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
		mlx5_ipool_malloc(priv->sh->ipool
				  [MLX5_IPOOL_RSS_EXPANTION_FLOW_ID], &flow_id);
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
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_TAG,
			.conf = set_tag,
		};
	} else {
		/*
		 * If we are the suffix flow of meter, tag already exist.
		 * Set the QUEUE/RSS action to void.
		 */
		split_actions[qrss_idx].type = RTE_FLOW_ACTION_TYPE_VOID;
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
 * @param[in] encap_idx
 *   The encap action index.
 *
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_mreg_tx_copy_prep(struct rte_eth_dev *dev,
		       struct rte_flow_action *ext_actions,
		       const struct rte_flow_action *actions,
		       int actions_n, struct rte_flow_error *error,
		       int encap_idx)
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
	if (encap_idx != 0)
		memcpy(ext_actions, actions, sizeof(*ext_actions) * encap_idx);
	if (encap_idx == actions_n - 1) {
		ext_actions[actions_n - 1] = (struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
			.conf = cp_mreg,
		};
		ext_actions[actions_n] = (struct rte_flow_action){
			.type = RTE_FLOW_ACTION_TYPE_END,
		};
	} else {
		ext_actions[encap_idx] = (struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
			.conf = cp_mreg,
		};
		memcpy(ext_actions + encap_idx + 1, actions + encap_idx,
				sizeof(*ext_actions) * (actions_n - encap_idx));
	}
	return 0;
}

/**
 * Check the match action from the action list.
 *
 * @param[in] actions
 *   Pointer to the list of actions.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] action
 *   The action to be check if exist.
 * @param[out] match_action_pos
 *   Pointer to the position of the matched action if exists, otherwise is -1.
 * @param[out] qrss_action_pos
 *   Pointer to the position of the Queue/RSS action if exists, otherwise is -1.
 * @param[out] modify_after_mirror
 *   Pointer to the flag of modify action after FDB mirroring.
 *
 * @return
 *   > 0 the total number of actions.
 *   0 if not found match action in action list.
 */
static int
flow_check_match_action(const struct rte_flow_action actions[],
			const struct rte_flow_attr *attr,
			enum rte_flow_action_type action,
			int *match_action_pos, int *qrss_action_pos,
			int *modify_after_mirror)
{
	const struct rte_flow_action_sample *sample;
	const struct rte_flow_action_raw_decap *decap;
	const struct rte_flow_action *action_cur = NULL;
	int actions_n = 0;
	uint32_t ratio = 0;
	int sub_type = 0;
	int flag = 0;
	int fdb_mirror = 0;

	*match_action_pos = -1;
	*qrss_action_pos = -1;
	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		if (actions->type == action) {
			flag = 1;
			*match_action_pos = actions_n;
		}
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
			*qrss_action_pos = actions_n;
			break;
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
			sample = actions->conf;
			ratio = sample->ratio;
			sub_type = ((const struct rte_flow_action *)
					(sample->actions))->type;
			if (ratio == 1 && attr->transfer &&
			    sub_type != RTE_FLOW_ACTION_TYPE_END)
				fdb_mirror = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		case RTE_FLOW_ACTION_TYPE_DEC_TTL:
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
		case RTE_FLOW_ACTION_TYPE_INC_TCP_SEQ:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_SEQ:
		case RTE_FLOW_ACTION_TYPE_INC_TCP_ACK:
		case RTE_FLOW_ACTION_TYPE_DEC_TCP_ACK:
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
		case RTE_FLOW_ACTION_TYPE_FLAG:
		case RTE_FLOW_ACTION_TYPE_MARK:
		case RTE_FLOW_ACTION_TYPE_SET_META:
		case RTE_FLOW_ACTION_TYPE_SET_TAG:
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
		case RTE_FLOW_ACTION_TYPE_METER:
			if (fdb_mirror)
				*modify_after_mirror = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			decap = actions->conf;
			action_cur = actions;
			while ((++action_cur)->type == RTE_FLOW_ACTION_TYPE_VOID)
				;
			if (action_cur->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
				const struct rte_flow_action_raw_encap *encap =
								action_cur->conf;
				if (decap->size <=
					MLX5_ENCAPSULATION_DECISION_SIZE &&
				    encap->size >
					MLX5_ENCAPSULATION_DECISION_SIZE)
					/* L3 encap. */
					break;
			}
			if (fdb_mirror)
				*modify_after_mirror = 1;
			break;
		default:
			break;
		}
		actions_n++;
	}
	if (flag && fdb_mirror && !*modify_after_mirror) {
		/* FDB mirroring uses the destination array to implement
		 * instead of FLOW_SAMPLER object.
		 */
		if (sub_type != RTE_FLOW_ACTION_TYPE_END)
			flag = 0;
	}
	/* Count RTE_FLOW_ACTION_TYPE_END. */
	return flag ? actions_n + 1 : 0;
}

#define SAMPLE_SUFFIX_ITEM 3

/**
 * Split the sample flow.
 *
 * As sample flow will split to two sub flow, sample flow with
 * sample action, the other actions will move to new suffix flow.
 *
 * Also add unique tag id with tag action in the sample flow,
 * the same tag id will be as match in the suffix flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] add_tag
 *   Add extra tag action flag.
 * @param[out] sfx_items
 *   Suffix flow match items (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[out] actions_sfx
 *   Suffix flow actions.
 * @param[out] actions_pre
 *   Prefix flow actions.
 * @param[in] actions_n
 *  The total number of actions.
 * @param[in] sample_action_pos
 *   The sample action position.
 * @param[in] qrss_action_pos
 *   The Queue/RSS action position.
 * @param[in] jump_table
 *   Add extra jump action flag.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, or unique flow_id, a negative errno value
 *   otherwise and rte_errno is set.
 */
static int
flow_sample_split_prep(struct rte_eth_dev *dev,
		       int add_tag,
		       const struct rte_flow_item items[],
		       struct rte_flow_item sfx_items[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_action actions_sfx[],
		       struct rte_flow_action actions_pre[],
		       int actions_n,
		       int sample_action_pos,
		       int qrss_action_pos,
		       int jump_table,
		       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rte_flow_action_set_tag *set_tag;
	struct mlx5_rte_flow_item_tag *tag_spec;
	struct mlx5_rte_flow_item_tag *tag_mask;
	struct rte_flow_action_jump *jump_action;
	uint32_t tag_id = 0;
	int append_index = 0;
	int set_tag_idx = -1;
	int index;
	int ret;

	if (sample_action_pos < 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "invalid position of sample "
					  "action in list");
	/* Prepare the actions for prefix and suffix flow. */
	if (add_tag) {
		/* Update the new added tag action index preceding
		 * the PUSH_VLAN or ENCAP action.
		 */
		const struct rte_flow_action_raw_encap *raw_encap;
		const struct rte_flow_action *action = actions;
		int encap_idx;
		int action_idx = 0;
		int raw_decap_idx = -1;
		int push_vlan_idx = -1;
		for (; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
			switch (action->type) {
			case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
				raw_decap_idx = action_idx;
				break;
			case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
				raw_encap = action->conf;
				if (raw_encap->size >
					MLX5_ENCAPSULATION_DECISION_SIZE) {
					encap_idx = raw_decap_idx != -1 ?
						    raw_decap_idx : action_idx;
					if (encap_idx < sample_action_pos &&
					    push_vlan_idx == -1)
						set_tag_idx = encap_idx;
				}
				break;
			case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
				encap_idx = action_idx;
				if (encap_idx < sample_action_pos &&
				    push_vlan_idx == -1)
					set_tag_idx = encap_idx;
				break;
			case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
				if (action_idx < sample_action_pos &&
				    push_vlan_idx == -1) {
					set_tag_idx = action_idx;
					push_vlan_idx = action_idx;
				}
				break;
			default:
				break;
			}
			action_idx++;
		}
	}
	/* Prepare the actions for prefix and suffix flow. */
	if (qrss_action_pos >= 0 && qrss_action_pos < sample_action_pos) {
		index = qrss_action_pos;
		/* Put the preceding the Queue/RSS action into prefix flow. */
		if (index != 0)
			memcpy(actions_pre, actions,
			       sizeof(struct rte_flow_action) * index);
		/* Put others preceding the sample action into prefix flow. */
		if (sample_action_pos > index + 1)
			memcpy(actions_pre + index, actions + index + 1,
			       sizeof(struct rte_flow_action) *
			       (sample_action_pos - index - 1));
		index = sample_action_pos - 1;
		/* Put Queue/RSS action into Suffix flow. */
		memcpy(actions_sfx, actions + qrss_action_pos,
		       sizeof(struct rte_flow_action));
		actions_sfx++;
	} else if (add_tag && set_tag_idx >= 0) {
		if (set_tag_idx > 0)
			memcpy(actions_pre, actions,
			       sizeof(struct rte_flow_action) * set_tag_idx);
		memcpy(actions_pre + set_tag_idx + 1, actions + set_tag_idx,
		       sizeof(struct rte_flow_action) *
		       (sample_action_pos - set_tag_idx));
		index = sample_action_pos;
	} else {
		index = sample_action_pos;
		if (index != 0)
			memcpy(actions_pre, actions,
			       sizeof(struct rte_flow_action) * index);
	}
	/* For CX5, add an extra tag action for NIC-RX and E-Switch ingress.
	 * For CX6DX and above, metadata registers Cx preserve their value,
	 * add an extra tag action for NIC-RX and E-Switch Domain.
	 */
	if (add_tag) {
		/* Prepare the prefix tag action. */
		append_index++;
		set_tag = (void *)(actions_pre + actions_n + append_index);
		/* Trust VF/SF on CX5 not supported meter so that the reserved
		 * metadata regC is REG_NON, back to use application tag
		 * index 0.
		 */
		if (unlikely(priv->sh->registers.aso_reg == REG_NON))
			ret = mlx5_flow_get_reg_id(dev, MLX5_APP_TAG, 0, error);
		else
			ret = mlx5_flow_get_reg_id(dev, MLX5_SAMPLE_ID, 0, error);
		if (ret < 0)
			return ret;
		mlx5_ipool_malloc(priv->sh->ipool
				  [MLX5_IPOOL_RSS_EXPANTION_FLOW_ID], &tag_id);
		*set_tag = (struct mlx5_rte_flow_action_set_tag) {
			.id = ret,
			.data = tag_id,
		};
		/* Prepare the suffix subflow items. */
		tag_spec = (void *)(sfx_items + SAMPLE_SUFFIX_ITEM);
		tag_spec->data = tag_id;
		tag_spec->id = set_tag->id;
		tag_mask = tag_spec + 1;
		tag_mask->data = UINT32_MAX;
		for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
			if (items->type == RTE_FLOW_ITEM_TYPE_PORT_ID ||
			    items->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR ||
			    items->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT) {
				memcpy(sfx_items, items, sizeof(*sfx_items));
				sfx_items++;
				break;
			}
		}
		sfx_items[0] = (struct rte_flow_item){
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = tag_spec,
			.last = NULL,
			.mask = tag_mask,
		};
		sfx_items[1] = (struct rte_flow_item){
			.type = (enum rte_flow_item_type)
				RTE_FLOW_ITEM_TYPE_END,
		};
		/* Prepare the tag action in prefix subflow. */
		set_tag_idx = (set_tag_idx == -1) ? index : set_tag_idx;
		actions_pre[set_tag_idx] =
			(struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_TAG,
			.conf = set_tag,
		};
		/* Update next sample position due to add one tag action */
		index += 1;
	}
	/* Copy the sample action into prefix flow. */
	memcpy(actions_pre + index, actions + sample_action_pos,
	       sizeof(struct rte_flow_action));
	index += 1;
	/* For the modify action after the sample action in E-Switch mirroring,
	 * Add the extra jump action in prefix subflow and jump into the next
	 * table, then do the modify action in the new table.
	 */
	if (jump_table) {
		/* Prepare the prefix jump action. */
		append_index++;
		jump_action = (void *)(actions_pre + actions_n + append_index);
		jump_action->group = jump_table;
		actions_pre[index++] =
			(struct rte_flow_action){
			.type = (enum rte_flow_action_type)
				RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = jump_action,
		};
	}
	actions_pre[index] = (struct rte_flow_action){
		.type = (enum rte_flow_action_type)
			RTE_FLOW_ACTION_TYPE_END,
	};
	/* Put the actions after sample into Suffix flow. */
	memcpy(actions_sfx, actions + sample_action_pos + 1,
	       sizeof(struct rte_flow_action) *
	       (actions_n - sample_action_pos - 1));
	return tag_id;
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
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
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
			   struct mlx5_flow_split_info *flow_split_info,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	const struct rte_flow_action *qrss = NULL;
	struct rte_flow_action *ext_actions = NULL;
	struct mlx5_flow *dev_flow = NULL;
	uint32_t qrss_id = 0;
	int mtr_sfx = 0;
	size_t act_size;
	int actions_n;
	int encap_idx;
	int ret;

	/* Check whether extensive metadata feature is engaged. */
	if (!config->dv_flow_en ||
	    config->dv_xmeta_en == MLX5_XMETA_MODE_LEGACY ||
	    !mlx5_flow_ext_mreg_supported(dev))
		return flow_create_split_inner(dev, flow, NULL, attr, items,
					       actions, flow_split_info, error);
	actions_n = flow_parse_metadata_split_actions_info(actions, &qrss,
							   &encap_idx);
	if (qrss) {
		/* Exclude hairpin flows from splitting. */
		if (qrss->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
			const struct rte_flow_action_queue *queue;

			queue = qrss->conf;
			if (mlx5_rxq_is_hairpin(dev, queue->index))
				qrss = NULL;
		} else if (qrss->type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss;

			rss = qrss->conf;
			if (mlx5_rxq_is_hairpin(dev, rss->queue[0]))
				qrss = NULL;
		}
	}
	if (qrss) {
		/* Check if it is in meter suffix table. */
		mtr_sfx = attr->group ==
			  ((attr->transfer && priv->fdb_def_rule) ?
			  (MLX5_FLOW_TABLE_LEVEL_METER - 1) :
			  MLX5_FLOW_TABLE_LEVEL_METER);
		/*
		 * Q/RSS action on NIC Rx should be split in order to pass by
		 * the mreg copy table (RX_CP_TBL) and then it jumps to the
		 * action table (RX_ACT_TBL) which has the split Q/RSS action.
		 */
		act_size = sizeof(struct rte_flow_action) * (actions_n + 1) +
			   sizeof(struct rte_flow_action_set_tag) +
			   sizeof(struct rte_flow_action_jump);
		ext_actions = mlx5_malloc(MLX5_MEM_ZERO, act_size, 0,
					  SOCKET_ID_ANY);
		if (!ext_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "metadata flow");
		/*
		 * Create the new actions list with removed Q/RSS action
		 * and appended set tag and jump to register copy table
		 * (RX_CP_TBL). We should preallocate unique tag ID here
		 * in advance, because it is needed for set tag action.
		 */
		qrss_id = flow_mreg_split_qrss_prep(dev, ext_actions, actions,
						    qrss, actions_n,
						    mtr_sfx, error);
		if (!mtr_sfx && !qrss_id) {
			ret = -rte_errno;
			goto exit;
		}
	} else if (attr->egress) {
		/*
		 * All the actions on NIC Tx should have a metadata register
		 * copy action to copy reg_a from WQE to reg_c[meta]
		 */
		act_size = sizeof(struct rte_flow_action) * (actions_n + 1) +
			   sizeof(struct mlx5_flow_action_copy_mreg);
		ext_actions = mlx5_malloc(MLX5_MEM_ZERO, act_size, 0,
					  SOCKET_ID_ANY);
		if (!ext_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "metadata flow");
		/* Create the action list appended with copy register. */
		ret = flow_mreg_tx_copy_prep(dev, ext_actions, actions,
					     actions_n, error, encap_idx);
		if (ret < 0)
			goto exit;
	}
	/* Add the unmodified original or prefix subflow. */
	ret = flow_create_split_inner(dev, flow, &dev_flow, attr,
				      items, ext_actions ? ext_actions :
				      actions, flow_split_info, error);
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
			.id = REG_NON,
		};
		struct rte_flow_item q_items[] = {
			{
				.type = (enum rte_flow_item_type)
					MLX5_RTE_FLOW_ITEM_TYPE_TAG,
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
		uint64_t layers = flow_get_prefix_layer_flags(dev_flow);

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
			dev_flow->handle->split_flow_id = qrss_id;
			ret = mlx5_flow_get_reg_id(dev, MLX5_COPY_MARK, 0,
						   error);
			if (ret < 0)
				goto exit;
			q_tag_spec.id = ret;
		}
		dev_flow = NULL;
		/* Add suffix subflow to execute Q/RSS. */
		flow_split_info->prefix_layers = layers;
		flow_split_info->prefix_mark = 0;
		flow_split_info->table_id = 0;
		ret = flow_create_split_inner(dev, flow, &dev_flow,
					      &q_attr, mtr_sfx ? items :
					      q_items, q_actions,
					      flow_split_info, error);
		if (ret < 0)
			goto exit;
		/* qrss ID should be freed if failed. */
		qrss_id = 0;
		MLX5_ASSERT(dev_flow);
	}

exit:
	/*
	 * We do not destroy the partially created sub_flows in case of error.
	 * These ones are included into parent flow list and will be destroyed
	 * by flow_drv_destroy.
	 */
	mlx5_ipool_free(priv->sh->ipool[MLX5_IPOOL_RSS_EXPANTION_FLOW_ID],
			qrss_id);
	mlx5_free(ext_actions);
	return ret;
}

/**
 * Create meter internal drop flow with the original pattern.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow
 *   Parent flow structure pointer.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
 * @param[in] fm
 *   Pointer to flow meter structure.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static uint32_t
flow_meter_create_drop_flow_with_org_pattern(struct rte_eth_dev *dev,
			struct rte_flow *flow,
			const struct rte_flow_attr *attr,
			const struct rte_flow_item items[],
			struct mlx5_flow_split_info *flow_split_info,
			struct mlx5_flow_meter_info *fm,
			struct rte_flow_error *error)
{
	struct mlx5_flow *dev_flow = NULL;
	struct rte_flow_attr drop_attr = *attr;
	struct rte_flow_action drop_actions[3];
	struct mlx5_flow_split_info drop_split_info = *flow_split_info;

	MLX5_ASSERT(fm->drop_cnt);
	drop_actions[0].type =
		(enum rte_flow_action_type)MLX5_RTE_FLOW_ACTION_TYPE_COUNT;
	drop_actions[0].conf = (void *)(uintptr_t)fm->drop_cnt;
	drop_actions[1].type = RTE_FLOW_ACTION_TYPE_DROP;
	drop_actions[1].conf = NULL;
	drop_actions[2].type = RTE_FLOW_ACTION_TYPE_END;
	drop_actions[2].conf = NULL;
	drop_split_info.external = false;
	drop_split_info.skip_scale |= 1 << MLX5_SCALE_FLOW_GROUP_BIT;
	drop_split_info.table_id = MLX5_MTR_TABLE_ID_DROP;
	drop_attr.group = MLX5_FLOW_TABLE_LEVEL_METER;
	return flow_create_split_inner(dev, flow, &dev_flow,
				&drop_attr, items, drop_actions,
				&drop_split_info, error);
}

static int
flow_count_vlan_items(const struct rte_flow_item items[])
{
	int items_n = 0;

	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		if (items->type == RTE_FLOW_ITEM_TYPE_VLAN ||
		    items->type == (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_VLAN)
			items_n++;
	}
	return items_n;
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
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
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
			struct mlx5_flow_split_info *flow_split_info,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
	struct rte_flow_action *sfx_actions = NULL;
	struct rte_flow_action *pre_actions = NULL;
	struct rte_flow_item *sfx_items = NULL;
	struct mlx5_flow *dev_flow = NULL;
	struct rte_flow_attr sfx_attr = *attr;
	struct mlx5_flow_meter_info *fm = NULL;
	uint8_t skip_scale_restore;
	bool has_mtr = false;
	bool has_modify = false;
	bool set_mtr_reg = true;
	bool is_mtr_hierarchy = false;
	uint32_t meter_id = 0;
	uint32_t mtr_idx = 0;
	uint32_t mtr_flow_id = 0;
	size_t act_size;
	size_t item_size;
	int actions_n = 0;
	int vlan_items_n = 0;
	int ret = 0;

	if (priv->mtr_en)
		actions_n = flow_check_meter_action(dev, actions, &has_mtr,
						    &has_modify, &meter_id);
	if (has_mtr) {
		if (flow->meter) {
			fm = flow_dv_meter_find_by_idx(priv, flow->meter);
			if (!fm)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL, "Meter not found.");
		} else {
			fm = mlx5_flow_meter_find(priv, meter_id, &mtr_idx);
			if (!fm)
				return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL, "Meter not found.");
			ret = mlx5_flow_meter_attach(priv, fm,
						     &sfx_attr, error);
			if (ret)
				return -rte_errno;
			flow->meter = mtr_idx;
		}
		MLX5_ASSERT(wks);
		wks->fm = fm;
		if (!fm->def_policy) {
			wks->policy = mlx5_flow_meter_policy_find(dev,
								  fm->policy_id,
								  NULL);
			MLX5_ASSERT(wks->policy);
			if (wks->policy->mark)
				wks->mark = 1;
			if (wks->policy->is_hierarchy) {
				wks->final_policy =
				mlx5_flow_meter_hierarchy_get_final_policy(dev,
								wks->policy);
				if (!wks->final_policy)
					return rte_flow_error_set(error,
					EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				"Failed to find terminal policy of hierarchy.");
				is_mtr_hierarchy = true;
			}
		}
		/*
		 * If it isn't default-policy Meter, and
		 * 1. Not meter hierarchy and there's no action in flow to change
		 *    packet (modify/encap/decap etc.), OR
		 * 2. No drop count needed for this meter.
		 * Then no need to use regC to save meter id anymore.
		 */
		if (!fm->def_policy && ((!has_modify && !is_mtr_hierarchy) || !fm->drop_cnt))
			set_mtr_reg = false;
		/* Prefix actions: meter, decap, encap, tag, jump, end, cnt. */
#define METER_PREFIX_ACTION 7
		act_size = (sizeof(struct rte_flow_action) *
			    (actions_n + METER_PREFIX_ACTION)) +
			   sizeof(struct mlx5_rte_flow_action_set_tag);
		/* Flow can have multiple VLAN items. Account for them in suffix items. */
		vlan_items_n = flow_count_vlan_items(items);
		/* Suffix items: tag, [vlans], port id, end. */
#define METER_SUFFIX_ITEM 3
		item_size = sizeof(struct rte_flow_item) * (METER_SUFFIX_ITEM + vlan_items_n) +
			    sizeof(struct mlx5_rte_flow_item_tag) * 2;
		sfx_actions = mlx5_malloc(MLX5_MEM_ZERO, (act_size + item_size),
					  0, SOCKET_ID_ANY);
		if (!sfx_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "meter flow");
		sfx_items = (struct rte_flow_item *)((char *)sfx_actions +
			     act_size);
		/* There's no suffix flow for meter of non-default policy. */
		if (!fm->def_policy)
			pre_actions = sfx_actions + 1;
		else
			pre_actions = sfx_actions + actions_n;
		ret = flow_meter_split_prep(dev, flow, wks, &sfx_attr,
					    items, sfx_items, actions,
					    sfx_actions, pre_actions,
					    (set_mtr_reg ? &mtr_flow_id : NULL),
					    error);
		if (ret) {
			ret = -rte_errno;
			goto exit;
		}
		/* Add the prefix subflow. */
		skip_scale_restore = flow_split_info->skip_scale;
		flow_split_info->skip_scale |=
			1 << MLX5_SCALE_JUMP_FLOW_GROUP_BIT;
		ret = flow_create_split_inner(dev, flow, &dev_flow,
					      attr, items, pre_actions,
					      flow_split_info, error);
		flow_split_info->skip_scale = skip_scale_restore;
		if (ret) {
			if (mtr_flow_id)
				mlx5_ipool_free(fm->flow_ipool, mtr_flow_id);
			ret = -rte_errno;
			goto exit;
		}
		if (mtr_flow_id) {
			dev_flow->handle->split_flow_id = mtr_flow_id;
			dev_flow->handle->is_meter_flow_id = 1;
		}
		if (!fm->def_policy) {
			if (!set_mtr_reg && fm->drop_cnt)
				ret =
			flow_meter_create_drop_flow_with_org_pattern(dev, flow,
							&sfx_attr, items,
							flow_split_info,
							fm, error);
			goto exit;
		}
		/* Setting the sfx group atrr. */
		sfx_attr.group = sfx_attr.transfer ?
				(MLX5_FLOW_TABLE_LEVEL_METER - 1) :
				 MLX5_FLOW_TABLE_LEVEL_METER;
		flow_split_info->prefix_layers =
				flow_get_prefix_layer_flags(dev_flow);
		flow_split_info->prefix_mark |= wks->mark;
		flow_split_info->table_id = MLX5_MTR_TABLE_ID_SUFFIX;
	}
	/* Add the prefix subflow. */
	ret = flow_create_split_metadata(dev, flow,
					 &sfx_attr, sfx_items ?
					 sfx_items : items,
					 sfx_actions ? sfx_actions : actions,
					 flow_split_info, error);
exit:
	if (sfx_actions)
		mlx5_free(sfx_actions);
	return ret;
}

/**
 * The splitting for sample feature.
 *
 * Once Sample action is detected in the action list, the flow actions should
 * be split into prefix sub flow and suffix sub flow.
 *
 * The original items remain in the prefix sub flow, all actions preceding the
 * sample action and the sample action itself will be copied to the prefix
 * sub flow, the actions following the sample action will be copied to the
 * suffix sub flow, Queue action always be located in the suffix sub flow.
 *
 * In order to make the packet from prefix sub flow matches with suffix sub
 * flow, an extra tag action be added into prefix sub flow, and the suffix sub
 * flow uses tag item with the unique flow id.
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
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 * @return
 *   0 on success, negative value otherwise
 */
static int
flow_create_split_sample(struct rte_eth_dev *dev,
			 struct rte_flow *flow,
			 const struct rte_flow_attr *attr,
			 const struct rte_flow_item items[],
			 const struct rte_flow_action actions[],
			 struct mlx5_flow_split_info *flow_split_info,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_action *sfx_actions = NULL;
	struct rte_flow_action *pre_actions = NULL;
	struct rte_flow_item *sfx_items = NULL;
	struct mlx5_flow *dev_flow = NULL;
	struct rte_flow_attr sfx_attr = *attr;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_flow_dv_sample_resource *sample_res;
	struct mlx5_flow_tbl_data_entry *sfx_tbl_data;
	struct mlx5_flow_tbl_resource *sfx_tbl;
	struct mlx5_flow_workspace *wks = mlx5_flow_get_thread_workspace();
#endif
	size_t act_size;
	size_t item_size;
	uint32_t fdb_tx = 0;
	int32_t tag_id = 0;
	int actions_n = 0;
	int sample_action_pos;
	int qrss_action_pos;
	int add_tag = 0;
	int modify_after_mirror = 0;
	uint16_t jump_table = 0;
	const uint32_t next_ft_step = 1;
	int ret = 0;
	struct mlx5_priv *item_port_priv = NULL;
	const struct rte_flow_item *item;

	if (priv->sampler_en)
		actions_n = flow_check_match_action(actions, attr,
					RTE_FLOW_ACTION_TYPE_SAMPLE,
					&sample_action_pos, &qrss_action_pos,
					&modify_after_mirror);
	if (actions_n) {
		/* The prefix actions must includes sample, tag, end. */
		act_size = sizeof(struct rte_flow_action) * (actions_n * 2 + 1)
			   + sizeof(struct mlx5_rte_flow_action_set_tag);
		item_size = sizeof(struct rte_flow_item) * SAMPLE_SUFFIX_ITEM +
			    sizeof(struct mlx5_rte_flow_item_tag) * 2;
		sfx_actions = mlx5_malloc(MLX5_MEM_ZERO, (act_size +
					  item_size), 0, SOCKET_ID_ANY);
		if (!sfx_actions)
			return rte_flow_error_set(error, ENOMEM,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL, "no memory to split "
						  "sample flow");
		for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
			if (item->type == RTE_FLOW_ITEM_TYPE_PORT_ID) {
				const struct rte_flow_item_port_id *spec;

				spec = (const struct rte_flow_item_port_id *)item->spec;
				if (spec)
					item_port_priv =
						mlx5_port_to_eswitch_info(spec->id, true);
				break;
			} else if (item->type == RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT) {
				const struct rte_flow_item_ethdev *spec;

				spec = (const struct rte_flow_item_ethdev *)item->spec;
				if (spec)
					item_port_priv =
						mlx5_port_to_eswitch_info(spec->port_id, true);
				break;
			} else if (item->type == RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR) {
				const struct rte_flow_item_ethdev *spec;

				spec = (const struct rte_flow_item_ethdev *)item->spec;
				if (spec)
					item_port_priv =
						mlx5_port_to_eswitch_info(spec->port_id, true);
				break;
			}
		}
		/* The representor_id is UINT16_MAX for uplink. */
		fdb_tx = (attr->transfer &&
			  flow_source_vport_representor(priv, item_port_priv));
		/*
		 * When reg_c_preserve is set, metadata registers Cx preserve
		 * their value even through packet duplication.
		 */
		add_tag = (!fdb_tx ||
			   priv->sh->cdev->config.hca_attr.reg_c_preserve);
		if (add_tag)
			sfx_items = (struct rte_flow_item *)((char *)sfx_actions
					+ act_size);
		if (modify_after_mirror)
			jump_table = attr->group * MLX5_FLOW_TABLE_FACTOR +
				     next_ft_step;
		pre_actions = sfx_actions + actions_n;
		tag_id = flow_sample_split_prep(dev, add_tag, items, sfx_items,
						actions, sfx_actions,
						pre_actions, actions_n,
						sample_action_pos,
						qrss_action_pos, jump_table,
						error);
		if (tag_id < 0 || (add_tag && !tag_id)) {
			ret = -rte_errno;
			goto exit;
		}
		if (modify_after_mirror)
			flow_split_info->skip_scale =
					1 << MLX5_SCALE_JUMP_FLOW_GROUP_BIT;
		/* Add the prefix subflow. */
		ret = flow_create_split_inner(dev, flow, &dev_flow, attr,
					      items, pre_actions,
					      flow_split_info, error);
		if (ret) {
			ret = -rte_errno;
			goto exit;
		}
		dev_flow->handle->split_flow_id = tag_id;
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		if (!modify_after_mirror) {
			/* Set the sfx group attr. */
			sample_res = (struct mlx5_flow_dv_sample_resource *)
						dev_flow->dv.sample_res;
			sfx_tbl = (struct mlx5_flow_tbl_resource *)
						sample_res->normal_path_tbl;
			sfx_tbl_data = container_of(sfx_tbl,
						struct mlx5_flow_tbl_data_entry,
						tbl);
			sfx_attr.group = sfx_attr.transfer ?
			(sfx_tbl_data->level - 1) : sfx_tbl_data->level;
		} else {
			MLX5_ASSERT(attr->transfer);
			sfx_attr.group = jump_table;
		}
		flow_split_info->prefix_layers =
				flow_get_prefix_layer_flags(dev_flow);
		MLX5_ASSERT(wks);
		flow_split_info->prefix_mark |= wks->mark;
		/* Suffix group level already be scaled with factor, set
		 * MLX5_SCALE_FLOW_GROUP_BIT of skip_scale to 1 to avoid scale
		 * again in translation.
		 */
		flow_split_info->skip_scale = 1 << MLX5_SCALE_FLOW_GROUP_BIT;
#endif
	}
	/* Add the suffix subflow. */
	ret = flow_create_split_meter(dev, flow, &sfx_attr,
				      sfx_items ? sfx_items : items,
				      sfx_actions ? sfx_actions : actions,
				      flow_split_info, error);
exit:
	if (sfx_actions)
		mlx5_free(sfx_actions);
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
 * @param[in] flow_split_info
 *   Pointer to flow split info structure.
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
			struct mlx5_flow_split_info *flow_split_info,
			struct rte_flow_error *error)
{
	int ret;

	ret = flow_create_split_sample(dev, flow, attr, items,
				       actions, flow_split_info, error);
	MLX5_ASSERT(ret <= 0);
	return ret;
}

static inline struct mlx5_flow_tunnel *
flow_tunnel_from_rule(const struct mlx5_flow *flow)
{
	struct mlx5_flow_tunnel *tunnel;

	tunnel = RTE_PTR_UNQUAL(flow->tunnel);

	return tunnel;
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
 *   A flow index on success, 0 otherwise and rte_errno is set.
 */
uintptr_t
flow_legacy_list_create(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item items[],
		    const struct rte_flow_action original_actions[],
		    bool external, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = NULL;
	struct mlx5_flow *dev_flow;
	const struct rte_flow_action_rss *rss = NULL;
	struct mlx5_translated_action_handle
		indir_actions[MLX5_MAX_INDIRECT_ACTIONS];
	int indir_actions_n = MLX5_MAX_INDIRECT_ACTIONS;
	union {
		struct mlx5_flow_expand_rss buf;
		uint8_t buffer[8192];
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
	struct mlx5_rte_flow_item_sq sq_specs[RTE_MAX_QUEUES_PER_PORT];
	struct mlx5_flow_expand_rss *buf = &expand_buffer.buf;
	struct mlx5_flow_rss_desc *rss_desc;
	const struct rte_flow_action *p_actions_rx;
	uint32_t i;
	uint32_t idx = 0;
	int hairpin_flow;
	struct rte_flow_attr attr_tx = { .priority = 0 };
	const struct rte_flow_action *actions;
	struct rte_flow_action *translated_actions = NULL;
	struct mlx5_flow_tunnel *tunnel;
	struct tunnel_default_miss_ctx default_miss_ctx = { 0, };
	struct mlx5_flow_workspace *wks = mlx5_flow_push_thread_workspace();
	struct mlx5_flow_split_info flow_split_info = {
		.external = !!external,
		.skip_scale = 0,
		.flow_idx = 0,
		.prefix_mark = 0,
		.prefix_layers = 0,
		.table_id = 0
	};
	int ret;
	struct mlx5_shared_action_rss *shared_rss_action;

	if (!wks)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to push flow workspace");
	memset(indir_actions, 0, sizeof(indir_actions));
	rss_desc = &wks->rss_desc;
	ret = flow_action_handles_translate(dev, original_actions,
					    indir_actions,
					    &indir_actions_n,
					    &translated_actions, error);
	if (ret < 0) {
		MLX5_ASSERT(translated_actions == NULL);
		return 0;
	}
	actions = translated_actions ? translated_actions : original_actions;
	p_actions_rx = actions;
	hairpin_flow = flow_check_hairpin_split(dev, attr, actions);
	ret = flow_drv_validate(dev, attr, items, p_actions_rx,
				external, hairpin_flow, error);
	if (ret < 0)
		goto error_before_hairpin_split;
	flow = mlx5_ipool_zmalloc(priv->flows[type], &idx);
	if (!flow) {
		rte_errno = ENOMEM;
		goto error_before_hairpin_split;
	}
	if (hairpin_flow > 0) {
		if (hairpin_flow > MLX5_MAX_SPLIT_ACTIONS) {
			rte_errno = EINVAL;
			goto error_before_hairpin_split;
		}
		flow_hairpin_split(dev, actions, actions_rx.actions,
				   actions_hairpin_tx.actions, items_tx.items,
				   idx);
		p_actions_rx = actions_rx.actions;
	}
	flow_split_info.flow_idx = idx;
	flow->drv_type = flow_get_drv_type(dev, attr);
	MLX5_ASSERT(flow->drv_type > MLX5_FLOW_TYPE_MIN &&
		    flow->drv_type < MLX5_FLOW_TYPE_MAX);
	memset(rss_desc, 0, offsetof(struct mlx5_flow_rss_desc, queue));
	/* RSS Action only works on NIC RX domain */
	if (attr->ingress)
		rss = flow_get_rss_action(dev, p_actions_rx);
	if (rss) {
		MLX5_ASSERT(rss->queue_num <= RTE_ETH_RSS_RETA_SIZE_512);
		rss_desc->symmetric_hash_function = MLX5_RSS_IS_SYMM(rss->func);
		/*
		 * The following information is required by
		 * mlx5_flow_hashfields_adjust() in advance.
		 */
		rss_desc->level = rss->level;
		/* RSS type 0 indicates default RSS type (RTE_ETH_RSS_IP). */
		rss_desc->types = !rss->types ? RTE_ETH_RSS_IP : rss->types;
	}
	flow->dev_handles = 0;
	if (rss && rss->types) {
		unsigned int graph_root;

		graph_root = find_graph_root(rss->level);
		ret = mlx5_flow_expand_rss(buf, sizeof(expand_buffer.buffer),
					   items, rss->types,
					   mlx5_support_expansion, graph_root);
		MLX5_ASSERT(ret > 0 &&
		       (unsigned int)ret < sizeof(expand_buffer.buffer));
		if (rte_log_can_log(mlx5_logtype, RTE_LOG_DEBUG)) {
			for (i = 0; i < buf->entries; ++i)
				mlx5_dbg__print_pattern(buf->entry[i].pattern);
		}
	} else {
		ret = mlx5_flow_expand_sqn((struct mlx5_flow_expand_sqn *)buf,
					   sizeof(expand_buffer.buffer),
					   items, sq_specs);
		if (ret) {
			rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_HANDLE,
					   NULL, "not enough memory for rte_flow");
			goto error;
		}
		if (buf->entries == 0) {
			buf->entries = 1;
			buf->entry[0].pattern = (void *)(uintptr_t)items;
		}
	}
	rss_desc->shared_rss = flow_get_shared_rss_action(dev, indir_actions,
						      indir_actions_n);
	for (i = 0; i < buf->entries; ++i) {
		/* Initialize flow split data. */
		flow_split_info.prefix_layers = 0;
		flow_split_info.prefix_mark = 0;
		flow_split_info.skip_scale = 0;
		/*
		 * The splitter may create multiple dev_flows,
		 * depending on configuration. In the simplest
		 * case it just creates unmodified original flow.
		 */
		ret = flow_create_split_outer(dev, flow, attr,
					      buf->entry[i].pattern,
					      p_actions_rx, &flow_split_info,
					      error);
		if (ret < 0)
			goto error;
		if (is_flow_tunnel_steer_rule(wks->flows[0].tof_type)) {
			ret = flow_tunnel_add_default_miss(dev, flow, attr,
							   p_actions_rx,
							   idx,
							   wks->flows[0].tunnel,
							   &default_miss_ctx,
							   error);
			if (ret < 0) {
				mlx5_free(default_miss_ctx.queue);
				goto error;
			}
		}
	}
	/* Create the tx flow. */
	if (hairpin_flow) {
		attr_tx.group = MLX5_HAIRPIN_TX_TABLE;
		attr_tx.ingress = 0;
		attr_tx.egress = 1;
		dev_flow = flow_drv_prepare(dev, flow, &attr_tx, items_tx.items,
					 actions_hairpin_tx.actions,
					 idx, error);
		if (!dev_flow)
			goto error;
		dev_flow->flow = flow;
		dev_flow->external = 0;
		SILIST_INSERT(&flow->dev_handles, dev_flow->handle_idx,
			      dev_flow->handle, next);
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
	 * The table is updated for ingress and transfer flows only, because
	 * the egress Flows belong to the different device and
	 * copy table should be updated in peer NIC Rx domain.
	 */
	if ((attr->ingress || attr->transfer) &&
	    (external || attr->group != MLX5_FLOW_MREG_CP_TABLE_GROUP)) {
		ret = flow_mreg_update_copy_table(dev, flow, actions, error);
		if (ret)
			goto error;
	}
	/*
	 * If the flow is external (from application) OR device is started,
	 * OR mreg discover, then apply immediately.
	 */
	if (external || dev->data->dev_started ||
	    (attr->group == MLX5_FLOW_MREG_CP_TABLE_GROUP &&
	     attr->priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR)) {
		ret = flow_drv_apply(dev, flow, error);
		if (ret < 0)
			goto error;
	}
	flow->type = type;
	flow_rxq_flags_set(dev, flow);
	rte_free(translated_actions);
	tunnel = flow_tunnel_from_rule(wks->flows);
	if (tunnel) {
		flow->tunnel = 1;
		flow->tunnel_id = tunnel->tunnel_id;
		rte_atomic_fetch_add_explicit(&tunnel->refctn, 1, rte_memory_order_relaxed);
		mlx5_free(default_miss_ctx.queue);
	}
	mlx5_flow_pop_thread_workspace();
	return idx;
error:
	MLX5_ASSERT(flow);
	ret = rte_errno; /* Save rte_errno before cleanup. */
	flow_mreg_del_copy_action(dev, flow);
	flow_drv_destroy(dev, flow);

	if (rss_desc->shared_rss) {
		shared_rss_action = (struct mlx5_shared_action_rss *)
			mlx5_ipool_get
			(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
			rss_desc->shared_rss);
		if (shared_rss_action)
			rte_atomic_fetch_sub_explicit(&(shared_rss_action)->refcnt, 1,
						      rte_memory_order_relaxed);
	}
	mlx5_ipool_free(priv->flows[type], idx);
	rte_errno = ret; /* Restore rte_errno. */
	ret = rte_errno;
	rte_errno = ret;
error_before_hairpin_split:
	mlx5_flow_pop_thread_workspace();
	rte_free(translated_actions);
	return 0;
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
		.ingress = 0,
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
	struct rte_flow_error error;

	return (void *)(uintptr_t)mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL,
						   &attr, &pattern,
						   actions, false, &error);
}

/**
 * Create a dedicated flow rule on e-switch table 1, matches ESW manager
 * and sq number, directs all packets to peer vport.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param sq_num
 *   SQ number.
 *
 * @return
 *   Flow ID on success, 0 otherwise and rte_errno is set.
 */
uint32_t
mlx5_flow_create_devx_sq_miss_flow(struct rte_eth_dev *dev, uint32_t sq_num)
{
	struct rte_flow_attr attr = {
		.group = 0,
		.priority = MLX5_FLOW_LOWEST_PRIO_INDICATOR,
		.ingress = 0,
		.egress = 0,
		.transfer = 1,
	};
	struct rte_flow_item_port_id port_spec = {
		.id = MLX5_PORT_ESW_MGR,
	};
	struct mlx5_rte_flow_item_sq sq_spec = {
		.queue = sq_num,
	};
	struct rte_flow_item pattern[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_PORT_ID,
			.spec = &port_spec,
		},
		{
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_SQ,
			.spec = &sq_spec,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action_jump jump = {
		.group = 1,
	};
	struct rte_flow_action_port_id port = {
		.id = dev->data->port_id,
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow_error error;

	/*
	 * Creates group 0, highest priority jump flow.
	 * Matches txq to bypass kernel packets.
	 */
	if (mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL, &attr, pattern, actions,
			     false, &error) == 0)
		return 0;
	/* Create group 1, lowest priority redirect flow for txq. */
	attr.group = 1;
	actions[0].conf = &port;
	actions[0].type = RTE_FLOW_ACTION_TYPE_PORT_ID;
	return mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL, &attr, pattern,
				actions, false, &error);
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
		   const struct rte_flow_action original_actions[],
		   struct rte_flow_error *error)
{
	int hairpin_flow;
	struct mlx5_translated_action_handle
		indir_actions[MLX5_MAX_INDIRECT_ACTIONS];
	int indir_actions_n = MLX5_MAX_INDIRECT_ACTIONS;
	const struct rte_flow_action *actions;
	struct rte_flow_action *translated_actions = NULL;
	int ret = flow_action_handles_translate(dev, original_actions,
						indir_actions,
						&indir_actions_n,
						&translated_actions, error);

	if (ret)
		return ret;
	actions = translated_actions ? translated_actions : original_actions;
	hairpin_flow = flow_check_hairpin_split(dev, attr, actions);
	ret = flow_drv_validate(dev, attr, items, actions,
				true, hairpin_flow, error);
	rte_free(translated_actions);
	return ret;
}

static int
mlx5_flow_cache_flow_info(struct rte_eth_dev *dev,
			  const struct rte_flow_attr *attr,
			  const uint32_t orig_prio,
			  const struct rte_flow_item *items,
			  const struct rte_flow_action *actions,
			  uint32_t flow_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_pmd_mlx5_flow_engine_mode_info *mode_info = &priv->mode_info;
	struct mlx5_dv_flow_info *flow_info, *tmp_info;
	struct rte_flow_error error;
	int len, ret;

	flow_info = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*flow_info), 0, SOCKET_ID_ANY);
	if (!flow_info) {
		DRV_LOG(ERR, "No enough memory for flow_info caching.");
		return -1;
	}
	flow_info->orig_prio = orig_prio;
	flow_info->attr = *attr;
	/* Standby mode rule awlays saves it in low priority entry. */
	flow_info->flow_idx_low_prio = flow_idx;

	/* Store matching items. */
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, NULL, 0, items, &error);
	if (ret <= 0) {
		DRV_LOG(ERR, "Can't get items length.");
		goto end;
	}
	len = RTE_ALIGN(ret, 16);
	flow_info->items = mlx5_malloc(MLX5_MEM_ZERO, len, 0, SOCKET_ID_ANY);
	if (!flow_info->items) {
		DRV_LOG(ERR, "No enough memory for items caching.");
		goto end;
	}
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, flow_info->items, ret, items, &error);
	if (ret <= 0) {
		DRV_LOG(ERR, "Can't duplicate items.");
		goto end;
	}

	/* Store flow actions. */
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, actions, &error);
	if (ret <= 0) {
		DRV_LOG(ERR, "Can't get actions length.");
		goto end;
	}
	len = RTE_ALIGN(ret, 16);
	flow_info->actions = mlx5_malloc(MLX5_MEM_ZERO, len, 0, SOCKET_ID_ANY);
	if (!flow_info->actions) {
		DRV_LOG(ERR, "No enough memory for actions caching.");
		goto end;
	}
	ret = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, flow_info->actions, ret, actions, &error);
	if (ret <= 0) {
		DRV_LOG(ERR, "Can't duplicate actions.");
		goto end;
	}

	/* Insert to the list end. */
	if (LIST_EMPTY(&mode_info->hot_upgrade)) {
		LIST_INSERT_HEAD(&mode_info->hot_upgrade, flow_info,  next);
	} else {
		tmp_info = LIST_FIRST(&mode_info->hot_upgrade);
		while (LIST_NEXT(tmp_info, next))
			tmp_info = LIST_NEXT(tmp_info, next);
		LIST_INSERT_AFTER(tmp_info, flow_info, next);
	}
	return 0;
end:
	if (flow_info->items)
		mlx5_free(flow_info->items);
	if (flow_info->actions)
		mlx5_free(flow_info->actions);
	mlx5_free(flow_info);
	return -1;
}

static int
mlx5_flow_cache_flow_toggle(struct rte_eth_dev *dev, bool orig_prio)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_pmd_mlx5_flow_engine_mode_info *mode_info = &priv->mode_info;
	struct mlx5_dv_flow_info *flow_info;
	struct rte_flow_attr attr;
	struct rte_flow_error error;
	struct rte_flow *high, *low;

	flow_info = LIST_FIRST(&mode_info->hot_upgrade);
	while (flow_info) {
		/* DUP flow may have the same priority. */
		if (flow_info->orig_prio != flow_info->attr.priority) {
			attr = flow_info->attr;
			if (orig_prio)
				attr.priority = flow_info->orig_prio;
			flow_info->flow_idx_high_prio = mlx5_flow_list_create(dev,
					MLX5_FLOW_TYPE_GEN, &attr,
					flow_info->items, flow_info->actions,
					true, &error);
			if (!flow_info->flow_idx_high_prio) {
				DRV_LOG(ERR, "Priority toggle failed internally.");
				goto err;
			}
		}
		flow_info = LIST_NEXT(flow_info, next);
	}
	/* Delete the low priority rules and swap the flow handle. */
	flow_info = LIST_FIRST(&mode_info->hot_upgrade);
	while (flow_info) {
		MLX5_ASSERT(flow_info->flow_idx_low_prio);
		if (flow_info->orig_prio != flow_info->attr.priority) {
			high = mlx5_ipool_get(priv->flows[MLX5_FLOW_TYPE_GEN],
					flow_info->flow_idx_high_prio);
			low = mlx5_ipool_get(priv->flows[MLX5_FLOW_TYPE_GEN],
					flow_info->flow_idx_low_prio);
			if (high && low) {
				RTE_SWAP(*low, *high);
				mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN,
						  flow_info->flow_idx_low_prio);
				flow_info->flow_idx_high_prio = 0;
			}
		}
		flow_info = LIST_NEXT(flow_info, next);
	}
	return 0;
err:
	/* Destroy preceding successful high priority rules. */
	flow_info = LIST_FIRST(&mode_info->hot_upgrade);
	while (flow_info) {
		if (flow_info->orig_prio != flow_info->attr.priority) {
			if (flow_info->flow_idx_high_prio)
				mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN,
						  flow_info->flow_idx_high_prio);
			else
				break;
			flow_info->flow_idx_high_prio = 0;
		}
		flow_info = LIST_NEXT(flow_info, next);
	}
	return -1;
}

/**
 * Set the mode of the flow engine of a process to active or standby during live migration.
 *
 * @param[in] mode
 *   MLX5 flow engine mode, @see `enum rte_pmd_mlx5_flow_engine_mode`.
 * @param[in] flags
 *   Flow engine mode specific flags.
 *
 * @return
 *   Negative value on error, positive on success.
 */
RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_flow_engine_set_mode, 23.03)
int
rte_pmd_mlx5_flow_engine_set_mode(enum rte_pmd_mlx5_flow_engine_mode mode, uint32_t flags)
{
	struct mlx5_priv *priv;
	struct rte_pmd_mlx5_flow_engine_mode_info *mode_info;
	struct mlx5_dv_flow_info *flow_info, *tmp_info;
	uint16_t port, port_id;
	uint16_t toggle_num = 0;
	struct rte_eth_dev *dev;
	enum rte_pmd_mlx5_flow_engine_mode orig_mode;
	uint32_t orig_flags;
	bool need_toggle = false;

	/* Check if flags combinations are supported. */
	if (flags && flags != RTE_PMD_MLX5_FLOW_ENGINE_FLAG_STANDBY_DUP_INGRESS) {
		DRV_LOG(ERR, "Doesn't support such flags %u", flags);
		return -1;
	}
	MLX5_ETH_FOREACH_DEV(port, NULL) {
		dev = &rte_eth_devices[port];
		priv = dev->data->dev_private;
		mode_info = &priv->mode_info;
		/* No mode change. Assume all devices hold the same mode. */
		if (mode_info->mode == mode) {
			DRV_LOG(INFO, "Process flow engine has been in mode %u", mode);
			if (mode_info->mode_flag != flags && !LIST_EMPTY(&mode_info->hot_upgrade)) {
				DRV_LOG(ERR, "Port %u has rule cache with different flag %u\n",
						port, mode_info->mode_flag);
				orig_mode = mode_info->mode;
				orig_flags = mode_info->mode_flag;
				goto err;
			}
			mode_info->mode_flag = flags;
			toggle_num++;
			continue;
		}
		/* Active -> standby. */
		if (mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_STANDBY) {
			if (!LIST_EMPTY(&mode_info->hot_upgrade)) {
				DRV_LOG(ERR, "Cached rule existed");
				orig_mode = mode_info->mode;
				orig_flags = mode_info->mode_flag;
				goto err;
			}
			mode_info->mode_flag = flags;
			mode_info->mode = mode;
			toggle_num++;
		/* Standby -> active. */
		} else if (mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE) {
			if (LIST_EMPTY(&mode_info->hot_upgrade)) {
				DRV_LOG(INFO, "No cached rule existed");
			} else {
				if (mlx5_flow_cache_flow_toggle(dev, true)) {
					orig_mode = mode_info->mode;
					orig_flags = mode_info->mode_flag;
					need_toggle = true;
					goto err;
				}
			}
			toggle_num++;
		}
	}
	if (mode == RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE) {
		/* Clear cache flow rules. */
		MLX5_ETH_FOREACH_DEV(port, NULL) {
			priv = rte_eth_devices[port].data->dev_private;
			mode_info = &priv->mode_info;
			flow_info = LIST_FIRST(&mode_info->hot_upgrade);
			while (flow_info) {
				tmp_info = LIST_NEXT(flow_info, next);
				LIST_REMOVE(flow_info, next);
				mlx5_free(flow_info->actions);
				mlx5_free(flow_info->items);
				mlx5_free(flow_info);
				flow_info = tmp_info;
			}
			MLX5_ASSERT(LIST_EMPTY(&mode_info->hot_upgrade));
		}
	}
	return toggle_num;
err:
	/* Rollback all preceding successful ports. */
	MLX5_ETH_FOREACH_DEV(port_id, NULL) {
		if (port_id == port)
			break;
		priv = rte_eth_devices[port_id].data->dev_private;
		mode_info = &priv->mode_info;
		if (need_toggle && !LIST_EMPTY(&mode_info->hot_upgrade) &&
		    mlx5_flow_cache_flow_toggle(dev, false))
			return -EPERM;
		mode_info->mode = orig_mode;
		mode_info->mode_flag = orig_flags;
	}
	return -EINVAL;
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
	struct rte_flow_attr *new_attr = (void *)(uintptr_t)attr;
	uint32_t prio = attr->priority;
	uintptr_t flow_idx;

	/*
	 * If the device is not started yet, it is not allowed to created a
	 * flow from application. PMD default flows and traffic control flows
	 * are not affected.
	 */
	if (unlikely(!dev->data->dev_started)) {
		DRV_LOG(DEBUG, "port %u is not started when "
			"inserting a flow", dev->data->port_id);
		rte_flow_error_set(error, ENODEV,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "port not started");
		return NULL;
	}
	if (unlikely(mlx5_need_cache_flow(priv, attr))) {
		if (attr->transfer ||
				(attr->ingress && !(priv->mode_info.mode_flag &
				RTE_PMD_MLX5_FLOW_ENGINE_FLAG_STANDBY_DUP_INGRESS)))
			new_attr->priority += 1;
	}
	flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_GEN, attr, items, actions,
		true, error);
	if (!flow_idx)
		return NULL;
	if (unlikely(mlx5_need_cache_flow(priv, attr))) {
		if (mlx5_flow_cache_flow_info(dev, attr, prio, items, actions, flow_idx)) {
			mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN, flow_idx);
			flow_idx = 0;
		}
	}
	return (void *)(uintptr_t)flow_idx;
}

uintptr_t
mlx5_flow_list_create(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		      const struct rte_flow_attr *attr,
		      const struct rte_flow_item items[],
		      const struct rte_flow_action actions[],
		      bool external, struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type(dev, attr);

	fops = flow_get_drv_ops(drv_type);
	return fops->list_create(dev, type, attr, items, actions, external,
		error);
}

/**
 * Destroy a flow in a list.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[in] flow_idx
 *   Index of flow to destroy.
 */
void
flow_legacy_list_destroy(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		     uintptr_t flow_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow = mlx5_ipool_get(priv->flows[type], (uint32_t)flow_idx);

	if (!flow)
		return;
	MLX5_ASSERT((type >= MLX5_FLOW_TYPE_CTL) && (type < MLX5_FLOW_TYPE_MAXI));
	MLX5_ASSERT(flow->type == type);
	/*
	 * Update RX queue flags only if port is started, otherwise it is
	 * already clean.
	 */
	if (dev->data->dev_started)
		flow_rxq_flags_trim(dev, flow);
	flow_drv_destroy(dev, flow);
	if (flow->tunnel) {
		struct mlx5_flow_tunnel *tunnel;

		tunnel = mlx5_find_tunnel_id(dev, flow->tunnel_id);
		RTE_VERIFY(tunnel);
		if (!(rte_atomic_fetch_sub_explicit(&tunnel->refctn, 1,
				rte_memory_order_relaxed) - 1))
			mlx5_flow_tunnel_free(dev, tunnel);
	}
	flow_mreg_del_copy_action(dev, flow);
	mlx5_ipool_free(priv->flows[type], flow_idx);
}

void
mlx5_flow_list_destroy(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		       uintptr_t flow_idx)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = { .transfer = 0 };
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type(dev, &attr);

	fops = flow_get_drv_ops(drv_type);
	fops->list_destroy(dev, type, flow_idx);
}

/**
 * Destroy all flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param type
 *   Flow type to be flushed.
 * @param active
 *   If flushing is called actively.
 */
void
mlx5_flow_list_flush(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		     bool active)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t num_flushed = 0, fidx = 1;
	struct rte_flow *flow;
	struct rte_pmd_mlx5_flow_engine_mode_info *mode_info = &priv->mode_info;
	struct mlx5_dv_flow_info *flow_info;

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	if (priv->sh->config.dv_flow_en == 2 &&
	    type == MLX5_FLOW_TYPE_GEN) {
		priv->hws_rule_flushing = true;
		flow_hw_q_flow_flush(dev, NULL);
		priv->hws_rule_flushing = false;
	}
#endif
	MLX5_IPOOL_FOREACH(priv->flows[type], fidx, flow) {
		if (priv->sh->config.dv_flow_en == 2) {
			mlx5_flow_list_destroy(dev, type, (uintptr_t)flow);
		} else {
			mlx5_flow_list_destroy(dev, type, fidx);
		}
		if (unlikely(mlx5_need_cache_flow(priv, NULL) && type == MLX5_FLOW_TYPE_GEN)) {
			flow_info = LIST_FIRST(&mode_info->hot_upgrade);
			while (flow_info) {
				/* Romove the cache flow info. */
				if (flow_info->flow_idx_low_prio == (uint32_t)(uintptr_t)fidx) {
					MLX5_ASSERT(!flow_info->flow_idx_high_prio);
					LIST_REMOVE(flow_info, next);
					mlx5_free(flow_info->items);
					mlx5_free(flow_info->actions);
					mlx5_free(flow_info);
					break;
				}
				flow_info = LIST_NEXT(flow_info, next);
			}
		}
		num_flushed++;
	}
	if (active) {
		DRV_LOG(INFO, "port %u: %u flows flushed before stopping",
			dev->data->port_id, num_flushed);
	}
}

/**
 * Stop all default actions for flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_stop_default(struct rte_eth_dev *dev)
{
#ifdef HAVE_MLX5_HWS_SUPPORT
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->config.dv_flow_en == 2) {
		mlx5_flow_nta_del_default_copy_action(dev);
		if (!rte_atomic_load_explicit(&priv->hws_mark_refcnt,
					      rte_memory_order_relaxed))
			flow_hw_rxq_flag_set(dev, false);
		return;
	}
#endif
	flow_mreg_del_default_copy_action(dev);
	mlx5_flow_rxq_flags_clear(dev);
}

/**
 * Set rxq flag.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] enable
 *   Flag to enable or not.
 */
void
flow_hw_rxq_flag_set(struct rte_eth_dev *dev, bool enable)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;

	if ((!priv->mark_enabled && !enable) ||
	    (priv->mark_enabled && enable))
		return;
	for (i = 0; i < priv->rxqs_n; ++i) {
		struct mlx5_rxq_ctrl *rxq_ctrl = mlx5_rxq_ctrl_get(dev, i);

		/* With RXQ start/stop feature, RXQ might be stopped. */
		if (!rxq_ctrl)
			continue;
		rxq_ctrl->rxq.mark = enable;
	}
	priv->mark_enabled = enable;
}

/**
 * Start all default actions for flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_start_default(struct rte_eth_dev *dev)
{
	struct rte_flow_error error;
#ifdef HAVE_MLX5_HWS_SUPPORT
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->sh->config.dv_flow_en == 2) {
		/*
		 * Ignore this failure, if the proxy port is not started, other
		 * default jump actions are not created and this rule will not
		 * be hit.
		 */
		if (mlx5_flow_nta_add_default_copy_action(dev, &error)) {
			DRV_LOG(DEBUG, "port %u failed to start default copy action: %s",
				dev->data->port_id, strerror(rte_errno));
			return 0;
		}
	}
#endif
	/* Make sure default copy action (reg_c[0] -> reg_b) is created. */
	return flow_mreg_add_default_copy_action(dev, &error);
}

/**
 * Release key of thread specific flow workspace data.
 */
void
flow_release_workspace(void *data)
{
	struct mlx5_flow_workspace *wks = data;
	struct mlx5_flow_workspace *next;

	while (wks) {
		next = wks->next;
		free(wks);
		wks = next;
	}
}

/**
 * Get thread specific current flow workspace.
 *
 * @return pointer to thread specific flow workspace data, NULL on error.
 */
struct mlx5_flow_workspace*
mlx5_flow_get_thread_workspace(void)
{
	struct mlx5_flow_workspace *data;

	data = mlx5_flow_os_get_specific_workspace();
	MLX5_ASSERT(data && data->inuse);
	if (!data || !data->inuse)
		DRV_LOG(ERR, "flow workspace not initialized.");
	return data;
}

/**
 * Allocate and init new flow workspace.
 *
 * @return pointer to flow workspace data, NULL on error.
 */
static struct mlx5_flow_workspace*
flow_alloc_thread_workspace(void)
{
	size_t data_size = RTE_ALIGN(sizeof(struct mlx5_flow_workspace), sizeof(long));
	size_t rss_queue_array_size = sizeof(uint16_t) * RTE_ETH_RSS_RETA_SIZE_512;
	size_t alloc_size = data_size + rss_queue_array_size;
#ifdef HAVE_MLX5_HWS_SUPPORT
	/* Dummy table size for the non-template API. */
	alloc_size += sizeof(struct rte_flow_template_table);
#endif
	struct mlx5_flow_workspace *data = calloc(1, alloc_size);

	if (!data) {
		DRV_LOG(ERR, "Failed to allocate flow workspace memory.");
		return NULL;
	}
	data->rss_desc.queue = RTE_PTR_ADD(data, data_size);
#ifdef HAVE_MLX5_HWS_SUPPORT
	data->table = RTE_PTR_ADD(data->rss_desc.queue, rss_queue_array_size);
#endif
	return data;
}

/**
 * Get new thread specific flow workspace.
 *
 * If current workspace inuse, create new one and set as current.
 *
 * @return pointer to thread specific flow workspace data, NULL on error.
 */
struct mlx5_flow_workspace*
mlx5_flow_push_thread_workspace(void)
{
	struct mlx5_flow_workspace *curr;
	struct mlx5_flow_workspace *data;

	curr = mlx5_flow_os_get_specific_workspace();
	if (!curr) {
		data = flow_alloc_thread_workspace();
		if (!data)
			return NULL;
		mlx5_flow_os_workspace_gc_add(data);
	} else if (!curr->inuse) {
		data = curr;
	} else if (curr->next) {
		data = curr->next;
	} else {
		data = flow_alloc_thread_workspace();
		if (!data)
			return NULL;
		curr->next = data;
		data->prev = curr;
	}
	data->inuse = 1;
	data->flow_idx = 0;
	/* Set as current workspace */
	if (mlx5_flow_os_set_specific_workspace(data))
		DRV_LOG(ERR, "Failed to set flow workspace to thread.");
	return data;
}

/**
 * Close current thread specific flow workspace.
 *
 * If previous workspace available, set it as current.
 *
 * @return pointer to thread specific flow workspace data, NULL on error.
 */
void
mlx5_flow_pop_thread_workspace(void)
{
	struct mlx5_flow_workspace *data = mlx5_flow_get_thread_workspace();

	if (!data)
		return;
	if (!data->inuse) {
		DRV_LOG(ERR, "Failed to close unused flow workspace.");
		return;
	}
	data->inuse = 0;
	if (!data->prev)
		return;
	if (mlx5_flow_os_set_specific_workspace(data->prev))
		DRV_LOG(ERR, "Failed to set flow workspace to thread.");
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
mlx5_flow_verify(struct rte_eth_dev *dev __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow *flow;
	uint32_t idx = 0;
	int ret = 0, i;

	for (i = 0; i < MLX5_FLOW_TYPE_MAXI; i++) {
		MLX5_IPOOL_FOREACH(priv->flows[i], idx, flow) {
			DRV_LOG(DEBUG, "port %u flow %p still referenced",
				dev->data->port_id, (void *)flow);
			ret++;
		}
	}
	return ret;
}

/**
 * Enable default hairpin egress flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param sq_num
 *   The SQ hw number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_ctrl_flow_source_queue(struct rte_eth_dev *dev,
			    uint32_t sq_num)
{
	const struct rte_flow_attr attr = {
		.egress = 1,
		.priority = 0,
	};
	struct mlx5_rte_flow_item_sq queue_spec = {
		.queue = sq_num,
	};
	struct mlx5_rte_flow_item_sq queue_mask = {
		.queue = UINT32_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_SQ,
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
	uint32_t flow_idx;
	struct rte_flow_error error;

	actions[0].type = RTE_FLOW_ACTION_TYPE_JUMP;
	actions[0].conf = &jump;
	actions[1].type = RTE_FLOW_ACTION_TYPE_END;
	flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL,
				    &attr, items, actions, false, &error);
	if (!flow_idx) {
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
		.priority = MLX5_FLOW_LOWEST_PRIO_INDICATOR,
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
	uint16_t *queue = alloca(sizeof(uint16_t) * priv->reta_idx_n);
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
	uintptr_t flow_idx;
	struct rte_flow_error error;
	struct mlx5_ctrl_flow_entry *entry;
	unsigned int i;

	if (!priv->reta_idx_n || !priv->rxqs_n) {
		return 0;
	}
	if (!(dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG))
		action_rss.types = 0;
	for (i = 0; i != priv->reta_idx_n; ++i)
		queue[i] = (*priv->reta_idx)[i];

	entry = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*entry), alignof(typeof(*entry)), SOCKET_ID_ANY);
	if (entry == NULL) {
		rte_errno = ENOMEM;
		goto err;
	}

	entry->owner_dev = dev;
	if (vlan_spec == NULL) {
		entry->info.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC;
	} else {
		entry->info.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC_VLAN;
		entry->info.uc.vlan = rte_be_to_cpu_16(vlan_spec->hdr.vlan_tci);
	}
	entry->info.uc.dmac = eth_spec->hdr.dst_addr;

	flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL,
				    &attr, items, actions, false, &error);
	if (!flow_idx) {
		mlx5_free(entry);
		goto err;
	}

	entry->flow = (struct rte_flow *)flow_idx;
	LIST_INSERT_HEAD(&priv->hw_ctrl_flows, entry, next);

	return 0;

err:
	return -rte_errno;
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

int
mlx5_legacy_dmac_flow_create(struct rte_eth_dev *dev, const struct rte_ether_addr *addr)
{
	struct rte_flow_item_eth unicast = {
		.hdr.dst_addr = *addr,
	};
	struct rte_flow_item_eth unicast_mask = {
		.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	};

	return mlx5_ctrl_flow(dev, &unicast, &unicast_mask);
}

int
mlx5_legacy_dmac_vlan_flow_create(struct rte_eth_dev *dev,
				  const struct rte_ether_addr *addr,
				  const uint16_t vid)
{
	struct rte_flow_item_eth unicast_spec = {
		.hdr.dst_addr = *addr,
	};
	struct rte_flow_item_eth unicast_mask = {
		.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	};
	struct rte_flow_item_vlan vlan_spec = {
		.hdr.vlan_tci = rte_cpu_to_be_16(vid),
	};
	struct rte_flow_item_vlan vlan_mask = rte_flow_item_vlan_mask;

	return mlx5_ctrl_flow_vlan(dev, &unicast_spec, &unicast_mask, &vlan_spec, &vlan_mask);
}

void
mlx5_legacy_ctrl_flow_destroy(struct rte_eth_dev *dev, struct mlx5_ctrl_flow_entry *entry)
{
	uintptr_t flow_idx;

	flow_idx = (uintptr_t)entry->flow;
	mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_CTL, flow_idx);
	LIST_REMOVE(entry, next);
	mlx5_free(entry);
}

int
mlx5_legacy_dmac_flow_destroy(struct rte_eth_dev *dev, const struct rte_ether_addr *addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;

	LIST_FOREACH(entry, &priv->hw_ctrl_flows, next) {
		if (entry->info.type != MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC ||
		    !rte_is_same_ether_addr(addr, &entry->info.uc.dmac))
			continue;

		mlx5_legacy_ctrl_flow_destroy(dev, entry);
		return 0;
	}
	return 0;
}

int
mlx5_legacy_dmac_vlan_flow_destroy(struct rte_eth_dev *dev,
				   const struct rte_ether_addr *addr,
				   const uint16_t vid)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;

	LIST_FOREACH(entry, &priv->hw_ctrl_flows, next) {
		if (entry->info.type != MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC_VLAN ||
		    !rte_is_same_ether_addr(addr, &entry->info.uc.dmac) ||
		    vid != entry->info.uc.vlan)
			continue;

		mlx5_legacy_ctrl_flow_destroy(dev, entry);
		return 0;
	}
	return 0;
}

/**
 * Create default miss flow rule matching lacp traffic
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param eth_spec
 *   An Ethernet flow spec to apply.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_lacp_miss(struct rte_eth_dev *dev)
{
	/*
	 * The LACP matching is done by only using ether type since using
	 * a multicast dst mac causes kernel to give low priority to this flow.
	 */
	static const struct rte_flow_item_eth lacp_spec = {
		.hdr.ether_type = RTE_BE16(0x8809),
	};
	static const struct rte_flow_item_eth lacp_mask = {
		.hdr.ether_type = 0xffff,
	};
	const struct rte_flow_attr attr = {
		.ingress = 1,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &lacp_spec,
			.mask = &lacp_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action actions[] = {
		{
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow_error error;
	uint32_t flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_CTL,
					&attr, items, actions,
					false, &error);

	if (!flow_idx)
		return -rte_errno;
	return 0;
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
	struct rte_pmd_mlx5_flow_engine_mode_info *mode_info = &priv->mode_info;
	struct mlx5_dv_flow_info *flow_info;

	mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN,
				(uintptr_t)(void *)flow);
	if (unlikely(mlx5_need_cache_flow(priv, NULL))) {
		flow_info = LIST_FIRST(&mode_info->hot_upgrade);
		while (flow_info) {
			/* Romove the cache flow info. */
			if (flow_info->flow_idx_low_prio == (uint32_t)(uintptr_t)flow) {
				MLX5_ASSERT(!flow_info->flow_idx_high_prio);
				LIST_REMOVE(flow_info, next);
				mlx5_free(flow_info->items);
				mlx5_free(flow_info->actions);
				mlx5_free(flow_info);
				break;
			}
			flow_info = LIST_NEXT(flow_info, next);
		}
	}
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
	mlx5_flow_list_flush(dev, MLX5_FLOW_TYPE_GEN, false);
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
	if (!enable && !priv->sh->config.repr_matching)
		return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "isolated mode cannot be disabled when "
					  "representor matching is disabled");
	priv->isolated = !!enable;
	if (enable)
		dev->dev_ops = &mlx5_dev_ops_isolate;
	else
		dev->dev_ops = &mlx5_dev_ops;

	dev->rx_descriptor_status = mlx5_rx_descriptor_status;
	dev->tx_descriptor_status = mlx5_tx_descriptor_status;

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
	       struct rte_flow *eflow,
	       const struct rte_flow_action *actions,
	       void *data,
	       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow *flow = NULL;
	enum mlx5_flow_drv_type ftype = MLX5_FLOW_TYPE_MIN;

	if (priv->sh->config.dv_flow_en == 2) {
#ifdef HAVE_MLX5_HWS_SUPPORT
		flow = eflow;
		ftype = MLX5_FLOW_TYPE_HW;
#endif
	} else {
		flow = (struct rte_flow *)mlx5_ipool_get(priv->flows[MLX5_FLOW_TYPE_GEN],
				(uintptr_t)(void *)eflow);
	}
	if (!flow) {
		return rte_flow_error_set(error, ENOENT,
			  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			  NULL,
			  "invalid flow handle");
	}
	if (ftype == MLX5_FLOW_TYPE_MIN)
		ftype = flow->drv_type;
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

	ret = flow_drv_query(dev, flow, actions, data,
			     error);
	if (ret < 0)
		return ret;
	return 0;
}

/**
 * Get rte_flow callbacks.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param ops
 *   Pointer to operation-specific structure.
 *
 * @return 0
 */
int
mlx5_flow_ops_get(struct rte_eth_dev *dev __rte_unused,
		  const struct rte_flow_ops **ops)
{
	*ops = &mlx5_flow_ops;
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
 * @param[out] is_rss
 *   Is RSS or not.
 * @param[out] domain_bitmap
 *   Domain bitmap.
 * @param[out] is_def_policy
 *   Is default policy or not.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
mlx5_flow_validate_mtr_acts(struct rte_eth_dev *dev,
			const struct rte_flow_action *actions[RTE_COLORS],
			struct rte_flow_attr *attr,
			bool *is_rss,
			uint8_t *domain_bitmap,
			uint8_t *policy_mode,
			struct rte_mtr_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->validate_mtr_acts(dev, actions, attr, is_rss,
				       domain_bitmap, policy_mode, error);
}

/**
 * Destroy the meter table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy
 *   Meter policy struct.
 */
void
mlx5_flow_destroy_mtr_acts(struct rte_eth_dev *dev,
		      struct mlx5_flow_meter_policy *mtr_policy)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_mtr_acts(dev, mtr_policy);
}

/**
 * Create policy action, lock free,
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
 *   Flow rule attributes.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
mlx5_flow_create_mtr_acts(struct rte_eth_dev *dev,
		      struct mlx5_flow_meter_policy *mtr_policy,
		      const struct rte_flow_action *actions[RTE_COLORS],
		      struct rte_flow_attr *attr,
		      struct rte_mtr_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_mtr_acts(dev, mtr_policy, actions, attr, error);
}

/**
 * Create policy rules, lock free,
 * (mutex should be acquired by caller).
 * Dispatcher for action type specific call.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] mtr_policy
 *   Meter policy struct.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
int
mlx5_flow_create_policy_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter_policy *mtr_policy)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_policy_rules(dev, mtr_policy);
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
void
mlx5_flow_destroy_policy_rules(struct rte_eth_dev *dev,
			     struct mlx5_flow_meter_policy *mtr_policy)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_policy_rules(dev, mtr_policy);
}

/**
 * Destroy the default policy table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_destroy_def_policy(struct rte_eth_dev *dev)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_def_policy(dev);
}

/**
 * Destroy the default policy table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
int
mlx5_flow_create_def_policy(struct rte_eth_dev *dev)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_def_policy(dev);
}

/**
 * Create the needed meter and suffix tables.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, -1 otherwise.
 */
int
mlx5_flow_create_mtr_tbls(struct rte_eth_dev *dev,
			struct mlx5_flow_meter_info *fm,
			uint32_t mtr_idx,
			uint8_t domain_bitmap)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_mtr_tbls(dev, fm, mtr_idx, domain_bitmap);
}

/**
 * Destroy the meter table set.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] tbl
 *   Pointer to the meter table set.
 */
void
mlx5_flow_destroy_mtr_tbls(struct rte_eth_dev *dev,
			   struct mlx5_flow_meter_info *fm)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_mtr_tbls(dev, fm);
}

/**
 * Destroy the global meter drop table.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 */
void
mlx5_flow_destroy_mtr_drop_tbls(struct rte_eth_dev *dev)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_mtr_drop_tbls(dev);
}

/**
 * Destroy the sub policy table with RX queue.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_policy
 *   Pointer to meter policy table.
 */
void
mlx5_flow_destroy_sub_policy_with_rxq(struct rte_eth_dev *dev,
		struct mlx5_flow_meter_policy *mtr_policy)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->destroy_sub_policy_with_rxq(dev, mtr_policy);
}

/**
 * Allocate the needed aso flow meter id.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Index to aso flow meter on success, NULL otherwise.
 */
uint32_t
mlx5_flow_mtr_alloc(struct rte_eth_dev *dev)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	return fops->create_meter(dev);
}

/**
 * Free the aso flow meter id.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] mtr_idx
 *  Index to aso flow meter to be free.
 *
 * @return
 *   0 on success.
 */
void
mlx5_flow_mtr_free(struct rte_eth_dev *dev, uint32_t mtr_idx)
{
	const struct mlx5_flow_driver_ops *fops;

	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_DV);
	fops->free_meter(dev, mtr_idx);
}

/**
 * Allocate a counter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 *
 * @return
 *   Index to allocated counter  on success, 0 otherwise.
 */
uint32_t
mlx5_counter_alloc(struct rte_eth_dev *dev)
{
	struct rte_flow_attr attr = { .transfer = 0 };

	return flow_get_drv_ops(flow_get_drv_type(dev, &attr))->counter_alloc
		(dev);
}

/**
 * Free a counter.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] cnt
 *   Index to counter to be free.
 */
void
mlx5_counter_free(struct rte_eth_dev *dev, uint32_t cnt)
{
	struct rte_flow_attr attr = { .transfer = 0 };

	flow_get_drv_ops(flow_get_drv_type(dev, &attr))->counter_free(dev, cnt);
}

/**
 * Query counter statistics.
 *
 * @param[in] dev
 *   Pointer to Ethernet device structure.
 * @param[in] cnt
 *   Index to counter to query.
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
mlx5_counter_query(struct rte_eth_dev *dev, uint32_t cnt,
		   bool clear, uint64_t *pkts, uint64_t *bytes, void **action)
{
	struct rte_flow_attr attr = { .transfer = 0 };

	return flow_get_drv_ops(flow_get_drv_type(dev, &attr))->counter_query
		(dev, cnt, clear, pkts, bytes, action);
}

/**
 * Get information about HWS pre-configurable resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[out] port_info
 *   Pointer to port information.
 * @param[out] queue_info
 *   Pointer to queue information.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_info_get(struct rte_eth_dev *dev,
		   struct rte_flow_port_info *port_info,
		   struct rte_flow_queue_info *queue_info,
		   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = {0};

	if (flow_get_drv_type(dev, &attr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"info get with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->info_get(dev, port_info, queue_info, error);
}

/**
 * Configure port HWS resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] port_attr
 *   Port configuration attributes.
 * @param[in] nb_queue
 *   Number of queue.
 * @param[in] queue_attr
 *   Array that holds attributes for each flow queue.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_port_configure(struct rte_eth_dev *dev,
			 const struct rte_flow_port_attr *port_attr,
			 uint16_t nb_queue,
			 const struct rte_flow_queue_attr *queue_attr[],
			 struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = {0};

	if (flow_get_drv_type(dev, &attr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"port configure with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->configure(dev, port_attr, nb_queue, queue_attr, error);
}

/**
 * Validate item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the item template attributes.
 * @param[in] items
 *   The template item pattern.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_pattern_validate(struct rte_eth_dev *dev,
		const struct rte_flow_pattern_template_attr *attr,
		const struct rte_flow_item items[],
		struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};
	uint64_t item_flags = 0;

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"pattern validate with incorrect steering mode");
		return -ENOTSUP;
	}
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->pattern_validate(dev, attr, items, &item_flags, error);
}

/**
 * Create flow item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the item template attributes.
 * @param[in] items
 *   The template item pattern.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static struct rte_flow_pattern_template *
mlx5_flow_pattern_template_create(struct rte_eth_dev *dev,
		const struct rte_flow_pattern_template_attr *attr,
		const struct rte_flow_item items[],
		struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"pattern create with incorrect steering mode");
		return NULL;
	}
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->pattern_template_create(dev, attr, items, error);
}

/**
 * Destroy flow item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] template
 *   Pointer to the item template to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_pattern_template_destroy(struct rte_eth_dev *dev,
				   struct rte_flow_pattern_template *template,
				   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = {0};

	if (flow_get_drv_type(dev, &attr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"pattern destroy with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->pattern_template_destroy(dev, template, error);
}

/**
 * Validate flow actions template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the action template attributes.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] masks
 *   List of actions that marks which of the action's member is constant.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_actions_validate(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"actions validate with incorrect steering mode");
		return -ENOTSUP;
	}
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->actions_validate(dev, attr, actions, masks, error);
}

/**
 * Create flow item template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the action template attributes.
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] masks
 *   List of actions that marks which of the action's member is constant.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static struct rte_flow_actions_template *
mlx5_flow_actions_template_create(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"action create with incorrect steering mode");
		return NULL;
	}
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->actions_template_create(dev, attr, actions, masks, error);
}

/**
 * Destroy flow action template.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] template
 *   Pointer to the action template to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_actions_template_destroy(struct rte_eth_dev *dev,
				   struct rte_flow_actions_template *template,
				   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = {0};

	if (flow_get_drv_type(dev, &attr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"action destroy with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->actions_template_destroy(dev, template, error);
}

/**
 * Create flow table.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to the table attributes.
 * @param[in] item_templates
 *   Item template array to be binded to the table.
 * @param[in] nb_item_templates
 *   Number of item template.
 * @param[in] action_templates
 *   Action template array to be binded to the table.
 * @param[in] nb_action_templates
 *   Number of action template.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Table on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_template_table *
mlx5_flow_table_create(struct rte_eth_dev *dev,
		       const struct rte_flow_template_table_attr *attr,
		       struct rte_flow_pattern_template *item_templates[],
		       uint8_t nb_item_templates,
		       struct rte_flow_actions_template *action_templates[],
		       uint8_t nb_action_templates,
		       struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"table create with incorrect steering mode");
		return NULL;
	}
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->template_table_create(dev,
					   attr,
					   item_templates,
					   nb_item_templates,
					   action_templates,
					   nb_action_templates,
					   error);
}

/**
 * PMD destroy flow table.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] table
 *   Pointer to the table to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_table_destroy(struct rte_eth_dev *dev,
			struct rte_flow_template_table *table,
			struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = {0};

	if (flow_get_drv_type(dev, &attr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"table destroy with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->template_table_destroy(dev, table, error);
}

/**
 * PMD group set miss actions.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] attr
 *   Pointer to group attributes
 * @param[in] actions
 *   Array of actions
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_flow_group_set_miss_actions(struct rte_eth_dev *dev,
				 uint32_t group_id,
				 const struct rte_flow_group_attr *attr,
				 const struct rte_flow_action actions[],
				 struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr fattr = {0};

	if (flow_get_drv_type(dev, &fattr) != MLX5_FLOW_TYPE_HW)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"group set miss actions with incorrect steering mode");
	fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
	return fops->group_set_miss_actions(dev, group_id, attr, actions, error);
}

/**
 * Allocate a new memory for the counter values wrapped by all the needed
 * management.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
mlx5_flow_create_counter_stat_mem_mng(struct mlx5_dev_ctx_shared *sh)
{
	struct mlx5_counter_stats_mem_mng *mem_mng;
	volatile struct flow_counter_stats *raw_data;
	int raws_n = MLX5_CNT_MR_ALLOC_BULK + MLX5_MAX_PENDING_QUERIES;
	int size = (sizeof(struct flow_counter_stats) *
			MLX5_COUNTERS_PER_POOL +
			sizeof(struct mlx5_counter_stats_raw)) * raws_n +
			sizeof(struct mlx5_counter_stats_mem_mng);
	size_t pgsize = rte_mem_page_size();
	uint8_t *mem;
	int ret;
	int i;

	if (pgsize == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	mem = mlx5_malloc(MLX5_MEM_ZERO, size, pgsize, SOCKET_ID_ANY);
	if (!mem) {
		rte_errno = ENOMEM;
		return -ENOMEM;
	}
	mem_mng = (struct mlx5_counter_stats_mem_mng *)(mem + size) - 1;
	size = sizeof(*raw_data) * MLX5_COUNTERS_PER_POOL * raws_n;
	ret = mlx5_os_wrapped_mkey_create(sh->cdev->ctx, sh->cdev->pd,
					  sh->cdev->pdn, mem, size,
					  &mem_mng->wm);
	if (ret) {
		rte_errno = errno;
		mlx5_free(mem);
		return -rte_errno;
	}
	mem_mng->raws = (struct mlx5_counter_stats_raw *)(mem + size);
	raw_data = (volatile struct flow_counter_stats *)mem;
	for (i = 0; i < raws_n; ++i) {
		mem_mng->raws[i].mem_mng = mem_mng;
		mem_mng->raws[i].data = raw_data + i * MLX5_COUNTERS_PER_POOL;
	}
	for (i = 0; i < MLX5_MAX_PENDING_QUERIES; ++i)
		LIST_INSERT_HEAD(&sh->sws_cmng.free_stat_raws,
				 mem_mng->raws + MLX5_CNT_MR_ALLOC_BULK + i,
				 next);
	LIST_INSERT_HEAD(&sh->sws_cmng.mem_mngs, mem_mng, next);
	sh->sws_cmng.mem_mng = mem_mng;
	return 0;
}

/**
 * Set the statistic memory to the new counter pool.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param[in] pool
 *   Pointer to the pool to set the statistic memory.
 *
 * @return
 *   0 on success, a negative errno value otherwise.
 */
static int
mlx5_flow_set_counter_stat_mem(struct mlx5_dev_ctx_shared *sh,
			       struct mlx5_flow_counter_pool *pool)
{
	struct mlx5_flow_counter_mng *cmng = &sh->sws_cmng;
	/* Resize statistic memory once used out. */
	if (!(pool->index % MLX5_CNT_MR_ALLOC_BULK) &&
	    mlx5_flow_create_counter_stat_mem_mng(sh)) {
		DRV_LOG(ERR, "Cannot resize counter stat mem.");
		return -1;
	}
	rte_spinlock_lock(&pool->sl);
	pool->raw = cmng->mem_mng->raws + pool->index % MLX5_CNT_MR_ALLOC_BULK;
	rte_spinlock_unlock(&pool->sl);
	pool->raw_hw = NULL;
	return 0;
}

#define MLX5_POOL_QUERY_FREQ_US 1000000

/**
 * Set the periodic procedure for triggering asynchronous batch queries for all
 * the counter pools.
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 */
void
mlx5_set_query_alarm(struct mlx5_dev_ctx_shared *sh)
{
	uint32_t pools_n, us;

	pools_n = rte_atomic_load_explicit(&sh->sws_cmng.n_valid, rte_memory_order_relaxed);
	us = MLX5_POOL_QUERY_FREQ_US / pools_n;
	DRV_LOG(DEBUG, "Set alarm for %u pools each %u us", pools_n, us);
	if (rte_eal_alarm_set(us, mlx5_flow_query_alarm, sh)) {
		sh->sws_cmng.query_thread_on = 0;
		DRV_LOG(ERR, "Cannot reinitialize query alarm");
	} else {
		sh->sws_cmng.query_thread_on = 1;
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
	struct mlx5_dev_ctx_shared *sh = arg;
	struct mlx5_flow_counter_mng *cmng = &sh->sws_cmng;
	uint16_t pool_index = cmng->pool_index;
	struct mlx5_flow_counter_pool *pool;
	uint16_t n_valid;
	int ret;

	if (cmng->pending_queries >= MLX5_MAX_PENDING_QUERIES)
		goto set_alarm;
	rte_spinlock_lock(&cmng->pool_update_sl);
	pool = cmng->pools[pool_index];
	n_valid = cmng->n_valid;
	rte_spinlock_unlock(&cmng->pool_update_sl);
	/* Set the statistic memory to the new created pool. */
	if ((!pool->raw && mlx5_flow_set_counter_stat_mem(sh, pool)))
		goto set_alarm;
	if (pool->raw_hw)
		/* There is a pool query in progress. */
		goto set_alarm;
	pool->raw_hw = LIST_FIRST(&cmng->free_stat_raws);
	if (!pool->raw_hw)
		/* No free counter statistics raw memory. */
		goto set_alarm;
	/*
	 * Identify the counters released between query trigger and query
	 * handle more efficiently. The counter released in this gap period
	 * should wait for a new round of query as the new arrived packets
	 * will not be taken into account.
	 */
	pool->query_gen++;
	ret = mlx5_devx_cmd_flow_counter_query(pool->min_dcs, 0,
					       MLX5_COUNTERS_PER_POOL,
					       NULL, NULL,
					       pool->raw_hw->mem_mng->wm.lkey,
					       (void *)(uintptr_t)
					       pool->raw_hw->data,
					       sh->devx_comp,
					       (uint64_t)(uintptr_t)pool);
	if (ret) {
		DRV_LOG(ERR, "Failed to trigger asynchronous query for dcs ID"
			" %d", pool->min_dcs->id);
		pool->raw_hw = NULL;
		goto set_alarm;
	}
	LIST_REMOVE(pool->raw_hw, next);
	cmng->pending_queries++;
	pool_index++;
	if (pool_index >= n_valid)
		pool_index = 0;
set_alarm:
	cmng->pool_index = pool_index;
	mlx5_set_query_alarm(sh);
}

/**
 * Check and callback event for new aged flow in the counter pool
 *
 * @param[in] sh
 *   Pointer to mlx5_dev_ctx_shared object.
 * @param[in] pool
 *   Pointer to Current counter pool.
 */
static void
mlx5_flow_aging_check(struct mlx5_dev_ctx_shared *sh,
		   struct mlx5_flow_counter_pool *pool)
{
	struct mlx5_priv *priv;
	struct mlx5_flow_counter *cnt;
	struct mlx5_age_info *age_info;
	struct mlx5_age_param *age_param;
	struct mlx5_counter_stats_raw *cur = pool->raw_hw;
	struct mlx5_counter_stats_raw *prev = pool->raw;
	const uint64_t curr_time = MLX5_CURR_TIME_SEC;
	const uint32_t time_delta = curr_time - pool->time_of_last_age_check;
	uint16_t expected = AGE_CANDIDATE;
	uint32_t i;

	pool->time_of_last_age_check = curr_time;
	for (i = 0; i < MLX5_COUNTERS_PER_POOL; ++i) {
		cnt = MLX5_POOL_GET_CNT(pool, i);
		age_param = MLX5_CNT_TO_AGE(cnt);
		if (rte_atomic_load_explicit(&age_param->state,
				    rte_memory_order_relaxed) != AGE_CANDIDATE)
			continue;
		if (cur->data[i].hits != prev->data[i].hits) {
			rte_atomic_store_explicit(&age_param->sec_since_last_hit, 0,
					 rte_memory_order_relaxed);
			continue;
		}
		if (rte_atomic_fetch_add_explicit(&age_param->sec_since_last_hit,
				       time_delta,
				       rte_memory_order_relaxed) + time_delta <= age_param->timeout)
			continue;
		/**
		 * Hold the lock first, or if between the
		 * state AGE_TMOUT and tailq operation the
		 * release happened, the release procedure
		 * may delete a non-existent tailq node.
		 */
		priv = rte_eth_devices[age_param->port_id].data->dev_private;
		age_info = GET_PORT_AGE_INFO(priv);
		rte_spinlock_lock(&age_info->aged_sl);
		if (rte_atomic_compare_exchange_strong_explicit(&age_param->state, &expected,
						AGE_TMOUT,
						rte_memory_order_relaxed,
						rte_memory_order_relaxed)) {
			TAILQ_INSERT_TAIL(&age_info->aged_counters, cnt, next);
			MLX5_AGE_SET(age_info, MLX5_AGE_EVENT_NEW);
		}
		rte_spinlock_unlock(&age_info->aged_sl);
	}
	mlx5_age_event_prepare(sh);
}

/**
 * Handler for the HW respond about ready values from an asynchronous batch
 * query. This function is probably called by the host thread.
 *
 * @param[in] sh
 *   The pointer to the shared device context.
 * @param[in] async_id
 *   The Devx async ID.
 * @param[in] status
 *   The status of the completion.
 */
void
mlx5_flow_async_pool_query_handle(struct mlx5_dev_ctx_shared *sh,
				  uint64_t async_id, int status)
{
	struct mlx5_flow_counter_pool *pool =
		(struct mlx5_flow_counter_pool *)(uintptr_t)async_id;
	struct mlx5_counter_stats_raw *raw_to_free;
	uint8_t query_gen = pool->query_gen ^ 1;
	struct mlx5_flow_counter_mng *cmng = &sh->sws_cmng;
	enum mlx5_counter_type cnt_type =
		pool->is_aged ? MLX5_COUNTER_TYPE_AGE :
				MLX5_COUNTER_TYPE_ORIGIN;

	if (unlikely(status)) {
		raw_to_free = pool->raw_hw;
	} else {
		raw_to_free = pool->raw;
		if (pool->is_aged)
			mlx5_flow_aging_check(sh, pool);
		rte_spinlock_lock(&pool->sl);
		pool->raw = pool->raw_hw;
		rte_spinlock_unlock(&pool->sl);
		/* Be sure the new raw counters data is updated in memory. */
		rte_io_wmb();
		if (!TAILQ_EMPTY(&pool->counters[query_gen])) {
			rte_spinlock_lock(&cmng->csl[cnt_type]);
			TAILQ_CONCAT(&cmng->counters[cnt_type],
				     &pool->counters[query_gen], next);
			rte_spinlock_unlock(&cmng->csl[cnt_type]);
		}
	}
	LIST_INSERT_HEAD(&sh->sws_cmng.free_stat_raws, raw_to_free, next);
	pool->raw_hw = NULL;
	sh->sws_cmng.pending_queries--;
}

static int
flow_group_to_table(uint32_t port_id, uint32_t group, uint32_t *table,
		    const struct flow_grp_info *grp_info,
		    struct rte_flow_error *error)
{
	if (grp_info->transfer && grp_info->external &&
	    grp_info->fdb_def_rule) {
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
	DRV_LOG(DEBUG, "port %u group=%#x table=%#x", port_id, group, *table);
	return 0;
}

/**
 * Translate the rte_flow group index to HW table value.
 *
 * If tunnel offload is disabled, all group ids converted to flow table
 * id using the standard method.
 * If tunnel offload is enabled, group id can be converted using the
 * standard or tunnel conversion method. Group conversion method
 * selection depends on flags in `grp_info` parameter:
 * - Internal (grp_info.external == 0) groups conversion uses the
 *   standard method.
 * - Group ids in JUMP action converted with the tunnel conversion.
 * - Group id in rule attribute conversion depends on a rule type and
 *   group id value:
 *   ** non zero group attributes converted with the tunnel method
 *   ** zero group attribute in non-tunnel rule is converted using the
 *      standard method - there's only one root table
 *   ** zero group attribute in steer tunnel rule is converted with the
 *      standard method - single root table
 *   ** zero group attribute in match tunnel rule is a special OvS
 *      case: that value is used for portability reasons. That group
 *      id is converted with the tunnel conversion method.
 *
 * @param[in] dev
 *   Port device
 * @param[in] tunnel
 *   PMD tunnel offload object
 * @param[in] group
 *   rte_flow group index value.
 * @param[out] table
 *   HW table value.
 * @param[in] grp_info
 *   flags used for conversion
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_flow_group_to_table(struct rte_eth_dev *dev,
			 const struct mlx5_flow_tunnel *tunnel,
			 uint32_t group, uint32_t *table,
			 const struct flow_grp_info *grp_info,
			 struct rte_flow_error *error)
{
	int ret;
	bool standard_translation;

	if (!grp_info->skip_scale && grp_info->external &&
	    group < MLX5_MAX_TABLES_EXTERNAL)
		group *= MLX5_FLOW_TABLE_FACTOR;
	if (is_tunnel_offload_active(dev)) {
		standard_translation = !grp_info->external ||
					grp_info->std_tbl_fix;
	} else {
		standard_translation = true;
	}
	DRV_LOG(DEBUG,
		"port %u group=%u transfer=%d external=%d fdb_def_rule=%d translate=%s",
		dev->data->port_id, group, grp_info->transfer,
		grp_info->external, grp_info->fdb_def_rule,
		standard_translation ? "STANDARD" : "TUNNEL");
	if (standard_translation)
		ret = flow_group_to_table(dev->data->port_id, group, table,
					  grp_info, error);
	else
		ret = tunnel_flow_group_to_flow_table(dev, tunnel, group,
						      table, error);

	return ret;
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
	enum modify_reg idx;
	int n = 0;

	/* reg_c[0] and reg_c[1] are reserved. */
	priv->sh->flow_mreg_c[n++] = REG_C_0;
	priv->sh->flow_mreg_c[n++] = REG_C_1;
	/* Discover availability of other reg_c's. */
	for (idx = REG_C_2; idx <= REG_C_7; ++idx) {
		struct rte_flow_attr attr = {
			.group = MLX5_FLOW_MREG_CP_TABLE_GROUP,
			.priority = MLX5_FLOW_LOWEST_PRIO_INDICATOR,
			.ingress = 1,
		};
		struct rte_flow_item items[] = {
			[0] = {
				.type = RTE_FLOW_ITEM_TYPE_END,
			},
		};
		struct rte_flow_action actions[] = {
			[0] = {
				.type = (enum rte_flow_action_type)
					MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
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
		uint32_t flow_idx;
		struct rte_flow *flow;
		struct rte_flow_error error;

		if (!priv->sh->config.dv_flow_en)
			break;
		/* Create internal flow, validation skips copy action. */
		flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_GEN, &attr,
					items, actions, false, &error);
		flow = mlx5_ipool_get(priv->flows[MLX5_FLOW_TYPE_GEN],
				      flow_idx);
		if (!flow)
			continue;
		priv->sh->flow_mreg_c[n++] = idx;
		mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN, flow_idx);
	}
	for (; n < MLX5_MREG_C_NUM; ++n)
		priv->sh->flow_mreg_c[n] = REG_NON;
	priv->sh->metadata_regc_check_flag = 1;
	return 0;
}

int
save_dump_file(const uint8_t *data, uint32_t size,
	uint32_t type, uint64_t id, void *arg, FILE *file)
{
	char line[BUF_SIZE];
	uint32_t out = 0;
	uint32_t k;
	uint32_t actions_num;
	struct rte_flow_query_count *count;

	memset(line, 0, BUF_SIZE);
	switch (type) {
	case DR_DUMP_REC_TYPE_PMD_MODIFY_HDR:
		actions_num = *(uint32_t *)(arg);
		out += snprintf(line + out, BUF_SIZE - out, "%d,0x%" PRIx64 ",%d,",
				type, id, actions_num);
		break;
	case DR_DUMP_REC_TYPE_PMD_PKT_REFORMAT:
		out += snprintf(line + out, BUF_SIZE - out, "%d,0x%" PRIx64 ",",
				type, id);
		break;
	case DR_DUMP_REC_TYPE_PMD_COUNTER:
		count = (struct rte_flow_query_count *)arg;
		fprintf(file,
			"%d,0x%" PRIx64 ",%" PRIu64 ",%" PRIu64 "\n",
			type, id, count->hits, count->bytes);
		return 0;
	default:
		return -1;
	}

	for (k = 0; k < size; k++) {
		/* Make sure we do not overrun the line buffer length. */
		if (out >= BUF_SIZE - 4) {
			line[out] = '\0';
			break;
		}
		out += snprintf(line + out, BUF_SIZE - out, "%02x",
				(data[k]) & 0xff);
	}
	fprintf(file, "%s\n", line);
	return 0;
}

int
mlx5_flow_query_counter(struct rte_eth_dev *dev, struct rte_flow *flow,
	struct rte_flow_query_count *count, struct rte_flow_error *error)
{
	struct rte_flow_action action[2];
	enum mlx5_flow_drv_type ftype;
	const struct mlx5_flow_driver_ops *fops;

	if (!flow) {
		return rte_flow_error_set(error, ENOENT,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"invalid flow handle");
	}
	action[0].type = RTE_FLOW_ACTION_TYPE_COUNT;
	action[1].type = RTE_FLOW_ACTION_TYPE_END;
	if (flow->counter) {
		memset(count, 0, sizeof(struct rte_flow_query_count));
		ftype = (enum mlx5_flow_drv_type)(flow->drv_type);
		MLX5_ASSERT(ftype > MLX5_FLOW_TYPE_MIN &&
						ftype < MLX5_FLOW_TYPE_MAX);
		fops = flow_get_drv_ops(ftype);
		return fops->query(dev, flow, action, count, error);
	}
	return -1;
}

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
/**
 * Dump flow ipool data to file
 *
 * @param[in] dev
 *   The pointer to Ethernet device.
 * @param[in] file
 *   A pointer to a file for output.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   0 on success, a negative value otherwise.
 */
int
mlx5_flow_dev_dump_ipool(struct rte_eth_dev *dev,
	struct rte_flow *flow, FILE *file,
	struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_dv_modify_hdr_resource  *modify_hdr;
	struct mlx5_flow_dv_encap_decap_resource *encap_decap;
	uint32_t handle_idx;
	struct mlx5_flow_handle *dh;
	struct rte_flow_query_count count;
	uint32_t actions_num;
	const uint8_t *data;
	size_t size;
	uint64_t id;
	uint32_t type;
	void *action = NULL;

	if (!flow) {
		return rte_flow_error_set(error, ENOENT,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"invalid flow handle");
	}
	handle_idx = flow->dev_handles;
	/* query counter */
	if (flow->counter &&
	(!mlx5_counter_query(dev, flow->counter, false,
	&count.hits, &count.bytes, &action)) && action) {
		id = (uint64_t)(uintptr_t)action;
		type = DR_DUMP_REC_TYPE_PMD_COUNTER;
		save_dump_file(NULL, 0, type,
			id, (void *)&count, file);
	}

	while (handle_idx) {
		dh = mlx5_ipool_get(priv->sh->ipool
				[MLX5_IPOOL_MLX5_FLOW], handle_idx);
		if (!dh)
			continue;
		handle_idx = dh->next.next;

		/* Get modify_hdr and encap_decap buf from ipools. */
		encap_decap = NULL;
		modify_hdr = dh->dvh.modify_hdr;

		if (dh->dvh.rix_encap_decap) {
			encap_decap = mlx5_ipool_get(priv->sh->ipool
						[MLX5_IPOOL_DECAP_ENCAP],
						dh->dvh.rix_encap_decap);
		}
		if (modify_hdr) {
			data = (const uint8_t *)modify_hdr->actions;
			size = (size_t)(modify_hdr->actions_num) * 8;
			id = (uint64_t)(uintptr_t)modify_hdr->action;
			actions_num = modify_hdr->actions_num;
			type = DR_DUMP_REC_TYPE_PMD_MODIFY_HDR;
			save_dump_file(data, size, type, id,
						(void *)(&actions_num), file);
		}
		if (encap_decap) {
			data = encap_decap->buf;
			size = encap_decap->size;
			id = (uint64_t)(uintptr_t)encap_decap->action;
			type = DR_DUMP_REC_TYPE_PMD_PKT_REFORMAT;
			save_dump_file(data, size, type,
						id, NULL, file);
		}
	}
	return 0;
}

/**
 * Dump all flow's encap_decap/modify_hdr/counter data to file
 *
 * @param[in] dev
 *   The pointer to Ethernet device.
 * @param[in] file
 *   A pointer to a file for output.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   0 on success, a negative value otherwise.
 */
static int
mlx5_flow_dev_dump_sh_all(struct rte_eth_dev *dev,
	FILE *file, struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_hlist *h;
	struct mlx5_flow_dv_modify_hdr_resource  *modify_hdr;
	struct mlx5_flow_dv_encap_decap_resource *encap_decap;
	struct rte_flow_query_count count;
	uint32_t actions_num;
	const uint8_t *data;
	size_t size;
	uint64_t id;
	uint32_t type;
	uint32_t i;
	uint32_t j;
	struct mlx5_list_inconst *l_inconst;
	struct mlx5_list_entry *e;
	int lcore_index;
	struct mlx5_flow_counter_mng *cmng = &priv->sh->sws_cmng;
	uint32_t max;
	void *action;

	/* encap_decap hlist is lcore_share, get global core cache. */
	i = MLX5_LIST_GLOBAL;
	h = sh->encaps_decaps;
	if (h) {
		for (j = 0; j <= h->mask; j++) {
			l_inconst = &h->buckets[j].l;
			if (!l_inconst || !l_inconst->cache[i])
				continue;

			e = LIST_FIRST(&l_inconst->cache[i]->h);
			while (e) {
				encap_decap =
				(struct mlx5_flow_dv_encap_decap_resource *)e;
				data = encap_decap->buf;
				size = encap_decap->size;
				id = (uint64_t)(uintptr_t)encap_decap->action;
				type = DR_DUMP_REC_TYPE_PMD_PKT_REFORMAT;
				save_dump_file(data, size, type,
					id, NULL, file);
				e = LIST_NEXT(e, next);
			}
		}
	}

	/* get modify_hdr */
	h = sh->modify_cmds;
	if (h) {
		lcore_index = rte_lcore_index(rte_lcore_id());
		if (unlikely(lcore_index == -1)) {
			lcore_index = MLX5_LIST_NLCORE;
			rte_spinlock_lock(&h->l_const.lcore_lock);
		}
		i = lcore_index;

		if (lcore_index == MLX5_LIST_NLCORE) {
			for (i = 0; i <= (uint32_t)lcore_index; i++) {
				for (j = 0; j <= h->mask; j++) {
					l_inconst = &h->buckets[j].l;
					if (!l_inconst || !l_inconst->cache[i])
						continue;

					e = LIST_FIRST(&l_inconst->cache[i]->h);
					while (e) {
						modify_hdr =
						(struct mlx5_flow_dv_modify_hdr_resource *)e;
						data = (const uint8_t *)modify_hdr->actions;
						size = (size_t)(modify_hdr->actions_num) * 8;
						actions_num = modify_hdr->actions_num;
						id = (uint64_t)(uintptr_t)modify_hdr->action;
						type = DR_DUMP_REC_TYPE_PMD_MODIFY_HDR;
						save_dump_file(data, size, type, id,
								(void *)(&actions_num), file);
						e = LIST_NEXT(e, next);
					}
				}
			}
		} else {
			for (j = 0; j <= h->mask; j++) {
				l_inconst = &h->buckets[j].l;
				if (!l_inconst || !l_inconst->cache[i])
					continue;

				e = LIST_FIRST(&l_inconst->cache[i]->h);
				while (e) {
					modify_hdr =
					(struct mlx5_flow_dv_modify_hdr_resource *)e;
					data = (const uint8_t *)modify_hdr->actions;
					size = (size_t)(modify_hdr->actions_num) * 8;
					actions_num = modify_hdr->actions_num;
					id = (uint64_t)(uintptr_t)modify_hdr->action;
					type = DR_DUMP_REC_TYPE_PMD_MODIFY_HDR;
					save_dump_file(data, size, type, id,
							(void *)(&actions_num), file);
					e = LIST_NEXT(e, next);
				}
			}
		}

		if (unlikely(lcore_index == MLX5_LIST_NLCORE))
			rte_spinlock_unlock(&h->l_const.lcore_lock);
	}

	/* get counter */
	MLX5_ASSERT(cmng->n_valid <= MLX5_COUNTER_POOLS_MAX_NUM);
	max = MLX5_COUNTERS_PER_POOL * cmng->n_valid;
	for (j = 1; j <= max; j++) {
		action = NULL;
		if ((!mlx5_counter_query(dev, j, false, &count.hits,
		&count.bytes, &action)) && action) {
			id = (uint64_t)(uintptr_t)action;
			type = DR_DUMP_REC_TYPE_PMD_COUNTER;
			save_dump_file(NULL, 0, type,
					id, (void *)&count, file);
		}
	}
	return 0;
}
#endif

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
 *   0 on success, a negative value otherwise.
 */
int
mlx5_flow_dev_dump(struct rte_eth_dev *dev, struct rte_flow *flow_idx,
		   FILE *file,
		   struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	uint32_t handle_idx;
	int ret;
	struct mlx5_flow_handle *dh;
	struct rte_flow *flow;

	if (!sh->config.dv_flow_en) {
		if (fputs("device dv flow disabled\n", file) <= 0)
			return -errno;
		return -ENOTSUP;
	}

	/* dump all */
	if (!flow_idx) {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		if (mlx5_flow_dev_dump_sh_all(dev, file, error))
			return -EINVAL;

		if (sh->config.dv_flow_en == 2)
			return mlx5dr_debug_dump(priv->dr_ctx, file);
#endif
		return mlx5_devx_cmd_flow_dump(sh->fdb_domain,
					       sh->rx_domain,
					       sh->tx_domain, file);
	}
	/* dump one */
	flow = mlx5_ipool_get(priv->flows[MLX5_FLOW_TYPE_GEN],
			(uintptr_t)(void *)flow_idx);
	if (!flow)
		return -EINVAL;

#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	mlx5_flow_dev_dump_ipool(dev, flow, file, error);
#endif
	handle_idx = flow->dev_handles;
	while (handle_idx) {
		dh = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_MLX5_FLOW],
				handle_idx);
		if (!dh)
			return -ENOENT;
		if (dh->drv_flow) {
			if (sh->config.dv_flow_en == 2)
				return -ENOTSUP;

			ret = mlx5_devx_cmd_flow_single_dump(dh->drv_flow,
							     file);
			if (ret)
				return -ENOENT;
		}
		handle_idx = dh->next.next;
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
 * @param[in] nb_countexts
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
 */
int
mlx5_flow_get_aged_flows(struct rte_eth_dev *dev, void **contexts,
			uint32_t nb_contexts, struct rte_flow_error *error)
{
	struct rte_flow_attr attr = { .transfer = 0 };

	return flow_get_drv_ops(flow_get_drv_type(dev, &attr))->get_aged_flows
		(dev, contexts, nb_contexts, error);
}

/**
 * Get aged-out flows per HWS queue.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] queue_id
 *   Flow queue to query.
 * @param[in] context
 *   The address of an array of pointers to the aged-out flows contexts.
 * @param[in] nb_countexts
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
 */
int
mlx5_flow_get_q_aged_flows(struct rte_eth_dev *dev, uint32_t queue_id,
			   void **contexts, uint32_t nb_contexts,
			   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;
	struct rte_flow_attr attr = { 0 };

	if (flow_get_drv_type(dev, &attr) == MLX5_FLOW_TYPE_HW) {
		fops = flow_get_drv_ops(MLX5_FLOW_TYPE_HW);
		return fops->get_q_aged_flows(dev, queue_id, contexts,
					      nb_contexts, error);
	}
	DRV_LOG(ERR, "port %u queue %u get aged flows is not supported.",
		dev->data->port_id, queue_id);
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "get Q aged flows with incorrect steering mode");
}

/* Wrapper for driver action_validate op callback */
static int
flow_drv_action_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_indir_action_conf *conf,
			 const struct rte_flow_action *action,
			 const struct mlx5_flow_driver_ops *fops,
			 struct rte_flow_error *error)
{
	static const char err_msg[] = "indirect action validation unsupported";

	if (!fops->action_validate) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return -rte_errno;
	}
	return fops->action_validate(dev, conf, action, error);
}

/**
 * Destroys the shared action by handle.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] handle
 *   Handle for the indirect action object to be destroyed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *
 * @note: wrapper for driver action_create op callback.
 */
static int
mlx5_action_handle_destroy(struct rte_eth_dev *dev,
			   struct rte_flow_action_handle *handle,
			   struct rte_flow_error *error)
{
	static const char err_msg[] = "indirect action destruction unsupported";
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));

	if (!fops->action_destroy) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return -rte_errno;
	}
	return fops->action_destroy(dev, handle, error);
}

/* Wrapper for driver action_destroy op callback */
static int
flow_drv_action_update(struct rte_eth_dev *dev,
		       struct rte_flow_action_handle *handle,
		       const void *update,
		       const struct mlx5_flow_driver_ops *fops,
		       struct rte_flow_error *error)
{
	static const char err_msg[] = "indirect action update unsupported";

	if (!fops->action_update) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return -rte_errno;
	}
	return fops->action_update(dev, handle, update, error);
}

/* Wrapper for driver action_destroy op callback */
static int
flow_drv_action_query(struct rte_eth_dev *dev,
		      const struct rte_flow_action_handle *handle,
		      void *data,
		      const struct mlx5_flow_driver_ops *fops,
		      struct rte_flow_error *error)
{
	static const char err_msg[] = "indirect action query unsupported";

	if (!fops->action_query) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return -rte_errno;
	}
	return fops->action_query(dev, handle, data, error);
}

/**
 * Create indirect action for reuse in multiple flow rules.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param conf
 *   Pointer to indirect action object configuration.
 * @param[in] action
 *   Action configuration for indirect action object creation.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 * @return
 *   A valid handle in case of success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_action_handle *
mlx5_action_handle_create(struct rte_eth_dev *dev,
			  const struct rte_flow_indir_action_conf *conf,
			  const struct rte_flow_action *action,
			  struct rte_flow_error *error)
{
	static const char err_msg[] = "indirect action creation unsupported";
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));

	if (flow_drv_action_validate(dev, conf, action, fops, error))
		return NULL;
	if (!fops->action_create) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return NULL;
	}
	return fops->action_create(dev, conf, action, error);
}

/**
 * Updates inplace the indirect action configuration pointed by *handle*
 * with the configuration provided as *update* argument.
 * The update of the indirect action configuration effects all flow rules
 * reusing the action via handle.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] handle
 *   Handle for the indirect action to be updated.
 * @param[in] update
 *   Action specification used to modify the action pointed by handle.
 *   *update* could be of same type with the action pointed by the *handle*
 *   handle argument, or some other structures like a wrapper, depending on
 *   the indirect action type.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_action_handle_update(struct rte_eth_dev *dev,
		struct rte_flow_action_handle *handle,
		const void *update,
		struct rte_flow_error *error)
{
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));
	int ret;
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_CT:
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		ret = 0;
		break;
	default:
		ret = flow_drv_action_validate(dev, NULL,
				(const struct rte_flow_action *)update,
				fops, error);
	}
	if (ret)
		return ret;
	return flow_drv_action_update(dev, handle, update, fops,
				      error);
}

/**
 * Query the indirect action by handle.
 *
 * This function allows retrieving action-specific data such as counters.
 * Data is gathered by special action which may be present/referenced in
 * more than one flow rule definition.
 *
 * see @RTE_FLOW_ACTION_TYPE_COUNT
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param[in] handle
 *   Handle for the indirect action to query.
 * @param[in, out] data
 *   Pointer to storage for the associated query data type.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. PMDs initialize this
 *   structure in case of error only.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_action_handle_query(struct rte_eth_dev *dev,
			 const struct rte_flow_action_handle *handle,
			 void *data,
			 struct rte_flow_error *error)
{
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));

	return flow_drv_action_query(dev, handle, data, fops, error);
}

static int
mlx5_action_handle_query_update(struct rte_eth_dev *dev,
				struct rte_flow_action_handle *handle,
				const void *update, void *query,
				enum rte_flow_query_update_mode qu_mode,
				struct rte_flow_error *error)
{
	struct rte_flow_attr attr = { .transfer = 0 };
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type(dev, &attr);
	const struct mlx5_flow_driver_ops *fops;

	if (drv_type == MLX5_FLOW_TYPE_MIN || drv_type == MLX5_FLOW_TYPE_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "invalid driver type");
	fops = flow_get_drv_ops(drv_type);
	if (!fops || !fops->action_query_update)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "no query_update handler");
	return fops->action_query_update(dev, handle, update,
					 query, qu_mode, error);
}


#define MLX5_DRV_FOPS_OR_ERR(dev, fops, drv_cb, ret)                           \
{                                                                              \
	struct rte_flow_attr attr = { .transfer = 0 };                         \
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type((dev), &attr);    \
	if (drv_type == MLX5_FLOW_TYPE_MIN ||                                  \
	    drv_type == MLX5_FLOW_TYPE_MAX) {                                  \
		rte_flow_error_set(error, ENOTSUP,                             \
				   RTE_FLOW_ERROR_TYPE_ACTION,                 \
				   NULL, "invalid driver type");               \
		return ret;                                                    \
	}                                                                      \
	(fops) = flow_get_drv_ops(drv_type);                                   \
	if (!(fops) || !(fops)->drv_cb) {                                      \
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, \
				   NULL, "no action_list handler");            \
		return ret;                                                    \
	}                                                                      \
}

static struct rte_flow_action_list_handle *
mlx5_action_list_handle_create(struct rte_eth_dev *dev,
			       const struct rte_flow_indir_action_conf *conf,
			       const struct rte_flow_action *actions,
			       struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops, action_list_handle_create, NULL);
	return fops->action_list_handle_create(dev, conf, actions, error);
}

static int
mlx5_action_list_handle_destroy(struct rte_eth_dev *dev,
				struct rte_flow_action_list_handle *handle,
				struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops, action_list_handle_destroy, ENOTSUP);
	return fops->action_list_handle_destroy(dev, handle, error);
}

static int
mlx5_flow_action_list_handle_query_update(struct rte_eth_dev *dev,
					  const
					  struct rte_flow_action_list_handle *handle,
					  const void **update, void **query,
					  enum rte_flow_query_update_mode mode,
					  struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops,
			     action_list_handle_query_update, ENOTSUP);
	return fops->action_list_handle_query_update(dev, handle, update, query,
						     mode, error);
}
static int
mlx5_flow_calc_table_hash(struct rte_eth_dev *dev,
			  const struct rte_flow_template_table *table,
			  const struct rte_flow_item pattern[],
			  uint8_t pattern_template_index,
			  uint32_t *hash, struct rte_flow_error *error)
{
	struct rte_flow_attr attr = { .transfer = 0 };
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type(dev, &attr);
	const struct mlx5_flow_driver_ops *fops;

	if (drv_type == MLX5_FLOW_TYPE_MIN || drv_type == MLX5_FLOW_TYPE_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "invalid driver type");
	fops = flow_get_drv_ops(drv_type);
	if (!fops || !fops->action_query_update)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "no query_update handler");
	return fops->flow_calc_table_hash(dev, table, pattern, pattern_template_index,
					  hash, error);
}

static int
mlx5_flow_calc_encap_hash(struct rte_eth_dev *dev,
			  const struct rte_flow_item pattern[],
			  enum rte_flow_encap_hash_field dest_field,
			  uint8_t *hash,
			  struct rte_flow_error *error)
{
	enum mlx5_flow_drv_type drv_type = flow_get_drv_type(dev, NULL);
	const struct mlx5_flow_driver_ops *fops;

	if (drv_type == MLX5_FLOW_TYPE_MIN || drv_type == MLX5_FLOW_TYPE_MAX)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "invalid driver type");
	fops = flow_get_drv_ops(drv_type);
	if (!fops || !fops->flow_calc_encap_hash)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "no calc encap hash handler");
	return fops->flow_calc_encap_hash(dev, pattern, dest_field, hash, error);
}

static int
mlx5_template_table_resize(struct rte_eth_dev *dev,
			   struct rte_flow_template_table *table,
			   uint32_t nb_rules, struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops, table_resize, ENOTSUP);
	return fops->table_resize(dev, table, nb_rules, error);
}

static int
mlx5_table_resize_complete(struct rte_eth_dev *dev,
			   struct rte_flow_template_table *table,
			   struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops, table_resize_complete, ENOTSUP);
	return fops->table_resize_complete(dev, table, error);
}

static int
mlx5_flow_async_update_resized(struct rte_eth_dev *dev, uint32_t queue,
			       const struct rte_flow_op_attr *op_attr,
			       struct rte_flow *rule, void *user_data,
			       struct rte_flow_error *error)
{
	const struct mlx5_flow_driver_ops *fops;

	MLX5_DRV_FOPS_OR_ERR(dev, fops, flow_update_resized, ENOTSUP);
	return fops->flow_update_resized(dev, queue, op_attr, rule, user_data, error);
}

/**
 * Destroy all indirect actions (shared RSS).
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_action_handle_flush(struct rte_eth_dev *dev)
{
	struct rte_flow_error error;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss;
	int ret = 0;
	uint32_t idx;

	ILIST_FOREACH(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
		      priv->rss_shared_actions, idx, shared_rss, next) {
		ret |= mlx5_action_handle_destroy(dev,
		       (struct rte_flow_action_handle *)(uintptr_t)idx, &error);
	}
	return ret;
}

/**
 * Validate existing indirect actions against current device configuration
 * and attach them to device resources.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_action_handle_attach(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_ind_table_obj *ind_tbl, *ind_tbl_last;

	LIST_FOREACH(ind_tbl, &priv->standalone_ind_tbls, next) {
		const char *message;
		uint32_t queue_idx;

		ret = mlx5_validate_rss_queues(dev, ind_tbl->queues,
					       ind_tbl->queues_n,
					       &message, &queue_idx);
		if (ret != 0) {
			DRV_LOG(ERR, "Port %u cannot use queue %u in RSS: %s",
				dev->data->port_id, ind_tbl->queues[queue_idx],
				message);
			break;
		}
	}
	if (ret != 0)
		return ret;
	LIST_FOREACH(ind_tbl, &priv->standalone_ind_tbls, next) {
		ret = mlx5_ind_table_obj_attach(dev, ind_tbl);
		if (ret != 0) {
			DRV_LOG(ERR, "Port %u could not attach "
				"indirection table obj %p",
				dev->data->port_id, (void *)ind_tbl);
			goto error;
		}
	}

	return 0;
error:
	ind_tbl_last = ind_tbl;
	LIST_FOREACH(ind_tbl, &priv->standalone_ind_tbls, next) {
		if (ind_tbl == ind_tbl_last)
			break;
		if (mlx5_ind_table_obj_detach(dev, ind_tbl) != 0)
			DRV_LOG(CRIT, "Port %u could not detach "
				"indirection table obj %p on rollback",
				dev->data->port_id, (void *)ind_tbl);
	}
	return ret;
}

/**
 * Detach indirect actions of the device from its resources.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_action_handle_detach(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_ind_table_obj *ind_tbl, *ind_tbl_last;

	LIST_FOREACH(ind_tbl, &priv->standalone_ind_tbls, next) {
		ret = mlx5_ind_table_obj_detach(dev, ind_tbl);
		if (ret != 0) {
			DRV_LOG(ERR, "Port %u could not detach "
				"indirection table obj %p",
				dev->data->port_id, (void *)ind_tbl);
			goto error;
		}
	}
	return 0;
error:
	ind_tbl_last = ind_tbl;
	LIST_FOREACH(ind_tbl, &priv->standalone_ind_tbls, next) {
		if (ind_tbl == ind_tbl_last)
			break;
		if (mlx5_ind_table_obj_attach(dev, ind_tbl) != 0)
			DRV_LOG(CRIT, "Port %u could not attach "
				"indirection table obj %p on rollback",
				dev->data->port_id, (void *)ind_tbl);
	}
	return ret;
}

#ifndef HAVE_MLX5DV_DR
#define MLX5_DOMAIN_SYNC_FLOW ((1 << 0) | (1 << 1))
#else
#define MLX5_DOMAIN_SYNC_FLOW \
	(MLX5DV_DR_DOMAIN_SYNC_FLAGS_SW | MLX5DV_DR_DOMAIN_SYNC_FLAGS_HW)
#endif

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_sync_flow, 20.11)
int rte_pmd_mlx5_sync_flow(uint16_t port_id, uint32_t domains)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	const struct mlx5_flow_driver_ops *fops;
	int ret;
	struct rte_flow_attr attr = { .transfer = 0 };

	fops = flow_get_drv_ops(flow_get_drv_type(dev, &attr));
	ret = fops->sync_domain(dev, domains, MLX5_DOMAIN_SYNC_FLOW);
	if (ret > 0)
		ret = -ret;
	return ret;
}

const struct mlx5_flow_tunnel *
mlx5_get_tof(const struct rte_flow_item *item,
	     const struct rte_flow_action *action,
	     enum mlx5_tof_rule_type *rule_type)
{
	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == (typeof(item->type))
				  MLX5_RTE_FLOW_ITEM_TYPE_TUNNEL) {
			*rule_type = MLX5_TUNNEL_OFFLOAD_MATCH_RULE;
			return flow_items_to_tunnel(item);
		}
	}
	for (; action->conf != RTE_FLOW_ACTION_TYPE_END; action++) {
		if (action->type == (typeof(action->type))
				    MLX5_RTE_FLOW_ACTION_TYPE_TUNNEL_SET) {
			*rule_type = MLX5_TUNNEL_OFFLOAD_SET_RULE;
			return flow_actions_to_tunnel(action);
		}
	}
	return NULL;
}

/**
 * tunnel offload functionality is defined for DV environment only
 */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
__extension__
union tunnel_offload_mark {
	uint32_t val;
	struct {
		uint32_t app_reserve:8;
		uint32_t table_id:15;
		uint32_t transfer:1;
		uint32_t _unused_:8;
	};
};

static bool
mlx5_access_tunnel_offload_db
	(struct rte_eth_dev *dev,
	 bool (*match)(struct rte_eth_dev *,
		       struct mlx5_flow_tunnel *, const void *),
	 void (*hit)(struct rte_eth_dev *, struct mlx5_flow_tunnel *, void *),
	 void (*miss)(struct rte_eth_dev *, void *),
	 void *ctx, bool lock_op);

static int
flow_tunnel_add_default_miss(struct rte_eth_dev *dev,
			     struct rte_flow *flow,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_action *app_actions,
			     uint32_t flow_idx,
			     const struct mlx5_flow_tunnel *tunnel,
			     struct tunnel_default_miss_ctx *ctx,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow *dev_flow;
	struct rte_flow_attr miss_attr = *attr;
	const struct rte_flow_item miss_items[2] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = NULL,
			.last = NULL,
			.mask = NULL
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
			.spec = NULL,
			.last = NULL,
			.mask = NULL
		}
	};
	union tunnel_offload_mark mark_id;
	struct rte_flow_action_mark miss_mark;
	struct rte_flow_action miss_actions[3] = {
		[0] = { .type = RTE_FLOW_ACTION_TYPE_MARK, .conf = &miss_mark },
		[2] = { .type = RTE_FLOW_ACTION_TYPE_END,  .conf = NULL }
	};
	const struct rte_flow_action_jump *jump_data;
	uint32_t i, flow_table = 0; /* prevent compilation warning */
	struct flow_grp_info grp_info = {
		.external = 1,
		.transfer = attr->transfer,
		.fdb_def_rule = !!priv->fdb_def_rule,
		.std_tbl_fix = 0,
	};
	int ret;

	if (!attr->transfer) {
		uint32_t q_size;

		miss_actions[1].type = RTE_FLOW_ACTION_TYPE_RSS;
		q_size = priv->reta_idx_n * sizeof(ctx->queue[0]);
		ctx->queue = mlx5_malloc(MLX5_MEM_SYS | MLX5_MEM_ZERO, q_size,
					 0, SOCKET_ID_ANY);
		if (!ctx->queue)
			return rte_flow_error_set
				(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				NULL, "invalid default miss RSS");
		ctx->action_rss.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
		ctx->action_rss.level = 0,
		ctx->action_rss.types = priv->rss_conf.rss_hf,
		ctx->action_rss.key_len = priv->rss_conf.rss_key_len,
		ctx->action_rss.queue_num = priv->reta_idx_n,
		ctx->action_rss.key = priv->rss_conf.rss_key,
		ctx->action_rss.queue = ctx->queue;
		if (!priv->reta_idx_n || !priv->rxqs_n)
			return rte_flow_error_set
				(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				NULL, "invalid port configuration");
		if (!(dev->data->dev_conf.rxmode.mq_mode & RTE_ETH_MQ_RX_RSS_FLAG))
			ctx->action_rss.types = 0;
		for (i = 0; i != priv->reta_idx_n; ++i)
			ctx->queue[i] = (*priv->reta_idx)[i];
	} else {
		miss_actions[1].type = RTE_FLOW_ACTION_TYPE_JUMP;
		ctx->miss_jump.group = MLX5_TNL_MISS_FDB_JUMP_GRP;
	}
	miss_actions[1].conf = (typeof(miss_actions[1].conf))ctx->raw;
	for (; app_actions->type != RTE_FLOW_ACTION_TYPE_JUMP; app_actions++);
	jump_data = app_actions->conf;
	miss_attr.priority = MLX5_TNL_MISS_RULE_PRIORITY;
	miss_attr.group = jump_data->group;
	ret = mlx5_flow_group_to_table(dev, tunnel, jump_data->group,
				       &flow_table, &grp_info, error);
	if (ret)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
					  NULL, "invalid tunnel id");
	mark_id.app_reserve = 0;
	mark_id.table_id = tunnel_flow_tbl_to_id(flow_table);
	mark_id.transfer = !!attr->transfer;
	mark_id._unused_ = 0;
	miss_mark.id = mark_id.val;
	dev_flow = flow_drv_prepare(dev, flow, &miss_attr,
				    miss_items, miss_actions, flow_idx, error);
	if (!dev_flow)
		return -rte_errno;
	dev_flow->flow = flow;
	dev_flow->external = true;
	dev_flow->tunnel = tunnel;
	dev_flow->tof_type = MLX5_TUNNEL_OFFLOAD_MISS_RULE;
	/* Subflow object was created, we must include one in the list. */
	SILIST_INSERT(&flow->dev_handles, dev_flow->handle_idx,
		      dev_flow->handle, next);
	DRV_LOG(DEBUG,
		"port %u tunnel type=%d id=%u miss rule priority=%u group=%u",
		dev->data->port_id, tunnel->app_tunnel.type,
		tunnel->tunnel_id, miss_attr.priority, miss_attr.group);
	ret = flow_drv_translate(dev, dev_flow, &miss_attr, miss_items,
				  miss_actions, error);
	if (!ret)
		ret = flow_mreg_update_copy_table(dev, flow, miss_actions,
						  error);

	return ret;
}

static const struct mlx5_flow_tbl_data_entry  *
tunnel_mark_decode(struct rte_eth_dev *dev, uint32_t mark)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_list_entry *he;
	union tunnel_offload_mark mbits = { .val = mark };
	union mlx5_flow_tbl_key table_key = {
		{
			.level = tunnel_id_to_flow_tbl(mbits.table_id),
			.id = 0,
			.reserved = 0,
			.dummy = 0,
			.is_fdb = !!mbits.transfer,
			.is_egress = 0,
		}
	};
	struct mlx5_flow_cb_ctx ctx = {
		.data = &table_key.v64,
	};

	he = mlx5_hlist_lookup(sh->flow_tbls, table_key.v64, &ctx);
	return he ?
	       container_of(he, struct mlx5_flow_tbl_data_entry, entry) : NULL;
}

static void
mlx5_flow_tunnel_grp2tbl_remove_cb(void *tool_ctx,
				   struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct tunnel_tbl_entry *tte = container_of(entry, typeof(*tte), hash);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_TNL_TBL_ID],
			tunnel_flow_tbl_to_id(tte->flow_table));
	mlx5_free(tte);
}

static int
mlx5_flow_tunnel_grp2tbl_match_cb(void *tool_ctx __rte_unused,
				  struct mlx5_list_entry *entry, void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	union tunnel_tbl_key tbl = {
		.val = *(uint64_t *)(ctx->data),
	};
	struct tunnel_tbl_entry *tte = container_of(entry, typeof(*tte), hash);

	return tbl.tunnel_id != tte->tunnel_id || tbl.group != tte->group;
}

static struct mlx5_list_entry *
mlx5_flow_tunnel_grp2tbl_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct tunnel_tbl_entry *tte;
	union tunnel_tbl_key tbl = {
		.val = *(uint64_t *)(ctx->data),
	};

	tte = mlx5_malloc(MLX5_MEM_SYS | MLX5_MEM_ZERO,
			  sizeof(*tte), 0,
			  SOCKET_ID_ANY);
	if (!tte)
		goto err;
	mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_TNL_TBL_ID],
			  &tte->flow_table);
	if (tte->flow_table >= MLX5_MAX_TABLES) {
		DRV_LOG(ERR, "Tunnel TBL ID %d exceed max limit.",
			tte->flow_table);
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_TNL_TBL_ID],
				tte->flow_table);
		goto err;
	} else if (!tte->flow_table) {
		goto err;
	}
	tte->flow_table = tunnel_id_to_flow_tbl(tte->flow_table);
	tte->tunnel_id = tbl.tunnel_id;
	tte->group = tbl.group;
	return &tte->hash;
err:
	if (tte)
		mlx5_free(tte);
	return NULL;
}

static struct mlx5_list_entry *
mlx5_flow_tunnel_grp2tbl_clone_cb(void *tool_ctx __rte_unused,
				  struct mlx5_list_entry *oentry,
				  void *cb_ctx __rte_unused)
{
	struct tunnel_tbl_entry *tte = mlx5_malloc(MLX5_MEM_SYS, sizeof(*tte),
						   0, SOCKET_ID_ANY);

	if (!tte)
		return NULL;
	memcpy(tte, oentry, sizeof(*tte));
	return &tte->hash;
}

static void
mlx5_flow_tunnel_grp2tbl_clone_free_cb(void *tool_ctx __rte_unused,
				       struct mlx5_list_entry *entry)
{
	struct tunnel_tbl_entry *tte = container_of(entry, typeof(*tte), hash);

	mlx5_free(tte);
}

static uint32_t
tunnel_flow_group_to_flow_table(struct rte_eth_dev *dev,
				const struct mlx5_flow_tunnel *tunnel,
				uint32_t group, uint32_t *table,
				struct rte_flow_error *error)
{
	struct mlx5_list_entry *he;
	struct tunnel_tbl_entry *tte;
	union tunnel_tbl_key key = {
		.tunnel_id = tunnel ? tunnel->tunnel_id : 0,
		.group = group
	};
	struct mlx5_flow_tunnel_hub *thub = mlx5_tunnel_hub(dev);
	struct mlx5_hlist *group_hash;
	struct mlx5_flow_cb_ctx ctx = {
		.data = &key.val,
	};

	group_hash = tunnel ? tunnel->groups : thub->groups;
	he = mlx5_hlist_register(group_hash, key.val, &ctx);
	if (!he)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
					  NULL,
					  "tunnel group index not supported");
	tte = container_of(he, typeof(*tte), hash);
	*table = tte->flow_table;
	DRV_LOG(DEBUG, "port %u tunnel %u group=%#x table=%#x",
		dev->data->port_id, key.tunnel_id, group, *table);
	return 0;
}

static void
mlx5_flow_tunnel_free(struct rte_eth_dev *dev,
		      struct mlx5_flow_tunnel *tunnel)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_indexed_pool *ipool;

	DRV_LOG(DEBUG, "port %u release pmd tunnel id=0x%x",
		dev->data->port_id, tunnel->tunnel_id);
	LIST_REMOVE(tunnel, chain);
	mlx5_hlist_destroy(tunnel->groups);
	ipool = priv->sh->ipool[MLX5_IPOOL_TUNNEL_ID];
	mlx5_ipool_free(ipool, tunnel->tunnel_id);
}

static bool
mlx5_access_tunnel_offload_db
	(struct rte_eth_dev *dev,
	 bool (*match)(struct rte_eth_dev *,
		       struct mlx5_flow_tunnel *, const void *),
	 void (*hit)(struct rte_eth_dev *, struct mlx5_flow_tunnel *, void *),
	 void (*miss)(struct rte_eth_dev *, void *),
	 void *ctx, bool lock_op)
{
	bool verdict = false;
	struct mlx5_flow_tunnel_hub *thub = mlx5_tunnel_hub(dev);
	struct mlx5_flow_tunnel *tunnel;

	rte_spinlock_lock(&thub->sl);
	LIST_FOREACH(tunnel, &thub->tunnels, chain) {
		verdict = match(dev, tunnel, (const void *)ctx);
		if (verdict)
			break;
	}
	if (!lock_op)
		rte_spinlock_unlock(&thub->sl);
	if (verdict && hit)
		hit(dev, tunnel, ctx);
	if (!verdict && miss)
		miss(dev, ctx);
	if (lock_op)
		rte_spinlock_unlock(&thub->sl);

	return verdict;
}

struct tunnel_db_find_tunnel_id_ctx {
	uint32_t tunnel_id;
	struct mlx5_flow_tunnel *tunnel;
};

static bool
find_tunnel_id_match(struct rte_eth_dev *dev,
		     struct mlx5_flow_tunnel *tunnel, const void *x)
{
	const struct tunnel_db_find_tunnel_id_ctx *ctx = x;

	RTE_SET_USED(dev);
	return tunnel->tunnel_id == ctx->tunnel_id;
}

static void
find_tunnel_id_hit(struct rte_eth_dev *dev,
		   struct mlx5_flow_tunnel *tunnel, void *x)
{
	struct tunnel_db_find_tunnel_id_ctx *ctx = x;
	RTE_SET_USED(dev);
	ctx->tunnel = tunnel;
}

static struct mlx5_flow_tunnel *
mlx5_find_tunnel_id(struct rte_eth_dev *dev, uint32_t id)
{
	struct tunnel_db_find_tunnel_id_ctx ctx = {
		.tunnel_id = id,
	};

	mlx5_access_tunnel_offload_db(dev, find_tunnel_id_match,
				      find_tunnel_id_hit, NULL, &ctx, true);

	return ctx.tunnel;
}

static struct mlx5_flow_tunnel *
mlx5_flow_tunnel_allocate(struct rte_eth_dev *dev,
			  const struct rte_flow_tunnel *app_tunnel)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_indexed_pool *ipool;
	struct mlx5_flow_tunnel *tunnel;
	uint32_t id;

	ipool = priv->sh->ipool[MLX5_IPOOL_TUNNEL_ID];
	tunnel = mlx5_ipool_zmalloc(ipool, &id);
	if (!tunnel)
		return NULL;
	if (id >= MLX5_MAX_TUNNELS) {
		mlx5_ipool_free(ipool, id);
		DRV_LOG(ERR, "Tunnel ID %d exceed max limit.", id);
		return NULL;
	}
	tunnel->groups = mlx5_hlist_create("tunnel groups", 64, false, true,
					   priv->sh,
					   mlx5_flow_tunnel_grp2tbl_create_cb,
					   mlx5_flow_tunnel_grp2tbl_match_cb,
					   mlx5_flow_tunnel_grp2tbl_remove_cb,
					   mlx5_flow_tunnel_grp2tbl_clone_cb,
					mlx5_flow_tunnel_grp2tbl_clone_free_cb);
	if (!tunnel->groups) {
		mlx5_ipool_free(ipool, id);
		return NULL;
	}
	/* initiate new PMD tunnel */
	memcpy(&tunnel->app_tunnel, app_tunnel, sizeof(*app_tunnel));
	tunnel->tunnel_id = id;
	tunnel->action.type = (typeof(tunnel->action.type))
			      MLX5_RTE_FLOW_ACTION_TYPE_TUNNEL_SET;
	tunnel->action.conf = tunnel;
	tunnel->item.type = (typeof(tunnel->item.type))
			    MLX5_RTE_FLOW_ITEM_TYPE_TUNNEL;
	tunnel->item.spec = tunnel;
	tunnel->item.last = NULL;
	tunnel->item.mask = NULL;

	DRV_LOG(DEBUG, "port %u new pmd tunnel id=0x%x",
		dev->data->port_id, tunnel->tunnel_id);

	return tunnel;
}

struct tunnel_db_get_tunnel_ctx {
	const struct rte_flow_tunnel *app_tunnel;
	struct mlx5_flow_tunnel *tunnel;
};

static bool get_tunnel_match(struct rte_eth_dev *dev,
			     struct mlx5_flow_tunnel *tunnel, const void *x)
{
	const struct tunnel_db_get_tunnel_ctx *ctx = x;

	RTE_SET_USED(dev);
	return !memcmp(ctx->app_tunnel, &tunnel->app_tunnel,
		       sizeof(*ctx->app_tunnel));
}

static void get_tunnel_hit(struct rte_eth_dev *dev,
			   struct mlx5_flow_tunnel *tunnel, void *x)
{
	/* called under tunnel spinlock protection */
	struct tunnel_db_get_tunnel_ctx *ctx = x;

	RTE_SET_USED(dev);
	tunnel->refctn++;
	ctx->tunnel = tunnel;
}

static void get_tunnel_miss(struct rte_eth_dev *dev, void *x)
{
	/* called under tunnel spinlock protection */
	struct mlx5_flow_tunnel_hub *thub = mlx5_tunnel_hub(dev);
	struct tunnel_db_get_tunnel_ctx *ctx = x;

	rte_spinlock_unlock(&thub->sl);
	ctx->tunnel = mlx5_flow_tunnel_allocate(dev, ctx->app_tunnel);
	rte_spinlock_lock(&thub->sl);
	if (ctx->tunnel) {
		ctx->tunnel->refctn = 1;
		LIST_INSERT_HEAD(&thub->tunnels, ctx->tunnel, chain);
	}
}


static int
mlx5_get_flow_tunnel(struct rte_eth_dev *dev,
		     const struct rte_flow_tunnel *app_tunnel,
		     struct mlx5_flow_tunnel **tunnel)
{
	struct tunnel_db_get_tunnel_ctx ctx = {
		.app_tunnel = app_tunnel,
	};

	mlx5_access_tunnel_offload_db(dev, get_tunnel_match, get_tunnel_hit,
				      get_tunnel_miss, &ctx, true);
	*tunnel = ctx.tunnel;
	return ctx.tunnel ? 0 : -ENOMEM;
}

void mlx5_release_tunnel_hub(struct mlx5_dev_ctx_shared *sh, uint16_t port_id)
{
	struct mlx5_flow_tunnel_hub *thub = sh->tunnel_hub;

	if (!thub)
		return;
	if (!LIST_EMPTY(&thub->tunnels))
		DRV_LOG(WARNING, "port %u tunnels present", port_id);
	mlx5_hlist_destroy(thub->groups);
	mlx5_free(thub);
}

int mlx5_alloc_tunnel_hub(struct mlx5_dev_ctx_shared *sh)
{
	int err;
	struct mlx5_flow_tunnel_hub *thub;

	thub = mlx5_malloc(MLX5_MEM_SYS | MLX5_MEM_ZERO, sizeof(*thub),
			   0, SOCKET_ID_ANY);
	if (!thub)
		return -ENOMEM;
	LIST_INIT(&thub->tunnels);
	rte_spinlock_init(&thub->sl);
	thub->groups = mlx5_hlist_create("flow groups", 64,
					 false, true, sh,
					 mlx5_flow_tunnel_grp2tbl_create_cb,
					 mlx5_flow_tunnel_grp2tbl_match_cb,
					 mlx5_flow_tunnel_grp2tbl_remove_cb,
					 mlx5_flow_tunnel_grp2tbl_clone_cb,
					mlx5_flow_tunnel_grp2tbl_clone_free_cb);
	if (!thub->groups) {
		err = -rte_errno;
		goto err;
	}
	sh->tunnel_hub = thub;

	return 0;

err:
	if (thub->groups)
		mlx5_hlist_destroy(thub->groups);
	if (thub)
		mlx5_free(thub);
	return err;
}

static inline int
mlx5_flow_tunnel_validate(struct rte_eth_dev *dev,
			  struct rte_flow_tunnel *tunnel,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->sh->config.dv_flow_en)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "flow DV interface is off");
	if (!is_tunnel_offload_active(dev))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "tunnel offload was not activated, consider setting dv_xmeta_en=3");
	if (!tunnel)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "no application tunnel");
	switch (tunnel->type) {
	default:
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "unsupported tunnel type");
	case RTE_FLOW_ITEM_TYPE_VXLAN:
	case RTE_FLOW_ITEM_TYPE_GRE:
	case RTE_FLOW_ITEM_TYPE_NVGRE:
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		break;
	}
	return 0;
}

static int
mlx5_flow_tunnel_decap_set(struct rte_eth_dev *dev,
		    struct rte_flow_tunnel *app_tunnel,
		    struct rte_flow_action **actions,
		    uint32_t *num_of_actions,
		    struct rte_flow_error *error)
{
	struct mlx5_flow_tunnel *tunnel;
	int ret = mlx5_flow_tunnel_validate(dev, app_tunnel, error);

	if (ret)
		return ret;
	ret = mlx5_get_flow_tunnel(dev, app_tunnel, &tunnel);
	if (ret < 0) {
		return rte_flow_error_set(error, ret,
					  RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL,
					  "failed to initialize pmd tunnel");
	}
	*actions = &tunnel->action;
	*num_of_actions = 1;
	return 0;
}

static int
mlx5_flow_tunnel_match(struct rte_eth_dev *dev,
		       struct rte_flow_tunnel *app_tunnel,
		       struct rte_flow_item **items,
		       uint32_t *num_of_items,
		       struct rte_flow_error *error)
{
	struct mlx5_flow_tunnel *tunnel;
	int ret = mlx5_flow_tunnel_validate(dev, app_tunnel, error);

	if (ret)
		return ret;
	ret = mlx5_get_flow_tunnel(dev, app_tunnel, &tunnel);
	if (ret < 0) {
		return rte_flow_error_set(error, ret,
					  RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
					  "failed to initialize pmd tunnel");
	}
	*items = &tunnel->item;
	*num_of_items = 1;
	return 0;
}

struct tunnel_db_element_release_ctx {
	struct rte_flow_item *items;
	struct rte_flow_action *actions;
	uint32_t num_elements;
	struct rte_flow_error *error;
	int ret;
};

static bool
tunnel_element_release_match(struct rte_eth_dev *dev,
			     struct mlx5_flow_tunnel *tunnel, const void *x)
{
	const struct tunnel_db_element_release_ctx *ctx = x;

	RTE_SET_USED(dev);
	if (ctx->num_elements != 1)
		return false;
	else if (ctx->items)
		return ctx->items == &tunnel->item;
	else if (ctx->actions)
		return ctx->actions == &tunnel->action;

	return false;
}

static void
tunnel_element_release_hit(struct rte_eth_dev *dev,
			   struct mlx5_flow_tunnel *tunnel, void *x)
{
	struct tunnel_db_element_release_ctx *ctx = x;
	ctx->ret = 0;
	if (!(rte_atomic_fetch_sub_explicit(&tunnel->refctn, 1, rte_memory_order_relaxed) - 1))
		mlx5_flow_tunnel_free(dev, tunnel);
}

static void
tunnel_element_release_miss(struct rte_eth_dev *dev, void *x)
{
	struct tunnel_db_element_release_ctx *ctx = x;
	RTE_SET_USED(dev);
	ctx->ret = rte_flow_error_set(ctx->error, EINVAL,
				      RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				      "invalid argument");
}

static int
mlx5_flow_tunnel_item_release(struct rte_eth_dev *dev,
		       struct rte_flow_item *pmd_items,
		       uint32_t num_items, struct rte_flow_error *err)
{
	struct tunnel_db_element_release_ctx ctx = {
		.items = pmd_items,
		.actions = NULL,
		.num_elements = num_items,
		.error = err,
	};

	mlx5_access_tunnel_offload_db(dev, tunnel_element_release_match,
				      tunnel_element_release_hit,
				      tunnel_element_release_miss, &ctx, false);

	return ctx.ret;
}

static int
mlx5_flow_tunnel_action_release(struct rte_eth_dev *dev,
			 struct rte_flow_action *pmd_actions,
			 uint32_t num_actions, struct rte_flow_error *err)
{
	struct tunnel_db_element_release_ctx ctx = {
		.items = NULL,
		.actions = pmd_actions,
		.num_elements = num_actions,
		.error = err,
	};

	mlx5_access_tunnel_offload_db(dev, tunnel_element_release_match,
				      tunnel_element_release_hit,
				      tunnel_element_release_miss, &ctx, false);

	return ctx.ret;
}

static int
mlx5_flow_tunnel_get_restore_info(struct rte_eth_dev *dev,
				  struct rte_mbuf *m,
				  struct rte_flow_restore_info *info,
				  struct rte_flow_error *err)
{
	uint64_t ol_flags = m->ol_flags;
	const struct mlx5_flow_tbl_data_entry *tble;
	const uint64_t mask = RTE_MBUF_F_RX_FDIR | RTE_MBUF_F_RX_FDIR_ID;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->tunnel_enabled == 0)
		goto err;
	if ((ol_flags & mask) != mask)
		goto err;
	tble = tunnel_mark_decode(dev, m->hash.fdir.hi);
	if (!tble) {
		DRV_LOG(DEBUG, "port %u invalid miss tunnel mark %#x",
			dev->data->port_id, m->hash.fdir.hi);
		goto err;
	}
	MLX5_ASSERT(tble->tunnel);
	memcpy(&info->tunnel, &tble->tunnel->app_tunnel, sizeof(info->tunnel));
	info->group_id = tble->group_id;
	info->flags = RTE_FLOW_RESTORE_INFO_TUNNEL |
		      RTE_FLOW_RESTORE_INFO_GROUP_ID |
		      RTE_FLOW_RESTORE_INFO_ENCAPSULATED;

	return 0;

err:
	return rte_flow_error_set(err, EINVAL,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "failed to get restore info");
}

#else /* HAVE_IBV_FLOW_DV_SUPPORT */
static int
mlx5_flow_tunnel_decap_set(__rte_unused struct rte_eth_dev *dev,
			   __rte_unused struct rte_flow_tunnel *app_tunnel,
			   __rte_unused struct rte_flow_action **actions,
			   __rte_unused uint32_t *num_of_actions,
			   __rte_unused struct rte_flow_error *error)
{
	return -ENOTSUP;
}

static int
mlx5_flow_tunnel_match(__rte_unused struct rte_eth_dev *dev,
		       __rte_unused struct rte_flow_tunnel *app_tunnel,
		       __rte_unused struct rte_flow_item **items,
		       __rte_unused uint32_t *num_of_items,
		       __rte_unused struct rte_flow_error *error)
{
	return -ENOTSUP;
}

static int
mlx5_flow_tunnel_item_release(__rte_unused struct rte_eth_dev *dev,
			      __rte_unused struct rte_flow_item *pmd_items,
			      __rte_unused uint32_t num_items,
			      __rte_unused struct rte_flow_error *err)
{
	return -ENOTSUP;
}

static int
mlx5_flow_tunnel_action_release(__rte_unused struct rte_eth_dev *dev,
				__rte_unused struct rte_flow_action *pmd_action,
				__rte_unused uint32_t num_actions,
				__rte_unused struct rte_flow_error *err)
{
	return -ENOTSUP;
}

static int
mlx5_flow_tunnel_get_restore_info(__rte_unused struct rte_eth_dev *dev,
				  __rte_unused struct rte_mbuf *m,
				  __rte_unused struct rte_flow_restore_info *i,
				  __rte_unused struct rte_flow_error *err)
{
	return -ENOTSUP;
}

static int
flow_tunnel_add_default_miss(__rte_unused struct rte_eth_dev *dev,
			     __rte_unused struct rte_flow *flow,
			     __rte_unused const struct rte_flow_attr *attr,
			     __rte_unused const struct rte_flow_action *actions,
			     __rte_unused uint32_t flow_idx,
			     __rte_unused const struct mlx5_flow_tunnel *tunnel,
			     __rte_unused struct tunnel_default_miss_ctx *ctx,
			     __rte_unused struct rte_flow_error *error)
{
	return -ENOTSUP;
}

static struct mlx5_flow_tunnel *
mlx5_find_tunnel_id(__rte_unused struct rte_eth_dev *dev,
		    __rte_unused uint32_t id)
{
	return NULL;
}

static void
mlx5_flow_tunnel_free(__rte_unused struct rte_eth_dev *dev,
		      __rte_unused struct mlx5_flow_tunnel *tunnel)
{
}

static uint32_t
tunnel_flow_group_to_flow_table(__rte_unused struct rte_eth_dev *dev,
				__rte_unused const struct mlx5_flow_tunnel *t,
				__rte_unused uint32_t group,
				__rte_unused uint32_t *table,
				struct rte_flow_error *error)
{
	return rte_flow_error_set(error, ENOTSUP,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "tunnel offload requires DV support");
}

void
mlx5_release_tunnel_hub(__rte_unused struct mlx5_dev_ctx_shared *sh,
			__rte_unused  uint16_t port_id)
{
}
#endif /* HAVE_IBV_FLOW_DV_SUPPORT */

/* Flex flow item API */
static struct rte_flow_item_flex_handle *
mlx5_flow_flex_item_create(struct rte_eth_dev *dev,
			   const struct rte_flow_item_flex_conf *conf,
			   struct rte_flow_error *error)
{
	static const char err_msg[] = "flex item creation unsupported";
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));

	if (!priv->pci_dev) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "create flex item on PF only");
		return NULL;
	}
	switch (priv->pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_BLUEFIELD2:
	case PCI_DEVICE_ID_MELLANOX_BLUEFIELD3:
		break;
	default:
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "flex item available on BlueField ports only");
		return NULL;
	}
	if (!fops->item_create) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return NULL;
	}
	return fops->item_create(dev, conf, error);
}

static int
mlx5_flow_flex_item_release(struct rte_eth_dev *dev,
			    const struct rte_flow_item_flex_handle *handle,
			    struct rte_flow_error *error)
{
	static const char err_msg[] = "flex item release unsupported";
	struct rte_flow_attr attr = { .transfer = 0 };
	const struct mlx5_flow_driver_ops *fops =
			flow_get_drv_ops(flow_get_drv_type(dev, &attr));

	if (!fops->item_release) {
		DRV_LOG(ERR, "port %u %s.", dev->data->port_id, err_msg);
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, err_msg);
		return -rte_errno;
	}
	return fops->item_release(dev, handle, error);
}

static void
mlx5_dbg__print_pattern(const struct rte_flow_item *item)
{
	int ret;
	struct rte_flow_error error;

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		char *item_name;
		ret = rte_flow_conv(RTE_FLOW_CONV_OP_ITEM_NAME_PTR, &item_name,
				    sizeof(item_name),
				    (void *)(uintptr_t)item->type, &error);
		if (ret > 0)
			printf("%s ", item_name);
		else
			printf("%d\n", (int)item->type);
	}
	printf("END\n");
}

static int
mlx5_flow_is_std_vxlan_port(const struct rte_flow_item *udp_item)
{
	const struct rte_flow_item_udp *spec = udp_item->spec;
	const struct rte_flow_item_udp *mask = udp_item->mask;
	uint16_t udp_dport = 0;

	if (spec != NULL) {
		if (!mask)
			mask = &rte_flow_item_udp_mask;
		udp_dport = rte_be_to_cpu_16(spec->hdr.dst_port &
				mask->hdr.dst_port);
	}
	return (!udp_dport || udp_dport == MLX5_UDP_PORT_VXLAN);
}

static const struct mlx5_flow_expand_node *
mlx5_flow_expand_rss_adjust_node(const struct rte_flow_item *pattern,
		unsigned int item_idx,
		const struct mlx5_flow_expand_node graph[],
		const struct mlx5_flow_expand_node *node)
{
	const struct rte_flow_item *item = pattern + item_idx, *prev_item;

	if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN &&
			node != NULL &&
			node->type == RTE_FLOW_ITEM_TYPE_VXLAN) {
		/*
		 * The expansion node is VXLAN and it is also the last
		 * expandable item in the pattern, so need to continue
		 * expansion of the inner tunnel.
		 */
		MLX5_ASSERT(item_idx > 0);
		prev_item = pattern + item_idx - 1;
		MLX5_ASSERT(prev_item->type == RTE_FLOW_ITEM_TYPE_UDP);
		if (mlx5_flow_is_std_vxlan_port(prev_item))
			return &graph[MLX5_EXPANSION_STD_VXLAN];
		return &graph[MLX5_EXPANSION_L3_VXLAN];
	}
	return node;
}

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
 * Discover the number of available flow priorities.
 *
 * @param dev
 *   Ethernet device.
 *
 * @return
 *   On success, number of available flow priorities.
 *   On failure, a negative errno-style code and rte_errno is set.
 */
int
mlx5_flow_discover_priorities(struct rte_eth_dev *dev)
{
	static const uint16_t vprio[] = {8, 16};
	const struct mlx5_priv *priv = dev->data->dev_private;
	const struct mlx5_flow_driver_ops *fops;
	enum mlx5_flow_drv_type type;
	int ret;

	type = mlx5_flow_os_get_type();
	if (type == MLX5_FLOW_TYPE_MAX) {
		type = MLX5_FLOW_TYPE_VERBS;
		if (priv->sh->cdev->config.devx && priv->sh->config.dv_flow_en)
			type = MLX5_FLOW_TYPE_DV;
	}
	fops = flow_get_drv_ops(type);
	if (fops->discover_priorities == NULL) {
		DRV_LOG(ERR, "Priority discovery not supported");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	ret = fops->discover_priorities(dev, vprio, RTE_DIM(vprio));
	if (ret < 0)
		return ret;
	switch (ret) {
	case 8:
		ret = RTE_DIM(priority_map_3);
		break;
	case 16:
		ret = RTE_DIM(priority_map_5);
		break;
	default:
		rte_errno = ENOTSUP;
		DRV_LOG(ERR,
			"port %u maximum priority: %d expected 8/16",
			dev->data->port_id, ret);
		return -rte_errno;
	}
	DRV_LOG(INFO, "port %u supported flow priorities:"
		" 0-%d for ingress or egress root table,"
		" 0-%d for non-root table or transfer root table.",
		dev->data->port_id, ret - 2,
		MLX5_NON_ROOT_FLOW_MAX_PRIO - 1);
	return ret;
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
uint32_t
mlx5_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
			  uint32_t subpriority)
{
	uint32_t res = 0;
	struct mlx5_priv *priv = dev->data->dev_private;

	switch (priv->sh->flow_max_priority) {
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
 * Get the priority for sending traffic to kernel table.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   On success: the value of priority for sending traffic to kernel table
 *   On failure: -1
 */
uint32_t
mlx5_get_send_to_kernel_priority(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t res;

	switch (priv->sh->flow_max_priority) {
	case RTE_DIM(priority_map_5):
		res = 15;
		break;
	case RTE_DIM(priority_map_3):
		res = 7;
		break;
	default:
		DRV_LOG(ERR,
			"port %u maximum priority: %d expected 8/16",
			dev->data->port_id, priv->sh->flow_max_priority);
		res = (uint32_t)-1;
	}
	return res;
}

/**
 * Get the E-Switch Manager vport id.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 *
 * @return
 *   The vport id.
 */
int16_t mlx5_flow_get_esw_manager_vport_id(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_common_device *cdev = priv->sh->cdev;

	/* New FW exposes E-Switch Manager vport ID, can use it directly. */
	if (cdev->config.hca_attr.esw_mgr_vport_id_valid)
		return (int16_t)cdev->config.hca_attr.esw_mgr_vport_id;

	if (priv->pci_dev == NULL)
		return 0;
	switch (priv->pci_dev->id.device_id) {
	case PCI_DEVICE_ID_MELLANOX_BLUEFIELD:
	case PCI_DEVICE_ID_MELLANOX_BLUEFIELD2:
	case PCI_DEVICE_ID_MELLANOX_BLUEFIELD3:
	/*
	 * In old FW which doesn't expose the E-Switch Manager vport ID in the capability,
	 * only the BF embedded CPUs control the E-Switch Manager port. Hence,
	 * ECPF vport ID is selected and not the host port (0) in any BF case.
	 */
		return (int16_t)MLX5_ECPF_VPORT_ID;
	default:
		return MLX5_PF_VPORT_ID;
	}
}

/**
 * Parse item to get the vport id.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] item
 *   The src port id match item.
 * @param[out] vport_id
 *   Pointer to put the vport id.
 * @param[out] all_ports
 *   Indicate if the item matches all ports.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int mlx5_flow_get_item_vport_id(struct rte_eth_dev *dev,
				const struct rte_flow_item *item,
				uint16_t *vport_id,
				bool *all_ports,
				struct rte_flow_error *error)
{
	struct mlx5_priv *port_priv;
	const struct rte_flow_item_port_id *pid_v = NULL;
	const struct rte_flow_item_ethdev *dev_v = NULL;
	uint32_t esw_mgr_port;
	uint32_t src_port;

	if (all_ports)
		*all_ports = false;
	switch (item->type) {
	case RTE_FLOW_ITEM_TYPE_PORT_ID:
		pid_v = item->spec;
		if (!pid_v)
			return 0;
		src_port = pid_v->id;
		esw_mgr_port = MLX5_PORT_ESW_MGR;
		break;
	case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
		dev_v = item->spec;
		if (!dev_v) {
			if (all_ports)
				*all_ports = true;
			return 0;
		}
		src_port = dev_v->port_id;
		esw_mgr_port = MLX5_REPRESENTED_PORT_ESW_MGR;
		break;
	case RTE_FLOW_ITEM_TYPE_PORT_REPRESENTOR:
		src_port = MLX5_REPRESENTED_PORT_ESW_MGR;
		esw_mgr_port = MLX5_REPRESENTED_PORT_ESW_MGR;
		break;
	default:
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
					  NULL, "Incorrect item type.");
	}
	if (src_port == esw_mgr_port) {
		*vport_id = mlx5_flow_get_esw_manager_vport_id(dev);
	} else {
		port_priv = mlx5_port_to_eswitch_info(src_port, false);
		if (!port_priv)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
						  NULL, "Failed to get port info.");
		*vport_id = port_priv->representor_id;
	}

	return 0;
}

int
mlx5_flow_pick_transfer_proxy(struct rte_eth_dev *dev,
			      uint16_t *proxy_port_id,
			      struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t port_id;

	if (!priv->sh->config.dv_esw_en)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "unable to provide a proxy port"
					  " without E-Switch configured");
	if (!priv->master && !priv->representor)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "unable to provide a proxy port"
					  " for port which is not a master"
					  " or a representor port");
	if (priv->master) {
		*proxy_port_id = dev->data->port_id;
		return 0;
	}
	MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
		const struct rte_eth_dev *port_dev = &rte_eth_devices[port_id];
		const struct mlx5_priv *port_priv = port_dev->data->dev_private;

		if (port_priv->master &&
		    port_priv->domain_id == priv->domain_id) {
			*proxy_port_id = port_id;
			return 0;
		}
	}
	return rte_flow_error_set(error, ENODEV,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "unable to find a proxy port");
}

/**
 * Discover IPv6 traffic class ID support in rdma-core and firmware.
 *
 * @param dev
 *   Ethernet device.
 *
 * @return
 *   0, rdma-core is good to work with firmware.
 *   -EOPNOTSUPP, rdma-core could not work with new IPv6 TC ID.
 */
int
mlx5_flow_discover_ipv6_tc_support(struct rte_eth_dev *dev)
{
	struct rte_flow_action_set_dscp set_dscp;
	struct rte_flow_attr attr;
	struct rte_flow_action actions[2];
	struct rte_flow_item items[3];
	struct rte_flow_error error;
	uint32_t flow_idx;

	memset(&attr, 0, sizeof(attr));
	memset(actions, 0, sizeof(actions));
	memset(items, 0, sizeof(items));
	attr.group = 1;
	attr.egress = 1;
	items[0].type = RTE_FLOW_ITEM_TYPE_ETH;
	items[1].type = RTE_FLOW_ITEM_TYPE_IPV6;
	items[2].type = RTE_FLOW_ITEM_TYPE_END;
	/* Random value */
	set_dscp.dscp = 9;
	actions[0].type = RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP;
	actions[0].conf = &set_dscp;
	actions[1].type = RTE_FLOW_ACTION_TYPE_END;

	flow_idx = mlx5_flow_list_create(dev, MLX5_FLOW_TYPE_GEN, &attr, items,
					 actions, true, &error);
	if (!flow_idx)
		return -EOPNOTSUPP;

	mlx5_flow_list_destroy(dev, MLX5_FLOW_TYPE_GEN, flow_idx);
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_create_geneve_tlv_parser, 24.03)
void *
rte_pmd_mlx5_create_geneve_tlv_parser(uint16_t port_id,
				      const struct rte_pmd_mlx5_geneve_tlv tlv_list[],
				      uint8_t nb_options)
{
#ifdef HAVE_MLX5_HWS_SUPPORT
	return mlx5_geneve_tlv_parser_create(port_id, tlv_list, nb_options);
#else
	(void)port_id;
	(void)tlv_list;
	(void)nb_options;
	DRV_LOG(ERR, "%s is not supported.", __func__);
	rte_errno = ENOTSUP;
	return NULL;
#endif
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_destroy_geneve_tlv_parser, 24.03)
int
rte_pmd_mlx5_destroy_geneve_tlv_parser(void *handle)
{
#ifdef HAVE_MLX5_HWS_SUPPORT
	return mlx5_geneve_tlv_parser_destroy(handle);
#else
	(void)handle;
	DRV_LOG(ERR, "%s is not supported.", __func__);
	rte_errno = ENOTSUP;
	return -rte_errno;
#endif
}

bool
mlx5_ctrl_flow_uc_dmac_exists(struct rte_eth_dev *dev, const struct rte_ether_addr *addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;
	bool exists = false;

	LIST_FOREACH(entry, &priv->hw_ctrl_flows, next) {
		if (entry->info.type == MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC &&
		    rte_is_same_ether_addr(addr, &entry->info.uc.dmac)) {
			exists = true;
			break;
		}
	}
	return exists;
}

bool
mlx5_ctrl_flow_uc_dmac_vlan_exists(struct rte_eth_dev *dev,
				   const struct rte_ether_addr *addr,
				   const uint16_t vid)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;
	bool exists = false;

	LIST_FOREACH(entry, &priv->hw_ctrl_flows, next) {
		if (entry->info.type == MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC_VLAN &&
		    rte_is_same_ether_addr(addr, &entry->info.uc.dmac) &&
		    vid == entry->info.uc.vlan) {
			exists = true;
			break;
		}
	}
	return exists;
}
