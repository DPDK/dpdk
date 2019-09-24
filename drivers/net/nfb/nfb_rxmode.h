/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#ifndef _NFB_RXMODE_H_
#define _NFB_RXMODE_H_

#include <nfb/nfb.h>
#include <nfb/ndp.h>

#include <rte_ethdev.h>

/**
 * Getter for promiscuous mode
 * @param dev
 *   Pointer to Ethernet device structure.
 * @return 1 if enabled 0 otherwise
 */
int
nfb_eth_promiscuous_get(struct rte_eth_dev *dev);

/**
 * DPDK callback to enable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return always 0
 */
int
nfb_eth_promiscuous_enable(struct rte_eth_dev *dev);

/**
 * DPDK callback to disable promiscuous mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return always 0
 */
int
nfb_eth_promiscuous_disable(struct rte_eth_dev *dev);

/**
 * Getter for allmulticast mode
 * @param dev
 *   Pointer to Ethernet device structure.
 * @return 1 if enabled 0 otherwise
 */
int
nfb_eth_allmulticast_get(struct rte_eth_dev *dev);

/**
 * DPDK callback to enable allmulticast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return always 0
 */
int
nfb_eth_allmulticast_enable(struct rte_eth_dev *dev);

/**
 * DPDK callback to disable allmulticast mode.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 *
 * @return always 0
 */
int
nfb_eth_allmulticast_disable(struct rte_eth_dev *dev);

#endif /* _NFB_RXMODE_H_ */
