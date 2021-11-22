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
	MLX5_RTE_FLOW_ITEM_TYPE_TUNNEL,
};

/* Private (internal) rte flow actions. */
enum mlx5_rte_flow_action_type {
	MLX5_RTE_FLOW_ACTION_TYPE_END = INT_MIN,
	MLX5_RTE_FLOW_ACTION_TYPE_TAG,
	MLX5_RTE_FLOW_ACTION_TYPE_MARK,
	MLX5_RTE_FLOW_ACTION_TYPE_COPY_MREG,
	MLX5_RTE_FLOW_ACTION_TYPE_DEFAULT_MISS,
	MLX5_RTE_FLOW_ACTION_TYPE_TUNNEL_SET,
	MLX5_RTE_FLOW_ACTION_TYPE_AGE,
};

#define MLX5_SHARED_ACTION_TYPE_OFFSET 30

enum {
	MLX5_SHARED_ACTION_TYPE_RSS,
	MLX5_SHARED_ACTION_TYPE_AGE,
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
	MLX5_ASO_FLOW_HIT,
	MLX5_SAMPLE_ID,
};

/* Default queue number. */
#define MLX5_RSSQ_DEFAULT_NUM 16

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

/* IPv6 Fragment Extension Header bit. */
#define MLX5_FLOW_LAYER_OUTER_L3_IPV6_FRAG_EXT (1u << 30)
#define MLX5_FLOW_LAYER_INNER_L3_IPV6_FRAG_EXT (1u << 31)

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
#define MLX5_FLOW_ACTION_SAMPLE (1ull << 36)
#define MLX5_FLOW_ACTION_TUNNEL_SET (1ull << 37)
#define MLX5_FLOW_ACTION_TUNNEL_MATCH (1ull << 38)

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

#define MLX5_ENCAPSULATION_DECISION_SIZE (sizeof(struct rte_ether_hdr) + \
					  sizeof(struct rte_ipv4_hdr))

/* IPv4 fragment_offset field contains relevant data in bits 2 to 15. */
#define MLX5_IPV4_FRAG_OFFSET_MASK \
		(RTE_IPV4_HDR_OFFSET_MASK | RTE_IPV4_HDR_MF_FLAG)

/* Specific item's fields can accept a range of values (using spec and last). */
#define MLX5_ITEM_RANGE_NOT_ACCEPTED	false
#define MLX5_ITEM_RANGE_ACCEPTED	true

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
	MLX5_FLOW_FATE_SHARED_RSS,
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
	struct mlx5_cache_entry entry; /**< Pointer to the next element. */
	struct mlx5_flow_tbl_resource *tbl;
	/**< Pointer to the table(group) the matcher associated with. */
	void *matcher_object; /**< Pointer to DV matcher */
	uint16_t crc; /**< CRC of key. */
	uint16_t priority; /**< Priority of matcher. */
	struct mlx5_flow_dv_match_params mask; /**< Matcher mask. */
};

#define MLX5_ENCAP_MAX_LEN 132

/* Encap/decap resource structure. */
struct mlx5_flow_dv_encap_decap_resource {
	struct mlx5_hlist_entry entry;
	/* Pointer to next element. */
	uint32_t refcnt; /**< Reference counter. */
	void *action;
	/**< Encap/decap action object. */
	uint8_t buf[MLX5_ENCAP_MAX_LEN];
	size_t size;
	uint8_t reformat_type;
	uint8_t ft_type;
	uint64_t flags; /**< Flags for RDMA API. */
	uint32_t idx; /**< Index for the index memory pool. */
};

/* Tag resource structure. */
struct mlx5_flow_dv_tag_resource {
	struct mlx5_hlist_entry entry;
	/**< hash list entry for tag resource, tag value as the key. */
	void *action;
	/**< Tag action object. */
	uint32_t refcnt; /**< Reference counter. */
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
	struct mlx5_hlist_entry entry;
	void *action; /**< Modify header action object. */
	/* Key area for hash list matching: */
	uint8_t ft_type; /**< Flow table type, Rx or Tx. */
	uint32_t actions_num; /**< Number of modification actions. */
	uint64_t flags; /**< Flags for RDMA API. */
	struct mlx5_modification_cmd actions[];
	/**< Modification actions. */
};

/* Modify resource key of the hash organization. */
union mlx5_flow_modify_hdr_key {
	struct {
		uint32_t ft_type:8;	/**< Flow table type, Rx or Tx. */
		uint32_t actions_num:5;	/**< Number of modification actions. */
		uint32_t group:19;	/**< Flow group id. */
		uint32_t cksum;		/**< Actions check sum. */
	};
	uint64_t v64;			/**< full 64bits value of key */
};

/* Jump action resource structure. */
struct mlx5_flow_dv_jump_tbl_resource {
	void *action; /**< Pointer to the rdma core action. */
};

/* Port ID resource structure. */
struct mlx5_flow_dv_port_id_action_resource {
	struct mlx5_cache_entry entry;
	void *action; /**< Action object. */
	uint32_t port_id; /**< Port ID value. */
	uint32_t idx; /**< Indexed pool memory index. */
};

/* Push VLAN action resource structure */
struct mlx5_flow_dv_push_vlan_action_resource {
	struct mlx5_cache_entry entry; /* Cache entry. */
	void *action; /**< Action object. */
	uint8_t ft_type; /**< Flow table type, Rx, Tx or FDB. */
	rte_be32_t vlan_tag; /**< VLAN tag value. */
	uint32_t idx; /**< Indexed pool memory index. */
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
	uint32_t idx;
	uint32_t rix_flow; /* Built flow for copy. */
};

/* Table tunnel parameter. */
struct mlx5_flow_tbl_tunnel_prm {
	const struct mlx5_flow_tunnel *tunnel;
	uint32_t group_id;
	bool external;
};

/* Table data structure of the hash organization. */
struct mlx5_flow_tbl_data_entry {
	struct mlx5_hlist_entry entry;
	/**< hash list entry, 64-bits key inside. */
	struct mlx5_flow_tbl_resource tbl;
	/**< flow table resource. */
	struct mlx5_cache_list matchers;
	/**< matchers' header associated with the flow table. */
	struct mlx5_flow_dv_jump_tbl_resource jump;
	/**< jump resource, at most one for each table created. */
	uint32_t idx; /**< index for the indexed mempool. */
	/**< tunnel offload */
	const struct mlx5_flow_tunnel *tunnel;
	uint32_t group_id;
	bool external;
	bool tunnel_offload; /* Tunnel offlod table or not. */
	bool is_egress; /**< Egress table. */
};

/* Sub rdma-core actions list. */
struct mlx5_flow_sub_actions_list {
	uint32_t actions_num; /**< Number of sample actions. */
	uint64_t action_flags;
	void *dr_queue_action;
	void *dr_tag_action;
	void *dr_cnt_action;
	void *dr_port_id_action;
	void *dr_encap_action;
};

/* Sample sub-actions resource list. */
struct mlx5_flow_sub_actions_idx {
	uint32_t rix_hrxq; /**< Hash Rx queue object index. */
	uint32_t rix_tag; /**< Index to the tag action. */
	uint32_t rix_port_id_action; /**< Index to port ID action resource. */
	uint32_t rix_encap_decap; /**< Index to encap/decap resource. */
};

/* Sample action resource structure. */
struct mlx5_flow_dv_sample_resource {
	struct mlx5_cache_entry entry; /**< Cache entry. */
	union {
		void *verbs_action; /**< Verbs sample action object. */
		void **sub_actions; /**< Sample sub-action array. */
	};
	struct rte_eth_dev *dev; /**< Device registers the action. */
	uint32_t idx; /** Sample object index. */
	uint8_t ft_type; /** Flow Table Type */
	uint32_t ft_id; /** Flow Table Level */
	uint32_t ratio;   /** Sample Ratio */
	uint64_t set_action; /** Restore reg_c0 value */
	void *normal_path_tbl; /** Flow Table pointer */
	void *default_miss; /** default_miss dr_action. */
	struct mlx5_flow_sub_actions_idx sample_idx;
	/**< Action index resources. */
	struct mlx5_flow_sub_actions_list sample_act;
	/**< Action resources. */
};

#define MLX5_MAX_DEST_NUM	2

/* Destination array action resource structure. */
struct mlx5_flow_dv_dest_array_resource {
	struct mlx5_cache_entry entry; /**< Cache entry. */
	uint32_t idx; /** Destination array action object index. */
	uint8_t ft_type; /** Flow Table Type */
	uint8_t num_of_dest; /**< Number of destination actions. */
	struct rte_eth_dev *dev; /**< Device registers the action. */
	void *action; /**< Pointer to the rdma core action. */
	struct mlx5_flow_sub_actions_idx sample_idx[MLX5_MAX_DEST_NUM];
	/**< Action index resources. */
	struct mlx5_flow_sub_actions_list sample_act[MLX5_MAX_DEST_NUM];
	/**< Action resources. */
};

/* Verbs specification header. */
struct ibv_spec_header {
	enum ibv_flow_spec_type type;
	uint16_t size;
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
	uint32_t rix_sample;
	/**< Index to sample action resource in cache. */
	uint32_t rix_dest_array;
	/**< Index to destination array resource in cache. */
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
		uint32_t rix_srss;
		/**< Indicates shared RSS fate action. */
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
	struct mlx5_flow_dv_sample_resource *sample_res;
	/**< Pointer to the sample action resource. */
	struct mlx5_flow_dv_dest_array_resource *dest_array_res;
	/**< Pointer to the destination array resource. */
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

/**
 * tunnel offload rules type
 */
enum mlx5_tof_rule_type {
	MLX5_TUNNEL_OFFLOAD_NONE = 0,
	MLX5_TUNNEL_OFFLOAD_SET_RULE,
	MLX5_TUNNEL_OFFLOAD_MATCH_RULE,
	MLX5_TUNNEL_OFFLOAD_MISS_RULE,
};

/** Device flow structure. */
__extension__
struct mlx5_flow {
	struct rte_flow *flow; /**< Pointer to the main flow. */
	uint32_t flow_idx; /**< The memory pool index to the main flow. */
	uint64_t hash_fields; /**< Hash Rx queue hash fields. */
	uint64_t act_flags;
	/**< Bit-fields of detected actions, see MLX5_FLOW_ACTION_*. */
	bool external; /**< true if the flow is created external to PMD. */
	uint8_t ingress:1; /**< 1 if the flow is ingress. */
	uint8_t skip_scale:1;
	/**< 1 if skip the scale the table with factor. */
	union {
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
		struct mlx5_flow_dv_workspace dv;
#endif
		struct mlx5_flow_verbs_workspace verbs;
	};
	struct mlx5_flow_handle *handle;
	uint32_t handle_idx; /* Index of the mlx5 flow handle memory. */
	const struct mlx5_flow_tunnel *tunnel;
	enum mlx5_tof_rule_type tof_type;
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

	rte_spinlock_t sl; /**< Meter action spinlock. */

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

#define MLX5_MAX_TUNNELS 256
#define MLX5_TNL_MISS_RULE_PRIORITY 3
#define MLX5_TNL_MISS_FDB_JUMP_GRP  0x1234faac

/*
 * When tunnel offload is active, all JUMP group ids are converted
 * using the same method. That conversion is applied both to tunnel and
 * regular rule types.
 * Group ids used in tunnel rules are relative to it's tunnel (!).
 * Application can create number of steer rules, using the same
 * tunnel, with different group id in each rule.
 * Each tunnel stores its groups internally in PMD tunnel object.
 * Groups used in regular rules do not belong to any tunnel and are stored
 * in tunnel hub.
 */

struct mlx5_flow_tunnel {
	LIST_ENTRY(mlx5_flow_tunnel) chain;
	struct rte_flow_tunnel app_tunnel;	/** app tunnel copy */
	uint32_t tunnel_id;			/** unique tunnel ID */
	uint32_t refctn;
	struct rte_flow_action action;
	struct rte_flow_item item;
	struct mlx5_hlist *groups;		/** tunnel groups */
};

/** PMD tunnel related context */
struct mlx5_flow_tunnel_hub {
	/* Tunnels list
	 * Access to the list MUST be MT protected
	 */
	LIST_HEAD(, mlx5_flow_tunnel) tunnels;
	 /* protect access to the tunnels list */
	rte_spinlock_t sl;
	struct mlx5_hlist *groups;		/** non tunnel groups */
};

/* convert jump group to flow table ID in tunnel rules */
struct tunnel_tbl_entry {
	struct mlx5_hlist_entry hash;
	uint32_t flow_table;
};

static inline uint32_t
tunnel_id_to_flow_tbl(uint32_t id)
{
	return id | (1u << 16);
}

static inline uint32_t
tunnel_flow_tbl_to_id(uint32_t flow_tbl)
{
	return flow_tbl & ~(1u << 16);
}

union tunnel_tbl_key {
	uint64_t val;
	struct {
		uint32_t tunnel_id;
		uint32_t group;
	};
};

static inline struct mlx5_flow_tunnel_hub *
mlx5_tunnel_hub(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	return priv->sh->tunnel_hub;
}

static inline bool
is_tunnel_offload_active(const struct rte_eth_dev *dev)
{
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	const struct mlx5_priv *priv = dev->data->dev_private;
	return !!priv->config.dv_miss_info;
#else
	RTE_SET_USED(dev);
	return false;
#endif
}

static inline bool
is_flow_tunnel_match_rule(enum mlx5_tof_rule_type tof_rule_type)
{
	return tof_rule_type == MLX5_TUNNEL_OFFLOAD_MATCH_RULE;
}

static inline bool
is_flow_tunnel_steer_rule(enum mlx5_tof_rule_type tof_rule_type)
{
	return tof_rule_type == MLX5_TUNNEL_OFFLOAD_SET_RULE;
}

static inline const struct mlx5_flow_tunnel *
flow_actions_to_tunnel(const struct rte_flow_action actions[])
{
	return actions[0].conf;
}

static inline const struct mlx5_flow_tunnel *
flow_items_to_tunnel(const struct rte_flow_item items[])
{
	return items[0].spec;
}

/* Flow structure. */
struct rte_flow {
	ILIST_ENTRY(uint32_t)next; /**< Index to the next flow structure. */
	uint32_t dev_handles;
	/**< Device flow handles that are part of the flow. */
	uint32_t drv_type:2; /**< Driver type. */
	uint32_t tunnel:1;
	uint32_t meter:16; /**< Holds flow meter id. */
	uint32_t rix_mreg_copy;
	/**< Index to metadata register copy table resource. */
	uint32_t counter; /**< Holds flow counter. */
	uint32_t tunnel_id;  /**< Tunnel id */
	uint32_t age; /**< Holds ASO age bit index. */
} __rte_packed;

/*
 * Define list of valid combinations of RX Hash fields
 * (see enum ibv_rx_hash_fields).
 */
#define MLX5_RSS_HASH_IPV4 (IBV_RX_HASH_SRC_IPV4 | IBV_RX_HASH_DST_IPV4)
#define MLX5_RSS_HASH_IPV4_TCP \
	(MLX5_RSS_HASH_IPV4 | \
	 IBV_RX_HASH_SRC_PORT_TCP | IBV_RX_HASH_DST_PORT_TCP)
#define MLX5_RSS_HASH_IPV4_UDP \
	(MLX5_RSS_HASH_IPV4 | \
	 IBV_RX_HASH_SRC_PORT_UDP | IBV_RX_HASH_DST_PORT_UDP)
#define MLX5_RSS_HASH_IPV6 (IBV_RX_HASH_SRC_IPV6 | IBV_RX_HASH_DST_IPV6)
#define MLX5_RSS_HASH_IPV6_TCP \
	(MLX5_RSS_HASH_IPV6 | \
	 IBV_RX_HASH_SRC_PORT_TCP | IBV_RX_HASH_DST_PORT_TCP)
#define MLX5_RSS_HASH_IPV6_UDP \
	(MLX5_RSS_HASH_IPV6 | \
	 IBV_RX_HASH_SRC_PORT_UDP | IBV_RX_HASH_DST_PORT_UDP)
#define MLX5_RSS_HASH_IPV4_SRC_ONLY IBV_RX_HASH_SRC_IPV4
#define MLX5_RSS_HASH_IPV4_DST_ONLY IBV_RX_HASH_DST_IPV4
#define MLX5_RSS_HASH_IPV6_SRC_ONLY IBV_RX_HASH_SRC_IPV6
#define MLX5_RSS_HASH_IPV6_DST_ONLY IBV_RX_HASH_DST_IPV6
#define MLX5_RSS_HASH_IPV4_UDP_SRC_ONLY \
	(MLX5_RSS_HASH_IPV4 | IBV_RX_HASH_SRC_PORT_UDP)
#define MLX5_RSS_HASH_IPV4_UDP_DST_ONLY \
	(MLX5_RSS_HASH_IPV4 | IBV_RX_HASH_DST_PORT_UDP)
#define MLX5_RSS_HASH_IPV6_UDP_SRC_ONLY \
	(MLX5_RSS_HASH_IPV6 | IBV_RX_HASH_SRC_PORT_UDP)
#define MLX5_RSS_HASH_IPV6_UDP_DST_ONLY \
	(MLX5_RSS_HASH_IPV6 | IBV_RX_HASH_DST_PORT_UDP)
#define MLX5_RSS_HASH_IPV4_TCP_SRC_ONLY \
	(MLX5_RSS_HASH_IPV4 | IBV_RX_HASH_SRC_PORT_TCP)
#define MLX5_RSS_HASH_IPV4_TCP_DST_ONLY \
	(MLX5_RSS_HASH_IPV4 | IBV_RX_HASH_DST_PORT_TCP)
#define MLX5_RSS_HASH_IPV6_TCP_SRC_ONLY \
	(MLX5_RSS_HASH_IPV6 | IBV_RX_HASH_SRC_PORT_TCP)
#define MLX5_RSS_HASH_IPV6_TCP_DST_ONLY \
	(MLX5_RSS_HASH_IPV6 | IBV_RX_HASH_DST_PORT_TCP)
#define MLX5_RSS_HASH_NONE 0ULL

/* array of valid combinations of RX Hash fields for RSS */
static const uint64_t mlx5_rss_hash_fields[] = {
	MLX5_RSS_HASH_IPV4,
	MLX5_RSS_HASH_IPV4_TCP,
	MLX5_RSS_HASH_IPV4_UDP,
	MLX5_RSS_HASH_IPV6,
	MLX5_RSS_HASH_IPV6_TCP,
	MLX5_RSS_HASH_IPV6_UDP,
	MLX5_RSS_HASH_NONE,
};

/* Shared RSS action structure */
struct mlx5_shared_action_rss {
	ILIST_ENTRY(uint32_t)next; /**< Index to the next RSS structure. */
	uint32_t refcnt; /**< Atomically accessed refcnt. */
	struct rte_flow_action_rss origin; /**< Original rte RSS action. */
	uint8_t key[MLX5_RSS_HASH_KEY_LEN]; /**< RSS hash key. */
	struct mlx5_ind_table_obj *ind_tbl;
	/**< Hash RX queues (hrxq, hrxq_tunnel fields) indirection table. */
	uint32_t hrxq[MLX5_RSS_HASH_FIELDS_LEN];
	/**< Hash RX queue indexes mapped to mlx5_rss_hash_fields */
	rte_spinlock_t action_rss_sl; /**< Shared RSS action spinlock. */
};

struct rte_flow_shared_action {
	uint32_t id;
};

/* Thread specific flow workspace intermediate data. */
struct mlx5_flow_workspace {
	/* If creating another flow in same thread, push new as stack. */
	struct mlx5_flow_workspace *prev;
	struct mlx5_flow_workspace *next;
	uint32_t inuse; /* can't create new flow with current. */
	struct mlx5_flow flows[MLX5_NUM_MAX_DEV_FLOWS];
	struct mlx5_flow_rss_desc rss_desc;
	uint32_t rssq_num; /* Allocated queue num in rss_desc. */
	uint32_t flow_idx; /* Intermediate device flow index. */
};

struct mlx5_flow_split_info {
	uint32_t external:1;
	/**< True if flow is created by request external to PMD. */
	uint32_t prefix_mark:1; /**< Prefix subflow mark flag. */
	uint32_t skip_scale:8; /**< Skip the scale the table with factor. */
	uint32_t flow_idx; /**< This memory pool index to the flow. */
	uint64_t prefix_layers; /**< Prefix subflow layers. */
};

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
typedef int (*mlx5_flow_action_validate_t)
				(struct rte_eth_dev *dev,
				 const struct rte_flow_shared_action_conf *conf,
				 const struct rte_flow_action *action,
				 struct rte_flow_error *error);
typedef struct rte_flow_shared_action *(*mlx5_flow_action_create_t)
				(struct rte_eth_dev *dev,
				 const struct rte_flow_shared_action_conf *conf,
				 const struct rte_flow_action *action,
				 struct rte_flow_error *error);
typedef int (*mlx5_flow_action_destroy_t)
				(struct rte_eth_dev *dev,
				 struct rte_flow_shared_action *action,
				 struct rte_flow_error *error);
typedef int (*mlx5_flow_action_update_t)
			(struct rte_eth_dev *dev,
			 struct rte_flow_shared_action *action,
			 const void *action_conf,
			 struct rte_flow_error *error);
typedef int (*mlx5_flow_action_query_t)
			(struct rte_eth_dev *dev,
			 const struct rte_flow_shared_action *action,
			 void *data,
			 struct rte_flow_error *error);
typedef int (*mlx5_flow_sync_domain_t)
			(struct rte_eth_dev *dev,
			 uint32_t domains,
			 uint32_t flags);

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
	mlx5_flow_action_validate_t action_validate;
	mlx5_flow_action_create_t action_create;
	mlx5_flow_action_destroy_t action_destroy;
	mlx5_flow_action_update_t action_update;
	mlx5_flow_action_query_t action_query;
	mlx5_flow_sync_domain_t sync_domain;
};

/* mlx5_flow.c */

struct mlx5_flow_workspace *mlx5_flow_get_thread_workspace(void);
__extension__
struct flow_grp_info {
	uint64_t external:1;
	uint64_t transfer:1;
	uint64_t fdb_def_rule:1;
	/* force standard group translation */
	uint64_t std_tbl_fix:1;
	uint64_t skip_scale:1;
};

static inline bool
tunnel_use_standard_attr_group_translate
		    (const struct rte_eth_dev *dev,
		     const struct rte_flow_attr *attr,
		     const struct mlx5_flow_tunnel *tunnel,
		     enum mlx5_tof_rule_type tof_rule_type)
{
	bool verdict;

	if (!is_tunnel_offload_active(dev))
		/* no tunnel offload API */
		verdict = true;
	else if (tunnel) {
		/*
		 * OvS will use jump to group 0 in tunnel steer rule.
		 * If tunnel steer rule starts from group 0 (attr.group == 0)
		 * that 0 group must be translated with standard method.
		 * attr.group == 0 in tunnel match rule translated with tunnel
		 * method
		 */
		verdict = !attr->group &&
			  is_flow_tunnel_steer_rule(tof_rule_type);
	} else {
		/*
		 * non-tunnel group translation uses standard method for
		 * root group only: attr.group == 0
		 */
		verdict = !attr->group;
	}

	return verdict;
}

int mlx5_flow_group_to_table(struct rte_eth_dev *dev,
			     const struct mlx5_flow_tunnel *tunnel,
			     uint32_t group, uint32_t *table,
			     const struct flow_grp_info *flags,
			     struct rte_flow_error *error);
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
int mlx5_validate_action_rss(struct rte_eth_dev *dev,
			     const struct rte_flow_action *action,
			     struct rte_flow_error *error);
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
			      bool range_accepted,
			      struct rte_flow_error *error);
int mlx5_flow_validate_item_eth(const struct rte_flow_item *item,
				uint64_t item_flags, bool ext_vlan_sup,
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
				 bool range_accepted,
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
int mlx5_flow_dv_discover_counter_offset_support(struct rte_eth_dev *dev);
int mlx5_shared_action_flush(struct rte_eth_dev *dev);
int mlx5_flow_discover_dr_action_support(struct rte_eth_dev *dev);
void mlx5_release_tunnel_hub(struct mlx5_dev_ctx_shared *sh, uint16_t port_id);
int mlx5_alloc_tunnel_hub(struct mlx5_dev_ctx_shared *sh);

/* Hash list callbacks for flow tables: */
struct mlx5_hlist_entry *flow_dv_tbl_create_cb(struct mlx5_hlist *list,
					       uint64_t key, void *entry_ctx);
void flow_dv_tbl_remove_cb(struct mlx5_hlist *list,
			   struct mlx5_hlist_entry *entry);
struct mlx5_flow_tbl_resource *flow_dv_tbl_resource_get(struct rte_eth_dev *dev,
		uint32_t table_id, uint8_t egress, uint8_t transfer,
		bool external, const struct mlx5_flow_tunnel *tunnel,
		uint32_t group_id, uint8_t dummy, struct rte_flow_error *error);

struct mlx5_hlist_entry *flow_dv_tag_create_cb(struct mlx5_hlist *list,
					       uint64_t key, void *cb_ctx);
void flow_dv_tag_remove_cb(struct mlx5_hlist *list,
			   struct mlx5_hlist_entry *entry);

int flow_dv_modify_match_cb(struct mlx5_hlist *list,
			    struct mlx5_hlist_entry *entry,
			    uint64_t key, void *cb_ctx);
struct mlx5_hlist_entry *flow_dv_modify_create_cb(struct mlx5_hlist *list,
						  uint64_t key, void *ctx);
void flow_dv_modify_remove_cb(struct mlx5_hlist *list,
			      struct mlx5_hlist_entry *entry);

struct mlx5_hlist_entry *flow_dv_mreg_create_cb(struct mlx5_hlist *list,
						uint64_t key, void *ctx);
void flow_dv_mreg_remove_cb(struct mlx5_hlist *list,
			    struct mlx5_hlist_entry *entry);

int flow_dv_encap_decap_match_cb(struct mlx5_hlist *list,
				 struct mlx5_hlist_entry *entry,
				 uint64_t key, void *cb_ctx);
struct mlx5_hlist_entry *flow_dv_encap_decap_create_cb(struct mlx5_hlist *list,
				uint64_t key, void *cb_ctx);
void flow_dv_encap_decap_remove_cb(struct mlx5_hlist *list,
				   struct mlx5_hlist_entry *entry);

int flow_dv_matcher_match_cb(struct mlx5_cache_list *list,
			     struct mlx5_cache_entry *entry, void *ctx);
struct mlx5_cache_entry *flow_dv_matcher_create_cb(struct mlx5_cache_list *list,
		struct mlx5_cache_entry *entry, void *ctx);
void flow_dv_matcher_remove_cb(struct mlx5_cache_list *list,
			       struct mlx5_cache_entry *entry);

int flow_dv_port_id_match_cb(struct mlx5_cache_list *list,
			     struct mlx5_cache_entry *entry, void *cb_ctx);
struct mlx5_cache_entry *flow_dv_port_id_create_cb(struct mlx5_cache_list *list,
		struct mlx5_cache_entry *entry, void *cb_ctx);
void flow_dv_port_id_remove_cb(struct mlx5_cache_list *list,
			       struct mlx5_cache_entry *entry);

int flow_dv_push_vlan_match_cb(struct mlx5_cache_list *list,
			       struct mlx5_cache_entry *entry, void *cb_ctx);
struct mlx5_cache_entry *flow_dv_push_vlan_create_cb
				(struct mlx5_cache_list *list,
				 struct mlx5_cache_entry *entry, void *cb_ctx);
void flow_dv_push_vlan_remove_cb(struct mlx5_cache_list *list,
				 struct mlx5_cache_entry *entry);

int flow_dv_sample_match_cb(struct mlx5_cache_list *list,
			    struct mlx5_cache_entry *entry, void *cb_ctx);
struct mlx5_cache_entry *flow_dv_sample_create_cb
				(struct mlx5_cache_list *list,
				 struct mlx5_cache_entry *entry, void *cb_ctx);
void flow_dv_sample_remove_cb(struct mlx5_cache_list *list,
			      struct mlx5_cache_entry *entry);

int flow_dv_dest_array_match_cb(struct mlx5_cache_list *list,
				struct mlx5_cache_entry *entry, void *cb_ctx);
struct mlx5_cache_entry *flow_dv_dest_array_create_cb
				(struct mlx5_cache_list *list,
				 struct mlx5_cache_entry *entry, void *cb_ctx);
void flow_dv_dest_array_remove_cb(struct mlx5_cache_list *list,
				  struct mlx5_cache_entry *entry);
struct mlx5_aso_age_action *flow_aso_age_get_by_idx(struct rte_eth_dev *dev,
						    uint32_t age_idx);
const struct mlx5_flow_tunnel *
mlx5_get_tof(const struct rte_flow_item *items,
	     const struct rte_flow_action *actions,
	     enum mlx5_tof_rule_type *rule_type);


#endif /* RTE_PMD_MLX5_FLOW_H_ */
