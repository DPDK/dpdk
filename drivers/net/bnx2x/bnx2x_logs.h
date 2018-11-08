/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 * Copyright (c) 2015-2018 Cavium Inc.
 * All rights reserved.
 * www.cavium.com
 */

#ifndef _PMD_LOGS_H_
#define _PMD_LOGS_H_

extern int bnx2x_logtype_init;
#define PMD_INIT_LOG(level, sc, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnx2x_logtype_init, \
	"[bnx2x_pmd: %s] %s() " fmt "\n", (sc)->devinfo.name, __func__, ##args)

#define PMD_INIT_FUNC_TRACE(sc) PMD_INIT_LOG(DEBUG, sc, " >>")

extern int bnx2x_logtype_driver;
#define PMD_DRV_LOG_RAW(level, sc, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnx2x_logtype_driver, \
		"[%s:%d(%s)] " fmt,	__func__, __LINE__, \
		(sc)->devinfo.name ? (sc)->devinfo.name : "", ## args)

#define PMD_DRV_LOG(level, sc, fmt, args...) \
	PMD_DRV_LOG_RAW(level, sc, fmt "\n", ## args)

#ifdef RTE_LIBRTE_BNX2X_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnx2x_logtype_driver, \
	"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnx2x_logtype_driver, \
		"%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_BNX2X_DEBUG_PERIODIC
#define PMD_DEBUG_PERIODIC_LOG(level, sc, fmt, args...) \
	rte_log(RTE_LOG_ ## level, bnx2x_logtype_driver, \
		"%s(%s): " fmt "\n", __func__, \
		(sc)->devinfo.name ? (sc)->devinfo.name : "", ## args)
#else
#define PMD_DEBUG_PERIODIC_LOG(level, sc, fmt, args...) do { } while (0)
#endif

#endif /* _PMD_LOGS_H_ */
