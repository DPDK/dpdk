/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#ifndef __DPAA_QDMA_LOGS_H__
#define __DPAA_QDMA_LOGS_H__

extern int dpaa_qdma_logtype;
#define RTE_LOGTYPE_DPAA_QDMA dpaa_qdma_logtype

#define DPAA_QDMA_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA_QDMA, __VA_ARGS__)

#define DPAA_QDMA_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA_QDMA, "%s(): ", __func__, __VA_ARGS__)

#define DPAA_QDMA_FUNC_TRACE() DPAA_QDMA_DEBUG(">>")

#define DPAA_QDMA_INFO(fmt, ...) \
	DPAA_QDMA_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA_QDMA_ERR(fmt, ...) \
	DPAA_QDMA_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA_QDMA_WARN(fmt, ...) \
	DPAA_QDMA_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_QDMA_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA_QDMA, __VA_ARGS__)

#define DPAA_QDMA_DP_DEBUG(fmt, ...) \
	DPAA_QDMA_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA_QDMA_DP_INFO(fmt, ...) \
	DPAA_QDMA_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA_QDMA_DP_WARN(fmt, ...) \
	DPAA_QDMA_DP_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* __DPAA_QDMA_LOGS_H__ */
