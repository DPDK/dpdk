/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef _EVT_COMMON_
#define _EVT_COMMON_

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eventdev.h>
#include <rte_service.h>

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

static inline int
evt_service_setup(uint32_t service_id)
{
	int32_t core_cnt;
	unsigned int lcore = 0;
	uint32_t core_array[RTE_MAX_LCORE];
	uint8_t cnt;
	uint8_t min_cnt = UINT8_MAX;

	if (!rte_service_lcore_count())
		return -ENOENT;

	core_cnt = rte_service_lcore_list(core_array,
			RTE_MAX_LCORE);
	if (core_cnt < 0)
		return -ENOENT;
	/* Get the core which has least number of services running. */
	while (core_cnt--) {
		/* Reset default mapping */
		rte_service_map_lcore_set(service_id,
				core_array[core_cnt], 0);
		cnt = rte_service_lcore_count_services(
				core_array[core_cnt]);
		if (cnt < min_cnt) {
			lcore = core_array[core_cnt];
			min_cnt = cnt;
		}
	}
	if (rte_service_map_lcore_set(service_id, lcore, 1))
		return -ENOENT;

	return 0;
}

#endif /*  _EVT_COMMON_*/
