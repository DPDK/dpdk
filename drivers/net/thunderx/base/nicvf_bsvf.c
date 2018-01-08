/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <assert.h>
#include <stddef.h>
#include <err.h>

#include "nicvf_bsvf.h"
#include "nicvf_plat.h"

static STAILQ_HEAD(, svf_entry) head = STAILQ_HEAD_INITIALIZER(head);

void
nicvf_bsvf_push(struct svf_entry *entry)
{
	assert(entry != NULL);
	assert(entry->vf != NULL);

	STAILQ_INSERT_TAIL(&head, entry, next);
}

struct svf_entry *
nicvf_bsvf_pop(void)
{
	struct svf_entry *entry;

	assert(!STAILQ_EMPTY(&head));

	entry = STAILQ_FIRST(&head);

	assert(entry != NULL);
	assert(entry->vf != NULL);

	STAILQ_REMOVE_HEAD(&head, next);

	return entry;
}

int
nicvf_bsvf_empty(void)
{
	return STAILQ_EMPTY(&head);
}
