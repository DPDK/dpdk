/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2018 Atomic Rules LLC
 */

#ifndef _ARK_ETHDEV_RX_H_
#define _ARK_ETHDEV_RX_H_

#include <stdint.h>

#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_ethdev_driver.h>

extern uint64_t ark_timestamp_rx_dynflag;
extern int ark_timestamp_dynfield_offset;

int eth_ark_dev_rx_queue_setup(struct rte_eth_dev *dev,
			       uint16_t queue_idx,
			       uint16_t nb_desc,
			       unsigned int socket_id,
			       const struct rte_eth_rxconf *rx_conf,
			       struct rte_mempool *mp);
uint32_t eth_ark_dev_rx_queue_count(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
int eth_ark_rx_stop_queue(struct rte_eth_dev *dev, uint16_t queue_id);
int eth_ark_rx_start_queue(struct rte_eth_dev *dev, uint16_t queue_id);
uint16_t eth_ark_recv_pkts_noop(void *rx_queue, struct rte_mbuf **rx_pkts,
				uint16_t nb_pkts);
uint16_t eth_ark_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
void eth_ark_dev_rx_queue_release(void *rx_queue);
void eth_rx_queue_stats_get(void *vqueue, struct rte_eth_stats *stats);
void eth_rx_queue_stats_reset(void *vqueue);
void eth_ark_rx_dump_queue(struct rte_eth_dev *dev, uint16_t queue_id,
			   const char *msg);
void eth_ark_udm_force_close(struct rte_eth_dev *dev);

#endif
