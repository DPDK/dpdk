/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016-2020 Intel Corporation
 */

#ifndef _DLB2_EVDEV_LOG_H_
#define _DLB2_EVDEV_LOG_H_

extern int eventdev_dlb2_log_level;
#define RTE_LOGTYPE_EVENTDEV_DLB2 eventdev_dlb2_log_level

/* Dynamic logging */
#define DLB2_LOG_IMPL(level, ...) \
	RTE_LOG_LINE_PREFIX(level, EVENTDEV_DLB2, "%s", __func__, __VA_ARGS__)

#define DLB2_LOG_INFO(fmt, ...) \
	DLB2_LOG_IMPL(INFO, fmt, ## __VA_ARGS__)

#define DLB2_LOG_ERR(fmt, ...) \
	DLB2_LOG_IMPL(ERR, fmt, ## __VA_ARGS__)

/* remove debug logs at compile time unless actually debugging */
#define DLB2_LOG_LINE_DBG(...) \
	RTE_LOG_DP_LINE(DEBUG, EVENTDEV_DLB2, __VA_ARGS__)

#endif /* _DLB2_EVDEV_LOG_H_ */
