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
 *     * Neither the name of Cavium nor the names of its
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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>

#include "evt_options.h"
#include "evt_test.h"
#include "parser.h"

void
evt_options_default(struct evt_options *opt)
{
	memset(opt, 0, sizeof(*opt));
	opt->verbose_level = 1; /* Enable minimal prints */
	opt->dev_id = 0;
	strncpy(opt->test_name, "order_queue", EVT_TEST_NAME_MAX_LEN);
	opt->nb_flows = 1024;
	opt->socket_id = SOCKET_ID_ANY;
	opt->pool_sz = 16 * 1024;
	opt->wkr_deq_dep = 16;
	opt->nb_pkts = (1ULL << 26); /* do ~64M packets */
}

void
evt_options_dump(struct evt_options *opt)
{
	int lcore_id;
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(opt->dev_id, &dev_info);
	evt_dump("driver", "%s", dev_info.driver_name);
	evt_dump("test", "%s", opt->test_name);
	evt_dump("dev", "%d", opt->dev_id);
	evt_dump("verbose_level", "%d", opt->verbose_level);
	evt_dump("socket_id", "%d", opt->socket_id);
	evt_dump("pool_sz", "%d", opt->pool_sz);
	evt_dump("master lcore", "%d", rte_get_master_lcore());
	evt_dump("nb_pkts", "%"PRIu64, opt->nb_pkts);
	evt_dump_begin("available lcores");
	RTE_LCORE_FOREACH(lcore_id)
		printf("%d ", lcore_id);
	evt_dump_end;
	evt_dump_nb_flows(opt);
	evt_dump_worker_dequeue_depth(opt);
}
