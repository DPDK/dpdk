/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef __THUNDERX_NICVF_LOGS__
#define __THUNDERX_NICVF_LOGS__

#include <assert.h>

#define PMD_INIT_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_INIT
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")
#else
#define PMD_INIT_FUNC_TRACE() do { } while (0)
#endif

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define NICVF_RX_ASSERT(x) assert(x)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#define NICVF_RX_ASSERT(x) do { } while (0)
#endif

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define NICVF_TX_ASSERT(x) assert(x)
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#define NICVF_TX_ASSERT(x) do { } while (0)
#endif

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_DRIVER
#define PMD_DRV_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define PMD_DRV_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")
#else
#define PMD_DRV_LOG(level, fmt, args...) do { } while (0)
#define PMD_DRV_FUNC_TRACE() do { } while (0)
#endif

#ifdef RTE_LIBRTE_THUNDERX_NICVF_DEBUG_MBOX
#define PMD_MBOX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#define PMD_MBOX_FUNC_TRACE() PMD_DRV_LOG(DEBUG, ">>")
#else
#define PMD_MBOX_LOG(level, fmt, args...) do { } while (0)
#define PMD_MBOX_FUNC_TRACE() do { } while (0)
#endif

#endif /* __THUNDERX_NICVF_LOGS__ */
