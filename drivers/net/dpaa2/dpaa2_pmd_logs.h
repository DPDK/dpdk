/*-
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _DPAA2_PMD_LOGS_H_
#define _DPAA2_PMD_LOGS_H_

extern int dpaa2_logtype_pmd;

#define DPAA2_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa2_logtype_pmd, "dpaa2_net: " \
		fmt "\n", ##args)

#define DPAA2_PMD_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa2_logtype_pmd, "dpaa2_net: %s(): "\
		fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() DPAA2_PMD_LOG(DEBUG, " >>")

#define DPAA2_PMD_CRIT(fmt, args...) \
	DPAA2_PMD_LOG(CRIT, fmt, ## args)
#define DPAA2_PMD_INFO(fmt, args...) \
	DPAA2_PMD_LOG(INFO, fmt, ## args)
#define DPAA2_PMD_ERR(fmt, args...) \
	DPAA2_PMD_LOG(ERR, fmt, ## args)
#define DPAA2_PMD_WARN(fmt, args...) \
	DPAA2_PMD_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_PMD_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#define DPAA2_PMD_DP_DEBUG(fmt, args...) \
	DPAA2_PMD_DP_LOG(DEBUG, fmt, ## args)
#define DPAA2_PMD_DP_INFO(fmt, args...) \
	DPAA2_PMD_DP_LOG(INFO, fmt, ## args)
#define DPAA2_PMD_DP_WARN(fmt, args...) \
	DPAA2_PMD_DP_LOG(WARNING, fmt, ## args)

#endif /* _DPAA2_PMD_LOGS_H_ */
