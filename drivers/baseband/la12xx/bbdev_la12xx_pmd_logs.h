/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 NXP
 */

#ifndef _BBDEV_LA12XX_PMD_LOGS_H_
#define _BBDEV_LA12XX_PMD_LOGS_H_

extern int bbdev_la12xx_logtype;
#define RTE_LOGTYPE_BBDEV_LA12XX bbdev_la12xx_logtype

#define rte_bbdev_log(level, ...) \
	RTE_LOG_LINE(level, BBDEV_LA12XX, __VA_ARGS__)

#ifdef RTE_LIBRTE_BBDEV_DEBUG
#define rte_bbdev_log_debug(...) \
	rte_bbdev_log(DEBUG, __VA_ARGS__)
#else
#define rte_bbdev_log_debug(...)
#endif

#define PMD_INIT_FUNC_TRACE() rte_bbdev_log_debug(">>")

/* DP Logs, toggled out at compile time if level lower than current level */
#define rte_bbdev_dp_log(level, ...) \
	RTE_LOG_DP_LINE(level, BBDEV_LA12XX, __VA_ARGS__)

#endif /* _BBDEV_LA12XX_PMD_LOGS_H_ */
