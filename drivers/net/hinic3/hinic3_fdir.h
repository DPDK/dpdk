/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC3_FDIR_H_
#define _HINIC3_FDIR_H_

#define HINIC3_FLOW_MAX_PATTERN_NUM 16

#define HINIC3_TCAM_DYNAMIC_BLOCK_SIZE 16

#define HINIC3_TCAM_DYNAMIC_MAX_FILTERS 1024

#define HINIC3_PKT_TCAM_DYNAMIC_INDEX_START(block_index) \
	(HINIC3_TCAM_DYNAMIC_BLOCK_SIZE * (block_index))

/* Indicate a traffic filtering rule. */
struct rte_flow {
	TAILQ_ENTRY(rte_flow) node;
	enum rte_filter_type filter_type;
	void *rule;
};

struct hinic3_fdir_rule_key {
	struct rte_eth_ipv4_flow ipv4;
	struct rte_eth_ipv6_flow ipv6;
	struct rte_eth_ipv4_flow inner_ipv4;
	struct rte_eth_ipv6_flow inner_ipv6;
	struct rte_eth_tunnel_flow tunnel;
	uint16_t src_port;
	uint16_t dst_port;
	uint8_t proto;
};

struct hinic3_fdir_filter {
	int tcam_index;
	uint8_t ip_type; /**< Inner ip type. */
	uint8_t outer_ip_type;
	uint8_t tunnel_type;
	struct hinic3_fdir_rule_key key_mask;
	struct hinic3_fdir_rule_key key_spec;
	uint32_t rq_index; /**< Queue assigned when matched. */
};

/* This structure is used to describe a basic filter type. */
struct hinic3_filter_t {
	uint16_t filter_rule_nums;
	enum rte_filter_type filter_type;
	struct rte_eth_ethertype_filter ethertype_filter;
	struct hinic3_fdir_filter fdir_filter;
};

enum hinic3_fdir_tunnel_mode {
	HINIC3_FDIR_TUNNEL_MODE_NORMAL = 0,
	HINIC3_FDIR_TUNNEL_MODE_VXLAN = 1,
};

enum hinic3_fdir_ip_type {
	HINIC3_FDIR_IP_TYPE_IPV4 = 0,
	HINIC3_FDIR_IP_TYPE_IPV6 = 1,
	HINIC3_FDIR_IP_TYPE_ANY = 2,
};

/* Describe the key structure of the TCAM. */
struct hinic3_tcam_key_mem {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN)
	uint32_t rsvd0 : 16;
	uint32_t ip_proto : 8;
	uint32_t tunnel_type : 4;
	uint32_t rsvd1 : 4;

	uint32_t function_id : 15;
	uint32_t ip_type : 1;

	uint32_t sipv4_h : 16;
	uint32_t sipv4_l : 16;

	uint32_t dipv4_h : 16;
	uint32_t dipv4_l : 16;
	uint32_t rsvd2 : 16;

	uint32_t rsvd3;

	uint32_t rsvd4 : 16;
	uint32_t dport : 16;

	uint32_t sport : 16;
	uint32_t rsvd5 : 16;

	uint32_t rsvd6 : 16;
	uint32_t outer_sipv4_h : 16;
	uint32_t outer_sipv4_l : 16;

	uint32_t outer_dipv4_h : 16;
	uint32_t outer_dipv4_l : 16;
	uint32_t vni_h : 16;

	uint32_t vni_l : 16;
	uint32_t rsvd7 : 16;
#else
	uint32_t rsvd1 : 4;
	uint32_t tunnel_type : 4;
	uint32_t ip_proto : 8;
	uint32_t rsvd0 : 16;

	uint32_t sipv4_h : 16;
	uint32_t ip_type : 1;
	uint32_t function_id : 15;

	uint32_t dipv4_h : 16;
	uint32_t sipv4_l : 16;

	uint32_t rsvd2 : 16;
	uint32_t dipv4_l : 16;

	uint32_t rsvd3;

	uint32_t dport : 16;
	uint32_t rsvd4 : 16;

	uint32_t rsvd5 : 16;
	uint32_t sport : 16;

	uint32_t outer_sipv4_h : 16;
	uint32_t rsvd6 : 16;

	uint32_t outer_dipv4_h : 16;
	uint32_t outer_sipv4_l : 16;

	uint32_t vni_h : 16;
	uint32_t outer_dipv4_l : 16;

	uint32_t rsvd7 : 16;
	uint32_t vni_l : 16;
#endif
};

/*
 * Define the IPv6-related TCAM key data structure in common
 * scenarios or IPv6 tunnel scenarios.
 */
struct hinic3_tcam_key_ipv6_mem {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN)
	uint32_t rsvd0 : 16;
	/* Indicates the normal IPv6 nextHdr or inner IPv4/IPv6 next proto. */
	uint32_t ip_proto : 8;
	uint32_t tunnel_type : 4;
	uint32_t outer_ip_type : 1;
	uint32_t rsvd1 : 3;

	uint32_t function_id : 15;
	uint32_t ip_type : 1;
	uint32_t sipv6_key0 : 16;

	uint32_t sipv6_key1 : 16;
	uint32_t sipv6_key2 : 16;

	uint32_t sipv6_key3 : 16;
	uint32_t sipv6_key4 : 16;

	uint32_t sipv6_key5 : 16;
	uint32_t sipv6_key6 : 16;

	uint32_t sipv6_key7 : 16;
	uint32_t dport : 16;

	uint32_t sport : 16;
	uint32_t dipv6_key0 : 16;

	uint32_t dipv6_key1 : 16;
	uint32_t dipv6_key2 : 16;

	uint32_t dipv6_key3 : 16;
	uint32_t dipv6_key4 : 16;

	uint32_t dipv6_key5 : 16;
	uint32_t dipv6_key6 : 16;

	uint32_t dipv6_key7 : 16;
	uint32_t rsvd2 : 16;
#else
	uint32_t rsvd1 : 3;
	uint32_t outer_ip_type : 1;
	uint32_t tunnel_type : 4;
	uint32_t ip_proto : 8;
	uint32_t rsvd0 : 16;

	uint32_t sipv6_key0 : 16;
	uint32_t ip_type : 1;
	uint32_t function_id : 15;

	uint32_t sipv6_key2 : 16;
	uint32_t sipv6_key1 : 16;

	uint32_t sipv6_key4 : 16;
	uint32_t sipv6_key3 : 16;

	uint32_t sipv6_key6 : 16;
	uint32_t sipv6_key5 : 16;

	uint32_t dport : 16;
	uint32_t sipv6_key7 : 16;

	uint32_t dipv6_key0 : 16;
	uint32_t sport : 16;

	uint32_t dipv6_key2 : 16;
	uint32_t dipv6_key1 : 16;

	uint32_t dipv6_key4 : 16;
	uint32_t dipv6_key3 : 16;

	uint32_t dipv6_key6 : 16;
	uint32_t dipv6_key5 : 16;

	uint32_t rsvd2 : 16;
	uint32_t dipv6_key7 : 16;
#endif
};

/*
 * Define the tcam key value data structure related to IPv6 in
 * the VXLAN scenario.
 */
struct hinic3_tcam_key_vxlan_ipv6_mem {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN)
	uint32_t rsvd0 : 16;
	uint32_t ip_proto : 8;
	uint32_t tunnel_type : 4;
	uint32_t rsvd1 : 4;

	uint32_t function_id : 15;
	uint32_t ip_type : 1;
	uint32_t dipv6_key0 : 16;

	uint32_t dipv6_key1 : 16;
	uint32_t dipv6_key2 : 16;

	uint32_t dipv6_key3 : 16;
	uint32_t dipv6_key4 : 16;

	uint32_t dipv6_key5 : 16;
	uint32_t dipv6_key6 : 16;

	uint32_t dipv6_key7 : 16;
	uint32_t dport : 16;

	uint32_t sport : 16;
	uint32_t rsvd2 : 16;

	uint32_t rsvd3 : 16;
	uint32_t outer_sipv4_h : 16;

	uint32_t outer_sipv4_l : 16;
	uint32_t outer_dipv4_h : 16;

	uint32_t outer_dipv4_l : 16;
	uint32_t vni_h : 16;

	uint32_t vni_l : 16;
	uint32_t rsvd4 : 16;
#else
	uint32_t rsvd1 : 4;
	uint32_t tunnel_type : 4;
	uint32_t ip_proto : 8;
	uint32_t rsvd0 : 16;

	uint32_t dipv6_key0 : 16;
	uint32_t ip_type : 1;
	uint32_t function_id : 15;

	uint32_t dipv6_key2 : 16;
	uint32_t dipv6_key1 : 16;

	uint32_t dipv6_key4 : 16;
	uint32_t dipv6_key3 : 16;

	uint32_t dipv6_key6 : 16;
	uint32_t dipv6_key5 : 16;

	uint32_t dport : 16;
	uint32_t dipv6_key7 : 16;

	uint32_t rsvd2 : 16;
	uint32_t sport : 16;

	uint32_t outer_sipv4_h : 16;
	uint32_t rsvd3 : 16;

	uint32_t outer_dipv4_h : 16;
	uint32_t outer_sipv4_l : 16;

	uint32_t vni_h : 16;
	uint32_t outer_dipv4_l : 16;

	uint32_t rsvd4 : 16;
	uint32_t vni_l : 16;
#endif
};

/*
 * TCAM key structure. The two unions indicate the key and mask respectively.
 * The TCAM key is consistent with the TCAM entry.
 */
struct hinic3_tcam_key {
	union {
		struct hinic3_tcam_key_mem key_info;
		struct hinic3_tcam_key_ipv6_mem key_info_ipv6;
		struct hinic3_tcam_key_vxlan_ipv6_mem key_info_vxlan_ipv6;
	};
	union {
		struct hinic3_tcam_key_mem key_mask;
		struct hinic3_tcam_key_ipv6_mem key_mask_ipv6;
		struct hinic3_tcam_key_vxlan_ipv6_mem key_mask_vxlan_ipv6;
	};
};

/* Structure indicates the TCAM filter. */
struct hinic3_tcam_filter {
	TAILQ_ENTRY(hinic3_tcam_filter)
	entries; /**< Filter entry, used for linked list operations. */
	uint16_t dynamic_block_id;	 /**< Dynamic block ID. */
	uint16_t index;			 /**< TCAM index. */
	struct hinic3_tcam_key tcam_key; /**< Indicate TCAM key. */
	uint16_t queue;			 /**< Allocated RX queue. */
};

/* Define a linked list header for storing hinic3_tcam_filter data. */
TAILQ_HEAD(hinic3_tcam_filter_list, hinic3_tcam_filter);

struct hinic3_tcam_dynamic_block {
	TAILQ_ENTRY(hinic3_tcam_dynamic_block) entries;
	uint16_t dynamic_block_id;
	uint16_t dynamic_index_cnt;
	uint8_t dynamic_index[HINIC3_TCAM_DYNAMIC_BLOCK_SIZE];
};

/* Define a linked list header for storing hinic3_tcam_dynamic_block data. */
TAILQ_HEAD(hinic3_tcam_dynamic_filter_list, hinic3_tcam_dynamic_block);

/* Indicate TCAM dynamic block info. */
struct hinic3_tcam_dynamic_block_info {
	struct hinic3_tcam_dynamic_filter_list tcam_dynamic_list;
	uint16_t dynamic_block_cnt;
};

/* Structure is used to store TCAM information. */
struct hinic3_tcam_info {
	struct hinic3_tcam_filter_list tcam_list;
	struct hinic3_tcam_dynamic_block_info tcam_dynamic_info;
};

/* Obtain the upper and lower 16 bits. */
#define HINIC3_32_UPPER_16_BITS(n) ((((n) >> 16)) & 0xffff)
#define HINIC3_32_LOWER_16_BITS(n) ((n) & 0xffff)

/* Number of protocol rules */
#define HINIC3_ARP_RULE_NUM  3
#define HINIC3_RARP_RULE_NUM 1
#define HINIC3_SLOW_RULE_NUM 2
#define HINIC3_LLDP_RULE_NUM 2
#define HINIC3_CNM_RULE_NUM  1
#define HINIC3_ECP_RULE_NUM  2

/* Define Ethernet type. */
#define RTE_ETHER_TYPE_CNM 0x22e7
#define RTE_ETHER_TYPE_ECP 0x8940

/* Protocol type of the data packet. */
enum hinic3_ether_type {
	HINIC3_PKT_TYPE_ARP = 1,
	HINIC3_PKT_TYPE_ARP_REQ,
	HINIC3_PKT_TYPE_ARP_REP,
	HINIC3_PKT_TYPE_RARP,
	HINIC3_PKT_TYPE_LACP,
	HINIC3_PKT_TYPE_LLDP,
	HINIC3_PKT_TYPE_OAM,
	HINIC3_PKT_TYPE_CDCP,
	HINIC3_PKT_TYPE_CNM,
	HINIC3_PKT_TYPE_ECP = 10,

	HINIC3_PKT_UNKNOWN = 31,
};

int hinic3_flow_add_del_fdir_filter(struct rte_eth_dev *dev,
				    struct hinic3_fdir_filter *fdir_filter,
				    bool add);
int hinic3_flow_add_del_ethertype_filter(struct rte_eth_dev *dev,
					 struct rte_eth_ethertype_filter *ethertype_filter,
					 bool add);

void hinic3_free_fdir_filter(struct rte_eth_dev *dev);
int hinic3_enable_rxq_fdir_filter(struct rte_eth_dev *dev, uint32_t queue_id,
				  uint32_t able);
int hinic3_flow_parse_attr(const struct rte_flow_attr *attr,
			   struct rte_flow_error *error);

#endif /**< _HINIC3_FDIR_H_ */
