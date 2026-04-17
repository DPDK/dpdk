/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#ifndef PLATFORM_PRIVATE_H
#define PLATFORM_PRIVATE_H

#include <bus_driver.h>
#include <rte_bus.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_log.h>
#include <rte_os.h>

#include "bus_platform_driver.h"

extern int platform_bus_logtype;
#define RTE_LOGTYPE_PLATFORM_BUS platform_bus_logtype
#define PLATFORM_LOG_LINE(level, ...) \
	RTE_LOG_LINE(level, PLATFORM_BUS, __VA_ARGS__)

#endif /* PLATFORM_PRIVATE_H */
