/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_string_fns.h>
#include <rte_tailq.h>

#include "rte_eth_softnic_internals.h"

int
softnic_swq_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->swq_list);

	return 0;
}

void
softnic_swq_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct softnic_swq *swq;

		swq = TAILQ_FIRST(&p->swq_list);
		if (swq == NULL)
			break;

		TAILQ_REMOVE(&p->swq_list, swq, node);
		rte_ring_free(swq->r);
		free(swq);
	}
}

void
softnic_softnic_swq_free_keep_rxq_txq(struct pmd_internals *p)
{
	struct softnic_swq *swq, *tswq;

	RTE_TAILQ_FOREACH_SAFE(swq, &p->swq_list, node, tswq) {
		if ((strncmp(swq->name, "RXQ", strlen("RXQ")) == 0) ||
			(strncmp(swq->name, "TXQ", strlen("TXQ")) == 0))
			continue;

		TAILQ_REMOVE(&p->swq_list, swq, node);
		rte_ring_free(swq->r);
		free(swq);
	}
}

struct softnic_swq *
softnic_swq_find(struct pmd_internals *p,
	const char *name)
{
	struct softnic_swq *swq;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(swq, &p->swq_list, node)
		if (strcmp(swq->name, name) == 0)
			return swq;

	return NULL;
}

struct softnic_swq *
softnic_swq_create(struct pmd_internals *p,
	const char *name,
	struct softnic_swq_params *params)
{
	char ring_name[NAME_SIZE];
	struct softnic_swq *swq;
	struct rte_ring *r;
	unsigned int flags = RING_F_SP_ENQ | RING_F_SC_DEQ;

	/* Check input params */
	if (name == NULL ||
		softnic_swq_find(p, name) ||
		params == NULL ||
		params->size == 0)
		return NULL;

	/* Resource create */
	snprintf(ring_name, sizeof(ring_name), "%s_%s",
		p->params.name,
		name);

	r = rte_ring_create(ring_name,
		params->size,
		p->params.cpu_id,
		flags);

	if (r == NULL)
		return NULL;

	/* Node allocation */
	swq = calloc(1, sizeof(struct softnic_swq));
	if (swq == NULL) {
		rte_ring_free(r);
		return NULL;
	}

	/* Node fill in */
	strlcpy(swq->name, name, sizeof(swq->name));
	swq->r = r;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&p->swq_list, swq, node);

	return swq;
}
