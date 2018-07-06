/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_mbuf.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

#define BUFFER_SIZE_MIN        (sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)

int
softnic_mempool_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->mempool_list);

	return 0;
}

void
softnic_mempool_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_mempool *mempool;

		mempool = TAILQ_FIRST(&p->mempool_list);
		if (mempool == NULL)
			break;

		TAILQ_REMOVE(&p->mempool_list, mempool, node);
		rte_mempool_free(mempool->m);
		free(mempool);
	}
}

struct softnic_mempool *
softnic_mempool_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_mempool *mempool;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(mempool, &p->mempool_list, node)
		if (strcmp(mempool->name, name) == 0)
			return mempool;

	return NULL;
}

struct softnic_mempool *
softnic_mempool_create(struct pmd_internals *p,
	const char *name,
	struct softnic_mempool_params *params)
{
	char mempool_name[NAME_SIZE];
	struct softnic_mempool *mempool;
	struct rte_mempool *m;

	/* Check input params */
	if (name == NULL ||
		softnic_mempool_find(p, name) ||
		params == NULL ||
		params->buffer_size < BUFFER_SIZE_MIN ||
		params->pool_size == 0)
		return NULL;

	/* Resource create */
	snprintf(mempool_name, sizeof(mempool_name), "%s_%s",
		p->params.name,
		name);

	m = rte_pktmbuf_pool_create(mempool_name,
		params->pool_size,
		params->cache_size,
		0,
		params->buffer_size - sizeof(struct rte_mbuf),
		p->params.cpu_id);

	if (m == NULL)
		return NULL;

	/* Node allocation */
	mempool = calloc(1, sizeof(struct softnic_mempool));
	if (mempool == NULL) {
		rte_mempool_free(m);
		return NULL;
	}

	/* Node fill in */
	strlcpy(mempool->name, name, sizeof(mempool->name));
	mempool->m = m;
	mempool->buffer_size = params->buffer_size;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->mempool_list, mempool, node);

	return mempool;
}
