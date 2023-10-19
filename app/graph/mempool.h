/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#ifndef APP_GRAPH_MEMPOOL_H
#define APP_GRAPH_MEMPOOL_H

#include <cmdline_parse.h>
#include <rte_mempool.h>

struct mempool_config {
	char name[RTE_MEMPOOL_NAMESIZE];
	int pool_size;
	int cache_size;
	int buffer_size;
	int numa_node;
};

extern cmdline_parse_inst_t mempool_config_cmd_ctx;
extern cmdline_parse_inst_t mempool_help_cmd_ctx;

int mempool_process(struct mempool_config *config);

#endif
