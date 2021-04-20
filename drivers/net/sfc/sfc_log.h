/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
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

#define SFC_LOG_PREFIX_MAX	32

/* Log PMD message, automatically add prefix and \n */
#define SFC_LOG(sas, level, type, ...) \
	do {								\
		const struct sfc_adapter_shared *_sas = (sas);		\
									\
		rte_log(level, type,					\
			RTE_FMT("%s" RTE_FMT_HEAD(__VA_ARGS__ ,) "\n",	\
				_sas->log_prefix,			\
				RTE_FMT_TAIL(__VA_ARGS__,)));		\
	} while (0)

#define sfc_err(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_ERR,			\
			_sa->priv.logtype_main, __VA_ARGS__);		\
	} while (0)

#define sfc_warn(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_WARNING,		\
			_sa->priv.logtype_main, __VA_ARGS__);		\
	} while (0)

#define sfc_notice(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_NOTICE,		\
			_sa->priv.logtype_main, __VA_ARGS__);		\
	} while (0)

#define sfc_info(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_INFO,			\
			_sa->priv.logtype_main, __VA_ARGS__);		\
	} while (0)

#define sfc_dbg(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_DEBUG,		\
			_sa->priv.logtype_main, __VA_ARGS__);		\
	} while (0)

#define sfc_log_init(sa, ...) \
	do {								\
		const struct sfc_adapter *_sa = (sa);			\
									\
		SFC_LOG(_sa->priv.shared, RTE_LOG_INFO,			\
			_sa->priv.logtype_main,				\
			RTE_FMT("%s(): "				\
				RTE_FMT_HEAD(__VA_ARGS__ ,),		\
				__func__,				\
				RTE_FMT_TAIL(__VA_ARGS__ ,)));		\
	} while (0)


#endif /* _SFC_LOG_H_ */
