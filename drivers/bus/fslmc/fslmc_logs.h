/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2016 NXP
 *
 */

#ifndef _FSLMC_LOGS_H_
#define _FSLMC_LOGS_H_

extern int dpaa2_logtype_bus;
#define RTE_LOGTYPE_DPAA2_BUS dpaa2_logtype_bus

#define DPAA2_BUS_LOG(level, ...) \
	RTE_LOG_LINE(level, DPAA2_BUS, __VA_ARGS__)

/* Debug logs are with Function names */
#define DPAA2_BUS_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, DPAA2_BUS, "%s(): ", __func__, __VA_ARGS__)

#define DPAA2_BUS_INFO(fmt, ...) \
	DPAA2_BUS_LOG(INFO, fmt, ## __VA_ARGS__)
#define DPAA2_BUS_ERR(fmt, ...) \
	DPAA2_BUS_LOG(ERR, fmt, ## __VA_ARGS__)
#define DPAA2_BUS_WARN(fmt, ...) \
	DPAA2_BUS_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _FSLMC_LOGS_H_ */
