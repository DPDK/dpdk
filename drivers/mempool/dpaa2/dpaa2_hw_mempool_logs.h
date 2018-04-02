/*-
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef _DPAA2_HW_MEMPOOL_LOGS_H_
#define _DPAA2_HW_MEMPOOL_LOGS_H_

extern int dpaa2_logtype_mempool;

#define DPAA2_MEMPOOL_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa2_logtype_mempool, \
		"mempool/dpaa2: " fmt "\n", ##args)

/* Debug logs are with Function names */
#define DPAA2_MEMPOOL_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa2_logtype_mempool, \
		"mempool/dpaa2: %s(): " fmt "\n", __func__, ##args)

#define DPAA2_MEMPOOL_INFO(fmt, args...) \
	DPAA2_MEMPOOL_LOG(INFO, fmt, ## args)
#define DPAA2_MEMPOOL_ERR(fmt, args...) \
	DPAA2_MEMPOOL_LOG(ERR, fmt, ## args)
#define DPAA2_MEMPOOL_WARN(fmt, args...) \
	DPAA2_MEMPOOL_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_MEMPOOL_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#define DPAA2_MEMPOOL_DP_DEBUG(fmt, args...) \
	DPAA2_MEMPOOL_DP_LOG(DEBUG, fmt, ## args)
#define DPAA2_MEMPOOL_DP_INFO(fmt, args...) \
	DPAA2_MEMPOOL_DP_LOG(INFO, fmt, ## args)
#define DPAA2_MEMPOOL_DP_WARN(fmt, args...) \
	DPAA2_MEMPOOL_DP_LOG(WARNING, fmt, ## args)

#endif /* _DPAA2_HW_MEMPOOL_LOGS_H_ */
