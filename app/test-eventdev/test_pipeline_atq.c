/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Cavium, Inc.
 */

#include "test_pipeline_common.h"

/* See http://dpdk.org/doc/guides/tools/testeventdev.html for test details */

static __rte_always_inline int
pipeline_atq_nb_event_queues(struct evt_options *opt)
{
	RTE_SET_USED(opt);

	return rte_eth_dev_count_avail();
}

static int
pipeline_atq_worker_single_stage_tx(void *arg)
{
	PIPELINE_WROKER_SINGLE_STAGE_INIT;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
			pipeline_tx_pkt(ev.mbuf);
			w->processed_pkts++;
			continue;
		}
		pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		pipeline_event_enqueue(dev, port, &ev);
	}

	return 0;
}

static int
pipeline_atq_worker_single_stage_fwd(void *arg)
{
	PIPELINE_WROKER_SINGLE_STAGE_INIT;
	const uint8_t tx_queue = t->tx_service.queue_id;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		w->processed_pkts++;
		ev.queue_id = tx_queue;
		pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		pipeline_event_enqueue(dev, port, &ev);
	}

	return 0;
}

static int
pipeline_atq_worker_single_stage_burst_tx(void *arg)
{
	PIPELINE_WROKER_SINGLE_STAGE_BURST_INIT;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {

				pipeline_tx_pkt(ev[i].mbuf);
				ev[i].op = RTE_EVENT_OP_RELEASE;
				w->processed_pkts++;
			} else
				pipeline_fwd_event(&ev[i],
						RTE_SCHED_TYPE_ATOMIC);
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}

	return 0;
}

static int
pipeline_atq_worker_single_stage_burst_fwd(void *arg)
{
	PIPELINE_WROKER_SINGLE_STAGE_BURST_INIT;
	const uint8_t tx_queue = t->tx_service.queue_id;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			ev[i].queue_id = tx_queue;
			pipeline_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
			w->processed_pkts++;
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}

	return 0;
}

static int
pipeline_atq_worker_multi_stage_tx(void *arg)
{
	PIPELINE_WROKER_MULTI_STAGE_INIT;
	const uint8_t nb_stages = t->opt->nb_stages;


	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		cq_id = ev.sub_event_type % nb_stages;

		if (cq_id == last_queue) {
			if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {

				pipeline_tx_pkt(ev.mbuf);
				w->processed_pkts++;
				continue;
			}
			pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		} else {
			ev.sub_event_type++;
			pipeline_fwd_event(&ev, sched_type_list[cq_id]);
		}

		pipeline_event_enqueue(dev, port, &ev);
	}
	return 0;
}

static int
pipeline_atq_worker_multi_stage_fwd(void *arg)
{
	PIPELINE_WROKER_MULTI_STAGE_INIT;
	const uint8_t nb_stages = t->opt->nb_stages;
	const uint8_t tx_queue = t->tx_service.queue_id;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		cq_id = ev.sub_event_type % nb_stages;

		if (cq_id == last_queue) {
			w->processed_pkts++;
			ev.queue_id = tx_queue;
			pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		} else {
			ev.sub_event_type++;
			pipeline_fwd_event(&ev, sched_type_list[cq_id]);
		}

		pipeline_event_enqueue(dev, port, &ev);
	}
	return 0;
}

static int
pipeline_atq_worker_multi_stage_burst_tx(void *arg)
{
	PIPELINE_WROKER_MULTI_STAGE_BURST_INIT;
	const uint8_t nb_stages = t->opt->nb_stages;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			cq_id = ev[i].sub_event_type % nb_stages;

			if (cq_id == last_queue) {
				if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {

					pipeline_tx_pkt(ev[i].mbuf);
					ev[i].op = RTE_EVENT_OP_RELEASE;
					w->processed_pkts++;
					continue;
				}

				pipeline_fwd_event(&ev[i],
						RTE_SCHED_TYPE_ATOMIC);
			} else {
				ev[i].sub_event_type++;
				pipeline_fwd_event(&ev[i],
						sched_type_list[cq_id]);
			}
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}
	return 0;
}

static int
pipeline_atq_worker_multi_stage_burst_fwd(void *arg)
{
	PIPELINE_WROKER_MULTI_STAGE_BURST_INIT;
	const uint8_t nb_stages = t->opt->nb_stages;
	const uint8_t tx_queue = t->tx_service.queue_id;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			cq_id = ev[i].sub_event_type % nb_stages;

			if (cq_id == last_queue) {
				w->processed_pkts++;
				ev[i].queue_id = tx_queue;
				pipeline_fwd_event(&ev[i],
						RTE_SCHED_TYPE_ATOMIC);
			} else {
				ev[i].sub_event_type++;
				pipeline_fwd_event(&ev[i],
						sched_type_list[cq_id]);
			}
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}
	return 0;
}

static int
worker_wrapper(void *arg)
{
	struct worker_data *w  = arg;
	struct evt_options *opt = w->t->opt;
	const bool burst = evt_has_burst_mode(w->dev_id);
	const bool mt_safe = !w->t->mt_unsafe;
	const uint8_t nb_stages = opt->nb_stages;
	RTE_SET_USED(opt);

	if (nb_stages == 1) {
		if (!burst && mt_safe)
			return pipeline_atq_worker_single_stage_tx(arg);
		else if (!burst && !mt_safe)
			return pipeline_atq_worker_single_stage_fwd(arg);
		else if (burst && mt_safe)
			return pipeline_atq_worker_single_stage_burst_tx(arg);
		else if (burst && !mt_safe)
			return pipeline_atq_worker_single_stage_burst_fwd(arg);
	} else {
		if (!burst && mt_safe)
			return pipeline_atq_worker_multi_stage_tx(arg);
		else if (!burst && !mt_safe)
			return pipeline_atq_worker_multi_stage_fwd(arg);
		if (burst && mt_safe)
			return pipeline_atq_worker_multi_stage_burst_tx(arg);
		else if (burst && !mt_safe)
			return pipeline_atq_worker_multi_stage_burst_fwd(arg);
	}
	rte_panic("invalid worker\n");
}

static int
pipeline_atq_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	struct test_pipeline *t = evt_test_priv(test);

	if (t->mt_unsafe)
		rte_service_component_runstate_set(t->tx_service.service_id, 1);
	return pipeline_launch_lcores(test, opt, worker_wrapper);
}

static int
pipeline_atq_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	int ret;
	int nb_ports;
	int nb_queues;
	uint8_t queue;
	struct rte_event_dev_info info;
	struct test_pipeline *t = evt_test_priv(test);
	uint8_t tx_evqueue_id = 0;
	uint8_t queue_arr[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t nb_worker_queues = 0;

	nb_ports = evt_nr_active_lcores(opt->wlcores);
	nb_queues = rte_eth_dev_count_avail();

	/* One extra port and queueu for Tx service */
	if (t->mt_unsafe) {
		tx_evqueue_id = nb_queues;
		nb_ports++;
		nb_queues++;
	}


	rte_event_dev_info_get(opt->dev_id, &info);

	const struct rte_event_dev_config config = {
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_events_limit  = info.max_num_events,
			.nb_event_queue_flows = opt->nb_flows,
			.nb_event_port_dequeue_depth =
				info.max_event_port_dequeue_depth,
			.nb_event_port_enqueue_depth =
				info.max_event_port_enqueue_depth,
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
	for (queue = 0; queue < nb_queues; queue++) {
		q_conf.event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;

		if (t->mt_unsafe) {
			if (queue == tx_evqueue_id) {
				q_conf.event_queue_cfg =
					RTE_EVENT_QUEUE_CFG_SINGLE_LINK;
			} else {
				queue_arr[nb_worker_queues] = queue;
				nb_worker_queues++;
			}
		}

		ret = rte_event_queue_setup(opt->dev_id, queue, &q_conf);
		if (ret) {
			evt_err("failed to setup queue=%d", queue);
			return ret;
		}
	}

	if (opt->wkr_deq_dep > info.max_event_port_dequeue_depth)
		opt->wkr_deq_dep = info.max_event_port_dequeue_depth;

	/* port configuration */
	const struct rte_event_port_conf p_conf = {
			.dequeue_depth = opt->wkr_deq_dep,
			.enqueue_depth = info.max_event_port_dequeue_depth,
			.new_event_threshold = info.max_num_events,
	};

	if (t->mt_unsafe) {
		ret = pipeline_event_port_setup(test, opt, queue_arr,
				nb_worker_queues, p_conf);
		if (ret)
			return ret;

		ret = pipeline_event_tx_service_setup(test, opt, tx_evqueue_id,
				nb_ports - 1, p_conf);
	} else
		ret = pipeline_event_port_setup(test, opt, NULL, nb_queues,
				p_conf);

	if (ret)
		return ret;

	/*
	 * The pipelines are setup in the following manner:
	 *
	 * eth_dev_count = 2, nb_stages = 2, atq mode
	 *
	 * Multi thread safe :
	 *	queues = 2
	 *	stride = 1
	 *
	 *	event queue pipelines:
	 *	eth0 -> q0 ->tx
	 *	eth1 -> q1 ->tx
	 *
	 *	q0, q1 are configured as ATQ so, all the different stages can
	 *	be enqueued on the same queue.
	 *
	 * Multi thread unsafe :
	 *	queues = 3
	 *	stride = 1
	 *
	 *	event queue pipelines:
	 *	eth0 -> q0
	 *		  } (q3->tx) Tx service
	 *	eth1 -> q1
	 *
	 *	q0,q1 are configured as stated above.
	 *	q3 configured as SINGLE_LINK|ATOMIC.
	 */
	ret = pipeline_event_rx_adapter_setup(opt, 1, p_conf);
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
pipeline_atq_opt_dump(struct evt_options *opt)
{
	pipeline_opt_dump(opt, pipeline_atq_nb_event_queues(opt));
}

static int
pipeline_atq_opt_check(struct evt_options *opt)
{
	return pipeline_opt_check(opt, pipeline_atq_nb_event_queues(opt));
}

static bool
pipeline_atq_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < pipeline_atq_nb_event_queues(opt) ||
			dev_info.max_event_ports <
			evt_nr_active_lcores(opt->wlcores)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d",
			pipeline_atq_nb_event_queues(opt),
			dev_info.max_event_queues,
			evt_nr_active_lcores(opt->wlcores),
			dev_info.max_event_ports);
	}

	return true;
}

static const struct evt_test_ops pipeline_atq =  {
	.cap_check          = pipeline_atq_capability_check,
	.opt_check          = pipeline_atq_opt_check,
	.opt_dump           = pipeline_atq_opt_dump,
	.test_setup         = pipeline_test_setup,
	.mempool_setup      = pipeline_mempool_setup,
	.ethdev_setup	    = pipeline_ethdev_setup,
	.eventdev_setup     = pipeline_atq_eventdev_setup,
	.launch_lcores      = pipeline_atq_launch_lcores,
	.eventdev_destroy   = pipeline_eventdev_destroy,
	.mempool_destroy    = pipeline_mempool_destroy,
	.ethdev_destroy	    = pipeline_ethdev_destroy,
	.test_result        = pipeline_test_result,
	.test_destroy       = pipeline_test_destroy,
};

EVT_TEST_REGISTER(pipeline_atq);
