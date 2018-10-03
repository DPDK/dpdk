/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Marvell International Ltd.
 * Copyright(c) 2018 Semihalf.
 * All rights reserved.
 */

#ifndef _MVNETA_RXTX_H_
#define _MVNETA_RXTX_H_

#include "mvneta_ethdev.h"

int mvneta_alloc_rx_bufs(struct rte_eth_dev *dev);

void mvneta_flush_queues(struct rte_eth_dev *dev);

void mvneta_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id,
			 struct rte_eth_rxq_info *qinfo);
void mvneta_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			 struct rte_eth_txq_info *qinfo);

void mvneta_set_tx_function(struct rte_eth_dev *dev);

uint16_t
mvneta_rx_pkt_burst(void *rxq, struct rte_mbuf **rx_pkts, uint16_t nb_pkts);

int
mvneta_rx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		      unsigned int socket,
		      const struct rte_eth_rxconf *conf __rte_unused,
		      struct rte_mempool *mp);
int
mvneta_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		      unsigned int socket, const struct rte_eth_txconf *conf);

void mvneta_rx_queue_release(void *rxq);
void mvneta_tx_queue_release(void *txq);

#endif /* _MVNETA_RXTX_H_ */
