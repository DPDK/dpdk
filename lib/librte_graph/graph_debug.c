/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_common.h>
#include <rte_debug.h>

#include "graph_private.h"

void
node_dump(FILE *f, struct node *n)
{
	rte_edge_t i;

	fprintf(f, "node <%s>\n", n->name);
	fprintf(f, "  id=%" PRIu32 "\n", n->id);
	fprintf(f, "  flags=0x%" PRIx64 "\n", n->flags);
	fprintf(f, "  addr=%p\n", n);
	fprintf(f, "  process=%p\n", n->process);
	fprintf(f, "  nb_edges=%d\n", n->nb_edges);

	for (i = 0; i < n->nb_edges; i++)
		fprintf(f, "     edge[%d] <%s>\n", i, n->next_nodes[i]);
}
