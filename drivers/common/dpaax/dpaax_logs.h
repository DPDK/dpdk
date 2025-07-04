/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _DPAAX_LOGS_H_
#define _DPAAX_LOGS_H_

#include <rte_log.h>

extern int dpaax_logger;
#define RTE_LOGTYPE_DPAAX_LOGGER dpaax_logger

#ifdef RTE_LIBRTE_DPAAX_DEBUG
#define DPAAX_HWWARN(cond, fmt, ...) \
	do {\
		if (cond) \
			DPAAX_LOG(DEBUG, "WARN: " fmt, ##__VA_ARGS__); \
	} while (0)
#else
#define DPAAX_HWWARN(cond, fmt, ...) do { } while (0)
#endif

#define DPAAX_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAAX_LOGGER, __VA_ARGS__)

/* Debug logs are with Function names */
#define DPAAX_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAAX_LOGGER, "%s(): ", __func__, __VA_ARGS__)

#define DPAAX_INFO(fmt, ...) \
	DPAAX_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAAX_ERR(fmt, ...) \
	DPAAX_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAAX_WARN(fmt, ...) \
	DPAAX_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _DPAAX_LOGS_H_ */
