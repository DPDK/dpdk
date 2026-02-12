/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef _PMD_LOGS_H_
#define _PMD_LOGS_H_

extern int bnx2x_logtype_init;
#define RTE_LOGTYPE_BNX2X_INIT bnx2x_logtype_init
#define PMD_INIT_LOG(level, sc, ...) \
	RTE_LOG_LINE_PREFIX(level, BNX2X_INIT, "[%s] %s() ", \
		(sc)->devinfo.name RTE_LOG_COMMA __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE(sc) PMD_INIT_LOG(DEBUG, sc, " >>")

extern int bnx2x_logtype_driver;
#define RTE_LOGTYPE_BNX2X_DRIVER bnx2x_logtype_driver
#define PMD_DRV_LOG(level, sc, ...) \
	RTE_LOG_LINE_PREFIX(level, BNX2X_DRIVER, "[%s:%d(%s)] ", \
		__func__ RTE_LOG_COMMA __LINE__ RTE_LOG_COMMA \
		(sc)->devinfo.name ? (sc)->devinfo.name : "", __VA_ARGS__)

#ifdef RTE_LIBRTE_BNX2X_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, BNX2X_DRIVER, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, BNX2X_DRIVER, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
#define PMD_DEBUG_PERIODIC_LOG(level, sc, ...) \
	RTE_LOG_LINE_PREFIX(level, BNX2X_DRIVER, "%s(%s): ", \
		__func__ RTE_LOG_COMMA (sc)->devinfo.name ? (sc)->devinfo.name : "", __VA_ARGS__)
#else
#define PMD_DEBUG_PERIODIC_LOG(...) do { } while (0)
#endif

#endif /* _PMD_LOGS_H_ */
