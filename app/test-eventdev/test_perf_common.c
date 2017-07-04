/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium 2017.
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
 *     * Neither the name of Cavium networks nor the names of its
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
