/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_ETHDEV_H_
#define _NGBE_ETHDEV_H_

#include "ngbe_ptypes.h"
#include <rte_time.h>
#include <rte_ethdev.h>
#include <rte_ethdev_core.h>

/* need update link, bit flag */
#define NGBE_FLAG_NEED_LINK_UPDATE  ((uint32_t)(1 << 0))
#define NGBE_FLAG_MAILBOX           ((uint32_t)(1 << 1))
#define NGBE_FLAG_PHY_INTERRUPT     ((uint32_t)(1 << 2))
#define NGBE_FLAG_MACSEC            ((uint32_t)(1 << 3))
#define NGBE_FLAG_NEED_LINK_CONFIG  ((uint32_t)(1 << 4))

#define NGBE_VFTA_SIZE 128
#define NGBE_HKEY_MAX_INDEX 10
/*Default value of Max Rx Queue*/
#define NGBE_MAX_RX_QUEUE_NUM	8

#ifndef NBBY
#define NBBY	8	/* number of bits in a byte */
#endif
#define NGBE_HWSTRIP_BITMAP_SIZE \
	(NGBE_MAX_RX_QUEUE_NUM / (sizeof(uint32_t) * NBBY))

#define NGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

/* The overhead from MTU to max frame size. */
#define NGBE_ETH_OVERHEAD (RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN)

#define NGBE_RSS_OFFLOAD_ALL ( \
	RTE_ETH_RSS_IPV4 | \
	RTE_ETH_RSS_NONFRAG_IPV4_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV4_UDP | \
	RTE_ETH_RSS_IPV6 | \
	RTE_ETH_RSS_NONFRAG_IPV6_TCP | \
	RTE_ETH_RSS_NONFRAG_IPV6_UDP | \
	RTE_ETH_RSS_IPV6_EX | \
	RTE_ETH_RSS_IPV6_TCP_EX | \
	RTE_ETH_RSS_IPV6_UDP_EX)

#define NGBE_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define NGBE_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

/* structure for interrupt relative data */
struct ngbe_interrupt {
	uint32_t flags;
	uint32_t mask_misc;
	uint32_t mask_misc_orig; /* save mask during delayed handler */
	uint64_t mask;
	uint64_t mask_orig; /* save mask during delayed handler */
};

#define NGBE_NB_STAT_MAPPING  32
#define NB_QMAP_FIELDS_PER_QSM_REG 4
#define QMAP_FIELD_RESERVED_BITS_MASK 0x0f
struct ngbe_stat_mappings {
	uint32_t tqsm[NGBE_NB_STAT_MAPPING];
	uint32_t rqsm[NGBE_NB_STAT_MAPPING];
};

struct ngbe_vfta {
	uint32_t vfta[NGBE_VFTA_SIZE];
};

struct ngbe_hwstrip {
	uint32_t bitmap[NGBE_HWSTRIP_BITMAP_SIZE];
};

/**
 * Response sent back to ngbe driver from user app after callback
 */
enum ngbe_mb_event_rsp {
	NGBE_MB_EVENT_NOOP_ACK,  /**< skip mbox request and ACK */
	NGBE_MB_EVENT_NOOP_NACK, /**< skip mbox request and NACK */
	NGBE_MB_EVENT_PROCEED,  /**< proceed with mbox request  */
	NGBE_MB_EVENT_MAX       /**< max value of this enum */
};

/**
 * Data sent to the user application when the callback is executed.
 */
struct ngbe_mb_event_param {
	uint16_t vfid;     /**< Virtual Function number */
	uint16_t msg_type; /**< VF to PF message type, defined in ngbe_mbx.h */
	uint16_t retval;   /**< return value */
	void *msg;         /**< pointer to message */
};

/*
 * VF data which used by PF host only
 */
#define NGBE_MAX_VF_MC_ENTRIES      30

struct ngbe_uta_info {
	uint8_t  uc_filter_type;
	uint16_t uta_in_use;
	uint32_t uta_shadow[NGBE_MAX_UTA];
};

struct ngbe_vf_info {
	uint8_t vf_mac_addresses[RTE_ETHER_ADDR_LEN];
	uint16_t vf_mc_hashes[NGBE_MAX_VF_MC_ENTRIES];
	uint16_t num_vf_mc_hashes;
	bool clear_to_send;
	uint16_t vlan_count;
	uint8_t api_version;
	uint16_t switch_domain_id;
	uint16_t xcast_mode;
	uint16_t mac_count;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct ngbe_adapter {
	struct ngbe_hw             hw;
	struct ngbe_hw_stats       stats;
	struct ngbe_interrupt      intr;
	struct ngbe_stat_mappings  stat_mappings;
	struct ngbe_vfta           shadow_vfta;
	struct ngbe_hwstrip        hwstrip;
	struct ngbe_vf_info        *vfdata;
	struct ngbe_uta_info       uta_info;
	bool                       rx_bulk_alloc_allowed;
	struct rte_timecounter     systime_tc;
	struct rte_timecounter     rx_tstamp_tc;
	struct rte_timecounter     tx_tstamp_tc;

	/* For RSS reta table update */
	uint8_t rss_reta_updated;
};

static inline struct ngbe_adapter *
ngbe_dev_adapter(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = dev->data->dev_private;

	return ad;
}

static inline struct ngbe_hw *
ngbe_dev_hw(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = ngbe_dev_adapter(dev);
	struct ngbe_hw *hw = &ad->hw;

	return hw;
}

#define NGBE_DEV_STATS(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->stats)

static inline struct ngbe_interrupt *
ngbe_dev_intr(struct rte_eth_dev *dev)
{
	struct ngbe_adapter *ad = ngbe_dev_adapter(dev);
	struct ngbe_interrupt *intr = &ad->intr;

	return intr;
}

#define NGBE_DEV_STAT_MAPPINGS(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->stat_mappings)

#define NGBE_DEV_VFTA(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->shadow_vfta)

#define NGBE_DEV_HWSTRIP(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->hwstrip)

#define NGBE_DEV_VFDATA(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->vfdata)

#define NGBE_DEV_UTA_INFO(dev) \
	(&((struct ngbe_adapter *)(dev)->data->dev_private)->uta_info)

/*
 * Rx/Tx function prototypes
 */
void ngbe_dev_clear_queues(struct rte_eth_dev *dev);

void ngbe_dev_free_queues(struct rte_eth_dev *dev);

void ngbe_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

void ngbe_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);

int  ngbe_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int  ngbe_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

uint32_t ngbe_dev_rx_queue_count(void *rx_queue);

int ngbe_dev_rx_descriptor_status(void *rx_queue, uint16_t offset);
int ngbe_dev_tx_descriptor_status(void *tx_queue, uint16_t offset);

int ngbe_dev_rx_init(struct rte_eth_dev *dev);

void ngbe_dev_tx_init(struct rte_eth_dev *dev);

int ngbe_dev_rxtx_start(struct rte_eth_dev *dev);

void ngbe_dev_save_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id);
void ngbe_dev_store_rx_queue(struct ngbe_hw *hw, uint16_t rx_queue_id);
void ngbe_dev_save_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id);
void ngbe_dev_store_tx_queue(struct ngbe_hw *hw, uint16_t tx_queue_id);

int ngbe_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int ngbe_dev_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int ngbe_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);

int ngbe_dev_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void ngbe_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

void ngbe_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

int
ngbe_rx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode);
int
ngbe_tx_burst_mode_get(struct rte_eth_dev *dev, __rte_unused uint16_t queue_id,
		      struct rte_eth_burst_mode *mode);

uint16_t ngbe_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_recv_pkts_bulk_alloc(void *rx_queue, struct rte_mbuf **rx_pkts,
				    uint16_t nb_pkts);

uint16_t ngbe_recv_pkts_sc_single_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);
uint16_t ngbe_recv_pkts_sc_bulk_alloc(void *rx_queue,
		struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

uint16_t ngbe_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_xmit_pkts_simple(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t ngbe_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

int ngbe_dev_rss_hash_update(struct rte_eth_dev *dev,
			      struct rte_eth_rss_conf *rss_conf);

int ngbe_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				struct rte_eth_rss_conf *rss_conf);

void ngbe_set_ivar_map(struct ngbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

void ngbe_configure_port(struct rte_eth_dev *dev);

int
ngbe_dev_link_update_share(struct rte_eth_dev *dev,
		int wait_to_complete);

/*
 * misc function prototypes
 */
void ngbe_vlan_hw_filter_enable(struct rte_eth_dev *dev);

void ngbe_vlan_hw_filter_disable(struct rte_eth_dev *dev);

void ngbe_vlan_hw_strip_config(struct rte_eth_dev *dev);

int ngbe_pf_host_init(struct rte_eth_dev *eth_dev);

void ngbe_pf_host_uninit(struct rte_eth_dev *eth_dev);

void ngbe_pf_mbx_process(struct rte_eth_dev *eth_dev);

int ngbe_pf_host_configure(struct rte_eth_dev *eth_dev);

/* High threshold controlling when to start sending XOFF frames. */
#define NGBE_FC_XOFF_HITH              128 /*KB*/
/* Low threshold controlling when to start sending XON frames. */
#define NGBE_FC_XON_LOTH               64 /*KB*/

/* Timer value included in XOFF frames. */
#define NGBE_FC_PAUSE_TIME 0x680

#define NGBE_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define NGBE_LINK_UP_CHECK_TIMEOUT   1000 /* ms */
#define NGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */

/*
 *  Default values for Rx/Tx configuration
 */
#define NGBE_DEFAULT_RX_FREE_THRESH  32
#define NGBE_DEFAULT_RX_PTHRESH      8
#define NGBE_DEFAULT_RX_HTHRESH      8
#define NGBE_DEFAULT_RX_WTHRESH      0

#define NGBE_DEFAULT_TX_FREE_THRESH  32
#define NGBE_DEFAULT_TX_PTHRESH      32
#define NGBE_DEFAULT_TX_HTHRESH      0
#define NGBE_DEFAULT_TX_WTHRESH      0

/* Additional timesync values. */
#define NGBE_INCVAL_1GB         0x2000000 /* all speed is same in Emerald */
#define NGBE_INCVAL_SHIFT_1GB   22 /* all speed is same in Emerald */

#define NGBE_CYCLECOUNTER_MASK   0xffffffffffffffffULL

/* store statistics names and its offset in stats structure */
struct rte_ngbe_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

const uint32_t *ngbe_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int ngbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
int ngbe_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
int ngbe_dev_rss_reta_query(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size);
void ngbe_vlan_hw_strip_bitmap_set(struct rte_eth_dev *dev,
		uint16_t queue, bool on);
void ngbe_config_vlan_strip_on_all_queues(struct rte_eth_dev *dev,
						  int mask);
void ngbe_dev_setup_link_alarm_handler(void *param);
void ngbe_read_stats_registers(struct ngbe_hw *hw,
			   struct ngbe_hw_stats *hw_stats);

#endif /* _NGBE_ETHDEV_H_ */
