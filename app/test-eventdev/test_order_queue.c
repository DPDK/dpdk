/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <stdio.h>
#include <unistd.h>

#include "test_order_common.h"

/* See http://doc.dpdk.org/guides/tools/testeventdev.html for test details */

static __rte_always_inline void
order_queue_process_stage_0(struct rte_event *const ev)
{
	ev->queue_id = 1; /* q1 atomic queue */
	ev->op = RTE_EVENT_OP_FORWARD;
	ev->sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev->event_type = RTE_EVENT_TYPE_CPU;
}

static int
order_queue_worker(void *arg, const bool flow_id_cap)
{
	ORDER_WORKER_INIT;
	struct rte_event ev;

	while (t->err == false) {
		uint16_t event = rte_event_dequeue_burst(dev_id, port,
					&ev, 1, 0);
		if (!event) {
			if (__atomic_load_n(outstand_pkts, __ATOMIC_RELAXED) <= 0)
				break;
			rte_pause();
			continue;
		}

		if (!flow_id_cap)
			order_flow_id_copy_from_mbuf(t, &ev);

		if (ev.queue_id == 0) { /* from ordered queue */
			order_queue_process_stage_0(&ev);
			while (rte_event_enqueue_burst(dev_id, port, &ev, 1)
					!= 1)
				rte_pause();
		} else if (ev.queue_id == 1) { /* from atomic queue */
			order_process_stage_1(t, &ev, nb_flows,
					expected_flow_seq, outstand_pkts);
		} else {
			order_process_stage_invalid(t, &ev);
		}
	}
	return 0;
}

static int
order_queue_worker_burst(void *arg, const bool flow_id_cap)
{
	ORDER_WORKER_INIT;
	struct rte_event ev[BURST_SIZE];
	uint16_t i;

	while (t->err == false) {
		uint16_t const nb_rx = rte_event_dequeue_burst(dev_id, port, ev,
				BURST_SIZE, 0);

		if (nb_rx == 0) {
			if (__atomic_load_n(outstand_pkts, __ATOMIC_RELAXED) <= 0)
				break;
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {

			if (!flow_id_cap)
				order_flow_id_copy_from_mbuf(t, &ev[i]);

			if (ev[i].queue_id == 0) { /* from ordered queue */
				order_queue_process_stage_0(&ev[i]);
			} else if (ev[i].queue_id == 1) {/* from atomic queue */
				order_process_stage_1(t, &ev[i], nb_flows,
					expected_flow_seq, outstand_pkts);
				ev[i].op = RTE_EVENT_OP_RELEASE;
			} else {
				order_process_stage_invalid(t, &ev[i]);
			}
		}

		uint16_t enq;

		enq = rte_event_enqueue_burst(dev_id, port, ev, nb_rx);
		while (enq < nb_rx) {
			enq += rte_event_enqueue_burst(dev_id, port,
							ev + enq, nb_rx - enq);
		}
	}
	return 0;
}

static int
worker_wrapper(void *arg)
{
	struct worker_data *w  = arg;
	const bool burst = evt_has_burst_mode(w->dev_id);
	const bool flow_id_cap = evt_has_flow_id(w->dev_id);

	if (burst) {
		if (flow_id_cap)
			return order_queue_worker_burst(arg, true);
		else
			return order_queue_worker_burst(arg, false);
	} else {
		if (flow_id_cap)
			return order_queue_worker(arg, true);
		else
			return order_queue_worker(arg, false);
	}
}

static int
order_queue_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	return order_launch_lcores(test, opt, worker_wrapper);
}

#define NB_QUEUES 2
static int
order_queue_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	int ret;

	const uint8_t nb_workers = evt_nr_active_lcores(opt->wlcores);
	/* number of active worker cores + 1 producer */
	const uint8_t nb_ports = nb_workers + 1;

	ret = evt_configure_eventdev(opt, NB_QUEUES, nb_ports);
	if (ret) {
		evt_err("failed to configure eventdev %d", opt->dev_id);
		return ret;
	}

	/* q0 (ordered queue) configuration */
	struct rte_event_queue_conf q0_ordered_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.schedule_type = RTE_SCHED_TYPE_ORDERED,
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
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
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

	if (!evt_has_distributed_sched(opt->dev_id)) {
		uint32_t service_id;
		rte_event_dev_service_id_get(opt->dev_id, &service_id);
		ret = evt_service_setup(service_id);
		if (ret) {
			evt_err("No service lcore found to run event dev.");
			return ret;
		}
	}

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
	.launch_lcores      = order_queue_launch_lcores,
	.eventdev_destroy   = order_eventdev_destroy,
	.mempool_destroy    = order_mempool_destroy,
	.test_result        = order_test_result,
	.test_destroy       = order_test_destroy,
};

EVT_TEST_REGISTER(order_queue);
