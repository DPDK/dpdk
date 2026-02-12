/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP_LOGS_H__
#define __NFP_LOGS_H__

#include <rte_log.h>

extern int nfp_logtype_init;
#define RTE_LOGTYPE_NFP_INIT nfp_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_INIT, "%s(): ", __func__, __VA_ARGS__)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int nfp_logtype_rx;
#define RTE_LOGTYPE_NFP_RX nfp_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int nfp_logtype_tx;
#define RTE_LOGTYPE_NFP_TX nfp_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

extern int nfp_logtype_cpp;
#define RTE_LOGTYPE_NFP_CPP nfp_logtype_cpp
#define PMD_CPP_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_CPP, "%s(): ", __func__, __VA_ARGS__)

extern int nfp_logtype_driver;
#define RTE_LOGTYPE_NFP_DRIVER nfp_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFP_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* __NFP_LOGS_H__ */
