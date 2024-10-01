/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */
#ifndef __CN20K_ETHDEV_H__
#define __CN20K_ETHDEV_H__

#include <cn20k_rxtx.h>
#include <cnxk_ethdev.h>
#include <cnxk_security.h>

/* Rx and Tx routines */
void cn20k_eth_set_rx_function(struct rte_eth_dev *eth_dev);
void cn20k_eth_set_tx_function(struct rte_eth_dev *eth_dev);

#endif /* __CN20K_ETHDEV_H__ */
