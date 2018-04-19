/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2017-2018 NXP
 *
 */

#ifndef _DPAA_SEC_LOG_H_
#define _DPAA_SEC_LOG_H_

extern int dpaa_logtype_sec;

#define DPAA_SEC_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaa_logtype_sec, "dpaa_sec: " \
		fmt "\n", ##args)

#define DPAA_SEC_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaa_logtype_sec, "dpaa_sec: %s(): " \
		fmt "\n", __func__, ##args)

#define PMD_INIT_FUNC_TRACE() DPAA_SEC_LOG(DEBUG, " >>")

#define DPAA_SEC_INFO(fmt, args...) \
	DPAA_SEC_LOG(INFO, fmt, ## args)
#define DPAA_SEC_ERR(fmt, args...) \
	DPAA_SEC_LOG(ERR, fmt, ## args)
#define DPAA_SEC_WARN(fmt, args...) \
	DPAA_SEC_LOG(WARNING, fmt, ## args)

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAA_SEC_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#define DPAA_SEC_DP_DEBUG(fmt, args...) \
	DPAA_SEC_DP_LOG(DEBUG, fmt, ## args)
#define DPAA_SEC_DP_INFO(fmt, args...) \
	DPAA_SEC_DP_LOG(INFO, fmt, ## args)
#define DPAA_SEC_DP_WARN(fmt, args...) \
	DPAA_SEC_DP_LOG(WARNING, fmt, ## args)
#define DPAA_SEC_DP_ERR(fmt, args...) \
	DPAA_SEC_DP_LOG(ERR, fmt, ## args)

#endif /* _DPAA_SEC_LOG_H_ */
