/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _AVF_LOG_H_
#define _AVF_LOG_H_

extern int avf_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, avf_logtype_init, "%s(): " fmt "\n", \
		__func__, ## args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

extern int avf_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, avf_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_AVF_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_AVF_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_AVF_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while (0)
#endif

#endif /* _AVF_LOG_H_ */
