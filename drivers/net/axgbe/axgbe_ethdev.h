/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 *   Copyright(c) 2018 Synopsys, Inc. All rights reserved.
 */

#ifndef RTE_ETH_AXGBE_H_
#define RTE_ETH_AXGBE_H_

#include <rte_mempool.h>
#include <rte_lcore.h>
#include "axgbe_common.h"

/*
 * Structure to store private data for each port.
 */
struct axgbe_port {
	/*  Ethdev where port belongs*/
	struct rte_eth_dev *eth_dev;
	/* Pci dev info */
	const struct rte_pci_device *pci_dev;
};

#endif /* RTE_ETH_AXGBE_H_ */
