/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _DPAAX_LOGS_H_
#define _DPAAX_LOGS_H_

#include <rte_log.h>

extern int dpaax_logger;
#define RTE_LOGTYPE_DPAAX_LOGGER dpaax_logger

#ifdef RTE_LIBRTE_DPAAX_DEBUG
#define DPAAX_HWWARN(cond, fmt, args...) \
	do {\
		if (cond) \
			DPAAX_LOG(DEBUG, "WARN: " fmt, ##args); \
	} while (0)
#else
#define DPAAX_HWWARN(cond, fmt, args...) do { } while (0)
#endif

#define DPAAX_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAAX_LOGGER, __VA_ARGS__)

/* Debug logs are with Function names */
#define DPAAX_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAAX_LOGGER, "%s(): ", __func__, __VA_ARGS__)

#define DPAAX_INFO(fmt, args...) \
	DPAAX_LOG(INFO, fmt, ## args)
#define DPAAX_ERR(fmt, args...) \
	DPAAX_LOG(ERR, fmt, ## args)
#define DPAAX_WARN(fmt, args...) \
	DPAAX_LOG(WARNING, fmt, ## args)

#endif /* _DPAAX_LOGS_H_ */
