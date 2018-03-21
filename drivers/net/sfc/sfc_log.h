/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2016-2018 Solarflare Communications Inc.
 * All rights reserved.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_LOG_H_
#define _SFC_LOG_H_

/** Generic driver log type */
extern uint32_t sfc_logtype_driver;

/** Common log type name prefix */
#define SFC_LOGTYPE_PREFIX	"pmd.net.sfc."

/** Log PMD generic message, add a prefix and a line break */
#define SFC_GENERIC_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, sfc_logtype_driver,			\
		RTE_FMT("PMD: " RTE_FMT_HEAD(__VA_ARGS__ ,) "\n",	\
			RTE_FMT_TAIL(__VA_ARGS__ ,)))

/** Name prefix for the per-device log type used to report basic information */
#define SFC_LOGTYPE_MAIN_STR	SFC_LOGTYPE_PREFIX "main"

/** Device MCDI log type name prefix */
#define SFC_LOGTYPE_MCDI_STR	SFC_LOGTYPE_PREFIX "mcdi"

/** Level value used by MCDI log statements */
#define SFC_LOG_LEVEL_MCDI	RTE_LOG_INFO

/* Log PMD message, automatically add prefix and \n */
#define SFC_LOG(sa, level, type, ...) \
	do {								\
		const struct sfc_adapter *__sa = (sa);			\
									\
		rte_log(level, type,					\
			RTE_FMT("PMD: sfc_efx " PCI_PRI_FMT " #%" PRIu8	\
				": " RTE_FMT_HEAD(__VA_ARGS__ ,) "\n",	\
				__sa->pci_addr.domain,			\
				__sa->pci_addr.bus,			\
				__sa->pci_addr.devid,			\
				__sa->pci_addr.function,		\
				__sa->port_id,				\
				RTE_FMT_TAIL(__VA_ARGS__,)));		\
	} while (0)

#define sfc_err(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, RTE_LOG_ERR, _sa->logtype_main,		\
			__VA_ARGS__);					\
	} while (0)

#define sfc_warn(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, RTE_LOG_WARNING, _sa->logtype_main,	\
			__VA_ARGS__);					\
	} while (0)

#define sfc_notice(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, RTE_LOG_NOTICE, _sa->logtype_main,		\
			__VA_ARGS__);					\
	} while (0)

#define sfc_info(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, RTE_LOG_INFO, _sa->logtype_main,		\
			__VA_ARGS__);					\
	} while (0)

#define sfc_log_init(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, RTE_LOG_INFO, _sa->logtype_main,		\
			RTE_FMT("%s(): "				\
				RTE_FMT_HEAD(__VA_ARGS__ ,),		\
				__func__,				\
				RTE_FMT_TAIL(__VA_ARGS__ ,)));		\
	} while (0)

#define sfc_log_mcdi(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa, SFC_LOG_LEVEL_MCDI, _sa->mcdi.logtype,	\
			__VA_ARGS__);					\
	} while (0)


#endif /* _SFC_LOG_H_ */
