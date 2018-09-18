/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Ericsson AB
 */

#include "dsw_evdev.h"

#include <stdbool.h>

#include <rte_atomic.h>
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

	dest_port_id = dsw_schedule(dsw, event->queue_id, flow_hash);

	dsw_port_buffer_non_paused(dsw, source_port, dest_port_id, event);
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
		dsw_port_flush_out_buffers(dsw, source_port);
		return 0;
	}

	if (unlikely(events_len > source_port->enqueue_depth))
		events_len = source_port->enqueue_depth;

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

	for (i = 0; i < events_len; i++) {
		const struct rte_event *event = &events[i];

		if (likely(num_release == 0 ||
			   event->op != RTE_EVENT_OP_RELEASE))
			dsw_port_buffer_event(dsw, source_port, event);
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

static uint16_t
dsw_port_dequeue_burst(struct dsw_port *port, struct rte_event *events,
		       uint16_t num)
{
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

	if (unlikely(num > source_port->dequeue_depth))
		num = source_port->dequeue_depth;

	dequeued = dsw_port_dequeue_burst(source_port, events, num);

	source_port->pending_releases = dequeued;

	if (dequeued > 0) {
		DSW_LOG_DP_PORT(DEBUG, source_port->id, "Dequeued %d events.\n",
				dequeued);

		dsw_port_return_credits(dsw, source_port, dequeued);
	}
	/* XXX: Assuming the port can't produce any more work,
	 *	consider flushing the output buffer, on dequeued ==
	 *	0.
	 */

	return dequeued;
}
