/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Cavium, Inc.
 */

#include "test_pipeline_common.h"

static int32_t
pipeline_event_tx_burst_service_func(void *args)
{

	int i;
	struct tx_service_data *tx = args;
	const uint8_t dev = tx->dev_id;
	const uint8_t port = tx->port_id;
	struct rte_event ev[BURST_SIZE + 1];

	uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev, BURST_SIZE, 0);

	if (!nb_rx) {
		for (i = 0; i < tx->nb_ethports; i++)
			rte_eth_tx_buffer_flush(i, 0, tx->tx_buf[i]);
		return 0;
	}

	for (i = 0; i < nb_rx; i++) {
		struct rte_mbuf *m = ev[i].mbuf;
		rte_eth_tx_buffer(m->port, 0, tx->tx_buf[m->port], m);
	}
	tx->processed_pkts += nb_rx;

	return 0;
}

static int32_t
pipeline_event_tx_service_func(void *args)
{

	int i;
	struct tx_service_data *tx = args;
	const uint8_t dev = tx->dev_id;
	const uint8_t port = tx->port_id;
	struct rte_event ev;

	uint16_t nb_rx = rte_event_dequeue_burst(dev, port, &ev, 1, 0);

	if (!nb_rx) {
		for (i = 0; i < tx->nb_ethports; i++)
			rte_eth_tx_buffer_flush(i, 0, tx->tx_buf[i]);
		return 0;
	}

	struct rte_mbuf *m = ev.mbuf;
	rte_eth_tx_buffer(m->port, 0, tx->tx_buf[m->port], m);
	tx->processed_pkts++;

	return 0;
}

int
pipeline_test_result(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	int i;
	uint64_t total = 0;
	struct test_pipeline *t = evt_test_priv(test);

	printf("Packet distribution across worker cores :\n");
	for (i = 0; i < t->nb_workers; i++)
		total += t->worker[i].processed_pkts;
	for (i = 0; i < t->nb_workers; i++)
		printf("Worker %d packets: "CLGRN"%"PRIx64" "CLNRM"percentage:"
				CLGRN" %3.2f\n"CLNRM, i,
				t->worker[i].processed_pkts,
				(((double)t->worker[i].processed_pkts)/total)
				* 100);
	return t->result;
}

void
pipeline_opt_dump(struct evt_options *opt, uint8_t nb_queues)
{
	evt_dump("nb_worker_lcores", "%d", evt_nr_active_lcores(opt->wlcores));
	evt_dump_worker_lcores(opt);
	evt_dump_nb_stages(opt);
	evt_dump("nb_evdev_ports", "%d", pipeline_nb_event_ports(opt));
	evt_dump("nb_evdev_queues", "%d", nb_queues);
	evt_dump_queue_priority(opt);
	evt_dump_sched_type_list(opt);
	evt_dump_producer_type(opt);
}

static inline uint64_t
processed_pkts(struct test_pipeline *t)
{
	uint8_t i;
	uint64_t total = 0;

	rte_smp_rmb();
	if (t->mt_unsafe)
		total = t->tx_service.processed_pkts;
	else
		for (i = 0; i < t->nb_workers; i++)
			total += t->worker[i].processed_pkts;

	return total;
}

int
pipeline_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *))
{
	int ret, lcore_id;
	struct test_pipeline *t = evt_test_priv(test);

	int port_idx = 0;
	/* launch workers */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (!(opt->wlcores[lcore_id]))
			continue;

		ret = rte_eal_remote_launch(worker,
				 &t->worker[port_idx], lcore_id);
		if (ret) {
			evt_err("failed to launch worker %d", lcore_id);
			return ret;
		}
		port_idx++;
	}

	uint64_t perf_cycles = rte_get_timer_cycles();
	const uint64_t perf_sample = rte_get_timer_hz();

	static float total_mpps;
	static uint64_t samples;

	uint64_t prev_pkts = 0;

	while (t->done == false) {
		const uint64_t new_cycles = rte_get_timer_cycles();

		if ((new_cycles - perf_cycles) > perf_sample) {
			const uint64_t curr_pkts = processed_pkts(t);

			float mpps = (float)(curr_pkts - prev_pkts)/1000000;

			prev_pkts = curr_pkts;
			perf_cycles = new_cycles;
			total_mpps += mpps;
			++samples;
			printf(CLGRN"\r%.3f mpps avg %.3f mpps"CLNRM,
					mpps, total_mpps/samples);
			fflush(stdout);
		}
	}
	printf("\n");
	return 0;
}

int
pipeline_opt_check(struct evt_options *opt, uint64_t nb_queues)
{
	unsigned int lcores;
	/*
	 * N worker + 1 master
	 */
	lcores = 2;

	if (!rte_eth_dev_count_avail()) {
		evt_err("test needs minimum 1 ethernet dev");
		return -1;
	}

	if (rte_lcore_count() < lcores) {
		evt_err("test need minimum %d lcores", lcores);
		return -1;
	}

	/* Validate worker lcores */
	if (evt_lcores_has_overlap(opt->wlcores, rte_get_master_lcore())) {
		evt_err("worker lcores overlaps with master lcore");
		return -1;
	}
	if (evt_has_disabled_lcore(opt->wlcores)) {
		evt_err("one or more workers lcores are not enabled");
		return -1;
	}
	if (!evt_has_active_lcore(opt->wlcores)) {
		evt_err("minimum one worker is required");
		return -1;
	}

	if (nb_queues > EVT_MAX_QUEUES) {
		evt_err("number of queues exceeds %d", EVT_MAX_QUEUES);
		return -1;
	}
	if (pipeline_nb_event_ports(opt) > EVT_MAX_PORTS) {
		evt_err("number of ports exceeds %d", EVT_MAX_PORTS);
		return -1;
	}

	if (evt_has_invalid_stage(opt))
		return -1;

	if (evt_has_invalid_sched_type(opt))
		return -1;

	return 0;
}

#define NB_RX_DESC			128
#define NB_TX_DESC			512
int
pipeline_ethdev_setup(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	uint8_t nb_queues = 1;
	uint8_t mt_state = 0;
	struct test_pipeline *t = evt_test_priv(test);
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = ETHER_MAX_LEN,
			.offloads = DEV_RX_OFFLOAD_CRC_STRIP,
			.ignore_offload_bitfield = 1,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_IP,
			},
		},
	};

	RTE_SET_USED(opt);
	if (!rte_eth_dev_count_avail()) {
		evt_err("No ethernet ports found.\n");
		return -ENODEV;
	}

	RTE_ETH_FOREACH_DEV(i) {
		struct rte_eth_dev_info dev_info;

		memset(&dev_info, 0, sizeof(struct rte_eth_dev_info));
		rte_eth_dev_info_get(i, &dev_info);
		mt_state = !(dev_info.tx_offload_capa &
				DEV_TX_OFFLOAD_MT_LOCKFREE);
		rx_conf = dev_info.default_rxconf;
		rx_conf.offloads = port_conf.rxmode.offloads;

		if (rte_eth_dev_configure(i, nb_queues, nb_queues,
					&port_conf)
				< 0) {
			evt_err("Failed to configure eth port [%d]\n", i);
			return -EINVAL;
		}

		if (rte_eth_rx_queue_setup(i, 0, NB_RX_DESC,
				rte_socket_id(), &rx_conf, t->pool) < 0) {
			evt_err("Failed to setup eth port [%d] rx_queue: %d.\n",
					i, 0);
			return -EINVAL;
		}
		if (rte_eth_tx_queue_setup(i, 0, NB_TX_DESC,
					rte_socket_id(), NULL) < 0) {
			evt_err("Failed to setup eth port [%d] tx_queue: %d.\n",
					i, 0);
			return -EINVAL;
		}

		t->mt_unsafe |= mt_state;
		t->tx_service.tx_buf[i] =
			rte_malloc(NULL, RTE_ETH_TX_BUFFER_SIZE(BURST_SIZE), 0);
		if (t->tx_service.tx_buf[i] == NULL)
			rte_panic("Unable to allocate Tx buffer memory.");
		rte_eth_promiscuous_enable(i);
	}

	return 0;
}

int
pipeline_event_port_setup(struct evt_test *test, struct evt_options *opt,
		uint8_t *queue_arr, uint8_t nb_queues,
		const struct rte_event_port_conf p_conf)
{
	int i;
	int ret;
	uint8_t port;
	struct test_pipeline *t = evt_test_priv(test);


	/* setup one port per worker, linking to all queues */
	for (port = 0; port < evt_nr_active_lcores(opt->wlcores); port++) {
		struct worker_data *w = &t->worker[port];

		w->dev_id = opt->dev_id;
		w->port_id = port;
		w->t = t;
		w->processed_pkts = 0;

		ret = rte_event_port_setup(opt->dev_id, port, &p_conf);
		if (ret) {
			evt_err("failed to setup port %d", port);
			return ret;
		}

		if (queue_arr == NULL) {
			if (rte_event_port_link(opt->dev_id, port, NULL, NULL,
						0) != nb_queues)
				goto link_fail;
		} else {
			for (i = 0; i < nb_queues; i++) {
				if (rte_event_port_link(opt->dev_id, port,
						&queue_arr[i], NULL, 1) != 1)
					goto link_fail;
			}
		}
	}

	return 0;

link_fail:
	evt_err("failed to link all queues to port %d", port);
	return -EINVAL;
}

int
pipeline_event_rx_adapter_setup(struct evt_options *opt, uint8_t stride,
		struct rte_event_port_conf prod_conf)
{
	int ret = 0;
	uint16_t prod;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf;

	memset(&queue_conf, 0,
			sizeof(struct rte_event_eth_rx_adapter_queue_conf));
	queue_conf.ev.sched_type = opt->sched_type_list[0];
	RTE_ETH_FOREACH_DEV(prod) {
		uint32_t cap;

		ret = rte_event_eth_rx_adapter_caps_get(opt->dev_id,
				prod, &cap);
		if (ret) {
			evt_err("failed to get event rx adapter[%d]"
					" capabilities",
					opt->dev_id);
			return ret;
		}
		queue_conf.ev.queue_id = prod * stride;
		ret = rte_event_eth_rx_adapter_create(prod, opt->dev_id,
				&prod_conf);
		if (ret) {
			evt_err("failed to create rx adapter[%d]", prod);
			return ret;
		}
		ret = rte_event_eth_rx_adapter_queue_add(prod, prod, -1,
				&queue_conf);
		if (ret) {
			evt_err("failed to add rx queues to adapter[%d]", prod);
			return ret;
		}

		if (!(cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT)) {
			uint32_t service_id;

			rte_event_eth_rx_adapter_service_id_get(prod,
					&service_id);
			ret = evt_service_setup(service_id);
			if (ret) {
				evt_err("Failed to setup service core"
						" for Rx adapter\n");
				return ret;
			}
		}

		ret = rte_eth_dev_start(prod);
		if (ret) {
			evt_err("Ethernet dev [%d] failed to start."
					" Using synthetic producer", prod);
			return ret;
		}

		ret = rte_event_eth_rx_adapter_start(prod);
		if (ret) {
			evt_err("Rx adapter[%d] start failed", prod);
			return ret;
		}
		printf("%s: Port[%d] using Rx adapter[%d] started\n", __func__,
				prod, prod);
	}

	return ret;
}

int
pipeline_event_tx_service_setup(struct evt_test *test, struct evt_options *opt,
		uint8_t tx_queue_id, uint8_t tx_port_id,
		const struct rte_event_port_conf p_conf)
{
	int ret;
	struct rte_service_spec serv;
	struct test_pipeline *t = evt_test_priv(test);
	struct tx_service_data *tx = &t->tx_service;

	ret = rte_event_port_setup(opt->dev_id, tx_port_id, &p_conf);
	if (ret) {
		evt_err("failed to setup port %d", tx_port_id);
		return ret;
	}

	if (rte_event_port_link(opt->dev_id, tx_port_id, &tx_queue_id,
				NULL, 1) != 1) {
		evt_err("failed to link queues to port %d", tx_port_id);
		return -EINVAL;
	}

	tx->dev_id = opt->dev_id;
	tx->queue_id = tx_queue_id;
	tx->port_id = tx_port_id;
	tx->nb_ethports = rte_eth_dev_count_avail();
	tx->t = t;

	/* Register Tx service */
	memset(&serv, 0, sizeof(struct rte_service_spec));
	snprintf(serv.name, sizeof(serv.name), "Tx_service");

	if (evt_has_burst_mode(opt->dev_id))
		serv.callback = pipeline_event_tx_burst_service_func;
	else
		serv.callback = pipeline_event_tx_service_func;

	serv.callback_userdata = (void *)tx;
	ret = rte_service_component_register(&serv, &tx->service_id);
	if (ret) {
		evt_err("failed to register Tx service");
		return ret;
	}

	ret = evt_service_setup(tx->service_id);
	if (ret) {
		evt_err("Failed to setup service core for Tx service\n");
		return ret;
	}

	rte_service_runstate_set(tx->service_id, 1);

	return 0;
}


void
pipeline_ethdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	RTE_SET_USED(test);
	RTE_SET_USED(opt);
	struct test_pipeline *t = evt_test_priv(test);

	if (t->mt_unsafe) {
		rte_service_component_runstate_set(t->tx_service.service_id, 0);
		rte_service_runstate_set(t->tx_service.service_id, 0);
		rte_service_component_unregister(t->tx_service.service_id);
	}

	RTE_ETH_FOREACH_DEV(i) {
		rte_event_eth_rx_adapter_stop(i);
		rte_eth_dev_stop(i);
		rte_eth_dev_close(i);
	}
}

void
pipeline_eventdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(test);

	rte_event_dev_stop(opt->dev_id);
	rte_event_dev_close(opt->dev_id);
}

int
pipeline_mempool_setup(struct evt_test *test, struct evt_options *opt)
{
	struct test_pipeline *t = evt_test_priv(test);

	t->pool = rte_pktmbuf_pool_create(test->name, /* mempool name */
			opt->pool_sz, /* number of elements*/
			512, /* cache size*/
			0,
			RTE_MBUF_DEFAULT_BUF_SIZE,
			opt->socket_id); /* flags */

	if (t->pool == NULL) {
		evt_err("failed to create mempool");
		return -ENOMEM;
	}

	return 0;
}

void
pipeline_mempool_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_pipeline *t = evt_test_priv(test);

	rte_mempool_free(t->pool);
}

int
pipeline_test_setup(struct evt_test *test, struct evt_options *opt)
{
	void *test_pipeline;

	test_pipeline = rte_zmalloc_socket(test->name,
			sizeof(struct test_pipeline), RTE_CACHE_LINE_SIZE,
			opt->socket_id);
	if (test_pipeline  == NULL) {
		evt_err("failed to allocate test_pipeline memory");
		goto nomem;
	}
	test->test_priv = test_pipeline;

	struct test_pipeline *t = evt_test_priv(test);

	t->nb_workers = evt_nr_active_lcores(opt->wlcores);
	t->outstand_pkts = opt->nb_pkts * evt_nr_active_lcores(opt->wlcores);
	t->done = false;
	t->nb_flows = opt->nb_flows;
	t->result = EVT_TEST_FAILED;
	t->opt = opt;
	opt->prod_type = EVT_PROD_TYPE_ETH_RX_ADPTR;
	memcpy(t->sched_type_list, opt->sched_type_list,
			sizeof(opt->sched_type_list));
	return 0;
nomem:
	return -ENOMEM;
}

void
pipeline_test_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);

	rte_free(test->test_priv);
}
