/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc 2017.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "test_perf_common.h"

int
perf_test_result(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_perf *t = evt_test_priv(test);

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
scheduler(void *arg)
{
	struct test_perf *t = arg;
	const uint8_t dev_id = t->opt->dev_id;

	while (t->done == false)
		rte_event_schedule(dev_id);

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

		ret = rte_eal_remote_launch(perf_producer, &t->prod[port_idx],
					 lcore_id);
		if (ret) {
			evt_err("failed to launch perf_producer %d", lcore_id);
			return ret;
		}
		port_idx++;
	}

	/* launch scheduler */
	if (!evt_has_distributed_sched(opt->dev_id)) {
		ret = rte_eal_remote_launch(scheduler, t, opt->slcore);
		if (ret) {
			evt_err("failed to launch sched %d", opt->slcore);
			return ret;
		}
	}

	const uint64_t total_pkts = opt->nb_pkts *
			evt_nr_active_lcores(opt->plcores);

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
				t->done = true;
				t->result = EVT_TEST_SUCCESS;
				rte_smp_wmb();
				break;
			}
		}

		if (new_cycles - dead_lock_cycles > dead_lock_sample) {
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

int
perf_event_dev_port_setup(struct evt_test *test, struct evt_options *opt,
				uint8_t stride, uint8_t nb_queues)
{
	struct test_perf *t = evt_test_priv(test);
	uint8_t port, prod;
	int ret = -1;

	/* port configuration */
	const struct rte_event_port_conf wkr_p_conf = {
			.dequeue_depth = opt->wkr_deq_dep,
			.enqueue_depth = 64,
			.new_event_threshold = 4096,
	};

	/* setup one port per worker, linking to all queues */
	for (port = 0; port < evt_nr_active_lcores(opt->wlcores);
				port++) {
		struct worker_data *w = &t->worker[port];

		w->dev_id = opt->dev_id;
		w->port_id = port;
		w->t = t;
		w->processed_pkts = 0;
		w->latency = 0;

		ret = rte_event_port_setup(opt->dev_id, port, &wkr_p_conf);
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
	const struct rte_event_port_conf prod_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 32,
			.new_event_threshold = 1200,
	};
	prod = 0;
	for ( ; port < perf_nb_event_ports(opt); port++) {
		struct prod_data *p = &t->prod[port];

		p->dev_id = opt->dev_id;
		p->port_id = port;
		p->queue_id = prod * stride;
		p->t = t;

		ret = rte_event_port_setup(opt->dev_id, port, &prod_conf);
		if (ret) {
			evt_err("failed to setup port %d", port);
			return ret;
		}
		prod++;
	}

	return ret;
}

int
perf_opt_check(struct evt_options *opt, uint64_t nb_queues)
{
	unsigned int lcores;
	bool need_slcore = !evt_has_distributed_sched(opt->dev_id);

	/* N producer + N worker + 1 scheduler(based on dev capa) + 1 master */
	lcores = need_slcore ? 4 : 3;

	if (rte_lcore_count() < lcores) {
		evt_err("test need minimum %d lcores", lcores);
		return -1;
	}

	/* Validate worker lcores */
	if (evt_lcores_has_overlap(opt->wlcores, rte_get_master_lcore())) {
		evt_err("worker lcores overlaps with master lcore");
		return -1;
	}
	if (need_slcore && evt_lcores_has_overlap(opt->wlcores, opt->slcore)) {
		evt_err("worker lcores overlaps with scheduler lcore");
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

	/* Validate producer lcores */
	if (evt_lcores_has_overlap(opt->plcores, rte_get_master_lcore())) {
		evt_err("producer lcores overlaps with master lcore");
		return -1;
	}
	if (need_slcore && evt_lcores_has_overlap(opt->plcores, opt->slcore)) {
		evt_err("producer lcores overlaps with scheduler lcore");
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

	/* Validate scheduler lcore */
	if (!evt_has_distributed_sched(opt->dev_id) &&
			opt->slcore == (int)rte_get_master_lcore()) {
		evt_err("scheduler lcore and master lcore should be different");
		return -1;
	}
	if (need_slcore && !rte_lcore_is_enabled(opt->slcore)) {
		evt_err("scheduler lcore is not enabled");
		return -1;
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
	if (opt->nb_stages == 1 && opt->fwd_latency) {
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
	if (!evt_has_distributed_sched(opt->dev_id))
		evt_dump_scheduler_lcore(opt);
	evt_dump_nb_stages(opt);
	evt_dump("nb_evdev_ports", "%d", perf_nb_event_ports(opt));
	evt_dump("nb_evdev_queues", "%d", nb_queues);
	evt_dump_queue_priority(opt);
	evt_dump_sched_type_list(opt);
}

void
perf_eventdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(test);

	rte_event_dev_stop(opt->dev_id);
	rte_event_dev_close(opt->dev_id);
}

static inline void
perf_elt_init(struct rte_mempool *mp, void *arg __rte_unused,
	    void *obj, unsigned i __rte_unused)
{
	memset(obj, 0, mp->elt_size);
}

int
perf_mempool_setup(struct evt_test *test, struct evt_options *opt)
{
	struct test_perf *t = evt_test_priv(test);

	t->pool = rte_mempool_create(test->name, /* mempool name */
				opt->pool_sz, /* number of elements*/
				sizeof(struct perf_elt), /* element size*/
				512, /* cache size*/
				0, NULL, NULL,
				perf_elt_init, /* obj constructor */
				NULL, opt->socket_id, 0); /* flags */
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

	t->outstand_pkts = opt->nb_pkts * evt_nr_active_lcores(opt->plcores);
	t->nb_workers = evt_nr_active_lcores(opt->wlcores);
	t->done = false;
	t->nb_pkts = opt->nb_pkts;
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
