/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#ifndef _IFPGA_LOGS_H_
#define _IFPGA_LOGS_H_

#include <rte_log.h>

extern int ifpga_bus_logtype;
#define RTE_LOGTYPE_IFPGA_BUS ifpga_bus_logtype

#define IFPGA_BUS_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, IFPGA_BUS, "%s(): ", __func__, __VA_ARGS__)

#define IFPGA_BUS_FUNC_TRACE() IFPGA_BUS_LOG(DEBUG, ">>")

#define IFPGA_BUS_DEBUG(fmt, ...) \
	IFPGA_BUS_LOG(DEBUG, fmt, ## __VA_ARGS__)
#define IFPGA_BUS_INFO(fmt, ...) \
	IFPGA_BUS_LOG(INFO, fmt, ## __VA_ARGS__)
#define IFPGA_BUS_ERR(fmt, ...) \
	IFPGA_BUS_LOG(ERR, fmt, ## __VA_ARGS__)
#define IFPGA_BUS_WARN(fmt, ...) \
	IFPGA_BUS_LOG(WARNING, fmt, ## __VA_ARGS__)

#endif /* _IFPGA_BUS_LOGS_H_ */
