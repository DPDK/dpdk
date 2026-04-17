/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2021 NVIDIA Corporation & Affiliates
 */

#ifndef BUS_AUXILIARY_PRIVATE_H
#define BUS_AUXILIARY_PRIVATE_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/queue.h>

#include <bus_driver.h>

#include "bus_auxiliary_driver.h"

extern int auxiliary_bus_logtype;
#define RTE_LOGTYPE_AUXILIARY_BUS auxiliary_bus_logtype

#define AUXILIARY_LOG(level, ...) \
	RTE_LOG_LINE(level, AUXILIARY_BUS, __VA_ARGS__)

/*
 * Structure describing the auxiliary bus
 */
struct rte_auxiliary_bus {
	struct rte_bus bus;                  /* Inherit the generic class */
};

extern struct rte_auxiliary_bus auxiliary_bus;

/*
 * Test whether the auxiliary device exist.
 */
bool auxiliary_dev_exists(const char *name);

/*
 * Scan the content of the auxiliary bus, and the devices in the devices
 * list.
 */
int auxiliary_scan(void);

/*
 * Update a device being scanned.
 */
void auxiliary_on_scan(struct rte_auxiliary_device *aux_dev);

/*
 * Match the auxiliary driver and device by driver function.
 */
bool auxiliary_match(const struct rte_auxiliary_driver *aux_drv,
		     const struct rte_auxiliary_device *aux_dev);

#endif /* BUS_AUXILIARY_PRIVATE_H */
