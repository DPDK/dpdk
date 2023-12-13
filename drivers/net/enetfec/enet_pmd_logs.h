/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef _ENETFEC_LOGS_H_
#define _ENETFEC_LOGS_H_

#include <rte_log.h>

extern int enetfec_logtype_pmd;
#define RTE_LOGTYPE_ENETFEC_NET enetfec_logtype_pmd

/* PMD related logs */
#define ENETFEC_PMD_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ENETFEC_NET, "%s()", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() ENET_PMD_LOG(DEBUG, " >>")

#define ENETFEC_PMD_DEBUG(fmt, args...) \
	ENETFEC_PMD_LOG(DEBUG, fmt, ## args)
#define ENETFEC_PMD_ERR(fmt, args...) \
	ENETFEC_PMD_LOG(ERR, fmt, ## args)
#define ENETFEC_PMD_INFO(fmt, args...) \
	ENETFEC_PMD_LOG(INFO, fmt, ## args)

#define ENETFEC_PMD_WARN(fmt, args...) \
	ENETFEC_PMD_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define ENETFEC_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, ENETFEC_NET, __VA_ARGS__)

#endif /* _ENETFEC_LOGS_H_ */
