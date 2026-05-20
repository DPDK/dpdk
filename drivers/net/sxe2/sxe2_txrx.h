/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TXRX_H
#define SXE2_TXRX_H

#include <ethdev_driver.h>
#include "sxe2_queue.h"

void sxe2_set_common_function(struct rte_eth_dev *dev);

uint16_t sxe2_tx_pkts_prepare(void *tx_queue,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
void sxe2_tx_mode_func_set(struct rte_eth_dev *dev);
void __rte_cold sxe2_rx_queue_reset(struct sxe2_rx_queue *rxq);
void sxe2_rx_mode_func_set(struct rte_eth_dev *dev);

#endif /* SXE2_TXRX_H */
