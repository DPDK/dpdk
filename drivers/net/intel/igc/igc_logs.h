/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#ifndef _IGC_LOGS_H_
#define _IGC_LOGS_H_

#include <rte_log.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int igc_logtype_init;
#define RTE_LOGTYPE_IGC_INIT igc_logtype_init
extern int igc_logtype_driver;
#define RTE_LOGTYPE_IGC_DRIVER igc_logtype_driver

#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IGC_INIT, "%s(): ", __func__, __VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

#ifdef RTE_ETHDEV_DEBUG_RX
#define PMD_RX_LOG(...) PMD_DRV_LOG(__VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
#define PMD_TX_LOG(...) PMD_DRV_LOG(__VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IGC_DRIVER, "%s(): ", __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* _IGC_LOGS_H_ */
