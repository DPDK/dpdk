/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Cavium, Inc
 */

#ifndef _CPT_PMD_LOGS_H_
#define _CPT_PMD_LOGS_H_

#include <rte_log.h>

/*
 * This file defines log macros
 */

/*
 * otx*_cryptodev.h file would define the CPT_LOGTYPE macro for the
 * platform.
 */
#define RTE_LOGTYPE_CPT CPT_LOGTYPE

#define CPT_PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, CPT, "%s(): ", __func__, __VA_ARGS__)

#define CPT_PMD_INIT_FUNC_TRACE() CPT_PMD_DRV_LOG(DEBUG, " >>")

#define CPT_LOG_INFO(...) \
	CPT_PMD_DRV_LOG(INFO, __VA_ARGS__)
#define CPT_LOG_WARN(...) \
	CPT_PMD_DRV_LOG(WARNING, __VA_ARGS__)
#define CPT_LOG_ERR(...) \
	CPT_PMD_DRV_LOG(ERR, __VA_ARGS__)

/*
 * DP logs, toggled out at compile time if level lower than current level.
 */
#define CPT_LOG_DP(level, ...) \
	RTE_LOG_DP_LINE(level, CPT, __VA_ARGS__)

#define CPT_LOG_DP_DEBUG(...) \
	CPT_LOG_DP(DEBUG, __VA_ARGS__)
#define CPT_LOG_DP_INFO(...) \
	CPT_LOG_DP(INFO, __VA_ARGS__)
#define CPT_LOG_DP_WARN(...) \
	CPT_LOG_DP(WARNING, __VA_ARGS__)
#define CPT_LOG_DP_ERR(...) \
	CPT_LOG_DP(ERR, __VA_ARGS__)

#endif /* _CPT_PMD_LOGS_H_ */
