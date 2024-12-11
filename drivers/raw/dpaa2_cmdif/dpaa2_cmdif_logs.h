/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef __DPAA2_CMDIF_LOGS_H__
#define __DPAA2_CMDIF_LOGS_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int dpaa2_cmdif_logtype;
#define RTE_LOGTYPE_DPAA2_CMDIF dpaa2_cmdif_logtype

#define DPAA2_CMDIF_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_CMDIF, __VA_ARGS__)

#define DPAA2_CMDIF_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_CMDIF, "%s(): ", __func__, __VA_ARGS__)

#define DPAA2_CMDIF_FUNC_TRACE() DPAA2_CMDIF_DEBUG(">>")

#define DPAA2_CMDIF_INFO(fmt, ...) \
	DPAA2_CMDIF_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_CMDIF_ERR(fmt, ...) \
	DPAA2_CMDIF_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_CMDIF_WARN(fmt, ...) \
	DPAA2_CMDIF_LOG(WARNING, fmt, ## __VA_ARGS__)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA2_CMDIF_DP_LOG(level, ...) \
	RTE_LOG_DP_LINE(level, DPAA2_CMDIF, __VA_ARGS__)

#define DPAA2_CMDIF_DP_DEBUG(fmt, ...) \
	DPAA2_CMDIF_DP_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define DPAA2_CMDIF_DP_INFO(fmt, ...) \
	DPAA2_CMDIF_DP_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_CMDIF_DP_WARN(fmt, ...) \
	DPAA2_CMDIF_DP_LOG(WARNING, fmt, ## __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __DPAA2_CMDIF_LOGS_H__ */
