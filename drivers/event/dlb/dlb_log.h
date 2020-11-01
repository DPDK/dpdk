/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB_EVDEV_LOG_H_
#define _DLB_EVDEV_LOG_H_

extern int eventdev_dlb_log_level;

/* Dynamic logging */
#define DLB_LOG_IMPL(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, eventdev_dlb_log_level, "%s" fmt "\n", \
		__func__, ##args)

#define DLB_LOG_INFO(fmt, args...) \
	DLB_LOG_IMPL(INFO, fmt, ## args)

#define DLB_LOG_ERR(fmt, args...) \
	DLB_LOG_IMPL(ERR, fmt, ## args)

/* remove debug logs at compile time unless actually debugging */
#define DLB_LOG_DBG(fmt, args...) \
	RTE_LOG_DP(DEBUG, PMD, fmt, ## args)

#endif /* _DLB_EVDEV_LOG_H_ */
