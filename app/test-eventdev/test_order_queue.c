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

#include <stdio.h>
#include <unistd.h>

#include "test_order_common.h"

/* See http://dpdk.org/doc/guides/tools/testeventdev.html for test details */

#define NB_QUEUES 2
static int
order_queue_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	int ret;

	const uint8_t nb_workers = evt_nr_active_lcores(opt->wlcores);
	/* number of active worker cores + 1 producer */
	const uint8_t nb_ports = nb_workers + 1;

	const struct rte_event_dev_config config = {
			.nb_event_queues = NB_QUEUES,/* q0 ordered, q1 atomic */
			.nb_event_ports = nb_ports,
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

	/* q0 (ordered queue) configuration */
	struct rte_event_queue_conf q0_ordered_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ORDERED_ONLY,
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	ret = rte_event_queue_setup(opt->dev_id, 0, &q0_ordered_conf);
	if (ret) {
		evt_err("failed to setup queue0 eventdev %d", opt->dev_id);
		return ret;
	}

	/* q1 (atomic queue) configuration */
	struct rte_event_queue_conf q1_atomic_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY,
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	ret = rte_event_queue_setup(opt->dev_id, 1, &q1_atomic_conf);
	if (ret) {
		evt_err("failed to setup queue1 eventdev %d", opt->dev_id);
		return ret;
	}

	/* setup one port per worker, linking to all queues */
	ret = order_event_dev_port_setup(test, opt, nb_workers, NB_QUEUES);
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
order_queue_opt_dump(struct evt_options *opt)
{
	order_opt_dump(opt);
	evt_dump("nb_evdev_queues", "%d", NB_QUEUES);
}

static bool
order_queue_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < NB_QUEUES || dev_info.max_event_ports <
			order_nb_event_ports(opt)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d",
			NB_QUEUES, dev_info.max_event_queues,
			order_nb_event_ports(opt), dev_info.max_event_ports);
		return false;
	}

	return true;
}

static const struct evt_test_ops order_queue =  {
	.cap_check          = order_queue_capability_check,
	.opt_check          = order_opt_check,
	.opt_dump           = order_queue_opt_dump,
	.test_setup         = order_test_setup,
	.mempool_setup      = order_mempool_setup,
	.eventdev_setup     = order_queue_eventdev_setup,
	.eventdev_destroy   = order_eventdev_destroy,
	.mempool_destroy    = order_mempool_destroy,
	.test_result        = order_test_result,
	.test_destroy       = order_test_destroy,
};

EVT_TEST_REGISTER(order_queue);
