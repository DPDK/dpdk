/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _VMXNET3_LOGS_H_
#define _VMXNET3_LOGS_H_

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_INIT
#define PMD_INIT_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")
#else
#define PMD_INIT_LOG(level, fmt, args...) do { } while(0)
#define PMD_INIT_FUNC_TRACE() do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_TX_FREE
#define PMD_TX_FREE_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_TX_FREE_LOG(level, fmt, args...) do { } while(0)
#endif

#ifdef RTE_LIBRTE_VMXNET3_DEBUG_DRIVER
#define PMD_DRV_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_DRV_LOG(level, fmt, args...) do { } while(0)
#endif

#endif /* _VMXNET3_LOGS_H_ */
