/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Red Hat, Inc.
 */

#ifndef DEV_DRIVER_H
#define DEV_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>
#include <rte_dev.h>

/**
 * A structure describing a device driver.
 */
struct rte_driver {
	RTE_TAILQ_ENTRY(rte_driver) next; /**< Next in list. */
	const char *name;                   /**< Driver name. */
	const char *alias;              /**< Driver alias. */
};

/**
 * A structure describing a generic device.
 */
struct rte_device {
	RTE_TAILQ_ENTRY(rte_device) next; /**< Next device */
	const char *name;             /**< Device name */
	const char *bus_info;         /**< Device bus specific information */
	const struct rte_driver *driver; /**< Driver assigned after probing */
	const struct rte_bus *bus;    /**< Bus handle assigned on scan */
	int numa_node;                /**< NUMA node connection */
	struct rte_devargs *devargs;  /**< Arguments for latest probing */
};

#ifdef __cplusplus
}
#endif

#endif /* DEV_DRIVER_H */
