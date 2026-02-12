/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2013-2017 Wind River Systems, Inc.
 */

#ifndef _AVP_LOGS_H_
#define _AVP_LOGS_H_

#include <rte_log.h>

#ifdef RTE_LIBRTE_AVP_DEBUG_RX
#define PMD_RX_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AVP_DRIVER, "%s() rx: ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG_LINE(...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_AVP_DEBUG_TX
#define PMD_TX_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AVP_DRIVER, "%s() tx: ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG_LINE(...) do { } while (0)
#endif

extern int avp_logtype_driver;
#define RTE_LOGTYPE_AVP_DRIVER avp_logtype_driver

#define PMD_DRV_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, AVP_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _AVP_LOGS_H_ */
