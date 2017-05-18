/*-
 *   BSD LICENSE
 *
 * Copyright (c) 2016-2017 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SFC_DEBUG_H_
#define _SFC_DEBUG_H_

#include <rte_debug.h>

#ifdef RTE_LIBRTE_SFC_EFX_DEBUG
/* Avoid dependency from RTE_LOG_LEVEL to be able to enable debug check
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
		rte_panic("sfc " PCI_PRI_FMT " #%" PRIu8 ": " fmt "\n",	\
			  _sa->pci_addr.domain, _sa->pci_addr.bus,	\
			  _sa->pci_addr.devid, _sa->pci_addr.function,	\
			  _sa->port_id, ##args);			\
	} while (0)

#endif /* _SFC_DEBUG_H_ */
