/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#ifndef _DSW_EVDEV_H_
#define _DSW_EVDEV_H_

#include <eventdev_pmd.h>

#include <rte_event_ring.h>
#include <rte_eventdev.h>

#define DSW_PMD_NAME RTE_STR(event_dsw)

#define DSW_MAX_PORTS (64)
#define DSW_MAX_PORT_DEQUEUE_DEPTH (128)
#define DSW_MAX_PORT_ENQUEUE_DEPTH (128)
#define DSW_MAX_PORT_OUT_BUFFER (32)

#define DSW_MAX_QUEUES (16)

#define DSW_MAX_EVENTS (16384)

/* Multiple 24-bit flow ids will map to the same DSW-level flow. The
 * number of DSW flows should be high enough make it unlikely that
 * flow ids of several large flows hash to the same DSW-level flow.
 * Such collisions will limit parallelism and thus the number of cores
 * that may be utilized. However, configuring a large number of DSW
 * flows might potentially, depending on traffic and actual
 * application flow id value range, result in each such DSW-level flow
 * being very small. The effect of migrating such flows will be small,
 * in terms amount of processing load redistributed. This will in turn
 * reduce the load balancing speed, since flow migration rate has an
 * upper limit. Code changes are required to allow > 32k DSW-level
 * flows.
 */
#define DSW_MAX_FLOWS_BITS (13)
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

/* 'Background tasks' are polling the control rings for *
 *  migration-related messages, or flush the output buffer (so
 *  buffered events doesn't linger too long). Shouldn't be too low,
 *  since the system won't benefit from the 'batching' effects from
 *  the output buffer, and shouldn't be too high, since it will make
 *  buffered events linger too long in case the port goes idle.
 */
#define DSW_MAX_PORT_OPS_PER_BG_TASK (128)

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

#define DSW_MAX_LOAD (INT16_MAX)
#define DSW_LOAD_FROM_PERCENT(x) ((int16_t)(((x)*DSW_MAX_LOAD)/100))
#define DSW_LOAD_TO_PERCENT(x) ((100*x)/DSW_MAX_LOAD)

/* The thought behind keeping the load update interval shorter than
 * the migration interval is that the load from newly migrated flows
 * should 'show up' on the load measurement before new migrations are
 * considered. This is to avoid having too many flows, from too many
 * source ports, to be migrated too quickly to a lightly loaded port -
 * in particular since this might cause the system to oscillate.
 */
#define DSW_LOAD_UPDATE_INTERVAL (DSW_MIGRATION_INTERVAL/4)
#define DSW_OLD_LOAD_WEIGHT (1)

/* The minimum time (in us) between two flow migrations. What puts an
 * upper limit on the actual migration rate is primarily the pace in
 * which the ports send and receive control messages, which in turn is
 * largely a function of how much cycles are spent the processing of
 * an event burst.
 */
#define DSW_MIGRATION_INTERVAL (1000)
#define DSW_MIN_SOURCE_LOAD_FOR_MIGRATION (DSW_LOAD_FROM_PERCENT(70))
#define DSW_MAX_TARGET_LOAD_FOR_MIGRATION (DSW_LOAD_FROM_PERCENT(95))
#define DSW_REBALANCE_THRESHOLD (DSW_LOAD_FROM_PERCENT(3))

#define DSW_MAX_EVENTS_RECORDED (128)

#define DSW_MAX_FLOWS_PER_MIGRATION (8)

/* Only one outstanding migration per port is allowed */
#define DSW_MAX_PAUSED_FLOWS (DSW_MAX_PORTS*DSW_MAX_FLOWS_PER_MIGRATION)

/* Enough room for pause request/confirm and unpaus request/confirm for
 * all possible senders.
 */
#define DSW_CTL_IN_RING_SIZE ((DSW_MAX_PORTS-1)*4)

/* With DSW_SORT_DEQUEUED enabled, the scheduler will, at the point of
 * dequeue(), arrange events so that events with the same flow id on
 * the same queue forms a back-to-back "burst", and also so that such
 * bursts of different flow ids, but on the same queue, also come
 * consecutively. All this in an attempt to improve data and
 * instruction cache usage for the application, at the cost of a
 * scheduler overhead increase.
 */

/* #define DSW_SORT_DEQUEUED */

struct dsw_queue_flow {
	uint8_t queue_id;
	uint16_t flow_hash;
};

enum dsw_migration_state {
	DSW_MIGRATION_STATE_IDLE,
	DSW_MIGRATION_STATE_PAUSING,
	DSW_MIGRATION_STATE_UNPAUSING
};

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

	uint16_t ops_since_bg_task;

	/* most recent 'background' processing */
	uint64_t last_bg;

	/* For port load measurement. */
	uint64_t next_load_update;
	uint64_t load_update_interval;
	uint64_t measurement_start;
	uint64_t busy_start;
	uint64_t busy_cycles;
	uint64_t total_busy_cycles;

	/* For the ctl interface and flow migration mechanism. */
	uint64_t next_emigration;
	uint64_t migration_interval;
	enum dsw_migration_state migration_state;

	uint64_t emigration_start;
	uint64_t emigrations;
	uint64_t emigration_latency;

	uint8_t emigration_target_port_ids[DSW_MAX_FLOWS_PER_MIGRATION];
	struct dsw_queue_flow
		emigration_target_qfs[DSW_MAX_FLOWS_PER_MIGRATION];
	uint8_t emigration_targets_len;
	uint8_t cfm_cnt;

	uint64_t immigrations;

	uint16_t paused_flows_len;
	struct dsw_queue_flow paused_flows[DSW_MAX_PAUSED_FLOWS];

	/* In a very contrived worst case all inflight events can be
	 * laying around paused here.
	 */
	uint16_t paused_events_len;
	struct rte_event paused_events[DSW_MAX_EVENTS];

	uint16_t emigrating_events_len;
	/* Buffer for not-yet-processed events pertaining to a flow
	 * emigrating from this port. These events will be forwarded
	 * to the target port.
	 */
	struct rte_event emigrating_events[DSW_MAX_EVENTS];

	uint16_t seen_events_len;
	uint16_t seen_events_idx;
	struct dsw_queue_flow seen_events[DSW_MAX_EVENTS_RECORDED];

	uint64_t enqueue_calls;
	uint64_t new_enqueued;
	uint64_t forward_enqueued;
	uint64_t release_enqueued;
	uint64_t queue_enqueued[DSW_MAX_QUEUES];

	uint64_t dequeue_calls;
	uint64_t dequeued;
	uint64_t queue_dequeued[DSW_MAX_QUEUES];

	uint16_t out_buffer_len[DSW_MAX_PORTS];
	struct rte_event out_buffer[DSW_MAX_PORTS][DSW_MAX_PORT_OUT_BUFFER];

	uint16_t in_buffer_len;
	uint16_t in_buffer_start;
	/* This buffer may contain events that were read up from the
	 * in_ring during the flow migration process.
	 */
	struct rte_event in_buffer[DSW_MAX_EVENTS];

	struct rte_event_ring *in_ring __rte_cache_aligned;

	struct rte_ring *ctl_in_ring __rte_cache_aligned;

	/* Estimate of current port load. */
	int16_t load __rte_cache_aligned;
	/* Estimate of flows currently migrating to this port. */
	int32_t immigration_load __rte_cache_aligned;
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

	int32_t credits_on_loan __rte_cache_aligned;
};

#define DSW_CTL_PAUS_REQ (0)
#define DSW_CTL_UNPAUS_REQ (1)
#define DSW_CTL_CFM (2)

struct dsw_ctl_msg {
	uint8_t type;
	uint8_t originating_port_id;
	uint8_t qfs_len;
	struct dsw_queue_flow qfs[DSW_MAX_FLOWS_PER_MIGRATION];
} __rte_aligned(4);

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
void dsw_event_maintain(void *port, int op);

int dsw_xstats_get_names(const struct rte_eventdev *dev,
			 enum rte_event_dev_xstats_mode mode,
			 uint8_t queue_port_id,
			 struct rte_event_dev_xstats_name *xstats_names,
			 uint64_t *ids, unsigned int size);
int dsw_xstats_get(const struct rte_eventdev *dev,
		   enum rte_event_dev_xstats_mode mode, uint8_t queue_port_id,
		   const uint64_t ids[], uint64_t values[], unsigned int n);
uint64_t dsw_xstats_get_by_name(const struct rte_eventdev *dev,
				const char *name, uint64_t *id);

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
