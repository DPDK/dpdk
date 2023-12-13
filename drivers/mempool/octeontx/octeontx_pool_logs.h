/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_POOL_LOGS_H__
#define	__OCTEONTX_POOL_LOGS_H__

#include <rte_debug.h>

#define FPAVF_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, OCTEONTX_FPAVF, "%s() line %u: ", \
		__func__ RTE_LOG_COMMA __LINE__, __VA_ARGS__)

#define fpavf_log_info(fmt, ...) FPAVF_LOG(INFO, fmt, ##__VA_ARGS__)
#define fpavf_log_dbg(fmt, ...) FPAVF_LOG(DEBUG, fmt, ##__VA_ARGS__)
#define fpavf_log_err(fmt, ...) FPAVF_LOG(ERR, fmt, ##__VA_ARGS__)
#define fpavf_func_trace fpavf_log_dbg


extern int octeontx_logtype_fpavf;
#define RTE_LOGTYPE_OCTEONTX_FPAVF octeontx_logtype_fpavf

#endif /* __OCTEONTX_POOL_LOGS_H__*/
