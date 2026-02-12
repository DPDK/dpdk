/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _DPAA2_EVENTDEV_LOGS_H_
#define _DPAA2_EVENTDEV_LOGS_H_

extern int dpaa2_logtype_event;
#define RTE_LOGTYPE_DPAA2_EVENT dpaa2_logtype_event

#define DPAA2_EVENTDEV_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_EVENT, __VA_ARGS__)

#define DPAA2_EVENTDEV_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_EVENT, "%s():", __func__, __VA_ARGS__)

#define EVENTDEV_INIT_FUNC_TRACE() DPAA2_EVENTDEV_DEBUG(" >>")

#define DPAA2_EVENTDEV_INFO(fmt, ...) \
	DPAA2_EVENTDEV_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_EVENTDEV_ERR(fmt, ...) \
	DPAA2_EVENTDEV_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_EVENTDEV_WARN(fmt, ...) \
	DPAA2_EVENTDEV_LOG(WARNING, fmt, ## __VA_ARGS__)

#define dpaa2_evdev_info(fmt, ...) DPAA2_EVENTDEV_LOG(INFO, fmt, ##__VA_ARGS__)
#define dpaa2_evdev_dbg(fmt, ...) DPAA2_EVENTDEV_LOG(DEBUG, fmt, ##__VA_ARGS__)
#define dpaa2_evdev_err(fmt, ...) DPAA2_EVENTDEV_LOG(ERR, fmt, ##__VA_ARGS__)

#endif /* _DPAA2_EVENTDEV_LOGS_H_ */
