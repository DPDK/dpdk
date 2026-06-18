/* SPDX-License-Identifier: BSD-3-Clause
 *   Copyright 2016,2021 NXP
 */

#ifndef BUS_FSLMC_PRIVATE_H
#define BUS_FSLMC_PRIVATE_H

#include <bus_driver.h>

#include <bus_fslmc_driver.h>

extern struct rte_bus rte_fslmc_bus;

void fslmc_bus_remove_device(struct rte_dpaa2_device *dev);

#endif /* BUS_FSLMC_PRIVATE_H */
