/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_FLOW_H__
#define __NFP_FLOW_H__

#include "nfp_net_common.h"

/* The firmware expects lengths in units of long words */
#define NFP_FL_LW_SIZ                   2

/*
 * Maximum number of items in struct rte_flow_action_vxlan_encap.
 * ETH / IPv4(6) / UDP / VXLAN / END
 */
#define ACTION_VXLAN_ENCAP_ITEMS_NUM 5

struct vxlan_data {
	struct rte_flow_action_vxlan_encap conf;
	struct rte_flow_item items[ACTION_VXLAN_ENCAP_ITEMS_NUM];
};

enum nfp_flower_tun_type {
	NFP_FL_TUN_NONE   = 0,
	NFP_FL_TUN_GRE    = 1,
	NFP_FL_TUN_VXLAN  = 2,
	NFP_FL_TUN_GENEVE = 4,
};

enum nfp_flow_type {
	NFP_FLOW_COMMON,
	NFP_FLOW_ENCAP,
	NFP_FLOW_DECAP,
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

struct nfp_fl_tun {
	LIST_ENTRY(nfp_fl_tun) next;
	uint8_t ref_cnt;
	struct nfp_fl_tun_entry {
		uint8_t v6_flag;
		uint8_t dst_addr[RTE_ETHER_ADDR_LEN];
		uint8_t src_addr[RTE_ETHER_ADDR_LEN];
		union {
			rte_be32_t dst_ipv4;
			uint8_t dst_ipv6[16];
		} dst;
		union {
			rte_be32_t src_ipv4;
			uint8_t src_ipv6[16];
		} src;
	} payload;
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

struct nfp_ipv4_addr_entry {
	LIST_ENTRY(nfp_ipv4_addr_entry) next;
	rte_be32_t ipv4_addr;
	int ref_count;
};

struct nfp_ipv6_addr_entry {
	LIST_ENTRY(nfp_ipv6_addr_entry) next;
	uint8_t ipv6_addr[16];
	int ref_count;
};

#define NFP_TUN_PRE_TUN_RULE_LIMIT  32

struct nfp_flow_priv {
	uint32_t hash_seed; /**< Hash seed for hash tables in this structure. */
	uint64_t flower_version; /**< Flow version, always increase. */

	/* Mask hash table */
	struct nfp_fl_mask_id mask_ids; /**< Entry for mask hash table */
	struct rte_hash *mask_table; /**< Hash table to store mask ids. */

	/* Flow hash table */
	struct rte_hash *flow_table; /**< Hash table to store flow rules. */

	/* Flow stats */
	uint32_t active_mem_unit; /**< The size of active mem units. */
	uint32_t total_mem_units; /**< The size of total mem units. */
	uint32_t stats_ring_size; /**< The size of stats id ring. */
	struct nfp_fl_stats_id stats_ids; /**< The stats id ring. */
	struct nfp_fl_stats *stats; /**< Store stats of flow. */
	rte_spinlock_t stats_lock; /** < Lock the update of 'stats' field. */

	/* Pre tunnel rule */
	uint16_t pre_tun_cnt; /**< The size of pre tunnel rule */
	uint8_t pre_tun_bitmap[NFP_TUN_PRE_TUN_RULE_LIMIT]; /**< Bitmap of pre tunnel rule */
	struct rte_hash *pre_tun_table; /**< Hash table to store pre tunnel rule */

	/* IPv4 off */
	LIST_HEAD(, nfp_ipv4_addr_entry) ipv4_off_list; /**< Store ipv4 off */
	rte_spinlock_t ipv4_off_lock; /**< Lock the ipv4 off list */

	/* IPv6 off */
	LIST_HEAD(, nfp_ipv6_addr_entry) ipv6_off_list; /**< Store ipv6 off */
	rte_spinlock_t ipv6_off_lock; /**< Lock the ipv6 off list */

	/* Neighbor next */
	LIST_HEAD(, nfp_fl_tun)nn_list; /**< Store nn entry */
	/* Conntrack */
	struct rte_hash *ct_zone_table; /**< Hash table to store ct zone entry */
	struct nfp_ct_zone_entry *ct_zone_wc; /**< The wildcard ct zone entry */
	struct rte_hash *ct_map_table; /**< Hash table to store ct map entry */
};

struct rte_flow {
	struct nfp_fl_payload payload;
	struct nfp_fl_tun tun;
	size_t length;
	uint32_t hash_key;
	uint32_t mtr_id;
	uint32_t port_id;
	bool install_flag;
	bool tcp_flag;    /**< Used in the SET_TP_* action */
	bool merge_flag;
	enum nfp_flow_type type;
	uint16_t ref_cnt;
};

/* Forward declaration */
struct nfp_flower_representor;

int nfp_flow_priv_init(struct nfp_pf_dev *pf_dev);
void nfp_flow_priv_uninit(struct nfp_pf_dev *pf_dev);
int nfp_net_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);
bool nfp_flow_inner_item_get(const struct rte_flow_item items[],
		const struct rte_flow_item **inner_item);
struct rte_flow *nfp_flow_process(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		bool validate_flag,
		uint64_t cookie,
		bool install_flag,
		bool merge_flag);
int nfp_flow_table_add_merge(struct nfp_flow_priv *priv,
		struct rte_flow *nfp_flow);
int nfp_flow_teardown(struct nfp_flow_priv *priv,
		struct rte_flow *nfp_flow,
		bool validate_flag);
void nfp_flow_free(struct rte_flow *nfp_flow);
int nfp_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		struct rte_flow_error *error);

#endif /* __NFP_FLOW_H__ */
