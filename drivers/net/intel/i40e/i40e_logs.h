/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef _I40E_LOGS_H_
#define _I40E_LOGS_H_

extern int i40e_logtype_init;
#define RTE_LOGTYPE_I40E_INIT i40e_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, I40E_INIT, "%s(): ", __func__, __VA_ARGS__)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_ETHDEV_DEBUG_RX
extern int i40e_logtype_rx;
#define RTE_LOGTYPE_I40E_RX i40e_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, I40E_RX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int i40e_logtype_tx;
#define RTE_LOGTYPE_I40E_TX i40e_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, I40E_TX, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

extern int i40e_logtype_driver;
#define RTE_LOGTYPE_I40E_DRIVER i40e_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, I40E_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _I40E_LOGS_H_ */
