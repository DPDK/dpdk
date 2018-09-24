/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2017 Intel Corporation
 */

#include <rte_ring.h>
#include <rte_hash_crc.h>
#include <rte_event_ring.h>
#include "sw_evdev.h"
#include "iq_chunk.h"

#define SW_IQS_MASK (SW_IQS_MAX-1)

/* Retrieve the highest priority IQ or -1 if no pkts available. Doing the
 * CLZ twice is faster than caching the value due to data dependencies
 */
#define PKT_MASK_TO_IQ(pkts) \
	(__builtin_ctz(pkts | (1 << SW_IQS_MAX)))

#if SW_IQS_MAX != 4
#error Misconfigured PRIO_TO_IQ caused by SW_IQS_MAX value change
#endif
#define PRIO_TO_IQ(prio) (prio >> 6)

#define MAX_PER_IQ_DEQUEUE 48
#define FLOWID_MASK (SW_QID_NUM_FIDS-1)
/* use cheap bit mixing, we only need to lose a few bits */
#define SW_HASH_FLOWID(f) (((f) ^ (f >> 10)) & FLOWID_MASK)

static inline uint32_t
sw_schedule_atomic_to_cq(struct sw_evdev *sw, struct sw_qid * const qid,
		uint32_t iq_num, unsigned int count)
{
	struct rte_event qes[MAX_PER_IQ_DEQUEUE]; /* count <= MAX */
	struct rte_event blocked_qes[MAX_PER_IQ_DEQUEUE];
	uint32_t nb_blocked = 0;
	uint32_t i;

	if (count > MAX_PER_IQ_DEQUEUE)
		count = MAX_PER_IQ_DEQUEUE;

	/* This is the QID ID. The QID ID is static, hence it can be
	 * used to identify the stage of processing in history lists etc
	 */
	uint32_t qid_id = qid->id;

	iq_dequeue_burst(sw, &qid->iq[iq_num], qes, count);
	for (i = 0; i < count; i++) {
		const struct rte_event *qe = &qes[i];
		const uint16_t flow_id = SW_HASH_FLOWID(qes[i].flow_id);
		struct sw_fid_t *fid = &qid->fids[flow_id];
		int cq = fid->cq;

		if (cq < 0) {
			uint32_t cq_idx;
			if (qid->cq_next_tx >= qid->cq_num_mapped_cqs)
				qid->cq_next_tx = 0;
			cq_idx = qid->cq_next_tx++;

			cq = qid->cq_map[cq_idx];

			/* find least used */
			int cq_free_cnt = sw->cq_ring_space[cq];
			for (cq_idx = 0; cq_idx < qid->cq_num_mapped_cqs;
					cq_idx++) {
				int test_cq = qid->cq_map[cq_idx];
				int test_cq_free = sw->cq_ring_space[test_cq];
				if (test_cq_free > cq_free_cnt) {
					cq = test_cq;
					cq_free_cnt = test_cq_free;
				}
			}

			fid->cq = cq; /* this pins early */
		}

		if (sw->cq_ring_space[cq] == 0 ||
				sw->ports[cq].inflights == SW_PORT_HIST_LIST) {
			blocked_qes[nb_blocked++] = *qe;
			continue;
		}

		struct sw_port *p = &sw->ports[cq];

		/* at this point we can queue up the packet on the cq_buf */
		fid->pcount++;
		p->cq_buf[p->cq_buf_count++] = *qe;
		p->inflights++;
		sw->cq_ring_space[cq]--;

		int head = (p->hist_head++ & (SW_PORT_HIST_LIST-1));
		p->hist_list[head].fid = flow_id;
		p->hist_list[head].qid = qid_id;

		p->stats.tx_pkts++;
		qid->stats.tx_pkts++;
		qid->to_port[cq]++;

		/* if we just filled in the last slot, flush the buffer */
		if (sw->cq_ring_space[cq] == 0) {
			struct rte_event_ring *worker = p->cq_worker_ring;
			rte_event_ring_enqueue_burst(worker, p->cq_buf,
					p->cq_buf_count,
					&sw->cq_ring_space[cq]);
			p->cq_buf_count = 0;
		}
	}
	iq_put_back(sw, &qid->iq[iq_num], blocked_qes, nb_blocked);

	return count - nb_blocked;
}

static inline uint32_t
sw_schedule_parallel_to_cq(struct sw_evdev *sw, struct sw_qid * const qid,
		uint32_t iq_num, unsigned int count, int keep_order)
{
	uint32_t i;
	uint32_t cq_idx = qid->cq_next_tx;

	/* This is the QID ID. The QID ID is static, hence it can be
	 * used to identify the stage of processing in history lists etc
	 */
	uint32_t qid_id = qid->id;

	if (count > MAX_PER_IQ_DEQUEUE)
		count = MAX_PER_IQ_DEQUEUE;

	if (keep_order)
		/* only schedule as many as we have reorder buffer entries */
		count = RTE_MIN(count,
				rte_ring_count(qid->reorder_buffer_freelist));

	for (i = 0; i < count; i++) {
		const struct rte_event *qe = iq_peek(&qid->iq[iq_num]);
		uint32_t cq_check_count = 0;
		uint32_t cq;

		/*
		 *  for parallel, just send to next available CQ in round-robin
		 * fashion. So scan for an available CQ. If all CQs are full
		 * just return and move on to next QID
		 */
		do {
			if (++cq_check_count > qid->cq_num_mapped_cqs)
				goto exit;
			if (cq_idx >= qid->cq_num_mapped_cqs)
				cq_idx = 0;
			cq = qid->cq_map[cq_idx++];

		} while (rte_event_ring_free_count(
				sw->ports[cq].cq_worker_ring) == 0 ||
				sw->ports[cq].inflights == SW_PORT_HIST_LIST);

		struct sw_port *p = &sw->ports[cq];
		if (sw->cq_ring_space[cq] == 0 ||
				p->inflights == SW_PORT_HIST_LIST)
			break;

		sw->cq_ring_space[cq]--;

		qid->stats.tx_pkts++;

		const int head = (p->hist_head & (SW_PORT_HIST_LIST-1));
		p->hist_list[head].fid = SW_HASH_FLOWID(qe->flow_id);
		p->hist_list[head].qid = qid_id;

		if (keep_order)
			rte_ring_sc_dequeue(qid->reorder_buffer_freelist,
					(void *)&p->hist_list[head].rob_entry);

		sw->ports[cq].cq_buf[sw->ports[cq].cq_buf_count++] = *qe;
		iq_pop(sw, &qid->iq[iq_num]);

		rte_compiler_barrier();
		p->inflights++;
		p->stats.tx_pkts++;
		p->hist_head++;
	}
exit:
	qid->cq_next_tx = cq_idx;
	return i;
}

static uint32_t
sw_schedule_dir_to_cq(struct sw_evdev *sw, struct sw_qid * const qid,
		uint32_t iq_num, unsigned int count __rte_unused)
{
	uint32_t cq_id = qid->cq_map[0];
	struct sw_port *port = &sw->ports[cq_id];

	/* get max burst enq size for cq_ring */
	uint32_t count_free = sw->cq_ring_space[cq_id];
	if (count_free == 0)
		return 0;

	/* burst dequeue from the QID IQ ring */
	struct sw_iq *iq = &qid->iq[iq_num];
	uint32_t ret = iq_dequeue_burst(sw, iq,
			&port->cq_buf[port->cq_buf_count], count_free);
	port->cq_buf_count += ret;

	/* Update QID, Port and Total TX stats */
	qid->stats.tx_pkts += ret;
	port->stats.tx_pkts += ret;

	/* Subtract credits from cached value */
	sw->cq_ring_space[cq_id] -= ret;

	return ret;
}

static uint32_t
sw_schedule_qid_to_cq(struct sw_evdev *sw)
{
	uint32_t pkts = 0;
	uint32_t qid_idx;

	sw->sched_cq_qid_called++;

	for (qid_idx = 0; qid_idx < sw->qid_count; qid_idx++) {
		struct sw_qid *qid = sw->qids_prioritized[qid_idx];

		int type = qid->type;
		int iq_num = PKT_MASK_TO_IQ(qid->iq_pkt_mask);

		/* zero mapped CQs indicates directed */
		if (iq_num >= SW_IQS_MAX || qid->cq_num_mapped_cqs == 0)
			continue;

		uint32_t pkts_done = 0;
		uint32_t count = iq_count(&qid->iq[iq_num]);

		if (count > 0) {
			if (type == SW_SCHED_TYPE_DIRECT)
				pkts_done += sw_schedule_dir_to_cq(sw, qid,
						iq_num, count);
			else if (type == RTE_SCHED_TYPE_ATOMIC)
				pkts_done += sw_schedule_atomic_to_cq(sw, qid,
						iq_num, count);
			else
				pkts_done += sw_schedule_parallel_to_cq(sw, qid,
						iq_num, count,
						type == RTE_SCHED_TYPE_ORDERED);
		}

		/* Check if the IQ that was polled is now empty, and unset it
		 * in the IQ mask if its empty.
		 */
		int all_done = (pkts_done == count);

		qid->iq_pkt_mask &= ~(all_done << (iq_num));
		pkts += pkts_done;
	}

	return pkts;
}

/* This function will perform re-ordering of packets, and injecting into
 * the appropriate QID IQ. As LB and DIR QIDs are in the same array, but *NOT*
 * contiguous in that array, this function accepts a "range" of QIDs to scan.
 */
static uint16_t
sw_schedule_reorder(struct sw_evdev *sw, int qid_start, int qid_end)
{
	/* Perform egress reordering */
	struct rte_event *qe;
	uint32_t pkts_iter = 0;

	for (; qid_start < qid_end; qid_start++) {
		struct sw_qid *qid = &sw->qids[qid_start];
		int i, num_entries_in_use;

		if (qid->type != RTE_SCHED_TYPE_ORDERED)
			continue;

		num_entries_in_use = rte_ring_free_count(
					qid->reorder_buffer_freelist);

		for (i = 0; i < num_entries_in_use; i++) {
			struct reorder_buffer_entry *entry;
			int j;

			entry = &qid->reorder_buffer[qid->reorder_buffer_index];

			if (!entry->ready)
				break;

			for (j = 0; j < entry->num_fragments; j++) {
				uint16_t dest_qid;
				uint16_t dest_iq;

				int idx = entry->fragment_index + j;
				qe = &entry->fragments[idx];

				dest_qid = qe->queue_id;
				dest_iq  = PRIO_TO_IQ(qe->priority);

				if (dest_qid >= sw->qid_count) {
					sw->stats.rx_dropped++;
					continue;
				}

				pkts_iter++;

				struct sw_qid *q = &sw->qids[dest_qid];
				struct sw_iq *iq = &q->iq[dest_iq];

				/* we checked for space above, so enqueue must
				 * succeed
				 */
				iq_enqueue(sw, iq, qe);
				q->iq_pkt_mask |= (1 << (dest_iq));
				q->iq_pkt_count[dest_iq]++;
				q->stats.rx_pkts++;
			}

			entry->ready = (j != entry->num_fragments);
			entry->num_fragments -= j;
			entry->fragment_index += j;

			if (!entry->ready) {
				entry->fragment_index = 0;

				rte_ring_sp_enqueue(
						qid->reorder_buffer_freelist,
						entry);

				qid->reorder_buffer_index++;
				qid->reorder_buffer_index %= qid->window_size;
			}
		}
	}
	return pkts_iter;
}

static __rte_always_inline void
sw_refill_pp_buf(struct sw_evdev *sw, struct sw_port *port)
{
	RTE_SET_USED(sw);
	struct rte_event_ring *worker = port->rx_worker_ring;
	port->pp_buf_start = 0;
	port->pp_buf_count = rte_event_ring_dequeue_burst(worker, port->pp_buf,
			RTE_DIM(port->pp_buf), NULL);
}

static __rte_always_inline uint32_t
__pull_port_lb(struct sw_evdev *sw, uint32_t port_id, int allow_reorder)
{
	static struct reorder_buffer_entry dummy_rob;
	uint32_t pkts_iter = 0;
	struct sw_port *port = &sw->ports[port_id];

	/* If shadow ring has 0 pkts, pull from worker ring */
	if (port->pp_buf_count == 0)
		sw_refill_pp_buf(sw, port);

	while (port->pp_buf_count) {
		const struct rte_event *qe = &port->pp_buf[port->pp_buf_start];
		struct sw_hist_list_entry *hist_entry = NULL;
		uint8_t flags = qe->op;
		const uint16_t eop = !(flags & QE_FLAG_NOT_EOP);
		int needs_reorder = 0;
		/* if no-reordering, having PARTIAL == NEW */
		if (!allow_reorder && !eop)
			flags = QE_FLAG_VALID;

		/*
		 * if we don't have space for this packet in an IQ,
		 * then move on to next queue. Technically, for a
		 * packet that needs reordering, we don't need to check
		 * here, but it simplifies things not to special-case
		 */
		uint32_t iq_num = PRIO_TO_IQ(qe->priority);
		struct sw_qid *qid = &sw->qids[qe->queue_id];

		/* now process based on flags. Note that for directed
		 * queues, the enqueue_flush masks off all but the
		 * valid flag. This makes FWD and PARTIAL enqueues just
		 * NEW type, and makes DROPS no-op calls.
		 */
		if ((flags & QE_FLAG_COMPLETE) && port->inflights > 0) {
			const uint32_t hist_tail = port->hist_tail &
					(SW_PORT_HIST_LIST - 1);

			hist_entry = &port->hist_list[hist_tail];
			const uint32_t hist_qid = hist_entry->qid;
			const uint32_t hist_fid = hist_entry->fid;

			struct sw_fid_t *fid =
				&sw->qids[hist_qid].fids[hist_fid];
			fid->pcount -= eop;
			if (fid->pcount == 0)
				fid->cq = -1;

			if (allow_reorder) {
				/* set reorder ready if an ordered QID */
				uintptr_t rob_ptr =
					(uintptr_t)hist_entry->rob_entry;
				const uintptr_t valid = (rob_ptr != 0);
				needs_reorder = valid;
				rob_ptr |=
					((valid - 1) & (uintptr_t)&dummy_rob);
				struct reorder_buffer_entry *tmp_rob_ptr =
					(struct reorder_buffer_entry *)rob_ptr;
				tmp_rob_ptr->ready = eop * needs_reorder;
			}

			port->inflights -= eop;
			port->hist_tail += eop;
		}
		if (flags & QE_FLAG_VALID) {
			port->stats.rx_pkts++;

			if (allow_reorder && needs_reorder) {
				struct reorder_buffer_entry *rob_entry =
						hist_entry->rob_entry;

				hist_entry->rob_entry = NULL;
				/* Although fragmentation not currently
				 * supported by eventdev API, we support it
				 * here. Open: How do we alert the user that
				 * they've exceeded max frags?
				 */
				int num_frag = rob_entry->num_fragments;
				if (num_frag == SW_FRAGMENTS_MAX)
					sw->stats.rx_dropped++;
				else {
					int idx = rob_entry->num_fragments++;
					rob_entry->fragments[idx] = *qe;
				}
				goto end_qe;
			}

			/* Use the iq_num from above to push the QE
			 * into the qid at the right priority
			 */

			qid->iq_pkt_mask |= (1 << (iq_num));
			iq_enqueue(sw, &qid->iq[iq_num], qe);
			qid->iq_pkt_count[iq_num]++;
			qid->stats.rx_pkts++;
			pkts_iter++;
		}

end_qe:
		port->pp_buf_start++;
		port->pp_buf_count--;
	} /* while (avail_qes) */

	return pkts_iter;
}

static uint32_t
sw_schedule_pull_port_lb(struct sw_evdev *sw, uint32_t port_id)
{
	return __pull_port_lb(sw, port_id, 1);
}

static uint32_t
sw_schedule_pull_port_no_reorder(struct sw_evdev *sw, uint32_t port_id)
{
	return __pull_port_lb(sw, port_id, 0);
}

static uint32_t
sw_schedule_pull_port_dir(struct sw_evdev *sw, uint32_t port_id)
{
	uint32_t pkts_iter = 0;
	struct sw_port *port = &sw->ports[port_id];

	/* If shadow ring has 0 pkts, pull from worker ring */
	if (port->pp_buf_count == 0)
		sw_refill_pp_buf(sw, port);

	while (port->pp_buf_count) {
		const struct rte_event *qe = &port->pp_buf[port->pp_buf_start];
		uint8_t flags = qe->op;

		if ((flags & QE_FLAG_VALID) == 0)
			goto end_qe;

		uint32_t iq_num = PRIO_TO_IQ(qe->priority);
		struct sw_qid *qid = &sw->qids[qe->queue_id];
		struct sw_iq *iq = &qid->iq[iq_num];

		port->stats.rx_pkts++;

		/* Use the iq_num from above to push the QE
		 * into the qid at the right priority
		 */
		qid->iq_pkt_mask |= (1 << (iq_num));
		iq_enqueue(sw, iq, qe);
		qid->iq_pkt_count[iq_num]++;
		qid->stats.rx_pkts++;
		pkts_iter++;

end_qe:
		port->pp_buf_start++;
		port->pp_buf_count--;
	} /* while port->pp_buf_count */

	return pkts_iter;
}

void
sw_event_schedule(struct rte_eventdev *dev)
{
	struct sw_evdev *sw = sw_pmd_priv(dev);
	uint32_t in_pkts, out_pkts;
	uint32_t out_pkts_total = 0, in_pkts_total = 0;
	int32_t sched_quanta = sw->sched_quanta;
	uint32_t i;

	sw->sched_called++;
	if (unlikely(!sw->started))
		return;

	do {
		uint32_t in_pkts_this_iteration = 0;

		/* Pull from rx_ring for ports */
		do {
			in_pkts = 0;
			for (i = 0; i < sw->port_count; i++) {
				/* ack the unlinks in progress as done */
				if (sw->ports[i].unlinks_in_progress)
					sw->ports[i].unlinks_in_progress = 0;

				if (sw->ports[i].is_directed)
					in_pkts += sw_schedule_pull_port_dir(sw, i);
				else if (sw->ports[i].num_ordered_qids > 0)
					in_pkts += sw_schedule_pull_port_lb(sw, i);
				else
					in_pkts += sw_schedule_pull_port_no_reorder(sw, i);
			}

			/* QID scan for re-ordered */
			in_pkts += sw_schedule_reorder(sw, 0,
					sw->qid_count);
			in_pkts_this_iteration += in_pkts;
		} while (in_pkts > 4 &&
				(int)in_pkts_this_iteration < sched_quanta);

		out_pkts = sw_schedule_qid_to_cq(sw);
		out_pkts_total += out_pkts;
		in_pkts_total += in_pkts_this_iteration;

		if (in_pkts == 0 && out_pkts == 0)
			break;
	} while ((int)out_pkts_total < sched_quanta);

	sw->stats.tx_pkts += out_pkts_total;
	sw->stats.rx_pkts += in_pkts_total;

	sw->sched_no_iq_enqueues += (in_pkts_total == 0);
	sw->sched_no_cq_enqueues += (out_pkts_total == 0);

	/* push all the internal buffered QEs in port->cq_ring to the
	 * worker cores: aka, do the ring transfers batched.
	 */
	for (i = 0; i < sw->port_count; i++) {
		struct rte_event_ring *worker = sw->ports[i].cq_worker_ring;
		rte_event_ring_enqueue_burst(worker, sw->ports[i].cq_buf,
				sw->ports[i].cq_buf_count,
				&sw->cq_ring_space[i]);
		sw->ports[i].cq_buf_count = 0;
	}

}
