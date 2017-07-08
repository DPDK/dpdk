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
perf_queue_nb_event_queues(struct evt_options *opt)
{
	/* nb_queues = number of producers * number of stages */
	return evt_nr_active_lcores(opt->plcores) * opt->nb_stages;
}

static inline __attribute__((always_inline)) void
mark_fwd_latency(struct rte_event *const ev,
		const uint8_t nb_stages)
{
	if (unlikely((ev->queue_id % nb_stages) == 0)) {
		struct perf_elt *const m = ev->event_ptr;

		m->timestamp = rte_get_timer_cycles();
	}
}

static inline __attribute__((always_inline)) void
fwd_event(struct rte_event *const ev, uint8_t *const sched_type_list,
		const uint8_t nb_stages)
{
	ev->queue_id++;
	ev->sched_type = sched_type_list[ev->queue_id % nb_stages];
	ev->op = RTE_EVENT_OP_FORWARD;
	ev->event_type = RTE_EVENT_TYPE_CPU;
}

static int
perf_queue_worker(void *arg, const int enable_fwd_latency)
{
	PERF_WORKER_INIT;
	struct rte_event ev;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}
		if (enable_fwd_latency)
		/* first q in pipeline, mark timestamp to compute fwd latency */
			mark_fwd_latency(&ev, nb_stages);

		/* last stage in pipeline */
		if (unlikely((ev.queue_id % nb_stages) == laststage)) {
			if (enable_fwd_latency)
				cnt = perf_process_last_stage_latency(pool,
					&ev, w, bufs, sz, cnt);
			else
				cnt = perf_process_last_stage(pool,
					&ev, w, bufs, sz, cnt);
		} else {
			fwd_event(&ev, sched_type_list, nb_stages);
			while (rte_event_enqueue_burst(dev, port, &ev, 1) != 1)
				rte_pause();
		}
	}
	return 0;
}

static int
perf_queue_worker_burst(void *arg, const int enable_fwd_latency)
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
				/* first queue in pipeline.
				 * mark time stamp to compute fwd latency
				 */
				mark_fwd_latency(&ev[i], nb_stages);
			}
			/* last stage in pipeline */
			if (unlikely((ev[i].queue_id % nb_stages) ==
						 laststage)) {
				if (enable_fwd_latency)
					cnt = perf_process_last_stage_latency(
						pool, &ev[i], w, bufs, sz, cnt);
				else
					cnt = perf_process_last_stage(pool,
						&ev[i], w, bufs, sz, cnt);

				ev[i].op = RTE_EVENT_OP_RELEASE;
			} else {
				fwd_event(&ev[i], sched_type_list, nb_stages);
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
		return perf_queue_worker(arg, 0);
	else if (!burst && fwd_latency)
		return perf_queue_worker(arg, 1);
	else if (burst && !fwd_latency)
		return perf_queue_worker_burst(arg, 0);
	else if (burst && fwd_latency)
		return perf_queue_worker_burst(arg, 1);

	rte_panic("invalid worker\n");
}

static int
perf_queue_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	return perf_launch_lcores(test, opt, worker_wrapper);
}

static int
perf_queue_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	uint8_t queue;
	int nb_stages = opt->nb_stages;
	int ret;

	const struct rte_event_dev_config config = {
			.nb_event_queues = perf_queue_nb_event_queues(opt),
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
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	/* queue configurations */
	for (queue = 0; queue < perf_queue_nb_event_queues(opt); queue++) {
		q_conf.event_queue_cfg =  evt_sched_type2queue_cfg
				(opt->sched_type_list[queue % nb_stages]);

		if (opt->q_priority) {
			uint8_t stage_pos = queue % nb_stages;
			/* Configure event queues(stage 0 to stage n) with
			 * RTE_EVENT_DEV_PRIORITY_LOWEST to
			 * RTE_EVENT_DEV_PRIORITY_HIGHEST.
			 */
			uint8_t step = RTE_EVENT_DEV_PRIORITY_LOWEST /
					(nb_stages - 1);
			/* Higher prio for the queues closer to last stage */
			q_conf.priority = RTE_EVENT_DEV_PRIORITY_LOWEST -
					(step * stage_pos);
		}
		ret = rte_event_queue_setup(opt->dev_id, queue, &q_conf);
		if (ret) {
			evt_err("failed to setup queue=%d", queue);
			return ret;
		}
	}

	ret = perf_event_dev_port_setup(test, opt, nb_stages /* stride */,
					perf_queue_nb_event_queues(opt));
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
perf_queue_opt_dump(struct evt_options *opt)
{
	evt_dump_fwd_latency(opt);
	perf_opt_dump(opt, perf_queue_nb_event_queues(opt));
}

static int
perf_queue_opt_check(struct evt_options *opt)
{
	return perf_opt_check(opt, perf_queue_nb_event_queues(opt));
}

static bool
perf_queue_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < perf_queue_nb_event_queues(opt) ||
			dev_info.max_event_ports < perf_nb_event_ports(opt)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d",
			perf_queue_nb_event_queues(opt),
			dev_info.max_event_queues,
			perf_nb_event_ports(opt), dev_info.max_event_ports);
	}

	return true;
}

static const struct evt_test_ops perf_queue =  {
	.cap_check          = perf_queue_capability_check,
	.opt_check          = perf_queue_opt_check,
	.opt_dump           = perf_queue_opt_dump,
	.test_setup         = perf_test_setup,
	.mempool_setup      = perf_mempool_setup,
	.eventdev_setup     = perf_queue_eventdev_setup,
	.launch_lcores      = perf_queue_launch_lcores,
	.eventdev_destroy   = perf_eventdev_destroy,
	.mempool_destroy    = perf_mempool_destroy,
	.test_result        = perf_test_result,
	.test_destroy       = perf_test_destroy,
};

EVT_TEST_REGISTER(perf_queue);
