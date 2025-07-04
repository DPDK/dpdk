/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _DPAA2_HW_MEMPOOL_LOGS_H_
#define _DPAA2_HW_MEMPOOL_LOGS_H_

extern int dpaa2_logtype_mempool;
#define RTE_LOGTYPE_DPAA2_MEMPOOL dpaa2_logtype_mempool

#define DPAA2_MEMPOOL_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_MEMPOOL, __VA_ARGS__)

/* Debug logs are with Function names */
#define DPAA2_MEMPOOL_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_MEMPOOL, "%s(): ", __func__, __VA_ARGS__)

#define DPAA2_MEMPOOL_INFO(fmt, ...) \
	DPAA2_MEMPOOL_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_MEMPOOL_ERR(fmt, ...) \
	DPAA2_MEMPOOL_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_MEMPOOL_WARN(fmt, ...) \
	DPAA2_MEMPOOL_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_MEMPOOL_DP_LOG(level, fmt, ...) \
	RTE_LOG_DP(level, DPAA2_MEMPOOL, fmt, ## __VA_ARGS__)

#define DPAA2_MEMPOOL_DP_DEBUG(fmt, ...) \
	DPAA2_MEMPOOL_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA2_MEMPOOL_DP_INFO(fmt, ...) \
	DPAA2_MEMPOOL_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_MEMPOOL_DP_WARN(fmt, ...) \
	DPAA2_MEMPOOL_DP_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _DPAA2_HW_MEMPOOL_LOGS_H_ */
