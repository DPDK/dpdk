/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HN_LOGS_H_
#define _HN_LOGS_H_

#include <rte_log.h>

extern int hn_logtype_init;
#define RTE_LOGTYPE_HN_INIT hn_logtype_init
extern int hn_logtype_driver;
#define RTE_LOGTYPE_HN_DRIVER hn_logtype_driver

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HN_INIT, "%s(): ", __func__, __VA_ARGS__)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_NETVSC_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HN_DRIVER, "%s() rx: ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_NETVSC_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HN_DRIVER, "%s() tx: ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HN_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _HN_LOGS_H_ */
