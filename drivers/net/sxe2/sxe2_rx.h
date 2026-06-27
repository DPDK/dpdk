/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_RX_H
#define SXE2_RX_H

#include "sxe2_queue.h"

int32_t __rte_cold sxe2_rx_queue_setup(struct rte_eth_dev *dev,
				uint16_t queue_idx, uint16_t nb_desc, uint32_t socket_id,
				const struct rte_eth_rxconf *rx_conf,
				struct rte_mempool *mp);

int32_t __rte_cold sxe2_rx_queue_stop(struct rte_eth_dev *dev, uint16_t rx_queue_id);

void __rte_cold sxe2_rx_queue_mbufs_release(struct sxe2_rx_queue *rxq);

void __rte_cold sxe2_rx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);

void __rte_cold sxe2_all_rxqs_release(struct rte_eth_dev *dev);

void __rte_cold sxe2_rx_queue_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		struct rte_eth_rxq_info *qinfo);

int32_t __rte_cold sxe2_rx_queue_start(struct rte_eth_dev *dev, uint16_t rx_queue_id);

int32_t __rte_cold sxe2_rxqs_all_start(struct rte_eth_dev *dev);

void __rte_cold sxe2_rxqs_all_stop(struct rte_eth_dev *dev);

int32_t sxe2_get_monitor_addr(void *rx_queue, struct rte_power_monitor_cond *pmc);

#endif /* SXE2_RX_H */
