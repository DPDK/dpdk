/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020 Beijing WangXun Technology Co., Ltd.
 * Copyright(c) 2010-2017 Intel Corporation
 */

#ifndef _TXGBE_LOGS_H_
#define _TXGBE_LOGS_H_

#include <inttypes.h>

/*
 * PMD_USER_LOG: for user
 */
extern int txgbe_logtype_init;
#define RTE_LOGTYPE_TXGBE_INIT txgbe_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, TXGBE_INIT, "%s(): ", __func__, __VA_ARGS__)

extern int txgbe_logtype_driver;
#define RTE_LOGTYPE_TXGBE_DRIVER txgbe_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, TXGBE_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#ifdef RTE_LIBRTE_TXGBE_DEBUG_RX
extern int txgbe_logtype_rx;
#define RTE_LOGTYPE_TXGBE_RX txgbe_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, TXGBE_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX
extern int txgbe_logtype_tx;
#define RTE_LOGTYPE_TXGBE_TX txgbe_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, TXGBE_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX_FREE
extern int txgbe_logtype_tx_free;
#define RTE_LOGTYPE_TXGBE_TX_FREE txgbe_logtype_tx_free
#define RTE_LOGTYPE_TXGBE_TX txgbe_logtype_tx
#define PMD_TX_FREE_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, TXGBE_TX_FREE, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_FREE_LOG(...) do { } while (0)
#endif

#define DEBUGOUT(fmt, ...)        PMD_DRV_LOG(DEBUG, fmt, ##__VA_ARGS__)
#define PMD_INIT_FUNC_TRACE()     PMD_DRV_LOG(DEBUG, ">>")

#endif /* _TXGBE_LOGS_H_ */
