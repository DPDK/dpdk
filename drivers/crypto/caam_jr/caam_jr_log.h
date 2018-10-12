/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#ifndef _CAAM_JR_LOG_H_
#define _CAAM_JR_LOG_H_

#include <rte_log.h>

extern int caam_jr_logtype;

#define CAAM_JR_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, caam_jr_logtype, "caam_jr: " \
		fmt "\n", ##args)

#define CAAM_JR_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, caam_jr_logtype, "caam_jr: %s(): " \
		fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() CAAM_JR_DEBUG(" >>")

#define CAAM_JR_INFO(fmt, args...) \
	CAAM_JR_LOG(INFO, fmt, ## args)
#define CAAM_JR_ERR(fmt, args...) \
	CAAM_JR_LOG(ERR, fmt, ## args)
#define CAAM_JR_WARN(fmt, args...) \
	CAAM_JR_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define CAAM_JR_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt "\n", ## args)

#define CAAM_JR_DP_DEBUG(fmt, args...) \
	CAAM_JR_DP_LOG(DEBUG, fmt, ## args)
#define CAAM_JR_DP_INFO(fmt, args...) \
	CAAM_JR_DP_LOG(INFO, fmt, ## args)
#define CAAM_JR_DP_WARN(fmt, args...) \
	CAAM_JR_DP_LOG(WARNING, fmt, ## args)
#define CAAM_JR_DP_ERR(fmt, args...) \
	CAAM_JR_DP_LOG(ERR, fmt, ## args)

#endif /* _CAAM_JR_LOG_H_ */
