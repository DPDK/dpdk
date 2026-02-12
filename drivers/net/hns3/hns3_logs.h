/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_LOGS_H
#define HNS3_LOGS_H

extern int hns3_logtype_init;
#define RTE_LOGTYPE_HNS3_INIT hns3_logtype_init
#define PMD_INIT_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, HNS3_INIT, "%s(): ", __func__, __VA_ARGS__)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

extern int hns3_logtype_driver;
#define RTE_LOGTYPE_HNS3_DRIVER hns3_logtype_driver
#define PMD_DRV_LOG(hw, level, ...) \
	RTE_LOG_LINE_PREFIX(level, HNS3_DRIVER, "%s %s(): ", \
		(hw)->data->name RTE_LOG_COMMA __func__, __VA_ARGS__)

#define hns3_err(hw, fmt, ...) \
	PMD_DRV_LOG(hw, ERR, fmt, ## __VA_ARGS__)

#define hns3_warn(hw, fmt, ...) \
	PMD_DRV_LOG(hw, WARNING, fmt, ## __VA_ARGS__)

#define hns3_notice(hw, fmt, ...) \
	PMD_DRV_LOG(hw, NOTICE, fmt, ## __VA_ARGS__)

#define hns3_info(hw, fmt, ...) \
	PMD_DRV_LOG(hw, INFO, fmt, ## __VA_ARGS__)

#define hns3_dbg(hw, fmt, ...) \
	PMD_DRV_LOG(hw, DEBUG, fmt, ## __VA_ARGS__)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int hns3_logtype_rx;
#define RTE_LOGTYPE_HNS3_RX hns3_logtype_rx
#define PMD_RX_LOG(hw, level, ...) \
	RTE_LOG_LINE_PREFIX(level, HNS3_RX, "%s %s(): ", \
		(hw)->data->name RTE_LOG_COMMA __func__, __VA_ARGS__)
#else
#define PMD_RX_LOG(...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int hns3_logtype_tx;
#define RTE_LOGTYPE_HNS3_TX hns3_logtype_tx
#define PMD_TX_LOG(hw, level, ...) \
	RTE_LOG_LINE_PREFIX(level, HNS3_TX, "%s %s(): ", \
		(hw)->data->name RTE_LOG_COMMA __func__, __VA_ARGS__)
#else
#define PMD_TX_LOG(...) do { } while (0)
#endif

#endif /* HNS3_LOGS_H */
