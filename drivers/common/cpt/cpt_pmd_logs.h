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
#define CPT_PMD_DRV_LOG_RAW(level, fmt, args...) \
		rte_log(RTE_LOG_ ## level, CPT_LOGTYPE, \
			"cpt: %s(): " fmt "\n", __func__, ##args)

#define CPT_PMD_INIT_FUNC_TRACE() CPT_PMD_DRV_LOG_RAW(DEBUG, " >>")

#define CPT_LOG_INFO(fmt, args...) \
	CPT_PMD_DRV_LOG_RAW(INFO, fmt, ## args)
#define CPT_LOG_WARN(fmt, args...) \
	CPT_PMD_DRV_LOG_RAW(WARNING, fmt, ## args)
#define CPT_LOG_ERR(fmt, args...) \
	CPT_PMD_DRV_LOG_RAW(ERR, fmt, ## args)

/*
 * DP logs, toggled out at compile time if level lower than current level.
 * DP logs would be logged under 'PMD' type. So for dynamic logging, the
 * level of 'pmd' has to be used.
 */
#define CPT_LOG_DP(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt "\n", ## args)

#define CPT_LOG_DP_DEBUG(fmt, args...) \
	CPT_LOG_DP(DEBUG, fmt, ## args)
#define CPT_LOG_DP_INFO(fmt, args...) \
	CPT_LOG_DP(INFO, fmt, ## args)
#define CPT_LOG_DP_WARN(fmt, args...) \
	CPT_LOG_DP(WARNING, fmt, ## args)
#define CPT_LOG_DP_ERR(fmt, args...) \
	CPT_LOG_DP(ERR, fmt, ## args)

#endif /* _CPT_PMD_LOGS_H_ */
