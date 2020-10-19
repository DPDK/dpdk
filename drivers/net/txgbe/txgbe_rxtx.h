/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_RXTX_H_
#define _TXGBE_RXTX_H_

#define TXGBE_TX_MAX_SEG                    40

uint64_t txgbe_get_tx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_queue_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_rx_port_offloads(struct rte_eth_dev *dev);
uint64_t txgbe_get_tx_queue_offloads(struct rte_eth_dev *dev);

#endif /* _TXGBE_RXTX_H_ */
