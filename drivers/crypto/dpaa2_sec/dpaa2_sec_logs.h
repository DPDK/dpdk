/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016,2019 NXP
 *
 */

#ifndef _DPAA2_SEC_LOGS_H_
#define _DPAA2_SEC_LOGS_H_

extern int dpaa2_logtype_sec;
#define RTE_LOGTYPE_DPAA2_SEC dpaa2_logtype_sec

#define DPAA2_SEC_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_SEC, __VA_ARGS__)

#define DPAA2_SEC_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_SEC, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() DPAA2_SEC_DEBUG(">>")

#define DPAA2_SEC_INFO(fmt, ...) \
	DPAA2_SEC_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_SEC_ERR(fmt, ...) \
	DPAA2_SEC_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_SEC_WARN(fmt, ...) \
	DPAA2_SEC_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_SEC_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA2_SEC, __VA_ARGS__)

#define DPAA2_SEC_DP_DEBUG(fmt, ...) \
	DPAA2_SEC_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA2_SEC_DP_INFO(fmt, ...) \
	DPAA2_SEC_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_SEC_DP_WARN(fmt, ...) \
	DPAA2_SEC_DP_LOG(WARNING, fmt, ## __VA_ARGS__)
#define DPAA2_SEC_DP_ERR(fmt, ...) \
	DPAA2_SEC_DP_LOG(ERR, fmt, ## __VA_ARGS__)


#endif /* _DPAA2_SEC_LOGS_H_ */
