/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __L3FWD_EVENTDEV_H__
#define __L3FWD_EVENTDEV_H__

#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_event_eth_tx_adapter.h>
#include <rte_spinlock.h>

#include "l3fwd.h"

typedef uint32_t (*event_device_setup_cb)(void);
typedef void (*event_queue_setup_cb)(uint32_t event_queue_cfg);
typedef void (*event_port_setup_cb)(void);
typedef void (*adapter_setup_cb)(void);
typedef int (*event_loop_cb)(void *);

struct l3fwd_event_setup_ops {
	event_device_setup_cb event_device_setup;
	event_queue_setup_cb event_queue_setup;
	event_port_setup_cb event_port_setup;
	adapter_setup_cb adapter_setup;
	event_loop_cb lpm_event_loop;
	event_loop_cb em_event_loop;
};

struct l3fwd_event_resources {
	struct l3fwd_event_setup_ops ops;
	uint8_t sched_type;
	uint8_t tx_mode_q;
	uint8_t enabled;
	uint8_t eth_rx_queues;
};

struct l3fwd_event_resources *l3fwd_get_eventdev_rsrc(void);
void l3fwd_event_resource_setup(void);
void l3fwd_event_set_generic_ops(struct l3fwd_event_setup_ops *ops);
void l3fwd_event_set_internal_port_ops(struct l3fwd_event_setup_ops *ops);

#endif /* __L3FWD_EVENTDEV_H__ */
