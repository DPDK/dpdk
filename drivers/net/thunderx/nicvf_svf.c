/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#include <assert.h>
#include <stddef.h>

#include <rte_debug.h>
#include <rte_malloc.h>

#include "base/nicvf_bsvf.h"

#include "nicvf_svf.h"

void
nicvf_svf_push(struct nicvf *vf)
{
	struct svf_entry *entry = NULL;

	assert(vf != NULL);

	entry = rte_zmalloc("nicvf", sizeof(*entry), RTE_CACHE_LINE_SIZE);
	if (entry == NULL)
		rte_panic("Cannot allocate memory for svf_entry\n");

	entry->vf = vf;

	nicvf_bsvf_push(entry);
}

struct nicvf *
nicvf_svf_pop(void)
{
	struct nicvf *vf;
	struct svf_entry *entry;

	entry = nicvf_bsvf_pop();

	vf = entry->vf;

	rte_free(entry);

	return vf;
}

int
nicvf_svf_empty(void)
{
	return nicvf_bsvf_empty();
}
