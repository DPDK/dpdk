/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
 */

#ifndef _CAAM_JR_LOG_H_
#define _CAAM_JR_LOG_H_

#include <rte_log.h>

extern int caam_jr_logtype;
#define RTE_LOGTYPE_CAAM_JR caam_jr_logtype

#define CAAM_JR_LOG(level, ...) \
	RTE_LOG_LINE(level, CAAM_JR, __VA_ARGS__)

#define CAAM_JR_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, CAAM_JR, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() CAAM_JR_DEBUG(" >>")

#define CAAM_JR_INFO(fmt, ...) \
	CAAM_JR_LOG(INFO, fmt, ## __VA_ARGS__)
#define CAAM_JR_ERR(fmt, ...) \
	CAAM_JR_LOG(ERR, fmt, ## __VA_ARGS__)
#define CAAM_JR_WARN(fmt, ...) \
	CAAM_JR_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define CAAM_JR_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, CAAM_JR, __VA_ARGS__)

#define CAAM_JR_DP_DEBUG(fmt, ...) \
	CAAM_JR_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define CAAM_JR_DP_INFO(fmt, ...) \
	CAAM_JR_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define CAAM_JR_DP_WARN(fmt, ...) \
	CAAM_JR_DP_LOG(WARNING, fmt, ## __VA_ARGS__)
#define CAAM_JR_DP_ERR(fmt, ...) \
	CAAM_JR_DP_LOG(ERR, fmt, ## __VA_ARGS__)

#endif /* _CAAM_JR_LOG_H_ */
