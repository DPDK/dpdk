/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Hisilicon Limited.
 */

#ifndef _HNS3_FDIR_H_
#define _HNS3_FDIR_H_

#include <rte_flow.h>

struct hns3_fd_key_cfg {
	uint8_t key_sel;
	uint8_t inner_sipv6_word_en;
	uint8_t inner_dipv6_word_en;
	uint8_t outer_sipv6_word_en;
	uint8_t outer_dipv6_word_en;
	uint32_t tuple_active;
	uint32_t meta_data_active;
};

enum HNS3_FD_STAGE {
	HNS3_FD_STAGE_1,
	HNS3_FD_STAGE_2,
	HNS3_FD_STAGE_NUM,
};

enum HNS3_FD_ACTION {
	HNS3_FD_ACTION_ACCEPT_PACKET,
	HNS3_FD_ACTION_DROP_PACKET,
};

struct hns3_fd_cfg {
	uint8_t fd_mode;
	uint16_t max_key_length;
	uint32_t rule_num[HNS3_FD_STAGE_NUM]; /* rule entry number */
	uint16_t cnt_num[HNS3_FD_STAGE_NUM];  /* rule hit counter number */
	struct hns3_fd_key_cfg key_cfg[HNS3_FD_STAGE_NUM];
};

/* OUTER_XXX indicates tuples in tunnel header of tunnel packet
 * INNER_XXX indicate tuples in tunneled header of tunnel packet or
 *           tuples of non-tunnel packet
 */
enum HNS3_FD_TUPLE {
	OUTER_DST_MAC,
	OUTER_SRC_MAC,
	OUTER_VLAN_TAG_FST,
	OUTER_VLAN_TAG_SEC,
	OUTER_ETH_TYPE,
	OUTER_L2_RSV,
	OUTER_IP_TOS,
	OUTER_IP_PROTO,
	OUTER_SRC_IP,
	OUTER_DST_IP,
	OUTER_L3_RSV,
	OUTER_SRC_PORT,
	OUTER_DST_PORT,
	OUTER_L4_RSV,
	OUTER_TUN_VNI,
	OUTER_TUN_FLOW_ID,
	INNER_DST_MAC,
	INNER_SRC_MAC,
	INNER_VLAN_TAG1,
	INNER_VLAN_TAG2,
	INNER_ETH_TYPE,
	INNER_L2_RSV,
	INNER_IP_TOS,
	INNER_IP_PROTO,
	INNER_SRC_IP,
	INNER_DST_IP,
	INNER_L3_RSV,
	INNER_SRC_PORT,
	INNER_DST_PORT,
	INNER_SCTP_TAG,
	MAX_TUPLE,
};

#define VLAN_TAG_NUM_MAX 2
#define VNI_OR_TNI_LEN 3
#define IP_ADDR_LEN    4 /* Length of IPv6 address. */
#define IP_ADDR_KEY_ID 3 /* The last 32bit of IP address as FDIR search key */
#define IPV6_ADDR_WORD_MASK 3 /* The last two word of IPv6 as FDIR search key */

struct hns3_fd_rule_tuples {
	uint8_t src_mac[RTE_ETHER_ADDR_LEN];
	uint8_t dst_mac[RTE_ETHER_ADDR_LEN];
	uint32_t src_ip[IP_ADDR_LEN];
	uint32_t dst_ip[IP_ADDR_LEN];
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t vlan_tag1;
	uint16_t vlan_tag2;
	uint16_t ether_type;
	uint8_t ip_tos;
	uint8_t ip_proto;
	uint32_t sctp_tag;
	uint16_t outer_src_port;
	uint16_t tunnel_type;
	uint16_t outer_ether_type;
	uint8_t outer_proto;
	uint8_t outer_tun_vni[VNI_OR_TNI_LEN];
	uint8_t outer_tun_flow_id;
};

struct hns3_fd_ad_data {
	uint16_t ad_id;
	uint8_t drop_packet;
	uint8_t forward_to_direct_queue;
	uint16_t queue_id;
	uint8_t use_counter;
	uint8_t counter_id;
	uint8_t use_next_stage;
	uint8_t write_rule_id_to_bd;
	uint8_t next_input_key;
	uint16_t rule_id;
};

struct hns3_flow_counter {
	LIST_ENTRY(hns3_flow_counter) next; /* Pointer to the next counter. */
	uint32_t shared:1;   /* Share counter ID with other flow rules. */
	uint32_t ref_cnt:31; /* Reference counter. */
	uint16_t id;   /* Counter ID. */
	uint64_t hits; /* Number of packets matched by the rule. */
};

#define HNS3_RULE_FLAG_FDID		0x1
#define HNS3_RULE_FLAG_VF_ID		0x2
#define HNS3_RULE_FLAG_COUNTER		0x4

struct hns3_fdir_key_conf {
	struct hns3_fd_rule_tuples spec;
	struct hns3_fd_rule_tuples mask;
	uint8_t vlan_num;
	uint8_t outer_vlan_num;
};

struct hns3_fdir_rule {
	struct hns3_fdir_key_conf key_conf;
	uint32_t input_set;
	uint32_t flags;
	uint32_t fd_id; /* APP marked unique value for this rule. */
	uint8_t action;
	/* VF id, avaiblable when flags with HNS3_RULE_FLAG_VF_ID. */
	uint8_t vf_id;
	uint16_t queue_id;
	uint16_t location;
	struct rte_flow_action_count act_cnt;
};

/* FDIR filter list structure */
struct hns3_fdir_rule_ele {
	TAILQ_ENTRY(hns3_fdir_rule_ele) entries;
	struct hns3_fdir_rule fdir_conf;
};

/* rss filter list structure */
struct hns3_rss_conf_ele {
	TAILQ_ENTRY(hns3_rss_conf_ele) entries;
	struct hns3_rss_conf filter_info;
};

/* hns3_flow memory list structure */
struct hns3_flow_mem {
	TAILQ_ENTRY(hns3_flow_mem) entries;
	struct rte_flow *flow;
};

TAILQ_HEAD(hns3_fdir_rule_list, hns3_fdir_rule_ele);
TAILQ_HEAD(hns3_rss_filter_list, hns3_rss_conf_ele);
TAILQ_HEAD(hns3_flow_mem_list, hns3_flow_mem);

struct hns3_process_private {
	struct hns3_fdir_rule_list fdir_list;
	struct hns3_rss_filter_list filter_rss_list;
	struct hns3_flow_mem_list flow_list;
};

/*
 *  A structure used to define fields of a FDIR related info.
 */
struct hns3_fdir_info {
	rte_spinlock_t flows_lock;
	struct hns3_fdir_rule_list fdir_list;
	struct hns3_fdir_rule_ele **hash_map;
	struct rte_hash *hash_handle;
	struct hns3_fd_cfg fd_cfg;
};

struct rte_flow {
	enum rte_filter_type filter_type;
	void *rule;
	uint32_t counter_id;
};
struct hns3_adapter;

int hns3_init_fd_config(struct hns3_adapter *hns);
int hns3_fdir_filter_init(struct hns3_adapter *hns);
void hns3_fdir_filter_uninit(struct hns3_adapter *hns);
int hns3_fdir_filter_program(struct hns3_adapter *hns,
			     struct hns3_fdir_rule *rule, bool del);
int hns3_clear_all_fdir_filter(struct hns3_adapter *hns);
int hns3_get_count(struct hns3_hw *hw, uint32_t id, uint64_t *value);
void hns3_filterlist_init(struct rte_eth_dev *dev);
int hns3_restore_all_fdir_filter(struct hns3_adapter *hns);

#endif /* _HNS3_FDIR_H_ */
