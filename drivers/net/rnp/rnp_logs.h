/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef __RNP_LOGS_H__
#define __RNP_LOGS_H__

#include <rte_log.h>

extern int rnp_init_logtype;
#define RTE_LOGTYPE_RNP_INIT rnp_init_logtype
#define RNP_PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, RNP_INIT, "%s(): ", __func__,  __VA_ARGS__)
#define PMD_INIT_FUNC_TRACE() RNP_PMD_INIT_LOG(DEBUG, " >>")
#define RNP_PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, RNP_INIT, \
			"%s(): ", __func__, __VA_ARGS__)
#define RNP_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, RNP_INIT, \
			"rnp_net: (%d) ", __LINE__, __VA_ARGS__)
#define RNP_PMD_ERR(fmt, ...) \
	RNP_PMD_LOG(ERR, fmt, ## __VA_ARGS__)
#define RNP_PMD_WARN(fmt, ...) \
	RNP_PMD_LOG(WARNING, fmt, ## __VA_ARGS__)
#define RNP_PMD_INFO(fmt, ...) \
	RNP_PMD_LOG(INFO, fmt, ## __VA_ARGS__)

#ifdef RTE_LIBRTE_RNP_REG_DEBUG
#define RNP_PMD_REG_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, RNP_INIT, \
			"%s(): ", __func__, __VA_ARGS__)
#else
#define RNP_PMD_REG_LOG(...) do { } while (0)
#endif

#endif /* __RNP_LOGS_H__ */
