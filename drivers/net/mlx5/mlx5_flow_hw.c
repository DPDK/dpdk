/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_stdatomic.h>

#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_common.h"
#include "mlx5_defs.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"
#include "mlx5_rx.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
#include "mlx5_hws_cnt.h"

/** Fast path async flow API functions. */
static struct rte_flow_fp_ops mlx5_flow_hw_fp_ops;

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

#define MLX5_HW_PORT_IS_PROXY(priv) \
	(!!((priv)->sh->esw_mode && (priv)->master))


struct mlx5_indlst_legacy {
	struct mlx5_indirect_list indirect;
	struct rte_flow_action_handle *handle;
	enum rte_flow_action_type legacy_type;
};

#define MLX5_CONST_ENCAP_ITEM(encap_type, ptr) \
(((const struct encap_type *)(ptr))->definition)

/**
 * Returns the size of a struct with a following layout:
 *
 * @code{.c}
 * struct rte_flow_hw {
 *     // rte_flow_hw fields
 *     uint8_t rule[mlx5dr_rule_get_handle_size()];
 * };
 * @endcode
 *
 * Such struct is used as a basic container for HW Steering flow rule.
 */
static size_t
mlx5_flow_hw_entry_size(void)
{
	return sizeof(struct rte_flow_hw) + mlx5dr_rule_get_handle_size();
}

/**
 * Returns the size of "auxed" rte_flow_hw structure which is assumed to be laid out as follows:
 *
 * @code{.c}
 * struct {
 *     struct rte_flow_hw {
 *         // rte_flow_hw fields
 *         uint8_t rule[mlx5dr_rule_get_handle_size()];
 *     } flow;
 *     struct rte_flow_hw_aux aux;
 * };
 * @endcode
 *
 * Such struct is used whenever rte_flow_hw_aux cannot be allocated separately from the rte_flow_hw
 * e.g., when table is resizable.
 */
static size_t
mlx5_flow_hw_auxed_entry_size(void)
{
	size_t rule_size = mlx5dr_rule_get_handle_size();

	return sizeof(struct rte_flow_hw) + rule_size + sizeof(struct rte_flow_hw_aux);
}

/**
 * Returns a valid pointer to rte_flow_hw_aux associated with given rte_flow_hw
 * depending on template table configuration.
 */
static __rte_always_inline struct rte_flow_hw_aux *
mlx5_flow_hw_aux(uint16_t port_id, struct rte_flow_hw *flow)
{
	struct rte_flow_template_table *table = flow->table;

	if (!flow->nt_rule) {
		if (rte_flow_template_table_resizable(port_id, &table->cfg.attr)) {
			size_t offset = sizeof(struct rte_flow_hw) + mlx5dr_rule_get_handle_size();

			return RTE_PTR_ADD(flow, offset);
		} else {
			return &table->flow_aux[flow->idx - 1];
		}
	} else {
		return flow->nt2hws->flow_aux;
	}
}

static __rte_always_inline void
mlx5_flow_hw_aux_set_age_idx(struct rte_flow_hw *flow,
			     struct rte_flow_hw_aux *aux,
			     uint32_t age_idx)
{
	/*
	 * Only when creating a flow rule, the type will be set explicitly.
	 * Or else, it should be none in the rule update case.
	 */
	if (unlikely(flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE))
		aux->upd.age_idx = age_idx;
	else
		aux->orig.age_idx = age_idx;
}

static __rte_always_inline uint32_t
mlx5_flow_hw_aux_get_age_idx(struct rte_flow_hw *flow, struct rte_flow_hw_aux *aux)
{
	if (unlikely(flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE))
		return aux->upd.age_idx;
	else
		return aux->orig.age_idx;
}

static __rte_always_inline void
mlx5_flow_hw_aux_set_mtr_id(struct rte_flow_hw *flow,
			    struct rte_flow_hw_aux *aux,
			    uint32_t mtr_id)
{
	if (unlikely(flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE))
		aux->upd.mtr_id = mtr_id;
	else
		aux->orig.mtr_id = mtr_id;
}

static __rte_always_inline uint32_t
mlx5_flow_hw_aux_get_mtr_id(struct rte_flow_hw *flow, struct rte_flow_hw_aux *aux)
{
	if (unlikely(flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE))
		return aux->upd.mtr_id;
	else
		return aux->orig.mtr_id;
}

static __rte_always_inline struct mlx5_hw_q_job *
flow_hw_action_job_init(struct mlx5_priv *priv, uint32_t queue,
			const struct rte_flow_action_handle *handle,
			void *user_data, void *query_data,
			enum mlx5_hw_job_type type,
			enum mlx5_hw_indirect_type indirect_type,
			struct rte_flow_error *error);
static void
flow_hw_age_count_release(struct mlx5_priv *priv, uint32_t queue, struct rte_flow_hw *flow,
			  struct rte_flow_error *error);

static int
mlx5_tbl_multi_pattern_process(struct rte_eth_dev *dev,
			       struct rte_flow_template_table *tbl,
			       struct mlx5_multi_pattern_segment *segment,
			       uint32_t bulk_size,
			       struct rte_flow_error *error);
static void
mlx5_destroy_multi_pattern_segment(struct mlx5_multi_pattern_segment *segment);

static __rte_always_inline enum mlx5_indirect_list_type
flow_hw_inlist_type_get(const struct rte_flow_action *actions);

static int
flow_hw_allocate_actions(struct rte_eth_dev *dev,
			 uint64_t action_flags,
			 struct rte_flow_error *error);

static int
flow_hw_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[],
		 bool external __rte_unused, int hairpin __rte_unused,
		 struct rte_flow_error *error);

bool
mlx5_hw_ctx_validate(const struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	const struct mlx5_priv *priv = dev->data->dev_private;

	if (!priv->dr_ctx) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "non-template flow engine was not configured");
		return false;
	}
	return true;
}

static int
flow_hw_allocate_actions(struct rte_eth_dev *dev,
			 uint64_t action_flags,
			 struct rte_flow_error *error);

static __rte_always_inline int
mlx5_multi_pattern_reformat_to_index(enum mlx5dr_action_type type)
{
	switch (type) {
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		return 0;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		return 1;
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
		return 2;
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		return 3;
	default:
		break;
	}
	return -1;
}

/* Include only supported reformat actions for BWC non template API. */
static __rte_always_inline int
mlx5_bwc_multi_pattern_reformat_to_index(enum mlx5dr_action_type type)
{
	switch (type) {
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
	case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
		return mlx5_multi_pattern_reformat_to_index(type);
	default:
		break;
	}
	return -1;
}

static __rte_always_inline enum mlx5dr_action_type
mlx5_multi_pattern_reformat_index_to_type(uint32_t ix)
{
	switch (ix) {
	case 0:
		return MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
	case 1:
		return MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
	case 2:
		return MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2;
	case 3:
		return MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3;
	default:
		break;
	}
	return MLX5DR_ACTION_TYP_MAX;
}

static inline enum mlx5dr_table_type
get_mlx5dr_fdb_table_type(const struct rte_flow_attr *attr,
			  uint32_t specialize, bool fdb_unified_en)
{
	if (fdb_unified_en && !!attr->group) {
		if ((specialize & (RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG |
				   RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG)) == 0)
			return MLX5DR_TABLE_TYPE_FDB_UNIFIED;
		MLX5_ASSERT((specialize & (RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG |
					   RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG)) !=
					  (RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG |
					   RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG));
		if (specialize & RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG)
			return MLX5DR_TABLE_TYPE_FDB_RX;
		if (specialize & RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG)
			return MLX5DR_TABLE_TYPE_FDB_TX;
	}

	return MLX5DR_TABLE_TYPE_FDB;
}

static inline enum mlx5dr_table_type
get_mlx5dr_table_type(const struct rte_flow_attr *attr, uint32_t specialize,
		      bool fdb_unified_en)
{
	enum mlx5dr_table_type type;

	if (attr->transfer)
		type = get_mlx5dr_fdb_table_type(attr, specialize, fdb_unified_en);
	else if (attr->egress)
		type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		type = MLX5DR_TABLE_TYPE_NIC_RX;
	return type;
}

/* Non template default queue size used for inner ctrl queue. */
#define MLX5_NT_DEFAULT_QUEUE_SIZE 32

static int flow_hw_flush_all_ctrl_flows(struct rte_eth_dev *dev);
static int flow_hw_translate_group(struct rte_eth_dev *dev,
				   const struct mlx5_flow_template_table_cfg *cfg,
				   uint32_t group,
				   uint32_t *table_group,
				   struct rte_flow_error *error);
static __rte_always_inline int
flow_hw_set_vlan_vid_construct(struct rte_eth_dev *dev,
			       struct mlx5_modification_cmd *mhdr_cmd,
			       struct mlx5_action_construct_data *act_data,
			       const struct mlx5_hw_actions *hw_acts,
			       const struct rte_flow_action *action);
static void
flow_hw_construct_quota(struct mlx5_priv *priv,
			struct mlx5dr_rule_action *rule_act, uint32_t qid);

static int
mlx5_flow_ct_init(struct rte_eth_dev *dev,
		  uint32_t nb_conn_tracks,
		  uint16_t nb_queue);

static __rte_always_inline uint32_t flow_hw_tx_tag_regc_mask(struct rte_eth_dev *dev);
static __rte_always_inline uint32_t flow_hw_tx_tag_regc_value(struct rte_eth_dev *dev);

static int flow_hw_async_create_validate(struct rte_eth_dev *dev,
					 const uint32_t queue,
					 const struct rte_flow_template_table *table,
					 enum rte_flow_table_insertion_type insertion_type,
					 const uint32_t rule_index,
					 const struct rte_flow_item items[],
					 const uint8_t pattern_template_index,
					 const struct rte_flow_action actions[],
					 const uint8_t action_template_index,
					 struct rte_flow_error *error);
static int flow_hw_async_update_validate(struct rte_eth_dev *dev,
					 const uint32_t queue,
					 const struct rte_flow_hw *flow,
					 const struct rte_flow_action actions[],
					 const uint8_t action_template_index,
					 struct rte_flow_error *error);
static int flow_hw_async_destroy_validate(struct rte_eth_dev *dev,
					  const uint32_t queue,
					  const struct rte_flow_hw *flow,
					  struct rte_flow_error *error);

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops;

/* DR action flags with different table. */
static uint32_t mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_MAX]
				[MLX5DR_TABLE_TYPE_MAX] = {
	{
		MLX5DR_ACTION_FLAG_ROOT_RX,
		MLX5DR_ACTION_FLAG_ROOT_TX,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
		MLX5DR_ACTION_FLAG_ROOT_FDB,
	},
	{
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB,
		MLX5DR_ACTION_FLAG_HWS_FDB_RX,
		MLX5DR_ACTION_FLAG_HWS_FDB_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED,
	},
};

/**
 * Jump table flags.
 * Can jump to FDB_RX table from FDB_RX or UNIFIED tables.
 * Can jump to FDB_TX table from FDB_TX or UNIFIED tables.
 * Can jump to UNIFIED table from all tables.
 */
static uint32_t mlx5_hw_act_dest_table_flag[MLX5DR_TABLE_TYPE_MAX] = {
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB,
		(MLX5DR_ACTION_FLAG_HWS_FDB_RX | MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED),
		(MLX5DR_ACTION_FLAG_HWS_FDB_TX | MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED),
		(MLX5DR_ACTION_FLAG_HWS_FDB_RX | MLX5DR_ACTION_FLAG_HWS_FDB_TX |
		MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED),
};

/* Ethernet item spec for promiscuous mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_promisc_spec = {
	.hdr.dst_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};
/* Ethernet item mask for promiscuous mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_promisc_mask = {
	.hdr.dst_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

/* Ethernet item spec for all multicast mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_mcast_spec = {
	.hdr.dst_addr.addr_bytes = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};
/* Ethernet item mask for all multicast mode. */
static const struct rte_flow_item_eth ctrl_rx_eth_mcast_mask = {
	.hdr.dst_addr.addr_bytes = { 0x01, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

/* Ethernet item spec for IPv4 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv4_mcast_spec = {
	.hdr.dst_addr.addr_bytes = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};
/* Ethernet item mask for IPv4 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv4_mcast_mask = {
	.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

/* Ethernet item spec for IPv6 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv6_mcast_spec = {
	.hdr.dst_addr.addr_bytes = { 0x33, 0x33, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};
/* Ethernet item mask for IPv6 multicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_ipv6_mcast_mask = {
	.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0x00, 0x00, 0x00, 0x00 },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

/* Ethernet item mask for unicast traffic. */
static const struct rte_flow_item_eth ctrl_rx_eth_dmac_mask = {
	.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

/* Ethernet item spec for broadcast. */
static const struct rte_flow_item_eth ctrl_rx_eth_bcast_spec = {
	.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	.hdr.ether_type = 0,
};

static inline uint32_t
flow_hw_q_pending(struct mlx5_priv *priv, uint32_t queue)
{
	struct mlx5_hw_q *q = &priv->hw_q[queue];

	MLX5_ASSERT(q->size >= q->job_idx);
	return (q->size - q->job_idx) + q->ongoing_flow_ops;
}

static inline void
flow_hw_q_inc_flow_ops(struct mlx5_priv *priv, uint32_t queue)
{
	struct mlx5_hw_q *q = &priv->hw_q[queue];

	q->ongoing_flow_ops++;
}

static inline void
flow_hw_q_dec_flow_ops(struct mlx5_priv *priv, uint32_t queue)
{
	struct mlx5_hw_q *q = &priv->hw_q[queue];

	q->ongoing_flow_ops--;
}

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
	*hash_fields |= fields;
}

uint64_t
mlx5_flow_hw_action_flags_get(const struct rte_flow_action actions[],
			 const struct rte_flow_action **qrss,
			 const struct rte_flow_action **mark,
			 int *encap_idx,
			 int *act_cnt,
			 struct rte_flow_error *error)
{
	uint64_t action_flags = 0;
	const struct rte_flow_action *action;
	const struct rte_flow_action_raw_encap *raw_encap;
	int raw_decap_idx = -1;
	int action_idx;

	*encap_idx = -1;
	action_idx = 0;
	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		int type = (int)action->type;
		switch (type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			switch (MLX5_INDIRECT_ACTION_TYPE_GET(action->conf)) {
			case MLX5_INDIRECT_ACTION_TYPE_RSS:
				goto rss;
			case MLX5_INDIRECT_ACTION_TYPE_AGE:
				goto age;
			case MLX5_INDIRECT_ACTION_TYPE_COUNT:
				goto count;
			case MLX5_INDIRECT_ACTION_TYPE_CT:
				goto ct;
			case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
				goto meter;
			default:
				goto error;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			action_flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			action_flags |= MLX5_FLOW_ACTION_MARK;
			*mark = action;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			action_flags |= MLX5_FLOW_ACTION_OF_PUSH_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			action_flags |= MLX5_FLOW_ACTION_OF_POP_VLAN;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			action_flags |= MLX5_FLOW_ACTION_OF_SET_VLAN_VID;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			*qrss = action;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
rss:
			action_flags |= MLX5_FLOW_ACTION_RSS;
			*qrss = action;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			*encap_idx = action_idx;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			raw_encap = action->conf;
			if (raw_encap->size > MLX5_ENCAPSULATION_DECISION_SIZE)
				*encap_idx = raw_decap_idx != -1 ?
					     raw_decap_idx : action_idx;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			raw_decap_idx = action_idx;
			break;
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			action_flags |= MLX5_FLOW_ACTION_SEND_TO_KERNEL;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			action_flags |= MLX5_FLOW_ACTION_PORT_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
age:
			action_flags |= MLX5_FLOW_ACTION_AGE;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
count:
			action_flags |= MLX5_FLOW_ACTION_COUNT;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
ct:
			action_flags |= MLX5_FLOW_ACTION_CT;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
meter:
			action_flags |= MLX5_FLOW_ACTION_METER;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			action_flags |= MLX5_FLOW_ACTION_DEFAULT_MISS;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
			action_flags |= MLX5_FLOW_ACTION_JUMP_TO_TABLE_INDEX;
			break;
		case RTE_FLOW_ACTION_TYPE_SAMPLE:
			action_flags |= MLX5_FLOW_ACTION_SAMPLE;
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
		case RTE_FLOW_ACTION_TYPE_END:
			break;
		default:
			goto error;
		}
		action_idx++;
	}
	if (*encap_idx == -1)
		*encap_idx = action_idx;
	action_idx++; /* The END action. */
	*act_cnt = action_idx;
	return action_flags;
error:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
			   action, "invalid flow action");
	return 0;
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
	uint32_t specialize = cfg->attr.specialize;
	struct mlx5_flow_group *grp;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = error,
		.data = &jattr,
		.data2 = &specialize,
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

	grp = container_of(jump, struct mlx5_flow_group, jump);
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
 * @param[in] item_flags
 *   Item flags for non template rule.
 * @param[in] is_template
 *   True if it is a template rule.
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
		rss_desc.symmetric_hash_function = MLX5_RSS_IS_SYMM(rss->func);
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

	ct = mlx5_ipool_get(priv->hws_ctpool->cts, idx);
	if (!ct || (!priv->shared_host && mlx5_aso_ct_available(priv->sh, queue, ct)))
		return -1;
	rule_act->action = priv->hws_ctpool->dr_action;
	rule_act->aso_ct.offset = ct->offset;
	rule_act->aso_ct.direction = ct->is_original ?
		MLX5DR_ACTION_ASO_CT_DIRECTION_INITIATOR :
		MLX5DR_ACTION_ASO_CT_DIRECTION_RESPONDER;
	return 0;
}

static void
flow_hw_template_destroy_reformat_action(struct mlx5_hw_encap_decap_action *encap_decap)
{
	if (encap_decap->action && !encap_decap->multi_pattern)
		mlx5dr_action_destroy(encap_decap->action);
}

static void
flow_hw_template_destroy_mhdr_action(struct mlx5_hw_modify_header_action *mhdr)
{
	if (mhdr->action && !mhdr->multi_pattern)
		mlx5dr_action_destroy(mhdr->action);
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
__flow_hw_actions_release(struct rte_eth_dev *dev, struct mlx5_hw_actions *acts)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (acts->mark)
		if (!(rte_atomic_fetch_sub_explicit(&priv->hws_mark_refcnt, 1,
				rte_memory_order_relaxed) - 1))
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
		flow_hw_template_destroy_reformat_action(acts->encap_decap);
		mlx5_free(acts->encap_decap);
		acts->encap_decap = NULL;
	}
	if (acts->push_remove) {
		if (acts->push_remove->action)
			mlx5dr_action_destroy(acts->push_remove->action);
		mlx5_free(acts->push_remove);
		acts->push_remove = NULL;
	}
	if (acts->mhdr) {
		flow_hw_template_destroy_mhdr_action(acts->mhdr);
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
 * Release the action data back into the pool without destroy any action.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] acts
 *   Pointer to the template HW steering DR actions.
 */
static inline void
__flow_hw_act_data_flush(struct rte_eth_dev *dev, struct mlx5_hw_actions *acts)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_action_construct_data *data;

	while (!LIST_EMPTY(&acts->act_list)) {
		data = LIST_FIRST(&acts->act_list);
		LIST_REMOVE(data, next);
		mlx5_ipool_free(priv->acts_ipool, data->idx);
	}
}

/*
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
__flow_hw_action_template_destroy(struct rte_eth_dev *dev, struct mlx5_hw_actions *acts)
{
	__flow_hw_act_data_flush(dev, acts);
	__flow_hw_actions_release(dev, acts);
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

static __rte_always_inline int
__flow_hw_act_data_indirect_append(struct mlx5_priv *priv,
				   struct mlx5_hw_actions *acts,
				   enum rte_flow_action_type type,
				   enum rte_flow_action_type mask_type,
				   uint16_t action_src,
				   uint16_t action_dst)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->indirect.expected_type = mask_type;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return 0;
}

static __rte_always_inline int
flow_hw_act_data_indirect_list_append(struct mlx5_priv *priv,
				      struct mlx5_hw_actions *acts,
				      enum rte_flow_action_type type,
				      uint16_t action_src, uint16_t action_dst,
				      indirect_list_callback_t cb)
{
	struct mlx5_action_construct_data *act_data;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return -1;
	act_data->indirect_list_cb = cb;
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

/**
 * Append dynamic push action to the dynamic action list.
 *
 * @param[in] dev
 *   Pointer to the port.
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
 *    Data pointer on success, NULL otherwise and rte_errno is set.
 */
static __rte_always_inline void *
__flow_hw_act_data_push_append(struct rte_eth_dev *dev,
			       struct mlx5_hw_actions *acts,
			       enum rte_flow_action_type type,
			       uint16_t action_src,
			       uint16_t action_dst,
			       uint16_t len)
{
	struct mlx5_action_construct_data *act_data;
	struct mlx5_priv *priv = dev->data->dev_private;

	act_data = __flow_hw_act_data_alloc(priv, type, action_src, action_dst);
	if (!act_data)
		return NULL;
	act_data->ipv6_ext.len = len;
	LIST_INSERT_HEAD(&acts->act_list, act_data, next);
	return act_data;
}

static __rte_always_inline int
__flow_hw_act_data_hdr_modify_append(struct mlx5_priv *priv,
				     struct mlx5_hw_actions *acts,
				     enum rte_flow_action_type type,
				     uint16_t action_src,
				     uint16_t action_dst,
				     const struct rte_flow_action_modify_field *mf,
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
	act_data->modify_header.action = *mf;
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
	act_data->shared_rss.symmetric_hash_function =
		MLX5_RSS_IS_SYMM(rss->origin.func);
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
		    action_src, action_dst, idx, shared_rss)) {
			DRV_LOG(WARNING, "Indirect RSS action index %d translate failed", act_idx);
			return -1;
		}
		break;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		if (__flow_hw_act_data_shared_cnt_append(priv, acts,
			(enum rte_flow_action_type)
			MLX5_RTE_FLOW_ACTION_TYPE_COUNT,
			action_src, action_dst, act_idx)) {
			DRV_LOG(WARNING, "Indirect count action translate failed");
			return -1;
		}
		break;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		/* Not supported, prevent by validate function. */
		MLX5_ASSERT(0);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		if (flow_hw_ct_compile(dev, MLX5_HW_INV_QUEUE,
				       idx, &acts->rule_acts[action_dst])) {
			DRV_LOG(WARNING, "Indirect CT action translate failed");
			return -1;
		}
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		if (__flow_hw_act_data_shared_mtr_append(priv, acts,
			(enum rte_flow_action_type)
			MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK,
			action_src, action_dst, idx)) {
			DRV_LOG(WARNING, "Indirect meter mark action translate failed");
			return -1;
		}
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
			  const struct mlx5_modification_cmd *cmd,
			  const struct rte_flow_attr *attr)
{
	struct mlx5_modification_cmd last_cmd = { { 0 } };
	struct mlx5_modification_cmd new_cmd = { { 0 } };
	const uint32_t cmds_num = mhdr->mhdr_cmds_num;
	unsigned int last_type;
	bool should_insert = false;

	/*
	 * Modify header action list does not require NOPs in root table,
	 * because different type of underlying object is used:
	 * - in root table - MODIFY_HEADER_CONTEXT (does not require NOPs),
	 * - in non-root - either inline modify action or based on Modify Header Pattern
	 *   (which requires NOPs).
	 */
	if (attr->group == 0)
		return false;
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
		else if (last_type == MLX5_MODIFICATION_TYPE_COPY ||
			 last_type == MLX5_MODIFICATION_TYPE_ADD_FIELD)
			should_insert = new_cmd.field == last_cmd.dst_field;
		else if (last_type == MLX5_MODIFICATION_TYPE_NOP)
			should_insert = false;
		else
			MLX5_ASSERT(false); /* Other types are not supported. */
		break;
	case MLX5_MODIFICATION_TYPE_COPY:
	case MLX5_MODIFICATION_TYPE_ADD_FIELD:
		if (last_type == MLX5_MODIFICATION_TYPE_SET ||
		    last_type == MLX5_MODIFICATION_TYPE_ADD)
			should_insert = (new_cmd.field == last_cmd.field ||
					 new_cmd.dst_field == last_cmd.field);
		else if (last_type == MLX5_MODIFICATION_TYPE_COPY ||
			 last_type == MLX5_MODIFICATION_TYPE_ADD_FIELD)
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
				   struct mlx5_flow_dv_modify_hdr_resource *resource,
				   const struct rte_flow_attr *attr)
{
	uint32_t idx;
	int ret;

	for (idx = 0; idx < resource->actions_num; ++idx) {
		struct mlx5_modification_cmd *src = &resource->actions[idx];

		if (flow_hw_should_insert_nop(mhdr, src, attr)) {
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
			     const struct rte_flow_action *action, /* Current action from AT. */
			     const struct rte_flow_action *action_mask, /* Current mask from AT. */
			     struct mlx5_hw_actions *acts,
			     struct mlx5_hw_modify_header_action *mhdr,
			     uint16_t src_pos,
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
			    tag_index == RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX)
				value = rte_cpu_to_be_32(value << 16);
			else
				value = rte_cpu_to_be_32(value);
			item.spec = &value;
		} else if (conf->dst.field == RTE_FLOW_FIELD_GTP_PSC_QFI ||
			   conf->dst.field == RTE_FLOW_FIELD_GENEVE_OPT_TYPE) {
			/*
			 * Both QFI and Geneve option type are passed as an uint8_t integer,
			 * but it is accessed through a 2nd least significant byte of a 32-bit
			 * field in modify header command.
			 */
			value = *(const uint8_t *)item.spec;
			value = rte_cpu_to_be_32(value << 8);
			item.spec = &value;
		} else if (conf->dst.field == RTE_FLOW_FIELD_VXLAN_LAST_RSVD) {
			value = *(const uint8_t *)item.spec << 24;
			value = rte_cpu_to_be_32(value);
			item.spec = &value;
		}
	} else {
		type = conf->operation == RTE_FLOW_MODIFY_SET ?
		       MLX5_MODIFICATION_TYPE_COPY : MLX5_MODIFICATION_TYPE_ADD_FIELD;
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
	if (flow_hw_should_insert_nop(mhdr, &resource->actions[0], attr)) {
		ret = flow_hw_mhdr_cmd_nop_append(mhdr);
		if (ret)
			return rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "too many modify field operations specified");
	}
	cmds_start = mhdr->mhdr_cmds_num;
	ret = flow_hw_converted_mhdr_cmds_append(mhdr, resource, attr);
	if (ret)
		return rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "too many modify field operations specified");

	cmds_end = mhdr->mhdr_cmds_num;
	if (shared)
		return 0;
	ret = __flow_hw_act_data_hdr_modify_append(priv, acts, RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
						   src_pos, mhdr->pos, conf,
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
				 const struct rte_flow_action *action,
				 const struct rte_flow_action *action_mask,
				 struct mlx5_hw_actions *acts,
				 uint16_t action_src, uint16_t action_dst,
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
					  "represented_port action must"
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
				 action_src, action_dst);
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
			 struct mlx5_hw_q_job *job, bool push,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	const struct rte_flow_action_meter_mark *meter_mark = action->conf;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_flow_meter_info *fm;
	uint32_t mtr_id = 0;
	uintptr_t handle = (uintptr_t)MLX5_INDIRECT_ACTION_TYPE_METER_MARK <<
					MLX5_INDIRECT_ACTION_TYPE_OFFSET;

	if (priv->shared_host) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Meter mark actions can only be created on the host port");
		return NULL;
	}
	if (meter_mark->profile == NULL)
		return NULL;
	aso_mtr = mlx5_ipool_malloc(pool->idx_pool, &mtr_id);
	if (!aso_mtr) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "failed to allocate aso meter entry");
		if (mtr_id)
			mlx5_ipool_free(pool->idx_pool, mtr_id);
		return NULL;
	}
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
	aso_mtr->init_color = fm->color_aware ? RTE_COLORS : RTE_COLOR_GREEN;
	job->action = (void *)(handle | mtr_id);
	/* Update ASO flow meter by wqe. */
	if (mlx5_aso_meter_update_by_wqe(priv, queue, aso_mtr,
					 &priv->mtr_bulk, job, push)) {
		mlx5_ipool_free(pool->idx_pool, mtr_id);
		return NULL;
	}
	/* Wait for ASO object completion. */
	if (queue == MLX5_HW_INV_QUEUE &&
	    mlx5_aso_mtr_wait(priv, aso_mtr, true)) {
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
			   uint32_t queue,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_hw_q_job *job =
		flow_hw_action_job_init(priv, queue, NULL, NULL, NULL,
					MLX5_HW_Q_JOB_TYPE_CREATE,
					MLX5_HW_INDIRECT_TYPE_LEGACY, NULL);

	if (!job)
		return -1;
	aso_mtr = flow_hw_meter_mark_alloc(dev, queue, action, job,
					   true, error);
	if (!aso_mtr) {
		flow_hw_job_put(priv, job, queue);
		return -1;
	}

	/* Compile METER_MARK action */
	acts[aso_mtr_pos].action = pool->action;
	acts[aso_mtr_pos].aso_meter.offset = aso_mtr->offset;
	*index = aso_mtr->fm.meter_id;
	return 0;
}

static int
flow_hw_translate_indirect_mirror(__rte_unused struct rte_eth_dev *dev,
				  __rte_unused const struct mlx5_action_construct_data *act_data,
				  const struct rte_flow_action *action,
				  struct mlx5dr_rule_action *dr_rule)
{
	const struct rte_flow_action_indirect_list *list_conf = action->conf;
	const struct mlx5_mirror *mirror = (typeof(mirror))list_conf->handle;

	dr_rule->action = mirror->mirror_action;
	return 0;
}

/**
 * HWS mirror implemented as FW island.
 * The action does not support indirect list flow configuration.
 * If template handle was masked, use handle mirror action in flow rules.
 * Otherwise let flow rule specify mirror handle.
 */
static int
hws_table_tmpl_translate_indirect_mirror(struct rte_eth_dev *dev,
					 const struct rte_flow_action *action,
					 const struct rte_flow_action *mask,
					 struct mlx5_hw_actions *acts,
					 uint16_t action_src, uint16_t action_dst)
{
	int ret = 0;
	const struct rte_flow_action_indirect_list *mask_conf = mask->conf;

	if (mask_conf && mask_conf->handle) {
		/**
		 * If mirror handle was masked, assign fixed DR5 mirror action.
		 */
		flow_hw_translate_indirect_mirror(dev, NULL, action,
						  &acts->rule_acts[action_dst]);
	} else {
		struct mlx5_priv *priv = dev->data->dev_private;
		ret = flow_hw_act_data_indirect_list_append
			(priv, acts, RTE_FLOW_ACTION_TYPE_INDIRECT_LIST,
			 action_src, action_dst,
			 flow_hw_translate_indirect_mirror);
	}

	return ret;
}

static int
flow_hw_reformat_action(__rte_unused struct rte_eth_dev *dev,
			__rte_unused const struct mlx5_action_construct_data *data,
			const struct rte_flow_action *action,
			struct mlx5dr_rule_action *dr_rule)
{
	const struct rte_flow_action_indirect_list *indlst_conf = action->conf;

	dr_rule->action = ((struct mlx5_hw_encap_decap_action *)
			   (indlst_conf->handle))->action;
	if (!dr_rule->action)
		return -EINVAL;
	return 0;
}

/**
 * Template conf must not be masked. If handle is masked, use the one in template,
 * otherwise update per flow rule.
 */
static int
hws_table_tmpl_translate_indirect_reformat(struct rte_eth_dev *dev,
					   const struct rte_flow_action *action,
					   const struct rte_flow_action *mask,
					   struct mlx5_hw_actions *acts,
					   uint16_t action_src, uint16_t action_dst)
{
	int ret = -1;
	const struct rte_flow_action_indirect_list *mask_conf = mask->conf;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (mask_conf && mask_conf->handle && !mask_conf->conf)
		/**
		 * If handle was masked, assign fixed DR action.
		 */
		ret = flow_hw_reformat_action(dev, NULL, action,
					      &acts->rule_acts[action_dst]);
	else if (mask_conf && !mask_conf->handle && !mask_conf->conf)
		ret = flow_hw_act_data_indirect_list_append
			(priv, acts, RTE_FLOW_ACTION_TYPE_INDIRECT_LIST,
			 action_src, action_dst, flow_hw_reformat_action);
	return ret;
}

static int
flow_dr_set_meter(struct mlx5_priv *priv,
		  struct mlx5dr_rule_action *dr_rule,
		  const struct rte_flow_action_indirect_list *action_conf)
{
	const struct mlx5_indlst_legacy *legacy_obj =
		(typeof(legacy_obj))action_conf->handle;
	struct mlx5_aso_mtr_pool *mtr_pool = priv->hws_mpool;
	uint32_t act_idx = (uint32_t)(uintptr_t)legacy_obj->handle;
	uint32_t mtr_id = act_idx & (RTE_BIT32(MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	struct mlx5_aso_mtr *aso_mtr = mlx5_ipool_get(mtr_pool->idx_pool, mtr_id);

	if (!aso_mtr)
		return -EINVAL;
	dr_rule->action = mtr_pool->action;
	dr_rule->aso_meter.offset = aso_mtr->offset;
	return 0;
}

__rte_always_inline static void
flow_dr_mtr_flow_color(struct mlx5dr_rule_action *dr_rule, enum rte_color init_color)
{
	dr_rule->aso_meter.init_color =
		(enum mlx5dr_action_aso_meter_color)rte_col_2_mlx5_col(init_color);
}

static int
flow_hw_translate_indirect_meter(struct rte_eth_dev *dev,
				 const struct mlx5_action_construct_data *act_data,
				 const struct rte_flow_action *action,
				 struct mlx5dr_rule_action *dr_rule)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_indirect_list *action_conf = action->conf;
	const struct rte_flow_indirect_update_flow_meter_mark **flow_conf =
		(typeof(flow_conf))action_conf->conf;

	ret = flow_dr_set_meter(priv, dr_rule, action_conf);
	if (ret)
		return ret;
	if (!act_data->shared_meter.conf_masked) {
		if (flow_conf && flow_conf[0] && flow_conf[0]->init_color < RTE_COLORS)
			flow_dr_mtr_flow_color(dr_rule, flow_conf[0]->init_color);
	}
	return 0;
}

static int
hws_table_tmpl_translate_indirect_meter(struct rte_eth_dev *dev,
					const struct rte_flow_action *action,
					const struct rte_flow_action *mask,
					struct mlx5_hw_actions *acts,
					uint16_t action_src, uint16_t action_dst)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_indirect_list *action_conf = action->conf;
	const struct rte_flow_action_indirect_list *mask_conf = mask->conf;
	bool is_handle_masked = mask_conf && mask_conf->handle;
	bool is_conf_masked = mask_conf && mask_conf->conf && mask_conf->conf[0];
	struct mlx5dr_rule_action *dr_rule = &acts->rule_acts[action_dst];

	if (is_handle_masked) {
		ret = flow_dr_set_meter(priv, dr_rule, action->conf);
		if (ret)
			return ret;
	}
	if (is_conf_masked) {
		const struct
			rte_flow_indirect_update_flow_meter_mark **flow_conf =
			(typeof(flow_conf))action_conf->conf;
		flow_dr_mtr_flow_color(dr_rule,
				       flow_conf[0]->init_color);
	}
	if (!is_handle_masked || !is_conf_masked) {
		struct mlx5_action_construct_data *act_data;

		ret = flow_hw_act_data_indirect_list_append
			(priv, acts, RTE_FLOW_ACTION_TYPE_INDIRECT_LIST,
			 action_src, action_dst, flow_hw_translate_indirect_meter);
		if (ret)
			return ret;
		act_data = LIST_FIRST(&acts->act_list);
		act_data->shared_meter.conf_masked = is_conf_masked;
	}
	return 0;
}

static int
hws_table_tmpl_translate_indirect_legacy(struct rte_eth_dev *dev,
					 const struct rte_flow_action *action,
					 const struct rte_flow_action *mask,
					 struct mlx5_hw_actions *acts,
					 uint16_t action_src, uint16_t action_dst)
{
	int ret;
	const struct rte_flow_action_indirect_list *indlst_conf = action->conf;
	struct mlx5_indlst_legacy *indlst_obj = (typeof(indlst_obj))indlst_conf->handle;
	uint32_t act_idx = (uint32_t)(uintptr_t)indlst_obj->handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;

	switch (type) {
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		ret = hws_table_tmpl_translate_indirect_meter(dev, action, mask,
							      acts, action_src,
							      action_dst);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

/*
 * template .. indirect_list handle Ht conf Ct ..
 * mask     .. indirect_list handle Hm conf Cm ..
 *
 * PMD requires Ht != 0 to resolve handle type.
 * If Ht was masked (Hm != 0) DR5 action will be set according to Ht and will
 * not change. Otherwise, DR5 action will be resolved during flow rule build.
 * If Ct was masked (Cm != 0), table template processing updates base
 * indirect action configuration with Ct parameters.
 */
static int
table_template_translate_indirect_list(struct rte_eth_dev *dev,
				       const struct rte_flow_action *action,
				       const struct rte_flow_action *mask,
				       struct mlx5_hw_actions *acts,
				       uint16_t action_src, uint16_t action_dst)
{
	int ret = 0;
	enum mlx5_indirect_list_type type;
	const struct rte_flow_action_indirect_list *list_conf = action->conf;

	if (!list_conf || !list_conf->handle)
		return -EINVAL;
	type = mlx5_get_indirect_list_type(list_conf->handle);
	switch (type) {
	case MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY:
		ret = hws_table_tmpl_translate_indirect_legacy(dev, action, mask,
							       acts, action_src,
							       action_dst);
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
		ret = hws_table_tmpl_translate_indirect_mirror(dev, action, mask,
							       acts, action_src,
							       action_dst);
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
		if (list_conf->conf)
			return -EINVAL;
		ret = hws_table_tmpl_translate_indirect_reformat(dev, action, mask,
								 acts, action_src,
								 action_dst);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static void
mlx5_set_reformat_header(struct mlx5dr_action_reformat_header *hdr,
			 uint8_t *encap_data,
			 size_t data_size)
{
	hdr->sz = data_size;
	hdr->data = encap_data;
}

static int
mlx5_tbl_translate_reformat(struct mlx5_priv *priv,
			    struct mlx5_hw_actions *acts,
			    struct rte_flow_actions_template *at,
			    const struct rte_flow_item *enc_item,
			    const struct rte_flow_item *enc_item_m,
			    uint8_t *encap_data, uint8_t *encap_data_m,
			    struct mlx5_tbl_multi_pattern_ctx *mp_ctx,
			    size_t data_size, uint16_t reformat_src,
			    enum mlx5dr_action_type refmt_type,
			    struct rte_flow_error *error)
{
	int mp_reformat_ix = mlx5_multi_pattern_reformat_to_index(refmt_type);
	struct mlx5dr_action_reformat_header hdr;
	uint8_t buf[MLX5_ENCAP_MAX_LEN];
	bool shared_rfmt = false;
	int ret;

	MLX5_ASSERT(at->reformat_off != UINT16_MAX);
	if (enc_item) {
		MLX5_ASSERT(!encap_data);
		ret = flow_dv_convert_encap_data(enc_item, buf, &data_size, error);
		if (ret)
			return ret;
		encap_data = buf;
		if (enc_item_m)
			shared_rfmt = true;
	} else if (encap_data && encap_data_m) {
		shared_rfmt = true;
	}
	acts->encap_decap = mlx5_malloc(MLX5_MEM_ZERO,
					sizeof(*acts->encap_decap) + data_size,
					0, SOCKET_ID_ANY);
	if (!acts->encap_decap)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "no memory for reformat context");
	acts->encap_decap_pos = at->reformat_off;
	acts->encap_decap->data_size = data_size;
	acts->encap_decap->action_type = refmt_type;
	if (shared_rfmt || mp_reformat_ix < 0) {
		uint16_t reformat_ix = at->reformat_off;
		/*
		 * This copy is only needed in non template mode.
		 * In order to create the action later.
		 */
		memcpy(acts->encap_decap->data, encap_data, data_size);
		acts->rule_acts[reformat_ix].reformat.data = acts->encap_decap->data;
		acts->rule_acts[reformat_ix].reformat.offset = 0;
		acts->encap_decap->shared = true;
	} else {
		uint32_t ix;
		typeof(mp_ctx->reformat[0]) *reformat = mp_ctx->reformat +
							mp_reformat_ix;
		mlx5_set_reformat_header(&hdr, encap_data, data_size);
		ix = reformat->elements_num++;
		reformat->reformat_hdr[ix] = hdr;
		acts->rule_acts[at->reformat_off].reformat.hdr_idx = ix;
		acts->encap_decap->multi_pattern = 1;
		ret = __flow_hw_act_data_encap_append
			(priv, acts, (at->actions + reformat_src)->type,
			 reformat_src, at->reformat_off, data_size);
		if (ret)
			return -rte_errno;
		mlx5_multi_pattern_activate(mp_ctx);
	}
	return 0;
}

static int
mlx5_tbl_create_reformat_action(struct mlx5_priv *priv,
				const struct rte_flow_template_table_attr *table_attr,
				struct mlx5_hw_actions *acts,
				struct rte_flow_actions_template *at,
				uint8_t *encap_data,
				size_t data_size,
				enum mlx5dr_action_type refmt_type)
{
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	bool unified_fdb = is_unified_fdb(priv);
	enum mlx5dr_table_type tbl_type = get_mlx5dr_table_type(attr, table_attr->specialize,
								unified_fdb);
	struct mlx5dr_action_reformat_header hdr;

	mlx5_set_reformat_header(&hdr, encap_data, data_size);
	uint16_t reformat_ix = at->reformat_off;
	uint32_t flags = mlx5_hw_act_flag[!!attr->group][tbl_type] |
				MLX5DR_ACTION_FLAG_SHARED;

	acts->encap_decap->action = mlx5dr_action_create_reformat(priv->dr_ctx, refmt_type,
							   1, &hdr, 0, flags);
	if (!acts->encap_decap->action)
		return -rte_errno;
	acts->rule_acts[reformat_ix].action = acts->encap_decap->action;
	return 0;
}

static int
mlx5_tbl_translate_modify_header(struct rte_eth_dev *dev,
				 const struct mlx5_flow_template_table_cfg *cfg,
				 struct mlx5_hw_actions *acts,
				 struct mlx5_tbl_multi_pattern_ctx *mp_ctx,
				 struct mlx5_hw_modify_header_action *mhdr,
				 struct rte_flow_error *error)
{
	uint16_t mhdr_ix = mhdr->pos;
	struct mlx5dr_action_mh_pattern pattern = {
		.sz = sizeof(struct mlx5_modification_cmd) * mhdr->mhdr_cmds_num
	};

	if (flow_hw_validate_compiled_modify_field(dev, cfg, mhdr, error)) {
		__flow_hw_action_template_destroy(dev, acts);
		return -rte_errno;
	}
	acts->mhdr = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*acts->mhdr),
				 0, SOCKET_ID_ANY);
	if (!acts->mhdr)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "translate modify_header: no memory for modify header context");
	rte_memcpy(acts->mhdr, mhdr, sizeof(*mhdr));
	if (!mhdr->shared) {
		pattern.data = (__be64 *)acts->mhdr->mhdr_cmds;
		typeof(mp_ctx->mh) *mh = &mp_ctx->mh;
		uint32_t idx = mh->elements_num;
		mh->pattern[mh->elements_num++] = pattern;
		acts->mhdr->multi_pattern = 1;
		acts->rule_acts[mhdr_ix].modify_header.pattern_idx = idx;
		mlx5_multi_pattern_activate(mp_ctx);
	}
	return 0;
}

static int
mlx5_tbl_ensure_shared_modify_header(struct rte_eth_dev *dev,
				     const struct mlx5_flow_template_table_cfg *cfg,
				     struct mlx5_hw_actions *acts,
				     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	const struct rte_flow_template_table_attr *table_attr = &cfg->attr;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	enum mlx5dr_table_type tbl_type = get_mlx5dr_table_type(attr, table_attr->specialize,
								unified_fdb);
	struct mlx5dr_action_mh_pattern pattern = {
		.sz = sizeof(struct mlx5_modification_cmd) * acts->mhdr->mhdr_cmds_num
	};
	uint16_t mhdr_ix = acts->mhdr->pos;
	uint32_t flags = mlx5_hw_act_flag[!!attr->group][tbl_type] | MLX5DR_ACTION_FLAG_SHARED;

	pattern.data = (__be64 *)acts->mhdr->mhdr_cmds;
	acts->mhdr->action = mlx5dr_action_create_modify_header(priv->dr_ctx, 1,
								&pattern, 0, flags);
	if (!acts->mhdr->action)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "translate modify_header: failed to create DR action");
	acts->rule_acts[mhdr_ix].action = acts->mhdr->action;
	return 0;
}

static int
mlx5_create_ipv6_ext_reformat(struct rte_eth_dev *dev,
			      const struct mlx5_flow_template_table_cfg *cfg,
			      struct mlx5_hw_actions *acts,
			      struct rte_flow_actions_template *at,
			      uint8_t *push_data, uint8_t *push_data_m,
			      size_t push_size, uint16_t recom_src,
			      enum mlx5dr_action_type recom_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	const struct rte_flow_template_table_attr *table_attr = &cfg->attr;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	enum mlx5dr_table_type type =
		get_mlx5dr_table_type(attr, table_attr->specialize, unified_fdb);
	struct mlx5_action_construct_data *act_data;
	struct mlx5dr_action_reformat_header hdr = {0};
	uint32_t flag, bulk = 0;

	flag = mlx5_hw_act_flag[!!attr->group][type];
	acts->push_remove = mlx5_malloc(MLX5_MEM_ZERO,
					sizeof(*acts->push_remove) + push_size,
					0, SOCKET_ID_ANY);
	if (!acts->push_remove)
		return -ENOMEM;

	switch (recom_type) {
	case MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT:
		if (!push_data || !push_size)
			goto err1;
		if (!push_data_m) {
			bulk = rte_log2_u32(table_attr->nb_flows);
		} else {
			flag |= MLX5DR_ACTION_FLAG_SHARED;
			acts->push_remove->shared = 1;
		}
		acts->push_remove->data_size = push_size;
		memcpy(acts->push_remove->data, push_data, push_size);
		hdr.data = push_data;
		hdr.sz = push_size;
		break;
	case MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT:
		flag |= MLX5DR_ACTION_FLAG_SHARED;
		acts->push_remove->shared = 1;
		break;
	default:
		break;
	}

	acts->push_remove->action =
		mlx5dr_action_create_reformat_ipv6_ext(priv->dr_ctx,
				recom_type, &hdr, bulk, flag);
	if (!acts->push_remove->action)
		goto err1;
	acts->rule_acts[at->recom_off].action = acts->push_remove->action;
	acts->rule_acts[at->recom_off].ipv6_ext.header = acts->push_remove->data;
	acts->rule_acts[at->recom_off].ipv6_ext.offset = 0;
	acts->push_remove_pos = at->recom_off;
	if (!acts->push_remove->shared) {
		act_data = __flow_hw_act_data_push_append(dev, acts,
				RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH,
				recom_src, at->recom_off, push_size);
		if (!act_data)
			goto err;
	}
	return 0;
err:
	if (acts->push_remove->action)
		mlx5dr_action_destroy(acts->push_remove->action);
err1:
	if (acts->push_remove) {
		mlx5_free(acts->push_remove);
		acts->push_remove = NULL;
	}
	return -EINVAL;
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
 * @param[in] nt_mode
 *   Non template rule translate.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno otherwise and rte_errno is set.
 */
static int
__flow_hw_translate_actions_template(struct rte_eth_dev *dev,
				     const struct mlx5_flow_template_table_cfg *cfg,
				     struct mlx5_hw_actions *acts,
				     struct rte_flow_actions_template *at,
				     struct mlx5_tbl_multi_pattern_ctx *mp_ctx,
				     bool nt_mode,
				     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *hca_attr = &priv->sh->cdev->config.hca_attr;
	const struct rte_flow_template_table_attr *table_attr = &cfg->attr;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	struct rte_flow_action *actions = at->actions;
	struct rte_flow_action *masks = at->masks;
	enum mlx5dr_action_type refmt_type = MLX5DR_ACTION_TYP_LAST;
	enum mlx5dr_action_type recom_type = MLX5DR_ACTION_TYP_LAST;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	const struct rte_flow_action_ipv6_ext_push *ipv6_ext_data;
	const struct rte_flow_item *enc_item = NULL, *enc_item_m = NULL;
	uint16_t reformat_src = 0, recom_src = 0;
	uint8_t *encap_data = NULL, *encap_data_m = NULL;
	uint8_t *push_data = NULL, *push_data_m = NULL;
	size_t data_size = 0, push_size = 0;
	struct mlx5_hw_modify_header_action mhdr = { 0 };
	struct rte_flow_error sub_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};
	bool actions_end = false;
	uint32_t type;
	bool reformat_used = false;
	bool recom_used = false;
	unsigned int of_vlan_offset;
	uint32_t ct_idx;
	int ret, err;
	uint32_t target_grp = 0;
	bool unified_fdb = is_unified_fdb(priv);

	flow_hw_modify_field_init(&mhdr, at);
	type = get_mlx5dr_table_type(attr, cfg->attr.specialize, unified_fdb);
	for (; !actions_end; actions++, masks++) {
		uint64_t pos = actions - at->actions;
		uint16_t src_pos = pos - at->src_off[pos];
		uint16_t dr_pos = at->dr_off[pos];

		switch ((int)actions->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT_LIST:
			if (!attr->group) {
				DRV_LOG(ERR, "Indirect action is not supported in root table.");
				goto err;
			}
			ret = table_template_translate_indirect_list
				(dev, actions, masks, acts, src_pos, dr_pos);
			if (ret)
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			if (!attr->group) {
				DRV_LOG(ERR, "Indirect action is not supported in root table.");
				goto err;
			}
			if (actions->conf && masks->conf) {
				if (flow_hw_shared_action_translate
				(dev, actions, acts, src_pos, dr_pos))
					goto err;
			} else if (__flow_hw_act_data_indirect_append
					(priv, acts, RTE_FLOW_ACTION_TYPE_INDIRECT,
					 masks->type, src_pos, dr_pos)){
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			acts->rule_acts[dr_pos].action =
				priv->hw_drop[!!attr->group];
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			if (!attr->group) {
				DRV_LOG(ERR, "Port representor is not supported in root table.");
				goto err;
			}
			acts->rule_acts[dr_pos].action = priv->hw_def_miss;
			break;
		case RTE_FLOW_ACTION_TYPE_FLAG:
			acts->mark = true;
			acts->rule_acts[dr_pos].tag.value =
				mlx5_flow_mark_set(MLX5_FLOW_MARK_DEFAULT);
			acts->rule_acts[dr_pos].action =
				priv->hw_tag[!!attr->group];
			rte_atomic_fetch_add_explicit(&priv->hws_mark_refcnt, 1,
					rte_memory_order_relaxed);
			flow_hw_rxq_flag_set(dev, true);
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			acts->mark = true;
			if (masks->conf &&
			    ((const struct rte_flow_action_mark *)
			     masks->conf)->id)
				acts->rule_acts[dr_pos].tag.value =
					mlx5_flow_mark_set
					(((const struct rte_flow_action_mark *)
					(actions->conf))->id);
			else if (__flow_hw_act_data_general_append(priv, acts,
								   actions->type,
								   src_pos, dr_pos))
				goto err;
			acts->rule_acts[dr_pos].action =
				priv->hw_tag[!!attr->group];
			rte_atomic_fetch_add_explicit(&priv->hws_mark_refcnt, 1,
					rte_memory_order_relaxed);
			flow_hw_rxq_flag_set(dev, true);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			acts->rule_acts[dr_pos].action =
				priv->hw_push_vlan[type];
			if (is_template_masked_push_vlan(masks->conf))
				acts->rule_acts[dr_pos].push_vlan.vlan_hdr =
					vlan_hdr_to_be32(actions);
			else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos))
				goto err;
			of_vlan_offset = is_of_vlan_pcp_present(actions) ?
					MLX5_HW_VLAN_PUSH_PCP_IDX :
					MLX5_HW_VLAN_PUSH_VID_IDX;
			actions += of_vlan_offset;
			masks += of_vlan_offset;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			acts->rule_acts[dr_pos].action =
				priv->hw_pop_vlan[type];
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			if (masks->conf &&
			    ((const struct rte_flow_action_jump *)
			     masks->conf)->group) {
				uint32_t jump_group =
					((const struct rte_flow_action_jump *)
					actions->conf)->group;
				acts->jump = flow_hw_jump_action_register
						(dev, cfg, jump_group, &sub_error);
				if (!acts->jump)
					goto err;
				acts->rule_acts[dr_pos].action = (!!attr->group) ?
								 acts->jump->hws_action :
								 acts->jump->root_action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)){
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (masks->conf &&
			    ((const struct rte_flow_action_queue *)
			     masks->conf)->index) {
				acts->tir = flow_hw_tir_action_register
				(dev, mlx5_hw_act_flag[!!attr->group][type],
				 actions);
				if (!acts->tir)
					goto err;
				acts->rule_acts[dr_pos].action =
					acts->tir->action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (actions->conf && masks->conf) {
				acts->tir = flow_hw_tir_action_register
				(dev, mlx5_hw_act_flag[!!attr->group][type],
				 actions);
				if (!acts->tir)
					goto err;
				acts->rule_acts[dr_pos].action =
					acts->tir->action;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			MLX5_ASSERT(!reformat_used);
			enc_item = MLX5_CONST_ENCAP_ITEM(rte_flow_action_vxlan_encap,
							 actions->conf);
			if (masks->conf)
				enc_item_m = MLX5_CONST_ENCAP_ITEM(rte_flow_action_vxlan_encap,
								   masks->conf);
			reformat_used = true;
			reformat_src = src_pos;
			refmt_type = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			MLX5_ASSERT(!reformat_used);
			enc_item = MLX5_CONST_ENCAP_ITEM(rte_flow_action_nvgre_encap,
							 actions->conf);
			if (masks->conf)
				enc_item_m = MLX5_CONST_ENCAP_ITEM(rte_flow_action_nvgre_encap,
								   masks->conf);
			reformat_used = true;
			reformat_src = src_pos;
			refmt_type = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
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
				MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2 :
				MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3;
			} else {
				reformat_used = true;
				refmt_type =
				MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
			}
			reformat_src = src_pos;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			MLX5_ASSERT(!reformat_used);
			reformat_used = true;
			refmt_type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			reformat_used = true;
			refmt_type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH:
			if (!hca_attr->flex.query_match_sample_info ||
			    !hca_attr->flex.parse_graph_anchor ||
			    !priv->sh->srh_flex_parser.flex.mapnum) {
				DRV_LOG(ERR, "SRv6 anchor is not supported.");
				goto err;
			}
			MLX5_ASSERT(!recom_used && !recom_type);
			recom_used = true;
			recom_type = MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT;
			ipv6_ext_data =
				(const struct rte_flow_action_ipv6_ext_push *)masks->conf;
			if (ipv6_ext_data)
				push_data_m = ipv6_ext_data->data;
			ipv6_ext_data =
				(const struct rte_flow_action_ipv6_ext_push *)actions->conf;
			if (ipv6_ext_data) {
				push_data = ipv6_ext_data->data;
				push_size = ipv6_ext_data->size;
			}
			recom_src = src_pos;
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE:
			if (!hca_attr->flex.query_match_sample_info ||
			    !hca_attr->flex.parse_graph_anchor ||
			    !priv->sh->srh_flex_parser.flex.mapnum) {
				DRV_LOG(ERR, "SRv6 anchor is not supported.");
				goto err;
			}
			recom_used = true;
			recom_type = MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT;
			break;
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
			ret = flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, &sub_error);
			if (ret)
				goto err;
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				rte_flow_error_set(&sub_error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"Send to kernel action on root table is not supported in HW steering mode");
				goto err;
			}
			acts->rule_acts[dr_pos].action = priv->hw_send_to_kernel[type];
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			err = flow_hw_modify_field_compile(dev, attr, actions,
							   masks, acts, &mhdr,
							   src_pos, &sub_error);
			if (err)
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			if (flow_hw_represented_port_compile
					(dev, attr, actions,
					 masks, acts, src_pos, dr_pos, &sub_error))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			ret = flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, &sub_error);
			if (ret)
				goto err;
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				rte_flow_error_set(&sub_error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"Age action on root table is not supported in HW steering mode");
				goto err;
			}
			if (__flow_hw_act_data_general_append(priv, acts,
							      actions->type,
							      src_pos,
							      dr_pos))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			ret = flow_hw_translate_group(dev, cfg, attr->group,
						&target_grp, &sub_error);
			if (ret)
				goto err;
			if (target_grp == 0) {
				__flow_hw_action_template_destroy(dev, acts);
				rte_flow_error_set(&sub_error, ENOTSUP,
					RTE_FLOW_ERROR_TYPE_ACTION,
					NULL,
					"Counter action on root table is not supported in HW steering mode");
				goto err;
			}
			if ((at->action_flags & MLX5_FLOW_ACTION_AGE) ||
			    (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE))
				/*
				 * When both COUNT and AGE are requested, it is
				 * saved as AGE action which creates also the
				 * counter.
				 */
				break;
			if (masks->conf &&
			    ((const struct rte_flow_action_count *)
			     masks->conf)->id) {
				err = flow_hw_cnt_compile(dev, dr_pos, acts);
				if (err)
					goto err;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			if (masks->conf) {
				ct_idx = MLX5_INDIRECT_ACTION_IDX_GET(actions->conf);
				if (flow_hw_ct_compile(dev, MLX5_HW_INV_QUEUE, ct_idx,
						       &acts->rule_acts[dr_pos]))
					goto err;
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)) {
				goto err;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			if (actions->conf && masks->conf &&
			    ((const struct rte_flow_action_meter_mark *)
			     masks->conf)->profile) {
				err = flow_hw_meter_mark_compile(dev,
								 dr_pos, actions,
								 acts->rule_acts,
								 &acts->mtr_id,
								 MLX5_HW_INV_QUEUE,
								 &sub_error);
				if (err)
					goto err;
			} else if (__flow_hw_act_data_general_append(priv, acts,
								     actions->type,
								     src_pos,
								     dr_pos))
				goto err;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			/* Internal, can be skipped. */
			if (!!attr->group) {
				DRV_LOG(ERR, "DEFAULT MISS action is only"
					" supported in root table.");
				goto err;
			}
			acts->rule_acts[dr_pos].action = priv->hw_def_miss;
			break;
		case RTE_FLOW_ACTION_TYPE_NAT64:
			if (masks->conf &&
			    ((const struct rte_flow_action_nat64 *)masks->conf)->type) {
				const struct rte_flow_action_nat64 *nat64_c =
					(const struct rte_flow_action_nat64 *)actions->conf;

				acts->rule_acts[dr_pos].action =
					priv->action_nat64[type][nat64_c->type];
			} else if (__flow_hw_act_data_general_append(priv, acts,
								     actions->type,
								     src_pos, dr_pos))
				goto err;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
			if (masks->conf &&
			    ((const struct rte_flow_action_jump_to_table_index *)
			     masks->conf)->table) {
				struct rte_flow_template_table *jump_table =
					((const struct rte_flow_action_jump_to_table_index *)
					actions->conf)->table;
				acts->rule_acts[dr_pos].jump_to_matcher.offset =
					((const struct rte_flow_action_jump_to_table_index *)
					actions->conf)->index;
				if (likely(!rte_flow_template_table_resizable(dev->data->port_id,
									&jump_table->cfg.attr))) {
					acts->rule_acts[dr_pos].action =
						jump_table->matcher_info[0].jump;
				} else {
					uint32_t selector;
					rte_rwlock_read_lock(&jump_table->matcher_replace_rwlk);
					selector = jump_table->matcher_selector;
					acts->rule_acts[dr_pos].action =
						jump_table->matcher_info[selector].jump;
					rte_rwlock_read_unlock(&jump_table->matcher_replace_rwlk);
				}
			} else if (__flow_hw_act_data_general_append
					(priv, acts, actions->type,
					 src_pos, dr_pos)){
				goto err;
			}
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_MIRROR:
			if (__flow_hw_act_data_general_append(priv, acts,
							      actions->type,
							      src_pos, dr_pos))
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
		ret = mlx5_tbl_translate_modify_header(dev, cfg, acts, mp_ctx, &mhdr, &sub_error);
		if (ret)
			goto err;
		if (!nt_mode && mhdr.shared) {
			ret = mlx5_tbl_ensure_shared_modify_header(dev, cfg, acts, &sub_error);
			if (ret)
				goto err;
		}
	}
	if (reformat_used) {
		ret = mlx5_tbl_translate_reformat(priv, acts, at,
						  enc_item, enc_item_m,
						  encap_data, encap_data_m,
						  mp_ctx, data_size,
						  reformat_src,
						  refmt_type, &sub_error);
		if (ret)
			goto err;
		if (!nt_mode && acts->encap_decap->shared) {
			ret = mlx5_tbl_create_reformat_action(priv, table_attr, acts, at,
							      encap_data, data_size,
							      refmt_type);
			if (ret)
				goto err;
		}
	}
	if (recom_used) {
		MLX5_ASSERT(at->recom_off != UINT16_MAX);
		ret = mlx5_create_ipv6_ext_reformat(dev, cfg, acts, at, push_data,
						    push_data_m, push_size, recom_src,
						    recom_type);
		if (ret)
			goto err;
	}
	return 0;
err:
	/* If rte_errno was not initialized and reached error state. */
	if (!rte_errno)
		rte_errno = EINVAL;
	err = rte_errno;
	__flow_hw_action_template_destroy(dev, acts);
	if (error != NULL && sub_error.type != RTE_FLOW_ERROR_TYPE_NONE) {
		rte_memcpy(error, &sub_error, sizeof(sub_error));
		return -EINVAL;
	}
	return rte_flow_error_set(error, err,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "fail to create rte table");
}

/**
 * Translate rte_flow actions to DR action.
 *
 * As the action template has already indicated the actions. Translate
 * the rte_flow actions to DR action if possible. So in flow create
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
flow_hw_translate_actions_template(struct rte_eth_dev *dev,
			    const struct mlx5_flow_template_table_cfg *cfg,
			    struct mlx5_hw_actions *acts,
			    struct rte_flow_actions_template *at,
			    struct mlx5_tbl_multi_pattern_ctx *mp_ctx,
			    struct rte_flow_error *error)
{
	return __flow_hw_translate_actions_template(dev, cfg, acts, at, mp_ctx, false, error);
}

static __rte_always_inline struct mlx5dr_rule_action *
flow_hw_get_dr_action_buffer(struct mlx5_priv *priv,
			     struct rte_flow_template_table *table,
			     uint8_t action_template_index,
			     uint32_t queue)
{
	uint32_t offset = action_template_index * priv->nb_queue + queue;

	return &table->rule_acts[offset].acts[0];
}

static void
flow_hw_populate_rule_acts_caches(struct rte_eth_dev *dev,
				  struct rte_flow_template_table *table,
				  uint8_t at_idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t q;

	for (q = 0; q < priv->nb_queue; ++q) {
		struct mlx5dr_rule_action *rule_acts =
				flow_hw_get_dr_action_buffer(priv, table, at_idx, q);

		rte_memcpy(rule_acts, table->ats[at_idx].acts.rule_acts,
			   sizeof(table->ats[at_idx].acts.rule_acts));
	}
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
flow_hw_translate_all_actions_templates(struct rte_eth_dev *dev,
			  struct rte_flow_template_table *tbl,
			  struct rte_flow_error *error)
{
	int ret;
	uint32_t i;

	for (i = 0; i < tbl->nb_action_templates; i++) {
		if (flow_hw_translate_actions_template(dev, &tbl->cfg,
						&tbl->ats[i].acts,
						tbl->ats[i].action_template,
						&tbl->mpctx, error))
			goto err;
		flow_hw_populate_rule_acts_caches(dev, tbl, i);
	}
	ret = mlx5_tbl_multi_pattern_process(dev, tbl, &tbl->mpctx.segments[0],
					     rte_log2_u32(tbl->cfg.attr.nb_flows),
					     error);
	if (ret)
		goto err;
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
		rss_desc.symmetric_hash_function = act_data->shared_rss.symmetric_hash_function;
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
 * @param[in] item_flags
 *   Item flags.
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
				struct rte_flow_template_table *table __rte_unused,
				const uint64_t item_flags, uint64_t action_flags,
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
	struct rte_flow_hw_aux *aux;
	uint32_t act_idx = (uint32_t)(uintptr_t)action->conf;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx &
		       ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
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
		act_data.shared_rss.symmetric_hash_function =
			MLX5_RSS_IS_SYMM(shared_rss->origin.func);

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
		flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_CNT_ID;
		flow->cnt_id = act_idx;
		break;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
		/*
		 * Save the index with the indirect type, to recognize
		 * it in flow destroy.
		 */
		mlx5_flow_hw_aux_set_age_idx(flow, aux, act_idx);
		flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX;
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
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_CNT_ID;
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
flow_hw_modify_field_construct(struct mlx5_modification_cmd *mhdr_cmd,
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
		    tag_index == RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX)
			*value_p = rte_cpu_to_be_32(*value_p << 16);
		else
			*value_p = rte_cpu_to_be_32(*value_p);
	} else if (mhdr_action->dst.field == RTE_FLOW_FIELD_GTP_PSC_QFI ||
		   mhdr_action->dst.field == RTE_FLOW_FIELD_GENEVE_OPT_TYPE) {
		uint32_t tmp;

		/*
		 * Both QFI and Geneve option type are passed as an uint8_t integer,
		 * but it is accessed through a 2nd least significant byte of a 32-bit
		 * field in modify header command.
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
		if (flow_hw_mhdr_cmd_is_nop(&mhdr_cmd[i])) {
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
		/*
		 * IPv6 DSCP uses OUT_IPV6_TRAFFIC_CLASS as ID but it starts from 2
		 * bits left. Shift the data left for IPv6 DSCP
		 */
		if (field->id == MLX5_MODI_OUT_IPV6_TRAFFIC_CLASS &&
		    mhdr_action->dst.field == RTE_FLOW_FIELD_IPV6_DSCP)
			data <<= MLX5_IPV6_HDR_DSCP_SHIFT;
		data = (data & mask) >> off_b;
		mhdr_cmd[i++].data1 = rte_cpu_to_be_32(data);
		++field;
	} while (field->size);
	return 0;
}

/**
 * Release any actions allocated for the flow rule during actions construction.
 *
 * @param[in] flow
 *   Pointer to flow structure.
 */
static void
flow_hw_release_actions(struct rte_eth_dev *dev,
			uint32_t queue,
			struct rte_flow_hw *flow)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(dev->data->port_id, flow);

	if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_FATE_JUMP)
		flow_hw_jump_release(dev, flow->jump);
	else if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_FATE_HRXQ)
		mlx5_hrxq_obj_release(dev, flow->hrxq);
	if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_CNT_ID)
		flow_hw_age_count_release(priv, queue, flow, NULL);
	if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_MTR_ID)
		mlx5_ipool_free(pool->idx_pool, mlx5_flow_hw_aux_get_mtr_id(flow, aux));
}

/**
 * Construct flow action array.
 *
 * For action template contains dynamic actions, these actions need to
 * be updated according to the rte_flow action during flow creation.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] flow
 *   Pointer to flow structure.
 * @param[in] ap
 *   Pointer to container for temporarily constructed actions' parameters.
 * @param[in] hw_acts
 *   Pointer to translated actions from template.
 * @param[in] items_flags
 *   Item flags.
 * @param[in] table
 *   Pointer to the template table.
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
			  struct rte_flow_hw *flow,
			  struct mlx5_flow_hw_action_params *ap,
			  const struct mlx5_hw_action_template *hw_at,
			  uint64_t item_flags,
			  struct rte_flow_template_table *table,
			  const struct rte_flow_action actions[],
			  struct mlx5dr_rule_action *rule_acts,
			  uint32_t queue,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct mlx5_action_construct_data *act_data;
	const struct rte_flow_actions_template *at = hw_at->action_template;
	const struct mlx5_hw_actions *hw_acts = &hw_at->acts;
	const struct rte_flow_action *action;
	const struct rte_flow_action_raw_encap *raw_encap_data;
	const struct rte_flow_action_ipv6_ext_push *ipv6_push;
	const struct rte_flow_item *enc_item = NULL;
	const struct rte_flow_action_ethdev *port_action = NULL;
	const struct rte_flow_action_age *age = NULL;
	const struct rte_flow_action_nat64 *nat64_c = NULL;
	struct rte_flow_attr attr = {
		.ingress = 1,
	};
	uint32_t ft_flag;
	int ret;
	size_t encap_len = 0;
	uint32_t age_idx = 0;
	uint32_t mtr_idx = 0;
	struct mlx5_aso_mtr *aso_mtr;
	struct mlx5_multi_pattern_segment *mp_segment = NULL;
	struct rte_flow_hw_aux *aux;

	attr.group = table->grp->group_id;
	ft_flag = mlx5_hw_act_flag[!!table->grp->group_id][table->type];
	if (table->type >= MLX5DR_TABLE_TYPE_FDB && table->type < MLX5DR_TABLE_TYPE_MAX) {
		attr.transfer = 1;
		attr.ingress = 1;
	} else if (table->type == MLX5DR_TABLE_TYPE_NIC_TX) {
		attr.egress = 1;
		attr.ingress = 0;
	} else {
		attr.ingress = 1;
	}
	if (hw_acts->mhdr && hw_acts->mhdr->mhdr_cmds_num > 0 && !hw_acts->mhdr->shared) {
		uint16_t pos = hw_acts->mhdr->pos;

		mp_segment = mlx5_multi_pattern_segment_find(table, flow->res_idx);
		if (!mp_segment || !mp_segment->mhdr_action)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "No modify header action found");
		rule_acts[pos].action = mp_segment->mhdr_action;
		/* offset is relative to DR action */
		rule_acts[pos].modify_header.offset =
					flow->res_idx - mp_segment->head_index;
		rule_acts[pos].modify_header.data =
					(uint8_t *)ap->mhdr_cmd;
		MLX5_ASSERT(hw_acts->mhdr->mhdr_cmds_num <= MLX5_MHDR_MAX_CMD);
		rte_memcpy(ap->mhdr_cmd, hw_acts->mhdr->mhdr_cmds,
			   sizeof(*ap->mhdr_cmd) * hw_acts->mhdr->mhdr_cmds_num);
	}
	LIST_FOREACH(act_data, &hw_acts->act_list, next) {
		uint32_t jump_group;
		uint32_t tag;
		struct mlx5_hw_jump_action *jump;
		struct mlx5_hrxq *hrxq;
		uint32_t ct_idx;
		cnt_id_t cnt_id;
		uint32_t *cnt_queue;
		uint32_t mtr_id;
		struct rte_flow_template_table *jump_table;

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
		switch ((int)act_data->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT_LIST:
			act_data->indirect_list_cb(dev, act_data, action,
						   &rule_acts[act_data->action_dst]);
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			if (flow_hw_shared_action_construct
					(dev, queue, action, table,
					 item_flags, at->action_flags, flow,
					 &rule_acts[act_data->action_dst]))
				goto error;
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
				goto error;
			rule_acts[act_data->action_dst].action =
			(!!attr.group) ? jump->hws_action : jump->root_action;
			flow->jump = jump;
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_FATE_JUMP;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			hrxq = flow_hw_tir_action_register(dev, ft_flag, action);
			if (!hrxq)
				goto error;
			rule_acts[act_data->action_dst].action = hrxq->action;
			flow->hrxq = hrxq;
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_FATE_HRXQ;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_RSS:
			if (flow_hw_shared_action_get
				(dev, act_data, item_flags,
				 &rule_acts[act_data->action_dst]))
				goto error;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			enc_item = ((const struct rte_flow_action_vxlan_encap *)
				   action->conf)->definition;
			if (flow_dv_convert_encap_data(enc_item, ap->encap_data, &encap_len, NULL))
				goto error;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			enc_item = ((const struct rte_flow_action_nvgre_encap *)
				   action->conf)->definition;
			if (flow_dv_convert_encap_data(enc_item, ap->encap_data, &encap_len, NULL))
				goto error;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap_data =
				(const struct rte_flow_action_raw_encap *)
				 action->conf;
			MLX5_ASSERT(raw_encap_data->size == act_data->encap.len);
			if (unlikely(act_data->encap.len > MLX5_ENCAP_MAX_LEN))
				return -1;
			rte_memcpy(ap->encap_data, raw_encap_data->data, act_data->encap.len);
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH:
			ipv6_push =
				(const struct rte_flow_action_ipv6_ext_push *)action->conf;
			MLX5_ASSERT(ipv6_push->size == act_data->ipv6_ext.len);
			if (unlikely(act_data->ipv6_ext.len > MLX5_PUSH_MAX_LEN))
				return -1;
			rte_memcpy(ap->ipv6_push_data, ipv6_push->data,
				   act_data->ipv6_ext.len);
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			if (action->type == RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID)
				ret = flow_hw_set_vlan_vid_construct(dev, ap->mhdr_cmd,
								     act_data,
								     hw_acts,
								     action);
			else
				ret = flow_hw_modify_field_construct(ap->mhdr_cmd,
								     act_data,
								     hw_acts,
								     action);
			if (ret)
				goto error;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			port_action = action->conf;
			if (!priv->hw_vport[port_action->port_id])
				goto error;
			rule_acts[act_data->action_dst].action =
					priv->hw_vport[port_action->port_id];
			break;
		case RTE_FLOW_ACTION_TYPE_QUOTA:
			flow_hw_construct_quota(priv,
						rule_acts + act_data->action_dst,
						act_data->shared_meter.id);
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
			age = action->conf;
			/*
			 * First, create the AGE parameter, then create its
			 * counter later:
			 * Regular counter - in next case.
			 * Indirect counter - update it after the loop.
			 */
			age_idx = mlx5_hws_age_action_create(priv, queue, 0,
							     age,
							     flow->res_idx,
							     error);
			if (age_idx == 0)
				goto error;
			mlx5_flow_hw_aux_set_age_idx(flow, aux, age_idx);
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX;
			if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT)
				/*
				 * When AGE uses indirect counter, no need to
				 * create counter but need to update it with the
				 * AGE parameter, will be done after the loop.
				 */
				break;
			/* Fall-through. */
		case RTE_FLOW_ACTION_TYPE_COUNT:
			cnt_queue = mlx5_hws_cnt_get_queue(priv, &queue);
			ret = mlx5_hws_cnt_pool_get(priv->hws_cpool, cnt_queue, &cnt_id, age_idx);
			if (ret != 0) {
				rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_ACTION,
						action, "Failed to allocate flow counter");
				goto error;
			}
			ret = mlx5_hws_cnt_pool_get_action_offset
				(priv->hws_cpool,
				 cnt_id,
				 &rule_acts[act_data->action_dst].action,
				 &rule_acts[act_data->action_dst].counter.offset
				 );
			if (ret != 0)
				goto error;
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_CNT_ID;
			flow->cnt_id = cnt_id;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
			ret = mlx5_hws_cnt_pool_get_action_offset
				(priv->hws_cpool,
				 act_data->shared_counter.id,
				 &rule_acts[act_data->action_dst].action,
				 &rule_acts[act_data->action_dst].counter.offset
				 );
			if (ret != 0)
				goto error;
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_CNT_ID;
			flow->cnt_id = act_data->shared_counter.id;
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
			ct_idx = MLX5_INDIRECT_ACTION_IDX_GET(action->conf);
			if (flow_hw_ct_compile(dev, queue, ct_idx,
					       &rule_acts[act_data->action_dst]))
				goto error;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK:
			mtr_id = act_data->shared_meter.id &
				((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
			/* Find ASO object. */
			aso_mtr = mlx5_ipool_get(pool->idx_pool, mtr_id);
			if (!aso_mtr)
				goto error;
			rule_acts[act_data->action_dst].action =
							pool->action;
			rule_acts[act_data->action_dst].aso_meter.offset =
							aso_mtr->offset;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			/*
			 * Allocate meter directly will slow down flow
			 * insertion rate.
			 */
			ret = flow_hw_meter_mark_compile(dev,
				act_data->action_dst, action,
				rule_acts, &mtr_idx, MLX5_HW_INV_QUEUE, error);
			if (ret != 0)
				goto error;
			aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
			mlx5_flow_hw_aux_set_mtr_id(flow, aux, mtr_idx);
			flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_MTR_ID;
			break;
		case RTE_FLOW_ACTION_TYPE_NAT64:
			nat64_c = action->conf;
			MLX5_ASSERT(table->type < MLX5DR_TABLE_TYPE_MAX);
			rule_acts[act_data->action_dst].action =
				priv->action_nat64[table->type][nat64_c->type];
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
			jump_table = ((const struct rte_flow_action_jump_to_table_index *)
						action->conf)->table;
			if (likely(!rte_flow_template_table_resizable(dev->data->port_id,
								      &table->cfg.attr))) {
				rule_acts[act_data->action_dst].action =
					jump_table->matcher_info[0].jump;
			} else {
				uint32_t selector;
				rte_rwlock_read_lock(&table->matcher_replace_rwlk);
				selector = table->matcher_selector;
				rule_acts[act_data->action_dst].action =
					jump_table->matcher_info[selector].jump;
				rte_rwlock_read_unlock(&table->matcher_replace_rwlk);
			}
			rule_acts[act_data->action_dst].jump_to_matcher.offset =
				((const struct rte_flow_action_jump_to_table_index *)
				action->conf)->index;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_MIRROR: {
			const struct mlx5_mirror *mirror = action->conf;

			rule_acts[act_data->action_dst].action = mirror->mirror_action;
			break;
		}
		default:
			break;
		}
	}
	if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_COUNT) {
		/* If indirect count is used, then CNT_ID flag should be set. */
		MLX5_ASSERT(flow->flags & MLX5_FLOW_HW_FLOW_FLAG_CNT_ID);
		if (at->action_flags & MLX5_FLOW_ACTION_INDIRECT_AGE) {
			/* If indirect AGE is used, then AGE_IDX flag should be set. */
			MLX5_ASSERT(flow->flags & MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX);
			aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
			age_idx = mlx5_flow_hw_aux_get_age_idx(flow, aux) &
				  MLX5_HWS_AGE_IDX_MASK;
			if (mlx5_hws_cnt_age_get(priv->hws_cpool, flow->cnt_id) != age_idx)
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
		mlx5_hws_cnt_age_set(priv->hws_cpool, flow->cnt_id, age_idx);
	}
	if (hw_acts->encap_decap && !hw_acts->encap_decap->shared) {
		int ix = mlx5_multi_pattern_reformat_to_index(hw_acts->encap_decap->action_type);
		struct mlx5dr_rule_action *ra = &rule_acts[hw_acts->encap_decap_pos];

		if (ix < 0)
			goto error;
		if (!mp_segment)
			mp_segment = mlx5_multi_pattern_segment_find(table, flow->res_idx);
		if (!mp_segment || !mp_segment->reformat_action[ix])
			goto error;
		ra->action = mp_segment->reformat_action[ix];
		/* reformat offset is relative to selected DR action */
		ra->reformat.offset = flow->res_idx - mp_segment->head_index;
		ra->reformat.data = ap->encap_data;
	}
	if (hw_acts->push_remove && !hw_acts->push_remove->shared) {
		rule_acts[hw_acts->push_remove_pos].ipv6_ext.offset =
				flow->res_idx - 1;
		rule_acts[hw_acts->push_remove_pos].ipv6_ext.header = ap->ipv6_push_data;
	}
	if (mlx5_hws_cnt_id_valid(hw_acts->cnt_id)) {
		flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_CNT_ID;
		flow->cnt_id = hw_acts->cnt_id;
	}
	return 0;

error:
	flow_hw_release_actions(dev, queue, flow);
	return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "Action construction failed");
}

static const struct rte_flow_item *
flow_hw_get_rule_items(struct rte_eth_dev *dev,
		       const struct rte_flow_template_table *table,
		       const struct rte_flow_item items[],
		       uint8_t pattern_template_index,
		       struct mlx5_flow_hw_pattern_params *pp)
{
	struct rte_flow_pattern_template *pt = table->its[pattern_template_index];

	/* Only one implicit item can be added to flow rule pattern. */
	MLX5_ASSERT(!pt->implicit_port || !pt->implicit_tag);
	/* At least one item was allocated in pattern params for items. */
	MLX5_ASSERT(MLX5_HW_MAX_ITEMS >= 1);
	if (pt->implicit_port) {
		if (pt->orig_item_nb + 1 > MLX5_HW_MAX_ITEMS) {
			rte_errno = ENOMEM;
			return NULL;
		}
		/* Set up represented port item in pattern params. */
		pp->port_spec = (struct rte_flow_item_ethdev){
			.port_id = dev->data->port_id,
		};
		pp->items[0] = (struct rte_flow_item){
			.type = RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT,
			.spec = &pp->port_spec,
		};
		rte_memcpy(&pp->items[1], items, sizeof(*items) * pt->orig_item_nb);
		return pp->items;
	} else if (pt->implicit_tag) {
		if (pt->orig_item_nb + 1 > MLX5_HW_MAX_ITEMS) {
			rte_errno = ENOMEM;
			return NULL;
		}
		/* Set up tag item in pattern params. */
		pp->tag_spec = (struct rte_flow_item_tag){
			.data = flow_hw_tx_tag_regc_value(dev),
		};
		pp->items[0] = (struct rte_flow_item){
			.type = (enum rte_flow_item_type)MLX5_RTE_FLOW_ITEM_TYPE_TAG,
			.spec = &pp->tag_spec,
		};
		rte_memcpy(&pp->items[1], items, sizeof(*items) * pt->orig_item_nb);
		return pp->items;
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
 * @param[in] table
 *   Pointer to the template table.
 * @param[in] insertion_type
 *   Insertion type for flow rules.
 * @param[in] rule_index
 *   The item pattern flow follows from the table.
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
static __rte_always_inline struct rte_flow *
flow_hw_async_flow_create_generic(struct rte_eth_dev *dev,
				  uint32_t queue,
				  const struct rte_flow_op_attr *attr,
				  struct rte_flow_template_table *table,
				  enum rte_flow_table_insertion_type insertion_type,
				  uint32_t rule_index,
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
	struct mlx5dr_rule_action *rule_acts;
	struct rte_flow_hw *flow = NULL;
	const struct rte_flow_item *rule_items;
	struct rte_flow_error sub_error = { 0 };
	uint32_t flow_idx = 0;
	uint32_t res_idx = 0;
	int ret;

	if (mlx5_fp_debug_enabled()) {
		if (flow_hw_async_create_validate(dev, queue, table, insertion_type, rule_index,
			items, pattern_template_index, actions, action_template_index, error))
			return NULL;
	}
	flow = mlx5_ipool_malloc(table->flow, &flow_idx);
	if (!flow) {
		rte_errno = ENOMEM;
		goto error;
	}
	flow->nt_rule = false;
	rule_acts = flow_hw_get_dr_action_buffer(priv, table, action_template_index, queue);
	/*
	 * Set the table here in order to know the destination table
	 * when free the flow afterward.
	 */
	flow->table = table;
	flow->mt_idx = pattern_template_index;
	flow->idx = flow_idx;
	if (table->resource) {
		mlx5_ipool_malloc(table->resource, &res_idx);
		if (!res_idx) {
			rte_errno = ENOMEM;
			goto error;
		}
		flow->res_idx = res_idx;
	} else {
		flow->res_idx = flow_idx;
	}
	flow->flags = 0;
	/*
	 * Set the flow operation type here in order to know if the flow memory
	 * should be freed or not when get the result from dequeue.
	 */
	flow->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_CREATE;
	flow->user_data = user_data;
	rule_attr.user_data = flow;
	/*
	 * Indexed pool returns 1-based indices, but mlx5dr expects 0-based indices
	 * for rule insertion hints.
	 */
	flow->rule_idx = (rule_index == UINT32_MAX) ? flow->res_idx - 1 : rule_index;
	rule_attr.rule_idx = flow->rule_idx;
	/*
	 * Construct the flow actions based on the input actions.
	 * The implicitly appended action is always fixed, like metadata
	 * copy action from FDB to NIC Rx.
	 * No need to copy and contrust a new "actions" list based on the
	 * user's input, in order to save the cost.
	 */
	if (flow_hw_actions_construct(dev, flow, &priv->hw_q[queue].ap,
				      &table->ats[action_template_index],
				      table->its[pattern_template_index]->item_flags,
				      flow->table, actions,
				      rule_acts, queue, &sub_error))
		goto error;
	rule_items = flow_hw_get_rule_items(dev, table, items,
					    pattern_template_index, &priv->hw_q[queue].pp);
	if (!rule_items)
		goto error;
	if (likely(!rte_flow_template_table_resizable(dev->data->port_id, &table->cfg.attr))) {
		ret = mlx5dr_rule_create(table->matcher_info[0].matcher,
					 pattern_template_index, rule_items,
					 action_template_index, rule_acts,
					 &rule_attr,
					 (struct mlx5dr_rule *)flow->rule);
	} else {
		struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
		uint32_t selector;

		flow->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_CREATE;
		rte_rwlock_read_lock(&table->matcher_replace_rwlk);
		selector = table->matcher_selector;
		ret = mlx5dr_rule_create(table->matcher_info[selector].matcher,
					 pattern_template_index, rule_items,
					 action_template_index, rule_acts,
					 &rule_attr,
					 (struct mlx5dr_rule *)flow->rule);
		rte_rwlock_read_unlock(&table->matcher_replace_rwlk);
		aux->matcher_selector = selector;
		flow->flags |= MLX5_FLOW_HW_FLOW_FLAG_MATCHER_SELECTOR;
	}

	if (likely(!ret)) {
		flow_hw_q_inc_flow_ops(priv, queue);
		return (struct rte_flow *)flow;
	}
error:
	if (table->resource && res_idx)
		mlx5_ipool_free(table->resource, res_idx);
	if (flow_idx)
		mlx5_ipool_free(table->flow, flow_idx);
	if (sub_error.cause != RTE_FLOW_ERROR_TYPE_NONE && error != NULL)
		*error = sub_error;
	else
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "fail to create rte flow");
	return NULL;
}

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
	uint32_t rule_index = UINT32_MAX;

	return flow_hw_async_flow_create_generic(dev, queue, attr, table,
		RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN, rule_index,
		items, pattern_template_index, actions, action_template_index,
		user_data, error);
}

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
	struct rte_flow_item items[] = {{.type = RTE_FLOW_ITEM_TYPE_END,}};
	uint8_t pattern_template_index = 0;

	return flow_hw_async_flow_create_generic(dev, queue, attr, table,
		RTE_FLOW_TABLE_INSERTION_TYPE_INDEX, rule_index,
		items, pattern_template_index, actions, action_template_index,
		user_data, error);
}

static struct rte_flow *
flow_hw_async_flow_create_by_index_with_pattern(struct rte_eth_dev *dev,
						uint32_t queue,
						const struct rte_flow_op_attr *attr,
						struct rte_flow_template_table *table,
						uint32_t rule_index,
						const struct rte_flow_item items[],
						uint8_t pattern_template_index,
						const struct rte_flow_action actions[],
						uint8_t action_template_index,
						void *user_data,
						struct rte_flow_error *error)
{
	return flow_hw_async_flow_create_generic(dev, queue, attr, table,
		RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN, rule_index,
		items, pattern_template_index, actions, action_template_index,
		user_data, error);
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
	struct mlx5dr_rule_action *rule_acts;
	struct rte_flow_hw *of = (struct rte_flow_hw *)flow;
	struct rte_flow_hw *nf;
	struct rte_flow_hw_aux *aux;
	struct rte_flow_template_table *table = of->table;
	uint32_t res_idx = 0;
	int ret;

	if (mlx5_fp_debug_enabled()) {
		if (flow_hw_async_update_validate(dev, queue, of, actions, action_template_index,
						  error))
			return -rte_errno;
	}
	aux = mlx5_flow_hw_aux(dev->data->port_id, of);
	nf = &aux->upd_flow;
	nf->nt_rule = false;
	rule_acts = flow_hw_get_dr_action_buffer(priv, table, action_template_index, queue);
	/*
	 * Set the table here in order to know the destination table
	 * when free the flow afterwards.
	 */
	nf->table = table;
	nf->mt_idx = of->mt_idx;
	nf->idx = of->idx;
	if (table->resource) {
		mlx5_ipool_malloc(table->resource, &res_idx);
		if (!res_idx) {
			rte_errno = ENOMEM;
			goto error;
		}
		nf->res_idx = res_idx;
	} else {
		nf->res_idx = of->res_idx;
	}
	nf->flags = 0;
	/* Indicate the construction function to set the proper fields. */
	nf->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE;
	/*
	 * Indexed pool returns 1-based indices, but mlx5dr expects 0-based indices
	 * for rule insertion hints.
	 * If there is only one STE, the update will be atomic by nature.
	 */
	nf->rule_idx = nf->res_idx - 1;
	rule_attr.rule_idx = nf->rule_idx;
	/*
	 * Construct the flow actions based on the input actions.
	 * The implicitly appended action is always fixed, like metadata
	 * copy action from FDB to NIC Rx.
	 * No need to copy and contrust a new "actions" list based on the
	 * user's input, in order to save the cost.
	 */
	if (flow_hw_actions_construct(dev, nf, &priv->hw_q[queue].ap,
				      &table->ats[action_template_index],
				      table->its[nf->mt_idx]->item_flags,
				      table, actions,
				      rule_acts, queue, error)) {
		rte_errno = EINVAL;
		goto error;
	}
	/*
	 * Set the flow operation type here in order to know if the flow memory
	 * should be freed or not when get the result from dequeue.
	 */
	of->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE;
	of->user_data = user_data;
	of->flags |= MLX5_FLOW_HW_FLOW_FLAG_UPD_FLOW;
	rule_attr.user_data = of;
	ret = mlx5dr_rule_action_update((struct mlx5dr_rule *)of->rule,
					action_template_index, rule_acts, &rule_attr);
	if (likely(!ret)) {
		flow_hw_q_inc_flow_ops(priv, queue);
		return 0;
	}
error:
	if (table->resource && res_idx)
		mlx5_ipool_free(table->resource, res_idx);
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
	bool resizable = rte_flow_template_table_resizable(dev->data->port_id,
							   &fh->table->cfg.attr);
	int ret;

	if (mlx5_fp_debug_enabled()) {
		if (flow_hw_async_destroy_validate(dev, queue, fh, error))
			return -rte_errno;
	}
	fh->operation_type = !resizable ?
			     MLX5_FLOW_HW_FLOW_OP_TYPE_DESTROY :
			     MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_DESTROY;
	fh->user_data = user_data;
	rule_attr.user_data = fh;
	rule_attr.rule_idx = fh->rule_idx;
	ret = mlx5dr_rule_destroy((struct mlx5dr_rule *)fh->rule, &rule_attr);
	if (ret) {
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "fail to destroy rte flow");
	}
	flow_hw_q_inc_flow_ops(priv, queue);
	return 0;
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
	struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(priv->dev_data->port_id, flow);
	uint32_t *cnt_queue;
	uint32_t age_idx = aux->orig.age_idx;

	MLX5_ASSERT(flow->flags & MLX5_FLOW_HW_FLOW_FLAG_CNT_ID);
	if (mlx5_hws_cnt_is_shared(priv->hws_cpool, flow->cnt_id)) {
		if ((flow->flags & MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX) &&
		    !mlx5_hws_age_is_indirect(age_idx)) {
			/* Remove this AGE parameter from indirect counter. */
			mlx5_hws_cnt_age_set(priv->hws_cpool, flow->cnt_id, 0);
			/* Release the AGE parameter. */
			mlx5_hws_age_action_destroy(priv, age_idx, error);
		}
		return;
	}
	cnt_queue = mlx5_hws_cnt_get_queue(priv, &queue);
	/* Put the counter first to reduce the race risk in BG thread. */
	mlx5_hws_cnt_pool_put(priv->hws_cpool, cnt_queue, &flow->cnt_id);
	if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX) {
		if (mlx5_hws_age_is_indirect(age_idx)) {
			uint32_t idx = age_idx & MLX5_HWS_AGE_IDX_MASK;

			mlx5_hws_age_nb_cnt_decrease(priv, idx);
		} else {
			/* Release the AGE parameter. */
			mlx5_hws_age_action_destroy(priv, age_idx, error);
		}
	}
}

static __rte_always_inline void
flow_hw_pull_legacy_indirect_comp(struct rte_eth_dev *dev, struct mlx5_hw_q_job *job,
				  uint32_t queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_action *aso_ct;
	struct mlx5_aso_mtr *aso_mtr;
	uint32_t type, idx;

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
			idx = MLX5_INDIRECT_ACTION_IDX_GET(job->action);
			aso_ct = mlx5_ipool_get(priv->hws_ctpool->cts, idx);
			aso_ct->state = ASO_CONNTRACK_READY;
		}
	} else if (job->type == MLX5_HW_Q_JOB_TYPE_QUERY) {
		type = MLX5_INDIRECT_ACTION_TYPE_GET(job->action);
		if (type == MLX5_INDIRECT_ACTION_TYPE_CT) {
			idx = MLX5_INDIRECT_ACTION_IDX_GET(job->action);
			aso_ct = mlx5_ipool_get(priv->hws_ctpool->cts, idx);
			mlx5_aso_ct_obj_analyze(job->query.user,
						job->query.hw);
			aso_ct->state = ASO_CONNTRACK_READY;
		}
	}
}

static __rte_always_inline int
mlx5_hw_pull_flow_transfer_comp(struct rte_eth_dev *dev,
				uint32_t queue, struct rte_flow_op_result res[],
				uint16_t n_res)
{
	uint32_t size, i;
	struct rte_flow_hw *flow = NULL;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_ring *ring = priv->hw_q[queue].flow_transfer_completed;

	size = RTE_MIN(rte_ring_count(ring), n_res);
	for (i = 0; i < size; i++) {
		res[i].status = RTE_FLOW_OP_SUCCESS;
		rte_ring_dequeue(ring, (void **)&flow);
		res[i].user_data = flow->user_data;
		flow_hw_q_dec_flow_ops(priv, queue);
	}
	return (int)size;
}

static inline int
__flow_hw_pull_indir_action_comp(struct rte_eth_dev *dev,
				 uint32_t queue,
				 struct rte_flow_op_result res[],
				 uint16_t n_res)

{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_ring *r = priv->hw_q[queue].indir_cq;
	void *user_data = NULL;
	int ret_comp, i;

	ret_comp = (int)rte_ring_count(r);
	if (ret_comp > n_res)
		ret_comp = n_res;
	for (i = 0; i < ret_comp; i++) {
		rte_ring_dequeue(r, &user_data);
		res[i].user_data = user_data;
		res[i].status = RTE_FLOW_OP_SUCCESS;
	}
	if (!priv->shared_host) {
		if (ret_comp < n_res && priv->hws_mpool)
			ret_comp += mlx5_aso_pull_completion(&priv->hws_mpool->sq[queue],
					&res[ret_comp], n_res - ret_comp);
		if (ret_comp < n_res && priv->hws_ctpool)
			ret_comp += mlx5_aso_pull_completion(&priv->ct_mng->aso_sqs[queue],
					&res[ret_comp], n_res - ret_comp);
	}
	if (ret_comp < n_res && priv->quota_ctx.sq)
		ret_comp += mlx5_aso_pull_completion(&priv->quota_ctx.sq[queue],
						     &res[ret_comp],
						     n_res - ret_comp);
	for (i = 0; i <  ret_comp; i++) {
		struct mlx5_hw_q_job *job = (struct mlx5_hw_q_job *)res[i].user_data;

		/* Restore user data. */
		res[i].user_data = job->user_data;
		if (job->indirect_type == MLX5_HW_INDIRECT_TYPE_LEGACY)
			flow_hw_pull_legacy_indirect_comp(dev, job, queue);
		/*
		 * Current PMD supports 2 indirect action list types - MIRROR and REFORMAT.
		 * These indirect list types do not post WQE to create action.
		 * Future indirect list types that do post WQE will add
		 * completion handlers here.
		 */
		flow_hw_job_put(priv, job, queue);
	}
	return ret_comp;
}

static __rte_always_inline void
hw_cmpl_flow_update_or_destroy(struct rte_eth_dev *dev,
			       struct rte_flow_hw *flow,
			       uint32_t queue, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	struct rte_flow_template_table *table = flow->table;
	/* Release the original resource index in case of update. */
	uint32_t res_idx = flow->res_idx;

	if (flow->flags & MLX5_FLOW_HW_FLOW_FLAGS_ALL) {
		struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(dev->data->port_id, flow);

		if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_FATE_JUMP)
			flow_hw_jump_release(dev, flow->jump);
		else if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_FATE_HRXQ)
			mlx5_hrxq_obj_release(dev, flow->hrxq);
		if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_CNT_ID)
			flow_hw_age_count_release(priv, queue, flow, error);
		if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_MTR_ID)
			mlx5_ipool_free(pool->idx_pool, aux->orig.mtr_id);
		if (flow->flags & MLX5_FLOW_HW_FLOW_FLAG_UPD_FLOW) {
			struct rte_flow_hw *upd_flow = &aux->upd_flow;

			rte_memcpy(flow, upd_flow, offsetof(struct rte_flow_hw, rule));
			aux->orig = aux->upd;
			flow->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_CREATE;
			if (!flow->nt_rule && table->resource)
				mlx5_ipool_free(table->resource, res_idx);
		}
	}
	if (flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_DESTROY ||
	    flow->operation_type == MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_DESTROY) {
		if (!flow->nt_rule) {
			if (table->resource)
				mlx5_ipool_free(table->resource, res_idx);
			mlx5_ipool_free(table->flow, flow->idx);
		}
	}
}

static __rte_always_inline void
hw_cmpl_resizable_tbl(struct rte_eth_dev *dev,
		      struct rte_flow_hw *flow,
		      uint32_t queue, enum rte_flow_op_status status,
		      struct rte_flow_error *error)
{
	struct rte_flow_template_table *table = flow->table;
	struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(dev->data->port_id, flow);
	uint32_t selector = aux->matcher_selector;
	uint32_t other_selector = (selector + 1) & 1;

	MLX5_ASSERT(flow->flags & MLX5_FLOW_HW_FLOW_FLAG_MATCHER_SELECTOR);
	switch (flow->operation_type) {
	case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_CREATE:
		rte_atomic_fetch_add_explicit
			(&table->matcher_info[selector].refcnt, 1,
			 rte_memory_order_relaxed);
		break;
	case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_DESTROY:
		rte_atomic_fetch_sub_explicit
			(&table->matcher_info[selector].refcnt, 1,
			 rte_memory_order_relaxed);
		hw_cmpl_flow_update_or_destroy(dev, flow, queue, error);
		break;
	case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_MOVE:
		if (status == RTE_FLOW_OP_SUCCESS) {
			rte_atomic_fetch_sub_explicit
				(&table->matcher_info[selector].refcnt, 1,
				 rte_memory_order_relaxed);
			rte_atomic_fetch_add_explicit
				(&table->matcher_info[other_selector].refcnt, 1,
				 rte_memory_order_relaxed);
			aux->matcher_selector = other_selector;
		}
		break;
	default:
		break;
	}
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
	int ret, i;

	/* 1. Pull the flow completion. */
	ret = mlx5dr_send_queue_poll(priv->dr_ctx, queue, res, n_res);
	if (ret < 0)
		return rte_flow_error_set(error, rte_errno,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"fail to query flow queue");
	for (i = 0; i <  ret; i++) {
		struct rte_flow_hw *flow = res[i].user_data;

		/* Restore user data. */
		res[i].user_data = flow->user_data;
		switch (flow->operation_type) {
		case MLX5_FLOW_HW_FLOW_OP_TYPE_DESTROY:
		case MLX5_FLOW_HW_FLOW_OP_TYPE_UPDATE:
			hw_cmpl_flow_update_or_destroy(dev, flow, queue, error);
			break;
		case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_CREATE:
		case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_DESTROY:
		case MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_MOVE:
			hw_cmpl_resizable_tbl(dev, flow, queue, res[i].status, error);
			break;
		default:
			break;
		}
		flow_hw_q_dec_flow_ops(priv, queue);
	}
	/* 2. Pull indirect action comp. */
	if (ret < n_res)
		ret += __flow_hw_pull_indir_action_comp(dev, queue, &res[ret],
							n_res - ret);
	if (ret < n_res)
		ret += mlx5_hw_pull_flow_transfer_comp(dev, queue, &res[ret],
						       n_res - ret);

	return ret;
}

static uint32_t
mlx5_hw_push_queue(struct rte_ring *pending_q, struct rte_ring *cmpl_q)
{
	void *job = NULL;
	uint32_t i, size = rte_ring_count(pending_q);

	for (i = 0; i < size; i++) {
		rte_ring_dequeue(pending_q, &job);
		rte_ring_enqueue(cmpl_q, job);
	}
	return size;
}

static inline uint32_t
__flow_hw_push_action(struct rte_eth_dev *dev,
		    uint32_t queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_q *hw_q = &priv->hw_q[queue];

	mlx5_hw_push_queue(hw_q->indir_iq, hw_q->indir_cq);
	mlx5_hw_push_queue(hw_q->flow_transfer_pending,
			   hw_q->flow_transfer_completed);
	if (!priv->shared_host) {
		if (priv->hws_ctpool)
			mlx5_aso_push_wqe(priv->sh,
					  &priv->ct_mng->aso_sqs[queue]);
		if (priv->hws_mpool)
			mlx5_aso_push_wqe(priv->sh,
					  &priv->hws_mpool->sq[queue]);
	}
	return flow_hw_q_pending(priv, queue);
}

static int
__flow_hw_push(struct rte_eth_dev *dev,
	       uint32_t queue,
	       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret, num;

	num = __flow_hw_push_action(dev, queue);
	ret = mlx5dr_send_queue_action(priv->dr_ctx, queue,
				       MLX5DR_SEND_QUEUE_ACTION_DRAIN_ASYNC);
	if (ret) {
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "fail to push flows");
		return ret;
	}
	return num;
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
	     uint32_t queue, struct rte_flow_error *error)
{
	int ret = __flow_hw_push(dev, queue, error);

	return ret >= 0 ? 0 : ret;
}

/**
 * Drain the enqueued flows' completion.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to pull the flow.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 on success, negative value otherwise and rte_errno is set.
 */
static int
__flow_hw_pull_comp(struct rte_eth_dev *dev,
		    uint32_t queue, struct rte_flow_error *error)
{
	struct rte_flow_op_result comp[BURST_THR];
	int ret, i, empty_loop = 0;
	uint32_t pending_rules;

	ret = __flow_hw_push(dev, queue, error);
	if (ret < 0)
		return ret;
	pending_rules = ret;
	while (pending_rules) {
		ret = flow_hw_pull(dev, queue, comp, BURST_THR, error);
		if (ret < 0)
			return -1;
		if (!ret) {
			rte_delay_us_sleep(MLX5_ASO_WQE_CQE_RESPONSE_DELAY);
			if (++empty_loop > 5) {
				DRV_LOG(WARNING, "No available dequeue %u, quit.", pending_rules);
				break;
			}
			continue;
		}
		for (i = 0; i < ret; i++) {
			if (comp[i].status == RTE_FLOW_OP_ERROR)
				DRV_LOG(WARNING, "Flow flush get error CQE.");
		}
		/*
		 * Indirect **SYNC** METER_MARK and CT actions do not
		 * remove completion after WQE post.
		 * That implementation avoids HW timeout.
		 * The completion is removed before the following WQE post.
		 * However, HWS queue updates do not reflect that behaviour.
		 * Therefore, during port destruction sync queue may have
		 * pending completions.
		 */
		pending_rules -= RTE_MIN(pending_rules, (uint32_t)ret);
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
	struct mlx5_hw_q *hw_q = &priv->hw_q[MLX5_DEFAULT_FLUSH_QUEUE];
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
		if (__flow_hw_pull_comp(dev, queue, error))
			return -1;
	}
	/* Flush flow per-table from MLX5_DEFAULT_FLUSH_QUEUE. */
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
							error))
					return -1;
				pending_rules = 0;
			}
		}
	}
	/* Drain left completion. */
	if (pending_rules &&
	    __flow_hw_pull_comp(dev, MLX5_DEFAULT_FLUSH_QUEUE, error))
		return -1;
	return 0;
}

static int
mlx5_tbl_multi_pattern_process(struct rte_eth_dev *dev,
			       struct rte_flow_template_table *tbl,
			       struct mlx5_multi_pattern_segment *segment,
			       uint32_t bulk_size,
			       struct rte_flow_error *error)
{
	int ret = 0;
	uint32_t i;
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	struct mlx5_tbl_multi_pattern_ctx *mpctx = &tbl->mpctx;
	const struct rte_flow_template_table_attr *table_attr = &tbl->cfg.attr;
	const struct rte_flow_attr *attr = &table_attr->flow_attr;
	enum mlx5dr_table_type type =
		get_mlx5dr_table_type(attr, table_attr->specialize, unified_fdb);
	uint32_t flags = mlx5_hw_act_flag[!!attr->group][type];
	struct mlx5dr_action *dr_action = NULL;

	for (i = 0; i < MLX5_MULTIPATTERN_ENCAP_NUM; i++) {
		typeof(mpctx->reformat[0]) *reformat = mpctx->reformat + i;
		enum mlx5dr_action_type reformat_type =
			mlx5_multi_pattern_reformat_index_to_type(i);

		if (!reformat->elements_num)
			continue;
		dr_action = reformat_type == MLX5DR_ACTION_TYP_INSERT_HEADER ?
			mlx5dr_action_create_insert_header
			(priv->dr_ctx, reformat->elements_num,
			 reformat->insert_hdr, bulk_size, flags) :
			mlx5dr_action_create_reformat
			(priv->dr_ctx, reformat_type, reformat->elements_num,
			 reformat->reformat_hdr, bulk_size, flags);
		if (!dr_action) {
			ret = rte_flow_error_set(error, rte_errno,
						 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						 NULL,
						 "failed to create multi-pattern encap action");
			goto error;
		}
		segment->reformat_action[i] = dr_action;
	}
	if (mpctx->mh.elements_num) {
		typeof(mpctx->mh) *mh = &mpctx->mh;
		dr_action = mlx5dr_action_create_modify_header
			(priv->dr_ctx, mpctx->mh.elements_num, mh->pattern,
			 bulk_size, flags);
		if (!dr_action) {
			ret = rte_flow_error_set(error, rte_errno,
						  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "failed to create multi-pattern header modify action");
			goto error;
		}
		segment->mhdr_action = dr_action;
	}
	if (dr_action) {
		segment->capacity = RTE_BIT32(bulk_size);
		if (segment != &mpctx->segments[MLX5_MAX_TABLE_RESIZE_NUM - 1])
			segment[1].head_index = segment->head_index + segment->capacity;
	}
	return 0;
error:
	mlx5_destroy_multi_pattern_segment(segment);
	return ret;
}

static int
mlx5_hw_build_template_table(struct rte_eth_dev *dev,
			     uint8_t nb_action_templates,
			     struct rte_flow_actions_template *action_templates[],
			     struct mlx5dr_action_template *at[],
			     struct rte_flow_template_table *tbl,
			     struct rte_flow_error *error)
{
	int ret;
	uint8_t i;

	for (i = 0; i < nb_action_templates; i++) {
		uint32_t refcnt = rte_atomic_fetch_add_explicit(&action_templates[i]->refcnt, 1,
						     rte_memory_order_relaxed) + 1;

		if (refcnt <= 1) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   &action_templates[i], "invalid AT refcount");
			goto at_error;
		}
		at[i] = action_templates[i]->tmpl;
		tbl->ats[i].action_template = action_templates[i];
		LIST_INIT(&tbl->ats[i].acts.act_list);
		/* do NOT translate table action if `dev` was not started */
		if (!dev->data->dev_started)
			continue;
		ret = flow_hw_translate_actions_template(dev, &tbl->cfg,
						  &tbl->ats[i].acts,
						  action_templates[i],
						  &tbl->mpctx, error);
		if (ret) {
			i++;
			goto at_error;
		}
		flow_hw_populate_rule_acts_caches(dev, tbl, i);
	}
	tbl->nb_action_templates = nb_action_templates;
	if (mlx5_is_multi_pattern_active(&tbl->mpctx)) {
		ret = mlx5_tbl_multi_pattern_process(dev, tbl,
						     &tbl->mpctx.segments[0],
						     rte_log2_u32(tbl->cfg.attr.nb_flows),
						     error);
		if (ret)
			goto at_error;
	}
	return 0;

at_error:
	while (i--) {
		__flow_hw_action_template_destroy(dev, &tbl->ats[i].acts);
		rte_atomic_fetch_sub_explicit(&action_templates[i]->refcnt,
				   1, rte_memory_order_relaxed);
	}
	return rte_errno;
}

static bool
flow_hw_validate_template_domain(const struct rte_flow_attr *table_attr,
				 uint32_t ingress, uint32_t egress, uint32_t transfer)
{
	if (table_attr->ingress)
		return ingress != 0;
	else if (table_attr->egress)
		return egress != 0;
	else
		return transfer;
}

static bool
flow_hw_validate_table_domain(const struct rte_flow_attr *table_attr)
{
	return table_attr->ingress + table_attr->egress + table_attr->transfer
		== 1;
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
	bool unified_fdb = is_unified_fdb(priv);
	struct mlx5dr_matcher_attr matcher_attr = {0};
	struct mlx5dr_action_jump_to_matcher_attr jump_attr = {
		.type = MLX5DR_ACTION_JUMP_TO_MATCHER_BY_INDEX,
		.matcher = NULL,
	};
	struct rte_flow_template_table *tbl = NULL;
	struct mlx5_flow_group *grp;
	struct mlx5dr_match_template *mt[MLX5_HW_TBL_MAX_ITEM_TEMPLATE];
	struct mlx5dr_action_template *at[MLX5_HW_TBL_MAX_ACTION_TEMPLATE];
	const struct rte_flow_template_table_attr *attr = &table_cfg->attr;
	struct rte_flow_attr flow_attr = attr->flow_attr;
	uint32_t specialize = table_cfg->attr.specialize;
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = &sub_error,
		.data = &flow_attr,
		.data2 = &specialize,
	};
	struct mlx5_indexed_pool_config cfg = {
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
	bool rpool_needed;
	size_t tbl_mem_size;
	enum mlx5dr_table_type table_type;
	int err;

	if (!flow_hw_validate_table_domain(&attr->flow_attr)) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ATTR,
				   NULL, "invalid table domain attributes");
		return NULL;
	}
	for (i = 0; i < nb_item_templates; i++) {
		const struct rte_flow_pattern_template_attr *pt_attr =
			&item_templates[i]->attr;
		bool match = flow_hw_validate_template_domain(&attr->flow_attr,
							      pt_attr->ingress,
							      pt_attr->egress,
							      pt_attr->transfer);
		if (!match) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "pattern template domain does not match table");
			return NULL;
		}
		if (item_templates[i]->item_flags & MLX5_FLOW_LAYER_ECPRI &&
		    !mlx5_flex_parser_ecpri_exist(dev))
			if (mlx5_flex_parser_ecpri_alloc(dev)) {
				rte_flow_error_set(error, EIO,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						   NULL,
						   "failed to create Flex parser "
						   "profile for ECPRI");
				goto error;
			}
	}
	for (i = 0; i < nb_action_templates; i++) {
		const struct rte_flow_actions_template *at = action_templates[i];
		bool match = flow_hw_validate_template_domain(&attr->flow_attr,
							      at->attr.ingress,
							      at->attr.egress,
							      at->attr.transfer);
		if (!match) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "action template domain does not match table");
			return NULL;
		}
	}
	/* HWS layer accepts only 1 item template with root table. */
	if (!attr->flow_attr.group)
		max_tpl = 1;
	cfg.max_idx = nb_flows;
	cfg.size = !rte_flow_template_table_resizable(dev->data->port_id, attr) ?
		   mlx5_flow_hw_entry_size() :
		   mlx5_flow_hw_auxed_entry_size();
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
	/*
	 * Amount of memory required for rte_flow_template_table struct:
	 * - Size of the struct itself.
	 * - VLA of DR rule action containers at the end =
	 *     number of actions templates * number of queues * size of DR rule actions container.
	 */
	tbl_mem_size = sizeof(*tbl);
	tbl_mem_size += nb_action_templates * priv->nb_queue * sizeof(tbl->rule_acts[0]);
	/* Allocate the table memory. */
	tbl = mlx5_malloc(MLX5_MEM_ZERO, tbl_mem_size, RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!tbl)
		goto error;
	tbl->cfg = *table_cfg;
	/* Allocate flow indexed pool. */
	tbl->flow = mlx5_ipool_create(&cfg);
	if (!tbl->flow)
		goto error;
	/* Allocate table of auxiliary flow rule structs. */
	tbl->flow_aux = mlx5_malloc(MLX5_MEM_ZERO, sizeof(struct rte_flow_hw_aux) * nb_flows,
				    RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (!tbl->flow_aux)
		goto error;
	/* Register the flow group. */
	ge = mlx5_hlist_register(priv->sh->groups, attr->flow_attr.group, &ctx);
	if (!ge)
		goto error;
	grp = container_of(ge, struct mlx5_flow_group, entry);
	/* Verify unified fdb sub domains consistency */
	table_type = get_mlx5dr_table_type(&flow_attr, specialize, unified_fdb);
	if (table_type != grp->type) {
		DRV_LOG(ERR, "Table type (%u) does not match group id (%u) type (%u)",
			table_type, grp->group_id, grp->type);
		rte_errno = EINVAL;
		goto error;
	}
	tbl->grp = grp;
	/* Prepare matcher information. */
	matcher_attr.resizable = !!rte_flow_template_table_resizable
					(dev->data->port_id, &table_cfg->attr);
	matcher_attr.optimize_flow_src = MLX5DR_MATCHER_FLOW_SRC_ANY;
	matcher_attr.priority = attr->flow_attr.priority;
	matcher_attr.optimize_using_rule_idx = true;
	matcher_attr.mode = MLX5DR_MATCHER_RESOURCE_MODE_RULE;
	matcher_attr.insert_mode = flow_hw_matcher_insert_mode_get(attr->insertion_type);
	if (matcher_attr.insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX) {
		if (attr->insertion_type == RTE_FLOW_TABLE_INSERTION_TYPE_INDEX_WITH_PATTERN) {
			matcher_attr.isolated = true;
			matcher_attr.match_mode = MLX5DR_MATCHER_MATCH_MODE_DEFAULT;
		} else {
			matcher_attr.isolated = false;
			matcher_attr.match_mode = MLX5DR_MATCHER_MATCH_MODE_ALWAYS_HIT;
		}
	}
	if (attr->hash_func == RTE_FLOW_TABLE_HASH_FUNC_CRC16) {
		DRV_LOG(ERR, "16-bit checksum hash type is not supported");
		rte_errno = ENOTSUP;
		goto it_error;
	}
	matcher_attr.distribute_mode = flow_hw_matcher_distribute_mode_get(attr->hash_func);
	matcher_attr.rule.num_log = rte_log2_u32(nb_flows);
	/* Parse hints information. */
	if (attr->specialize) {
		uint32_t val = RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG |
			       RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG;

		if ((attr->specialize & val) == val) {
			DRV_LOG(ERR, "Invalid hint value %x",
				attr->specialize);
			rte_errno = EINVAL;
			goto it_error;
		}
		if (attr->specialize &
		    RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_WIRE_ORIG) {
			matcher_attr.optimize_flow_src =
				MLX5DR_MATCHER_FLOW_SRC_WIRE;
		} else if (attr->specialize &
			 RTE_FLOW_TABLE_SPECIALIZE_TRANSFER_VPORT_ORIG) {
			matcher_attr.optimize_flow_src =
				MLX5DR_MATCHER_FLOW_SRC_VPORT;
		}
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
		if (item_templates[i]->item_flags & MLX5_FLOW_ITEM_COMPARE)
			matcher_attr.mode = MLX5DR_MATCHER_RESOURCE_MODE_HTABLE;
		ret = rte_atomic_fetch_add_explicit(&item_templates[i]->refcnt, 1,
					 rte_memory_order_relaxed) + 1;
		if (ret <= 1) {
			rte_errno = EINVAL;
			goto it_error;
		}
		mt[i] = item_templates[i]->mt;
		tbl->its[i] = item_templates[i];
	}
	tbl->nb_item_templates = nb_item_templates;
	/* Build the action template. */
	err = mlx5_hw_build_template_table(dev, nb_action_templates,
					   action_templates, at, tbl, &sub_error);
	if (err) {
		i = nb_item_templates;
		goto it_error;
	}
	tbl->matcher_info[0].matcher = mlx5dr_matcher_create
		(tbl->grp->tbl, mt, nb_item_templates, at, nb_action_templates, &matcher_attr);
	if (!tbl->matcher_info[0].matcher)
		goto at_error;
	tbl->matcher_attr = matcher_attr;
	tbl->type = table_type;
	if (matcher_attr.isolated) {
		jump_attr.matcher = tbl->matcher_info[0].matcher;
		tbl->matcher_info[0].jump = mlx5dr_action_create_jump_to_matcher(priv->dr_ctx,
				&jump_attr, mlx5_hw_act_flag[!!attr->flow_attr.group][tbl->type]);
		if (!tbl->matcher_info[0].jump)
			goto jtm_error;
	}
	/*
	 * Only the matcher supports update and needs more than 1 WQE, an additional
	 * index is needed. Or else the flow index can be reused.
	 */
	rpool_needed = mlx5dr_matcher_is_updatable(tbl->matcher_info[0].matcher) &&
		       mlx5dr_matcher_is_dependent(tbl->matcher_info[0].matcher);
	if (rpool_needed) {
		/* Allocate rule indexed pool. */
		cfg.size = 0;
		cfg.type = "mlx5_hw_table_rule";
		cfg.max_idx += priv->hw_q[0].size;
		tbl->resource = mlx5_ipool_create(&cfg);
		if (!tbl->resource)
			goto res_error;
	}
	if (port_started)
		LIST_INSERT_HEAD(&priv->flow_hw_tbl, tbl, next);
	else
		LIST_INSERT_HEAD(&priv->flow_hw_tbl_ongo, tbl, next);
	rte_rwlock_init(&tbl->matcher_replace_rwlk);
	return tbl;
res_error:
	if (tbl->matcher_info[0].jump)
		mlx5dr_action_destroy(tbl->matcher_info[0].jump);
jtm_error:
	if (tbl->matcher_info[0].matcher)
		(void)mlx5dr_matcher_destroy(tbl->matcher_info[0].matcher);
at_error:
	for (i = 0; i < nb_action_templates; i++) {
		__flow_hw_action_template_destroy(dev, &tbl->ats[i].acts);
		rte_atomic_fetch_sub_explicit(&action_templates[i]->refcnt,
				   1, rte_memory_order_relaxed);
	}
	i = nb_item_templates;
it_error:
	while (i--)
		rte_atomic_fetch_sub_explicit(&item_templates[i]->refcnt,
				   1, rte_memory_order_relaxed);
error:
	err = rte_errno;
	if (tbl) {
		if (tbl->grp)
			mlx5_hlist_unregister(priv->sh->groups,
					      &tbl->grp->entry);
		if (tbl->flow_aux)
			mlx5_free(tbl->flow_aux);
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
		if (flow_hw_translate_all_actions_templates(dev, tbl, error))
			return -1;
		LIST_REMOVE(tbl, next);
		LIST_INSERT_HEAD(&priv->flow_hw_tbl, tbl, next);
	}
	return 0;
}

static inline int
__translate_group(struct rte_eth_dev *dev,
			const struct rte_flow_attr *flow_attr,
			bool external,
			uint32_t group,
			uint32_t *table_group,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_sh_config *config = &priv->sh->config;

	if (config->dv_esw_en &&
	    priv->fdb_def_rule &&
	    external &&
	    flow_attr->transfer) {
		if (group > MLX5_HW_MAX_TRANSFER_GROUP)
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ATTR_GROUP,
						  NULL,
						  "group index not supported");
		*table_group = group + 1;
	} else if (config->dv_esw_en &&
		   (config->repr_matching || config->dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS) &&
		   external &&
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
	const struct rte_flow_attr *flow_attr = &cfg->attr.flow_attr;

	return __translate_group(dev, flow_attr, cfg->external, group, table_group, error);
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
	if (!cfg.attr.flow_attr.group &&
	    rte_flow_template_table_resizable(dev->data->port_id, attr)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "table cannot be resized: invalid group");
		return NULL;
	}
	return flow_hw_table_create(dev, &cfg, item_templates, nb_item_templates,
				    action_templates, nb_action_templates, error);
}

static void
mlx5_destroy_multi_pattern_segment(struct mlx5_multi_pattern_segment *segment)
{
	int i;

	if (segment->mhdr_action)
		mlx5dr_action_destroy(segment->mhdr_action);
	for (i = 0; i < MLX5_MULTIPATTERN_ENCAP_NUM; i++) {
		if (segment->reformat_action[i])
			mlx5dr_action_destroy(segment->reformat_action[i]);
	}
	segment->capacity = 0;
}

static void
flow_hw_destroy_table_multi_pattern_ctx(struct rte_flow_template_table *table)
{
	int sx;

	for (sx = 0; sx < MLX5_MAX_TABLE_RESIZE_NUM; sx++)
		mlx5_destroy_multi_pattern_segment(table->mpctx.segments + sx);
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
	if (table->resource)
		mlx5_ipool_flush_cache(table->resource);
	mlx5_ipool_flush_cache(table->flow);
	/* Check if ipool has allocated objects. */
	if (table->refcnt ||
	    mlx5_ipool_get_next(table->flow, &fidx) ||
	    (table->resource && mlx5_ipool_get_next(table->resource, &ridx))) {
		DRV_LOG(WARNING, "Table %p is still in use.", (void *)table);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "table is in use");
	}
	LIST_REMOVE(table, next);
	for (i = 0; i < table->nb_item_templates; i++)
		rte_atomic_fetch_sub_explicit(&table->its[i]->refcnt,
				   1, rte_memory_order_relaxed);
	for (i = 0; i < table->nb_action_templates; i++) {
		__flow_hw_action_template_destroy(dev, &table->ats[i].acts);
		rte_atomic_fetch_sub_explicit(&table->ats[i].action_template->refcnt,
				   1, rte_memory_order_relaxed);
	}
	flow_hw_destroy_table_multi_pattern_ctx(table);
	if (table->matcher_info[0].jump)
		mlx5dr_action_destroy(table->matcher_info[0].jump);
	if (table->matcher_info[0].matcher)
		mlx5dr_matcher_destroy(table->matcher_info[0].matcher);
	if (table->matcher_info[1].jump)
		mlx5dr_action_destroy(table->matcher_info[1].jump);
	if (table->matcher_info[1].matcher)
		mlx5dr_matcher_destroy(table->matcher_info[1].matcher);
	mlx5_hlist_unregister(priv->sh->groups, &table->grp->entry);
	if (table->resource)
		mlx5_ipool_destroy(table->resource);
	mlx5_free(table->flow_aux);
	mlx5_ipool_destroy(table->flow);
	mlx5_free(table);
	return 0;
}

/**
 * Parse group's miss actions.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] cfg
 *   Pointer to the table_cfg structure.
 * @param[in] actions
 *   Array of actions to perform on group miss. Supported types:
 *   RTE_FLOW_ACTION_TYPE_JUMP, RTE_FLOW_ACTION_TYPE_VOID, RTE_FLOW_ACTION_TYPE_END.
 * @param[out] dst_group_id
 *   Pointer to destination group id output. will be set to 0 if actions is END,
 *   otherwise will be set to destination group id.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

static int
flow_hw_group_parse_miss_actions(struct rte_eth_dev *dev,
				 struct mlx5_flow_template_table_cfg *cfg,
				 const struct rte_flow_action actions[],
				 uint32_t *dst_group_id,
				 struct rte_flow_error *error)
{
	const struct rte_flow_action_jump *jump_conf;
	uint32_t temp = 0;
	uint32_t i;

	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		switch (actions[i].type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			continue;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			if (temp)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, actions,
							  "Miss actions can contain only a single JUMP");

			jump_conf = (const struct rte_flow_action_jump *)actions[i].conf;
			if (!jump_conf)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  jump_conf, "Jump conf must not be NULL");

			if (flow_hw_translate_group(dev, cfg, jump_conf->group, &temp, error))
				return -rte_errno;

			if (!temp)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
							  "Failed to set group miss actions - Invalid target group");
			break;
		default:
			return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
						  &actions[i], "Unsupported default miss action type");
		}
	}

	*dst_group_id = temp;
	return 0;
}

/**
 * Set group's miss group.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] cfg
 *   Pointer to the table_cfg structure.
 * @param[in] src_grp
 *   Pointer to source group structure.
 *   if NULL, a new group will be created based on group id from cfg->attr.flow_attr.group.
 * @param[in] dst_grp
 *   Pointer to destination group structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

static int
flow_hw_group_set_miss_group(struct rte_eth_dev *dev,
			     struct mlx5_flow_template_table_cfg *cfg,
			     struct mlx5_flow_group *src_grp,
			     struct mlx5_flow_group *dst_grp,
			     struct rte_flow_error *error)
{
	struct rte_flow_error sub_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = &sub_error,
		.data = &cfg->attr.flow_attr,
		.data2 = &cfg->attr.specialize,
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_list_entry *ge;
	bool ref = false;
	int ret;

	if (!dst_grp)
		return -EINVAL;

	/* If group doesn't exist - needs to be created. */
	if (!src_grp) {
		ge = mlx5_hlist_register(priv->sh->groups, cfg->attr.flow_attr.group, &ctx);
		if (!ge)
			return -rte_errno;

		src_grp = container_of(ge, struct mlx5_flow_group, entry);
		LIST_INSERT_HEAD(&priv->flow_hw_grp, src_grp, next);
		ref = true;
	} else if (!src_grp->miss_group) {
		/* If group exists, but has no miss actions - need to increase ref_cnt. */
		LIST_INSERT_HEAD(&priv->flow_hw_grp, src_grp, next);
		src_grp->entry.ref_cnt++;
		ref = true;
	}

	ret = mlx5dr_table_set_default_miss(src_grp->tbl, dst_grp->tbl);
	if (ret)
		goto mlx5dr_error;

	/* If group existed and had old miss actions - ref_cnt is already correct.
	 * However, need to reduce ref counter for old miss group.
	 */
	if (src_grp->miss_group)
		mlx5_hlist_unregister(priv->sh->groups, &src_grp->miss_group->entry);

	src_grp->miss_group = dst_grp;
	return 0;

mlx5dr_error:
	/* Reduce src_grp ref_cnt back & remove from grp list in case of mlx5dr error */
	if (ref) {
		mlx5_hlist_unregister(priv->sh->groups, &src_grp->entry);
		LIST_REMOVE(src_grp, next);
	}

	return rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "Failed to set group miss actions");
}

/**
 * Unset group's miss group.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] grp
 *   Pointer to group structure.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

static int
flow_hw_group_unset_miss_group(struct rte_eth_dev *dev,
			       struct mlx5_flow_group *grp,
			       struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;

	/* If group doesn't exist - no need to change anything. */
	if (!grp)
		return 0;

	/* If group exists, but miss actions is already default behavior -
	 * no need to change anything.
	 */
	if (!grp->miss_group)
		return 0;

	ret = mlx5dr_table_set_default_miss(grp->tbl, NULL);
	if (ret)
		return rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Failed to unset group miss actions");

	mlx5_hlist_unregister(priv->sh->groups, &grp->miss_group->entry);
	grp->miss_group = NULL;

	LIST_REMOVE(grp, next);
	mlx5_hlist_unregister(priv->sh->groups, &grp->entry);

	return 0;
}

/**
 * Set group miss actions.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] group_id
 *   Group id.
 * @param[in] attr
 *   Pointer to group attributes structure.
 * @param[in] actions
 *   Array of actions to perform on group miss. Supported types:
 *   RTE_FLOW_ACTION_TYPE_JUMP, RTE_FLOW_ACTION_TYPE_VOID, RTE_FLOW_ACTION_TYPE_END.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */

int
mlx5_flow_hw_group_set_miss_actions(struct rte_eth_dev *dev,
			       uint32_t group_id,
			       const struct rte_flow_group_attr *attr,
			       const struct rte_flow_action actions[],
			       struct rte_flow_error *error)
{
	struct rte_flow_error sub_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.external = true,
		.attr = {
			.flow_attr = {
				.group = group_id,
				.ingress = attr->ingress,
				.egress = attr->egress,
				.transfer = attr->transfer,
			},
		},
	};
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = &sub_error,
		.data = &cfg.attr.flow_attr,
		.data2 = &cfg.attr.specialize,
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_group *src_grp = NULL;
	struct mlx5_flow_group *dst_grp = NULL;
	struct mlx5_list_entry *ge;
	uint32_t dst_group_id = 0;
	int ret;

	if (flow_hw_translate_group(dev, &cfg, group_id, &group_id, error))
		return -rte_errno;

	if (!group_id)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Failed to set group miss actions - invalid group id");

	ret = flow_hw_group_parse_miss_actions(dev, &cfg, actions, &dst_group_id, error);
	if (ret)
		return -rte_errno;

	if (dst_group_id == group_id) {
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Failed to set group miss actions - target group id must differ from group_id");
	}

	cfg.attr.flow_attr.group = group_id;
	ge = mlx5_hlist_lookup(priv->sh->groups, group_id, &ctx);
	if (ge)
		src_grp = container_of(ge, struct mlx5_flow_group, entry);

	if (dst_group_id) {
		/* Increase ref_cnt for new miss group. */
		cfg.attr.flow_attr.group = dst_group_id;
		ge = mlx5_hlist_register(priv->sh->groups, dst_group_id, &ctx);
		if (!ge)
			return -rte_errno;

		dst_grp = container_of(ge, struct mlx5_flow_group, entry);

		cfg.attr.flow_attr.group = group_id;
		ret = flow_hw_group_set_miss_group(dev, &cfg, src_grp, dst_grp, error);
		if (ret)
			goto error;
	} else {
		return flow_hw_group_unset_miss_group(dev, src_grp, error);
	}

	return 0;

error:
	if (dst_grp)
		mlx5_hlist_unregister(priv->sh->groups, &dst_grp->entry);
	return -rte_errno;
}

static bool
flow_hw_modify_field_is_used(const struct rte_flow_action_modify_field *action,
			     enum rte_flow_field_id field)
{
	return action->src.field == field || action->dst.field == field;
}

static bool
flow_hw_modify_field_is_geneve_opt(enum rte_flow_field_id field)
{
	return field == RTE_FLOW_FIELD_GENEVE_OPT_TYPE ||
	       field == RTE_FLOW_FIELD_GENEVE_OPT_CLASS ||
	       field == RTE_FLOW_FIELD_GENEVE_OPT_DATA;
}

static bool
flow_hw_modify_field_is_add_dst_valid(const struct rte_flow_action_modify_field *conf)
{
	if (conf->operation != RTE_FLOW_MODIFY_ADD)
		return true;
	if (conf->src.field == RTE_FLOW_FIELD_POINTER ||
	    conf->src.field == RTE_FLOW_FIELD_VALUE)
		return true;
	switch (conf->dst.field) {
	case RTE_FLOW_FIELD_IPV4_TTL:
	case RTE_FLOW_FIELD_IPV6_HOPLIMIT:
	case RTE_FLOW_FIELD_TCP_SEQ_NUM:
	case RTE_FLOW_FIELD_TCP_ACK_NUM:
	case RTE_FLOW_FIELD_TAG:
	case RTE_FLOW_FIELD_META:
	case RTE_FLOW_FIELD_FLEX_ITEM:
	case RTE_FLOW_FIELD_TCP_DATA_OFFSET:
	case RTE_FLOW_FIELD_IPV4_IHL:
	case RTE_FLOW_FIELD_IPV4_TOTAL_LEN:
	case RTE_FLOW_FIELD_IPV6_PAYLOAD_LEN:
		return true;
	default:
		break;
	}
	return false;
}

/**
 * Validate the level value for modify field action.
 *
 * @param[in] data
 *   Pointer to the rte_flow_field_data structure either src or dst.
 * @param[in] inner_supported
 *   Indicator whether inner should be supported.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_modify_field_level(const struct rte_flow_field_data *data,
				    bool inner_supported,
				    struct rte_flow_error *error)
{
	switch ((int)data->field) {
	case RTE_FLOW_FIELD_START:
	case RTE_FLOW_FIELD_VLAN_TYPE:
	case RTE_FLOW_FIELD_RANDOM:
	case RTE_FLOW_FIELD_FLEX_ITEM:
		/*
		 * Level shouldn't be valid since field isn't supported or
		 * doesn't use 'level'.
		 */
		break;
	case RTE_FLOW_FIELD_MARK:
	case RTE_FLOW_FIELD_META:
	case RTE_FLOW_FIELD_METER_COLOR:
	case RTE_FLOW_FIELD_HASH_RESULT:
		/* For meta data fields encapsulation level is don't-care. */
		break;
	case RTE_FLOW_FIELD_TAG:
	case MLX5_RTE_FLOW_FIELD_META_REG:
		/*
		 * The tag array for RTE_FLOW_FIELD_TAG type is provided using
		 * 'tag_index' field. In old API, it was provided using 'level'
		 * field and it is still supported for backwards compatibility.
		 * Therefore, for meta tag field only, level is matter. It is
		 * taken as tag index when 'tag_index' field isn't set, and
		 * return error otherwise.
		 */
		if (data->level > 0) {
			if (data->tag_index > 0)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  data,
							  "tag array can be provided using 'level' or 'tag_index' fields, not both");
			DRV_LOG(WARNING,
				"tag array provided in 'level' field instead of 'tag_index' field.");
		}
		break;
	case RTE_FLOW_FIELD_MAC_DST:
	case RTE_FLOW_FIELD_MAC_SRC:
	case RTE_FLOW_FIELD_MAC_TYPE:
	case RTE_FLOW_FIELD_IPV4_IHL:
	case RTE_FLOW_FIELD_IPV4_TOTAL_LEN:
	case RTE_FLOW_FIELD_IPV4_DSCP:
	case RTE_FLOW_FIELD_IPV4_ECN:
	case RTE_FLOW_FIELD_IPV4_TTL:
	case RTE_FLOW_FIELD_IPV4_SRC:
	case RTE_FLOW_FIELD_IPV4_DST:
	case RTE_FLOW_FIELD_IPV6_TRAFFIC_CLASS:
	case RTE_FLOW_FIELD_IPV6_FLOW_LABEL:
	case RTE_FLOW_FIELD_IPV6_PAYLOAD_LEN:
	case RTE_FLOW_FIELD_IPV6_HOPLIMIT:
	case RTE_FLOW_FIELD_IPV6_SRC:
	case RTE_FLOW_FIELD_IPV6_DST:
	case RTE_FLOW_FIELD_TCP_PORT_SRC:
	case RTE_FLOW_FIELD_TCP_PORT_DST:
	case RTE_FLOW_FIELD_TCP_FLAGS:
	case RTE_FLOW_FIELD_TCP_DATA_OFFSET:
	case RTE_FLOW_FIELD_UDP_PORT_SRC:
	case RTE_FLOW_FIELD_UDP_PORT_DST:
		if (data->level > 2)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  data,
						  "second inner header fields modification is not supported");
		if (inner_supported)
			break;
		/* Fallthrough */
	case RTE_FLOW_FIELD_VLAN_ID:
	case RTE_FLOW_FIELD_IPV4_PROTO:
	case RTE_FLOW_FIELD_IPV6_PROTO:
	case RTE_FLOW_FIELD_IPV6_DSCP:
	case RTE_FLOW_FIELD_IPV6_ECN:
	case RTE_FLOW_FIELD_TCP_SEQ_NUM:
	case RTE_FLOW_FIELD_TCP_ACK_NUM:
	case RTE_FLOW_FIELD_ESP_PROTO:
	case RTE_FLOW_FIELD_ESP_SPI:
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
	case RTE_FLOW_FIELD_VXLAN_VNI:
	case RTE_FLOW_FIELD_VXLAN_LAST_RSVD:
	case RTE_FLOW_FIELD_GENEVE_VNI:
	case RTE_FLOW_FIELD_GENEVE_OPT_TYPE:
	case RTE_FLOW_FIELD_GENEVE_OPT_CLASS:
	case RTE_FLOW_FIELD_GENEVE_OPT_DATA:
	case RTE_FLOW_FIELD_GTP_TEID:
	case RTE_FLOW_FIELD_GTP_PSC_QFI:
		if (data->level > 1)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  data,
						  "inner header fields modification is not supported");
		break;
	case RTE_FLOW_FIELD_MPLS:
		if (data->level == 1)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  data,
						  "outer MPLS header modification is not supported");
		if (data->level > 2)
			return rte_flow_error_set(error, ENOTSUP,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  data,
						  "inner MPLS header modification is not supported");
		break;
	case RTE_FLOW_FIELD_POINTER:
	case RTE_FLOW_FIELD_VALUE:
	default:
		MLX5_ASSERT(false);
	}
	return 0;
}

static int
flow_hw_validate_action_modify_field(struct rte_eth_dev *dev,
				     const struct rte_flow_action *action,
				     const struct rte_flow_action *mask,
				     struct rte_flow_error *error)
{
	const struct rte_flow_action_modify_field *action_conf = action->conf;
	const struct rte_flow_action_modify_field *mask_conf = mask->conf;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hca_attr *attr = &priv->sh->cdev->config.hca_attr;
	int ret;

	if (!mask_conf)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "modify_field mask conf is missing");
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
	ret = flow_hw_validate_modify_field_level(&action_conf->dst, false, error);
	if (ret)
		return ret;
	if (action_conf->dst.field != RTE_FLOW_FIELD_FLEX_ITEM &&
	    !flow_hw_modify_field_is_geneve_opt(action_conf->dst.field)) {
		if (action_conf->dst.tag_index &&
		    !flow_modify_field_support_tag_array(action_conf->dst.field))
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"destination tag index is not supported");
		if (action_conf->dst.class_id)
			return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"destination class id is not supported");
	}
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
		if (action_conf->src.field != RTE_FLOW_FIELD_FLEX_ITEM &&
		    !flow_hw_modify_field_is_geneve_opt(action_conf->src.field)) {
			if (action_conf->src.tag_index &&
			    !flow_modify_field_support_tag_array(action_conf->src.field))
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"source tag index is not supported");
			if (action_conf->src.class_id)
				return rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, action,
					"source class id is not supported");
		}
		if (mask_conf->src.level != UINT8_MAX)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source encapsulation level must be fully masked");
		if (mask_conf->src.offset != UINT32_MAX)
			return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"source offset level must be fully masked");
		ret = flow_hw_validate_modify_field_level(&action_conf->src, true, error);
		if (ret)
			return ret;
	}
	if ((action_conf->dst.field == RTE_FLOW_FIELD_TAG &&
	     action_conf->dst.tag_index >= MLX5_FLOW_HW_TAGS_MAX &&
	     action_conf->dst.tag_index != RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX) ||
	    (action_conf->src.field == RTE_FLOW_FIELD_TAG &&
	     action_conf->src.tag_index >= MLX5_FLOW_HW_TAGS_MAX &&
	     action_conf->src.tag_index != RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
				 "tag index is out of range");
	if ((action_conf->dst.field == RTE_FLOW_FIELD_TAG &&
	     flow_hw_get_reg_id(dev, RTE_FLOW_ITEM_TYPE_TAG, action_conf->dst.tag_index) == REG_NON) ||
	    (action_conf->src.field == RTE_FLOW_FIELD_TAG &&
	     flow_hw_get_reg_id(dev, RTE_FLOW_ITEM_TYPE_TAG, action_conf->src.tag_index) == REG_NON))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "tag index is out of range");
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
	if (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_RANDOM))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying random value is not supported");
	/**
	 * Geneve VNI modification is supported only when Geneve header is
	 * parsed natively. When GENEVE options are supported, they both Geneve
	 * and options headers are parsed as a flex parser.
	 */
	if (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_GENEVE_VNI) &&
	    attr->geneve_tlv_opt)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying Geneve VNI is not supported when GENEVE opt is supported");
	if (priv->tlv_options == NULL &&
	    (flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_GENEVE_OPT_TYPE) ||
	     flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_GENEVE_OPT_CLASS) ||
	     flow_hw_modify_field_is_used(action_conf, RTE_FLOW_FIELD_GENEVE_OPT_DATA)))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"modifying Geneve TLV option is supported only after parser configuration");
	/* Due to HW bug, tunnel MPLS header is read only. */
	if (action_conf->dst.field == RTE_FLOW_FIELD_MPLS)
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"MPLS cannot be used as destination");
	/* ADD_FIELD is not supported for all the fields. */
	if (!flow_hw_modify_field_is_add_dst_valid(action_conf))
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ACTION, action,
				"invalid add_field destination");
	return 0;
}

static int
flow_hw_validate_action_port_representor(struct rte_eth_dev *dev __rte_unused,
					 const struct rte_flow_actions_template_attr *attr,
					 const struct rte_flow_action *action,
					 const struct rte_flow_action *mask,
					 struct rte_flow_error *error)
{
	const struct rte_flow_action_ethdev *action_conf = NULL;
	const struct rte_flow_action_ethdev *mask_conf = NULL;

	/* If transfer is set, port has been validated as proxy port. */
	if (!attr->transfer)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "cannot use port_representor actions"
					  " without an E-Switch");
	if (!action || !mask)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "actiona and mask configuration must be set");
	action_conf = action->conf;
	mask_conf = mask->conf;
	if (!mask_conf || mask_conf->port_id != MLX5_REPRESENTED_PORT_ESW_MGR ||
	    !action_conf || action_conf->port_id != MLX5_REPRESENTED_PORT_ESW_MGR)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "only eswitch manager port 0xffff is"
					  " supported");
	return 0;
}

static int
flow_hw_validate_target_port_id(struct rte_eth_dev *dev,
				uint16_t target_port_id)
{
	struct mlx5_priv *port_priv;
	struct mlx5_priv *dev_priv;

	if (target_port_id == MLX5_REPRESENTED_PORT_ESW_MGR)
		return 0;

	port_priv = mlx5_port_to_eswitch_info(target_port_id, false);
	if (!port_priv) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "Port %u Failed to obtain E-Switch info for port %u",
			dev->data->port_id, target_port_id);
		return -rte_errno;
	}

	dev_priv = mlx5_dev_to_eswitch_info(dev);
	if (!dev_priv) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "Port %u Failed to obtain E-Switch info for transfer proxy",
			dev->data->port_id);
		return -rte_errno;
	}

	if (port_priv->domain_id != dev_priv->domain_id) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "Port %u Failed to obtain E-Switch info for transfer proxy",
			dev->data->port_id);
		return -rte_errno;
	}

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
		if (!action_conf)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						  action, "port index was not provided");

		if (flow_hw_validate_target_port_id(dev, action_conf->port_id))
			return rte_flow_error_set(error, rte_errno, RTE_FLOW_ERROR_TYPE_ACTION,
						  action, "port index is invalid");
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
 * @param[in] indirect
 *   If true, then provided action was passed using an indirect action.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_validate_action_meter_mark(struct rte_eth_dev *dev,
			      const struct rte_flow_action *action,
			      bool indirect,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	RTE_SET_USED(action);

	if (!priv->sh->cdev->config.devx)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "meter_mark action not supported");
	if (!indirect && priv->shared_host)
		return rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "meter_mark action can only be used on host port");
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
		ret = flow_hw_validate_action_meter_mark(dev, mask, true, error);
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
 * Validate ipv6_ext_push action.
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
flow_hw_validate_action_ipv6_ext_push(struct rte_eth_dev *dev __rte_unused,
				      const struct rte_flow_action *action,
				      struct rte_flow_error *error)
{
	const struct rte_flow_action_ipv6_ext_push *raw_push_data = action->conf;

	if (!raw_push_data || !raw_push_data->size || !raw_push_data->data)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "invalid ipv6_ext_push data");
	if (raw_push_data->type != IPPROTO_ROUTING ||
	    raw_push_data->size > MLX5_PUSH_MAX_LEN)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Unsupported ipv6_ext_push type or length");
	return 0;
}

/**
 * Process `... / raw_decap / raw_encap / ...` actions sequence.
 * The PMD handles the sequence as a single encap or decap reformat action,
 * depending on the raw_encap configuration.
 *
 * The function assumes that the raw_decap / raw_encap location
 * in actions template list complies with relative HWS actions order:
 * for the required reformat configuration:
 * ENCAP configuration must appear before [JUMP|DROP|PORT]
 * DECAP configuration must appear at the template head.
 */
static uint64_t
mlx5_decap_encap_reformat_type(const struct rte_flow_action *actions,
			       uint32_t encap_ind, uint64_t flags)
{
	const struct rte_flow_action_raw_encap *encap = actions[encap_ind].conf;

	if ((flags & MLX5_FLOW_ACTION_DECAP) == 0)
		return MLX5_FLOW_ACTION_ENCAP;
	if (actions[encap_ind - 1].type != RTE_FLOW_ACTION_TYPE_RAW_DECAP)
		return MLX5_FLOW_ACTION_ENCAP;
	return encap->size >= MLX5_ENCAPSULATION_DECISION_SIZE ?
	       MLX5_FLOW_ACTION_ENCAP : MLX5_FLOW_ACTION_DECAP;
}

enum mlx5_hw_indirect_list_relative_position {
	MLX5_INDIRECT_LIST_POSITION_UNKNOWN = -1,
	MLX5_INDIRECT_LIST_POSITION_BEFORE_MH = 0,
	MLX5_INDIRECT_LIST_POSITION_AFTER_MH,
};

static enum mlx5_hw_indirect_list_relative_position
mlx5_hw_indirect_list_mh_position(const struct rte_flow_action *action)
{
	const struct rte_flow_action_indirect_list *conf = action->conf;
	enum mlx5_indirect_list_type list_type = mlx5_get_indirect_list_type(conf->handle);
	enum mlx5_hw_indirect_list_relative_position pos = MLX5_INDIRECT_LIST_POSITION_UNKNOWN;
	const union {
		struct mlx5_indlst_legacy *legacy;
		struct mlx5_hw_encap_decap_action *reformat;
		struct rte_flow_action_list_handle *handle;
	} h = { .handle = conf->handle};

	switch (list_type) {
	case  MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY:
		switch (h.legacy->legacy_type) {
		case RTE_FLOW_ACTION_TYPE_AGE:
		case RTE_FLOW_ACTION_TYPE_COUNT:
		case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
		case RTE_FLOW_ACTION_TYPE_QUOTA:
			pos = MLX5_INDIRECT_LIST_POSITION_BEFORE_MH;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			pos = MLX5_INDIRECT_LIST_POSITION_AFTER_MH;
			break;
		default:
			pos = MLX5_INDIRECT_LIST_POSITION_UNKNOWN;
			break;
		}
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
		pos = MLX5_INDIRECT_LIST_POSITION_AFTER_MH;
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
		switch (h.reformat->action_type) {
		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2:
		case MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2:
			pos = MLX5_INDIRECT_LIST_POSITION_BEFORE_MH;
			break;
		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2:
		case MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3:
			pos = MLX5_INDIRECT_LIST_POSITION_AFTER_MH;
			break;
		default:
			pos = MLX5_INDIRECT_LIST_POSITION_UNKNOWN;
			break;
		}
		break;
	default:
		pos = MLX5_INDIRECT_LIST_POSITION_UNKNOWN;
		break;
	}
	return pos;
}

#define MLX5_HW_EXPAND_MH_FAILED 0xffff

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
		enum mlx5_hw_indirect_list_relative_position pos;
		enum rte_flow_action_type type = actions[i].type;
		uint64_t reformat_type;

		if (type == RTE_FLOW_ACTION_TYPE_INDIRECT)
			type = masks[i].type;
		switch (type) {
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_DROP:
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL:
		case RTE_FLOW_ACTION_TYPE_JUMP:
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
		case RTE_FLOW_ACTION_TYPE_VOID:
		case RTE_FLOW_ACTION_TYPE_END:
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			reformat_type =
				mlx5_decap_encap_reformat_type(actions, i,
							       flags);
			if (reformat_type == MLX5_FLOW_ACTION_DECAP) {
				i++;
				goto insert;
			}
			if (actions[i - 1].type == RTE_FLOW_ACTION_TYPE_RAW_DECAP)
				i--;
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT_LIST:
			pos = mlx5_hw_indirect_list_mh_position(&actions[i]);
			if (pos == MLX5_INDIRECT_LIST_POSITION_UNKNOWN)
				return MLX5_HW_EXPAND_MH_FAILED;
			if (pos == MLX5_INDIRECT_LIST_POSITION_BEFORE_MH)
				goto insert;
			break;
		default:
			i++; /* new MF inserted AFTER actions[i] */
			goto insert;
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
flow_hw_validate_action_default_miss(struct rte_eth_dev *dev,
				     const struct rte_flow_actions_template_attr *attr,
				     uint64_t action_flags,
				     struct rte_flow_error *error)
{
	/*
	 * The private DEFAULT_MISS action is used internally for LACP in control
	 * flows. So this validation can be ignored. It can be kept right now since
	 * the validation will be done only once.
	 */
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!attr->ingress || attr->egress || attr->transfer)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "DEFAULT MISS is only supported in ingress.");
	if (!priv->hw_def_miss)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "DEFAULT MISS action does not exist.");
	if (action_flags & MLX5_FLOW_FATE_ACTIONS)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					  "DEFAULT MISS should be the only termination.");
	return 0;
}

static int
flow_hw_validate_action_nat64(struct rte_eth_dev *dev,
			      const struct rte_flow_actions_template_attr *attr,
			      const struct rte_flow_action *action,
			      const struct rte_flow_action *mask,
			      uint64_t action_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_nat64 *nat64_c;
	enum rte_flow_nat64_type cov_type;

	RTE_SET_USED(action_flags);
	if (mask->conf && ((const struct rte_flow_action_nat64 *)mask->conf)->type) {
		nat64_c = (const struct rte_flow_action_nat64 *)action->conf;
		cov_type = nat64_c->type;
		if ((attr->ingress && !priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_RX][cov_type]) ||
		    (attr->egress && !priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_TX][cov_type]))
			goto err_out;
		if (attr->transfer) {
			if (!is_unified_fdb(priv)) {
				if (!priv->action_nat64[MLX5DR_TABLE_TYPE_FDB][cov_type])
					goto err_out;
			} else {
				if (!priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_RX][cov_type] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_TX][cov_type] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_UNIFIED][cov_type])
					goto err_out;
			}
		}
	} else {
		/*
		 * Usually, the actions will be used on both directions. For non-masked actions,
		 * both directions' actions will be checked.
		 */
		if (attr->ingress)
			if (!priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_RX][RTE_FLOW_NAT64_6TO4] ||
			    !priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_RX][RTE_FLOW_NAT64_4TO6])
				goto err_out;
		if (attr->egress)
			if (!priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_TX][RTE_FLOW_NAT64_6TO4] ||
			    !priv->action_nat64[MLX5DR_TABLE_TYPE_NIC_TX][RTE_FLOW_NAT64_4TO6])
				goto err_out;
		if (attr->transfer) {
			if (!is_unified_fdb(priv)) {
				if (!priv->action_nat64[MLX5DR_TABLE_TYPE_FDB]
						       [RTE_FLOW_NAT64_6TO4] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB]
						       [RTE_FLOW_NAT64_4TO6])
					goto err_out;
			} else {
				if (!priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_RX]
						       [RTE_FLOW_NAT64_6TO4] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_RX]
						       [RTE_FLOW_NAT64_4TO6] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_TX]
						       [RTE_FLOW_NAT64_6TO4] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_TX]
						       [RTE_FLOW_NAT64_4TO6] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_UNIFIED]
						       [RTE_FLOW_NAT64_6TO4] ||
				    !priv->action_nat64[MLX5DR_TABLE_TYPE_FDB_UNIFIED]
						       [RTE_FLOW_NAT64_4TO6])
					goto err_out;
			}
		}
	}
	return 0;
err_out:
	return rte_flow_error_set(error, EOPNOTSUPP, RTE_FLOW_ERROR_TYPE_ACTION,
				  NULL, "NAT64 action is not supported.");
}

static int
flow_hw_validate_action_jump(struct rte_eth_dev *dev,
			     const struct rte_flow_actions_template_attr *attr,
			     const struct rte_flow_action *action,
			     const struct rte_flow_action *mask,
			     struct rte_flow_error *error)
{
	const struct rte_flow_action_jump *m = mask->conf;
	const struct rte_flow_action_jump *v = action->conf;
	struct mlx5_flow_template_table_cfg cfg = {
		.external = true,
		.attr = {
			.flow_attr = {
				.ingress = attr->ingress,
				.egress = attr->egress,
				.transfer = attr->transfer,
			},
		},
	};
	uint32_t t_group = 0;

	if (!m || !m->group)
		return 0;
	if (!v)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Invalid jump action configuration");
	if (flow_hw_translate_group(dev, &cfg, v->group, &t_group, error))
		return -rte_errno;
	if (t_group == 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Unsupported action - jump to root table");
	return 0;
}

static int
mlx5_flow_validate_action_jump_to_table_index(const struct rte_flow_action *action,
			     const struct rte_flow_action *mask,
			     struct rte_flow_error *error)
{
	const struct rte_flow_action_jump_to_table_index *m = mask->conf;
	const struct rte_flow_action_jump_to_table_index *v = action->conf;
	struct mlx5dr_action *jump_action;
	uint32_t t_group = 0;

	if (!m || !m->table)
		return 0;
	if (!v)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Invalid jump to matcher action configuration");
	t_group = v->table->grp->group_id;
	if (t_group == 0)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Unsupported action - jump to root table");
	if (likely(!rte_flow_template_table_resizable(0, &v->table->cfg.attr))) {
		jump_action = v->table->matcher_info[0].jump;
	} else {
		uint32_t selector;
		rte_rwlock_read_lock(&v->table->matcher_replace_rwlk);
		selector = v->table->matcher_selector;
		jump_action = v->table->matcher_info[selector].jump;
		rte_rwlock_read_unlock(&v->table->matcher_replace_rwlk);
	}
	if (jump_action == NULL)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "Unsupported action - table is not an rule array");
	return 0;
}

static int
mlx5_hw_validate_action_mark(struct rte_eth_dev *dev,
			     const struct rte_flow_action *template_action,
			     const struct rte_flow_action *template_mask,
			     uint64_t action_flags,
			     const struct rte_flow_actions_template_attr *template_attr,
			     struct rte_flow_error *error)
{
	const struct rte_flow_action_mark *mark_mask = template_mask->conf;
	const struct rte_flow_action *action =
		mark_mask && mark_mask->id ? template_action :
		&(const struct rte_flow_action) {
		.type = RTE_FLOW_ACTION_TYPE_MARK,
		.conf = &(const struct rte_flow_action_mark) {
			.id = MLX5_FLOW_MARK_MAX - 1
		}
	};
	const struct rte_flow_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer
	};

	return mlx5_flow_validate_action_mark(dev, action, action_flags,
					      &attr, error);
}

static int
mlx5_hw_validate_action_queue(struct rte_eth_dev *dev,
			      const struct rte_flow_action *template_action,
			      const struct rte_flow_action *template_mask,
			      const struct rte_flow_actions_template_attr *template_attr,
			      uint64_t action_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_queue *queue_mask = template_mask->conf;
	const struct rte_flow_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer
	};
	bool masked = queue_mask != NULL && queue_mask->index;

	if (template_attr->egress || (template_attr->transfer && !priv->jump_fdb_rx_en))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "QUEUE action supported for ingress only");
	if (masked)
		return mlx5_flow_validate_action_queue(template_action, action_flags, dev,
						       &attr, error);
	else
		return 0;
}

static int
mlx5_hw_validate_action_rss(struct rte_eth_dev *dev,
			      const struct rte_flow_action *template_action,
			      const struct rte_flow_action *template_mask,
			      const struct rte_flow_actions_template_attr *template_attr,
			      __rte_unused uint64_t action_flags,
			      struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_rss *mask = template_mask->conf;

	if (template_attr->egress || (template_attr->transfer && !priv->jump_fdb_rx_en))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ATTR, NULL,
					  "RSS action supported for ingress only");
	if (mask != NULL)
		return mlx5_validate_action_rss(dev, template_action, error);
	else
		return 0;
}

static int
mlx5_hw_validate_action_l2_encap(struct rte_eth_dev *dev,
				 const struct rte_flow_action *template_action,
				 const struct rte_flow_action *template_mask,
				 const struct rte_flow_actions_template_attr *template_attr,
				 uint64_t action_flags,
				 struct rte_flow_error *error)
{
	const struct rte_flow_action_vxlan_encap default_action_conf = {
		.definition = (struct rte_flow_item *)
			(struct rte_flow_item [1]) {
			[0] = { .type = RTE_FLOW_ITEM_TYPE_END }
		}
	};
	const struct rte_flow_action *action = template_mask->conf ?
		template_action : &(const struct rte_flow_action) {
			.type = template_mask->type,
			.conf = &default_action_conf
	};
	const struct rte_flow_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer
	};

	return mlx5_flow_dv_validate_action_l2_encap(dev, action_flags, action,
						     &attr, error);
}

static int
mlx5_hw_validate_action_l2_decap(struct rte_eth_dev *dev,
				 const struct rte_flow_action *template_action,
				 const struct rte_flow_action *template_mask,
				 const struct rte_flow_actions_template_attr *template_attr,
				 uint64_t action_flags,
				 struct rte_flow_error *error)
{
	const struct rte_flow_action_vxlan_encap default_action_conf = {
		.definition = (struct rte_flow_item *)
			(struct rte_flow_item [1]) {
				[0] = { .type = RTE_FLOW_ITEM_TYPE_END }
			}
	};
	const struct rte_flow_action *action = template_mask->conf ?
					       template_action : &(const struct rte_flow_action) {
			.type = template_mask->type,
			.conf = &default_action_conf
		};
	const struct rte_flow_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer
	};
	uint64_t item_flags =
		action->type == RTE_FLOW_ACTION_TYPE_VXLAN_DECAP ?
		MLX5_FLOW_LAYER_VXLAN : 0;

	return mlx5_flow_dv_validate_action_decap(dev, action_flags, action,
						  item_flags, &attr, error);
}

static int
mlx5_hw_validate_action_conntrack(struct rte_eth_dev *dev,
				  const struct rte_flow_action *template_action,
				  const struct rte_flow_action *template_mask,
				  const struct rte_flow_actions_template_attr *template_attr,
				  uint64_t action_flags,
				  struct rte_flow_error *error)
{
	RTE_SET_USED(template_action);
	RTE_SET_USED(template_mask);
	RTE_SET_USED(template_attr);
	return mlx5_flow_dv_validate_action_aso_ct(dev, action_flags,
						   MLX5_FLOW_LAYER_OUTER_L4_TCP,
						   false, error);
}

static int
flow_hw_validate_action_raw_encap(const struct rte_flow_action *action,
				  const struct rte_flow_action *mask,
				  struct rte_flow_error *error)
{
	const struct rte_flow_action_raw_encap *mask_conf = mask->conf;
	const struct rte_flow_action_raw_encap *action_conf = action->conf;

	if (!mask_conf || !mask_conf->size)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, mask,
					  "raw_encap: size must be masked");
	if (!action_conf || !action_conf->size)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION, action,
					  "raw_encap: invalid action configuration");
	if (mask_conf->data && !action_conf->data)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "raw_encap: masked data is missing");
	return 0;
}


static int
flow_hw_validate_action_raw_reformat(struct rte_eth_dev *dev,
				     const struct rte_flow_action *template_action,
				     const struct rte_flow_action *template_mask,
				     const struct
				     rte_flow_actions_template_attr *template_attr,
				     uint64_t *action_flags,
				     struct rte_flow_error *error)
{
	const struct rte_flow_action *encap_action = NULL;
	const struct rte_flow_action *encap_mask = NULL;
	const struct rte_flow_action_raw_decap *raw_decap = NULL;
	const struct rte_flow_action_raw_encap *raw_encap = NULL;
	const struct rte_flow_attr attr = {
		.ingress = template_attr->ingress,
		.egress = template_attr->egress,
		.transfer = template_attr->transfer
	};
	uint64_t item_flags = 0;
	int ret, actions_n = 0;

	if (template_action->type == RTE_FLOW_ACTION_TYPE_RAW_DECAP) {
		raw_decap = template_mask->conf ?
			    template_action->conf : &empty_decap;
		if ((template_action + 1)->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
			if ((template_mask + 1)->type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  template_mask + 1, "invalid mask type");
			encap_action = template_action + 1;
			encap_mask = template_mask + 1;
		}
	} else {
		encap_action = template_action;
		encap_mask = template_mask;
	}
	if (encap_action) {
		raw_encap = encap_action->conf;
		ret = flow_hw_validate_action_raw_encap(encap_action,
							encap_mask, error);
		if (ret)
			return ret;
	}
	return mlx5_flow_dv_validate_action_raw_encap_decap(dev, raw_decap,
							    raw_encap, &attr,
							    action_flags,
							    &actions_n,
							    template_action,
							    item_flags, error);
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
	bool actions_end = false;
	uint16_t i;
	int ret;
	const struct rte_flow_action_ipv6_ext_remove *remove_data;

	if (!mlx5_hw_ctx_validate(dev, error))
		return -rte_errno;
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
		switch ((int)action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT_LIST:
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
		case RTE_FLOW_ACTION_TYPE_FLAG:
			/* TODO: Validation logic */
			action_flags |= MLX5_FLOW_ACTION_FLAG;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			ret = mlx5_hw_validate_action_mark(dev, action, mask,
							   action_flags,
							   attr, error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_MARK;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			ret = mlx5_flow_validate_action_drop
				(dev, action_flags,
				 &(struct rte_flow_attr){.egress = attr->egress},
				 error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DROP;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			/* Only validate the jump to root table in template stage. */
			ret = flow_hw_validate_action_jump(dev, attr, action, mask, error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_JUMP;
			break;
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
		case RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL: {
			bool res;

			if (priv->shared_host)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  action,
							  "action not supported in guest port");
			if (attr->ingress) {
				res = priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_NIC_RX];
			} else if (attr->egress) {
				res = priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_NIC_TX];
			} else {
				if (!is_unified_fdb(priv))
					res = priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_FDB];
				else
					res =
					    priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_FDB_RX] &&
					    priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_FDB_TX] &&
					    priv->hw_send_to_kernel[MLX5DR_TABLE_TYPE_FDB_UNIFIED];
			}
			if (!res)
				return rte_flow_error_set(error, ENOTSUP,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  action,
							  "action is not available");

			action_flags |= MLX5_FLOW_ACTION_SEND_TO_KERNEL;
			break;
		}
#endif
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			ret = mlx5_hw_validate_action_queue(dev, action, mask,
							    attr, action_flags,
							    error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_QUEUE;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = mlx5_hw_validate_action_rss(dev, action, mask,
							  attr, action_flags,
							  error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_RSS;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			ret = mlx5_hw_validate_action_l2_encap(dev, action, mask,
							       attr, action_flags,
							       error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			ret = mlx5_hw_validate_action_l2_decap(dev, action, mask,
							       attr, action_flags,
							       error);
			if (ret)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			ret = flow_hw_validate_action_raw_reformat(dev, action,
								   mask, attr,
								   &action_flags,
								   error);
			if (ret)
				return ret;
			if (action->type == RTE_FLOW_ACTION_TYPE_RAW_DECAP &&
			   (action + 1)->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
				action_flags |= MLX5_FLOW_XCAP_ACTIONS;
				i++;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH:
			ret = flow_hw_validate_action_ipv6_ext_push(dev, action, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_IPV6_ROUTING_PUSH;
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE:
			remove_data = action->conf;
			/* Remove action must be shared. */
			if (remove_data->type != IPPROTO_ROUTING || !mask) {
				DRV_LOG(ERR, "Only supports shared IPv6 routing remove");
				return -EINVAL;
			}
			action_flags |= MLX5_FLOW_ACTION_IPV6_ROUTING_REMOVE;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			ret = flow_hw_validate_action_meter_mark(dev, action, false, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_METER;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			ret = flow_hw_validate_action_modify_field(dev, action, mask,
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
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			ret = flow_hw_validate_action_port_representor
					(dev, attr, action, mask, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_PORT_REPRESENTOR;
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
			ret = mlx5_hw_validate_action_conntrack(dev, action, mask,
								attr, action_flags,
								error);
			if (ret)
				return ret;
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
		case RTE_FLOW_ACTION_TYPE_NAT64:
			ret = flow_hw_validate_action_nat64(dev, attr, action, mask,
							    action_flags, error);
			if (ret != 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_NAT64;
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			actions_end = true;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			ret = flow_hw_validate_action_default_miss(dev, attr,
								   action_flags, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_DEFAULT_MISS;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
			ret = mlx5_flow_validate_action_jump_to_table_index(action, mask, error);
			if (ret < 0)
				return ret;
			action_flags |= MLX5_FLOW_ACTION_JUMP_TO_TABLE_INDEX;
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
	return mlx5_flow_hw_actions_validate(dev, attr, actions, masks, NULL, error);
}


static enum mlx5dr_action_type mlx5_hw_dr_action_types[] = {
	[RTE_FLOW_ACTION_TYPE_MARK] = MLX5DR_ACTION_TYP_TAG,
	[RTE_FLOW_ACTION_TYPE_FLAG] = MLX5DR_ACTION_TYP_TAG,
	[RTE_FLOW_ACTION_TYPE_DROP] = MLX5DR_ACTION_TYP_DROP,
	[RTE_FLOW_ACTION_TYPE_JUMP] = MLX5DR_ACTION_TYP_TBL,
	[RTE_FLOW_ACTION_TYPE_QUEUE] = MLX5DR_ACTION_TYP_TIR,
	[RTE_FLOW_ACTION_TYPE_RSS] = MLX5DR_ACTION_TYP_TIR,
	[RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP] = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2,
	[RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP] = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2,
	[RTE_FLOW_ACTION_TYPE_VXLAN_DECAP] = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2,
	[RTE_FLOW_ACTION_TYPE_NVGRE_DECAP] = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2,
	[RTE_FLOW_ACTION_TYPE_MODIFY_FIELD] = MLX5DR_ACTION_TYP_MODIFY_HDR,
	[RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT] = MLX5DR_ACTION_TYP_VPORT,
	[RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR] = MLX5DR_ACTION_TYP_MISS,
	[RTE_FLOW_ACTION_TYPE_CONNTRACK] = MLX5DR_ACTION_TYP_ASO_CT,
	[RTE_FLOW_ACTION_TYPE_OF_POP_VLAN] = MLX5DR_ACTION_TYP_POP_VLAN,
	[RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN] = MLX5DR_ACTION_TYP_PUSH_VLAN,
	[RTE_FLOW_ACTION_TYPE_SEND_TO_KERNEL] = MLX5DR_ACTION_TYP_DEST_ROOT,
	[RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH] = MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT,
	[RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE] = MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT,
	[RTE_FLOW_ACTION_TYPE_NAT64] = MLX5DR_ACTION_TYP_NAT64,
	[RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX] = MLX5DR_ACTION_TYP_JUMP_TO_MATCHER,
};

static inline void
action_template_set_type(struct rte_flow_actions_template *at,
			 enum mlx5dr_action_type *action_types,
			 unsigned int action_src, uint16_t *curr_off,
			 enum mlx5dr_action_type type)
{
	at->dr_off[action_src] = *curr_off;
	action_types[*curr_off] = type;
	*curr_off = *curr_off + 1;
}

static int
flow_hw_dr_actions_template_handle_shared(int type, uint32_t action_src,
					  enum mlx5dr_action_type *action_types,
					  uint16_t *curr_off, uint16_t *cnt_off,
					  struct rte_flow_actions_template *at)
{
	switch (type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
		action_template_set_type(at, action_types, action_src, curr_off,
					 MLX5DR_ACTION_TYP_TIR);
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
	case RTE_FLOW_ACTION_TYPE_COUNT:
		/*
		 * Both AGE and COUNT action need counter, the first one fills
		 * the action_types array, and the second only saves the offset.
		 */
		if (*cnt_off == UINT16_MAX) {
			*cnt_off = *curr_off;
			action_template_set_type(at, action_types,
						 action_src, curr_off,
						 MLX5DR_ACTION_TYP_CTR);
		}
		at->dr_off[action_src] = *cnt_off;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		action_template_set_type(at, action_types, action_src, curr_off,
					 MLX5DR_ACTION_TYP_ASO_CT);
		break;
	case RTE_FLOW_ACTION_TYPE_QUOTA:
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		action_template_set_type(at, action_types, action_src, curr_off,
					 MLX5DR_ACTION_TYP_ASO_METER);
		break;
	default:
		DRV_LOG(WARNING, "Unsupported shared action type: %d", type);
		return -EINVAL;
	}
	return 0;
}


static int
flow_hw_template_actions_list(struct rte_flow_actions_template *at,
			      unsigned int action_src,
			      enum mlx5dr_action_type *action_types,
			      uint16_t *curr_off, uint16_t *cnt_off)
{
	int ret;
	const struct rte_flow_action_indirect_list *indlst_conf = at->actions[action_src].conf;
	enum mlx5_indirect_list_type list_type = mlx5_get_indirect_list_type(indlst_conf->handle);
	const union {
		struct mlx5_indlst_legacy *legacy;
		struct rte_flow_action_list_handle *handle;
	} indlst_obj = { .handle = indlst_conf->handle };
	enum mlx5dr_action_type type;

	switch (list_type) {
	case MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY:
		ret = flow_hw_dr_actions_template_handle_shared
			(indlst_obj.legacy->legacy_type, action_src,
			 action_types, curr_off, cnt_off, at);
		if (ret)
			return ret;
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
		action_template_set_type(at, action_types, action_src, curr_off,
					 MLX5DR_ACTION_TYP_DEST_ARRAY);
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
		type = ((struct mlx5_hw_encap_decap_action *)
			(indlst_conf->handle))->action_type;
		action_template_set_type(at, action_types, action_src, curr_off, type);
		break;
	default:
		DRV_LOG(ERR, "Unsupported indirect list type");
		return -EINVAL;
	}
	return 0;
}

/**
 * Create DR action template based on a provided sequence of flow actions.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] at
 *   Pointer to flow actions template to be updated.
 * @param[out] action_types
 *   Action types array to be filled.
 * @param[out] tmpl_flags
 *   Template DR flags to be filled.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
flow_hw_parse_flow_actions_to_dr_actions(struct rte_eth_dev *dev,
					struct rte_flow_actions_template *at,
					enum mlx5dr_action_type action_types[MLX5_HW_MAX_ACTS],
					uint32_t *tmpl_flags __rte_unused)
{
	unsigned int i;
	uint16_t curr_off;
	enum mlx5dr_action_type reformat_act_type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
	uint16_t reformat_off = UINT16_MAX;
	uint16_t mhdr_off = UINT16_MAX;
	uint16_t recom_off = UINT16_MAX;
	uint16_t cnt_off = UINT16_MAX;
	enum mlx5dr_action_type recom_type = MLX5DR_ACTION_TYP_LAST;
	int ret;

	for (i = 0, curr_off = 0; at->actions[i].type != RTE_FLOW_ACTION_TYPE_END; ++i) {
		const struct rte_flow_action_raw_encap *raw_encap_data;
		size_t data_size;
		enum mlx5dr_action_type type;

		if (curr_off >= MLX5_HW_MAX_ACTS)
			goto err_actions_num;
		switch ((int)at->actions[i].type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT_LIST:
			ret = flow_hw_template_actions_list(at, i, action_types,
							    &curr_off, &cnt_off);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			ret = flow_hw_dr_actions_template_handle_shared
				(at->masks[i].type, i, action_types,
				 &curr_off, &cnt_off, at);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_DECAP:
			MLX5_ASSERT(reformat_off == UINT16_MAX);
			reformat_off = curr_off++;
			reformat_act_type = mlx5_hw_dr_action_types[at->actions[i].type];
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH:
			MLX5_ASSERT(recom_off == UINT16_MAX);
			recom_type = MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT;
			recom_off = curr_off++;
			break;
		case RTE_FLOW_ACTION_TYPE_IPV6_EXT_REMOVE:
			MLX5_ASSERT(recom_off == UINT16_MAX);
			recom_type = MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT;
			recom_off = curr_off++;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			raw_encap_data = at->actions[i].conf;
			data_size = raw_encap_data->size;
			if (reformat_off != UINT16_MAX) {
				reformat_act_type = data_size < MLX5_ENCAPSULATION_DECISION_SIZE ?
					MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2 :
					MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3;
			} else {
				reformat_off = curr_off++;
				reformat_act_type = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			reformat_off = curr_off++;
			reformat_act_type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			if (mhdr_off == UINT16_MAX) {
				mhdr_off = curr_off++;
				type = mlx5_hw_dr_action_types[at->actions[i].type];
				action_types[mhdr_off] = type;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			type = mlx5_hw_dr_action_types[at->actions[i].type];
			at->dr_off[i] = curr_off;
			action_types[curr_off++] = type;
			i += is_of_vlan_pcp_present(at->actions + i) ?
				MLX5_HW_VLAN_PUSH_PCP_IDX :
				MLX5_HW_VLAN_PUSH_VID_IDX;
			break;
		case RTE_FLOW_ACTION_TYPE_METER_MARK:
			at->dr_off[i] = curr_off;
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
			at->dr_off[i] = cnt_off;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS:
			at->dr_off[i] = curr_off;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_MISS;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
			*tmpl_flags |= MLX5DR_ACTION_TEMPLATE_FLAG_RELAXED_ORDER;
			at->dr_off[i] = curr_off;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_JUMP_TO_MATCHER;
			break;
		case MLX5_RTE_FLOW_ACTION_TYPE_MIRROR:
			at->dr_off[i] = curr_off;
			action_types[curr_off++] = MLX5DR_ACTION_TYP_DEST_ARRAY;
				break;
		default:
			type = mlx5_hw_dr_action_types[at->actions[i].type];
			at->dr_off[i] = curr_off;
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
	if (recom_off != UINT16_MAX) {
		at->recom_off = recom_off;
		action_types[recom_off] = recom_type;
	}
	at->dr_actions_num = curr_off;

	/* Create srh flex parser for remove anchor. */
	if ((recom_type == MLX5DR_ACTION_TYP_POP_IPV6_ROUTE_EXT ||
	     recom_type == MLX5DR_ACTION_TYP_PUSH_IPV6_ROUTE_EXT) &&
	    (ret = mlx5_alloc_srh_flex_parser(dev))) {
		DRV_LOG(ERR, "Failed to create srv6 flex parser");
		return ret;
	}
	return 0;
err_actions_num:
	DRV_LOG(ERR, "Number of HW actions (%u) exceeded maximum (%u) allowed in template",
		curr_off, MLX5_HW_MAX_ACTS);
	return -EINVAL;
}

static int
flow_hw_set_vlan_vid(struct rte_eth_dev *dev,
		     struct rte_flow_action *ra,
		     struct rte_flow_action *rm,
		     struct rte_flow_action_modify_field *spec,
		     struct rte_flow_action_modify_field *mask,
		     int set_vlan_vid_ix,
		     struct rte_flow_error *error)
{
	const bool masked = rm[set_vlan_vid_ix].conf &&
		(((const struct rte_flow_action_of_set_vlan_vid *)
			rm[set_vlan_vid_ix].conf)->vlan_vid != 0);
	const struct rte_flow_action_of_set_vlan_vid *conf =
		ra[set_vlan_vid_ix].conf;
	int width = mlx5_flow_item_field_width(dev, RTE_FLOW_FIELD_VLAN_ID, 0,
					       NULL, error);
	MLX5_ASSERT(width);
	*spec = (typeof(*spec)) {
		.operation = RTE_FLOW_MODIFY_SET,
		.dst = {
			.field = RTE_FLOW_FIELD_VLAN_ID,
			.level = 0, .offset = 0,
		},
		.src = {
			.field = RTE_FLOW_FIELD_VALUE,
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
		},
		.width = 0xffffffff,
	};
	if (masked) {
		uint32_t mask_val = 0xffffffff;

		rte_memcpy(spec->src.value, &conf->vlan_vid, sizeof(conf->vlan_vid));
		rte_memcpy(mask->src.value, &mask_val, sizeof(mask_val));
	}
	ra[set_vlan_vid_ix].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	ra[set_vlan_vid_ix].conf = spec;
	rm[set_vlan_vid_ix].type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD;
	rm[set_vlan_vid_ix].conf = mask;
	return 0;
}

static __rte_always_inline int
flow_hw_set_vlan_vid_construct(struct rte_eth_dev *dev,
			       struct mlx5_modification_cmd *mhdr_cmd,
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
		},
		.width = width,
	};
	struct rte_flow_action modify_action = {
		.type = RTE_FLOW_ACTION_TYPE_MODIFY_FIELD,
		.conf = &conf
	};

	rte_memcpy(conf.src.value, &vid, sizeof(vid));
	return flow_hw_modify_field_construct(mhdr_cmd, act_data, hw_acts, &modify_action);
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
 * @param[in] nt_mode
 *   Non template mode.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Action template pointer on success, NULL otherwise and rte_errno is set.
 */
static struct rte_flow_actions_template *
__flow_hw_actions_template_create(struct rte_eth_dev *dev,
			const struct rte_flow_actions_template_attr *attr,
			const struct rte_flow_action actions[],
			const struct rte_flow_action masks[],
			bool nt_mode,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int len, act_len, mask_len;
	int orig_act_len;
	unsigned int act_num;
	unsigned int i;
	struct rte_flow_actions_template *at = NULL;
	uint16_t pos;
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
	uint16_t src_off[MLX5_HW_MAX_ACTS] = {0, };
	enum mlx5dr_action_type action_types[MLX5_HW_MAX_ACTS] = { MLX5DR_ACTION_TYP_LAST };
	uint32_t tmpl_flags = 0;
	int ret;

	if (!nt_mode && mlx5_flow_hw_actions_validate(dev, attr, actions, masks,
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
		ret = flow_hw_set_vlan_vid(dev, ra, rm,
					   &set_vlan_vid_spec, &set_vlan_vid_mask,
					   set_vlan_vid_ix, error);
		if (ret)
			goto error;
		action_flags |= MLX5_FLOW_ACTION_MODIFY_FIELD;
	}
	if (action_flags & MLX5_FLOW_ACTION_QUOTA) {
		mf_actions[expand_mf_num] = quota_color_inc_action;
		mf_masks[expand_mf_num] = quota_color_inc_mask;
		expand_mf_num++;
	}
	if (priv->sh->config.dv_xmeta_en == MLX5_XMETA_MODE_META32_HWS &&
	    priv->sh->config.dv_esw_en &&
	    !attr->transfer &&
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
		if (pos == MLX5_HW_EXPAND_MH_FAILED) {
			rte_flow_error_set(error, ENOMEM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "modify header expansion failed");
			return NULL;
		}
		act_num += expand_mf_num;
		for (i = pos + expand_mf_num; i < act_num; i++)
			src_off[i] += expand_mf_num;
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
	len += RTE_ALIGN(act_num * sizeof(*at->dr_off), 16);
	len += RTE_ALIGN(act_num * sizeof(*at->src_off), 16);
	orig_act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, NULL, 0, actions, error);
	if (orig_act_len <= 0)
		return NULL;
	len += RTE_ALIGN(orig_act_len, 16);
	at = mlx5_malloc(MLX5_MEM_ZERO, len + sizeof(*at),
			 RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
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
	at->dr_off = (uint16_t *)((uint8_t *)at->masks + mask_len);
	at->src_off = RTE_PTR_ADD(at->dr_off,
				  RTE_ALIGN(act_num * sizeof(*at->dr_off), 16));
	memcpy(at->src_off, src_off, act_num * sizeof(at->src_off[0]));
	at->orig_actions = RTE_PTR_ADD(at->src_off,
				       RTE_ALIGN(act_num * sizeof(*at->src_off), 16));
	orig_act_len = rte_flow_conv(RTE_FLOW_CONV_OP_ACTIONS, at->orig_actions, orig_act_len,
				     actions, error);
	if (orig_act_len <= 0)
		goto error;
	at->actions_num = act_num;
	for (i = 0; i < at->actions_num; ++i)
		at->dr_off[i] = UINT16_MAX;
	at->reformat_off = UINT16_MAX;
	at->mhdr_off = UINT16_MAX;
	at->recom_off = UINT16_MAX;
	for (i = 0; actions->type != RTE_FLOW_ACTION_TYPE_END;
	     actions++, masks++, i++) {
		const struct rte_flow_action_modify_field *info;

		switch (actions->type) {
		/*
		 * mlx5 PMD hacks indirect action index directly to the action conf.
		 * The rte_flow_conv() function copies the content from conf pointer.
		 * Need to restore the indirect action index from action conf here.
		 */
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			at->actions[i].conf = ra[i].conf;
			at->masks[i].conf = rm[i].conf;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			info = actions->conf;
			if ((info->dst.field == RTE_FLOW_FIELD_FLEX_ITEM &&
			     flow_hw_flex_item_acquire(dev, info->dst.flex_handle,
						       &at->flex_item)) ||
			    (info->src.field == RTE_FLOW_FIELD_FLEX_ITEM &&
			     flow_hw_flex_item_acquire(dev, info->src.flex_handle,
						       &at->flex_item)))
				goto error;
			break;
		default:
			break;
		}
	}
	ret = flow_hw_parse_flow_actions_to_dr_actions(dev, at, action_types, &tmpl_flags);
	if (ret)
		goto error;
	at->action_flags = action_flags;
	/* In non template mode there is no need to create the dr template. */
	if (nt_mode)
		return at;
	at->tmpl = mlx5dr_action_template_create(action_types, tmpl_flags);
	if (!at->tmpl) {
		DRV_LOG(ERR, "Failed to create DR action template: %d", rte_errno);
		goto error;
	}
	rte_atomic_fetch_add_explicit(&at->refcnt, 1, rte_memory_order_relaxed);
	LIST_INSERT_HEAD(&priv->flow_hw_at, at, next);
	return at;
error:
	if (at) {
		mlx5_free(at);
	}
	rte_flow_error_set(error, rte_errno,
			   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "Failed to create action template");
	return NULL;
}

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
	return __flow_hw_actions_template_create(dev, attr, actions, masks, false, error);
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
	uint64_t flag = MLX5_FLOW_ACTION_IPV6_ROUTING_REMOVE |
			MLX5_FLOW_ACTION_IPV6_ROUTING_PUSH;

	if (rte_atomic_load_explicit(&template->refcnt, rte_memory_order_relaxed) > 1) {
		DRV_LOG(WARNING, "Action template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "action template is in use");
	}
	if (template->action_flags & flag)
		mlx5_free_srh_flex_parser(dev);
	LIST_REMOVE(template, next);
	flow_hw_flex_item_release(dev, &template->flex_item);
	if (template->tmpl)
		mlx5dr_action_template_destroy(template->tmpl);
	mlx5_free(template);
	return 0;
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
	copied_items = mlx5_malloc(MLX5_MEM_ZERO, size, 0, SOCKET_ID_ANY);
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
flow_hw_item_compare_field_validate(enum rte_flow_field_id arg_field,
				    enum rte_flow_field_id base_field,
				    struct rte_flow_error *error)
{
	switch (arg_field) {
	case RTE_FLOW_FIELD_TAG:
	case RTE_FLOW_FIELD_META:
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
		break;
	case RTE_FLOW_FIELD_RANDOM:
		if (base_field == RTE_FLOW_FIELD_VALUE)
			return 0;
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "compare random is supported only with immediate value");
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "compare item argument field is not supported");
	}
	switch (base_field) {
	case RTE_FLOW_FIELD_TAG:
	case RTE_FLOW_FIELD_META:
	case RTE_FLOW_FIELD_VALUE:
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
		break;
	default:
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "compare item base field is not supported");
	}
	return 0;
}

static inline uint32_t
flow_hw_item_compare_width_supported(enum rte_flow_field_id field)
{
	switch (field) {
	case RTE_FLOW_FIELD_TAG:
	case RTE_FLOW_FIELD_META:
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
		return 32;
	case RTE_FLOW_FIELD_RANDOM:
		return 16;
	default:
		break;
	}
	return 0;
}

static int
flow_hw_validate_item_compare(const struct rte_flow_item *item,
			      struct rte_flow_error *error)
{
	const struct rte_flow_item_compare *comp_m = item->mask;
	const struct rte_flow_item_compare *comp_v = item->spec;
	int ret;

	if (unlikely(!comp_m))
		return rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "compare item mask is missing");
	if (comp_m->width != UINT32_MAX)
		return rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "compare item only support full mask");
	ret = flow_hw_item_compare_field_validate(comp_m->a.field,
						  comp_m->b.field, error);
	if (ret < 0)
		return ret;
	if (comp_v) {
		uint32_t width;

		if (comp_v->operation != comp_m->operation ||
		    comp_v->a.field != comp_m->a.field ||
		    comp_v->b.field != comp_m->b.field)
			return rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "compare item spec/mask not matching");
		width = flow_hw_item_compare_width_supported(comp_v->a.field);
		MLX5_ASSERT(width > 0);
		if ((comp_v->width & comp_m->width) != width)
			return rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "compare item only support full mask");
	}
	return 0;
}

static inline int
mlx5_hw_validate_item_nsh(struct rte_eth_dev *dev,
			  const struct rte_flow_item *item,
			  struct rte_flow_error *error)
{
	return mlx5_flow_validate_item_nsh(dev, item, error);
}

static bool
mlx5_hw_flow_tunnel_ip_check(uint64_t last_item, uint64_t *item_flags)
{
	bool tunnel;

	if (last_item == MLX5_FLOW_LAYER_OUTER_L3_IPV4 ||
	    last_item == MLX5_FLOW_LAYER_OUTER_L3_IPV6) {
		tunnel = true;
		*item_flags |= MLX5_FLOW_LAYER_IPIP;
	} else if (last_item == MLX5_FLOW_LAYER_OUTER_L3_IPV6 ||
		   last_item == MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT) {
		tunnel = true;
		*item_flags |= MLX5_FLOW_LAYER_IPV6_ENCAP;
	} else {
		tunnel = false;
	}
	return tunnel;
}

const struct rte_flow_item_ipv4 hws_nic_ipv4_mask = {
	.hdr = {
		.version = 0xf,
		.ihl = 0xf,
		.type_of_service = 0xff,
		.total_length = RTE_BE16(0xffff),
		.packet_id = RTE_BE16(0xffff),
		.fragment_offset = RTE_BE16(0xffff),
		.time_to_live = 0xff,
		.next_proto_id = 0xff,
		.src_addr = RTE_BE32(0xffffffff),
		.dst_addr = RTE_BE32(0xffffffff),
	},
};

const struct rte_flow_item_ipv6 hws_nic_ipv6_mask = {
	.hdr = {
		.vtc_flow = RTE_BE32(0xffffffff),
		.payload_len = RTE_BE16(0xffff),
		.proto = 0xff,
		.hop_limits = 0xff,
		.src_addr = RTE_IPV6_MASK_FULL,
		.dst_addr = RTE_IPV6_MASK_FULL,
	},
	.has_frag_ext = 1,
};

const struct rte_flow_item_ecpri hws_nic_ecpri_mask = {
	.hdr = {
		.common = {
			.u32 = RTE_BE32(0xffffffff),
		},
		.dummy[0] = 0xffffffff,
	},
};


static int
flow_hw_validate_item_ptype(const struct rte_flow_item *item,
			    struct rte_flow_error *error)
{
	const struct rte_flow_item_ptype *ptype = item->mask;

	/* HWS does not allow empty PTYPE mask */
	if (!ptype)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "empty ptype mask");
	if (!(ptype->packet_type &
	      (RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK |
	       RTE_PTYPE_INNER_L2_MASK | RTE_PTYPE_INNER_L3_MASK |
	       RTE_PTYPE_INNER_L4_MASK)))
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "ptype mask not supported");
	return 0;
}

struct mlx5_hw_pattern_validation_ctx {
	const struct rte_flow_item *geneve_item;
	const struct rte_flow_item *flex_item;
};

static int
__flow_hw_pattern_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_pattern_template_attr *attr,
			 const struct rte_flow_item items[],
			 uint64_t *item_flags,
			 bool nt_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *item;
	const struct rte_flow_item *gtp_item = NULL;
	const struct rte_flow_item *gre_item = NULL;
	const struct rte_flow_attr flow_attr = {
		.ingress = attr->ingress,
		.egress = attr->egress,
		.transfer = attr->transfer
	};
	int ret, tag_idx;
	uint32_t tag_bitmap = 0;
	uint64_t last_item = 0;

	if (!mlx5_hw_ctx_validate(dev, error))
		return -rte_errno;
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
	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		bool tunnel = *item_flags & MLX5_FLOW_LAYER_TUNNEL;

		switch ((int)item->type) {
		case RTE_FLOW_ITEM_TYPE_PTYPE:
			ret = flow_hw_validate_item_ptype(item, error);
			if (ret)
				return ret;
			last_item = MLX5_FLOW_ITEM_PTYPE;
			break;
		case RTE_FLOW_ITEM_TYPE_TAG:
		{
			const struct rte_flow_item_tag *tag =
				(const struct rte_flow_item_tag *)item->spec;

			if (tag == NULL)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Tag spec is NULL");
			if (tag->index >= MLX5_FLOW_HW_TAGS_MAX &&
			    tag->index != RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Invalid tag index");
			tag_idx = flow_hw_get_reg_id(dev, RTE_FLOW_ITEM_TYPE_TAG, tag->index);
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
			last_item = MLX5_FLOW_ITEM_TAG;
			break;
		}
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		{
			const struct rte_flow_item_tag *tag =
				(const struct rte_flow_item_tag *)item->spec;
			uint16_t regcs = (uint8_t)priv->sh->cdev->config.hca_attr.set_reg_c;

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
			last_item = MLX5_FLOW_ITEM_REPRESENTED_PORT;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			/* ingress + group 0 is not supported */
			*item_flags |= MLX5_FLOW_ITEM_METADATA;
			break;
		case RTE_FLOW_ITEM_TYPE_METER_COLOR:
		{
			int reg = flow_hw_get_reg_id(dev,
						     RTE_FLOW_ITEM_TYPE_METER_COLOR,
						     0);
			if (reg == REG_NON)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
							  NULL,
							  "Unsupported meter color register");
			if (*item_flags &
			    (MLX5_FLOW_ITEM_QUOTA | MLX5_FLOW_LAYER_ASO_CT))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Only one ASO item is supported");
			last_item = MLX5_FLOW_ITEM_METER_COLOR;
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
			last_item = MLX5_FLOW_ITEM_AGGR_AFFINITY;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			last_item = MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
		{
			last_item = MLX5_FLOW_LAYER_GENEVE_OPT;
			/*
			 * For non template the parser is internally created before
			 * the flow creation.
			 */
			if (!nt_flow) {
				ret = mlx5_flow_geneve_tlv_option_validate(priv, item,
									   error);
				if (ret < 0)
					return ret;
			}
			break;
		}
		case RTE_FLOW_ITEM_TYPE_COMPARE:
		{
			last_item = MLX5_FLOW_ITEM_COMPARE;
			ret = flow_hw_validate_item_compare(item, error);
			if (ret)
				return ret;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5_flow_validate_item_eth(dev, item,
							  *item_flags,
							  true, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
				    MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5_flow_dv_validate_item_vlan(item, *item_flags,
							      dev, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_VLAN :
				    MLX5_FLOW_LAYER_OUTER_VLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			tunnel |= mlx5_hw_flow_tunnel_ip_check(last_item,
							       item_flags);
			ret = mlx5_flow_dv_validate_item_ipv4(dev, item,
							      *item_flags,
							      last_item, 0,
							      &hws_nic_ipv4_mask,
							      error);
			if (ret)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
				    MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			tunnel |= mlx5_hw_flow_tunnel_ip_check(last_item,
							       item_flags);
			ret = mlx5_flow_validate_item_ipv6(dev, item,
							   *item_flags,
							   last_item, 0,
							   &hws_nic_ipv6_mask,
							   error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
				    MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5_flow_validate_item_udp(dev, item,
							  *item_flags,
							  0xff, error);
			if (ret)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
				    MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5_flow_validate_item_tcp
				(dev, item, *item_flags,
				 0xff, &nic_tcp_mask, error);
			if (ret < 0)
				return ret;
			last_item = tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
				    MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			gtp_item = item;
			ret = mlx5_flow_dv_validate_item_gtp(dev, gtp_item,
							     *item_flags, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GTP;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			ret = mlx5_flow_dv_validate_item_gtp_psc(dev, item,
								 last_item,
								 gtp_item,
								 false, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GTP_PSC;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5_flow_validate_item_vxlan(dev, 0, item,
							    *item_flags,
							    false, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5_flow_validate_item_vxlan_gpe(item,
								*item_flags,
								dev, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5_flow_validate_item_mpls(dev, item,
							   *item_flags,
							   last_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_MPLS;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
		case RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			last_item = MLX5_FLOW_ITEM_SQ;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = mlx5_flow_validate_item_gre(dev, item,
							  *item_flags,
							  0xff, error);
			if (ret < 0)
				return ret;
			gre_item = item;
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			if (!(*item_flags & MLX5_FLOW_LAYER_GRE))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, item, "GRE item is missing");
			ret = mlx5_flow_validate_item_gre_key
				(dev, item, *item_flags, gre_item, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GRE_KEY;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
			if (!(*item_flags & MLX5_FLOW_LAYER_GRE))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, item, "GRE item is missing");
			ret = mlx5_flow_validate_item_gre_option(dev, item,
								 *item_flags,
								 &flow_attr,
								 gre_item,
								 error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			ret = mlx5_flow_validate_item_nvgre(dev, item,
							    *item_flags, 0xff,
							    error);
			if (ret)
				return ret;
			last_item = MLX5_FLOW_LAYER_NVGRE;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			ret = mlx5_flow_validate_item_icmp(dev, item,
							   *item_flags, 0xff,
							   error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			ret = mlx5_flow_validate_item_icmp6(dev, item,
							    *item_flags, 0xff,
							    error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST:
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY:
			ret = mlx5_flow_validate_item_icmp6_echo(dev, item,
								 *item_flags,
								 0xff, error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_LAYER_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_CONNTRACK:
			if (*item_flags &
			    (MLX5_FLOW_ITEM_QUOTA | MLX5_FLOW_LAYER_ASO_CT))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Only one ASO item is supported");
			ret = mlx5_flow_dv_validate_item_aso_ct(dev, item,
								item_flags,
								error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ITEM_TYPE_QUOTA:
			if (*item_flags &
			    (MLX5_FLOW_ITEM_METER_COLOR |
			     MLX5_FLOW_LAYER_ASO_CT))
				return rte_flow_error_set
					(error, EINVAL,
					 RTE_FLOW_ERROR_TYPE_ITEM, NULL, "Only one ASO item is supported");
			last_item = MLX5_FLOW_ITEM_QUOTA;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			ret = mlx5_flow_os_validate_item_esp(dev, item,
							     *item_flags, 0xff,
							     error);
			if (ret < 0)
				return ret;
			last_item = MLX5_FLOW_ITEM_ESP;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
			last_item = tunnel ?
				    MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT :
				    MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT;
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX: {
			enum rte_flow_item_flex_tunnel_mode tunnel_mode = FLEX_TUNNEL_MODE_SINGLE;

			ret = mlx5_flex_get_tunnel_mode(item, &tunnel_mode);
			if (ret < 0)
				return rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_ITEM,
							  item, "Unable to get flex item mode");
			if (tunnel_mode == FLEX_TUNNEL_MODE_TUNNEL)
				last_item = MLX5_FLOW_ITEM_FLEX_TUNNEL;
			else
				last_item = tunnel ?
					    MLX5_FLOW_ITEM_INNER_FLEX :
					    MLX5_FLOW_ITEM_OUTER_FLEX;
			break;
		}
		case RTE_FLOW_ITEM_TYPE_RANDOM:
			last_item = MLX5_FLOW_ITEM_RANDOM;
			break;
		case RTE_FLOW_ITEM_TYPE_NSH:
			last_item = MLX5_FLOW_ITEM_NSH;
			ret = mlx5_hw_validate_item_nsh(dev, item, error);
			if (ret < 0)
				return ret;
			break;
		case RTE_FLOW_ITEM_TYPE_INTEGRITY:
			/*
			 * Integrity flow item validation require access to
			 * both item mask and spec.
			 * Current HWS model allows item mask in pattern
			 * template and item spec in flow rule.
			 */
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			ret = mlx5_flow_validate_item_ecpri(dev, item, *item_flags, last_item,
							    RTE_ETHER_TYPE_ECPRI,
							    &hws_nic_ecpri_mask, error);
			if (ret < 0)
				return ret;
			*item_flags |= MLX5_FLOW_LAYER_ECPRI;
			break;
		case RTE_FLOW_ITEM_TYPE_IB_BTH:
		case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
		case RTE_FLOW_ITEM_TYPE_VOID:
		case RTE_FLOW_ITEM_TYPE_END:
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL,
						  "Unsupported item type");
		}
		*item_flags |= last_item;
	}
	return 1 + RTE_PTR_DIFF(item, items) / sizeof(item[0]);
}

static int
flow_hw_pattern_validate(struct rte_eth_dev *dev,
			 const struct rte_flow_pattern_template_attr *attr,
			 const struct rte_flow_item items[],
			 uint64_t *item_flags,
			 struct rte_flow_error *error)
{
	return __flow_hw_pattern_validate(dev, attr, items, item_flags, false, error);
}

/*
 * Verify that the tested flow patterns fits STE size limit in HWS group.
 *
 *
 * Return values:
 * 0       : Tested patterns fit STE size limit
 * -EINVAL : Invalid parameters detected
 * -E2BIG  : Tested patterns exceed STE size limit
 */
static int
pattern_template_validate(struct rte_eth_dev *dev,
			  struct rte_flow_pattern_template *pt[],
			  uint32_t pt_num,
			  struct rte_flow_error *error)
{
	struct mlx5_flow_template_table_cfg tbl_cfg = {
		.attr = {
			.nb_flows = 64,
			.insertion_type = RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN,
			.hash_func = RTE_FLOW_TABLE_HASH_FUNC_DEFAULT,
			.flow_attr = {
				.group = 1,
				.ingress = pt[0]->attr.ingress,
				.egress = pt[0]->attr.egress,
				.transfer = pt[0]->attr.transfer
			}
		}
	};
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_actions_template *action_template;
	struct rte_flow_template_table *tmpl_tbl;
	int ret;

	if (pt[0]->attr.ingress) {
		action_template =
			priv->action_template_drop[MLX5DR_TABLE_TYPE_NIC_RX];
	} else if (pt[0]->attr.egress) {
		action_template =
			priv->action_template_drop[MLX5DR_TABLE_TYPE_NIC_TX];
	} else if (pt[0]->attr.transfer) {
		action_template =
			priv->action_template_drop[MLX5DR_TABLE_TYPE_FDB];
	} else {
		ret = EINVAL;
		goto end;
	}

	if (pt[0]->item_flags & MLX5_FLOW_ITEM_COMPARE)
		tbl_cfg.attr.nb_flows = 1;
	tmpl_tbl = flow_hw_table_create(dev, &tbl_cfg, pt, pt_num,
					&action_template, 1, error);
	if (tmpl_tbl) {
		ret = 0;
		flow_hw_table_destroy(dev, tmpl_tbl, error);
	} else {
		switch (rte_errno) {
		case E2BIG:
			ret = E2BIG;
			break;
		case ENOTSUP:
			ret = EINVAL;
			break;
		default:
			ret = 0;
			break;
		}
	}
end:
	if (ret)
		rte_flow_error_set(error, ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "failed to validate pattern template");
	return -ret;
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
	uint64_t orig_item_nb, item_flags = 0;
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
	int it_items_size;
	unsigned int i = 0;
	int rc;

	/* Validate application items only */
	rc = flow_hw_pattern_validate(dev, attr, items, &item_flags, error);
	if (rc < 0)
		return NULL;
	orig_item_nb = rc;
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
		if (item_flags & MLX5_FLOW_ITEM_SQ) {
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
	it = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*it), 0, SOCKET_ID_ANY);
	if (!it) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate item template");
		goto error;
	}
	it->attr = *attr;
	it->item_flags = item_flags;
	it->orig_item_nb = orig_item_nb;
	it_items_size = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, NULL, 0, tmpl_items, error);
	if (it_items_size <= 0) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to determine buffer size for pattern");
		goto error;
	}
	it_items_size = RTE_ALIGN(it_items_size, 16);
	it->items = mlx5_malloc(MLX5_MEM_ZERO, it_items_size, 0, SOCKET_ID_ANY);
	if (it->items == NULL) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Cannot allocate memory for pattern");
		goto error;
	}
	rc = rte_flow_conv(RTE_FLOW_CONV_OP_PATTERN, it->items, it_items_size, tmpl_items, error);
	if (rc <= 0) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "Failed to store pattern");
		goto error;
	}
	it->mt = mlx5dr_match_template_create(tmpl_items, attr->relaxed_matching);
	if (!it->mt) {
		rte_flow_error_set(error, rte_errno,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot create match template");
		goto error;
	}
	if (copied_items) {
		if (attr->ingress)
			it->implicit_port = true;
		else if (attr->egress)
			it->implicit_tag = true;
		mlx5_free(copied_items);
		copied_items = NULL;
	}
	/* Either inner or outer, can't both. */
	if (it->item_flags & (MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT |
			      MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT)) {
		if (((it->item_flags & MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT) &&
		     (it->item_flags & MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT)) ||
		    (mlx5_alloc_srh_flex_parser(dev))) {
			rte_flow_error_set(error, rte_errno,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "cannot create IPv6 routing extension support");
			goto error;
		}
	}
	if (it->item_flags & MLX5_FLOW_ITEM_FLEX) {
		for (i = 0; items[i].type != RTE_FLOW_ITEM_TYPE_END; i++) {
			const struct rte_flow_item_flex *spec = items[i].spec;
			struct rte_flow_item_flex_handle *handle;

			if (items[i].type != RTE_FLOW_ITEM_TYPE_FLEX)
				continue;
			handle = spec->handle;
			if (flow_hw_flex_item_acquire(dev, handle,
						      &it->flex_item)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						   NULL, "cannot create hw FLEX item");
				goto error;
			}
		}
	}
	if (it->item_flags & MLX5_FLOW_LAYER_GENEVE_OPT) {
		for (i = 0; items[i].type != RTE_FLOW_ITEM_TYPE_END; i++) {
			const struct rte_flow_item_geneve_opt *spec =
				items[i].spec;

			if (items[i].type != RTE_FLOW_ITEM_TYPE_GENEVE_OPT)
				continue;
			if (mlx5_geneve_tlv_option_register(priv, spec,
							    &it->geneve_opt_mng)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						   NULL, "cannot register GENEVE TLV option");
				goto error;
			}
		}
	}
	rte_atomic_fetch_add_explicit(&it->refcnt, 1, rte_memory_order_relaxed);
	rc = pattern_template_validate(dev, &it, 1, error);
	if (rc)
		goto error;
	LIST_INSERT_HEAD(&priv->flow_hw_itt, it, next);
	return it;
error:
	if (it) {
		if (it->flex_item)
			flow_hw_flex_item_release(dev, &it->flex_item);
		if (it->geneve_opt_mng.nb_options)
			mlx5_geneve_tlv_options_unregister(priv, &it->geneve_opt_mng);
		if (it->mt)
			claim_zero(mlx5dr_match_template_destroy(it->mt));
		mlx5_free(it->items);
		mlx5_free(it);
	}
	if (copied_items)
		mlx5_free(copied_items);
	return NULL;
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
	struct mlx5_priv *priv = dev->data->dev_private;

	if (rte_atomic_load_explicit(&template->refcnt, rte_memory_order_relaxed) > 1) {
		DRV_LOG(WARNING, "Item template %p is still in use.",
			(void *)template);
		return rte_flow_error_set(error, EBUSY,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "item template is in use");
	}
	if (template->item_flags & (MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT |
				    MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT))
		mlx5_free_srh_flex_parser(dev);
	LIST_REMOVE(template, next);
	flow_hw_flex_item_release(dev, &template->flex_item);
	mlx5_geneve_tlv_options_unregister(priv, &template->geneve_opt_mng);
	claim_zero(mlx5dr_match_template_destroy(template->mt));
	mlx5_free(template->items);
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
	uint32_t *specialize = (uint32_t *)ctx->data2;
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	struct mlx5dr_table_attr dr_tbl_attr = {0};
	struct rte_flow_error *error = ctx->error;
	struct mlx5_flow_group *grp_data;
	struct mlx5dr_table *tbl = NULL;
	struct mlx5dr_action *jump;
	uint32_t hws_flags;
	uint32_t idx = 0;
	MKSTR(matcher_name, "%s_%s_%u_%u_matcher_list",
	      attr->transfer ? "FDB" : "NIC", attr->egress ? "egress" : "ingress",
	      attr->group, idx);

	grp_data = mlx5_ipool_zmalloc(sh->ipool[MLX5_IPOOL_HW_GRP], &idx);
	if (!grp_data) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL,
				   "cannot allocate flow table data entry");
		return NULL;
	}
	dr_tbl_attr.level = attr->group;
	dr_tbl_attr.type = get_mlx5dr_table_type(attr, *specialize, unified_fdb);
	tbl = mlx5dr_table_create(priv->dr_ctx, &dr_tbl_attr);
	if (!tbl)
		goto error;
	grp_data->tbl = tbl;
	if (attr->group) {
		hws_flags = mlx5_hw_act_dest_table_flag[dr_tbl_attr.type];
		/* For case of jump from FDB Tx to FDB Rx as it is supported now. */
		if (priv->jump_fdb_rx_en &&
		    dr_tbl_attr.type == MLX5DR_TABLE_TYPE_FDB_RX)
			hws_flags |= MLX5DR_ACTION_FLAG_HWS_FDB_TX;
		/* Jump action be used by non-root table. */
		jump = mlx5dr_action_create_dest_table
			(priv->dr_ctx, tbl,
			 hws_flags);
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

	grp_data->matchers = mlx5_list_create(matcher_name, sh, true,
					      flow_matcher_create_cb,
					      flow_matcher_match_cb,
					      flow_matcher_remove_cb,
					      flow_matcher_clone_cb,
					      flow_matcher_clone_free_cb);
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
	mlx5_list_destroy(grp_data->matchers);
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
		((grp_data->type < MLX5DR_TABLE_TYPE_FDB) &&
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
			 is_unified_fdb(priv) ?
				(MLX5DR_ACTION_FLAG_HWS_FDB_RX |
				 MLX5DR_ACTION_FLAG_HWS_FDB_TX |
				 MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) :
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
	bool unified_fdb = is_unified_fdb(priv);
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
				 unified_fdb ?
					(MLX5DR_ACTION_FLAG_HWS_FDB_RX |
					 MLX5DR_ACTION_FLAG_HWS_FDB_TX |
					 MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) :
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

#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
static __rte_always_inline void
_create_send_to_kernel_actions(struct mlx5_priv *priv, int type)
{
	int action_flag;

	action_flag = mlx5_hw_act_flag[1][type];
	priv->hw_send_to_kernel[type] =
		mlx5dr_action_create_dest_root(priv->dr_ctx,
				MLX5_HW_LOWEST_PRIO_ROOT,
				action_flag);
	if (!priv->hw_send_to_kernel[type])
		DRV_LOG(WARNING, "Unable to create HWS send to kernel action");
}
#endif

static void
flow_hw_create_send_to_kernel_actions(__rte_unused struct mlx5_priv *priv,
				      __rte_unused bool is_proxy)
{
#ifdef HAVE_MLX5DV_DR_ACTION_CREATE_DEST_ROOT_TABLE
	int i, from, to;
	bool unified_fdb = is_unified_fdb(priv);

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i <= MLX5DR_TABLE_TYPE_NIC_TX; i++)
		_create_send_to_kernel_actions(priv, i);

	if (is_proxy) {
		from = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_RX : MLX5DR_TABLE_TYPE_FDB;
		to = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_UNIFIED : MLX5DR_TABLE_TYPE_FDB;
		for (i = from; i <= to; i++)
			_create_send_to_kernel_actions(priv, i);
	}
#endif
}

static void
flow_hw_destroy_send_to_kernel_action(struct mlx5_priv *priv)
{
	int i;

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		if (priv->hw_send_to_kernel[i]) {
			mlx5dr_action_destroy(priv->hw_send_to_kernel[i]);
			priv->hw_send_to_kernel[i] = NULL;
		}
	}
}

static bool
flow_hw_should_create_nat64_actions(struct mlx5_priv *priv)
{
	int i;

	/* Check if all registers are available. */
	for (i = 0; i < MLX5_FLOW_NAT64_REGS_MAX; ++i)
		if (priv->sh->registers.nat64_regs[i] == REG_NON)
			return false;

	return true;
}

static void
flow_hw_destroy_nat64_actions(struct mlx5_priv *priv)
{
	uint32_t i;

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		if (priv->action_nat64[i][RTE_FLOW_NAT64_6TO4]) {
			(void)mlx5dr_action_destroy(priv->action_nat64[i][RTE_FLOW_NAT64_6TO4]);
			priv->action_nat64[i][RTE_FLOW_NAT64_6TO4] = NULL;
		}
		if (priv->action_nat64[i][RTE_FLOW_NAT64_4TO6]) {
			(void)mlx5dr_action_destroy(priv->action_nat64[i][RTE_FLOW_NAT64_4TO6]);
			priv->action_nat64[i][RTE_FLOW_NAT64_4TO6] = NULL;
		}
	}
}

static int
_create_nat64_actions(struct mlx5_priv *priv,
		      struct mlx5dr_action_nat64_attr *attr,
		      int type,
		      struct rte_flow_error *error)
{
	const uint32_t flags[MLX5DR_TABLE_TYPE_MAX] = {
		MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_SHARED,
		MLX5DR_ACTION_FLAG_HWS_TX | MLX5DR_ACTION_FLAG_SHARED,
		MLX5DR_ACTION_FLAG_HWS_FDB | MLX5DR_ACTION_FLAG_SHARED,
		MLX5DR_ACTION_FLAG_HWS_FDB_RX | MLX5DR_ACTION_FLAG_SHARED,
		MLX5DR_ACTION_FLAG_HWS_FDB_TX | MLX5DR_ACTION_FLAG_SHARED,
		MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED | MLX5DR_ACTION_FLAG_SHARED,
	};
	struct mlx5dr_action *act;

	attr->flags = (enum mlx5dr_action_nat64_flags)
		(MLX5DR_ACTION_NAT64_V6_TO_V4 | MLX5DR_ACTION_NAT64_BACKUP_ADDR);
	act = mlx5dr_action_create_nat64(priv->dr_ctx, attr, flags[type]);
	if (!act)
		return rte_flow_error_set(error, rte_errno,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to create v6 to v4 action.");
	priv->action_nat64[type][RTE_FLOW_NAT64_6TO4] = act;
	attr->flags = (enum mlx5dr_action_nat64_flags)
		(MLX5DR_ACTION_NAT64_V4_TO_V6 | MLX5DR_ACTION_NAT64_BACKUP_ADDR);
	act = mlx5dr_action_create_nat64(priv->dr_ctx, attr, flags[type]);
	if (!act)
		return rte_flow_error_set(error, rte_errno,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"Failed to create v4 to v6 action.");
	priv->action_nat64[type][RTE_FLOW_NAT64_4TO6] = act;
	return 0;
}

static int
flow_hw_create_nat64_actions(struct mlx5_priv *priv, struct rte_flow_error *error)
{
	struct mlx5dr_action_nat64_attr attr;
	uint8_t regs[MLX5_FLOW_NAT64_REGS_MAX];
	uint32_t i, from, to;
	int rc;
	bool unified_fdb = is_unified_fdb(priv);

	attr.registers = regs;
	/* Try to use 3 registers by default. */
	attr.num_of_registers = MLX5_FLOW_NAT64_REGS_MAX;
	for (i = 0; i < MLX5_FLOW_NAT64_REGS_MAX; i++) {
		MLX5_ASSERT(priv->sh->registers.nat64_regs[i] != REG_NON);
		regs[i] = mlx5_convert_reg_to_field(priv->sh->registers.nat64_regs[i]);
	}
	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i <= MLX5DR_TABLE_TYPE_NIC_TX; i++) {
		rc = _create_nat64_actions(priv, &attr, i, error);
		if (rc)
			return rc;
	}
	if (priv->sh->config.dv_esw_en) {
		from = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_RX :
					MLX5DR_TABLE_TYPE_FDB;
		to = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_UNIFIED :
			MLX5DR_TABLE_TYPE_FDB;

		for (i = from; i <= to; i++) {
			rc = _create_nat64_actions(priv, &attr, i, error);
			if (rc)
				return rc;
		}
	}
	return 0;
}

/**
 * Create an egress pattern template matching on source SQ.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to pattern template on success. NULL otherwise, and rte_errno is set.
 */
static struct rte_flow_pattern_template *
flow_hw_create_tx_repr_sq_pattern_tmpl(struct rte_eth_dev *dev, struct rte_flow_error *error)
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

	return flow_hw_pattern_template_create(dev, &attr, items, error);
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
	MLX5_ASSERT(rte_popcount32(mask) >= rte_popcount32(priv->vport_meta_mask));
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
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to actions template on success. NULL otherwise, and rte_errno is set.
 */
static struct rte_flow_actions_template *
flow_hw_create_tx_repr_tag_jump_acts_tmpl(struct rte_eth_dev *dev,
					  struct rte_flow_error *error)
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
		.width = rte_popcount32(tag_mask),
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
	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m, error);
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
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, negative errno value otherwise.
 */
static int
flow_hw_setup_tx_repr_tagging(struct rte_eth_dev *dev, struct rte_flow_error *error)
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
	priv->hw_tx_repr_tagging_pt =
		flow_hw_create_tx_repr_sq_pattern_tmpl(dev, error);
	if (!priv->hw_tx_repr_tagging_pt)
		goto err;
	priv->hw_tx_repr_tagging_at =
		flow_hw_create_tx_repr_tag_jump_acts_tmpl(dev, error);
	if (!priv->hw_tx_repr_tagging_at)
		goto err;
	priv->hw_tx_repr_tagging_tbl = flow_hw_table_create(dev, &cfg,
							    &priv->hw_tx_repr_tagging_pt, 1,
							    &priv->hw_tx_repr_tagging_at, 1,
							    error);
	if (!priv->hw_tx_repr_tagging_tbl)
		goto err;
	return 0;
err:
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_esw_mgr_pattern_template(struct rte_eth_dev *dev,
					     struct rte_flow_error *error)
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

	return flow_hw_pattern_template_create(dev, &attr, items, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_regc_sq_pattern_template(struct rte_eth_dev *dev,
					     struct rte_flow_error *error)
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

	return flow_hw_pattern_template_create(dev, &attr, items, error);
}

/**
 * Creates a flow pattern template with unmasked represented port matching.
 * This template is used to set up a table for default transfer flows
 * directing packets to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_ctrl_port_pattern_template(struct rte_eth_dev *dev,
					  struct rte_flow_error *error)
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

	return flow_hw_pattern_template_create(dev, &attr, items, error);
}

/*
 * Creating a flow pattern template with all ETH packets matching.
 * This template is used to set up a table for default Tx copy (Tx metadata
 * to REG_C_1) flow rule usage.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_tx_default_mreg_copy_pattern_template(struct rte_eth_dev *dev,
						     struct rte_flow_error *error)
{
	struct rte_flow_pattern_template_attr tx_pa_attr = {
		.relaxed_matching = 0,
		.egress = 1,
	};
	struct rte_flow_item_eth promisc = {
		.hdr.dst_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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

	return flow_hw_pattern_template_create(dev, &tx_pa_attr, eth_all, error);
}

/*
 * Creating a flow pattern template with all LACP packets matching, only for NIC
 * ingress domain.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow pattern template on success, NULL otherwise.
 */
static struct rte_flow_pattern_template *
flow_hw_create_lacp_rx_pattern_template(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct rte_flow_pattern_template_attr pa_attr = {
		.relaxed_matching = 0,
		.ingress = 1,
	};
	struct rte_flow_item_eth lacp_mask = {
		.dst.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.src.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.type = 0xFFFF,
	};
	struct rte_flow_item eth_all[] = {
		[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.mask = &lacp_mask,
		},
		[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	return flow_hw_pattern_template_create(dev, &pa_attr, eth_all, error);
}

/**
 * Creates a flow actions template with modify field action and masked jump action.
 * Modify field action sets the least significant bit of REG_C_0 (usable by user-space)
 * to 1, meaning that packet was originated from E-Switch Manager. Jump action
 * transfers steering to group 1.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_regc_jump_actions_template(struct rte_eth_dev *dev,
					       struct rte_flow_error *error)
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
		.width = rte_popcount32(marker_mask),
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
	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_jump_actions_template(struct rte_eth_dev *dev,
					  uint32_t group,
					  struct rte_flow_error *error)
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

	return flow_hw_actions_template_create(dev, &attr, actions_v,
					       actions_m, error);
}

/**
 * Creates a flow action template with a unmasked REPRESENTED_PORT action.
 * It is used to create control flow tables.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow action template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_ctrl_port_actions_template(struct rte_eth_dev *dev,
					  struct rte_flow_error *error)
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

	return flow_hw_actions_template_create(dev, &attr, actions_v, actions_m, error);
}

/*
 * Creating an actions template to use header modify action for register
 * copying. This template is used to set up a table for copy flow.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_tx_default_mreg_copy_actions_template(struct rte_eth_dev *dev,
						     struct rte_flow_error *error)
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

	return flow_hw_actions_template_create(dev, &tx_act_attr, actions,
					       masks, error);
}

/*
 * Creating an actions template to use default miss to re-route packets to the
 * kernel driver stack.
 * On root table, only DEFAULT_MISS action can be used.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow actions template on success, NULL otherwise.
 */
static struct rte_flow_actions_template *
flow_hw_create_lacp_rx_actions_template(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct rte_flow_actions_template_attr act_attr = {
		.ingress = 1,
	};
	const struct rte_flow_action actions[] = {
		[0] = {
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};

	return flow_hw_actions_template_create(dev, &act_attr, actions, actions, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_ctrl_sq_miss_root_table(struct rte_eth_dev *dev,
				       struct rte_flow_pattern_template *it,
				       struct rte_flow_actions_template *at,
				       struct rte_flow_error *error)
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

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_ctrl_sq_miss_table(struct rte_eth_dev *dev,
				  struct rte_flow_pattern_template *it,
				  struct rte_flow_actions_template *at,
				  struct rte_flow_error *error)
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

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table*
flow_hw_create_tx_default_mreg_copy_table(struct rte_eth_dev *dev,
					  struct rte_flow_pattern_template *pt,
					  struct rte_flow_actions_template *at,
					  struct rte_flow_error *error)
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

	return flow_hw_table_create(dev, &tx_tbl_cfg, &pt, 1, &at, 1, error);
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
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   Pointer to flow table on success, NULL otherwise.
 */
static struct rte_flow_template_table *
flow_hw_create_ctrl_jump_table(struct rte_eth_dev *dev,
			       struct rte_flow_pattern_template *it,
			       struct rte_flow_actions_template *at,
			       struct rte_flow_error *error)
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

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, error);
}

/**
 * Cleans up all template tables and pattern, and actions templates used for
 * FDB control flow rules.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
flow_hw_cleanup_ctrl_fdb_tables(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_hw_ctrl_fdb *hw_ctrl_fdb;

	if (!priv->hw_ctrl_fdb)
		return;
	hw_ctrl_fdb = priv->hw_ctrl_fdb;
	/* Clean up templates used for LACP default miss table. */
	if (hw_ctrl_fdb->hw_lacp_rx_tbl)
		claim_zero(flow_hw_table_destroy(dev, hw_ctrl_fdb->hw_lacp_rx_tbl, NULL));
	if (hw_ctrl_fdb->lacp_rx_actions_tmpl)
		claim_zero(flow_hw_actions_template_destroy(dev, hw_ctrl_fdb->lacp_rx_actions_tmpl,
			   NULL));
	if (hw_ctrl_fdb->lacp_rx_items_tmpl)
		claim_zero(flow_hw_pattern_template_destroy(dev, hw_ctrl_fdb->lacp_rx_items_tmpl,
			   NULL));
	/* Clean up templates used for default Tx metadata copy. */
	if (hw_ctrl_fdb->hw_tx_meta_cpy_tbl)
		claim_zero(flow_hw_table_destroy(dev, hw_ctrl_fdb->hw_tx_meta_cpy_tbl, NULL));
	if (hw_ctrl_fdb->tx_meta_actions_tmpl)
		claim_zero(flow_hw_actions_template_destroy(dev, hw_ctrl_fdb->tx_meta_actions_tmpl,
			   NULL));
	if (hw_ctrl_fdb->tx_meta_items_tmpl)
		claim_zero(flow_hw_pattern_template_destroy(dev, hw_ctrl_fdb->tx_meta_items_tmpl,
			   NULL));
	/* Clean up templates used for default FDB jump rule. */
	if (hw_ctrl_fdb->hw_esw_zero_tbl)
		claim_zero(flow_hw_table_destroy(dev, hw_ctrl_fdb->hw_esw_zero_tbl, NULL));
	if (hw_ctrl_fdb->jump_one_actions_tmpl)
		claim_zero(flow_hw_actions_template_destroy(dev, hw_ctrl_fdb->jump_one_actions_tmpl,
			   NULL));
	if (hw_ctrl_fdb->port_items_tmpl)
		claim_zero(flow_hw_pattern_template_destroy(dev, hw_ctrl_fdb->port_items_tmpl,
			   NULL));
	/* Clean up templates used for default SQ miss flow rules - non-root table. */
	if (hw_ctrl_fdb->hw_esw_sq_miss_tbl)
		claim_zero(flow_hw_table_destroy(dev, hw_ctrl_fdb->hw_esw_sq_miss_tbl, NULL));
	if (hw_ctrl_fdb->regc_sq_items_tmpl)
		claim_zero(flow_hw_pattern_template_destroy(dev, hw_ctrl_fdb->regc_sq_items_tmpl,
			   NULL));
	if (hw_ctrl_fdb->port_actions_tmpl)
		claim_zero(flow_hw_actions_template_destroy(dev, hw_ctrl_fdb->port_actions_tmpl,
			   NULL));
	/* Clean up templates used for default SQ miss flow rules - root table. */
	if (hw_ctrl_fdb->hw_esw_sq_miss_root_tbl)
		claim_zero(flow_hw_table_destroy(dev, hw_ctrl_fdb->hw_esw_sq_miss_root_tbl, NULL));
	if (hw_ctrl_fdb->regc_jump_actions_tmpl)
		claim_zero(flow_hw_actions_template_destroy(dev,
			   hw_ctrl_fdb->regc_jump_actions_tmpl, NULL));
	if (hw_ctrl_fdb->esw_mgr_items_tmpl)
		claim_zero(flow_hw_pattern_template_destroy(dev, hw_ctrl_fdb->esw_mgr_items_tmpl,
			   NULL));
	/* Clean up templates structure for FDB control flow rules. */
	mlx5_free(hw_ctrl_fdb);
	priv->hw_ctrl_fdb = NULL;
}

/*
 * Create a table on the root group to for the LACP traffic redirecting.
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
flow_hw_create_lacp_rx_table(struct rte_eth_dev *dev,
			     struct rte_flow_pattern_template *it,
			     struct rte_flow_actions_template *at,
			     struct rte_flow_error *error)
{
	struct rte_flow_template_table_attr attr = {
		.flow_attr = {
			.group = 0,
			.priority = 0,
			.ingress = 1,
			.egress = 0,
			.transfer = 0,
		},
		.nb_flows = 1,
	};
	struct mlx5_flow_template_table_cfg cfg = {
		.attr = attr,
		.external = false,
	};

	return flow_hw_table_create(dev, &cfg, &it, 1, &at, 1, error);
}

/**
 * Creates a set of flow tables used to create control flows used
 * when E-Switch is engaged.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, negative values otherwise
 */
static int
flow_hw_create_ctrl_tables(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_hw_ctrl_fdb *hw_ctrl_fdb;
	uint32_t xmeta = priv->sh->config.dv_xmeta_en;
	uint32_t repr_matching = priv->sh->config.repr_matching;
	uint32_t fdb_def_rule = priv->sh->config.fdb_def_rule;

	MLX5_ASSERT(priv->hw_ctrl_fdb == NULL);
	hw_ctrl_fdb = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*hw_ctrl_fdb), 0, SOCKET_ID_ANY);
	if (!hw_ctrl_fdb) {
		DRV_LOG(ERR, "port %u failed to allocate memory for FDB control flow templates",
			dev->data->port_id);
		rte_errno = ENOMEM;
		goto err;
	}
	priv->hw_ctrl_fdb = hw_ctrl_fdb;
	if (fdb_def_rule) {
		/* Create templates and table for default SQ miss flow rules - root table. */
		hw_ctrl_fdb->esw_mgr_items_tmpl =
				flow_hw_create_ctrl_esw_mgr_pattern_template(dev, error);
		if (!hw_ctrl_fdb->esw_mgr_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to create E-Switch Manager item"
				" template for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->regc_jump_actions_tmpl =
				flow_hw_create_ctrl_regc_jump_actions_template(dev, error);
		if (!hw_ctrl_fdb->regc_jump_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to create REG_C set and jump action template"
				" for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->hw_esw_sq_miss_root_tbl =
				flow_hw_create_ctrl_sq_miss_root_table
					(dev, hw_ctrl_fdb->esw_mgr_items_tmpl,
					 hw_ctrl_fdb->regc_jump_actions_tmpl, error);
		if (!hw_ctrl_fdb->hw_esw_sq_miss_root_tbl) {
			DRV_LOG(ERR, "port %u failed to create table for default sq miss (root table)"
				" for control flows", dev->data->port_id);
			goto err;
		}
		/* Create templates and table for default SQ miss flow rules - non-root table. */
		hw_ctrl_fdb->regc_sq_items_tmpl =
				flow_hw_create_ctrl_regc_sq_pattern_template(dev, error);
		if (!hw_ctrl_fdb->regc_sq_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to create SQ item template for"
				" control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->port_actions_tmpl =
				flow_hw_create_ctrl_port_actions_template(dev, error);
		if (!hw_ctrl_fdb->port_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to create port action template"
				" for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->hw_esw_sq_miss_tbl =
				flow_hw_create_ctrl_sq_miss_table
					(dev, hw_ctrl_fdb->regc_sq_items_tmpl,
					 hw_ctrl_fdb->port_actions_tmpl, error);
		if (!hw_ctrl_fdb->hw_esw_sq_miss_tbl) {
			DRV_LOG(ERR, "port %u failed to create table for default sq miss (non-root table)"
				" for control flows", dev->data->port_id);
			goto err;
		}
		/* Create templates and table for default FDB jump flow rules. */
		hw_ctrl_fdb->port_items_tmpl =
				flow_hw_create_ctrl_port_pattern_template(dev, error);
		if (!hw_ctrl_fdb->port_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to create SQ item template for"
				" control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->jump_one_actions_tmpl =
				flow_hw_create_ctrl_jump_actions_template
					(dev, MLX5_HW_LOWEST_USABLE_GROUP, error);
		if (!hw_ctrl_fdb->jump_one_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to create jump action template"
				" for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->hw_esw_zero_tbl = flow_hw_create_ctrl_jump_table
				(dev, hw_ctrl_fdb->port_items_tmpl,
				 hw_ctrl_fdb->jump_one_actions_tmpl, error);
		if (!hw_ctrl_fdb->hw_esw_zero_tbl) {
			DRV_LOG(ERR, "port %u failed to create table for default jump to group 1"
				" for control flows", dev->data->port_id);
			goto err;
		}
	}
	/* Create templates and table for default Tx metadata copy flow rule. */
	if (!repr_matching && xmeta == MLX5_XMETA_MODE_META32_HWS) {
		hw_ctrl_fdb->tx_meta_items_tmpl =
			flow_hw_create_tx_default_mreg_copy_pattern_template(dev, error);
		if (!hw_ctrl_fdb->tx_meta_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to Tx metadata copy pattern"
				" template for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->tx_meta_actions_tmpl =
			flow_hw_create_tx_default_mreg_copy_actions_template(dev, error);
		if (!hw_ctrl_fdb->tx_meta_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to Tx metadata copy actions"
				" template for control flows", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->hw_tx_meta_cpy_tbl =
			flow_hw_create_tx_default_mreg_copy_table
				(dev, hw_ctrl_fdb->tx_meta_items_tmpl,
				 hw_ctrl_fdb->tx_meta_actions_tmpl, error);
		if (!hw_ctrl_fdb->hw_tx_meta_cpy_tbl) {
			DRV_LOG(ERR, "port %u failed to create table for default"
				" Tx metadata copy flow rule", dev->data->port_id);
			goto err;
		}
	}
	/* Create LACP default miss table. */
	if (!priv->sh->config.lacp_by_user && priv->pf_bond >= 0 && priv->master) {
		hw_ctrl_fdb->lacp_rx_items_tmpl =
				flow_hw_create_lacp_rx_pattern_template(dev, error);
		if (!hw_ctrl_fdb->lacp_rx_items_tmpl) {
			DRV_LOG(ERR, "port %u failed to create pattern template"
				" for LACP Rx traffic", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->lacp_rx_actions_tmpl =
				flow_hw_create_lacp_rx_actions_template(dev, error);
		if (!hw_ctrl_fdb->lacp_rx_actions_tmpl) {
			DRV_LOG(ERR, "port %u failed to create actions template"
				" for LACP Rx traffic", dev->data->port_id);
			goto err;
		}
		hw_ctrl_fdb->hw_lacp_rx_tbl = flow_hw_create_lacp_rx_table
				(dev, hw_ctrl_fdb->lacp_rx_items_tmpl,
				 hw_ctrl_fdb->lacp_rx_actions_tmpl, error);
		if (!hw_ctrl_fdb->hw_lacp_rx_tbl) {
			DRV_LOG(ERR, "port %u failed to create template table for"
				" for LACP Rx traffic", dev->data->port_id);
			goto err;
		}
	}
	return 0;

err:
	flow_hw_cleanup_ctrl_fdb_tables(dev);
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
flow_hw_ct_pool_destroy(struct rte_eth_dev *dev,
			struct mlx5_aso_ct_pool *pool)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (pool->dr_action)
		mlx5dr_action_destroy(pool->dr_action);
	if (!priv->shared_host) {
		if (pool->devx_obj)
			claim_zero(mlx5_devx_cmd_destroy(pool->devx_obj));
		if (pool->cts)
			mlx5_ipool_destroy(pool->cts);
	}
	mlx5_free(pool);
}

static struct mlx5_aso_ct_pool *
flow_hw_ct_pool_create(struct rte_eth_dev *dev,
		       uint32_t nb_conn_tracks)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool;
	struct mlx5_devx_obj *obj;
	uint32_t nb_cts = rte_align32pow2(nb_conn_tracks);
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
	uint32_t flags = 0;

	pool = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*pool), 0, SOCKET_ID_ANY);
	if (!pool) {
		rte_errno = ENOMEM;
		return NULL;
	}
	if (!priv->shared_host) {
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
		cfg.max_idx = nb_cts;
		pool->cts = mlx5_ipool_create(&cfg);
		if (!pool->cts)
			goto err;
		obj = mlx5_devx_cmd_create_conn_track_offload_obj(priv->sh->cdev->ctx,
								  priv->sh->cdev->pdn,
								  log_obj_size);
		if (!obj) {
			rte_errno = ENODATA;
			DRV_LOG(ERR, "Failed to create conn_track_offload_obj using DevX.");
			goto err;
		}
		pool->devx_obj = obj;
	} else {
		struct rte_eth_dev *host_dev = priv->shared_host;
		struct mlx5_priv *host_priv = host_dev->data->dev_private;

		pool->devx_obj = host_priv->hws_ctpool->devx_obj;
		pool->cts = host_priv->hws_ctpool->cts;
		MLX5_ASSERT(pool->cts);
		MLX5_ASSERT(!nb_conn_tracks);
	}
	reg_id = mlx5_flow_get_reg_id(dev, MLX5_ASO_CONNTRACK, 0, NULL);
	flags |= MLX5DR_ACTION_FLAG_HWS_RX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (priv->sh->config.dv_esw_en && priv->master) {
		flags |= ((is_unified_fdb(priv)) ?
			(MLX5DR_ACTION_FLAG_HWS_FDB_RX |
			 MLX5DR_ACTION_FLAG_HWS_FDB_TX |
			 MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) :
			 MLX5DR_ACTION_FLAG_HWS_FDB);
	}
	pool->dr_action = mlx5dr_action_create_aso_ct(priv->dr_ctx,
						      (struct mlx5dr_devx_obj *)pool->devx_obj,
						      reg_id - REG_C_0, flags);
	if (!pool->dr_action)
		goto err;
	pool->sq = priv->ct_mng->aso_sqs;
	/* Assign the last extra ASO SQ as public SQ. */
	pool->shared_sq = &priv->ct_mng->aso_sqs[priv->nb_queue - 1];
	return pool;
err:
	flow_hw_ct_pool_destroy(dev, pool);
	return NULL;
}

static int
mlx5_flow_ct_init(struct rte_eth_dev *dev,
		  uint32_t nb_conn_tracks,
		  uint16_t nb_queue)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t mem_size;
	int ret = -ENOMEM;

	if (!priv->shared_host) {
		mem_size = sizeof(struct mlx5_aso_sq) * nb_queue +
				sizeof(*priv->ct_mng);
		priv->ct_mng = mlx5_malloc(MLX5_MEM_ZERO, mem_size,
						RTE_CACHE_LINE_SIZE,
						SOCKET_ID_ANY);
		if (!priv->ct_mng)
			goto err;
		ret = mlx5_aso_ct_queue_init(priv->sh, priv->ct_mng,
						nb_queue);
		if (ret)
			goto err;
	}
	priv->hws_ctpool = flow_hw_ct_pool_create(dev, nb_conn_tracks);
	if (!priv->hws_ctpool)
		goto err;
	priv->sh->ct_aso_en = 1;
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
	return ret;
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
_create_vlan(struct mlx5_priv *priv, enum mlx5dr_table_type type)
{
	const enum mlx5dr_action_flags flags[MLX5DR_TABLE_TYPE_MAX] = {
		MLX5DR_ACTION_FLAG_HWS_RX,
		MLX5DR_ACTION_FLAG_HWS_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB,
		MLX5DR_ACTION_FLAG_HWS_FDB_RX,
		MLX5DR_ACTION_FLAG_HWS_FDB_TX,
		MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED,
	};

	/* rte_errno is set in the mlx5dr_action* functions. */
	priv->hw_pop_vlan[type] =
		mlx5dr_action_create_pop_vlan(priv->dr_ctx, flags[type]);
	if (!priv->hw_pop_vlan[type])
		return -rte_errno;
	priv->hw_push_vlan[type] =
		mlx5dr_action_create_push_vlan(priv->dr_ctx, flags[type]);
	if (!priv->hw_push_vlan[type])
		return -rte_errno;
	return 0;
}

static int
flow_hw_create_vlan(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	enum mlx5dr_table_type i, from, to;
	int rc;
	bool unified_fdb = is_unified_fdb(priv);

	for (i = MLX5DR_TABLE_TYPE_NIC_RX; i <= MLX5DR_TABLE_TYPE_NIC_TX; i++) {
		rc = _create_vlan(priv, i);
		if (rc)
			return rc;
	}
	from = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_RX : MLX5DR_TABLE_TYPE_FDB;
	to = unified_fdb ? MLX5DR_TABLE_TYPE_FDB_UNIFIED : MLX5DR_TABLE_TYPE_FDB;
	if (priv->sh->config.dv_esw_en && priv->master) {
		for (i = from; i <= to; i++) {
			rc = _create_vlan(priv, i);
			if (rc)
				return rc;
		}
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
				       RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
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
 * @param[in] nt_mode
 *   Non template mode.
 *
 * @return
 *   Pointer to copied HWS configuration is returned on success.
 *   Otherwise, NULL is returned and rte_errno is set.
 */
static struct mlx5_flow_hw_attr *
flow_hw_alloc_copy_config(const struct rte_flow_port_attr *port_attr,
			  const uint16_t nb_queue,
			  const struct rte_flow_queue_attr *queue_attr[],
			  bool nt_mode,
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
	hw_attr->nt_mode = nt_mode;
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

/*
 * No need to explicitly release drop action templates on port stop.
 * Drop action templates release with other action templates during
 * mlx5_dev_close -> flow_hw_resource_release -> flow_hw_actions_template_destroy
 */
static void
flow_hw_action_template_drop_release(struct rte_eth_dev *dev)
{
	int i;
	struct mlx5_priv *priv = dev->data->dev_private;

	for (i = 0; i < MLX5DR_TABLE_TYPE_MAX; i++) {
		if (!priv->action_template_drop[i])
			continue;
		flow_hw_actions_template_destroy(dev,
						 priv->action_template_drop[i],
						 NULL);
		priv->action_template_drop[i] = NULL;
	}
}

static int
flow_hw_action_template_drop_init(struct rte_eth_dev *dev,
			  struct rte_flow_error *error)
{
	uint32_t i, from, to;
	const struct rte_flow_action drop[2] = {
		[0] = { .type = RTE_FLOW_ACTION_TYPE_DROP },
		[1] = { .type = RTE_FLOW_ACTION_TYPE_END },
	};
	const struct rte_flow_action *actions = drop;
	const struct rte_flow_action *masks = drop;
	const struct rte_flow_actions_template_attr attr[MLX5DR_TABLE_TYPE_MAX] = {
		[MLX5DR_TABLE_TYPE_NIC_RX] = { .ingress = 1 },
		[MLX5DR_TABLE_TYPE_NIC_TX] = { .egress = 1 },
		[MLX5DR_TABLE_TYPE_FDB] = { .transfer = 1 },
		[MLX5DR_TABLE_TYPE_FDB_RX] = { .transfer = 1 },
		[MLX5DR_TABLE_TYPE_FDB_TX] = { .transfer = 1 },
		[MLX5DR_TABLE_TYPE_FDB_UNIFIED] = { .transfer = 1 },
	};
	struct mlx5_priv *priv = dev->data->dev_private;

	from = MLX5DR_TABLE_TYPE_NIC_RX;
	to = MLX5DR_TABLE_TYPE_NIC_TX;
	for (i = from; i <= to; i++) {
		priv->action_template_drop[i] =
			flow_hw_actions_template_create(dev, &attr[i], actions, masks, error);
		if (!priv->action_template_drop[i])
			return -1;
	}

	if (!(priv->sh->config.dv_esw_en && priv->master))
		return 0;

	from = MLX5DR_TABLE_TYPE_FDB;
	to = is_unified_fdb(priv) ? MLX5DR_TABLE_TYPE_FDB_UNIFIED : MLX5DR_TABLE_TYPE_FDB;
	for (i = from; i <= to; i++) {
		priv->action_template_drop[i] =
			flow_hw_actions_template_create(dev, &attr[i], actions, masks, error);
		if (!priv->action_template_drop[i])
			return -1;
	}
	return 0;
}

static void
__flow_hw_resource_release(struct rte_eth_dev *dev, bool ctx_close)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_template_table *tbl, *temp_tbl;
	struct rte_flow_pattern_template *it, *temp_it;
	struct rte_flow_actions_template *at, *temp_at;
	struct mlx5_flow_group *grp, *temp_grp;
	uint32_t i;

	flow_hw_rxq_flag_set(dev, false);
	flow_hw_flush_all_ctrl_flows(dev);
	flow_hw_cleanup_ctrl_fdb_tables(dev);
	flow_hw_cleanup_tx_repr_tagging(dev);
	flow_hw_cleanup_ctrl_rx_tables(dev);
	flow_hw_action_template_drop_release(dev);
	grp = LIST_FIRST(&priv->flow_hw_grp);
	while (grp) {
		temp_grp = LIST_NEXT(grp, next);
		claim_zero(flow_hw_group_unset_miss_group(dev, grp, NULL));
		grp = temp_grp;
	}
	tbl = LIST_FIRST(&priv->flow_hw_tbl_ongo);
	while (tbl) {
		temp_tbl = LIST_NEXT(tbl, next);
		claim_zero(flow_hw_table_destroy(dev, tbl, NULL));
		tbl = temp_tbl;
	}
	tbl = LIST_FIRST(&priv->flow_hw_tbl);
	while (tbl) {
		temp_tbl = LIST_NEXT(tbl, next);
		claim_zero(flow_hw_table_destroy(dev, tbl, NULL));
		tbl = temp_tbl;
	}
	it = LIST_FIRST(&priv->flow_hw_itt);
	while (it) {
		temp_it = LIST_NEXT(it, next);
		claim_zero(flow_hw_pattern_template_destroy(dev, it, NULL));
		it = temp_it;
	}
	at = LIST_FIRST(&priv->flow_hw_at);
	while (at) {
		temp_at = LIST_NEXT(at, next);
		claim_zero(flow_hw_actions_template_destroy(dev, at, NULL));
		at = temp_at;
	}
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		if (priv->hw_drop[i])
			mlx5dr_action_destroy(priv->hw_drop[i]);
		if (priv->hw_tag[i])
			mlx5dr_action_destroy(priv->hw_tag[i]);
	}
	if (priv->hw_def_miss)
		mlx5dr_action_destroy(priv->hw_def_miss);
	flow_hw_destroy_nat64_actions(priv);
	flow_hw_destroy_vlan(dev);
	flow_hw_destroy_send_to_kernel_action(priv);
	flow_hw_free_vport_actions(priv);
	if (priv->acts_ipool) {
		mlx5_ipool_destroy(priv->acts_ipool);
		priv->acts_ipool = NULL;
	}
	if (priv->hws_age_req)
		mlx5_hws_age_pool_destroy(priv);
	if (!priv->shared_host && priv->hws_cpool) {
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
	if (priv->hw_q) {
		for (i = 0; i < priv->nb_queue; i++) {
			struct mlx5_hw_q *hwq = &priv->hw_q[i];
			rte_ring_free(hwq->indir_iq);
			rte_ring_free(hwq->indir_cq);
			rte_ring_free(hwq->flow_transfer_pending);
			rte_ring_free(hwq->flow_transfer_completed);
		}
		mlx5_free(priv->hw_q);
		priv->hw_q = NULL;
	}
	if (ctx_close) {
		if (priv->dr_ctx) {
			claim_zero(mlx5dr_context_close(priv->dr_ctx));
			priv->dr_ctx = NULL;
		}
	}
	if (priv->shared_host) {
		struct mlx5_priv *host_priv = priv->shared_host->data->dev_private;
		rte_atomic_fetch_sub_explicit(&host_priv->shared_refcnt, 1,
				rte_memory_order_relaxed);
		priv->shared_host = NULL;
	}
	if (priv->hw_attr) {
		mlx5_free(priv->hw_attr);
		priv->hw_attr = NULL;
	}
	priv->nb_queue = 0;
}

static __rte_always_inline struct rte_ring *
mlx5_hwq_ring_create(uint16_t port_id, uint32_t queue, uint32_t size, const char *str)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];

	snprintf(mz_name, sizeof(mz_name), "port_%u_%s_%u", port_id, str, queue);
	return rte_ring_create(mz_name, size, SOCKET_ID_ANY,
			       RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ);
}

static int
flow_hw_validate_attributes(const struct rte_flow_port_attr *port_attr,
			    uint16_t nb_queue,
			    const struct rte_flow_queue_attr *queue_attr[],
			    bool nt_mode, struct rte_flow_error *error)
{
	uint32_t size;
	unsigned int i;

	if (port_attr == NULL)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Port attributes must be non-NULL");

	if (nb_queue == 0 && !nt_mode)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "At least one flow queue is required");

	if (queue_attr == NULL)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Queue attributes must be non-NULL");

	size = queue_attr[0]->size;
	for (i = 1; i < nb_queue; ++i) {
		if (queue_attr[i]->size != size)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL,
						  "All flow queues must have the same size");
	}

	return 0;
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
 * @param[in] nt_mode
 *   Non-template mode.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
__flow_hw_configure(struct rte_eth_dev *dev,
		  const struct rte_flow_port_attr *port_attr,
		  uint16_t nb_queue,
		  const struct rte_flow_queue_attr *queue_attr[],
		  bool nt_mode,
		  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_priv *host_priv = NULL;
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
	bool unified_fdb = is_unified_fdb(priv);
	int ret = 0;
	uint32_t action_flags;
	bool strict_queue = false;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	if (mlx5dr_rule_get_handle_size() != MLX5_DR_RULE_SIZE) {
		rte_errno = EINVAL;
		goto err;
	}
	if (flow_hw_validate_attributes(port_attr, nb_queue, queue_attr, nt_mode, error))
		return -rte_errno;
	/*
	 * Calling rte_flow_configure() again is allowed if
	 * provided configuration matches the initially provided one,
	 * or previous configuration was default non template one.
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
		/* If previous configuration was not default non template mode config. */
		if (!priv->hw_attr->nt_mode) {
			if (flow_hw_compare_config(priv->hw_attr, port_attr, nb_queue, queue_attr))
				return 0;
			else
				return rte_flow_error_set(error, ENOTSUP,
							RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
							"Changing HWS configuration attributes "
							"is not supported");
		}
		/* Reconfiguration, need to release all resources from previous allocation. */
		__flow_hw_resource_release(dev, true);
	}
	priv->hw_attr = flow_hw_alloc_copy_config(port_attr, nb_queue, queue_attr, nt_mode, error);
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

	memcpy(_queue_attr, queue_attr, sizeof(void *) * nb_queue);
	_queue_attr[nb_queue] = &ctrl_queue_attr;
	priv->acts_ipool = mlx5_ipool_create(&cfg);
	if (!priv->acts_ipool)
		goto err;
	/* Allocate the queue job descriptor LIFO. */
	mem_size = sizeof(priv->hw_q[0]) * nb_q_updated;
	for (i = 0; i < nb_q_updated; i++) {
		mem_size += (sizeof(struct mlx5_hw_q_job *) +
			     sizeof(struct mlx5_hw_q_job)) * _queue_attr[i]->size;
	}
	priv->hw_q = mlx5_malloc(MLX5_MEM_ZERO, mem_size,
				 64, SOCKET_ID_ANY);
	if (!priv->hw_q) {
		rte_errno = ENOMEM;
		goto err;
	}
	for (i = 0; i < nb_q_updated; i++) {
		priv->hw_q[i].job_idx = _queue_attr[i]->size;
		priv->hw_q[i].size = _queue_attr[i]->size;
		priv->hw_q[i].ongoing_flow_ops = 0;
		if (i == 0)
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)
					    &priv->hw_q[nb_q_updated];
		else
			priv->hw_q[i].job = (struct mlx5_hw_q_job **)&job[_queue_attr[i - 1]->size];
		job = (struct mlx5_hw_q_job *)
		      &priv->hw_q[i].job[_queue_attr[i]->size];
		for (j = 0; j < _queue_attr[i]->size; j++)
			priv->hw_q[i].job[j] = &job[j];
		/* Notice ring name length is limited. */
		priv->hw_q[i].indir_cq = mlx5_hwq_ring_create
			(dev->data->port_id, i, _queue_attr[i]->size, "indir_act_cq");
		if (!priv->hw_q[i].indir_cq)
			goto err;
		priv->hw_q[i].indir_iq = mlx5_hwq_ring_create
			(dev->data->port_id, i, _queue_attr[i]->size, "indir_act_iq");
		if (!priv->hw_q[i].indir_iq)
			goto err;
		priv->hw_q[i].flow_transfer_pending = mlx5_hwq_ring_create
			(dev->data->port_id, i, _queue_attr[i]->size, "tx_pending");
		if (!priv->hw_q[i].flow_transfer_pending)
			goto err;
		priv->hw_q[i].flow_transfer_completed = mlx5_hwq_ring_create
			(dev->data->port_id, i, _queue_attr[i]->size, "tx_done");
		if (!priv->hw_q[i].flow_transfer_completed)
			goto err;
	}
	dr_ctx_attr.pd = priv->sh->cdev->pd;
	dr_ctx_attr.queues = nb_q_updated;
	/* Assign initial value of STC numbers for representors. */
	if (priv->representor)
		dr_ctx_attr.initial_log_stc_memory = MLX5_REPR_STC_MEMORY_LOG;
	/* Queue size should all be the same. Take the first one. */
	dr_ctx_attr.queue_size = _queue_attr[0]->size;
	if (port_attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
		struct rte_eth_dev *host_dev = NULL;
		uint16_t port_id;

		MLX5_ASSERT(rte_eth_dev_is_valid_port(port_attr->host_port_id));
		if (is_proxy) {
			DRV_LOG(ERR, "cross vHCA shared mode not supported "
				"for E-Switch confgiurations");
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
		rte_atomic_fetch_add_explicit(&host_priv->shared_refcnt, 1,
				rte_memory_order_relaxed);
	}
	/* Set backward compatibale mode to support non template RTE FLOW API.*/
	dr_ctx_attr.bwc = true;
	priv->dr_ctx = mlx5dr_context_open(priv->sh->cdev->ctx, &dr_ctx_attr);
	/* rte_errno has been updated by HWS layer. */
	if (!priv->dr_ctx)
		goto err;
	priv->nb_queue = nb_q_updated;
	ret = flow_hw_action_template_drop_init(dev, error);
	if (ret)
		goto err;
	ret = flow_hw_create_ctrl_rx_tables(dev);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				   "Failed to set up Rx control flow templates");
		goto err;
	}
	/* Initialize quotas */
	if (port_attr->nb_quotas || (host_priv && host_priv->quota_ctx.devx_obj)) {
		ret = mlx5_flow_quota_init(dev, port_attr->nb_quotas);
		if (ret) {
			rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					   "Failed to initialize quota.");
			goto err;
		}
	}
	/* Initialize meter library*/
	if (port_attr->nb_meters || (host_priv && host_priv->hws_mpool))
		if (mlx5_flow_meter_init(dev, port_attr->nb_meters, 0, 0, nb_q_updated))
			goto err;
	/* Add global actions. */
	for (i = 0; i < MLX5_HW_ACTION_FLAG_MAX; i++) {
		uint32_t act_flags = 0;
		uint32_t tag_flags = mlx5_hw_act_flag[i][0];

		act_flags = mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_NIC_RX] |
			    mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_NIC_TX];
		if (is_proxy) {
			/* Tag action is valid only in FDB_Rx domain. */
			if (unified_fdb) {
				act_flags |=
					(mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB_RX] |
					 mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB_TX] |
					 mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB_UNIFIED]);
				if (i == MLX5_HW_ACTION_FLAG_NONE_ROOT)
					tag_flags |= mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB_RX];
			} else {
				act_flags |= mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB];
				if (i == MLX5_HW_ACTION_FLAG_NONE_ROOT)
					tag_flags |= mlx5_hw_act_flag[i][MLX5DR_TABLE_TYPE_FDB];
			}
		}
		priv->hw_drop[i] = mlx5dr_action_create_dest_drop(priv->dr_ctx, act_flags);
		if (!priv->hw_drop[i])
			goto err;
		priv->hw_tag[i] = mlx5dr_action_create_tag
			(priv->dr_ctx, tag_flags);
		if (!priv->hw_tag[i])
			goto err;
	}
	if (priv->sh->config.dv_esw_en && priv->sh->config.repr_matching) {
		ret = flow_hw_setup_tx_repr_tagging(dev, error);
		if (ret)
			goto err;
	}
	/*
	 * DEFAULT_MISS action have different behaviors in different domains.
	 * In FDB, it will steering the packets to the E-switch manager.
	 * In NIC Rx root, it will steering the packet to the kernel driver stack.
	 * An action with all bits set in the flag can be created and the HWS
	 * layer will translate it properly when being used in different rules.
	 */
	action_flags = MLX5DR_ACTION_FLAG_ROOT_RX | MLX5DR_ACTION_FLAG_HWS_RX |
		       MLX5DR_ACTION_FLAG_ROOT_TX | MLX5DR_ACTION_FLAG_HWS_TX;
	if (is_proxy) {
		if (unified_fdb)
			action_flags |=
				(MLX5DR_ACTION_FLAG_ROOT_FDB |
				 MLX5DR_ACTION_FLAG_HWS_FDB_RX |
				 MLX5DR_ACTION_FLAG_HWS_FDB_TX |
				 MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED);
		else
			action_flags |=
				(MLX5DR_ACTION_FLAG_ROOT_FDB |
				 MLX5DR_ACTION_FLAG_HWS_FDB);
	}
	priv->hw_def_miss = mlx5dr_action_create_default_miss(priv->dr_ctx, action_flags);
	if (!priv->hw_def_miss)
		goto err;
	if (is_proxy) {
		ret = flow_hw_create_vport_actions(priv);
		if (ret) {
			rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL, "Failed to create vport actions.");
			goto err;
		}
		ret = flow_hw_create_ctrl_tables(dev, error);
		if (ret) {
			rte_errno = -ret;
			goto err;
		}
	}
	if (!priv->shared_host)
		flow_hw_create_send_to_kernel_actions(priv, is_proxy);
	if (port_attr->nb_conn_tracks || (host_priv && host_priv->hws_ctpool)) {
		if (mlx5_flow_ct_init(dev, port_attr->nb_conn_tracks, nb_q_updated))
			goto err;
	}
	if (port_attr->nb_counters || (host_priv && host_priv->hws_cpool)) {
		struct mlx5_hws_cnt_pool *hws_cpool = host_priv ? host_priv->hws_cpool : NULL;

		ret = mlx5_hws_cnt_pool_create(dev, port_attr->nb_counters,
					       nb_queue, hws_cpool, error);
		if (ret)
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
		if (port_attr->flags & RTE_FLOW_PORT_FLAG_SHARE_INDIRECT) {
			DRV_LOG(ERR, "Aging is not supported "
				"in cross vHCA sharing mode");
			ret = -ENOTSUP;
			goto err;
		}
		strict_queue = !!(port_attr->flags & RTE_FLOW_PORT_FLAG_STRICT_QUEUE);
		ret = mlx5_hws_age_pool_init(dev, port_attr->nb_aging_objects,
						nb_queue, strict_queue);
		if (ret < 0)
			goto err;
	}
	ret = flow_hw_create_vlan(dev);
	if (ret) {
		rte_flow_error_set(error, -ret, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "Failed to VLAN actions.");
		goto err;
	}
	if (flow_hw_should_create_nat64_actions(priv)) {
		if (flow_hw_create_nat64_actions(priv, error))
			goto err;
	} else {
		DRV_LOG(WARNING, "Cannot create NAT64 action on port %u, "
			"please check the FW version. NAT64 will not be supported.",
			dev->data->port_id);
	}
	if (_queue_attr)
		mlx5_free(_queue_attr);
	if (port_attr->flags & RTE_FLOW_PORT_FLAG_STRICT_QUEUE)
		priv->hws_strict_queue = 1;
	dev->flow_fp_ops = &mlx5_flow_hw_fp_ops;
	return 0;
err:
	__flow_hw_resource_release(dev, true);
	if (_queue_attr)
		mlx5_free(_queue_attr);
	/* Do not overwrite the internal errno information. */
	if (ret && error->type != RTE_FLOW_ERROR_TYPE_NONE)
		return ret;
	return rte_flow_error_set(error, rte_errno,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				  "fail to configure port");
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
	struct rte_flow_error shadow_error = {0, };

	if (!error)
		error = &shadow_error;
	return __flow_hw_configure(dev, port_attr, nb_queue, queue_attr, false, error);
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

	if (!priv->dr_ctx)
		return;
	__flow_hw_resource_release(dev, false);
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
	info->is_wire = mlx5_is_port_on_mpesw_device(priv) ? priv->mpesw_uplink : priv->master;
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

static int
flow_hw_conntrack_destroy(struct rte_eth_dev *dev,
			  uint32_t idx,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_ct_pool *pool = priv->hws_ctpool;
	struct mlx5_aso_ct_action *ct;

	if (priv->shared_host)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"CT destruction is not allowed to guest port");
	ct = mlx5_ipool_get(pool->cts, idx);
	if (!ct) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Invalid CT destruction index");
	}
	rte_atomic_store_explicit(&ct->state, ASO_CONNTRACK_FREE,
				 rte_memory_order_relaxed);
	mlx5_ipool_free(pool->cts, idx);
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

	if (priv->shared_host)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"CT query is not allowed to guest port");
	ct = mlx5_ipool_get(pool->cts, idx);
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
	int ret = 0;

	if (priv->shared_host)
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"CT update is not allowed to guest port");
	ct = mlx5_ipool_get(pool->cts, idx);
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

	if (priv->shared_host) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"CT create is not allowed to guest port");
		return NULL;
	}
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
	return MLX5_INDIRECT_ACT_HWS_CT_GEN_IDX(ct_idx);
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
		if (!priv->hws_age_req) {
			if (flow_hw_allocate_actions(dev, MLX5_FLOW_ACTION_AGE,
						     error))
				return rte_flow_error_set
					(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL, "aging pool not initialized");
		}
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
		if (!priv->hws_cpool) {
			if (flow_hw_allocate_actions(dev, MLX5_FLOW_ACTION_COUNT,
						     error))
				return rte_flow_error_set
					(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL, "counters pool not initialized");
		}
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		if (priv->hws_ctpool == NULL) {
			if (flow_hw_allocate_actions(dev, MLX5_FLOW_ACTION_CT,
						     error))
				return rte_flow_error_set
					(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					 NULL, "CT pool not initialized");
		}
		return mlx5_validate_action_ct(dev, action->conf, error);
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		return flow_hw_validate_action_meter_mark(dev, action, true, error);
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
flow_hw_action_job_init(struct mlx5_priv *priv, uint32_t queue,
			const struct rte_flow_action_handle *handle,
			void *user_data, void *query_data,
			enum mlx5_hw_job_type type,
			enum mlx5_hw_indirect_type indirect_type,
			struct rte_flow_error *error)
{
	struct mlx5_hw_q_job *job;

	if (queue == MLX5_HW_INV_QUEUE)
		queue = CTRL_QUEUE_ID(priv);
	job = flow_hw_job_get(priv, queue);
	if (!job) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Action destroy failed due to queue full.");
		return NULL;
	}
	job->type = type;
	job->action = handle;
	job->user_data = user_data;
	job->query.user = query_data;
	job->indirect_type = indirect_type;
	return job;
}

struct mlx5_hw_q_job *
mlx5_flow_action_job_init(struct mlx5_priv *priv, uint32_t queue,
			  const struct rte_flow_action_handle *handle,
			  void *user_data, void *query_data,
			  enum mlx5_hw_job_type type,
			  struct rte_flow_error *error)
{
	return flow_hw_action_job_init(priv, queue, handle, user_data, query_data,
				       type, MLX5_HW_INDIRECT_TYPE_LEGACY, error);
}

static __rte_always_inline void
flow_hw_action_finalize(struct rte_eth_dev *dev, uint32_t queue,
			struct mlx5_hw_q_job *job,
			bool push, bool aso, bool status)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (queue == MLX5_HW_INV_QUEUE)
		queue = CTRL_QUEUE_ID(priv);
	if (likely(status)) {
		/* 1. add new job to a queue */
		if (!aso)
			rte_ring_enqueue(push ?
					 priv->hw_q[queue].indir_cq :
					 priv->hw_q[queue].indir_iq,
					 job);
		/* 2. send pending jobs */
		if (push)
			__flow_hw_push_action(dev, queue);
	} else {
		flow_hw_job_put(priv, job, queue);
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
	uint32_t age_idx;
	bool push = flow_hw_action_push(attr);
	bool aso = false;
	bool force_job = action->type == RTE_FLOW_ACTION_TYPE_METER_MARK;

	if (!mlx5_hw_ctx_validate(dev, error))
		return NULL;
	if (attr || force_job) {
		job = flow_hw_action_job_init(priv, queue, NULL, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_CREATE,
					      MLX5_HW_INDIRECT_TYPE_LEGACY, error);
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
		aso_mtr = flow_hw_meter_mark_alloc(dev, queue, action, job, push, error);
		if (!aso_mtr)
			break;
		handle = (void *)(uintptr_t)job->action;
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
	if (job && !force_job) {
		job->action = handle;
		flow_hw_action_finalize(dev, queue, job, push, aso,
					handle != NULL);
	}
	return handle;
}

static int
mlx5_flow_update_meter_mark(struct rte_eth_dev *dev, uint32_t queue,
			    const struct rte_flow_update_meter_mark *upd_meter_mark,
			    uint32_t idx, bool push,
			    struct mlx5_hw_q_job *job, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_aso_mtr_pool *pool = priv->hws_mpool;
	const struct rte_flow_action_meter_mark *meter_mark = &upd_meter_mark->meter_mark;
	struct mlx5_aso_mtr *aso_mtr = mlx5_ipool_get(pool->idx_pool, idx);
	struct mlx5_flow_meter_info *fm;

	if (!aso_mtr)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Invalid meter_mark update index");
	fm = &aso_mtr->fm;
	if (upd_meter_mark->profile_valid)
		fm->profile = (struct mlx5_flow_meter_profile *)
			(meter_mark->profile);
	if (upd_meter_mark->color_mode_valid)
		fm->color_aware = meter_mark->color_mode;
	if (upd_meter_mark->state_valid)
		fm->is_enable = meter_mark->state;
	aso_mtr->state = (queue == MLX5_HW_INV_QUEUE) ?
			 ASO_METER_WAIT : ASO_METER_WAIT_ASYNC;
	/* Update ASO flow meter by wqe. */
	if (mlx5_aso_meter_update_by_wqe(priv, queue,
					 aso_mtr, &priv->mtr_bulk, job, push))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Unable to update ASO meter WQE");
	/* Wait for ASO object completion. */
	if (queue == MLX5_HW_INV_QUEUE &&
	    mlx5_aso_mtr_wait(priv, aso_mtr, true))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Unable to wait for ASO meter CQE");
	return 0;
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
	const struct rte_flow_modify_conntrack *ct_conf =
		(const struct rte_flow_modify_conntrack *)update;
	struct mlx5_hw_q_job *job = NULL;
	uint32_t act_idx = (uint32_t)(uintptr_t)handle;
	uint32_t type = act_idx >> MLX5_INDIRECT_ACTION_TYPE_OFFSET;
	uint32_t idx = act_idx & ((1u << MLX5_INDIRECT_ACTION_TYPE_OFFSET) - 1);
	int ret = 0;
	bool push = flow_hw_action_push(attr);
	bool aso = false;
	bool force_job = type == MLX5_INDIRECT_ACTION_TYPE_METER_MARK;

	if (attr || force_job) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_UPDATE,
					      MLX5_HW_INDIRECT_TYPE_LEGACY, error);
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
		ret = flow_hw_conntrack_update(dev, queue, update, idx,
					       job, push, error);
		break;
	case MLX5_INDIRECT_ACTION_TYPE_METER_MARK:
		aso = true;
		ret = mlx5_flow_update_meter_mark(dev, queue, update, idx, push,
						  job, error);
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
	if (job && !force_job)
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
	bool force_job = type == MLX5_INDIRECT_ACTION_TYPE_METER_MARK;

	if (attr || force_job) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_DESTROY,
					      MLX5_HW_INDIRECT_TYPE_LEGACY, error);
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
		ret = flow_hw_conntrack_destroy(dev, idx, error);
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
		if (mlx5_aso_meter_update_by_wqe(priv, queue, aso_mtr,
						 &priv->mtr_bulk, job, push)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to update ASO meter WQE");
			break;
		}
		/* Wait for ASO object completion. */
		if (queue == MLX5_HW_INV_QUEUE &&
		    mlx5_aso_mtr_wait(priv, aso_mtr, true)) {
			ret = -EINVAL;
			rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Unable to wait for ASO meter CQE");
			break;
		}
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
	if (job && !force_job)
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
	switch (rte_atomic_load_explicit(&param->state, rte_memory_order_relaxed)) {
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
		resp->sec_since_last_hit = rte_atomic_load_explicit
				 (&param->sec_since_last_hit, rte_memory_order_relaxed);
	return 0;
}

static int
flow_hw_query(struct rte_eth_dev *dev, struct rte_flow *flow,
	      const struct rte_flow_action *actions, void *data,
	      struct rte_flow_error *error)
{
	int ret = -EINVAL;
	struct rte_flow_hw *hw_flow = (struct rte_flow_hw *)flow;
	struct rte_flow_hw_aux *aux;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			if (!(hw_flow->flags & MLX5_FLOW_HW_FLOW_FLAG_CNT_ID))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
							  "counter not defined in the rule");
			ret = flow_hw_query_counter(dev, hw_flow->cnt_id, data,
						    error);
			break;
		case RTE_FLOW_ACTION_TYPE_AGE:
			if (!(hw_flow->flags & MLX5_FLOW_HW_FLOW_FLAG_AGE_IDX))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
							  "age data not available");
			aux = mlx5_flow_hw_aux(dev->data->port_id, hw_flow);
			ret = flow_hw_query_age(dev, mlx5_flow_hw_aux_get_age_idx(hw_flow, aux),
						data, error);
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
	struct rte_flow_error shadow_error = {0, };

	if (!err)
		err = &shadow_error;
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
	uint32_t idx = MLX5_INDIRECT_ACTION_IDX_GET(handle);
	uint32_t age_idx = act_idx & MLX5_HWS_AGE_IDX_MASK;
	int ret;
	bool push = flow_hw_action_push(attr);
	bool aso = false;

	if (attr) {
		job = flow_hw_action_job_init(priv, queue, handle, user_data,
					      data, MLX5_HW_Q_JOB_TYPE_QUERY,
					      MLX5_HW_INDIRECT_TYPE_LEGACY, error);
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
		ret = flow_hw_conntrack_query(dev, queue, idx, data,
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
					      MLX5_HW_INDIRECT_TYPE_LEGACY, error);
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
	if (!priv->hws_age_req)
		return rte_flow_error_set(error, ENOENT,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "No aging initialized");
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
/**
 * Initialization function for non template API which calls
 * flow_hw_configure with default values.
 * Configure non queues cause 1 queue is configured by default for inner usage.
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[out] error
 *   Pointer to the error structure.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
flow_hw_init(struct rte_eth_dev *dev,
	     struct rte_flow_error *error)
{
	const struct rte_flow_port_attr port_attr = {0};
	const struct rte_flow_queue_attr queue_attr = {.size = MLX5_NT_DEFAULT_QUEUE_SIZE};
	const struct rte_flow_queue_attr *attr_list = &queue_attr;

	/**
	 * If user uses template and non template API:
	 * User will call flow_hw_configure and non template
	 * API will use the allocated actions.
	 * Init function will not call flow_hw_configure.
	 *
	 * If user uses only non template API's:
	 * Init function will call flow_hw_configure.
	 * It will not allocate memory for actions.
	 * When needed allocation, it will handle same as for SWS today,
	 * meaning using bulk allocations and resize as needed.
	 */
	/* Configure hws with default values. */
	DRV_LOG(DEBUG, "Apply default configuration, zero number of queues, inner control queue size is %u",
		MLX5_NT_DEFAULT_QUEUE_SIZE);
	return __flow_hw_configure(dev, &port_attr, 0, &attr_list, true, error);
}

static int flow_hw_prepare(struct rte_eth_dev *dev,
			   const struct rte_flow_action actions[] __rte_unused,
			   enum mlx5_flow_type type,
			   struct rte_flow_hw **flow,
			   struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t idx = 0;

	 /*
	  * Notice pool idx size = (sizeof(struct rte_flow_hw)
	  * + sizeof(struct rte_flow_nt2hws)) for HWS mode.
	  */
	*flow = mlx5_ipool_zmalloc(priv->flows[type], &idx);
	if (!(*flow))
		return rte_flow_error_set(error, ENOMEM,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			"cannot allocate flow memory");
	/* Allocating 2 structures in one pool slot, updating nt2hw pointer.*/
	(*flow)->nt2hws = (struct rte_flow_nt2hws *)
				((uintptr_t)(*flow) + sizeof(struct rte_flow_hw));
	(*flow)->idx = idx;
	(*flow)->nt2hws->flow_aux = mlx5_malloc(MLX5_MEM_ZERO, sizeof(struct rte_flow_hw_aux),
				    RTE_CACHE_LINE_SIZE, rte_dev_numa_node(dev->device));
	if (!(*flow)->nt2hws->flow_aux)
		return rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
				"cannot allocate flow aux memory");
	return 0;
}

static inline void
flow_hw_set_dv_fields(struct rte_flow_template_table_attr *table_attr, uint32_t fdb_unified_en,
		      bool *root, uint8_t *ft_type, uint64_t *flags)
{
	if (table_attr->flow_attr.transfer)
		*ft_type = MLX5DV_FLOW_TABLE_TYPE_FDB;
	else
		*ft_type = table_attr->flow_attr.egress ?
					MLX5DV_FLOW_TABLE_TYPE_NIC_TX :
					MLX5DV_FLOW_TABLE_TYPE_NIC_RX;
	uint32_t group = table_attr->flow_attr.group;
	*root = group ? 0 : 1;
	*flags = mlx5_hw_act_flag[!!group][get_mlx5dr_table_type(&table_attr->flow_attr,
								 table_attr->specialize,
								 fdb_unified_en)];
}

static int
flow_hw_modify_hdr_resource_register
			(struct rte_eth_dev *dev,
			 struct rte_flow_template_table *table,
			 struct mlx5_hw_actions *hw_acts,
			 struct rte_flow_hw *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	struct rte_flow_template_table_attr *table_attr = &table->cfg.attr;
	struct mlx5_flow_dv_modify_hdr_resource *dv_resource_ptr = NULL;
	union {
		struct mlx5_flow_dv_modify_hdr_resource dv_resource;
		uint8_t data[sizeof(struct mlx5_flow_dv_modify_hdr_resource) +
			     sizeof(struct mlx5_modification_cmd) * MLX5_MHDR_MAX_CMD];
	} dummy;
	int ret;

	if (hw_acts->mhdr) {
		dummy.dv_resource.actions_num = hw_acts->mhdr->mhdr_cmds_num;
		memcpy(dummy.dv_resource.actions, hw_acts->mhdr->mhdr_cmds,
			sizeof(struct mlx5_modification_cmd) * dummy.dv_resource.actions_num);
	} else {
		return 0;
	}
	flow_hw_set_dv_fields(table_attr, unified_fdb,
			      &dummy.dv_resource.root, &dummy.dv_resource.ft_type,
			      &dummy.dv_resource.flags);
	dummy.dv_resource.flags |= MLX5DR_ACTION_FLAG_SHARED;
	ret = __flow_modify_hdr_resource_register(dev, &dummy.dv_resource,
		&dv_resource_ptr, error);
	if (ret)
		return ret;
	MLX5_ASSERT(dv_resource_ptr);
	dev_flow->nt2hws->modify_hdr = dv_resource_ptr;
	/* keep action for the rule construction. */
	hw_acts->rule_acts[hw_acts->mhdr->pos].action = dv_resource_ptr->action;
	/* Bulk size is 1, so index is 1. */
	dev_flow->res_idx = 1;
	return 0;
}

static int
flow_hw_encap_decap_resource_register
			(struct rte_eth_dev *dev,
			 struct rte_flow_template_table *table,
			 struct mlx5_hw_actions *hw_acts,
			 struct rte_flow_hw *dev_flow,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	struct rte_flow_template_table_attr *table_attr = &table->cfg.attr;
	struct mlx5_flow_dv_encap_decap_resource *dv_resource_ptr = NULL;
	struct mlx5_flow_dv_encap_decap_resource dv_resource;
	struct mlx5_tbl_multi_pattern_ctx *mpctx = &table->mpctx;
	int ret;
	bool is_root;
	int ix;

	if (hw_acts->encap_decap)
		dv_resource.reformat_type = hw_acts->encap_decap->action_type;
	else
		return 0;
	flow_hw_set_dv_fields(table_attr, unified_fdb, &is_root, &dv_resource.ft_type,
			      &dv_resource.flags);
	ix = mlx5_bwc_multi_pattern_reformat_to_index((enum mlx5dr_action_type)
			dv_resource.reformat_type);
	if (ix < 0)
		return ix;
	if (hw_acts->encap_decap->shared) {
		dv_resource.size = hw_acts->encap_decap->data_size;
		MLX5_ASSERT(dv_resource.size <= MLX5_ENCAP_MAX_LEN);
		memcpy(&dv_resource.buf, hw_acts->encap_decap->data, dv_resource.size);
		dv_resource.flags |= MLX5DR_ACTION_FLAG_SHARED;
	} else {
		typeof(mpctx->reformat[0]) *reformat = mpctx->reformat + ix;
		if (!reformat->elements_num)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					NULL, "No reformat action exist in the table.");
		dv_resource.size = reformat->reformat_hdr->sz;
		MLX5_ASSERT(dv_resource.size <= MLX5_ENCAP_MAX_LEN);
		memcpy(&dv_resource.buf, reformat->reformat_hdr->data, dv_resource.size);
	}
	ret = __flow_encap_decap_resource_register(dev, &dv_resource, is_root,
		&dv_resource_ptr, error);
	if (ret)
		return ret;
	MLX5_ASSERT(dv_resource_ptr);
	dev_flow->nt2hws->rix_encap_decap = dv_resource_ptr->idx;
	/* keep action for the rule construction. */
	if (hw_acts->encap_decap->shared)
		hw_acts->rule_acts[hw_acts->encap_decap_pos].action = dv_resource_ptr->action;
	else
		mpctx->segments[0].reformat_action[ix] = dv_resource_ptr->action;
	/* Bulk size is 1, so index is 1. */
	dev_flow->res_idx = 1;
	return 0;
}

static enum rte_flow_action_type
flow_nta_get_indirect_action_type(const struct rte_flow_action *action)
{
	switch (MLX5_INDIRECT_ACTION_TYPE_GET(action->conf)) {
	case MLX5_INDIRECT_ACTION_TYPE_RSS:
		return RTE_FLOW_ACTION_TYPE_RSS;
	case MLX5_INDIRECT_ACTION_TYPE_AGE:
		return RTE_FLOW_ACTION_TYPE_AGE;
	case MLX5_INDIRECT_ACTION_TYPE_COUNT:
		return RTE_FLOW_ACTION_TYPE_COUNT;
	case MLX5_INDIRECT_ACTION_TYPE_CT:
		return RTE_FLOW_ACTION_TYPE_CONNTRACK;
	default:
		break;
	}
	return RTE_FLOW_ACTION_TYPE_END;
}

static void
flow_nta_set_mh_mask_conf(const struct rte_flow_action_modify_field *action_conf,
			  struct rte_flow_action_modify_field *mask_conf)
{
	memset(mask_conf, 0xff, sizeof(*mask_conf));
	mask_conf->operation = action_conf->operation;
	mask_conf->dst.field = action_conf->dst.field;
	mask_conf->src.field = action_conf->src.field;
}

union actions_conf {
	struct rte_flow_action_modify_field modify_field;
	struct rte_flow_action_raw_encap raw_encap;
	struct rte_flow_action_vxlan_encap vxlan_encap;
	struct rte_flow_action_nvgre_encap nvgre_encap;
};

static int
flow_nta_build_template_mask(const struct rte_flow_action actions[],
			     struct rte_flow_action masks[MLX5_HW_MAX_ACTS],
			     union actions_conf mask_conf[MLX5_HW_MAX_ACTS])
{
	int i;

	for (i = 0; i == 0 || actions[i - 1].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		const struct rte_flow_action *action = &actions[i];
		struct rte_flow_action *mask = &masks[i];
		union actions_conf *conf = &mask_conf[i];

		mask->type = action->type;
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			mask->type = flow_nta_get_indirect_action_type(action);
			if (!mask->type)
				return -EINVAL;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			flow_nta_set_mh_mask_conf(action->conf, (void *)conf);
			mask->conf = conf;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			/* This mask will set this action as shared. */
			memset(conf, 0xff, sizeof(struct rte_flow_action_raw_encap));
			mask->conf = conf;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			/* This mask will set this action as shared. */
			conf->vxlan_encap.definition =
				((const struct rte_flow_action_vxlan_encap *)
					action->conf)->definition;
			mask->conf = conf;
			break;
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			/* This mask will set this action as shared. */
			conf->nvgre_encap.definition =
				((const struct rte_flow_action_nvgre_encap *)
					action->conf)->definition;
			mask->conf = conf;
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			memset(conf, 0xff, sizeof(struct rte_flow_action_of_set_vlan_vid));
			mask->conf = conf;
			break;
		default:
			break;
		}
	}
	return 0;
#undef NTA_CHECK_CONF_BUF_SIZE
}

static int
flow_hw_translate_flow_actions(struct rte_eth_dev *dev,
			       const struct rte_flow_attr *attr,
			       const struct rte_flow_action actions[],
			       struct rte_flow_hw *flow,
			       struct mlx5_flow_hw_action_params *ap,
			       struct mlx5_hw_actions *hw_acts,
			       uint64_t item_flags, uint64_t action_flags,
			       bool external,
			       struct rte_flow_error *error)
{
	int ret = 0;
	uint32_t src_group = 0;
	enum mlx5dr_table_type table_type;
	struct mlx5_flow_group grp;
	struct rte_flow_actions_template *at = NULL;
	struct rte_flow_actions_template_attr template_attr = {
		.egress = attr->egress,
		.ingress = attr->ingress,
		.transfer = attr->transfer,
	};
	struct rte_flow_action masks[MLX5_HW_MAX_ACTS];
	union actions_conf mask_conf[MLX5_HW_MAX_ACTS];

	RTE_SET_USED(action_flags);
	memset(masks, 0, sizeof(masks));
	memset(mask_conf, 0, sizeof(mask_conf));
	/* Only set the needed fields explicitly. */
	struct mlx5_flow_workspace *wks = mlx5_flow_push_thread_workspace();
	struct rte_flow_template_table *table;

	/*
	 * Notice All direct actions will be unmasked,
	 * except for modify header and encap,
	 * and therefore will be parsed as part of action construct.
	 * Modify header is always shared in HWS,
	 * encap is masked such that it will be treated as shared.
	 * shared actions will be parsed as part of template translation
	 * and not during action construct.
	 */
	if (!wks)
		return rte_flow_error_set(error, ENOMEM,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "failed to push flow workspace");
	table = wks->table;
	flow_nta_build_template_mask(actions, masks, mask_conf);
	/* The group in the attribute translation was done in advance. */
	ret = __translate_group(dev, attr, external, attr->group, &src_group, error);
	if (ret)
		return ret;
	if (attr->transfer)
		table_type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		table_type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		table_type = MLX5DR_TABLE_TYPE_NIC_RX;
	at = __flow_hw_actions_template_create(dev, &template_attr, actions, masks, true, error);
	if (!at) {
		ret = -rte_errno;
		goto end;
	}
	grp.group_id = src_group;
	table->grp = &grp;
	table->type = table_type;
	table->cfg.external = external;
	table->nb_action_templates = 1;
	memcpy(&table->cfg.attr.flow_attr, attr, sizeof(*attr));
	table->cfg.attr.flow_attr.group = src_group;
	table->ats[0].action_template = at;
	ret = __flow_hw_translate_actions_template(dev, &table->cfg, hw_acts, at,
						   &table->mpctx, true, error);
	if (ret)
		goto end;
	/* handle bulk actions register. */
	ret = flow_hw_encap_decap_resource_register(dev, table, hw_acts, flow, error);
	if (ret)
		goto end;
	ret = flow_hw_modify_hdr_resource_register(dev, table, hw_acts, flow, error);
	if (ret)
		goto end;
	table->ats[0].acts = *hw_acts;
	ret = flow_hw_actions_construct(dev, flow, ap,
					&table->ats[0], item_flags, table,
					actions, hw_acts->rule_acts, 0, error);
	if (ret)
		goto end;
	goto end;
end:
	if (ret)
		/* Make sure that there is no garbage in the actions. */
		__flow_hw_action_template_destroy(dev, hw_acts);
	else
		__flow_hw_act_data_flush(dev, hw_acts);
	if (at)
		mlx5_free(at);
	mlx5_flow_pop_thread_workspace();
	return ret;
}

static int
flow_hw_unregister_matcher(struct rte_eth_dev *dev,
			   struct mlx5_flow_dv_matcher *matcher)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_group *group = matcher->group;
	int ret = 0;

	if (group) {
		if (matcher->matcher_object)
			ret |= mlx5_list_unregister(group->matchers, &matcher->entry);
		ret |= mlx5_hlist_unregister(priv->sh->groups, &group->entry);
	}
	return ret;
}

static int flow_hw_register_matcher(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item items[],
				    bool external,
				    struct rte_flow_hw *flow,
				    struct mlx5_flow_dv_matcher *matcher,
				    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_error sub_error = {
		.type = RTE_FLOW_ERROR_TYPE_NONE,
		.cause = NULL,
		.message = NULL,
	};
	struct rte_flow_attr flow_attr = *attr;
	uint32_t specialize = 0; /* No unified FDB. */
	struct mlx5_flow_cb_ctx ctx = {
		.dev = dev,
		.error = &sub_error,
		.data = &flow_attr,
		.data2 = &specialize,
	};
	void *items_ptr = &items;
	struct mlx5_flow_cb_ctx matcher_ctx = {
		.error = &sub_error,
		.data = matcher,
		.data2 = items_ptr,
	};
	struct mlx5_list_entry *group_entry = NULL;
	struct mlx5_list_entry *matcher_entry = NULL;
	struct mlx5_flow_dv_matcher *resource;
	struct mlx5_list *matchers_list;
	struct mlx5_flow_group *flow_group;
	int ret;


	matcher->crc = rte_raw_cksum((const void *)matcher->mask.buf,
				    matcher->mask.size);
	matcher->priority = attr->priority;
	ret = __translate_group(dev, attr, external, attr->group, &flow_attr.group, error);
	if (ret)
		return ret;

	/* Register the flow group. */
	group_entry = mlx5_hlist_register(priv->sh->groups, flow_attr.group, &ctx);
	if (!group_entry)
		goto error;
	flow_group = container_of(group_entry, struct mlx5_flow_group, entry);

	matchers_list = flow_group->matchers;
	matcher->group = flow_group;
	matcher_entry = mlx5_list_register(matchers_list, &matcher_ctx);
	if (!matcher_entry)
		goto error;
	resource = container_of(matcher_entry, typeof(*resource), entry);
	flow->nt2hws->matcher = resource;
	return 0;

error:
	if (group_entry)
		mlx5_hlist_unregister(priv->sh->groups, group_entry);
	if (error) {
		if (sub_error.type != RTE_FLOW_ERROR_TYPE_NONE)
			rte_memcpy(error, &sub_error, sizeof(sub_error));
	}
	return rte_flow_error_set(error, rte_errno,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "fail to register matcher");
}

static int
flow_hw_allocate_actions(struct rte_eth_dev *dev,
			 uint64_t action_flags,
			 struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	uint obj_num;

	error->type = RTE_FLOW_ERROR_TYPE_NONE;
	if (action_flags & MLX5_FLOW_ACTION_AGE) {
		/* If no age objects were previously allocated. */
		if (!priv->hws_age_req) {
			/* If no counters were previously allocated. */
			if (!priv->hws_cpool) {
				obj_num = MLX5_CNT_NT_MAX(priv);
				ret = mlx5_hws_cnt_pool_create(dev, obj_num,
							       priv->nb_queue,
							       NULL, error);
				if (ret)
					goto err;
			}
			/* Allocate same number of counters. */
			ret = mlx5_hws_age_pool_init(dev, priv->hws_cpool->cfg.request_num,
						     priv->nb_queue, false);
			if (ret)
				goto err;
		}
	}
	if (action_flags & MLX5_FLOW_ACTION_COUNT) {
		/* If no counters were previously allocated. */
		if (!priv->hws_cpool) {
			obj_num = MLX5_CNT_NT_MAX(priv);
			ret = mlx5_hws_cnt_pool_create(dev, obj_num,
						       priv->nb_queue, NULL,
						       error);
			if (ret)
				goto err;
		}
	}
	if (action_flags & MLX5_FLOW_ACTION_CT) {
		/* If no CT were previously allocated. */
		if (!priv->hws_ctpool) {
			obj_num = MLX5_CT_NT_MAX(priv);
			ret = mlx5_flow_ct_init(dev, obj_num, priv->nb_queue);
			if (ret)
				goto err;
		}
	}
	if (action_flags & MLX5_FLOW_ACTION_METER) {
		/* If no meters were previously allocated. */
		if (!priv->hws_mpool) {
			obj_num = MLX5_MTR_NT_MAX(priv);
			ret = mlx5_flow_meter_init(dev, obj_num, 0, 0,
						   priv->nb_queue);
			if (ret)
				goto err;
		}
	}
	return 0;
err:
	if (ret && error->type != RTE_FLOW_ERROR_TYPE_NONE)
		return ret;
	return rte_flow_error_set(error, ret,
				  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				  NULL, "fail to allocate actions");
}

static int flow_hw_apply(const struct rte_flow_item items[],
			 struct mlx5dr_rule_action rule_actions[],
			 struct rte_flow_hw *flow,
			 struct rte_flow_error *error)
{
	struct mlx5dr_bwc_rule *rule = NULL;

	rule = mlx5dr_bwc_rule_create((struct mlx5dr_bwc_matcher *)
		flow->nt2hws->matcher->matcher_object,
		items, rule_actions);
	flow->nt2hws->nt_rule = rule;
	if (!rule) {
		return rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
			NULL, "fail to create rte flow");
	}
	return 0;
}

#ifdef HAVE_MLX5_HWS_SUPPORT
/**
 * Create a flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] type
 *   Flow type.
 * @param[in] attr
 *   Flow rule attributes.
 * @param[in] items
 *   Pattern specification (list terminated by the END pattern item).
 * @param[in] actions
 *   Associated actions (list terminated by the END action).
 * @param[in] external
 *   This flow rule is created by request external to PMD.
 * @param[out] flow
 *   Flow pointer
 * @param[out] error
 *   Perform verbose error reporting if not NULL.
 *
 * @return
 *   0 on success, negative errno value otherwise and rte_errno set.
 */
int
flow_hw_create_flow(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		    const struct rte_flow_attr *attr,
		    const struct rte_flow_item items[],
		    const struct rte_flow_action actions[],
		    uint64_t item_flags, uint64_t action_flags, bool external,
		    struct rte_flow_hw **flow, struct rte_flow_error *error)
{
	int ret;
	struct mlx5_hw_actions hw_act = { { NULL } };
	struct mlx5_flow_hw_action_params ap;
	struct mlx5_flow_dv_matcher matcher = {
		.mask = {
			.size = sizeof(matcher.mask.buf),
		},
	};
	uint32_t tbl_type;

	struct mlx5_flow_attr flow_attr = {
		.port_id = dev->data->port_id,
		.group = attr->group,
		.priority = attr->priority,
		.rss_level = 0,
		.act_flags = action_flags,
		.tbl_type = 0,
	};

	if (attr->transfer)
		tbl_type = MLX5DR_TABLE_TYPE_FDB;
	else if (attr->egress)
		tbl_type = MLX5DR_TABLE_TYPE_NIC_TX;
	else
		tbl_type = MLX5DR_TABLE_TYPE_NIC_RX;
	flow_attr.tbl_type = tbl_type;

	/* Allocate needed memory. */
	ret = flow_hw_prepare(dev, actions, type, flow, error);
	if (ret)
		goto error;

	/* TODO TBD flow_hw_handle_tunnel_offload(). */
	(*flow)->nt_rule = true;
	(*flow)->nt2hws->matcher = &matcher;
	ret = __flow_dv_translate_items_hws(items, &flow_attr, &matcher.mask.buf,
					    MLX5_SET_MATCHER_HS_M, NULL,
					    NULL, true, error);

	if (ret)
		goto error;

	if (item_flags & MLX5_FLOW_LAYER_ECPRI && !mlx5_flex_parser_ecpri_exist(dev))
		if (mlx5_flex_parser_ecpri_alloc(dev)) {
			rte_flow_error_set(error, EIO,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   NULL,
					   "failed to create Flex parser "
					   "profile for ECPRI");
			goto error;
		}
	ret = flow_hw_register_matcher(dev, attr, items, external, *flow, &matcher, error);
	if (ret) {
		if (rte_errno == E2BIG)
			rte_flow_error_set(error, E2BIG, RTE_FLOW_ERROR_TYPE_ITEM, NULL,
				"flow pattern is too big");
		goto error;
	}

	/*
	 * ASO allocation – iterating on actions list to allocate missing resources.
	 * In the future when validate function in hws will be added,
	 * The output actions bit mask instead of
	 * looping on the actions array twice.
	 */
	ret = flow_hw_allocate_actions(dev, action_flags, error);
	if (ret)
		goto error;

	/* Note: the actions should be saved in the sub-flow rule itself for reference. */
	ret = flow_hw_translate_flow_actions(dev, attr, actions, *flow, &ap, &hw_act,
					item_flags, action_flags, external, error);
	if (ret)
		goto error;

	/*
	 * If the flow is external (from application) OR device is started,
	 * OR mreg discover, then apply immediately.
	 */
	if (external || dev->data->dev_started ||
	    (attr->group == MLX5_FLOW_MREG_CP_TABLE_GROUP &&
	     attr->priority == MLX5_FLOW_LOWEST_PRIO_INDICATOR)) {
		ret = flow_hw_apply(items, hw_act.rule_acts, *flow, error);
		if (ret)
			goto error;
	}
	ret = 0;
error:
	/*
	 * Release memory allocated.
	 * Cannot use __flow_hw_actions_release(dev, &hw_act);
	 * since it destroys the actions as well.
	 */
	if (hw_act.encap_decap)
		mlx5_free(hw_act.encap_decap);
	if (hw_act.push_remove)
		mlx5_free(hw_act.push_remove);
	if (hw_act.mhdr)
		mlx5_free(hw_act.mhdr);
	if (ret) {
		/* release after actual error */
		if ((*flow)->nt2hws && (*flow)->nt2hws->matcher)
			flow_hw_unregister_matcher(dev, (*flow)->nt2hws->matcher);
	}
	return ret;
}
#endif

void
flow_hw_destroy(struct rte_eth_dev *dev, struct rte_flow_hw *flow)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;

	if (!flow || !flow->nt2hws)
		return;

	if (flow->nt2hws->nt_rule) {
		ret = mlx5dr_bwc_rule_destroy(flow->nt2hws->nt_rule);
		if (ret)
			DRV_LOG(ERR, "bwc rule destroy failed");
	}
	flow->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_DESTROY;
	/* Notice this function does not handle shared/static actions. */
	hw_cmpl_flow_update_or_destroy(dev, flow, 0, NULL);

	/**
	 * TODO: TBD - Release tunnel related memory allocations(mlx5_flow_tunnel_free)
	 * – needed only if supporting tunnel offloads, notice update RX queue flags in SWS.
	 */

	 /**
	  * Notice matcher destroy will take place when matcher's list is destroyed
	  * , same as for DV.
	  */
	if (flow->nt2hws->flow_aux)
		mlx5_free(flow->nt2hws->flow_aux);

	if (flow->nt2hws->rix_encap_decap)
		flow_encap_decap_resource_release(dev, flow->nt2hws->rix_encap_decap);
	if (flow->nt2hws->modify_hdr) {
		MLX5_ASSERT(flow->nt2hws->modify_hdr->action);
		mlx5_hlist_unregister(priv->sh->modify_cmds,
				      &flow->nt2hws->modify_hdr->entry);
	}
	if (flow->nt2hws->matcher)
		flow_hw_unregister_matcher(dev, flow->nt2hws->matcher);
	if (flow->nt2hws->sample_release_ctx != NULL) {
		mlx5_nta_sample_mirror_entry_release(dev, flow->nt2hws->sample_release_ctx);
		flow->nt2hws->sample_release_ctx = NULL;
	}
}

#ifdef HAVE_MLX5_HWS_SUPPORT
/**
 * Destroy a flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] type
 *   Flow type.
 * @param[in] flow_addr
 *   Address of flow to destroy.
 */
void
flow_hw_list_destroy(struct rte_eth_dev *dev, enum mlx5_flow_type type,
		     uintptr_t flow_addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_hw *flow = (struct rte_flow_hw *)flow_addr;
	struct mlx5_nta_rss_flow_head head = { .slh_first = flow };

	if (!flow || !flow->nt2hws || flow->nt2hws->chaned_flow)
		return;
	mlx5_flow_nta_del_copy_action(dev, flow->nt2hws->rix_mreg_copy);
	while (!SLIST_EMPTY(&head)) {
		flow = SLIST_FIRST(&head);
		SLIST_REMOVE_HEAD(&head, nt2hws->next);
		flow_hw_destroy(dev, flow);
		/* Release flow memory by idx */
		mlx5_ipool_free(priv->flows[type], flow->idx);
	}
}
#endif

/**
 * Create a flow.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] type
 *   Flow type.
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
 *   A flow addr on success, 0 otherwise and rte_errno is set.
 */
static uintptr_t flow_hw_list_create(struct rte_eth_dev *dev,
				     enum mlx5_flow_type type,
				     const struct rte_flow_attr *attr,
				     const struct rte_flow_item items[],
				     const struct rte_flow_action actions[],
				     bool external,
				     struct rte_flow_error *error)
{
	int ret;
	int split;
	int encap_idx;
	uint32_t cpy_idx = 0;
	int actions_n = 0;
	struct rte_flow_hw *flow = NULL;
	struct rte_flow_hw *prfx_flow = NULL;
	const struct rte_flow_action *qrss = NULL;
	const struct rte_flow_action *mark = NULL;
	uint64_t item_flags = 0;
	uint64_t action_flags = mlx5_flow_hw_action_flags_get(actions, &qrss, &mark,
							 &encap_idx, &actions_n, error);
	struct mlx5_flow_hw_split_resource resource = {
		.suffix = {
			.attr = attr,
			.items = items,
			.actions = actions,
		},
	};
	struct rte_flow_error shadow_error = {0, };
	const struct rte_flow_pattern_template_attr pattern_template_attr = {
		.relaxed_matching = 0,
		.ingress = attr->ingress,
		.egress = attr->egress,
		.transfer = attr->transfer,
	};

	/* Validate application items only */
	ret = __flow_hw_pattern_validate(dev, &pattern_template_attr, items,
						&item_flags, true, error);
	if (ret < 0)
		return 0;

	RTE_SET_USED(encap_idx);
	if (!error)
		error = &shadow_error;
	split = mlx5_flow_nta_split_metadata(dev, attr, actions, qrss, action_flags,
					     actions_n, external, &resource, error);
	if (split < 0)
		return split;

	/* Update the metadata copy table - MLX5_FLOW_MREG_CP_TABLE_GROUP */
	if (((attr->ingress && attr->group != MLX5_FLOW_MREG_CP_TABLE_GROUP) ||
	     attr->transfer) && external) {
		ret = mlx5_flow_nta_update_copy_table(dev, &cpy_idx, mark,
						      action_flags, error);
		if (ret)
			goto free;
	}
	if (action_flags & MLX5_FLOW_ACTION_SAMPLE) {
		flow = mlx5_nta_sample_flow_list_create(dev, type, attr, items, actions,
							item_flags, action_flags, error);
		if (flow != NULL)
			return (uintptr_t)flow;
		goto free;
	}
	if (action_flags & MLX5_FLOW_ACTION_RSS) {
		const struct rte_flow_action_rss
			*rss_conf = flow_nta_locate_rss(dev, actions, error);
		flow = flow_nta_handle_rss(dev, attr, items, actions, rss_conf,
					   item_flags, action_flags, external,
					   type, error);
		if (flow) {
			flow->nt2hws->rix_mreg_copy = cpy_idx;
			cpy_idx = 0;
			if (!split)
				return (uintptr_t)flow;
			goto prefix_flow;
		}
		goto free;
	}
	/* Create single flow. */
	ret = flow_hw_create_flow(dev, type, resource.suffix.attr, resource.suffix.items,
				  resource.suffix.actions, item_flags, action_flags,
				  external, &flow, error);
	if (ret)
		goto free;
	if (flow) {
		flow->nt2hws->rix_mreg_copy = cpy_idx;
		cpy_idx = 0;
		if (!split)
			return (uintptr_t)flow;
		/* Fall Through to prefix flow creation. */
	}
prefix_flow:
	ret = flow_hw_create_flow(dev, type, attr, items, resource.prefix.actions,
				  item_flags, action_flags, external, &prfx_flow, error);
	if (ret)
		goto free;
	if (prfx_flow) {
		prfx_flow->nt2hws->rix_mreg_copy = flow->nt2hws->rix_mreg_copy;
		flow->nt2hws->chaned_flow = 1;
		SLIST_INSERT_AFTER(prfx_flow, flow, nt2hws->next);
		mlx5_flow_nta_split_resource_free(dev, &resource);
		return (uintptr_t)prfx_flow;
	}
free:
	if (prfx_flow)
		flow_hw_list_destroy(dev, type, (uintptr_t)prfx_flow);
	if (flow)
		flow_hw_list_destroy(dev, type, (uintptr_t)flow);
	if (cpy_idx)
		mlx5_flow_nta_del_copy_action(dev, cpy_idx);
	if (split > 0)
		mlx5_flow_nta_split_resource_free(dev, &resource);
	return 0;
}

static void
mlx5_mirror_destroy_clone(struct rte_eth_dev *dev,
			  struct mlx5_mirror_clone *clone)
{
	switch (clone->type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
	case RTE_FLOW_ACTION_TYPE_QUEUE:
		mlx5_hrxq_release(dev,
				  ((struct mlx5_hrxq *)(clone->action_ctx))->idx);
		break;
	case RTE_FLOW_ACTION_TYPE_JUMP:
		flow_hw_jump_release(dev, clone->action_ctx);
		break;
	case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
	case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
	default:
		break;
	}
}

void
mlx5_hw_mirror_destroy(struct rte_eth_dev *dev, struct mlx5_mirror *mirror)
{
	uint32_t i;

	mlx5_indirect_list_remove_entry(&mirror->indirect);
	for (i = 0; i < mirror->clones_num; i++)
		mlx5_mirror_destroy_clone(dev, &mirror->clone[i]);
	if (mirror->mirror_action)
		mlx5dr_action_destroy(mirror->mirror_action);
	mlx5_free(mirror);
}

static __rte_always_inline bool
mlx5_mirror_terminal_action(const struct rte_flow_action *action)
{
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_JUMP:
	case RTE_FLOW_ACTION_TYPE_RSS:
	case RTE_FLOW_ACTION_TYPE_QUEUE:
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
	case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
	case RTE_FLOW_ACTION_TYPE_DROP:
		return true;
	default:
		break;
	}
	return false;
}

static bool
mlx5_mirror_validate_sample_action(struct rte_eth_dev *dev,
				   const struct rte_flow_attr *flow_attr,
				   const struct rte_flow_action *action)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_ethdev *port = NULL;
	bool is_proxy = MLX5_HW_PORT_IS_PROXY(priv);

	if (!action)
		return false;
	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
	case RTE_FLOW_ACTION_TYPE_RSS:
		if (flow_attr->transfer)
			return false;
		break;
	case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
		if (!is_proxy || !flow_attr->transfer)
			return false;
		port = action->conf;
		if (!port || port->port_id != MLX5_REPRESENTED_PORT_ESW_MGR)
			return false;
		break;
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
	case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		if (!is_proxy || !flow_attr->transfer)
			return false;
		if (action[0].type == RTE_FLOW_ACTION_TYPE_RAW_DECAP &&
		    action[1].type != RTE_FLOW_ACTION_TYPE_RAW_ENCAP)
			return false;
		break;
	case RTE_FLOW_ACTION_TYPE_JUMP_TO_TABLE_INDEX:
	case RTE_FLOW_ACTION_TYPE_JUMP:
	case RTE_FLOW_ACTION_TYPE_DROP:
		break;
	default:
		return false;
	}
	return true;
}

/**
 * Valid mirror actions list includes one or two SAMPLE actions
 * followed by JUMP.
 *
 * @return
 * Number of mirrors *action* list was valid.
 * -EINVAL otherwise.
 */
static int
mlx5_hw_mirror_actions_list_validate(struct rte_eth_dev *dev,
				     const struct rte_flow_attr *flow_attr,
				     const struct rte_flow_action *actions)
{
	if (actions[0].type == RTE_FLOW_ACTION_TYPE_SAMPLE) {
		int i = 1;
		bool valid;
		const struct rte_flow_action_sample *sample = actions[0].conf;
		valid = mlx5_mirror_validate_sample_action(dev, flow_attr,
							   sample->actions);
		if (!valid)
			return -EINVAL;
		if (actions[1].type == RTE_FLOW_ACTION_TYPE_SAMPLE) {
			i = 2;
			sample = actions[1].conf;
			valid = mlx5_mirror_validate_sample_action(dev, flow_attr,
								   sample->actions);
			if (!valid)
				return -EINVAL;
		}
		return mlx5_mirror_terminal_action(actions + i) ? i + 1 : -EINVAL;
	}
	return -EINVAL;
}

static int
mirror_format_tir(struct rte_eth_dev *dev,
		  struct mlx5_mirror_clone *clone,
		  const struct mlx5_flow_template_table_cfg *table_cfg,
		  const struct rte_flow_action *action,
		  struct mlx5dr_action_dest_attr *dest_attr,
		  struct rte_flow_error *error)
{
	uint32_t hws_flags;
	enum mlx5dr_table_type table_type;
	struct mlx5_hrxq *tir_ctx;
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);

	table_type = get_mlx5dr_table_type(&table_cfg->attr.flow_attr, table_cfg->attr.specialize,
					   unified_fdb);
	hws_flags = mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_NONE_ROOT][table_type];
	tir_ctx = flow_hw_tir_action_register(dev, hws_flags, action);
	if (!tir_ctx)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "failed to create QUEUE action for mirror clone");
	dest_attr->dest = tir_ctx->action;
	clone->action_ctx = tir_ctx;
	return 0;
}

static int
mirror_format_jump(struct rte_eth_dev *dev,
		   struct mlx5_mirror_clone *clone,
		   const struct mlx5_flow_template_table_cfg *table_cfg,
		   const struct rte_flow_action *action,
		   struct mlx5dr_action_dest_attr *dest_attr,
		   struct rte_flow_error *error)
{
	const struct rte_flow_action_jump *jump_conf = action->conf;
	struct mlx5_hw_jump_action *jump = flow_hw_jump_action_register
						(dev, table_cfg,
						 jump_conf->group, error);

	if (!jump)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  action, "failed to create JUMP action for mirror clone");
	dest_attr->dest = jump->hws_action;
	clone->action_ctx = jump;
	return 0;
}

static int
mirror_format_port(struct rte_eth_dev *dev,
		   const struct rte_flow_action *action,
		   struct mlx5dr_action_dest_attr *dest_attr,
		   struct rte_flow_error __rte_unused *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_action_ethdev *port_action = action->conf;

	dest_attr->dest = priv->hw_vport[port_action->port_id];
	return 0;
}

static int
hw_mirror_clone_reformat(const struct rte_flow_action *actions,
			 struct mlx5dr_action_dest_attr *dest_attr,
			 enum mlx5dr_action_type *action_type,
			 uint8_t *reformat_buf, bool decap)
{
	int ret;
	const struct rte_flow_item *encap_item = NULL;
	const struct rte_flow_action_raw_encap *encap_conf = NULL;
	typeof(dest_attr->reformat) *reformat = &dest_attr->reformat;

	switch (actions[0].type) {
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		encap_conf = actions[0].conf;
		break;
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		encap_item = MLX5_CONST_ENCAP_ITEM(rte_flow_action_vxlan_encap,
						   actions);
		break;
	case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
		encap_item = MLX5_CONST_ENCAP_ITEM(rte_flow_action_nvgre_encap,
						   actions);
		break;
	default:
		return -EINVAL;
	}
	*action_type = decap ?
		       MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3 :
		       MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
	if (encap_item) {
		ret = flow_dv_convert_encap_data(encap_item, reformat_buf,
						 &reformat->reformat_data_sz, NULL);
		if (ret)
			return -EINVAL;
		reformat->reformat_data = reformat_buf;
	} else {
		reformat->reformat_data = (void *)(uintptr_t)encap_conf->data;
		reformat->reformat_data_sz = encap_conf->size;
	}
	return 0;
}

static int
hw_mirror_format_clone(struct rte_eth_dev *dev,
			struct mlx5_mirror_clone *clone,
			const struct mlx5_flow_template_table_cfg *table_cfg,
			const struct rte_flow_action *actions,
			struct mlx5dr_action_dest_attr *dest_attr,
			uint8_t *reformat_buf, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret;
	uint32_t i;
	bool decap_seen = false;

	for (i = 0; actions[i].type != RTE_FLOW_ACTION_TYPE_END; i++) {
		dest_attr->action_type[i] = mlx5_hw_dr_action_types[actions[i].type];
		switch (actions[i].type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_RSS:
			ret = mirror_format_tir(dev, clone, table_cfg,
						&actions[i], dest_attr, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			ret = mirror_format_port(dev, &actions[i],
						 dest_attr, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			ret = mirror_format_jump(dev, clone, table_cfg,
						 &actions[i], dest_attr, error);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR:
			dest_attr->dest = priv->hw_def_miss;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			decap_seen = true;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		case RTE_FLOW_ACTION_TYPE_NVGRE_ENCAP:
			ret = hw_mirror_clone_reformat(&actions[i], dest_attr,
						       &dest_attr->action_type[i],
						       reformat_buf, decap_seen);
			if (ret < 0)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION,
							  &actions[i],
							  "failed to create reformat action");
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			break;
		default:
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_ACTION,
						  &actions[i], "unsupported sample action");
		}
		clone->type = actions->type;
	}
	dest_attr->action_type[i] = MLX5DR_ACTION_TYP_LAST;
	return 0;
}

struct mlx5_mirror *
mlx5_hw_create_mirror(struct rte_eth_dev *dev,
		      const struct mlx5_flow_template_table_cfg *table_cfg,
		      const struct rte_flow_action *actions,
		      struct rte_flow_error *error)
{
	uint32_t hws_flags;
	int ret = 0, i, clones_num;
	struct mlx5_mirror *mirror;
	enum mlx5dr_table_type table_type;
	struct mlx5_priv *priv = dev->data->dev_private;
	bool unified_fdb = is_unified_fdb(priv);
	const struct rte_flow_attr *flow_attr = &table_cfg->attr.flow_attr;
	uint8_t reformat_buf[MLX5_MIRROR_MAX_CLONES_NUM][MLX5_ENCAP_MAX_LEN];
	struct mlx5dr_action_dest_attr mirror_attr[MLX5_MIRROR_MAX_CLONES_NUM + 1];
	enum mlx5dr_action_type array_action_types[MLX5_MIRROR_MAX_CLONES_NUM + 1]
						  [MLX5_MIRROR_MAX_SAMPLE_ACTIONS_LEN + 1];

	memset(mirror_attr, 0, sizeof(mirror_attr));
	memset(array_action_types, 0, sizeof(array_action_types));
	table_type = get_mlx5dr_table_type(flow_attr, table_cfg->attr.specialize, unified_fdb);
	hws_flags = mlx5_hw_act_flag[MLX5_HW_ACTION_FLAG_NONE_ROOT][table_type];
	clones_num = mlx5_hw_mirror_actions_list_validate(dev, flow_attr,
							  actions);
	if (clones_num < 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Invalid mirror list format");
		return NULL;
	}
	mirror = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*mirror),
			     0, SOCKET_ID_ANY);
	if (!mirror) {
		rte_flow_error_set(error, ENOMEM, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Failed to allocate mirror context");
		return NULL;
	}

	mirror->indirect.type = MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR;
	mirror->clones_num = clones_num;
	for (i = 0; i < clones_num; i++) {
		const struct rte_flow_action *clone_actions;

		mirror_attr[i].action_type = array_action_types[i];
		if (actions[i].type == RTE_FLOW_ACTION_TYPE_SAMPLE) {
			const struct rte_flow_action_sample *sample = actions[i].conf;

			clone_actions = sample->actions;
		} else {
			clone_actions = &actions[i];
		}
		ret = hw_mirror_format_clone(dev, &mirror->clone[i], table_cfg,
					     clone_actions, &mirror_attr[i],
					     reformat_buf[i], error);

		if (ret)
			goto error;
	}
	hws_flags |= MLX5DR_ACTION_FLAG_SHARED;
	mirror->mirror_action = mlx5dr_action_create_dest_array(priv->dr_ctx,
								clones_num,
								mirror_attr,
								hws_flags);
	if (!mirror->mirror_action) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Failed to create HWS mirror action");
		goto error;
	}
	return mirror;
error:
	mlx5_hw_mirror_destroy(dev, mirror);
	return NULL;
}

static struct rte_flow_action_list_handle *
mlx5_hw_mirror_handle_create(struct rte_eth_dev *dev,
			     const struct mlx5_flow_template_table_cfg *table_cfg,
			     const struct rte_flow_action *actions,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_mirror *mirror = mlx5_hw_create_mirror(dev, table_cfg, actions, error);

	if (mirror)
		mlx5_indirect_list_add_entry(&priv->indirect_list_head, &mirror->indirect);
	return (struct rte_flow_action_list_handle *)mirror;
}

void
mlx5_destroy_legacy_indirect(__rte_unused struct rte_eth_dev *dev,
			     struct mlx5_indirect_list *ptr)
{
	struct mlx5_indlst_legacy *obj = (typeof(obj))ptr;

	switch (obj->legacy_type) {
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		break; /* ASO meters were released in mlx5_flow_meter_flush() */
	default:
		break;
	}
	mlx5_free(obj);
}

static struct rte_flow_action_list_handle *
mlx5_create_legacy_indlst(struct rte_eth_dev *dev, uint32_t queue,
			  const struct rte_flow_op_attr *attr,
			  const struct rte_flow_indir_action_conf *conf,
			  const struct rte_flow_action *actions,
			  void *user_data, struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_indlst_legacy *indlst_obj = mlx5_malloc(MLX5_MEM_ZERO,
							    sizeof(*indlst_obj),
							    0, SOCKET_ID_ANY);

	if (!indlst_obj)
		return NULL;
	indlst_obj->handle = flow_hw_action_handle_create(dev, queue, attr, conf,
							  actions, user_data,
							  error);
	if (!indlst_obj->handle) {
		mlx5_free(indlst_obj);
		return NULL;
	}
	indlst_obj->legacy_type = actions[0].type;
	indlst_obj->indirect.type = MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY;
	mlx5_indirect_list_add_entry(&priv->indirect_list_head, &indlst_obj->indirect);
	return (struct rte_flow_action_list_handle *)indlst_obj;
}

static __rte_always_inline enum mlx5_indirect_list_type
flow_hw_inlist_type_get(const struct rte_flow_action *actions)
{
	switch (actions[0].type) {
	case RTE_FLOW_ACTION_TYPE_SAMPLE:
		return MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR;
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
		return actions[1].type == RTE_FLOW_ACTION_TYPE_END ?
		       MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY :
		       MLX5_INDIRECT_ACTION_LIST_TYPE_ERR;
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		return MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT;
	default:
		break;
	}
	return MLX5_INDIRECT_ACTION_LIST_TYPE_ERR;
}

static struct rte_flow_action_list_handle*
mlx5_hw_decap_encap_handle_create(struct rte_eth_dev *dev,
				  const struct mlx5_flow_template_table_cfg *table_cfg,
				  const struct rte_flow_action *actions,
				  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	const struct rte_flow_attr *flow_attr = &table_cfg->attr.flow_attr;
	const struct rte_flow_action *encap = NULL;
	const struct rte_flow_action *decap = NULL;
	struct rte_flow_indir_action_conf indirect_conf = {
		.ingress = flow_attr->ingress,
		.egress = flow_attr->egress,
		.transfer = flow_attr->transfer,
	};
	struct mlx5_hw_encap_decap_action *handle;
	uint64_t action_flags = 0;

	/*
	 * Allow
	 * 1. raw_decap / raw_encap / end
	 * 2. raw_encap / end
	 * 3. raw_decap / end
	 */
	while (actions->type != RTE_FLOW_ACTION_TYPE_END) {
		if (actions->type == RTE_FLOW_ACTION_TYPE_RAW_DECAP) {
			if (action_flags) {
				rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						   actions, "Invalid indirect action list sequence");
				return NULL;
			}
			action_flags |= MLX5_FLOW_ACTION_DECAP;
			decap = actions;
		} else if (actions->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP) {
			if (action_flags & MLX5_FLOW_ACTION_ENCAP) {
				rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						   actions, "Invalid indirect action list sequence");
				return NULL;
			}
			action_flags |= MLX5_FLOW_ACTION_ENCAP;
			encap = actions;
		} else {
			rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
					   actions, "Invalid indirect action type in list");
			return NULL;
		}
		actions++;
	}
	if (!decap && !encap) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Invalid indirect action combinations");
		return NULL;
	}
	handle = mlx5_reformat_action_create(dev, &indirect_conf, encap, decap, error);
	if (!handle) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Failed to create HWS decap_encap action");
		return NULL;
	}
	handle->indirect.type = MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT;
	LIST_INSERT_HEAD(&priv->indirect_list_head, &handle->indirect, entry);
	return (struct rte_flow_action_list_handle *)handle;
}

static struct rte_flow_action_list_handle *
flow_hw_async_action_list_handle_create(struct rte_eth_dev *dev, uint32_t queue,
					const struct rte_flow_op_attr *attr,
					const struct rte_flow_indir_action_conf *conf,
					const struct rte_flow_action *actions,
					void *user_data,
					struct rte_flow_error *error)
{
	struct mlx5_hw_q_job *job = NULL;
	bool push = flow_hw_action_push(attr);
	enum mlx5_indirect_list_type list_type;
	struct rte_flow_action_list_handle *handle;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_template_table_cfg table_cfg = {
		.external = true,
		.attr = {
			.flow_attr = {
				.ingress = conf->ingress,
				.egress = conf->egress,
				.transfer = conf->transfer
			}
		}
	};

	if (!mlx5_hw_ctx_validate(dev, error))
		return NULL;
	if (!actions) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   NULL, "No action list");
		return NULL;
	}
	list_type = flow_hw_inlist_type_get(actions);
	if (list_type == MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY) {
		/*
		 * Legacy indirect actions already have
		 * async resources management. No need to do it twice.
		 */
		handle = mlx5_create_legacy_indlst(dev, queue, attr, conf,
						   actions, user_data, error);
		goto end;
	}
	if (attr) {
		job = flow_hw_action_job_init(priv, queue, NULL, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_CREATE,
					      MLX5_HW_INDIRECT_TYPE_LIST, error);
		if (!job)
			return NULL;
	}
	switch (list_type) {
	case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
		/*
		 * Mirror action is only supported in HWS group. Setting group to
		 * non-zero will ensure that the action resources are allocated correctly.
		 */
		table_cfg.attr.flow_attr.group = 1;
		handle = mlx5_hw_mirror_handle_create(dev, &table_cfg,
						      actions, error);
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
		handle = mlx5_hw_decap_encap_handle_create(dev, &table_cfg,
							   actions, error);
		break;
	default:
		handle = NULL;
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
				   actions, "Invalid list");
	}
	if (job) {
		job->action = handle;
		flow_hw_action_finalize(dev, queue, job, push, false,
					handle != NULL);
	}
end:
	return handle;
}

static struct rte_flow_action_list_handle *
flow_hw_action_list_handle_create(struct rte_eth_dev *dev,
				  const struct rte_flow_indir_action_conf *conf,
				  const struct rte_flow_action *actions,
				  struct rte_flow_error *error)
{
	return flow_hw_async_action_list_handle_create(dev, MLX5_HW_INV_QUEUE,
						       NULL, conf, actions,
						       NULL, error);
}

static int
flow_hw_async_action_list_handle_destroy
			(struct rte_eth_dev *dev, uint32_t queue,
			 const struct rte_flow_op_attr *attr,
			 struct rte_flow_action_list_handle *handle,
			 void *user_data, struct rte_flow_error *error)
{
	int ret = 0;
	struct mlx5_hw_q_job *job = NULL;
	bool push = flow_hw_action_push(attr);
	struct mlx5_priv *priv = dev->data->dev_private;
	enum mlx5_indirect_list_type type =
		mlx5_get_indirect_list_type((void *)handle);

	if (type == MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY) {
		struct mlx5_indlst_legacy *legacy = (typeof(legacy))handle;

		ret = flow_hw_action_handle_destroy(dev, queue, attr,
						    legacy->handle,
						    user_data, error);
		mlx5_indirect_list_remove_entry(&legacy->indirect);
		goto end;
	}
	if (attr) {
		job = flow_hw_action_job_init(priv, queue, NULL, user_data,
					      NULL, MLX5_HW_Q_JOB_TYPE_DESTROY,
					      MLX5_HW_INDIRECT_TYPE_LIST, error);
		if (!job)
			return rte_errno;
	}
	switch (type) {
	case MLX5_INDIRECT_ACTION_LIST_TYPE_MIRROR:
		mlx5_hw_mirror_destroy(dev, (struct mlx5_mirror *)handle);
		break;
	case MLX5_INDIRECT_ACTION_LIST_TYPE_REFORMAT:
		LIST_REMOVE(&((struct mlx5_hw_encap_decap_action *)handle)->indirect,
			    entry);
		mlx5_reformat_action_destroy(dev, handle, error);
		break;
	default:
		ret = rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_ACTION,
					  NULL, "Invalid indirect list handle");
	}
	if (job) {
		flow_hw_action_finalize(dev, queue, job, push, false, true);
	}
end:
	return ret;
}

static int
flow_hw_action_list_handle_destroy(struct rte_eth_dev *dev,
				   struct rte_flow_action_list_handle *handle,
				   struct rte_flow_error *error)
{
	return flow_hw_async_action_list_handle_destroy(dev, MLX5_HW_INV_QUEUE,
							NULL, handle, NULL,
							error);
}

static int
flow_hw_async_action_list_handle_query_update
		(struct rte_eth_dev *dev, uint32_t queue_id,
		 const struct rte_flow_op_attr *attr,
		 const struct rte_flow_action_list_handle *handle,
		 const void **update, void **query,
		 enum rte_flow_query_update_mode mode,
		 void *user_data, struct rte_flow_error *error)
{
	enum mlx5_indirect_list_type type =
		mlx5_get_indirect_list_type((const void *)handle);

	if (type == MLX5_INDIRECT_ACTION_LIST_TYPE_LEGACY) {
		struct mlx5_indlst_legacy *legacy = (void *)(uintptr_t)handle;

		if (update && query)
			return flow_hw_async_action_handle_query_update
				(dev, queue_id, attr, legacy->handle,
				 update, query, mode, user_data, error);
		else if (update && update[0])
			return flow_hw_action_handle_update(dev, queue_id, attr,
							    legacy->handle, update[0],
							    user_data, error);
		else if (query && query[0])
			return flow_hw_action_handle_query(dev, queue_id, attr,
							   legacy->handle, query[0],
							   user_data, error);
		else
			return rte_flow_error_set(error, EINVAL,
						  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "invalid legacy handle query_update parameters");
	}
	return -ENOTSUP;
}

static int
flow_hw_action_list_handle_query_update(struct rte_eth_dev *dev,
					const struct rte_flow_action_list_handle *handle,
					const void **update, void **query,
					enum rte_flow_query_update_mode mode,
					struct rte_flow_error *error)
{
	return flow_hw_async_action_list_handle_query_update
					(dev, MLX5_HW_INV_QUEUE, NULL, handle,
					 update, query, mode, NULL, error);
}

static int
flow_hw_calc_table_hash(struct rte_eth_dev *dev,
			 const struct rte_flow_template_table *table,
			 const struct rte_flow_item pattern[],
			 uint8_t pattern_template_index,
			 uint32_t *hash, struct rte_flow_error *error)
{
	const struct rte_flow_item *items;
	struct mlx5_flow_hw_pattern_params pp;
	int res;

	items = flow_hw_get_rule_items(dev, table, pattern,
				       pattern_template_index,
				       &pp);
	res = mlx5dr_rule_hash_calculate(mlx5_table_matcher(table), items,
					 pattern_template_index,
					 MLX5DR_RULE_HASH_CALC_MODE_RAW,
					 hash);
	if (res)
		return rte_flow_error_set(error, res,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL,
					  "hash could not be calculated");
	return 0;
}

static int
flow_hw_calc_encap_hash(struct rte_eth_dev *dev,
			const struct rte_flow_item pattern[],
			enum rte_flow_encap_hash_field dest_field,
			uint8_t *hash,
			struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5dr_crc_encap_entropy_hash_fields data;
	enum mlx5dr_crc_encap_entropy_hash_size res_size =
			dest_field == RTE_FLOW_ENCAP_HASH_FIELD_SRC_PORT ?
				MLX5DR_CRC_ENCAP_ENTROPY_HASH_SIZE_16 :
				MLX5DR_CRC_ENCAP_ENTROPY_HASH_SIZE_8;
	int res;

	memset(&data, 0, sizeof(struct mlx5dr_crc_encap_entropy_hash_fields));

	for (; pattern->type != RTE_FLOW_ITEM_TYPE_END; pattern++) {
		switch (pattern->type) {
		case RTE_FLOW_ITEM_TYPE_IPV4:
			data.dst.ipv4_addr =
				((const struct rte_flow_item_ipv4 *)(pattern->spec))->hdr.dst_addr;
			data.src.ipv4_addr =
				((const struct rte_flow_item_ipv4 *)(pattern->spec))->hdr.src_addr;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			memcpy(data.dst.ipv6_addr,
			       &((const struct rte_flow_item_ipv6 *)(pattern->spec))->hdr.dst_addr,
			       sizeof(data.dst.ipv6_addr));
			memcpy(data.src.ipv6_addr,
			       &((const struct rte_flow_item_ipv6 *)(pattern->spec))->hdr.src_addr,
			       sizeof(data.src.ipv6_addr));
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			data.next_protocol = IPPROTO_UDP;
			data.dst_port =
				((const struct rte_flow_item_udp *)(pattern->spec))->hdr.dst_port;
			data.src_port =
				((const struct rte_flow_item_udp *)(pattern->spec))->hdr.src_port;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			data.next_protocol = IPPROTO_TCP;
			data.dst_port =
				((const struct rte_flow_item_tcp *)(pattern->spec))->hdr.dst_port;
			data.src_port =
				((const struct rte_flow_item_tcp *)(pattern->spec))->hdr.src_port;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			data.next_protocol = IPPROTO_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			data.next_protocol = IPPROTO_ICMPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			data.next_protocol = IPPROTO_GRE;
			break;
		default:
			break;
		}
	}
	res = mlx5dr_crc_encap_entropy_hash_calc(priv->dr_ctx, &data, hash, res_size);
	if (res)
		return rte_flow_error_set(error, res,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "error while calculating encap hash");
	return 0;
}

static int
flow_hw_table_resize_multi_pattern_actions(struct rte_eth_dev *dev,
					   struct rte_flow_template_table *table,
					   uint32_t nb_flows,
					   struct rte_flow_error *error)
{
	struct mlx5_multi_pattern_segment *segment = table->mpctx.segments;
	uint32_t bulk_size;
	int i, ret;

	/**
	 * Segment always allocates Modify Header Argument Objects number in
	 * powers of 2.
	 * On resize, PMD adds minimal required argument objects number.
	 * For example, if table size was 10, it allocated 16 argument objects.
	 * Resize to 15 will not add new objects.
	 */
	for (i = 1;
	     i < MLX5_MAX_TABLE_RESIZE_NUM && segment->capacity;
	     i++, segment++) {
		/* keep the devtools/checkpatches.sh happy */
	}
	if (i == MLX5_MAX_TABLE_RESIZE_NUM)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "too many resizes");
	if (segment->head_index - 1 >= nb_flows)
		return 0;
	bulk_size = rte_align32pow2(nb_flows - segment->head_index + 1);
	ret = mlx5_tbl_multi_pattern_process(dev, table, segment,
					     rte_log2_u32(bulk_size),
					     error);
	if (ret)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "too many resizes");
	return i;
}

static int
flow_hw_table_resize(struct rte_eth_dev *dev,
		     struct rte_flow_template_table *table,
		     uint32_t nb_flows,
		     struct rte_flow_error *error)
{
	struct mlx5dr_action_template *at[MLX5_HW_TBL_MAX_ACTION_TEMPLATE];
	struct mlx5dr_match_template *mt[MLX5_HW_TBL_MAX_ITEM_TEMPLATE];
	struct mlx5dr_matcher_attr matcher_attr = table->matcher_attr;
	struct mlx5dr_action_jump_to_matcher_attr jump_attr = {
		.type = MLX5DR_ACTION_JUMP_TO_MATCHER_BY_INDEX,
		.matcher = NULL,
	};
	struct mlx5_multi_pattern_segment *segment = NULL;
	struct mlx5dr_matcher *matcher = NULL;
	struct mlx5dr_action *jump = NULL;
	struct mlx5_priv *priv = dev->data->dev_private;
	uint32_t i, selector = table->matcher_selector;
	uint32_t other_selector = (selector + 1) & 1;
	int ret;

	if (!rte_flow_template_table_resizable(dev->data->port_id, &table->cfg.attr))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "no resizable attribute");
	if (table->matcher_info[other_selector].matcher)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "last table resize was not completed");
	if (nb_flows <= table->cfg.attr.nb_flows)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "shrinking table is not supported");
	ret = mlx5_ipool_resize(table->flow, nb_flows, error);
	if (ret)
		return ret;
	/*
	 * A resizable matcher doesn't support rule update. In this case, the ipool
	 * for the resource is not created and there is no need to resize it.
	 */
	MLX5_ASSERT(!table->resource);
	if (mlx5_is_multi_pattern_active(&table->mpctx)) {
		ret = flow_hw_table_resize_multi_pattern_actions(dev, table, nb_flows, error);
		if (ret < 0)
			return ret;
		if (ret > 0)
			segment = table->mpctx.segments + ret;
	}
	for (i = 0; i < table->nb_item_templates; i++)
		mt[i] = table->its[i]->mt;
	for (i = 0; i < table->nb_action_templates; i++)
		at[i] = table->ats[i].action_template->tmpl;
	nb_flows = rte_align32pow2(nb_flows);
	matcher_attr.rule.num_log = rte_log2_u32(nb_flows);
	matcher = mlx5dr_matcher_create(table->grp->tbl, mt,
					table->nb_item_templates, at,
					table->nb_action_templates,
					&matcher_attr);
	if (!matcher) {
		ret = rte_flow_error_set(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					 table, "failed to create new matcher");
		goto error;
	}
	if (matcher_attr.isolated) {
		jump_attr.matcher = matcher;
		jump = mlx5dr_action_create_jump_to_matcher(priv->dr_ctx, &jump_attr,
			mlx5_hw_act_flag[!!table->cfg.attr.flow_attr.group][table->type]);
		if (!jump) {
			ret = rte_flow_error_set(error, rte_errno,
						RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						table, "failed to create jump to matcher action");
			goto error;
		}
	}
	rte_rwlock_write_lock(&table->matcher_replace_rwlk);
	ret = mlx5dr_matcher_resize_set_target
			(table->matcher_info[selector].matcher, matcher);
	if (ret) {
		rte_rwlock_write_unlock(&table->matcher_replace_rwlk);
		ret = rte_flow_error_set(error, rte_errno,
					 RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					 table, "failed to initiate matcher swap");
		goto error;
	}
	table->cfg.attr.nb_flows = nb_flows;
	table->matcher_info[other_selector].matcher = matcher;
	table->matcher_info[other_selector].jump = jump;
	table->matcher_selector = other_selector;
	rte_atomic_store_explicit(&table->matcher_info[other_selector].refcnt,
				  0, rte_memory_order_relaxed);
	rte_rwlock_write_unlock(&table->matcher_replace_rwlk);
	return 0;
error:
	if (segment)
		mlx5_destroy_multi_pattern_segment(segment);
	if (jump)
		mlx5dr_action_destroy(jump);
	if (matcher) {
		ret = mlx5dr_matcher_destroy(matcher);
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "failed to destroy new matcher");
	}
	return ret;
}

static int
flow_hw_table_resize_complete(__rte_unused struct rte_eth_dev *dev,
			      struct rte_flow_template_table *table,
			      struct rte_flow_error *error)
{
	int ret;
	uint32_t selector = table->matcher_selector;
	uint32_t other_selector = (selector + 1) & 1;
	struct mlx5_matcher_info *matcher_info = &table->matcher_info[other_selector];
	uint32_t matcher_refcnt;

	if (!rte_flow_template_table_resizable(dev->data->port_id, &table->cfg.attr))
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "no resizable attribute");
	matcher_refcnt = rte_atomic_load_explicit(&matcher_info->refcnt,
						  rte_memory_order_relaxed);
	if (!matcher_info->matcher || matcher_refcnt)
		return rte_flow_error_set(error, EBUSY,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "cannot complete table resize");
	if (matcher_info->jump)
		mlx5dr_action_destroy(matcher_info->jump);
	ret = mlx5dr_matcher_destroy(matcher_info->matcher);
	if (ret)
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  table, "failed to destroy retired matcher");
	matcher_info->matcher = NULL;
	return 0;
}

static int
flow_hw_update_resized(struct rte_eth_dev *dev, uint32_t queue,
		       const struct rte_flow_op_attr *attr,
		       struct rte_flow *flow, void *user_data,
		       struct rte_flow_error *error)
{
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_hw *hw_flow = (struct rte_flow_hw *)flow;
	struct rte_flow_template_table *table = hw_flow->table;
	struct rte_flow_hw_aux *aux = mlx5_flow_hw_aux(dev->data->port_id, hw_flow);
	uint32_t table_selector = table->matcher_selector;
	uint32_t rule_selector = aux->matcher_selector;
	uint32_t other_selector;
	struct mlx5dr_matcher *other_matcher;
	struct mlx5dr_rule_attr rule_attr = {
		.queue_id = queue,
		.burst = attr->postpone,
	};

	MLX5_ASSERT(hw_flow->flags & MLX5_FLOW_HW_FLOW_FLAG_MATCHER_SELECTOR);
	/**
	 * mlx5dr_matcher_resize_rule_move() accepts original table matcher -
	 * the one that was used BEFORE table resize.
	 * Since the function is called AFTER table resize,
	 * `table->matcher_selector` always points to the new matcher and
	 * `aux->matcher_selector` points to a matcher used to create the flow.
	 */
	other_selector = rule_selector == table_selector ?
			 (rule_selector + 1) & 1 : rule_selector;
	other_matcher = table->matcher_info[other_selector].matcher;
	if (!other_matcher)
		return rte_flow_error_set(error, EINVAL,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "no active table resize");
	hw_flow->operation_type = MLX5_FLOW_HW_FLOW_OP_TYPE_RSZ_TBL_MOVE;
	hw_flow->user_data = user_data;
	rule_attr.user_data = hw_flow;
	if (rule_selector == table_selector) {
		struct rte_ring *ring = !attr->postpone ?
					priv->hw_q[queue].flow_transfer_completed :
					priv->hw_q[queue].flow_transfer_pending;
		rte_ring_enqueue(ring, hw_flow);
		flow_hw_q_inc_flow_ops(priv, queue);
		return 0;
	}
	ret = mlx5dr_matcher_resize_rule_move(other_matcher,
					      (struct mlx5dr_rule *)hw_flow->rule,
					      &rule_attr);
	if (ret) {
		return rte_flow_error_set(error, rte_errno,
					  RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "flow transfer failed");
	}
	flow_hw_q_inc_flow_ops(priv, queue);
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
flow_hw_validate(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		 const struct rte_flow_item items[],
		 const struct rte_flow_action actions[] __rte_unused,
		 bool external, int hairpin __rte_unused,
		 struct rte_flow_error *error)
{
	const struct rte_flow_pattern_template_attr pattern_template_attr = {
		.relaxed_matching = 0,
		.ingress = attr->ingress,
		.egress = attr->egress,
		.transfer = attr->transfer,
	};
	uint64_t item_flags = 0;
	int ret = 0;

	if (external) {
		/* Validate application items only */
		ret = __flow_hw_pattern_validate(dev, &pattern_template_attr, items,
						    &item_flags, true, error);
		if (ret < 0)
			return -rte_errno;
	}
	return 0;
}

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops = {
	.list_create = flow_hw_list_create,
	.list_destroy = flow_hw_list_destroy,
	.validate = flow_hw_validate,
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
	.table_resize = flow_hw_table_resize,
	.group_set_miss_actions = mlx5_flow_hw_group_set_miss_actions,
	.async_flow_create = flow_hw_async_flow_create,
	.async_flow_create_by_index = flow_hw_async_flow_create_by_index,
	.async_flow_update = flow_hw_async_flow_update,
	.async_flow_destroy = flow_hw_async_flow_destroy,
	.flow_update_resized = flow_hw_update_resized,
	.table_resize_complete = flow_hw_table_resize_complete,
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
	.action_list_handle_create = flow_hw_action_list_handle_create,
	.action_list_handle_destroy = flow_hw_action_list_handle_destroy,
	.action_list_handle_query_update =
		flow_hw_action_list_handle_query_update,
	.async_action_list_handle_create =
		flow_hw_async_action_list_handle_create,
	.async_action_list_handle_destroy =
		flow_hw_async_action_list_handle_destroy,
	.async_action_list_handle_query_update =
		flow_hw_async_action_list_handle_query_update,
	.query = flow_hw_query,
	.get_aged_flows = flow_hw_get_aged_flows,
	.get_q_aged_flows = flow_hw_get_q_aged_flows,
	.item_create = flow_dv_item_create,
	.item_release = flow_dv_item_release,
	.flow_calc_table_hash = flow_hw_calc_table_hash,
	.flow_calc_encap_hash = flow_hw_calc_encap_hash,
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
 * @param info
 *   Additional info about control flow rule.
 * @param external
 *   External ctrl flow.
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
			 uint8_t action_template_idx,
			 struct mlx5_ctrl_flow_info *info,
			 bool external)
{
	struct mlx5_priv *priv = proxy_dev->data->dev_private;
	uint32_t queue = CTRL_QUEUE_ID(priv);
	struct rte_flow_op_attr op_attr = {
		.postpone = 0,
	};
	struct rte_flow *flow = NULL;
	struct mlx5_ctrl_flow_entry *entry = NULL;
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
	ret = __flow_hw_pull_comp(proxy_dev, queue, NULL);
	if (ret) {
		DRV_LOG(ERR, "port %u failed to insert control flow",
			proxy_dev->data->port_id);
		rte_errno = EINVAL;
		ret = -rte_errno;
		goto error;
	}
	entry->owner_dev = owner_dev;
	entry->flow = flow;
	if (info)
		entry->info = *info;
	else
		entry->info.type = MLX5_CTRL_FLOW_TYPE_GENERAL;
	if (external)
		LIST_INSERT_HEAD(&priv->hw_ext_ctrl_flows, entry, next);
	else
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
	ret = __flow_hw_pull_comp(dev, queue, NULL);
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
	struct mlx5_ctrl_flow_entry *cf;
	struct mlx5_ctrl_flow_entry *cf_next;
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
	struct mlx5_ctrl_flow_entry *cf;
	struct mlx5_ctrl_flow_entry *cf_next;
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
	cf = LIST_FIRST(&priv->hw_ext_ctrl_flows);
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
mlx5_flow_hw_esw_create_sq_miss_flow(struct rte_eth_dev *dev, uint32_t sqn, bool external)
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_SQ_MISS_ROOT,
		.esw_mgr_sq = sqn,
	};
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
	if (!proxy_priv->hw_ctrl_fdb ||
	    !proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_root_tbl ||
	    !proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_tbl) {
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
	ret = flow_hw_create_ctrl_flow(dev, proxy_dev,
				       proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_root_tbl,
				       items, 0, actions, 0, &flow_info, external);
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
	flow_info.type = MLX5_CTRL_FLOW_TYPE_SQ_MISS;
	ret = flow_hw_create_ctrl_flow(dev, proxy_dev,
				       proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_tbl,
				       items, 0, actions, 0, &flow_info, external);
	if (ret) {
		DRV_LOG(ERR, "Port %u failed to create HWS SQ miss flow rule for SQ %u, ret %d",
			port_id, sqn, ret);
		return ret;
	}
	return 0;
}

static bool
flow_hw_is_matching_sq_miss_flow(struct mlx5_ctrl_flow_entry *cf,
				 struct rte_eth_dev *dev,
				 uint32_t sqn)
{
	if (cf->owner_dev != dev)
		return false;
	if (cf->info.type == MLX5_CTRL_FLOW_TYPE_SQ_MISS_ROOT && cf->info.esw_mgr_sq == sqn)
		return true;
	if (cf->info.type == MLX5_CTRL_FLOW_TYPE_SQ_MISS && cf->info.esw_mgr_sq == sqn)
		return true;
	return false;
}

int
mlx5_flow_hw_esw_destroy_sq_miss_flow(struct rte_eth_dev *dev, uint32_t sqn)
{
	uint16_t port_id = dev->data->port_id;
	uint16_t proxy_port_id = dev->data->port_id;
	struct rte_eth_dev *proxy_dev;
	struct mlx5_priv *proxy_priv;
	struct mlx5_ctrl_flow_entry *cf;
	struct mlx5_ctrl_flow_entry *cf_next;
	int ret;

	ret = rte_flow_pick_transfer_proxy(port_id, &proxy_port_id, NULL);
	if (ret) {
		DRV_LOG(ERR, "Unable to pick transfer proxy port for port %u. Transfer proxy "
			     "port must be present for default SQ miss flow rules to exist.",
			     port_id);
		return ret;
	}
	proxy_dev = &rte_eth_devices[proxy_port_id];
	proxy_priv = proxy_dev->data->dev_private;
	/* FDB default flow rules must be enabled. */
	MLX5_ASSERT(proxy_priv->sh->config.fdb_def_rule);
	if (!proxy_priv->dr_ctx)
		return 0;
	if (!proxy_priv->hw_ctrl_fdb ||
	    !proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_root_tbl ||
	    !proxy_priv->hw_ctrl_fdb->hw_esw_sq_miss_tbl)
		return 0;
	cf = LIST_FIRST(&proxy_priv->hw_ctrl_flows);
	while (cf != NULL) {
		cf_next = LIST_NEXT(cf, next);
		if (flow_hw_is_matching_sq_miss_flow(cf, dev, sqn)) {
			claim_zero(flow_hw_destroy_ctrl_flow(proxy_dev, cf->flow));
			LIST_REMOVE(cf, next);
			mlx5_free(cf);
		}
		cf = cf_next;
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_JUMP,
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
	/* FDB default flow rules must be enabled. */
	MLX5_ASSERT(proxy_priv->sh->config.fdb_def_rule);
	if (!proxy_priv->dr_ctx) {
		DRV_LOG(DEBUG, "Transfer proxy port (port %u) of port %u must be configured "
			       "for HWS to create default FDB jump rule. Default rule will "
			       "not be created.",
			       proxy_port_id, port_id);
		return 0;
	}
	if (!proxy_priv->hw_ctrl_fdb || !proxy_priv->hw_ctrl_fdb->hw_esw_zero_tbl) {
		DRV_LOG(ERR, "Transfer proxy port (port %u) of port %u was configured, but "
			     "default flow tables were not created.",
			     proxy_port_id, port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return flow_hw_create_ctrl_flow(dev, proxy_dev,
					proxy_priv->hw_ctrl_fdb->hw_esw_zero_tbl,
					items, 0, actions, 0, &flow_info, false);
}

int
mlx5_flow_hw_create_tx_default_mreg_copy_flow(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_item_eth promisc = {
		.hdr.dst_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.hdr.src_addr.addr_bytes = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_TX_META_COPY,
	};

	MLX5_ASSERT(priv->master);
	if (!priv->dr_ctx ||
	    !priv->hw_ctrl_fdb ||
	    !priv->hw_ctrl_fdb->hw_tx_meta_cpy_tbl)
		return 0;
	return flow_hw_create_ctrl_flow(dev, dev,
					priv->hw_ctrl_fdb->hw_tx_meta_cpy_tbl,
					eth_all, 0, copy_reg_action, 0, &flow_info, false);
}

int
mlx5_flow_hw_tx_repr_matching_flow(struct rte_eth_dev *dev, uint32_t sqn, bool external)
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_TX_REPR_MATCH,
		.tx_repr_sq = sqn,
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
					items, 0, actions, 0, &flow_info, external);
}

int
mlx5_flow_hw_lacp_rx_flow(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct rte_flow_item_eth lacp_item = {
		.type = RTE_BE16(RTE_ETHER_TYPE_SLOW),
	};
	struct rte_flow_item eth_lacp[] = {
		[0] = {
			.type = RTE_FLOW_ITEM_TYPE_ETH,
			.spec = &lacp_item,
			.mask = &lacp_item,
		},
		[1] = {
			.type = RTE_FLOW_ITEM_TYPE_END,
		},
	};
	struct rte_flow_action miss_action[] = {
		[0] = {
			.type = (enum rte_flow_action_type)
				MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS,
		},
		[1] = {
			.type = RTE_FLOW_ACTION_TYPE_END,
		},
	};
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_LACP_RX,
	};

	if (!priv->dr_ctx || !priv->hw_ctrl_fdb || !priv->hw_ctrl_fdb->hw_lacp_rx_tbl)
		return 0;
	return flow_hw_create_ctrl_flow(dev, dev,
					priv->hw_ctrl_fdb->hw_lacp_rx_tbl,
					eth_lacp, 0, miss_action, 0, &flow_info, false);
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS,
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
	return flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0, &flow_info, false);
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
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS,
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
		if (flow_hw_create_ctrl_flow(dev, dev,
					     tbl, items, 0, actions, 0, &flow_info, false))
			return -rte_errno;
	}
	return 0;
}

static int
__flow_hw_ctrl_flows_unicast_create(struct rte_eth_dev *dev,
				    struct rte_flow_template_table *tbl,
				    const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type,
				    const struct rte_ether_addr *addr)
{
	struct rte_flow_item_eth eth_spec = {
		.hdr.dst_addr = *addr,
	};
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC,
		.uc = {
			.dmac = *addr,
		},
	};

	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &eth_spec,
	};
	items[1] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_VOID };
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };

	if (flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0, &flow_info, false))
		return -rte_errno;

	return 0;
}

static int
__flow_hw_ctrl_flows_unicast(struct rte_eth_dev *dev,
			     struct rte_flow_template_table *tbl,
			     const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	unsigned int i;
	int ret;

	for (i = 0; i < MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (rte_is_zero_ether_addr(mac))
			continue;

		ret = __flow_hw_ctrl_flows_unicast_create(dev, tbl, rss_type, mac);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static int
__flow_hw_ctrl_flows_unicast_vlan_create(struct rte_eth_dev *dev,
					 struct rte_flow_template_table *tbl,
					 const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type,
					 const struct rte_ether_addr *addr,
					 const uint16_t vid)
{
	struct rte_flow_item_eth eth_spec = {
		.hdr.dst_addr = *addr,
	};
	struct rte_flow_item_vlan vlan_spec = {
		.tci = rte_cpu_to_be_16(vid),
	};
	struct rte_flow_item items[5];
	struct rte_flow_action actions[] = {
		{ .type = RTE_FLOW_ACTION_TYPE_RSS },
		{ .type = RTE_FLOW_ACTION_TYPE_END },
	};
	struct mlx5_ctrl_flow_info flow_info = {
		.type = MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC_VLAN,
		.uc = {
			.dmac = *addr,
			.vlan = vid,
		},
	};

	memset(items, 0, sizeof(items));
	items[0] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_ETH,
		.spec = &eth_spec,
	};
	items[1] = (struct rte_flow_item){
		.type = RTE_FLOW_ITEM_TYPE_VLAN,
		.spec = &vlan_spec,
	};
	items[2] = flow_hw_get_ctrl_rx_l3_item(rss_type);
	items[3] = flow_hw_get_ctrl_rx_l4_item(rss_type);
	items[4] = (struct rte_flow_item){ .type = RTE_FLOW_ITEM_TYPE_END };

	if (flow_hw_create_ctrl_flow(dev, dev, tbl, items, 0, actions, 0, &flow_info, false))
		return -rte_errno;

	return 0;
}

static int
__flow_hw_ctrl_flows_unicast_vlan(struct rte_eth_dev *dev,
				  struct rte_flow_template_table *tbl,
				  const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	unsigned int i;
	unsigned int j;

	for (i = 0; i < MLX5_MAX_MAC_ADDRESSES; ++i) {
		struct rte_ether_addr *mac = &dev->data->mac_addrs[i];

		if (rte_is_zero_ether_addr(mac))
			continue;

		for (j = 0; j < priv->vlan_filter_n; ++j) {
			uint16_t vlan = priv->vlan_filter[j];
			int ret;

			ret = __flow_hw_ctrl_flows_unicast_vlan_create(dev, tbl, rss_type,
								       mac, vlan);
			if (ret < 0)
				return ret;
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
		return __flow_hw_ctrl_flows_unicast(dev, tbl, rss_type);
	case MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN:
		return __flow_hw_ctrl_flows_unicast_vlan(dev, tbl, rss_type);
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
	int j;
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

static int
mlx5_flow_hw_ctrl_flow_single(struct rte_eth_dev *dev,
			      const enum mlx5_flow_ctrl_rx_eth_pattern_type eth_pattern_type,
			      const struct rte_ether_addr *addr,
			      const uint16_t vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_flow_hw_ctrl_rx *hw_ctrl_rx;
	unsigned int j;
	int ret = 0;

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

	/* TODO: this part should be somehow refactored. It's common with common flow creation. */
	for (j = 0; j < MLX5_FLOW_HW_CTRL_RX_EXPANDED_RSS_MAX; ++j) {
		const enum mlx5_flow_ctrl_rx_expanded_rss_type rss_type = j;
		const unsigned int pti = eth_pattern_type;
		struct rte_flow_actions_template *at;
		struct mlx5_flow_hw_ctrl_rx_table *tmpls = &hw_ctrl_rx->tables[pti][j];
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

		MLX5_ASSERT(eth_pattern_type == MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC ||
			    eth_pattern_type == MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN);

		if (eth_pattern_type == MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC)
			ret = __flow_hw_ctrl_flows_unicast_create(dev, tmpls->tbl, rss_type, addr);
		else
			ret = __flow_hw_ctrl_flows_unicast_vlan_create(dev, tmpls->tbl, rss_type,
								       addr, vlan);
		if (ret) {
			DRV_LOG(ERR, "port %u Failed to create unicast control flow rule.",
				dev->data->port_id);
			return ret;
		}
	}

	return 0;
}

int
mlx5_flow_hw_ctrl_flow_dmac(struct rte_eth_dev *dev,
			    const struct rte_ether_addr *addr)
{
	return mlx5_flow_hw_ctrl_flow_single(dev, MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC,
					     addr, 0);
}


int
mlx5_flow_hw_ctrl_flow_dmac_destroy(struct rte_eth_dev *dev,
				    const struct rte_ether_addr *addr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;
	struct mlx5_ctrl_flow_entry *tmp;
	int ret;

	/*
	 * HWS does not have automatic RSS flow expansion,
	 * so each variant of the control flow rule is a separate entry in the list.
	 * In that case, the whole list must be traversed.
	 */
	entry = LIST_FIRST(&priv->hw_ctrl_flows);
	while (entry != NULL) {
		tmp = LIST_NEXT(entry, next);

		if (entry->info.type != MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC ||
		    !rte_is_same_ether_addr(addr, &entry->info.uc.dmac)) {
			entry = tmp;
			continue;
		}

		ret = flow_hw_destroy_ctrl_flow(dev, entry->flow);
		LIST_REMOVE(entry, next);
		mlx5_free(entry);
		if (ret)
			return ret;

		entry = tmp;
	}
	return 0;
}

int
mlx5_flow_hw_ctrl_flow_dmac_vlan(struct rte_eth_dev *dev,
				 const struct rte_ether_addr *addr,
				 const uint16_t vlan)
{
	return mlx5_flow_hw_ctrl_flow_single(dev, MLX5_FLOW_HW_CTRL_RX_ETH_PATTERN_DMAC_VLAN,
					     addr, vlan);
}

int
mlx5_flow_hw_ctrl_flow_dmac_vlan_destroy(struct rte_eth_dev *dev,
					 const struct rte_ether_addr *addr,
					 const uint16_t vlan)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_ctrl_flow_entry *entry;
	struct mlx5_ctrl_flow_entry *tmp;
	int ret;

	/*
	 * HWS does not have automatic RSS flow expansion,
	 * so each variant of the control flow rule is a separate entry in the list.
	 * In that case, the whole list must be traversed.
	 */
	entry = LIST_FIRST(&priv->hw_ctrl_flows);
	while (entry != NULL) {
		tmp = LIST_NEXT(entry, next);

		if (entry->info.type != MLX5_CTRL_FLOW_TYPE_DEFAULT_RX_RSS_UNICAST_DMAC_VLAN ||
		    !rte_is_same_ether_addr(addr, &entry->info.uc.dmac) ||
		    vlan != entry->info.uc.vlan) {
			entry = tmp;
			continue;
		}

		ret = flow_hw_destroy_ctrl_flow(dev, entry->flow);
		LIST_REMOVE(entry, next);
		mlx5_free(entry);
		if (ret)
			return ret;

		entry = tmp;
	}
	return 0;
}

struct mlx5_ecpri_parser_profile *
flow_hw_get_ecpri_parser_profile(void *dr_ctx)
{
	uint16_t port_id;
	bool found = false;
	struct mlx5_priv *priv;

	MLX5_ETH_FOREACH_DEV(port_id, NULL) {
		priv = rte_eth_devices[port_id].data->dev_private;
		if (priv->dr_ctx == dr_ctx) {
			found = true;
			break;
		}
	}
	if (found)
		return &priv->sh->ecpri_parser;
	rte_errno = ENODEV;
	return NULL;
}


static __rte_always_inline uint32_t
mlx5_reformat_domain_to_tbl_type(const struct rte_flow_indir_action_conf *domain,
				 bool fdb_unified_en)
{
	uint32_t tbl_type;

	if (domain->transfer)
		tbl_type = (fdb_unified_en ?
			   (MLX5DR_ACTION_FLAG_HWS_FDB_RX |
			    MLX5DR_ACTION_FLAG_HWS_FDB_TX |
			    MLX5DR_ACTION_FLAG_HWS_FDB_UNIFIED) :
			   MLX5DR_ACTION_FLAG_HWS_FDB);
	else if (domain->egress)
		tbl_type = MLX5DR_ACTION_FLAG_HWS_TX;
	else if (domain->ingress)
		tbl_type = MLX5DR_ACTION_FLAG_HWS_RX;
	else
		tbl_type = UINT32_MAX;
	return tbl_type;
}

static struct mlx5_hw_encap_decap_action *
__mlx5_reformat_create(struct rte_eth_dev *dev,
		       const struct rte_flow_action_raw_encap *encap_conf,
		       const struct rte_flow_indir_action_conf *domain,
		       enum mlx5dr_action_type type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_encap_decap_action *handle;
	struct mlx5dr_action_reformat_header hdr;
	uint32_t flags;
	bool unified_fdb = is_unified_fdb(priv);

	flags = mlx5_reformat_domain_to_tbl_type(domain, unified_fdb);
	flags |= (uint32_t)MLX5DR_ACTION_FLAG_SHARED;
	if (flags == UINT32_MAX) {
		DRV_LOG(ERR, "Reformat: invalid indirect action configuration");
		return NULL;
	}
	/* Allocate new list entry. */
	handle = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*handle), 0, SOCKET_ID_ANY);
	if (!handle) {
		DRV_LOG(ERR, "Reformat: failed to allocate reformat entry");
		return NULL;
	}
	handle->action_type = type;
	hdr.sz = encap_conf ? encap_conf->size : 0;
	hdr.data = encap_conf ? encap_conf->data : NULL;
	handle->action = mlx5dr_action_create_reformat(priv->dr_ctx,
					type, 1, &hdr, 0, flags);
	if (!handle->action) {
		DRV_LOG(ERR, "Reformat: failed to create reformat action");
		mlx5_free(handle);
		return NULL;
	}
	return handle;
}

/**
 * Create mlx5 reformat action.
 *
 * @param[in] dev
 *   Pointer to rte_eth_dev structure.
 * @param[in] conf
 *   Pointer to the indirect action parameters.
 * @param[in] encap_action
 *   Pointer to the raw_encap action configuration.
 * @param[in] decap_action
 *   Pointer to the raw_decap action configuration.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *   A valid shared action handle in case of success, NULL otherwise and
 *   rte_errno is set.
 */
struct mlx5_hw_encap_decap_action*
mlx5_reformat_action_create(struct rte_eth_dev *dev,
			    const struct rte_flow_indir_action_conf *conf,
			    const struct rte_flow_action *encap_action,
			    const struct rte_flow_action *decap_action,
			    struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_encap_decap_action *handle;
	const struct rte_flow_action_raw_encap *encap = NULL;
	const struct rte_flow_action_raw_decap *decap = NULL;
	enum mlx5dr_action_type type = MLX5DR_ACTION_TYP_LAST;

	MLX5_ASSERT(!encap_action || encap_action->type == RTE_FLOW_ACTION_TYPE_RAW_ENCAP);
	MLX5_ASSERT(!decap_action || decap_action->type == RTE_FLOW_ACTION_TYPE_RAW_DECAP);
	if (priv->sh->config.dv_flow_en != 2) {
		rte_flow_error_set(error, ENOTSUP,
				   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
				   "Reformat: hardware does not support");
		return NULL;
	}
	if (!conf || (conf->transfer + conf->egress + conf->ingress != 1)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
				   "Reformat: domain should be specified");
		return NULL;
	}
	if ((encap_action && !encap_action->conf) || (decap_action && !decap_action->conf)) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
				   "Reformat: missed action configuration");
		return NULL;
	}
	if (encap_action && !decap_action) {
		encap = (const struct rte_flow_action_raw_encap *)encap_action->conf;
		if (!encap->size || encap->size > MLX5_ENCAP_MAX_LEN ||
		    encap->size < MLX5_ENCAPSULATION_DECISION_SIZE) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
					   "Reformat: Invalid encap length");
			return NULL;
		}
		type = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L2;
	} else if (decap_action && !encap_action) {
		decap = (const struct rte_flow_action_raw_decap *)decap_action->conf;
		if (!decap->size || decap->size < MLX5_ENCAPSULATION_DECISION_SIZE) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
					   "Reformat: Invalid decap length");
			return NULL;
		}
		type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L2_TO_L2;
	} else if (encap_action && decap_action) {
		decap = (const struct rte_flow_action_raw_decap *)decap_action->conf;
		encap = (const struct rte_flow_action_raw_encap *)encap_action->conf;
		if (decap->size < MLX5_ENCAPSULATION_DECISION_SIZE &&
		    encap->size >= MLX5_ENCAPSULATION_DECISION_SIZE &&
		    encap->size <= MLX5_ENCAP_MAX_LEN) {
			type = MLX5DR_ACTION_TYP_REFORMAT_L2_TO_TNL_L3;
		} else if (decap->size >= MLX5_ENCAPSULATION_DECISION_SIZE &&
			   encap->size < MLX5_ENCAPSULATION_DECISION_SIZE) {
			type = MLX5DR_ACTION_TYP_REFORMAT_TNL_L3_TO_L2;
		} else {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
					   "Reformat: Invalid decap & encap length");
			return NULL;
		}
	} else if (!encap_action && !decap_action) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
				   "Reformat: Invalid decap & encap configurations");
		return NULL;
	}
	if (!priv->dr_ctx) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_ACTION,
				   encap_action, "Reformat: HWS not supported");
		return NULL;
	}
	handle = __mlx5_reformat_create(dev, encap, conf, type);
	if (!handle) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION, encap_action,
				   "Reformat: failed to create indirect action");
		return NULL;
	}
	return handle;
}

/**
 * Destroy the indirect reformat action.
 * Release action related resources on the NIC and the memory.
 * Lock free, (mutex should be acquired by caller).
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 * @param[in] handle
 *   The indirect action list handle to be removed.
 * @param[out] error
 *   Perform verbose error reporting if not NULL. Initialized in case of
 *   error only.
 *
 * @return
 *   0 on success, otherwise negative errno value.
 */
int
mlx5_reformat_action_destroy(struct rte_eth_dev *dev,
			     struct rte_flow_action_list_handle *handle,
			     struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_hw_encap_decap_action *action;

	action = (struct mlx5_hw_encap_decap_action *)handle;
	if (!priv->dr_ctx || !action)
		return rte_flow_error_set(error, ENOTSUP,
					  RTE_FLOW_ERROR_TYPE_ACTION, handle,
					  "Reformat: invalid action handle");
	mlx5dr_action_destroy(action->action);
	mlx5_free(handle);
	return 0;
}

static bool
flow_hw_is_item_masked(const struct rte_flow_item *item)
{
	const uint8_t *byte;
	int size;
	int i;

	if (item->mask == NULL)
		return false;

	switch ((int)item->type) {
	case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
		size = sizeof(struct rte_flow_item_tag);
		break;
	case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
		size = sizeof(struct mlx5_rte_flow_item_sq);
		break;
	default:
		size = rte_flow_conv(RTE_FLOW_CONV_OP_ITEM_MASK, NULL, 0, item, NULL);
		/*
		 * Pattern template items are passed to this function.
		 * These items were already validated, so error is not expected.
		 * Also, if mask is NULL, then spec size is bigger than 0 always.
		 */
		MLX5_ASSERT(size > 0);
	}

	byte = (const uint8_t *)item->mask;
	for (i = 0; i < size; ++i)
		if (byte[i])
			return true;

	return false;
}

static int
flow_hw_validate_rule_pattern(struct rte_eth_dev *dev,
			      const struct rte_flow_template_table *table,
			      const uint8_t pattern_template_idx,
			      const struct rte_flow_item items[],
			      struct rte_flow_error *error)
{
	const struct rte_flow_pattern_template *pt;
	const struct rte_flow_item *pt_item;

	if (pattern_template_idx >= table->nb_item_templates)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Pattern template index out of range");

	pt = table->its[pattern_template_idx];
	pt_item = pt->items;

	/* If any item was prepended, skip it. */
	if (pt->implicit_port || pt->implicit_tag)
		pt_item++;

	for (; pt_item->type != RTE_FLOW_ITEM_TYPE_END; pt_item++, items++) {
		if (pt_item->type != items->type)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
						  items, "Item type does not match the template");

		/*
		 * Assumptions:
		 * - Currently mlx5dr layer contains info on which fields in masks are supported.
		 * - This info is not exposed to PMD directly.
		 * - Because of that, it is assumed that since pattern template is correct,
		 *   then, items' masks in pattern template have nonzero values only in
		 *   supported fields.
		 *   This is known, because a temporary mlx5dr matcher is created during pattern
		 *   template creation to validate the template.
		 * - As a result, it is safe to look for nonzero bytes in mask to determine if
		 *   item spec is needed in a flow rule.
		 */
		if (!flow_hw_is_item_masked(pt_item))
			continue;

		if (items->spec == NULL)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
						  items, "Item spec is required");

		switch (items->type) {
		const struct rte_flow_item_ethdev *ethdev;
		const struct rte_flow_item_tx_queue *tx_queue;
		struct mlx5_txq_ctrl *txq;

		case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
			ethdev = items->spec;
			if (flow_hw_validate_target_port_id(dev, ethdev->port_id)) {
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, items,
							  "Invalid port");
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			tx_queue = items->spec;
			if (mlx5_is_external_txq(dev, tx_queue->tx_queue))
				continue;
			txq = mlx5_txq_get(dev, tx_queue->tx_queue);
			if (!txq)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ITEM_SPEC, items,
							  "Invalid Tx queue");
			mlx5_txq_release(dev, tx_queue->tx_queue);
		default:
			break;
		}
	}

	return 0;
}

static bool
flow_hw_valid_indirect_action_type(const struct rte_flow_action *user_action,
				   const enum rte_flow_action_type expected_type)
{
	uint32_t user_indirect_type = MLX5_INDIRECT_ACTION_TYPE_GET(user_action->conf);
	uint32_t expected_indirect_type;

	switch ((int)expected_type) {
	case RTE_FLOW_ACTION_TYPE_RSS:
	case MLX5_RTE_FLOW_ACTION_TYPE_RSS:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_RSS;
		break;
	case RTE_FLOW_ACTION_TYPE_COUNT:
	case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_COUNT;
		break;
	case RTE_FLOW_ACTION_TYPE_AGE:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_AGE;
		break;
	case RTE_FLOW_ACTION_TYPE_CONNTRACK:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_CT;
		break;
	case RTE_FLOW_ACTION_TYPE_METER_MARK:
	case MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_METER_MARK;
		break;
	case RTE_FLOW_ACTION_TYPE_QUOTA:
		expected_indirect_type = MLX5_INDIRECT_ACTION_TYPE_QUOTA;
		break;
	default:
		return false;
	}

	return user_indirect_type == expected_indirect_type;
}

static int
flow_hw_validate_rule_actions(struct rte_eth_dev *dev,
			      const struct rte_flow_template_table *table,
			      const uint8_t actions_template_idx,
			      const struct rte_flow_action actions[],
			      struct rte_flow_error *error)
{
	const struct rte_flow_actions_template *at;
	const struct mlx5_hw_actions *hw_acts;
	const struct mlx5_action_construct_data *act_data;
	unsigned int idx;

	if (actions_template_idx >= table->nb_action_templates)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Actions template index out of range");

	at = table->ats[actions_template_idx].action_template;
	hw_acts = &table->ats[actions_template_idx].acts;

	for (idx = 0; actions[idx].type != RTE_FLOW_ACTION_TYPE_END; ++idx) {
		const struct rte_flow_action *user_action = &actions[idx];
		const struct rte_flow_action *tmpl_action = &at->orig_actions[idx];

		if (user_action->type != tmpl_action->type)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						  user_action,
						  "Action type does not match type specified in "
						  "actions template");
	}

	/*
	 * Only go through unmasked actions and check if configuration is provided.
	 * Configuration of masked actions is ignored.
	 */
	LIST_FOREACH(act_data, &hw_acts->act_list, next) {
		const struct rte_flow_action *user_action;

		user_action = &actions[act_data->action_src];

		/* Skip actions which do not require conf. */
		switch ((int)act_data->type) {
		case RTE_FLOW_ACTION_TYPE_COUNT:
		case MLX5_RTE_FLOW_ACTION_TYPE_COUNT:
		case MLX5_RTE_FLOW_ACTION_TYPE_METER_MARK:
		case MLX5_RTE_FLOW_ACTION_TYPE_RSS:
			continue;
		default:
			break;
		}

		if (user_action->conf == NULL)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ACTION,
						  user_action,
						  "Action requires configuration");

		switch ((int)user_action->type) {
		enum rte_flow_action_type expected_type;
		const struct rte_flow_action_ethdev *ethdev;
		const struct rte_flow_action_modify_field *mf;

		case RTE_FLOW_ACTION_TYPE_INDIRECT:
			expected_type = act_data->indirect.expected_type;
			if (!flow_hw_valid_indirect_action_type(user_action, expected_type))
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
							  user_action,
							  "Indirect action type does not match "
							  "the type specified in the mask");
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (mlx5_flow_validate_target_queue(dev, user_action, error))
				return -rte_errno;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			if (mlx5_validate_action_rss(dev, user_action, error))
				return -rte_errno;
			break;
		case RTE_FLOW_ACTION_TYPE_MODIFY_FIELD:
			/* TODO: Compare other fields if needed. */
			mf = user_action->conf;
			if (mf->operation != act_data->modify_header.action.operation ||
			    mf->src.field != act_data->modify_header.action.src.field ||
			    mf->dst.field != act_data->modify_header.action.dst.field ||
			    mf->width != act_data->modify_header.action.width)
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
							  user_action,
							  "Modify field configuration does not "
							  "match configuration from actions "
							  "template");
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
			ethdev = user_action->conf;
			if (flow_hw_validate_target_port_id(dev, ethdev->port_id)) {
				return rte_flow_error_set(error, EINVAL,
							  RTE_FLOW_ERROR_TYPE_ACTION_CONF,
							  user_action, "Invalid port");
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

static int
flow_hw_async_op_validate(struct rte_eth_dev *dev,
			  const uint32_t queue,
			  const struct rte_flow_template_table *table,
			  struct rte_flow_error *error)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	MLX5_ASSERT(table != NULL);

	if (table->cfg.external && queue >= priv->hw_attr->nb_queue)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
					  "Incorrect queue");

	return 0;
}

/**
 * Validate user input for rte_flow_async_create() implementation.
 *
 * If RTE_LIBRTE_MLX5_DEBUG macro is not defined, this function is a no-op.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to create the flow.
 * @param[in] table
 *   Pointer to template table.
 * @param[in] rule_index
 *   The item pattern flow follows from the table.
 * @param[in] items
 *   Items with flow spec value.
 * @param[in] pattern_template_index
 *   The item pattern flow follows from the table.
 * @param[in] actions
 *   Action with flow spec value.
 * @param[in] action_template_index
 *   The action pattern flow follows from the table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 if user input is valid.
 *    Negative errno otherwise, rte_errno and error struct is populated.
 */
static int
flow_hw_async_create_validate(struct rte_eth_dev *dev,
			      const uint32_t queue,
			      const struct rte_flow_template_table *table,
			      enum rte_flow_table_insertion_type insertion_type,
			      uint32_t rule_index,
			      const struct rte_flow_item items[],
			      const uint8_t pattern_template_index,
			      const struct rte_flow_action actions[],
			      const uint8_t action_template_index,
			      struct rte_flow_error *error)
{
	if (flow_hw_async_op_validate(dev, queue, table, error))
		return -rte_errno;

	if (insertion_type != table->cfg.attr.insertion_type)
		return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					  NULL, "Flow rule insertion type mismatch with table configuration");

	if (table->cfg.attr.insertion_type != RTE_FLOW_TABLE_INSERTION_TYPE_PATTERN)
		if (rule_index >= table->cfg.attr.nb_flows)
			return rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
						  NULL, "Flow rule index exceeds table size");

	if (table->cfg.attr.insertion_type != RTE_FLOW_TABLE_INSERTION_TYPE_INDEX)
		if (flow_hw_validate_rule_pattern(dev, table, pattern_template_index, items, error))
			return -rte_errno;

	if (flow_hw_validate_rule_actions(dev, table, action_template_index, actions, error))
		return -rte_errno;

	return 0;
}

/**
 * Validate user input for rte_flow_async_update() implementation.
 *
 * If RTE_LIBRTE_MLX5_DEBUG macro is not defined, this function is a no-op.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to create the flow.
 * @param[in] flow
 *   Flow rule to be updated.
 * @param[in] actions
 *   Action with flow spec value.
 * @param[in] action_template_index
 *   The action pattern flow follows from the table.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 if user input is valid.
 *    Negative errno otherwise, rte_errno and error struct is set.
 */
static int
flow_hw_async_update_validate(struct rte_eth_dev *dev,
			      const uint32_t queue,
			      const struct rte_flow_hw *flow,
			      const struct rte_flow_action actions[],
			      const uint8_t action_template_index,
			      struct rte_flow_error *error)
{
	if (flow_hw_async_op_validate(dev, queue, flow->table, error))
		return -rte_errno;

	if (flow_hw_validate_rule_actions(dev, flow->table, action_template_index, actions, error))
		return -rte_errno;

	return 0;
}

/**
 * Validate user input for rte_flow_async_destroy() implementation.
 *
 * If RTE_LIBRTE_MLX5_DEBUG macro is not defined, this function is a no-op.
 *
 * @param[in] dev
 *   Pointer to the rte_eth_dev structure.
 * @param[in] queue
 *   The queue to create the flow.
 * @param[in] flow
 *   Flow rule to be destroyed.
 * @param[out] error
 *   Pointer to error structure.
 *
 * @return
 *    0 if user input is valid.
 *    Negative errno otherwise, rte_errno and error struct is set.
 */
static int
flow_hw_async_destroy_validate(struct rte_eth_dev *dev,
			       const uint32_t queue,
			       const struct rte_flow_hw *flow,
			       struct rte_flow_error *error)
{
	if (flow_hw_async_op_validate(dev, queue, flow->table, error))
		return -rte_errno;

	return 0;
}

static struct rte_flow_fp_ops mlx5_flow_hw_fp_ops = {
	.async_create = flow_hw_async_flow_create,
	.async_create_by_index = flow_hw_async_flow_create_by_index,
	.async_create_by_index_with_pattern = flow_hw_async_flow_create_by_index_with_pattern,
	.async_actions_update = flow_hw_async_flow_update,
	.async_destroy = flow_hw_async_flow_destroy,
	.push = flow_hw_push,
	.pull = flow_hw_pull,
	.async_action_handle_create = flow_hw_action_handle_create,
	.async_action_handle_destroy = flow_hw_action_handle_destroy,
	.async_action_handle_update = flow_hw_action_handle_update,
	.async_action_handle_query = flow_hw_action_handle_query,
	.async_action_handle_query_update = flow_hw_async_action_handle_query_update,
	.async_action_list_handle_create = flow_hw_async_action_list_handle_create,
	.async_action_list_handle_destroy = flow_hw_async_action_list_handle_destroy,
	.async_action_list_handle_query_update =
		flow_hw_async_action_list_handle_query_update,
};

#endif
