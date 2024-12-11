/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef _DPAA_LOGS_H_
#define _DPAA_LOGS_H_

#include <rte_log.h>

extern int dpaa_logtype_bus;
#define RTE_LOGTYPE_DPAA_BUS dpaa_logtype_bus

#define DPAA_BUS_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA_BUS, __VA_ARGS__)

#ifdef RTE_LIBRTE_DPAA_DEBUG_BUS
#define DPAA_BUS_HWWARN(cond, fmt, ...) \
	do {\
		if (cond) \
			DPAA_BUS_LOG(DEBUG, "WARN: " fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DPAA_BUS_HWWARN(cond, fmt, ...) do { } while (0)
#endif

#define DPAA_BUS_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA_BUS, "%s(): ", __func__, __VA_ARGS__)

#define BUS_INIT_FUNC_TRACE() DPAA_BUS_DEBUG(" >>")

#define DPAA_BUS_INFO(fmt, ...) \
	DPAA_BUS_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA_BUS_ERR(fmt, ...) \
	DPAA_BUS_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA_BUS_WARN(fmt, ...) \
	DPAA_BUS_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _DPAA_LOGS_H_ */
