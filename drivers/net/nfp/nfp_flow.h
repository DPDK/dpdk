/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOW_H_
#define _NFP_FLOW_H_

#include <sys/queue.h>
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

/* Compressed HW representation of TCP Flags */
#define NFP_FL_TCP_FLAG_FIN             RTE_BIT32(0)
#define NFP_FL_TCP_FLAG_SYN             RTE_BIT32(1)
#define NFP_FL_TCP_FLAG_RST             RTE_BIT32(2)
#define NFP_FL_TCP_FLAG_PSH             RTE_BIT32(3)
#define NFP_FL_TCP_FLAG_URG             RTE_BIT32(4)

#define NFP_FL_META_FLAG_MANAGE_MASK    RTE_BIT32(7)

#define NFP_FLOWER_MASK_VLAN_CFI        RTE_BIT32(12)

#define NFP_MASK_TABLE_ENTRIES          1024

/* The maximum action list size (in bytes) supported by the NFP. */
#define NFP_FL_MAX_A_SIZ                1216

/* The firmware expects lengths in units of long words */
#define NFP_FL_LW_SIZ                   2

#define NFP_FL_SC_ACT_DROP      0x80000000
#define NFP_FL_SC_ACT_USER      0x7D000000
#define NFP_FL_SC_ACT_POPV      0x6A000000
#define NFP_FL_SC_ACT_NULL      0x00000000

/* GRE Tunnel flags */
#define NFP_FL_GRE_FLAG_KEY         (1 << 2)

/* Action opcodes */
#define NFP_FL_ACTION_OPCODE_OUTPUT             0
#define NFP_FL_ACTION_OPCODE_PUSH_VLAN          1
#define NFP_FL_ACTION_OPCODE_POP_VLAN           2
#define NFP_FL_ACTION_OPCODE_PUSH_MPLS          3
#define NFP_FL_ACTION_OPCODE_POP_MPLS           4
#define NFP_FL_ACTION_OPCODE_USERSPACE          5
#define NFP_FL_ACTION_OPCODE_SET_TUNNEL         6
#define NFP_FL_ACTION_OPCODE_SET_ETHERNET       7
#define NFP_FL_ACTION_OPCODE_SET_MPLS           8
#define NFP_FL_ACTION_OPCODE_SET_IPV4_ADDRS     9
#define NFP_FL_ACTION_OPCODE_SET_IPV4_TTL_TOS   10
#define NFP_FL_ACTION_OPCODE_SET_IPV6_SRC       11
#define NFP_FL_ACTION_OPCODE_SET_IPV6_DST       12
#define NFP_FL_ACTION_OPCODE_SET_IPV6_TC_HL_FL  13
#define NFP_FL_ACTION_OPCODE_SET_UDP            14
#define NFP_FL_ACTION_OPCODE_SET_TCP            15
#define NFP_FL_ACTION_OPCODE_PRE_LAG            16
#define NFP_FL_ACTION_OPCODE_PRE_TUNNEL         17
#define NFP_FL_ACTION_OPCODE_PRE_GS             18
#define NFP_FL_ACTION_OPCODE_GS                 19
#define NFP_FL_ACTION_OPCODE_PUSH_NSH           20
#define NFP_FL_ACTION_OPCODE_POP_NSH            21
#define NFP_FL_ACTION_OPCODE_SET_QUEUE          22
#define NFP_FL_ACTION_OPCODE_CONNTRACK          23
#define NFP_FL_ACTION_OPCODE_METER              24
#define NFP_FL_ACTION_OPCODE_CT_NAT_EXT         25
#define NFP_FL_ACTION_OPCODE_PUSH_GENEVE        26
#define NFP_FL_ACTION_OPCODE_NUM                32

#define NFP_FL_OUT_FLAGS_LAST            RTE_BIT32(15)

/* Tunnel ports */
#define NFP_FL_PORT_TYPE_TUN            0x50000000

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
	/* pre tunnel rule */
	uint16_t pre_tun_cnt; /**< The size of pre tunnel rule */
	uint8_t pre_tun_bitmap[NFP_TUN_PRE_TUN_RULE_LIMIT]; /**< Bitmap of pre tunnel rule */
	struct rte_hash *pre_tun_table; /**< Hash table to store pre tunnel rule */
	/* IPv4 off */
	LIST_HEAD(, nfp_ipv4_addr_entry) ipv4_off_list; /**< Store ipv4 off */
	rte_spinlock_t ipv4_off_lock; /**< Lock the ipv4 off list */
	/* IPv6 off */
	LIST_HEAD(, nfp_ipv6_addr_entry) ipv6_off_list; /**< Store ipv6 off */
	rte_spinlock_t ipv6_off_lock; /**< Lock the ipv6 off list */
	/* neighbor next */
	LIST_HEAD(, nfp_fl_tun)nn_list; /**< Store nn entry */
};

struct rte_flow {
	struct nfp_fl_payload payload;
	struct nfp_fl_tun tun;
	size_t length;
	uint32_t hash_key;
	uint32_t mtr_id;
	uint32_t port_id;
	bool install_flag;
	enum nfp_flow_type type;
};

int nfp_flow_priv_init(struct nfp_pf_dev *pf_dev);
void nfp_flow_priv_uninit(struct nfp_pf_dev *pf_dev);
int nfp_net_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);

#endif /* _NFP_FLOW_H_ */
