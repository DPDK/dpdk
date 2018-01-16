/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Cavium, Inc.
 */

#include "test_pipeline_common.h"

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

int
pipeline_opt_check(struct evt_options *opt, uint64_t nb_queues)
{
	unsigned int lcores;
	/*
	 * N worker + 1 master
	 */
	lcores = 2;

	if (!rte_eth_dev_count()) {
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
	int i;
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
	if (!rte_eth_dev_count()) {
		evt_err("No ethernet ports found.\n");
		return -ENODEV;
	}

	for (i = 0; i < rte_eth_dev_count(); i++) {
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
		rte_eth_promiscuous_enable(i);
	}

	return 0;
}

void
pipeline_ethdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	int i;
	RTE_SET_USED(test);
	RTE_SET_USED(opt);

	for (i = 0; i < rte_eth_dev_count(); i++) {
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
