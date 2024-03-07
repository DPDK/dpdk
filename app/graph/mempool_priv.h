/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_MEMPOOL_PRIV_H
#define APP_GRAPH_MEMPOOL_PRIV_H

#include "mempool.h"

struct mempools {
	struct mempool_config config[RTE_MAX_ETHPORTS];
	struct rte_mempool *mp[RTE_MAX_ETHPORTS];
	uint8_t	nb_pools;
};

#endif
