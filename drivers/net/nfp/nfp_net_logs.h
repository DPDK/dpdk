/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014, 2015 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef _NFP_NET_LOGS_H_
#define _NFP_NET_LOGS_H_

#include <rte_log.h>

extern int nfp_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_init, \
		"%s(): " fmt "\n", __func__, ## args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_LIBRTE_NFP_NET_DEBUG_RX
#define PMD_RX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s() rx: " fmt "\n", __func__, ## args)
#else
#define PMD_RX_LOG(level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_LIBRTE_NFP_NET_DEBUG_TX
#define PMD_TX_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s() tx: " fmt "\n", __func__, ## args)
#define ASSERT(x) if (!(x)) rte_panic("NFP_NET: x")
#else
#define PMD_TX_LOG(level, fmt, args...) do { } while (0)
#define ASSERT(x) do { } while (0)
#endif

#define RTE_LIBRTE_NFP_NET_DEBUG_CPP

#ifdef RTE_LIBRTE_NFP_NET_DEBUG_CPP
#define PMD_CPP_LOG(level, fmt, args...) \
	RTE_LOG(level, PMD, "%s(): " fmt "\n", __func__, ## args)
#else
#define PMD_CPP_LOG(level, fmt, args...) do { } while (0)
#endif

extern int nfp_logtype_driver;
#define PMD_DRV_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, nfp_logtype_driver, \
		"%s(): " fmt "\n", __func__, ## args)

#endif /* _NFP_NET_LOGS_H_ */
