/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include "dsw_evdev.h"

#ifdef DSW_SORT_DEQUEUED
#include "dsw_sort.h"
#endif

#include <stdbool.h>
#include <string.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_memcpy.h>
#include <rte_random.h>

static bool
dsw_port_acquire_credits(struct dsw_evdev *dsw, struct dsw_port *port,
			 int32_t credits)
{
	int32_t inflight_credits = port->inflight_credits;
	int32_t missing_credits = credits - inflight_credits;
	int32_t total_on_loan;
	int32_t available;
	int32_t acquired_credits;
	int32_t new_total_on_loan;

	if (likely(missing_credits <= 0)) {
		port->inflight_credits -= credits;
		return true;
	}

	total_on_loan = rte_atomic32_read(&dsw->credits_on_loan);
	available = dsw->max_inflight - total_on_loan;
	acquired_credits = RTE_MAX(missing_credits, DSW_PORT_MIN_CREDITS);

	if (available < acquired_credits)
		return false;

	/* This is a race, no locks are involved, and thus some other
	 * thread can allocate tokens in between the check and the
	 * allocation.
	 */
	new_total_on_loan = rte_atomic32_add_return(&dsw->credits_on_loan,
						    acquired_credits);

	if (unlikely(new_total_on_loan > dsw->max_inflight)) {
		/* Some other port took the last credits */
		rte_atomic32_sub(&dsw->credits_on_loan, acquired_credits);
		return false;
	}

	DSW_LOG_DP_PORT(DEBUG, port->id, "Acquired %d tokens from pool.\n",
			acquired_credits);

	port->inflight_credits += acquired_credits;
	port->inflight_credits -= credits;

	return true;
}

static void
dsw_port_return_credits(struct dsw_evdev *dsw, struct dsw_port *port,
			int32_t credits)
{
	port->inflight_credits += credits;

	if (unlikely(port->inflight_credits > DSW_PORT_MAX_CREDITS)) {
		int32_t leave_credits = DSW_PORT_MIN_CREDITS;
		int32_t return_credits =
			port->inflight_credits - leave_credits;

		port->inflight_credits = leave_credits;

		rte_atomic32_sub(&dsw->credits_on_loan, return_credits);

		DSW_LOG_DP_PORT(DEBUG, port->id,
				"Returned %d tokens to pool.\n",
				return_credits);
	}
}

static void
dsw_port_enqueue_stats(struct dsw_port *port, uint16_t num_new,
		       uint16_t num_forward, uint16_t num_release)
{
	port->new_enqueued += num_new;
	port->forward_enqueued += num_forward;
	port->release_enqueued += num_release;
}

static void
dsw_port_queue_enqueue_stats(struct dsw_port *source_port, uint8_t queue_id)
{
	source_port->queue_enqueued[queue_id]++;
}

static void
dsw_port_dequeue_stats(struct dsw_port *port, uint16_t num)
{
	port->dequeued += num;
}

static void
dsw_port_queue_dequeued_stats(struct dsw_port *source_port, uint8_t queue_id)
{
	source_port->queue_dequeued[queue_id]++;
}

static void
dsw_port_load_record(struct dsw_port *port, unsigned int dequeued)
{
	if (dequeued > 0 && port->busy_start == 0)
		/* work period begins */
		port->busy_start = rte_get_timer_cycles();
	else if (dequeued == 0 && port->busy_start > 0) {
		/* work period ends */
		uint64_t work_period =
			rte_get_timer_cycles() - port->busy_start;
		port->busy_cycles += work_period;
		port->busy_start = 0;
	}
}

static int16_t
dsw_port_load_close_period(struct dsw_port *port, uint64_t now)
{
	uint64_t passed = now - port->measurement_start;
	uint64_t busy_cycles = port->busy_cycles;

	if (port->busy_start > 0) {
		busy_cycles += (now - port->busy_start);
		port->busy_start = now;
	}

	int16_t load = (DSW_MAX_LOAD * busy_cycles) / passed;

	port->measurement_start = now;
	port->busy_cycles = 0;

	port->total_busy_cycles += busy_cycles;

	return load;
}

static void
dsw_port_load_update(struct dsw_port *port, uint64_t now)
{
	int16_t old_load;
	int16_t period_load;
	int16_t new_load;

	old_load = rte_atomic16_read(&port->load);

	period_load = dsw_port_load_close_period(port, now);

	new_load = (period_load + old_load*DSW_OLD_LOAD_WEIGHT) /
		(DSW_OLD_LOAD_WEIGHT+1);

	rte_atomic16_set(&port->load, new_load);
}

static void
dsw_port_consider_load_update(struct dsw_port *port, uint64_t now)
{
	if (now < port->next_load_update)
		return;

	port->next_load_update = now + port->load_update_interval;

	dsw_port_load_update(port, now);
}

static void
dsw_port_ctl_enqueue(struct dsw_port *port, struct dsw_ctl_msg *msg)
{
	void *raw_msg;

	memcpy(&raw_msg, msg, sizeof(*msg));

	/* there's always room on the ring */
	while (rte_ring_enqueue(port->ctl_in_ring, raw_msg) != 0)
		rte_pause();
}

static int
dsw_port_ctl_dequeue(struct dsw_port *port, struct dsw_ctl_msg *msg)
{
	void *raw_msg;
	int rc;

	rc = rte_ring_dequeue(port->ctl_in_ring, &raw_msg);

	if (rc == 0)
		memcpy(msg, &raw_msg, sizeof(*msg));

	return rc;
}

static void
dsw_port_ctl_broadcast(struct dsw_evdev *dsw, struct dsw_port *source_port,
		       uint8_t type, uint8_t queue_id, uint16_t flow_hash)
{
	uint16_t port_id;
	struct dsw_ctl_msg msg = {
		.type = type,
		.originating_port_id = source_port->id,
		.queue_id = queue_id,
		.flow_hash = flow_hash
	};

	for (port_id = 0; port_id < dsw->num_ports; port_id++)
		if (port_id != source_port->id)
			dsw_port_ctl_enqueue(&dsw->ports[port_id], &msg);
}

static bool
dsw_port_is_flow_paused(struct dsw_port *port, uint8_t queue_id,
			uint16_t flow_hash)
{
	uint16_t i;

	for (i = 0; i < port->paused_flows_len; i++) {
		struct dsw_queue_flow *qf = &port->paused_flows[i];
		if (qf->queue_id == queue_id &&
		    qf->flow_hash == flow_hash)
			return true;
	}
	return false;
}

static void
dsw_port_add_paused_flow(struct dsw_port *port, uint8_t queue_id,
			 uint16_t paused_flow_hash)
{
	port->paused_flows[port->paused_flows_len] = (struct dsw_queue_flow) {
		.queue_id = queue_id,
		.flow_hash = paused_flow_hash
	};
	port->paused_flows_len++;
}

static void
dsw_port_remove_paused_flow(struct dsw_port *port, uint8_t queue_id,
			    uint16_t paused_flow_hash)
{
	uint16_t i;

	for (i = 0; i < port->paused_flows_len; i++) {
		struct dsw_queue_flow *qf = &port->paused_flows[i];

		if (qf->queue_id == queue_id &&
		    qf->flow_hash == paused_flow_hash) {
			uint16_t last_idx = port->paused_flows_len-1;
			if (i != last_idx)
				port->paused_flows[i] =
					port->paused_flows[last_idx];
			port->paused_flows_len--;
			break;
		}
	}
}

static void
dsw_port_flush_out_buffers(struct dsw_evdev *dsw, struct dsw_port *source_port);

static void
dsw_port_handle_pause_flow(struct dsw_evdev *dsw, struct dsw_port *port,
			   uint8_t originating_port_id, uint8_t queue_id,
			   uint16_t paused_flow_hash)
{
	struct dsw_ctl_msg cfm = {
		.type = DSW_CTL_CFM,
		.originating_port_id = port->id,
		.queue_id = queue_id,
		.flow_hash = paused_flow_hash
	};

	DSW_LOG_DP_PORT(DEBUG, port->id, "Pausing queue_id %d flow_hash %d.\n",
			queue_id, paused_flow_hash);

	/* There might be already-scheduled events belonging to the
	 * paused flow in the output buffers.
	 */
	dsw_port_flush_out_buffers(dsw, port);

	dsw_port_add_paused_flow(port, queue_id, paused_flow_hash);

	/* Make sure any stores to the original port's in_ring is seen
	 * before the ctl message.
	 */
	rte_smp_wmb();

	dsw_port_ctl_enqueue(&dsw->ports[originating_port_id], &cfm);
}

static void
dsw_find_lowest_load_port(uint8_t *port_ids, uint16_t num_port_ids,
			  uint8_t exclude_port_id, int16_t *port_loads,
			  uint8_t *target_port_id, int16_t *target_load)
{
	int16_t candidate_port_id = -1;
	int16_t candidate_load = DSW_MAX_LOAD;
	uint16_t i;

	for (i = 0; i < num_port_ids; i++) {
		uint8_t port_id = port_ids[i];
		if (port_id != exclude_port_id) {
			int16_t load = port_loads[port_id];
			if (candidate_port_id == -1 ||
			    load < candidate_load) {
				candidate_port_id = port_id;
				candidate_load = load;
			}
		}
	}
	*target_port_id = candidate_port_id;
	*target_load = candidate_load;
}

struct dsw_queue_flow_burst {
	struct dsw_queue_flow queue_flow;
	uint16_t count;
};

static inline int
dsw_cmp_burst(const void *v_burst_a, const void *v_burst_b)
{
	const struct dsw_queue_flow_burst *burst_a = v_burst_a;
	const struct dsw_queue_flow_burst *burst_b = v_burst_b;

	int a_count = burst_a->count;
	int b_count = burst_b->count;

	return a_count - b_count;
}

#define DSW_QF_TO_INT(_qf)					\
	((int)((((_qf)->queue_id)<<16)|((_qf)->flow_hash)))

static inline int
dsw_cmp_qf(const void *v_qf_a, const void *v_qf_b)
{
	const struct dsw_queue_flow *qf_a = v_qf_a;
	const struct dsw_queue_flow *qf_b = v_qf_b;

	return DSW_QF_TO_INT(qf_a) - DSW_QF_TO_INT(qf_b);
}

static uint16_t
dsw_sort_qfs_to_bursts(struct dsw_queue_flow *qfs, uint16_t qfs_len,
		       struct dsw_queue_flow_burst *bursts)
{
	uint16_t i;
	struct dsw_queue_flow_burst *current_burst = NULL;
	uint16_t num_bursts = 0;

	/* We don't need the stable property, and the list is likely
	 * large enough for qsort() to outperform dsw_stable_sort(),
	 * so we use qsort() here.
	 */
	qsort(qfs, qfs_len, sizeof(qfs[0]), dsw_cmp_qf);

	/* arrange the (now-consecutive) events into bursts */
	for (i = 0; i < qfs_len; i++) {
		if (i == 0 ||
		    dsw_cmp_qf(&qfs[i], &current_burst->queue_flow) != 0) {
			current_burst = &bursts[num_bursts];
			current_burst->queue_flow = qfs[i];
			current_burst->count = 0;
			num_bursts++;
		}
		current_burst->count++;
	}

	qsort(bursts, num_bursts, sizeof(bursts[0]), dsw_cmp_burst);

	return num_bursts;
}

static bool
dsw_retrieve_port_loads(struct dsw_evdev *dsw, int16_t *port_loads,
			int16_t load_limit)
{
	bool below_limit = false;
	uint16_t i;

	for (i = 0; i < dsw->num_ports; i++) {
		int16_t load = rte_atomic16_read(&dsw->ports[i].load);
		if (load < load_limit)
			below_limit = true;
		port_loads[i] = load;
	}
	return below_limit;
}

static bool
dsw_select_migration_target(struct dsw_evdev *dsw,
			    struct dsw_port *source_port,
			    struct dsw_queue_flow_burst *bursts,
			    uint16_t num_bursts, int16_t *port_loads,
			    int16_t max_load, struct dsw_queue_flow *target_qf,
			    uint8_t *target_port_id)
{
	uint16_t source_load = port_loads[source_port->id];
	uint16_t i;

	for (i = 0; i < num_bursts; i++) {
		struct dsw_queue_flow *qf = &bursts[i].queue_flow;

		if (dsw_port_is_flow_paused(source_port, qf->queue_id,
					    qf->flow_hash))
			continue;

		struct dsw_queue *queue = &dsw->queues[qf->queue_id];
		int16_t target_load;

		dsw_find_lowest_load_port(queue->serving_ports,
					  queue->num_serving_ports,
					  source_port->id, port_loads,
					  target_port_id, &target_load);

		if (target_load < source_load &&
		    target_load < max_load) {
			*target_qf = *qf;
			return true;
		}
	}

	DSW_LOG_DP_PORT(DEBUG, source_port->id, "For the %d flows considered, "
			"no target port found with load less than %d.\n",
			num_bursts, DSW_LOAD_TO_PERCENT(max_load));

	return false;
}

static uint8_t
dsw_schedule(struct dsw_evdev *dsw, uint8_t queue_id, uint16_t flow_hash)
{
	struct dsw_queue *queue = &dsw->queues[queue_id];
	uint8_t port_id;

	if (queue->num_serving_ports > 1)
		port_id = queue->flow_to_port_map[flow_hash];
	else
		/* A single-link queue, or atomic/ordered/parallel but
		 * with just a single serving port.
		 */
		port_id = queue->serving_ports[0];

	DSW_LOG_DP(DEBUG, "Event with queue_id %d flow_hash %d is scheduled "
		   "to port %d.\n", queue_id, flow_hash, port_id);

	return port_id;
}

static void
dsw_port_transmit_buffered(struct dsw_evdev *dsw, struct dsw_port *source_port,
			   uint8_t dest_port_id)
{
	struct dsw_port *dest_port = &(dsw->ports[dest_port_id]);
	uint16_t *buffer_len = &source_port->out_buffer_len[dest_port_id];
	struct rte_event *buffer = source_port->out_buffer[dest_port_id];
	uint16_t enqueued = 0;

	if (*buffer_len == 0)
		return;

	/* The rings are dimensioned to fit all in-flight events (even
	 * on a single ring), so looping will work.
	 */
	do {
		enqueued +=
			rte_event_ring_enqueue_burst(dest_port->in_ring,
						     buffer+enqueued,
						     *buffer_len-enqueued,
						     NULL);
	} while (unlikely(enqueued != *buffer_len));

	(*buffer_len) = 0;
}

static uint16_t
dsw_port_get_parallel_flow_id(struct dsw_port *port)
{
	uint16_t flow_id = port->next_parallel_flow_id;

	port->next_parallel_flow_id =
		(port->next_parallel_flow_id + 1) % DSW_PARALLEL_FLOWS;

	return flow_id;
}

static void
dsw_port_buffer_paused(struct dsw_port *port,
		       const struct rte_event *paused_event)
{
	port->paused_events[port->paused_events_len] = *paused_event;
	port->paused_events_len++;
}

static void
dsw_port_buffer_non_paused(struct dsw_evdev *dsw, struct dsw_port *source_port,
			   uint8_t dest_port_id, const struct rte_event *event)
{
	struct rte_event *buffer = source_port->out_buffer[dest_port_id];
	uint16_t *buffer_len = &source_port->out_buffer_len[dest_port_id];

	if (*buffer_len == DSW_MAX_PORT_OUT_BUFFER)
		dsw_port_transmit_buffered(dsw, source_port, dest_port_id);

	buffer[*buffer_len] = *event;

	(*buffer_len)++;
}

#define DSW_FLOW_ID_BITS (24)
static uint16_t
dsw_flow_id_hash(uint32_t flow_id)
{
	uint16_t hash = 0;
	uint16_t offset = 0;

	do {
		hash ^= ((flow_id >> offset) & DSW_MAX_FLOWS_MASK);
		offset += DSW_MAX_FLOWS_BITS;
	} while (offset < DSW_FLOW_ID_BITS);

	return hash;
}

static void
dsw_port_buffer_parallel(struct dsw_evdev *dsw, struct dsw_port *source_port,
			 struct rte_event event)
{
	uint8_t dest_port_id;

	event.flow_id = dsw_port_get_parallel_flow_id(source_port);

	dest_port_id = dsw_schedule(dsw, event.queue_id,
				    dsw_flow_id_hash(event.flow_id));

	dsw_port_buffer_non_paused(dsw, source_port, dest_port_id, &event);
}

static void
dsw_port_buffer_event(struct dsw_evdev *dsw, struct dsw_port *source_port,
		      const struct rte_event *event)
{
	uint16_t flow_hash;
	uint8_t dest_port_id;

	if (unlikely(dsw->queues[event->queue_id].schedule_type ==
		     RTE_SCHED_TYPE_PARALLEL)) {
		dsw_port_buffer_parallel(dsw, source_port, *event);
		return;
	}

	flow_hash = dsw_flow_id_hash(event->flow_id);

	if (unlikely(dsw_port_is_flow_paused(source_port, event->queue_id,
					     flow_hash))) {
		dsw_port_buffer_paused(source_port, event);
		return;
	}

	dest_port_id = dsw_schedule(dsw, event->queue_id, flow_hash);

	dsw_port_buffer_non_paused(dsw, source_port, dest_port_id, event);
}

static void
dsw_port_flush_paused_events(struct dsw_evdev *dsw,
			     struct dsw_port *source_port,
			     uint8_t queue_id, uint16_t paused_flow_hash)
{
	uint16_t paused_events_len = source_port->paused_events_len;
	struct rte_event paused_events[paused_events_len];
	uint8_t dest_port_id;
	uint16_t i;

	if (paused_events_len == 0)
		return;

	if (dsw_port_is_flow_paused(source_port, queue_id, paused_flow_hash))
		return;

	rte_memcpy(paused_events, source_port->paused_events,
		   paused_events_len * sizeof(struct rte_event));

	source_port->paused_events_len = 0;

	dest_port_id = dsw_schedule(dsw, queue_id, paused_flow_hash);

	for (i = 0; i < paused_events_len; i++) {
		struct rte_event *event = &paused_events[i];
		uint16_t flow_hash;

		flow_hash = dsw_flow_id_hash(event->flow_id);

		if (event->queue_id == queue_id &&
		    flow_hash == paused_flow_hash)
			dsw_port_buffer_non_paused(dsw, source_port,
						   dest_port_id, event);
		else
			dsw_port_buffer_paused(source_port, event);
	}
}

static void
dsw_port_migration_stats(struct dsw_port *port)
{
	uint64_t migration_latency;

	migration_latency = (rte_get_timer_cycles() - port->migration_start);
	port->migration_latency += migration_latency;
	port->migrations++;
}

static void
dsw_port_end_migration(struct dsw_evdev *dsw, struct dsw_port *port)
{
	uint8_t queue_id = port->migration_target_qf.queue_id;
	uint16_t flow_hash = port->migration_target_qf.flow_hash;

	port->migration_state = DSW_MIGRATION_STATE_IDLE;
	port->seen_events_len = 0;

	dsw_port_migration_stats(port);

	if (dsw->queues[queue_id].schedule_type != RTE_SCHED_TYPE_PARALLEL) {
		dsw_port_remove_paused_flow(port, queue_id, flow_hash);
		dsw_port_flush_paused_events(dsw, port, queue_id, flow_hash);
	}

	DSW_LOG_DP_PORT(DEBUG, port->id, "Migration completed for queue_id "
			"%d flow_hash %d.\n", queue_id, flow_hash);
}

static void
dsw_port_consider_migration(struct dsw_evdev *dsw,
			    struct dsw_port *source_port,
			    uint64_t now)
{
	bool any_port_below_limit;
	struct dsw_queue_flow *seen_events = source_port->seen_events;
	uint16_t seen_events_len = source_port->seen_events_len;
	struct dsw_queue_flow_burst bursts[DSW_MAX_EVENTS_RECORDED];
	uint16_t num_bursts;
	int16_t source_port_load;
	int16_t port_loads[dsw->num_ports];

	if (now < source_port->next_migration)
		return;

	if (dsw->num_ports == 1)
		return;

	DSW_LOG_DP_PORT(DEBUG, source_port->id, "Considering migration.\n");

	/* Randomize interval to avoid having all threads considering
	 * migration at the same in point in time, which might lead to
	 * all choosing the same target port.
	 */
	source_port->next_migration = now +
		source_port->migration_interval / 2 +
		rte_rand() % source_port->migration_interval;

	if (source_port->migration_state != DSW_MIGRATION_STATE_IDLE) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id,
				"Migration already in progress.\n");
		return;
	}

	/* For simplicity, avoid migration in the unlikely case there
	 * is still events to consume in the in_buffer (from the last
	 * migration).
	 */
	if (source_port->in_buffer_len > 0) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id, "There are still "
				"events in the input buffer.\n");
		return;
	}

	source_port_load = rte_atomic16_read(&source_port->load);
	if (source_port_load < DSW_MIN_SOURCE_LOAD_FOR_MIGRATION) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id,
				"Load %d is below threshold level %d.\n",
				DSW_LOAD_TO_PERCENT(source_port_load),
		       DSW_LOAD_TO_PERCENT(DSW_MIN_SOURCE_LOAD_FOR_MIGRATION));
		return;
	}

	/* Avoid starting any expensive operations (sorting etc), in
	 * case of a scenario with all ports above the load limit.
	 */
	any_port_below_limit =
		dsw_retrieve_port_loads(dsw, port_loads,
					DSW_MAX_TARGET_LOAD_FOR_MIGRATION);
	if (!any_port_below_limit) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id,
				"Candidate target ports are all too highly "
				"loaded.\n");
		return;
	}

	/* Sort flows into 'bursts' to allow attempting to migrating
	 * small (but still active) flows first - this it to avoid
	 * having large flows moving around the worker cores too much
	 * (to avoid cache misses, among other things). Of course, the
	 * number of recorded events (queue+flow ids) are limited, and
	 * provides only a snapshot, so only so many conclusions can
	 * be drawn from this data.
	 */
	num_bursts = dsw_sort_qfs_to_bursts(seen_events, seen_events_len,
					    bursts);
	/* For non-big-little systems, there's no point in moving the
	 * only (known) flow.
	 */
	if (num_bursts < 2) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id, "Only a single flow "
				"queue_id %d flow_hash %d has been seen.\n",
				bursts[0].queue_flow.queue_id,
				bursts[0].queue_flow.flow_hash);
		return;
	}

	/* The strategy is to first try to find a flow to move to a
	 * port with low load (below the migration-attempt
	 * threshold). If that fails, we try to find a port which is
	 * below the max threshold, and also less loaded than this
	 * port is.
	 */
	if (!dsw_select_migration_target(dsw, source_port, bursts, num_bursts,
					 port_loads,
					 DSW_MIN_SOURCE_LOAD_FOR_MIGRATION,
					 &source_port->migration_target_qf,
					 &source_port->migration_target_port_id)
	    &&
	    !dsw_select_migration_target(dsw, source_port, bursts, num_bursts,
					 port_loads,
					 DSW_MAX_TARGET_LOAD_FOR_MIGRATION,
					 &source_port->migration_target_qf,
				       &source_port->migration_target_port_id))
		return;

	DSW_LOG_DP_PORT(DEBUG, source_port->id, "Migrating queue_id %d "
			"flow_hash %d from port %d to port %d.\n",
			source_port->migration_target_qf.queue_id,
			source_port->migration_target_qf.flow_hash,
			source_port->id, source_port->migration_target_port_id);

	/* We have a winner. */

	source_port->migration_state = DSW_MIGRATION_STATE_PAUSING;
	source_port->migration_start = rte_get_timer_cycles();

	/* No need to go through the whole pause procedure for
	 * parallel queues, since atomic/ordered semantics need not to
	 * be maintained.
	 */

	if (dsw->queues[source_port->migration_target_qf.queue_id].schedule_type
	    == RTE_SCHED_TYPE_PARALLEL) {
		uint8_t queue_id = source_port->migration_target_qf.queue_id;
		uint16_t flow_hash = source_port->migration_target_qf.flow_hash;
		uint8_t dest_port_id = source_port->migration_target_port_id;

		/* Single byte-sized stores are always atomic. */
		dsw->queues[queue_id].flow_to_port_map[flow_hash] =
			dest_port_id;
		rte_smp_wmb();

		dsw_port_end_migration(dsw, source_port);

		return;
	}

	/* There might be 'loopback' events already scheduled in the
	 * output buffers.
	 */
	dsw_port_flush_out_buffers(dsw, source_port);

	dsw_port_add_paused_flow(source_port,
				 source_port->migration_target_qf.queue_id,
				 source_port->migration_target_qf.flow_hash);

	dsw_port_ctl_broadcast(dsw, source_port, DSW_CTL_PAUS_REQ,
			       source_port->migration_target_qf.queue_id,
			       source_port->migration_target_qf.flow_hash);
	source_port->cfm_cnt = 0;
}

static void
dsw_port_flush_paused_events(struct dsw_evdev *dsw,
			     struct dsw_port *source_port,
			     uint8_t queue_id, uint16_t paused_flow_hash);

static void
dsw_port_handle_unpause_flow(struct dsw_evdev *dsw, struct dsw_port *port,
			     uint8_t originating_port_id, uint8_t queue_id,
			     uint16_t paused_flow_hash)
{
	struct dsw_ctl_msg cfm = {
		.type = DSW_CTL_CFM,
		.originating_port_id = port->id,
		.queue_id = queue_id,
		.flow_hash = paused_flow_hash
	};

	DSW_LOG_DP_PORT(DEBUG, port->id, "Un-pausing queue_id %d flow_hash %d.\n",
			queue_id, paused_flow_hash);

	dsw_port_remove_paused_flow(port, queue_id, paused_flow_hash);

	rte_smp_rmb();

	dsw_port_ctl_enqueue(&dsw->ports[originating_port_id], &cfm);

	dsw_port_flush_paused_events(dsw, port, queue_id, paused_flow_hash);
}

#define FORWARD_BURST_SIZE (32)

static void
dsw_port_forward_migrated_flow(struct dsw_port *source_port,
			       struct rte_event_ring *dest_ring,
			       uint8_t queue_id,
			       uint16_t flow_hash)
{
	uint16_t events_left;

	/* Control ring message should been seen before the ring count
	 * is read on the port's in_ring.
	 */
	rte_smp_rmb();

	events_left = rte_event_ring_count(source_port->in_ring);

	while (events_left > 0) {
		uint16_t in_burst_size =
			RTE_MIN(FORWARD_BURST_SIZE, events_left);
		struct rte_event in_burst[in_burst_size];
		uint16_t in_len;
		uint16_t i;

		in_len = rte_event_ring_dequeue_burst(source_port->in_ring,
						      in_burst,
						      in_burst_size, NULL);
		/* No need to care about bursting forwarded events (to
		 * the destination port's in_ring), since migration
		 * doesn't happen very often, and also the majority of
		 * the dequeued events will likely *not* be forwarded.
		 */
		for (i = 0; i < in_len; i++) {
			struct rte_event *e = &in_burst[i];
			if (e->queue_id == queue_id &&
			    dsw_flow_id_hash(e->flow_id) == flow_hash) {
				while (rte_event_ring_enqueue_burst(dest_ring,
								    e, 1,
								    NULL) != 1)
					rte_pause();
			} else {
				uint16_t last_idx = source_port->in_buffer_len;
				source_port->in_buffer[last_idx] = *e;
				source_port->in_buffer_len++;
			}
		}

		events_left -= in_len;
	}
}

static void
dsw_port_move_migrating_flow(struct dsw_evdev *dsw,
			     struct dsw_port *source_port)
{
	uint8_t queue_id = source_port->migration_target_qf.queue_id;
	uint16_t flow_hash = source_port->migration_target_qf.flow_hash;
	uint8_t dest_port_id = source_port->migration_target_port_id;
	struct dsw_port *dest_port = &dsw->ports[dest_port_id];

	dsw_port_flush_out_buffers(dsw, source_port);

	rte_smp_wmb();

	dsw->queues[queue_id].flow_to_port_map[flow_hash] =
		dest_port_id;

	dsw_port_forward_migrated_flow(source_port, dest_port->in_ring,
				       queue_id, flow_hash);

	/* Flow table update and migration destination port's enqueues
	 * must be seen before the control message.
	 */
	rte_smp_wmb();

	dsw_port_ctl_broadcast(dsw, source_port, DSW_CTL_UNPAUS_REQ, queue_id,
			       flow_hash);
	source_port->cfm_cnt = 0;
	source_port->migration_state = DSW_MIGRATION_STATE_UNPAUSING;
}

static void
dsw_port_handle_confirm(struct dsw_evdev *dsw, struct dsw_port *port)
{
	port->cfm_cnt++;

	if (port->cfm_cnt == (dsw->num_ports-1)) {
		switch (port->migration_state) {
		case DSW_MIGRATION_STATE_PAUSING:
			DSW_LOG_DP_PORT(DEBUG, port->id, "Going into forwarding "
					"migration state.\n");
			port->migration_state = DSW_MIGRATION_STATE_FORWARDING;
			break;
		case DSW_MIGRATION_STATE_UNPAUSING:
			dsw_port_end_migration(dsw, port);
			break;
		default:
			RTE_ASSERT(0);
			break;
		}
	}
}

static void
dsw_port_ctl_process(struct dsw_evdev *dsw, struct dsw_port *port)
{
	struct dsw_ctl_msg msg;

	/* So any table loads happens before the ring dequeue, in the
	 * case of a 'paus' message.
	 */
	rte_smp_rmb();

	if (dsw_port_ctl_dequeue(port, &msg) == 0) {
		switch (msg.type) {
		case DSW_CTL_PAUS_REQ:
			dsw_port_handle_pause_flow(dsw, port,
						   msg.originating_port_id,
						   msg.queue_id, msg.flow_hash);
			break;
		case DSW_CTL_UNPAUS_REQ:
			dsw_port_handle_unpause_flow(dsw, port,
						     msg.originating_port_id,
						     msg.queue_id,
						     msg.flow_hash);
			break;
		case DSW_CTL_CFM:
			dsw_port_handle_confirm(dsw, port);
			break;
		}
	}
}

static void
dsw_port_note_op(struct dsw_port *port, uint16_t num_events)
{
	/* To pull the control ring reasonbly often on busy ports,
	 * each dequeued/enqueued event is considered an 'op' too.
	 */
	port->ops_since_bg_task += (num_events+1);
}

static void
dsw_port_bg_process(struct dsw_evdev *dsw, struct dsw_port *port)
{
	if (unlikely(port->migration_state == DSW_MIGRATION_STATE_FORWARDING &&
		     port->pending_releases == 0))
		dsw_port_move_migrating_flow(dsw, port);

	/* Polling the control ring is relatively inexpensive, and
	 * polling it often helps bringing down migration latency, so
	 * do this for every iteration.
	 */
	dsw_port_ctl_process(dsw, port);

	/* To avoid considering migration and flushing output buffers
	 * on every dequeue/enqueue call, the scheduler only performs
	 * such 'background' tasks every nth
	 * (i.e. DSW_MAX_PORT_OPS_PER_BG_TASK) operation.
	 */
	if (unlikely(port->ops_since_bg_task >= DSW_MAX_PORT_OPS_PER_BG_TASK)) {
		uint64_t now;

		now = rte_get_timer_cycles();

		port->last_bg = now;

		/* Logic to avoid having events linger in the output
		 * buffer too long.
		 */
		dsw_port_flush_out_buffers(dsw, port);

		dsw_port_consider_load_update(port, now);

		dsw_port_consider_migration(dsw, port, now);

		port->ops_since_bg_task = 0;
	}
}

static void
dsw_port_flush_out_buffers(struct dsw_evdev *dsw, struct dsw_port *source_port)
{
	uint16_t dest_port_id;

	for (dest_port_id = 0; dest_port_id < dsw->num_ports; dest_port_id++)
		dsw_port_transmit_buffered(dsw, source_port, dest_port_id);
}

uint16_t
dsw_event_enqueue(void *port, const struct rte_event *ev)
{
	return dsw_event_enqueue_burst(port, ev, unlikely(ev == NULL) ? 0 : 1);
}

static __rte_always_inline uint16_t
dsw_event_enqueue_burst_generic(void *port, const struct rte_event events[],
				uint16_t events_len, bool op_types_known,
				uint16_t num_new, uint16_t num_release,
				uint16_t num_non_release)
{
	struct dsw_port *source_port = port;
	struct dsw_evdev *dsw = source_port->dsw;
	bool enough_credits;
	uint16_t i;

	DSW_LOG_DP_PORT(DEBUG, source_port->id, "Attempting to enqueue %d "
			"events to port %d.\n", events_len, source_port->id);

	dsw_port_bg_process(dsw, source_port);

	/* XXX: For performance (=ring efficiency) reasons, the
	 * scheduler relies on internal non-ring buffers instead of
	 * immediately sending the event to the destination ring. For
	 * a producer that doesn't intend to produce or consume any
	 * more events, the scheduler provides a way to flush the
	 * buffer, by means of doing an enqueue of zero events. In
	 * addition, a port cannot be left "unattended" (e.g. unused)
	 * for long periods of time, since that would stall
	 * migration. Eventdev API extensions to provide a cleaner way
	 * to archieve both of these functions should be
	 * considered.
	 */
	if (unlikely(events_len == 0)) {
		dsw_port_note_op(source_port, DSW_MAX_PORT_OPS_PER_BG_TASK);
		return 0;
	}

	if (unlikely(events_len > source_port->enqueue_depth))
		events_len = source_port->enqueue_depth;

	dsw_port_note_op(source_port, events_len);

	if (!op_types_known)
		for (i = 0; i < events_len; i++) {
			switch (events[i].op) {
			case RTE_EVENT_OP_RELEASE:
				num_release++;
				break;
			case RTE_EVENT_OP_NEW:
				num_new++;
				/* Falls through. */
			default:
				num_non_release++;
				break;
			}
		}

	/* Technically, we could allow the non-new events up to the
	 * first new event in the array into the system, but for
	 * simplicity reasons, we deny the whole burst if the port is
	 * above the water mark.
	 */
	if (unlikely(num_new > 0 && rte_atomic32_read(&dsw->credits_on_loan) >
		     source_port->new_event_threshold))
		return 0;

	enough_credits = dsw_port_acquire_credits(dsw, source_port,
						  num_non_release);
	if (unlikely(!enough_credits))
		return 0;

	source_port->pending_releases -= num_release;

	dsw_port_enqueue_stats(source_port, num_new,
			       num_non_release-num_new, num_release);

	for (i = 0; i < events_len; i++) {
		const struct rte_event *event = &events[i];

		if (likely(num_release == 0 ||
			   event->op != RTE_EVENT_OP_RELEASE))
			dsw_port_buffer_event(dsw, source_port, event);
		dsw_port_queue_enqueue_stats(source_port, event->queue_id);
	}

	DSW_LOG_DP_PORT(DEBUG, source_port->id, "%d non-release events "
			"accepted.\n", num_non_release);

	return num_non_release;
}

uint16_t
dsw_event_enqueue_burst(void *port, const struct rte_event events[],
			uint16_t events_len)
{
	return dsw_event_enqueue_burst_generic(port, events, events_len, false,
					       0, 0, 0);
}

uint16_t
dsw_event_enqueue_new_burst(void *port, const struct rte_event events[],
			    uint16_t events_len)
{
	return dsw_event_enqueue_burst_generic(port, events, events_len, true,
					       events_len, 0, events_len);
}

uint16_t
dsw_event_enqueue_forward_burst(void *port, const struct rte_event events[],
				uint16_t events_len)
{
	return dsw_event_enqueue_burst_generic(port, events, events_len, true,
					       0, 0, events_len);
}

uint16_t
dsw_event_dequeue(void *port, struct rte_event *events, uint64_t wait)
{
	return dsw_event_dequeue_burst(port, events, 1, wait);
}

static void
dsw_port_record_seen_events(struct dsw_port *port, struct rte_event *events,
			    uint16_t num)
{
	uint16_t i;

	dsw_port_dequeue_stats(port, num);

	for (i = 0; i < num; i++) {
		uint16_t l_idx = port->seen_events_idx;
		struct dsw_queue_flow *qf = &port->seen_events[l_idx];
		struct rte_event *event = &events[i];
		qf->queue_id = event->queue_id;
		qf->flow_hash = dsw_flow_id_hash(event->flow_id);

		port->seen_events_idx = (l_idx+1) % DSW_MAX_EVENTS_RECORDED;

		dsw_port_queue_dequeued_stats(port, event->queue_id);
	}

	if (unlikely(port->seen_events_len != DSW_MAX_EVENTS_RECORDED))
		port->seen_events_len =
			RTE_MIN(port->seen_events_len + num,
				DSW_MAX_EVENTS_RECORDED);
}

#ifdef DSW_SORT_DEQUEUED

#define DSW_EVENT_TO_INT(_event)				\
	((int)((((_event)->queue_id)<<16)|((_event)->flow_id)))

static inline int
dsw_cmp_event(const void *v_event_a, const void *v_event_b)
{
	const struct rte_event *event_a = v_event_a;
	const struct rte_event *event_b = v_event_b;

	return DSW_EVENT_TO_INT(event_a) - DSW_EVENT_TO_INT(event_b);
}
#endif

static uint16_t
dsw_port_dequeue_burst(struct dsw_port *port, struct rte_event *events,
		       uint16_t num)
{
	struct dsw_port *source_port = port;
	struct dsw_evdev *dsw = source_port->dsw;

	dsw_port_ctl_process(dsw, source_port);

	if (unlikely(port->in_buffer_len > 0)) {
		uint16_t dequeued = RTE_MIN(num, port->in_buffer_len);

		rte_memcpy(events, &port->in_buffer[port->in_buffer_start],
			   dequeued * sizeof(struct rte_event));

		port->in_buffer_start += dequeued;
		port->in_buffer_len -= dequeued;

		if (port->in_buffer_len == 0)
			port->in_buffer_start = 0;

		return dequeued;
	}

	return rte_event_ring_dequeue_burst(port->in_ring, events, num, NULL);
}

uint16_t
dsw_event_dequeue_burst(void *port, struct rte_event *events, uint16_t num,
			uint64_t wait __rte_unused)
{
	struct dsw_port *source_port = port;
	struct dsw_evdev *dsw = source_port->dsw;
	uint16_t dequeued;

	source_port->pending_releases = 0;

	dsw_port_bg_process(dsw, source_port);

	if (unlikely(num > source_port->dequeue_depth))
		num = source_port->dequeue_depth;

	dequeued = dsw_port_dequeue_burst(source_port, events, num);

	source_port->pending_releases = dequeued;

	dsw_port_load_record(source_port, dequeued);

	dsw_port_note_op(source_port, dequeued);

	if (dequeued > 0) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id, "Dequeued %d events.\n",
				dequeued);

		dsw_port_return_credits(dsw, source_port, dequeued);

		/* One potential optimization one might think of is to
		 * add a migration state (prior to 'pausing'), and
		 * only record seen events when the port is in this
		 * state (and transit to 'pausing' when enough events
		 * have been gathered). However, that schema doesn't
		 * seem to improve performance.
		 */
		dsw_port_record_seen_events(port, events, dequeued);
	}
	/* XXX: Assuming the port can't produce any more work,
	 *	consider flushing the output buffer, on dequeued ==
	 *	0.
	 */

#ifdef DSW_SORT_DEQUEUED
	dsw_stable_sort(events, dequeued, sizeof(events[0]), dsw_cmp_event);
#endif

	return dequeued;
}
