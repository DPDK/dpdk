/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Aquantia Corporation
 */
#ifndef ATL_LOGS_H
#define ATL_LOGS_H

#include <rte_log.h>

extern int atl_logtype_init;
#define RTE_LOGTYPE_ATL_INIT atl_logtype_init

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ATL_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#define PMD_RX_LOG(level, ...) \
	RTE_LOG_DP_LINE_PREFIX(level, ATL_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#define PMD_TX_LOG(level, ...) \
	RTE_LOG_DP_LINE_PREFIX(level, ATL_DRIVER, "%s(): ", __func__, __VA_ARGS__)

extern int atl_logtype_driver;
#define RTE_LOGTYPE_ATL_DRIVER atl_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ATL_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif
