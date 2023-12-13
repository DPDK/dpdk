/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2022-2023, Advanced Micro Devices, Inc.
 */

#ifndef CDX_LOGS_H
#define CDX_LOGS_H

extern int cdx_logtype_bus;
#define RTE_LOGTYPE_CDX_BUS cdx_logtype_bus

#define CDX_BUS_LOG(level, ...) \
	RTE_LOG_LINE(level, CDX_BUS, __VA_ARGS__)

/* Debug logs with Function names */
#define CDX_BUS_DEBUG(...) \
	RTE_LOG_LINE_PREFIX(DEBUG, CDX_BUS, "%s(): ", __func__, __VA_ARGS__)

#define CDX_BUS_INFO(fmt, args...) \
	CDX_BUS_LOG(INFO, fmt, ## args)
#define CDX_BUS_ERR(fmt, args...) \
	CDX_BUS_LOG(ERR, fmt, ## args)
#define CDX_BUS_WARN(fmt, args...) \
	CDX_BUS_LOG(WARNING, fmt, ## args)

#endif /* CDX_LOGS_H */
