/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Marvell.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <rte_common.h>
#include <rte_mbuf.h>

#include "mempool_priv.h"
#include "module_api.h"

static const char
cmd_mempool_help[] = "mempool <mempool_name> size <mbuf_size> buffers <number_of_buffers> "
		     "cache <cache_size> numa <numa_id>";

struct mempools mpconfig;

int
mempool_process(struct mempool_config *config)
{
	struct rte_mempool *mp;
	uint8_t nb_pools;

	nb_pools = mpconfig.nb_pools;
	rte_strscpy(mpconfig.config[nb_pools].name, config->name, RTE_MEMPOOL_NAMESIZE);
	mpconfig.config[nb_pools].pool_size = config->pool_size;
	mpconfig.config[nb_pools].buffer_size = config->buffer_size;
	mpconfig.config[nb_pools].cache_size = config->cache_size;
	mpconfig.config[nb_pools].numa_node = config->numa_node;

	mp = rte_pktmbuf_pool_create(config->name, config->pool_size, config->cache_size,
		128, config->buffer_size, config->numa_node);
	if (!mp)
		return -EINVAL;

	mpconfig.mp[nb_pools] = mp;
	nb_pools++;
	mpconfig.nb_pools = nb_pools;

	return 0;
}

void
cmd_help_mempool_parsed(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
			__rte_unused void *data)
{
	size_t len;

	len = strlen(conn->msg_out);
	conn->msg_out += len;
	snprintf(conn->msg_out, conn->msg_out_len_max, "\n%s\n%s\n",
		 "----------------------------- mempool command help -----------------------------",
		 cmd_mempool_help);

	len = strlen(conn->msg_out);
	conn->msg_out_len_max -= len;
}

void
cmd_mempool_parsed(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct cmd_mempool_result *res = parsed_result;
	struct mempool_config config;
	int rc = -EINVAL;


	rte_strscpy(config.name, res->name, RTE_MEMPOOL_NAMESIZE);
	config.name[strlen(res->name)] = '\0';
	config.pool_size = res->nb_bufs;
	config.buffer_size = res->buf_sz;
	config.cache_size = res->cache_size;
	config.numa_node = res->node;

	rc = mempool_process(&config);
	if (rc < 0)
		printf(MSG_CMD_FAIL, "mempool");
}
