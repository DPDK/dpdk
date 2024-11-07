/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023-2024 Broadcom
 * All rights reserved.
 */

#ifndef __CFA_DEBUG_DEFS_H_
#define __CFA_DEBUG_DEFS_H_

#include <rte_log.h>

extern int bnxt_logtype_driver;

/*
 * The cfa_trace infrastructure assumes that log level debug is the lowest
 * numerically.  This is true for firmware, but the RTE log levels have debug as
 * the highest level.  Need to provide a conversion for calling rte_log.  We
 * don't want to simply use the RTE log levels since there are checks such as:
 *
 * #if CFA_COMP_DBG_LEVEL(COMP_ID) <= CFA_DEBUG_LEVEL_DBG
 *
 * Those checks would not have the desired effect if the RTE log levels are
 * substituted for the CFA log levels like this:
 *
 * #define CFA_DEBUG_LEVEL_DBG RTE_LOG_DEBUG
 * #define CFA_DEBUG_LEVEL_INFO RTE_LOG_INFO
 * #define CFA_DEBUG_LEVEL_WARN RTE_LOG_WARNING
 * #define CFA_DEBUG_LEVEL_CRITICAL RTE_LOG_CRIT
 * #define CFA_DEBUG_LEVEL_FATAL RTE_LOG_EMERG
 */

#define CFA_TO_RTE_LOG(level)                                                  \
	((level) == CFA_DEBUG_LEVEL_DBG ?                                      \
		 RTE_LOG_DEBUG :                                               \
		 (level) == CFA_DEBUG_LEVEL_INFO ?                             \
		 RTE_LOG_INFO :                                                \
		 (level) == CFA_DEBUG_LEVEL_WARN ?                             \
		 RTE_LOG_WARNING :                                             \
		 (level) == CFA_DEBUG_LEVEL_CRITICAL ? RTE_LOG_CRIT :          \
						       RTE_LOG_EMERG)

#define CFA_TRACE(level, ...)                                                  \
	rte_log(CFA_TO_RTE_LOG(level), bnxt_logtype_driver, ##__VA_ARGS__)

#endif /* __CFA_DEBUG_DEFS_H_ */
