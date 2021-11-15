/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef _ENETFEC_LOGS_H_
#define _ENETFEC_LOGS_H_

#include <rte_log.h>

extern int enetfec_logtype_pmd;

/* PMD related logs */
#define ENETFEC_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, enetfec_logtype_pmd, "\nfec_net: %s()" \
		fmt "\n", __func__, ##args)

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
#define ENETFEC_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#endif /* _ENETFEC_LOGS_H_ */
