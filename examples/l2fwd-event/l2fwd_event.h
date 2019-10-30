/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L2FWD_EVENT_H__
#define __L2FWD_EVENT_H__

#include <rte_common.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_mbuf.h>
#include <rte_spinlock.h>

#include "l2fwd_common.h"

struct event_setup_ops {
};

struct l2fwd_event_resources {
	uint8_t tx_mode_q;
	struct event_setup_ops ops;
};

void l2fwd_event_resource_setup(struct l2fwd_resources *rsrc);
void l2fwd_event_set_generic_ops(struct event_setup_ops *ops);
void l2fwd_event_set_internal_port_ops(struct event_setup_ops *ops);

#endif /* __L2FWD_EVENT_H__ */
