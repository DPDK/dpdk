/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _NGBE_LOGS_H_
#define _NGBE_LOGS_H_

/*
 * PMD_USER_LOG: for user
 */
extern int ngbe_logtype_init;
#define RTE_LOGTYPE_NGBE_INIT ngbe_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NGBE_INIT, "%s(): ", __func__, __VA_ARGS__)

extern int ngbe_logtype_driver;
#define RTE_LOGTYPE_NGBE_DRIVER ngbe_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NGBE_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int ngbe_logtype_rx;
#define RTE_LOGTYPE_NGBE_RX ngbe_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NGBE_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int ngbe_logtype_tx;
#define RTE_LOGTYPE_NGBE_TX ngbe_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NGBE_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#define DEBUGOUT(fmt, ...)        PMD_DRV_LOG(DEBUG, fmt, ##__VA_ARGS__)
#define PMD_INIT_FUNC_TRACE()     PMD_DRV_LOG(DEBUG, ">>")

#endif /* _NGBE_LOGS_H_ */
