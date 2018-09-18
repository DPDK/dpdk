/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#ifndef _DSW_EVDEV_H_
#define _DSW_EVDEV_H_

#include <rte_eventdev.h>

#define DSW_PMD_NAME RTE_STR(event_dsw)

/* Code changes are required to allow more ports. */
#define DSW_MAX_PORTS (64)
#define DSW_MAX_PORT_DEQUEUE_DEPTH (128)
#define DSW_MAX_PORT_ENQUEUE_DEPTH (128)

#define DSW_MAX_QUEUES (16)

#define DSW_MAX_EVENTS (16384)

/* Code changes are required to allow more flows than 32k. */
#define DSW_MAX_FLOWS_BITS (15)
#define DSW_MAX_FLOWS (1<<(DSW_MAX_FLOWS_BITS))
#define DSW_MAX_FLOWS_MASK (DSW_MAX_FLOWS-1)

struct dsw_queue {
	uint8_t schedule_type;
	uint16_t num_serving_ports;
};

struct dsw_evdev {
	struct rte_eventdev_data *data;

	struct dsw_queue queues[DSW_MAX_QUEUES];
	uint8_t num_queues;
};

static inline struct dsw_evdev *
dsw_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#endif
