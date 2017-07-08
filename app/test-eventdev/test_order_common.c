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

#include "test_order_common.h"

int
order_test_result(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_order *t = evt_test_priv(test);

	return t->result;
}

static inline int
order_producer(void *arg)
{
	struct prod_data *p  = arg;
	struct test_order *t = p->t;
	struct evt_options *opt = t->opt;
	const uint8_t dev_id = p->dev_id;
	const uint8_t port = p->port_id;
	struct rte_mempool *pool = t->pool;
	const uint64_t nb_pkts = t->nb_pkts;
	uint32_t *producer_flow_seq = t->producer_flow_seq;
	const uint32_t nb_flows = t->nb_flows;
	uint64_t count = 0;
	struct rte_mbuf *m;
	struct rte_event ev;

	if (opt->verbose_level > 1)
		printf("%s(): lcore %d dev_id %d port=%d queue=%d\n",
			 __func__, rte_lcore_id(), dev_id, port, p->queue_id);

	ev.event = 0;
	ev.op = RTE_EVENT_OP_NEW;
	ev.queue_id = p->queue_id;
	ev.sched_type = RTE_SCHED_TYPE_ORDERED;
	ev.priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
	ev.event_type =  RTE_EVENT_TYPE_CPU;
	ev.sub_event_type = 0; /* stage 0 */

	while (count < nb_pkts && t->err == false) {
		m = rte_pktmbuf_alloc(pool);
		if (m == NULL)
			continue;

		const uint32_t flow = (uintptr_t)m % nb_flows;
		/* Maintain seq number per flow */
		m->seqn = producer_flow_seq[flow]++;

		ev.flow_id = flow;
		ev.mbuf = m;

		while (rte_event_enqueue_burst(dev_id, port, &ev, 1) != 1) {
			if (t->err)
				break;
			rte_pause();
		}

		count++;
	}
	return 0;
}

int
order_opt_check(struct evt_options *opt)
{
	/* 1 producer + N workers + 1 master */
	if (rte_lcore_count() < 3) {
		evt_err("test need minimum 3 lcores");
		return -1;
	}

	/* Validate worker lcores */
	if (evt_lcores_has_overlap(opt->wlcores, rte_get_master_lcore())) {
		evt_err("worker lcores overlaps with master lcore");
		return -1;
	}

	if (evt_nr_active_lcores(opt->plcores) == 0) {
		evt_err("missing the producer lcore");
		return -1;
	}

	if (evt_nr_active_lcores(opt->plcores) != 1) {
		evt_err("only one producer lcore must be selected");
		return -1;
	}

	int plcore = evt_get_first_active_lcore(opt->plcores);

	if (plcore < 0) {
		evt_err("failed to find active producer");
		return plcore;
	}

	if (evt_lcores_has_overlap(opt->wlcores, plcore)) {
		evt_err("worker lcores overlaps producer lcore");
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

	/* Validate producer lcore */
	if (plcore == (int)rte_get_master_lcore()) {
		evt_err("producer lcore and master lcore should be different");
		return -1;
	}
	if (!rte_lcore_is_enabled(plcore)) {
		evt_err("producer lcore is not enabled");
		return -1;
	}

	/* Fixups */
	if (opt->nb_pkts == 0)
		opt->nb_pkts = INT64_MAX;

	return 0;
}

int
order_test_setup(struct evt_test *test, struct evt_options *opt)
{
	void *test_order;

	test_order = rte_zmalloc_socket(test->name, sizeof(struct test_order),
				RTE_CACHE_LINE_SIZE, opt->socket_id);
	if (test_order  == NULL) {
		evt_err("failed to allocate test_order memory");
		goto nomem;
	}
	test->test_priv = test_order;

	struct test_order *t = evt_test_priv(test);

	t->producer_flow_seq = rte_zmalloc_socket("test_producer_flow_seq",
				 sizeof(*t->producer_flow_seq) * opt->nb_flows,
				RTE_CACHE_LINE_SIZE, opt->socket_id);

	if (t->producer_flow_seq  == NULL) {
		evt_err("failed to allocate t->producer_flow_seq memory");
		goto prod_nomem;
	}

	t->expected_flow_seq = rte_zmalloc_socket("test_expected_flow_seq",
				 sizeof(*t->expected_flow_seq) * opt->nb_flows,
				RTE_CACHE_LINE_SIZE, opt->socket_id);

	if (t->expected_flow_seq  == NULL) {
		evt_err("failed to allocate t->expected_flow_seq memory");
		goto exp_nomem;
	}
	rte_atomic64_set(&t->outstand_pkts, opt->nb_pkts);
	t->err = false;
	t->nb_pkts = opt->nb_pkts;
	t->nb_flows = opt->nb_flows;
	t->result = EVT_TEST_FAILED;
	t->opt = opt;
	return 0;

exp_nomem:
	rte_free(t->producer_flow_seq);
prod_nomem:
	rte_free(test->test_priv);
nomem:
	return -ENOMEM;
}

void
order_test_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_order *t = evt_test_priv(test);

	rte_free(t->expected_flow_seq);
	rte_free(t->producer_flow_seq);
	rte_free(test->test_priv);
}

int
order_mempool_setup(struct evt_test *test, struct evt_options *opt)
{
	struct test_order *t = evt_test_priv(test);

	t->pool  = rte_pktmbuf_pool_create(test->name, opt->pool_sz,
					256 /* Cache */, 0,
					512, /* Use very small mbufs */
					opt->socket_id);
	if (t->pool == NULL) {
		evt_err("failed to create mempool");
		return -ENOMEM;
	}

	return 0;
}

void
order_mempool_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(opt);
	struct test_order *t = evt_test_priv(test);

	rte_mempool_free(t->pool);
}

void
order_eventdev_destroy(struct evt_test *test, struct evt_options *opt)
{
	RTE_SET_USED(test);

	rte_event_dev_stop(opt->dev_id);
	rte_event_dev_close(opt->dev_id);
}

void
order_opt_dump(struct evt_options *opt)
{
	evt_dump_producer_lcores(opt);
	evt_dump("nb_wrker_lcores", "%d", evt_nr_active_lcores(opt->wlcores));
	evt_dump_worker_lcores(opt);
	evt_dump("nb_evdev_ports", "%d", order_nb_event_ports(opt));
}

int
order_launch_lcores(struct evt_test *test, struct evt_options *opt,
			int (*worker)(void *))
{
	int ret, lcore_id;
	struct test_order *t = evt_test_priv(test);

	int wkr_idx = 0;
	/* launch workers */
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (!(opt->wlcores[lcore_id]))
			continue;

		ret = rte_eal_remote_launch(worker, &t->worker[wkr_idx],
					 lcore_id);
		if (ret) {
			evt_err("failed to launch worker %d", lcore_id);
			return ret;
		}
		wkr_idx++;
	}

	/* launch producer */
	int plcore = evt_get_first_active_lcore(opt->plcores);

	ret = rte_eal_remote_launch(order_producer, &t->prod, plcore);
	if (ret) {
		evt_err("failed to launch order_producer %d", plcore);
		return ret;
	}

	uint64_t cycles = rte_get_timer_cycles();
	int64_t old_remaining  = -1;

	while (t->err == false) {

		rte_event_schedule(opt->dev_id);

		uint64_t new_cycles = rte_get_timer_cycles();
		int64_t remaining = rte_atomic64_read(&t->outstand_pkts);

		if (remaining <= 0) {
			t->result = EVT_TEST_SUCCESS;
			break;
		}

		if (new_cycles - cycles > rte_get_timer_hz() * 1) {
			printf(CLGRN"\r%"PRId64""CLNRM, remaining);
			fflush(stdout);
			if (old_remaining == remaining) {
				rte_event_dev_dump(opt->dev_id, stdout);
				evt_err("No schedules for seconds, deadlock");
				t->err = true;
				rte_smp_wmb();
				break;
			}
			old_remaining = remaining;
			cycles = new_cycles;
		}
	}
	printf("\r");

	return 0;
}

int
order_event_dev_port_setup(struct evt_test *test, struct evt_options *opt,
				uint8_t nb_workers, uint8_t nb_queues)
{
	int ret;
	uint8_t port;
	struct test_order *t = evt_test_priv(test);

	/* port configuration */
	const struct rte_event_port_conf wkr_p_conf = {
			.dequeue_depth = opt->wkr_deq_dep,
			.enqueue_depth = 64,
			.new_event_threshold = 4096,
	};

	/* setup one port per worker, linking to all queues */
	for (port = 0; port < nb_workers; port++) {
		struct worker_data *w = &t->worker[port];

		w->dev_id = opt->dev_id;
		w->port_id = port;
		w->t = t;

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
	/* port for producer, no links */
	const struct rte_event_port_conf prod_conf = {
			.dequeue_depth = 8,
			.enqueue_depth = 32,
			.new_event_threshold = 1200,
	};
	struct prod_data *p = &t->prod;

	p->dev_id = opt->dev_id;
	p->port_id = port; /* last port */
	p->queue_id = 0;
	p->t = t;

	ret = rte_event_port_setup(opt->dev_id, port, &prod_conf);
	if (ret) {
		evt_err("failed to setup producer port %d", port);
		return ret;
	}

	return ret;
}
