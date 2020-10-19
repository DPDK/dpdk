/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include "base/txgbe.h"
#include "txgbe_ptypes.h"

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
#define TXGBE_VLAN_TAG_SIZE 4
#define TXGBE_HKEY_MAX_INDEX 10
/*Default value of Max Rx Queue*/
#define TXGBE_MAX_RX_QUEUE_NUM	128
#define TXGBE_VMDQ_DCB_NB_QUEUES     TXGBE_MAX_RX_QUEUE_NUM

#define TXGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

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

struct txgbe_uta_info {
	uint8_t  uc_filter_type;
	uint16_t uta_in_use;
	uint32_t uta_shadow[TXGBE_MAX_UTA];
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
	struct txgbe_hw_stats       stats;
	struct txgbe_interrupt      intr;
	struct txgbe_stat_mappings  stat_mappings;
	struct txgbe_uta_info       uta_info;
	bool rx_bulk_alloc_allowed;
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

#define TXGBE_DEV_UTA_INFO(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->uta_info)

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

void txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

int
txgbe_dev_link_update_share(struct rte_eth_dev *dev,
		int wait_to_complete);

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

/* store statistics names and its offset in stats structure */
struct rte_txgbe_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

const uint32_t *txgbe_dev_supported_ptypes_get(struct rte_eth_dev *dev);
int txgbe_dev_set_mc_addr_list(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
void txgbe_dev_setup_link_alarm_handler(void *param);
void txgbe_read_stats_registers(struct txgbe_hw *hw,
			   struct txgbe_hw_stats *hw_stats);

#endif /* _TXGBE_ETHDEV_H_ */
