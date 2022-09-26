/* SPDX-License-Identifier: BSD-3-Clause
 *   Copyright 2016,2021 NXP
 */

#ifndef BUS_FSLMC_PRIVATE_H
#define BUS_FSLMC_PRIVATE_H

#include <bus_driver.h>

#include <bus_fslmc_driver.h>

/*
 * FSLMC bus
 */
struct rte_fslmc_bus {
	struct rte_bus bus;     /**< Generic Bus object */
	TAILQ_HEAD(, rte_dpaa2_device) device_list;
				/**< FSLMC DPAA2 Device list */
	TAILQ_HEAD(, rte_dpaa2_driver) driver_list;
				/**< FSLMC DPAA2 Driver list */
	int device_count[DPAA2_DEVTYPE_MAX];
				/**< Count of all devices scanned */
};

extern struct rte_fslmc_bus rte_fslmc_bus;

#endif /* BUS_FSLMC_PRIVATE_H */
