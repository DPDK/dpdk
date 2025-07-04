/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017-2018 NXP
 *
 */

#ifndef _DPAA_SEC_LOG_H_
#define _DPAA_SEC_LOG_H_

extern int dpaa_logtype_sec;
#define RTE_LOGTYPE_DPAA_SEC dpaa_logtype_sec

#define DPAA_SEC_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA_SEC, __VA_ARGS__)

#define DPAA_SEC_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA_SEC, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() DPAA_SEC_DEBUG(" >>")

#define DPAA_SEC_INFO(fmt, ...) \
	DPAA_SEC_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA_SEC_ERR(fmt, ...) \
	DPAA_SEC_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA_SEC_WARN(fmt, ...) \
	DPAA_SEC_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_SEC_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA_SEC, __VA_ARGS__)

#define DPAA_SEC_DP_DEBUG(fmt, ...) \
	DPAA_SEC_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA_SEC_DP_INFO(fmt, ...) \
	DPAA_SEC_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA_SEC_DP_WARN(fmt, ...) \
	DPAA_SEC_DP_LOG(WARNING, fmt, ## __VA_ARGS__)
#define DPAA_SEC_DP_ERR(fmt, ...) \
	DPAA_SEC_DP_LOG(ERR, fmt, ## __VA_ARGS__)

#endif /* _DPAA_SEC_LOG_H_ */
