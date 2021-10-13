/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IGC_TXRX_H_
#define _IGC_TXRX_H_

#include "igc_ethdev.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * RX/TX function prototypes
 */
void eth_igc_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void eth_igc_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
void igc_dev_clear_queues(struct rte_eth_dev *dev);
int eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);

uint32_t eth_igc_rx_queue_count(void *rx_queue);

int eth_igc_rx_descriptor_status(void *rx_queue, uint16_t offset);

int eth_igc_tx_descriptor_status(void *tx_queue, uint16_t offset);

int eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);
int eth_igc_tx_done_cleanup(void *txqueue, uint32_t free_cnt);

int igc_rx_init(struct rte_eth_dev *dev);
void igc_tx_init(struct rte_eth_dev *dev);
void igc_rss_disable(struct rte_eth_dev *dev);
void
igc_hw_rss_hash_set(struct igc_hw *hw, struct rte_eth_rss_conf *rss_conf);
int igc_del_rss_filter(struct rte_eth_dev *dev);
void igc_rss_conf_set(struct igc_rss_filter *out,
		const struct rte_flow_action_rss *rss);
int igc_add_rss_filter(struct rte_eth_dev *dev, struct igc_rss_filter *rss);
void igc_clear_rss_filter(struct rte_eth_dev *dev);
void eth_igc_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
void eth_igc_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);
void eth_igc_vlan_strip_queue_set(struct rte_eth_dev *dev,
			uint16_t rx_queue_id, int on);
#ifdef __cplusplus
}
#endif

#endif /* _IGC_TXRX_H_ */
