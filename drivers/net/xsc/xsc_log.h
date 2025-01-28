/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#ifndef _XSC_LOG_H_
#define _XSC_LOG_H_

#include <rte_log.h>

extern int xsc_logtype_init;
extern int xsc_logtype_driver;

#define RTE_LOGTYPE_XSC_INIT	xsc_logtype_init
#define RTE_LOGTYPE_XSC_DRV	xsc_logtype_driver

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, XSC_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, XSC_DRV, "%s(): ", __func__, __VA_ARGS__)

#endif /* _XSC_LOG_H_ */
