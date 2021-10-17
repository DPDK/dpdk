/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 NXP
 */

#ifndef _BBDEV_LA12XX_PMD_LOGS_H_
#define _BBDEV_LA12XX_PMD_LOGS_H_

extern int bbdev_la12xx_logtype;

#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, bbdev_la12xx_logtype, fmt "\n", \
		##__VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(fmt, ...) \
		rte_bbdev_log(DEBUG, "la12xx_pmd: " fmt, \
		##__VA_ARGS__)
#else
#define rte_bbdev_log_debug(fmt, ...)
#endif

#define PMD_INIT_FUNC_TRACE() rte_bbdev_log_debug(">>")

/* DP Logs, toggled out at compile time if level lower than current level */
#define rte_bbdev_dp_log(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#endif /* _BBDEV_LA12XX_PMD_LOGS_H_ */
