/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2017 Wind River Systems, Inc.
 */

#ifndef _AVP_LOGS_H_
#define _AVP_LOGS_H_

#include <rte_log.h>

#ifdef RTE_LIBRTE_AVP_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s() rx: " fmt, __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_AVP_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s() tx: " fmt, __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

extern int avp_logtype_driver;

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, avp_logtype_driver, \
		"%s(): " fmt, __func__, ## args)

#endif /* _AVP_LOGS_H_ */
