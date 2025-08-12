/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>

#include <mlx5_malloc.h>
#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_flow.h"
#include "mlx5_rx.h"
#include "rte_common.h"

#ifdef HAVE_MLX5_HWS_SUPPORT

struct mlx5_nta_rss_ctx {
	struct rte_eth_dev *dev;
	struct rte_flow_attr *attr;
	struct rte_flow_item *pattern;
	struct rte_flow_action *actions;
	const struct rte_flow_action_rss *rss_conf;
	struct rte_flow_error *error;
	struct mlx5_nta_rss_flow_head *head;
	uint64_t pattern_flags;
	enum mlx5_flow_type flow_type;
	bool external;
};

#define MLX5_RSS_PTYPE_ITEM_INDEX 0
#ifdef MLX5_RSS_PTYPE_DEBUG
#define MLX5_RSS_PTYPE_ACTION_INDEX 1
#else
#define MLX5_RSS_PTYPE_ACTION_INDEX 0
#endif

#define MLX5_RSS_PTYPE_ITEMS_NUM (MLX5_RSS_PTYPE_ITEM_INDEX + 2)
#define MLX5_RSS_PTYPE_ACTIONS_NUM (MLX5_RSS_PTYPE_ACTION_INDEX + 2)

static int
mlx5_nta_ptype_rss_flow_create(struct mlx5_nta_rss_ctx *ctx,
			       uint32_t ptype, uint64_t rss_type)
{
	int ret;
	struct rte_flow_hw *flow;
	struct rte_flow_item_ptype *ptype_spec = (void *)(uintptr_t)
				    ctx->pattern[MLX5_RSS_PTYPE_ITEM_INDEX].spec;
	struct rte_flow_action_rss *rss_conf = (void *)(uintptr_t)
				    ctx->actions[MLX5_RSS_PTYPE_ACTION_INDEX].conf;
	bool dbg_log = rte_log_can_log(mlx5_logtype, RTE_LOG_DEBUG);
	uint32_t mark_id = 0;
#ifdef MLX5_RSS_PTYPE_DEBUG
	struct rte_flow_action_mark *mark = (void *)(uintptr_t)
				     ctx->actions[MLX5_RSS_PTYPE_ACTION_INDEX - 1].conf;

	/*
	 * Inner L3 and L4 ptype values are too large for 24bit mark
	 */
	mark->id =
		((ptype & (RTE_PTYPE_INNER_L3_MASK | RTE_PTYPE_INNER_L4_MASK)) == ptype) ?
		ptype >> 20 : ptype;
	mark_id = mark->id;
	dbg_log = true;
#endif
	ptype_spec->packet_type = ptype;
	rss_conf->types = rss_type;
	ret = flow_hw_create_flow(ctx->dev, MLX5_FLOW_TYPE_GEN, ctx->attr,
				  ctx->pattern, ctx->actions,
				  MLX5_FLOW_ITEM_PTYPE, MLX5_FLOW_ACTION_RSS,
				  ctx->external, &flow, ctx->error);
	if (ret == 0) {
		SLIST_INSERT_HEAD(ctx->head, flow, nt2hws->next);
		if (dbg_log) {
			DRV_LOG(NOTICE,
				"PTYPE RSS: group %u ptype spec %#x rss types %#lx mark %#x\n",
			       ctx->attr->group, ptype_spec->packet_type,
			       (unsigned long)rss_conf->types, mark_id);
		}
	}
	return ret;
}

/*
 * Call conditions:
 * * Flow pattern did not include outer L3 and L4 items.
 * * RSS configuration had L3 hash types.
 */
static struct rte_flow_hw *
mlx5_hw_rss_expand_l3(struct mlx5_nta_rss_ctx *rss_ctx)
{
	int ret;
	int ptype_ip4, ptype_ip6;
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_ctx->rss_conf->types);

	if (rss_ctx->rss_conf->level < 2) {
		ptype_ip4 = RTE_PTYPE_L3_IPV4;
		ptype_ip6 = RTE_PTYPE_L3_IPV6;
	} else {
		ptype_ip4 = RTE_PTYPE_INNER_L3_IPV4;
		ptype_ip6 = RTE_PTYPE_INNER_L3_IPV6;
	}
	if (rss_types & MLX5_IPV4_LAYER_TYPES) {
		ret = mlx5_nta_ptype_rss_flow_create
			(rss_ctx, ptype_ip4, (rss_types & ~MLX5_IPV6_LAYER_TYPES));
		if (ret)
			goto error;
	}
	if (rss_types & MLX5_IPV6_LAYER_TYPES) {
		ret = mlx5_nta_ptype_rss_flow_create
			(rss_ctx, ptype_ip6, rss_types & ~MLX5_IPV4_LAYER_TYPES);
		if (ret)
			goto error;
	}
	return SLIST_FIRST(rss_ctx->head);

error:
	flow_hw_list_destroy(rss_ctx->dev, rss_ctx->flow_type,
			     (uintptr_t)SLIST_FIRST(rss_ctx->head));
	return NULL;
}

static void
mlx5_nta_rss_expand_l3_l4(struct mlx5_nta_rss_ctx *rss_ctx,
			  uint64_t rss_types, uint64_t rss_l3_types)
{
	int ret;
	int ptype_l3, ptype_l4_esp, ptype_l4_udp, ptype_l4_tcp;
	uint64_t rss = rss_types &
		 ~(rss_l3_types == MLX5_IPV4_LAYER_TYPES ?
		  MLX5_IPV6_LAYER_TYPES : MLX5_IPV4_LAYER_TYPES);


	if (rss_ctx->rss_conf->level < 2) {
		ptype_l3 = rss_l3_types == MLX5_IPV4_LAYER_TYPES ?
			   RTE_PTYPE_L3_IPV4 : RTE_PTYPE_L3_IPV6;
		ptype_l4_esp = RTE_PTYPE_L4_ESP;
		ptype_l4_udp = RTE_PTYPE_L4_UDP;
		ptype_l4_tcp = RTE_PTYPE_L4_TCP;
	} else {
		ptype_l3 = rss_l3_types == MLX5_IPV4_LAYER_TYPES ?
			   RTE_PTYPE_INNER_L3_IPV4 : RTE_PTYPE_INNER_L3_IPV6;
		ptype_l4_esp = RTE_PTYPE_INNER_L4_ESP;
		ptype_l4_udp = RTE_PTYPE_INNER_L4_UDP;
		ptype_l4_tcp = RTE_PTYPE_INNER_L4_TCP;
	}
	if (rss_types & RTE_ETH_RSS_ESP) {
		ret = mlx5_nta_ptype_rss_flow_create
			(rss_ctx, ptype_l3 | ptype_l4_esp,
			rss & ~(RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP));
		if (ret)
			goto error;
	}
	if (rss_types & RTE_ETH_RSS_UDP) {
		ret = mlx5_nta_ptype_rss_flow_create(rss_ctx,
			ptype_l3 | ptype_l4_udp,
			rss & ~(RTE_ETH_RSS_ESP | RTE_ETH_RSS_TCP));
		if (ret)
			goto error;
	}
	if (rss_types & RTE_ETH_RSS_TCP) {
		ret = mlx5_nta_ptype_rss_flow_create(rss_ctx,
			ptype_l3 | ptype_l4_tcp,
			rss & ~(RTE_ETH_RSS_ESP | RTE_ETH_RSS_UDP));
		if (ret)
			goto error;
	}
	return;
error:
	flow_hw_list_destroy(rss_ctx->dev, rss_ctx->flow_type,
			     (uintptr_t)SLIST_FIRST(rss_ctx->head));
}

/*
 * Call conditions:
 * * Flow pattern did not include L4 item.
 * * RSS configuration had L4 hash types.
 */
static struct rte_flow_hw *
mlx5_hw_rss_expand_l4(struct mlx5_nta_rss_ctx *rss_ctx)
{
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_ctx->rss_conf->types);
	uint64_t l3_item = rss_ctx->pattern_flags &
			   (rss_ctx->rss_conf->level < 2 ?
			    MLX5_FLOW_LAYER_OUTER_L3 : MLX5_FLOW_LAYER_INNER_L3);

	if (l3_item) {
		/*
		 * Outer L3 header was present in the original pattern.
		 * Expand L4 level only.
		 */
		if (l3_item & MLX5_FLOW_LAYER_L3_IPV4)
			mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types, MLX5_IPV4_LAYER_TYPES);
		else
			mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types, MLX5_IPV6_LAYER_TYPES);
	} else {
		if (rss_types & (MLX5_IPV4_LAYER_TYPES | MLX5_IPV6_LAYER_TYPES)) {
			mlx5_hw_rss_expand_l3(rss_ctx);
			/*
			 * No outer L3 item in application flow pattern.
			 * RSS hash types are L3 and L4.
			 * ** Expand L3 according to RSS configuration and L4.
			 */
			if (rss_types & MLX5_IPV4_LAYER_TYPES)
				mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types,
							  MLX5_IPV4_LAYER_TYPES);
			if (rss_types & MLX5_IPV6_LAYER_TYPES)
				mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types,
							  MLX5_IPV6_LAYER_TYPES);
		} else {
			/*
			 * No outer L3 item in application flow pattern,
			 * RSS hash type is L4 only.
			 */
			mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types,
						  MLX5_IPV4_LAYER_TYPES);
			mlx5_nta_rss_expand_l3_l4(rss_ctx, rss_types,
						  MLX5_IPV6_LAYER_TYPES);
		}
	}
	return SLIST_EMPTY(rss_ctx->head) ? NULL : SLIST_FIRST(rss_ctx->head);
}

static struct mlx5_indexed_pool *
mlx5_nta_ptype_ipool_create(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_indexed_pool_config ipool_cfg = {
		.size = 1,
		.trunk_size = 32,
		.grow_trunk = 5,
		.grow_shift = 1,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.max_idx = MLX5_FLOW_TABLE_PTYPE_RSS_NUM,
		.free = mlx5_free,
		.type = "mlx5_nta_ptype_rss"
	};
	return mlx5_ipool_create(&ipool_cfg);
}

static void
mlx5_hw_release_rss_ptype_group(struct rte_eth_dev *dev, uint32_t group)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->ptype_rss_groups)
		return;
	mlx5_ipool_free(priv->ptype_rss_groups, group);
}

static uint32_t
mlx5_hw_get_rss_ptype_group(struct rte_eth_dev *dev)
{
	void *obj;
	uint32_t idx = 0;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->ptype_rss_groups) {
		priv->ptype_rss_groups = mlx5_nta_ptype_ipool_create(dev);
		if (!priv->ptype_rss_groups) {
			DRV_LOG(DEBUG, "PTYPE RSS: failed to allocate groups pool");
			return 0;
		}
	}
	obj = mlx5_ipool_malloc(priv->ptype_rss_groups, &idx);
	if (!obj) {
		DRV_LOG(DEBUG, "PTYPE RSS: failed to fetch ptype group from the pool");
		return 0;
	}
	return idx + MLX5_FLOW_TABLE_PTYPE_RSS_BASE;
}

static struct rte_flow_hw *
mlx5_hw_rss_ptype_create_miss_flow(struct rte_eth_dev *dev,
				   const struct rte_flow_action_rss *rss_conf,
				   uint32_t ptype_group, bool external,
				   struct rte_flow_error *error)
{
	int ret;
	struct rte_flow_hw *flow = NULL;
	const struct rte_flow_attr miss_attr = {
		.ingress = 1,
		.group = ptype_group,
		.priority = 3
	};
	const struct rte_flow_item miss_pattern[2] = {
		[0] = { .type = RTE_FLOW_ITEM_TYPE_ETH },
		[1] = { .type = RTE_FLOW_ITEM_TYPE_END }
	};
	struct rte_flow_action miss_actions[] = {
#ifdef MLX5_RSS_PTYPE_DEBUG
		[MLX5_RSS_PTYPE_ACTION_INDEX - 1] = {
			.type = RTE_FLOW_ACTION_TYPE_MARK,
			.conf = &(const struct rte_flow_action_mark){.id = 0xfac}
		},
#endif
		[MLX5_RSS_PTYPE_ACTION_INDEX] = {
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = rss_conf
		},
		[MLX5_RSS_PTYPE_ACTION_INDEX + 1] = { .type = RTE_FLOW_ACTION_TYPE_END }
	};

	ret = flow_hw_create_flow(dev, MLX5_FLOW_TYPE_GEN, &miss_attr,
				  miss_pattern, miss_actions, 0,
				  MLX5_FLOW_ACTION_RSS, external, &flow, error);
	return ret == 0 ? flow : NULL;
}

static struct rte_flow_hw *
mlx5_hw_rss_ptype_create_base_flow(struct rte_eth_dev *dev,
				   const struct rte_flow_attr *attr,
				   const struct rte_flow_item pattern[],
				   const struct rte_flow_action orig_actions[],
				   uint32_t ptype_group, uint64_t item_flags,
				   uint64_t action_flags, bool external,
				   enum mlx5_flow_type flow_type,
				   struct rte_flow_error *error)
{
	int ret, i = 0;
	struct rte_flow_hw *flow = NULL;
	struct rte_flow_action actions[MLX5_HW_MAX_ACTS];
	enum mlx5_indirect_type indirect_type;
	const struct rte_flow_action_jump jump_conf = {
		.group = ptype_group
	};

	do {
		switch (orig_actions[i].type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			indirect_type = (typeof(indirect_type))
					MLX5_INDIRECT_ACTION_TYPE_GET
					(orig_actions[i].conf);
			if (indirect_type != MLX5_INDIRECT_ACTION_TYPE_RSS) {
				actions[i] = orig_actions[i];
				break;
			}
			/* Fall through */
		case RTE_FLOW_ACTION_TYPE_RSS:
			actions[i].type = RTE_FLOW_ACTION_TYPE_JUMP;
			actions[i].conf = &jump_conf;
			break;
		default:
			actions[i] = orig_actions[i];
		}

	} while (actions[i++].type != RTE_FLOW_ACTION_TYPE_END);
	action_flags &= ~MLX5_FLOW_ACTION_RSS;
	action_flags |= MLX5_FLOW_ACTION_JUMP;
	ret = flow_hw_create_flow(dev, flow_type, attr, pattern, actions,
				  item_flags, action_flags, external, &flow, error);
	return ret == 0 ? flow : NULL;
}

const struct rte_flow_action_rss *
flow_nta_locate_rss(struct rte_eth_dev *dev,
		    const struct rte_flow_action actions[],
		    struct rte_flow_error *error)
{
	const struct rte_flow_action *a;
	const struct rte_flow_action_rss *rss_conf = NULL;

	for (a = actions; a->type != RTE_FLOW_ACTION_TYPE_END; a++) {
		if (a->type == RTE_FLOW_ACTION_TYPE_RSS) {
			rss_conf = a->conf;
			break;
		}
		if (a->type == RTE_FLOW_ACTION_TYPE_INDIRECT &&
		    MLX5_INDIRECT_ACTION_TYPE_GET(a->conf) ==
		    MLX5_INDIRECT_ACTION_TYPE_RSS) {
			struct mlx5_priv *priv = dev->data->dev_private;
			struct mlx5_shared_action_rss *shared_rss;
			uint32_t handle = (uint32_t)(uintptr_t)a->conf;

			shared_rss = mlx5_ipool_get
				(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS],
				 MLX5_INDIRECT_ACTION_IDX_GET(handle));
			if (!shared_rss) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						   a->conf, "invalid shared RSS handle");
				return NULL;
			}
			rss_conf = &shared_rss->origin;
			break;
		}
	}
	if (a->type == RTE_FLOW_ACTION_TYPE_END) {
		rte_flow_error_set(error, 0, RTE_FLOW_ERROR_TYPE_NONE, NULL, NULL);
		return NULL;
	}
	return rss_conf;
}

static __rte_always_inline void
mlx5_nta_rss_init_ptype_ctx(struct mlx5_nta_rss_ctx *rss_ctx,
			    struct rte_eth_dev *dev,
			    struct rte_flow_attr *ptype_attr,
			    struct rte_flow_item *ptype_pattern,
			    struct rte_flow_action *ptype_actions,
			    const struct rte_flow_action_rss *rss_conf,
			    struct mlx5_nta_rss_flow_head *head,
			    struct rte_flow_error *error,
			    uint64_t item_flags,
			    enum mlx5_flow_type flow_type, bool external)
{
	rss_ctx->dev = dev;
	rss_ctx->attr = ptype_attr;
	rss_ctx->pattern = ptype_pattern;
	rss_ctx->actions = ptype_actions;
	rss_ctx->rss_conf = rss_conf;
	rss_ctx->error = error;
	rss_ctx->head = head;
	rss_ctx->pattern_flags = item_flags;
	rss_ctx->flow_type = flow_type;
	rss_ctx->external = external;
}

static struct rte_flow_hw *
flow_nta_create_single(struct rte_eth_dev *dev,
		       const struct rte_flow_attr *attr,
		       const struct rte_flow_item items[],
		       const struct rte_flow_action actions[],
		       struct rte_flow_action_rss *rss_conf,
		       int64_t item_flags, uint64_t action_flags,
		       bool external, bool copy_actions,
		       enum mlx5_flow_type flow_type,
		       struct rte_flow_error *error)
{
	int ret;
	struct rte_flow_hw *flow = NULL;
	struct rte_flow_action copy[MLX5_HW_MAX_ACTS];
	const struct rte_flow_action *_actions;

	if (copy_actions) {
		int i;

		_actions = copy;
		for (i = 0; ; i++) {
			copy[i] = actions[i];
			switch (actions[i].type) {
			case RTE_FLOW_ACTION_TYPE_RSS:
				copy[i].conf = rss_conf;
				break;
			case RTE_FLOW_ACTION_TYPE_INDIRECT:
				if (MLX5_INDIRECT_ACTION_TYPE_GET(actions[i].conf) ==
					MLX5_INDIRECT_ACTION_TYPE_RSS) {
					copy[i].type = RTE_FLOW_ACTION_TYPE_RSS;
					copy[i].conf = rss_conf;
				}
				break;
			case RTE_FLOW_ACTION_TYPE_END:
				goto end;
			default:
				break;
			}
		}
	} else {
		_actions = actions;
	}
end:
	ret = flow_hw_create_flow(dev, flow_type, attr, items, _actions,
				  item_flags, action_flags, external, &flow, error);
	return ret == 0 ? flow : NULL;
}

/*
 * MLX5 HW hashes IPv4 and IPv6 L3 headers and UDP, TCP, ESP L4 headers.
 * RSS expansion is required when RSS action was configured to hash
 * network protocol that was not mentioned in flow pattern.
 *
 */
#define MLX5_PTYPE_RSS_OUTER_MASK (RTE_PTYPE_L3_IPV4 | RTE_PTYPE_L3_IPV6 | \
				  RTE_PTYPE_L4_UDP | RTE_PTYPE_L4_TCP | \
				  RTE_PTYPE_L4_ESP)
#define MLX5_PTYPE_RSS_INNER_MASK (RTE_PTYPE_INNER_L3_IPV4 | RTE_PTYPE_INNER_L3_IPV6 | \
				  RTE_PTYPE_INNER_L4_TCP | RTE_PTYPE_INNER_L4_UDP)

struct rte_flow_hw *
flow_nta_handle_rss(struct rte_eth_dev *dev,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item items[],
		    const struct rte_flow_action actions[],
		    const struct rte_flow_action_rss *rss_conf,
		    uint64_t item_flags, uint64_t action_flags,
		    bool external, enum mlx5_flow_type flow_type,
		    struct rte_flow_error *error)
{
	struct rte_flow_hw *rss_base = NULL, *rss_next = NULL, *rss_miss = NULL;
	struct rte_flow_action_rss ptype_rss_conf = *rss_conf;
	struct mlx5_nta_rss_ctx rss_ctx;
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_conf->types);
	bool expand = true;
	bool copy_actions = false;
	bool inner_rss = rss_conf->level > 1;
	bool outer_rss = !inner_rss;
	bool l3_item = (outer_rss && (item_flags & MLX5_FLOW_LAYER_OUTER_L3)) ||
		       (inner_rss && (item_flags & MLX5_FLOW_LAYER_INNER_L3));
	bool l4_item = (outer_rss && (item_flags & MLX5_FLOW_LAYER_OUTER_L4)) ||
		       (inner_rss && (item_flags & MLX5_FLOW_LAYER_INNER_L4));
	bool l3_hash = rss_types & (MLX5_IPV4_LAYER_TYPES | MLX5_IPV6_LAYER_TYPES);
	bool l4_hash = rss_types & (RTE_ETH_RSS_UDP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_ESP);
	struct mlx5_nta_rss_flow_head expansion_head = SLIST_HEAD_INITIALIZER(0);
	struct rte_flow_attr ptype_attr = {
		.ingress = 1
	};
	struct rte_flow_item_ptype ptype_spec = { .packet_type = 0 };
	const struct rte_flow_item_ptype ptype_mask = {
		.packet_type = outer_rss ?
			MLX5_PTYPE_RSS_OUTER_MASK : MLX5_PTYPE_RSS_INNER_MASK
	};
	struct rte_flow_item ptype_pattern[MLX5_RSS_PTYPE_ITEMS_NUM] = {
		[MLX5_RSS_PTYPE_ITEM_INDEX] = {
			.type = RTE_FLOW_ITEM_TYPE_PTYPE,
			.spec = &ptype_spec,
			.mask = &ptype_mask
		},
		[MLX5_RSS_PTYPE_ITEM_INDEX + 1] = { .type = RTE_FLOW_ITEM_TYPE_END }
	};
	struct rte_flow_action ptype_actions[MLX5_RSS_PTYPE_ACTIONS_NUM] = {
#ifdef MLX5_RSS_PTYPE_DEBUG
		[MLX5_RSS_PTYPE_ACTION_INDEX - 1] = {
			.type = RTE_FLOW_ACTION_TYPE_MARK,
			.conf = &(const struct rte_flow_action_mark) {.id = 101}
		},
#endif
		[MLX5_RSS_PTYPE_ACTION_INDEX] = {
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = &ptype_rss_conf
		},
		[MLX5_RSS_PTYPE_ACTION_INDEX + 1] = { .type = RTE_FLOW_ACTION_TYPE_END }
	};

	ptype_rss_conf.types = rss_types;
	if (l4_item) {
		/*
		 * Original flow pattern extended up to L4 level.
		 * L4 is the maximal expansion level.
		 * Original pattern does not need expansion.
		 */
		expand = false;
	} else if (!l4_hash) {
		if (!l3_hash) {
			/*
			 * RSS action was not configured to hash L3 or L4.
			 * No expansion needed.
			 */
			expand = false;
		} else if (l3_item) {
			/*
			 * Original flow pattern extended up to L3 level.
			 * RSS action was not set for L4 hash.
			 */
			bool ip4_item =
				(outer_rss && (item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV4)) ||
				(inner_rss && (item_flags & MLX5_FLOW_LAYER_INNER_L3_IPV4));
			bool ip6_item =
				(outer_rss && (item_flags & MLX5_FLOW_LAYER_OUTER_L3_IPV6)) ||
				(inner_rss && (item_flags & MLX5_FLOW_LAYER_INNER_L3_IPV6));
			bool ip4_hash = rss_types & MLX5_IPV4_LAYER_TYPES;
			bool ip6_hash = rss_types & MLX5_IPV6_LAYER_TYPES;

			expand = false;
			if (ip4_item && ip4_hash) {
				ptype_rss_conf.types &= ~MLX5_IPV6_LAYER_TYPES;
				copy_actions = true;
			} else if (ip6_item && ip6_hash) {
				/*
				 * MLX5 HW will not activate TIR IPv6 hash
				 * if that TIR has also IPv4 hash
				 */
				ptype_rss_conf.types &= ~MLX5_IPV4_LAYER_TYPES;
				copy_actions = true;
			}
		}
	}
	if (!expand)
		return flow_nta_create_single(dev, attr, items, actions,
					      &ptype_rss_conf, item_flags,
					      action_flags, external,
					      copy_actions, flow_type, error);
	/* Create RSS expansions in dedicated PTYPE flow group */
	ptype_attr.group = mlx5_hw_get_rss_ptype_group(dev);
	if (!ptype_attr.group) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
				   NULL, "cannot get RSS PTYPE group");
		return NULL;
	}
	mlx5_nta_rss_init_ptype_ctx(&rss_ctx, dev, &ptype_attr, ptype_pattern,
				    ptype_actions, &ptype_rss_conf, &expansion_head,
				    error, item_flags, flow_type, external);
	rss_miss = mlx5_hw_rss_ptype_create_miss_flow(dev, &ptype_rss_conf, ptype_attr.group,
						      external, error);
	if (!rss_miss)
		goto error;
	if (l4_hash) {
		rss_next = mlx5_hw_rss_expand_l4(&rss_ctx);
		if (!rss_next)
			goto error;
	} else if (l3_hash) {
		rss_next = mlx5_hw_rss_expand_l3(&rss_ctx);
		if (!rss_next)
			goto error;
	}
	rss_base = mlx5_hw_rss_ptype_create_base_flow(dev, attr, items, actions,
						      ptype_attr.group, item_flags,
						      action_flags, external,
						      flow_type, error);
	if (!rss_base)
		goto error;
	SLIST_INSERT_HEAD(&expansion_head, rss_miss, nt2hws->next);
	SLIST_INSERT_HEAD(&expansion_head, rss_base, nt2hws->next);
	/**
	 * PMD must return to application a reference to the base flow.
	 * This way RSS expansion could work with counter, meter and other
	 * flow actions.
	 */
	MLX5_ASSERT(rss_base == SLIST_FIRST(&expansion_head));
	rss_next = SLIST_NEXT(rss_base, nt2hws->next);
	while (rss_next) {
		rss_next->nt2hws->chaned_flow = 1;
		rss_next = SLIST_NEXT(rss_next, nt2hws->next);
	}
	return SLIST_FIRST(&expansion_head);

error:
	if (rss_miss)
		flow_hw_list_destroy(dev, flow_type, (uintptr_t)rss_miss);
	if (rss_next)
		flow_hw_list_destroy(dev, flow_type, (uintptr_t)rss_next);
	mlx5_hw_release_rss_ptype_group(dev, ptype_attr.group);
	return NULL;
}

#endif
