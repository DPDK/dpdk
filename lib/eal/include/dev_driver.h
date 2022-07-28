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

#ifdef __cplusplus
}
#endif

#endif /* DEV_DRIVER_H */
