/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef	__OCTEONTX_POOL_LOGS_H__
#define	__OCTEONTX_POOL_LOGS_H__

#include <rte_debug.h>

#ifdef RTE_LIBRTE_OCTEONTX_MEMPOOL_DEBUG
#define fpavf_log_info(fmt, args...) \
	RTE_LOG(INFO, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#define fpavf_log_dbg(fmt, args...) \
	RTE_LOG(DEBUG, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)

#define mbox_log_info(fmt, args...) \
	RTE_LOG(INFO, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#define mbox_log_dbg(fmt, args...) \
	RTE_LOG(DEBUG, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#else
#define fpavf_log_info(fmt, args...)
#define fpavf_log_dbg(fmt, args...)
#define mbox_log_info(fmt, args...)
#define mbox_log_dbg(fmt, args...)
#endif

#define fpavf_func_trace fpavf_log_dbg
#define fpavf_log_err(fmt, args...) \
	RTE_LOG(ERR, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)
#define mbox_func_trace mbox_log_dbg
#define mbox_log_err(fmt, args...) \
	RTE_LOG(ERR, PMD, "%s() line %u: " fmt "\n", \
		__func__, __LINE__, ## args)

#endif /* __OCTEONTX_POOL_LOGS_H__*/
