/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include <stdint.h>

#include "base/txgbe.h"
#include "txgbe_ptypes.h"
#ifdef RTE_LIB_SECURITY
#include "txgbe_ipsec.h"
#endif
#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_time.h>
#include <rte_ethdev.h>
#include <rte_ethdev_core.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <bus_pci_driver.h>
#include <rte_tm_driver.h>

/* need update link, bit flag */
#define TXGBE_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define TXGBE_FLAG_MAILBOX          (uint32_t)(1 << 1)
#define TXGBE_FLAG_PHY_INTERRUPT    (uint32_t)(1 << 2)
#define TXGBE_FLAG_MACSEC           (uint32_t)(1 << 3)
#define TXGBE_FLAG_NEED_LINK_CONFIG (uint32_t)(1 << 4)
#define TXGBE_FLAG_NEED_AN_CONFIG   (uint32_t)(1 << 5)
#define TXGBE_FLAG_OVERHEAT         (uint32_t)(1 << 6)
#define TXGBE_FLAG_TX_DESC_ERR      (uint32_t)(1 << 7)

/*
 * Defines that were not part of txgbe_type.h as they are not used by the
 * FreeBSD driver.
 */
#define TXGBE_VFTA_SIZE 128
#define TXGBE_HKEY_MAX_INDEX 10
/*Default value of Max Rx Queue*/
#define TXGBE_MAX_RX_QUEUE_NUM	128
#define TXGBE_VMDQ_DCB_NB_QUEUES     TXGBE_MAX_RX_QUEUE_NUM
#define TXGBE_NONE_MODE_TX_NB_QUEUES 64

#ifndef NBBY
#define NBBY	8	/* number of bits in a byte */
#endif
#define TXGBE_HWSTRIP_BITMAP_SIZE \
	(TXGBE_MAX_RX_QUEUE_NUM / (sizeof(uint32_t) * NBBY))

#define TXGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

#define TXGBE_MAX_QUEUE_NUM_PER_VF  8

#define TXGBE_5TUPLE_MAX_PRI            7
#define TXGBE_5TUPLE_MIN_PRI            1

#define TXGBE_MAX_MTU			9414
/* The overhead from MTU to max frame size. */
#define TXGBE_ETH_OVERHEAD (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)

#define TXGBE_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV4_SCTP | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_NONFRAG_IPV6_SCTP | \
	RTE_ETH_RSS_IPV6_EX | \
	RTE_ETH_RSS_IPV6_TCP_EX | \
	RTE_ETH_RSS_IPV6_UDP_EX)

#define TXGBE_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define TXGBE_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

#define TXGBE_MAX_FDIR_FILTER_NUM       (1024 * 32)
#define TXGBE_MAX_L2_TN_FILTER_NUM      128

/*
 * Information about the fdir mode.
 */
struct txgbe_hw_fdir_mask {
	uint16_t vlan_tci_mask;
	uint32_t src_ipv4_mask;
	uint32_t dst_ipv4_mask;
	uint16_t src_ipv6_mask;
	uint16_t dst_ipv6_mask;
	uint16_t src_port_mask;
	uint16_t dst_port_mask;
	uint16_t flex_bytes_mask;
	uint8_t  pkt_type_mask; /* reversed mask for hw */
};

struct txgbe_fdir_filter {
	TAILQ_ENTRY(txgbe_fdir_filter) entries;
	struct txgbe_atr_input input; /* key of fdir filter*/
	uint32_t fdirflags; /* drop or forward */
	uint32_t fdirhash; /* hash value for fdir */
	uint8_t queue; /* assigned rx queue */
};

/* list of fdir filters */
TAILQ_HEAD(txgbe_fdir_filter_list, txgbe_fdir_filter);

struct txgbe_fdir_rule {
	struct txgbe_hw_fdir_mask mask;
	struct txgbe_atr_input input; /* key of fdir filter */
	bool b_spec; /* If TRUE, input, fdirflags, queue have meaning. */
	bool b_mask; /* If TRUE, mask has meaning. */
	enum rte_fdir_mode mode; /* IP, MAC VLAN, Tunnel */
	uint32_t fdirflags; /* drop or forward */
	uint32_t soft_id; /* an unique value for this rule */
	uint8_t queue; /* assigned rx queue */
	uint8_t flex_bytes_offset;
	bool flex_relative;
};

struct txgbe_hw_fdir_info {
	struct txgbe_hw_fdir_mask mask;
	uint8_t     flex_bytes_offset;
	bool        flex_relative;
	uint16_t    collision;
	uint16_t    free;
	uint16_t    maxhash;
	uint8_t     maxlen;
	uint64_t    add;
	uint64_t    remove;
	uint64_t    f_add;
	uint64_t    f_remove;
	struct txgbe_fdir_filter_list fdir_list; /* filter list*/
	/* store the pointers of the filters, index is the hash value. */
	struct txgbe_fdir_filter **hash_map;
	struct rte_hash *hash_handle; /* cuckoo hash handler */
	bool mask_added; /* If already got mask from consistent filter */
};

struct txgbe_rte_flow_rss_conf {
	struct rte_flow_action_rss conf; /**< RSS parameters. */
	uint8_t key[TXGBE_HKEY_MAX_INDEX * sizeof(uint32_t)]; /* Hash key. */
	uint16_t queue[TXGBE_MAX_RX_QUEUE_NUM]; /**< Queues indices to use. */
};

/* structure for interrupt relative data */
struct txgbe_interrupt {
	uint32_t flags;
	uint32_t mask_misc;
	uint32_t mask_misc_orig; /* save mask during delayed handler */
	uint64_t mask;
	uint64_t mask_orig; /* save mask during delayed handler */
};

#define TXGBE_NB_STAT_MAPPING  32
#define QSM_REG_NB_BITS_PER_QMAP_FIELD 8
#define NB_QMAP_FIELDS_PER_QSM_REG 4
#define QMAP_FIELD_RESERVED_BITS_MASK 0x0f
struct txgbe_stat_mappings {
	uint32_t tqsm[TXGBE_NB_STAT_MAPPING];
	uint32_t rqsm[TXGBE_NB_STAT_MAPPING];
};

struct txgbe_vfta {
	uint32_t vfta[TXGBE_VFTA_SIZE];
};

struct txgbe_hwstrip {
	uint32_t bitmap[TXGBE_HWSTRIP_BITMAP_SIZE];
};

/*
 * VF data which used by PF host only
 */
#define TXGBE_MAX_VF_MC_ENTRIES      30

struct txgbe_uta_info {
	uint8_t  uc_filter_type;
	uint16_t uta_in_use;
	uint32_t uta_shadow[TXGBE_MAX_UTA];
};

struct txgbe_vf_info {
	uint8_t vf_mac_addresses[RTE_ETHER_ADDR_LEN];
	uint16_t vf_mc_hashes[TXGBE_MAX_VF_MC_ENTRIES];
	uint16_t num_vf_mc_hashes;
	bool clear_to_send;
	uint16_t tx_rate[TXGBE_MAX_QUEUE_NUM_PER_VF];
	uint16_t vlan_count;
	uint8_t api_version;
	uint16_t switch_domain_id;
	uint16_t xcast_mode;
	uint16_t mac_count;
};

TAILQ_HEAD(txgbe_5tuple_filter_list, txgbe_5tuple_filter);

struct txgbe_5tuple_filter_info {
	uint32_t dst_ip;
	uint32_t src_ip;
	uint16_t dst_port;
	uint16_t src_port;
	enum txgbe_5tuple_protocol proto;        /* l4 protocol. */
	uint8_t priority;        /* seven levels (001b-111b), 111b is highest,
				  * used when more than one filter matches.
				  */
	uint8_t dst_ip_mask:1,   /* if mask is 1b, do not compare dst ip. */
		src_ip_mask:1,   /* if mask is 1b, do not compare src ip. */
		dst_port_mask:1, /* if mask is 1b, do not compare dst port. */
		src_port_mask:1, /* if mask is 1b, do not compare src port. */
		proto_mask:1;    /* if mask is 1b, do not compare protocol. */
};

/* 5tuple filter structure */
struct txgbe_5tuple_filter {
	TAILQ_ENTRY(txgbe_5tuple_filter) entries;
	uint16_t index;       /* the index of 5tuple filter */
	struct txgbe_5tuple_filter_info filter_info;
	uint16_t queue;       /* rx queue assigned to */
};

#define TXGBE_5TUPLE_ARRAY_SIZE \
	(RTE_ALIGN(TXGBE_MAX_FTQF_FILTERS, (sizeof(uint32_t) * NBBY)) / \
	 (sizeof(uint32_t) * NBBY))

struct txgbe_ethertype_filter {
	uint16_t ethertype;
	uint32_t etqf;
	uint32_t etqs;
	/**
	 * If this filter is added by configuration,
	 * it should not be removed.
	 */
	bool     conf;
};

/*
 * Structure to store filters' info.
 */
struct txgbe_filter_info {
	uint8_t ethertype_mask;  /* Bit mask for every used ethertype filter */
	/* store used ethertype filters*/
	struct txgbe_ethertype_filter ethertype_filters[TXGBE_ETF_ID_MAX];
	/* Bit mask for every used 5tuple filter */
	uint32_t fivetuple_mask[TXGBE_5TUPLE_ARRAY_SIZE];
	struct txgbe_5tuple_filter_list fivetuple_list;
	/* store the SYN filter info */
	uint32_t syn_info;
	/* store the rss filter info */
	struct txgbe_rte_flow_rss_conf rss_info;
};

struct txgbe_l2_tn_key {
	enum rte_eth_tunnel_type          l2_tn_type;
	uint32_t                          tn_id;
};

struct txgbe_l2_tn_filter {
	TAILQ_ENTRY(txgbe_l2_tn_filter)    entries;
	struct txgbe_l2_tn_key             key;
	uint32_t                           pool;
};

TAILQ_HEAD(txgbe_l2_tn_filter_list, txgbe_l2_tn_filter);

struct txgbe_l2_tn_info {
	struct txgbe_l2_tn_filter_list      l2_tn_list;
	struct txgbe_l2_tn_filter         **hash_map;
	struct rte_hash                    *hash_handle;
	bool e_tag_en; /* e-tag enabled */
	bool e_tag_fwd_en; /* e-tag based forwarding enabled */
	uint16_t e_tag_ether_type; /* ether type for e-tag */
};

struct rte_flow {
	enum rte_filter_type filter_type;
	void *rule;
};

/* The configuration of bandwidth */
struct txgbe_bw_conf {
	uint8_t tc_num; /* Number of TCs. */
};

/* Struct to store Traffic Manager shaper profile. */
struct txgbe_tm_shaper_profile {
	TAILQ_ENTRY(txgbe_tm_shaper_profile) node;
	uint32_t shaper_profile_id;
	uint32_t reference_count;
	struct rte_tm_shaper_params profile;
};

TAILQ_HEAD(txgbe_shaper_profile_list, txgbe_tm_shaper_profile);

/* node type of Traffic Manager */
enum txgbe_tm_node_type {
	TXGBE_TM_NODE_TYPE_PORT,
	TXGBE_TM_NODE_TYPE_TC,
	TXGBE_TM_NODE_TYPE_QUEUE,
	TXGBE_TM_NODE_TYPE_MAX,
};

/* Struct to store Traffic Manager node configuration. */
struct txgbe_tm_node {
	TAILQ_ENTRY(txgbe_tm_node) node;
	uint32_t id;
	uint32_t priority;
	uint32_t weight;
	uint32_t reference_count;
	uint16_t no;
	struct txgbe_tm_node *parent;
	struct txgbe_tm_shaper_profile *shaper_profile;
	struct rte_tm_node_params params;
};

TAILQ_HEAD(txgbe_tm_node_list, txgbe_tm_node);

/* The configuration of Traffic Manager */
struct txgbe_tm_conf {
	struct txgbe_shaper_profile_list shaper_profile_list;
	struct txgbe_tm_node *root; /* root node - port */
	struct txgbe_tm_node_list tc_list; /* node list for all the TCs */
	struct txgbe_tm_node_list queue_list; /* node list for all the queues */
	/**
	 * The number of added TC nodes.
	 * It should be no more than the TC number of this port.
	 */
	uint32_t nb_tc_node;
	/**
	 * The number of added queue nodes.
	 * It should be no more than the queue number of this port.
	 */
	uint32_t nb_queue_node;
	/**
	 * This flag is used to check if APP can change the TM node
	 * configuration.
	 * When it's true, means the configuration is applied to HW,
	 * APP should not change the configuration.
	 * As we don't support on-the-fly configuration, when starting
	 * the port, APP should call the hierarchy_commit API to set this
	 * flag to true. When stopping the port, this flag should be set
	 * to false.
	 */
	bool committed;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
	struct txgbe_hw_stats       stats;
	struct rte_eth_fdir_conf    fdir_conf;
	struct txgbe_hw_fdir_info   fdir;
	struct txgbe_interrupt      intr;
	struct txgbe_stat_mappings  stat_mappings;
	struct txgbe_vfta           shadow_vfta;
	struct txgbe_hwstrip        hwstrip;
	struct txgbe_dcb_config     dcb_config;
	struct txgbe_vf_info        *vfdata;
	struct txgbe_uta_info       uta_info;
	struct txgbe_filter_info    filter;
	struct txgbe_l2_tn_info     l2_tn;
	struct txgbe_bw_conf        bw_conf;
#ifdef RTE_LIB_SECURITY
	struct txgbe_ipsec          ipsec;
#endif
	bool rx_bulk_alloc_allowed;
	bool rx_vec_allowed;
	struct rte_timecounter      systime_tc;
	struct rte_timecounter      rx_tstamp_tc;
	struct rte_timecounter      tx_tstamp_tc;
	struct txgbe_tm_conf        tm_conf;

	/* For RSS reta table update */
	uint8_t rss_reta_updated;

	RTE_ATOMIC(uint32_t) link_thread_running;
	rte_thread_t link_thread_tid;
};

#define TXGBE_DEV_ADAPTER(dev) \
	((struct txgbe_adapter *)(dev)->data->dev_private)

#define TXGBE_DEV_FDIR_CONF(dev)	(&TXGBE_DEV_ADAPTER(dev)->fdir_conf)

#define TXGBE_DEV_HW(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hw)

#define TXGBE_DEV_STATS(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->stats)

#define TXGBE_DEV_INTR(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->intr)

#define TXGBE_DEV_FDIR(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->fdir)

#define TXGBE_DEV_STAT_MAPPINGS(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->stat_mappings)

#define TXGBE_DEV_VFTA(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->shadow_vfta)

#define TXGBE_DEV_HWSTRIP(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hwstrip)

#define TXGBE_DEV_DCB_CONFIG(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->dcb_config)

#define TXGBE_DEV_VFDATA(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->vfdata)

#define TXGBE_DEV_MR_INFO(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->mr_data)

#define TXGBE_DEV_UTA_INFO(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->uta_info)

#define TXGBE_DEV_FILTER(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->filter)

#define TXGBE_DEV_L2_TN(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->l2_tn)

#define TXGBE_DEV_BW_CONF(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->bw_conf)

#define TXGBE_DEV_TM_CONF(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->tm_conf)

#define TXGBE_DEV_IPSEC(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->ipsec)

/*
 * RX/TX function prototypes
 */
void txgbe_dev_clear_queues(struct rte_eth_dev *dev);

void txgbe_dev_free_queues(struct rte_eth_dev *dev);

void txgbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

void txgbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

int  txgbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int  txgbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

int txgbe_dev_rx_queue_count(void *rx_queue);

int txgbe_dev_rx_descriptor_status(void *rx_queue, uint16_t offset);
int txgbe_dev_tx_descriptor_status(void *tx_queue, uint16_t offset);

int txgbe_dev_rx_init(struct rte_eth_dev *dev);

void txgbe_dev_tx_init(struct rte_eth_dev *dev);

int txgbe_dev_rxtx_start(struct rte_eth_dev *dev);

void txgbe_dev_save_rx_queue(struct txgbe_hw *hw, uint16_t rx_queue_id);
void txgbe_dev_store_rx_queue(struct txgbe_hw *hw, uint16_t rx_queue_id);
void txgbe_dev_save_tx_queue(struct txgbe_hw *hw, uint16_t tx_queue_id);
void txgbe_dev_store_tx_queue(struct txgbe_hw *hw, uint16_t tx_queue_id);

int txgbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int txgbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int txgbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int txgbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void txgbe_tx_queue_clear_error(void *param);

void txgbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

void txgbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

int txgbevf_dev_rx_init(struct rte_eth_dev *dev);

void txgbevf_dev_tx_init(struct rte_eth_dev *dev);

void txgbevf_dev_rxtx_start(struct rte_eth_dev *dev);

uint16_t txgbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t txgbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				    uint16_t nb_pkts);

uint16_t txgbe_recv_pkts_lro_single_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t txgbe_recv_pkts_lro_bulk_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t txgbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t txgbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t txgbe_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

int txgbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf);

int txgbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				struct rte_eth_rss_conf *rss_conf);

bool txgbe_rss_update(enum txgbe_mac_type mac_type);

int txgbe_add_del_ntuple_filter(struct rte_eth_dev *dev,
			struct rte_eth_ntuple_filter *filter,
			bool add);
int txgbe_add_del_ethertype_filter(struct rte_eth_dev *dev,
			struct rte_eth_ethertype_filter *filter,
			bool add);
int txgbe_syn_filter_set(struct rte_eth_dev *dev,
			struct rte_eth_syn_filter *filter,
			bool add);
int txgbe_ntuple_filter_uninit(struct rte_eth_dev *eth_dev);

int txgbevf_inject_5tuple_filter(struct rte_eth_dev *dev,
				 struct txgbe_5tuple_filter *filter);
void txgbevf_remove_5tuple_filter(struct rte_eth_dev *dev, u16 index);

/**
 * l2 tunnel configuration.
 */
struct txgbe_l2_tunnel_conf {
	enum rte_eth_tunnel_type l2_tunnel_type;
	uint16_t ether_type; /* ether type in l2 header */
	uint32_t tunnel_id; /* port tag id for e-tag */
	uint16_t vf_id; /* VF id for tag insertion */
	uint32_t pool; /* destination pool for tag based forwarding */
};

int
txgbe_dev_l2_tunnel_filter_add(struct rte_eth_dev *dev,
			       struct txgbe_l2_tunnel_conf *l2_tunnel,
			       bool restore);
int
txgbe_dev_l2_tunnel_filter_del(struct rte_eth_dev *dev,
			       struct txgbe_l2_tunnel_conf *l2_tunnel);
void txgbe_filterlist_init(void);
void txgbe_filterlist_flush(void);

void txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

/*
 * Flow director function prototypes
 */
int txgbe_fdir_configure(struct rte_eth_dev *dev);
int txgbe_fdir_set_input_mask(struct rte_eth_dev *dev);
uint16_t txgbe_fdir_get_flex_base(struct txgbe_fdir_rule *rule);
int txgbe_fdir_set_flexbytes_offset(struct rte_eth_dev *dev,
				    uint16_t offset, uint16_t flex_base);
int txgbe_fdir_filter_program(struct rte_eth_dev *dev,
			      struct txgbe_fdir_rule *rule,
			      bool del, bool update);

void txgbe_configure_pb(struct rte_eth_dev *dev);
void txgbe_configure_port(struct rte_eth_dev *dev);
void txgbe_configure_dcb(struct rte_eth_dev *dev);

int
txgbe_dev_link_update_share(struct rte_eth_dev *dev,
		int wait_to_complete);
int
txgbe_dev_wait_setup_link_complete(struct rte_eth_dev *dev,
		uint32_t timeout_ms);
int txgbe_pf_host_init(struct rte_eth_dev *eth_dev);

void txgbe_pf_host_uninit(struct rte_eth_dev *eth_dev);

void txgbe_pf_mbx_process(struct rte_eth_dev *eth_dev);

int txgbe_pf_host_configure(struct rte_eth_dev *eth_dev);

uint32_t txgbe_convert_vm_rx_mask_to_val(uint16_t rx_mask, uint32_t orig_val);

void txgbe_fdir_filter_restore(struct rte_eth_dev *dev);
int txgbe_clear_all_fdir_filter(struct rte_eth_dev *dev);

extern const struct rte_flow_ops txgbe_flow_ops;

void txgbe_clear_all_ethertype_filter(struct rte_eth_dev *dev);
void txgbe_clear_all_ntuple_filter(struct rte_eth_dev *dev);
void txgbe_clear_syn_filter(struct rte_eth_dev *dev);
int txgbe_clear_all_l2_tn_filter(struct rte_eth_dev *dev);

int txgbe_set_vf_rate_limit(struct rte_eth_dev *dev, uint16_t vf,
			    uint16_t tx_rate, uint64_t q_msk);
int txgbe_tm_ops_get(struct rte_eth_dev *dev, void *ops);
void txgbe_tm_conf_init(struct rte_eth_dev *dev);
void txgbe_tm_conf_uninit(struct rte_eth_dev *dev);
int txgbe_set_queue_rate_limit(struct rte_eth_dev *dev, uint16_t queue_idx,
			       uint32_t tx_rate);
int txgbe_rss_conf_init(struct txgbe_rte_flow_rss_conf *out,
			const struct rte_flow_action_rss *in);
int txgbe_action_rss_same(const struct rte_flow_action_rss *comp,
			  const struct rte_flow_action_rss *with);
int txgbe_config_rss_filter(struct rte_eth_dev *dev,
		struct txgbe_rte_flow_rss_conf *conf, bool add);

static inline int
txgbe_ethertype_filter_lookup(struct txgbe_filter_info *filter_info,
			      uint16_t ethertype)
{
	int i;

	for (i = 0; i < TXGBE_ETF_ID_MAX; i++) {
		if (filter_info->ethertype_filters[i].ethertype == ethertype &&
		    (filter_info->ethertype_mask & (1 << i)))
			return i;
	}
	return -1;
}

static inline int
txgbe_ethertype_filter_insert(struct txgbe_filter_info *filter_info,
			      struct txgbe_ethertype_filter *ethertype_filter)
{
	int i;

	for (i = 0; i < TXGBE_ETF_ID_MAX; i++) {
		if (filter_info->ethertype_mask & (1 << i))
			continue;

		filter_info->ethertype_mask |= 1 << i;
		filter_info->ethertype_filters[i].ethertype =
				ethertype_filter->ethertype;
		filter_info->ethertype_filters[i].etqf =
				ethertype_filter->etqf;
		filter_info->ethertype_filters[i].etqs =
				ethertype_filter->etqs;
		filter_info->ethertype_filters[i].conf =
				ethertype_filter->conf;
		break;
	}
	return (i < TXGBE_ETF_ID_MAX ? i : -1);
}

static inline int
txgbe_ethertype_filter_remove(struct txgbe_filter_info *filter_info,
			      uint8_t idx)
{
	if (idx >= TXGBE_ETF_ID_MAX)
		return -1;
	filter_info->ethertype_mask &= ~(1 << idx);
	filter_info->ethertype_filters[idx].ethertype = 0;
	filter_info->ethertype_filters[idx].etqf = 0;
	filter_info->ethertype_filters[idx].etqs = 0;
	filter_info->ethertype_filters[idx].etqs = FALSE;
	return idx;
}

#ifdef RTE_LIB_SECURITY
int txgbe_ipsec_ctx_create(struct rte_eth_dev *dev);
#endif

/* High threshold controlling when to start sending XOFF frames. */
#define TXGBE_FC_XOFF_HITH              128 /*KB*/
/* Low threshold controlling when to start sending XON frames. */
#define TXGBE_FC_XON_LOTH               64 /*KB*/

/* Timer value included in XOFF frames. */
#define TXGBE_FC_PAUSE_TIME 0x680

#define TXGBE_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define TXGBE_LINK_UP_CHECK_TIMEOUT   1000 /* ms */
#define TXGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */

/*
 *  Default values for RX/TX configuration
 */
#define TXGBE_DEFAULT_RX_FREE_THRESH  32
#define TXGBE_DEFAULT_RX_PTHRESH      8
#define TXGBE_DEFAULT_RX_HTHRESH      8
#define TXGBE_DEFAULT_RX_WTHRESH      0

#define TXGBE_DEFAULT_TX_FREE_THRESH  32
#define TXGBE_DEFAULT_TX_PTHRESH      32
#define TXGBE_DEFAULT_TX_HTHRESH      0
#define TXGBE_DEFAULT_TX_WTHRESH      0

/* Additional timesync values. */
#define NSEC_PER_SEC             1000000000L
#define TXGBE_INCVAL_10GB        0xCCCCCC
#define TXGBE_INCVAL_1GB         0x800000
#define TXGBE_INCVAL_100         0xA00000
#define TXGBE_INCVAL_10          0xC7F380
#define TXGBE_INCVAL_FPGA        0x800000
#define TXGBE_INCVAL_SHIFT_10GB  20
#define TXGBE_INCVAL_SHIFT_1GB   18
#define TXGBE_INCVAL_SHIFT_100   15
#define TXGBE_INCVAL_SHIFT_10    12
#define TXGBE_INCVAL_SHIFT_FPGA  17

#define TXGBE_CYCLECOUNTER_MASK   0xffffffffffffffffULL

/* store statistics names and its offset in stats structure */
struct rte_txgbe_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

const uint32_t *txgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev,
					       size_t *no_of_elements);
int txgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
int txgbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int txgbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
void txgbe_dev_setup_link_alarm_handler(void *param);
void txgbe_read_stats_registers(struct txgbe_hw *hw,
			   struct txgbe_hw_stats *hw_stats);

void txgbe_vlan_hw_filter_enable(struct rte_eth_dev *dev);
void txgbe_vlan_hw_filter_disable(struct rte_eth_dev *dev);
void txgbe_vlan_hw_strip_config(struct rte_eth_dev *dev);
void txgbe_vlan_hw_strip_bitmap_set(struct rte_eth_dev *dev,
		uint16_t queue, bool on);
void txgbe_config_vlan_strip_on_all_queues(struct rte_eth_dev *dev,
						  int mask);

#endif /* _TXGBE_ETHDEV_H_ */
