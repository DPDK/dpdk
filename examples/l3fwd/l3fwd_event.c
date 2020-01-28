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

static void
l3fwd_event_capability_setup(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();
	uint32_t caps = 0;
	uint16_t i;
	int ret;

	RTE_ETH_FOREACH_DEV(i) {
		ret = rte_event_eth_tx_adapter_caps_get(0, i, &caps);
		if (ret)
			rte_exit(EXIT_FAILURE,
				 "Invalid capability for Tx adptr port %d\n",
				 i);

		evt_rsrc->tx_mode_q |= !(caps &
				   RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT);
	}

	if (evt_rsrc->tx_mode_q)
		l3fwd_event_set_generic_ops(&evt_rsrc->ops);
	else
		l3fwd_event_set_internal_port_ops(&evt_rsrc->ops);
}

void
l3fwd_event_resource_setup(void)
{
	struct l3fwd_event_resources *evt_rsrc = l3fwd_get_eventdev_rsrc();

	if (!evt_rsrc->enabled)
		return;

	if (!rte_event_dev_count())
		rte_exit(EXIT_FAILURE, "No Eventdev found");

	/* Setup eventdev capability callbacks */
	l3fwd_event_capability_setup();
}
