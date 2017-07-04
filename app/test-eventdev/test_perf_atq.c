/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium 2017.
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
 *     * Neither the name of Cavium nor the names of its
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
	.eventdev_destroy   = perf_eventdev_destroy,
	.mempool_destroy    = perf_mempool_destroy,
	.test_result        = perf_test_result,
	.test_destroy       = perf_test_destroy,
};

EVT_TEST_REGISTER(perf_atq);
