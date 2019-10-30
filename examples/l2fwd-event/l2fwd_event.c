/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>
#include <getopt.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>

#include "l2fwd_event.h"

void
l2fwd_event_resource_setup(struct l2fwd_resources *rsrc)
{
	struct l2fwd_event_resources *evt_rsrc;

	if (!rte_event_dev_count())
		rte_panic("No Eventdev found\n");

	evt_rsrc = rte_zmalloc("l2fwd_event",
				 sizeof(struct l2fwd_event_resources), 0);
	if (evt_rsrc == NULL)
		rte_panic("Failed to allocate memory\n");

	rsrc->evt_rsrc = evt_rsrc;
}
