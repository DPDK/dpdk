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

#define DPAAX_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, dpaax_logger, "dpaax: " fmt "\n", \
		##args)

/* Debug logs are with Function names */
#define DPAAX_DEBUG(fmt, args...) \
	rte_log(RTE_LOG_DEBUG, dpaax_logger, "dpaax: %s():	 " fmt "\n", \
		__func__, ##args)

#define DPAAX_INFO(fmt, args...) \
	DPAAX_LOG(INFO, fmt, ## args)
#define DPAAX_ERR(fmt, args...) \
	DPAAX_LOG(ERR, fmt, ## args)
#define DPAAX_WARN(fmt, args...) \
	DPAAX_LOG(WARNING, fmt, ## args)

#endif /* _DPAAX_LOGS_H_ */
