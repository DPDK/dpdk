/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_LOGS__
#define __THUNDERX_NICVF_LOGS__

#include <assert.h>

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_RX
#define NICVF_RX_ASSERT(x) assert(x)
#else
#define NICVF_RX_ASSERT(x) do { } while (0)
#endif

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_TX
#define NICVF_TX_ASSERT(x) assert(x)
#else
#define NICVF_TX_ASSERT(x) do { } while (0)
#endif

#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nicvf_logtype_init, \
			"%s(): " fmt "\n", __func__, ## args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")

#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nicvf_logtype_driver, \
			"%s(): " fmt "\n", __func__, ## args)
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")

#define PMD_MBOX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nicvf_logtype_mbox, \
			"%s(): " fmt "\n", __func__, ## args)
#define PMD_MBOX_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")

#define PMD_RX_LOG PMD_DRV_LOG
#define PMD_TX_LOG PMD_DRV_LOG

extern int nicvf_logtype_init;
extern int nicvf_logtype_driver;
extern int nicvf_logtype_mbox;

#endif /* __THUNDERX_NICVF_LOGS__ */
