/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_DEBUG_H_
#define _SFC_DEBUG_H_

#include <rte_debug.h>

#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
/* Avoid dependency from RTE_LOG_DP_LEVEL to be able to enable debug check
 * in the driver only.
 */
#define SFC_ASSERT(exp)			RTE_VERIFY(exp)
#else
/* If the driver debug is not enabled, follow DPDK debug/non-debug */
#define SFC_ASSERT(exp)			RTE_ASSERT(exp)
#endif

/* Log PMD message, automatically add prefix and \n */
#define sfc_panic(sa, fmt, args...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		rte_panic("sfc " PCI_PRI_FMT				\
			  " #%" PRIu16 ": " fmt "\n",			\
			  _sa->pci_addr.domain, _sa->pci_addr.bus,	\
			  _sa->pci_addr.devid, _sa->pci_addr.function,	\
			  _sa->port_id, ##args);			\
	} while (0)

#endif /* _SFC_DEBUG_H_ */
