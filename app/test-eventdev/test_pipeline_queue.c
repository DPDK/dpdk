/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Cavium, Inc.
 */

#include "test_pipeline_common.h"

/* See http://doc.dpdk.org/guides/tools/testeventdev.html for test details */

static __rte_always_inline int
pipeline_queue_nb_event_queues(struct evt_options *opt)
{
	uint16_t eth_count = rte_eth_dev_count_avail();

	return (eth_count * opt->nb_stages) + eth_count;
}

static __rte_noinline int
pipeline_queue_worker_single_stage_tx(void *arg)
{
	PIPELINE_WORKER_SINGLE_STAGE_INIT;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
			pipeline_event_tx(dev, port, &ev);
			w->processed_pkts++;
		} else {
			ev.queue_id++;
			pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
			pipeline_event_enqueue(dev, port, &ev);
		}
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_single_stage_fwd(void *arg)
{
	PIPELINE_WORKER_SINGLE_STAGE_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		ev.queue_id = tx_queue[ev.mbuf->port];
		rte_event_eth_tx_adapter_txq_set(ev.mbuf, 0);
		pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		pipeline_event_enqueue(dev, port, &ev);
		w->processed_pkts++;
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_single_stage_burst_tx(void *arg)
{
	PIPELINE_WORKER_SINGLE_STAGE_BURST_INIT;

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
				pipeline_event_tx(dev, port, &ev[i]);
				ev[i].op = RTE_EVENT_OP_RELEASE;
				w->processed_pkts++;
			} else {
				ev[i].queue_id++;
				pipeline_fwd_event(&ev[i],
						RTE_SCHED_TYPE_ATOMIC);
			}
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_single_stage_burst_fwd(void *arg)
{
	PIPELINE_WORKER_SINGLE_STAGE_BURST_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			ev[i].queue_id = tx_queue[ev[i].mbuf->port];
			rte_event_eth_tx_adapter_txq_set(ev[i].mbuf, 0);
			pipeline_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
		w->processed_pkts += nb_rx;
	}

	return 0;
}


static __rte_noinline int
pipeline_queue_worker_multi_stage_tx(void *arg)
{
	PIPELINE_WORKER_MULTI_STAGE_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		cq_id = ev.queue_id % nb_stages;

		if (ev.queue_id == tx_queue[ev.mbuf->port]) {
			pipeline_event_tx(dev, port, &ev);
			w->processed_pkts++;
			continue;
		}

		ev.queue_id++;
		pipeline_fwd_event(&ev, cq_id != last_queue ?
				sched_type_list[cq_id] :
				RTE_SCHED_TYPE_ATOMIC);
		pipeline_event_enqueue(dev, port, &ev);
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_multi_stage_fwd(void *arg)
{
	PIPELINE_WORKER_MULTI_STAGE_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t event = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

		if (!event) {
			rte_pause();
			continue;
		}

		cq_id = ev.queue_id % nb_stages;

		if (cq_id == last_queue) {
			ev.queue_id = tx_queue[ev.mbuf->port];
			rte_event_eth_tx_adapter_txq_set(ev.mbuf, 0);
			pipeline_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
			w->processed_pkts++;
		} else {
			ev.queue_id++;
			pipeline_fwd_event(&ev, sched_type_list[cq_id]);
		}

		pipeline_event_enqueue(dev, port, &ev);
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_multi_stage_burst_tx(void *arg)
{
	PIPELINE_WORKER_MULTI_STAGE_BURST_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			cq_id = ev[i].queue_id % nb_stages;

			if (ev[i].queue_id == tx_queue[ev[i].mbuf->port]) {
				pipeline_event_tx(dev, port, &ev[i]);
				ev[i].op = RTE_EVENT_OP_RELEASE;
				w->processed_pkts++;
				continue;
			}

			ev[i].queue_id++;
			pipeline_fwd_event(&ev[i], cq_id != last_queue ?
					sched_type_list[cq_id] :
					RTE_SCHED_TYPE_ATOMIC);
		}

		pipeline_event_enqueue_burst(dev, port, ev, nb_rx);
	}

	return 0;
}

static __rte_noinline int
pipeline_queue_worker_multi_stage_burst_fwd(void *arg)
{
	PIPELINE_WORKER_MULTI_STAGE_BURST_INIT;
	const uint8_t *tx_queue = t->tx_evqueue_id;

	while (t->done == false) {
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BURST_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			cq_id = ev[i].queue_id % nb_stages;

			if (cq_id == last_queue) {
				ev[i].queue_id = tx_queue[ev[i].mbuf->port];
				rte_event_eth_tx_adapter_txq_set(ev[i].mbuf, 0);
				pipeline_fwd_event(&ev[i],
						RTE_SCHED_TYPE_ATOMIC);
				w->processed_pkts++;
			} else {
				ev[i].queue_id++;
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
	const bool internal_port = w->t->internal_port;
	const uint8_t nb_stages = opt->nb_stages;
	RTE_SET_USED(opt);

	if (nb_stages == 1) {
		if (!burst && internal_port)
			return pipeline_queue_worker_single_stage_tx(arg);
		else if (!burst && !internal_port)
			return pipeline_queue_worker_single_stage_fwd(arg);
		else if (burst && internal_port)
			return pipeline_queue_worker_single_stage_burst_tx(arg);
		else if (burst && !internal_port)
			return pipeline_queue_worker_single_stage_burst_fwd(
					arg);
	} else {
		if (!burst && internal_port)
			return pipeline_queue_worker_multi_stage_tx(arg);
		else if (!burst && !internal_port)
			return pipeline_queue_worker_multi_stage_fwd(arg);
		else if (burst && internal_port)
			return pipeline_queue_worker_multi_stage_burst_tx(arg);
		else if (burst && !internal_port)
			return pipeline_queue_worker_multi_stage_burst_fwd(arg);

	}
	rte_panic("invalid worker\n");
}

static int
pipeline_queue_launch_lcores(struct evt_test *test, struct evt_options *opt)
{
	return pipeline_launch_lcores(test, opt, worker_wrapper);
}

static int
pipeline_queue_eventdev_setup(struct evt_test *test, struct evt_options *opt)
{
	int ret;
	int nb_ports;
	int nb_queues;
	int nb_stages = opt->nb_stages;
	uint8_t queue;
	uint8_t tx_evport_id = 0;
	uint8_t tx_evqueue_id[RTE_MAX_ETHPORTS];
	uint8_t queue_arr[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t nb_worker_queues = 0;
	uint16_t prod = 0;
	struct rte_event_dev_info info;
	struct test_pipeline *t = evt_test_priv(test);

	nb_ports = evt_nr_active_lcores(opt->wlcores);
	nb_queues = rte_eth_dev_count_avail() * (nb_stages);

	/* One queue for Tx adapter per port */
	nb_queues += rte_eth_dev_count_avail();

	memset(tx_evqueue_id, 0, sizeof(uint8_t) * RTE_MAX_ETHPORTS);
	memset(queue_arr, 0, sizeof(uint8_t) * RTE_EVENT_MAX_QUEUES_PER_DEV);

	rte_event_dev_info_get(opt->dev_id, &info);
	ret = evt_configure_eventdev(opt, nb_queues, nb_ports);
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
		uint8_t slot;

		q_conf.event_queue_cfg = 0;
		slot = queue % (nb_stages + 1);
		if (slot == nb_stages) {
			q_conf.schedule_type = RTE_SCHED_TYPE_ATOMIC;
			if (!t->internal_port) {
				q_conf.event_queue_cfg =
					RTE_EVENT_QUEUE_CFG_SINGLE_LINK;
			}
			tx_evqueue_id[prod++] = queue;
		} else {
			q_conf.schedule_type = opt->sched_type_list[slot];
			queue_arr[nb_worker_queues] = queue;
			nb_worker_queues++;
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

	if (!t->internal_port) {
		ret = pipeline_event_port_setup(test, opt, queue_arr,
				nb_worker_queues, p_conf);
		if (ret)
			return ret;
	} else
		ret = pipeline_event_port_setup(test, opt, NULL, nb_queues,
				p_conf);

	if (ret)
		return ret;
	/*
	 * The pipelines are setup in the following manner:
	 *
	 * eth_dev_count = 2, nb_stages = 2.
	 *
	 *	queues = 6
	 *	stride = 3
	 *
	 *	event queue pipelines:
	 *	eth0 -> q0 -> q1 -> (q2->tx)
	 *	eth1 -> q3 -> q4 -> (q5->tx)
	 *
	 *	q2, q5 configured as ATOMIC | SINGLE_LINK
	 *
	 */
	ret = pipeline_event_rx_adapter_setup(opt, nb_stages + 1, p_conf);
	if (ret)
		return ret;

	ret = pipeline_event_tx_adapter_setup(opt, p_conf);
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

	/* Connect the tx_evqueue_id to the Tx adapter port */
	if (!t->internal_port) {
		RTE_ETH_FOREACH_DEV(prod) {
			ret = rte_event_eth_tx_adapter_event_port_get(prod,
					&tx_evport_id);
			if (ret) {
				evt_err("Unable to get Tx adptr[%d] evprt[%d]",
						prod, tx_evport_id);
				return ret;
			}

			if (rte_event_port_link(opt->dev_id, tx_evport_id,
						&tx_evqueue_id[prod],
						NULL, 1) != 1) {
				evt_err("Unable to link Tx adptr[%d] evprt[%d]",
						prod, tx_evport_id);
				return ret;
			}
		}
	}

	ret = rte_event_dev_start(opt->dev_id);
	if (ret) {
		evt_err("failed to start eventdev %d", opt->dev_id);
		return ret;
	}


	RTE_ETH_FOREACH_DEV(prod) {
		ret = rte_eth_dev_start(prod);
		if (ret) {
			evt_err("Ethernet dev [%d] failed to start."
					" Using synthetic producer", prod);
			return ret;
		}

	}

	RTE_ETH_FOREACH_DEV(prod) {
		ret = rte_event_eth_rx_adapter_start(prod);
		if (ret) {
			evt_err("Rx adapter[%d] start failed", prod);
			return ret;
		}

		ret = rte_event_eth_tx_adapter_start(prod);
		if (ret) {
			evt_err("Tx adapter[%d] start failed", prod);
			return ret;
		}
	}

	memcpy(t->tx_evqueue_id, tx_evqueue_id, sizeof(uint8_t) *
			RTE_MAX_ETHPORTS);

	return 0;
}

static void
pipeline_queue_opt_dump(struct evt_options *opt)
{
	pipeline_opt_dump(opt, pipeline_queue_nb_event_queues(opt));
}

static int
pipeline_queue_opt_check(struct evt_options *opt)
{
	return pipeline_opt_check(opt, pipeline_queue_nb_event_queues(opt));
}

static bool
pipeline_queue_capability_check(struct evt_options *opt)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	if (dev_info.max_event_queues < pipeline_queue_nb_event_queues(opt) ||
			dev_info.max_event_ports <
			evt_nr_active_lcores(opt->wlcores)) {
		evt_err("not enough eventdev queues=%d/%d or ports=%d/%d",
			pipeline_queue_nb_event_queues(opt),
			dev_info.max_event_queues,
			evt_nr_active_lcores(opt->wlcores),
			dev_info.max_event_ports);
	}

	return true;
}

static const struct evt_test_ops pipeline_queue =  {
	.cap_check          = pipeline_queue_capability_check,
	.opt_check          = pipeline_queue_opt_check,
	.opt_dump           = pipeline_queue_opt_dump,
	.test_setup         = pipeline_test_setup,
	.mempool_setup      = pipeline_mempool_setup,
	.ethdev_setup	    = pipeline_ethdev_setup,
	.eventdev_setup     = pipeline_queue_eventdev_setup,
	.launch_lcores      = pipeline_queue_launch_lcores,
	.eventdev_destroy   = pipeline_eventdev_destroy,
	.mempool_destroy    = pipeline_mempool_destroy,
	.ethdev_destroy	    = pipeline_ethdev_destroy,
	.test_result        = pipeline_test_result,
	.test_destroy       = pipeline_test_destroy,
};

EVT_TEST_REGISTER(pipeline_queue);
