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
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, txgbe_logtype_init, \
		"%s(): " fmt "\n", __func__, ##args)

extern int txgbe_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, txgbe_logtype_driver, \
		"%s(): " fmt "\n", __func__, ##args)

#ifdef RTE_LIBRTE_TXGBE_DEBUG_RX
extern int txgbe_logtype_rx;
#define PMD_RX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, txgbe_logtype_rx,	\
		"%s(): " fmt "\n", __func__, ##args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX
extern int txgbe_logtype_tx;
#define PMD_TX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, txgbe_logtype_tx,	\
		"%s(): " fmt "\n", __func__, ##args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_TXGBE_DEBUG_TX_FREE
extern int txgbe_logtype_tx_free;
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, txgbe_logtype_tx_free,	\
		"%s(): " fmt "\n", __func__, ##args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while (0)
#endif

#define DEBUGOUT(fmt, args...)    PMD_DRV_LOG(DEBUG, fmt, ##args)
#define PMD_INIT_FUNC_TRACE()     PMD_DRV_LOG(DEBUG, ">>")

extern int txgbe_logtype_bp;
#define BP_LOG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, txgbe_logtype_bp, \
		"[%"PRIu64".%"PRIu64"]%s(%d): " fmt, \
		usec_stamp() / 1000000, usec_stamp() % 1000000, \
		__func__, __LINE__, ##args)

#endif /* _TXGBE_LOGS_H_ */
