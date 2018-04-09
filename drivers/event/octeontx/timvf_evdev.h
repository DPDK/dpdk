/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef __TIMVF_EVDEV_H__
#define __TIMVF_EVDEV_H__

#include <rte_common.h>

#include <octeontx_mbox.h>

#define timvf_log(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, otx_logtype_timvf, \
			"[%s] %s() " fmt "\n", \
			RTE_STR(event_timer_octeontx), __func__, ## args)

#define timvf_log_info(fmt, ...) timvf_log(INFO, fmt, ##__VA_ARGS__)
#define timvf_log_dbg(fmt, ...) timvf_log(DEBUG, fmt, ##__VA_ARGS__)
#define timvf_log_err(fmt, ...) timvf_log(ERR, fmt, ##__VA_ARGS__)
#define timvf_func_trace timvf_log_dbg

extern int otx_logtype_timvf;

struct timvf_info {
	uint16_t domain; /* Domain id */
	uint8_t total_timvfs; /* Total timvf available in domain */
};

int timvf_info(struct timvf_info *tinfo);
void *timvf_bar(uint8_t id, uint8_t bar);

#endif /* __TIMVF_EVDEV_H__ */
