/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_TXRX_H
#define SXE2_TXRX_H

#include <ethdev_driver.h>
#include "sxe2_queue.h"
void sxe2_set_common_function(struct rte_eth_dev *dev);

int32_t __rte_cold sxe2_tx_simple_batch_support_check(struct rte_eth_dev *dev,
	uint32_t *batch_flags);
uint16_t sxe2_tx_pkts_prepare(void *tx_queue,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);
int32_t sxe2_tx_done_cleanup(void *txq, uint32_t free_cnt);
void sxe2_tx_mode_func_set(struct rte_eth_dev *dev);
void __rte_cold sxe2_rx_queue_reset(struct sxe2_rx_queue *rxq);
void sxe2_rx_mode_func_set(struct rte_eth_dev *dev);

int32_t sxe2_tx_burst_mode_get(struct rte_eth_dev *dev,
			__rte_unused uint16_t queue_id, struct rte_eth_burst_mode *mode);
int32_t sxe2_rx_burst_mode_get(struct rte_eth_dev *dev,
			__rte_unused uint16_t queue_id, struct rte_eth_burst_mode *mode);

extern const uint32_t sxe2_ptype_tbl[];

const uint32_t *sxe2_dev_supported_ptypes_get(struct rte_eth_dev *dev,
					      size_t *no_of_elements);

#endif /* SXE2_TXRX_H */
