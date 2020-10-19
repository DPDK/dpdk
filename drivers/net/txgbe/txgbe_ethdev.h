/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include "base/txgbe.h"

/* need update link, bit flag */
#define TXGBE_FLAG_NEED_LINK_UPDATE (uint32_t)(1 << 0)
#define TXGBE_FLAG_MAILBOX          (uint32_t)(1 << 1)
#define TXGBE_FLAG_PHY_INTERRUPT    (uint32_t)(1 << 2)
#define TXGBE_FLAG_MACSEC           (uint32_t)(1 << 3)
#define TXGBE_FLAG_NEED_LINK_CONFIG (uint32_t)(1 << 4)

/*
 * Defines that were not part of txgbe_type.h as they are not used by the
 * FreeBSD driver.
 */
#define TXGBE_HKEY_MAX_INDEX 10

#define TXGBE_QUEUE_ITR_INTERVAL_DEFAULT	500 /* 500us */

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

#define TXGBE_MISC_VEC_ID               RTE_INTR_VEC_ZERO_OFFSET
#define TXGBE_RX_VEC_START              RTE_INTR_VEC_RXTX_OFFSET

/* structure for interrupt relative data */
struct txgbe_interrupt {
	uint32_t flags;
	uint32_t mask_misc;
	/* to save original mask during delayed handler */
	uint32_t mask_misc_orig;
	uint32_t mask[2];
};

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
	struct txgbe_interrupt      intr;
};

#define TXGBE_DEV_HW(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hw)

#define TXGBE_DEV_INTR(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->intr)

void txgbe_set_ivar_map(struct txgbe_hw *hw, int8_t direction,
			       uint8_t queue, uint8_t msix_vector);

#define TXGBE_LINK_DOWN_CHECK_TIMEOUT 4000 /* ms */
#define TXGBE_LINK_UP_CHECK_TIMEOUT   1000 /* ms */
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
