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

	evt_info("Packet distribution across worker cores :");
	for (i = 0; i < t->nb_workers; i++)
		total += t->worker[i].processed_pkts;
	for (i = 0; i < t->nb_workers; i++)
		evt_info("Worker %d packets: "CLGRN"%"PRIx64""CLNRM" percentage:"
				CLGRN" %3.2f"CLNRM, i,
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

	if (opt->prod_type != EVT_PROD_TYPE_ETH_RX_ADPTR) {
		evt_err("Invalid producer type '%s' valid producer '%s'",
			evt_prod_id_to_name(opt->prod_type),
			evt_prod_id_to_name(EVT_PROD_TYPE_ETH_RX_ADPTR));
		return -1;
	}

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
	int ret;
	uint8_t nb_queues = 1;
	struct test_pipeline *t = evt_test_priv(test);
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_IP,
			},
		},
	};

	if (!rte_eth_dev_count_avail()) {
		evt_err("No ethernet ports found.");
		return -ENODEV;
	}

	if (opt->max_pkt_sz < RTE_ETHER_MIN_LEN) {
		evt_err("max_pkt_sz can not be less than %d",
			RTE_ETHER_MIN_LEN);
		return -EINVAL;
	}

	port_conf.rxmode.max_rx_pkt_len = opt->max_pkt_sz;
	if (opt->max_pkt_sz > RTE_ETHER_MAX_LEN)
		port_conf.rxmode.offloads |= DEV_RX_OFFLOAD_JUMBO_FRAME;

	t->internal_port = 1;
	RTE_ETH_FOREACH_DEV(i) {
		struct rte_eth_dev_info dev_info;
		struct rte_eth_conf local_port_conf = port_conf;
		uint32_t caps = 0;

		ret = rte_event_eth_tx_adapter_caps_get(opt->dev_id, i, &caps);
		if (ret != 0) {
			evt_err("failed to get event tx adapter[%d] caps", i);
			return ret;
		}

		if (!(caps & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT))
			t->internal_port = 0;

		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret != 0) {
			evt_err("Error during getting device (port %u) info: %s\n",
				i, strerror(-ret));
			return ret;
		}

		rx_conf = dev_info.default_rxconf;
		rx_conf.offloads = port_conf.rxmode.offloads;

		local_port_conf.rx_adv_conf.rss_conf.rss_hf &=
			dev_info.flow_type_rss_offloads;
		if (local_port_conf.rx_adv_conf.rss_conf.rss_hf !=
				port_conf.rx_adv_conf.rss_conf.rss_hf) {
			evt_info("Port %u modified RSS hash function based on hardware support,"
				"requested:%#"PRIx64" configured:%#"PRIx64"",
				i,
				port_conf.rx_adv_conf.rss_conf.rss_hf,
				local_port_conf.rx_adv_conf.rss_conf.rss_hf);
		}

		if (rte_eth_dev_configure(i, nb_queues, nb_queues,
					&local_port_conf)
				< 0) {
			evt_err("Failed to configure eth port [%d]", i);
			return -EINVAL;
		}

		if (rte_eth_rx_queue_setup(i, 0, NB_RX_DESC,
				rte_socket_id(), &rx_conf, t->pool) < 0) {
			evt_err("Failed to setup eth port [%d] rx_queue: %d.",
					i, 0);
			return -EINVAL;
		}
		if (rte_eth_tx_queue_setup(i, 0, NB_TX_DESC,
					rte_socket_id(), NULL) < 0) {
			evt_err("Failed to setup eth port [%d] tx_queue: %d.",
					i, 0);
			return -EINVAL;
		}

		ret = rte_eth_promiscuous_enable(i);
		if (ret != 0) {
			evt_err("Failed to enable promiscuous mode for eth port [%d]: %s",
				i, rte_strerror(-ret));
			return ret;
		}
	}

	return 0;
}

int
pipeline_event_port_setup(struct evt_test *test, struct evt_options *opt,
		uint8_t *queue_arr, uint8_t nb_queues,
		const struct rte_event_port_conf p_conf)
{
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

		if (rte_event_port_link(opt->dev_id, port, queue_arr, NULL,
					nb_queues) != nb_queues)
			goto link_fail;
	}

	return 0;

link_fail:
	evt_err("failed to link queues to port %d", port);
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
			uint32_t service_id = -1U;

			rte_event_eth_rx_adapter_service_id_get(prod,
					&service_id);
			ret = evt_service_setup(service_id);
			if (ret) {
				evt_err("Failed to setup service core"
						" for Rx adapter");
				return ret;
			}
		}

		evt_info("Port[%d] using Rx adapter[%d] configured", prod,
				prod);
	}

	return ret;
}

int
pipeline_event_tx_adapter_setup(struct evt_options *opt,
		struct rte_event_port_conf port_conf)
{
	int ret = 0;
	uint16_t consm;

	RTE_ETH_FOREACH_DEV(consm) {
		uint32_t cap;

		ret = rte_event_eth_tx_adapter_caps_get(opt->dev_id,
				consm, &cap);
		if (ret) {
			evt_err("failed to get event tx adapter[%d] caps",
					consm);
			return ret;
		}

		ret = rte_event_eth_tx_adapter_create(consm, opt->dev_id,
				&port_conf);
		if (ret) {
			evt_err("failed to create tx adapter[%d]", consm);
			return ret;
		}

		ret = rte_event_eth_tx_adapter_queue_add(consm, consm, -1);
		if (ret) {
			evt_err("failed to add tx queues to adapter[%d]",
					consm);
			return ret;
		}

		if (!(cap & RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT)) {
			uint32_t service_id = -1U;

			ret = rte_event_eth_tx_adapter_service_id_get(consm,
								   &service_id);
			if (ret != -ESRCH && ret != 0) {
				evt_err("Failed to get Tx adptr service ID");
				return ret;
			}
			ret = evt_service_setup(service_id);
			if (ret) {
				evt_err("Failed to setup service core"
						" for Tx adapter");
				return ret;
			}
		}

		evt_info("Port[%d] using Tx adapter[%d] Configured", consm,
				consm);
	}

	return ret;
}

void
pipeline_ethdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	RTE_SET_USED(test);
	RTE_SET_USED(opt);

	RTE_ETH_FOREACH_DEV(i) {
		rte_event_eth_rx_adapter_stop(i);
		rte_event_eth_tx_adapter_stop(i);
		rte_eth_dev_stop(i);
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
	int i, ret;

	if (!opt->mbuf_sz)
		opt->mbuf_sz = RTE_MBUF_DEFAULT_BUF_SIZE;

	if (!opt->max_pkt_sz)
		opt->max_pkt_sz = RTE_ETHER_MAX_LEN;

	RTE_ETH_FOREACH_DEV(i) {
		struct rte_eth_dev_info dev_info;
		uint16_t data_size = 0;

		memset(&dev_info, 0, sizeof(dev_info));
		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret != 0) {
			evt_err("Error during getting device (port %u) info: %s\n",
				i, strerror(-ret));
			return ret;
		}

		if (dev_info.rx_desc_lim.nb_mtu_seg_max != UINT16_MAX &&
				dev_info.rx_desc_lim.nb_mtu_seg_max != 0) {
			data_size = opt->max_pkt_sz /
				dev_info.rx_desc_lim.nb_mtu_seg_max;
			data_size += RTE_PKTMBUF_HEADROOM;

			if (data_size  > opt->mbuf_sz)
				opt->mbuf_sz = data_size;
		}
	}

	t->pool = rte_pktmbuf_pool_create(test->name, /* mempool name */
			opt->pool_sz, /* number of elements*/
			512, /* cache size*/
			0,
			opt->mbuf_sz,
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
