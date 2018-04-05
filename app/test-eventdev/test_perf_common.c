/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include "test_perf_common.h"

int
perf_test_result(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	int i;
	uint64_t total = 0;
	struct test_perf *t = evt_test_priv(test);

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

static inline int
perf_producer(void *arg)
{
	struct prod_data *p  = arg;
	struct test_perf *t = p->t;
	struct evt_options *opt = t->opt;
	const uint8_t dev_id = p->dev_id;
	const uint8_t port = p->port_id;
	struct rte_mempool *pool = t->pool;
	const uint64_t nb_pkts = t->nb_pkts;
	const uint32_t nb_flows = t->nb_flows;
	uint32_t flow_counter = 0;
	uint64_t count = 0;
	struct perf_elt *m;
	struct rte_event ev;

	if (opt->verbose_level > 1)
		printf("%s(): lcore %d dev_id %d port=%d queue %d\n", __func__,
				rte_lcore_id(), dev_id, port, p->queue_id);

	ev.event = 0;
	ev.op = RTE_EVENT_OP_NEW;
	ev.queue_id = p->queue_id;
	ev.sched_type = t->opt->sched_type_list[0];
	ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	ev.event_type =  RTE_EVENT_TYPE_CPU;
	ev.sub_event_type = 0; /* stage 0 */

	while (count < nb_pkts && t->done == false) {
		if (rte_mempool_get(pool, (void **)&m) < 0)
			continue;

		ev.flow_id = flow_counter++ % nb_flows;
		ev.event_ptr = m;
		m->timestamp = rte_get_timer_cycles();
		while (rte_event_enqueue_burst(dev_id, port, &ev, 1) != 1) {
			if (t->done)
				break;
			rte_pause();
			m->timestamp = rte_get_timer_cycles();
		}
		count++;
	}

	return 0;
}

static inline int
perf_event_timer_producer(void *arg)
{
	struct prod_data *p  = arg;
	struct test_perf *t = p->t;
	struct evt_options *opt = t->opt;
	uint32_t flow_counter = 0;
	uint64_t count = 0;
	uint64_t arm_latency = 0;
	const uint8_t nb_timer_adptrs = opt->nb_timer_adptrs;
	const uint32_t nb_flows = t->nb_flows;
	const uint64_t nb_timers = opt->nb_timers;
	struct rte_mempool *pool = t->pool;
	struct perf_elt *m;
	struct rte_event_timer_adapter **adptr = t->timer_adptr;
	struct rte_event_timer tim;
	uint64_t timeout_ticks = opt->expiry_nsec / opt->timer_tick_nsec;

	memset(&tim, 0, sizeof(struct rte_event_timer));
	timeout_ticks = opt->optm_timer_tick_nsec ?
			(timeout_ticks * opt->timer_tick_nsec)
			/ opt->optm_timer_tick_nsec : timeout_ticks;
	timeout_ticks += timeout_ticks ? 0 : 1;
	tim.ev.event_type =  RTE_EVENT_TYPE_TIMER;
	tim.ev.op = RTE_EVENT_OP_NEW;
	tim.ev.sched_type = t->opt->sched_type_list[0];
	tim.ev.queue_id = p->queue_id;
	tim.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	tim.state = RTE_EVENT_TIMER_NOT_ARMED;
	tim.timeout_ticks = timeout_ticks;

	if (opt->verbose_level > 1)
		printf("%s(): lcore %d\n", __func__, rte_lcore_id());

	while (count < nb_timers && t->done == false) {
		if (rte_mempool_get(pool, (void **)&m) < 0)
			continue;

		m->tim = tim;
		m->tim.ev.flow_id = flow_counter++ % nb_flows;
		m->tim.ev.event_ptr = m;
		m->timestamp = rte_get_timer_cycles();
		while (rte_event_timer_arm_burst(
				adptr[flow_counter % nb_timer_adptrs],
				(struct rte_event_timer **)&m, 1) != 1) {
			if (t->done)
				break;
			rte_pause();
			m->timestamp = rte_get_timer_cycles();
		}
		arm_latency += rte_get_timer_cycles() - m->timestamp;
		count++;
	}
	fflush(stdout);
	rte_delay_ms(1000);
	printf("%s(): lcore %d Average event timer arm latency = %.3f us\n",
			__func__, rte_lcore_id(), (float)(arm_latency / count) /
			(rte_get_timer_hz() / 1000000));
	return 0;
}

static inline int
perf_event_timer_producer_burst(void *arg)
{
	int i;
	struct prod_data *p  = arg;
	struct test_perf *t = p->t;
	struct evt_options *opt = t->opt;
	uint32_t flow_counter = 0;
	uint64_t count = 0;
	uint64_t arm_latency = 0;
	const uint8_t nb_timer_adptrs = opt->nb_timer_adptrs;
	const uint32_t nb_flows = t->nb_flows;
	const uint64_t nb_timers = opt->nb_timers;
	struct rte_mempool *pool = t->pool;
	struct perf_elt *m[BURST_SIZE + 1] = {NULL};
	struct rte_event_timer_adapter **adptr = t->timer_adptr;
	struct rte_event_timer tim;
	uint64_t timeout_ticks = opt->expiry_nsec / opt->timer_tick_nsec;

	memset(&tim, 0, sizeof(struct rte_event_timer));
	timeout_ticks = opt->optm_timer_tick_nsec ?
			(timeout_ticks * opt->timer_tick_nsec)
			/ opt->optm_timer_tick_nsec : timeout_ticks;
	timeout_ticks += timeout_ticks ? 0 : 1;
	tim.ev.event_type =  RTE_EVENT_TYPE_TIMER;
	tim.ev.op = RTE_EVENT_OP_NEW;
	tim.ev.sched_type = t->opt->sched_type_list[0];
	tim.ev.queue_id = p->queue_id;
	tim.ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	tim.state = RTE_EVENT_TIMER_NOT_ARMED;
	tim.timeout_ticks = timeout_ticks;

	if (opt->verbose_level > 1)
		printf("%s(): lcore %d\n", __func__, rte_lcore_id());

	while (count < nb_timers && t->done == false) {
		if (rte_mempool_get_bulk(pool, (void **)m, BURST_SIZE) < 0)
			continue;
		for (i = 0; i < BURST_SIZE; i++) {
			rte_prefetch0(m[i + 1]);
			m[i]->tim = tim;
			m[i]->tim.ev.flow_id = flow_counter++ % nb_flows;
			m[i]->tim.ev.event_ptr = m[i];
			m[i]->timestamp = rte_get_timer_cycles();
		}
		rte_event_timer_arm_tmo_tick_burst(
				adptr[flow_counter % nb_timer_adptrs],
				(struct rte_event_timer **)m,
				tim.timeout_ticks,
				BURST_SIZE);
		arm_latency += rte_get_timer_cycles() - m[i - 1]->timestamp;
		count += BURST_SIZE;
	}
	fflush(stdout);
	rte_delay_ms(1000);
	printf("%s(): lcore %d Average event timer arm latency = %.3f us\n",
			__func__, rte_lcore_id(), (float)(arm_latency / count) /
			(rte_get_timer_hz() / 1000000));
	return 0;
}

static int
perf_producer_wrapper(void *arg)
{
	struct prod_data *p  = arg;
	struct test_perf *t = p->t;
	/* Launch the producer function only in case of synthetic producer. */
	if (t->opt->prod_type == EVT_PROD_TYPE_SYNT)
		return perf_producer(arg);
	else if (t->opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR &&
			!t->opt->timdev_use_burst)
		return perf_event_timer_producer(arg);
	else if (t->opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR &&
			t->opt->timdev_use_burst)
		return perf_event_timer_producer_burst(arg);
	return 0;
}

static inline uint64_t
processed_pkts(struct test_perf *t)
{
	uint8_t i;
	uint64_t total = 0;

	rte_smp_rmb();
	for (i = 0; i < t->nb_workers; i++)
		total += t->worker[i].processed_pkts;

	return total;
}

static inline uint64_t
total_latency(struct test_perf *t)
{
	uint8_t i;
	uint64_t total = 0;

	rte_smp_rmb();
	for (i = 0; i < t->nb_workers; i++)
		total += t->worker[i].latency;

	return total;
}


int
perf_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *))
{
	int ret, lcore_id;
	struct test_perf *t = evt_test_priv(test);

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

	/* launch producers */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (!(opt->plcores[lcore_id]))
			continue;

		ret = rte_eal_remote_launch(perf_producer_wrapper,
				&t->prod[port_idx], lcore_id);
		if (ret) {
			evt_err("failed to launch perf_producer %d", lcore_id);
			return ret;
		}
		port_idx++;
	}

	const uint64_t total_pkts = t->outstand_pkts;

	uint64_t dead_lock_cycles = rte_get_timer_cycles();
	int64_t dead_lock_remaining  =  total_pkts;
	const uint64_t dead_lock_sample = rte_get_timer_hz() * 5;

	uint64_t perf_cycles = rte_get_timer_cycles();
	int64_t perf_remaining  = total_pkts;
	const uint64_t perf_sample = rte_get_timer_hz();

	static float total_mpps;
	static uint64_t samples;

	const uint64_t freq_mhz = rte_get_timer_hz() / 1000000;
	int64_t remaining = t->outstand_pkts - processed_pkts(t);

	while (t->done == false) {
		const uint64_t new_cycles = rte_get_timer_cycles();

		if ((new_cycles - perf_cycles) > perf_sample) {
			const uint64_t latency = total_latency(t);
			const uint64_t pkts = processed_pkts(t);

			remaining = t->outstand_pkts - pkts;
			float mpps = (float)(perf_remaining-remaining)/1000000;

			perf_remaining = remaining;
			perf_cycles = new_cycles;
			total_mpps += mpps;
			++samples;
			if (opt->fwd_latency && pkts > 0) {
				printf(CLGRN"\r%.3f mpps avg %.3f mpps [avg fwd latency %.3f us] "CLNRM,
					mpps, total_mpps/samples,
					(float)(latency/pkts)/freq_mhz);
			} else {
				printf(CLGRN"\r%.3f mpps avg %.3f mpps"CLNRM,
					mpps, total_mpps/samples);
			}
			fflush(stdout);

			if (remaining <= 0) {
				t->result = EVT_TEST_SUCCESS;
				if (opt->prod_type == EVT_PROD_TYPE_SYNT ||
					opt->prod_type ==
					EVT_PROD_TYPE_EVENT_TIMER_ADPTR) {
					t->done = true;
					rte_smp_wmb();
					break;
				}
			}
		}

		if (new_cycles - dead_lock_cycles > dead_lock_sample &&
				opt->prod_type == EVT_PROD_TYPE_SYNT) {
			remaining = t->outstand_pkts - processed_pkts(t);
			if (dead_lock_remaining == remaining) {
				rte_event_dev_dump(opt->dev_id, stdout);
				evt_err("No schedules for seconds, deadlock");
				t->done = true;
				rte_smp_wmb();
				break;
			}
			dead_lock_remaining = remaining;
			dead_lock_cycles = new_cycles;
		}
	}
	printf("\n");
	return 0;
}

static int
perf_event_rx_adapter_setup(struct evt_options *opt, uint8_t stride,
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

static int
perf_event_timer_adapter_setup(struct test_perf *t)
{
	int i;
	int ret;
	struct rte_event_timer_adapter_info adapter_info;
	struct rte_event_timer_adapter *wl;
	uint8_t nb_producers = evt_nr_active_lcores(t->opt->plcores);
	uint8_t flags = RTE_EVENT_TIMER_ADAPTER_F_ADJUST_RES;

	if (nb_producers == 1)
		flags |= RTE_EVENT_TIMER_ADAPTER_F_SP_PUT;

	for (i = 0; i < t->opt->nb_timer_adptrs; i++) {
		struct rte_event_timer_adapter_conf config = {
			.event_dev_id = t->opt->dev_id,
			.timer_adapter_id = i,
			.timer_tick_ns = t->opt->timer_tick_nsec,
			.max_tmo_ns = t->opt->max_tmo_nsec,
			.nb_timers = 2 * 1024 * 1024,
			.flags = flags,
		};

		wl = rte_event_timer_adapter_create(&config);
		if (wl == NULL) {
			evt_err("failed to create event timer ring %d", i);
			return rte_errno;
		}

		memset(&adapter_info, 0,
				sizeof(struct rte_event_timer_adapter_info));
		rte_event_timer_adapter_get_info(wl, &adapter_info);
		t->opt->optm_timer_tick_nsec = adapter_info.min_resolution_ns;

		if (!(adapter_info.caps &
				RTE_EVENT_TIMER_ADAPTER_CAP_INTERNAL_PORT)) {
			uint32_t service_id;

			rte_event_timer_adapter_service_id_get(wl,
					&service_id);
			ret = evt_service_setup(service_id);
			if (ret) {
				evt_err("Failed to setup service core"
						" for timer adapter\n");
				return ret;
			}
			rte_service_runstate_set(service_id, 1);
		}

		ret = rte_event_timer_adapter_start(wl);
		if (ret) {
			evt_err("failed to Start event timer adapter %d", i);
			return ret;
		}
		t->timer_adptr[i] = wl;
	}
	return 0;
}

int
perf_event_dev_port_setup(struct evt_test *test, struct evt_options *opt,
				uint8_t stride, uint8_t nb_queues,
				const struct rte_event_port_conf *port_conf)
{
	struct test_perf *t = evt_test_priv(test);
	uint16_t port, prod;
	int ret = -1;

	/* setup one port per worker, linking to all queues */
	for (port = 0; port < evt_nr_active_lcores(opt->wlcores);
				port++) {
		struct worker_data *w = &t->worker[port];

		w->dev_id = opt->dev_id;
		w->port_id = port;
		w->t = t;
		w->processed_pkts = 0;
		w->latency = 0;

		ret = rte_event_port_setup(opt->dev_id, port, port_conf);
		if (ret) {
			evt_err("failed to setup port %d", port);
			return ret;
		}

		ret = rte_event_port_link(opt->dev_id, port, NULL, NULL, 0);
		if (ret != nb_queues) {
			evt_err("failed to link all queues to port %d", port);
			return -EINVAL;
		}
	}

	/* port for producers, no links */
	if (opt->prod_type == EVT_PROD_TYPE_ETH_RX_ADPTR) {
		for ( ; port < perf_nb_event_ports(opt); port++) {
			struct prod_data *p = &t->prod[port];
			p->t = t;
		}

		ret = perf_event_rx_adapter_setup(opt, stride, *port_conf);
		if (ret)
			return ret;
	} else if (opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR) {
		prod = 0;
		for ( ; port < perf_nb_event_ports(opt); port++) {
			struct prod_data *p = &t->prod[port];
			p->queue_id = prod * stride;
			p->t = t;
			prod++;
		}

		ret = perf_event_timer_adapter_setup(t);
		if (ret)
			return ret;
	} else {
		prod = 0;
		for ( ; port < perf_nb_event_ports(opt); port++) {
			struct prod_data *p = &t->prod[port];

			p->dev_id = opt->dev_id;
			p->port_id = port;
			p->queue_id = prod * stride;
			p->t = t;

			ret = rte_event_port_setup(opt->dev_id, port,
					port_conf);
			if (ret) {
				evt_err("failed to setup port %d", port);
				return ret;
			}
			prod++;
		}
	}

	return ret;
}

int
perf_opt_check(struct evt_options *opt, uint64_t nb_queues)
{
	unsigned int lcores;

	/* N producer + N worker + 1 master when producer cores are used
	 * Else N worker + 1 master when Rx adapter is used
	 */
	lcores = opt->prod_type == EVT_PROD_TYPE_SYNT ? 3 : 2;

	if (rte_lcore_count() < lcores) {
		evt_err("test need minimum %d lcores", lcores);
		return -1;
	}

	/* Validate worker lcores */
	if (evt_lcores_has_overlap(opt->wlcores, rte_get_master_lcore())) {
		evt_err("worker lcores overlaps with master lcore");
		return -1;
	}
	if (evt_lcores_has_overlap_multi(opt->wlcores, opt->plcores)) {
		evt_err("worker lcores overlaps producer lcores");
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

	if (opt->prod_type == EVT_PROD_TYPE_SYNT) {
		/* Validate producer lcores */
		if (evt_lcores_has_overlap(opt->plcores,
					rte_get_master_lcore())) {
			evt_err("producer lcores overlaps with master lcore");
			return -1;
		}
		if (evt_has_disabled_lcore(opt->plcores)) {
			evt_err("one or more producer lcores are not enabled");
			return -1;
		}
		if (!evt_has_active_lcore(opt->plcores)) {
			evt_err("minimum one producer is required");
			return -1;
		}
	}

	if (evt_has_invalid_stage(opt))
		return -1;

	if (evt_has_invalid_sched_type(opt))
		return -1;

	if (nb_queues > EVT_MAX_QUEUES) {
		evt_err("number of queues exceeds %d", EVT_MAX_QUEUES);
		return -1;
	}
	if (perf_nb_event_ports(opt) > EVT_MAX_PORTS) {
		evt_err("number of ports exceeds %d", EVT_MAX_PORTS);
		return -1;
	}

	/* Fixups */
	if ((opt->nb_stages == 1 &&
			opt->prod_type != EVT_PROD_TYPE_EVENT_TIMER_ADPTR) &&
			opt->fwd_latency) {
		evt_info("fwd_latency is valid when nb_stages > 1, disabling");
		opt->fwd_latency = 0;
	}

	if (opt->fwd_latency && !opt->q_priority) {
		evt_info("enabled queue priority for latency measurement");
		opt->q_priority = 1;
	}
	if (opt->nb_pkts == 0)
		opt->nb_pkts = INT64_MAX/evt_nr_active_lcores(opt->plcores);

	return 0;
}

void
perf_opt_dump(struct evt_options *opt, uint8_t nb_queues)
{
	evt_dump("nb_prod_lcores", "%d", evt_nr_active_lcores(opt->plcores));
	evt_dump_producer_lcores(opt);
	evt_dump("nb_worker_lcores", "%d", evt_nr_active_lcores(opt->wlcores));
	evt_dump_worker_lcores(opt);
	evt_dump_nb_stages(opt);
	evt_dump("nb_evdev_ports", "%d", perf_nb_event_ports(opt));
	evt_dump("nb_evdev_queues", "%d", nb_queues);
	evt_dump_queue_priority(opt);
	evt_dump_sched_type_list(opt);
	evt_dump_producer_type(opt);
}

void
perf_eventdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	int i;
	struct test_perf *t = evt_test_priv(test);

	if (opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR) {
		for (i = 0; i < opt->nb_timer_adptrs; i++)
			rte_event_timer_adapter_stop(t->timer_adptr[i]);
	}
	rte_event_dev_stop(opt->dev_id);
	rte_event_dev_close(opt->dev_id);
}

static inline void
perf_elt_init(struct rte_mempool *mp, void *arg __rte_unused,
	    void *obj, unsigned i __rte_unused)
{
	memset(obj, 0, mp->elt_size);
}

#define NB_RX_DESC			128
#define NB_TX_DESC			512
int
perf_ethdev_setup(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	struct test_perf *t = evt_test_priv(test);
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
			.max_rx_pkt_len = ETHER_MAX_LEN,
			.split_hdr_size = 0,
			.header_split   = 0,
			.hw_ip_checksum = 0,
			.hw_vlan_filter = 0,
			.hw_vlan_strip  = 0,
			.hw_vlan_extend = 0,
			.jumbo_frame    = 0,
			.hw_strip_crc   = 1,
		},
		.rx_adv_conf = {
			.rss_conf = {
				.rss_key = NULL,
				.rss_hf = ETH_RSS_IP,
			},
		},
	};

	if (opt->prod_type == EVT_PROD_TYPE_SYNT ||
			opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR)
		return 0;

	if (!rte_eth_dev_count_avail()) {
		evt_err("No ethernet ports found.");
		return -ENODEV;
	}

	RTE_ETH_FOREACH_DEV(i) {

		if (rte_eth_dev_configure(i, 1, 1,
					&port_conf)
				< 0) {
			evt_err("Failed to configure eth port [%d]", i);
			return -EINVAL;
		}

		if (rte_eth_rx_queue_setup(i, 0, NB_RX_DESC,
				rte_socket_id(), NULL, t->pool) < 0) {
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

		rte_eth_promiscuous_enable(i);
	}

	return 0;
}

void perf_ethdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	uint16_t i;
	RTE_SET_USED(test);

	if (opt->prod_type == EVT_PROD_TYPE_ETH_RX_ADPTR) {
		RTE_ETH_FOREACH_DEV(i) {
			rte_event_eth_rx_adapter_stop(i);
			rte_eth_dev_stop(i);
			rte_eth_dev_close(i);
		}
	}
}

int
perf_mempool_setup(struct evt_test *test, struct evt_options *opt)
{
	struct test_perf *t = evt_test_priv(test);

	if (opt->prod_type == EVT_PROD_TYPE_SYNT ||
			opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR) {
		t->pool = rte_mempool_create(test->name, /* mempool name */
				opt->pool_sz, /* number of elements*/
				sizeof(struct perf_elt), /* element size*/
				512, /* cache size*/
				0, NULL, NULL,
				perf_elt_init, /* obj constructor */
				NULL, opt->socket_id, 0); /* flags */
	} else {
		t->pool = rte_pktmbuf_pool_create(test->name, /* mempool name */
				opt->pool_sz, /* number of elements*/
				512, /* cache size*/
				0,
				RTE_MBUF_DEFAULT_BUF_SIZE,
				opt->socket_id); /* flags */

	}

	if (t->pool == NULL) {
		evt_err("failed to create mempool");
		return -ENOMEM;
	}

	return 0;
}

void
perf_mempool_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_perf *t = evt_test_priv(test);

	rte_mempool_free(t->pool);
}

int
perf_test_setup(struct evt_test *test, struct evt_options *opt)
{
	void *test_perf;

	test_perf = rte_zmalloc_socket(test->name, sizeof(struct test_perf),
				RTE_CACHE_LINE_SIZE, opt->socket_id);
	if (test_perf  == NULL) {
		evt_err("failed to allocate test_perf memory");
		goto nomem;
	}
	test->test_priv = test_perf;

	struct test_perf *t = evt_test_priv(test);

	if (opt->prod_type == EVT_PROD_TYPE_EVENT_TIMER_ADPTR) {
		t->outstand_pkts = opt->nb_timers *
			evt_nr_active_lcores(opt->plcores);
		t->nb_pkts = opt->nb_timers;
	} else {
		t->outstand_pkts = opt->nb_pkts *
			evt_nr_active_lcores(opt->plcores);
		t->nb_pkts = opt->nb_pkts;
	}

	t->nb_workers = evt_nr_active_lcores(opt->wlcores);
	t->done = false;
	t->nb_flows = opt->nb_flows;
	t->result = EVT_TEST_FAILED;
	t->opt = opt;
	memcpy(t->sched_type_list, opt->sched_type_list,
			sizeof(opt->sched_type_list));
	return 0;
nomem:
	return -ENOMEM;
}

void
perf_test_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);

	rte_free(test->test_priv);
}
