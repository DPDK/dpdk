/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_LOGS_H_
#define _NBL_LOGS_H_

#include <rte_log.h>

extern int nbl_logtype_init;
#define RTE_LOGTYPE_NBL_INIT nbl_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NBL_INIT, "%s(): ", __func__, __VA_ARGS__)

extern int nbl_logtype_driver;
#define RTE_LOGTYPE_NBL_DRIVER nbl_logtype_driver

#define NBL_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NBL_DRIVER, "%s: ", __func__, __VA_ARGS__)

#define NBL_ASSERT(exp)		RTE_VERIFY(exp)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, ">>")

#endif
