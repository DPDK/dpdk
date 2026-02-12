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

extern struct rte_platform_bus platform_bus;

/* Platform bus iterators. */
#define FOREACH_DEVICE_ON_PLATFORM_BUS(p) \
	RTE_TAILQ_FOREACH(p, &(platform_bus.device_list), next)

#define FOREACH_DRIVER_ON_PLATFORM_BUS(p) \
	RTE_TAILQ_FOREACH(p, &(platform_bus.driver_list), next)

/*
 * Structure describing platform bus.
 */
struct rte_platform_bus {
	struct rte_bus bus; /* Core bus */
	RTE_TAILQ_HEAD(, rte_platform_device) device_list; /* List of bus devices */
	RTE_TAILQ_HEAD(, rte_platform_driver) driver_list; /* List of bus drivers */
};

extern int platform_bus_logtype;
#define RTE_LOGTYPE_PLATFORM_BUS platform_bus_logtype
#define PLATFORM_LOG_LINE(level, ...) \
	RTE_LOG_LINE(level, PLATFORM_BUS, __VA_ARGS__)

/*
 * Iterate registered platform devices and find one that matches provided string.
 */
void *
platform_bus_dev_iterate(const void *start, const char *str,
			 const struct rte_dev_iterator *it __rte_unused);

#endif /* PLATFORM_PRIVATE_H */
