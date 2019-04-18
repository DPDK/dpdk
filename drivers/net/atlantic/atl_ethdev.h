/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */

#ifndef _ATLANTIC_ETHDEV_H_
#define _ATLANTIC_ETHDEV_H_
#include <rte_errno.h>
#include "rte_ethdev.h"

#include "atl_types.h"
#include "hw_atl/hw_atl_utils.h"

#define ATL_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX)

#define ATL_DEV_PRIVATE_TO_HW(adapter) \
	(&((struct atl_adapter *)adapter)->hw)

#define ATL_DEV_TO_ADAPTER(dev) \
	((struct atl_adapter *)(dev)->data->dev_private)

#define ATL_DEV_PRIVATE_TO_INTR(adapter) \
	(&((struct atl_adapter *)adapter)->intr)

#define ATL_DEV_PRIVATE_TO_CFG(adapter) \
	(&((struct atl_adapter *)adapter)->hw_cfg)

#define ATL_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define ATL_FLAG_MACSEC (uint32_t)(4 << 0)

struct atl_interrupt {
	uint32_t flags;
	uint32_t mask;
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct atl_adapter {
	struct aq_hw_s             hw;
	struct aq_hw_cfg_s         hw_cfg;
	struct atl_sw_stats        sw_stats;
	struct atl_interrupt       intr;
};

/*
 * RX/TX function prototypes
 */
void atl_rx_queue_release(void *rxq);
void atl_tx_queue_release(void *txq);

int atl_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

int atl_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
		uint16_t nb_tx_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

uint32_t atl_rx_queue_count(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int atl_dev_rx_descriptor_status(void *rx_queue, uint16_t offset);
int atl_dev_tx_descriptor_status(void *tx_queue, uint16_t offset);

int atl_dev_rx_queue_intr_enable(struct rte_eth_dev *eth_dev,
				 uint16_t queue_id);
int atl_dev_rx_queue_intr_disable(struct rte_eth_dev *eth_dev,
				  uint16_t queue_id);

int atl_rx_init(struct rte_eth_dev *dev);
int atl_tx_init(struct rte_eth_dev *dev);

int atl_start_queues(struct rte_eth_dev *dev);
int atl_stop_queues(struct rte_eth_dev *dev);
void atl_free_queues(struct rte_eth_dev *dev);

int atl_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int atl_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int atl_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);
int atl_tx_queue_stop(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void atl_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);

void atl_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

uint16_t atl_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
		uint16_t nb_pkts);

uint16_t atl_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

uint16_t atl_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
		uint16_t nb_pkts);

int atl_macsec_enable(struct rte_eth_dev *dev, uint8_t encr, uint8_t repl_prot);
int atl_macsec_disable(struct rte_eth_dev *dev);
int atl_macsec_config_txsc(struct rte_eth_dev *dev, uint8_t *mac);
int atl_macsec_config_rxsc(struct rte_eth_dev *dev,
			   uint8_t *mac, uint16_t pi);
int atl_macsec_select_txsa(struct rte_eth_dev *dev, uint8_t idx,
			   uint8_t an, uint32_t pn, uint8_t *key);
int atl_macsec_select_rxsa(struct rte_eth_dev *dev, uint8_t idx,
			   uint8_t an, uint32_t pn, uint8_t *key);

bool is_atlantic_supported(struct rte_eth_dev *dev);

#endif /* _ATLANTIC_ETHDEV_H_ */
