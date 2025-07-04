/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _ENETC_LOGS_H_
#define _ENETC_LOGS_H_

extern int enetc_logtype_pmd;
#define RTE_LOGTYPE_ENETC_NET enetc_logtype_pmd

#define ENETC_PMD_LOG(level, ...) \
	RTE_LOG_LINE(level, ENETC_NET, __VA_ARGS__)

#define ENETC_PMD_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, ENETC_NET, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() ENETC_PMD_DEBUG(">>")

#define ENETC_PMD_CRIT(fmt, ...) \
	ENETC_PMD_LOG(CRIT, fmt, ## __VA_ARGS__)
#define ENETC_PMD_INFO(fmt, ...) \
	ENETC_PMD_LOG(INFO, fmt, ## __VA_ARGS__)
#define ENETC_PMD_NOTICE(fmt, ...) \
	ENETC_PMD_LOG(NOTICE, fmt, ## __VA_ARGS__)
#define ENETC_PMD_ERR(fmt, ...) \
	ENETC_PMD_LOG(ERR, fmt, ## __VA_ARGS__)
#define ENETC_PMD_WARN(fmt, ...) \
	ENETC_PMD_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define ENETC_PMD_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, ENETC_NET, __VA_ARGS__)

#define ENETC_PMD_DP_DEBUG(fmt, ...) \
	ENETC_PMD_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define ENETC_PMD_DP_INFO(fmt, ...) \
	ENETC_PMD_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define ENETC_PMD_DP_WARN(fmt, ...) \
	ENETC_PMD_DP_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _ENETC_LOGS_H_*/
