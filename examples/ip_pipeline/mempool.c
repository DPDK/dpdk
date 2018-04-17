/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_string_fns.h>

#include "mempool.h"

#define BUFFER_SIZE_MIN        (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

static struct mempool_list mempool_list;

int
mempool_init(void)
{
	TAILQ_INIT(&mempool_list);

	return 0;
}

struct mempool *
mempool_find(const char *name)
{
	struct mempool *mempool;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(mempool, &mempool_list, node)
		if (strcmp(mempool->name, name) == 0)
			return mempool;

	return NULL;
}

struct mempool *
mempool_create(const char *name, struct mempool_params *params)
{
	struct mempool *mempool;
	struct rte_mempool *m;

	/* Check input params */
	if ((name == NULL) ||
		mempool_find(name) ||
		(params == NULL) ||
		(params->buffer_size < BUFFER_SIZE_MIN) ||
		(params->pool_size == 0))
		return NULL;

	/* Resource create */
	m = rte_pktmbuf_pool_create(
		name,
		params->pool_size,
		params->cache_size,
		0,
		params->buffer_size - sizeof(struct rte_mbuf),
		params->cpu_id);

	if (m == NULL)
		return NULL;

	/* Node allocation */
	mempool = calloc(1, sizeof(struct mempool));
	if (mempool == NULL) {
		rte_mempool_free(m);
		return NULL;
	}

	/* Node fill in */
	strlcpy(mempool->name, name, sizeof(mempool->name));
	mempool->m = m;
	mempool->buffer_size = params->buffer_size;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&mempool_list, mempool, node);

	return mempool;
}
