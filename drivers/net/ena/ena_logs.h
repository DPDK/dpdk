/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 */

#ifndef _ENA_LOGS_H_
#define _ENA_LOGS_H_

extern int ena_logtype_init;
#define RTE_LOGTYPE_ENA_INIT ena_logtype_init
#define PMD_INIT_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ENA_INIT, "%s(): ", __func__, __VA_ARGS__)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int ena_logtype_rx;
#define RTE_LOGTYPE_ENA_RX ena_logtype_rx
#define PMD_RX_LOG_LINE(level, ...)	\
	RTE_LOG_LINE_PREFIX(level, ENA_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG_LINE(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int ena_logtype_tx;
#define RTE_LOGTYPE_ENA_TX ena_logtype_tx
#define PMD_TX_LOG_LINE(level, ...)	\
	RTE_LOG_LINE_PREFIX(level, ENA_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG_LINE(...) do { } while (0)
#endif

extern int ena_logtype_driver;
#define RTE_LOGTYPE_ENA_DRIVER ena_logtype_driver
#define PMD_DRV_LOG_LINE(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ENA_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _ENA_LOGS_H_ */
