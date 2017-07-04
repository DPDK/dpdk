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
