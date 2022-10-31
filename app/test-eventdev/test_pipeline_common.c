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
	evt_dump("nb_eth_rx_queues", "%d", opt->eth_queues);
	evt_dump("event_vector", "%d", opt->ena_vector);
	if (opt->ena_vector) {
		evt_dump("vector_size", "%d", opt->vector_size);
		evt_dump("vector_tmo_ns", "%" PRIu64 "", opt->vector_tmo_nsec);
	}
}

static inline uint64_t
processed_pkts(struct test_pipeline *t)
{
	uint8_t i;
	uint64_t total = 0;

	for (i = 0; i < t->nb_workers; i++)
		total += t->worker[i].processed_pkts;

	return total;
}

/* RFC863 discard port */
#define UDP_SRC_PORT 9
#define UDP_DST_PORT 9

/* RFC2544 reserved test subnet 192.18.0.0 */
#define IP_SRC_ADDR(x, y) ((192U << 24) | (18 << 16) | ((x) << 8) | (y))
#define IP_DST_ADDR(x, y) ((192U << 24) | (18 << 16) | ((x) << 8) | (y))

#define IP_DEFTTL  64 /* from RFC 1340. */
#define IP_VERSION 0x40
#define IP_HDRLEN  0x05 /* default IP header length == five 32-bits words. */
#define IP_VHL_DEF (IP_VERSION | IP_HDRLEN)

static void
setup_pkt_udp_ip_headers(struct rte_ipv4_hdr *ip_hdr,
			 struct rte_udp_hdr *udp_hdr, uint16_t pkt_data_len,
			 uint8_t port, uint8_t flow)
{
	uint16_t *ptr16;
	uint32_t ip_cksum;
	uint16_t pkt_len;

	/*
	 * Initialize UDP header.
	 */
	pkt_len = (uint16_t)(pkt_data_len + sizeof(struct rte_udp_hdr));
	udp_hdr->src_port = rte_cpu_to_be_16(UDP_SRC_PORT);
	udp_hdr->dst_port = rte_cpu_to_be_16(UDP_DST_PORT);
	udp_hdr->dgram_len = rte_cpu_to_be_16(pkt_len);
	udp_hdr->dgram_cksum = 0; /* No UDP checksum. */

	/*
	 * Initialize IP header.
	 */
	pkt_len = (uint16_t)(pkt_len + sizeof(struct rte_ipv4_hdr));
	ip_hdr->version_ihl = IP_VHL_DEF;
	ip_hdr->type_of_service = 0;
	ip_hdr->fragment_offset = 0;
	ip_hdr->time_to_live = IP_DEFTTL;
	ip_hdr->next_proto_id = IPPROTO_UDP;
	ip_hdr->packet_id = 0;
	ip_hdr->total_length = rte_cpu_to_be_16(pkt_len);
	ip_hdr->src_addr = rte_cpu_to_be_32(IP_SRC_ADDR(port, 1));
	ip_hdr->dst_addr = rte_cpu_to_be_32(IP_DST_ADDR(port + 1, flow));

	/*
	 * Compute IP header checksum.
	 */
	ptr16 = (unaligned_uint16_t *)ip_hdr;
	ip_cksum = 0;
	ip_cksum += ptr16[0];
	ip_cksum += ptr16[1];
	ip_cksum += ptr16[2];
	ip_cksum += ptr16[3];
	ip_cksum += ptr16[4];
	ip_cksum += ptr16[6];
	ip_cksum += ptr16[7];
	ip_cksum += ptr16[8];
	ip_cksum += ptr16[9];

	/*
	 * Reduce 32 bit checksum to 16 bits and complement it.
	 */
	ip_cksum = ((ip_cksum & 0xFFFF0000) >> 16) + (ip_cksum & 0x0000FFFF);
	if (ip_cksum > 65535)
		ip_cksum -= 65535;
	ip_cksum = (~ip_cksum) & 0x0000FFFF;
	if (ip_cksum == 0)
		ip_cksum = 0xFFFF;
	ip_hdr->hdr_checksum = (uint16_t)ip_cksum;
}

static void
pipeline_tx_first(struct test_pipeline *t, struct evt_options *opt)
{
#define TX_DEF_PACKET_LEN 64
	uint16_t eth_port_id = 0;
	uint16_t pkt_sz, rc;
	uint32_t i;

	pkt_sz = opt->tx_pkt_sz;
	if (pkt_sz > opt->max_pkt_sz)
		pkt_sz = opt->max_pkt_sz;
	if (!pkt_sz)
		pkt_sz = TX_DEF_PACKET_LEN;

	RTE_ETH_FOREACH_DEV(eth_port_id) {
		struct rte_ether_addr src_mac;
		struct rte_ether_addr dst_mac;
		struct rte_ether_hdr eth_hdr;

		/* Send to the same dest.mac as port mac */
		rte_eth_macaddr_get(eth_port_id, &dst_mac);
		rte_eth_random_addr((uint8_t *)&src_mac);

		rte_ether_addr_copy(&dst_mac, &eth_hdr.dst_addr);
		rte_ether_addr_copy(&src_mac, &eth_hdr.src_addr);
		eth_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);

		for (i = 0; i < opt->tx_first; i++) {
			struct rte_udp_hdr *pkt_udp_hdr;
			struct rte_ipv4_hdr ip_hdr;
			struct rte_udp_hdr udp_hdr;
			struct rte_mbuf *mbuf;

			mbuf = rte_pktmbuf_alloc(
				opt->per_port_pool ? t->pool[i] : t->pool[0]);
			if (mbuf == NULL)
				continue;

			setup_pkt_udp_ip_headers(
				&ip_hdr, &udp_hdr,
				pkt_sz - sizeof(struct rte_ether_hdr) -
					sizeof(struct rte_ipv4_hdr) -
					sizeof(struct rte_udp_hdr),
				eth_port_id, i);
			mbuf->port = eth_port_id;
			mbuf->data_len = pkt_sz;
			mbuf->pkt_len = pkt_sz;

			/* Copy Ethernet header */
			rte_memcpy(rte_pktmbuf_mtod_offset(mbuf, char *, 0),
				   &eth_hdr, sizeof(struct rte_ether_hdr));

			/* Copy Ipv4 header */
			rte_memcpy(rte_pktmbuf_mtod_offset(
					   mbuf, char *,
					   sizeof(struct rte_ether_hdr)),
				   &ip_hdr, sizeof(struct rte_ipv4_hdr));

			/* Copy UDP header */
			rte_memcpy(
				rte_pktmbuf_mtod_offset(
					mbuf, char *,
					sizeof(struct rte_ipv4_hdr) +
						sizeof(struct rte_ether_hdr)),
				&udp_hdr, sizeof(struct rte_udp_hdr));
			pkt_udp_hdr = rte_pktmbuf_mtod_offset(
				mbuf, struct rte_udp_hdr *,
				sizeof(struct rte_ipv4_hdr) +
					sizeof(struct rte_ether_hdr));
			pkt_udp_hdr->src_port =
				rte_cpu_to_be_16(UDP_SRC_PORT + i);
			pkt_udp_hdr->dst_port =
				rte_cpu_to_be_16(UDP_SRC_PORT + i);

			rc = rte_eth_tx_burst(eth_port_id, 0, &mbuf, 1);
			if (rc == 0)
				rte_pktmbuf_free(mbuf);
		}
	}
}

int
pipeline_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *))
{
	struct test_pipeline *t = evt_test_priv(test);
	int ret, lcore_id;
	int port_idx = 0;

	if (opt->tx_first)
		pipeline_tx_first(t, opt);

	/* launch workers */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
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

	/* N worker + main */
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
	if (evt_lcores_has_overlap(opt->wlcores, rte_get_main_lcore())) {
		evt_err("worker lcores overlaps with main lcore");
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

	if (opt->prod_type != EVT_PROD_TYPE_ETH_RX_ADPTR) {
		evt_err("Invalid producer type, only --prod_type_ethdev is supported");
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
	uint16_t i, j;
	int ret;
	uint8_t nb_queues = 1;
	struct test_pipeline *t = evt_test_priv(test);
	struct rte_eth_rxconf rx_conf;
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = RTE_ETH_MQ_RX_RSS,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = RTE_ETH_RSS_IP,
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

	port_conf.rxmode.mtu = opt->max_pkt_sz - RTE_ETHER_HDR_LEN -
		RTE_ETHER_CRC_LEN;

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

		ret = rte_event_eth_rx_adapter_caps_get(opt->dev_id, i, &caps);
		if (ret != 0) {
			evt_err("failed to get event tx adapter[%d] caps", i);
			return ret;
		}

		if (!(caps & RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT))
			local_port_conf.rxmode.offloads |=
				RTE_ETH_RX_OFFLOAD_RSS_HASH;

		ret = rte_eth_dev_info_get(i, &dev_info);
		if (ret != 0) {
			evt_err("Error during getting device (port %u) info: %s\n",
				i, strerror(-ret));
			return ret;
		}

		/* Enable mbuf fast free if PMD has the capability. */
		if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
			local_port_conf.txmode.offloads |=
				RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

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

		if (rte_eth_dev_configure(i, opt->eth_queues, nb_queues,
					  &local_port_conf) < 0) {
			evt_err("Failed to configure eth port [%d]", i);
			return -EINVAL;
		}

		for (j = 0; j < opt->eth_queues; j++) {
			if (rte_eth_rx_queue_setup(
				    i, j, NB_RX_DESC, rte_socket_id(), &rx_conf,
				    opt->per_port_pool ? t->pool[i] :
							      t->pool[0]) < 0) {
				evt_err("Failed to setup eth port [%d] rx_queue: %d.",
					i, 0);
				return -EINVAL;
			}
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
	struct rte_mempool *vector_pool = NULL;
	struct rte_event_eth_rx_adapter_queue_conf queue_conf;

	memset(&queue_conf, 0,
			sizeof(struct rte_event_eth_rx_adapter_queue_conf));
	queue_conf.ev.sched_type = opt->sched_type_list[0];
	if (opt->ena_vector) {
		unsigned int nb_elem = (opt->pool_sz / opt->vector_size) << 1;

		nb_elem = RTE_MAX(512U, nb_elem);
		nb_elem += evt_nr_active_lcores(opt->wlcores) * 32;
		vector_pool = rte_event_vector_pool_create(
			"vector_pool", nb_elem, 32, opt->vector_size,
			opt->socket_id);
		if (vector_pool == NULL) {
			evt_err("failed to create event vector pool");
			return -ENOMEM;
		}
	}
	RTE_ETH_FOREACH_DEV(prod) {
		struct rte_event_eth_rx_adapter_vector_limits limits;
		uint32_t cap;

		ret = rte_event_eth_rx_adapter_caps_get(opt->dev_id,
				prod, &cap);
		if (ret) {
			evt_err("failed to get event rx adapter[%d]"
					" capabilities",
					opt->dev_id);
			return ret;
		}

		if (opt->ena_vector) {
			memset(&limits, 0, sizeof(limits));
			ret = rte_event_eth_rx_adapter_vector_limits_get(
				opt->dev_id, prod, &limits);
			if (ret) {
				evt_err("failed to get vector limits");
				return ret;
			}

			if (opt->vector_size < limits.min_sz ||
			    opt->vector_size > limits.max_sz) {
				evt_err("Vector size [%d] not within limits max[%d] min[%d]",
					opt->vector_size, limits.max_sz,
					limits.min_sz);
				return -EINVAL;
			}

			if (limits.log2_sz &&
			    !rte_is_power_of_2(opt->vector_size)) {
				evt_err("Vector size [%d] not power of 2",
					opt->vector_size);
				return -EINVAL;
			}

			if (opt->vector_tmo_nsec > limits.max_timeout_ns ||
			    opt->vector_tmo_nsec < limits.min_timeout_ns) {
				evt_err("Vector timeout [%" PRIu64
					"] not within limits max[%" PRIu64
					"] min[%" PRIu64 "]",
					opt->vector_tmo_nsec,
					limits.max_timeout_ns,
					limits.min_timeout_ns);
				return -EINVAL;
			}

			if (cap & RTE_EVENT_ETH_RX_ADAPTER_CAP_EVENT_VECTOR) {
				queue_conf.vector_sz = opt->vector_size;
				queue_conf.vector_timeout_ns =
					opt->vector_tmo_nsec;
				queue_conf.rx_queue_flags |=
				RTE_EVENT_ETH_RX_ADAPTER_QUEUE_EVENT_VECTOR;
				queue_conf.vector_mp = vector_pool;
			} else {
				evt_err("Rx adapter doesn't support event vector");
				return -EINVAL;
			}
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

		if (opt->ena_vector) {
			if (!(cap &
			      RTE_EVENT_ETH_TX_ADAPTER_CAP_EVENT_VECTOR)) {
				evt_err("Tx adapter doesn't support event vector");
				return -EINVAL;
			}
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

static void
pipeline_vector_array_free(struct rte_event events[], uint16_t num)
{
	uint16_t i;

	for (i = 0; i < num; i++) {
		rte_pktmbuf_free_bulk(
			&events[i].vec->mbufs[events[i].vec->elem_offset],
			events[i].vec->nb_elem);
		rte_mempool_put(rte_mempool_from_obj(events[i].vec),
				events[i].vec);
	}
}

static void
pipeline_event_port_flush(uint8_t dev_id __rte_unused, struct rte_event ev,
			  void *args __rte_unused)
{
	if (ev.event_type & RTE_EVENT_TYPE_VECTOR)
		pipeline_vector_array_free(&ev, 1);
	else
		rte_pktmbuf_free(ev.mbuf);
}

void
pipeline_worker_cleanup(uint8_t dev, uint8_t port, struct rte_event ev[],
			uint16_t enq, uint16_t deq)
{
	int i;

	if (deq) {
		for (i = enq; i < deq; i++) {
			if (ev[i].op == RTE_EVENT_OP_RELEASE)
				continue;
			if (ev[i].event_type & RTE_EVENT_TYPE_VECTOR)
				pipeline_vector_array_free(&ev[i], 1);
			else
				rte_pktmbuf_free(ev[i].mbuf);
		}

		for (i = 0; i < deq; i++)
			ev[i].op = RTE_EVENT_OP_RELEASE;

		rte_event_enqueue_burst(dev, port, ev, deq);
	}

	rte_event_port_quiesce(dev, port, pipeline_event_port_flush, NULL);
}

void
pipeline_ethdev_rx_stop(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i, j;
	RTE_SET_USED(test);

	if (opt->prod_type == EVT_PROD_TYPE_ETH_RX_ADPTR) {
		RTE_ETH_FOREACH_DEV(i) {
			rte_event_eth_rx_adapter_stop(i);
			rte_event_eth_rx_adapter_queue_del(i, i, -1);
			for (j = 0; j < opt->eth_queues; j++)
				rte_eth_dev_rx_queue_stop(i, j);
		}
	}
}

void
pipeline_ethdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	RTE_SET_USED(test);
	RTE_SET_USED(opt);

	RTE_ETH_FOREACH_DEV(i) {
		rte_event_eth_tx_adapter_stop(i);
		rte_event_eth_tx_adapter_queue_del(i, i, -1);
		rte_eth_dev_tx_queue_stop(i, 0);
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
		if (opt->per_port_pool) {
			char name[RTE_MEMPOOL_NAMESIZE];

			snprintf(name, RTE_MEMPOOL_NAMESIZE, "%s-%d",
				 test->name, i);
			t->pool[i] = rte_pktmbuf_pool_create(
				name,	      /* mempool name */
				opt->pool_sz, /* number of elements*/
				0,	      /* cache size*/
				0, opt->mbuf_sz, opt->socket_id); /* flags */

			if (t->pool[i] == NULL) {
				evt_err("failed to create mempool %s", name);
				return -ENOMEM;
			}
		}
	}

	if (!opt->per_port_pool) {
		t->pool[0] = rte_pktmbuf_pool_create(
			test->name,   /* mempool name */
			opt->pool_sz, /* number of elements*/
			0,	      /* cache size*/
			0, opt->mbuf_sz, opt->socket_id); /* flags */

		if (t->pool[0] == NULL) {
			evt_err("failed to create mempool");
			return -ENOMEM;
		}
	}

	return 0;
}

void
pipeline_mempool_destroy(struct evt_test *test, struct evt_options *opt)
{
	struct test_pipeline *t = evt_test_priv(test);
	int i;

	RTE_SET_USED(opt);
	if (opt->per_port_pool) {
		RTE_ETH_FOREACH_DEV(i)
			rte_mempool_free(t->pool[i]);
	} else {
		rte_mempool_free(t->pool[0]);
	}
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
