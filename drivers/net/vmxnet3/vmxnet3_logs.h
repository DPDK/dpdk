/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VMXNET3_LOGS_H_
#define _VMXNET3_LOGS_H_

extern int vmxnet3_logtype_init;
#define RTE_LOGTYPE_VMXNET3_INIT vmxnet3_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VMXNET3_INIT, "%s(): ", __func__, __VA_ARGS__)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_RX
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VMXNET3_DRIVER, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_TX
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VMXNET3_DRIVER, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VMXNET3_DRIVER, "%s(): ", __func__, __VA_ARGS__)
#else
#define PMD_TX_FREE_LOG(...) do { } while(0)
#endif

extern int vmxnet3_logtype_driver;
#define RTE_LOGTYPE_VMXNET3_DRIVER vmxnet3_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, VMXNET3_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#endif /* _VMXNET3_LOGS_H_ */
