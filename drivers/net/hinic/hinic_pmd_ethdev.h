/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Huawei Technologies Co., Ltd
 */

#ifndef _HINIC_PMD_ETHDEV_H_
#define _HINIC_PMD_ETHDEV_H_

#include <rte_ethdev.h>
#include <rte_ethdev_core.h>
#include <rte_ethdev_driver.h>

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

#define HINIC_MAX_MTU_SIZE              9600
#define HINIC_MIN_MTU_SIZE              256

#define HINIC_VLAN_TAG_SIZE             4
#define HINIC_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + HINIC_VLAN_TAG_SIZE * 2)

#define HINIC_MIN_FRAME_SIZE        (HINIC_MIN_MTU_SIZE + HINIC_ETH_OVERHEAD)
#define HINIC_MAX_JUMBO_FRAME_SIZE  (HINIC_MAX_MTU_SIZE + HINIC_ETH_OVERHEAD)

#define HINIC_MTU_TO_PKTLEN(mtu)    ((mtu) + HINIC_ETH_OVERHEAD)

#define HINIC_PKTLEN_TO_MTU(pktlen) ((pktlen) - HINIC_ETH_OVERHEAD)

/* The max frame size with default MTU */
#define HINIC_ETH_MAX_LEN           (RTE_ETHER_MTU + HINIC_ETH_OVERHEAD)

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

	/* if mask is 1b, do not compare the response bit domain */
	uint8_t dst_ip_mask:1,
		src_ip_mask:1,
		dst_port_mask:1,
		src_port_mask:1,
		proto_mask:1;
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
	uint16_t proto_mask;
	uint16_t tunnel_flag;
	uint16_t tunnel_inner_src_port_mask;
	uint16_t tunnel_inner_dst_port_mask;
	uint16_t dst_ipv6_mask;
};

/* Flow Director attribute */
struct hinic_atr_input {
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t src_port;
	uint16_t dst_port;
	uint16_t proto;
	uint16_t tunnel_flag;
	uint16_t tunnel_inner_src_port;
	uint16_t tunnel_inner_dst_port;
	uint8_t  dst_ipv6[16];
};

enum hinic_fdir_mode {
	HINIC_FDIR_MODE_NORMAL      = 0,
	HINIC_FDIR_MODE_TCAM        = 1,
};

#define HINIC_PF_MAX_TCAM_FILTERS	1024
#define HINIC_VF_MAX_TCAM_FILTERS	128
#define HINIC_SUPPORT_PF_MAX_NUM	4
#define HINIC_TOTAL_PF_MAX_NUM		16
#define HINIC_SUPPORT_VF_MAX_NUM	32
#define HINIC_TCAM_BLOCK_TYPE_PF	0 /* 1024 tcam index of a block */
#define HINIC_TCAM_BLOCK_TYPE_VF	1 /* 128 tcam index of a block */

#define HINIC_PKT_VF_TCAM_INDEX_START(block_index)  \
		(HINIC_PF_MAX_TCAM_FILTERS * HINIC_SUPPORT_PF_MAX_NUM + \
		HINIC_VF_MAX_TCAM_FILTERS * (block_index))

TAILQ_HEAD(hinic_tcam_filter_list, hinic_tcam_filter);

struct hinic_tcam_info {
	struct hinic_tcam_filter_list tcam_list;
	u8 tcam_index_array[HINIC_PF_MAX_TCAM_FILTERS];
	u16 tcam_block_index;
	u16 tcam_rule_nums;
};

struct tag_tcam_key_mem {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN)

		u32 rsvd0:16;
		u32 function_id:16;

		u32 protocol:8;
		/*
		 * tunnel packet, mask must be 0xff, spec value is 1;
		 * normal packet, mask must be 0, spec value is 0;
		 * if tunnal packet, ucode use
		 * sip/dip/protocol/src_port/dst_dport from inner packet
		 */
		u32 tunnel_flag:8;
		u32 sip_h:16;

		u32 sip_l:16;
		u32 dip_h:16;

		u32 dip_l:16;
		u32 src_port:16;

		u32 dst_port:16;
		/*
		 * tunnel packet and normal packet,
		 * ext_dip mask must be 0xffffffff
		 */
		u32 ext_dip_h:16;
		u32 ext_dip_l:16;
		u32 rsvd2:16;
#else
		u32 function_id:16;
		u32 rsvd0:16;

		u32 sip_h:16;
		u32 tunnel_flag:8;
		u32 protocol:8;

		u32 dip_h:16;
		u32 sip_l:16;

		u32 src_port:16;
		u32 dip_l:16;

		u32 ext_dip_h:16;
		u32 dst_port:16;

		u32 rsvd2:16;
		u32 ext_dip_l:16;
#endif
};

struct tag_tcam_key_ipv6_mem {
#if (RTE_BYTE_ORDER == RTE_BIG_ENDIAN)
		u32 rsvd0:16;
		u32 ipv6_flag:1;
		u32 protocol:7;
		u32 function_id:8;

		u32 dst_port:16;
		u32 ipv6_key0:16;

		u32 ipv6_key1:16;
		u32 ipv6_key2:16;

		u32 ipv6_key3:16;
		u32 ipv6_key4:16;

		u32 ipv6_key5:16;
		u32 ipv6_key6:16;

		u32 ipv6_key7:16;
		u32 rsvd2:16;
#else
		u32 function_id:8;
		u32 protocol:7;
		u32 ipv6_flag:1;
		u32 rsvd0:16;

		u32 ipv6_key0:16;
		u32 dst_port:16;

		u32 ipv6_key2:16;
		u32 ipv6_key1:16;

		u32 ipv6_key4:16;
		u32 ipv6_key3:16;

		u32 ipv6_key6:16;
		u32 ipv6_key5:16;

		u32 rsvd2:16;
		u32 ipv6_key7:16;
#endif
};

struct tag_tcam_key {
	union {
		struct tag_tcam_key_mem key_info;
		struct tag_tcam_key_ipv6_mem key_info_ipv6;
	};

	union {
		struct tag_tcam_key_mem key_mask;
		struct tag_tcam_key_ipv6_mem key_mask_ipv6;
	};
};

struct hinic_fdir_rule {
	struct hinic_hw_fdir_mask mask;
	struct hinic_atr_input hinic_fdir; /* key of fdir filter */
	uint8_t queue; /* queue assigned when matched */
	enum hinic_fdir_mode mode; /* fdir type */
	u16 tcam_index;
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

struct hinic_tcam_filter {
	TAILQ_ENTRY(hinic_tcam_filter) entries;
	uint16_t index; /* tcam index */
	struct tag_tcam_key tcam_key;
	uint16_t queue; /* rx queue assigned to */
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

	bool pause_set;
	struct nic_pause_config nic_pause;

	u32 vfta[HINIC_VFTA_SIZE];	/* VLAN bitmap */

	struct rte_ether_addr default_addr;
	struct rte_ether_addr *mc_list;
	/* info */
	unsigned int flags;
	struct nic_service_cap nic_cap;
	u32 rx_mode_status;	/* promisc or allmulticast */
	pthread_mutex_t rx_mode_mutex;
	u32 dev_status;

	char proc_dev_name[HINIC_DEV_NAME_LEN];
	/* PF0->COS4, PF1->COS5, PF2->COS6, PF3->COS7,
	 * vf: the same with associate pf
	 */
	u32 default_cos;
	u32 rx_csum_en;

	struct hinic_filter_info    filter;
	struct hinic_tcam_info      tcam;
	struct hinic_ntuple_filter_list filter_ntuple_list;
	struct hinic_ethertype_filter_list filter_ethertype_list;
	struct hinic_fdir_rule_filter_list filter_fdir_rule_list;
	struct hinic_flow_mem_list hinic_flow_list;
};

void hinic_free_fdir_filter(struct hinic_nic_dev *nic_dev);

void hinic_destroy_fdir_filter(struct rte_eth_dev *dev);
#endif /* _HINIC_PMD_ETHDEV_H_ */
