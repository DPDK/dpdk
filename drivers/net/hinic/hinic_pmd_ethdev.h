/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_ETHDEV_H_
#define _HINIC_PMD_ETHDEV_H_

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

#include "base/hinic_compat.h"
#include "base/hinic_pmd_cfg.h"

#define HINIC_DEV_NAME_LEN	32
#define HINIC_MAX_RX_QUEUES	64

/* mbuf pool for copy invalid mbuf segs */
#define HINIC_COPY_MEMPOOL_DEPTH	128
#define HINIC_COPY_MBUF_SIZE		4096

#define SIZE_8BYTES(size)	(ALIGN((u32)(size), 8) >> 3)

#define HINIC_ETH_DEV_TO_PRIVATE_NIC_DEV(dev) \
	((struct hinic_nic_dev *)(dev)->data->dev_private)

#define HINIC_MAX_QUEUE_DEPTH		4096
#define HINIC_MIN_QUEUE_DEPTH		128
#define HINIC_TXD_ALIGN                 1
#define HINIC_RXD_ALIGN                 1

#define HINIC_UINT32_BIT_SIZE      (CHAR_BIT * sizeof(uint32_t))
#define HINIC_VFTA_SIZE            (4096 / HINIC_UINT32_BIT_SIZE)

enum hinic_dev_status {
	HINIC_DEV_INIT,
	HINIC_DEV_CLOSE,
	HINIC_DEV_START,
	HINIC_DEV_INTR_EN,
};

#define HINIC_MAX_Q_FILTERS	64 /* hinic just support 64 filter types */
#define HINIC_PKT_TYPE_FIND_ID(pkt_type) ((pkt_type) - HINIC_MAX_Q_FILTERS)

/* 5tuple filter info */
struct hinic_5tuple_filter_info {
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t dst_port;
	uint16_t src_port;
	uint8_t proto; /* l4 protocol. */
	/*
	 * seven levels (001b-111b), 111b is highest,
	 * used when more than one filter matches.
	 */
	uint8_t priority;
	uint8_t dst_ip_mask:1, /* if mask is 1b, do not compare dst ip. */
		src_ip_mask:1, /* if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /* if mask is 1b, do not compare dst port. */
		src_port_mask:1, /* if mask is 1b, do not compare src port. */
		proto_mask:1; /* if mask is 1b, do not compare protocol. */
};

/* 5tuple filter structure */
struct hinic_5tuple_filter {
	TAILQ_ENTRY(hinic_5tuple_filter) entries;
	uint16_t index;       /* the index of 5tuple filter */
	struct hinic_5tuple_filter_info filter_info;
	uint16_t queue;       /* rx queue assigned to */
};

TAILQ_HEAD(hinic_5tuple_filter_list, hinic_5tuple_filter);

/*
 * If this filter is added by configuration,
 * it should not be removed.
 */
struct hinic_pkt_filter {
	uint16_t pkt_proto;
	uint8_t qid;
	bool	enable;
};

/* Structure to store filters' info. */
struct hinic_filter_info {
	uint8_t pkt_type;
	uint8_t qid;
	uint64_t type_mask;  /* Bit mask for every used filter */
	struct hinic_5tuple_filter_list fivetuple_list;
	struct hinic_pkt_filter pkt_filters[HINIC_MAX_Q_FILTERS];
};

/* Information about the fdir mode. */
struct hinic_hw_fdir_mask {
	uint32_t src_ipv4_mask;
	uint32_t dst_ipv4_mask;
	uint16_t src_port_mask;
	uint16_t dst_port_mask;
};

/* Flow Director attribute */
struct hinic_atr_input {
	u32 dst_ip;
	u32 src_ip;
	u16 src_port;
	u16 dst_port;
};

struct hinic_fdir_rule {
	struct hinic_hw_fdir_mask mask;
	struct hinic_atr_input hinic_fdir; /* key of fdir filter */
	uint8_t queue; /* queue assigned when matched */
};

/* ntuple filter list structure */
struct hinic_ntuple_filter_ele {
	TAILQ_ENTRY(hinic_ntuple_filter_ele) entries;
	struct rte_eth_ntuple_filter filter_info;
};

/* ethertype filter list structure */
struct hinic_ethertype_filter_ele {
	TAILQ_ENTRY(hinic_ethertype_filter_ele) entries;
	struct rte_eth_ethertype_filter filter_info;
};

/* fdir filter list structure */
struct hinic_fdir_rule_ele {
	TAILQ_ENTRY(hinic_fdir_rule_ele) entries;
	struct hinic_fdir_rule filter_info;
};

struct rte_flow {
	enum rte_filter_type filter_type;
	void *rule;
};

/* hinic_flow memory list structure */
struct hinic_flow_mem {
	TAILQ_ENTRY(hinic_flow_mem) entries;
	struct rte_flow *flow;
};

TAILQ_HEAD(hinic_ntuple_filter_list, hinic_ntuple_filter_ele);
TAILQ_HEAD(hinic_ethertype_filter_list, hinic_ethertype_filter_ele);
TAILQ_HEAD(hinic_fdir_rule_filter_list, hinic_fdir_rule_ele);
TAILQ_HEAD(hinic_flow_mem_list, hinic_flow_mem);

extern const struct rte_flow_ops hinic_flow_ops;

/* hinic nic_device */
struct hinic_nic_dev {
	/* hardware device */
	struct hinic_hwdev *hwdev;
	struct hinic_txq **txqs;
	struct hinic_rxq **rxqs;
	struct rte_mempool *cpy_mpool;
	u16 num_qps;
	u16 num_sq;
	u16 num_rq;
	u16 mtu_size;
	u8 rss_tmpl_idx;
	u8 rss_indir_flag;
	u8 num_rss;
	u8 rx_queue_list[HINIC_MAX_RX_QUEUES];

	u32 vfta[HINIC_VFTA_SIZE];	/* VLAN bitmap */

	struct rte_ether_addr default_addr;
	struct rte_ether_addr *mc_list;
	/* info */
	unsigned int flags;
	struct nic_service_cap nic_cap;
	u32 rx_mode_status;	/* promisc or allmulticast */
	unsigned long dev_status;

	/* dpdk only */
	char proc_dev_name[HINIC_DEV_NAME_LEN];
	/* PF0->COS4, PF1->COS5, PF2->COS6, PF3->COS7,
	 * vf: the same with associate pf
	 */
	u32 default_cos;

	struct hinic_filter_info    filter;
	struct hinic_ntuple_filter_list filter_ntuple_list;
	struct hinic_ethertype_filter_list filter_ethertype_list;
	struct hinic_fdir_rule_filter_list filter_fdir_rule_list;
	struct hinic_flow_mem_list hinic_flow_list;
};

#endif /* _HINIC_PMD_ETHDEV_H_ */
