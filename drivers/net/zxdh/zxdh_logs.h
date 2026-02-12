/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_LOGS_H
#define ZXDH_LOGS_H

#include <rte_log.h>

extern int zxdh_logtype_driver;
#define RTE_LOGTYPE_ZXDH_DRIVER zxdh_logtype_driver
#define PMD_DRV_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZXDH_DRIVER, "%s(): ", __func__, __VA_ARGS__)

extern int zxdh_logtype_rx;
#define RTE_LOGTYPE_ZXDH_RX zxdh_logtype_rx
#define PMD_RX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZXDH_RX, "%s(): ", __func__, __VA_ARGS__)

extern int zxdh_logtype_tx;
#define RTE_LOGTYPE_ZXDH_TX zxdh_logtype_tx
#define PMD_TX_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZXDH_TX, "%s(): ", __func__, __VA_ARGS__)

extern int zxdh_logtype_msg;
#define RTE_LOGTYPE_ZXDH_MSG zxdh_logtype_msg
#define PMD_MSG_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, ZXDH_MSG, "%s(): ", __func__, __VA_ARGS__)

#endif /* ZXDH_LOGS_H */
