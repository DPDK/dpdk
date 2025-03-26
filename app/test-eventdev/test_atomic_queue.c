/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Ericsson AB
 */

#include <stdio.h>
#include <unistd.h>

#include "test_atomic_common.h"

#define NB_QUEUES 2
#define NB_STAGES 2

static rte_spinlock_t *atomic_locks;

static inline void
atomic_queue_process_stage_0(struct test_order *const t,
		struct rte_event *const ev,
		uint32_t nb_flows,
		uint32_t port)
{
	const uint32_t flow = *order_mbuf_flow_id(t, ev->mbuf);

	atomic_lock_verify(atomic_locks, 0, flow, nb_flows, t, port);

	ev->queue_id = 1;
	ev->op = RTE_EVENT_OP_FORWARD;
	ev->sched_type = RTE_SCHED_TYPE_ATOMIC;
	ev->event_type = RTE_EVENT_TYPE_CPU;

	atomic_spinlock_unlock(atomic_locks, 0, flow, nb_flows);
}

static inline void
atomic_queue_process_stage_1(struct test_order *const t,
		struct rte_event *const ev,
		uint32_t nb_flows,
		rte_spinlock_t *atomic_locks,
		uint32_t *const expected_flow_seq,
		RTE_ATOMIC(uint64_t) * const outstand_pkts,
		uint32_t port)
{
	const uint32_t flow = *order_mbuf_flow_id(t, ev->mbuf);
	const uint32_t seq = *order_mbuf_seqn(t, ev->mbuf);

	atomic_lock_verify(atomic_locks, 1, flow, nb_flows, t, port);

	/* compare the seqn against expected value */
	if (seq != expected_flow_seq[flow]) {
		evt_err("flow=%x seqn mismatch got=%x expected=%x", flow, seq,
				expected_flow_seq[flow]);
		t->err = true;
	}

	expected_flow_seq[flow]++;
	rte_pktmbuf_free(ev->mbuf);

	rte_atomic_fetch_sub_explicit(outstand_pkts, 1, rte_memory_order_relaxed);

	ev->op = RTE_EVENT_OP_RELEASE;

	atomic_spinlock_unlock(atomic_locks, 1, flow, nb_flows);
}

static int
atomic_queue_worker_burst(void *arg, bool flow_id_cap, uint32_t max_burst)
{
	ORDER_WORKER_INIT;
	struct rte_event ev[BURST_SIZE];
	uint16_t i;

	while (t->err == false) {

		uint16_t const nb_rx = rte_event_dequeue_burst(dev_id, port, ev, max_burst, 0);

		if (nb_rx == 0) {
			if (rte_atomic_load_explicit(outstand_pkts, rte_memory_order_relaxed) <= 0)
				break;
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			if (!flow_id_cap)
				order_flow_id_copy_from_mbuf(t, &ev[i]);

			switch (ev[i].queue_id) {
			case 0:
				atomic_queue_process_stage_0(t, &ev[i], nb_flows, port);
				break;
			case 1:
				atomic_queue_process_stage_1(t, &ev[i], nb_flows, atomic_locks,
						expected_flow_seq, outstand_pkts, port);
				break;
			default:
				order_process_stage_invalid(t, &ev[i]);
				break;
			}
		}

		uint16_t total_enq = 0;

		do {
			total_enq += rte_event_enqueue_burst(
					dev_id, port, ev + total_enq, nb_rx - total_enq);
		} while (total_enq < nb_rx);
	}

	return 0;
}

static int
worker_wrapper(void *arg)
{
	struct worker_data *w = arg;
	int max_burst = evt_has_burst_mode(w->dev_id) ? BURST_SIZE : 1;
	const bool flow_id_cap = evt_has_flow_id(w->dev_id);

	return atomic_queue_worker_burst(arg, flow_id_cap, max_burst);
}

static int
atomic_queue_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	int ret = atomic_launch_lcores(test, opt, worker_wrapper);
	rte_free(atomic_locks);
	return ret;
}

static int
atomic_queue_eventdev_setup(struct evt_test *test, struct evt_options *opt)
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

	/* q0 configuration */
	struct rte_event_queue_conf q0_atomic_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	ret = rte_event_queue_setup(opt->dev_id, 0, &q0_atomic_conf);
	if (ret) {
		evt_err("failed to setup queue0 eventdev %d err %d", opt->dev_id, ret);
		return ret;
	}

	/* q1 configuration */
	struct rte_event_queue_conf q1_atomic_conf = {
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.schedule_type = RTE_SCHED_TYPE_ATOMIC,
			.nb_atomic_flows = opt->nb_flows,
			.nb_atomic_order_sequences = opt->nb_flows,
	};
	ret = rte_event_queue_setup(opt->dev_id, 1, &q1_atomic_conf);
	if (ret) {
		evt_err("failed to setup queue0 eventdev %d err %d", opt->dev_id, ret);
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

	atomic_locks = atomic_init_locks(NB_STAGES, opt->nb_flows);
	if (atomic_locks == NULL)
		return -1;

	return 0;
}

static void
atomic_queue_opt_dump(struct evt_options *opt)
{
	order_opt_dump(opt);
	evt_dump("nb_evdev_queues", "%d", NB_QUEUES);
}

static bool
atomic_queue_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < NB_QUEUES ||
			dev_info.max_event_ports < order_nb_event_ports(opt)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d", NB_QUEUES,
				dev_info.max_event_queues, order_nb_event_ports(opt),
				dev_info.max_event_ports);
		return false;
	}

	return true;
}

static const struct evt_test_ops atomic_queue = {
	.cap_check        = atomic_queue_capability_check,
	.opt_check        = order_opt_check,
	.opt_dump         = atomic_queue_opt_dump,
	.test_setup       = order_test_setup,
	.mempool_setup    = order_mempool_setup,
	.eventdev_setup   = atomic_queue_eventdev_setup,
	.launch_lcores    = atomic_queue_launch_lcores,
	.eventdev_destroy = order_eventdev_destroy,
	.mempool_destroy  = order_mempool_destroy,
	.test_result      = order_test_result,
	.test_destroy     = order_test_destroy,
};

EVT_TEST_REGISTER(atomic_queue);
