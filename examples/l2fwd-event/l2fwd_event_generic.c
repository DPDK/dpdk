/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <stdbool.h>
#include <getopt.h>

#include <rte_cycles.h>
#include <rte_ethdev.h>
#include <rte_eventdev.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_lcore.h>
#include <rte_spinlock.h>

#include "l2fwd_common.h"
#include "l2fwd_event.h"

void
l2fwd_event_set_generic_ops(struct event_setup_ops *ops)
{
	RTE_SET_USED(ops);
}
