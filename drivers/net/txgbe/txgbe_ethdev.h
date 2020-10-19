/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include "base/txgbe.h"

/*
 * Defines that were not part of txgbe_type.h as they are not used by the
 * FreeBSD driver.
 */
#define TXGBE_HKEY_MAX_INDEX 10

#define TXGBE_RSS_OFFLOAD_ALL ( \
	ETH_RSS_IPV4 | \
	ETH_RSS_NONFRAG_IPV4_TCP | \
	ETH_RSS_NONFRAG_IPV4_UDP | \
	ETH_RSS_IPV6 | \
	ETH_RSS_NONFRAG_IPV6_TCP | \
	ETH_RSS_NONFRAG_IPV6_UDP | \
	ETH_RSS_IPV6_EX | \
	ETH_RSS_IPV6_TCP_EX | \
	ETH_RSS_IPV6_UDP_EX)

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
};

#define TXGBE_DEV_HW(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hw)

#define TXGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */

/*
 *  Default values for RX/TX configuration
 */
#define TXGBE_DEFAULT_RX_FREE_THRESH  32
#define TXGBE_DEFAULT_RX_PTHRESH      8
#define TXGBE_DEFAULT_RX_HTHRESH      8
#define TXGBE_DEFAULT_RX_WTHRESH      0

#define TXGBE_DEFAULT_TX_FREE_THRESH  32
#define TXGBE_DEFAULT_TX_PTHRESH      32
#define TXGBE_DEFAULT_TX_HTHRESH      0
#define TXGBE_DEFAULT_TX_WTHRESH      0

#endif /* _TXGBE_ETHDEV_H_ */
