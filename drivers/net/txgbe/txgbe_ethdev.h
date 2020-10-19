/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#ifndef _TXGBE_ETHDEV_H_
#define _TXGBE_ETHDEV_H_

#include "base/txgbe.h"

/*
 * Structure to store private data for each driver instance (for each port).
 */
struct txgbe_adapter {
	struct txgbe_hw             hw;
};

#define TXGBE_DEV_HW(dev) \
	(&((struct txgbe_adapter *)(dev)->data->dev_private)->hw)

#define TXGBE_VMDQ_NUM_UC_MAC         4096 /* Maximum nb. of UC MAC addr. */
#endif /* _TXGBE_ETHDEV_H_ */
