/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018 NXP
 */

#ifndef _DPAAX_LOGS_H_
#define _DPAAX_LOGS_H_

#include <rte_log.h>

extern int dpaax_logger;

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

/* DP Logs, toggled out at compile time if level lower than current level */
#define DPAAX_DP_LOG(level, fmt, args...) \
	RTE_LOG_DP(level, PMD, fmt, ## args)

#define DPAAX_DP_DEBUG(fmt, args...) \
	DPAAX_DP_LOG(DEBUG, fmt, ## args)
#define DPAAX_DP_INFO(fmt, args...) \
	DPAAX_DP_LOG(INFO, fmt, ## args)
#define DPAAX_DP_WARN(fmt, args...) \
	DPAAX_DP_LOG(WARNING, fmt, ## args)

#endif /* _DPAAX_LOGS_H_ */
