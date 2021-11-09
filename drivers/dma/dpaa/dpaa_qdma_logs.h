/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#ifndef __DPAA_QDMA_LOGS_H__
#define __DPAA_QDMA_LOGS_H__

extern int dpaa_qdma_logtype;

#define DPAA_QDMA_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_qdma_logtype, "dpaa_qdma: " \
		fmt "\n", ## args)

#define DPAA_QDMA_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa_qdma_logtype, "dpaa_qdma: %s(): " \
		fmt "\n", __func__, ## args)

#define DPAA_QDMA_FUNC_TRACE() DPAA_QDMA_DEBUG(">>")

#define DPAA_QDMA_INFO(fmt, args...) \
	DPAA_QDMA_LOG(INFO, fmt, ## args)
#define DPAA_QDMA_ERR(fmt, args...) \
	DPAA_QDMA_LOG(ERR, fmt, ## args)
#define DPAA_QDMA_WARN(fmt, args...) \
	DPAA_QDMA_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_QDMA_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, "dpaa_qdma: " fmt "\n", ## args)

#define DPAA_QDMA_DP_DEBUG(fmt, args...) \
	DPAA_QDMA_DP_LOG(DEBUG, fmt, ## args)
#define DPAA_QDMA_DP_INFO(fmt, args...) \
	DPAA_QDMA_DP_LOG(INFO, fmt, ## args)
#define DPAA_QDMA_DP_WARN(fmt, args...) \
	DPAA_QDMA_DP_LOG(WARNING, fmt, ## args)

#endif /* __DPAA_QDMA_LOGS_H__ */
