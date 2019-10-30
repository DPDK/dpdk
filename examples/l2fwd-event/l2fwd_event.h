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

typedef uint32_t (*event_device_setup_cb)(struct l2fwd_resources *rsrc);

struct event_queues {
	uint8_t	nb_queues;
};

struct event_ports {
	uint8_t	nb_ports;
};

struct event_setup_ops {
	event_device_setup_cb event_device_setup;
};

struct l2fwd_event_resources {
	uint8_t tx_mode_q;
	uint8_t has_burst;
	uint8_t event_d_id;
	uint8_t disable_implicit_release;
	struct event_ports evp;
	struct event_queues evq;
	struct event_setup_ops ops;
};

void l2fwd_event_resource_setup(struct l2fwd_resources *rsrc);
void l2fwd_event_set_generic_ops(struct event_setup_ops *ops);
void l2fwd_event_set_internal_port_ops(struct event_setup_ops *ops);

#endif /* __L2FWD_EVENT_H__ */
