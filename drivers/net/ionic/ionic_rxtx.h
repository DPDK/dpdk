/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 Advanced Micro Devices, Inc.
 */

#ifndef _IONIC_RXTX_H_
#define _IONIC_RXTX_H_

#include <rte_mbuf.h>

struct ionic_rx_service {
	/* cb in */
	struct rte_mbuf **rx_pkts;
	/* cb out */
	uint16_t nb_rx;
};

uint16_t ionic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
	uint16_t nb_pkts);
uint16_t ionic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);
uint16_t ionic_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
	uint16_t nb_pkts);

int ionic_dev_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
	uint16_t nb_desc, uint32_t socket_id,
	const struct rte_eth_rxconf *rx_conf, struct rte_mempool *mp);
void ionic_dev_rx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int ionic_dev_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);
int ionic_dev_rx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t rx_queue_id);

int ionic_dev_tx_queue_setup(struct rte_eth_dev *dev, uint16_t tx_queue_id,
	uint16_t nb_desc,  uint32_t socket_id,
	const struct rte_eth_txconf *tx_conf);
void ionic_dev_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid);
int ionic_dev_tx_queue_stop(struct rte_eth_dev *eth_dev, uint16_t tx_queue_id);
int ionic_dev_tx_queue_start(struct rte_eth_dev *dev, uint16_t tx_queue_id);

void ionic_rxq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_rxq_info *qinfo);
void ionic_txq_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
	struct rte_eth_txq_info *qinfo);

#endif /* _IONIC_RXTX_H_ */
