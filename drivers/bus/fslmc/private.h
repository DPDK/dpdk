/* SPDX-License-Identifier: BSD-3-Clause
 *   Copyright 2016,2021 NXP
 */

#ifndef BUS_FSLMC_PRIVATE_H
#define BUS_FSLMC_PRIVATE_H

#include <bus_driver.h>

#include <bus_fslmc_driver.h>

extern struct rte_bus rte_fslmc_bus;

RTE_TAILQ_HEAD(fslmc_control_device_list, rte_device);
extern struct fslmc_control_device_list fslmc_control_devices;

void fslmc_bus_remove_device(struct rte_dpaa2_device *dev);
void fslmc_remove_control_device(struct rte_dpaa2_device *dev);

#endif /* BUS_FSLMC_PRIVATE_H */
