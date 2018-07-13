/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef _HN_LOGS_H_
#define _HN_LOGS_H_

#include <rte_log.h>

extern int hn_logtype_init;
extern int hn_logtype_driver;

#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hn_logtype_init, "%s(): " fmt "\n",\
		__func__, ## args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_NETVSC_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hn_logtype_driver, \
		"%s() rx: " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_NETVSC_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hn_logtype_driver, \
		"%s() tx: " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hn_logtype_driver, "%s(): " fmt "\n", \
		__func__, ## args)

#endif /* _HN_LOGS_H_ */
