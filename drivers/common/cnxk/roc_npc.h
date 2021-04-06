/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPC_H_
#define _ROC_NPC_H_

#include <sys/queue.h>

enum roc_npc_item_type {
	ROC_NPC_ITEM_TYPE_VOID,
	ROC_NPC_ITEM_TYPE_ANY,
	ROC_NPC_ITEM_TYPE_ETH,
	ROC_NPC_ITEM_TYPE_VLAN,
	ROC_NPC_ITEM_TYPE_E_TAG,
	ROC_NPC_ITEM_TYPE_IPV4,
	ROC_NPC_ITEM_TYPE_IPV6,
	ROC_NPC_ITEM_TYPE_ARP_ETH_IPV4,
	ROC_NPC_ITEM_TYPE_MPLS,
	ROC_NPC_ITEM_TYPE_ICMP,
	ROC_NPC_ITEM_TYPE_IGMP,
	ROC_NPC_ITEM_TYPE_UDP,
	ROC_NPC_ITEM_TYPE_TCP,
	ROC_NPC_ITEM_TYPE_SCTP,
	ROC_NPC_ITEM_TYPE_ESP,
	ROC_NPC_ITEM_TYPE_GRE,
	ROC_NPC_ITEM_TYPE_NVGRE,
	ROC_NPC_ITEM_TYPE_VXLAN,
	ROC_NPC_ITEM_TYPE_GTPC,
	ROC_NPC_ITEM_TYPE_GTPU,
	ROC_NPC_ITEM_TYPE_GENEVE,
	ROC_NPC_ITEM_TYPE_VXLAN_GPE,
	ROC_NPC_ITEM_TYPE_IPV6_EXT,
	ROC_NPC_ITEM_TYPE_GRE_KEY,
	ROC_NPC_ITEM_TYPE_HIGIG2,
	ROC_NPC_ITEM_TYPE_CPT_HDR,
	ROC_NPC_ITEM_TYPE_L3_CUSTOM,
	ROC_NPC_ITEM_TYPE_QINQ,
	ROC_NPC_ITEM_TYPE_END,
};

struct roc_npc_item_info {
	enum roc_npc_item_type type; /* Item type */
	uint32_t size;		     /* item size */
	const void *spec; /**< Pointer to item specification structure. */
	const void *mask; /**< Bit-mask applied to spec and last. */
	const void *last; /* For range */
};

#define ROC_NPC_MAX_ACTION_COUNT 12

enum roc_npc_action_type {
	ROC_NPC_ACTION_TYPE_END = (1 << 0),
	ROC_NPC_ACTION_TYPE_VOID = (1 << 1),
	ROC_NPC_ACTION_TYPE_MARK = (1 << 2),
	ROC_NPC_ACTION_TYPE_FLAG = (1 << 3),
	ROC_NPC_ACTION_TYPE_DROP = (1 << 4),
	ROC_NPC_ACTION_TYPE_QUEUE = (1 << 5),
	ROC_NPC_ACTION_TYPE_RSS = (1 << 6),
	ROC_NPC_ACTION_TYPE_DUP = (1 << 7),
	ROC_NPC_ACTION_TYPE_SEC = (1 << 8),
	ROC_NPC_ACTION_TYPE_COUNT = (1 << 9),
	ROC_NPC_ACTION_TYPE_PF = (1 << 10),
	ROC_NPC_ACTION_TYPE_VF = (1 << 11),
};

struct roc_npc_action {
	enum roc_npc_action_type type; /**< Action type. */
	const void *conf; /**< Pointer to action configuration object. */
};

struct roc_npc_action_mark {
	uint32_t id; /**< Integer value to return with packets. */
};

struct roc_npc_action_vf {
	uint32_t original : 1;	/**< Use original VF ID if possible. */
	uint32_t reserved : 31; /**< Reserved, must be zero. */
	uint32_t id;		/**< VF ID. */
};

struct roc_npc_action_queue {
	uint16_t index; /**< Queue index to use. */
};

struct roc_npc_attr {
	uint32_t priority;	/**< Rule priority level within group. */
	uint32_t ingress : 1;	/**< Rule applies to ingress traffic. */
	uint32_t egress : 1;	/**< Rule applies to egress traffic. */
	uint32_t reserved : 30; /**< Reserved, must be zero. */
};

struct roc_npc_flow {
	uint8_t nix_intf;
	uint8_t enable;
	uint32_t mcam_id;
	int32_t ctr_id;
	uint32_t priority;
#define ROC_NPC_MAX_MCAM_WIDTH_DWORDS 7
	/* Contiguous match string */
	uint64_t mcam_data[ROC_NPC_MAX_MCAM_WIDTH_DWORDS];
	uint64_t mcam_mask[ROC_NPC_MAX_MCAM_WIDTH_DWORDS];
	uint64_t npc_action;
	uint64_t vtag_action;

	TAILQ_ENTRY(roc_npc_flow) next;
};

enum roc_npc_intf {
	ROC_NPC_INTF_RX = 0,
	ROC_NPC_INTF_TX = 1,
	ROC_NPC_INTF_MAX = 2,
};

struct roc_npc {
	struct roc_nix *roc_nix;
	uint8_t switch_header_type;
	uint16_t flow_prealloc_size;
	uint16_t flow_max_priority;
	uint16_t channel;
	uint16_t pf_func;
	uint64_t kex_capability;
	uint64_t rx_parse_nibble;

#define ROC_NPC_MEM_SZ (5 * 1024)
	uint8_t reserved[ROC_NPC_MEM_SZ];
} __plt_cache_aligned;

#endif /* _ROC_NPC_H_ */
