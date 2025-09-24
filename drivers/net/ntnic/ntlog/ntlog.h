/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTOSS_SYSTEM_NTLOG_H
#define NTOSS_SYSTEM_NTLOG_H

#include <stdarg.h>
#include <stdint.h>
#include <rte_log.h>

extern int nt_log_general;
#define RTE_LOGTYPE_GENERAL nt_log_general
extern int nt_log_nthw;
#define RTE_LOGTYPE_NTHW nt_log_nthw
extern int nt_log_filter;
#define RTE_LOGTYPE_FILTER nt_log_filter
extern int nt_log_ntnic;
#define RTE_LOGTYPE_NTNIC nt_log_ntnic

#define NT_DRIVER_NAME "ntnic"

/* Common log format */
#define NT_LOG_TEMPLATE_COM(level, module, ...) \
	RTE_LOG_LINE_PREFIX(level, module, "%s: ", NT_DRIVER_NAME, __VA_ARGS__)

/* Extended log format */
#define NT_LOG_TEMPLATE_EXT(level, module, ...) \
	RTE_LOG_LINE_PREFIX(level, module, "[%s:%u] ", __func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#define NT_PMD_DRV_GENERAL_LOG(level, module, format, ...) \
	NT_LOG_TEMPLATE_##format(level, module, __VA_ARGS__)

#define NT_PMD_DRV_NTHW_LOG(level, module, format, ...) \
	NT_LOG_TEMPLATE_##format(level, module, __VA_ARGS__)

#define NT_PMD_DRV_FILTER_LOG(level, module, format, ...) \
	NT_LOG_TEMPLATE_##format(level, module, __VA_ARGS__)

#define NT_PMD_DRV_NTNIC_LOG(level, module, format, ...) \
	NT_LOG_TEMPLATE_##format(level, module, __VA_ARGS__)

#define NT_LOG_ERR(level, module, ...) NT_PMD_DRV_##module##_LOG(ERR, module, COM, __VA_ARGS__)
#define NT_LOG_WRN(level, module, ...) NT_PMD_DRV_##module##_LOG(WARNING, module, COM, __VA_ARGS__)
#define NT_LOG_INF(level, module, ...) NT_PMD_DRV_##module##_LOG(INFO, module, COM, __VA_ARGS__)
#define NT_LOG_DBG(level, module, ...) NT_PMD_DRV_##module##_LOG(DEBUG, module, COM, __VA_ARGS__)

#define NT_LOG_DBGX_ERR(level, module, ...) \
	NT_PMD_DRV_##module##_LOG(ERR, module, EXT, __VA_ARGS__)
#define NT_LOG_DBGX_WRN(level, module, ...) \
	NT_PMD_DRV_##module##_LOG(WARNING, module, EXT, __VA_ARGS__)
#define NT_LOG_DBGX_INF(level, module, ...) \
	NT_PMD_DRV_##module##_LOG(INFO, module, EXT, __VA_ARGS__)
#define NT_LOG_DBGX_DBG(level, module, ...) \
	NT_PMD_DRV_##module##_LOG(DEBUG, module, EXT, __VA_ARGS__)

#define NT_LOG(level, module, ...) NT_LOG_##level(level, module, __VA_ARGS__)

#define NT_LOG_DBGX(level, module, ...) NT_LOG_DBGX_##level(level, module, __VA_ARGS__)

/*
 * nt log helper functions
 * to create a string for NT_LOG usage to output a one-liner log
 * to use when one single function call to NT_LOG is not optimal - that is
 * you do not know the number of parameters at programming time or it is variable
 */
char *nthw_log_helper_str_alloc(const char *sinit);
void nthw_log_helper_str_add(char *s, const char *format, ...);
void nthw_log_helper_str_free(char *s);

#endif	/* NTOSS_SYSTEM_NTLOG_H */
