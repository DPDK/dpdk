/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 ZTE Corporation
 */

#ifndef ZXDH_FLOW_H
#define ZXDH_FLOW_H

#include <stddef.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_ether.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_sctp.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_byteorder.h>
#include <rte_flow_driver.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_GROUP                  1
#define ZXDH_MAX_FLOW_NUM          2048
#define MAX_FLOW_COUNT_NUM         ZXDH_MAX_FLOW_NUM
#define ZXDH_FLOW_GROUP_TCAM       1

#ifndef IPv4_BYTES
#define IPv4_BYTES_FMT "%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8
#define IPv4_BYTES(addr) \
		(uint8_t)(((addr) >> 24) & 0xFF),\
		(uint8_t)(((addr) >> 16) & 0xFF),\
		(uint8_t)(((addr) >> 8) & 0xFF),\
		(uint8_t)((addr) & 0xFF)
#endif

#ifndef IPv6_BYTES
#define IPv6_BYTES_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:" \
						"%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define IPv6_BYTES(addr) \
	addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], \
	addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]
#endif

enum {
	FD_ACTION_VXLAN_ENCAP = 0,
	FD_ACTION_VXLAN_DECAP = 1,
	FD_ACTION_RSS_BIT = 2,
	FD_ACTION_COUNT_BIT = 3,
	FD_ACTION_DROP_BIT = 4,
	FD_ACTION_MARK_BIT = 5,
	FD_ACTION_QUEUE_BIT = 6,
};

/**
 * HW table, little-endian as default
 **/
struct fd_flow_key {
	struct rte_ether_addr mac_dst; /**< Destination MAC. */
	struct rte_ether_addr mac_src; /**< Source MAC. */
	rte_be16_t ether_type; /**< EtherType  */
	union {
		struct {
			rte_be16_t cvlan_pri:4; /**< vlanid 0xfff is valid */
			rte_be16_t cvlan_vlanid:12; /**< vlanid 0xfff is valid */
		};
		rte_be16_t  vlan_tci;
	};

	uint8_t  src_ip[16];  /** ip src  */
	uint8_t  dst_ip[16];  /** ip dst  */
	uint8_t  rsv0;
	union {
		uint8_t  tos;
		uint8_t  tc;
	};
	uint8_t  nw_proto;
	uint8_t  frag_flag;/*1表示分片 0 表示非分片*/
	rte_be16_t  tp_src;
	rte_be16_t  tp_dst;

	uint8_t rsv1;/**/
	uint8_t vni[3];/**/

	rte_be16_t vfid;
	uint8_t rsv2[18];
};

struct fd_flow_result {
	rte_le16_t qid;
	uint8_t rsv0;

	uint8_t action_idx:7;
	uint8_t hit_flag:1;

	rte_le32_t mark_fd_id;
	rte_le32_t countid:20;
	rte_le32_t encap1_index:12;

	rte_le16_t encap0_index:12;
	rte_le16_t rsv1:4;
	uint8_t rss_hash_factor;
	uint8_t rss_hash_alg;
};

struct fd_flow_entry {
	struct fd_flow_key key;
	struct fd_flow_key key_mask;
	struct fd_flow_result result;
};

struct flow_stats {
	uint32_t hit_pkts_hi;
	uint32_t hit_pkts_lo;
	uint32_t hit_bytes_hi;
	uint32_t hit_bytes_lo;
};


enum dh_flow_type {
	 FLOW_TYPE_FLOW = 0,
	 FLOW_TYPE_FD_TCAM,
	 FLOW_TYPE_FD_SW,
};

struct zxdh_flow_info {
	enum dh_flow_type flowtype;
	uint16_t hw_idx;
	uint16_t rsv;
	union {
		struct fd_flow_entry fd_flow;
	};
};

struct tunnel_encap_ip {
	rte_be32_t ip_addr[4];
};

struct tunnel_encap0 {
	uint8_t tos;
	uint8_t rsv2[2];
	uint8_t rsv1: 6;
	uint8_t ethtype: 1;
	uint8_t hit_flag: 1;
	uint16_t dst_mac1;
	uint16_t tp_dst;
	uint32_t dst_mac2;
	uint32_t ttl:8;
	uint32_t vni:24;
	struct tunnel_encap_ip dip;
};

struct tunnel_encap1 {
	uint32_t rsv1: 31;
	uint32_t hit_flag: 1;
	uint16_t src_mac1;
	uint16_t vlan_tci;
	uint32_t src_mac2;
	uint32_t rsv;
	struct tunnel_encap_ip sip;
};

struct zxdh_flow {
	uint8_t direct; /* 0 in 1 out */
	uint8_t group;  /* rule group id */
	uint8_t pri; /* priority */
	uint8_t hash_search_index; /*  */
	struct zxdh_flow_info  flowentry;
	struct tunnel_encap0  encap0;
	struct tunnel_encap1  encap1;
};
TAILQ_HEAD(dh_flow_list, rte_flow);

struct rte_flow {
	TAILQ_ENTRY(rte_flow) next;
	void *driver_flow;
	uint32_t type;
	uint16_t port_id;
};

struct count_res {
	rte_spinlock_t count_lock;
	uint8_t count_ref;
	uint8_t rev[3];
};

/* Struct to store engine created. */
struct dh_flow_engine {
	TAILQ_ENTRY(dh_flow_engine) node;
	enum dh_flow_type type;
	int (*apply)
		(struct rte_eth_dev *dev,
		 struct zxdh_flow *dh_flow,
		 struct rte_flow_error *error,
		 uint16_t vport, uint16_t pcieid);

	int (*parse_pattern_action)
		(struct rte_eth_dev *dev,
		 const struct rte_flow_attr *attr,
		 const struct rte_flow_item pattern[],
		 const struct rte_flow_action *actions,
		 struct rte_flow_error *error,
		 struct zxdh_flow *dh_flow);

	int (*destroy)
		(struct rte_eth_dev *dev,
		 struct zxdh_flow *dh_flow,
		 struct rte_flow_error *error,
		 uint16_t vport, uint16_t pcieid);

	int (*query_count)
		(struct rte_eth_dev *dev,
		 struct zxdh_flow *dh_flow,
		 struct rte_flow_query_count *count,
		 struct rte_flow_error *error);
};
TAILQ_HEAD(dh_engine_list, dh_flow_engine);

void zxdh_register_flow_engine(struct dh_flow_engine *engine);

extern const struct rte_flow_ops zxdh_flow_ops;

void zxdh_flow_global_init(void);
void zxdh_flow_init(struct rte_eth_dev *dev);
int pf_fd_hw_apply(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
				 struct rte_flow_error *error, uint16_t vport, uint16_t pcieid);
int pf_fd_hw_destroy(struct rte_eth_dev *dev, struct zxdh_flow *dh_flow,
				 struct rte_flow_error *error, uint16_t vport, uint16_t pcieid);
int pf_fd_hw_query_count(struct rte_eth_dev *dev,
						struct zxdh_flow *flow,
						struct rte_flow_query_count *count,
						struct rte_flow_error *error);
int zxdh_flow_ops_get(struct rte_eth_dev *dev, const struct rte_flow_ops **ops);
void zxdh_flow_release(struct rte_eth_dev *dev);

#endif /* ZXDH_FLOW_H */
