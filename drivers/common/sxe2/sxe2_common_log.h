/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef SXE2_COMMON_LOG_H
#define SXE2_COMMON_LOG_H

extern int32_t sxe2_common_log;
extern int32_t sxe2_log_init;
extern int32_t sxe2_log_driver;
extern int32_t sxe2_log_rx;
extern int32_t sxe2_log_tx;
extern int32_t sxe2_log_hw;

#define RTE_LOGTYPE_SXE2_COM  sxe2_common_log
#define RTE_LOGTYPE_SXE2_INIT sxe2_log_init
#define RTE_LOGTYPE_SXE2_DRV  sxe2_log_driver
#define RTE_LOGTYPE_SXE2_RX   sxe2_log_rx
#define RTE_LOGTYPE_SXE2_TX   sxe2_log_tx
#define RTE_LOGTYPE_SXE2_HW   sxe2_log_hw

#define SXE2_PMD_LOG(level, log_type, ...) \
	RTE_LOG_LINE_PREFIX(level, log_type, "%s(): ", \
		__func__, __VA_ARGS__)

#define SXE2_PMD_DRV_LOG(level, log_type, adapter, ...) \
	RTE_LOG_LINE_PREFIX(level, log_type, "%s(): port:%u ", \
		__func__ RTE_LOG_COMMA \
		adapter->dev_port_id, __VA_ARGS__)

#define PMD_LOG_DEBUG(logtype, fmt, ...) \
	SXE2_PMD_LOG(DEBUG, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_INFO(logtype, fmt, ...) \
	SXE2_PMD_LOG(INFO, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_NOTICE(logtype, fmt, ...) \
	SXE2_PMD_LOG(NOTICE, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_WARN(logtype, fmt, ...) \
	SXE2_PMD_LOG(WARNING, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_ERR(logtype, fmt, ...) \
	SXE2_PMD_LOG(ERR, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_CRIT(logtype, fmt, ...) \
	SXE2_PMD_LOG(CRIT, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_ALERT(logtype, fmt, ...) \
	SXE2_PMD_LOG(ALERT, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_LOG_EMERG(logtype, fmt, ...) \
	SXE2_PMD_LOG(EMERG, SXE2_##logtype, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_DEBUG(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(DEBUG, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_INFO(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(INFO, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_NOTICE(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(NOTICE, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_WARN(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(WARNING, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_ERR(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(ERR, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_CRIT(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(CRIT, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_ALERT(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(ALERT, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_DEV_LOG_EMERG(adapter, logtype, fmt, ...) \
	SXE2_PMD_DRV_LOG(EMERG, SXE2_##logtype, adapter, fmt, ##__VA_ARGS__)

#define PMD_INIT_FUNC_TRACE() PMD_LOG_DEBUG(INIT, " >>")

#endif /* SXE2_COMMON_LOG_H */
