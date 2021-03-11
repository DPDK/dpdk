/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
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
		const struct sfc_adapter_shared *_sas;			\
									\
		_sas = (sa)->priv.shared;				\
		rte_panic("sfc " PCI_PRI_FMT				\
			  " #%" PRIu16 ": " fmt "\n",			\
			  _sas->pci_addr.domain, _sas->pci_addr.bus,	\
			  _sas->pci_addr.devid, _sas->pci_addr.function,\
			  _sas->port_id, ##args);			\
	} while (0)

#endif /* _SFC_DEBUG_H_ */
