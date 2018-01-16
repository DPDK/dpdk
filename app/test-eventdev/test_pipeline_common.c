/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Cavium, Inc.
 */

#include "test_pipeline_common.h"

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
