/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_atomic.h>
#include <rte_cycles.h>

#include "sw_evdev.h"
#include "event_ring.h"

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
	qe_ring_enqueue_burst(p->rx_worker_ring, &ev, 1, &free_count);

	/* each release returns one credit */
	p->outstanding_releases--;
	p->inflight_credits++;
}

uint16_t
sw_event_enqueue_burst(void *port, const struct rte_event ev[], uint16_t num)
{
	int32_t i;
	uint8_t new_ops[PORT_ENQUEUE_MAX_BURST_SIZE];
	struct sw_port *p = port;
	struct sw_evdev *sw = (void *)p->sw;
	uint32_t sw_inflights = rte_atomic32_read(&sw->inflights);

	if (unlikely(p->inflight_max < sw_inflights))
		return 0;

	if (num > PORT_ENQUEUE_MAX_BURST_SIZE)
		num = PORT_ENQUEUE_MAX_BURST_SIZE;

	if (p->inflight_credits < num) {
		/* check if event enqueue brings port over max threshold */
		uint32_t credit_update_quanta = sw->credit_update_quanta;
		if (sw_inflights + credit_update_quanta > sw->nb_events_limit)
			return 0;

		rte_atomic32_add(&sw->inflights, credit_update_quanta);
		p->inflight_credits += (credit_update_quanta);

		if (p->inflight_credits < num)
			return 0;
	}

	uint32_t forwards = 0;
	for (i = 0; i < num; i++) {
		int op = ev[i].op;
		int outstanding = p->outstanding_releases > 0;
		const uint8_t invalid_qid = (ev[i].queue_id >= sw->qid_count);

		p->inflight_credits -= (op == RTE_EVENT_OP_NEW);
		p->inflight_credits += (op == RTE_EVENT_OP_RELEASE) *
					outstanding;
		forwards += (op == RTE_EVENT_OP_FORWARD);

		new_ops[i] = sw_qe_flag_map[op];
		new_ops[i] &= ~(invalid_qid << QE_FLAG_VALID_SHIFT);

		/* FWD and RELEASE packets will both resolve to taken (assuming
		 * correct usage of the API), providing very high correct
		 * prediction rate.
		 */
		if ((new_ops[i] & QE_FLAG_COMPLETE) && outstanding)
			p->outstanding_releases--;

		/* error case: branch to avoid touching p->stats */
		if (unlikely(invalid_qid)) {
			p->stats.rx_dropped++;
			p->inflight_credits++;
		}
	}

	/* handle directed port forward credits */
	p->inflight_credits -= forwards * p->is_directed;

	/* returns number of events actually enqueued */
	uint32_t enq = qe_ring_enqueue_burst_with_ops(p->rx_worker_ring, ev, i,
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
	struct sw_evdev *sw = (void *)p->sw;
	struct qe_ring *ring = p->cq_worker_ring;
	uint32_t credit_update_quanta = sw->credit_update_quanta;

	/* check that all previous dequeues have been released */
	if (!p->is_directed) {
		uint16_t out_rels = p->outstanding_releases;
		uint16_t i;
		for (i = 0; i < out_rels; i++)
			sw_event_release(p, i);
	}

	/* returns number of events actually dequeued */
	uint16_t ndeq = qe_ring_dequeue_burst(ring, ev, num);
	if (unlikely(ndeq == 0)) {
		p->outstanding_releases = 0;
		p->zero_polls++;
		p->total_polls++;
		goto end;
	}

	/* only add credits for directed ports - LB ports send RELEASEs */
	p->inflight_credits += ndeq * p->is_directed;
	p->outstanding_releases = ndeq;
	p->last_dequeue_burst_sz = ndeq;
	p->last_dequeue_ticks = rte_get_timer_cycles();
	p->poll_buckets[(ndeq - 1) >> SW_DEQ_STAT_BUCKET_SHIFT]++;
	p->total_polls++;

end:
	if (p->inflight_credits >= credit_update_quanta * 2 &&
			p->inflight_credits > credit_update_quanta + ndeq) {
		rte_atomic32_sub(&sw->inflights, credit_update_quanta);
		p->inflight_credits -= credit_update_quanta;
	}
	return ndeq;
}

uint16_t
sw_event_dequeue(void *port, struct rte_event *ev, uint64_t wait)
{
	return sw_event_dequeue_burst(port, ev, 1, wait);
}
