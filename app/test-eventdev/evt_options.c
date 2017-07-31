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

typedef int (*option_parser_t)(struct evt_options *opt,
		const char *arg);

struct long_opt_parser {
	const char *lgopt_name;
	option_parser_t parser_fn;
};

static int
evt_parse_nb_flows(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint32(&(opt->nb_flows), arg);

	return ret;
}

static int
evt_parse_dev_id(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint8(&(opt->dev_id), arg);

	return ret;
}

static int
evt_parse_verbose(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->verbose_level = atoi(arg);
	return 0;
}

static int
evt_parse_fwd_latency(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->fwd_latency = 1;
	return 0;
}

static int
evt_parse_queue_priority(struct evt_options *opt, const char *arg __rte_unused)
{
	opt->q_priority = 1;
	return 0;
}

static int
evt_parse_test_name(struct evt_options *opt, const char *arg)
{
	snprintf(opt->test_name, EVT_TEST_NAME_MAX_LEN, "%s", arg);
	return 0;
}

static int
evt_parse_slcore(struct evt_options *opt, const char *arg)
{
	opt->slcore = atoi(arg);
	return 0;
}

static int
evt_parse_socket_id(struct evt_options *opt, const char *arg)
{
	opt->socket_id = atoi(arg);
	return 0;
}

static int
evt_parse_wkr_deq_dep(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint16(&(opt->wkr_deq_dep), arg);
	return ret;
}

static int
evt_parse_nb_pkts(struct evt_options *opt, const char *arg)
{
	int ret;

	ret = parser_read_uint64(&(opt->nb_pkts), arg);

	return ret;
}

static int
evt_parse_pool_sz(struct evt_options *opt, const char *arg)
{
	opt->pool_sz = atoi(arg);

	return 0;
}

static int
evt_parse_plcores(struct evt_options *opt, const char *corelist)
{
	int ret;

	ret = parse_lcores_list(opt->plcores, corelist);
	if (ret == -E2BIG)
		evt_err("duplicate lcores in plcores");

	return ret;
}

static int
evt_parse_work_lcores(struct evt_options *opt, const char *corelist)
{
	int ret;

	ret = parse_lcores_list(opt->wlcores, corelist);
	if (ret == -E2BIG)
		evt_err("duplicate lcores in wlcores");

	return ret;
}

static void
usage(char *program)
{
	printf("usage : %s [EAL options] -- [application options]\n", program);
	printf("application options:\n");
	printf("\t--verbose          : verbose level\n"
		"\t--dev              : device id of the event device\n"
		"\t--test             : name of the test application to run\n"
		"\t--socket_id        : socket_id of application resources\n"
		"\t--pool_sz          : pool size of the mempool\n"
		"\t--slcore           : lcore id of the scheduler\n"
		"\t--plcores          : list of lcore ids for producers\n"
		"\t--wlcores          : list of lcore ids for workers\n"
		"\t--stlist           : list of scheduled types of the stages\n"
		"\t--nb_flows         : number of flows to produce\n"
		"\t--nb_pkts          : number of packets to produce\n"
		"\t--worker_deq_depth : dequeue depth of the worker\n"
		"\t--fwd_latency      : perform fwd_latency measurement\n"
		"\t--queue_priority   : enable queue priority\n"
		);
	printf("available tests:\n");
	evt_test_dump_names();
}

static int
evt_parse_sched_type_list(struct evt_options *opt, const char *arg)
{
	char c;
	int i = 0, j = -1;

	for (i = 0; i < EVT_MAX_STAGES; i++)
		opt->sched_type_list[i] = (uint8_t)-1;

	i = 0;

	do {
		c = arg[++j];

		switch (c) {
		case 'o':
		case 'O':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_ORDERED;
			break;
		case 'a':
		case 'A':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_ATOMIC;
			break;
		case 'p':
		case 'P':
			opt->sched_type_list[i++] = RTE_SCHED_TYPE_PARALLEL;
			break;
		case ',':
			break;
		default:
			if (c != '\0') {
				evt_err("invalid sched_type %c", c);
				return -EINVAL;
			}
		}
	} while (c != '\0');

	opt->nb_stages = i;
	return 0;
}

static struct option lgopts[] = {
	{ EVT_NB_FLOWS,         1, 0, 0 },
	{ EVT_DEVICE,           1, 0, 0 },
	{ EVT_VERBOSE,          1, 0, 0 },
	{ EVT_TEST,             1, 0, 0 },
	{ EVT_PROD_LCORES,      1, 0, 0 },
	{ EVT_WORK_LCORES,      1, 0, 0 },
	{ EVT_SOCKET_ID,        1, 0, 0 },
	{ EVT_POOL_SZ,          1, 0, 0 },
	{ EVT_NB_PKTS,          1, 0, 0 },
	{ EVT_WKR_DEQ_DEP,      1, 0, 0 },
	{ EVT_SCHED_LCORE,      1, 0, 0 },
	{ EVT_SCHED_TYPE_LIST,  1, 0, 0 },
	{ EVT_FWD_LATENCY,      0, 0, 0 },
	{ EVT_QUEUE_PRIORITY,   0, 0, 0 },
	{ EVT_HELP,             0, 0, 0 },
	{ NULL,                 0, 0, 0 }
};

static int
evt_opts_parse_long(int opt_idx, struct evt_options *opt)
{
	unsigned int i;

	struct long_opt_parser parsermap[] = {
		{ EVT_NB_FLOWS, evt_parse_nb_flows},
		{ EVT_DEVICE, evt_parse_dev_id},
		{ EVT_VERBOSE, evt_parse_verbose},
		{ EVT_TEST, evt_parse_test_name},
		{ EVT_PROD_LCORES, evt_parse_plcores},
		{ EVT_WORK_LCORES, evt_parse_work_lcores},
		{ EVT_SOCKET_ID, evt_parse_socket_id},
		{ EVT_POOL_SZ, evt_parse_pool_sz},
		{ EVT_NB_PKTS, evt_parse_nb_pkts},
		{ EVT_WKR_DEQ_DEP, evt_parse_wkr_deq_dep},
		{ EVT_SCHED_LCORE, evt_parse_slcore},
		{ EVT_SCHED_TYPE_LIST, evt_parse_sched_type_list},
		{ EVT_FWD_LATENCY, evt_parse_fwd_latency},
		{ EVT_QUEUE_PRIORITY, evt_parse_queue_priority},
	};

	for (i = 0; i < RTE_DIM(parsermap); i++) {
		if (strncmp(lgopts[opt_idx].name, parsermap[i].lgopt_name,
				strlen(parsermap[i].lgopt_name)) == 0)
			return parsermap[i].parser_fn(opt, optarg);
	}

	return -EINVAL;
}

int
evt_options_parse(struct evt_options *opt, int argc, char **argv)
{
	int opts, retval, opt_idx;

	while ((opts = getopt_long(argc, argv, "", lgopts, &opt_idx)) != EOF) {
		switch (opts) {
		case 0: /* long options */
			if (!strcmp(lgopts[opt_idx].name, "help")) {
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			}

			retval = evt_opts_parse_long(opt_idx, opt);
			if (retval != 0)
				return retval;
			break;
		default:
			return -EINVAL;
		}
	}
	return 0;
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
