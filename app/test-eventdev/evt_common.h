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

#ifndef _EVT_COMMON_
#define _EVT_COMMON_

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eventdev.h>

#define CLNRM  "\x1b[0m"
#define CLRED  "\x1b[31m"
#define CLGRN  "\x1b[32m"
#define CLYEL  "\x1b[33m"

#define evt_err(fmt, args...) \
	fprintf(stderr, CLRED"error: %s() "fmt CLNRM "\n", __func__, ## args)

#define evt_info(fmt, args...) \
	fprintf(stdout, CLYEL""fmt CLNRM "\n", ## args)

#define EVT_STR_FMT 20

#define evt_dump(str, fmt, val...) \
	printf("\t%-*s : "fmt"\n", EVT_STR_FMT, str, ## val)

#define evt_dump_begin(str) printf("\t%-*s : {", EVT_STR_FMT, str)

#define evt_dump_end printf("\b}\n")

#define EVT_MAX_STAGES           64
#define EVT_MAX_PORTS            256
#define EVT_MAX_QUEUES           256

static inline bool
evt_has_distributed_sched(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED) ?
			true : false;
}

static inline bool
evt_has_burst_mode(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_BURST_MODE) ?
			true : false;
}


static inline bool
evt_has_all_types_queue(uint8_t dev_id)
{
	struct rte_event_dev_info dev_info;

	rte_event_dev_info_get(dev_id, &dev_info);
	return (dev_info.event_dev_cap & RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES) ?
			true : false;
}

static inline uint32_t
evt_sched_type2queue_cfg(uint8_t sched_type)
{
	uint32_t ret;

	switch (sched_type) {
	case RTE_SCHED_TYPE_ATOMIC:
		ret = RTE_EVENT_QUEUE_CFG_ATOMIC_ONLY;
		break;
	case RTE_SCHED_TYPE_ORDERED:
		ret = RTE_EVENT_QUEUE_CFG_ORDERED_ONLY;
		break;
	case RTE_SCHED_TYPE_PARALLEL:
		ret = RTE_EVENT_QUEUE_CFG_PARALLEL_ONLY;
		break;
	default:
		rte_panic("Invalid sched_type %d\n", sched_type);
	}
	return ret;
}

#endif /*  _EVT_COMMON_*/
