/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TX_H
#define SXE2_TX_H

#include "sxe2_queue.h"

void __rte_cold sxe2_tx_queue_reset(struct sxe2_tx_queue *txq);

int32_t __rte_cold sxe2_tx_queue_start(struct rte_eth_dev *dev, uint16_t queue_id);

void sxe2_tx_queue_mbufs_release(struct sxe2_tx_queue *txq);

int32_t __rte_cold sxe2_tx_queue_stop(struct rte_eth_dev *dev, uint16_t queue_id);

int32_t __rte_cold sxe2_tx_queue_setup(struct rte_eth_dev *dev,
		uint16_t queue_idx, uint16_t nb_desc, uint32_t socket_id,
		const struct rte_eth_txconf *tx_conf);

void __rte_cold sxe2_tx_queue_release(struct rte_eth_dev *dev, uint16_t queue_idx);

void __rte_cold sxe2_all_txqs_release(struct rte_eth_dev *dev);

void __rte_cold sxe2_tx_queue_info_get(struct rte_eth_dev *dev, uint16_t queue_id,
		struct rte_eth_txq_info *qinfo);

int32_t __rte_cold sxe2_txqs_all_start(struct rte_eth_dev *dev);

void __rte_cold sxe2_txqs_all_stop(struct rte_eth_dev *dev);

#endif /* SXE2_TX_H */
