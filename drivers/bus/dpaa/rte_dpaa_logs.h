/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef _DPAA_LOGS_H_
#define _DPAA_LOGS_H_

#include <rte_log.h>

extern int dpaa_logtype_bus;
extern int dpaa_logtype_mempool;
extern int dpaa_logtype_pmd;
extern int dpaa_logtype_eventdev;

#define DPAA_BUS_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_bus, "dpaa: " fmt "\n", ##args)

#ifdef RTE_LIBRTE_DPAA_DEBUG_BUS
#define DPAA_BUS_HWWARN(cond, fmt, args...) \
	do {\
		if (cond) \
			DPAA_BUS_LOG(DEBUG, "WARN: " fmt, ##args); \
	} while (0)
#else
#define DPAA_BUS_HWWARN(cond, fmt, args...) do { } while (0)
#endif

#define DPAA_BUS_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa_logtype_bus, "dpaa: %s(): " fmt "\n", \
		__func__, ##args)

#define BUS_INIT_FUNC_TRACE() DPAA_BUS_DEBUG(" >>")

#define DPAA_BUS_INFO(fmt, args...) \
	DPAA_BUS_LOG(INFO, fmt, ## args)
#define DPAA_BUS_ERR(fmt, args...) \
	DPAA_BUS_LOG(ERR, fmt, ## args)
#define DPAA_BUS_WARN(fmt, args...) \
	DPAA_BUS_LOG(WARNING, fmt, ## args)

/* Mempool related logs */

#define DPAA_MEMPOOL_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_mempool, "%s(): " fmt "\n", \
		__func__, ##args)

#define MEMPOOL_INIT_FUNC_TRACE() DPAA_MEMPOOL_LOG(DEBUG, " >>")

#define DPAA_MEMPOOL_DPDEBUG(fmt, args...) \
	RTE_LOG_DP(DEBUG, PMD, fmt, ## args)
#define DPAA_MEMPOOL_DEBUG(fmt, args...) \
	DPAA_MEMPOOL_LOG(DEBUG, fmt, ## args)
#define DPAA_MEMPOOL_ERR(fmt, args...) \
	DPAA_MEMPOOL_LOG(ERR, fmt, ## args)
#define DPAA_MEMPOOL_INFO(fmt, args...) \
	DPAA_MEMPOOL_LOG(INFO, fmt, ## args)
#define DPAA_MEMPOOL_WARN(fmt, args...) \
	DPAA_MEMPOOL_LOG(WARNING, fmt, ## args)

/* PMD related logs */

#define DPAA_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_pmd, "%s(): " fmt "\n", \
		__func__, ##args)

#define PMD_INIT_FUNC_TRACE() DPAA_PMD_LOG(DEBUG, " >>")

#define DPAA_PMD_DEBUG(fmt, args...) \
	DPAA_PMD_LOG(DEBUG, fmt, ## args)
#define DPAA_PMD_ERR(fmt, args...) \
	DPAA_PMD_LOG(ERR, fmt, ## args)
#define DPAA_PMD_INFO(fmt, args...) \
	DPAA_PMD_LOG(INFO, fmt, ## args)
#define DPAA_PMD_WARN(fmt, args...) \
	DPAA_PMD_LOG(WARNING, fmt, ## args)

#define DPAA_EVENTDEV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_eventdev, "%s(): " fmt "\n", \
		__func__, ##args)

#define EVENTDEV_INIT_FUNC_TRACE() DPAA_EVENTDEV_LOG(DEBUG, " >>")

#define DPAA_EVENTDEV_DEBUG(fmt, args...) \
	DPAA_EVENTDEV_LOG(DEBUG, fmt, ## args)
#define DPAA_EVENTDEV_ERR(fmt, args...) \
	DPAA_EVENTDEV_LOG(ERR, fmt, ## args)
#define DPAA_EVENTDEV_INFO(fmt, args...) \
	DPAA_EVENTDEV_LOG(INFO, fmt, ## args)
#define DPAA_EVENTDEV_WARN(fmt, args...) \
	DPAA_EVENTDEV_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#endif /* _DPAA_LOGS_H_ */
