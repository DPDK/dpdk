/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#ifndef _ICE_LOGS_H_
#define _ICE_LOGS_H_

extern int ice_logtype_init;
#define RTE_LOGTYPE_ICE_INIT ice_logtype_init
extern int ice_logtype_driver;
#define RTE_LOGTYPE_ICE_DRIVER ice_logtype_driver

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ICE_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_ETHDEV_DEBUG_RX
extern int ice_logtype_rx;
#define RTE_LOGTYPE_ICE_RX ice_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ICE_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int ice_logtype_tx;
#define RTE_LOGTYPE_ICE_TX ice_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ICE_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ICE_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _ICE_LOGS_H_ */
