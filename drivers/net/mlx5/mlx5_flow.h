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

#include <rte_atomic.h>
#include <rte_alarm.h>
#include <rte_mtr.h>

#include <mlx5_glue.h>
#include <mlx5_prm.h>

#include "mlx5.h"

/* Private rte flow items. */
enum mlx5_rte_flow_item_type {
	MLX5_RTE_FLOW_ITEM_TYPE_END = INT_MIN,
	MLX5_RTE_FLOW_ITEM_TYPE_TAG,
	MLX5_RTE_FLOW_ITEM_TYPE_TX_QUEUE,
	MLX5_RTE_FLOW_ITEM_TYPE_VLAN,
};

/* Private (internal) rte flow actions. */
enum mlx5_rte_flow_action_type {
	MLX5_RTE_FLOW_ACTION_TYPE_END = INT_MIN,
	MLX5_RTE_FLOW_ACTION_TYPE_TAG,
	MLX5_RTE_FLOW_ACTION_TYPE_MARK,
	MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
	MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS,
};

/* Matches on selected register. */
struct mlx5_rte_flow_item_tag {
	enum modify_reg id;
	uint32_t data;
};

/* Modify selected register. */
struct mlx5_rte_flow_action_set_tag {
	enum modify_reg id;
	uint32_t data;
};

struct mlx5_flow_action_copy_mreg {
	enum modify_reg dst;
	enum modify_reg src;
};

/* Matches on source queue. */
struct mlx5_rte_flow_item_tx_queue {
	uint32_t queue;
};

/* Feature name to allocate metadata register. */
enum mlx5_feature_name {
	MLX5_HAIRPIN_RX,
	MLX5_HAIRPIN_TX,
	MLX5_METADATA_RX,
	MLX5_METADATA_TX,
	MLX5_METADATA_FDB,
	MLX5_FLOW_MARK,
	MLX5_APP_TAG,
	MLX5_COPY_MARK,
	MLX5_MTR_COLOR,
	MLX5_MTR_SFX,
};

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
/* List of tunnel Layer bits continued below. */

/* General pattern items bits. */
#define MLX5_FLOW_ITEM_METADATA (1u << 16)
#define MLX5_FLOW_ITEM_PORT_ID (1u << 17)
#define MLX5_FLOW_ITEM_TAG (1u << 18)
#define MLX5_FLOW_ITEM_MARK (1u << 19)

/* Pattern MISC bits. */
#define MLX5_FLOW_LAYER_ICMP (1u << 20)
#define MLX5_FLOW_LAYER_ICMP6 (1u << 21)
#define MLX5_FLOW_LAYER_GRE_KEY (1u << 22)

/* Pattern tunnel Layer bits (continued). */
#define MLX5_FLOW_LAYER_IPIP (1u << 23)
#define MLX5_FLOW_LAYER_IPV6_ENCAP (1u << 24)
#define MLX5_FLOW_LAYER_NVGRE (1u << 25)
#define MLX5_FLOW_LAYER_GENEVE (1u << 26)

/* Queue items. */
#define MLX5_FLOW_ITEM_TX_QUEUE (1u << 27)

/* Pattern tunnel Layer bits (continued). */
#define MLX5_FLOW_LAYER_GTP (1u << 28)

/* Pattern eCPRI Layer bit. */
#define MLX5_FLOW_LAYER_ECPRI (UINT64_C(1) << 29)

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
	 MLX5_FLOW_LAYER_GRE | MLX5_FLOW_LAYER_NVGRE | MLX5_FLOW_LAYER_MPLS | \
	 MLX5_FLOW_LAYER_IPIP | MLX5_FLOW_LAYER_IPV6_ENCAP | \
	 MLX5_FLOW_LAYER_GENEVE | MLX5_FLOW_LAYER_GTP)

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
#define MLX5_FLOW_ACTION_ENCAP (1u << 22)
#define MLX5_FLOW_ACTION_DECAP (1u << 23)
#define MLX5_FLOW_ACTION_INC_TCP_SEQ (1u << 24)
#define MLX5_FLOW_ACTION_DEC_TCP_SEQ (1u << 25)
#define MLX5_FLOW_ACTION_INC_TCP_ACK (1u << 26)
#define MLX5_FLOW_ACTION_DEC_TCP_ACK (1u << 27)
#define MLX5_FLOW_ACTION_SET_TAG (1ull << 28)
#define MLX5_FLOW_ACTION_MARK_EXT (1ull << 29)
#define MLX5_FLOW_ACTION_SET_META (1ull << 30)
#define MLX5_FLOW_ACTION_METER (1ull << 31)
#define MLX5_FLOW_ACTION_SET_IPV4_DSCP (1ull << 32)
#define MLX5_FLOW_ACTION_SET_IPV6_DSCP (1ull << 33)
#define MLX5_FLOW_ACTION_AGE (1ull << 34)
#define MLX5_FLOW_ACTION_DEFAULT_MISS (1ull << 35)

#define MLX5_FLOW_FATE_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_QUEUE | \
	 MLX5_FLOW_ACTION_RSS | MLX5_FLOW_ACTION_JUMP | \
	 MLX5_FLOW_ACTION_DEFAULT_MISS)

#define MLX5_FLOW_FATE_ESWITCH_ACTIONS \
	(MLX5_FLOW_ACTION_DROP | MLX5_FLOW_ACTION_PORT_ID | \
	 MLX5_FLOW_ACTION_JUMP)


#define MLX5_FLOW_MODIFY_HDR_ACTIONS (MLX5_FLOW_ACTION_SET_IPV4_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV4_DST | \
				      MLX5_FLOW_ACTION_SET_IPV6_SRC | \
				      MLX5_FLOW_ACTION_SET_IPV6_DST | \
				      MLX5_FLOW_ACTION_SET_TP_SRC | \
				      MLX5_FLOW_ACTION_SET_TP_DST | \
				      MLX5_FLOW_ACTION_SET_TTL | \
				      MLX5_FLOW_ACTION_DEC_TTL | \
				      MLX5_FLOW_ACTION_SET_MAC_SRC | \
				      MLX5_FLOW_ACTION_SET_MAC_DST | \
				      MLX5_FLOW_ACTION_INC_TCP_SEQ | \
				      MLX5_FLOW_ACTION_DEC_TCP_SEQ | \
				      MLX5_FLOW_ACTION_INC_TCP_ACK | \
				      MLX5_FLOW_ACTION_DEC_TCP_ACK | \
				      MLX5_FLOW_ACTION_OF_SET_VLAN_VID | \
				      MLX5_FLOW_ACTION_SET_TAG | \
				      MLX5_FLOW_ACTION_MARK_EXT | \
				      MLX5_FLOW_ACTION_SET_META | \
				      MLX5_FLOW_ACTION_SET_IPV4_DSCP | \
				      MLX5_FLOW_ACTION_SET_IPV6_DSCP)

#define MLX5_FLOW_VLAN_ACTIONS (MLX5_FLOW_ACTION_OF_POP_VLAN | \
				MLX5_FLOW_ACTION_OF_PUSH_VLAN)

#define MLX5_FLOW_XCAP_ACTIONS (MLX5_FLOW_ACTION_ENCAP | MLX5_FLOW_ACTION_DECAP)

#ifndef IPPROTO_MPLS
#define IPPROTO_MPLS 137
#endif

/* UDP port number for MPLS */
#define MLX5_UDP_PORT_MPLS 6635

/* UDP port numbers for VxLAN. */
#define MLX5_UDP_PORT_VXLAN 4789
#define MLX5_UDP_PORT_VXLAN_GPE 4790

/* UDP port numbers for GENEVE. */
#define MLX5_UDP_PORT_GENEVE 6081

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

/* IBV hash bits for L3 SRC. */
#define MLX5_L3_SRC_IBV_RX_HASH (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_SRC_IPV6)

/* IBV hash bits for L3 DST. */
#define MLX5_L3_DST_IBV_RX_HASH (IBV_RX_HASH_DST_IPV4 | IBV_RX_HASH_DST_IPV6)

/* IBV hash bits for TCP. */
#define MLX5_TCP_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_TCP | \
			      IBV_RX_HASH_DST_PORT_TCP)

/* IBV hash bits for UDP. */
#define MLX5_UDP_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_UDP | \
			      IBV_RX_HASH_DST_PORT_UDP)

/* IBV hash bits for L4 SRC. */
#define MLX5_L4_SRC_IBV_RX_HASH (IBV_RX_HASH_SRC_PORT_TCP | \
				 IBV_RX_HASH_SRC_PORT_UDP)

/* IBV hash bits for L4 DST. */
#define MLX5_L4_DST_IBV_RX_HASH (IBV_RX_HASH_DST_PORT_TCP | \
				 IBV_RX_HASH_DST_PORT_UDP)

/* Geneve header first 16Bit */
#define MLX5_GENEVE_VER_MASK 0x3
#define MLX5_GENEVE_VER_SHIFT 14
#define MLX5_GENEVE_VER_VAL(a) \
		(((a) >> (MLX5_GENEVE_VER_SHIFT)) & (MLX5_GENEVE_VER_MASK))
#define MLX5_GENEVE_OPTLEN_MASK 0x3F
#define MLX5_GENEVE_OPTLEN_SHIFT 7
#define MLX5_GENEVE_OPTLEN_VAL(a) \
	    (((a) >> (MLX5_GENEVE_OPTLEN_SHIFT)) & (MLX5_GENEVE_OPTLEN_MASK))
#define MLX5_GENEVE_OAMF_MASK 0x1
#define MLX5_GENEVE_OAMF_SHIFT 7
#define MLX5_GENEVE_OAMF_VAL(a) \
		(((a) >> (MLX5_GENEVE_OAMF_SHIFT)) & (MLX5_GENEVE_OAMF_MASK))
#define MLX5_GENEVE_CRITO_MASK 0x1
#define MLX5_GENEVE_CRITO_SHIFT 6
#define MLX5_GENEVE_CRITO_VAL(a) \
		(((a) >> (MLX5_GENEVE_CRITO_SHIFT)) & (MLX5_GENEVE_CRITO_MASK))
#define MLX5_GENEVE_RSVD_MASK 0x3F
#define MLX5_GENEVE_RSVD_VAL(a) ((a) & (MLX5_GENEVE_RSVD_MASK))
/*
 * The length of the Geneve options fields, expressed in four byte multiples,
 * not including the eight byte fixed tunnel.
 */
#define MLX5_GENEVE_OPT_LEN_0 14
#define MLX5_GENEVE_OPT_LEN_1 63

#define MLX5_ENCAPSULATION_DECISION_SIZE (sizeof(struct rte_flow_item_eth) + \
					  sizeof(struct rte_flow_item_ipv4))

/* Software header modify action numbers of a flow. */
#define MLX5_ACT_NUM_MDF_IPV4		1
#define MLX5_ACT_NUM_MDF_IPV6		4
#define MLX5_ACT_NUM_MDF_MAC		2
#define MLX5_ACT_NUM_MDF_VID		1
#define MLX5_ACT_NUM_MDF_PORT		2
#define MLX5_ACT_NUM_MDF_TTL		1
#define MLX5_ACT_NUM_DEC_TTL		MLX5_ACT_NUM_MDF_TTL
#define MLX5_ACT_NUM_MDF_TCPSEQ		1
#define MLX5_ACT_NUM_MDF_TCPACK		1
#define MLX5_ACT_NUM_SET_REG		1
#define MLX5_ACT_NUM_SET_TAG		1
#define MLX5_ACT_NUM_CPY_MREG		MLX5_ACT_NUM_SET_TAG
#define MLX5_ACT_NUM_SET_MARK		MLX5_ACT_NUM_SET_TAG
#define MLX5_ACT_NUM_SET_META		MLX5_ACT_NUM_SET_TAG
#define MLX5_ACT_NUM_SET_DSCP		1

enum mlx5_flow_drv_type {
	MLX5_FLOW_TYPE_MIN,
	MLX5_FLOW_TYPE_DV,
	MLX5_FLOW_TYPE_VERBS,
	MLX5_FLOW_TYPE_MAX,
};

/* Fate action type. */
enum mlx5_flow_fate_type {
	MLX5_FLOW_FATE_NONE, /* Egress flow. */
	MLX5_FLOW_FATE_QUEUE,
	MLX5_FLOW_FATE_JUMP,
	MLX5_FLOW_FATE_PORT_ID,
	MLX5_FLOW_FATE_DROP,
	MLX5_FLOW_FATE_DEFAULT_MISS,
	MLX5_FLOW_FATE_MAX,
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
	/**< Pointer to the next element. */
	struct mlx5_flow_tbl_resource *tbl;
	/**< Pointer to the table(group) the matcher associated with. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *matcher_object; /**< Pointer to DV matcher */
	uint16_t crc; /**< CRC of key. */
	uint16_t priority; /**< Priority of matcher. */
	struct mlx5_flow_dv_match_params mask; /**< Matcher mask. */
};

#define MLX5_ENCAP_MAX_LEN 132

/* Encap/decap resource structure. */
struct mlx5_flow_dv_encap_decap_resource {
	ILIST_ENTRY(uint32_t)next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Encap/decap action object. */
	uint8_t buf[MLX5_ENCAP_MAX_LEN];
	size_t size;
	uint8_t reformat_type;
	uint8_t ft_type;
	uint64_t flags; /**< Flags for RDMA API. */
};

/* Tag resource structure. */
struct mlx5_flow_dv_tag_resource {
	struct mlx5_hlist_entry entry;
	/**< hash list entry for tag resource, tag value as the key. */
	void *action;
	/**< Tag action object. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	uint32_t idx; /**< Index for the index memory pool. */
};

/*
 * Number of modification commands.
 * The maximal actions amount in FW is some constant, and it is 16 in the
 * latest releases. In some old releases, it will be limited to 8.
 * Since there is no interface to query the capacity, the maximal value should
 * be used to allow PMD to create the flow. The validation will be done in the
 * lower driver layer or FW. A failure will be returned if exceeds the maximal
 * supported actions number on the root table.
 * On non-root tables, there is no limitation, but 32 is enough right now.
 */
#define MLX5_MAX_MODIFY_NUM			32
#define MLX5_ROOT_TBL_MODIFY_NUM		16

/* Modify resource structure */
struct mlx5_flow_dv_modify_hdr_resource {
	LIST_ENTRY(mlx5_flow_dv_modify_hdr_resource) next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Modify header action object. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint32_t actions_num; /**< Number of modification actions. */
	uint64_t flags; /**< Flags for RDMA API. */
	struct mlx5_modification_cmd actions[];
	/**< Modification actions. */
};

/* Jump action resource structure. */
struct mlx5_flow_dv_jump_tbl_resource {
	rte_atomic32_t refcnt; /**< Reference counter. */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	void *action; /**< Pointer to the rdma core action. */
};

/* Port ID resource structure. */
struct mlx5_flow_dv_port_id_action_resource {
	ILIST_ENTRY(uint32_t)next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Action object. */
	uint32_t port_id; /**< Port ID value. */
};

/* Push VLAN action resource structure */
struct mlx5_flow_dv_push_vlan_action_resource {
	ILIST_ENTRY(uint32_t)next;
	/* Pointer to next element. */
	rte_atomic32_t refcnt; /**< Reference counter. */
	void *action; /**< Action object. */
	uint8_t ft_type; /**< Flow table type, Rx, Tx or FDB. */
	rte_be32_t vlan_tag; /**< VLAN tag value. */
};

/* Metadata register copy table entry. */
struct mlx5_flow_mreg_copy_resource {
	/*
	 * Hash list entry for copy table.
	 *  - Key is 32/64-bit MARK action ID.
	 *  - MUST be the first entry.
	 */
	struct mlx5_hlist_entry hlist_ent;
	LIST_ENTRY(mlx5_flow_mreg_copy_resource) next;
	/* List entry for device flows. */
	uint32_t refcnt; /* Reference counter. */
	uint32_t appcnt; /* Apply/Remove counter. */
	uint32_t idx;
	uint32_t rix_flow; /* Built flow for copy. */
};

/* Table data structure of the hash organization. */
struct mlx5_flow_tbl_data_entry {
	struct mlx5_hlist_entry entry;
	/**< hash list entry, 64-bits key inside. */
	struct mlx5_flow_tbl_resource tbl;
	/**< flow table resource. */
	LIST_HEAD(matchers, mlx5_flow_dv_matcher) matchers;
	/**< matchers' header associated with the flow table. */
	struct mlx5_flow_dv_jump_tbl_resource jump;
	/**< jump resource, at most one for each table created. */
	uint32_t idx; /**< index for the indexed mempool. */
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
};

/* RSS description. */
struct mlx5_flow_rss_desc {
	uint32_t level;
	uint32_t queue_num; /**< Number of entries in @p queue. */
	uint64_t types; /**< Specific RSS hash types (see ETH_RSS_*). */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	uint16_t queue[]; /**< Destination queues to redirect traffic to. */
};

/* PMD flow priority for tunnel */
#define MLX5_TUNNEL_PRIO_GET(rss_desc) \
	((rss_desc)->level >= 2 ? MLX5_PRIORITY_MAP_L2 : MLX5_PRIORITY_MAP_L4)


/** Device flow handle structure for DV mode only. */
struct mlx5_flow_handle_dv {
	/* Flow DV api: */
	struct mlx5_flow_dv_matcher *matcher; /**< Cache to matcher. */
	struct mlx5_flow_dv_modify_hdr_resource *modify_hdr;
	/**< Pointer to modify header resource in cache. */
	uint32_t rix_encap_decap;
	/**< Index to encap/decap resource in cache. */
	uint32_t rix_push_vlan;
	/**< Index to push VLAN action resource in cache. */
	uint32_t rix_tag;
	/**< Index to the tag action. */
} __rte_packed;

/** Device flow handle structure: used both for creating & destroying. */
struct mlx5_flow_handle {
	SILIST_ENTRY(uint32_t)next;
	struct mlx5_vf_vlan vf_vlan; /**< Structure for VF VLAN workaround. */
	/**< Index to next device flow handle. */
	uint64_t layers;
	/**< Bit-fields of present layers, see MLX5_FLOW_LAYER_*. */
	void *drv_flow; /**< pointer to driver flow object. */
	uint32_t split_flow_id:28; /**< Sub flow unique match flow id. */
	uint32_t mark:1; /**< Metadate rxq mark flag. */
	uint32_t fate_action:3; /**< Fate action type. */
	union {
		uint32_t rix_hrxq; /**< Hash Rx queue object index. */
		uint32_t rix_jump; /**< Index to the jump action resource. */
		uint32_t rix_port_id_action;
		/**< Index to port ID action resource. */
		uint32_t rix_fate;
		/**< Generic value indicates the fate action. */
		uint32_t rix_default_fate;
		/**< Indicates default miss fate action. */
	};
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	struct mlx5_flow_handle_dv dvh;
#endif
} __rte_packed;

/*
 * Size for Verbs device flow handle structure only. Do not use the DV only
 * structure in Verbs. No DV flows attributes will be accessed.
 * Macro offsetof() could also be used here.
 */
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
#define MLX5_FLOW_HANDLE_VERBS_SIZE \
	(sizeof(struct mlx5_flow_handle) - sizeof(struct mlx5_flow_handle_dv))
#else
#define MLX5_FLOW_HANDLE_VERBS_SIZE (sizeof(struct mlx5_flow_handle))
#endif

/*
 * Max number of actions per DV flow.
 * See CREATE_FLOW_MAX_FLOW_ACTIONS_SUPPORTED
 * in rdma-core file providers/mlx5/verbs.c.
 */
#define MLX5_DV_MAX_NUMBER_OF_ACTIONS 8

/** Device flow structure only for DV flow creation. */
struct mlx5_flow_dv_workspace {
	uint32_t group; /**< The group index. */
	uint8_t transfer; /**< 1 if the flow is E-Switch flow. */
	int actions_n; /**< number of actions. */
	void *actions[MLX5_DV_MAX_NUMBER_OF_ACTIONS]; /**< Action list. */
	struct mlx5_flow_dv_encap_decap_resource *encap_decap;
	/**< Pointer to encap/decap resource in cache. */
	struct mlx5_flow_dv_push_vlan_action_resource *push_vlan_res;
	/**< Pointer to push VLAN action resource in cache. */
	struct mlx5_flow_dv_tag_resource *tag_resource;
	/**< pointer to the tag action. */
	struct mlx5_flow_dv_port_id_action_resource *port_id_action;
	/**< Pointer to port ID action resource. */
	struct mlx5_flow_dv_jump_tbl_resource *jump;
	/**< Pointer to the jump action resource. */
	struct mlx5_flow_dv_match_params value;
	/**< Holds the value that the packet is compared to. */
};

/*
 * Maximal Verbs flow specifications & actions size.
 * Some elements are mutually exclusive, but enough space should be allocated.
 * Tunnel cases: 1. Max 2 Ethernet + IP(v6 len > v4 len) + TCP/UDP headers.
 *               2. One tunnel header (exception: GRE + MPLS),
 *                  SPEC length: GRE == tunnel.
 * Actions: 1. 1 Mark OR Flag.
 *          2. 1 Drop (if any).
 *          3. No limitation for counters, but it makes no sense to support too
 *             many counters in a single device flow.
 */
#ifdef HAVE_IBV_DEVICE_MPLS_SUPPORT
#define MLX5_VERBS_MAX_SPEC_SIZE \
		( \
			(2 * (sizeof(struct ibv_flow_spec_eth) + \
			      sizeof(struct ibv_flow_spec_ipv6) + \
			      sizeof(struct ibv_flow_spec_tcp_udp)) + \
			sizeof(struct ibv_flow_spec_gre) + \
			sizeof(struct ibv_flow_spec_mpls)) \
		)
#else
#define MLX5_VERBS_MAX_SPEC_SIZE \
		( \
			(2 * (sizeof(struct ibv_flow_spec_eth) + \
			      sizeof(struct ibv_flow_spec_ipv6) + \
			      sizeof(struct ibv_flow_spec_tcp_udp)) + \
			sizeof(struct ibv_flow_spec_tunnel)) \
		)
#endif

#if defined(HAVE_IBV_DEVICE_COUNTERS_SET_V42) || \
	defined(HAVE_IBV_DEVICE_COUNTERS_SET_V45)
#define MLX5_VERBS_MAX_ACT_SIZE \
		( \
			sizeof(struct ibv_flow_spec_action_tag) + \
			sizeof(struct ibv_flow_spec_action_drop) + \
			sizeof(struct ibv_flow_spec_counter_action) * 4 \
		)
#else
#define MLX5_VERBS_MAX_ACT_SIZE \
		( \
			sizeof(struct ibv_flow_spec_action_tag) + \
			sizeof(struct ibv_flow_spec_action_drop) \
		)
#endif

#define MLX5_VERBS_MAX_SPEC_ACT_SIZE \
		(MLX5_VERBS_MAX_SPEC_SIZE + MLX5_VERBS_MAX_ACT_SIZE)

/** Device flow structure only for Verbs flow creation. */
struct mlx5_flow_verbs_workspace {
	unsigned int size; /**< Size of the attribute. */
	struct ibv_flow_attr attr; /**< Verbs flow attribute buffer. */
	uint8_t specs[MLX5_VERBS_MAX_SPEC_ACT_SIZE];
	/**< Specifications & actions buffer of verbs flow. */
};

/** Maximal number of device sub-flows supported. */
#define MLX5_NUM_MAX_DEV_FLOWS 32

/** Device flow structure. */
struct mlx5_flow {
	struct rte_flow *flow; /**< Pointer to the main flow. */
	uint32_t flow_idx; /**< The memory pool index to the main flow. */
	uint64_t hash_fields; /**< Hash Rx queue hash fields. */
	uint64_t act_flags;
	/**< Bit-fields of detected actions, see MLX5_FLOW_ACTION_*. */
	bool external; /**< true if the flow is created external to PMD. */
	uint8_t ingress; /**< 1 if the flow is ingress. */
	union {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		struct mlx5_flow_dv_workspace dv;
#endif
		struct mlx5_flow_verbs_workspace verbs;
	};
	struct mlx5_flow_handle *handle;
	uint32_t handle_idx; /* Index of the mlx5 flow handle memory. */
};

/* Flow meter state. */
#define MLX5_FLOW_METER_DISABLE 0
#define MLX5_FLOW_METER_ENABLE 1

#define MLX5_MAN_WIDTH 8
/* Modify this value if enum rte_mtr_color changes. */
#define RTE_MTR_DROPPED RTE_COLORS

/* Meter policer statistics */
struct mlx5_flow_policer_stats {
	uint32_t cnt[RTE_COLORS + 1];
	/**< Color counter, extra for drop. */
	uint64_t stats_mask;
	/**< Statistics mask for the colors. */
};

/* Meter table structure. */
struct mlx5_meter_domain_info {
	struct mlx5_flow_tbl_resource *tbl;
	/**< Meter table. */
	struct mlx5_flow_tbl_resource *sfx_tbl;
	/**< Meter suffix table. */
	void *any_matcher;
	/**< Meter color not match default criteria. */
	void *color_matcher;
	/**< Meter color match criteria. */
	void *jump_actn;
	/**< Meter match action. */
	void *policer_rules[RTE_MTR_DROPPED + 1];
	/**< Meter policer for the match. */
};

/* Meter table set for TX RX FDB. */
struct mlx5_meter_domains_infos {
	uint32_t ref_cnt;
	/**< Table user count. */
	struct mlx5_meter_domain_info egress;
	/**< TX meter table. */
	struct mlx5_meter_domain_info ingress;
	/**< RX meter table. */
	struct mlx5_meter_domain_info transfer;
	/**< FDB meter table. */
	void *drop_actn;
	/**< Drop action as not matched. */
	void *count_actns[RTE_MTR_DROPPED + 1];
	/**< Counters for match and unmatched statistics. */
	uint32_t fmp[MLX5_ST_SZ_DW(flow_meter_parameters)];
	/**< Flow meter parameter. */
	size_t fmp_size;
	/**< Flow meter parameter size. */
	void *meter_action;
	/**< Flow meter action. */
};

/* Meter parameter structure. */
struct mlx5_flow_meter {
	TAILQ_ENTRY(mlx5_flow_meter) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t idx; /* Index to meter object. */
	uint32_t meter_id;
	/**< Meter id. */
	struct mlx5_flow_meter_profile *profile;
	/**< Meter profile parameters. */

	/** Policer actions (per meter output color). */
	enum rte_mtr_policer_action action[RTE_COLORS];

	/** Set of stats counters to be enabled.
	 * @see enum rte_mtr_stats_type
	 */
	uint64_t stats_mask;

	/**< Rule applies to ingress traffic. */
	uint32_t ingress:1;

	/**< Rule applies to egress traffic. */
	uint32_t egress:1;
	/**
	 * Instead of simply matching the properties of traffic as it would
	 * appear on a given DPDK port ID, enabling this attribute transfers
	 * a flow rule to the lowest possible level of any device endpoints
	 * found in the pattern.
	 *
	 * When supported, this effectively enables an application to
	 * re-route traffic not necessarily intended for it (e.g. coming
	 * from or addressed to different physical ports, VFs or
	 * applications) at the device level.
	 *
	 * It complements the behavior of some pattern items such as
	 * RTE_FLOW_ITEM_TYPE_PHY_PORT and is meaningless without them.
	 *
	 * When transferring flow rules, ingress and egress attributes keep
	 * their original meaning, as if processing traffic emitted or
	 * received by the application.
	 */
	uint32_t transfer:1;
	struct mlx5_meter_domains_infos *mfts;
	/**< Flow table created for this meter. */
	struct mlx5_flow_policer_stats policer_stats;
	/**< Meter policer statistics. */
	uint32_t ref_cnt;
	/**< Use count. */
	uint32_t active_state:1;
	/**< Meter state. */
	uint32_t shared:1;
	/**< Meter shared or not. */
};

/* RFC2697 parameter structure. */
struct mlx5_flow_meter_srtcm_rfc2697_prm {
	/* green_saturation_value = cbs_mantissa * 2^cbs_exponent */
	uint32_t cbs_exponent:5;
	uint32_t cbs_mantissa:8;
	/* cir = 8G * cir_mantissa * 1/(2^cir_exponent) Bytes/Sec */
	uint32_t cir_exponent:5;
	uint32_t cir_mantissa:8;
	/* yellow _saturation_value = ebs_mantissa * 2^ebs_exponent */
	uint32_t ebs_exponent:5;
	uint32_t ebs_mantissa:8;
};

/* Flow meter profile structure. */
struct mlx5_flow_meter_profile {
	TAILQ_ENTRY(mlx5_flow_meter_profile) next;
	/**< Pointer to the next flow meter structure. */
	uint32_t meter_profile_id; /**< Profile id. */
	struct rte_mtr_meter_profile profile; /**< Profile detail. */
	union {
		struct mlx5_flow_meter_srtcm_rfc2697_prm srtcm_prm;
		/**< srtcm_rfc2697 struct. */
	};
	uint32_t ref_cnt; /**< Use count. */
};

/* Fdir flow structure */
struct mlx5_fdir_flow {
	LIST_ENTRY(mlx5_fdir_flow) next; /* Pointer to the next element. */
	struct mlx5_fdir *fdir; /* Pointer to fdir. */
	uint32_t rix_flow; /* Index to flow. */
};

#define HAIRPIN_FLOW_ID_BITS 28

/* Flow structure. */
struct rte_flow {
	ILIST_ENTRY(uint32_t)next; /**< Index to the next flow structure. */
	uint32_t dev_handles;
	/**< Device flow handles that are part of the flow. */
	uint32_t drv_type:2; /**< Driver type. */
	uint32_t fdir:1; /**< Identifier of associated FDIR if any. */
	uint32_t hairpin_flow_id:HAIRPIN_FLOW_ID_BITS;
	/**< The flow id used for hairpin. */
	uint32_t copy_applied:1; /**< The MARK copy Flow os applied. */
	uint32_t rix_mreg_copy;
	/**< Index to metadata register copy table resource. */
	uint32_t counter; /**< Holds flow counter. */
	uint16_t meter; /**< Holds flow meter id. */
} __rte_packed;

typedef int (*mlx5_flow_validate_t)(struct rte_eth_dev *dev,
				    const struct rte_flow_attr *attr,
				    const struct rte_flow_item items[],
				    const struct rte_flow_action actions[],
				    bool external,
				    int hairpin,
				    struct rte_flow_error *error);
typedef struct mlx5_flow *(*mlx5_flow_prepare_t)
	(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
	 const struct rte_flow_item items[],
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
typedef struct mlx5_meter_domains_infos *(*mlx5_flow_create_mtr_tbls_t)
					    (struct rte_eth_dev *dev,
					     const struct mlx5_flow_meter *fm);
typedef int (*mlx5_flow_destroy_mtr_tbls_t)(struct rte_eth_dev *dev,
					struct mlx5_meter_domains_infos *tbls);
typedef int (*mlx5_flow_create_policer_rules_t)
					(struct rte_eth_dev *dev,
					 struct mlx5_flow_meter *fm,
					 const struct rte_flow_attr *attr);
typedef int (*mlx5_flow_destroy_policer_rules_t)
					(struct rte_eth_dev *dev,
					 const struct mlx5_flow_meter *fm,
					 const struct rte_flow_attr *attr);
typedef uint32_t (*mlx5_flow_counter_alloc_t)
				   (struct rte_eth_dev *dev);
typedef void (*mlx5_flow_counter_free_t)(struct rte_eth_dev *dev,
					 uint32_t cnt);
typedef int (*mlx5_flow_counter_query_t)(struct rte_eth_dev *dev,
					 uint32_t cnt,
					 bool clear, uint64_t *pkts,
					 uint64_t *bytes);
typedef int (*mlx5_flow_get_aged_flows_t)
					(struct rte_eth_dev *dev,
					 void **context,
					 uint32_t nb_contexts,
					 struct rte_flow_error *error);
struct mlx5_flow_driver_ops {
	mlx5_flow_validate_t validate;
	mlx5_flow_prepare_t prepare;
	mlx5_flow_translate_t translate;
	mlx5_flow_apply_t apply;
	mlx5_flow_remove_t remove;
	mlx5_flow_destroy_t destroy;
	mlx5_flow_query_t query;
	mlx5_flow_create_mtr_tbls_t create_mtr_tbls;
	mlx5_flow_destroy_mtr_tbls_t destroy_mtr_tbls;
	mlx5_flow_create_policer_rules_t create_policer_rules;
	mlx5_flow_destroy_policer_rules_t destroy_policer_rules;
	mlx5_flow_counter_alloc_t counter_alloc;
	mlx5_flow_counter_free_t counter_free;
	mlx5_flow_counter_query_t counter_query;
	mlx5_flow_get_aged_flows_t get_aged_flows;
};

/* mlx5_flow.c */

struct mlx5_flow_id_pool *mlx5_flow_id_pool_alloc(uint32_t max_id);
void mlx5_flow_id_pool_release(struct mlx5_flow_id_pool *pool);
uint32_t mlx5_flow_id_get(struct mlx5_flow_id_pool *pool, uint32_t *id);
uint32_t mlx5_flow_id_release(struct mlx5_flow_id_pool *pool,
			      uint32_t id);
int mlx5_flow_group_to_table(const struct rte_flow_attr *attributes,
			     bool external, uint32_t group, bool fdb_def_rule,
			     uint32_t *table, struct rte_flow_error *error);
uint64_t mlx5_flow_hashfields_adjust(struct mlx5_flow_rss_desc *rss_desc,
				     int tunnel, uint64_t layer_types,
				     uint64_t hash_fields);
int mlx5_flow_discover_priorities(struct rte_eth_dev *dev);
uint32_t mlx5_flow_adjust_priority(struct rte_eth_dev *dev, int32_t priority,
				   uint32_t subpriority);
int mlx5_flow_get_reg_id(struct rte_eth_dev *dev,
				     enum mlx5_feature_name feature,
				     uint32_t id,
				     struct rte_flow_error *error);
const struct rte_flow_action *mlx5_flow_find_action
					(const struct rte_flow_action *actions,
					 enum rte_flow_action_type action);
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
int mlx5_flow_validate_action_default_miss(uint64_t action_flags,
				const struct rte_flow_attr *attr,
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
int mlx5_flow_validate_item_gre_key(const struct rte_flow_item *item,
				    uint64_t item_flags,
				    const struct rte_flow_item *gre_item,
				    struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv4(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t last_item,
				 uint16_t ether_type,
				 const struct rte_flow_item_ipv4 *acc_mask,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_ipv6(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint64_t last_item,
				 uint16_t ether_type,
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
				 struct rte_eth_dev *dev,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  struct rte_flow_error *error);
int mlx5_flow_validate_item_vxlan_gpe(const struct rte_flow_item *item,
				      uint64_t item_flags,
				      struct rte_eth_dev *dev,
				      struct rte_flow_error *error);
int mlx5_flow_validate_item_icmp(const struct rte_flow_item *item,
				 uint64_t item_flags,
				 uint8_t target_protocol,
				 struct rte_flow_error *error);
int mlx5_flow_validate_item_icmp6(const struct rte_flow_item *item,
				   uint64_t item_flags,
				   uint8_t target_protocol,
				   struct rte_flow_error *error);
int mlx5_flow_validate_item_nvgre(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  uint8_t target_protocol,
				  struct rte_flow_error *error);
int mlx5_flow_validate_item_geneve(const struct rte_flow_item *item,
				   uint64_t item_flags,
				   struct rte_eth_dev *dev,
				   struct rte_flow_error *error);
int mlx5_flow_validate_item_ecpri(const struct rte_flow_item *item,
				  uint64_t item_flags,
				  uint64_t last_item,
				  uint16_t ether_type,
				  const struct rte_flow_item_ecpri *acc_mask,
				  struct rte_flow_error *error);
struct mlx5_meter_domains_infos *mlx5_flow_create_mtr_tbls
					(struct rte_eth_dev *dev,
					 const struct mlx5_flow_meter *fm);
int mlx5_flow_destroy_mtr_tbls(struct rte_eth_dev *dev,
			       struct mlx5_meter_domains_infos *tbl);
int mlx5_flow_create_policer_rules(struct rte_eth_dev *dev,
				   struct mlx5_flow_meter *fm,
				   const struct rte_flow_attr *attr);
int mlx5_flow_destroy_policer_rules(struct rte_eth_dev *dev,
				    struct mlx5_flow_meter *fm,
				    const struct rte_flow_attr *attr);
int mlx5_flow_meter_flush(struct rte_eth_dev *dev,
			  struct rte_mtr_error *error);
#endif /* RTE_PMD_MLX5_FLOW_H_ */
