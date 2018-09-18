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
#define DSW_MAX_PORT_OUT_BUFFER (32)

#define DSW_MAX_QUEUES (16)

#define DSW_MAX_EVENTS (16384)

/* Code changes are required to allow more flows than 32k. */
#define DSW_MAX_FLOWS_BITS (15)
#define DSW_MAX_FLOWS (1<<(DSW_MAX_FLOWS_BITS))
#define DSW_MAX_FLOWS_MASK (DSW_MAX_FLOWS-1)

/* Eventdev RTE_SCHED_TYPE_PARALLEL doesn't have a concept of flows,
 * but the 'dsw' scheduler (more or less) randomly assign flow id to
 * events on parallel queues, to be able to reuse some of the
 * migration mechanism and scheduling logic from
 * RTE_SCHED_TYPE_ATOMIC. By moving one of the parallel "flows" from a
 * particular port, the likely-hood of events being scheduled to this
 * port is reduced, and thus a kind of statistical load balancing is
 * achieved.
 */
#define DSW_PARALLEL_FLOWS (1024)

/* Avoid making small 'loans' from the central in-flight event credit
 * pool, to improve efficiency.
 */
#define DSW_MIN_CREDIT_LOAN (64)
#define DSW_PORT_MAX_CREDITS (2*DSW_MIN_CREDIT_LOAN)
#define DSW_PORT_MIN_CREDITS (DSW_MIN_CREDIT_LOAN)

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

	int32_t inflight_credits;

	int32_t new_event_threshold;

	uint16_t pending_releases;

	uint16_t next_parallel_flow_id;

	uint16_t out_buffer_len[DSW_MAX_PORTS];
	struct rte_event out_buffer[DSW_MAX_PORTS][DSW_MAX_PORT_OUT_BUFFER];

	struct rte_event_ring *in_ring __rte_cache_aligned;
} __rte_cache_aligned;

struct dsw_queue {
	uint8_t schedule_type;
	uint8_t serving_ports[DSW_MAX_PORTS];
	uint16_t num_serving_ports;

	uint8_t flow_to_port_map[DSW_MAX_FLOWS] __rte_cache_aligned;
};

struct dsw_evdev {
	struct rte_eventdev_data *data;

	struct dsw_port ports[DSW_MAX_PORTS];
	uint16_t num_ports;
	struct dsw_queue queues[DSW_MAX_QUEUES];
	uint8_t num_queues;
	int32_t max_inflight;

	rte_atomic32_t credits_on_loan __rte_cache_aligned;
};

uint16_t dsw_event_enqueue(void *port, const struct rte_event *event);
uint16_t dsw_event_enqueue_burst(void *port,
				 const struct rte_event events[],
				 uint16_t events_len);
uint16_t dsw_event_enqueue_new_burst(void *port,
				     const struct rte_event events[],
				     uint16_t events_len);
uint16_t dsw_event_enqueue_forward_burst(void *port,
					 const struct rte_event events[],
					 uint16_t events_len);

uint16_t dsw_event_dequeue(void *port, struct rte_event *ev, uint64_t wait);
uint16_t dsw_event_dequeue_burst(void *port, struct rte_event *events,
				 uint16_t num, uint64_t wait);

static inline struct dsw_evdev *
dsw_pmd_priv(const struct rte_eventdev *eventdev)
{
	return eventdev->data->dev_private;
}

#define DSW_LOG_DP(level, fmt, args...)					\
	RTE_LOG_DP(level, EVENTDEV, "[%s] %s() line %u: " fmt,		\
		   DSW_PMD_NAME,					\
		   __func__, __LINE__, ## args)

#define DSW_LOG_DP_PORT(level, port_id, fmt, args...)		\
	DSW_LOG_DP(level, "<Port %d> " fmt, port_id, ## args)

#endif
