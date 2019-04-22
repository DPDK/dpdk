/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_FLOW_H_
#define RTE_PMD_MLX5_FLOW_H_

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

#include "mlx5.h"
#include "mlx5_prm.h"

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

/* General pattern items bits. */
#define MLX5_FLOW_ITEM_METADATA (1u << 16)
#define MLX5_FLOW_ITEM_PORT_ID (1u << 17)

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

/* Layer Masks. */
#define MLX5_FLOW_LAYER_L2 \
	(MLX5_FLOW_LAYER_OUTER_L2 | MLX5_FLOW_LAYER_INNER_L2)
#define MLX5_FLOW_LAYER_L3_IPV4 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV4 | MLX5_FLOW_LAYER_INNER_L3_IPV4)
#define MLX5_FLOW_LAYER_L3_IPV6 \
	(MLX5_FLOW_LAYER_OUTER_L3_IPV6 | MLX5_FLOW_LAYER_INNER_L3_IPV6)
#define MLX5_FLOW_LAYER_L3 \
	(MLX5_FLOW_LAYER_L3_IPV4 | MLX5_FLOW_LAYER_L3_IPV6)
#define MLX5_FLOW_LAYER_L4 \
	(MLX5_FLOW_LAYER_OUTER_L4 | MLX5_FLOW_LAYER_INNER_L4)

/* Actions */
#define MLX5_FLOW_ACTION_DROP (1u << 0)
#define MLX5_FLOW_ACTION_QUEUE (1u << 1)
#define MLX5_FLOW_ACTION_RSS (1u << 2)
#define MLX5_FLOW_ACTION_FLAG (1u << 3)
#define MLX5_FLOW_ACTION_MARK (1u << 4)
#define MLX5_FLOW_ACTION_COUNT (1u << 5)
#define MLX5_FLOW_ACTION_PORT_ID (1u << 6)
#define MLX5_FLOW_ACTION_OF_POP_VLAN (1u << 7)
#define MLX5_FLOW_ACTION_OF_PUSH_VLAN (1u << 8)
#define MLX5_FLOW_ACTION_OF_SET_VLAN_VID (1u << 9)
#define MLX5_FLOW_ACTION_OF_SET_VLAN_PCP (1u << 10)
#define MLX5_FLOW_ACTION_SET_IPV4_SRC (1u << 11)
#define MLX5_FLOW_ACTION_SET_IPV4_DST (1u << 12)
#define MLX5_FLOW_ACTION_SET_IPV6_SRC (1u << 13)
#define MLX5_FLOW_ACTION_SET_IPV6_DST (1u << 14)
#define MLX5_FLOW_ACTION_SET_TP_SRC (1u << 15)
#define MLX5_FLOW_ACTION_SET_TP_DST (1u << 16)
#define MLX5_FLOW_ACTION_JUMP (1u << 17)
#define MLX5_FLOW_ACTION_SET_TTL (1u << 18)
#define MLX5_FLOW_ACTION_DEC_TTL (1u << 19)
#define MLX5_FLOW_ACTION_SET_MAC_SRC (1u << 20)
#define MLX5_FLOW_ACTION_SET_MAC_DST (1u << 21)
#define MLX5_FLOW_ACTION_VXLAN_ENCAP (1u << 22)
#define MLX5_FLOW_ACTION_VXLAN_DECAP (1u << 23)
#define MLX5_FLOW_ACTION_NVGRE_ENCAP (1u << 24)
#define MLX5_FLOW_ACTION_NVGRE_DECAP (1u << 25)
#define MLX5_FLOW_ACTION_RAW_ENCAP (1u << 26)
#define MLX5_FLOW_ACTION_RAW_DECAP (1u << 27)

#define MLX5_FLOW_FATE_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_QUEUE | \
	 MLX5_FLOW_ACTION_RSS | MLX5_FLOW_ACTION_JUMP)

#define MLX5_FLOW_FATE_ESWITCH_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_PORT_ID | \
	 MLX5_FLOW_ACTION_JUMP)

#define MLX5_FLOW_ENCAP_ACTIONS	(MLX5_FLOW_ACTION_VXLAN_ENCAP | \
				 MLX5_FLOW_ACTION_NVGRE_ENCAP | \
				 MLX5_FLOW_ACTION_RAW_ENCAP)

#define MLX5_FLOW_DECAP_ACTIONS	(MLX5_FLOW_ACTION_VXLAN_DECAP | \
				 MLX5_FLOW_ACTION_NVGRE_DECAP | \
				 MLX5_FLOW_ACTION_RAW_DECAP)

#define MLX5_FLOW_MODIFY_HDR_ACTIONS (MLX5_FLOW_ACTION_SET_IPV4_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV4_DST | \
				      MLX5_FLOW_ACTION_SET_IPV6_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV6_DST | \
				      MLX5_FLOW_ACTION_SET_TP_SRC | \
				      MLX5_FLOW_ACTION_SET_TP_DST | \
				      MLX5_FLOW_ACTION_SET_TTL | \
				      MLX5_FLOW_ACTION_DEC_TTL | \
				      MLX5_FLOW_ACTION_SET_MAC_SRC | \
				      MLX5_FLOW_ACTION_SET_MAC_DST)

#ifndef IPPROTO_MPLS
#define IPPROTO_MPLS 137
#endif

/* UDP port number for MPLS */
#define MLX5_UDP_PORT_MPLS 6635

/* UDP port numbers for VxLAN. */
#define MLX5_UDP_PORT_VXLAN 4789
#define MLX5_UDP_PORT_VXLAN_GPE 4790

/* Priority reserved for default flows. */
#define MLX5_FLOW_PRIO_RSVD ((uint32_t)-1)

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

/* Valid layer type for IPV4 RSS. */
#define MLX5_IPV4_LAYER_TYPES \
	(ETH_RSS_IPV4 | ETH_RSS_FRAG_IPV4 | \
	 ETH_RSS_NONFRAG_IPV4_TCP | ETH_RSS_NONFRAG_IPV4_UDP | \
	 ETH_RSS_NONFRAG_IPV4_OTHER)

/* IBV hash source bits  for IPV4. */
#define MLX5_IPV4_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4)

/* Valid layer type for IPV6 RSS. */
#define MLX5_IPV6_LAYER_TYPES \
	(ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6 | ETH_RSS_NONFRAG_IPV6_TCP | \
	 ETH_RSS_NONFRAG_IPV6_UDP | ETH_RSS_IPV6_EX  | ETH_RSS_IPV6_TCP_EX | \
	 ETH_RSS_IPV6_UDP_EX | ETH_RSS_NONFRAG_IPV6_OTHER)

/* IBV hash source bits  for IPV6. */
#define MLX5_IPV6_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6)

enum mlx5_flow_drv_type {
	MLX5_FLOW_TYPE_MIN,
	MLX5_FLOW_TYPE_DV,
	MLX5_FLOW_TYPE_TCF,
	MLX5_FLOW_TYPE_VERBS,
	MLX5_FLOW_TYPE_MAX,
};

/* Matcher PRM representation */
struct mlx5_flow_dv_match_params {
	size_t size;
	/**< Size of match value. Do NOT split size and key! */
	uint32_t buf[MLX5_ST_SZ_DW(fte_match_param)];
	/**< Matcher value. This value is used as the mask or as a key. */
};

/* Matcher structure. */
struct mlx5_flow_dv_matcher {
	LIST_ENTRY(mlx5_flow_dv_matcher) next;
	/* Pointer to the next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *matcher_object; /**< Pointer to DV matcher */
	uint16_t crc; /**< CRC of key. */
	uint16_t priority; /**< Priority of matcher. */
	uint8_t egress; /**< Egress matcher. */
	uint8_t transfer; /**< 1 if the flow is E-Switch flow. */
	uint32_t group; /**< The matcher group. */
	struct mlx5_flow_dv_match_params mask; /**< Matcher mask. */
};

#define MLX5_ENCAP_MAX_LEN 132

/* Encap/decap resource structure. */
struct mlx5_flow_dv_encap_decap_resource {
	LIST_ENTRY(mlx5_flow_dv_encap_decap_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *verbs_action;
	/**< Verbs encap/decap action object. */
	uint8_t buf[MLX5_ENCAP_MAX_LEN];
	size_t size;
	uint8_t reformat_type;
	uint8_t ft_type;
	uint64_t flags; /**< Flags for RDMA API. */
};

/* Tag resource structure. */
struct mlx5_flow_dv_tag_resource {
	LIST_ENTRY(mlx5_flow_dv_tag_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Verbs tag action object. */
	uint32_t tag; /**< the tag value. */
};

/* Number of modification commands. */
#define MLX5_MODIFY_NUM 8

/* Modify resource structure */
struct mlx5_flow_dv_modify_hdr_resource {
	LIST_ENTRY(mlx5_flow_dv_modify_hdr_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	struct ibv_flow_action *verbs_action;
	/**< Verbs modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint32_t actions_num; /**< Number of modification actions. */
	struct mlx5_modification_cmd actions[MLX5_MODIFY_NUM];
	/**< Modification actions. */
	uint64_t flags; /**< Flags for RDMA API. */
};

/* Jump action resource structure. */
struct mlx5_flow_dv_jump_tbl_resource {
	LIST_ENTRY(mlx5_flow_dv_jump_tbl_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action; /**< Pointer to the rdma core action. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	struct mlx5_flow_tbl_resource *tbl; /**< The target table. */
};

/* Port ID resource structure. */
struct mlx5_flow_dv_port_id_action_resource {
	LIST_ENTRY(mlx5_flow_dv_port_id_action_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Verbs tag action object. */
	uint32_t port_id; /**< Port ID value. */
};

/*
 * Max number of actions per DV flow.
 * See CREATE_FLOW_MAX_FLOW_ACTIONS_SUPPORTED
 * In rdma-core file providers/mlx5/verbs.c
 */
#define MLX5_DV_MAX_NUMBER_OF_ACTIONS 8

/* DV flows structure. */
struct mlx5_flow_dv {
	uint64_t hash_fields; /**< Fields that participate in the hash. */
	struct mlx5_hrxq *hrxq; /**< Hash Rx queues. */
	/* Flow DV api: */
	struct mlx5_flow_dv_matcher *matcher; /**< Cache to matcher. */
	struct mlx5_flow_dv_match_params value;
	/**< Holds the value that the packet is compared to. */
	struct mlx5_flow_dv_encap_decap_resource *encap_decap;
	/**< Pointer to encap/decap resource in cache. */
	struct mlx5_flow_dv_modify_hdr_resource *modify_hdr;
	/**< Pointer to modify header resource in cache. */
	struct ibv_flow *flow; /**< Installed flow. */
	struct mlx5_flow_dv_jump_tbl_resource *jump;
	/**< Pointer to the jump action resource. */
	struct mlx5_flow_dv_port_id_action_resource *port_id_action;
	/**< Pointer to port ID action resource. */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	void *actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS];
	/**< Action list. */
#endif
	int actions_n; /**< number of actions. */
};

/** Linux TC flower driver for E-Switch flow. */
struct mlx5_flow_tcf {
	struct nlmsghdr *nlh;
	struct tcmsg *tcm;
	uint32_t *ptc_flags; /**< tc rule applied flags. */
	union { /**< Tunnel encap/decap descriptor. */
		struct flow_tcf_tunnel_hdr *tunnel;
		struct flow_tcf_vxlan_decap *vxlan_decap;
		struct flow_tcf_vxlan_encap *vxlan_encap;
	};
	uint32_t applied:1; /**< Whether rule is currently applied. */
#ifndef NDEBUG
	uint32_t nlsize; /**< Size of NL message buffer for debug check. */
#endif
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
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

/** Device flow structure. */
struct mlx5_flow {
	LIST_ENTRY(mlx5_flow) next;
	struct rte_flow *flow; /**< Pointer to the main flow. */
	uint64_t layers;
	/**< Bit-fields of present layers, see MLX5_FLOW_LAYER_*. */
	union {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		struct mlx5_flow_dv dv;
#endif
		struct mlx5_flow_tcf tcf;
		struct mlx5_flow_verbs verbs;
	};
};

/* Counters information. */
struct mlx5_flow_counter {
	LIST_ENTRY(mlx5_flow_counter) next; /**< Pointer to the next counter. */
	uint32_t shared:1; /**< Share counter ID with other flow rules. */
	uint32_t ref_cnt:31; /**< Reference counter. */
	uint32_t id; /**< Counter ID. */
	union {  /**< Holds the counters for the rule. */
#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42)
		struct ibv_counter_set *cs;
#elif defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
		struct ibv_counters *cs;
#endif
		struct mlx5_devx_counter_set *dcs;
	};
	uint64_t hits; /**< Number of packets matched by the rule. */
	uint64_t bytes; /**< Number of bytes matched by the rule. */
	void *action; /**< Pointer to the dv action. */
};

/* Flow structure. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) next; /**< Pointer to the next flow structure. */
	enum mlx5_flow_drv_type drv_type; /**< Driver type. */
	struct mlx5_flow_counter *counter; /**< Holds flow counter. */
	struct mlx5_flow_dv_tag_resource *tag_resource;
	/**< pointer to the tag action. */
	struct rte_flow_action_rss rss;/**< RSS context. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	uint16_t (*queue)[]; /**< Destination queues to redirect traffic to. */
	LIST_HEAD(dev_flows, mlx5_flow) dev_flows;
	/**< Device flows that are part of the flow. */
	uint64_t actions;
	/**< Bit-fields of detected actions, see MLX5_FLOW_ACTION_*. */
	struct mlx5_fdir *fdir; /**< Pointer to associated FDIR if any. */
	uint8_t ingress; /**< 1 if the flow is ingress. */
	uint32_t group; /**< The group index. */
	uint8_t transfer; /**< 1 if the flow is E-Switch flow. */
};

typedef int (*mlx5_flow_validate_t)(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item items[],
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error);
typedef struct mlx5_flow *(*mlx5_flow_prepare_t)
	(const struct rte_flow_attr *attr, const struct rte_flow_item items[],
	 const struct rte_flow_action actions[], struct rte_flow_error *error);
typedef int (*mlx5_flow_translate_t)(struct rte_eth_dev *dev,
				     struct mlx5_flow *dev_flow,
				     const struct rte_flow_attr *attr,
				     const struct rte_flow_item items[],
				     const struct rte_flow_action actions[],
				     struct rte_flow_error *error);
typedef int (*mlx5_flow_apply_t)(struct rte_eth_dev *dev, struct rte_flow *flow,
				 struct rte_flow_error *error);
typedef void (*mlx5_flow_remove_t)(struct rte_eth_dev *dev,
				   struct rte_flow *flow);
typedef void (*mlx5_flow_destroy_t)(struct rte_eth_dev *dev,
				    struct rte_flow *flow);
typedef int (*mlx5_flow_query_t)(struct rte_eth_dev *dev,
				 struct rte_flow *flow,
				 const struct rte_flow_action *actions,
				 void *data,
				 struct rte_flow_error *error);
struct mlx5_flow_driver_ops {
	mlx5_flow_validate_t validate;
	mlx5_flow_prepare_t prepare;
	mlx5_flow_translate_t translate;
	mlx5_flow_apply_t apply;
	mlx5_flow_remove_t remove;
	mlx5_flow_destroy_t destroy;
	mlx5_flow_query_t query;
};

/* mlx5_flow.c */

uint64_t mlx5_flow_hashfields_adjust(struct mlx5_flow *dev_flow, int tunnel,
				     uint64_t layer_types,
				     uint64_t hash_fields);
uint32_t mlx5_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
				   uint32_t subpriority);
int mlx5_flow_validate_action_count(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    struct rte_flow_error *error);
int mlx5_flow_validate_action_drop(uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_flag(uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_mark(const struct rte_flow_action *action,
				   uint64_t action_flags,
				   const struct rte_flow_attr *attr,
				   struct rte_flow_error *error);
int mlx5_flow_validate_action_queue(const struct rte_flow_action *action,
				    uint64_t action_flags,
				    struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    struct rte_flow_error *error);
int mlx5_flow_validate_action_rss(const struct rte_flow_action *action,
				  uint64_t action_flags,
				  struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attr,
				  uint64_t item_flags,
				  struct rte_flow_error *error);
int mlx5_flow_validate_attributes(struct rte_eth_dev *dev,
				  const struct rte_flow_attr *attributes,
				  struct rte_flow_error *error);
int mlx5_flow_item_acceptable(const struct rte_flow_item *item,
			      const uint8_t *mask,
			      const uint8_t *nic_mask,
			      unsigned int size,
			      struct rte_flow_error *error);
int mlx5_flow_validate_item_eth(const struct rte_flow_item *item,
				uint64_t item_flags,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_gre(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv4(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 const struct rte_flow_item_ipv4 *acc_mask,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv6(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 const struct rte_flow_item_ipv6 *acc_mask,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_mpls(struct rte_eth_dev *dev,
				 const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t prev_layer,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_tcp(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				const struct rte_flow_item_tcp *flow_mask,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_udp(const struct rte_flow_item *item,
				uint64_t item_flags,
				uint8_t target_protocol,
				struct rte_flow_error *error);
int mlx5_flow_validate_item_vlan(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan_gpe(const struct rte_flow_item *item,
				      uint64_t item_flags,
				      struct rte_eth_dev *dev,
				      struct rte_flow_error *error);

/* mlx5_flow_tcf.c */

int mlx5_flow_tcf_init(struct mlx5_flow_tcf_context *ctx,
		       unsigned int ifindex, struct rte_flow_error *error);
struct mlx5_flow_tcf_context *mlx5_flow_tcf_context_create(void);
void mlx5_flow_tcf_context_destroy(struct mlx5_flow_tcf_context *ctx);

#endif /* RTE_PMD_MLX5_FLOW_H_ */
