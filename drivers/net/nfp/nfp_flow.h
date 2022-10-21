/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOW_H_
#define _NFP_FLOW_H_

#include <rte_bitops.h>
#include <ethdev_driver.h>

#define NFP_FLOWER_LAYER_EXT_META       RTE_BIT32(0)
#define NFP_FLOWER_LAYER_PORT           RTE_BIT32(1)
#define NFP_FLOWER_LAYER_MAC            RTE_BIT32(2)
#define NFP_FLOWER_LAYER_TP             RTE_BIT32(3)
#define NFP_FLOWER_LAYER_IPV4           RTE_BIT32(4)
#define NFP_FLOWER_LAYER_IPV6           RTE_BIT32(5)
#define NFP_FLOWER_LAYER_CT             RTE_BIT32(6)
#define NFP_FLOWER_LAYER_VXLAN          RTE_BIT32(7)

#define NFP_FLOWER_LAYER2_GRE           RTE_BIT32(0)
#define NFP_FLOWER_LAYER2_QINQ          RTE_BIT32(4)
#define NFP_FLOWER_LAYER2_GENEVE        RTE_BIT32(5)
#define NFP_FLOWER_LAYER2_GENEVE_OP     RTE_BIT32(6)
#define NFP_FLOWER_LAYER2_TUN_IPV6      RTE_BIT32(7)

#define NFP_FL_META_FLAG_MANAGE_MASK    RTE_BIT32(7)

#define NFP_MASK_TABLE_ENTRIES          1024

/* The maximum action list size (in bytes) supported by the NFP. */
#define NFP_FL_MAX_A_SIZ                1216

/* The firmware expects lengths in units of long words */
#define NFP_FL_LW_SIZ                   2

/* Tunnel ports */
#define NFP_FL_PORT_TYPE_TUN            0x50000000

enum nfp_flower_tun_type {
	NFP_FL_TUN_NONE   = 0,
	NFP_FL_TUN_GRE    = 1,
	NFP_FL_TUN_VXLAN  = 2,
	NFP_FL_TUN_GENEVE = 4,
};

struct nfp_fl_key_ls {
	uint32_t key_layer_two;
	uint8_t key_layer;
	int key_size;
	int act_size;
	uint32_t port;
	uint16_t vlan;
	enum nfp_flower_tun_type tun_type;
};

struct nfp_fl_rule_metadata {
	uint8_t key_len;
	uint8_t mask_len;
	uint8_t act_len;
	uint8_t flags;
	rte_be32_t host_ctx_id;
	rte_be64_t host_cookie __rte_packed;
	rte_be64_t flow_version __rte_packed;
	rte_be32_t shortcut;
};

struct nfp_fl_payload {
	struct nfp_fl_rule_metadata *meta;
	char *unmasked_data;
	char *mask_data;
	char *action_data;
};

#define CIRC_CNT(head, tail, size)     (((head) - (tail)) & ((size) - 1))
#define CIRC_SPACE(head, tail, size)   CIRC_CNT((tail), ((head) + 1), (size))
struct circ_buf {
	uint32_t head;
	uint32_t tail;
	char *buf;
};

#define NFP_FLOWER_MASK_ENTRY_RS        256
#define NFP_FLOWER_MASK_ELEMENT_RS      sizeof(uint8_t)
struct nfp_fl_mask_id {
	struct circ_buf free_list;
	uint8_t init_unallocated;
};

#define NFP_FL_STATS_ELEM_RS            sizeof(uint32_t)
struct nfp_fl_stats_id {
	struct circ_buf free_list;
	uint32_t init_unallocated;
};

#define NFP_FL_STAT_ID_MU_NUM           0xffc00000
#define NFP_FL_STAT_ID_STAT             0x003fffff
struct nfp_fl_stats {
	uint64_t pkts;
	uint64_t bytes;
};

struct nfp_flow_priv {
	uint32_t hash_seed; /**< Hash seed for hash tables in this structure. */
	uint64_t flower_version; /**< Flow version, always increase. */
	/* mask hash table */
	struct nfp_fl_mask_id mask_ids; /**< Entry for mask hash table */
	struct rte_hash *mask_table; /**< Hash table to store mask ids. */
	/* flow hash table */
	struct rte_hash *flow_table; /**< Hash table to store flow rules. */
	/* flow stats */
	uint32_t active_mem_unit; /**< The size of active mem units. */
	uint32_t total_mem_units; /**< The size of total mem units. */
	uint32_t stats_ring_size; /**< The size of stats id ring. */
	struct nfp_fl_stats_id stats_ids; /**< The stats id ring. */
	struct nfp_fl_stats *stats; /**< Store stats of flow. */
	rte_spinlock_t stats_lock; /** < Lock the update of 'stats' field. */
};

struct rte_flow {
	struct nfp_fl_payload payload;
	size_t length;
	uint32_t hash_key;
	bool install_flag;
};

int nfp_flow_priv_init(struct nfp_pf_dev *pf_dev);
void nfp_flow_priv_uninit(struct nfp_pf_dev *pf_dev);
int nfp_net_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);

#endif /* _NFP_FLOW_H_ */
