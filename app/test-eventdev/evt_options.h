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

#ifndef _EVT_OPTIONS_
#define _EVT_OPTIONS_

#include <stdio.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_eventdev.h>
#include <rte_lcore.h>

#include "evt_common.h"

#define EVT_BOOL_FMT(x)          ((x) ? "true" : "false")

#define EVT_VERBOSE              ("verbose")
#define EVT_DEVICE               ("dev")
#define EVT_TEST                 ("test")
#define EVT_SCHED_LCORE          ("slcore")
#define EVT_PROD_LCORES          ("plcores")
#define EVT_WORK_LCORES          ("wlcores")
#define EVT_NB_FLOWS             ("nb_flows")
#define EVT_SOCKET_ID            ("socket_id")
#define EVT_POOL_SZ              ("pool_sz")
#define EVT_WKR_DEQ_DEP          ("worker_deq_depth")
#define EVT_NB_PKTS              ("nb_pkts")
#define EVT_NB_STAGES            ("nb_stages")
#define EVT_SCHED_TYPE_LIST      ("stlist")
#define EVT_FWD_LATENCY          ("fwd_latency")
#define EVT_QUEUE_PRIORITY       ("queue_priority")
#define EVT_HELP                 ("help")

struct evt_options {
#define EVT_TEST_NAME_MAX_LEN     32
	char test_name[EVT_TEST_NAME_MAX_LEN];
	bool plcores[RTE_MAX_LCORE];
	bool wlcores[RTE_MAX_LCORE];
	uint8_t sched_type_list[EVT_MAX_STAGES];
	int slcore;
	uint32_t nb_flows;
	int socket_id;
	int pool_sz;
	int nb_stages;
	int verbose_level;
	uint64_t nb_pkts;
	uint16_t wkr_deq_dep;
	uint8_t dev_id;
	uint32_t fwd_latency:1;
	uint32_t q_priority:1;
};

void evt_options_default(struct evt_options *opt);
int evt_options_parse(struct evt_options *opt, int argc, char **argv);
void evt_options_dump(struct evt_options *opt);

/* options check helpers */
static inline bool
evt_lcores_has_overlap(bool lcores[], int lcore)
{
	if (lcores[lcore] == true) {
		evt_err("lcore overlaps at %d", lcore);
		return true;
	}

	return false;
}

static inline bool
evt_lcores_has_overlap_multi(bool lcoresx[], bool lcoresy[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcoresx[i] && lcoresy[i]) {
			evt_err("lcores overlaps at %d", i);
			return true;
		}
	}
	return false;
}

static inline bool
evt_has_active_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			return true;
	return false;
}

static inline int
evt_nr_active_lcores(bool lcores[])
{
	int i;
	int c = 0;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			c++;
	return c;
}

static inline int
evt_get_first_active_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if (lcores[i])
			return i;
	return -1;
}

static inline bool
evt_has_disabled_lcore(bool lcores[])
{
	int i;

	for (i = 0; i < RTE_MAX_LCORE; i++)
		if ((lcores[i] == true) && !(rte_lcore_is_enabled(i)))
			return true;
	return false;
}

static inline bool
evt_has_invalid_stage(struct evt_options *opt)
{
	if (!opt->nb_stages) {
		evt_err("need minimum one stage, check --stlist");
		return true;
	}
	if (opt->nb_stages > EVT_MAX_STAGES) {
		evt_err("requested changes are beyond EVT_MAX_STAGES=%d",
			EVT_MAX_STAGES);
		return true;
	}
	return false;
}

static inline bool
evt_has_invalid_sched_type(struct evt_options *opt)
{
	int i;

	for (i = 0; i < opt->nb_stages; i++) {
		if (opt->sched_type_list[i] > RTE_SCHED_TYPE_PARALLEL) {
			evt_err("invalid sched_type %d at %d",
				opt->sched_type_list[i], i);
			return true;
		}
	}
	return false;
}

/* option dump helpers */
static inline void
evt_dump_worker_lcores(struct evt_options *opt)
{
	int c;

	evt_dump_begin("worker lcores");
	for  (c = 0; c < RTE_MAX_LCORE; c++) {
		if (opt->wlcores[c])
			printf("%d ", c);
	}
	evt_dump_end;
}

static inline void
evt_dump_producer_lcores(struct evt_options *opt)
{
	int c;

	evt_dump_begin("producer lcores");
	for  (c = 0; c < RTE_MAX_LCORE; c++) {
		if (opt->plcores[c])
			printf("%d ", c);
	}
	evt_dump_end;
}

static inline void
evt_dump_nb_flows(struct evt_options *opt)
{
	evt_dump("nb_flows", "%d", opt->nb_flows);
}

static inline void
evt_dump_scheduler_lcore(struct evt_options *opt)
{
	evt_dump("scheduler lcore", "%d", opt->slcore);
}

static inline void
evt_dump_worker_dequeue_depth(struct evt_options *opt)
{
	evt_dump("worker deq depth", "%d", opt->wkr_deq_dep);
}

static inline void
evt_dump_nb_stages(struct evt_options *opt)
{
	evt_dump("nb_stages", "%d", opt->nb_stages);
}

static inline void
evt_dump_fwd_latency(struct evt_options *opt)
{
	evt_dump("fwd_latency", "%s", EVT_BOOL_FMT(opt->fwd_latency));
}

static inline void
evt_dump_queue_priority(struct evt_options *opt)
{
	evt_dump("queue_priority", "%s", EVT_BOOL_FMT(opt->q_priority));
}

static inline const char*
evt_sched_type_2_str(uint8_t sched_type)
{

	if (sched_type == RTE_SCHED_TYPE_ORDERED)
		return "O";
	else if (sched_type == RTE_SCHED_TYPE_ATOMIC)
		return "A";
	else if (sched_type == RTE_SCHED_TYPE_PARALLEL)
		return "P";
	else
		return "I";
}

static inline void
evt_dump_sched_type_list(struct evt_options *opt)
{
	int i;

	evt_dump_begin("sched_type_list");
	for (i = 0; i < opt->nb_stages; i++)
		printf("%s ", evt_sched_type_2_str(opt->sched_type_list[i]));

	evt_dump_end;
}

#endif /* _EVT_OPTIONS_ */
