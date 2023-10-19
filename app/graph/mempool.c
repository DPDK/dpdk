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

static void
cli_mempool_help(__rte_unused void *parsed_result, __rte_unused struct cmdline *cl,
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

static void
cli_mempool(void *parsed_result, __rte_unused struct cmdline *cl, __rte_unused void *data)
{
	struct mempool_config_cmd_tokens *res = parsed_result;
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

cmdline_parse_token_string_t mempool_config_add_mempool =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, mempool, "mempool");
cmdline_parse_token_string_t mempool_config_add_name =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, name, NULL);
cmdline_parse_token_string_t mempool_config_add_size =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, size, "size");
cmdline_parse_token_num_t mempool_config_add_buf_sz =
	TOKEN_NUM_INITIALIZER(struct mempool_config_cmd_tokens, buf_sz, RTE_UINT16);
cmdline_parse_token_string_t mempool_config_add_buffers =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, buffers, "buffers");
cmdline_parse_token_num_t mempool_config_add_nb_bufs =
	TOKEN_NUM_INITIALIZER(struct mempool_config_cmd_tokens, nb_bufs, RTE_UINT16);
cmdline_parse_token_string_t mempool_config_add_cache =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, cache, "cache");
cmdline_parse_token_num_t mempool_config_add_cache_size =
	TOKEN_NUM_INITIALIZER(struct mempool_config_cmd_tokens, cache_size, RTE_UINT16);
cmdline_parse_token_string_t mempool_config_add_numa =
	TOKEN_STRING_INITIALIZER(struct mempool_config_cmd_tokens, numa, "numa");
cmdline_parse_token_num_t mempool_config_add_node =
	TOKEN_NUM_INITIALIZER(struct mempool_config_cmd_tokens, node, RTE_UINT16);

cmdline_parse_inst_t mempool_config_cmd_ctx = {
	.f = cli_mempool,
	.data = NULL,
	.help_str = cmd_mempool_help,
	.tokens = {
		(void *)&mempool_config_add_mempool,
		(void *)&mempool_config_add_name,
		(void *)&mempool_config_add_size,
		(void *)&mempool_config_add_buf_sz,
		(void *)&mempool_config_add_buffers,
		(void *)&mempool_config_add_nb_bufs,
		(void *)&mempool_config_add_cache,
		(void *)&mempool_config_add_cache_size,
		(void *)&mempool_config_add_numa,
		(void *)&mempool_config_add_node,
		NULL,
	},
};

cmdline_parse_token_string_t mempool_help_cmd =
	TOKEN_STRING_INITIALIZER(struct mempool_help_cmd_tokens, help, "help");
cmdline_parse_token_string_t mempool_help_mempool =
	TOKEN_STRING_INITIALIZER(struct mempool_help_cmd_tokens, mempool, "mempool");

cmdline_parse_inst_t mempool_help_cmd_ctx = {
	.f = cli_mempool_help,
	.data = NULL,
	.help_str = "",
	.tokens = {
		(void *)&mempool_help_cmd,
		(void *)&mempool_help_mempool,
		NULL,
	},
};
