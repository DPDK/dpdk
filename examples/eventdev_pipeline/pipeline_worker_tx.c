/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright 2017 Cavium, Inc.
 */

#include "pipeline_common.h"

static __rte_always_inline void
worker_fwd_event(struct rte_event *ev, uint8_t sched)
{
	ev->event_type = RTE_EVENT_TYPE_CPU;
	ev->op = RTE_EVENT_OP_FORWARD;
	ev->sched_type = sched;
}

static __rte_always_inline void
worker_event_enqueue(const uint8_t dev, const uint8_t port,
		struct rte_event *ev)
{
	while (rte_event_enqueue_burst(dev, port, ev, 1) != 1)
		rte_pause();
}

static __rte_always_inline void
worker_event_enqueue_burst(const uint8_t dev, const uint8_t port,
		struct rte_event *ev, const uint16_t nb_rx)
{
	uint16_t enq;

	enq = rte_event_enqueue_burst(dev, port, ev, nb_rx);
	while (enq < nb_rx) {
		enq += rte_event_enqueue_burst(dev, port,
						ev + enq, nb_rx - enq);
	}
}

static __rte_always_inline void
worker_tx_pkt(const uint8_t dev, const uint8_t port, struct rte_event *ev)
{
	exchange_mac(ev->mbuf);
	rte_event_eth_tx_adapter_txq_set(ev->mbuf, 0);
	while (!rte_event_eth_tx_adapter_enqueue(dev, port, ev, 1, 0))
		rte_pause();
}

/* Single stage pipeline workers */

static int
worker_do_tx_single(void *arg)
{
	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	size_t fwd = 0, received = 0, tx = 0;
	struct rte_event ev;

	while (!fdata->done) {

		if (!rte_event_dequeue_burst(dev, port, &ev, 1, 0)) {
			rte_pause();
			continue;
		}

		received++;

		if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
			worker_tx_pkt(dev, port, &ev);
			tx++;
		} else {
			work();
			ev.queue_id++;
			worker_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
			worker_event_enqueue(dev, port, &ev);
			fwd++;
		}
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);
	return 0;
}

static int
worker_do_tx_single_atq(void *arg)
{
	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	size_t fwd = 0, received = 0, tx = 0;
	struct rte_event ev;

	while (!fdata->done) {

		if (!rte_event_dequeue_burst(dev, port, &ev, 1, 0)) {
			rte_pause();
			continue;
		}

		received++;

		if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
			worker_tx_pkt(dev, port, &ev);
			tx++;
		} else {
			work();
			worker_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
			worker_event_enqueue(dev, port, &ev);
			fwd++;
		}
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);
	return 0;
}

static int
worker_do_tx_single_burst(void *arg)
{
	struct rte_event ev[BATCH_SIZE + 1];

	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	size_t fwd = 0, received = 0, tx = 0;

	while (!fdata->done) {
		uint16_t i;
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BATCH_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}
		received += nb_rx;

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {

				worker_tx_pkt(dev, port, &ev[i]);
				ev[i].op = RTE_EVENT_OP_RELEASE;
				tx++;

			} else {
				ev[i].queue_id++;
				worker_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
			}
			work();
		}

		worker_event_enqueue_burst(dev, port, ev, nb_rx);
		fwd += nb_rx;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);
	return 0;
}

static int
worker_do_tx_single_burst_atq(void *arg)
{
	struct rte_event ev[BATCH_SIZE + 1];

	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	size_t fwd = 0, received = 0, tx = 0;

	while (!fdata->done) {
		uint16_t i;
		uint16_t nb_rx = rte_event_dequeue_burst(dev, port, ev,
				BATCH_SIZE, 0);

		if (!nb_rx) {
			rte_pause();
			continue;
		}

		received += nb_rx;

		for (i = 0; i < nb_rx; i++) {
			rte_prefetch0(ev[i + 1].mbuf);
			if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {

				worker_tx_pkt(dev, port, &ev[i]);
				ev[i].op = RTE_EVENT_OP_RELEASE;
				tx++;
			} else
				worker_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
			work();
		}

		worker_event_enqueue_burst(dev, port, ev, nb_rx);
		fwd += nb_rx;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);
	return 0;
}

/* Multi stage Pipeline Workers */

static int
worker_do_tx(void *arg)
{
	struct rte_event ev;

	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	const uint8_t lst_qid = cdata.num_stages - 1;
	size_t fwd = 0, received = 0, tx = 0;


	while (!fdata->done) {

		if (!rte_event_dequeue_burst(dev, port, &ev, 1, 0)) {
			rte_pause();
			continue;
		}

		received++;
		const uint8_t cq_id = ev.queue_id % cdata.num_stages;

		if (cq_id >= lst_qid) {
			if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
				worker_tx_pkt(dev, port, &ev);
				tx++;
				continue;
			}

			worker_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
			ev.queue_id = (cq_id == lst_qid) ?
				cdata.next_qid[ev.queue_id] : ev.queue_id;
		} else {
			ev.queue_id = cdata.next_qid[ev.queue_id];
			worker_fwd_event(&ev, cdata.queue_type);
		}
		work();

		worker_event_enqueue(dev, port, &ev);
		fwd++;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);

	return 0;
}

static int
worker_do_tx_atq(void *arg)
{
	struct rte_event ev;

	struct worker_data *data = (struct worker_data *)arg;
	const uint8_t dev = data->dev_id;
	const uint8_t port = data->port_id;
	const uint8_t lst_qid = cdata.num_stages - 1;
	size_t fwd = 0, received = 0, tx = 0;

	while (!fdata->done) {

		if (!rte_event_dequeue_burst(dev, port, &ev, 1, 0)) {
			rte_pause();
			continue;
		}

		received++;
		const uint8_t cq_id = ev.sub_event_type % cdata.num_stages;

		if (cq_id == lst_qid) {
			if (ev.sched_type == RTE_SCHED_TYPE_ATOMIC) {
				worker_tx_pkt(dev, port, &ev);
				tx++;
				continue;
			}

			worker_fwd_event(&ev, RTE_SCHED_TYPE_ATOMIC);
		} else {
			ev.sub_event_type++;
			worker_fwd_event(&ev, cdata.queue_type);
		}
		work();

		worker_event_enqueue(dev, port, &ev);
		fwd++;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);

	return 0;
}

static int
worker_do_tx_burst(void *arg)
{
	struct rte_event ev[BATCH_SIZE];

	struct worker_data *data = (struct worker_data *)arg;
	uint8_t dev = data->dev_id;
	uint8_t port = data->port_id;
	uint8_t lst_qid = cdata.num_stages - 1;
	size_t fwd = 0, received = 0, tx = 0;

	while (!fdata->done) {
		uint16_t i;
		const uint16_t nb_rx = rte_event_dequeue_burst(dev, port,
				ev, BATCH_SIZE, 0);

		if (nb_rx == 0) {
			rte_pause();
			continue;
		}
		received += nb_rx;

		for (i = 0; i < nb_rx; i++) {
			const uint8_t cq_id = ev[i].queue_id % cdata.num_stages;

			if (cq_id >= lst_qid) {
				if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {
					worker_tx_pkt(dev, port, &ev[i]);
					tx++;
					ev[i].op = RTE_EVENT_OP_RELEASE;
					continue;
				}
				ev[i].queue_id = (cq_id == lst_qid) ?
					cdata.next_qid[ev[i].queue_id] :
					ev[i].queue_id;

				worker_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
			} else {
				ev[i].queue_id = cdata.next_qid[ev[i].queue_id];
				worker_fwd_event(&ev[i], cdata.queue_type);
			}
			work();
		}
		worker_event_enqueue_burst(dev, port, ev, nb_rx);

		fwd += nb_rx;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);

	return 0;
}

static int
worker_do_tx_burst_atq(void *arg)
{
	struct rte_event ev[BATCH_SIZE];

	struct worker_data *data = (struct worker_data *)arg;
	uint8_t dev = data->dev_id;
	uint8_t port = data->port_id;
	uint8_t lst_qid = cdata.num_stages - 1;
	size_t fwd = 0, received = 0, tx = 0;

	while (!fdata->done) {
		uint16_t i;

		const uint16_t nb_rx = rte_event_dequeue_burst(dev, port,
				ev, BATCH_SIZE, 0);

		if (nb_rx == 0) {
			rte_pause();
			continue;
		}
		received += nb_rx;

		for (i = 0; i < nb_rx; i++) {
			const uint8_t cq_id = ev[i].sub_event_type %
				cdata.num_stages;

			if (cq_id == lst_qid) {
				if (ev[i].sched_type == RTE_SCHED_TYPE_ATOMIC) {
					worker_tx_pkt(dev, port, &ev[i]);
					tx++;
					ev[i].op = RTE_EVENT_OP_RELEASE;
					continue;
				}

				worker_fwd_event(&ev[i], RTE_SCHED_TYPE_ATOMIC);
			} else {
				ev[i].sub_event_type++;
				worker_fwd_event(&ev[i], cdata.queue_type);
			}
			work();
		}

		worker_event_enqueue_burst(dev, port, ev, nb_rx);
		fwd += nb_rx;
	}

	if (!cdata.quiet)
		printf("  worker %u thread done. RX=%zu FWD=%zu TX=%zu\n",
				rte_lcore_id(), received, fwd, tx);

	return 0;
}

static int
setup_eventdev_worker_tx_enq(struct worker_data *worker_data)
{
	uint8_t i;
	const uint8_t atq = cdata.all_type_queues ? 1 : 0;
	const uint8_t dev_id = 0;
	const uint8_t nb_ports = cdata.num_workers;
	uint8_t nb_slots = 0;
	uint8_t nb_queues = rte_eth_dev_count_avail();

	/*
	 * In case where all type queues are not enabled, use queues equal to
	 * number of stages * eth_dev_count and one extra queue per pipeline
	 * for Tx.
	 */
	if (!atq) {
		nb_queues *= cdata.num_stages;
		nb_queues += rte_eth_dev_count_avail();
	}

	struct rte_event_dev_config config = {
			.nb_event_queues = nb_queues,
			.nb_event_ports = nb_ports,
			.nb_single_link_event_port_queues = 0,
			.nb_events_limit  = 4096,
			.nb_event_queue_flows = 1024,
			.nb_event_port_dequeue_depth = 128,
			.nb_event_port_enqueue_depth = 128,
	};
	struct rte_event_port_conf wkr_p_conf = {
			.dequeue_depth = cdata.worker_cq_depth,
			.enqueue_depth = 64,
			.new_event_threshold = 4096,
	};
	struct rte_event_queue_conf wkr_q_conf = {
			.schedule_type = cdata.queue_type,
			.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
			.nb_atomic_flows = 1024,
			.nb_atomic_order_sequences = 1024,
	};

	int ret, ndev = rte_event_dev_count();

	if (ndev < 1) {
		printf("%d: No Eventdev Devices Found\n", __LINE__);
		return -1;
	}


	struct rte_event_dev_info dev_info;
	ret = rte_event_dev_info_get(dev_id, &dev_info);
	printf("\tEventdev %d: %s\n", dev_id, dev_info.driver_name);

	if (dev_info.max_num_events < config.nb_events_limit)
		config.nb_events_limit = dev_info.max_num_events;
	if (dev_info.max_event_port_dequeue_depth <
			config.nb_event_port_dequeue_depth)
		config.nb_event_port_dequeue_depth =
				dev_info.max_event_port_dequeue_depth;
	if (dev_info.max_event_port_enqueue_depth <
			config.nb_event_port_enqueue_depth)
		config.nb_event_port_enqueue_depth =
				dev_info.max_event_port_enqueue_depth;

	ret = rte_event_dev_configure(dev_id, &config);
	if (ret < 0) {
		printf("%d: Error configuring device\n", __LINE__);
		return -1;
	}

	printf("  Stages:\n");
	for (i = 0; i < nb_queues; i++) {

		if (atq) {

			nb_slots = cdata.num_stages;
			wkr_q_conf.event_queue_cfg =
				RTE_EVENT_QUEUE_CFG_ALL_TYPES;
		} else {
			uint8_t slot;

			nb_slots = cdata.num_stages + 1;
			slot = i % nb_slots;
			wkr_q_conf.schedule_type = slot == cdata.num_stages ?
				RTE_SCHED_TYPE_ATOMIC : cdata.queue_type;
		}

		if (rte_event_queue_setup(dev_id, i, &wkr_q_conf) < 0) {
			printf("%d: error creating qid %d\n", __LINE__, i);
			return -1;
		}
		cdata.qid[i] = i;
		cdata.next_qid[i] = i+1;
		if (cdata.enable_queue_priorities) {
			const uint32_t prio_delta =
				(RTE_EVENT_DEV_PRIORITY_LOWEST) /
				nb_slots;

			/* higher priority for queues closer to tx */
			wkr_q_conf.priority =
				RTE_EVENT_DEV_PRIORITY_LOWEST - prio_delta *
				(i % nb_slots);
		}

		const char *type_str = "Atomic";
		switch (wkr_q_conf.schedule_type) {
		case RTE_SCHED_TYPE_ORDERED:
			type_str = "Ordered";
			break;
		case RTE_SCHED_TYPE_PARALLEL:
			type_str = "Parallel";
			break;
		}
		printf("\tStage %d, Type %s\tPriority = %d\n", i, type_str,
				wkr_q_conf.priority);
	}

	printf("\n");
	if (wkr_p_conf.new_event_threshold > config.nb_events_limit)
		wkr_p_conf.new_event_threshold = config.nb_events_limit;
	if (wkr_p_conf.dequeue_depth > config.nb_event_port_dequeue_depth)
		wkr_p_conf.dequeue_depth = config.nb_event_port_dequeue_depth;
	if (wkr_p_conf.enqueue_depth > config.nb_event_port_enqueue_depth)
		wkr_p_conf.enqueue_depth = config.nb_event_port_enqueue_depth;

	/* set up one port per worker, linking to all stage queues */
	for (i = 0; i < cdata.num_workers; i++) {
		struct worker_data *w = &worker_data[i];
		w->dev_id = dev_id;
		if (rte_event_port_setup(dev_id, i, &wkr_p_conf) < 0) {
			printf("Error setting up port %d\n", i);
			return -1;
		}

		if (rte_event_port_link(dev_id, i, NULL, NULL, 0)
				!= nb_queues) {
			printf("%d: error creating link for port %d\n",
					__LINE__, i);
			return -1;
		}
		w->port_id = i;
	}
	/*
	 * Reduce the load on ingress event queue by splitting the traffic
	 * across multiple event queues.
	 * for example, nb_stages =  2 and nb_ethdev = 2 then
	 *
	 *	nb_queues = (2 * 2) + 2 = 6 (non atq)
	 *	rx_stride = 3
	 *
	 * So, traffic is split across queue 0 and queue 3 since queue id for
	 * rx adapter is chosen <ethport_id> * <rx_stride> i.e in the above
	 * case eth port 0, 1 will inject packets into event queue 0, 3
	 * respectively.
	 *
	 * This forms two set of queue pipelines 0->1->2->tx and 3->4->5->tx.
	 */
	cdata.rx_stride = atq ? 1 : nb_slots;
	ret = rte_event_dev_service_id_get(dev_id,
				&fdata->evdev_service_id);
	if (ret != -ESRCH && ret != 0) {
		printf("Error getting the service ID\n");
		return -1;
	}
	rte_service_runstate_set(fdata->evdev_service_id, 1);
	rte_service_set_runstate_mapped_check(fdata->evdev_service_id, 0);

	if (rte_event_dev_start(dev_id) < 0)
		rte_exit(EXIT_FAILURE, "Error starting eventdev");

	return dev_id;
}


struct rx_adptr_services {
	uint16_t nb_rx_adptrs;
	uint32_t *rx_adpt_arr;
};

static int32_t
service_rx_adapter(void *arg)
{
	int i;
	struct rx_adptr_services *adptr_services = arg;

	for (i = 0; i < adptr_services->nb_rx_adptrs; i++)
		rte_service_run_iter_on_app_lcore(
				adptr_services->rx_adpt_arr[i], 1);
	return 0;
}

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static inline int
port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_rxconf rx_conf;
	static const struct rte_eth_conf port_conf_default = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_hf = ETH_RSS_IP |
					  ETH_RSS_TCP |
					  ETH_RSS_UDP,
			}
		}
	};
	const uint16_t rx_rings = 1, tx_rings = 1;
	const uint16_t rx_ring_size = 512, tx_ring_size = 512;
	struct rte_eth_conf port_conf = port_conf_default;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	rx_conf = dev_info.default_rxconf;
	rx_conf.offloads = port_conf.rxmode.offloads;

	port_conf.rx_adv_conf.rss_conf.rss_hf &=
		dev_info.flow_type_rss_offloads;
	if (port_conf.rx_adv_conf.rss_conf.rss_hf !=
			port_conf_default.rx_adv_conf.rss_conf.rss_hf) {
		printf("Port %u modified RSS hash function based on hardware support,"
			"requested:%#"PRIx64" configured:%#"PRIx64"\n",
			port,
			port_conf_default.rx_adv_conf.rss_conf.rss_hf,
			port_conf.rx_adv_conf.rss_conf.rss_hf);
	}

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, rx_ring_size,
				rte_eth_dev_socket_id(port), &rx_conf,
				mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf_default.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, tx_ring_size,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0) {
		printf("Failed to get MAC address (port %u): %s\n",
				port, rte_strerror(-retval));
		return retval;
	}

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			" %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned int)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	if (retval != 0)
		return retval;

	return 0;
}

static int
init_ports(uint16_t num_ports)
{
	uint16_t portid;

	if (!cdata.num_mbuf)
		cdata.num_mbuf = 16384 * num_ports;

	struct rte_mempool *mp = rte_pktmbuf_pool_create("packet_pool",
			/* mbufs */ cdata.num_mbuf,
			/* cache_size */ 512,
			/* priv_size*/ 0,
			/* data_room_size */ RTE_MBUF_DEFAULT_BUF_SIZE,
			rte_socket_id());

	RTE_ETH_FOREACH_DEV(portid)
		if (port_init(portid, mp) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu16 "\n",
					portid);

	return 0;
}

static void
init_adapters(uint16_t nb_ports)
{
	int i;
	int ret;
	uint8_t evdev_id = 0;
	struct rx_adptr_services *adptr_services = NULL;
	struct rte_event_dev_info dev_info;

	ret = rte_event_dev_info_get(evdev_id, &dev_info);
	adptr_services = rte_zmalloc(NULL, sizeof(struct rx_adptr_services), 0);

	struct rte_event_port_conf adptr_p_conf = {
		.dequeue_depth = cdata.worker_cq_depth,
		.enqueue_depth = 64,
		.new_event_threshold = 4096,
	};

	init_ports(nb_ports);
	if (adptr_p_conf.new_event_threshold > dev_info.max_num_events)
		adptr_p_conf.new_event_threshold = dev_info.max_num_events;
	if (adptr_p_conf.dequeue_depth > dev_info.max_event_port_dequeue_depth)
		adptr_p_conf.dequeue_depth =
			dev_info.max_event_port_dequeue_depth;
	if (adptr_p_conf.enqueue_depth > dev_info.max_event_port_enqueue_depth)
		adptr_p_conf.enqueue_depth =
			dev_info.max_event_port_enqueue_depth;

	struct rte_event_eth_rx_adapter_queue_conf queue_conf;
	memset(&queue_conf, 0, sizeof(queue_conf));
	queue_conf.ev.sched_type = cdata.queue_type;

	for (i = 0; i < nb_ports; i++) {
		uint32_t cap;
		uint32_t service_id;

		ret = rte_event_eth_rx_adapter_create(i, evdev_id,
				&adptr_p_conf);
		if (ret)
			rte_exit(EXIT_FAILURE,
					"failed to create rx adapter[%d]", i);

		ret = rte_event_eth_rx_adapter_caps_get(evdev_id, i, &cap);
		if (ret)
			rte_exit(EXIT_FAILURE,
					"failed to get event rx adapter "
					"capabilities");

		queue_conf.ev.queue_id = cdata.rx_stride ?
			(i * cdata.rx_stride)
			: (uint8_t)cdata.qid[0];

		ret = rte_event_eth_rx_adapter_queue_add(i, i, -1, &queue_conf);
		if (ret)
			rte_exit(EXIT_FAILURE,
					"Failed to add queues to Rx adapter");

		/* Producer needs to be scheduled. */
		if (!(cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT)) {
			ret = rte_event_eth_rx_adapter_service_id_get(i,
					&service_id);
			if (ret != -ESRCH && ret != 0) {
				rte_exit(EXIT_FAILURE,
				"Error getting the service ID for rx adptr\n");
			}

			rte_service_runstate_set(service_id, 1);
			rte_service_set_runstate_mapped_check(service_id, 0);

			adptr_services->nb_rx_adptrs++;
			adptr_services->rx_adpt_arr = rte_realloc(
					adptr_services->rx_adpt_arr,
					adptr_services->nb_rx_adptrs *
					sizeof(uint32_t), 0);
			adptr_services->rx_adpt_arr[
				adptr_services->nb_rx_adptrs - 1] =
				service_id;
		}

		ret = rte_event_eth_rx_adapter_start(i);
		if (ret)
			rte_exit(EXIT_FAILURE, "Rx adapter[%d] start failed",
					i);
	}

	/* We already know that Tx adapter has INTERNAL port cap*/
	ret = rte_event_eth_tx_adapter_create(cdata.tx_adapter_id, evdev_id,
			&adptr_p_conf);
	if (ret)
		rte_exit(EXIT_FAILURE, "failed to create tx adapter[%d]",
				cdata.tx_adapter_id);

	for (i = 0; i < nb_ports; i++) {
		ret = rte_event_eth_tx_adapter_queue_add(cdata.tx_adapter_id, i,
				-1);
		if (ret)
			rte_exit(EXIT_FAILURE,
					"Failed to add queues to Tx adapter");
	}

	ret = rte_event_eth_tx_adapter_start(cdata.tx_adapter_id);
	if (ret)
		rte_exit(EXIT_FAILURE, "Tx adapter[%d] start failed",
				cdata.tx_adapter_id);

	if (adptr_services->nb_rx_adptrs) {
		struct rte_service_spec service;

		memset(&service, 0, sizeof(struct rte_service_spec));
		snprintf(service.name, sizeof(service.name), "rx_service");
		service.callback = service_rx_adapter;
		service.callback_userdata = (void *)adptr_services;

		int32_t ret = rte_service_component_register(&service,
				&fdata->rxadptr_service_id);
		if (ret)
			rte_exit(EXIT_FAILURE,
				"Rx adapter service register failed");

		rte_service_runstate_set(fdata->rxadptr_service_id, 1);
		rte_service_component_runstate_set(fdata->rxadptr_service_id,
				1);
		rte_service_set_runstate_mapped_check(fdata->rxadptr_service_id,
				0);
	} else {
		memset(fdata->rx_core, 0, sizeof(unsigned int) * MAX_NUM_CORE);
		rte_free(adptr_services);
	}

	if (!adptr_services->nb_rx_adptrs && (dev_info.event_dev_cap &
			 RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED))
		fdata->cap.scheduler = NULL;
}

static void
worker_tx_enq_opt_check(void)
{
	int i;
	int ret;
	uint32_t cap = 0;
	uint8_t rx_needed = 0;
	uint8_t sched_needed = 0;
	struct rte_event_dev_info eventdev_info;

	memset(&eventdev_info, 0, sizeof(struct rte_event_dev_info));
	rte_event_dev_info_get(0, &eventdev_info);

	if (cdata.all_type_queues && !(eventdev_info.event_dev_cap &
				RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES))
		rte_exit(EXIT_FAILURE,
				"Event dev doesn't support all type queues\n");
	sched_needed = !(eventdev_info.event_dev_cap &
		RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED);

	RTE_ETH_FOREACH_DEV(i) {
		ret = rte_event_eth_rx_adapter_caps_get(0, i, &cap);
		if (ret)
			rte_exit(EXIT_FAILURE,
				"failed to get event rx adapter capabilities");
		rx_needed |=
			!(cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT);
	}

	if (cdata.worker_lcore_mask == 0 ||
			(rx_needed && cdata.rx_lcore_mask == 0) ||
			(sched_needed && cdata.sched_lcore_mask == 0)) {
		printf("Core part of pipeline was not assigned any cores. "
			"This will stall the pipeline, please check core masks "
			"(use -h for details on setting core masks):\n"
			"\trx: %"PRIu64"\n\tsched: %"PRIu64
			"\n\tworkers: %"PRIu64"\n", cdata.rx_lcore_mask,
			cdata.sched_lcore_mask, cdata.worker_lcore_mask);
		rte_exit(-1, "Fix core masks\n");
	}

	if (!sched_needed)
		memset(fdata->sched_core, 0,
				sizeof(unsigned int) * MAX_NUM_CORE);
	if (!rx_needed)
		memset(fdata->rx_core, 0,
				sizeof(unsigned int) * MAX_NUM_CORE);

	memset(fdata->tx_core, 0, sizeof(unsigned int) * MAX_NUM_CORE);
}

static worker_loop
get_worker_loop_single_burst(uint8_t atq)
{
	if (atq)
		return worker_do_tx_single_burst_atq;

	return worker_do_tx_single_burst;
}

static worker_loop
get_worker_loop_single_non_burst(uint8_t atq)
{
	if (atq)
		return worker_do_tx_single_atq;

	return worker_do_tx_single;
}

static worker_loop
get_worker_loop_burst(uint8_t atq)
{
	if (atq)
		return worker_do_tx_burst_atq;

	return worker_do_tx_burst;
}

static worker_loop
get_worker_loop_non_burst(uint8_t atq)
{
	if (atq)
		return worker_do_tx_atq;

	return worker_do_tx;
}

static worker_loop
get_worker_single_stage(bool burst)
{
	uint8_t atq = cdata.all_type_queues ? 1 : 0;

	if (burst)
		return get_worker_loop_single_burst(atq);

	return get_worker_loop_single_non_burst(atq);
}

static worker_loop
get_worker_multi_stage(bool burst)
{
	uint8_t atq = cdata.all_type_queues ? 1 : 0;

	if (burst)
		return get_worker_loop_burst(atq);

	return get_worker_loop_non_burst(atq);
}

void
set_worker_tx_enq_setup_data(struct setup_data *caps, bool burst)
{
	if (cdata.num_stages == 1)
		caps->worker = get_worker_single_stage(burst);
	else
		caps->worker = get_worker_multi_stage(burst);

	caps->check_opt = worker_tx_enq_opt_check;
	caps->scheduler = schedule_devices;
	caps->evdev_setup = setup_eventdev_worker_tx_enq;
	caps->adptr_setup = init_adapters;
}
