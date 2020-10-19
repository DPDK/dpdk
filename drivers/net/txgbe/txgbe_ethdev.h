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

#endif /* _TXGBE_ETHDEV_H_ */
