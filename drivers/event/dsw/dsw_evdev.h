/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#ifndef _DSW_EVDEV_H_
#define _DSW_EVDEV_H_

#include <rte_event_ring.h>
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

/* The rings are dimensioned so that all in-flight events can reside
 * on any one of the port rings, to avoid the trouble of having to
 * care about the case where there's no room on the destination port's
 * input ring.
 */
#define DSW_IN_RING_SIZE (DSW_MAX_EVENTS)

struct dsw_port {
	uint16_t id;

	/* Keeping a pointer here to avoid container_of() calls, which
	 * are expensive since they are very frequent and will result
	 * in an integer multiplication (since the port id is an index
	 * into the dsw_evdev port array).
	 */
	struct dsw_evdev *dsw;

	uint16_t dequeue_depth;
	uint16_t enqueue_depth;

	int32_t new_event_threshold;

	struct rte_event_ring *in_ring __rte_cache_aligned;
} __rte_cache_aligned;

struct dsw_queue {
	uint8_t schedule_type;
	uint16_t num_serving_ports;
};

struct dsw_evdev {
	struct rte_eventdev_data *data;

	struct dsw_port ports[DSW_MAX_PORTS];
	uint16_t num_ports;
	struct dsw_queue queues[DSW_MAX_QUEUES];
	uint8_t num_queues;
};

static inline struct dsw_evdev *
dsw_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#endif
