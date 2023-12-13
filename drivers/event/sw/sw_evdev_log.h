/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _SW_EVDEV_LOG_H_
#define _SW_EVDEV_LOG_H_

extern int eventdev_sw_log_level;
#define RTE_LOGTYPE_EVENT_SW eventdev_sw_log_level

#define SW_LOG_IMPL(level, ...) \
	RTE_LOG_LINE_PREFIX(level, EVENT_SW, "%s", __func__, __VA_ARGS__)

#define SW_LOG_INFO(fmt, args...) \
	SW_LOG_IMPL(INFO, fmt, ## args)

#define SW_LOG_DBG(fmt, args...) \
	SW_LOG_IMPL(DEBUG, fmt, ## args)

#define SW_LOG_ERR(fmt, args...) \
	SW_LOG_IMPL(ERR, fmt, ## args)

#endif /* _SW_EVDEV_LOG_H_ */
