/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>
#include <getopt.h>

#include <rte_malloc.h>

#include "l3fwd.h"
#include "l3fwd_event.h"

struct l3fwd_event_resources *
l3fwd_get_eventdev_rsrc(void)
{
	static struct l3fwd_event_resources *rsrc;

	if (rsrc != NULL)
		return rsrc;

	rsrc = rte_zmalloc("l3fwd", sizeof(struct l3fwd_event_resources), 0);
	if (rsrc != NULL) {
		rsrc->sched_type = RTE_SCHED_TYPE_ATOMIC;
		rsrc->eth_rx_queues = 1;
		return rsrc;
	}

	rte_exit(EXIT_FAILURE, "Unable to allocate memory for eventdev cfg\n");

	return NULL;
}

void
l3fwd_event_resource_setup(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	if (!evt_rsrc->enabled)
		return;
}
