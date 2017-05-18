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

#ifndef _SFC_LOG_H_
#define _SFC_LOG_H_

/* Log PMD message, automatically add prefix and \n */
#define SFC_LOG(sa, level, ...) \
	do {								\
		const struct sfc_adapter *__sa = (sa);			\
									\
		RTE_LOG(level, PMD,					\
			RTE_FMT("sfc_efx " PCI_PRI_FMT " #%" PRIu8 ": "	\
				RTE_FMT_HEAD(__VA_ARGS__,) "\n",	\
				__sa->pci_addr.domain,			\
				__sa->pci_addr.bus,			\
				__sa->pci_addr.devid,			\
				__sa->pci_addr.function,		\
				__sa->port_id,				\
				RTE_FMT_TAIL(__VA_ARGS__,)));		\
	} while (0)

#define sfc_err(sa, ...) \
	SFC_LOG(sa, ERR, __VA_ARGS__)

#define sfc_warn(sa, ...) \
	SFC_LOG(sa, WARNING, __VA_ARGS__)

#define sfc_notice(sa, ...) \
	SFC_LOG(sa, NOTICE, __VA_ARGS__)

#define sfc_info(sa, ...) \
	SFC_LOG(sa, INFO, __VA_ARGS__)

#define sfc_log_init(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		if (_sa->debug_init)					\
			SFC_LOG(_sa, INFO,				\
				RTE_FMT("%s(): "			\
					RTE_FMT_HEAD(__VA_ARGS__,),	\
					__func__,			\
					RTE_FMT_TAIL(__VA_ARGS__,)));	\
	} while (0)

#endif /* _SFC_LOG_H_ */
