/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTOSS_SYSTEM_NTLOG_H
#define NTOSS_SYSTEM_NTLOG_H

#include <stdarg.h>
#include <stdint.h>
#include <rte_log.h>

extern int nt_logtype;

#define NT_DRIVER_NAME "ntnic"

#define NT_PMD_DRV_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, nt_logtype, \
		RTE_FMT(NT_DRIVER_NAME ": " \
			RTE_FMT_HEAD(__VA_ARGS__, ""), \
		RTE_FMT_TAIL(__VA_ARGS__, "")))


#define NT_LOG_ERR(...) NT_PMD_DRV_LOG(ERR, __VA_ARGS__)
#define NT_LOG_WRN(...) NT_PMD_DRV_LOG(WARNING, __VA_ARGS__)
#define NT_LOG_INF(...) NT_PMD_DRV_LOG(INFO, __VA_ARGS__)
#define NT_LOG_DBG(...) NT_PMD_DRV_LOG(DEBUG, __VA_ARGS__)

#define NT_LOG(level, module, ...) \
	NT_LOG_##level(#module ": " #level ":" __VA_ARGS__)

#define NT_LOG_DBGX(level, module, ...) \
		rte_log(RTE_LOG_ ##level, nt_logtype, \
				RTE_FMT(NT_DRIVER_NAME #module ": [%s:%u]" \
					RTE_FMT_HEAD(__VA_ARGS__, ""), __func__, __LINE__, \
				RTE_FMT_TAIL(__VA_ARGS__, "")))
/*
 * nt log helper functions
 * to create a string for NT_LOG usage to output a one-liner log
 * to use when one single function call to NT_LOG is not optimal - that is
 * you do not know the number of parameters at programming time or it is variable
 */
char *ntlog_helper_str_alloc(const char *sinit);

void ntlog_helper_str_add(char *s, const char *format, ...);

void ntlog_helper_str_free(char *s);

#endif	/* NTOSS_SYSTEM_NTLOG_H */
