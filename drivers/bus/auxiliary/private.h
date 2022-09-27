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

#define AUXILIARY_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, auxiliary_bus_logtype, \
		RTE_FMT("auxiliary bus: " RTE_FMT_HEAD(__VA_ARGS__,) "\n", \
			RTE_FMT_TAIL(__VA_ARGS__,)))

/*
 * Structure describing the auxiliary bus
 */
struct rte_auxiliary_bus {
	struct rte_bus bus;                  /* Inherit the generic class */
	TAILQ_HEAD(, rte_auxiliary_device) device_list;  /* List of devices */
	TAILQ_HEAD(, rte_auxiliary_driver) driver_list;  /* List of drivers */
};

extern struct rte_auxiliary_bus auxiliary_bus;

/* Auxiliary bus iterators */
#define FOREACH_DEVICE_ON_AUXILIARY_BUS(p) \
	TAILQ_FOREACH(p, &(auxiliary_bus.device_list), next)

#define FOREACH_DRIVER_ON_AUXILIARY_BUS(p) \
	TAILQ_FOREACH(p, &(auxiliary_bus.driver_list), next)

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
 * Validate whether a device with given auxiliary device should be ignored
 * or not.
 */
bool auxiliary_is_ignored_device(const char *name);

/*
 * Add an auxiliary device to the auxiliary bus (append to auxiliary device
 * list). This function also updates the bus references of the auxiliary
 * device and the generic device object embedded within.
 */
void auxiliary_add_device(struct rte_auxiliary_device *aux_dev);

/*
 * Insert an auxiliary device in the auxiliary bus at a particular location
 * in the device list. It also updates the auxiliary bus reference of the
 * new devices to be inserted.
 */
void auxiliary_insert_device(struct rte_auxiliary_device *exist_aux_dev,
			     struct rte_auxiliary_device *new_aux_dev);

/*
 * Match the auxiliary driver and device by driver function.
 */
bool auxiliary_match(const struct rte_auxiliary_driver *aux_drv,
		     const struct rte_auxiliary_device *aux_dev);

/*
 * Iterate over devices, matching any device against the provided string.
 */
void *auxiliary_dev_iterate(const void *start, const char *str,
			    const struct rte_dev_iterator *it);

#endif /* BUS_AUXILIARY_PRIVATE_H */
