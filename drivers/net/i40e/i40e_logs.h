/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _I40E_LOGS_H_
#define _I40E_LOGS_H_

extern int i40e_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, i40e_logtype_init, "%s(): " fmt "\n", \
		__func__, ##args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_I40E_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_I40E_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_I40E_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while(0)
#endif

extern int i40e_logtype_driver;
#define PMD_DRV_LOG_RAW(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, i40e_logtype_driver, "%s(): " fmt, \
		__func__, ## args)

#define PMD_DRV_LOG(level, fmt, args...) \
	PMD_DRV_LOG_RAW(level, fmt "\n", ## args)

#endif /* _I40E_LOGS_H_ */
