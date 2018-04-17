/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_string_fns.h>

#include "swq.h"

static struct swq_list swq_list;

int
swq_init(void)
{
	TAILQ_INIT(&swq_list);

	return 0;
}

struct swq *
swq_find(const char *name)
{
	struct swq *swq;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(swq, &swq_list, node)
		if (strcmp(swq->name, name) == 0)
			return swq;

	return NULL;
}

struct swq *
swq_create(const char *name, struct swq_params *params)
{
	struct swq *swq;
	struct rte_ring *r;
	unsigned int flags = RING_F_SP_ENQ | RING_F_SC_DEQ;

	/* Check input params */
	if ((name == NULL) ||
		swq_find(name) ||
		(params == NULL) ||
		(params->size == 0))
		return NULL;

	/* Resource create */
	r = rte_ring_create(
		name,
		params->size,
		params->cpu_id,
		flags);

	if (r == NULL)
		return NULL;

	/* Node allocation */
	swq = calloc(1, sizeof(struct swq));
	if (swq == NULL) {
		rte_ring_free(r);
		return NULL;
	}

	/* Node fill in */
	strlcpy(swq->name, name, sizeof(swq->name));
	swq->r = r;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&swq_list, swq, node);

	return swq;
}
