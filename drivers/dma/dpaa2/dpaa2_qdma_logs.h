/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018, 2021 NXP
 */

#ifndef __DPAA2_QDMA_LOGS_H__
#define __DPAA2_QDMA_LOGS_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int dpaa2_qdma_logtype;
#define RTE_LOGTYPE_DPAA2_QDMA dpaa2_qdma_logtype

#define DPAA2_QDMA_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_QDMA, __VA_ARGS__)

#define DPAA2_QDMA_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_QDMA, "%s(): ", __func__, __VA_ARGS__)

#define DPAA2_QDMA_FUNC_TRACE() DPAA2_QDMA_DEBUG(">>")

#define DPAA2_QDMA_INFO(fmt, args...) \
	DPAA2_QDMA_LOG(INFO, fmt, ## args)
#define DPAA2_QDMA_ERR(fmt, args...) \
	DPAA2_QDMA_LOG(ERR, fmt, ## args)
#define DPAA2_QDMA_WARN(fmt, args...) \
	DPAA2_QDMA_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_QDMA_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA2_QDMA, __VA_ARGS__)

#define DPAA2_QDMA_DP_DEBUG(fmt, args...) \
	DPAA2_QDMA_DP_LOG(DEBUG, fmt, ## args)
#define DPAA2_QDMA_DP_INFO(fmt, args...) \
	DPAA2_QDMA_DP_LOG(INFO, fmt, ## args)
#define DPAA2_QDMA_DP_WARN(fmt, args...) \
	DPAA2_QDMA_DP_LOG(WARNING, fmt, ## args)

#ifdef __cplusplus
}
#endif

#endif /* __DPAA2_QDMA_LOGS_H__ */
