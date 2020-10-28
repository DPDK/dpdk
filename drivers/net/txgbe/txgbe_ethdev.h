/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include <stdint.h>

#include "base/txgbe.h"
#include "txgbe_ptypes.h"
#include <rte_time.h>

/* need update link, bit flag */
#define TXGBE_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define TXGBE_FLAG_MAILBOX          (uint32_t)(1 << 1)
#define TXGBE_FLAG_PHY_INTERRUPT    (uint32_t)(1 << 2)
#define TXGBE_FLAG_MACSEC           (uint32_t)(1 << 3)
#define TXGBE_FLAG_NEED_LINK_CONFIG (uint32_t)(1 << 4)

/*
 * Defines that were not part of txgbe_type.h as they are not used by the
 * FreeBSD driver.
 */
#define TXGBE_VFTA_SIZE 128
#define TXGBE_VLAN_TAG_SIZE 4
#define TXGBE_HKEY_MAX_INDEX 10
/*Default value of Max Rx Queue*/
#define TXGBE_MAX_RX_QUEUE_NUM	128
#define TXGBE_VMDQ_DCB_NB_QUEUES     TXGBE_MAX_RX_QUEUE_NUM

#ifndef NBBY
#define NBBY	8	/* number of bits in a byte */
#endif
#define TXGBE_HWSTRIP_BITMAP_SIZE \
	(TXGBE_MAX_RX_QUEUE_NUM / (sizeof(uint32_t) * NBBY))

#define TXGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

#define TXGBE_MAX_QUEUE_NUM_PER_VF  8

#define TXGBE_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX)

#define TXGBE_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define TXGBE_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

/* structure for interrupt relative data */
struct txgbe_interrupt {
	uint32_t flags;
	uint32_t mask_misc;
	/* to save original mask during delayed handler */
	uint32_t mask_misc_orig;
	uint32_t mask[2];
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

#define TXGBE_MAX_MIRROR_RULES 4  /* Maximum nb. of mirror rules. */

struct txgbe_mirror_info {
	struct rte_eth_mirror_conf mr_conf[TXGBE_MAX_MIRROR_RULES];
	/* store PF mirror rules configuration */
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
};

/* The configuration of bandwidth */
struct txgbe_bw_conf {
	uint8_t tc_num; /* Number of TCs. */
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
	struct txgbe_hw_stats       stats;
	struct txgbe_interrupt      intr;
	struct txgbe_stat_mappings  stat_mappings;
	struct txgbe_vfta           shadow_vfta;
	struct txgbe_hwstrip        hwstrip;
	struct txgbe_dcb_config     dcb_config;
	struct txgbe_mirror_info    mr_data;
	struct txgbe_vf_info        *vfdata;
	struct txgbe_uta_info       uta_info;
	struct txgbe_filter_info    filter;
	struct txgbe_bw_conf        bw_conf;
	bool rx_bulk_alloc_allowed;
	struct rte_timecounter      systime_tc;
	struct rte_timecounter      rx_tstamp_tc;
	struct rte_timecounter      tx_tstamp_tc;

	/* For RSS reta table update */
	uint8_t rss_reta_updated;
};

#define TXGBE_DEV_ADAPTER(dev) \
	((struct txgbe_adapter *)(dev)->data->dev_private)

#define TXGBE_DEV_HW(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hw)

#define TXGBE_DEV_STATS(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->stats)

#define TXGBE_DEV_INTR(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->intr)

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
#define TXGBE_DEV_BW_CONF(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->bw_conf)


/*
 * RX/TX function prototypes
 */
void txgbe_dev_clear_queues(struct rte_eth_dev *dev);

void txgbe_dev_free_queues(struct rte_eth_dev *dev);

void txgbe_dev_rx_queue_release(void *rxq);

void txgbe_dev_tx_queue_release(void *txq);

int  txgbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int  txgbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

uint32_t txgbe_dev_rx_queue_count(struct rte_eth_dev *dev,
		uint16_t rx_queue_id);

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

void txgbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

void txgbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

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

bool txgbe_rss_update_sp(enum txgbe_mac_type mac_type);

void txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

void txgbe_configure_pb(struct rte_eth_dev *dev);
void txgbe_configure_port(struct rte_eth_dev *dev);
void txgbe_configure_dcb(struct rte_eth_dev *dev);

int
txgbe_dev_link_update_share(struct rte_eth_dev *dev,
		int wait_to_complete);
int txgbe_pf_host_init(struct rte_eth_dev *eth_dev);

void txgbe_pf_host_uninit(struct rte_eth_dev *eth_dev);

void txgbe_pf_mbx_process(struct rte_eth_dev *eth_dev);

int txgbe_pf_host_configure(struct rte_eth_dev *eth_dev);

uint32_t txgbe_convert_vm_rx_mask_to_val(uint16_t rx_mask, uint32_t orig_val);

int txgbe_set_vf_rate_limit(struct rte_eth_dev *dev, uint16_t vf,
			    uint16_t tx_rate, uint64_t q_msk);
int txgbe_set_queue_rate_limit(struct rte_eth_dev *dev, uint16_t queue_idx,
			       uint16_t tx_rate);
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

const uint32_t *txgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev);
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
