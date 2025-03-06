/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2025 Ericsson AB
 */

#include "test_atomic_common.h"

static inline bool
test_done(struct test_order *const t)
{
	return t->err || t->result == EVT_TEST_SUCCESS;
}

static inline int
atomic_producer(void *arg)
{
	struct prod_data *p = arg;
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

	ev = (struct rte_event) {
		.op = RTE_EVENT_OP_NEW,
		.queue_id = p->queue_id,
		.sched_type = RTE_SCHED_TYPE_ATOMIC,
		.priority = RTE_EVENT_DEV_PRIORITY_NORMAL,
		.event_type = RTE_EVENT_TYPE_CPU,
		.sub_event_type = 0
	};

	while (count < nb_pkts && t->err == false) {
		m = rte_pktmbuf_alloc(pool);
		if (m == NULL)
			continue;

		/* Maintain seq number per flow */

		const flow_id_t flow = rte_rand_max(nb_flows);

		*order_mbuf_flow_id(t, m) = flow;
		*order_mbuf_seqn(t, m) = producer_flow_seq[flow]++;

		ev.flow_id = flow;
		ev.mbuf = m;

		while (rte_event_enqueue_burst(dev_id, port, &ev, 1) != 1) {
			if (t->err)
				break;
			rte_pause();
		}

		count++;
	}

	if (!evt_is_maintenance_free(dev_id)) {
		while (!test_done(t)) {
			rte_event_maintain(dev_id, port, RTE_EVENT_DEV_MAINT_OP_FLUSH);
			rte_pause();
		}
	}

	return 0;
}

int
atomic_launch_lcores(struct evt_test *test, struct evt_options *opt,
		int (*worker)(void *))
{
	int ret, lcore_id;
	struct test_order *t = evt_test_priv(test);

	/* launch workers */

	int wkr_idx = 0;
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		if (!(opt->wlcores[lcore_id]))
			continue;

		ret = rte_eal_remote_launch(worker, &t->worker[wkr_idx], lcore_id);
		if (ret) {
			evt_err("failed to launch worker %d", lcore_id);
			return ret;
		}
		wkr_idx++;
	}

	/* launch producer */
	int plcore = evt_get_first_active_lcore(opt->plcores);

	ret = rte_eal_remote_launch(atomic_producer, &t->prod, plcore);
	if (ret) {
		evt_err("failed to launch order_producer %d", plcore);
		return ret;
	}

	uint64_t prev_time = rte_get_timer_cycles();
	int64_t prev_outstanding_pkts = -1;

	while (t->err == false) {
		uint64_t current_time = rte_get_timer_cycles();
		int64_t outstanding_pkts = rte_atomic_load_explicit(
				&t->outstand_pkts, rte_memory_order_relaxed);

		if (outstanding_pkts <= 0) {
			t->result = EVT_TEST_SUCCESS;
			break;
		}

		if (current_time - prev_time > rte_get_timer_hz() * IDLE_TIMEOUT) {
			printf(CLGRN "\r%" PRId64 "" CLNRM, outstanding_pkts);
			fflush(stdout);
			if (prev_outstanding_pkts == outstanding_pkts) {
				rte_event_dev_dump(opt->dev_id, stdout);
				evt_err("No events processed during one period, deadlock");
				t->err = true;
				break;
			}
			prev_outstanding_pkts = outstanding_pkts;
			prev_time = current_time;
		}
	}
	printf("\r");

	return 0;
}
