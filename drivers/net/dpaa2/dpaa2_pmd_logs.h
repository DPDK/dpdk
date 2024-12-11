/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _DPAA2_PMD_LOGS_H_
#define _DPAA2_PMD_LOGS_H_

extern int dpaa2_logtype_pmd;
#define RTE_LOGTYPE_DPAA2_NET dpaa2_logtype_pmd

#define DPAA2_PMD_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_NET, __VA_ARGS__)

#define DPAA2_PMD_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_NET, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() DPAA2_PMD_DEBUG(">>")

#define DPAA2_PMD_CRIT(fmt, ...) \
	DPAA2_PMD_LOG(CRIT, fmt, ## __VA_ARGS__)
#define DPAA2_PMD_INFO(fmt, ...) \
	DPAA2_PMD_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_PMD_ERR(fmt, ...) \
	DPAA2_PMD_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_PMD_WARN(fmt, ...) \
	DPAA2_PMD_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_PMD_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA2_NET, __VA_ARGS__)

#define DPAA2_PMD_DP_DEBUG(fmt, ...) \
	DPAA2_PMD_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA2_PMD_DP_INFO(fmt, ...) \
	DPAA2_PMD_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_PMD_DP_WARN(fmt, ...) \
	DPAA2_PMD_DP_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _DPAA2_PMD_LOGS_H_ */
