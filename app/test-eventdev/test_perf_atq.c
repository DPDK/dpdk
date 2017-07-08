/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc 2017.
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
 *     * Neither the name of Cavium, Inc nor the names of its
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

#include "test_perf_common.h"

/* See http://dpdk.org/doc/guides/tools/testeventdev.html for test details */

static inline int
atq_nb_event_queues(struct evt_options *opt)
{
	/* nb_queues = number of producers */
	return evt_nr_active_lcores(opt->plcores);
}

static inline __attribute__((always_inline)) void
atq_mark_fwd_latency(struct rte_event *const ev)
{
	if (unlikely(ev->sub_event_type == 0)) {
		struct perf_elt *const m = ev->event_ptr;

		m->timestamp = rte_get_timer_cycles();
	}
}

static inline __attribute__((always_inline)) void
atq_fwd_event(struct rte_event *const ev, uint8_t *const sched_type_list,
		const uint8_t nb_stages)
{
	ev->sub_event_type++;
	ev->sched_type = sched_type_list[ev->sub_event_type % nb_stages];
	ev->op = RTE_EVENT_OP_FORWARD;
	ev->event_type = RTE_EVENT_TYPE_CPU;
}

static int
perf_atq_worker(void *arg, const int enable_fwd_latency)
{
	PERF_WORKER_INIT;
	struct rte_event ev;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (enable_fwd_latency)
			rte_prefetch0(ev.event_ptr);

		if (!event) {
			rte_pause();
			continue;
		}

		if (enable_fwd_latency)
		/* first stage in pipeline, mark ts to compute fwd latency */
			atq_mark_fwd_latency(&ev);

		/* last stage in pipeline */
		if (unlikely((ev.sub_event_type % nb_stages) == laststage)) {
			if (enable_fwd_latency)
				cnt = perf_process_last_stage_latency(pool,
					&ev, w, bufs, sz, cnt);
			else
				cnt = perf_process_last_stage(pool, &ev, w,
					 bufs, sz, cnt);
		} else {
			atq_fwd_event(&ev, sched_type_list, nb_stages);
			while (rte_event_enqueue_burst(dev, port, &ev, 1) != 1)
				rte_pause();
		}
	}
	return 0;
}

static int
perf_atq_worker_burst(void *arg, const int enable_fwd_latency)
{
	PERF_WORKER_INIT;
	uint16_t i;
	/* +1 to avoid prefetch out of array check */
	struct rte_event ev[BURST_SIZE + 1];

	while (t->done == false) {
		uint16_t const nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			if (enable_fwd_latency) {
				rte_prefetch0(ev[i+1].event_ptr);
				/* first stage in pipeline.
				 * mark time stamp to compute fwd latency
				 */
				atq_mark_fwd_latency(&ev[i]);
			}
			/* last stage in pipeline */
			if (unlikely((ev[i].sub_event_type % nb_stages)
						== laststage)) {
				if (enable_fwd_latency)
					cnt = perf_process_last_stage_latency(
						pool, &ev[i], w, bufs, sz, cnt);
				else
					cnt = perf_process_last_stage(pool,
						&ev[i], w, bufs, sz, cnt);

				ev[i].op = RTE_EVENT_OP_RELEASE;
			} else {
				atq_fwd_event(&ev[i], sched_type_list,
						nb_stages);
			}
		}

		uint16_t enq;

		enq = rte_event_enqueue_burst(dev, port, ev, nb_rx);
		while (enq < nb_rx) {
			enq += rte_event_enqueue_burst(dev, port,
							ev + enq, nb_rx - enq);
		}
	}
	return 0;
}

static int
worker_wrapper(void *arg)
{
	struct worker_data *w  = arg;
	struct evt_options *opt = w->t->opt;

	const bool burst = evt_has_burst_mode(w->dev_id);
	const int fwd_latency = opt->fwd_latency;

	/* allow compiler to optimize */
	if (!burst && !fwd_latency)
		return perf_atq_worker(arg, 0);
	else if (!burst && fwd_latency)
		return perf_atq_worker(arg, 1);
	else if (burst && !fwd_latency)
		return perf_atq_worker_burst(arg, 0);
	else if (burst && fwd_latency)
		return perf_atq_worker_burst(arg, 1);

	rte_panic("invalid worker\n");
}

static int
perf_atq_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	return perf_launch_lcores(test, opt, worker_wrapper);
}

static int
perf_atq_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	int ret;
	uint8_t queue;

	const struct rte_event_dev_config config = {
			.nb_event_queues = atq_nb_event_queues(opt),
			.nb_event_ports = perf_nb_event_ports(opt),
			.nb_events_limit  = 4096,
			.nb_event_queue_flows = opt->nb_flows,
			.nb_event_port_dequeue_depth = 128,
			.nb_event_port_enqueue_depth = 128,
	};

	ret = rte_event_dev_configure(opt->dev_id, &config);
	if (ret) {
		evt_err("failed to configure eventdev %d", opt->dev_id);
		return ret;
	}

	struct rte_event_queue_conf q_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES,
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	/* queue configurations */
	for (queue = 0; queue < atq_nb_event_queues(opt); queue++) {
		ret = rte_event_queue_setup(opt->dev_id, queue, &q_conf);
		if (ret) {
			evt_err("failed to setup queue=%d", queue);
			return ret;
		}
	}

	ret = perf_event_dev_port_setup(test, opt, 1 /* stride */,
					atq_nb_event_queues(opt));
	if (ret)
		return ret;

	ret = rte_event_dev_start(opt->dev_id);
	if (ret) {
		evt_err("failed to start eventdev %d", opt->dev_id);
		return ret;
	}

	return 0;
}

static void
perf_atq_opt_dump(struct evt_options *opt)
{
	perf_opt_dump(opt, atq_nb_event_queues(opt));
}

static int
perf_atq_opt_check(struct evt_options *opt)
{
	return perf_opt_check(opt, atq_nb_event_queues(opt));
}

static bool
perf_atq_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < atq_nb_event_queues(opt) ||
			dev_info.max_event_ports < perf_nb_event_ports(opt)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d",
			atq_nb_event_queues(opt), dev_info.max_event_queues,
			perf_nb_event_ports(opt), dev_info.max_event_ports);
	}
	if (!evt_has_all_types_queue(opt->dev_id))
		return false;

	return true;
}

static const struct evt_test_ops perf_atq =  {
	.cap_check          = perf_atq_capability_check,
	.opt_check          = perf_atq_opt_check,
	.opt_dump           = perf_atq_opt_dump,
	.test_setup         = perf_test_setup,
	.mempool_setup      = perf_mempool_setup,
	.eventdev_setup     = perf_atq_eventdev_setup,
	.launch_lcores      = perf_atq_launch_lcores,
	.eventdev_destroy   = perf_eventdev_destroy,
	.mempool_destroy    = perf_mempool_destroy,
	.test_result        = perf_test_result,
	.test_destroy       = perf_test_destroy,
};

EVT_TEST_REGISTER(perf_atq);
