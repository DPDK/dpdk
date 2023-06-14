/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>

#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_defs.h"
#include "mlx5_flow.h"
#include "mlx5_rx.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
#include "mlx5_hws_cnt.h"

/* The maximum actions support in the flow. */
#define MLX5_HW_MAX_ACTS 16

/*
 * The default ipool threshold value indicates which per_core_cache
 * value to set.
 */
#define MLX5_HW_IPOOL_SIZE_THRESHOLD (1 << 19)
/* The default min local cache size. */
#define MLX5_HW_IPOOL_CACHE_MIN (1 << 9)

/* Default push burst threshold. */
#define BURST_THR 32u

/* Default queue to flush the flows. */
#define MLX5_DEFAULT_FLUSH_QUEUE 0

/* Maximum number of rules in control flow tables. */
#define MLX5_HW_CTRL_FLOW_NB_RULES (4096)

/* Lowest flow group usable by an application if group translation is done. */
#define MLX5_HW_LOWEST_USABLE_GROUP (1)

/* Maximum group index usable by user applications for transfer flows. */
#define MLX5_HW_MAX_TRANSFER_GROUP (UINT32_MAX - 1)

/* Maximum group index usable by user applications for egress flows. */
#define MLX5_HW_MAX_EGRESS_GROUP (UINT32_MAX - 1)

/* Lowest priority for HW root table. */
#define MLX5_HW_LOWEST_PRIO_ROOT 15

/* Lowest priority for HW non-root table. */
#define MLX5_HW_LOWEST_PRIO_NON_ROOT (UINT32_MAX)

/* Priorities for Rx control flow rules. */
#define MLX5_HW_CTRL_RX_PRIO_L2 (MLX5_HW_LOWEST_PRIO_ROOT)
#define MLX5_HW_CTRL_RX_PRIO_L3 (MLX5_HW_LOWEST_PRIO_ROOT - 1)
#define MLX5_HW_CTRL_RX_PRIO_L4 (MLX5_HW_LOWEST_PRIO_ROOT - 2)

#define MLX5_HW_VLAN_PUSH_TYPE_IDX 0
#define MLX5_HW_VLAN_PUSH_VID_IDX 1
#define MLX5_HW_VLAN_PUSH_PCP_IDX 2

static int flow_hw_flush_all_ctrl_flows(struct rte_eth_dev *dev);
static int flow_hw_translate_group(struct rte_eth_dev *dev,
				   const struct mlx5_flow_template_table_cfg *cfg,
				   uint32_t group,
				   uint32_t *table_group,
				   struct rte_flow_error *error);
static __rte_always_inline int
flow_hw_set_vlan_vid_construct(struct rte_eth_dev *dev,
			       struct mlx5_hw_q_job *job,
			       struct mlx5_action_construct_data *act_data,
			       const struct mlx5_hw_actions *hw_acts,
			       const struct rte_flow_action *action);
static void
flow_hw_construct_quota(struct mlx5_priv *priv,
			struct mlx5dr_rule_action *rule_act, uint32_t qid);

static __rte_always_inline uint32_t flow_hw_tx_tag_regc_mask(struct rte_eth_dev *dev);
static __rte_always_inline uint32_t flow_hw_tx_tag_regc_value(struct rte_eth_dev *dev);

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops;

/* DR action flags with different table. */
static uint32_t mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_MAX]
				[MLX5DR_TABLE_TYPE_MAX] = {
	{
		MLX5DR_ACTION_FLAG_ROOT_RX,
		MLX5DR_ACTION_FLAG_ROOT_TX,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
	},
	{
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB,
	},
};

/* Ethernet item spec for promiscuous mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_promisc_spec = {
	.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};
/* Ethernet item mask for promiscuous mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_promisc_mask = {
	.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};

/* Ethernet item spec for all multicast mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_mcast_spec = {
	.hdr.dst_addr.addr_bytes = "\x01\x00\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};
/* Ethernet item mask for all multicast mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_mcast_mask = {
	.hdr.dst_addr.addr_bytes = "\x01\x00\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};

/* Ethernet item spec for IPv4 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv4_mcast_spec = {
	.hdr.dst_addr.addr_bytes = "\x01\x00\x5e\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};
/* Ethernet item mask for IPv4 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv4_mcast_mask = {
	.hdr.dst_addr.addr_bytes = "\xff\xff\xff\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};

/* Ethernet item spec for IPv6 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv6_mcast_spec = {
	.hdr.dst_addr.addr_bytes = "\x33\x33\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};
/* Ethernet item mask for IPv6 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv6_mcast_mask = {
	.hdr.dst_addr.addr_bytes = "\xff\xff\x00\x00\x00\x00",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};

/* Ethernet item mask for unicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_dmac_mask = {
	.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};

/* Ethernet item spec for broadcast. */
static const struct rte_flow_item_eth ctrl_rx_eth_bcast_spec = {
	.hdr.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
	.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	.hdr.ether_type = 0,
};
static inline enum mlx5dr_matcher_insert_mode
flow_hw_matcher_insert_mode_get(enum rte_flow_table_insertion_type insert_type)
{
	if (insert_type == RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN)
		return MLX5DR_MATCHER_INSERT_BY_HASH;
	else
		return MLX5DR_MATCHER_INSERT_BY_INDEX;
}

static inline enum mlx5dr_matcher_distribute_mode
flow_hw_matcher_distribute_mode_get(enum rte_flow_table_hash_func hash_func)
{
	if (hash_func == RTE_FLOW_TABLE_HASH_FUNC_LINEAR)
		return MLX5DR_MATCHER_DISTRIBUTE_BY_LINEAR;
	else
		return MLX5DR_MATCHER_DISTRIBUTE_BY_HASH;
}

/**
 * Set the hash fields according to the @p rss_desc information.
 *
 * @param[in] rss_desc
 *   Pointer to the mlx5_flow_rss_desc.
 * @param[out] hash_fields
 *   Pointer to the RSS hash fields.
 */
static void
flow_hw_hashfields_set(struct mlx5_flow_rss_desc *rss_desc,
		       uint64_t *hash_fields)
{
	uint64_t fields = 0;
	int rss_inner = 0;
	uint64_t rss_types = rte_eth_rss_hf_refine(rss_desc->types);

#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
	if (rss_desc->level >= 2)
		rss_inner = 1;
#endif
	if (rss_types & MLX5_IPV4_LAYER_TYPES) {
		if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
			fields |= IBV_RX_HASH_SRC_IPV4;
		else if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
			fields |= IBV_RX_HASH_DST_IPV4;
		else
			fields |= MLX5_IPV4_IBV_RX_HASH;
	} else if (rss_types & MLX5_IPV6_LAYER_TYPES) {
		if (rss_types & RTE_ETH_RSS_L3_SRC_ONLY)
			fields |= IBV_RX_HASH_SRC_IPV6;
		else if (rss_types & RTE_ETH_RSS_L3_DST_ONLY)
			fields |= IBV_RX_HASH_DST_IPV6;
		else
			fields |= MLX5_IPV6_IBV_RX_HASH;
	}
	if (rss_types & RTE_ETH_RSS_UDP) {
		if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
			fields |= IBV_RX_HASH_SRC_PORT_UDP;
		else if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
			fields |= IBV_RX_HASH_DST_PORT_UDP;
		else
			fields |= MLX5_UDP_IBV_RX_HASH;
	} else if (rss_types & RTE_ETH_RSS_TCP) {
		if (rss_types & RTE_ETH_RSS_L4_SRC_ONLY)
			fields |= IBV_RX_HASH_SRC_PORT_TCP;
		else if (rss_types & RTE_ETH_RSS_L4_DST_ONLY)
			fields |= IBV_RX_HASH_DST_PORT_TCP;
		else
			fields |= MLX5_TCP_IBV_RX_HASH;
	}
	if (rss_types & RTE_ETH_RSS_ESP)
		fields |= IBV_RX_HASH_IPSEC_SPI;
	if (rss_inner)
		fields |= IBV_RX_HASH_INNER;
	*hash_fields = fields;
}

/**
 * Generate the matching pattern item flags.
 *
 * @param[in] items
 *   Pointer to the list of items.
 *
 * @return
 *   Matching item flags. RSS hash field function
 *   silently ignores the flags which are unsupported.
 */
static uint64_t
flow_hw_matching_item_flags_get(const struct rte_flow_item items[])
{
	uint64_t item_flags = 0;
	uint64_t last_item = 0;

	for (; items->type != RTE_FLOW_ITEM_TYPE_END; items++) {
		int tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
		int item_type = items->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
					     MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
					     MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
					     MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
			last_item = tunnel ? MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT :
					     MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			last_item = MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			last_item = MLX5_FLOW_LAYER_GTP;
			break;
		default:
			break;
		}
		item_flags |= last_item;
	}
	return item_flags;
}

/**
 * Register destination table DR jump action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] table_attr
 *   Pointer to the flow attributes.
 * @param[in] dest_group
 *   The destination group ID.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Table on success, NULL otherwise and rte_errno is set.
 */
static struct mlx5_hw_jump_action *
flow_hw_jump_action_register(struct rte_eth_dev *dev,
			     const struct mlx5_flow_template_table_cfg *cfg,
			     uint32_t dest_group,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_attr jattr = cfg->attr.flow_attr;
	struct mlx5_flow_group *grp;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &jattr,
	};
	struct mlx5_list_entry *ge;
	uint32_t target_group;

	target_group = dest_group;
	if (flow_hw_translate_group(dev, cfg, dest_group, &target_group, error))
		return NULL;
	jattr.group = target_group;
	ge = mlx5_hlist_register(priv->sh->flow_tbls, target_group, &ctx);
	if (!ge)
		return NULL;
	grp = container_of(ge, struct mlx5_flow_group, entry);
	return &grp->jump;
}

/**
 * Release jump action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] jump
 *   Pointer to the jump action.
 */

static void
flow_hw_jump_release(struct rte_eth_dev *dev, struct mlx5_hw_jump_action *jump)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_group *grp;

	grp = container_of
		(jump, struct mlx5_flow_group, jump);
	mlx5_hlist_unregister(priv->sh->flow_tbls, &grp->entry);
}

/**
 * Register queue/RSS action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] hws_flags
 *   DR action flags.
 * @param[in] action
 *   rte flow action.
 *
 * @return
 *    Table on success, NULL otherwise and rte_errno is set.
 */
static inline struct mlx5_hrxq*
flow_hw_tir_action_register(struct rte_eth_dev *dev,
			    uint32_t hws_flags,
			    const struct rte_flow_action *action)
{
	struct mlx5_flow_rss_desc rss_desc = {
		.hws_flags = hws_flags,
	};
	struct mlx5_hrxq *hrxq;

	if (action->type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		const struct rte_flow_action_queue *queue = action->conf;

		rss_desc.const_q = &queue->index;
		rss_desc.queue_num = 1;
	} else {
		const struct rte_flow_action_rss *rss = action->conf;

		rss_desc.queue_num = rss->queue_num;
		rss_desc.const_q = rss->queue;
		memcpy(rss_desc.key,
		       !rss->key ? rss_hash_default_key : rss->key,
		       MLX5_RSS_HASH_KEY_LEN);
		rss_desc.key_len = MLX5_RSS_HASH_KEY_LEN;
		rss_desc.types = !rss->types ? RTE_ETH_RSS_IP : rss->types;
		flow_hw_hashfields_set(&rss_desc, &rss_desc.hash_fields);
		flow_dv_action_rss_l34_hash_adjust(rss->types,
						   &rss_desc.hash_fields);
		if (rss->level > 1) {
			rss_desc.hash_fields |= IBV_RX_HASH_INNER;
			rss_desc.tunnel = 1;
		}
	}
	hrxq = mlx5_hrxq_get(dev, &rss_desc);
	return hrxq;
}

static __rte_always_inline int
flow_hw_ct_compile(struct rte_eth_dev *dev,
		   uint32_t queue, uint32_t idx,
		   struct mlx5dr_rule_action *rule_act)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_action *ct;

	ct = mlx5_ipool_get(priv->hws_ctpool->cts, MLX5_ACTION_CTX_CT_GET_IDX(idx));
	if (!ct || mlx5_aso_ct_available(priv->sh, queue, ct))
		return -1;
	rule_act->action = priv->hws_ctpool->dr_action;
	rule_act->aso_ct.offset = ct->offset;
	rule_act->aso_ct.direction = ct->is_original ?
		MLX5DR_ACTION_ASO_CT_DIRECTION_INITIATOR :
		MLX5DR_ACTION_ASO_CT_DIRECTION_RESPONDER;
	return 0;
}

/**
 * Destroy DR actions created by action template.
 *
 * For DR actions created during table creation's action translate.
 * Need to destroy the DR action when destroying the table.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 */
static void
__flow_hw_action_template_destroy(struct rte_eth_dev *dev,
				 struct mlx5_hw_actions *acts)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_action_construct_data *data;

	while (!LIST_EMPTY(&acts->act_list)) {
		data = LIST_FIRST(&acts->act_list);
		LIST_REMOVE(data, next);
		mlx5_ipool_free(priv->acts_ipool, data->idx);
	}

	if (acts->mark)
		if (!(__atomic_fetch_sub(&priv->hws_mark_refcnt, 1, __ATOMIC_RELAXED) - 1))
			flow_hw_rxq_flag_set(dev, false);

	if (acts->jump) {
		struct mlx5_flow_group *grp;

		grp = container_of
			(acts->jump, struct mlx5_flow_group, jump);
		mlx5_hlist_unregister(priv->sh->flow_tbls, &grp->entry);
		acts->jump = NULL;
	}
	if (acts->tir) {
		mlx5_hrxq_release(dev, acts->tir->idx);
		acts->tir = NULL;
	}
	if (acts->encap_decap) {
		if (acts->encap_decap->action)
			mlx5dr_action_destroy(acts->encap_decap->action);
		mlx5_free(acts->encap_decap);
		acts->encap_decap = NULL;
	}
	if (acts->mhdr) {
		if (acts->mhdr->action)
			mlx5dr_action_destroy(acts->mhdr->action);
		mlx5_free(acts->mhdr);
		acts->mhdr = NULL;
	}
	if (mlx5_hws_cnt_id_valid(acts->cnt_id)) {
		mlx5_hws_cnt_shared_put(priv->hws_cpool, &acts->cnt_id);
		acts->cnt_id = 0;
	}
	if (acts->mtr_id) {
		mlx5_ipool_free(priv->hws_mpool->idx_pool, acts->mtr_id);
		acts->mtr_id = 0;
	}
}

/**
 * Append dynamic action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline struct mlx5_action_construct_data *
__flow_hw_act_data_alloc(struct mlx5_priv *priv,
			 enum rte_flow_action_type type,
			 uint16_t action_src,
			 uint16_t action_dst)
{
	struct mlx5_action_construct_data *act_data;
	uint32_t idx = 0;

	act_data = mlx5_ipool_zmalloc(priv->acts_ipool, &idx);
	if (!act_data)
		return NULL;
	act_data->idx = idx;
	act_data->type = type;
	act_data->action_src = action_src;
	act_data->action_dst = action_dst;
	return act_data;
}

/**
 * Append dynamic action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
__flow_hw_act_data_general_append(struct mlx5_priv *priv,
				  struct mlx5_hw_actions *acts,
				  enum rte_flow_action_type type,
				  uint16_t action_src,
				  uint16_t action_dst)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

/**
 * Append dynamic encap action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 * @param[in] len
 *   Length of the data to be updated.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
__flow_hw_act_data_encap_append(struct mlx5_priv *priv,
				struct mlx5_hw_actions *acts,
				enum rte_flow_action_type type,
				uint16_t action_src,
				uint16_t action_dst,
				uint16_t len)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->encap.len = len;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

static __rte_always_inline int
__flow_hw_act_data_hdr_modify_append(struct mlx5_priv *priv,
				     struct mlx5_hw_actions *acts,
				     enum rte_flow_action_type type,
				     uint16_t action_src,
				     uint16_t action_dst,
				     uint16_t mhdr_cmds_off,
				     uint16_t mhdr_cmds_end,
				     bool shared,
				     struct field_modify_info *field,
				     struct field_modify_info *dcopy,
				     uint32_t *mask)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->modify_header.mhdr_cmds_off = mhdr_cmds_off;
	act_data->modify_header.mhdr_cmds_end = mhdr_cmds_end;
	act_data->modify_header.shared = shared;
	rte_memcpy(act_data->modify_header.field, field,
		   sizeof(*field) * MLX5_ACT_MAX_MOD_FIELDS);
	rte_memcpy(act_data->modify_header.dcopy, dcopy,
		   sizeof(*dcopy) * MLX5_ACT_MAX_MOD_FIELDS);
	rte_memcpy(act_data->modify_header.mask, mask,
		   sizeof(*mask) * MLX5_ACT_MAX_MOD_FIELDS);
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

/**
 * Append shared RSS action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 * @param[in] idx
 *   Shared RSS index.
 * @param[in] rss
 *   Pointer to the shared RSS info.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
__flow_hw_act_data_shared_rss_append(struct mlx5_priv *priv,
				     struct mlx5_hw_actions *acts,
				     enum rte_flow_action_type type,
				     uint16_t action_src,
				     uint16_t action_dst,
				     uint32_t idx,
				     struct mlx5_shared_action_rss *rss)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->shared_rss.level = rss->origin.level;
	act_data->shared_rss.types = !rss->origin.types ? RTE_ETH_RSS_IP :
				     rss->origin.types;
	act_data->shared_rss.idx = idx;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

/**
 * Append shared counter action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 * @param[in] cnt_id
 *   Shared counter id.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
__flow_hw_act_data_shared_cnt_append(struct mlx5_priv *priv,
				     struct mlx5_hw_actions *acts,
				     enum rte_flow_action_type type,
				     uint16_t action_src,
				     uint16_t action_dst,
				     cnt_id_t cnt_id)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->type = type;
	act_data->shared_counter.id = cnt_id;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

/**
 * Append shared meter_mark action to the dynamic action list.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] type
 *   Action type.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 * @param[in] mtr_id
 *   Shared meter id.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
__flow_hw_act_data_shared_mtr_append(struct mlx5_priv *priv,
				     struct mlx5_hw_actions *acts,
				     enum rte_flow_action_type type,
				     uint16_t action_src,
				     uint16_t action_dst,
				     cnt_id_t mtr_id)
{	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->type = type;
	act_data->shared_meter.id = mtr_id;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

/**
 * Translate shared indirect action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev data structure.
 * @param[in] action
 *   Pointer to the shared indirect rte_flow action.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] action_src
 *   Offset of source rte flow action.
 * @param[in] action_dst
 *   Offset of destination DR action.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
flow_hw_shared_action_translate(struct rte_eth_dev *dev,
				const struct rte_flow_action *action,
				struct mlx5_hw_actions *acts,
				uint16_t action_src,
				uint16_t action_dst)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_shared_action_rss *shared_rss;
	uint32_t act_idx = (uint32_t)(uintptr_t)action->conf;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx &
		       ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		shared_rss = mlx5_ipool_get
		  (priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
		if (!shared_rss || __flow_hw_act_data_shared_rss_append
		    (priv, acts,
		    (enum rte_flow_action_type)MLX5_RTE_FLOW_ACTION_TYPE_RSS,
		    action_src, action_dst, idx, shared_rss))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		if (__flow_hw_act_data_shared_cnt_append(priv, acts,
			(enum rte_flow_action_type)
			MLX5_RTE_FLOW_ACTION_TYPE_COUNT,
			action_src, action_dst, act_idx))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		/* Not supported, prevent by validate function. */
		MLX5_ASSERT(0);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		if (flow_hw_ct_compile(dev, MLX5_HW_INV_QUEUE,
				       idx, &acts->rule_acts[action_dst]))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		if (__flow_hw_act_data_shared_mtr_append(priv, acts,
			(enum rte_flow_action_type)
			MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK,
			action_src, action_dst, idx))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		flow_hw_construct_quota(priv, &acts->rule_acts[action_dst], idx);
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type:%d", type);
		break;
	}
	return 0;
}

static __rte_always_inline bool
flow_hw_action_modify_field_is_shared(const struct rte_flow_action *action,
				      const struct rte_flow_action *mask)
{
	const struct rte_flow_action_modify_field *v = action->conf;
	const struct rte_flow_action_modify_field *m = mask->conf;

	if (v->src.field == RTE_FLOW_FIELD_VALUE) {
		uint32_t j;

		if (m == NULL)
			return false;
		for (j = 0; j < RTE_DIM(m->src.value); ++j) {
			/*
			 * Immediate value is considered to be masked
			 * (and thus shared by all flow rules), if mask
			 * is non-zero. Partial mask over immediate value
			 * is not allowed.
			 */
			if (m->src.value[j])
				return true;
		}
		return false;
	}
	if (v->src.field == RTE_FLOW_FIELD_POINTER)
		return m->src.pvalue != NULL;
	/*
	 * Source field types other than VALUE and
	 * POINTER are always shared.
	 */
	return true;
}

static __rte_always_inline bool
flow_hw_should_insert_nop(const struct mlx5_hw_modify_header_action *mhdr,
			  const struct mlx5_modification_cmd *cmd)
{
	struct mlx5_modification_cmd last_cmd = { { 0 } };
	struct mlx5_modification_cmd new_cmd = { { 0 } };
	const uint32_t cmds_num = mhdr->mhdr_cmds_num;
	unsigned int last_type;
	bool should_insert = false;

	if (cmds_num == 0)
		return false;
	last_cmd = *(&mhdr->mhdr_cmds[cmds_num - 1]);
	last_cmd.data0 = rte_be_to_cpu_32(last_cmd.data0);
	last_cmd.data1 = rte_be_to_cpu_32(last_cmd.data1);
	last_type = last_cmd.action_type;
	new_cmd = *cmd;
	new_cmd.data0 = rte_be_to_cpu_32(new_cmd.data0);
	new_cmd.data1 = rte_be_to_cpu_32(new_cmd.data1);
	switch (new_cmd.action_type) {
	case MLX5_MODIFICATION_TYPE_SET:
	case MLX5_MODIFICATION_TYPE_ADD:
		if (last_type == MLX5_MODIFICATION_TYPE_SET ||
		    last_type == MLX5_MODIFICATION_TYPE_ADD)
			should_insert = new_cmd.field == last_cmd.field;
		else if (last_type == MLX5_MODIFICATION_TYPE_COPY)
			should_insert = new_cmd.field == last_cmd.dst_field;
		else if (last_type == MLX5_MODIFICATION_TYPE_NOP)
			should_insert = false;
		else
			MLX5_ASSERT(false); /* Other types are not supported. */
		break;
	case MLX5_MODIFICATION_TYPE_COPY:
		if (last_type == MLX5_MODIFICATION_TYPE_SET ||
		    last_type == MLX5_MODIFICATION_TYPE_ADD)
			should_insert = (new_cmd.field == last_cmd.field ||
					 new_cmd.dst_field == last_cmd.field);
		else if (last_type == MLX5_MODIFICATION_TYPE_COPY)
			should_insert = (new_cmd.field == last_cmd.dst_field ||
					 new_cmd.dst_field == last_cmd.dst_field);
		else if (last_type == MLX5_MODIFICATION_TYPE_NOP)
			should_insert = false;
		else
			MLX5_ASSERT(false); /* Other types are not supported. */
		break;
	default:
		/* Other action types should be rejected on AT validation. */
		MLX5_ASSERT(false);
		break;
	}
	return should_insert;
}

static __rte_always_inline int
flow_hw_mhdr_cmd_nop_append(struct mlx5_hw_modify_header_action *mhdr)
{
	struct mlx5_modification_cmd *nop;
	uint32_t num = mhdr->mhdr_cmds_num;

	if (num + 1 >= MLX5_MHDR_MAX_CMD)
		return -ENOMEM;
	nop = mhdr->mhdr_cmds + num;
	nop->data0 = 0;
	nop->action_type = MLX5_MODIFICATION_TYPE_NOP;
	nop->data0 = rte_cpu_to_be_32(nop->data0);
	nop->data1 = 0;
	mhdr->mhdr_cmds_num = num + 1;
	return 0;
}

static __rte_always_inline int
flow_hw_mhdr_cmd_append(struct mlx5_hw_modify_header_action *mhdr,
			struct mlx5_modification_cmd *cmd)
{
	uint32_t num = mhdr->mhdr_cmds_num;

	if (num + 1 >= MLX5_MHDR_MAX_CMD)
		return -ENOMEM;
	mhdr->mhdr_cmds[num] = *cmd;
	mhdr->mhdr_cmds_num = num + 1;
	return 0;
}

static __rte_always_inline int
flow_hw_converted_mhdr_cmds_append(struct mlx5_hw_modify_header_action *mhdr,
				   struct mlx5_flow_dv_modify_hdr_resource *resource)
{
	uint32_t idx;
	int ret;

	for (idx = 0; idx < resource->actions_num; ++idx) {
		struct mlx5_modification_cmd *src = &resource->actions[idx];

		if (flow_hw_should_insert_nop(mhdr, src)) {
			ret = flow_hw_mhdr_cmd_nop_append(mhdr);
			if (ret)
				return ret;
		}
		ret = flow_hw_mhdr_cmd_append(mhdr, src);
		if (ret)
			return ret;
	}
	return 0;
}

static __rte_always_inline void
flow_hw_modify_field_init(struct mlx5_hw_modify_header_action *mhdr,
			  struct rte_flow_actions_template *at)
{
	memset(mhdr, 0, sizeof(*mhdr));
	/* Modify header action without any commands is shared by default. */
	mhdr->shared = true;
	mhdr->pos = at->mhdr_off;
}

static __rte_always_inline int
flow_hw_modify_field_compile(struct rte_eth_dev *dev,
			     const struct rte_flow_attr *attr,
			     const struct rte_flow_action *action_start, /* Start of AT actions. */
			     const struct rte_flow_action *action, /* Current action from AT. */
			     const struct rte_flow_action *action_mask, /* Current mask from AT. */
			     struct mlx5_hw_actions *acts,
			     struct mlx5_hw_modify_header_action *mhdr,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_modify_field *conf = action->conf;
	union {
		struct mlx5_flow_dv_modify_hdr_resource resource;
		uint8_t data[sizeof(struct mlx5_flow_dv_modify_hdr_resource) +
			     sizeof(struct mlx5_modification_cmd) * MLX5_MHDR_MAX_CMD];
	} dummy;
	struct mlx5_flow_dv_modify_hdr_resource *resource;
	struct rte_flow_item item = {
		.spec = NULL,
		.mask = NULL
	};
	struct field_modify_info field[MLX5_ACT_MAX_MOD_FIELDS] = {
						{0, 0, MLX5_MODI_OUT_NONE} };
	struct field_modify_info dcopy[MLX5_ACT_MAX_MOD_FIELDS] = {
						{0, 0, MLX5_MODI_OUT_NONE} };
	uint32_t mask[MLX5_ACT_MAX_MOD_FIELDS] = { 0 };
	uint32_t type, value = 0;
	uint16_t cmds_start, cmds_end;
	bool shared;
	int ret;

	/*
	 * Modify header action is shared if previous modify_field actions
	 * are shared and currently compiled action is shared.
	 */
	shared = flow_hw_action_modify_field_is_shared(action, action_mask);
	mhdr->shared &= shared;
	if (conf->src.field == RTE_FLOW_FIELD_POINTER ||
	    conf->src.field == RTE_FLOW_FIELD_VALUE) {
		type = conf->operation == RTE_FLOW_MODIFY_SET ? MLX5_MODIFICATION_TYPE_SET :
								MLX5_MODIFICATION_TYPE_ADD;
		/* For SET/ADD fill the destination field (field) first. */
		mlx5_flow_field_id_to_modify_info(&conf->dst, field, mask,
						  conf->width, dev,
						  attr, error);
		item.spec = conf->src.field == RTE_FLOW_FIELD_POINTER ?
				(void *)(uintptr_t)conf->src.pvalue :
				(void *)(uintptr_t)&conf->src.value;
		if (conf->dst.field == RTE_FLOW_FIELD_META ||
		    conf->dst.field == RTE_FLOW_FIELD_TAG ||
		    conf->dst.field == RTE_FLOW_FIELD_METER_COLOR ||
		    conf->dst.field == (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG) {
			uint8_t tag_index = flow_tag_index_get(&conf->dst);

			value = *(const unaligned_uint32_t *)item.spec;
			if (conf->dst.field == RTE_FLOW_FIELD_TAG &&
			    tag_index == MLX5_LINEAR_HASH_TAG_INDEX)
				value = rte_cpu_to_be_32(value << 16);
			else
				value = rte_cpu_to_be_32(value);
			item.spec = &value;
		} else if (conf->dst.field == RTE_FLOW_FIELD_GTP_PSC_QFI) {
			/*
			 * QFI is passed as an uint8_t integer, but it is accessed through
			 * a 2nd least significant byte of a 32-bit field in modify header command.
			 */
			value = *(const uint8_t *)item.spec;
			value = rte_cpu_to_be_32(value << 8);
			item.spec = &value;
		}
	} else {
		type = MLX5_MODIFICATION_TYPE_COPY;
		/* For COPY fill the destination field (dcopy) without mask. */
		mlx5_flow_field_id_to_modify_info(&conf->dst, dcopy, NULL,
						  conf->width, dev,
						  attr, error);
		/* Then construct the source field (field) with mask. */
		mlx5_flow_field_id_to_modify_info(&conf->src, field, mask,
						  conf->width, dev,
						  attr, error);
	}
	item.mask = &mask;
	memset(&dummy, 0, sizeof(dummy));
	resource = &dummy.resource;
	ret = flow_dv_convert_modify_action(&item, field, dcopy, resource, type, error);
	if (ret)
		return ret;
	MLX5_ASSERT(resource->actions_num > 0);
	/*
	 * If previous modify field action collide with this one, then insert NOP command.
	 * This NOP command will not be a part of action's command range used to update commands
	 * on rule creation.
	 */
	if (flow_hw_should_insert_nop(mhdr, &resource->actions[0])) {
		ret = flow_hw_mhdr_cmd_nop_append(mhdr);
		if (ret)
			return rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "too many modify field operations specified");
	}
	cmds_start = mhdr->mhdr_cmds_num;
	ret = flow_hw_converted_mhdr_cmds_append(mhdr, resource);
	if (ret)
		return rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "too many modify field operations specified");

	cmds_end = mhdr->mhdr_cmds_num;
	if (shared)
		return 0;
	ret = __flow_hw_act_data_hdr_modify_append(priv, acts, RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
						   action - action_start, mhdr->pos,
						   cmds_start, cmds_end, shared,
						   field, dcopy, mask);
	if (ret)
		return rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "not enough memory to store modify field metadata");
	return 0;
}

static uint32_t
flow_hw_count_nop_modify_field(struct mlx5_hw_modify_header_action *mhdr)
{
	uint32_t i;
	uint32_t nops = 0;

	for (i = 0; i < mhdr->mhdr_cmds_num; ++i) {
		struct mlx5_modification_cmd cmd = mhdr->mhdr_cmds[i];

		cmd.data0 = rte_be_to_cpu_32(cmd.data0);
		if (cmd.action_type == MLX5_MODIFICATION_TYPE_NOP)
			++nops;
	}
	return nops;
}

static int
flow_hw_validate_compiled_modify_field(struct rte_eth_dev *dev,
				       const struct mlx5_flow_template_table_cfg *cfg,
				       struct mlx5_hw_modify_header_action *mhdr,
				       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *hca_attr = &priv->sh->cdev->config.hca_attr;

	/*
	 * Header modify pattern length limitation is only valid for HWS groups, i.e. groups > 0.
	 * In group 0, MODIFY_FIELD actions are handled with header modify actions
	 * managed by rdma-core.
	 */
	if (cfg->attr.flow_attr.group != 0 &&
	    mhdr->mhdr_cmds_num > hca_attr->max_header_modify_pattern_length) {
		uint32_t nops = flow_hw_count_nop_modify_field(mhdr);

		DRV_LOG(ERR, "Too many modify header commands generated from "
			     "MODIFY_FIELD actions. "
			     "Generated HW commands = %u (amount of NOP commands = %u). "
			     "Maximum supported = %u.",
			     mhdr->mhdr_cmds_num, nops,
			     hca_attr->max_header_modify_pattern_length);
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "Number of MODIFY_FIELD actions exceeds maximum "
					  "supported limit of actions");
	}
	return 0;
}

static int
flow_hw_represented_port_compile(struct rte_eth_dev *dev,
				 const struct rte_flow_attr *attr,
				 const struct rte_flow_action *action_start,
				 const struct rte_flow_action *action,
				 const struct rte_flow_action *action_mask,
				 struct mlx5_hw_actions *acts,
				 uint16_t action_dst,
				 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_ethdev *v = action->conf;
	const struct rte_flow_action_ethdev *m = action_mask->conf;
	int ret;

	if (!attr->group)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "represented_port action cannot"
					  " be used on group 0");
	if (!attr->transfer)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER,
					  NULL,
					  "represented_port action requires"
					  " transfer attribute");
	if (attr->ingress || attr->egress)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "represented_port action cannot"
					  " be used with direction attributes");
	if (!priv->master)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "represented_port acton must"
					  " be used on proxy port");
	if (m && !!m->port_id) {
		struct mlx5_priv *port_priv;

		if (!v)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						  action, "port index was not provided");
		port_priv = mlx5_port_to_eswitch_info(v->port_id, false);
		if (port_priv == NULL)
			return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "port does not exist or unable to"
					 " obtain E-Switch info for port");
		MLX5_ASSERT(priv->hw_vport != NULL);
		if (priv->hw_vport[v->port_id]) {
			acts->rule_acts[action_dst].action =
					priv->hw_vport[v->port_id];
		} else {
			return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "cannot use represented_port action"
					 " with this port");
		}
	} else {
		ret = __flow_hw_act_data_general_append
				(priv, acts, action->type,
				 action - action_start, action_dst);
		if (ret)
			return rte_flow_error_set
					(error, ENOMEM,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					 "not enough memory to store"
					 " vport action");
	}
	return 0;
}

static __rte_always_inline int
flow_hw_meter_compile(struct rte_eth_dev *dev,
		      const struct mlx5_flow_template_table_cfg *cfg,
		      uint16_t aso_mtr_pos,
		      uint16_t jump_pos,
		      const struct rte_flow_action *action,
		      struct mlx5_hw_actions *acts,
		      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr *aso_mtr;
	const struct rte_flow_action_meter *meter = action->conf;
	uint32_t group = cfg->attr.flow_attr.group;

	aso_mtr = mlx5_aso_meter_by_idx(priv, meter->mtr_id);
	acts->rule_acts[aso_mtr_pos].action = priv->mtr_bulk.action;
	acts->rule_acts[aso_mtr_pos].aso_meter.offset = aso_mtr->offset;
	acts->jump = flow_hw_jump_action_register
		(dev, cfg, aso_mtr->fm.group, error);
	if (!acts->jump)
		return -ENOMEM;
	acts->rule_acts[jump_pos].action = (!!group) ?
				    acts->jump->hws_action :
				    acts->jump->root_action;
	if (mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr))
		return -ENOMEM;
	return 0;
}

static __rte_always_inline int
flow_hw_cnt_compile(struct rte_eth_dev *dev, uint32_t  start_pos,
		      struct mlx5_hw_actions *acts)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t pos = start_pos;
	cnt_id_t cnt_id;
	int ret;

	ret = mlx5_hws_cnt_shared_get(priv->hws_cpool, &cnt_id, 0);
	if (ret != 0)
		return ret;
	ret = mlx5_hws_cnt_pool_get_action_offset
				(priv->hws_cpool,
				 cnt_id,
				 &acts->rule_acts[pos].action,
				 &acts->rule_acts[pos].counter.offset);
	if (ret != 0)
		return ret;
	acts->cnt_id = cnt_id;
	return 0;
}

static __rte_always_inline bool
is_of_vlan_pcp_present(const struct rte_flow_action *actions)
{
	/*
	 * Order of RTE VLAN push actions is
	 * OF_PUSH_VLAN / OF_SET_VLAN_VID [ / OF_SET_VLAN_PCP ]
	 */
	return actions[MLX5_HW_VLAN_PUSH_PCP_IDX].type ==
		RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP;
}

static __rte_always_inline bool
is_template_masked_push_vlan(const struct rte_flow_action_of_push_vlan *mask)
{
	/*
	 * In masked push VLAN template all RTE push actions are masked.
	 */
	return mask && mask->ethertype != 0;
}

static rte_be32_t vlan_hdr_to_be32(const struct rte_flow_action *actions)
{
/*
 * OpenFlow Switch Specification defines 801.1q VID as 12+1 bits.
 */
	rte_be32_t type, vid, pcp;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	rte_be32_t vid_lo, vid_hi;
#endif

	type = ((const struct rte_flow_action_of_push_vlan *)
		actions[MLX5_HW_VLAN_PUSH_TYPE_IDX].conf)->ethertype;
	vid = ((const struct rte_flow_action_of_set_vlan_vid *)
		actions[MLX5_HW_VLAN_PUSH_VID_IDX].conf)->vlan_vid;
	pcp = is_of_vlan_pcp_present(actions) ?
	      ((const struct rte_flow_action_of_set_vlan_pcp *)
		      actions[MLX5_HW_VLAN_PUSH_PCP_IDX].conf)->vlan_pcp : 0;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	vid_hi = vid & 0xff;
	vid_lo = vid >> 8;
	return (((vid_lo << 8) | (pcp << 5) | vid_hi) << 16) | type;
#else
	return (type << 16) | (pcp << 13) | vid;
#endif
}

static __rte_always_inline struct mlx5_aso_mtr *
flow_hw_meter_mark_alloc(struct rte_eth_dev *dev, uint32_t queue,
			 const struct rte_flow_action *action,
			 void *user_data, bool push)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	const struct rte_flow_action_meter_mark *meter_mark = action->conf;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_flow_meter_info *fm;
	uint32_t mtr_id;

	aso_mtr = mlx5_ipool_malloc(priv->hws_mpool->idx_pool, &mtr_id);
	if (!aso_mtr)
		return NULL;
	/* Fill the flow meter parameters. */
	aso_mtr->type = ASO_METER_INDIRECT;
	fm = &aso_mtr->fm;
	fm->meter_id = mtr_id;
	fm->profile = (struct mlx5_flow_meter_profile *)(meter_mark->profile);
	fm->is_enable = meter_mark->state;
	fm->color_aware = meter_mark->color_mode;
	aso_mtr->pool = pool;
	aso_mtr->state = (queue == MLX5_HW_INV_QUEUE) ?
			  ASO_METER_WAIT : ASO_METER_WAIT_ASYNC;
	aso_mtr->offset = mtr_id - 1;
	aso_mtr->init_color = (meter_mark->color_mode) ?
		meter_mark->init_color : RTE_COLOR_GREEN;
	/* Update ASO flow meter by wqe. */
	if (mlx5_aso_meter_update_by_wqe(priv->sh, queue, aso_mtr,
					 &priv->mtr_bulk, user_data, push)) {
		mlx5_ipool_free(pool->idx_pool, mtr_id);
		return NULL;
	}
	/* Wait for ASO object completion. */
	if (queue == MLX5_HW_INV_QUEUE &&
	    mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr)) {
		mlx5_ipool_free(pool->idx_pool, mtr_id);
		return NULL;
	}
	return aso_mtr;
}

static __rte_always_inline int
flow_hw_meter_mark_compile(struct rte_eth_dev *dev,
			   uint16_t aso_mtr_pos,
			   const struct rte_flow_action *action,
			   struct mlx5dr_rule_action *acts,
			   uint32_t *index,
			   uint32_t queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_aso_mtr *aso_mtr;

	aso_mtr = flow_hw_meter_mark_alloc(dev, queue, action, NULL, true);
	if (!aso_mtr)
		return -1;

	/* Compile METER_MARK action */
	acts[aso_mtr_pos].action = pool->action;
	acts[aso_mtr_pos].aso_meter.offset = aso_mtr->offset;
	acts[aso_mtr_pos].aso_meter.init_color =
		(enum mlx5dr_action_aso_meter_color)
		rte_col_2_mlx5_col(aso_mtr->init_color);
	*index = aso_mtr->fm.meter_id;
	return 0;
}

/**
 * Translate rte_flow actions to DR action.
 *
 * As the action template has already indicated the actions. Translate
 * the rte_flow actions to DR action if possbile. So in flow create
 * stage we will save cycles from handing the actions' organizing.
 * For the actions with limited information, need to add these to a
 * list.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] cfg
 *   Pointer to the table configuration.
 * @param[in/out] acts
 *   Pointer to the template HW steering DR actions.
 * @param[in] at
 *   Action template.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
__flow_hw_actions_translate(struct rte_eth_dev *dev,
			    const struct mlx5_flow_template_table_cfg *cfg,
			    struct mlx5_hw_actions *acts,
			    struct rte_flow_actions_template *at,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_template_table_attr *table_attr = &cfg->attr;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	struct rte_flow_action *actions = at->actions;
	struct rte_flow_action *action_start = actions;
	struct rte_flow_action *masks = at->masks;
	enum mlx5dr_action_reformat_type refmt_type = 0;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	const struct rte_flow_item *enc_item = NULL, *enc_item_m = NULL;
	uint16_t reformat_src = 0;
	uint8_t *encap_data = NULL, *encap_data_m = NULL;
	size_t data_size = 0;
	struct mlx5_hw_modify_header_action mhdr = { 0 };
	bool actions_end = false;
	uint32_t type;
	bool reformat_used = false;
	unsigned int of_vlan_offset;
	uint16_t action_pos;
	uint16_t jump_pos;
	uint32_t ct_idx;
	int err;
	uint32_t target_grp = 0;

	flow_hw_modify_field_init(&mhdr, at);
	if (attr->transfer)
		type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		type = MLX5DR_TABLE_TYPE_NIC_RX;
	for (; !actions_end; actions++, masks++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			action_pos = at->actions_off[actions - at->actions];
			if (!attr->group) {
				DRV_LOG(ERR, "Indirect action is not supported in root table.");
				goto err;
			}
			if (actions->conf && masks->conf) {
				if (flow_hw_shared_action_translate
				(dev, actions, acts, actions - action_start, action_pos))
					goto err;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)){
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			action_pos = at->actions_off[actions - at->actions];
			acts->rule_acts[action_pos].action =
				priv->hw_drop[!!attr->group];
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			action_pos = at->actions_off[actions - at->actions];
			acts->mark = true;
			if (masks->conf &&
			    ((const struct rte_flow_action_mark *)
			     masks->conf)->id)
				acts->rule_acts[action_pos].tag.value =
					mlx5_flow_mark_set
					(((const struct rte_flow_action_mark *)
					(actions->conf))->id);
			else if (__flow_hw_act_data_general_append(priv, acts,
				actions->type, actions - action_start, action_pos))
				goto err;
			acts->rule_acts[action_pos].action =
				priv->hw_tag[!!attr->group];
			__atomic_fetch_add(&priv->hws_mark_refcnt, 1, __ATOMIC_RELAXED);
			flow_hw_rxq_flag_set(dev, true);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			action_pos = at->actions_off[actions - at->actions];
			acts->rule_acts[action_pos].action =
				priv->hw_push_vlan[type];
			if (is_template_masked_push_vlan(masks->conf))
				acts->rule_acts[action_pos].push_vlan.vlan_hdr =
					vlan_hdr_to_be32(actions);
			else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos))
				goto err;
			of_vlan_offset = is_of_vlan_pcp_present(actions) ?
					MLX5_HW_VLAN_PUSH_PCP_IDX :
					MLX5_HW_VLAN_PUSH_VID_IDX;
			actions += of_vlan_offset;
			masks += of_vlan_offset;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			action_pos = at->actions_off[actions - at->actions];
			acts->rule_acts[action_pos].action =
				priv->hw_pop_vlan[type];
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			action_pos = at->actions_off[actions - at->actions];
			if (masks->conf &&
			    ((const struct rte_flow_action_jump *)
			     masks->conf)->group) {
				uint32_t jump_group =
					((const struct rte_flow_action_jump *)
					actions->conf)->group;
				acts->jump = flow_hw_jump_action_register
						(dev, cfg, jump_group, error);
				if (!acts->jump)
					goto err;
				acts->rule_acts[action_pos].action = (!!attr->group) ?
						acts->jump->hws_action :
						acts->jump->root_action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)){
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			action_pos = at->actions_off[actions - at->actions];
			if (masks->conf &&
			    ((const struct rte_flow_action_queue *)
			     masks->conf)->index) {
				acts->tir = flow_hw_tir_action_register
				(dev,
				 mlx5_hw_act_flag[!!attr->group][type],
				 actions);
				if (!acts->tir)
					goto err;
				acts->rule_acts[action_pos].action =
					acts->tir->action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			action_pos = at->actions_off[actions - at->actions];
			if (actions->conf && masks->conf) {
				acts->tir = flow_hw_tir_action_register
				(dev,
				 mlx5_hw_act_flag[!!attr->group][type],
				 actions);
				if (!acts->tir)
					goto err;
				acts->rule_acts[action_pos].action =
					acts->tir->action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			MLX5_ASSERT(!reformat_used);
			enc_item = ((const struct rte_flow_action_vxlan_encap *)
				   actions->conf)->definition;
			if (masks->conf)
				enc_item_m = ((const struct rte_flow_action_vxlan_encap *)
					     masks->conf)->definition;
			reformat_used = true;
			reformat_src = actions - action_start;
			refmt_type = MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			MLX5_ASSERT(!reformat_used);
			enc_item = ((const struct rte_flow_action_nvgre_encap *)
				   actions->conf)->definition;
			if (masks->conf)
				enc_item_m = ((const struct rte_flow_action_nvgre_encap *)
					     masks->conf)->definition;
			reformat_used = true;
			reformat_src = actions - action_start;
			refmt_type = MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			MLX5_ASSERT(!reformat_used);
			reformat_used = true;
			refmt_type = MLX5DR_ACTION_REFORMAT_TYPE_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap_data =
				(const struct rte_flow_action_raw_encap *)
				 masks->conf;
			if (raw_encap_data)
				encap_data_m = raw_encap_data->data;
			raw_encap_data =
				(const struct rte_flow_action_raw_encap *)
				 actions->conf;
			encap_data = raw_encap_data->data;
			data_size = raw_encap_data->size;
			if (reformat_used) {
				refmt_type = data_size <
				MLX5_ENCAPSULATION_DECISION_SIZE ?
				MLX5DR_ACTION_REFORMAT_TYPE_TNL_L3_TO_L2 :
				MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L3;
			} else {
				reformat_used = true;
				refmt_type =
				MLX5DR_ACTION_REFORMAT_TYPE_L2_TO_TNL_L2;
			}
			reformat_src = actions - action_start;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			reformat_used = true;
			refmt_type = MLX5DR_ACTION_REFORMAT_TYPE_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, error);
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Send to kernel action on root table is not supported in HW steering mode");
			}
			action_pos = at->actions_off[actions - at->actions];
			acts->rule_acts[action_pos].action = priv->hw_send_to_kernel;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			err = flow_hw_modify_field_compile(dev, attr, action_start,
							   actions, masks, acts, &mhdr,
							   error);
			if (err)
				goto err;
			/*
			 * Adjust the action source position for the following.
			 * ... / MODIFY_FIELD: rx_cpy_pos / (QUEUE|RSS) / ...
			 * The next action will be Q/RSS, there will not be
			 * another adjustment and the real source position of
			 * the following actions will be decreased by 1.
			 * No change of the total actions in the new template.
			 */
			if ((actions - action_start) == at->rx_cpy_pos)
				action_start += 1;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			action_pos = at->actions_off[actions - at->actions];
			if (flow_hw_represented_port_compile
					(dev, attr, action_start, actions,
					 masks, acts, action_pos, error))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			/*
			 * METER action is compiled to 2 DR actions - ASO_METER and FT.
			 * Calculated DR offset is stored only for ASO_METER and FT
			 * is assumed to be the next action.
			 */
			action_pos = at->actions_off[actions - at->actions];
			jump_pos = action_pos + 1;
			if (actions->conf && masks->conf &&
			    ((const struct rte_flow_action_meter *)
			     masks->conf)->mtr_id) {
				err = flow_hw_meter_compile(dev, cfg,
						action_pos, jump_pos, actions, acts, error);
				if (err)
					goto err;
			} else if (__flow_hw_act_data_general_append(priv, acts,
							actions->type,
							actions - action_start,
							action_pos))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, error);
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Age action on root table is not supported in HW steering mode");
			}
			action_pos = at->actions_off[actions - at->actions];
			if (__flow_hw_act_data_general_append(priv, acts,
							 actions->type,
							 actions - action_start,
							 action_pos))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, error);
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				return rte_flow_error_set(error, ENOTSUP,
						RTE_FLOW_ERROR_TYPE_ACTION,
						NULL,
						"Counter action on root table is not supported in HW steering mode");
			}
			if ((at->action_flags & MLX5_FLOW_ACTION_AGE) ||
			    (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE))
				/*
				 * When both COUNT and AGE are requested, it is
				 * saved as AGE action which creates also the
				 * counter.
				 */
				break;
			action_pos = at->actions_off[actions - at->actions];
			if (masks->conf &&
			    ((const struct rte_flow_action_count *)
			     masks->conf)->id) {
				err = flow_hw_cnt_compile(dev, action_pos, acts);
				if (err)
					goto err;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			action_pos = at->actions_off[actions - at->actions];
			if (masks->conf) {
				ct_idx = MLX5_ACTION_CTX_CT_GET_IDX
					 ((uint32_t)(uintptr_t)actions->conf);
				if (flow_hw_ct_compile(dev, MLX5_HW_INV_QUEUE, ct_idx,
						       &acts->rule_acts[action_pos]))
					goto err;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 actions - action_start, action_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			action_pos = at->actions_off[actions - at->actions];
			if (actions->conf && masks->conf &&
			    ((const struct rte_flow_action_meter_mark *)
			     masks->conf)->profile) {
				err = flow_hw_meter_mark_compile(dev,
							action_pos, actions,
							acts->rule_acts,
							&acts->mtr_id,
							MLX5_HW_INV_QUEUE);
				if (err)
					goto err;
			} else if (__flow_hw_act_data_general_append(priv, acts,
							actions->type,
							actions - action_start,
							action_pos))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			break;
		default:
			break;
		}
	}
	if (mhdr.pos != UINT16_MAX) {
		uint32_t flags;
		uint32_t bulk_size;
		size_t mhdr_len;

		if (flow_hw_validate_compiled_modify_field(dev, cfg, &mhdr, error)) {
			__flow_hw_action_template_destroy(dev, acts);
			return -rte_errno;
		}
		acts->mhdr = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*acts->mhdr),
					 0, SOCKET_ID_ANY);
		if (!acts->mhdr)
			goto err;
		rte_memcpy(acts->mhdr, &mhdr, sizeof(*acts->mhdr));
		mhdr_len = sizeof(struct mlx5_modification_cmd) * acts->mhdr->mhdr_cmds_num;
		flags = mlx5_hw_act_flag[!!attr->group][type];
		if (acts->mhdr->shared) {
			flags |= MLX5DR_ACTION_FLAG_SHARED;
			bulk_size = 0;
		} else {
			bulk_size = rte_log2_u32(table_attr->nb_flows);
		}
		acts->mhdr->action = mlx5dr_action_create_modify_header
				(priv->dr_ctx, mhdr_len, (__be64 *)acts->mhdr->mhdr_cmds,
				 bulk_size, flags);
		if (!acts->mhdr->action)
			goto err;
		acts->rule_acts[acts->mhdr->pos].action = acts->mhdr->action;
	}
	if (reformat_used) {
		uint8_t buf[MLX5_ENCAP_MAX_LEN];
		bool shared_rfmt = true;

		MLX5_ASSERT(at->reformat_off != UINT16_MAX);
		if (enc_item) {
			MLX5_ASSERT(!encap_data);
			if (flow_dv_convert_encap_data(enc_item, buf, &data_size, error))
				goto err;
			encap_data = buf;
			if (!enc_item_m)
				shared_rfmt = false;
		} else if (encap_data && !encap_data_m) {
			shared_rfmt = false;
		}
		acts->encap_decap = mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(*acts->encap_decap) + data_size,
				    0, SOCKET_ID_ANY);
		if (!acts->encap_decap)
			goto err;
		if (data_size) {
			acts->encap_decap->data_size = data_size;
			memcpy(acts->encap_decap->data, encap_data, data_size);
		}
		acts->encap_decap->action = mlx5dr_action_create_reformat
				(priv->dr_ctx, refmt_type,
				 data_size, encap_data,
				 shared_rfmt ? 0 : rte_log2_u32(table_attr->nb_flows),
				 mlx5_hw_act_flag[!!attr->group][type] |
				 (shared_rfmt ? MLX5DR_ACTION_FLAG_SHARED : 0));
		if (!acts->encap_decap->action)
			goto err;
		acts->rule_acts[at->reformat_off].action = acts->encap_decap->action;
		acts->rule_acts[at->reformat_off].reformat.data = acts->encap_decap->data;
		if (shared_rfmt)
			acts->rule_acts[at->reformat_off].reformat.offset = 0;
		else if (__flow_hw_act_data_encap_append(priv, acts,
				 (action_start + reformat_src)->type,
				 reformat_src, at->reformat_off, data_size))
			goto err;
		acts->encap_decap->shared = shared_rfmt;
		acts->encap_decap_pos = at->reformat_off;
	}
	return 0;
err:
	err = rte_errno;
	__flow_hw_action_template_destroy(dev, acts);
	return rte_flow_error_set(error, err,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "fail to create rte table");
}

/**
 * Translate rte_flow actions to DR action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] tbl
 *   Pointer to the flow template table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_actions_translate(struct rte_eth_dev *dev,
			  struct rte_flow_template_table *tbl,
			  struct rte_flow_error *error)
{
	uint32_t i;

	for (i = 0; i < tbl->nb_action_templates; i++) {
		if (__flow_hw_actions_translate(dev, &tbl->cfg,
						&tbl->ats[i].acts,
						tbl->ats[i].action_template,
						error))
			goto err;
	}
	return 0;
err:
	while (i--)
		__flow_hw_action_template_destroy(dev, &tbl->ats[i].acts);
	return -1;
}

/**
 * Get shared indirect action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev data structure.
 * @param[in] act_data
 *   Pointer to the recorded action construct data.
 * @param[in] item_flags
 *   The matcher itme_flags used for RSS lookup.
 * @param[in] rule_act
 *   Pointer to the shared action's destination rule DR action.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
flow_hw_shared_action_get(struct rte_eth_dev *dev,
			  struct mlx5_action_construct_data *act_data,
			  const uint64_t item_flags,
			  struct mlx5dr_rule_action *rule_act)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_rss_desc rss_desc = { 0 };
	uint64_t hash_fields = 0;
	uint32_t hrxq_idx = 0;
	struct mlx5_hrxq *hrxq = NULL;
	int act_type = act_data->type;

	switch (act_type) {
	case MLX5_RTE_FLOW_ACTION_TYPE_RSS:
		rss_desc.level = act_data->shared_rss.level;
		rss_desc.types = act_data->shared_rss.types;
		flow_dv_hashfields_set(item_flags, &rss_desc, &hash_fields);
		hrxq_idx = flow_dv_action_rss_hrxq_lookup
			(dev, act_data->shared_rss.idx, hash_fields);
		if (hrxq_idx)
			hrxq = mlx5_ipool_get(priv->sh->ipool[MLX5_IPOOL_HRXQ],
					      hrxq_idx);
		if (hrxq) {
			rule_act->action = hrxq->action;
			return 0;
		}
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type:%d",
			act_data->type);
		break;
	}
	return -1;
}

static void
flow_hw_construct_quota(struct mlx5_priv *priv,
			struct mlx5dr_rule_action *rule_act, uint32_t qid)
{
	rule_act->action = priv->quota_ctx.dr_action;
	rule_act->aso_meter.offset = qid - 1;
	rule_act->aso_meter.init_color =
		MLX5DR_ACTION_ASO_METER_COLOR_GREEN;
}

/**
 * Construct shared indirect action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev data structure.
 * @param[in] queue
 *   The flow creation queue index.
 * @param[in] action
 *   Pointer to the shared indirect rte_flow action.
 * @param[in] table
 *   Pointer to the flow table.
 * @param[in] it_idx
 *   Item template index the action template refer to.
 * @param[in] action_flags
 *   Actions bit-map detected in this template.
 * @param[in, out] flow
 *   Pointer to the flow containing the counter.
 * @param[in] rule_act
 *   Pointer to the shared action's destination rule DR action.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
flow_hw_shared_action_construct(struct rte_eth_dev *dev, uint32_t queue,
				const struct rte_flow_action *action,
				struct rte_flow_template_table *table,
				const uint8_t it_idx, uint64_t action_flags,
				struct rte_flow_hw *flow,
				struct mlx5dr_rule_action *rule_act)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_action_construct_data act_data;
	struct mlx5_shared_action_rss *shared_rss;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_age_info *age_info;
	struct mlx5_hws_age_param *param;
	uint32_t act_idx = (uint32_t)(uintptr_t)action->conf;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx &
		       ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	uint64_t item_flags;
	cnt_id_t age_cnt;

	memset(&act_data, 0, sizeof(act_data));
	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		act_data.type = MLX5_RTE_FLOW_ACTION_TYPE_RSS;
		shared_rss = mlx5_ipool_get
			(priv->sh->ipool[MLX5_IPOOL_RSS_SHARED_ACTIONS], idx);
		if (!shared_rss)
			return -1;
		act_data.shared_rss.idx = idx;
		act_data.shared_rss.level = shared_rss->origin.level;
		act_data.shared_rss.types = !shared_rss->origin.types ?
					    RTE_ETH_RSS_IP :
					    shared_rss->origin.types;
		item_flags = table->its[it_idx]->item_flags;
		if (flow_hw_shared_action_get
				(dev, &act_data, item_flags, rule_act))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		if (mlx5_hws_cnt_pool_get_action_offset(priv->hws_cpool,
				act_idx,
				&rule_act->action,
				&rule_act->counter.offset))
			return -1;
		flow->cnt_id = act_idx;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		/*
		 * Save the index with the indirect type, to recognize
		 * it in flow destroy.
		 */
		flow->age_idx = act_idx;
		if (action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT)
			/*
			 * The mutual update for idirect AGE & COUNT will be
			 * performed later after we have ID for both of them.
			 */
			break;
		age_info = GET_PORT_AGE_INFO(priv);
		param = mlx5_ipool_get(age_info->ages_ipool, idx);
		if (param == NULL)
			return -1;
		if (action_flags & MLX5_FLOW_ACTION_COUNT) {
			if (mlx5_hws_cnt_pool_get(priv->hws_cpool,
						  &param->queue_id, &age_cnt,
						  idx) < 0)
				return -1;
			flow->cnt_id = age_cnt;
			param->nb_cnts++;
		} else {
			/*
			 * Get the counter of this indirect AGE or create one
			 * if doesn't exist.
			 */
			age_cnt = mlx5_hws_age_cnt_get(priv, param, idx);
			if (age_cnt == 0)
				return -1;
		}
		if (mlx5_hws_cnt_pool_get_action_offset(priv->hws_cpool,
						     age_cnt, &rule_act->action,
						     &rule_act->counter.offset))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		if (flow_hw_ct_compile(dev, queue, idx, rule_act))
			return -1;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		/* Find ASO object. */
		aso_mtr = mlx5_ipool_get(pool->idx_pool, idx);
		if (!aso_mtr)
			return -1;
		rule_act->action = pool->action;
		rule_act->aso_meter.offset = aso_mtr->offset;
		rule_act->aso_meter.init_color =
			(enum mlx5dr_action_aso_meter_color)
			rte_col_2_mlx5_col(aso_mtr->init_color);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		flow_hw_construct_quota(priv, rule_act, idx);
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type:%d", type);
		break;
	}
	return 0;
}

static __rte_always_inline int
flow_hw_mhdr_cmd_is_nop(const struct mlx5_modification_cmd *cmd)
{
	struct mlx5_modification_cmd cmd_he = {
		.data0 = rte_be_to_cpu_32(cmd->data0),
		.data1 = 0,
	};

	return cmd_he.action_type == MLX5_MODIFICATION_TYPE_NOP;
}

/**
 * Construct flow action array.
 *
 * For action template contains dynamic actions, these actions need to
 * be updated according to the rte_flow action during flow creation.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] job
 *   Pointer to job descriptor.
 * @param[in] hw_acts
 *   Pointer to translated actions from template.
 * @param[in] it_idx
 *   Item template index the action template refer to.
 * @param[in] actions
 *   Array of rte_flow action need to be checked.
 * @param[in] rule_acts
 *   Array of DR rule actions to be used during flow creation..
 * @param[in] acts_num
 *   Pointer to the real acts_num flow has.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
flow_hw_modify_field_construct(struct mlx5_hw_q_job *job,
			       struct mlx5_action_construct_data *act_data,
			       const struct mlx5_hw_actions *hw_acts,
			       const struct rte_flow_action *action)
{
	const struct rte_flow_action_modify_field *mhdr_action = action->conf;
	uint8_t values[16] = { 0 };
	unaligned_uint32_t *value_p;
	uint32_t i;
	struct field_modify_info *field;

	if (!hw_acts->mhdr)
		return -1;
	if (hw_acts->mhdr->shared || act_data->modify_header.shared)
		return 0;
	MLX5_ASSERT(mhdr_action->operation == RTE_FLOW_MODIFY_SET ||
		    mhdr_action->operation == RTE_FLOW_MODIFY_ADD);
	if (mhdr_action->src.field != RTE_FLOW_FIELD_VALUE &&
	    mhdr_action->src.field != RTE_FLOW_FIELD_POINTER)
		return 0;
	if (mhdr_action->src.field == RTE_FLOW_FIELD_VALUE)
		rte_memcpy(values, &mhdr_action->src.value, sizeof(values));
	else
		rte_memcpy(values, mhdr_action->src.pvalue, sizeof(values));
	if (mhdr_action->dst.field == RTE_FLOW_FIELD_META ||
	    mhdr_action->dst.field == RTE_FLOW_FIELD_TAG ||
	    mhdr_action->dst.field == RTE_FLOW_FIELD_METER_COLOR ||
	    mhdr_action->dst.field == (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG) {
		uint8_t tag_index = flow_tag_index_get(&mhdr_action->dst);

		value_p = (unaligned_uint32_t *)values;
		if (mhdr_action->dst.field == RTE_FLOW_FIELD_TAG &&
		    tag_index == MLX5_LINEAR_HASH_TAG_INDEX)
			*value_p = rte_cpu_to_be_32(*value_p << 16);
		else
			*value_p = rte_cpu_to_be_32(*value_p);
	} else if (mhdr_action->dst.field == RTE_FLOW_FIELD_GTP_PSC_QFI) {
		uint32_t tmp;

		/*
		 * QFI is passed as an uint8_t integer, but it is accessed through
		 * a 2nd least significant byte of a 32-bit field in modify header command.
		 */
		tmp = values[0];
		value_p = (unaligned_uint32_t *)values;
		*value_p = rte_cpu_to_be_32(tmp << 8);
	}
	i = act_data->modify_header.mhdr_cmds_off;
	field = act_data->modify_header.field;
	do {
		uint32_t off_b;
		uint32_t mask;
		uint32_t data;
		const uint8_t *mask_src;

		if (i >= act_data->modify_header.mhdr_cmds_end)
			return -1;
		if (flow_hw_mhdr_cmd_is_nop(&job->mhdr_cmd[i])) {
			++i;
			continue;
		}
		mask_src = (const uint8_t *)act_data->modify_header.mask;
		mask = flow_dv_fetch_field(mask_src + field->offset, field->size);
		if (!mask) {
			++field;
			continue;
		}
		off_b = rte_bsf32(mask);
		data = flow_dv_fetch_field(values + field->offset, field->size);
		data = (data & mask) >> off_b;
		job->mhdr_cmd[i++].data1 = rte_cpu_to_be_32(data);
		++field;
	} while (field->size);
	return 0;
}

/**
 * Construct flow action array.
 *
 * For action template contains dynamic actions, these actions need to
 * be updated according to the rte_flow action during flow creation.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] job
 *   Pointer to job descriptor.
 * @param[in] hw_acts
 *   Pointer to translated actions from template.
 * @param[in] it_idx
 *   Item template index the action template refer to.
 * @param[in] actions
 *   Array of rte_flow action need to be checked.
 * @param[in] rule_acts
 *   Array of DR rule actions to be used during flow creation..
 * @param[in] acts_num
 *   Pointer to the real acts_num flow has.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static __rte_always_inline int
flow_hw_actions_construct(struct rte_eth_dev *dev,
			  struct mlx5_hw_q_job *job,
			  const struct mlx5_hw_action_template *hw_at,
			  const uint8_t it_idx,
			  const struct rte_flow_action actions[],
			  struct mlx5dr_rule_action *rule_acts,
			  uint32_t queue,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct rte_flow_template_table *table = job->flow->table;
	struct mlx5_action_construct_data *act_data;
	const struct rte_flow_actions_template *at = hw_at->action_template;
	const struct mlx5_hw_actions *hw_acts = &hw_at->acts;
	const struct rte_flow_action *action;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	const struct rte_flow_item *enc_item = NULL;
	const struct rte_flow_action_ethdev *port_action = NULL;
	const struct rte_flow_action_meter *meter = NULL;
	const struct rte_flow_action_age *age = NULL;
	uint8_t *buf = job->encap_data;
	struct rte_flow_attr attr = {
			.ingress = 1,
	};
	uint32_t ft_flag;
	size_t encap_len = 0;
	int ret;
	uint32_t age_idx = 0;
	struct mlx5_aso_mtr *aso_mtr;

	rte_memcpy(rule_acts, hw_acts->rule_acts, sizeof(*rule_acts) * at->dr_actions_num);
	attr.group = table->grp->group_id;
	ft_flag = mlx5_hw_act_flag[!!table->grp->group_id][table->type];
	if (table->type == MLX5DR_TABLE_TYPE_FDB) {
		attr.transfer = 1;
		attr.ingress = 1;
	} else if (table->type == MLX5DR_TABLE_TYPE_NIC_TX) {
		attr.egress = 1;
		attr.ingress = 0;
	} else {
		attr.ingress = 1;
	}
	if (hw_acts->mhdr && hw_acts->mhdr->mhdr_cmds_num > 0) {
		uint16_t pos = hw_acts->mhdr->pos;

		if (!hw_acts->mhdr->shared) {
			rule_acts[pos].modify_header.offset =
						job->flow->res_idx - 1;
			rule_acts[pos].modify_header.data =
						(uint8_t *)job->mhdr_cmd;
			rte_memcpy(job->mhdr_cmd, hw_acts->mhdr->mhdr_cmds,
				   sizeof(*job->mhdr_cmd) * hw_acts->mhdr->mhdr_cmds_num);
		}
	}
	LIST_FOREACH(act_data, &hw_acts->act_list, next) {
		uint32_t jump_group;
		uint32_t tag;
		uint64_t item_flags;
		struct mlx5_hw_jump_action *jump;
		struct mlx5_hrxq *hrxq;
		uint32_t ct_idx;
		cnt_id_t cnt_id;
		uint32_t mtr_id;

		action = &actions[act_data->action_src];
		/*
		 * action template construction replaces
		 * OF_SET_VLAN_VID with MODIFY_FIELD
		 */
		if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)
			MLX5_ASSERT(act_data->type ==
				    RTE_FLOW_ACTION_TYPE_MODIFY_FIELD);
		else
			MLX5_ASSERT(action->type ==
				    RTE_FLOW_ACTION_TYPE_INDIRECT ||
				    (int)action->type == act_data->type);
		switch (act_data->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			if (flow_hw_shared_action_construct
					(dev, queue, action, table, it_idx,
					 at->action_flags, job->flow,
					 &rule_acts[act_data->action_dst]))
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			tag = mlx5_flow_mark_set
			      (((const struct rte_flow_action_mark *)
			      (action->conf))->id);
			rule_acts[act_data->action_dst].tag.value = tag;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			rule_acts[act_data->action_dst].push_vlan.vlan_hdr =
				vlan_hdr_to_be32(action);
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			jump_group = ((const struct rte_flow_action_jump *)
						action->conf)->group;
			jump = flow_hw_jump_action_register
				(dev, &table->cfg, jump_group, NULL);
			if (!jump)
				return -1;
			rule_acts[act_data->action_dst].action =
			(!!attr.group) ? jump->hws_action : jump->root_action;
			job->flow->jump = jump;
			job->flow->fate_type = MLX5_FLOW_FATE_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			hrxq = flow_hw_tir_action_register(dev,
					ft_flag,
					action);
			if (!hrxq)
				return -1;
			rule_acts[act_data->action_dst].action = hrxq->action;
			job->flow->hrxq = hrxq;
			job->flow->fate_type = MLX5_FLOW_FATE_QUEUE;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_RSS:
			item_flags = table->its[it_idx]->item_flags;
			if (flow_hw_shared_action_get
				(dev, act_data, item_flags,
				 &rule_acts[act_data->action_dst]))
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			enc_item = ((const struct rte_flow_action_vxlan_encap *)
				   action->conf)->definition;
			if (flow_dv_convert_encap_data(enc_item, buf, &encap_len, NULL))
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			enc_item = ((const struct rte_flow_action_nvgre_encap *)
				   action->conf)->definition;
			if (flow_dv_convert_encap_data(enc_item, buf, &encap_len, NULL))
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap_data =
				(const struct rte_flow_action_raw_encap *)
				 action->conf;
			rte_memcpy((void *)buf, raw_encap_data->data, act_data->encap.len);
			MLX5_ASSERT(raw_encap_data->size ==
				    act_data->encap.len);
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)
				ret = flow_hw_set_vlan_vid_construct(dev, job,
								     act_data,
								     hw_acts,
								     action);
			else
				ret = flow_hw_modify_field_construct(job,
								     act_data,
								     hw_acts,
								     action);
			if (ret)
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			port_action = action->conf;
			if (!priv->hw_vport[port_action->port_id])
				return -1;
			rule_acts[act_data->action_dst].action =
					priv->hw_vport[port_action->port_id];
			break;
		case RTE_FLOW_ACTION_TYPE_QUOTA:
			flow_hw_construct_quota(priv,
						rule_acts + act_data->action_dst,
						act_data->shared_meter.id);
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			meter = action->conf;
			mtr_id = meter->mtr_id;
			aso_mtr = mlx5_aso_meter_by_idx(priv, mtr_id);
			rule_acts[act_data->action_dst].action =
				priv->mtr_bulk.action;
			rule_acts[act_data->action_dst].aso_meter.offset =
								aso_mtr->offset;
			jump = flow_hw_jump_action_register
				(dev, &table->cfg, aso_mtr->fm.group, NULL);
			if (!jump)
				return -1;
			MLX5_ASSERT
				(!rule_acts[act_data->action_dst + 1].action);
			rule_acts[act_data->action_dst + 1].action =
					(!!attr.group) ? jump->hws_action :
							 jump->root_action;
			job->flow->jump = jump;
			job->flow->fate_type = MLX5_FLOW_FATE_JUMP;
			if (mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr))
				return -1;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			age = action->conf;
			/*
			 * First, create the AGE parameter, then create its
			 * counter later:
			 * Regular counter - in next case.
			 * Indirect counter - update it after the loop.
			 */
			age_idx = mlx5_hws_age_action_create(priv, queue, 0,
							     age,
							     job->flow->res_idx,
							     error);
			if (age_idx == 0)
				return -rte_errno;
			job->flow->age_idx = age_idx;
			if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT)
				/*
				 * When AGE uses indirect counter, no need to
				 * create counter but need to update it with the
				 * AGE parameter, will be done after the loop.
				 */
				break;
			/* Fall-through. */
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_hws_cnt_pool_get(priv->hws_cpool,
					(priv->shared_refcnt ||
					 priv->hws_cpool->cfg.host_cpool) ?
					NULL : &queue, &cnt_id, age_idx);
			if (ret != 0)
				return ret;
			ret = mlx5_hws_cnt_pool_get_action_offset
				(priv->hws_cpool,
				 cnt_id,
				 &rule_acts[act_data->action_dst].action,
				 &rule_acts[act_data->action_dst].counter.offset
				 );
			if (ret != 0)
				return ret;
			job->flow->cnt_id = cnt_id;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_hws_cnt_pool_get_action_offset
				(priv->hws_cpool,
				 act_data->shared_counter.id,
				 &rule_acts[act_data->action_dst].action,
				 &rule_acts[act_data->action_dst].counter.offset
				 );
			if (ret != 0)
				return ret;
			job->flow->cnt_id = act_data->shared_counter.id;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			ct_idx = MLX5_ACTION_CTX_CT_GET_IDX
				 ((uint32_t)(uintptr_t)action->conf);
			if (flow_hw_ct_compile(dev, queue, ct_idx,
					       &rule_acts[act_data->action_dst]))
				return -1;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK:
			mtr_id = act_data->shared_meter.id &
				((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
			/* Find ASO object. */
			aso_mtr = mlx5_ipool_get(pool->idx_pool, mtr_id);
			if (!aso_mtr)
				return -1;
			rule_acts[act_data->action_dst].action =
							pool->action;
			rule_acts[act_data->action_dst].aso_meter.offset =
							aso_mtr->offset;
			rule_acts[act_data->action_dst].aso_meter.init_color =
				(enum mlx5dr_action_aso_meter_color)
				rte_col_2_mlx5_col(aso_mtr->init_color);
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			/*
			 * Allocate meter directly will slow down flow
			 * insertion rate.
			 */
			ret = flow_hw_meter_mark_compile(dev,
				act_data->action_dst, action,
				rule_acts, &job->flow->mtr_id, MLX5_HW_INV_QUEUE);
			if (ret != 0)
				return ret;
			break;
		default:
			break;
		}
	}
	if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT) {
		if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE) {
			age_idx = job->flow->age_idx & MLX5_HWS_AGE_IDX_MASK;
			if (mlx5_hws_cnt_age_get(priv->hws_cpool,
						 job->flow->cnt_id) != age_idx)
				/*
				 * This is first use of this indirect counter
				 * for this indirect AGE, need to increase the
				 * number of counters.
				 */
				mlx5_hws_age_nb_cnt_increase(priv, age_idx);
		}
		/*
		 * Update this indirect counter the indirect/direct AGE in which
		 * using it.
		 */
		mlx5_hws_cnt_age_set(priv->hws_cpool, job->flow->cnt_id,
				     age_idx);
	}
	if (hw_acts->encap_decap && !hw_acts->encap_decap->shared) {
		rule_acts[hw_acts->encap_decap_pos].reformat.offset =
				job->flow->res_idx - 1;
		rule_acts[hw_acts->encap_decap_pos].reformat.data = buf;
	}
	if (mlx5_hws_cnt_id_valid(hw_acts->cnt_id))
		job->flow->cnt_id = hw_acts->cnt_id;
	return 0;
}

static const struct rte_flow_item *
flow_hw_get_rule_items(struct rte_eth_dev *dev,
		       struct rte_flow_template_table *table,
		       const struct rte_flow_item items[],
		       uint8_t pattern_template_index,
		       struct mlx5_hw_q_job *job)
{
	struct rte_flow_pattern_template *pt = table->its[pattern_template_index];

	/* Only one implicit item can be added to flow rule pattern. */
	MLX5_ASSERT(!pt->implicit_port || !pt->implicit_tag);
	/* At least one item was allocated in job descriptor for items. */
	MLX5_ASSERT(MLX5_HW_MAX_ITEMS >= 1);
	if (pt->implicit_port) {
		if (pt->orig_item_nb + 1 > MLX5_HW_MAX_ITEMS) {
			rte_errno = ENOMEM;
			return NULL;
		}
		/* Set up represented port item in job descriptor. */
		job->port_spec = (struct rte_flow_item_ethdev){
			.port_id = dev->data->port_id,
		};
		job->items[0] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &job->port_spec,
		};
		rte_memcpy(&job->items[1], items, sizeof(*items) * pt->orig_item_nb);
		return job->items;
	} else if (pt->implicit_tag) {
		if (pt->orig_item_nb + 1 > MLX5_HW_MAX_ITEMS) {
			rte_errno = ENOMEM;
			return NULL;
		}
		/* Set up tag item in job descriptor. */
		job->tag_spec = (struct rte_flow_item_tag){
			.data = flow_hw_tx_tag_regc_value(dev),
		};
		job->items[0] = (struct rte_flow_item){
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &job->tag_spec,
		};
		rte_memcpy(&job->items[1], items, sizeof(*items) * pt->orig_item_nb);
		return job->items;
	} else {
		return items;
	}
}

/**
 * Enqueue HW steering flow creation.
 *
 * The flow will be applied to the HW only if the postpone bit is not set or
 * the extra push function is called.
 * The flow creation status should be checked from dequeue result.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to create the flow.
 * @param[in] attr
 *   Pointer to the flow operation attributes.
 * @param[in] items
 *   Items with flow spec value.
 * @param[in] pattern_template_index
 *   The item pattern flow follows from the table.
 * @param[in] actions
 *   Action with flow spec value.
 * @param[in] action_template_index
 *   The action pattern flow follows from the table.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Flow pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow *
flow_hw_async_flow_create(struct rte_eth_dev *dev,
			  uint32_t queue,
			  const struct rte_flow_op_attr *attr,
			  struct rte_flow_template_table *table,
			  const struct rte_flow_item items[],
			  uint8_t pattern_template_index,
			  const struct rte_flow_action actions[],
			  uint8_t action_template_index,
			  void *user_data,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_rule_attr rule_attr = {
		.queue_id = queue,
		.user_data = user_data,
		.burst = attr->postpone,
	};
	struct mlx5dr_rule_action rule_acts[MLX5_HW_MAX_ACTS];
	struct rte_flow_hw *flow;
	struct mlx5_hw_q_job *job;
	const struct rte_flow_item *rule_items;
	uint32_t flow_idx;
	uint32_t res_idx = 0;
	int ret;

	if (unlikely((!dev->data->dev_started))) {
		rte_errno = EINVAL;
		goto error;
	}
	if (unlikely(!priv->hw_q[queue].job_idx)) {
		rte_errno = ENOMEM;
		goto error;
	}
	flow = mlx5_ipool_zmalloc(table->flow, &flow_idx);
	if (!flow)
		goto error;
	mlx5_ipool_malloc(table->resource, &res_idx);
	if (!res_idx)
		goto flow_free;
	/*
	 * Set the table here in order to know the destination table
	 * when free the flow afterwards.
	 */
	flow->table = table;
	flow->mt_idx = pattern_template_index;
	flow->idx = flow_idx;
	flow->res_idx = res_idx;
	job = priv->hw_q[queue].job[--priv->hw_q[queue].job_idx];
	/*
	 * Set the job type here in order to know if the flow memory
	 * should be freed or not when get the result from dequeue.
	 */
	job->type = MLX5_HW_Q_JOB_TYPE_CREATE;
	job->flow = flow;
	job->user_data = user_data;
	rule_attr.user_data = job;
	/*
	 * Indexed pool returns 1-based indices, but mlx5dr expects 0-based indices for rule
	 * insertion hints.
	 */
	MLX5_ASSERT(res_idx > 0);
	flow->rule_idx = res_idx - 1;
	rule_attr.rule_idx = flow->rule_idx;
	/*
	 * Construct the flow actions based on the input actions.
	 * The implicitly appended action is always fixed, like metadata
	 * copy action from FDB to NIC Rx.
	 * No need to copy and contrust a new "actions" list based on the
	 * user's input, in order to save the cost.
	 */
	if (flow_hw_actions_construct(dev, job,
				      &table->ats[action_template_index],
				      pattern_template_index, actions,
				      rule_acts, queue, error)) {
		rte_errno = EINVAL;
		goto free;
	}
	rule_items = flow_hw_get_rule_items(dev, table, items,
					    pattern_template_index, job);
	if (!rule_items)
		goto free;
	ret = mlx5dr_rule_create(table->matcher,
				 pattern_template_index, rule_items,
				 action_template_index, rule_acts,
				 &rule_attr, (struct mlx5dr_rule *)flow->rule);
	if (likely(!ret))
		return (struct rte_flow *)flow;
free:
	/* Flow created fail, return the descriptor and flow memory. */
	priv->hw_q[queue].job_idx++;
	mlx5_ipool_free(table->resource, res_idx);
flow_free:
	mlx5_ipool_free(table->flow, flow_idx);
error:
	rte_flow_error_set(error, rte_errno,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "fail to create rte flow");
	return NULL;
}

/**
 * Enqueue HW steering flow creation by index.
 *
 * The flow will be applied to the HW only if the postpone bit is not set or
 * the extra push function is called.
 * The flow creation status should be checked from dequeue result.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to create the flow.
 * @param[in] attr
 *   Pointer to the flow operation attributes.
 * @param[in] rule_index
 *   The item pattern flow follows from the table.
 * @param[in] actions
 *   Action with flow spec value.
 * @param[in] action_template_index
 *   The action pattern flow follows from the table.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Flow pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow *
flow_hw_async_flow_create_by_index(struct rte_eth_dev *dev,
			  uint32_t queue,
			  const struct rte_flow_op_attr *attr,
			  struct rte_flow_template_table *table,
			  uint32_t rule_index,
			  const struct rte_flow_action actions[],
			  uint8_t action_template_index,
			  void *user_data,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_rule_attr rule_attr = {
		.queue_id = queue,
		.user_data = user_data,
		.burst = attr->postpone,
	};
	struct mlx5dr_rule_action rule_acts[MLX5_HW_MAX_ACTS];
	struct rte_flow_hw *flow;
	struct mlx5_hw_q_job *job;
	uint32_t flow_idx;
	uint32_t res_idx = 0;
	int ret;

	if (unlikely(rule_index >= table->cfg.attr.nb_flows)) {
		rte_errno = EINVAL;
		goto error;
	}
	if (unlikely(!priv->hw_q[queue].job_idx)) {
		rte_errno = ENOMEM;
		goto error;
	}
	flow = mlx5_ipool_zmalloc(table->flow, &flow_idx);
	if (!flow)
		goto error;
	mlx5_ipool_malloc(table->resource, &res_idx);
	if (!res_idx)
		goto flow_free;
	/*
	 * Set the table here in order to know the destination table
	 * when free the flow afterwards.
	 */
	flow->table = table;
	flow->mt_idx = 0;
	flow->idx = flow_idx;
	flow->res_idx = res_idx;
	job = priv->hw_q[queue].job[--priv->hw_q[queue].job_idx];
	/*
	 * Set the job type here in order to know if the flow memory
	 * should be freed or not when get the result from dequeue.
	 */
	job->type = MLX5_HW_Q_JOB_TYPE_CREATE;
	job->flow = flow;
	job->user_data = user_data;
	rule_attr.user_data = job;
	/*
	 * Set the rule index.
	 */
	flow->rule_idx = rule_index;
	rule_attr.rule_idx = flow->rule_idx;
	/*
	 * Construct the flow actions based on the input actions.
	 * The implicitly appended action is always fixed, like metadata
	 * copy action from FDB to NIC Rx.
	 * No need to copy and contrust a new "actions" list based on the
	 * user's input, in order to save the cost.
	 */
	if (flow_hw_actions_construct(dev, job,
				      &table->ats[action_template_index],
				      action_template_index, actions,
				      rule_acts, queue, error)) {
		rte_errno = EINVAL;
		goto free;
	}
	ret = mlx5dr_rule_create(table->matcher,
				 0, NULL, action_template_index, rule_acts,
				 &rule_attr, (struct mlx5dr_rule *)flow->rule);
	if (likely(!ret))
		return (struct rte_flow *)flow;
free:
	/* Flow created fail, return the descriptor and flow memory. */
	priv->hw_q[queue].job_idx++;
	mlx5_ipool_free(table->resource, res_idx);
flow_free:
	mlx5_ipool_free(table->flow, flow_idx);
error:
	rte_flow_error_set(error, rte_errno,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "fail to create rte flow");
	return NULL;
}

/**
 * Enqueue HW steering flow update.
 *
 * The flow will be applied to the HW only if the postpone bit is not set or
 * the extra push function is called.
 * The flow destruction status should be checked from dequeue result.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to destroy the flow.
 * @param[in] attr
 *   Pointer to the flow operation attributes.
 * @param[in] flow
 *   Pointer to the flow to be destroyed.
 * @param[in] actions
 *   Action with flow spec value.
 * @param[in] action_template_index
 *   The action pattern flow follows from the table.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_async_flow_update(struct rte_eth_dev *dev,
			   uint32_t queue,
			   const struct rte_flow_op_attr *attr,
			   struct rte_flow *flow,
			   const struct rte_flow_action actions[],
			   uint8_t action_template_index,
			   void *user_data,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_rule_attr rule_attr = {
		.queue_id = queue,
		.user_data = user_data,
		.burst = attr->postpone,
	};
	struct mlx5dr_rule_action rule_acts[MLX5_HW_MAX_ACTS];
	struct rte_flow_hw *of = (struct rte_flow_hw *)flow;
	struct rte_flow_hw *nf;
	struct rte_flow_template_table *table = of->table;
	struct mlx5_hw_q_job *job;
	uint32_t res_idx = 0;
	int ret;

	if (unlikely(!priv->hw_q[queue].job_idx)) {
		rte_errno = ENOMEM;
		goto error;
	}
	mlx5_ipool_malloc(table->resource, &res_idx);
	if (!res_idx)
		goto error;
	job = priv->hw_q[queue].job[--priv->hw_q[queue].job_idx];
	nf = job->upd_flow;
	memset(nf, 0, sizeof(struct rte_flow_hw));
	/*
	 * Set the table here in order to know the destination table
	 * when free the flow afterwards.
	 */
	nf->table = table;
	nf->mt_idx = of->mt_idx;
	nf->idx = of->idx;
	nf->res_idx = res_idx;
	/*
	 * Set the job type here in order to know if the flow memory
	 * should be freed or not when get the result from dequeue.
	 */
	job->type = MLX5_HW_Q_JOB_TYPE_UPDATE;
	job->flow = nf;
	job->user_data = user_data;
	rule_attr.user_data = job;
	/*
	 * Indexed pool returns 1-based indices, but mlx5dr expects 0-based indices for rule
	 * insertion hints.
	 */
	MLX5_ASSERT(res_idx > 0);
	nf->rule_idx = res_idx - 1;
	rule_attr.rule_idx = nf->rule_idx;
	/*
	 * Construct the flow actions based on the input actions.
	 * The implicitly appended action is always fixed, like metadata
	 * copy action from FDB to NIC Rx.
	 * No need to copy and contrust a new "actions" list based on the
	 * user's input, in order to save the cost.
	 */
	if (flow_hw_actions_construct(dev, job,
				      &table->ats[action_template_index],
				      nf->mt_idx, actions,
				      rule_acts, queue, error)) {
		rte_errno = EINVAL;
		goto free;
	}
	/*
	 * Switch the old flow and the new flow.
	 */
	job->flow = of;
	job->upd_flow = nf;
	ret = mlx5dr_rule_action_update((struct mlx5dr_rule *)of->rule,
					action_template_index, rule_acts, &rule_attr);
	if (likely(!ret))
		return 0;
free:
	/* Flow created fail, return the descriptor and flow memory. */
	priv->hw_q[queue].job_idx++;
	mlx5_ipool_free(table->resource, res_idx);
error:
	return rte_flow_error_set(error, rte_errno,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"fail to update rte flow");
}

/**
 * Enqueue HW steering flow destruction.
 *
 * The flow will be applied to the HW only if the postpone bit is not set or
 * the extra push function is called.
 * The flow destruction status should be checked from dequeue result.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to destroy the flow.
 * @param[in] attr
 *   Pointer to the flow operation attributes.
 * @param[in] flow
 *   Pointer to the flow to be destroyed.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_async_flow_destroy(struct rte_eth_dev *dev,
			   uint32_t queue,
			   const struct rte_flow_op_attr *attr,
			   struct rte_flow *flow,
			   void *user_data,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_rule_attr rule_attr = {
		.queue_id = queue,
		.user_data = user_data,
		.burst = attr->postpone,
	};
	struct rte_flow_hw *fh = (struct rte_flow_hw *)flow;
	struct mlx5_hw_q_job *job;
	int ret;

	if (unlikely(!priv->hw_q[queue].job_idx)) {
		rte_errno = ENOMEM;
		goto error;
	}
	job = priv->hw_q[queue].job[--priv->hw_q[queue].job_idx];
	job->type = MLX5_HW_Q_JOB_TYPE_DESTROY;
	job->user_data = user_data;
	job->flow = fh;
	rule_attr.user_data = job;
	rule_attr.rule_idx = fh->rule_idx;
	ret = mlx5dr_rule_destroy((struct mlx5dr_rule *)fh->rule, &rule_attr);
	if (likely(!ret))
		return 0;
	priv->hw_q[queue].job_idx++;
error:
	return rte_flow_error_set(error, rte_errno,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"fail to destroy rte flow");
}

/**
 * Release the AGE and counter for given flow.
 *
 * @param[in] priv
 *   Pointer to the port private data structure.
 * @param[in] queue
 *   The queue to release the counter.
 * @param[in, out] flow
 *   Pointer to the flow containing the counter.
 * @param[out] error
 *   Pointer to error structure.
 */
static void
flow_hw_age_count_release(struct mlx5_priv *priv, uint32_t queue,
			  struct rte_flow_hw *flow,
			  struct rte_flow_error *error)
{
	if (mlx5_hws_cnt_is_shared(priv->hws_cpool, flow->cnt_id)) {
		if (flow->age_idx && !mlx5_hws_age_is_indirect(flow->age_idx)) {
			/* Remove this AGE parameter from indirect counter. */
			mlx5_hws_cnt_age_set(priv->hws_cpool, flow->cnt_id, 0);
			/* Release the AGE parameter. */
			mlx5_hws_age_action_destroy(priv, flow->age_idx, error);
			flow->age_idx = 0;
		}
		return;
	}
	/* Put the counter first to reduce the race risk in BG thread. */
	mlx5_hws_cnt_pool_put(priv->hws_cpool, &queue, &flow->cnt_id);
	flow->cnt_id = 0;
	if (flow->age_idx) {
		if (mlx5_hws_age_is_indirect(flow->age_idx)) {
			uint32_t idx = flow->age_idx & MLX5_HWS_AGE_IDX_MASK;

			mlx5_hws_age_nb_cnt_decrease(priv, idx);
		} else {
			/* Release the AGE parameter. */
			mlx5_hws_age_action_destroy(priv, flow->age_idx, error);
		}
		flow->age_idx = 0;
	}
}

static inline int
__flow_hw_pull_indir_action_comp(struct rte_eth_dev *dev,
				 uint32_t queue,
				 struct rte_flow_op_result res[],
				 uint16_t n_res)

{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_ring *r = priv->hw_q[queue].indir_cq;
	struct mlx5_hw_q_job *job;
	void *user_data = NULL;
	uint32_t type, idx;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_aso_ct_action *aso_ct;
	int ret_comp, i;

	ret_comp = (int)rte_ring_count(r);
	if (ret_comp > n_res)
		ret_comp = n_res;
	for (i = 0; i < ret_comp; i++) {
		rte_ring_dequeue(r, &user_data);
		res[i].user_data = user_data;
		res[i].status = RTE_FLOW_OP_SUCCESS;
	}
	if (ret_comp < n_res && priv->hws_mpool)
		ret_comp += mlx5_aso_pull_completion(&priv->hws_mpool->sq[queue],
				&res[ret_comp], n_res - ret_comp);
	if (ret_comp < n_res && priv->hws_ctpool)
		ret_comp += mlx5_aso_pull_completion(&priv->ct_mng->aso_sqs[queue],
				&res[ret_comp], n_res - ret_comp);
	if (ret_comp < n_res && priv->quota_ctx.sq)
		ret_comp += mlx5_aso_pull_completion(&priv->quota_ctx.sq[queue],
						     &res[ret_comp],
						     n_res - ret_comp);
	for (i = 0; i <  ret_comp; i++) {
		job = (struct mlx5_hw_q_job *)res[i].user_data;
		/* Restore user data. */
		res[i].user_data = job->user_data;
		if (MLX5_INDIRECT_ACTION_TYPE_GET(job->action) ==
		    MLX5_INDIRECT_ACTION_TYPE_QUOTA) {
			mlx5_quota_async_completion(dev, queue, job);
		} else if (job->type == MLX5_HW_Q_JOB_TYPE_DESTROY) {
			type = MLX5_INDIRECT_ACTION_TYPE_GET(job->action);
			if (type == MLX5_INDIRECT_ACTION_TYPE_METER_MARK) {
				idx = MLX5_INDIRECT_ACTION_IDX_GET(job->action);
				mlx5_ipool_free(priv->hws_mpool->idx_pool, idx);
			}
		} else if (job->type == MLX5_HW_Q_JOB_TYPE_CREATE) {
			type = MLX5_INDIRECT_ACTION_TYPE_GET(job->action);
			if (type == MLX5_INDIRECT_ACTION_TYPE_METER_MARK) {
				idx = MLX5_INDIRECT_ACTION_IDX_GET(job->action);
				aso_mtr = mlx5_ipool_get(priv->hws_mpool->idx_pool, idx);
				aso_mtr->state = ASO_METER_READY;
			} else if (type == MLX5_INDIRECT_ACTION_TYPE_CT) {
				idx = MLX5_ACTION_CTX_CT_GET_IDX
					((uint32_t)(uintptr_t)job->action);
				aso_ct = mlx5_ipool_get(priv->hws_ctpool->cts, idx);
				aso_ct->state = ASO_CONNTRACK_READY;
			}
		} else if (job->type == MLX5_HW_Q_JOB_TYPE_QUERY) {
			type = MLX5_INDIRECT_ACTION_TYPE_GET(job->action);
			if (type == MLX5_INDIRECT_ACTION_TYPE_CT) {
				idx = MLX5_ACTION_CTX_CT_GET_IDX
					((uint32_t)(uintptr_t)job->action);
				aso_ct = mlx5_ipool_get(priv->hws_ctpool->cts, idx);
				mlx5_aso_ct_obj_analyze(job->query.user,
							job->query.hw);
				aso_ct->state = ASO_CONNTRACK_READY;
			}
		}
		priv->hw_q[queue].job[priv->hw_q[queue].job_idx++] = job;
	}
	return ret_comp;
}

/**
 * Pull the enqueued flows.
 *
 * For flows enqueued from creation/destruction, the status should be
 * checked from the dequeue result.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to pull the result.
 * @param[in/out] res
 *   Array to save the results.
 * @param[in] n_res
 *   Available result with the array.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    Result number on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_pull(struct rte_eth_dev *dev,
	     uint32_t queue,
	     struct rte_flow_op_result res[],
	     uint16_t n_res,
	     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_hw_q_job *job;
	uint32_t res_idx;
	int ret, i;

	/* 1. Pull the flow completion. */
	ret = mlx5dr_send_queue_poll(priv->dr_ctx, queue, res, n_res);
	if (ret < 0)
		return rte_flow_error_set(error, rte_errno,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"fail to query flow queue");
	for (i = 0; i <  ret; i++) {
		job = (struct mlx5_hw_q_job *)res[i].user_data;
		/* Release the original resource index in case of update. */
		res_idx = job->flow->res_idx;
		/* Restore user data. */
		res[i].user_data = job->user_data;
		if (job->type == MLX5_HW_Q_JOB_TYPE_DESTROY ||
		    job->type == MLX5_HW_Q_JOB_TYPE_UPDATE) {
			if (job->flow->fate_type == MLX5_FLOW_FATE_JUMP)
				flow_hw_jump_release(dev, job->flow->jump);
			else if (job->flow->fate_type == MLX5_FLOW_FATE_QUEUE)
				mlx5_hrxq_obj_release(dev, job->flow->hrxq);
			if (mlx5_hws_cnt_id_valid(job->flow->cnt_id))
				flow_hw_age_count_release(priv, queue,
							  job->flow, error);
			if (job->flow->mtr_id) {
				mlx5_ipool_free(pool->idx_pool,	job->flow->mtr_id);
				job->flow->mtr_id = 0;
			}
			if (job->type == MLX5_HW_Q_JOB_TYPE_DESTROY) {
				mlx5_ipool_free(job->flow->table->resource, res_idx);
				mlx5_ipool_free(job->flow->table->flow, job->flow->idx);
			} else {
				rte_memcpy(job->flow, job->upd_flow,
					offsetof(struct rte_flow_hw, rule));
				mlx5_ipool_free(job->flow->table->resource, res_idx);
			}
		}
		priv->hw_q[queue].job[priv->hw_q[queue].job_idx++] = job;
	}
	/* 2. Pull indirect action comp. */
	if (ret < n_res)
		ret += __flow_hw_pull_indir_action_comp(dev, queue, &res[ret],
							n_res - ret);
	return ret;
}

static inline void
__flow_hw_push_action(struct rte_eth_dev *dev,
		    uint32_t queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_ring *iq = priv->hw_q[queue].indir_iq;
	struct rte_ring *cq = priv->hw_q[queue].indir_cq;
	void *job = NULL;
	uint32_t ret, i;

	ret = rte_ring_count(iq);
	for (i = 0; i < ret; i++) {
		rte_ring_dequeue(iq, &job);
		rte_ring_enqueue(cq, job);
	}
	if (priv->hws_ctpool)
		mlx5_aso_push_wqe(priv->sh, &priv->ct_mng->aso_sqs[queue]);
	if (priv->hws_mpool)
		mlx5_aso_push_wqe(priv->sh, &priv->hws_mpool->sq[queue]);
}

/**
 * Push the enqueued flows to HW.
 *
 * Force apply all the enqueued flows to the HW.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to push the flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_push(struct rte_eth_dev *dev,
	     uint32_t queue,
	     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	__flow_hw_push_action(dev, queue);
	ret = mlx5dr_send_queue_action(priv->dr_ctx, queue,
				       MLX5DR_SEND_QUEUE_ACTION_DRAIN_ASYNC);
	if (ret) {
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "fail to push flows");
		return ret;
	}
	return 0;
}

/**
 * Drain the enqueued flows' completion.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to pull the flow.
 * @param[in] pending_rules
 *   The pending flow number.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
__flow_hw_pull_comp(struct rte_eth_dev *dev,
		    uint32_t queue,
		    uint32_t pending_rules,
		    struct rte_flow_error *error)
{
	struct rte_flow_op_result comp[BURST_THR];
	int ret, i, empty_loop = 0;

	ret = flow_hw_push(dev, queue, error);
	if (ret < 0)
		return ret;
	while (pending_rules) {
		ret = flow_hw_pull(dev, queue, comp, BURST_THR, error);
		if (ret < 0)
			return -1;
		if (!ret) {
			rte_delay_us_sleep(20000);
			if (++empty_loop > 5) {
				DRV_LOG(WARNING, "No available dequeue, quit.");
				break;
			}
			continue;
		}
		for (i = 0; i < ret; i++) {
			if (comp[i].status == RTE_FLOW_OP_ERROR)
				DRV_LOG(WARNING, "Flow flush get error CQE.");
		}
		if ((uint32_t)ret > pending_rules) {
			DRV_LOG(WARNING, "Flow flush get extra CQE.");
			return rte_flow_error_set(error, ERANGE,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					"get extra CQE");
		}
		pending_rules -= ret;
		empty_loop = 0;
	}
	return 0;
}

/**
 * Flush created flows.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
int
flow_hw_q_flow_flush(struct rte_eth_dev *dev,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_q *hw_q;
	struct rte_flow_template_table *tbl;
	struct rte_flow_hw *flow;
	struct rte_flow_op_attr attr = {
		.postpone = 0,
	};
	uint32_t pending_rules = 0;
	uint32_t queue;
	uint32_t fidx;

	/*
	 * Ensure to push and dequeue all the enqueued flow
	 * creation/destruction jobs in case user forgot to
	 * dequeue. Or the enqueued created flows will be
	 * leaked. The forgotten dequeues would also cause
	 * flow flush get extra CQEs as expected and pending_rules
	 * be minus value.
	 */
	for (queue = 0; queue < priv->nb_queue; queue++) {
		hw_q = &priv->hw_q[queue];
		if (__flow_hw_pull_comp(dev, queue, hw_q->size - hw_q->job_idx,
					error))
			return -1;
	}
	/* Flush flow per-table from MLX5_DEFAULT_FLUSH_QUEUE. */
	hw_q = &priv->hw_q[MLX5_DEFAULT_FLUSH_QUEUE];
	LIST_FOREACH(tbl, &priv->flow_hw_tbl, next) {
		if (!tbl->cfg.external)
			continue;
		MLX5_IPOOL_FOREACH(tbl->flow, fidx, flow) {
			if (flow_hw_async_flow_destroy(dev,
						MLX5_DEFAULT_FLUSH_QUEUE,
						&attr,
						(struct rte_flow *)flow,
						NULL,
						error))
				return -1;
			pending_rules++;
			/* Drain completion with queue size. */
			if (pending_rules >= hw_q->size) {
				if (__flow_hw_pull_comp(dev,
						MLX5_DEFAULT_FLUSH_QUEUE,
						pending_rules, error))
					return -1;
				pending_rules = 0;
			}
		}
	}
	/* Drain left completion. */
	if (pending_rules &&
	    __flow_hw_pull_comp(dev, MLX5_DEFAULT_FLUSH_QUEUE, pending_rules,
				error))
		return -1;
	return 0;
}

/**
 * Create flow table.
 *
 * The input item and action templates will be binded to the table.
 * Flow memory will also be allocated. Matcher will be created based
 * on the item template. Action will be translated to the dedicated
 * DR action if possible.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] table_cfg
 *   Pointer to the table configuration.
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
flow_hw_table_create(struct rte_eth_dev *dev,
		     const struct mlx5_flow_template_table_cfg *table_cfg,
		     struct rte_flow_pattern_template *item_templates[],
		     uint8_t nb_item_templates,
		     struct rte_flow_actions_template *action_templates[],
		     uint8_t nb_action_templates,
		     struct rte_flow_error *error)
{
	struct rte_flow_error sub_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_matcher_attr matcher_attr = {0};
	struct rte_flow_template_table *tbl = NULL;
	struct mlx5_flow_group *grp;
	struct mlx5dr_match_template *mt[MLX5_HW_TBL_MAX_ITEM_TEMPLATE];
	struct mlx5dr_action_template *at[MLX5_HW_TBL_MAX_ACTION_TEMPLATE];
	const struct rte_flow_template_table_attr *attr = &table_cfg->attr;
	struct rte_flow_attr flow_attr = attr->flow_attr;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = &sub_error,
		.data = &flow_attr,
	};
	struct mlx5_indexed_pool_config cfg = {
		.size = sizeof(struct rte_flow_hw) + mlx5dr_rule_get_handle_size(),
		.trunk_size = 1 << 12,
		.per_core_cache = 1 << 13,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hw_table_flow",
	};
	struct mlx5_list_entry *ge;
	uint32_t i = 0, max_tpl = MLX5_HW_TBL_MAX_ITEM_TEMPLATE;
	uint32_t nb_flows = rte_align32pow2(attr->nb_flows);
	bool port_started = !!dev->data->dev_started;
	int err;

	/* HWS layer accepts only 1 item template with root table. */
	if (!attr->flow_attr.group)
		max_tpl = 1;
	cfg.max_idx = nb_flows;
	/* For table has very limited flows, disable cache. */
	if (nb_flows < cfg.trunk_size) {
		cfg.per_core_cache = 0;
		cfg.trunk_size = nb_flows;
	} else if (nb_flows <= MLX5_HW_IPOOL_SIZE_THRESHOLD) {
		cfg.per_core_cache = MLX5_HW_IPOOL_CACHE_MIN;
	}
	/* Check if we requires too many templates. */
	if (nb_item_templates > max_tpl ||
	    nb_action_templates > MLX5_HW_TBL_MAX_ACTION_TEMPLATE) {
		rte_errno = EINVAL;
		goto error;
	}
	/* Allocate the table memory. */
	tbl = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*tbl), 0, rte_socket_id());
	if (!tbl)
		goto error;
	tbl->cfg = *table_cfg;
	/* Allocate flow indexed pool. */
	tbl->flow = mlx5_ipool_create(&cfg);
	if (!tbl->flow)
		goto error;
	/* Allocate rule indexed pool. */
	cfg.size = 0;
	cfg.type = "mlx5_hw_table_rule";
	cfg.max_idx += priv->hw_q[0].size;
	tbl->resource = mlx5_ipool_create(&cfg);
	if (!tbl->resource)
		goto error;
	/* Register the flow group. */
	ge = mlx5_hlist_register(priv->sh->groups, attr->flow_attr.group, &ctx);
	if (!ge)
		goto error;
	grp = container_of(ge, struct mlx5_flow_group, entry);
	tbl->grp = grp;
	/* Prepare matcher information. */
	matcher_attr.optimize_flow_src = MLX5DR_MATCHER_FLOW_SRC_ANY;
	matcher_attr.priority = attr->flow_attr.priority;
	matcher_attr.optimize_using_rule_idx = true;
	matcher_attr.mode = MLX5DR_MATCHER_RESOURCE_MODE_RULE;
	matcher_attr.insert_mode = flow_hw_matcher_insert_mode_get(attr->insertion_type);
	if (attr->hash_func == RTE_FLOW_TABLE_HASH_FUNC_CRC16) {
		DRV_LOG(ERR, "16-bit checksum hash type is not supported");
		rte_errno = ENOTSUP;
		goto it_error;
	}
	matcher_attr.distribute_mode = flow_hw_matcher_distribute_mode_get(attr->hash_func);
	matcher_attr.rule.num_log = rte_log2_u32(nb_flows);
	/* Parse hints information. */
	if (attr->specialize) {
		if (attr->specialize == RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG)
			matcher_attr.optimize_flow_src = MLX5DR_MATCHER_FLOW_SRC_WIRE;
		else if (attr->specialize == RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG)
			matcher_attr.optimize_flow_src = MLX5DR_MATCHER_FLOW_SRC_VPORT;
		else
			DRV_LOG(INFO, "Unsupported hint value %x", attr->specialize);
	}
	/* Build the item template. */
	for (i = 0; i < nb_item_templates; i++) {
		uint32_t ret;

		if ((flow_attr.ingress && !item_templates[i]->attr.ingress) ||
		    (flow_attr.egress && !item_templates[i]->attr.egress) ||
		    (flow_attr.transfer && !item_templates[i]->attr.transfer)) {
			DRV_LOG(ERR, "pattern template and template table attribute mismatch");
			rte_errno = EINVAL;
			goto it_error;
		}
		ret = __atomic_fetch_add(&item_templates[i]->refcnt, 1,
					 __ATOMIC_RELAXED) + 1;
		if (ret <= 1) {
			rte_errno = EINVAL;
			goto it_error;
		}
		mt[i] = item_templates[i]->mt;
		tbl->its[i] = item_templates[i];
	}
	tbl->nb_item_templates = nb_item_templates;
	/* Build the action template. */
	for (i = 0; i < nb_action_templates; i++) {
		uint32_t ret;

		ret = __atomic_fetch_add(&action_templates[i]->refcnt, 1,
					 __ATOMIC_RELAXED) + 1;
		if (ret <= 1) {
			rte_errno = EINVAL;
			goto at_error;
		}
		at[i] = action_templates[i]->tmpl;
		tbl->ats[i].action_template = action_templates[i];
		LIST_INIT(&tbl->ats[i].acts.act_list);
		if (!port_started)
			continue;
		err = __flow_hw_actions_translate(dev, &tbl->cfg,
						  &tbl->ats[i].acts,
						  action_templates[i], &sub_error);
		if (err) {
			i++;
			goto at_error;
		}
	}
	tbl->nb_action_templates = nb_action_templates;
	tbl->matcher = mlx5dr_matcher_create
		(tbl->grp->tbl, mt, nb_item_templates, at, nb_action_templates, &matcher_attr);
	if (!tbl->matcher)
		goto at_error;
	tbl->type = attr->flow_attr.transfer ? MLX5DR_TABLE_TYPE_FDB :
		    (attr->flow_attr.egress ? MLX5DR_TABLE_TYPE_NIC_TX :
		    MLX5DR_TABLE_TYPE_NIC_RX);
	if (port_started)
		LIST_INSERT_HEAD(&priv->flow_hw_tbl, tbl, next);
	else
		LIST_INSERT_HEAD(&priv->flow_hw_tbl_ongo, tbl, next);
	return tbl;
at_error:
	while (i--) {
		__flow_hw_action_template_destroy(dev, &tbl->ats[i].acts);
		__atomic_fetch_sub(&action_templates[i]->refcnt,
				   1, __ATOMIC_RELAXED);
	}
	i = nb_item_templates;
it_error:
	while (i--)
		__atomic_fetch_sub(&item_templates[i]->refcnt,
				   1, __ATOMIC_RELAXED);
error:
	err = rte_errno;
	if (tbl) {
		if (tbl->grp)
			mlx5_hlist_unregister(priv->sh->groups,
					      &tbl->grp->entry);
		if (tbl->resource)
			mlx5_ipool_destroy(tbl->resource);
		if (tbl->flow)
			mlx5_ipool_destroy(tbl->flow);
		mlx5_free(tbl);
	}
	if (error != NULL) {
		if (sub_error.type == RTE_FLOW_ERROR_TYPE_NONE)
			rte_flow_error_set(error, err, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "Failed to create template table");
		else
			rte_memcpy(error, &sub_error, sizeof(sub_error));
	}
	return NULL;
}

/**
 * Update flow template table.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
int
flow_hw_table_update(struct rte_eth_dev *dev,
		     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_template_table *tbl;

	while ((tbl = LIST_FIRST(&priv->flow_hw_tbl_ongo)) != NULL) {
		if (flow_hw_actions_translate(dev, tbl, error))
			return -1;
		LIST_REMOVE(tbl, next);
		LIST_INSERT_HEAD(&priv->flow_hw_tbl, tbl, next);
	}
	return 0;
}

/**
 * Translates group index specified by the user in @p attr to internal
 * group index.
 *
 * Translation is done by incrementing group index, so group n becomes n + 1.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] cfg
 *   Pointer to the template table configuration.
 * @param[in] group
 *   Currently used group index (table group or jump destination).
 * @param[out] table_group
 *   Pointer to output group index.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success. Otherwise, returns negative error code, rte_errno is set
 *   and error structure is filled.
 */
static int
flow_hw_translate_group(struct rte_eth_dev *dev,
			const struct mlx5_flow_template_table_cfg *cfg,
			uint32_t group,
			uint32_t *table_group,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;
	const struct rte_flow_attr *flow_attr = &cfg->attr.flow_attr;

	if (config->dv_esw_en &&
	    priv->fdb_def_rule &&
	    cfg->external &&
	    flow_attr->transfer) {
		if (group > MLX5_HW_MAX_TRANSFER_GROUP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
						  NULL,
						  "group index not supported");
		*table_group = group + 1;
	} else if (config->dv_esw_en &&
		   (config->repr_matching || config->dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS) &&
		   cfg->external &&
		   flow_attr->egress) {
		/*
		 * On E-Switch setups, default egress flow rules are inserted to allow
		 * representor matching and/or preserving metadata across steering domains.
		 * These flow rules are inserted in group 0 and this group is reserved by PMD
		 * for these purposes.
		 *
		 * As a result, if representor matching or extended metadata mode is enabled,
		 * group provided by the user must be incremented to avoid inserting flow rules
		 * in group 0.
		 */
		if (group > MLX5_HW_MAX_EGRESS_GROUP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
						  NULL,
						  "group index not supported");
		*table_group = group + 1;
	} else {
		*table_group = group;
	}
	return 0;
}

/**
 * Create flow table.
 *
 * This function is a wrapper over @ref flow_hw_table_create(), which translates parameters
 * provided by user to proper internal values.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] attr
 *   Pointer to the table attributes.
 * @param[in] item_templates
 *   Item template array to be binded to the table.
 * @param[in] nb_item_templates
 *   Number of item templates.
 * @param[in] action_templates
 *   Action template array to be binded to the table.
 * @param[in] nb_action_templates
 *   Number of action templates.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Table on success, Otherwise, returns negative error code, rte_errno is set
 *   and error structure is filled.
 */
static struct rte_flow_template_table *
flow_hw_template_table_create(struct rte_eth_dev *dev,
			      const struct rte_flow_template_table_attr *attr,
			      struct rte_flow_pattern_template *item_templates[],
			      uint8_t nb_item_templates,
			      struct rte_flow_actions_template *action_templates[],
			      uint8_t nb_action_templates,
			      struct rte_flow_error *error)
{
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = *attr,
		.external = true,
	};
	uint32_t group = attr->flow_attr.group;

	if (flow_hw_translate_group(dev, &cfg, group, &cfg.attr.flow_attr.group, error))
		return NULL;
	return flow_hw_table_create(dev, &cfg, item_templates, nb_item_templates,
				    action_templates, nb_action_templates, error);
}

/**
 * Destroy flow table.
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
flow_hw_table_destroy(struct rte_eth_dev *dev,
		      struct rte_flow_template_table *table,
		      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i;
	uint32_t fidx = 1;
	uint32_t ridx = 1;

	/* Build ipool allocated object bitmap. */
	mlx5_ipool_flush_cache(table->resource);
	mlx5_ipool_flush_cache(table->flow);
	/* Check if ipool has allocated objects. */
	if (table->refcnt ||
	    mlx5_ipool_get_next(table->flow, &fidx) ||
	    mlx5_ipool_get_next(table->resource, &ridx)) {
		DRV_LOG(WARNING, "Table %p is still in use.", (void *)table);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "table in use");
	}
	LIST_REMOVE(table, next);
	for (i = 0; i < table->nb_item_templates; i++)
		__atomic_fetch_sub(&table->its[i]->refcnt,
				   1, __ATOMIC_RELAXED);
	for (i = 0; i < table->nb_action_templates; i++) {
		__flow_hw_action_template_destroy(dev, &table->ats[i].acts);
		__atomic_fetch_sub(&table->ats[i].action_template->refcnt,
				   1, __ATOMIC_RELAXED);
	}
	mlx5dr_matcher_destroy(table->matcher);
	mlx5_hlist_unregister(priv->sh->groups, &table->grp->entry);
	mlx5_ipool_destroy(table->resource);
	mlx5_ipool_destroy(table->flow);
	mlx5_free(table);
	return 0;
}

static bool
flow_hw_modify_field_is_used(const struct rte_flow_action_modify_field *action,
			     enum rte_flow_field_id field)
{
	return action->src.field == field || action->dst.field == field;
}

static int
flow_hw_validate_action_modify_field(const struct rte_flow_action *action,
				     const struct rte_flow_action *mask,
				     struct rte_flow_error *error)
{
	const struct rte_flow_action_modify_field *action_conf = action->conf;
	const struct rte_flow_action_modify_field *mask_conf = mask->conf;
	int ret;

	if (action_conf->operation != mask_conf->operation)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modify_field operation mask and template are not equal");
	if (action_conf->dst.field != mask_conf->dst.field)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"destination field mask and template are not equal");
	if (action_conf->dst.field == RTE_FLOW_FIELD_POINTER ||
	    action_conf->dst.field == RTE_FLOW_FIELD_VALUE ||
	    action_conf->dst.field == RTE_FLOW_FIELD_HASH_RESULT)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"immediate value, pointer and hash result cannot be used as destination");
	ret = flow_validate_modify_field_level(&action_conf->dst, error);
	if (ret)
		return ret;
	if (action_conf->dst.tag_index &&
	    !flow_modify_field_support_tag_array(action_conf->dst.field))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"destination tag index is not supported");
	if (action_conf->dst.class_id)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"destination class id is not supported");
	if (mask_conf->dst.level != UINT8_MAX)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"destination encapsulation level must be fully masked");
	if (mask_conf->dst.offset != UINT32_MAX)
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_ACTION, action,
			"destination offset level must be fully masked");
	if (action_conf->src.field != mask_conf->src.field)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"destination field mask and template are not equal");
	if (action_conf->src.field != RTE_FLOW_FIELD_POINTER &&
	    action_conf->src.field != RTE_FLOW_FIELD_VALUE) {
		if (action_conf->src.tag_index &&
		    !flow_modify_field_support_tag_array(action_conf->src.field))
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source tag index is not supported");
		if (action_conf->src.class_id)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source class id is not supported");
		if (mask_conf->src.level != UINT8_MAX)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source encapsulation level must be fully masked");
		if (mask_conf->src.offset != UINT32_MAX)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source offset level must be fully masked");
		ret = flow_validate_modify_field_level(&action_conf->src, error);
		if (ret)
			return ret;
	}
	if (mask_conf->width != UINT32_MAX)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modify_field width field must be fully masked");
	if (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_START))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying arbitrary place in a packet is not supported");
	if (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_VLAN_TYPE))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying vlan_type is not supported");
	if (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_GENEVE_VNI))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying Geneve VNI is not supported");
	/* Due to HW bug, tunnel MPLS header is read only. */
	if (action_conf->dst.field == RTE_FLOW_FIELD_MPLS)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"MPLS cannot be used as destination");
	return 0;
}

static int
flow_hw_validate_action_represented_port(struct rte_eth_dev *dev,
					 const struct rte_flow_action *action,
					 const struct rte_flow_action *mask,
					 struct rte_flow_error *error)
{
	const struct rte_flow_action_ethdev *action_conf = action->conf;
	const struct rte_flow_action_ethdev *mask_conf = mask->conf;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->sh->config.dv_esw_en)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot use represented_port actions"
					  " without an E-Switch");
	if (mask_conf && mask_conf->port_id) {
		struct mlx5_priv *port_priv;
		struct mlx5_priv *dev_priv;

		if (!action_conf)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						  action, "port index was not provided");
		port_priv = mlx5_port_to_eswitch_info(action_conf->port_id, false);
		if (!port_priv)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "failed to obtain E-Switch"
						  " info for port");
		dev_priv = mlx5_dev_to_eswitch_info(dev);
		if (!dev_priv)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "failed to obtain E-Switch"
						  " info for transfer proxy");
		if (port_priv->domain_id != dev_priv->domain_id)
			return rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "cannot forward to port from"
						  " a different E-Switch");
	}
	return 0;
}

/**
 * Validate AGE action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the indirect action.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[in] fixed_cnt
 *   Indicator if this list has a fixed COUNT action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_age(struct rte_eth_dev *dev,
			    const struct rte_flow_action *action,
			    uint64_t action_flags, bool fixed_cnt,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);

	if (!priv->sh->cdev->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "AGE action not supported");
	if (age_info->ages_ipool == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "aging pool not initialized");
	if ((action_flags & MLX5_FLOW_ACTION_AGE) ||
	    (action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "duplicate AGE actions set");
	if (fixed_cnt)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "AGE and fixed COUNT combination is not supported");
	return 0;
}

/**
 * Validate count action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the indirect action.
 * @param[in] mask
 *   Pointer to the indirect action mask.
 * @param[in] action_flags
 *   Holds the actions detected until now.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_count(struct rte_eth_dev *dev,
			      const struct rte_flow_action *action,
			      const struct rte_flow_action *mask,
			      uint64_t action_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_count *count = mask->conf;

	if (!priv->sh->cdev->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "count action not supported");
	if (!priv->hws_cpool)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "counters pool not initialized");
	if ((action_flags & MLX5_FLOW_ACTION_COUNT) ||
	    (action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "duplicate count actions set");
	if (count && count->id && (action_flags & MLX5_FLOW_ACTION_AGE))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, mask,
					  "AGE and COUNT action shared by mask combination is not supported");
	return 0;
}

/**
 * Validate meter_mark action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the indirect action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_meter_mark(struct rte_eth_dev *dev,
			      const struct rte_flow_action *action,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	RTE_SET_USED(action);

	if (!priv->sh->cdev->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "meter_mark action not supported");
	if (!priv->hws_mpool)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "meter_mark pool not initialized");
	return 0;
}

/**
 * Validate indirect action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the indirect action.
 * @param[in] mask
 *   Pointer to the indirect action mask.
 * @param[in, out] action_flags
 *   Holds the actions detected until now.
 * @param[in, out] fixed_cnt
 *   Pointer to indicator if this list has a fixed COUNT action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_indirect(struct rte_eth_dev *dev,
				 const struct rte_flow_action *action,
				 const struct rte_flow_action *mask,
				 uint64_t *action_flags, bool *fixed_cnt,
				 struct rte_flow_error *error)
{
	uint32_t type;
	int ret;

	if (!mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Unable to determine indirect action type without a mask specified");
	type = mask->type;
	switch (type) {
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		ret = flow_hw_validate_action_meter_mark(dev, mask, error);
		if (ret < 0)
			return ret;
		*action_flags |= MLX5_FLOW_ACTION_METER;
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		/* TODO: Validation logic (same as flow_hw_actions_validate) */
		*action_flags |= MLX5_FLOW_ACTION_RSS;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		/* TODO: Validation logic (same as flow_hw_actions_validate) */
		*action_flags |= MLX5_FLOW_ACTION_CT;
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		if (action->conf && mask->conf) {
			if ((*action_flags & MLX5_FLOW_ACTION_AGE) ||
			    (*action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE))
				/*
				 * AGE cannot use indirect counter which is
				 * shared with enother flow rules.
				 */
				return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "AGE and fixed COUNT combination is not supported");
			*fixed_cnt = true;
		}
		ret = flow_hw_validate_action_count(dev, action, mask,
						    *action_flags, error);
		if (ret < 0)
			return ret;
		*action_flags |= MLX5_FLOW_ACTION_INDIRECT_COUNT;
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
		ret = flow_hw_validate_action_age(dev, action, *action_flags,
						  *fixed_cnt, error);
		if (ret < 0)
			return ret;
		*action_flags |= MLX5_FLOW_ACTION_INDIRECT_AGE;
		break;
	case RTE_FLOW_ACTION_TYPE_QUOTA:
		/* TODO: add proper quota verification */
		*action_flags |= MLX5_FLOW_ACTION_QUOTA;
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type: %d", type);
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, mask,
					  "Unsupported indirect action type");
	}
	return 0;
}

/**
 * Validate raw_encap action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] action
 *   Pointer to the indirect action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_raw_encap(struct rte_eth_dev *dev __rte_unused,
				  const struct rte_flow_action *action,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_raw_encap *raw_encap_data = action->conf;

	if (!raw_encap_data || !raw_encap_data->size || !raw_encap_data->data)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "invalid raw_encap_data");
	return 0;
}

static inline uint16_t
flow_hw_template_expand_modify_field(struct rte_flow_action actions[],
				     struct rte_flow_action masks[],
				     const struct rte_flow_action *mf_actions,
				     const struct rte_flow_action *mf_masks,
				     uint64_t flags, uint32_t act_num,
				     uint32_t mf_num)
{
	uint32_t i, tail;

	MLX5_ASSERT(actions && masks);
	MLX5_ASSERT(mf_num > 0);
	if (flags & MLX5_FLOW_ACTION_MODIFY_FIELD) {
		/*
		 * Application action template already has Modify Field.
		 * It's location will be used in DR.
		 * Expanded MF action can be added before the END.
		 */
		i = act_num - 1;
		goto insert;
	}
	/**
	 * Locate the first action positioned BEFORE the new MF.
	 *
	 * Search for a place to insert modify header
	 * from the END action backwards:
	 * 1. END is always present in actions array
	 * 2. END location is always at action[act_num - 1]
	 * 3. END always positioned AFTER modify field location
	 *
	 * Relative actions order is the same for RX, TX and FDB.
	 *
	 * Current actions order (draft-3)
	 * @see action_order_arr[]
	 */
	for (i = act_num - 2; (int)i >= 0; i--) {
		enum rte_flow_action_type type = actions[i].type;

		if (type == RTE_FLOW_ACTION_TYPE_INDIRECT)
			type = masks[i].type;
		switch (type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		case RTE_FLOW_ACTION_TYPE_DROP:
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
		case RTE_FLOW_ACTION_TYPE_JUMP:
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		case RTE_FLOW_ACTION_TYPE_VOID:
		case RTE_FLOW_ACTION_TYPE_END:
			break;
		default:
			i++; /* new MF inserted AFTER actions[i] */
			goto insert;
			break;
		}
	}
	i = 0;
insert:
	tail = act_num - i; /* num action to move */
	memmove(actions + i + mf_num, actions + i, sizeof(actions[0]) * tail);
	memcpy(actions + i, mf_actions, sizeof(actions[0]) * mf_num);
	memmove(masks + i + mf_num, masks + i, sizeof(masks[0]) * tail);
	memcpy(masks + i, mf_masks, sizeof(masks[0]) * mf_num);
	return i;
}

static int
flow_hw_validate_action_push_vlan(struct rte_eth_dev *dev,
				  const
				  struct rte_flow_actions_template_attr *attr,
				  const struct rte_flow_action *action,
				  const struct rte_flow_action *mask,
				  struct rte_flow_error *error)
{
#define X_FIELD(ptr, t, f) (((ptr)->conf) && ((t *)((ptr)->conf))->f)

	const bool masked_push =
		X_FIELD(mask + MLX5_HW_VLAN_PUSH_TYPE_IDX,
			const struct rte_flow_action_of_push_vlan, ethertype);
	bool masked_param;

	/*
	 * Mandatory actions order:
	 * OF_PUSH_VLAN / OF_SET_VLAN_VID [ / OF_SET_VLAN_PCP ]
	 */
	RTE_SET_USED(dev);
	RTE_SET_USED(attr);
	/* Check that mark matches OF_PUSH_VLAN */
	if (mask[MLX5_HW_VLAN_PUSH_TYPE_IDX].type !=
	    RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "OF_PUSH_VLAN: mask does not match");
	/* Check that the second template and mask items are SET_VLAN_VID */
	if (action[MLX5_HW_VLAN_PUSH_VID_IDX].type !=
	    RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID ||
	    mask[MLX5_HW_VLAN_PUSH_VID_IDX].type !=
	    RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "OF_PUSH_VLAN: invalid actions order");
	masked_param = X_FIELD(mask + MLX5_HW_VLAN_PUSH_VID_IDX,
			       const struct rte_flow_action_of_set_vlan_vid,
			       vlan_vid);
	/*
	 * PMD requires OF_SET_VLAN_VID mask to must match OF_PUSH_VLAN
	 */
	if (masked_push ^ masked_param)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "OF_SET_VLAN_VID: mask does not match OF_PUSH_VLAN");
	if (is_of_vlan_pcp_present(action)) {
		if (mask[MLX5_HW_VLAN_PUSH_PCP_IDX].type !=
		     RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action, "OF_SET_VLAN_PCP: missing mask configuration");
		masked_param = X_FIELD(mask + MLX5_HW_VLAN_PUSH_PCP_IDX,
				       const struct
				       rte_flow_action_of_set_vlan_pcp,
				       vlan_pcp);
		/*
		 * PMD requires OF_SET_VLAN_PCP mask to must match OF_PUSH_VLAN
		 */
		if (masked_push ^ masked_param)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION, action,
						  "OF_SET_VLAN_PCP: mask does not match OF_PUSH_VLAN");
	}
	return 0;
#undef X_FIELD
}

static int
mlx5_flow_hw_actions_validate(struct rte_eth_dev *dev,
			      const struct rte_flow_actions_template_attr *attr,
			      const struct rte_flow_action actions[],
			      const struct rte_flow_action masks[],
			      uint64_t *act_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_count *count_mask = NULL;
	bool fixed_cnt = false;
	uint64_t action_flags = 0;
	uint16_t i;
	bool actions_end = false;
	int ret;

	/* FDB actions are only valid to proxy port. */
	if (attr->transfer && (!priv->sh->config.dv_esw_en || !priv->master))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "transfer actions are only valid to proxy port");
	for (i = 0; !actions_end; ++i) {
		const struct rte_flow_action *action = &actions[i];
		const struct rte_flow_action *mask = &masks[i];

		MLX5_ASSERT(i < MLX5_HW_MAX_ACTS);
		if (action->type != RTE_FLOW_ACTION_TYPE_INDIRECT &&
		    action->type != mask->type)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "mask type does not match action type");
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			ret = flow_hw_validate_action_indirect(dev, action,
							       mask,
							       &action_flags,
							       &fixed_cnt,
							       error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			if (priv->shared_host)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  action,
							  "action not supported in guest port");
			if (!priv->hw_send_to_kernel)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  action,
							  "action is not available");
			action_flags |= MLX5_FLOW_ACTION_SEND_TO_KERNEL;
			break;
#endif
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_RSS;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			ret = flow_hw_validate_action_raw_encap(dev, action, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_METER;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			ret = flow_hw_validate_action_meter_mark(dev, action,
								 error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_METER;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			ret = flow_hw_validate_action_modify_field(action, mask,
								   error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			ret = flow_hw_validate_action_represented_port
					(dev, action, mask, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			if (count_mask && count_mask->id)
				fixed_cnt = true;
			ret = flow_hw_validate_action_age(dev, action,
							  action_flags,
							  fixed_cnt, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_AGE;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_hw_validate_action_count(dev, action, mask,
							    action_flags,
							    error);
			if (ret < 0)
				return ret;
			count_mask = mask->conf;
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_CT;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			action_flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			ret = flow_hw_validate_action_push_vlan
					(dev, attr, action, mask, error);
			if (ret != 0)
				return ret;
			i += is_of_vlan_pcp_present(action) ?
				MLX5_HW_VLAN_PUSH_PCP_IDX :
				MLX5_HW_VLAN_PUSH_VID_IDX;
			action_flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  action,
						  "action not supported in template API");
		}
	}
	if (act_flags != NULL)
		*act_flags = action_flags;
	return 0;
}

static int
flow_hw_actions_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_actions_template_attr *attr,
			 const struct rte_flow_action actions[],
			 const struct rte_flow_action masks[],
			 struct rte_flow_error *error)
{
	return mlx5_flow_hw_actions_validate(dev, attr, actions, masks, NULL,
					     error);
}


static enum mlx5dr_action_type mlx5_hw_dr_action_types[] = {
	[RTE_FLOW_ACTION_TYPE_MARK] = MLX5DR_ACTION_TYP_TAG,
	[RTE_FLOW_ACTION_TYPE_DROP] = MLX5DR_ACTION_TYP_DROP,
	[RTE_FLOW_ACTION_TYPE_JUMP] = MLX5DR_ACTION_TYP_FT,
	[RTE_FLOW_ACTION_TYPE_QUEUE] = MLX5DR_ACTION_TYP_TIR,
	[RTE_FLOW_ACTION_TYPE_RSS] = MLX5DR_ACTION_TYP_TIR,
	[RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP] = MLX5DR_ACTION_TYP_L2_TO_TNL_L2,
	[RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP] = MLX5DR_ACTION_TYP_L2_TO_TNL_L2,
	[RTE_FLOW_ACTION_TYPE_VXLAN_DECAP] = MLX5DR_ACTION_TYP_TNL_L2_TO_L2,
	[RTE_FLOW_ACTION_TYPE_NVGRE_DECAP] = MLX5DR_ACTION_TYP_TNL_L2_TO_L2,
	[RTE_FLOW_ACTION_TYPE_MODIFY_FIELD] = MLX5DR_ACTION_TYP_MODIFY_HDR,
	[RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT] = MLX5DR_ACTION_TYP_VPORT,
	[RTE_FLOW_ACTION_TYPE_CONNTRACK] = MLX5DR_ACTION_TYP_ASO_CT,
	[RTE_FLOW_ACTION_TYPE_OF_POP_VLAN] = MLX5DR_ACTION_TYP_POP_VLAN,
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN] = MLX5DR_ACTION_TYP_PUSH_VLAN,
	[RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL] = MLX5DR_ACTION_TYP_DEST_ROOT,
};

static int
flow_hw_dr_actions_template_handle_shared(const struct rte_flow_action *mask,
					  unsigned int action_src,
					  enum mlx5dr_action_type *action_types,
					  uint16_t *curr_off, uint16_t *cnt_off,
					  struct rte_flow_actions_template *at)
{
	uint32_t type;

	if (!mask) {
		DRV_LOG(WARNING, "Unable to determine indirect action type "
			"without a mask specified");
		return -EINVAL;
	}
	type = mask->type;
	switch (type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
		at->actions_off[action_src] = *curr_off;
		action_types[*curr_off] = MLX5DR_ACTION_TYP_TIR;
		*curr_off = *curr_off + 1;
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
	case RTE_FLOW_ACTION_TYPE_COUNT:
		/*
		 * Both AGE and COUNT action need counter, the first one fills
		 * the action_types array, and the second only saves the offset.
		 */
		if (*cnt_off == UINT16_MAX) {
			*cnt_off = *curr_off;
			action_types[*cnt_off] = MLX5DR_ACTION_TYP_CTR;
			*curr_off = *curr_off + 1;
		}
		at->actions_off[action_src] = *cnt_off;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		at->actions_off[action_src] = *curr_off;
		action_types[*curr_off] = MLX5DR_ACTION_TYP_ASO_CT;
		*curr_off = *curr_off + 1;
		break;
	case RTE_FLOW_ACTION_TYPE_QUOTA:
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		at->actions_off[action_src] = *curr_off;
		action_types[*curr_off] = MLX5DR_ACTION_TYP_ASO_METER;
		*curr_off = *curr_off + 1;
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type: %d", type);
		return -EINVAL;
	}
	return 0;
}

/**
 * Create DR action template based on a provided sequence of flow actions.
 *
 * @param[in] at
 *   Pointer to flow actions template to be updated.
 *
 * @return
 *   DR action template pointer on success and action offsets in @p at are updated.
 *   NULL otherwise.
 */
static struct mlx5dr_action_template *
flow_hw_dr_actions_template_create(struct rte_flow_actions_template *at)
{
	struct mlx5dr_action_template *dr_template;
	enum mlx5dr_action_type action_types[MLX5_HW_MAX_ACTS] = { MLX5DR_ACTION_TYP_LAST };
	unsigned int i;
	uint16_t curr_off;
	enum mlx5dr_action_type reformat_act_type = MLX5DR_ACTION_TYP_TNL_L2_TO_L2;
	uint16_t reformat_off = UINT16_MAX;
	uint16_t mhdr_off = UINT16_MAX;
	uint16_t cnt_off = UINT16_MAX;
	int ret;
	for (i = 0, curr_off = 0; at->actions[i].type != RTE_FLOW_ACTION_TYPE_END; ++i) {
		const struct rte_flow_action_raw_encap *raw_encap_data;
		size_t data_size;
		enum mlx5dr_action_type type;

		if (curr_off >= MLX5_HW_MAX_ACTS)
			goto err_actions_num;
		switch (at->actions[i].type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			ret = flow_hw_dr_actions_template_handle_shared
								 (&at->masks[i],
								  i,
								  action_types,
								  &curr_off,
								  &cnt_off, at);
			if (ret)
				return NULL;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			MLX5_ASSERT(reformat_off == UINT16_MAX);
			reformat_off = curr_off++;
			reformat_act_type = mlx5_hw_dr_action_types[at->actions[i].type];
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap_data = at->actions[i].conf;
			data_size = raw_encap_data->size;
			if (reformat_off != UINT16_MAX) {
				reformat_act_type = data_size < MLX5_ENCAPSULATION_DECISION_SIZE ?
					MLX5DR_ACTION_TYP_TNL_L3_TO_L2 :
					MLX5DR_ACTION_TYP_L2_TO_TNL_L3;
			} else {
				reformat_off = curr_off++;
				reformat_act_type = MLX5DR_ACTION_TYP_L2_TO_TNL_L2;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			reformat_off = curr_off++;
			reformat_act_type = MLX5DR_ACTION_TYP_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			if (mhdr_off == UINT16_MAX) {
				mhdr_off = curr_off++;
				type = mlx5_hw_dr_action_types[at->actions[i].type];
				action_types[mhdr_off] = type;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			at->actions_off[i] = curr_off;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_ASO_METER;
			if (curr_off >= MLX5_HW_MAX_ACTS)
				goto err_actions_num;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_FT;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			type = mlx5_hw_dr_action_types[at->actions[i].type];
			at->actions_off[i] = curr_off;
			action_types[curr_off++] = type;
			i += is_of_vlan_pcp_present(at->actions + i) ?
				MLX5_HW_VLAN_PUSH_PCP_IDX :
				MLX5_HW_VLAN_PUSH_VID_IDX;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			at->actions_off[i] = curr_off;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_ASO_METER;
			if (curr_off >= MLX5_HW_MAX_ACTS)
				goto err_actions_num;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
		case RTE_FLOW_ACTION_TYPE_COUNT:
			/*
			 * Both AGE and COUNT action need counter, the first
			 * one fills the action_types array, and the second only
			 * saves the offset.
			 */
			if (cnt_off == UINT16_MAX) {
				cnt_off = curr_off++;
				action_types[cnt_off] = MLX5DR_ACTION_TYP_CTR;
			}
			at->actions_off[i] = cnt_off;
			break;
		default:
			type = mlx5_hw_dr_action_types[at->actions[i].type];
			at->actions_off[i] = curr_off;
			action_types[curr_off++] = type;
			break;
		}
	}
	if (curr_off >= MLX5_HW_MAX_ACTS)
		goto err_actions_num;
	if (mhdr_off != UINT16_MAX)
		at->mhdr_off = mhdr_off;
	if (reformat_off != UINT16_MAX) {
		at->reformat_off = reformat_off;
		action_types[reformat_off] = reformat_act_type;
	}
	dr_template = mlx5dr_action_template_create(action_types);
	if (dr_template)
		at->dr_actions_num = curr_off;
	else
		DRV_LOG(ERR, "Failed to create DR action template: %d", rte_errno);
	return dr_template;
err_actions_num:
	DRV_LOG(ERR, "Number of HW actions (%u) exceeded maximum (%u) allowed in template",
		curr_off, MLX5_HW_MAX_ACTS);
	return NULL;
}

static void
flow_hw_set_vlan_vid(struct rte_eth_dev *dev,
		     struct rte_flow_action *ra,
		     struct rte_flow_action *rm,
		     struct rte_flow_action_modify_field *spec,
		     struct rte_flow_action_modify_field *mask,
		     int set_vlan_vid_ix)
{
	struct rte_flow_error error;
	const bool masked = rm[set_vlan_vid_ix].conf &&
		(((const struct rte_flow_action_of_set_vlan_vid *)
			rm[set_vlan_vid_ix].conf)->vlan_vid != 0);
	const struct rte_flow_action_of_set_vlan_vid *conf =
		ra[set_vlan_vid_ix].conf;
	rte_be16_t vid = masked ? conf->vlan_vid : 0;
	int width = mlx5_flow_item_field_width(dev, RTE_FLOW_FIELD_VLAN_ID, 0,
					       NULL, &error);
	*spec = (typeof(*spec)) {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = RTE_FLOW_FIELD_VLAN_ID,
			.level = 0, .offset = 0,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
			.level = vid,
			.offset = 0,
		},
		.width = width,
	};
	*mask = (typeof(*mask)) {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = RTE_FLOW_FIELD_VLAN_ID,
			.level = 0xff, .offset = 0xffffffff,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
			.level = masked ? (1U << width) - 1 : 0,
			.offset = 0,
		},
		.width = 0xffffffff,
	};
	ra[set_vlan_vid_ix].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	ra[set_vlan_vid_ix].conf = spec;
	rm[set_vlan_vid_ix].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	rm[set_vlan_vid_ix].conf = mask;
}

static __rte_always_inline int
flow_hw_set_vlan_vid_construct(struct rte_eth_dev *dev,
			       struct mlx5_hw_q_job *job,
			       struct mlx5_action_construct_data *act_data,
			       const struct mlx5_hw_actions *hw_acts,
			       const struct rte_flow_action *action)
{
	struct rte_flow_error error;
	rte_be16_t vid = ((const struct rte_flow_action_of_set_vlan_vid *)
			   action->conf)->vlan_vid;
	int width = mlx5_flow_item_field_width(dev, RTE_FLOW_FIELD_VLAN_ID, 0,
					       NULL, &error);
	struct rte_flow_action_modify_field conf = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = RTE_FLOW_FIELD_VLAN_ID,
			.level = 0, .offset = 0,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
			.level = vid,
			.offset = 0,
		},
		.width = width,
	};
	struct rte_flow_action modify_action = {
		.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
		.conf = &conf
	};

	return flow_hw_modify_field_construct(job, act_data, hw_acts,
					      &modify_action);
}

static int
flow_hw_flex_item_acquire(struct rte_eth_dev *dev,
			  struct rte_flow_item_flex_handle *handle,
			  uint8_t *flex_item)
{
	int index = mlx5_flex_acquire_index(dev, handle, false);

	MLX5_ASSERT(index >= 0 && index < (int)(sizeof(uint32_t) * CHAR_BIT));
	if (index < 0)
		return -1;
	if (!(*flex_item & RTE_BIT32(index))) {
		/* Don't count same flex item again. */
		if (mlx5_flex_acquire_index(dev, handle, true) != index)
			MLX5_ASSERT(false);
		*flex_item |= (uint8_t)RTE_BIT32(index);
	}
	return 0;
}

static void
flow_hw_flex_item_release(struct rte_eth_dev *dev, uint8_t *flex_item)
{
	while (*flex_item) {
		int index = rte_bsf32(*flex_item);

		mlx5_flex_release_index(dev, index);
		*flex_item &= ~(uint8_t)RTE_BIT32(index);
	}
}
static __rte_always_inline void
flow_hw_actions_template_replace_container(const
					   struct rte_flow_action *actions,
					   const
					   struct rte_flow_action *masks,
					   struct rte_flow_action *new_actions,
					   struct rte_flow_action *new_masks,
					   struct rte_flow_action **ra,
					   struct rte_flow_action **rm,
					   uint32_t act_num)
{
	memcpy(new_actions, actions, sizeof(actions[0]) * act_num);
	memcpy(new_masks, masks, sizeof(masks[0]) * act_num);
	*ra = (void *)(uintptr_t)new_actions;
	*rm = (void *)(uintptr_t)new_masks;
}

/* Action template copies these actions in rte_flow_conv() */

static const struct rte_flow_action rx_meta_copy_action =  {
	.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
	.conf = &(struct rte_flow_action_modify_field){
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)
				MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_B,
		},
		.src = {
			.field = (enum rte_flow_field_id)
				MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_1,
		},
		.width = 32,
	}
};

static const struct rte_flow_action rx_meta_copy_mask = {
	.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
	.conf = &(struct rte_flow_action_modify_field){
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)
				MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = (enum rte_flow_field_id)
				MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.width = UINT32_MAX,
	}
};

static const struct rte_flow_action quota_color_inc_action = {
	.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
	.conf = &(struct rte_flow_action_modify_field) {
		.operation = RTE_FLOW_MODIFY_ADD,
		.dst = {
			.field = RTE_FLOW_FIELD_METER_COLOR,
			.level = 0, .offset = 0
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
			.level = 1,
			.offset = 0,
		},
		.width = 2
	}
};

static const struct rte_flow_action quota_color_inc_mask = {
	.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
	.conf = &(struct rte_flow_action_modify_field) {
		.operation = RTE_FLOW_MODIFY_ADD,
		.dst = {
			.field = RTE_FLOW_FIELD_METER_COLOR,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
			.level = 3,
			.offset = 0
		},
		.width = UINT32_MAX
	}
};

/**
 * Create flow action template.
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
 *   Action template pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_actions_template *
flow_hw_actions_template_create(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int len, act_len, mask_len;
	unsigned int act_num;
	unsigned int i;
	struct rte_flow_actions_template *at = NULL;
	uint16_t pos = UINT16_MAX;
	uint64_t action_flags = 0;
	struct rte_flow_action tmp_action[MLX5_HW_MAX_ACTS];
	struct rte_flow_action tmp_mask[MLX5_HW_MAX_ACTS];
	struct rte_flow_action *ra = (void *)(uintptr_t)actions;
	struct rte_flow_action *rm = (void *)(uintptr_t)masks;
	int set_vlan_vid_ix = -1;
	struct rte_flow_action_modify_field set_vlan_vid_spec = {0, };
	struct rte_flow_action_modify_field set_vlan_vid_mask = {0, };
	struct rte_flow_action mf_actions[MLX5_HW_MAX_ACTS];
	struct rte_flow_action mf_masks[MLX5_HW_MAX_ACTS];
	uint32_t expand_mf_num = 0;

	if (mlx5_flow_hw_actions_validate(dev, attr, actions, masks,
					  &action_flags, error))
		return NULL;
	for (i = 0; ra[i].type != RTE_FLOW_ACTION_TYPE_END; ++i) {
		switch (ra[i].type) {
		/* OF_PUSH_VLAN *MUST* come before OF_SET_VLAN_VID */
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			i += is_of_vlan_pcp_present(ra + i) ?
				MLX5_HW_VLAN_PUSH_PCP_IDX :
				MLX5_HW_VLAN_PUSH_VID_IDX;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			set_vlan_vid_ix = i;
			break;
		default:
			break;
		}
	}
	/*
	 * Count flow actions to allocate required space for storing DR offsets and to check
	 * if temporary buffer would not be overrun.
	 */
	act_num = i + 1;
	if (act_num >= MLX5_HW_MAX_ACTS) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, NULL, "Too many actions");
		return NULL;
	}
	if (set_vlan_vid_ix != -1) {
		/* If temporary action buffer was not used, copy template actions to it */
		if (ra == actions)
			flow_hw_actions_template_replace_container(actions,
								   masks,
								   tmp_action,
								   tmp_mask,
								   &ra, &rm,
								   act_num);
		flow_hw_set_vlan_vid(dev, ra, rm,
				     &set_vlan_vid_spec, &set_vlan_vid_mask,
				     set_vlan_vid_ix);
		action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
	}
	if (action_flags & MLX5_FLOW_ACTION_QUOTA) {
		mf_actions[expand_mf_num] = quota_color_inc_action;
		mf_masks[expand_mf_num] = quota_color_inc_mask;
		expand_mf_num++;
	}
	if (priv->sh->config.dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS &&
	    priv->sh->config.dv_esw_en &&
	    (action_flags & (MLX5_FLOW_ACTION_QUEUE | MLX5_FLOW_ACTION_RSS))) {
		/* Insert META copy */
		mf_actions[expand_mf_num] = rx_meta_copy_action;
		mf_masks[expand_mf_num] = rx_meta_copy_mask;
		expand_mf_num++;
	}
	if (expand_mf_num) {
		if (act_num + expand_mf_num > MLX5_HW_MAX_ACTS) {
			rte_flow_error_set(error, E2BIG,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   NULL, "cannot expand: too many actions");
			return NULL;
		}
		if (ra == actions)
			flow_hw_actions_template_replace_container(actions,
								   masks,
								   tmp_action,
								   tmp_mask,
								   &ra, &rm,
								   act_num);
		/* Application should make sure only one Q/RSS exist in one rule. */
		pos = flow_hw_template_expand_modify_field(ra, rm,
							   mf_actions,
							   mf_masks,
							   action_flags,
							   act_num,
							   expand_mf_num);
		act_num += expand_mf_num;
		action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
	}
	act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, ra, error);
	if (act_len <= 0)
		return NULL;
	len = RTE_ALIGN(act_len, 16);
	mask_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, rm, error);
	if (mask_len <= 0)
		return NULL;
	len += RTE_ALIGN(mask_len, 16);
	len += RTE_ALIGN(act_num * sizeof(*at->actions_off), 16);
	at = mlx5_malloc(MLX5_MEM_ZERO, len + sizeof(*at),
			 RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (!at) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate action template");
		return NULL;
	}
	/* Actions part is in the first part. */
	at->attr = *attr;
	at->actions = (struct rte_flow_action *)(at + 1);
	act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, at->actions,
				len, ra, error);
	if (act_len <= 0)
		goto error;
	/* Masks part is in the second part. */
	at->masks = (struct rte_flow_action *)(((uint8_t *)at->actions) + act_len);
	mask_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, at->masks,
				 len - act_len, rm, error);
	if (mask_len <= 0)
		goto error;
	/* DR actions offsets in the third part. */
	at->actions_off = (uint16_t *)((uint8_t *)at->masks + mask_len);
	at->actions_num = act_num;
	for (i = 0; i < at->actions_num; ++i)
		at->actions_off[i] = UINT16_MAX;
	at->reformat_off = UINT16_MAX;
	at->mhdr_off = UINT16_MAX;
	at->rx_cpy_pos = pos;
	/*
	 * mlx5 PMD hacks indirect action index directly to the action conf.
	 * The rte_flow_conv() function copies the content from conf pointer.
	 * Need to restore the indirect action index from action conf here.
	 */
	for (i = 0; actions->type != RTE_FLOW_ACTION_TYPE_END;
	     actions++, masks++, i++) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_INDIRECT) {
			at->actions[i].conf = actions->conf;
			at->masks[i].conf = masks->conf;
		}
		if (actions->type == RTE_FLOW_ACTION_TYPE_MODIFY_FIELD) {
			const struct rte_flow_action_modify_field *info = actions->conf;

			if ((info->dst.field == RTE_FLOW_FIELD_FLEX_ITEM &&
			     flow_hw_flex_item_acquire(dev, info->dst.flex_handle,
						       &at->flex_item)) ||
			     (info->src.field == RTE_FLOW_FIELD_FLEX_ITEM &&
			      flow_hw_flex_item_acquire(dev, info->src.flex_handle,
							&at->flex_item))) {
				rte_flow_error_set(error, rte_errno,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						   "Failed to acquire flex item");
				goto error;
			}
		}
	}
	at->tmpl = flow_hw_dr_actions_template_create(at);
	if (!at->tmpl)
		goto error;
	at->action_flags = action_flags;
	__atomic_fetch_add(&at->refcnt, 1, __ATOMIC_RELAXED);
	LIST_INSERT_HEAD(&priv->flow_hw_at, at, next);
	return at;
error:
	if (at) {
		if (at->tmpl)
			mlx5dr_action_template_destroy(at->tmpl);
		mlx5_free(at);
	}
	rte_flow_error_set(error, rte_errno,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Failed to create action template");
	return NULL;
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
flow_hw_actions_template_destroy(struct rte_eth_dev *dev,
				 struct rte_flow_actions_template *template,
				 struct rte_flow_error *error __rte_unused)
{
	if (__atomic_load_n(&template->refcnt, __ATOMIC_RELAXED) > 1) {
		DRV_LOG(WARNING, "Action template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "action template in using");
	}
	LIST_REMOVE(template, next);
	flow_hw_flex_item_release(dev, &template->flex_item);
	if (template->tmpl)
		mlx5dr_action_template_destroy(template->tmpl);
	mlx5_free(template);
	return 0;
}

static uint32_t
flow_hw_count_items(const struct rte_flow_item *items)
{
	const struct rte_flow_item *curr_item;
	uint32_t nb_items;

	nb_items = 0;
	for (curr_item = items; curr_item->type != RTE_FLOW_ITEM_TYPE_END; ++curr_item)
		++nb_items;
	return ++nb_items;
}

static struct rte_flow_item *
flow_hw_prepend_item(const struct rte_flow_item *items,
		     const uint32_t nb_items,
		     const struct rte_flow_item *new_item,
		     struct rte_flow_error *error)
{
	struct rte_flow_item *copied_items;
	size_t size;

	/* Allocate new array of items. */
	size = sizeof(*copied_items) * (nb_items + 1);
	copied_items = mlx5_malloc(MLX5_MEM_ZERO, size, 0, rte_socket_id());
	if (!copied_items) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate item template");
		return NULL;
	}
	/* Put new item at the beginning and copy the rest. */
	copied_items[0] = *new_item;
	rte_memcpy(&copied_items[1], items, sizeof(*items) * nb_items);
	return copied_items;
}

static int
flow_hw_pattern_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_pattern_template_attr *attr,
			 const struct rte_flow_item items[],
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int i, tag_idx;
	bool items_end = false;
	uint32_t tag_bitmap = 0;

	if (!attr->ingress && !attr->egress && !attr->transfer)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "at least one of the direction attributes"
					  " must be specified");
	if (priv->sh->config.dv_esw_en) {
		MLX5_ASSERT(priv->master || priv->representor);
		if (priv->master) {
			if ((attr->ingress && attr->egress) ||
			    (attr->ingress && attr->transfer) ||
			    (attr->egress && attr->transfer))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
							  "only one direction attribute at once"
							  " can be used on transfer proxy port");
		} else {
			if (attr->transfer)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, NULL,
							  "transfer attribute cannot be used with"
							  " port representors");
			if (attr->ingress && attr->egress)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
							  "ingress and egress direction attributes"
							  " cannot be used at the same time on"
							  " port representors");
		}
	} else {
		if (attr->transfer)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ATTR_TRANSFER, NULL,
						  "transfer attribute cannot be used when"
						  " E-Switch is disabled");
	}
	for (i = 0; !items_end; i++) {
		int type = items[i].type;

		switch (type) {
		case RTE_FLOW_ITEM_TYPE_TAG:
		{
			const struct rte_flow_item_tag *tag =
				(const struct rte_flow_item_tag *)items[i].spec;

			if (tag == NULL)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Tag spec is NULL");
			tag_idx = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_TAG, tag->index);
			if (tag_idx == REG_NON)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Unsupported tag index");
			if (tag_bitmap & (1 << tag_idx))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM,
							  NULL,
							  "Duplicated tag index");
			tag_bitmap |= 1 << tag_idx;
			break;
		}
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		{
			const struct rte_flow_item_tag *tag =
				(const struct rte_flow_item_tag *)items[i].spec;
			uint8_t regcs = (uint8_t)priv->sh->cdev->config.hca_attr.set_reg_c;

			if (!((1 << (tag->index - REG_C_0)) & regcs))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Unsupported internal tag index");
			if (tag_bitmap & (1 << tag->index))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM,
							  NULL,
							  "Duplicated tag index");
			tag_bitmap |= 1 << tag->index;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
			if (attr->ingress && priv->sh->config.repr_matching)
				return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
						  "represented port item cannot be used"
						  " when ingress attribute is set");
			if (attr->egress)
				return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
						  "represented port item cannot be used"
						  " when egress attribute is set");
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			if (!priv->sh->config.dv_esw_en ||
			    priv->sh->config.dv_xmeta_en != MLX5_XMETA_MODE_META32_HWS) {
				if (attr->ingress)
					return rte_flow_error_set(error, EINVAL,
								  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
								  "META item is not supported"
								  " on current FW with ingress"
								  " attribute");
			}
			break;
		case RTE_FLOW_ITEM_TYPE_METER_COLOR:
		{
			int reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_METER_COLOR, 0);
			if (reg == REG_NON)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Unsupported meter color register");
			break;
		}
		case RTE_FLOW_ITEM_TYPE_AGGR_AFFINITY:
		{
			if (!priv->sh->lag_rx_port_affinity_en)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
							  "Unsupported aggregated affinity with Older FW");
			if ((attr->transfer && priv->fdb_def_rule) || attr->egress)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM, NULL,
							  "Aggregated affinity item not supported"
							  " with egress or transfer"
							  " attribute");
			break;
		}
		case RTE_FLOW_ITEM_TYPE_VOID:
		case RTE_FLOW_ITEM_TYPE_ETH:
		case RTE_FLOW_ITEM_TYPE_VLAN:
		case RTE_FLOW_ITEM_TYPE_IPV4:
		case RTE_FLOW_ITEM_TYPE_IPV6:
		case RTE_FLOW_ITEM_TYPE_UDP:
		case RTE_FLOW_ITEM_TYPE_TCP:
		case RTE_FLOW_ITEM_TYPE_GTP:
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
		case RTE_FLOW_ITEM_TYPE_VXLAN:
		case RTE_FLOW_ITEM_TYPE_MPLS:
		case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
		case RTE_FLOW_ITEM_TYPE_GRE:
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
		case RTE_FLOW_ITEM_TYPE_ICMP:
		case RTE_FLOW_ITEM_TYPE_ICMP6:
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST:
		case RTE_FLOW_ITEM_TYPE_QUOTA:
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY:
		case RTE_FLOW_ITEM_TYPE_CONNTRACK:
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
		case RTE_FLOW_ITEM_TYPE_ESP:
		case RTE_FLOW_ITEM_TYPE_FLEX:
		case RTE_FLOW_ITEM_TYPE_IB_BTH:
			break;
		case RTE_FLOW_ITEM_TYPE_INTEGRITY:
			/*
			 * Integrity flow item validation require access to
			 * both item mask and spec.
			 * Current HWS model allows item mask in pattern
			 * template and item spec in flow rule.
			 */
			break;
		case RTE_FLOW_ITEM_TYPE_END:
			items_end = true;
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL,
						  "Unsupported item type");
		}
	}
	return 0;
}

static bool
flow_hw_pattern_has_sq_match(const struct rte_flow_item *items)
{
	unsigned int i;

	for (i = 0; items[i].type != RTE_FLOW_ITEM_TYPE_END; ++i)
		if (items[i].type == (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ)
			return true;
	return false;
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
 *  Item template pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_pattern_template *
flow_hw_pattern_template_create(struct rte_eth_dev *dev,
			     const struct rte_flow_pattern_template_attr *attr,
			     const struct rte_flow_item items[],
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_pattern_template *it;
	struct rte_flow_item *copied_items = NULL;
	const struct rte_flow_item *tmpl_items;
	uint64_t orig_item_nb;
	struct rte_flow_item port = {
		.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
		.mask = &rte_flow_item_ethdev_mask,
	};
	struct rte_flow_item_tag tag_v = {
		.data = 0,
		.index = REG_C_0,
	};
	struct rte_flow_item_tag tag_m = {
		.data = flow_hw_tx_tag_regc_mask(dev),
		.index = 0xff,
	};
	struct rte_flow_item tag = {
		.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG,
		.spec = &tag_v,
		.mask = &tag_m,
		.last = NULL
	};
	unsigned int i = 0;

	if (flow_hw_pattern_validate(dev, attr, items, error))
		return NULL;
	orig_item_nb = flow_hw_count_items(items);
	if (priv->sh->config.dv_esw_en &&
	    priv->sh->config.repr_matching &&
	    attr->ingress && !attr->egress && !attr->transfer) {
		copied_items = flow_hw_prepend_item(items, orig_item_nb, &port, error);
		if (!copied_items)
			return NULL;
		tmpl_items = copied_items;
	} else if (priv->sh->config.dv_esw_en &&
		   priv->sh->config.repr_matching &&
		   !attr->ingress && attr->egress && !attr->transfer) {
		if (flow_hw_pattern_has_sq_match(items)) {
			DRV_LOG(DEBUG, "Port %u omitting implicit REG_C_0 match for egress "
				       "pattern template", dev->data->port_id);
			tmpl_items = items;
			goto setup_pattern_template;
		}
		copied_items = flow_hw_prepend_item(items, orig_item_nb, &tag, error);
		if (!copied_items)
			return NULL;
		tmpl_items = copied_items;
	} else {
		tmpl_items = items;
	}
setup_pattern_template:
	it = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*it), 0, rte_socket_id());
	if (!it) {
		if (copied_items)
			mlx5_free(copied_items);
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate item template");
		return NULL;
	}
	it->attr = *attr;
	it->orig_item_nb = orig_item_nb;
	it->mt = mlx5dr_match_template_create(tmpl_items, attr->relaxed_matching);
	if (!it->mt) {
		if (copied_items)
			mlx5_free(copied_items);
		mlx5_free(it);
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot create match template");
		return NULL;
	}
	it->item_flags = flow_hw_matching_item_flags_get(tmpl_items);
	if (copied_items) {
		if (attr->ingress)
			it->implicit_port = true;
		else if (attr->egress)
			it->implicit_tag = true;
		mlx5_free(copied_items);
	}
	/* Either inner or outer, can't both. */
	if (it->item_flags & (MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT |
			      MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT)) {
		if (((it->item_flags & MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT) &&
		     (it->item_flags & MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT)) ||
		    (mlx5_alloc_srh_flex_parser(dev))) {
			claim_zero(mlx5dr_match_template_destroy(it->mt));
			mlx5_free(it);
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "cannot create IPv6 routing extension support");
			return NULL;
		}
	}
	for (i = 0; items[i].type != RTE_FLOW_ITEM_TYPE_END; ++i) {
		if (items[i].type == RTE_FLOW_ITEM_TYPE_FLEX) {
			const struct rte_flow_item_flex *spec =
				(const struct rte_flow_item_flex *)items[i].spec;
			struct rte_flow_item_flex_handle *handle = spec->handle;

			if (flow_hw_flex_item_acquire(dev, handle, &it->flex_item)) {
				claim_zero(mlx5dr_match_template_destroy(it->mt));
				mlx5_free(it);
				rte_flow_error_set(error, rte_errno,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						   "Failed to acquire flex item");
				return NULL;
			}
		}
	}
	__atomic_fetch_add(&it->refcnt, 1, __ATOMIC_RELAXED);
	LIST_INSERT_HEAD(&priv->flow_hw_itt, it, next);
	return it;
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
flow_hw_pattern_template_destroy(struct rte_eth_dev *dev,
			      struct rte_flow_pattern_template *template,
			      struct rte_flow_error *error __rte_unused)
{
	if (__atomic_load_n(&template->refcnt, __ATOMIC_RELAXED) > 1) {
		DRV_LOG(WARNING, "Item template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "item template in using");
	}
	if (template->item_flags & (MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT |
				    MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT))
		mlx5_free_srh_flex_parser(dev);
	LIST_REMOVE(template, next);
	flow_hw_flex_item_release(dev, &template->flex_item);
	claim_zero(mlx5dr_match_template_destroy(template->mt));
	mlx5_free(template);
	return 0;
}

/*
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
flow_hw_info_get(struct rte_eth_dev *dev,
		 struct rte_flow_port_info *port_info,
		 struct rte_flow_queue_info *queue_info,
		 struct rte_flow_error *error __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t port_id = dev->data->port_id;
	struct rte_mtr_capabilities mtr_cap;
	int ret;

	memset(port_info, 0, sizeof(*port_info));
	/* Queue size is unlimited from low-level. */
	port_info->max_nb_queues = UINT32_MAX;
	queue_info->max_size = UINT32_MAX;

	memset(&mtr_cap, 0, sizeof(struct rte_mtr_capabilities));
	ret = rte_mtr_capabilities_get(port_id, &mtr_cap, NULL);
	if (!ret)
		port_info->max_nb_meters = mtr_cap.n_max;
	port_info->max_nb_counters = priv->sh->hws_max_nb_counters;
	port_info->max_nb_aging_objects = port_info->max_nb_counters;
	return 0;
}

/**
 * Create group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] cb_ctx
 *   Pointer to the group creation context.
 *
 * @return
 *   Group entry on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_list_entry *
flow_hw_grp_create_cb(void *tool_ctx, void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct rte_eth_dev *dev = ctx->dev;
	struct rte_flow_attr *attr = (struct rte_flow_attr *)ctx->data;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_table_attr dr_tbl_attr = {0};
	struct rte_flow_error *error = ctx->error;
	struct mlx5_flow_group *grp_data;
	struct mlx5dr_table *tbl = NULL;
	struct mlx5dr_action *jump;
	uint32_t idx = 0;

	grp_data = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_HW_GRP], &idx);
	if (!grp_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	dr_tbl_attr.level = attr->group;
	if (attr->transfer)
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		dr_tbl_attr.type = MLX5DR_TABLE_TYPE_NIC_RX;
	tbl = mlx5dr_table_create(priv->dr_ctx, &dr_tbl_attr);
	if (!tbl)
		goto error;
	grp_data->tbl = tbl;
	if (attr->group) {
		/* Jump action be used by non-root table. */
		jump = mlx5dr_action_create_dest_table
			(priv->dr_ctx, tbl,
			 mlx5_hw_act_flag[!!attr->group][dr_tbl_attr.type]);
		if (!jump)
			goto error;
		grp_data->jump.hws_action = jump;
		/* Jump action be used by root table.  */
		jump = mlx5dr_action_create_dest_table
			(priv->dr_ctx, tbl,
			 mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_ROOT]
					 [dr_tbl_attr.type]);
		if (!jump)
			goto error;
		grp_data->jump.root_action = jump;
	}
	grp_data->dev = dev;
	grp_data->idx = idx;
	grp_data->group_id = attr->group;
	grp_data->type = dr_tbl_attr.type;
	return &grp_data->entry;
error:
	if (grp_data->jump.root_action)
		mlx5dr_action_destroy(grp_data->jump.root_action);
	if (grp_data->jump.hws_action)
		mlx5dr_action_destroy(grp_data->jump.hws_action);
	if (tbl)
		mlx5dr_table_destroy(tbl);
	if (idx)
		mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], idx);
	rte_flow_error_set(error, ENOMEM,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			   NULL,
			   "cannot allocate flow dr table");
	return NULL;
}

/**
 * Remove group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the entry to be removed.
 */
void
flow_hw_grp_remove_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_group *grp_data =
		    container_of(entry, struct mlx5_flow_group, entry);

	MLX5_ASSERT(entry && sh);
	/* To use the wrapper glue functions instead. */
	if (grp_data->jump.hws_action)
		mlx5dr_action_destroy(grp_data->jump.hws_action);
	if (grp_data->jump.root_action)
		mlx5dr_action_destroy(grp_data->jump.root_action);
	mlx5dr_table_destroy(grp_data->tbl);
	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], grp_data->idx);
}

/**
 * Match group callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be matched.
 * @param[in] cb_ctx
 *   Pointer to the group matching context.
 *
 * @return
 *   0 on matched, 1 on miss matched.
 */
int
flow_hw_grp_match_cb(void *tool_ctx __rte_unused, struct mlx5_list_entry *entry,
		     void *cb_ctx)
{
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_group *grp_data =
		container_of(entry, struct mlx5_flow_group, entry);
	struct rte_flow_attr *attr =
			(struct rte_flow_attr *)ctx->data;

	return (grp_data->dev != ctx->dev) ||
		(grp_data->group_id != attr->group) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_FDB) &&
		attr->transfer) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_NIC_TX) &&
		attr->egress) ||
		((grp_data->type != MLX5DR_TABLE_TYPE_NIC_RX) &&
		attr->ingress);
}

/**
 * Clone group entry callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be matched.
 * @param[in] cb_ctx
 *   Pointer to the group matching context.
 *
 * @return
 *   0 on matched, 1 on miss matched.
 */
struct mlx5_list_entry *
flow_hw_grp_clone_cb(void *tool_ctx, struct mlx5_list_entry *oentry,
		     void *cb_ctx)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_cb_ctx *ctx = cb_ctx;
	struct mlx5_flow_group *grp_data;
	struct rte_flow_error *error = ctx->error;
	uint32_t idx = 0;

	grp_data = mlx5_ipool_malloc(sh->ipool[MLX5_IPOOL_HW_GRP], &idx);
	if (!grp_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	memcpy(grp_data, oentry, sizeof(*grp_data));
	grp_data->idx = idx;
	return &grp_data->entry;
}

/**
 * Free cloned group entry callback.
 *
 * @param[in] tool_ctx
 *   Pointer to the hash list related context.
 * @param[in] entry
 *   Pointer to the group to be freed.
 */
void
flow_hw_grp_clone_free_cb(void *tool_ctx, struct mlx5_list_entry *entry)
{
	struct mlx5_dev_ctx_shared *sh = tool_ctx;
	struct mlx5_flow_group *grp_data =
		    container_of(entry, struct mlx5_flow_group, entry);

	mlx5_ipool_free(sh->ipool[MLX5_IPOOL_HW_GRP], grp_data->idx);
}

/**
 * Create and cache a vport action for given @p dev port. vport actions
 * cache is used in HWS with FDB flows.
 *
 * This function does not create any function if proxy port for @p dev port
 * was not configured for HW Steering.
 *
 * This function assumes that E-Switch is enabled and PMD is running with
 * HW Steering configured.
 *
 * @param dev
 *   Pointer to Ethernet device which will be the action destination.
 *
 * @return
 *   0 on success, positive value otherwise.
 */
int
flow_hw_create_vport_action(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	uint16_t port_id = dev->data->port_id;
	uint16_t proxy_port_id = port_id;
	int ret;

	ret = mlx5_flow_pick_transfer_proxy(dev, &proxy_port_id, NULL);
	if (ret)
		return ret;
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	if (!proxy_priv->hw_vport)
		return 0;
	if (proxy_priv->hw_vport[port_id]) {
		DRV_LOG(ERR, "port %u HWS vport action already created",
			port_id);
		return -EINVAL;
	}
	proxy_priv->hw_vport[port_id] = mlx5dr_action_create_dest_vport
			(proxy_priv->dr_ctx, priv->dev_port,
			 MLX5DR_ACTION_FLAG_HWS_FDB);
	if (!proxy_priv->hw_vport[port_id]) {
		DRV_LOG(ERR, "port %u unable to create HWS vport action",
			port_id);
		return -EINVAL;
	}
	return 0;
}

/**
 * Destroys the vport action associated with @p dev device
 * from actions' cache.
 *
 * This function does not destroy any action if there is no action cached
 * for @p dev or proxy port was not configured for HW Steering.
 *
 * This function assumes that E-Switch is enabled and PMD is running with
 * HW Steering configured.
 *
 * @param dev
 *   Pointer to Ethernet device which will be the action destination.
 */
void
flow_hw_destroy_vport_action(struct rte_eth_dev *dev)
{
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	uint16_t port_id = dev->data->port_id;
	uint16_t proxy_port_id = port_id;

	if (mlx5_flow_pick_transfer_proxy(dev, &proxy_port_id, NULL))
		return;
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	if (!proxy_priv->hw_vport || !proxy_priv->hw_vport[port_id])
		return;
	mlx5dr_action_destroy(proxy_priv->hw_vport[port_id]);
	proxy_priv->hw_vport[port_id] = NULL;
}

static int
flow_hw_create_vport_actions(struct mlx5_priv *priv)
{
	uint16_t port_id;

	MLX5_ASSERT(!priv->hw_vport);
	priv->hw_vport = mlx5_malloc(MLX5_MEM_ZERO,
				     sizeof(*priv->hw_vport) * RTE_MAX_ETHPORTS,
				     0, SOCKET_ID_ANY);
	if (!priv->hw_vport)
		return -ENOMEM;
	DRV_LOG(DEBUG, "port %u :: creating vport actions", priv->dev_data->port_id);
	DRV_LOG(DEBUG, "port %u ::    domain_id=%u", priv->dev_data->port_id, priv->domain_id);
	MLX5_ETH_FOREACH_DEV(port_id, NULL) {
		struct mlx5_priv *port_priv = rte_eth_devices[port_id].data->dev_private;

		if (!port_priv ||
		    port_priv->domain_id != priv->domain_id)
			continue;
		DRV_LOG(DEBUG, "port %u :: for port_id=%u, calling mlx5dr_action_create_dest_vport() with ibport=%u",
			priv->dev_data->port_id, port_id, port_priv->dev_port);
		priv->hw_vport[port_id] = mlx5dr_action_create_dest_vport
				(priv->dr_ctx, port_priv->dev_port,
				 MLX5DR_ACTION_FLAG_HWS_FDB);
		DRV_LOG(DEBUG, "port %u :: priv->hw_vport[%u]=%p",
			priv->dev_data->port_id, port_id, (void *)priv->hw_vport[port_id]);
		if (!priv->hw_vport[port_id])
			return -EINVAL;
	}
	return 0;
}

static void
flow_hw_free_vport_actions(struct mlx5_priv *priv)
{
	uint16_t port_id;

	if (!priv->hw_vport)
		return;
	for (port_id = 0; port_id < RTE_MAX_ETHPORTS; ++port_id)
		if (priv->hw_vport[port_id])
			mlx5dr_action_destroy(priv->hw_vport[port_id]);
	mlx5_free(priv->hw_vport);
	priv->hw_vport = NULL;
}

static void
flow_hw_create_send_to_kernel_actions(struct mlx5_priv *priv __rte_unused)
{
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
	priv->hw_send_to_kernel =
			mlx5dr_action_create_dest_root(priv->dr_ctx,
						       MLX5_HW_LOWEST_PRIO_ROOT,
						       MLX5DR_ACTION_FLAG_HWS_RX);
	if (!priv->hw_send_to_kernel) {
		DRV_LOG(WARNING, "Unable to create HWS send to kernel action");
		return;
	}
#endif
}

static void
flow_hw_destroy_send_to_kernel_action(struct mlx5_priv *priv)
{
	if (priv->hw_send_to_kernel) {
		mlx5dr_action_destroy(priv->hw_send_to_kernel);
		priv->hw_send_to_kernel = NULL;
	}
}

/**
 * Create an egress pattern template matching on source SQ.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to pattern template on success. NULL otherwise, and rte_errno is set.
 */
static struct rte_flow_pattern_template *
flow_hw_create_tx_repr_sq_pattern_tmpl(struct rte_eth_dev *dev)
{
	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 0,
		.egress = 1,
	};
	struct mlx5_rte_flow_item_sq sq_mask = {
		.queue = UINT32_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ,
			.mask = &sq_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};

	return flow_hw_pattern_template_create(dev, &attr, items, NULL);
}

static __rte_always_inline uint32_t
flow_hw_tx_tag_regc_mask(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t mask = priv->sh->dv_regc0_mask;

	/* Mask is verified during device initialization. Sanity checking here. */
	MLX5_ASSERT(mask != 0);
	/*
	 * Availability of sufficient number of bits in REG_C_0 is verified on initialization.
	 * Sanity checking here.
	 */
	MLX5_ASSERT(__builtin_popcount(mask) >= __builtin_popcount(priv->vport_meta_mask));
	return mask;
}

static __rte_always_inline uint32_t
flow_hw_tx_tag_regc_value(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t tag;

	/* Mask is verified during device initialization. Sanity checking here. */
	MLX5_ASSERT(priv->vport_meta_mask != 0);
	tag = priv->vport_meta_tag >> (rte_bsf32(priv->vport_meta_mask));
	/*
	 * Availability of sufficient number of bits in REG_C_0 is verified on initialization.
	 * Sanity checking here.
	 */
	MLX5_ASSERT((tag & priv->sh->dv_regc0_mask) == tag);
	return tag;
}

static void
flow_hw_update_action_mask(struct rte_flow_action *action,
			   struct rte_flow_action *mask,
			   enum rte_flow_action_type type,
			   void *conf_v,
			   void *conf_m)
{
	action->type = type;
	action->conf = conf_v;
	mask->type = type;
	mask->conf = conf_m;
}

/**
 * Create an egress actions template with MODIFY_FIELD action for setting unused REG_C_0 bits
 * to vport tag and JUMP action to group 1.
 *
 * If extended metadata mode is enabled, then MODIFY_FIELD action for copying software metadata
 * to REG_C_1 is added as well.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to actions template on success. NULL otherwise, and rte_errno is set.
 */
static struct rte_flow_actions_template *
flow_hw_create_tx_repr_tag_jump_acts_tmpl(struct rte_eth_dev *dev)
{
	uint32_t tag_mask = flow_hw_tx_tag_regc_mask(dev);
	uint32_t tag_value = flow_hw_tx_tag_regc_value(dev);
	struct rte_flow_actions_template_attr attr = {
		.egress = 1,
	};
	struct rte_flow_action_modify_field set_tag_v = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_0,
			.offset = rte_bsf32(tag_mask),
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
		},
		.width = __builtin_popcount(tag_mask),
	};
	struct rte_flow_action_modify_field set_tag_m = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
		},
		.width = UINT32_MAX,
	};
	struct rte_flow_action_modify_field copy_metadata_v = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_1,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_A,
		},
		.width = 32,
	};
	struct rte_flow_action_modify_field copy_metadata_m = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.width = UINT32_MAX,
	};
	struct rte_flow_action_jump jump_v = {
		.group = MLX5_HW_LOWEST_USABLE_GROUP,
	};
	struct rte_flow_action_jump jump_m = {
		.group = UINT32_MAX,
	};
	struct rte_flow_action actions_v[4] = { { 0 } };
	struct rte_flow_action actions_m[4] = { { 0 } };
	unsigned int idx = 0;

	rte_memcpy(set_tag_v.src.value, &tag_value, sizeof(tag_value));
	rte_memcpy(set_tag_m.src.value, &tag_mask, sizeof(tag_mask));
	flow_hw_update_action_mask(&actions_v[idx], &actions_m[idx],
				   RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
				   &set_tag_v, &set_tag_m);
	idx++;
	if (MLX5_SH(dev)->config.dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS) {
		flow_hw_update_action_mask(&actions_v[idx], &actions_m[idx],
					   RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
					   &copy_metadata_v, &copy_metadata_m);
		idx++;
	}
	flow_hw_update_action_mask(&actions_v[idx], &actions_m[idx], RTE_FLOW_ACTION_TYPE_JUMP,
				   &jump_v, &jump_m);
	idx++;
	flow_hw_update_action_mask(&actions_v[idx], &actions_m[idx], RTE_FLOW_ACTION_TYPE_END,
				   NULL, NULL);
	idx++;
	MLX5_ASSERT(idx <= RTE_DIM(actions_v));
	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m, NULL);
}

static void
flow_hw_cleanup_tx_repr_tagging(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->hw_tx_repr_tagging_tbl) {
		flow_hw_table_destroy(dev, priv->hw_tx_repr_tagging_tbl, NULL);
		priv->hw_tx_repr_tagging_tbl = NULL;
	}
	if (priv->hw_tx_repr_tagging_at) {
		flow_hw_actions_template_destroy(dev, priv->hw_tx_repr_tagging_at, NULL);
		priv->hw_tx_repr_tagging_at = NULL;
	}
	if (priv->hw_tx_repr_tagging_pt) {
		flow_hw_pattern_template_destroy(dev, priv->hw_tx_repr_tagging_pt, NULL);
		priv->hw_tx_repr_tagging_pt = NULL;
	}
}

/**
 * Setup templates and table used to create default Tx flow rules. These default rules
 * allow for matching Tx representor traffic using a vport tag placed in unused bits of
 * REG_C_0 register.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, negative errno value otherwise.
 */
static int
flow_hw_setup_tx_repr_tagging(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = 0,
			.priority = MLX5_HW_LOWEST_PRIO_ROOT,
			.egress = 1,
		},
		.nb_flows = MLX5_HW_CTRL_FLOW_NB_RULES,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = attr,
		.external = false,
	};

	MLX5_ASSERT(priv->sh->config.dv_esw_en);
	MLX5_ASSERT(priv->sh->config.repr_matching);
	priv->hw_tx_repr_tagging_pt = flow_hw_create_tx_repr_sq_pattern_tmpl(dev);
	if (!priv->hw_tx_repr_tagging_pt)
		goto error;
	priv->hw_tx_repr_tagging_at = flow_hw_create_tx_repr_tag_jump_acts_tmpl(dev);
	if (!priv->hw_tx_repr_tagging_at)
		goto error;
	priv->hw_tx_repr_tagging_tbl = flow_hw_table_create(dev, &cfg,
							    &priv->hw_tx_repr_tagging_pt, 1,
							    &priv->hw_tx_repr_tagging_at, 1,
							    NULL);
	if (!priv->hw_tx_repr_tagging_tbl)
		goto error;
	return 0;
error:
	flow_hw_cleanup_tx_repr_tagging(dev);
	return -rte_errno;
}

static uint32_t
flow_hw_esw_mgr_regc_marker_mask(struct rte_eth_dev *dev)
{
	uint32_t mask = MLX5_SH(dev)->dv_regc0_mask;

	/* Mask is verified during device initialization. */
	MLX5_ASSERT(mask != 0);
	return mask;
}

static uint32_t
flow_hw_esw_mgr_regc_marker(struct rte_eth_dev *dev)
{
	uint32_t mask = MLX5_SH(dev)->dv_regc0_mask;

	/* Mask is verified during device initialization. */
	MLX5_ASSERT(mask != 0);
	return RTE_BIT32(rte_bsf32(mask));
}

/**
 * Creates a flow pattern template used to match on E-Switch Manager.
 * This template is used to set up a table for SQ miss default flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_esw_mgr_pattern_template(struct rte_eth_dev *dev)
{
	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 0,
		.transfer = 1,
	};
	struct rte_flow_item_ethdev port_spec = {
		.port_id = MLX5_REPRESENTED_PORT_ESW_MGR,
	};
	struct rte_flow_item_ethdev port_mask = {
		.port_id = UINT16_MAX,
	};
	struct mlx5_rte_flow_item_sq sq_mask = {
		.queue = UINT32_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &port_spec,
			.mask = &port_mask,
		},
		{
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ,
			.mask = &sq_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};

	return flow_hw_pattern_template_create(dev, &attr, items, NULL);
}

/**
 * Creates a flow pattern template used to match REG_C_0 and a SQ.
 * Matching on REG_C_0 is set up to match on all bits usable by user-space.
 * If traffic was sent from E-Switch Manager, then all usable bits will be set to 0,
 * except the least significant bit, which will be set to 1.
 *
 * This template is used to set up a table for SQ miss default flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_regc_sq_pattern_template(struct rte_eth_dev *dev)
{
	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 0,
		.transfer = 1,
	};
	struct rte_flow_item_tag reg_c0_spec = {
		.index = (uint8_t)REG_C_0,
	};
	struct rte_flow_item_tag reg_c0_mask = {
		.index = 0xff,
		.data = flow_hw_esw_mgr_regc_marker_mask(dev),
	};
	struct mlx5_rte_flow_item_sq queue_mask = {
		.queue = UINT32_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &reg_c0_spec,
			.mask = &reg_c0_mask,
		},
		{
			.type = (enum rte_flow_item_type)
				MLX5_RTE_FLOW_ITEM_TYPE_SQ,
			.mask = &queue_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};

	return flow_hw_pattern_template_create(dev, &attr, items, NULL);
}

/**
 * Creates a flow pattern template with unmasked represented port matching.
 * This template is used to set up a table for default transfer flows
 * directing packets to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_port_pattern_template(struct rte_eth_dev *dev)
{
	struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 0,
		.transfer = 1,
	};
	struct rte_flow_item_ethdev port_mask = {
		.port_id = UINT16_MAX,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.mask = &port_mask,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};

	return flow_hw_pattern_template_create(dev, &attr, items, NULL);
}

/*
 * Creating a flow pattern template with all ETH packets matching.
 * This template is used to set up a table for default Tx copy (Tx metadata
 * to REG_C_1) flow rule usage.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_tx_default_mreg_copy_pattern_template(struct rte_eth_dev *dev)
{
	struct rte_flow_pattern_template_attr tx_pa_attr = {
		.relaxed_matching = 0,
		.egress = 1,
	};
	struct rte_flow_item_eth promisc = {
		.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.ether_type = 0,
	};
	struct rte_flow_item eth_all[] = {
		[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &promisc,
			.mask = &promisc,
		},
		[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_error drop_err;

	RTE_SET_USED(drop_err);
	return flow_hw_pattern_template_create(dev, &tx_pa_attr, eth_all, &drop_err);
}

/**
 * Creates a flow actions template with modify field action and masked jump action.
 * Modify field action sets the least significant bit of REG_C_0 (usable by user-space)
 * to 1, meaning that packet was originated from E-Switch Manager. Jump action
 * transfers steering to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_regc_jump_actions_template(struct rte_eth_dev *dev)
{
	uint32_t marker_mask = flow_hw_esw_mgr_regc_marker_mask(dev);
	uint32_t marker_bits = flow_hw_esw_mgr_regc_marker(dev);
	struct rte_flow_actions_template_attr attr = {
		.transfer = 1,
	};
	struct rte_flow_action_modify_field set_reg_v = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_0,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
		},
		.width = __builtin_popcount(marker_mask),
	};
	struct rte_flow_action_modify_field set_reg_m = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
		},
		.width = UINT32_MAX,
	};
	struct rte_flow_action_jump jump_v = {
		.group = MLX5_HW_LOWEST_USABLE_GROUP,
	};
	struct rte_flow_action_jump jump_m = {
		.group = UINT32_MAX,
	};
	struct rte_flow_action actions_v[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
			.conf = &set_reg_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_flow_action actions_m[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
			.conf = &set_reg_m,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_m,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};

	set_reg_v.dst.offset = rte_bsf32(marker_mask);
	rte_memcpy(set_reg_v.src.value, &marker_bits, sizeof(marker_bits));
	rte_memcpy(set_reg_m.src.value, &marker_mask, sizeof(marker_mask));
	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m, NULL);
}

/**
 * Creates a flow actions template with an unmasked JUMP action. Flows
 * based on this template will perform a jump to some group. This template
 * is used to set up tables for control flows.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param group
 *   Destination group for this action template.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_jump_actions_template(struct rte_eth_dev *dev,
					  uint32_t group)
{
	struct rte_flow_actions_template_attr attr = {
		.transfer = 1,
	};
	struct rte_flow_action_jump jump_v = {
		.group = group,
	};
	struct rte_flow_action_jump jump_m = {
		.group = UINT32_MAX,
	};
	struct rte_flow_action actions_v[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_flow_action actions_m[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_m,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};

	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m,
					       NULL);
}

/**
 * Creates a flow action template with a unmasked REPRESENTED_PORT action.
 * It is used to create control flow tables.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow action template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_port_actions_template(struct rte_eth_dev *dev)
{
	struct rte_flow_actions_template_attr attr = {
		.transfer = 1,
	};
	struct rte_flow_action_ethdev port_v = {
		.port_id = 0,
	};
	struct rte_flow_action actions_v[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
			.conf = &port_v,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_flow_action_ethdev port_m = {
		.port_id = 0,
	};
	struct rte_flow_action actions_m[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
			.conf = &port_m,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};

	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m,
					       NULL);
}

/*
 * Creating an actions template to use header modify action for register
 * copying. This template is used to set up a table for copy flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_tx_default_mreg_copy_actions_template(struct rte_eth_dev *dev)
{
	struct rte_flow_actions_template_attr tx_act_attr = {
		.egress = 1,
	};
	const struct rte_flow_action_modify_field mreg_action = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_1,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_A,
		},
		.width = 32,
	};
	const struct rte_flow_action_modify_field mreg_mask = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.level = UINT8_MAX,
			.tag_index = UINT8_MAX,
			.offset = UINT32_MAX,
		},
		.width = UINT32_MAX,
	};
	const struct rte_flow_action_jump jump_action = {
		.group = 1,
	};
	const struct rte_flow_action_jump jump_mask = {
		.group = UINT32_MAX,
	};
	const struct rte_flow_action actions[] = {
		[0] = {
			.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
			.conf = &mreg_action,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_action,
		},
		[2] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	const struct rte_flow_action masks[] = {
		[0] = {
			.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
			.conf = &mreg_mask,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump_mask,
		},
		[2] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct rte_flow_error drop_err;

	RTE_SET_USED(drop_err);
	return flow_hw_actions_template_create(dev, &tx_act_attr, actions,
					       masks, &drop_err);
}

/**
 * Creates a control flow table used to transfer traffic from E-Switch Manager
 * and TX queues from group 0 to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param it
 *   Pointer to flow pattern template.
 * @param at
 *   Pointer to flow actions template.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_ctrl_sq_miss_root_table(struct rte_eth_dev *dev,
				       struct rte_flow_pattern_template *it,
				       struct rte_flow_actions_template *at)
{
	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = 0,
			.priority = MLX5_HW_LOWEST_PRIO_ROOT,
			.ingress = 0,
			.egress = 0,
			.transfer = 1,
		},
		.nb_flows = MLX5_HW_CTRL_FLOW_NB_RULES,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = attr,
		.external = false,
	};

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, NULL);
}


/**
 * Creates a control flow table used to transfer traffic from E-Switch Manager
 * and TX queues from group 0 to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param it
 *   Pointer to flow pattern template.
 * @param at
 *   Pointer to flow actions template.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_ctrl_sq_miss_table(struct rte_eth_dev *dev,
				  struct rte_flow_pattern_template *it,
				  struct rte_flow_actions_template *at)
{
	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = 1,
			.priority = MLX5_HW_LOWEST_PRIO_NON_ROOT,
			.ingress = 0,
			.egress = 0,
			.transfer = 1,
		},
		.nb_flows = MLX5_HW_CTRL_FLOW_NB_RULES,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = attr,
		.external = false,
	};

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, NULL);
}

/*
 * Creating the default Tx metadata copy table on NIC Tx group 0.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param pt
 *   Pointer to flow pattern template.
 * @param at
 *   Pointer to flow actions template.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_tx_default_mreg_copy_table(struct rte_eth_dev *dev,
					  struct rte_flow_pattern_template *pt,
					  struct rte_flow_actions_template *at)
{
	struct rte_flow_template_table_attr tx_tbl_attr = {
		.flow_attr = {
			.group = 0, /* Root */
			.priority = MLX5_HW_LOWEST_PRIO_ROOT,
			.egress = 1,
		},
		.nb_flows = 1, /* One default flow rule for all. */
	};
	struct mlx5_flow_template_table_cfg tx_tbl_cfg = {
		.attr = tx_tbl_attr,
		.external = false,
	};
	struct rte_flow_error drop_err = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};

	RTE_SET_USED(drop_err);
	return flow_hw_table_create(dev, &tx_tbl_cfg, &pt, 1, &at, 1, &drop_err);
}

/**
 * Creates a control flow table used to transfer traffic
 * from group 0 to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param it
 *   Pointer to flow pattern template.
 * @param at
 *   Pointer to flow actions template.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table *
flow_hw_create_ctrl_jump_table(struct rte_eth_dev *dev,
			       struct rte_flow_pattern_template *it,
			       struct rte_flow_actions_template *at)
{
	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = 0,
			.priority = 0,
			.ingress = 0,
			.egress = 0,
			.transfer = 1,
		},
		.nb_flows = MLX5_HW_CTRL_FLOW_NB_RULES,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = attr,
		.external = false,
	};

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, NULL);
}

/**
 * Creates a set of flow tables used to create control flows used
 * when E-Switch is engaged.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, EINVAL otherwise
 */
static __rte_unused int
flow_hw_create_ctrl_tables(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_pattern_template *esw_mgr_items_tmpl = NULL;
	struct rte_flow_pattern_template *regc_sq_items_tmpl = NULL;
	struct rte_flow_pattern_template *port_items_tmpl = NULL;
	struct rte_flow_pattern_template *tx_meta_items_tmpl = NULL;
	struct rte_flow_actions_template *regc_jump_actions_tmpl = NULL;
	struct rte_flow_actions_template *port_actions_tmpl = NULL;
	struct rte_flow_actions_template *jump_one_actions_tmpl = NULL;
	struct rte_flow_actions_template *tx_meta_actions_tmpl = NULL;
	uint32_t xmeta = priv->sh->config.dv_xmeta_en;
	uint32_t repr_matching = priv->sh->config.repr_matching;

	/* Create templates and table for default SQ miss flow rules - root table. */
	esw_mgr_items_tmpl = flow_hw_create_ctrl_esw_mgr_pattern_template(dev);
	if (!esw_mgr_items_tmpl) {
		DRV_LOG(ERR, "port %u failed to create E-Switch Manager item"
			" template for control flows", dev->data->port_id);
		goto error;
	}
	regc_jump_actions_tmpl = flow_hw_create_ctrl_regc_jump_actions_template(dev);
	if (!regc_jump_actions_tmpl) {
		DRV_LOG(ERR, "port %u failed to create REG_C set and jump action template"
			" for control flows", dev->data->port_id);
		goto error;
	}
	MLX5_ASSERT(priv->hw_esw_sq_miss_root_tbl == NULL);
	priv->hw_esw_sq_miss_root_tbl = flow_hw_create_ctrl_sq_miss_root_table
			(dev, esw_mgr_items_tmpl, regc_jump_actions_tmpl);
	if (!priv->hw_esw_sq_miss_root_tbl) {
		DRV_LOG(ERR, "port %u failed to create table for default sq miss (root table)"
			" for control flows", dev->data->port_id);
		goto error;
	}
	/* Create templates and table for default SQ miss flow rules - non-root table. */
	regc_sq_items_tmpl = flow_hw_create_ctrl_regc_sq_pattern_template(dev);
	if (!regc_sq_items_tmpl) {
		DRV_LOG(ERR, "port %u failed to create SQ item template for"
			" control flows", dev->data->port_id);
		goto error;
	}
	port_actions_tmpl = flow_hw_create_ctrl_port_actions_template(dev);
	if (!port_actions_tmpl) {
		DRV_LOG(ERR, "port %u failed to create port action template"
			" for control flows", dev->data->port_id);
		goto error;
	}
	MLX5_ASSERT(priv->hw_esw_sq_miss_tbl == NULL);
	priv->hw_esw_sq_miss_tbl = flow_hw_create_ctrl_sq_miss_table(dev, regc_sq_items_tmpl,
								     port_actions_tmpl);
	if (!priv->hw_esw_sq_miss_tbl) {
		DRV_LOG(ERR, "port %u failed to create table for default sq miss (non-root table)"
			" for control flows", dev->data->port_id);
		goto error;
	}
	/* Create templates and table for default FDB jump flow rules. */
	port_items_tmpl = flow_hw_create_ctrl_port_pattern_template(dev);
	if (!port_items_tmpl) {
		DRV_LOG(ERR, "port %u failed to create SQ item template for"
			" control flows", dev->data->port_id);
		goto error;
	}
	jump_one_actions_tmpl = flow_hw_create_ctrl_jump_actions_template
			(dev, MLX5_HW_LOWEST_USABLE_GROUP);
	if (!jump_one_actions_tmpl) {
		DRV_LOG(ERR, "port %u failed to create jump action template"
			" for control flows", dev->data->port_id);
		goto error;
	}
	MLX5_ASSERT(priv->hw_esw_zero_tbl == NULL);
	priv->hw_esw_zero_tbl = flow_hw_create_ctrl_jump_table(dev, port_items_tmpl,
							       jump_one_actions_tmpl);
	if (!priv->hw_esw_zero_tbl) {
		DRV_LOG(ERR, "port %u failed to create table for default jump to group 1"
			" for control flows", dev->data->port_id);
		goto error;
	}
	/* Create templates and table for default Tx metadata copy flow rule. */
	if (!repr_matching && xmeta == MLX5_XMETA_MODE_META32_HWS) {
		tx_meta_items_tmpl = flow_hw_create_tx_default_mreg_copy_pattern_template(dev);
		if (!tx_meta_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to Tx metadata copy pattern"
				" template for control flows", dev->data->port_id);
			goto error;
		}
		tx_meta_actions_tmpl = flow_hw_create_tx_default_mreg_copy_actions_template(dev);
		if (!tx_meta_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to Tx metadata copy actions"
				" template for control flows", dev->data->port_id);
			goto error;
		}
		MLX5_ASSERT(priv->hw_tx_meta_cpy_tbl == NULL);
		priv->hw_tx_meta_cpy_tbl = flow_hw_create_tx_default_mreg_copy_table(dev,
					tx_meta_items_tmpl, tx_meta_actions_tmpl);
		if (!priv->hw_tx_meta_cpy_tbl) {
			DRV_LOG(ERR, "port %u failed to create table for default"
				" Tx metadata copy flow rule", dev->data->port_id);
			goto error;
		}
	}
	return 0;
error:
	if (priv->hw_esw_zero_tbl) {
		flow_hw_table_destroy(dev, priv->hw_esw_zero_tbl, NULL);
		priv->hw_esw_zero_tbl = NULL;
	}
	if (priv->hw_esw_sq_miss_tbl) {
		flow_hw_table_destroy(dev, priv->hw_esw_sq_miss_tbl, NULL);
		priv->hw_esw_sq_miss_tbl = NULL;
	}
	if (priv->hw_esw_sq_miss_root_tbl) {
		flow_hw_table_destroy(dev, priv->hw_esw_sq_miss_root_tbl, NULL);
		priv->hw_esw_sq_miss_root_tbl = NULL;
	}
	if (tx_meta_actions_tmpl)
		flow_hw_actions_template_destroy(dev, tx_meta_actions_tmpl, NULL);
	if (jump_one_actions_tmpl)
		flow_hw_actions_template_destroy(dev, jump_one_actions_tmpl, NULL);
	if (port_actions_tmpl)
		flow_hw_actions_template_destroy(dev, port_actions_tmpl, NULL);
	if (regc_jump_actions_tmpl)
		flow_hw_actions_template_destroy(dev, regc_jump_actions_tmpl, NULL);
	if (tx_meta_items_tmpl)
		flow_hw_pattern_template_destroy(dev, tx_meta_items_tmpl, NULL);
	if (port_items_tmpl)
		flow_hw_pattern_template_destroy(dev, port_items_tmpl, NULL);
	if (regc_sq_items_tmpl)
		flow_hw_pattern_template_destroy(dev, regc_sq_items_tmpl, NULL);
	if (esw_mgr_items_tmpl)
		flow_hw_pattern_template_destroy(dev, esw_mgr_items_tmpl, NULL);
	return -EINVAL;
}

static void
flow_hw_ct_mng_destroy(struct rte_eth_dev *dev,
		       struct mlx5_aso_ct_pools_mng *ct_mng)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	mlx5_aso_ct_queue_uninit(priv->sh, ct_mng);
	mlx5_free(ct_mng);
}

static void
flow_hw_ct_pool_destroy(struct rte_eth_dev *dev __rte_unused,
			struct mlx5_aso_ct_pool *pool)
{
	if (pool->dr_action)
		mlx5dr_action_destroy(pool->dr_action);
	if (pool->devx_obj)
		claim_zero(mlx5_devx_cmd_destroy(pool->devx_obj));
	if (pool->cts)
		mlx5_ipool_destroy(pool->cts);
	mlx5_free(pool);
}

static struct mlx5_aso_ct_pool *
flow_hw_ct_pool_create(struct rte_eth_dev *dev,
		       const struct rte_flow_port_attr *port_attr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool;
	struct mlx5_devx_obj *obj;
	uint32_t nb_cts = rte_align32pow2(port_attr->nb_conn_tracks);
	uint32_t log_obj_size = rte_log2_u32(nb_cts);
	struct mlx5_indexed_pool_config cfg = {
		.size = sizeof(struct mlx5_aso_ct_action),
		.trunk_size = 1 << 12,
		.per_core_cache = 1 << 13,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hw_ct_action",
	};
	int reg_id;
	uint32_t flags;

	if (port_attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
		DRV_LOG(ERR, "Connection tracking is not supported "
			     "in cross vHCA sharing mode");
		rte_errno = ENOTSUP;
		return NULL;
	}
	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool), 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		return NULL;
	}
	obj = mlx5_devx_cmd_create_conn_track_offload_obj(priv->sh->cdev->ctx,
							  priv->sh->cdev->pdn,
							  log_obj_size);
	if (!obj) {
		rte_errno = ENODATA;
		DRV_LOG(ERR, "Failed to create conn_track_offload_obj using DevX.");
		goto err;
	}
	pool->devx_obj = obj;
	reg_id = mlx5_flow_get_reg_id(dev, MLX5_ASO_CONNTRACK, 0, NULL);
	flags = MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (priv->sh->config.dv_esw_en && priv->master)
		flags |= MLX5DR_ACTION_FLAG_HWS_FDB;
	pool->dr_action = mlx5dr_action_create_aso_ct(priv->dr_ctx,
						      (struct mlx5dr_devx_obj *)obj,
						      reg_id - REG_C_0, flags);
	if (!pool->dr_action)
		goto err;
	/*
	 * No need for local cache if CT number is a small number. Since
	 * flow insertion rate will be very limited in that case. Here let's
	 * set the number to less than default trunk size 4K.
	 */
	if (nb_cts <= cfg.trunk_size) {
		cfg.per_core_cache = 0;
		cfg.trunk_size = nb_cts;
	} else if (nb_cts <= MLX5_HW_IPOOL_SIZE_THRESHOLD) {
		cfg.per_core_cache = MLX5_HW_IPOOL_CACHE_MIN;
	}
	pool->cts = mlx5_ipool_create(&cfg);
	if (!pool->cts)
		goto err;
	pool->sq = priv->ct_mng->aso_sqs;
	/* Assign the last extra ASO SQ as public SQ. */
	pool->shared_sq = &priv->ct_mng->aso_sqs[priv->nb_queue - 1];
	return pool;
err:
	flow_hw_ct_pool_destroy(dev, pool);
	return NULL;
}

static void
flow_hw_destroy_vlan(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	enum mlx5dr_table_type i;

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		if (priv->hw_pop_vlan[i]) {
			mlx5dr_action_destroy(priv->hw_pop_vlan[i]);
			priv->hw_pop_vlan[i] = NULL;
		}
		if (priv->hw_push_vlan[i]) {
			mlx5dr_action_destroy(priv->hw_push_vlan[i]);
			priv->hw_push_vlan[i] = NULL;
		}
	}
}

static int
flow_hw_create_vlan(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	enum mlx5dr_table_type i;
	const enum mlx5dr_action_flags flags[MLX5DR_TABLE_TYPE_MAX] = {
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB
	};

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i <= MLX5DR_TABLE_TYPE_NIC_TX; i++) {
		priv->hw_pop_vlan[i] =
			mlx5dr_action_create_pop_vlan(priv->dr_ctx, flags[i]);
		if (!priv->hw_pop_vlan[i])
			return -ENOENT;
		priv->hw_push_vlan[i] =
			mlx5dr_action_create_push_vlan(priv->dr_ctx, flags[i]);
		if (!priv->hw_pop_vlan[i])
			return -ENOENT;
	}
	if (priv->sh->config.dv_esw_en && priv->master) {
		priv->hw_pop_vlan[MLX5DR_TABLE_TYPE_FDB] =
			mlx5dr_action_create_pop_vlan
				(priv->dr_ctx, MLX5DR_ACTION_FLAG_HWS_FDB);
		if (!priv->hw_pop_vlan[MLX5DR_TABLE_TYPE_FDB])
			return -ENOENT;
		priv->hw_push_vlan[MLX5DR_TABLE_TYPE_FDB] =
			mlx5dr_action_create_push_vlan
				(priv->dr_ctx, MLX5DR_ACTION_FLAG_HWS_FDB);
		if (!priv->hw_pop_vlan[MLX5DR_TABLE_TYPE_FDB])
			return -ENOENT;
	}
	return 0;
}

static void
flow_hw_cleanup_ctrl_rx_tables(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int j;

	if (!priv->hw_ctrl_rx)
		return;
	for (i = 0; i < MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_MAX; ++i) {
		for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
			struct rte_flow_template_table *tbl = priv->hw_ctrl_rx->tables[i][j].tbl;
			struct rte_flow_pattern_template *pt = priv->hw_ctrl_rx->tables[i][j].pt;

			if (tbl)
				claim_zero(flow_hw_table_destroy(dev, tbl, NULL));
			if (pt)
				claim_zero(flow_hw_pattern_template_destroy(dev, pt, NULL));
		}
	}
	for (i = 0; i < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++i) {
		struct rte_flow_actions_template *at = priv->hw_ctrl_rx->rss[i];

		if (at)
			claim_zero(flow_hw_actions_template_destroy(dev, at, NULL));
	}
	mlx5_free(priv->hw_ctrl_rx);
	priv->hw_ctrl_rx = NULL;
}

static uint64_t
flow_hw_ctrl_rx_rss_type_hash_types(const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	switch (rss_type) {
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_NON_IP:
		return 0;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4:
		return RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4 | RTE_ETH_RSS_NONFRAG_IPV4_OTHER;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_UDP:
		return RTE_ETH_RSS_NONFRAG_IPV4_UDP;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_TCP:
		return RTE_ETH_RSS_NONFRAG_IPV4_TCP;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6:
		return RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6 | RTE_ETH_RSS_NONFRAG_IPV6_OTHER;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_UDP:
		return RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_IPV6_UDP_EX;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_TCP:
		return RTE_ETH_RSS_NONFRAG_IPV6_TCP | RTE_ETH_RSS_IPV6_TCP_EX;
	default:
		/* Should not reach here. */
		MLX5_ASSERT(false);
		return 0;
	}
}

static struct rte_flow_actions_template *
flow_hw_create_ctrl_rx_rss_template(struct rte_eth_dev *dev,
				    const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_actions_template_attr attr = {
		.ingress = 1,
	};
	uint16_t queue[RTE_MAX_QUEUES_PER_PORT];
	struct rte_flow_action_rss rss_conf = {
		.func = RTE_ETH_HASH_FUNCTION_DEFAULT,
		.level = 0,
		.types = 0,
		.key_len = priv->rss_conf.rss_key_len,
		.key = priv->rss_conf.rss_key,
		.queue_num = priv->reta_idx_n,
		.queue = queue,
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = &rss_conf,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_flow_action masks[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_RSS,
			.conf = &rss_conf,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_flow_actions_template *at;
	struct rte_flow_error error;
	unsigned int i;

	MLX5_ASSERT(priv->reta_idx_n > 0 && priv->reta_idx);
	/* Select proper RSS hash types and based on that configure the actions template. */
	rss_conf.types = flow_hw_ctrl_rx_rss_type_hash_types(rss_type);
	if (rss_conf.types) {
		for (i = 0; i < priv->reta_idx_n; ++i)
			queue[i] = (*priv->reta_idx)[i];
	} else {
		rss_conf.queue_num = 1;
		queue[0] = (*priv->reta_idx)[0];
	}
	at = flow_hw_actions_template_create(dev, &attr, actions, masks, &error);
	if (!at)
		DRV_LOG(ERR,
			"Failed to create ctrl flow actions template: rte_errno(%d), type(%d): %s",
			rte_errno, error.type,
			error.message ? error.message : "(no stated reason)");
	return at;
}

static uint32_t ctrl_rx_rss_priority_map[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX] = {
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_NON_IP] = MLX5_HW_CTRL_RX_PRIO_L2,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4] = MLX5_HW_CTRL_RX_PRIO_L3,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_UDP] = MLX5_HW_CTRL_RX_PRIO_L4,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_TCP] = MLX5_HW_CTRL_RX_PRIO_L4,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6] = MLX5_HW_CTRL_RX_PRIO_L3,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_UDP] = MLX5_HW_CTRL_RX_PRIO_L4,
	[MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_TCP] = MLX5_HW_CTRL_RX_PRIO_L4,
};

static uint32_t ctrl_rx_nb_flows_map[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_MAX] = {
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL] = 1,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL_MCAST] = 1,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST] = 1,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN] = MLX5_MAX_VLAN_IDS,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST] = 1,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN] = MLX5_MAX_VLAN_IDS,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST] = 1,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN] = MLX5_MAX_VLAN_IDS,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC] = MLX5_MAX_UC_MAC_ADDRESSES,
	[MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN] =
			MLX5_MAX_UC_MAC_ADDRESSES * MLX5_MAX_VLAN_IDS,
};

static struct rte_flow_template_table_attr
flow_hw_get_ctrl_rx_table_attr(enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type,
			       const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	return (struct rte_flow_template_table_attr){
		.flow_attr = {
			.group = 0,
			.priority = ctrl_rx_rss_priority_map[rss_type],
			.ingress = 1,
		},
		.nb_flows = ctrl_rx_nb_flows_map[eth_pattern_type],
	};
}

static struct rte_flow_item
flow_hw_get_ctrl_rx_eth_item(const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type)
{
	struct rte_flow_item item = {
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.mask = NULL,
	};

	switch (eth_pattern_type) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL:
		item.mask = &ctrl_rx_eth_promisc_mask;
		break;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL_MCAST:
		item.mask = &ctrl_rx_eth_mcast_mask;
		break;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		item.mask = &ctrl_rx_eth_dmac_mask;
		break;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
		item.mask = &ctrl_rx_eth_ipv4_mcast_mask;
		break;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
		item.mask = &ctrl_rx_eth_ipv6_mcast_mask;
		break;
	default:
		/* Should not reach here - ETH mask must be present. */
		item.type = RTE_FLOW_ITEM_TYPE_END;
		MLX5_ASSERT(false);
		break;
	}
	return item;
}

static struct rte_flow_item
flow_hw_get_ctrl_rx_vlan_item(const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type)
{
	struct rte_flow_item item = {
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.mask = NULL,
	};

	switch (eth_pattern_type) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		item.type = RTE_FLOW_ITEM_TYPE_VLAN;
		item.mask = &rte_flow_item_vlan_mask;
		break;
	default:
		/* Nothing to update. */
		break;
	}
	return item;
}

static struct rte_flow_item
flow_hw_get_ctrl_rx_l3_item(const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct rte_flow_item item = {
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.mask = NULL,
	};

	switch (rss_type) {
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_UDP:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_TCP:
		item.type = RTE_FLOW_ITEM_TYPE_IPV4;
		break;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_UDP:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_TCP:
		item.type = RTE_FLOW_ITEM_TYPE_IPV6;
		break;
	default:
		/* Nothing to update. */
		break;
	}
	return item;
}

static struct rte_flow_item
flow_hw_get_ctrl_rx_l4_item(const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct rte_flow_item item = {
		.type = RTE_FLOW_ITEM_TYPE_VOID,
		.mask = NULL,
	};

	switch (rss_type) {
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_UDP:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_UDP:
		item.type = RTE_FLOW_ITEM_TYPE_UDP;
		break;
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV4_TCP:
	case MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_IPV6_TCP:
		item.type = RTE_FLOW_ITEM_TYPE_TCP;
		break;
	default:
		/* Nothing to update. */
		break;
	}
	return item;
}

static struct rte_flow_pattern_template *
flow_hw_create_ctrl_rx_pattern_template
		(struct rte_eth_dev *dev,
		 const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type,
		 const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	const struct rte_flow_pattern_template_attr attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};
	struct rte_flow_item items[] = {
		/* Matching patterns */
		flow_hw_get_ctrl_rx_eth_item(eth_pattern_type),
		flow_hw_get_ctrl_rx_vlan_item(eth_pattern_type),
		flow_hw_get_ctrl_rx_l3_item(rss_type),
		flow_hw_get_ctrl_rx_l4_item(rss_type),
		/* Terminate pattern */
		{ .type = RTE_FLOW_ITEM_TYPE_END }
	};

	return flow_hw_pattern_template_create(dev, &attr, items, NULL);
}

static int
flow_hw_create_ctrl_rx_tables(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int j;
	int ret;

	MLX5_ASSERT(!priv->hw_ctrl_rx);
	priv->hw_ctrl_rx = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*priv->hw_ctrl_rx),
				       RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (!priv->hw_ctrl_rx) {
		DRV_LOG(ERR, "Failed to allocate memory for Rx control flow tables");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Create all pattern template variants. */
	for (i = 0; i < MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_MAX; ++i) {
		enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type = i;

		for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
			const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type = j;
			struct rte_flow_template_table_attr attr;
			struct rte_flow_pattern_template *pt;

			attr = flow_hw_get_ctrl_rx_table_attr(eth_pattern_type, rss_type);
			pt = flow_hw_create_ctrl_rx_pattern_template(dev, eth_pattern_type,
								     rss_type);
			if (!pt)
				goto err;
			priv->hw_ctrl_rx->tables[i][j].attr = attr;
			priv->hw_ctrl_rx->tables[i][j].pt = pt;
		}
	}
	return 0;
err:
	ret = rte_errno;
	flow_hw_cleanup_ctrl_rx_tables(dev);
	rte_errno = ret;
	return -ret;
}

void
mlx5_flow_hw_cleanup_ctrl_rx_templates(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_hw_ctrl_rx *hw_ctrl_rx;
	unsigned int i;
	unsigned int j;

	if (!priv->dr_ctx)
		return;
	if (!priv->hw_ctrl_rx)
		return;
	hw_ctrl_rx = priv->hw_ctrl_rx;
	for (i = 0; i < MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_MAX; ++i) {
		for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
			struct mlx5_flow_hw_ctrl_rx_table *tmpls = &hw_ctrl_rx->tables[i][j];

			if (tmpls->tbl) {
				claim_zero(flow_hw_table_destroy(dev, tmpls->tbl, NULL));
				tmpls->tbl = NULL;
			}
		}
	}
	for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
		if (hw_ctrl_rx->rss[j]) {
			claim_zero(flow_hw_actions_template_destroy(dev, hw_ctrl_rx->rss[j], NULL));
			hw_ctrl_rx->rss[j] = NULL;
		}
	}
}

/**
 * Copy the provided HWS configuration to a newly allocated buffer.
 *
 * @param[in] port_attr
 *   Port configuration attributes.
 * @param[in] nb_queue
 *   Number of queue.
 * @param[in] queue_attr
 *   Array that holds attributes for each flow queue.
 *
 * @return
 *   Pointer to copied HWS configuration is returned on success.
 *   Otherwise, NULL is returned and rte_errno is set.
 */
static struct mlx5_flow_hw_attr *
flow_hw_alloc_copy_config(const struct rte_flow_port_attr *port_attr,
			  const uint16_t nb_queue,
			  const struct rte_flow_queue_attr *queue_attr[],
			  struct rte_flow_error *error)
{
	struct mlx5_flow_hw_attr *hw_attr;
	size_t hw_attr_size;
	unsigned int i;

	hw_attr_size = sizeof(*hw_attr) + nb_queue * sizeof(*hw_attr->queue_attr);
	hw_attr = mlx5_malloc(MLX5_MEM_ZERO, hw_attr_size, 0, SOCKET_ID_ANY);
	if (!hw_attr) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Not enough memory to store configuration");
		return NULL;
	}
	memcpy(&hw_attr->port_attr, port_attr, sizeof(*port_attr));
	hw_attr->nb_queue = nb_queue;
	/* Queue attributes are placed after the mlx5_flow_hw_attr. */
	hw_attr->queue_attr = (struct rte_flow_queue_attr *)(hw_attr + 1);
	for (i = 0; i < nb_queue; ++i)
		memcpy(&hw_attr->queue_attr[i], queue_attr[i], sizeof(hw_attr->queue_attr[i]));
	return hw_attr;
}

/**
 * Compares the preserved HWS configuration with the provided one.
 *
 * @param[in] hw_attr
 *   Pointer to preserved HWS configuration.
 * @param[in] new_pa
 *   Port configuration attributes to compare.
 * @param[in] new_nbq
 *   Number of queues to compare.
 * @param[in] new_qa
 *   Array that holds attributes for each flow queue.
 *
 * @return
 *   True if configurations are the same, false otherwise.
 */
static bool
flow_hw_compare_config(const struct mlx5_flow_hw_attr *hw_attr,
		       const struct rte_flow_port_attr *new_pa,
		       const uint16_t new_nbq,
		       const struct rte_flow_queue_attr *new_qa[])
{
	const struct rte_flow_port_attr *old_pa = &hw_attr->port_attr;
	const uint16_t old_nbq = hw_attr->nb_queue;
	const struct rte_flow_queue_attr *old_qa = hw_attr->queue_attr;
	unsigned int i;

	if (old_pa->nb_counters != new_pa->nb_counters ||
	    old_pa->nb_aging_objects != new_pa->nb_aging_objects ||
	    old_pa->nb_meters != new_pa->nb_meters ||
	    old_pa->nb_conn_tracks != new_pa->nb_conn_tracks ||
	    old_pa->flags != new_pa->flags)
		return false;
	if (old_nbq != new_nbq)
		return false;
	for (i = 0; i < old_nbq; ++i)
		if (old_qa[i].size != new_qa[i]->size)
			return false;
	return true;
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
flow_hw_configure(struct rte_eth_dev *dev,
		  const struct rte_flow_port_attr *port_attr,
		  uint16_t nb_queue,
		  const struct rte_flow_queue_attr *queue_attr[],
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_priv *host_priv = NULL;
	struct mlx5dr_context *dr_ctx = NULL;
	struct mlx5dr_context_attr dr_ctx_attr = {0};
	struct mlx5_hw_q *hw_q;
	struct mlx5_hw_q_job *job = NULL;
	uint32_t mem_size, i, j;
	struct mlx5_indexed_pool_config cfg = {
		.size = sizeof(struct mlx5_action_construct_data),
		.trunk_size = 4096,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.free = mlx5_free,
		.type = "mlx5_hw_action_construct_data",
	};
	/*
	 * Adds one queue to be used by PMD.
	 * The last queue will be used by the PMD.
	 */
	uint16_t nb_q_updated = 0;
	struct rte_flow_queue_attr **_queue_attr = NULL;
	struct rte_flow_queue_attr ctrl_queue_attr = {0};
	bool is_proxy = !!(priv->sh->config.dv_esw_en && priv->master);
	int ret = 0;

	if (!port_attr || !nb_queue || !queue_attr) {
		rte_errno = EINVAL;
		goto err;
	}
	/*
	 * Calling rte_flow_configure() again is allowed if and only if
	 * provided configuration matches the initially provided one.
	 */
	if (priv->dr_ctx) {
		MLX5_ASSERT(priv->hw_attr != NULL);
		for (i = 0; i < priv->nb_queue; i++) {
			hw_q = &priv->hw_q[i];
			/* Make sure all queues are empty. */
			if (hw_q->size != hw_q->job_idx) {
				rte_errno = EBUSY;
				goto err;
			}
		}
		if (flow_hw_compare_config(priv->hw_attr, port_attr, nb_queue, queue_attr))
			return 0;
		else
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
						  "Changing HWS configuration attributes "
						  "is not supported");
	}
	priv->hw_attr = flow_hw_alloc_copy_config(port_attr, nb_queue, queue_attr, error);
	if (!priv->hw_attr) {
		ret = -rte_errno;
		goto err;
	}
	ctrl_queue_attr.size = queue_attr[0]->size;
	nb_q_updated = nb_queue + 1;
	_queue_attr = mlx5_malloc(MLX5_MEM_ZERO,
				  nb_q_updated *
				  sizeof(struct rte_flow_queue_attr *),
				  64, SOCKET_ID_ANY);
	if (!_queue_attr) {
		rte_errno = ENOMEM;
		goto err;
	}

	memcpy(_queue_attr, queue_attr,
	       sizeof(void *) * nb_queue);
	_queue_attr[nb_queue] = &ctrl_queue_attr;
	priv->acts_ipool = mlx5_ipool_create(&cfg);
	if (!priv->acts_ipool)
		goto err;
	/* Allocate the queue job descriptor LIFO. */
	mem_size = sizeof(priv->hw_q[0]) * nb_q_updated;
	for (i = 0; i < nb_q_updated; i++) {
		/*
		 * Check if the queues' size are all the same as the
		 * limitation from HWS layer.
		 */
		if (_queue_attr[i]->size != _queue_attr[0]->size) {
			rte_errno = EINVAL;
			goto err;
		}
		mem_size += (sizeof(struct mlx5_hw_q_job *) +
			    sizeof(struct mlx5_hw_q_job) +
			    sizeof(uint8_t) * MLX5_ENCAP_MAX_LEN +
			    sizeof(struct mlx5_modification_cmd) *
			    MLX5_MHDR_MAX_CMD +
			    sizeof(struct rte_flow_item) *
			    MLX5_HW_MAX_ITEMS +
				sizeof(struct rte_flow_hw)) *
			    _queue_attr[i]->size;
	}
	priv->hw_q = mlx5_malloc(MLX5_MEM_ZERO, mem_size,
				 64, SOCKET_ID_ANY);
	if (!priv->hw_q) {
		rte_errno = ENOMEM;
		goto err;
	}
	for (i = 0; i < nb_q_updated; i++) {
		char mz_name[RTE_MEMZONE_NAMESIZE];
		uint8_t *encap = NULL;
		struct mlx5_modification_cmd *mhdr_cmd = NULL;
		struct rte_flow_item *items = NULL;
		struct rte_flow_hw *upd_flow = NULL;

		priv->hw_q[i].job_idx = _queue_attr[i]->size;
		priv->hw_q[i].size = _queue_attr[i]->size;
		if (i == 0)
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)
					    &priv->hw_q[nb_q_updated];
		else
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)
				&job[_queue_attr[i - 1]->size - 1].items
				 [MLX5_HW_MAX_ITEMS];
		job = (struct mlx5_hw_q_job *)
		      &priv->hw_q[i].job[_queue_attr[i]->size];
		mhdr_cmd = (struct mlx5_modification_cmd *)
			   &job[_queue_attr[i]->size];
		encap = (uint8_t *)
			 &mhdr_cmd[_queue_attr[i]->size * MLX5_MHDR_MAX_CMD];
		items = (struct rte_flow_item *)
			 &encap[_queue_attr[i]->size * MLX5_ENCAP_MAX_LEN];
		upd_flow = (struct rte_flow_hw *)
			&items[_queue_attr[i]->size * MLX5_HW_MAX_ITEMS];
		for (j = 0; j < _queue_attr[i]->size; j++) {
			job[j].mhdr_cmd = &mhdr_cmd[j * MLX5_MHDR_MAX_CMD];
			job[j].encap_data = &encap[j * MLX5_ENCAP_MAX_LEN];
			job[j].items = &items[j * MLX5_HW_MAX_ITEMS];
			job[j].upd_flow = &upd_flow[j];
			priv->hw_q[i].job[j] = &job[j];
		}
		snprintf(mz_name, sizeof(mz_name), "port_%u_indir_act_cq_%u",
			 dev->data->port_id, i);
		priv->hw_q[i].indir_cq = rte_ring_create(mz_name,
				_queue_attr[i]->size, SOCKET_ID_ANY,
				RING_F_SP_ENQ | RING_F_SC_DEQ |
				RING_F_EXACT_SZ);
		if (!priv->hw_q[i].indir_cq)
			goto err;
		snprintf(mz_name, sizeof(mz_name), "port_%u_indir_act_iq_%u",
			 dev->data->port_id, i);
		priv->hw_q[i].indir_iq = rte_ring_create(mz_name,
				_queue_attr[i]->size, SOCKET_ID_ANY,
				RING_F_SP_ENQ | RING_F_SC_DEQ |
				RING_F_EXACT_SZ);
		if (!priv->hw_q[i].indir_iq)
			goto err;
	}
	dr_ctx_attr.pd = priv->sh->cdev->pd;
	dr_ctx_attr.queues = nb_q_updated;
	/* Queue size should all be the same. Take the first one. */
	dr_ctx_attr.queue_size = _queue_attr[0]->size;
	if (port_attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
		struct rte_eth_dev *host_dev = NULL;
		uint16_t port_id;

		MLX5_ASSERT(rte_eth_dev_is_valid_port(port_attr->host_port_id));
		if (is_proxy) {
			DRV_LOG(ERR, "cross vHCA shared mode not supported "
				     " for E-Switch confgiurations");
			rte_errno = ENOTSUP;
			goto err;
		}
		MLX5_ETH_FOREACH_DEV(port_id, dev->device) {
			if (port_id == port_attr->host_port_id) {
				host_dev = &rte_eth_devices[port_id];
				break;
			}
		}
		if (!host_dev || host_dev == dev ||
		    !host_dev->data || !host_dev->data->dev_private) {
			DRV_LOG(ERR, "Invalid cross vHCA host port %u",
				port_attr->host_port_id);
			rte_errno = EINVAL;
			goto err;
		}
		host_priv = host_dev->data->dev_private;
		if (host_priv->sh->cdev->ctx == priv->sh->cdev->ctx) {
			DRV_LOG(ERR, "Sibling ports %u and %u do not "
				     "require cross vHCA sharing mode",
				dev->data->port_id, port_attr->host_port_id);
			rte_errno = EINVAL;
			goto err;
		}
		if (host_priv->shared_host) {
			DRV_LOG(ERR, "Host port %u is not the sharing base",
				port_attr->host_port_id);
			rte_errno = EINVAL;
			goto err;
		}
		if (port_attr->nb_counters ||
		    port_attr->nb_aging_objects ||
		    port_attr->nb_meters ||
		    port_attr->nb_conn_tracks) {
			DRV_LOG(ERR,
				"Object numbers on guest port must be zeros");
			rte_errno = EINVAL;
			goto err;
		}
		dr_ctx_attr.shared_ibv_ctx = host_priv->sh->cdev->ctx;
		priv->shared_host = host_dev;
		__atomic_fetch_add(&host_priv->shared_refcnt, 1, __ATOMIC_RELAXED);
	}
	dr_ctx = mlx5dr_context_open(priv->sh->cdev->ctx, &dr_ctx_attr);
	/* rte_errno has been updated by HWS layer. */
	if (!dr_ctx)
		goto err;
	priv->dr_ctx = dr_ctx;
	priv->nb_queue = nb_q_updated;
	rte_spinlock_init(&priv->hw_ctrl_lock);
	LIST_INIT(&priv->hw_ctrl_flows);
	ret = flow_hw_create_ctrl_rx_tables(dev);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to set up Rx control flow templates");
		goto err;
	}
	/* Initialize quotas */
	if (port_attr->nb_quotas) {
		ret = mlx5_flow_quota_init(dev, port_attr->nb_quotas);
		if (ret)
			goto err;
	}
	/* Initialize meter library*/
	if (port_attr->nb_meters || (host_priv && host_priv->hws_mpool))
		if (mlx5_flow_meter_init(dev, port_attr->nb_meters, 0, 0, nb_q_updated))
			goto err;
	/* Add global actions. */
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		uint32_t act_flags = 0;

		act_flags = mlx5_hw_act_flag[i][0] | mlx5_hw_act_flag[i][1];
		if (is_proxy)
			act_flags |= mlx5_hw_act_flag[i][2];
		priv->hw_drop[i] = mlx5dr_action_create_dest_drop(priv->dr_ctx, act_flags);
		if (!priv->hw_drop[i])
			goto err;
		priv->hw_tag[i] = mlx5dr_action_create_tag
			(priv->dr_ctx, mlx5_hw_act_flag[i][0]);
		if (!priv->hw_tag[i])
			goto err;
	}
	if (priv->sh->config.dv_esw_en && priv->sh->config.repr_matching) {
		ret = flow_hw_setup_tx_repr_tagging(dev);
		if (ret) {
			rte_errno = -ret;
			goto err;
		}
	}
	if (is_proxy) {
		ret = flow_hw_create_vport_actions(priv);
		if (ret) {
			rte_errno = -ret;
			goto err;
		}
		ret = flow_hw_create_ctrl_tables(dev);
		if (ret) {
			rte_errno = -ret;
			goto err;
		}
	}
	if (!priv->shared_host)
		flow_hw_create_send_to_kernel_actions(priv);
	if (port_attr->nb_conn_tracks || (host_priv && host_priv->hws_ctpool)) {
		mem_size = sizeof(struct mlx5_aso_sq) * nb_q_updated +
			   sizeof(*priv->ct_mng);
		priv->ct_mng = mlx5_malloc(MLX5_MEM_ZERO, mem_size,
					   RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
		if (!priv->ct_mng)
			goto err;
		if (mlx5_aso_ct_queue_init(priv->sh, priv->ct_mng, nb_q_updated))
			goto err;
		priv->hws_ctpool = flow_hw_ct_pool_create(dev, port_attr);
		if (!priv->hws_ctpool)
			goto err;
		priv->sh->ct_aso_en = 1;
	}
	if (port_attr->nb_counters || (host_priv && host_priv->hws_cpool)) {
		priv->hws_cpool = mlx5_hws_cnt_pool_create(dev, port_attr,
							   nb_queue);
		if (priv->hws_cpool == NULL)
			goto err;
	}
	if (port_attr->nb_aging_objects) {
		if (port_attr->nb_counters == 0) {
			/*
			 * Aging management uses counter. Number counters
			 * requesting should take into account a counter for
			 * each flow rules containing AGE without counter.
			 */
			DRV_LOG(ERR, "Port %u AGE objects are requested (%u) "
				"without counters requesting.",
				dev->data->port_id,
				port_attr->nb_aging_objects);
			rte_errno = EINVAL;
			goto err;
		}
		ret = mlx5_hws_age_pool_init(dev, port_attr, nb_queue);
		if (ret < 0)
			goto err;
	}
	ret = flow_hw_create_vlan(dev);
	if (ret)
		goto err;
	if (_queue_attr)
		mlx5_free(_queue_attr);
	if (port_attr->flags & RTE_FLOW_PORT_FLAG_STRICT_QUEUE)
		priv->hws_strict_queue = 1;
	return 0;
err:
	if (priv->hws_ctpool) {
		flow_hw_ct_pool_destroy(dev, priv->hws_ctpool);
		priv->hws_ctpool = NULL;
	}
	if (priv->ct_mng) {
		flow_hw_ct_mng_destroy(dev, priv->ct_mng);
		priv->ct_mng = NULL;
	}
	if (priv->hws_age_req)
		mlx5_hws_age_pool_destroy(priv);
	if (priv->hws_cpool) {
		mlx5_hws_cnt_pool_destroy(priv->sh, priv->hws_cpool);
		priv->hws_cpool = NULL;
	}
	mlx5_flow_quota_destroy(dev);
	flow_hw_destroy_send_to_kernel_action(priv);
	flow_hw_free_vport_actions(priv);
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		if (priv->hw_drop[i])
			mlx5dr_action_destroy(priv->hw_drop[i]);
		if (priv->hw_tag[i])
			mlx5dr_action_destroy(priv->hw_tag[i]);
	}
	flow_hw_destroy_vlan(dev);
	if (dr_ctx)
		claim_zero(mlx5dr_context_close(dr_ctx));
	for (i = 0; i < nb_q_updated; i++) {
		rte_ring_free(priv->hw_q[i].indir_iq);
		rte_ring_free(priv->hw_q[i].indir_cq);
	}
	mlx5_free(priv->hw_q);
	priv->hw_q = NULL;
	if (priv->acts_ipool) {
		mlx5_ipool_destroy(priv->acts_ipool);
		priv->acts_ipool = NULL;
	}
	if (_queue_attr)
		mlx5_free(_queue_attr);
	if (priv->shared_host) {
		__atomic_fetch_sub(&host_priv->shared_refcnt, 1, __ATOMIC_RELAXED);
		priv->shared_host = NULL;
	}
	mlx5_free(priv->hw_attr);
	priv->hw_attr = NULL;
	/* Do not overwrite the internal errno information. */
	if (ret)
		return ret;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "fail to configure port");
}

/**
 * Release HWS resources.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 */
void
flow_hw_resource_release(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_template_table *tbl;
	struct rte_flow_pattern_template *it;
	struct rte_flow_actions_template *at;
	uint32_t i;

	if (!priv->dr_ctx)
		return;
	flow_hw_rxq_flag_set(dev, false);
	flow_hw_flush_all_ctrl_flows(dev);
	flow_hw_cleanup_tx_repr_tagging(dev);
	flow_hw_cleanup_ctrl_rx_tables(dev);
	while (!LIST_EMPTY(&priv->flow_hw_tbl_ongo)) {
		tbl = LIST_FIRST(&priv->flow_hw_tbl_ongo);
		flow_hw_table_destroy(dev, tbl, NULL);
	}
	while (!LIST_EMPTY(&priv->flow_hw_tbl)) {
		tbl = LIST_FIRST(&priv->flow_hw_tbl);
		flow_hw_table_destroy(dev, tbl, NULL);
	}
	while (!LIST_EMPTY(&priv->flow_hw_itt)) {
		it = LIST_FIRST(&priv->flow_hw_itt);
		flow_hw_pattern_template_destroy(dev, it, NULL);
	}
	while (!LIST_EMPTY(&priv->flow_hw_at)) {
		at = LIST_FIRST(&priv->flow_hw_at);
		flow_hw_actions_template_destroy(dev, at, NULL);
	}
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		if (priv->hw_drop[i])
			mlx5dr_action_destroy(priv->hw_drop[i]);
		if (priv->hw_tag[i])
			mlx5dr_action_destroy(priv->hw_tag[i]);
	}
	flow_hw_destroy_vlan(dev);
	flow_hw_destroy_send_to_kernel_action(priv);
	flow_hw_free_vport_actions(priv);
	if (priv->acts_ipool) {
		mlx5_ipool_destroy(priv->acts_ipool);
		priv->acts_ipool = NULL;
	}
	if (priv->hws_age_req)
		mlx5_hws_age_pool_destroy(priv);
	if (priv->hws_cpool) {
		mlx5_hws_cnt_pool_destroy(priv->sh, priv->hws_cpool);
		priv->hws_cpool = NULL;
	}
	if (priv->hws_ctpool) {
		flow_hw_ct_pool_destroy(dev, priv->hws_ctpool);
		priv->hws_ctpool = NULL;
	}
	if (priv->ct_mng) {
		flow_hw_ct_mng_destroy(dev, priv->ct_mng);
		priv->ct_mng = NULL;
	}
	mlx5_flow_quota_destroy(dev);
	for (i = 0; i < priv->nb_queue; i++) {
		rte_ring_free(priv->hw_q[i].indir_iq);
		rte_ring_free(priv->hw_q[i].indir_cq);
	}
	mlx5_free(priv->hw_q);
	priv->hw_q = NULL;
	claim_zero(mlx5dr_context_close(priv->dr_ctx));
	if (priv->shared_host) {
		struct mlx5_priv *host_priv = priv->shared_host->data->dev_private;
		__atomic_fetch_sub(&host_priv->shared_refcnt, 1, __ATOMIC_RELAXED);
		priv->shared_host = NULL;
	}
	priv->dr_ctx = NULL;
	mlx5_free(priv->hw_attr);
	priv->hw_attr = NULL;
	priv->nb_queue = 0;
}

/* Sets vport tag and mask, for given port, used in HWS rules. */
void
flow_hw_set_port_info(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint16_t port_id = dev->data->port_id;
	struct flow_hw_port_info *info;

	MLX5_ASSERT(port_id < RTE_MAX_ETHPORTS);
	info = &mlx5_flow_hw_port_infos[port_id];
	info->regc_mask = priv->vport_meta_mask;
	info->regc_value = priv->vport_meta_tag;
	info->is_wire = priv->master;
}

/* Clears vport tag and mask used for HWS rules. */
void
flow_hw_clear_port_info(struct rte_eth_dev *dev)
{
	uint16_t port_id = dev->data->port_id;
	struct flow_hw_port_info *info;

	MLX5_ASSERT(port_id < RTE_MAX_ETHPORTS);
	info = &mlx5_flow_hw_port_infos[port_id];
	info->regc_mask = 0;
	info->regc_value = 0;
	info->is_wire = 0;
}

/*
 * Initialize the information of available tag registers and an intersection
 * of all the probed devices' REG_C_Xs.
 * PS. No port concept in steering part, right now it cannot be per port level.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 */
void flow_hw_init_tags_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t meta_mode = priv->sh->config.dv_xmeta_en;
	uint8_t masks = (uint8_t)priv->sh->cdev->config.hca_attr.set_reg_c;
	uint32_t i, j;
	uint8_t reg_off;
	uint8_t unset = 0;
	uint8_t common_masks = 0;

	/*
	 * The CAPA is global for common device but only used in net.
	 * It is shared per eswitch domain.
	 */
	if (!!priv->sh->hws_tags)
		return;
	unset |= 1 << (priv->mtr_color_reg - REG_C_0);
	unset |= 1 << (REG_C_6 - REG_C_0);
	if (priv->sh->config.dv_esw_en)
		unset |= 1 << (REG_C_0 - REG_C_0);
	if (meta_mode == MLX5_XMETA_MODE_META32_HWS)
		unset |= 1 << (REG_C_1 - REG_C_0);
	masks &= ~unset;
	/*
	 * If available tag registers were previously calculated,
	 * calculate a bitmask with an intersection of sets of:
	 * - registers supported by current port,
	 * - previously calculated available tag registers.
	 */
	if (mlx5_flow_hw_avl_tags_init_cnt) {
		MLX5_ASSERT(mlx5_flow_hw_aso_tag == priv->mtr_color_reg);
		for (i = 0; i < MLX5_FLOW_HW_TAGS_MAX; i++) {
			if (mlx5_flow_hw_avl_tags[i] == REG_NON)
				continue;
			reg_off = mlx5_flow_hw_avl_tags[i] - REG_C_0;
			if ((1 << reg_off) & masks)
				common_masks |= (1 << reg_off);
		}
		if (common_masks != masks)
			masks = common_masks;
		else
			goto after_avl_tags;
	}
	j = 0;
	for (i = 0; i < MLX5_FLOW_HW_TAGS_MAX; i++) {
		if ((1 << i) & masks)
			mlx5_flow_hw_avl_tags[j++] = (enum modify_reg)(i + (uint32_t)REG_C_0);
	}
	/* Clear the rest of unusable tag indexes. */
	for (; j < MLX5_FLOW_HW_TAGS_MAX; j++)
		mlx5_flow_hw_avl_tags[j] = REG_NON;
after_avl_tags:
	priv->sh->hws_tags = 1;
	mlx5_flow_hw_aso_tag = (enum modify_reg)priv->mtr_color_reg;
	mlx5_flow_hw_avl_tags_init_cnt++;
}

/*
 * Reset the available tag registers information to NONE.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 */
void flow_hw_clear_tags_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->sh->hws_tags)
		return;
	priv->sh->hws_tags = 0;
	mlx5_flow_hw_avl_tags_init_cnt--;
	if (!mlx5_flow_hw_avl_tags_init_cnt)
		memset(mlx5_flow_hw_avl_tags, REG_NON,
		       sizeof(enum modify_reg) * MLX5_FLOW_HW_TAGS_MAX);
}

uint32_t mlx5_flow_hw_flow_metadata_config_refcnt;
uint8_t mlx5_flow_hw_flow_metadata_esw_en;
uint8_t mlx5_flow_hw_flow_metadata_xmeta_en;

/**
 * Initializes static configuration of META flow items.
 *
 * As a temporary workaround, META flow item is translated to a register,
 * based on statically saved dv_esw_en and dv_xmeta_en device arguments.
 * It is a workaround for flow_hw_get_reg_id() where port specific information
 * is not available at runtime.
 *
 * Values of dv_esw_en and dv_xmeta_en device arguments are taken from the first opened port.
 * This means that each mlx5 port will use the same configuration for translation
 * of META flow items.
 *
 * @param[in] dev
 *    Pointer to Ethernet device.
 */
void
flow_hw_init_flow_metadata_config(struct rte_eth_dev *dev)
{
	uint32_t refcnt;

	refcnt = __atomic_fetch_add(&mlx5_flow_hw_flow_metadata_config_refcnt, 1,
				    __ATOMIC_RELAXED);
	if (refcnt > 0)
		return;
	mlx5_flow_hw_flow_metadata_esw_en = MLX5_SH(dev)->config.dv_esw_en;
	mlx5_flow_hw_flow_metadata_xmeta_en = MLX5_SH(dev)->config.dv_xmeta_en;
}

/**
 * Clears statically stored configuration related to META flow items.
 */
void
flow_hw_clear_flow_metadata_config(void)
{
	uint32_t refcnt;

	refcnt = __atomic_fetch_sub(&mlx5_flow_hw_flow_metadata_config_refcnt, 1,
				    __ATOMIC_RELAXED) - 1;
	if (refcnt > 0)
		return;
	mlx5_flow_hw_flow_metadata_esw_en = 0;
	mlx5_flow_hw_flow_metadata_xmeta_en = 0;
}

static int
flow_hw_conntrack_destroy(struct rte_eth_dev *dev __rte_unused,
			  uint32_t idx,
			  struct rte_flow_error *error)
{
	uint16_t owner = (uint16_t)MLX5_ACTION_CTX_CT_GET_OWNER(idx);
	uint32_t ct_idx = MLX5_ACTION_CTX_CT_GET_IDX(idx);
	struct rte_eth_dev *owndev = &rte_eth_devices[owner];
	struct mlx5_priv *priv = owndev->data->dev_private;
	struct mlx5_aso_ct_pool *pool = priv->hws_ctpool;
	struct mlx5_aso_ct_action *ct;

	ct = mlx5_ipool_get(pool->cts, ct_idx);
	if (!ct) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid CT destruction index");
	}
	__atomic_store_n(&ct->state, ASO_CONNTRACK_FREE,
				 __ATOMIC_RELAXED);
	mlx5_ipool_free(pool->cts, ct_idx);
	return 0;
}

static int
flow_hw_conntrack_query(struct rte_eth_dev *dev, uint32_t queue, uint32_t idx,
			struct rte_flow_action_conntrack *profile,
			void *user_data, bool push,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool = priv->hws_ctpool;
	struct mlx5_aso_ct_action *ct;
	uint16_t owner = (uint16_t)MLX5_ACTION_CTX_CT_GET_OWNER(idx);
	uint32_t ct_idx;

	if (owner != PORT_ID(priv))
		return rte_flow_error_set(error, EACCES,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Can't query CT object owned by another port");
	ct_idx = MLX5_ACTION_CTX_CT_GET_IDX(idx);
	ct = mlx5_ipool_get(pool->cts, ct_idx);
	if (!ct) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid CT query index");
	}
	profile->peer_port = ct->peer;
	profile->is_original_dir = ct->is_original;
	if (mlx5_aso_ct_query_by_wqe(priv->sh, queue, ct, profile, user_data, push))
		return rte_flow_error_set(error, EIO,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Failed to query CT context");
	return 0;
}


static int
flow_hw_conntrack_update(struct rte_eth_dev *dev, uint32_t queue,
			 const struct rte_flow_modify_conntrack *action_conf,
			 uint32_t idx, void *user_data, bool push,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool = priv->hws_ctpool;
	struct mlx5_aso_ct_action *ct;
	const struct rte_flow_action_conntrack *new_prf;
	uint16_t owner = (uint16_t)MLX5_ACTION_CTX_CT_GET_OWNER(idx);
	uint32_t ct_idx;
	int ret = 0;

	if (PORT_ID(priv) != owner)
		return rte_flow_error_set(error, EACCES,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "Can't update CT object owned by another port");
	ct_idx = MLX5_ACTION_CTX_CT_GET_IDX(idx);
	ct = mlx5_ipool_get(pool->cts, ct_idx);
	if (!ct) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid CT update index");
	}
	new_prf = &action_conf->new_ct;
	if (action_conf->direction)
		ct->is_original = !!new_prf->is_original_dir;
	if (action_conf->state) {
		/* Only validate the profile when it needs to be updated. */
		ret = mlx5_validate_action_ct(dev, new_prf, error);
		if (ret)
			return ret;
		ret = mlx5_aso_ct_update_by_wqe(priv->sh, queue, ct, new_prf,
						user_data, push);
		if (ret)
			return rte_flow_error_set(error, EIO,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"Failed to send CT context update WQE");
		if (queue != MLX5_HW_INV_QUEUE)
			return 0;
		/* Block until ready or a failure in synchronous mode. */
		ret = mlx5_aso_ct_available(priv->sh, queue, ct);
		if (ret)
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "Timeout to get the CT update");
	}
	return ret;
}

static struct rte_flow_action_handle *
flow_hw_conntrack_create(struct rte_eth_dev *dev, uint32_t queue,
			 const struct rte_flow_action_conntrack *pro,
			 void *user_data, bool push,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool = priv->hws_ctpool;
	struct mlx5_aso_ct_action *ct;
	uint32_t ct_idx = 0;
	int ret;
	bool async = !!(queue != MLX5_HW_INV_QUEUE);

	if (!pool) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				   "CT is not enabled");
		return 0;
	}
	ct = mlx5_ipool_zmalloc(pool->cts, &ct_idx);
	if (!ct) {
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				   "Failed to allocate CT object");
		return 0;
	}
	ct->offset = ct_idx - 1;
	ct->is_original = !!pro->is_original_dir;
	ct->peer = pro->peer_port;
	ct->pool = pool;
	if (mlx5_aso_ct_update_by_wqe(priv->sh, queue, ct, pro, user_data, push)) {
		mlx5_ipool_free(pool->cts, ct_idx);
		rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
				   "Failed to update CT");
		return 0;
	}
	if (!async) {
		ret = mlx5_aso_ct_available(priv->sh, queue, ct);
		if (ret) {
			mlx5_ipool_free(pool->cts, ct_idx);
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "Timeout to get the CT update");
			return 0;
		}
	}
	return (struct rte_flow_action_handle *)(uintptr_t)
		MLX5_ACTION_CTX_CT_GEN_IDX(PORT_ID(priv), ct_idx);
}

/**
 * Validate shared action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   Which queue to be used.
 * @param[in] attr
 *   Operation attribute.
 * @param[in] conf
 *   Indirect action configuration.
 * @param[in] action
 *   rte_flow action detail.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
static int
flow_hw_action_handle_validate(struct rte_eth_dev *dev, uint32_t queue,
			       const struct rte_flow_op_attr *attr,
			       const struct rte_flow_indir_action_conf *conf,
			       const struct rte_flow_action *action,
			       void *user_data,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	RTE_SET_USED(attr);
	RTE_SET_USED(queue);
	RTE_SET_USED(user_data);
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_AGE:
		if (!priv->hws_age_req)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "aging pool not initialized");
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		if (!priv->hws_cpool)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "counters pool not initialized");
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		if (priv->hws_ctpool == NULL)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  NULL,
						  "CT pool not initialized");
		return mlx5_validate_action_ct(dev, action->conf, error);
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		return flow_hw_validate_action_meter_mark(dev, action, error);
	case RTE_FLOW_ACTION_TYPE_RSS:
		return flow_dv_action_validate(dev, conf, action, error);
	case RTE_FLOW_ACTION_TYPE_QUOTA:
		return 0;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "action type not supported");
	}
	return 0;
}

static __rte_always_inline bool
flow_hw_action_push(const struct rte_flow_op_attr *attr)
{
	return attr ? !attr->postpone : true;
}

static __rte_always_inline struct mlx5_hw_q_job *
flow_hw_job_get(struct mlx5_priv *priv, uint32_t queue)
{
	return priv->hw_q[queue].job[--priv->hw_q[queue].job_idx];
}

static __rte_always_inline void
flow_hw_job_put(struct mlx5_priv *priv, uint32_t queue)
{
	priv->hw_q[queue].job_idx++;
}

static __rte_always_inline struct mlx5_hw_q_job *
flow_hw_action_job_init(struct mlx5_priv *priv, uint32_t queue,
			const struct rte_flow_action_handle *handle,
			void *user_data, void *query_data,
			enum mlx5_hw_job_type type,
			struct rte_flow_error *error)
{
	struct mlx5_hw_q_job *job;

	MLX5_ASSERT(queue != MLX5_HW_INV_QUEUE);
	if (unlikely(!priv->hw_q[queue].job_idx)) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Action destroy failed due to queue full.");
		return NULL;
	}
	job = flow_hw_job_get(priv, queue);
	job->type = type;
	job->action = handle;
	job->user_data = user_data;
	job->query.user = query_data;
	return job;
}

static __rte_always_inline void
flow_hw_action_finalize(struct rte_eth_dev *dev, uint32_t queue,
			struct mlx5_hw_q_job *job,
			bool push, bool aso, bool status)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	if (likely(status)) {
		if (push)
			__flow_hw_push_action(dev, queue);
		if (!aso)
			rte_ring_enqueue(push ?
					 priv->hw_q[queue].indir_cq :
					 priv->hw_q[queue].indir_iq,
					 job);
	} else {
		flow_hw_job_put(priv, queue);
	}
}

/**
 * Create shared action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   Which queue to be used.
 * @param[in] attr
 *   Operation attribute.
 * @param[in] conf
 *   Indirect action configuration.
 * @param[in] action
 *   rte_flow action detail.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Action handle on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_action_handle *
flow_hw_action_handle_create(struct rte_eth_dev *dev, uint32_t queue,
			     const struct rte_flow_op_attr *attr,
			     const struct rte_flow_indir_action_conf *conf,
			     const struct rte_flow_action *action,
			     void *user_data,
			     struct rte_flow_error *error)
{
	struct rte_flow_action_handle *handle = NULL;
	struct mlx5_hw_q_job *job = NULL;
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_age *age;
	struct mlx5_aso_mtr *aso_mtr;
	cnt_id_t cnt_id;
	uint32_t mtr_id;
	uint32_t age_idx;
	bool push = flow_hw_action_push(attr);
	bool aso = false;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, NULL, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_CREATE,
					      error);
		if (!job)
			return NULL;
	}
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_AGE:
		if (priv->hws_strict_queue) {
			struct mlx5_age_info *info = GET_PORT_AGE_INFO(priv);

			if (queue >= info->hw_q_age->nb_rings) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   NULL,
						   "Invalid queue ID for indirect AGE.");
				rte_errno = EINVAL;
				return NULL;
			}
		}
		age = action->conf;
		age_idx = mlx5_hws_age_action_create(priv, queue, true, age,
						     0, error);
		if (age_idx == 0) {
			rte_flow_error_set(error, ENODEV,
					   RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					   "AGE are not configured!");
		} else {
			age_idx = (MLX5_INDIRECT_ACTION_TYPE_AGE <<
				   MLX5_INDIRECT_ACTION_TYPE_OFFSET) | age_idx;
			handle =
			    (struct rte_flow_action_handle *)(uintptr_t)age_idx;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		if (mlx5_hws_cnt_shared_get(priv->hws_cpool, &cnt_id, 0))
			rte_flow_error_set(error, ENODEV,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"counter are not configured!");
		else
			handle = (struct rte_flow_action_handle *)
				 (uintptr_t)cnt_id;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		aso = true;
		handle = flow_hw_conntrack_create(dev, queue, action->conf, job,
						  push, error);
		break;
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		aso = true;
		aso_mtr = flow_hw_meter_mark_alloc(dev, queue, action, job, push);
		if (!aso_mtr)
			break;
		mtr_id = (MLX5_INDIRECT_ACTION_TYPE_METER_MARK <<
			MLX5_INDIRECT_ACTION_TYPE_OFFSET) | (aso_mtr->fm.meter_id);
		handle = (struct rte_flow_action_handle *)(uintptr_t)mtr_id;
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		handle = flow_dv_action_create(dev, conf, action, error);
		break;
	case RTE_FLOW_ACTION_TYPE_QUOTA:
		aso = true;
		handle = mlx5_quota_alloc(dev, queue, action->conf,
					  job, push, error);
		break;
	default:
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "action type not supported");
		break;
	}
	if (job) {
		job->action = handle;
		flow_hw_action_finalize(dev, queue, job, push, aso,
					handle != NULL);
	}
	return handle;
}

/**
 * Update shared action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   Which queue to be used.
 * @param[in] attr
 *   Operation attribute.
 * @param[in] handle
 *   Action handle to be updated.
 * @param[in] update
 *   Update value.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_action_handle_update(struct rte_eth_dev *dev, uint32_t queue,
			     const struct rte_flow_op_attr *attr,
			     struct rte_flow_action_handle *handle,
			     const void *update,
			     void *user_data,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	const struct rte_flow_modify_conntrack *ct_conf =
		(const struct rte_flow_modify_conntrack *)update;
	const struct rte_flow_update_meter_mark *upd_meter_mark =
		(const struct rte_flow_update_meter_mark *)update;
	const struct rte_flow_action_meter_mark *meter_mark;
	struct mlx5_hw_q_job *job = NULL;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_flow_meter_info *fm;
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	int ret = 0;
	bool push = flow_hw_action_push(attr);
	bool aso = false;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_UPDATE,
					      error);
		if (!job)
			return -rte_errno;
	}
	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		ret = mlx5_hws_age_action_update(priv, idx, update, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		if (ct_conf->state)
			aso = true;
		ret = flow_hw_conntrack_update(dev, queue, update, act_idx,
					       job, push, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		aso = true;
		meter_mark = &upd_meter_mark->meter_mark;
		/* Find ASO object. */
		aso_mtr = mlx5_ipool_get(pool->idx_pool, idx);
		if (!aso_mtr) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Invalid meter_mark update index");
			break;
		}
		fm = &aso_mtr->fm;
		if (upd_meter_mark->profile_valid)
			fm->profile = (struct mlx5_flow_meter_profile *)
							(meter_mark->profile);
		if (upd_meter_mark->color_mode_valid)
			fm->color_aware = meter_mark->color_mode;
		if (upd_meter_mark->init_color_valid)
			aso_mtr->init_color = (meter_mark->color_mode) ?
				meter_mark->init_color : RTE_COLOR_GREEN;
		if (upd_meter_mark->state_valid)
			fm->is_enable = meter_mark->state;
		/* Update ASO flow meter by wqe. */
		if (mlx5_aso_meter_update_by_wqe(priv->sh, queue,
						 aso_mtr, &priv->mtr_bulk, job, push)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to update ASO meter WQE");
			break;
		}
		/* Wait for ASO object completion. */
		if (queue == MLX5_HW_INV_QUEUE &&
		    mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to wait for ASO meter CQE");
		}
		break;
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		ret = flow_dv_action_update(dev, handle, update, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		aso = true;
		ret = mlx5_quota_query_update(dev, queue, handle, update, NULL,
					      job, push, error);
		break;
	default:
		ret = -ENOTSUP;
		rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "action type not supported");
		break;
	}
	if (job)
		flow_hw_action_finalize(dev, queue, job, push, aso, ret == 0);
	return ret;
}

/**
 * Destroy shared action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   Which queue to be used.
 * @param[in] attr
 *   Operation attribute.
 * @param[in] handle
 *   Action handle to be destroyed.
 * @param[in] user_data
 *   Pointer to the user_data.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, negative value otherwise and rte_errno is set.
 */
static int
flow_hw_action_handle_destroy(struct rte_eth_dev *dev, uint32_t queue,
			      const struct rte_flow_op_attr *attr,
			      struct rte_flow_action_handle *handle,
			      void *user_data,
			      struct rte_flow_error *error)
{
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t age_idx = act_idx & MLX5_HWS_AGE_IDX_MASK;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_hw_q_job *job = NULL;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_flow_meter_info *fm;
	bool push = flow_hw_action_push(attr);
	bool aso = false;
	int ret = 0;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_DESTROY,
					      error);
		if (!job)
			return -rte_errno;
	}
	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		ret = mlx5_hws_age_action_destroy(priv, age_idx, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		age_idx = mlx5_hws_cnt_age_get(priv->hws_cpool, act_idx);
		if (age_idx != 0)
			/*
			 * If this counter belongs to indirect AGE, here is the
			 * time to update the AGE.
			 */
			mlx5_hws_age_nb_cnt_decrease(priv, age_idx);
		mlx5_hws_cnt_shared_put(priv->hws_cpool, &act_idx);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		ret = flow_hw_conntrack_destroy(dev, act_idx, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		aso_mtr = mlx5_ipool_get(pool->idx_pool, idx);
		if (!aso_mtr) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Invalid meter_mark destroy index");
			break;
		}
		fm = &aso_mtr->fm;
		fm->is_enable = 0;
		/* Update ASO flow meter by wqe. */
		if (mlx5_aso_meter_update_by_wqe(priv->sh, queue, aso_mtr,
						 &priv->mtr_bulk, job, push)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to update ASO meter WQE");
			break;
		}
		/* Wait for ASO object completion. */
		if (queue == MLX5_HW_INV_QUEUE &&
		    mlx5_aso_mtr_wait(priv->sh, MLX5_HW_INV_QUEUE, aso_mtr)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to wait for ASO meter CQE");
			break;
		}
		if (!job)
			mlx5_ipool_free(pool->idx_pool, idx);
		else
			aso = true;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		ret = flow_dv_action_destroy(dev, handle, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		break;
	default:
		ret = -ENOTSUP;
		rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "action type not supported");
		break;
	}
	if (job)
		flow_hw_action_finalize(dev, queue, job, push, aso, ret == 0);
	return ret;
}

static int
flow_hw_query_counter(const struct rte_eth_dev *dev, uint32_t counter,
		      void *data, struct rte_flow_error *error)
{
	struct mlx5_hws_cnt_pool *hpool;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hws_cnt *cnt;
	struct rte_flow_query_count *qc = data;
	uint32_t iidx;
	uint64_t pkts, bytes;

	if (!mlx5_hws_cnt_id_valid(counter))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"counter are not available");
	hpool = mlx5_hws_cnt_host_pool(priv->hws_cpool);
	iidx = mlx5_hws_cnt_iidx(hpool, counter);
	cnt = &hpool->pool[iidx];
	__hws_cnt_query_raw(priv->hws_cpool, counter, &pkts, &bytes);
	qc->hits_set = 1;
	qc->bytes_set = 1;
	qc->hits = pkts - cnt->reset.hits;
	qc->bytes = bytes - cnt->reset.bytes;
	if (qc->reset) {
		cnt->reset.bytes = bytes;
		cnt->reset.hits = pkts;
	}
	return 0;
}

/**
 * Query a flow rule AGE action for aging information.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] age_idx
 *   Index of AGE action parameter.
 * @param[out] data
 *   Data retrieved by the query.
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_query_age(const struct rte_eth_dev *dev, uint32_t age_idx, void *data,
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct mlx5_indexed_pool *ipool = age_info->ages_ipool;
	struct mlx5_hws_age_param *param = mlx5_ipool_get(ipool, age_idx);
	struct rte_flow_query_age *resp = data;

	if (!param || !param->timeout)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "age data not available");
	switch (__atomic_load_n(&param->state, __ATOMIC_RELAXED)) {
	case HWS_AGE_AGED_OUT_REPORTED:
	case HWS_AGE_AGED_OUT_NOT_REPORTED:
		resp->aged = 1;
		break;
	case HWS_AGE_CANDIDATE:
	case HWS_AGE_CANDIDATE_INSIDE_RING:
		resp->aged = 0;
		break;
	case HWS_AGE_FREE:
		/*
		 * When state is FREE the flow itself should be invalid.
		 * Fall-through.
		 */
	default:
		MLX5_ASSERT(0);
		break;
	}
	resp->sec_since_last_hit_valid = !resp->aged;
	if (resp->sec_since_last_hit_valid)
		resp->sec_since_last_hit = __atomic_load_n
				 (&param->sec_since_last_hit, __ATOMIC_RELAXED);
	return 0;
}

static int
flow_hw_query(struct rte_eth_dev *dev, struct rte_flow *flow,
	      const struct rte_flow_action *actions, void *data,
	      struct rte_flow_error *error)
{
	int ret = -EINVAL;
	struct rte_flow_hw *hw_flow = (struct rte_flow_hw *)flow;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_hw_query_counter(dev, hw_flow->cnt_id, data,
						    error);
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			ret = flow_hw_query_age(dev, hw_flow->age_idx, data,
						error);
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
 * Validate indirect action.
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
 *   0 on success, otherwise negative errno value.
 */
static int
flow_hw_action_validate(struct rte_eth_dev *dev,
			const struct rte_flow_indir_action_conf *conf,
			const struct rte_flow_action *action,
			struct rte_flow_error *err)
{
	return flow_hw_action_handle_validate(dev, MLX5_HW_INV_QUEUE, NULL,
					      conf, action, NULL, err);
}

/**
 * Create indirect action.
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
static struct rte_flow_action_handle *
flow_hw_action_create(struct rte_eth_dev *dev,
		       const struct rte_flow_indir_action_conf *conf,
		       const struct rte_flow_action *action,
		       struct rte_flow_error *err)
{
	return flow_hw_action_handle_create(dev, MLX5_HW_INV_QUEUE,
					    NULL, conf, action, NULL, err);
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
static int
flow_hw_action_destroy(struct rte_eth_dev *dev,
		       struct rte_flow_action_handle *handle,
		       struct rte_flow_error *error)
{
	return flow_hw_action_handle_destroy(dev, MLX5_HW_INV_QUEUE,
			NULL, handle, NULL, error);
}

/**
 * Updates in place shared action configuration.
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
static int
flow_hw_action_update(struct rte_eth_dev *dev,
		      struct rte_flow_action_handle *handle,
		      const void *update,
		      struct rte_flow_error *err)
{
	return flow_hw_action_handle_update(dev, MLX5_HW_INV_QUEUE,
			NULL, handle, update, NULL, err);
}

static int
flow_hw_action_handle_query(struct rte_eth_dev *dev, uint32_t queue,
			    const struct rte_flow_op_attr *attr,
			    const struct rte_flow_action_handle *handle,
			    void *data, void *user_data,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_q_job *job = NULL;
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t age_idx = act_idx & MLX5_HWS_AGE_IDX_MASK;
	int ret;
	bool push = flow_hw_action_push(attr);
	bool aso = false;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      data, MLX5_HW_Q_JOB_TYPE_QUERY,
					      error);
		if (!job)
			return -rte_errno;
	}
	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		ret = flow_hw_query_age(dev, age_idx, data, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		ret = flow_hw_query_counter(dev, act_idx, data, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		aso = true;
		if (job)
			job->query.user = data;
		ret = flow_hw_conntrack_query(dev, queue, act_idx, data,
					      job, push, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		aso = true;
		ret = mlx5_quota_query(dev, queue, handle, data,
				       job, push, error);
		break;
	default:
		ret = -ENOTSUP;
		rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "action type not supported");
		break;
	}
	if (job)
		flow_hw_action_finalize(dev, queue, job, push, aso, ret == 0);
	return ret;
}

static int
flow_hw_async_action_handle_query_update
			(struct rte_eth_dev *dev, uint32_t queue,
			 const struct rte_flow_op_attr *attr,
			 struct rte_flow_action_handle *handle,
			 const void *update, void *query,
			 enum rte_flow_query_update_mode qu_mode,
			 void *user_data, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool push = flow_hw_action_push(attr);
	bool aso = false;
	struct mlx5_hw_q_job *job = NULL;
	int ret = 0;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      query,
					      MLX5_HW_Q_JOB_TYPE_UPDATE_QUERY,
					      error);
		if (!job)
			return -rte_errno;
	}
	switch (MLX5_INDIRECT_ACTION_TYPE_GET(handle)) {
	case MLX5_INDIRECT_ACTION_TYPE_QUOTA:
		if (qu_mode != RTE_FLOW_QU_QUERY_FIRST) {
			ret = rte_flow_error_set
				(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION_CONF,
				 NULL, "quota action must query before update");
			break;
		}
		aso = true;
		ret = mlx5_quota_query_update(dev, queue, handle,
					      update, query, job, push, error);
		break;
	default:
		ret = rte_flow_error_set(error, ENOTSUP,
					 RTE_FLOW_ERROR_TYPE_ACTION_CONF, NULL, "update and query not supportred");
	}
	if (job)
		flow_hw_action_finalize(dev, queue, job, push, aso, ret == 0);
	return ret;
}

static int
flow_hw_action_query(struct rte_eth_dev *dev,
		     const struct rte_flow_action_handle *handle, void *data,
		     struct rte_flow_error *error)
{
	return flow_hw_action_handle_query(dev, MLX5_HW_INV_QUEUE, NULL,
			handle, data, NULL, error);
}

static int
flow_hw_action_query_update(struct rte_eth_dev *dev,
			    struct rte_flow_action_handle *handle,
			    const void *update, void *query,
			    enum rte_flow_query_update_mode qu_mode,
			    struct rte_flow_error *error)
{
	return flow_hw_async_action_handle_query_update(dev, MLX5_HW_INV_QUEUE,
							NULL, handle, update,
							query, qu_mode, NULL,
							error);
}

/**
 * Get aged-out flows of a given port on the given HWS flow queue.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] queue_id
 *   Flow queue to query. Ignored when RTE_FLOW_PORT_FLAG_STRICT_QUEUE not set.
 * @param[in, out] contexts
 *   The address of an array of pointers to the aged-out flows contexts.
 * @param[in] nb_contexts
 *   The length of context array pointers.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   if nb_contexts is 0, return the amount of all aged contexts.
 *   if nb_contexts is not 0 , return the amount of aged flows reported
 *   in the context array, otherwise negative errno value.
 */
static int
flow_hw_get_q_aged_flows(struct rte_eth_dev *dev, uint32_t queue_id,
			 void **contexts, uint32_t nb_contexts,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_age_info *age_info = GET_PORT_AGE_INFO(priv);
	struct rte_ring *r;
	int nb_flows = 0;

	if (nb_contexts && !contexts)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "empty context");
	if (priv->hws_strict_queue) {
		if (queue_id >= age_info->hw_q_age->nb_rings)
			return rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						NULL, "invalid queue id");
		r = age_info->hw_q_age->aged_lists[queue_id];
	} else {
		r = age_info->hw_age.aged_list;
		MLX5_AGE_SET(age_info, MLX5_AGE_TRIGGER);
	}
	if (nb_contexts == 0)
		return rte_ring_count(r);
	while ((uint32_t)nb_flows < nb_contexts) {
		uint32_t age_idx;

		if (rte_ring_dequeue_elem(r, &age_idx, sizeof(uint32_t)) < 0)
			break;
		/* get the AGE context if the aged-out index is still valid. */
		contexts[nb_flows] = mlx5_hws_age_context_get(priv, age_idx);
		if (!contexts[nb_flows])
			continue;
		nb_flows++;
	}
	return nb_flows;
}

/**
 * Get aged-out flows.
 *
 * This function is relevant only if RTE_FLOW_PORT_FLAG_STRICT_QUEUE isn't set.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] contexts
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
 */
static int
flow_hw_get_aged_flows(struct rte_eth_dev *dev, void **contexts,
		       uint32_t nb_contexts, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->hws_strict_queue)
		DRV_LOG(WARNING,
			"port %u get aged flows called in strict queue mode.",
			dev->data->port_id);
	return flow_hw_get_q_aged_flows(dev, 0, contexts, nb_contexts, error);
}

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops = {
	.info_get = flow_hw_info_get,
	.configure = flow_hw_configure,
	.pattern_validate = flow_hw_pattern_validate,
	.pattern_template_create = flow_hw_pattern_template_create,
	.pattern_template_destroy = flow_hw_pattern_template_destroy,
	.actions_validate = flow_hw_actions_validate,
	.actions_template_create = flow_hw_actions_template_create,
	.actions_template_destroy = flow_hw_actions_template_destroy,
	.template_table_create = flow_hw_template_table_create,
	.template_table_destroy = flow_hw_table_destroy,
	.async_flow_create = flow_hw_async_flow_create,
	.async_flow_create_by_index = flow_hw_async_flow_create_by_index,
	.async_flow_update = flow_hw_async_flow_update,
	.async_flow_destroy = flow_hw_async_flow_destroy,
	.pull = flow_hw_pull,
	.push = flow_hw_push,
	.async_action_create = flow_hw_action_handle_create,
	.async_action_destroy = flow_hw_action_handle_destroy,
	.async_action_update = flow_hw_action_handle_update,
	.async_action_query_update = flow_hw_async_action_handle_query_update,
	.async_action_query = flow_hw_action_handle_query,
	.action_validate = flow_hw_action_validate,
	.action_create = flow_hw_action_create,
	.action_destroy = flow_hw_action_destroy,
	.action_update = flow_hw_action_update,
	.action_query = flow_hw_action_query,
	.action_query_update = flow_hw_action_query_update,
	.query = flow_hw_query,
	.get_aged_flows = flow_hw_get_aged_flows,
	.get_q_aged_flows = flow_hw_get_q_aged_flows,
	.item_create = flow_dv_item_create,
	.item_release = flow_dv_item_release,
};

/**
 * Creates a control flow using flow template API on @p proxy_dev device,
 * on behalf of @p owner_dev device.
 *
 * This function uses locks internally to synchronize access to the
 * flow queue.
 *
 * Created flow is stored in private list associated with @p proxy_dev device.
 *
 * @param owner_dev
 *   Pointer to Ethernet device on behalf of which flow is created.
 * @param proxy_dev
 *   Pointer to Ethernet device on which flow is created.
 * @param table
 *   Pointer to flow table.
 * @param items
 *   Pointer to flow rule items.
 * @param item_template_idx
 *   Index of an item template associated with @p table.
 * @param actions
 *   Pointer to flow rule actions.
 * @param action_template_idx
 *   Index of an action template associated with @p table.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno set.
 */
static __rte_unused int
flow_hw_create_ctrl_flow(struct rte_eth_dev *owner_dev,
			 struct rte_eth_dev *proxy_dev,
			 struct rte_flow_template_table *table,
			 struct rte_flow_item items[],
			 uint8_t item_template_idx,
			 struct rte_flow_action actions[],
			 uint8_t action_template_idx)
{
	struct mlx5_priv *priv = proxy_dev->data->dev_private;
	uint32_t queue = CTRL_QUEUE_ID(priv);
	struct rte_flow_op_attr op_attr = {
		.postpone = 0,
	};
	struct rte_flow *flow = NULL;
	struct mlx5_hw_ctrl_flow *entry = NULL;
	int ret;

	rte_spinlock_lock(&priv->hw_ctrl_lock);
	entry = mlx5_malloc(MLX5_MEM_ZERO | MLX5_MEM_SYS, sizeof(*entry),
			    0, SOCKET_ID_ANY);
	if (!entry) {
		DRV_LOG(ERR, "port %u not enough memory to create control flows",
			proxy_dev->data->port_id);
		rte_errno = ENOMEM;
		ret = -rte_errno;
		goto error;
	}
	flow = flow_hw_async_flow_create(proxy_dev, queue, &op_attr, table,
					 items, item_template_idx,
					 actions, action_template_idx,
					 NULL, NULL);
	if (!flow) {
		DRV_LOG(ERR, "port %u failed to enqueue create control"
			" flow operation", proxy_dev->data->port_id);
		ret = -rte_errno;
		goto error;
	}
	ret = flow_hw_push(proxy_dev, queue, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to drain control flow queue",
			proxy_dev->data->port_id);
		goto error;
	}
	ret = __flow_hw_pull_comp(proxy_dev, queue, 1, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to insert control flow",
			proxy_dev->data->port_id);
		rte_errno = EINVAL;
		ret = -rte_errno;
		goto error;
	}
	entry->owner_dev = owner_dev;
	entry->flow = flow;
	LIST_INSERT_HEAD(&priv->hw_ctrl_flows, entry, next);
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	return 0;
error:
	if (entry)
		mlx5_free(entry);
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	return ret;
}

/**
 * Destroys a control flow @p flow using flow template API on @p dev device.
 *
 * This function uses locks internally to synchronize access to the
 * flow queue.
 *
 * If the @p flow is stored on any private list/pool, then caller must free up
 * the relevant resources.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param flow
 *   Pointer to flow rule.
 *
 * @return
 *   0 on success, non-zero value otherwise.
 */
static int
flow_hw_destroy_ctrl_flow(struct rte_eth_dev *dev, struct rte_flow *flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t queue = CTRL_QUEUE_ID(priv);
	struct rte_flow_op_attr op_attr = {
		.postpone = 0,
	};
	int ret;

	rte_spinlock_lock(&priv->hw_ctrl_lock);
	ret = flow_hw_async_flow_destroy(dev, queue, &op_attr, flow, NULL, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to enqueue destroy control"
			" flow operation", dev->data->port_id);
		goto exit;
	}
	ret = flow_hw_push(dev, queue, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to drain control flow queue",
			dev->data->port_id);
		goto exit;
	}
	ret = __flow_hw_pull_comp(dev, queue, 1, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to destroy control flow",
			dev->data->port_id);
		rte_errno = EINVAL;
		ret = -rte_errno;
		goto exit;
	}
exit:
	rte_spinlock_unlock(&priv->hw_ctrl_lock);
	return ret;
}

/**
 * Destroys control flows created on behalf of @p owner device on @p dev device.
 *
 * @param dev
 *   Pointer to Ethernet device on which control flows were created.
 * @param owner
 *   Pointer to Ethernet device owning control flows.
 *
 * @return
 *   0 on success, otherwise negative error code is returned and
 *   rte_errno is set.
 */
static int
flow_hw_flush_ctrl_flows_owned_by(struct rte_eth_dev *dev, struct rte_eth_dev *owner)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_ctrl_flow *cf;
	struct mlx5_hw_ctrl_flow *cf_next;
	int ret;

	cf = LIST_FIRST(&priv->hw_ctrl_flows);
	while (cf != NULL) {
		cf_next = LIST_NEXT(cf, next);
		if (cf->owner_dev == owner) {
			ret = flow_hw_destroy_ctrl_flow(dev, cf->flow);
			if (ret) {
				rte_errno = ret;
				return -ret;
			}
			LIST_REMOVE(cf, next);
			mlx5_free(cf);
		}
		cf = cf_next;
	}
	return 0;
}

/**
 * Destroys control flows created for @p owner_dev device.
 *
 * @param owner_dev
 *   Pointer to Ethernet device owning control flows.
 *
 * @return
 *   0 on success, otherwise negative error code is returned and
 *   rte_errno is set.
 */
int
mlx5_flow_hw_flush_ctrl_flows(struct rte_eth_dev *owner_dev)
{
	struct mlx5_priv *owner_priv = owner_dev->data->dev_private;
	struct rte_eth_dev *proxy_dev;
	uint16_t owner_port_id = owner_dev->data->port_id;
	uint16_t proxy_port_id = owner_dev->data->port_id;
	int ret;

	/* Flush all flows created by this port for itself. */
	ret = flow_hw_flush_ctrl_flows_owned_by(owner_dev, owner_dev);
	if (ret)
		return ret;
	/* Flush all flows created for this port on proxy port. */
	if (owner_priv->sh->config.dv_esw_en) {
		ret = rte_flow_pick_transfer_proxy(owner_port_id, &proxy_port_id, NULL);
		if (ret == -ENODEV) {
			DRV_LOG(DEBUG, "Unable to find transfer proxy port for port %u. It was "
				       "probably closed. Control flows were cleared.",
				       owner_port_id);
			rte_errno = 0;
			return 0;
		} else if (ret) {
			DRV_LOG(ERR, "Unable to find proxy port for port %u (ret = %d)",
				owner_port_id, ret);
			return ret;
		}
		proxy_dev = &rte_eth_devices[proxy_port_id];
	} else {
		proxy_dev = owner_dev;
	}
	return flow_hw_flush_ctrl_flows_owned_by(proxy_dev, owner_dev);
}

/**
 * Destroys all control flows created on @p dev device.
 *
 * @param owner_dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, otherwise negative error code is returned and
 *   rte_errno is set.
 */
static int
flow_hw_flush_all_ctrl_flows(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_ctrl_flow *cf;
	struct mlx5_hw_ctrl_flow *cf_next;
	int ret;

	cf = LIST_FIRST(&priv->hw_ctrl_flows);
	while (cf != NULL) {
		cf_next = LIST_NEXT(cf, next);
		ret = flow_hw_destroy_ctrl_flow(dev, cf->flow);
		if (ret) {
			rte_errno = ret;
			return -ret;
		}
		LIST_REMOVE(cf, next);
		mlx5_free(cf);
		cf = cf_next;
	}
	return 0;
}

int
mlx5_flow_hw_esw_create_sq_miss_flow(struct rte_eth_dev *dev, uint32_t sqn)
{
	uint16_t port_id = dev->data->port_id;
	struct rte_flow_item_ethdev esw_mgr_spec = {
		.port_id = MLX5_REPRESENTED_PORT_ESW_MGR,
	};
	struct rte_flow_item_ethdev esw_mgr_mask = {
		.port_id = MLX5_REPRESENTED_PORT_ESW_MGR,
	};
	struct rte_flow_item_tag reg_c0_spec = {
		.index = (uint8_t)REG_C_0,
		.data = flow_hw_esw_mgr_regc_marker(dev),
	};
	struct rte_flow_item_tag reg_c0_mask = {
		.index = 0xff,
		.data = flow_hw_esw_mgr_regc_marker_mask(dev),
	};
	struct mlx5_rte_flow_item_sq sq_spec = {
		.queue = sqn,
	};
	struct rte_flow_action_ethdev port = {
		.port_id = port_id,
	};
	struct rte_flow_item items[3] = { { 0 } };
	struct rte_flow_action actions[3] = { { 0 } };
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	uint16_t proxy_port_id = dev->data->port_id;
	int ret;

	ret = rte_flow_pick_transfer_proxy(port_id, &proxy_port_id, NULL);
	if (ret) {
		DRV_LOG(ERR, "Unable to pick transfer proxy port for port %u. Transfer proxy "
			     "port must be present to create default SQ miss flows.",
			     port_id);
		return ret;
	}
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	if (!proxy_priv->dr_ctx) {
		DRV_LOG(DEBUG, "Transfer proxy port (port %u) of port %u must be configured "
			       "for HWS to create default SQ miss flows. Default flows will "
			       "not be created.",
			       proxy_port_id, port_id);
		return 0;
	}
	if (!proxy_priv->hw_esw_sq_miss_root_tbl ||
	    !proxy_priv->hw_esw_sq_miss_tbl) {
		DRV_LOG(ERR, "Transfer proxy port (port %u) of port %u was configured, but "
			     "default flow tables were not created.",
			     proxy_port_id, port_id);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/*
	 * Create a root SQ miss flow rule - match E-Switch Manager and SQ,
	 * and jump to group 1.
	 */
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
		.spec = &esw_mgr_spec,
		.mask = &esw_mgr_mask,
	};
	items[1] = (struct rte_flow_item){
		.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ,
		.spec = &sq_spec,
	};
	items[2] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_END,
	};
	actions[0] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
	};
	actions[1] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_JUMP,
	};
	actions[2] = (struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	ret = flow_hw_create_ctrl_flow(dev, proxy_dev, proxy_priv->hw_esw_sq_miss_root_tbl,
				       items, 0, actions, 0);
	if (ret) {
		DRV_LOG(ERR, "Port %u failed to create root SQ miss flow rule for SQ %u, ret %d",
			port_id, sqn, ret);
		return ret;
	}
	/*
	 * Create a non-root SQ miss flow rule - match REG_C_0 marker and SQ,
	 * and forward to port.
	 */
	items[0] = (struct rte_flow_item){
		.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG,
		.spec = &reg_c0_spec,
		.mask = &reg_c0_mask,
	};
	items[1] = (struct rte_flow_item){
		.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ,
		.spec = &sq_spec,
	};
	items[2] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_END,
	};
	actions[0] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
		.conf = &port,
	};
	actions[1] = (struct rte_flow_action){
		.type = RTE_FLOW_ACTION_TYPE_END,
	};
	ret = flow_hw_create_ctrl_flow(dev, proxy_dev, proxy_priv->hw_esw_sq_miss_tbl,
				       items, 0, actions, 0);
	if (ret) {
		DRV_LOG(ERR, "Port %u failed to create HWS SQ miss flow rule for SQ %u, ret %d",
			port_id, sqn, ret);
		return ret;
	}
	return 0;
}

int
mlx5_flow_hw_esw_create_default_jump_flow(struct rte_eth_dev *dev)
{
	uint16_t port_id = dev->data->port_id;
	struct rte_flow_item_ethdev port_spec = {
		.port_id = port_id,
	};
	struct rte_flow_item items[] = {
		{
			.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &port_spec,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action_jump jump = {
		.group = 1,
	};
	struct rte_flow_action actions[] = {
		{
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
			.conf = &jump,
		},
		{
			.type = RTE_FLOW_ACTION_TYPE_END,
		}
	};
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	uint16_t proxy_port_id = dev->data->port_id;
	int ret;

	ret = rte_flow_pick_transfer_proxy(port_id, &proxy_port_id, NULL);
	if (ret) {
		DRV_LOG(ERR, "Unable to pick transfer proxy port for port %u. Transfer proxy "
			     "port must be present to create default FDB jump rule.",
			     port_id);
		return ret;
	}
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	if (!proxy_priv->dr_ctx) {
		DRV_LOG(DEBUG, "Transfer proxy port (port %u) of port %u must be configured "
			       "for HWS to create default FDB jump rule. Default rule will "
			       "not be created.",
			       proxy_port_id, port_id);
		return 0;
	}
	if (!proxy_priv->hw_esw_zero_tbl) {
		DRV_LOG(ERR, "Transfer proxy port (port %u) of port %u was configured, but "
			     "default flow tables were not created.",
			     proxy_port_id, port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return flow_hw_create_ctrl_flow(dev, proxy_dev,
					proxy_priv->hw_esw_zero_tbl,
					items, 0, actions, 0);
}

int
mlx5_flow_hw_create_tx_default_mreg_copy_flow(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_item_eth promisc = {
		.hdr.dst_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.src_addr.addr_bytes = "\x00\x00\x00\x00\x00\x00",
		.hdr.ether_type = 0,
	};
	struct rte_flow_item eth_all[] = {
		[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &promisc,
			.mask = &promisc,
		},
		[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action_modify_field mreg_action = {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_C_1,
		},
		.src = {
			.field = (enum rte_flow_field_id)MLX5_RTE_FLOW_FIELD_META_REG,
			.tag_index = REG_A,
		},
		.width = 32,
	};
	struct rte_flow_action copy_reg_action[] = {
		[0] = {
			.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
			.conf = &mreg_action,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_JUMP,
		},
		[2] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	MLX5_ASSERT(priv->master);
	if (!priv->dr_ctx || !priv->hw_tx_meta_cpy_tbl)
		return 0;
	return flow_hw_create_ctrl_flow(dev, dev,
					priv->hw_tx_meta_cpy_tbl,
					eth_all, 0, copy_reg_action, 0);
}

int
mlx5_flow_hw_tx_repr_matching_flow(struct rte_eth_dev *dev, uint32_t sqn)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rte_flow_item_sq sq_spec = {
		.queue = sqn,
	};
	struct rte_flow_item items[] = {
		{
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_SQ,
			.spec = &sq_spec,
		},
		{
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	/*
	 * Allocate actions array suitable for all cases - extended metadata enabled or not.
	 * With extended metadata there will be an additional MODIFY_FIELD action before JUMP.
	 */
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD },
		{ .type = RTE_FLOW_ACTION_TYPE_JUMP },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};

	/* It is assumed that caller checked for representor matching. */
	MLX5_ASSERT(priv->sh->config.repr_matching);
	if (!priv->dr_ctx) {
		DRV_LOG(DEBUG, "Port %u must be configured for HWS, before creating "
			       "default egress flow rules. Omitting creation.",
			       dev->data->port_id);
		return 0;
	}
	if (!priv->hw_tx_repr_tagging_tbl) {
		DRV_LOG(ERR, "Port %u is configured for HWS, but table for default "
			     "egress flow rules does not exist.",
			     dev->data->port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	/*
	 * If extended metadata mode is enabled, then an additional MODIFY_FIELD action must be
	 * placed before terminating JUMP action.
	 */
	if (priv->sh->config.dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS) {
		actions[1].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
		actions[2].type = RTE_FLOW_ACTION_TYPE_JUMP;
	}
	return flow_hw_create_ctrl_flow(dev, dev, priv->hw_tx_repr_tagging_tbl,
					items, 0, actions, 0);
}

static uint32_t
__calc_pattern_flags(const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type)
{
	switch (eth_pattern_type) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL:
		return MLX5_CTRL_PROMISCUOUS;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL_MCAST:
		return MLX5_CTRL_ALL_MULTICAST;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
		return MLX5_CTRL_BROADCAST;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
		return MLX5_CTRL_IPV4_MULTICAST;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
		return MLX5_CTRL_IPV6_MULTICAST;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		return MLX5_CTRL_DMAC;
	default:
		/* Should not reach here. */
		MLX5_ASSERT(false);
		return 0;
	}
}

static uint32_t
__calc_vlan_flags(const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type)
{
	switch (eth_pattern_type) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		return MLX5_CTRL_VLAN_FILTER;
	default:
		return 0;
	}
}

static bool
eth_pattern_type_is_requested(const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type,
			      uint32_t flags)
{
	uint32_t pattern_flags = __calc_pattern_flags(eth_pattern_type);
	uint32_t vlan_flags = __calc_vlan_flags(eth_pattern_type);
	bool pattern_requested = !!(pattern_flags & flags);
	bool consider_vlan = vlan_flags || (MLX5_CTRL_VLAN_FILTER & flags);
	bool vlan_requested = !!(vlan_flags & flags);

	if (consider_vlan)
		return pattern_requested && vlan_requested;
	else
		return pattern_requested;
}

static bool
rss_type_is_requested(struct mlx5_priv *priv,
		      const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct rte_flow_actions_template *at = priv->hw_ctrl_rx->rss[rss_type];
	unsigned int i;

	for (i = 0; at->actions[i].type != RTE_FLOW_ACTION_TYPE_END; ++i) {
		if (at->actions[i].type == RTE_FLOW_ACTION_TYPE_RSS) {
			const struct rte_flow_action_rss *rss = at->actions[i].conf;
			uint64_t rss_types = rss->types;

			if ((rss_types & priv->rss_conf.rss_hf) != rss_types)
				return false;
		}
	}
	return true;
}

static const struct rte_flow_item_eth *
__get_eth_spec(const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern)
{
	switch (pattern) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL:
		return &ctrl_rx_eth_promisc_spec;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL_MCAST:
		return &ctrl_rx_eth_mcast_spec;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
		return &ctrl_rx_eth_bcast_spec;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
		return &ctrl_rx_eth_ipv4_mcast_spec;
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
		return &ctrl_rx_eth_ipv6_mcast_spec;
	default:
		/* This case should not be reached. */
		MLX5_ASSERT(false);
		return NULL;
	}
}

static int
__flow_hw_ctrl_flows_single(struct rte_eth_dev *dev,
			    struct rte_flow_template_table *tbl,
			    const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern_type,
			    const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	const struct rte_flow_item_eth *eth_spec = __get_eth_spec(pattern_type);
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};

	if (!eth_spec)
		return -EINVAL;
	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = eth_spec,
	};
	items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_VOID };
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };
	/* Without VLAN filtering, only a single flow rule must be created. */
	return flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0);
}

static int
__flow_hw_ctrl_flows_single_vlan(struct rte_eth_dev *dev,
				 struct rte_flow_template_table *tbl,
				 const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern_type,
				 const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item_eth *eth_spec = __get_eth_spec(pattern_type);
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};
	unsigned int i;

	if (!eth_spec)
		return -EINVAL;
	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = eth_spec,
	};
	/* Optional VLAN for now will be VOID - will be filled later. */
	items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_VLAN };
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };
	/* Since VLAN filtering is done, create a single flow rule for each registered vid. */
	for (i = 0; i < priv->vlan_filter_n; ++i) {
		uint16_t vlan = priv->vlan_filter[i];
		struct rte_flow_item_vlan vlan_spec = {
			.hdr.vlan_tci = rte_cpu_to_be_16(vlan),
		};

		items[1].spec = &vlan_spec;
		if (flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0))
			return -rte_errno;
	}
	return 0;
}

static int
__flow_hw_ctrl_flows_unicast(struct rte_eth_dev *dev,
			     struct rte_flow_template_table *tbl,
			     const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern_type,
			     const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};
	const struct rte_ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;

	RTE_SET_USED(pattern_type);

	memset(&eth_spec, 0, sizeof(eth_spec));
	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &eth_spec,
	};
	items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_VOID };
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };
	for (i = 0; i < MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&eth_spec.hdr.dst_addr.addr_bytes, mac->addr_bytes, RTE_ETHER_ADDR_LEN);
		if (flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0))
			return -rte_errno;
	}
	return 0;
}

static int
__flow_hw_ctrl_flows_unicast_vlan(struct rte_eth_dev *dev,
				  struct rte_flow_template_table *tbl,
				  const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern_type,
				  const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_item_eth eth_spec;
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};
	const struct rte_ether_addr cmp = {
		.addr_bytes = "\x00\x00\x00\x00\x00\x00",
	};
	unsigned int i;
	unsigned int j;

	RTE_SET_USED(pattern_type);

	memset(&eth_spec, 0, sizeof(eth_spec));
	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &eth_spec,
	};
	items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_VLAN };
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };
	for (i = 0; i < MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (!memcmp(mac, &cmp, sizeof(*mac)))
			continue;
		memcpy(&eth_spec.hdr.dst_addr.addr_bytes, mac->addr_bytes, RTE_ETHER_ADDR_LEN);
		for (j = 0; j < priv->vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];
			struct rte_flow_item_vlan vlan_spec = {
				.hdr.vlan_tci = rte_cpu_to_be_16(vlan),
			};

			items[1].spec = &vlan_spec;
			if (flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0))
				return -rte_errno;
		}
	}
	return 0;
}

static int
__flow_hw_ctrl_flows(struct rte_eth_dev *dev,
		     struct rte_flow_template_table *tbl,
		     const enum mlx5_flow_ctrl_rx_eth_pattern_type pattern_type,
		     const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	switch (pattern_type) {
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_ALL_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST:
		return __flow_hw_ctrl_flows_single(dev, tbl, pattern_type, rss_type);
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_BCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV4_MCAST_VLAN:
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_IPV6_MCAST_VLAN:
		return __flow_hw_ctrl_flows_single_vlan(dev, tbl, pattern_type, rss_type);
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC:
		return __flow_hw_ctrl_flows_unicast(dev, tbl, pattern_type, rss_type);
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		return __flow_hw_ctrl_flows_unicast_vlan(dev, tbl, pattern_type, rss_type);
	default:
		/* Should not reach here. */
		MLX5_ASSERT(false);
		rte_errno = EINVAL;
		return -EINVAL;
	}
}


int
mlx5_flow_hw_ctrl_flows(struct rte_eth_dev *dev, uint32_t flags)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_hw_ctrl_rx *hw_ctrl_rx;
	unsigned int i;
	unsigned int j;
	int ret = 0;

	RTE_SET_USED(priv);
	RTE_SET_USED(flags);
	if (!priv->dr_ctx) {
		DRV_LOG(DEBUG, "port %u Control flow rules will not be created. "
			       "HWS needs to be configured beforehand.",
			       dev->data->port_id);
		return 0;
	}
	if (!priv->hw_ctrl_rx) {
		DRV_LOG(ERR, "port %u Control flow rules templates were not created.",
			dev->data->port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	hw_ctrl_rx = priv->hw_ctrl_rx;
	for (i = 0; i < MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_MAX; ++i) {
		const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type = i;

		if (!eth_pattern_type_is_requested(eth_pattern_type, flags))
			continue;
		for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
			const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type = j;
			struct rte_flow_actions_template *at;
			struct mlx5_flow_hw_ctrl_rx_table *tmpls = &hw_ctrl_rx->tables[i][j];
			const struct mlx5_flow_template_table_cfg cfg = {
				.attr = tmpls->attr,
				.external = 0,
			};

			if (!hw_ctrl_rx->rss[rss_type]) {
				at = flow_hw_create_ctrl_rx_rss_template(dev, rss_type);
				if (!at)
					return -rte_errno;
				hw_ctrl_rx->rss[rss_type] = at;
			} else {
				at = hw_ctrl_rx->rss[rss_type];
			}
			if (!rss_type_is_requested(priv, rss_type))
				continue;
			if (!tmpls->tbl) {
				tmpls->tbl = flow_hw_table_create(dev, &cfg,
								  &tmpls->pt, 1, &at, 1, NULL);
				if (!tmpls->tbl) {
					DRV_LOG(ERR, "port %u Failed to create template table "
						     "for control flow rules. Unable to create "
						     "control flow rules.",
						     dev->data->port_id);
					return -rte_errno;
				}
			}

			ret = __flow_hw_ctrl_flows(dev, tmpls->tbl, eth_pattern_type, rss_type);
			if (ret) {
				DRV_LOG(ERR, "port %u Failed to create control flow rule.",
					dev->data->port_id);
				return ret;
			}
		}
	}
	return 0;
}

void
mlx5_flow_meter_uninit(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (priv->mtr_policy_arr) {
		mlx5_free(priv->mtr_policy_arr);
		priv->mtr_policy_arr = NULL;
	}
	if (priv->mtr_profile_arr) {
		mlx5_free(priv->mtr_profile_arr);
		priv->mtr_profile_arr = NULL;
	}
	if (priv->hws_mpool) {
		mlx5_aso_mtr_queue_uninit(priv->sh, priv->hws_mpool, NULL);
		mlx5_ipool_destroy(priv->hws_mpool->idx_pool);
		mlx5_free(priv->hws_mpool);
		priv->hws_mpool = NULL;
	}
	if (priv->mtr_bulk.aso) {
		mlx5_free(priv->mtr_bulk.aso);
		priv->mtr_bulk.aso = NULL;
		priv->mtr_bulk.size = 0;
		mlx5_aso_queue_uninit(priv->sh, ASO_OPC_MOD_POLICER);
	}
	if (priv->mtr_bulk.action) {
		mlx5dr_action_destroy(priv->mtr_bulk.action);
		priv->mtr_bulk.action = NULL;
	}
	if (priv->mtr_bulk.devx_obj) {
		claim_zero(mlx5_devx_cmd_destroy(priv->mtr_bulk.devx_obj));
		priv->mtr_bulk.devx_obj = NULL;
	}
}

int
mlx5_flow_meter_init(struct rte_eth_dev *dev,
		     uint32_t nb_meters,
		     uint32_t nb_meter_profiles,
		     uint32_t nb_meter_policies,
		     uint32_t nb_queues)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_obj *dcs = NULL;
	uint32_t log_obj_size;
	int ret = 0;
	int reg_id;
	struct mlx5_aso_mtr *aso;
	uint32_t i;
	struct rte_flow_error error;
	uint32_t flags;
	uint32_t nb_mtrs = rte_align32pow2(nb_meters);
	struct mlx5_indexed_pool_config cfg = {
		.size = sizeof(struct mlx5_aso_mtr),
		.trunk_size = 1 << 12,
		.per_core_cache = 1 << 13,
		.need_lock = 1,
		.release_mem_en = !!priv->sh->config.reclaim_mode,
		.malloc = mlx5_malloc,
		.max_idx = nb_meters,
		.free = mlx5_free,
		.type = "mlx5_hw_mtr_mark_action",
	};

	if (!nb_meters) {
		ret = ENOTSUP;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter configuration is invalid.");
		goto err;
	}
	if (!priv->mtr_en || !priv->sh->meter_aso_en) {
		ret = ENOTSUP;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter ASO is not supported.");
		goto err;
	}
	priv->mtr_config.nb_meters = nb_meters;
	log_obj_size = rte_log2_u32(nb_meters >> 1);
	dcs = mlx5_devx_cmd_create_flow_meter_aso_obj
		(priv->sh->cdev->ctx, priv->sh->cdev->pdn,
			log_obj_size);
	if (!dcs) {
		ret = ENOMEM;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter ASO object allocation failed.");
		goto err;
	}
	priv->mtr_bulk.devx_obj = dcs;
	reg_id = mlx5_flow_get_reg_id(dev, MLX5_MTR_COLOR, 0, NULL);
	if (reg_id < 0) {
		ret = ENOTSUP;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter register is not available.");
		goto err;
	}
	flags = MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (priv->sh->config.dv_esw_en && priv->master)
		flags |= MLX5DR_ACTION_FLAG_HWS_FDB;
	priv->mtr_bulk.action = mlx5dr_action_create_aso_meter
			(priv->dr_ctx, (struct mlx5dr_devx_obj *)dcs,
				reg_id - REG_C_0, flags);
	if (!priv->mtr_bulk.action) {
		ret = ENOMEM;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter action creation failed.");
		goto err;
	}
	priv->mtr_bulk.aso = mlx5_malloc(MLX5_MEM_ZERO,
					 sizeof(struct mlx5_aso_mtr) *
					 nb_meters,
					 RTE_CACHE_LINE_SIZE,
					 SOCKET_ID_ANY);
	if (!priv->mtr_bulk.aso) {
		ret = ENOMEM;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter bulk ASO allocation failed.");
		goto err;
	}
	priv->mtr_bulk.size = nb_meters;
	aso = priv->mtr_bulk.aso;
	for (i = 0; i < priv->mtr_bulk.size; i++) {
		aso->type = ASO_METER_DIRECT;
		aso->state = ASO_METER_WAIT;
		aso->offset = i;
		aso++;
	}
	priv->hws_mpool = mlx5_malloc(MLX5_MEM_ZERO,
				sizeof(struct mlx5_aso_mtr_pool),
				RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!priv->hws_mpool) {
		ret = ENOMEM;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter ipool allocation failed.");
		goto err;
	}
	priv->hws_mpool->devx_obj = priv->mtr_bulk.devx_obj;
	priv->hws_mpool->action = priv->mtr_bulk.action;
	priv->hws_mpool->nb_sq = nb_queues;
	if (mlx5_aso_mtr_queue_init(priv->sh, priv->hws_mpool,
				    &priv->sh->mtrmng->pools_mng, nb_queues)) {
		ret = ENOMEM;
		rte_flow_error_set(&error, ENOMEM,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Meter ASO queue allocation failed.");
		goto err;
	}
	/*
	 * No need for local cache if Meter number is a small number.
	 * Since flow insertion rate will be very limited in that case.
	 * Here let's set the number to less than default trunk size 4K.
	 */
	if (nb_mtrs <= cfg.trunk_size) {
		cfg.per_core_cache = 0;
		cfg.trunk_size = nb_mtrs;
	} else if (nb_mtrs <= MLX5_HW_IPOOL_SIZE_THRESHOLD) {
		cfg.per_core_cache = MLX5_HW_IPOOL_CACHE_MIN;
	}
	priv->hws_mpool->idx_pool = mlx5_ipool_create(&cfg);
	if (nb_meter_profiles) {
		priv->mtr_config.nb_meter_profiles = nb_meter_profiles;
		priv->mtr_profile_arr =
			mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(struct mlx5_flow_meter_profile) *
				    nb_meter_profiles,
				    RTE_CACHE_LINE_SIZE,
				    SOCKET_ID_ANY);
		if (!priv->mtr_profile_arr) {
			ret = ENOMEM;
			rte_flow_error_set(&error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Meter profile allocation failed.");
			goto err;
		}
	}
	if (nb_meter_policies) {
		priv->mtr_config.nb_meter_policies = nb_meter_policies;
		priv->mtr_policy_arr =
			mlx5_malloc(MLX5_MEM_ZERO,
				    sizeof(struct mlx5_flow_meter_policy) *
				    nb_meter_policies,
				    RTE_CACHE_LINE_SIZE,
				    SOCKET_ID_ANY);
		if (!priv->mtr_policy_arr) {
			ret = ENOMEM;
			rte_flow_error_set(&error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Meter policy allocation failed.");
			goto err;
		}
	}
	return 0;
err:
	mlx5_flow_meter_uninit(dev);
	return ret;
}

#endif
