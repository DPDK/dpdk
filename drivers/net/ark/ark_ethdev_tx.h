/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_ETHDEV_TX_H_
#define _ARK_ETHDEV_TX_H_

#include <stdint.h>

#include <rte_ethdev_driver.h>


uint16_t eth_ark_xmit_pkts_noop(void *vtxq,
				struct rte_mbuf **tx_pkts,
				uint16_t nb_pkts);
uint16_t eth_ark_xmit_pkts(void *vtxq,
			   struct rte_mbuf **tx_pkts,
			   uint16_t nb_pkts);
int eth_ark_tx_queue_setup(struct rte_eth_dev *dev,
			   uint16_t queue_idx,
			   uint16_t nb_desc,
			   unsigned int socket_id,
			   const struct rte_eth_txconf *tx_conf);
void eth_ark_tx_queue_release(void *vtx_queue);
int eth_ark_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id);
int eth_ark_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id);
void eth_tx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats);
void eth_tx_queue_stats_reset(void *vqueue);

#endif
