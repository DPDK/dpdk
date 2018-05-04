/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __DPAA2_CMDIF_LOGS_H__
#define __DPAA2_CMDIF_LOGS_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int dpaa2_cmdif_logtype;

#define DPAA2_CMDIF_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa2_cmdif_logtype, "dpaa2_cmdif: " \
		fmt "\n", ## args)

#define DPAA2_CMDIF_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa2_cmdif_logtype, "dpaa2_cmdif: %s(): " \
		fmt "\n", __func__, ## args)

#define DPAA2_CMDIF_FUNC_TRACE() DPAA2_CMDIF_LOG(DEBUG, ">>")

#define DPAA2_CMDIF_INFO(fmt, args...) \
	DPAA2_CMDIF_LOG(INFO, fmt, ## args)
#define DPAA2_CMDIF_ERR(fmt, args...) \
	DPAA2_CMDIF_LOG(ERR, fmt, ## args)
#define DPAA2_CMDIF_WARN(fmt, args...) \
	DPAA2_CMDIF_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_CMDIF_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, "dpaa2_cmdif: " fmt "\n", ## args)

#define DPAA2_CMDIF_DP_DEBUG(fmt, args...) \
	DPAA2_CMDIF_DP_LOG(DEBUG, fmt, ## args)
#define DPAA2_CMDIF_DP_INFO(fmt, args...) \
	DPAA2_CMDIF_DP_LOG(INFO, fmt, ## args)
#define DPAA2_CMDIF_DP_WARN(fmt, args...) \
	DPAA2_CMDIF_DP_LOG(WARNING, fmt, ## args)

#ifdef __cplusplus
}
#endif

#endif /* __DPAA2_CMDIF_LOGS_H__ */
