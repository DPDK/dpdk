/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_event_ring.h>

#include "sw_evdev.h"

#define PORT_ENQUEUE_MAX_BURST_SIZE 64

static inline void
sw_event_release(struct sw_port *p, uint8_t index)
{
	/*
	 * Drops the next outstanding event in our history. Used on dequeue
	 * to clear any history before dequeuing more events.
	 */
	RTE_SET_USED(index);

	/* create drop message */
	struct rte_event ev;
	ev.op = sw_qe_flag_map[RTE_EVENT_OP_RELEASE];

	uint16_t free_count;
	rte_event_ring_enqueue_burst(p->rx_worker_ring, &ev, 1, &free_count);

	/* each release returns one credit */
	p->outstanding_releases--;
	p->inflight_credits++;
}

/*
 * special-case of rte_event_ring enqueue, with overriding the ops member on
 * the events that get written to the ring.
 */
static inline unsigned int
enqueue_burst_with_ops(struct rte_event_ring *r, const struct rte_event *events,
		unsigned int n, uint8_t *ops)
{
	struct rte_event tmp_evs[PORT_ENQUEUE_MAX_BURST_SIZE];
	unsigned int i;

	memcpy(tmp_evs, events, n * sizeof(events[0]));
	for (i = 0; i < n; i++)
		tmp_evs[i].op = ops[i];

	return rte_event_ring_enqueue_burst(r, tmp_evs, n, NULL);
}

uint16_t
sw_event_enqueue_burst(void *port, const struct rte_event ev[], uint16_t num)
{
	int32_t i;
	uint8_t new_ops[PORT_ENQUEUE_MAX_BURST_SIZE];
	struct sw_port *p = port;
	struct sw_evdev *sw = (void *)p->sw;
	uint32_t sw_inflights = rte_atomic32_read(&sw->inflights);
	uint32_t credit_update_quanta = sw->credit_update_quanta;
	int new = 0;

	if (num > PORT_ENQUEUE_MAX_BURST_SIZE)
		num = PORT_ENQUEUE_MAX_BURST_SIZE;

	for (i = 0; i < num; i++)
		new += (ev[i].op == RTE_EVENT_OP_NEW);

	if (unlikely(new > 0 && p->inflight_max < sw_inflights))
		return 0;

	if (p->inflight_credits < new) {
		/* check if event enqueue brings port over max threshold */
		if (sw_inflights + credit_update_quanta > sw->nb_events_limit)
			return 0;

		rte_atomic32_add(&sw->inflights, credit_update_quanta);
		p->inflight_credits += (credit_update_quanta);

		/* If there are fewer inflight credits than new events, limit
		 * the number of enqueued events.
		 */
		num = (p->inflight_credits < new) ? p->inflight_credits : new;
	}

	for (i = 0; i < num; i++) {
		int op = ev[i].op;
		int outstanding = p->outstanding_releases > 0;
		const uint8_t invalid_qid = (ev[i].queue_id >= sw->qid_count);

		p->inflight_credits -= (op == RTE_EVENT_OP_NEW);
		p->inflight_credits += (op == RTE_EVENT_OP_RELEASE) *
					outstanding;

		new_ops[i] = sw_qe_flag_map[op];
		new_ops[i] &= ~(invalid_qid << QE_FLAG_VALID_SHIFT);

		/* FWD and RELEASE packets will both resolve to taken (assuming
		 * correct usage of the API), providing very high correct
		 * prediction rate.
		 */
		if ((new_ops[i] & QE_FLAG_COMPLETE) && outstanding)
			p->outstanding_releases--;

		/* error case: branch to avoid touching p->stats */
		if (unlikely(invalid_qid && op != RTE_EVENT_OP_RELEASE)) {
			p->stats.rx_dropped++;
			p->inflight_credits++;
		}
	}

	/* returns number of events actually enqueued */
	uint32_t enq = enqueue_burst_with_ops(p->rx_worker_ring, ev, i,
					     new_ops);
	if (p->outstanding_releases == 0 && p->last_dequeue_burst_sz != 0) {
		uint64_t burst_ticks = rte_get_timer_cycles() -
				p->last_dequeue_ticks;
		uint64_t burst_pkt_ticks =
			burst_ticks / p->last_dequeue_burst_sz;
		p->avg_pkt_ticks -= p->avg_pkt_ticks / NUM_SAMPLES;
		p->avg_pkt_ticks += burst_pkt_ticks / NUM_SAMPLES;
		p->last_dequeue_ticks = 0;
	}

	/* Replenish credits if enough releases are performed */
	if (p->inflight_credits >= credit_update_quanta * 2) {
		rte_atomic32_sub(&sw->inflights, credit_update_quanta);
		p->inflight_credits -= credit_update_quanta;
	}

	return enq;
}

uint16_t
sw_event_enqueue(void *port, const struct rte_event *ev)
{
	return sw_event_enqueue_burst(port, ev, 1);
}

uint16_t
sw_event_dequeue_burst(void *port, struct rte_event *ev, uint16_t num,
		uint64_t wait)
{
	RTE_SET_USED(wait);
	struct sw_port *p = (void *)port;
	struct rte_event_ring *ring = p->cq_worker_ring;

	/* check that all previous dequeues have been released */
	if (p->implicit_release) {
		struct sw_evdev *sw = (void *)p->sw;
		uint32_t credit_update_quanta = sw->credit_update_quanta;
		uint16_t out_rels = p->outstanding_releases;
		uint16_t i;
		for (i = 0; i < out_rels; i++)
			sw_event_release(p, i);

		/* Replenish credits if enough releases are performed */
		if (p->inflight_credits >= credit_update_quanta * 2) {
			rte_atomic32_sub(&sw->inflights, credit_update_quanta);
			p->inflight_credits -= credit_update_quanta;
		}
	}

	/* returns number of events actually dequeued */
	uint16_t ndeq = rte_event_ring_dequeue_burst(ring, ev, num, NULL);
	if (unlikely(ndeq == 0)) {
		p->zero_polls++;
		p->total_polls++;
		goto end;
	}

	p->outstanding_releases += ndeq;
	p->last_dequeue_burst_sz = ndeq;
	p->last_dequeue_ticks = rte_get_timer_cycles();
	p->poll_buckets[(ndeq - 1) >> SW_DEQ_STAT_BUCKET_SHIFT]++;
	p->total_polls++;

end:
	return ndeq;
}

uint16_t
sw_event_dequeue(void *port, struct rte_event *ev, uint64_t wait)
{
	return sw_event_dequeue_burst(port, ev, 1, wait);
}
