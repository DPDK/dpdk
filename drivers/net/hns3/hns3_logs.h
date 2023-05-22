/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#ifndef HNS3_LOGS_H
#define HNS3_LOGS_H

extern int hns3_logtype_init;
#define PMD_INIT_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, hns3_logtype_init, "%s(): " fmt "\n", \
		__func__, ##args)
#define PMD_INIT_FUNC_TRACE() PMD_INIT_LOG(DEBUG, " >>")

extern int hns3_logtype_driver;
#define PMD_DRV_LOG_RAW(hw, level, fmt, args...) \
	rte_log(level, hns3_logtype_driver, "%s %s(): " fmt, \
		(hw)->data->name, __func__, ## args)

#define hns3_err(hw, fmt, args...) \
	PMD_DRV_LOG_RAW(hw, RTE_LOG_ERR, fmt "\n", ## args)

#define hns3_warn(hw, fmt, args...) \
	PMD_DRV_LOG_RAW(hw, RTE_LOG_WARNING, fmt "\n", ## args)

#define hns3_notice(hw, fmt, args...) \
	PMD_DRV_LOG_RAW(hw, RTE_LOG_NOTICE, fmt "\n", ## args)

#define hns3_info(hw, fmt, args...) \
	PMD_DRV_LOG_RAW(hw, RTE_LOG_INFO, fmt "\n", ## args)

#define hns3_dbg(hw, fmt, args...) \
	PMD_DRV_LOG_RAW(hw, RTE_LOG_DEBUG, fmt "\n", ## args)

#ifdef RTE_ETHDEV_DEBUG_RX
extern int hns3_logtype_rx;
#define PMD_RX_LOG(hw, level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, hns3_logtype_rx,	"%s %s(): " fmt "\n", \
		(hw)->data->name, __func__, ## args)
#else
#define PMD_RX_LOG(hw, level, fmt, args...) do { } while (0)
#endif

#ifdef RTE_ETHDEV_DEBUG_TX
extern int hns3_logtype_tx;
#define PMD_TX_LOG(hw, level, fmt, args...)			\
	rte_log(RTE_LOG_ ## level, hns3_logtype_tx,	"%s %s(): " fmt "\n", \
		(hw)->data->name, __func__, ## args)
#else
#define PMD_TX_LOG(hw, level, fmt, args...) do { } while (0)
#endif

#endif /* HNS3_LOGS_H */
