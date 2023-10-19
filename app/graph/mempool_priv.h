/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_MEMPOOL_PRIV_H
#define APP_GRAPH_MEMPOOL_PRIV_H

#include "mempool.h"

struct mempool_config_cmd_tokens {
	cmdline_fixed_string_t mempool;
	cmdline_fixed_string_t size;
	cmdline_fixed_string_t buffers;
	cmdline_fixed_string_t cache;
	cmdline_fixed_string_t numa;
	cmdline_fixed_string_t name;
	uint16_t buf_sz;
	uint16_t nb_bufs;
	uint16_t cache_size;
	uint16_t node;
};

struct mempool_help_cmd_tokens {
	cmdline_fixed_string_t help;
	cmdline_fixed_string_t mempool;
};

struct mempools {
	struct mempool_config config[RTE_MAX_ETHPORTS];
	struct rte_mempool *mp[RTE_MAX_ETHPORTS];
	uint8_t	nb_pools;
};

#endif
